/*
 * SPDX-License-Identifier: LicenseRef-MSLA
 * Copyright (c) 2024 Silicon Laboratories Inc. (www.silabs.com)
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
#include "common/authenticator/authenticator.h"
#include "common/authenticator/authenticator_radius.h"
#include "common/specs/eap.h"
#include "common/specs/eapol.h"
#include "common/specs/ieee802159.h"
#include "common/eap.h"
#include "common/eapol.h"
#include "common/iobuf.h"
#include "common/log.h"

#include "authenticator_eap.h"

void auth_eap_send(struct auth_ctx *auth, struct auth_supp_ctx *supp, struct pktbuf *pktbuf)
{
    struct eap_hdr eap;

    eap_trace("tx-eap", pktbuf_head(pktbuf), pktbuf_len(pktbuf));

    BUG_ON(pktbuf_len(pktbuf) < sizeof(eap));
    eap = *(const struct eap_hdr *)pktbuf_head(pktbuf);
    supp->eap_id = eap.identifier;

    eapol_write_hdr_head(pktbuf, EAPOL_PACKET_TYPE_EAP);
    auth_send_eapol(auth, supp, IEEE802159_KMP_ID_8021X,
                    pktbuf_head(pktbuf), pktbuf_len(pktbuf));

    /*
     *   RFC 3748 4.2. Success and Failure
     * Because the Success and Failure packets are not acknowledged, they are
     * not retransmitted by the authenticator, and may be potentially lost.
     */
    if (eap.code != EAP_CODE_SUCCESS && eap.code != EAP_CODE_FAILURE)
        auth_rt_timer_start(auth, supp, IEEE802159_KMP_ID_8021X,
                            pktbuf_head(pktbuf), pktbuf_len(pktbuf));
}

static void auth_eap_tls_reset_supp(struct auth_supp_ctx *supp)
{
    supp->eap_id = 0;
}

void auth_eap_send_request_identity(struct auth_ctx *auth, struct auth_supp_ctx *supp)
{
    struct pktbuf pktbuf = { };

    auth_eap_tls_reset_supp(supp);
    eap_write_hdr_head(&pktbuf, EAP_CODE_REQUEST, supp->eap_id + 1, EAP_TYPE_IDENTITY);
    auth_eap_send(auth, supp, &pktbuf);
    pktbuf_free(&pktbuf);
}

void auth_eap_send_failure(struct auth_ctx *auth, struct auth_supp_ctx *supp)
{
    struct pktbuf pktbuf = { };

    eap_write_hdr_head(&pktbuf, EAP_CODE_FAILURE, supp->eap_id + 1, 0);
    auth_eap_send(auth, supp, &pktbuf);
    pktbuf_free(&pktbuf);
}

void auth_eap_recv(struct auth_ctx *auth, struct auth_supp_ctx *supp, const void *buf, size_t buf_len)
{
    const struct eap_hdr *eap;

    if (buf_len < sizeof(*eap)) {
        TRACE(TR_DROP, "drop %-9s: malformed packet", "eap");
        return;
    }

    eap_trace("rx-eap", buf, buf_len);
    eap = buf;
    if (eap->identifier != supp->eap_id) {
        TRACE(TR_DROP, "drop %-9s: invalid identifier", "eap");
        return;
    }

    timer_stop(&auth->timer_group, &supp->rt_timer);

    if (auth->radius_fd >= 0)
        radius_send_eap(auth, supp, buf, buf_len);
    else
        // TODO: internal EAP-TLS implementation without RADIUS
        TRACE(TR_DROP, "drop %-9s: support disabled", "eap");
}
