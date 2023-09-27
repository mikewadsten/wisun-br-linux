/*
 * Copyright (c) 2013-2021, Pelion and affiliates.
 * Copyright (c) 2021-2023 Silicon Laboratories Inc. (www.silabs.com)
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include "common/log.h"
#include "common/rand.h"
#include "common/bits.h"
#include "common/named_values.h"
#include "common/iobuf.h"
#include "common/log_legacy.h"
#include "common/endian.h"

#include "nwk_interface/protocol.h"
#include "nwk_interface/protocol_stats.h"
#include "mpl/mpl.h"
#include "ipv6_stack/ipv6_routing_table.h"
#include "ipv6_stack/ipv6_routing_table.h"
#include "6lowpan/nd/nd_router_object.h"
#include "6lowpan/bootstraps/protocol_6lowpan.h"
#include "6lowpan/ws/ws_common_defines.h"
#include "6lowpan/ws/ws_common.h"

#include "common_protocols/ip.h"
#include "common_protocols/ipv6.h"
#include "common_protocols/icmpv6_prefix.h"

#include "common_protocols/icmpv6.h"

#define TRACE_GROUP "icmp"

/* Check to see if a message is recognisable ICMPv6, and if so, fill in code/type */
/* This used ONLY for the e.1 + e.2 tests in RFC 4443, to try to avoid ICMPv6 error loops */
/* Packet may be ill-formed, because we are considering an ICMPv6 error response. */
static bool is_icmpv6_msg(buffer_t *buf)
{
    const uint8_t *ptr = buffer_data_pointer(buf);
    uint16_t len = buffer_data_length(buf);

    /* IP header format:
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |Version| Traffic Class |           Flow Label                  |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     * |         Payload Length        |  Next Header  |   Hop Limit   |
     * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *    + Source Address (16) + Destination Address (16), total 40
     */
    if (len < IPV6_HDRLEN) {
        return false;
    }
    uint16_t ip_len = read_be16(ptr + IPV6_HDROFF_PAYLOAD_LENGTH);
    uint8_t nh = ptr[IPV6_HDROFF_NH];
    ptr += IPV6_HDRLEN;
    len -= IPV6_HDRLEN;
    if (ip_len > len) {
        return false;
    }
    len = ip_len;
    while (len) {
        uint16_t hdrlen;
        switch (nh) {
            case IPV6_NH_ICMPV6:
                if (len < 4) {
                    return false;
                }
                buf->options.type = ptr[0];
                buf->options.code = ptr[1];
                return true;
            case IPV6_NH_HOP_BY_HOP:
            case IPV6_NH_DEST_OPT:
            case IPV6_NH_ROUTING:
            case IPV6_NH_MOBILITY:
            case IPV6_NH_HIP:
            case IPV6_NH_SHIM6:
                if (len < 8) {
                    return false;
                }
                nh = ptr[0];
                hdrlen = (ptr[1] + 1) * 8;
                break;
            case IPV6_NH_AUTH:
                if (len < 8) {
                    return false;
                }
                nh = ptr[0];
                hdrlen = (ptr[1] + 2) * 4;
                break;
            default:
                return false;
        }
        if (hdrlen > len || (hdrlen & 7)) {
            return false;
        }
        ptr += hdrlen;
        len -= hdrlen;
    }
    return false;
}

