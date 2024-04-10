/*
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
#ifndef WSRD_H
#define WSRD_H

#include "wsrd/app/commandline.h"
#include "wsrd/ws/ws.h"
#include "common/rcp_api.h"
#include "common/timer.h"

struct wsrd {
    struct wsrd_conf config;
    struct rcp rcp;
    struct ws_ctx ws;
    struct timer_ctxt timer_ctx;
};

int wsrd_main(int argc, char *argv[]);

#endif
