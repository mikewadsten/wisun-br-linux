/*
 * Copyright (c) 2023 Silicon Laboratories Inc. (www.silabs.com)
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
#ifndef RPL_NODE_H
#define RPL_NODE_H

#include <netinet/in.h>
#include <net/if.h>
#include <stddef.h>
#include <stdint.h>

#include "wsrd/ipv6/rpl_pkt.h"

struct ipv6_ctx;
struct ipv6_neigh;

struct rpl_neigh {
    struct rpl_dio_base dio_base;
    struct rpl_opt_config config;
};

struct rpl_ctx {
    int fd;
};

void rpl_start(struct ipv6_ctx *ipv6);
void rpl_recv(struct ipv6_ctx *ipv6);

void rpl_neigh_add(struct ipv6_ctx *ipv6, struct ipv6_neigh *nce);
void rpl_neigh_del(struct ipv6_ctx *ipv6, struct ipv6_neigh *nce);

#endif