buffer_t *icmpv6_error(buffer_t *buf, struct net_if *cur, uint8_t type, uint8_t code, uint32_t aux)
{
    uint8_t *ptr;

    /* Don't send ICMP errors to improperly-secured packets (they either reach MLE etc successfully, or we just drop) */
    if (buf->options.ll_security_bypass_rx) {
        return buffer_free(buf);
    }

    if (cur == NULL) {
        cur = buf->interface;
    }

    /* Any ICMPv6 error in response to an UP packet implies an RX drop... */
    if ((buf->info & B_DIR_MASK) == B_DIR_UP) {
        protocol_stats_update(STATS_IP_RX_DROP, 1);
    }

    /* RFC 4443 processing rules e.1-2: don't send errors for ICMPv6 errors or redirects */
    if (is_icmpv6_msg(buf) && (buf->options.type < 128 || buf->options.type == ICMPV6_TYPE_INFO_REDIRECT)) {
        return buffer_free(buf);
    }

    /* RFC 4443 processing rules e.3-5: don't send errors for IP multicasts or link-layer multicasts+broadcasts (with exceptions) */
    if (addr_is_ipv6_multicast(buf->dst_sa.address) || buf->options.ll_broadcast_rx || buf->options.ll_multicast_rx) {
        if (!(type == ICMPV6_TYPE_ERROR_PACKET_TOO_BIG ||
                (type == ICMPV6_TYPE_ERROR_PARAMETER_PROBLEM && code == ICMPV6_CODE_PARAM_PRB_UNREC_IPV6_OPT))) {
            return buffer_free(buf);
        }
    }

    /* RFC 4443 processing rule e.6 - source doesn't identify a single node */
    if (addr_is_ipv6_unspecified(buf->src_sa.address) || addr_is_ipv6_multicast(buf->src_sa.address)) {
        return buffer_free(buf);
    }

    if (addr_interface_address_compare(cur, buf->dst_sa.address) == 0) {
        // RFC 4443 2.2 - if addressed to us, use that address as source
        memswap(buf->dst_sa.address, buf->src_sa.address, 16);
    } else {
        // Otherwise we will use normal address selection rule
        buf->dst_sa = buf->src_sa;

        // This makes buffer_route choose the address
        buf->src_sa.addr_type = ADDR_NONE;
    }

    buffer_turnaround(buf);

    if (!ipv6_buffer_route(buf)) {
        return buffer_free(buf);
    }
    cur = buf->interface;

    /* Token-bucket rate limiting */
    if (!cur->icmp_tokens) {
        return buffer_free(buf);
    }
    cur->icmp_tokens--;

    /* Include as much of the original packet as possible, without exceeding
     * minimum MTU of 1280. */
    uint16_t max_payload = ipv6_max_unfragmented_payload(buf, IPV6_MIN_LINK_MTU);
    if (buffer_data_length(buf) > max_payload - 8) {
        buffer_data_length_set(buf, max_payload - 8);
    }

    if ((buf = buffer_headroom(buf, 4)) == NULL) {
        return NULL;
    }
    ptr = buffer_data_reserve_header(buf, 4);
    ptr = write_be32(ptr, aux);
    buf->options.traffic_class = 0;
    buf->options.type = type;
    buf->options.code = code;
    buf->options.hop_limit = cur->cur_hop_limit;
    buf->info = (buffer_info_t)(B_FROM_ICMP | B_TO_ICMP | B_DIR_DOWN);

    return (buf);
}

static bool icmpv6_nd_options_validate(const uint8_t *data, size_t len)
{
    struct iobuf_read input = {
        .data_size = len,
        .data = data,
    };
    int opt_start, opt_len;

    while (iobuf_remaining_size(&input)) {
        opt_start = input.cnt;
        iobuf_pop_u8(&input); // Type
        opt_len = 8 * iobuf_pop_u8(&input);
        if (!opt_len)
            return false;
        input.cnt = opt_start;
        iobuf_pop_data_ptr(&input, opt_len);
    }
    return !input.err;
}

static bool icmpv6_nd_option_get(const uint8_t *data, size_t len, uint16_t option, struct iobuf_read *res)
{
    struct iobuf_read input = {
        .data_size = len,
        .data = data,
    };
    int opt_start, opt_len;
    uint8_t opt_type;

    memset(res, 0, sizeof(struct iobuf_read));
    res->err = true;
    while (iobuf_remaining_size(&input)) {
        opt_start = input.cnt;
        opt_type = iobuf_pop_u8(&input);
        opt_len = 8 * iobuf_pop_u8(&input);
        input.cnt = opt_start;
        if (opt_type == option) {
            res->data = iobuf_pop_data_ptr(&input, opt_len);
            if (!res->data)
                return false;
            res->err = false;
            res->data_size = opt_len;
            return true;
        }
        iobuf_pop_data_ptr(&input, opt_len);
    }
    return false;
}

static void icmpv6_na_wisun_aro_handler(struct net_if *cur_interface, const uint8_t *dptr, const uint8_t *src_addr)
{
    (void) src_addr;
    dptr += 2;
    uint16_t life_time;
    uint8_t nd_status  = *dptr;
    dptr += 4;
    life_time = read_be16(dptr);
    dptr += 2;
    if (memcmp(dptr, cur_interface->mac, 8) != 0) {
        return;
    }

    (void)life_time;
    if (nd_status != ARO_SUCCESS) {
        ws_common_black_list_neighbour(src_addr, nd_status);
        ws_common_aro_failure(cur_interface, src_addr);
    }
}

// Wi-SUN allows to use an ARO without an SLLAO. This function builds a dummy
// SLLAO using the information from the ARO, which can be processed using the
// standard ND procedure.
static bool icmpv6_nd_ws_sllao_dummy(struct iobuf_write *sllao, const uint8_t *aro_ptr, size_t aro_len)
{
    struct iobuf_read earo = {
        .data = aro_ptr,
        .data_size = aro_len,
    };
    const uint8_t *eui64;

    iobuf_pop_u8(&earo);          // Type
    iobuf_pop_u8(&earo);          // Length
    iobuf_pop_u8(&earo);          // Status
    iobuf_pop_data_ptr(&earo, 3); // Reserved
    iobuf_pop_be16(&earo);        // Registration Lifetime
    eui64 = iobuf_pop_data_ptr(&earo, 8);

    BUG_ON(sllao->len);
    iobuf_push_u8(sllao, ICMPV6_OPT_SRC_LL_ADDR);
    iobuf_push_u8(sllao, 0); // Length (filled after)
    iobuf_push_data(sllao, eui64, 8);
    while (sllao->len % 8)
        iobuf_push_u8(sllao, 0); // Padding
    sllao->data[1] = sllao->len / 8;

    return !earo.err;
}

