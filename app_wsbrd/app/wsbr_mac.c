/*
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 *
 * The licensor of this software is Silicon Laboratories Inc. Your use of this
 * software is governed by the terms of the Silicon Labs Master Software License
 * Agreement (MSLA) available at [1].  This software is distributed to you in
 * Object Code format and/or Source Code format and is governed by the sections
 * of the MSLA applicable to Object Code, Source Code and Modified Open Source
 * Code. By using this software, you agree to the terms of the MSLA.
 *
 * [1]: https://www.silabs.com/about-us/legal/master-software-license-agreement
 */

/* MAC API implementation */
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "common/log.h"
#include "common/bus.h"
#include "common/named_values.h"
#include "common/rcp_api.h"
#include "common/parsers.h"
#include "common/pcapng.h"
#include "common/spinel.h"
#include "common/bits.h"
#include "common/hif.h"
#include "common/iobuf.h"
#include "common/memutils.h"
#include "common/version.h"
#include "common/ws_regdb.h"
#include "common/ieee802154_frame.h"
#include "common/ieee802154_ie.h"
#include "common/specs/ieee802154.h"

#include "net/protocol.h"
#include "ws/ws_bootstrap.h"
#include "ws/ws_common.h"
#include "ws/ws_config.h"
#include "ws/ws_llc.h"

#include "frame_helpers.h"
#include "wsbrd.h"
#include "wsbr_mac.h"
#include "wsbr_pcapng.h"
#include "timers.h"
#include "tun.h"
#include "dbus.h"
#include "commandline_values.h"

struct ws_neigh *wsbr_get_neighbor(struct net_if *cur, const uint8_t eui64[8])
{
    return ws_neigh_get(&cur->ws_info.neighbor_storage, eui64);
}

void wsbr_data_req_ext(struct net_if *cur,
                       const struct mcps_data_req *data,
                       const struct mcps_data_req_ie_list *ie_ext)
{
    struct ws_neigh *neighbor_ws;
    struct mcps_data_rx_ie_list cnf_fail_ie = { };
    struct mcps_data_cnf cnf_fail = {
        .hif.handle = data->msduHandle,
        .hif.status = HIF_STATUS_TIMEDOUT,
    };
    struct ieee802154_hdr hdr = { };
    struct iobuf_write frame = { };

    BUG_ON(data->TxAckReq && data->fhss_type == HIF_FHSS_TYPE_ASYNC);
    BUG_ON(data->DstAddrMode != IEEE802154_ADDR_MODE_NONE &&
           (data->fhss_type == HIF_FHSS_TYPE_FFN_BC || data->fhss_type == HIF_FHSS_TYPE_LFN_BC || data->fhss_type == HIF_FHSS_TYPE_ASYNC));
    BUG_ON(data->DstAddrMode != IEEE802154_ADDR_MODE_64_BIT &&
           (data->fhss_type == HIF_FHSS_TYPE_FFN_UC || data->fhss_type == HIF_FHSS_TYPE_LFN_UC || data->fhss_type == HIF_FHSS_TYPE_LFN_PA));
    BUG_ON(!ie_ext);
    BUG_ON(ie_ext->payloadIovLength > 2);
    BUG_ON(ie_ext->headerIovLength != 1);
    BUG_ON(data->Key.SecurityLevel && data->Key.SecurityLevel != IEEE802154_SEC_LEVEL_ENC_MIC64);

    neighbor_ws = wsbr_get_neighbor(cur, data->DstAddr);
    if (data->DstAddrMode && !neighbor_ws) {
        WARN("%s: neighbor timeout before packet send", __func__);
        ws_llc_mac_confirm_cb(cur, &cnf_fail, &cnf_fail_ie);
        return;
    }

    hdr.frame_type = IEEE802154_FRAME_TYPE_DATA;
    hdr.ack_req    = data->TxAckReq;
    hdr.pan_id     = data->DstAddrMode ? -1 : cur->ws_info.pan_information.pan_id;
    memcpy(hdr.dst, data->DstAddrMode ? data->DstAddr : ieee802154_addr_bc, 8);
    memcpy(hdr.src, cur->rcp->eui64, 8);
    hdr.seqno      = data->SeqNumSuppressed ? -1 : 0;
    hdr.key_index  = data->Key.KeyIndex;
    ieee802154_frame_write_hdr(&frame, &hdr);
    iobuf_push_data(&frame, ie_ext->headerIeVectorList[0].iov_base,
                    ie_ext->headerIeVectorList[0].iov_len);
    if (ie_ext->payloadIovLength)
        ieee802154_ie_push_header(&frame, IEEE802154_IE_ID_HT2);
    for (int i = 0; i < ie_ext->payloadIovLength; i++)
        iobuf_push_data(&frame, ie_ext->payloadIeVectorList[i].iov_base,
                        ie_ext->payloadIeVectorList[i].iov_len);
    if (data->Key.SecurityLevel)
        iobuf_push_data_reserved(&frame, 8); // MIC-64

    rcp_req_data_tx(cur->rcp, frame.data, frame.len,
                    data->msduHandle,  data->fhss_type, neighbor_ws ? &neighbor_ws->fhss_data_unsecured : NULL,
                    neighbor_ws ? neighbor_ws->frame_counter_min : NULL,
                    data->rate_list[0].phy_mode_id ? data->rate_list : NULL,
                    data->ms_mode == WS_MODE_SWITCH_MAC ? HIF_MODE_SWITCH_TYPE_MAC : HIF_MODE_SWITCH_TYPE_PHY);
    iobuf_free(&frame);
}

void wsbr_tx_cnf(struct rcp *rcp, const struct rcp_tx_cnf *cnf)
{
    struct wsbr_ctxt *ctxt = container_of(rcp, struct wsbr_ctxt, rcp);
    struct mcps_data_cnf mcps_cnf = { .hif = *cnf };
    struct mcps_data_rx_ie_list mcps_ie = { };
    int ret;

    if (cnf->frame_len) {
        ret = wsbr_data_cnf_parse(cnf->frame, cnf->frame_len, &mcps_cnf, &mcps_ie);
        WARN_ON(ret < 0, "invalid ack frame");
        if (!ret && ctxt->config.pcap_file[0])
            wsbr_pcapng_write_frame(ctxt, cnf->timestamp_us, cnf->frame, cnf->frame_len);
    }
    ws_llc_mac_confirm_cb(&ctxt->net_if, &mcps_cnf, &mcps_ie);
}

void wsbr_rx_ind(struct rcp *rcp, const struct rcp_rx_ind *ind)
{
    struct wsbr_ctxt *ctxt = container_of(rcp, struct wsbr_ctxt, rcp);
    struct mcps_data_ind mcps_ind = { .hif = *ind };
    struct mcps_data_rx_ie_list mcps_ie = { };
    int ret;

    ret = wsbr_data_ind_parse(ind->frame, ind->frame_len,
                              &mcps_ind, &mcps_ie,
                              ctxt->net_if.ws_info.pan_information.pan_id);
    if (ret < 0)
        return;
    if (ctxt->config.pcap_file[0])
        wsbr_pcapng_write_frame(ctxt, ind->timestamp_us, ind->frame, ind->frame_len);
    ws_llc_mac_indication_cb(&ctxt->net_if, &mcps_ind, &mcps_ie);
}
