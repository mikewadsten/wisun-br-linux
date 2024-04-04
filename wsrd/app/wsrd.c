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
#include <stdlib.h>

#include "wsrd/app/commandline.h"
#include "common/log.h"
#include "common/version.h"
#include "wsrd.h"

int main(int argc, char *argv[])
{
    struct wsrd wsrd = { };

    INFO("Silicon Labs Wi-SUN router %s", version_daemon_str);

    parse_commandline(&wsrd.config, argc, argv);
    if (wsrd.config.color_output != -1)
        g_enable_color_traces = wsrd.config.color_output;

    return EXIT_SUCCESS;
}