/*
 *      Neighbor Solicitation Message Format
 *
 *        0                   1                   2                   3
 *        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |     Type      |     Code      |          Checksum             |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                           Reserved                            |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |                                                               |
 *       +                                                               +
 *       |                                                               |
 *       +                       Target Address                          +
 *       |                                                               |
 *       +                                                               +
 *       |                                                               |
 *       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *       |   Options ...
 *       +-+-+-+-+-+-+-+-+-+-+-+-
 *
 *
 *      Source/Target Link-layer Address
 *
 *       0                   1                   2                   3
 *       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *      |     Type      |    Length     |    Link-Layer Address ...
 *      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 */
static buffer_t *icmpv6_ns_handler(buffer_t *buf)
{
    struct iobuf_read iobuf = {
        .data_size = buffer_data_length(buf),
        .data = buffer_data_pointer(buf),
    };
    struct ipv6_nd_opt_earo na_earo = { };
    struct iobuf_write sllao_dummy = { };
    bool has_earo, has_sllao;
    struct iobuf_read sllao;
    struct iobuf_read earo;
    struct net_if *cur;
    uint8_t target[16];
    bool proxy = false;
    buffer_t *na_buf;

    cur = buf->interface;

    iobuf_pop_data_ptr(&iobuf, 4); // Reserved
    iobuf_pop_data(&iobuf, target, 16);

    has_sllao = icmpv6_nd_option_get(iobuf_ptr(&iobuf), iobuf_remaining_size(&iobuf),
                                     ICMPV6_OPT_SRC_LL_ADDR, &sllao);
    has_earo = icmpv6_nd_option_get(iobuf_ptr(&iobuf), iobuf_remaining_size(&iobuf),
                                    ICMPV6_OPT_ADDR_REGISTRATION, &earo);
    if (!cur->ipv6_neighbour_cache.recv_addr_reg)
        has_earo = false;
    //   Wi-SUN - IPv6 Neighbor Discovery Optimizations
    // Optional usage of SLLAO. The ARO already includes the EUI-64 that is the
    // link-layer address of the node transmitting the Neighbor Solicitation.
    // SLLAO provides a way to use a link layer address other than the EUI-64,
    // but that comes at a 10 octet overhead, and is unnecessary as FAN assumes
    // EUI-64 global uniqueness.
    if (has_earo && !has_sllao) {
        has_sllao = icmpv6_nd_ws_sllao_dummy(&sllao_dummy, earo.data, earo.data_size);
        sllao.data_size = sllao_dummy.len;
        sllao.data      = sllao_dummy.data;
        sllao.err       = false;
        sllao.cnt       = 0;
    }

    //   RFC 4861 Section 7.1.1 - Validation of Neighbor Solicitations
    // A node MUST silently discard any received Neighbor Solicitation
    // messages that do not satisfy all of the following validity checks:
    if (buf->options.hop_limit != 255)
        goto drop; // The IP Hop Limit field has a value of 255
    if (buf->options.code != 0)
        goto drop; // ICMP Code is 0.
    if (addr_is_ipv6_multicast(target))
        goto drop; // Target Address is not a multicast address.
    if (!icmpv6_nd_options_validate(iobuf_ptr(&iobuf), iobuf_remaining_size(&iobuf)))
        goto drop; // All included options have a length that is greater than zero.
    if (addr_is_ipv6_unspecified(buf->src_sa.address)) {
        // If the IP source address is the unspecified address,
        if (!memcmp(buf->dst_sa.address, ADDR_MULTICAST_SOLICITED, sizeof(ADDR_MULTICAST_SOLICITED)))
            goto drop; // the IP destination address is a solicited-node multicast address.
        if (has_sllao)
            goto drop; // there is no source link-layer address option in the message.
    }

    /* This first check's a bit dodgy - it responds to our address on the other
     * interface, which we should only do in the whiteboard case.
     */
    if (addr_interface_address_compare(cur, target) != 0) {
        //tr_debug("Received  NS for proxy %s", tr_ipv6(target));

        proxy = true;
        //Filter Link Local scope
        if (addr_is_ipv6_link_local(target)) {
            goto drop;
        }
    }

    if (has_earo) {
        /* If it had an ARO, and we're paying attention to it, possibilities:
         * 1) No reply to NS now, we need to contact border router (false return)
         * 2) Reply to NS now, with ARO (true return, aro_out.present true)
         * 3) Reply to NS now, without ARO (true return, aro_out.present false)
         */
        if (!nd_ns_earo_handler(cur, earo.data, earo.data_size,
                                has_sllao ? sllao.data : NULL,
                                buf->src_sa.address, target, &na_earo))
            goto drop;
    }

    /* If we're returning an ARO, then we assume the ARO handler has done the
     * necessary to the Neighbour Cache. Otherwise, normal RFC 4861 processing. */
    if (!na_earo.present && has_sllao && cur->if_llao_parse(cur, sllao.data, &buf->dst_sa))
        ipv6_neighbour_update_unsolicited(&cur->ipv6_neighbour_cache, buf->src_sa.address, buf->dst_sa.addr_type, buf->dst_sa.address);

    na_buf = icmpv6_build_na(cur, true, !proxy, addr_is_ipv6_multicast(buf->dst_sa.address), target,
                             na_earo.present ? &na_earo : NULL, buf->src_sa.address);

    buffer_free(buf);
    iobuf_free(&sllao_dummy);

    return na_buf;

drop:
    buf = buffer_free(buf);
    iobuf_free(&sllao_dummy);

    return buf;

}

