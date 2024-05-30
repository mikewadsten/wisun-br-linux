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
#include <systemd/sd-bus.h>

#include "common/ws_keys.h"
#include "app_wsrd/app/wsrd.h"

#include "dbus.h"

static int dbus_get_gaks(sd_bus *bus, const char *path, const char *interface,
                         const char *property, sd_bus_message *reply,
                         void *userdata, sd_bus_error *ret_error)
{
    struct wsrd *wsrd = userdata;
    uint8_t gak[16];

    // FIXME: get keys from supplicant
    ws_generate_gak(wsrd->config.ws_netname, wsrd->config.ws_gtk, gak);

    sd_bus_message_open_container(reply, 'a', "ay");
    sd_bus_message_append_array(reply, 'y', gak, sizeof(gak));
    sd_bus_message_close_container(reply);
    return 0;
}

static int dbus_get_pan_id(sd_bus *bus, const char *path, const char *interface,
                           const char *property, sd_bus_message *reply,
                           void *userdata, sd_bus_error *ret_error)
{
    sd_bus_message_append_basic(reply, 'q', userdata);
    return 0;
}

static int dbus_get_hw_address(sd_bus *bus, const char *path, const char *interface,
                               const char *property, sd_bus_message *reply,
                               void *userdata, sd_bus_error *ret_error)
{
    uint8_t *hw_addr = userdata;

    sd_bus_message_append_array(reply, 'y', hw_addr, 8);
    return 0;
}

const struct sd_bus_vtable wsrd_dbus_vtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("HwAddress",     "ay",  dbus_get_hw_address,     offsetof(struct wsrd, rcp.eui64),      0),
    SD_BUS_PROPERTY("PanId",         "q",   dbus_get_pan_id,         offsetof(struct wsrd, ws.pan_id),      0),
    SD_BUS_PROPERTY("Gaks",          "aay", dbus_get_gaks,           0,                                     0),
    SD_BUS_VTABLE_END,
};
