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
#include "common/version.h"
#include "common/log.h"

#include "dc.h"

struct dc g_dc = { };

int dc_main(int argc, char *argv[])
{
    struct dc *dc = &g_dc;

    INFO("Silicon Labs Wi-SUN Direct Connect %s", version_daemon_str);

    parse_commandline(&dc->cfg, argc, argv);
    if (dc->cfg.color_output != -1)
        g_enable_color_traces = dc->cfg.color_output;
    return 0;
}