static buffer_t *icmpv6_redirect_handler(buffer_t *buf, struct net_if *cur)
{
    const uint8_t *ptr = buffer_data_pointer(buf);
    const uint8_t *tgt = ptr + 4;
    const uint8_t *dest = ptr + 20;
    sockaddr_t tgt_ll = { .addr_type = ADDR_NONE };

    if (buf->options.hop_limit != 255) {
        goto drop;
    }

    if (!addr_is_ipv6_link_local(buf->src_sa.address)) {
        goto drop;
    }

    if (buf->options.code != 0) {
        goto drop;
    }

    if (!icmpv6_options_well_formed_in_buffer(buf, 36)) {
        goto drop;
    }

    if (addr_is_ipv6_multicast(dest)) {
        goto drop;
    }

    const uint8_t *tllao = icmpv6_find_option_in_buffer(buf, 36, ICMPV6_OPT_TGT_LL_ADDR);
    if (tllao) {
        cur->if_llao_parse(cur, tllao, &tgt_ll);
    }
    ipv6_destination_redirect(tgt, buf->src_sa.address, dest, buf->interface->id, tgt_ll.addr_type, tgt_ll.address);
    return buffer_free(buf);

drop:
    tr_warn("Redirect drop");
    return buffer_free(buf);
}

static buffer_t *icmpv6_na_handler(buffer_t *buf)
{
    struct net_if *cur;
    uint8_t *dptr = buffer_data_pointer(buf);
    uint8_t flags;
    const uint8_t *target;
    const uint8_t *tllao;
    if_address_entry_t *addr_entry;
    ipv6_neighbour_t *neighbour_entry;

    //"Parse NA at IPv6\n");

    if (buf->options.code != 0 || buf->options.hop_limit != 255) {
        goto drop;
    }

    if (!icmpv6_options_well_formed_in_buffer(buf, 20)) {
        goto drop;
    }

    // Skip the 4 reserved bytes
    flags = *dptr;
    dptr += 4;

    // Note the target IPv6 address
    target = dptr;

    if (addr_is_ipv6_multicast(target)) {
        goto drop;
    }

    /* Solicited flag must be clear if sent to a multicast address */
    if (addr_is_ipv6_multicast(buf->dst_sa.address) && (flags & NA_S)) {
        goto drop;
    }

    cur = buf->interface;

    /* RFC 4862 5.4.4 DAD checks */
    addr_entry = addr_get_entry(cur, target);
    if (addr_entry) {
        tr_debug("NA received for our own address: %s", tr_ipv6(target));
        goto drop;
    }

    const uint8_t *aro = icmpv6_find_option_in_buffer(buf, 20, ICMPV6_OPT_ADDR_REGISTRATION);
    if (aro && aro[1] != 2)
        aro = NULL;
    if (aro) {
        icmpv6_na_wisun_aro_handler(cur, aro, buf->src_sa.address);
    }

    /* No need to create a neighbour cache entry if one doesn't already exist */
    neighbour_entry = ipv6_neighbour_lookup(&cur->ipv6_neighbour_cache, target);
    if (!neighbour_entry) {
        goto drop;
    }

    tllao = icmpv6_find_option_in_buffer(buf, 20, ICMPV6_OPT_TGT_LL_ADDR);
    if (!tllao || !cur->if_llao_parse(cur, tllao, &buf->dst_sa)) {
        buf->dst_sa.addr_type = ADDR_NONE;
    }

    ipv6_neighbour_update_from_na(&cur->ipv6_neighbour_cache, neighbour_entry, flags, buf->dst_sa.addr_type, buf->dst_sa.address);
    if (neighbour_entry->state == IP_NEIGHBOUR_REACHABLE) {
        tr_debug("NA neigh update");
        ws_common_neighbor_update(cur, target);
    }

drop:
    return buffer_free(buf);
}


