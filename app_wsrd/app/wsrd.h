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
#ifndef WSRD_H
#define WSRD_H

#include "app_wsrd/app/commandline.h"
#include "app_wsrd/ws/ws.h"
#include "common/dhcp_client.h"
#include "common/timer.h"

struct wsrd {
    struct wsrd_conf config;
    struct ws_ctx ws;
    struct dhcp_client dhcp;
};

// Necessary for simulation and fuzzing, prefer passing a pointer when possible.
extern struct wsrd g_wsrd;

int wsrd_main(int argc, char *argv[]);

#endif