void trace_icmp(buffer_t *buf, bool is_rx)
{
    static const struct name_value icmp_frames[] = {
        { "na",              ICMPV6_TYPE_INFO_NA },
        { "ns",              ICMPV6_TYPE_INFO_NS },
        { "ra",              ICMPV6_TYPE_INFO_RA },
        { "rs",              ICMPV6_TYPE_INFO_RS },
        { "dac",             ICMPV6_TYPE_INFO_DAC },
        { "dar",             ICMPV6_TYPE_INFO_DAR },
        { "mpl",             ICMPV6_TYPE_INFO_MPL_CONTROL },
        { "ping rpl",        ICMPV6_TYPE_INFO_ECHO_REPLY },
        { "ping req",        ICMPV6_TYPE_INFO_ECHO_REQUEST },
        { "mc done",         ICMPV6_TYPE_INFO_MCAST_LIST_DONE },
        { "mc query",        ICMPV6_TYPE_INFO_MCAST_LIST_QUERY },
        { "mc reprt",        ICMPV6_TYPE_INFO_MCAST_LIST_REPORT },
        { "mc reprt v2",     ICMPV6_TYPE_INFO_MCAST_LIST_REPORT_V2 },
        { "redirect",        ICMPV6_TYPE_INFO_REDIRECT },
        { "e. dest unreach", ICMPV6_TYPE_ERROR_DESTINATION_UNREACH },
        { "e. pkt too big",  ICMPV6_TYPE_ERROR_PACKET_TOO_BIG },
        { "e. timeout",      ICMPV6_TYPE_ERROR_TIME_EXCEEDED },
        { "e. params",       ICMPV6_TYPE_ERROR_PARAMETER_PROBLEM },
        { NULL },
    };
    struct iobuf_read ns_earo_buf;
    char frame_type[40] = "";
    const char *ns_aro_str;
    uint8_t ns_earo_flags;

    strncat(frame_type, val_to_str(buf->options.type, icmp_frames, "[UNK]"),
            sizeof(frame_type) - strlen(frame_type) - 1);
    if (buf->options.type == ICMPV6_TYPE_INFO_NS) {
        if (buffer_data_length(buf) > 20 &&
            icmpv6_nd_option_get(buffer_data_pointer(buf) + 20, buffer_data_length(buf) - 20,
                                 ICMPV6_OPT_ADDR_REGISTRATION, &ns_earo_buf)) {
            iobuf_pop_u8(&ns_earo_buf); // Type
            iobuf_pop_u8(&ns_earo_buf); // Length
            iobuf_pop_u8(&ns_earo_buf); // Status
            iobuf_pop_u8(&ns_earo_buf); // Opaque
            ns_earo_flags = iobuf_pop_u8(&ns_earo_buf);
            if (FIELD_GET(IPV6_ND_OPT_EARO_FLAGS_R_MASK, ns_earo_flags) &&
                FIELD_GET(IPV6_ND_OPT_EARO_FLAGS_T_MASK, ns_earo_flags))
                ns_aro_str = " w/ earo";
            else
                ns_aro_str = " w/ aro";
            strncat(frame_type, ns_aro_str, sizeof(frame_type) - strlen(frame_type) - 1);
        }
    }
    if (is_rx)
        TRACE(TR_ICMP, "rx-icmp %-9s src:%s", frame_type, tr_ipv6(buf->src_sa.address));
    else
        TRACE(TR_ICMP, "tx-icmp %-9s dst:%s", frame_type, tr_ipv6(buf->dst_sa.address));
}

buffer_t *icmpv6_up(buffer_t *buf)
{
    struct iobuf_read iobuf = {
        .data      = buffer_data_pointer(buf),
        .data_size = buffer_data_length(buf),
    };
    struct net_if *cur = buf->interface;

    buf->options.type = iobuf_pop_u8(&iobuf);
    buf->options.code = iobuf_pop_u8(&iobuf);
    iobuf_pop_be16(&iobuf); // Checksum
    if (iobuf.err) {
        TRACE(TR_DROP, "drop %-9s: malformed header", "icmpv6");
        return buffer_free(buf);
    }

    if (buffer_ipv6_fcf(buf, IPV6_NH_ICMPV6)) {
        TRACE(TR_DROP, "drop %-9s: invalid checksum", "icmpv6");
        protocol_stats_update(STATS_IP_CKSUM_ERROR, 1);
        return buffer_free(buf);
    }

    buffer_data_strip_header(buf, 4);

    trace_icmp(buf, true);

    switch (buf->options.type) {
    case ICMPV6_TYPE_INFO_NS:
        return icmpv6_ns_handler(buf);

    case ICMPV6_TYPE_INFO_NA:
        return icmpv6_na_handler(buf);

    case ICMPV6_TYPE_INFO_REDIRECT:
        return icmpv6_redirect_handler(buf, cur);

    default:
        // FIXME: forward DAR/DAC to Linux?
        TRACE(TR_DROP, "drop %-9s: unsupported type %u", "icmpv6", buf->options.type);
        return buffer_free(buf);
    }
}

buffer_t *icmpv6_down(buffer_t *buf)
{
    struct net_if *cur = buf->interface;

    trace_icmp(buf, false);
    buf = buffer_headroom(buf, 4);
    if (buf) {
        uint8_t *dptr;
        dptr = buffer_data_reserve_header(buf, 4);
        buf->info = (buffer_info_t)(B_FROM_ICMP | B_TO_IPV6 | B_DIR_DOWN);

        if (buf->src_sa.addr_type != ADDR_IPV6) {
            if (addr_interface_select_source(cur, buf->src_sa.address, buf->dst_sa.address, 0) != 0) {
                tr_error("ICMP:InterFace Address Get Fail--> free Buffer");
                return buffer_free(buf);
            } else {
                buf->src_sa.addr_type = ADDR_IPV6;
            }

        }

        *dptr++ = buf->options.type;
        *dptr++ = buf->options.code;
        write_be16(dptr, 0);
        write_be16(dptr, buffer_ipv6_fcf(buf, IPV6_NH_ICMPV6));
        buf->options.type = IPV6_NH_ICMPV6;
        buf->options.code = 0;
        buf->options.traffic_class &= ~IP_TCLASS_ECN_MASK;
    }
    return (buf);
}

uint8_t *icmpv6_write_icmp_lla(struct net_if *cur, uint8_t *dptr, uint8_t icmp_opt, bool must, const uint8_t *ip_addr)
{
    dptr += cur->if_llao_write(cur, dptr, icmp_opt, must, ip_addr);

    return dptr;
}

void ack_receive_cb(struct buffer *buffer_ptr, uint8_t status)
{
    /*icmpv6_na_handler functionality based on ACK*/
    ipv6_neighbour_t *neighbour_entry;
    uint8_t ll_target[16];

    if (status != SOCKET_TX_DONE) {
        /*NS failed*/
        return;
    }

    if (buffer_ptr->dst_sa.addr_type == ADDR_IPV6) {
        /*Full IPv6 address*/
        memcpy(ll_target, buffer_ptr->dst_sa.address, 16);
    } else if (buffer_ptr->dst_sa.addr_type == ADDR_802_15_4_LONG) {
        // Build link local address from long MAC address
        memcpy(ll_target, ADDR_LINK_LOCAL_PREFIX, 8);
        memcpy(ll_target + 8, &buffer_ptr->dst_sa.address[2], 8);
        ll_target[8] ^= 2;
    } else {
        tr_warn("wrong address %d %s", buffer_ptr->dst_sa.addr_type, trace_array(buffer_ptr->dst_sa.address, 16));
        return;
    }

    neighbour_entry = ipv6_neighbour_lookup(&buffer_ptr->interface->ipv6_neighbour_cache, ll_target);
    if (neighbour_entry) {
        ipv6_neighbour_update_from_na(&buffer_ptr->interface->ipv6_neighbour_cache, neighbour_entry, NA_S, buffer_ptr->dst_sa.addr_type, buffer_ptr->dst_sa.address);
    }

    ws_common_neighbor_update(buffer_ptr->interface, ll_target);
}
void ack_remove_neighbour_cb(struct buffer *buffer_ptr, uint8_t status)
{
    /*icmpv6_na_handler functionality based on ACK*/
    uint8_t ll_target[16];
    (void)status;

    if (buffer_ptr->dst_sa.addr_type == ADDR_IPV6) {
        /*Full IPv6 address*/
        memcpy(ll_target, buffer_ptr->dst_sa.address, 16);
    } else if (buffer_ptr->dst_sa.addr_type == ADDR_802_15_4_LONG) {
        // Build link local address from long MAC address
        memcpy(ll_target, ADDR_LINK_LOCAL_PREFIX, 8);
        memcpy(ll_target + 8, &buffer_ptr->dst_sa.address[2], 8);
        ll_target[8] ^= 2;
    } else {
        tr_warn("wrong address %d %s", buffer_ptr->dst_sa.addr_type, trace_array(buffer_ptr->dst_sa.address, 16));
        return;
    }
    ws_common_neighbor_remove(buffer_ptr->interface, ll_target);
}

static void icmpv6_aro_cb(buffer_t *buf, uint8_t status)
{
    (void)status;
    uint8_t ll_address[16];
    if (buf->dst_sa.addr_type == ADDR_IPV6) {
        /*Full IPv6 address*/
        memcpy(ll_address, buf->dst_sa.address, 16);
    } else if (buf->dst_sa.addr_type == ADDR_802_15_4_LONG) {
        // Build link local address from long MAC address
        memcpy(ll_address, ADDR_LINK_LOCAL_PREFIX, 8);
        memcpy(ll_address + 8, &buf->dst_sa.address[2], 8);
        ll_address[8] ^= 2;
    }
}

buffer_t *icmpv6_build_ns(struct net_if *cur, const uint8_t target_addr[16], const uint8_t *prompting_src_addr,
                          bool unicast, bool unspecified_source, const struct ipv6_nd_opt_earo *aro)
{
    if (!cur || addr_is_ipv6_multicast(target_addr)) {
        return NULL;
    }

    buffer_t *buf = buffer_get(127);
    if (!buf) {
        return buf;
    }

    buf->options.type = ICMPV6_TYPE_INFO_NS;
    buf->options.code = 0;
    buf->options.hop_limit = 255;

    uint8_t *ptr = buffer_data_pointer(buf);
    ptr = write_be32(ptr, 0);
    memcpy(ptr, target_addr, 16);
    ptr += 16;

    if (aro) {
        *ptr++ = ICMPV6_OPT_ADDR_REGISTRATION;
        *ptr++ = 2;
        *ptr++ = aro->status; /* Should be ARO_SUCCESS in an NS */
        *ptr++ = 0;
        ptr = write_be16(ptr, 0);
        ptr = write_be16(ptr, aro->lifetime);
        memcpy(ptr, aro->eui64, 8);
        ptr += 8;
    }

    if (unicast) {
        memcpy(buf->dst_sa.address, target_addr, 16);
    } else {
        memcpy(buf->dst_sa.address, ADDR_MULTICAST_SOLICITED, 13);
        memcpy(buf->dst_sa.address + 13, target_addr + 13, 3);
    }
    buf->dst_sa.addr_type = ADDR_IPV6;

    if (unspecified_source) {
        memset(buf->src_sa.address, 0, 16);
    } else {
        /* RFC 4861 7.2.2. says we should use the source of traffic prompting the NS, if possible */
        /* This is also used to specify the address for ARO messages */
        if (aro || (prompting_src_addr && addr_is_assigned_to_interface(cur, prompting_src_addr))) {
            memcpy(buf->src_sa.address, prompting_src_addr, 16);
        } else {
            /* Otherwise, according to RFC 4861, we could use any address.
             * But there is a 6lowpan/RPL hiccup - a node may have registered
             * to us with an ARO, and we might send it's global address a NUD
             * probe. But it doesn't know _our_ global address, which default
             * address selection would favour.
             * If it was still a host, we'd get away with using our global
             * address, as we'd be its default route, so its reply comes to us.
             * But if it's switched to being a RPL router, it would send its
             * globally-addressed reply packet up the RPL DODAG.
             * Avoid the problem by using link-local source.
             * This will still leave us with an asymmetrical connection - its
             * global address on-link for us, and we send to it directly (and
             * can NUD probe it), whereas it regards us as off-link and will
             * go via RPL (and won't probe us). But it will work fine.
             */
            if (addr_interface_get_ll_address(cur, buf->src_sa.address, 0) < 0) {
                tr_debug("No address for NS");
                return buffer_free(buf);
            }
        }
        /* If ARO Success sending is omitted, MAC ACK is used instead */
        /* Setting callback for receiving ACK from adaptation layer */
        if (aro && cur->ipv6_neighbour_cache.omit_na_aro_success) {
            if (aro->lifetime > 1) {
                buf->ack_receive_cb = icmpv6_aro_cb;
            } else {
                buf->ack_receive_cb = ack_receive_cb;
            }
        }
    }
    if (unicast && (!aro && cur->ipv6_neighbour_cache.omit_na)) {
        /*MAC ACK is processed as success response*/
        buf->ack_receive_cb = ack_receive_cb;
    }

    buf->src_sa.addr_type = ADDR_IPV6;

    /* NS packets are implicitly on-link. If we ever find ourselves sending an
     * NS to a global address, it's because we are in some way regarding
     * it as on-link. (eg, redirect, RPL source routing header). We force
     * transmission as on-link here, regardless of routing table, to avoid any
     * potential oddities.
     */
    ipv6_buffer_route_to(buf, buf->dst_sa.address, cur);

    buffer_data_end_set(buf, ptr);
    buf->interface = cur;
    buf->info = (buffer_info_t)(B_FROM_ICMP | B_TO_ICMP | B_DIR_DOWN);

    return buf;
}

/*
 * Neighbor Advertisement Message Format
 *
 *  0                   1                   2                   3
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |     Type      |     Code      |          Checksum             |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |R|S|O|                     Reserved                            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                                                               |
 *  +                                                               +
 *  |                                                               |
 *  +                       Target Address                          +
 *  |                                                               |
 *  +                                                               +
 *  |                                                               |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |   Options ...
 *  +-+-+-+-+-+-+-+-+-+-+-+-
 *
 *    R              Router flag.
 *    S              Solicited flag.
 *    O              Override flag.
 */

buffer_t *icmpv6_build_na(struct net_if *cur, bool solicited, bool override, bool tllao_required,
                          const uint8_t target[static 16], const struct ipv6_nd_opt_earo *earo,
                          const uint8_t src_addr[static 16])
{
    uint8_t *ptr;
    uint8_t flags;

    /* Check if ARO response and status == success, then sending can be omitted with flag */
    if (cur->ipv6_neighbour_cache.omit_na_aro_success && earo &&
        !(earo->r && earo->t) && earo->status == ARO_SUCCESS) {
        tr_debug("Omit NA ARO success");
        return NULL;
    }
    /* All other than ARO NA messages are omitted and MAC ACK is considered as success */
    if (!tllao_required && (!earo && cur->ipv6_neighbour_cache.omit_na)) {
        return NULL;
    }


    buffer_t *buf = buffer_get(8 + 16 + 16 + 16); /* fixed, target addr, target ll addr, aro */
    if (!buf) {
        return NULL;
    }

    ptr = buffer_data_pointer(buf);
    buf->options.hop_limit = 255;

    // Set the ICMPv6 NA type and code fields as per RFC4861
    buf->options.type = ICMPV6_TYPE_INFO_NA;
    buf->options.code = 0x00;

    flags = 0;

    if (override) {
        flags |= NA_O;
    }

    if (addr_is_ipv6_unspecified(src_addr)) {
        // Solicited flag must be 0 if responding to DAD
        memcpy(buf->dst_sa.address, ADDR_LINK_LOCAL_ALL_NODES, 16);
    } else {
        if (solicited) {
            flags |= NA_S;
        }

        /* See RFC 6775 6.5.2 - errors are sent to LL64 address
         * derived from EUI-64, success to IP source address */
        if (earo && earo->status != ARO_SUCCESS) {
            memcpy(buf->dst_sa.address, ADDR_LINK_LOCAL_PREFIX, 8);
            memcpy(buf->dst_sa.address + 8, earo->eui64, 8);
            buf->dst_sa.address[8] ^= 2;
        } else {
            memcpy(buf->dst_sa.address, src_addr, 16);
        }
    }
    buf->dst_sa.addr_type = ADDR_IPV6;

    /* In theory we could just use addr_select_source(), as RFC 4861 allows
     * any address assigned to the interface as source. But RFC 6775 shows LL64
     * as the source in its appendix, sending NA to a global address, and our
     * lower layers go a bit funny with RPL during bootstrap if we send from a
     * global address to a global address. By favouring the target address as
     * source, we catch that 6LoWPAN case (the target is LL), as well as making
     * it look neater anyway.
     */
    if (addr_is_assigned_to_interface(cur, target)) {
        memcpy(buf->src_sa.address, target, 16);
    } else {
        const uint8_t *src = addr_select_source(cur, buf->dst_sa.address, 0);
        if (!src) {
            tr_debug("No address");
            return buffer_free(buf);
        }
        memcpy(buf->src_sa.address, src, 16);
    }
    buf->src_sa.addr_type = ADDR_IPV6;

    ptr = write_be32(ptr, (uint32_t) flags << 24);
    // Set the target IPv6 address
    memcpy(ptr, target, 16);
    ptr += 16;

    // Set the target Link-Layer address
    ptr = icmpv6_write_icmp_lla(cur, ptr, ICMPV6_OPT_TGT_LL_ADDR, tllao_required, target);

    if (earo) {
        *ptr++ = ICMPV6_OPT_ADDR_REGISTRATION;
        *ptr++ = 2;
        *ptr++ = earo->status;
        *ptr++ = earo->opaque;
        *ptr++ = FIELD_PREP(IPV6_ND_OPT_EARO_FLAGS_I_MASK, earo->i)
               | FIELD_PREP(IPV6_ND_OPT_EARO_FLAGS_R_MASK, earo->r)
               | FIELD_PREP(IPV6_ND_OPT_EARO_FLAGS_T_MASK, earo->t);
        *ptr++ = earo->tid;
        ptr = write_be16(ptr, earo->lifetime);
        memcpy(ptr, earo->eui64, 8);
        ptr += 8;
    }
    if (earo && (earo->status != ARO_SUCCESS && earo->status != ARO_TOPOLOGICALLY_INCORRECT)) {
        /*If Aro failed we will kill the neigbour after we have succeeded in sending message*/
        if (!ws_common_negative_aro_mark(cur, earo->eui64)) {
            tr_debug("Neighbour removed for negative response send");
            return buffer_free(buf);
        }
        buf->options.traffic_class = IP_DSCP_CS6 << IP_TCLASS_DSCP_SHIFT;
        buf->ack_receive_cb = ack_remove_neighbour_cb;
    }

    //Force Next Hop is destination
    ipv6_buffer_route_to(buf, buf->dst_sa.address, cur);

    buffer_data_end_set(buf, ptr);
    buf->info = (buffer_info_t)(B_DIR_DOWN | B_FROM_ICMP | B_TO_ICMP);
    buf->interface = cur;

    return (buf);
}

// TODO: remove this function, and call directly icmpv6_nd_options_validate()
// after popping the fields before offset from the packet.
bool icmpv6_options_well_formed_in_buffer(const buffer_t *buf, uint16_t offset)
{
    if (buffer_data_length(buf) < offset) {
        return false;
    }

    return icmpv6_nd_options_validate(buffer_data_pointer(buf) + offset,
                                      buffer_data_length(buf) - offset);
}

// TODO: remove this function and use directly icmpv6_nd_option_get()
const uint8_t *icmpv6_find_option_in_buffer(const buffer_t *buf, uint_fast16_t offset, uint8_t option)
{
    struct iobuf_read res;

    icmpv6_nd_option_get(buffer_data_pointer(buf) + offset, buffer_data_length(buf) - offset, option, &res);
    if (res.err)
        return NULL;
    else
        return res.data;
}
