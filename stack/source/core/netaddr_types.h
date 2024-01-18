/*
 * Copyright (c) 2008, 2010-2020, Pelion and affiliates.
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
#ifndef NETADDR_TYPES_H
#define NETADDR_TYPES_H
#include <stdbool.h>
#include <stdint.h>

typedef enum addrtype {
    ADDR_NONE = 0,                              /*!< No address */
    ADDR_802_15_4_SHORT = 2,                    /*!< 16-bit PAN with 16-bit 802.15.4 address */
    ADDR_802_15_4_LONG = 3,                     /*!< 16-bit PAN with 64-bit 802.15.4 address */
    ADDR_IPV6 = 4,                              /*!< 128 bit IPv6 address containing both Prefix (high 64 bits) and Interface identifier (low 64 bits) */
    // ADDR_DATA = 5,                           /*!< Attribute-based data-centric query */
    ADDR_BROADCAST = 6,                         /*!< Broadcast (inc RFC 4944 multicast) address (obsolescent) */
    ADDR_EUI_48 = 7,                            /*!< 48-bit Extended Unique Identifier (eg Ethernet) */
} addrtype_e;

#define ADDR_SIZE 16
typedef uint8_t address_t[ADDR_SIZE];

typedef struct ns_sockaddr {
    enum addrtype addr_type;              /*!< Type of address */
    address_t     address;                /*!< Source or destination address */
    uint16_t      port;                   /*!< Source or destination port */
} sockaddr_t;

static const uint8_t ADDR_LINK_LOCAL_PREFIX[8]         = { 0xfe, 0x80 };
static const uint8_t ADDR_SHORT_ADDR_SUFFIX[6]         = { 0x00, 0x00, 0x00, 0xff, 0xfe, 0x00};

static const uint8_t ADDR_MULTICAST_SOLICITED[13]      = { 0xff, 0x02, [11] = 0x01, 0xff};
static const uint8_t ADDR_IF_LOCAL_ALL_NODES[16]       = { 0xff, 0x01, [15] = 0x01 };
static const uint8_t ADDR_IF_LOCAL_ALL_ROUTERS[16]     = { 0xff, 0x01, [15] = 0x02 };
static const uint8_t ADDR_LINK_LOCAL_ALL_NODES[16]     = { 0xff, 0x02, [15] = 0x01 };
static const uint8_t ADDR_LINK_LOCAL_ALL_ROUTERS[16]   = { 0xff, 0x02, [15] = 0x02 };
static const uint8_t ADDR_REALM_LOCAL_ALL_NODES[16]    = { 0xff, 0x03, [15] = 0x01 };
static const uint8_t ADDR_REALM_LOCAL_ALL_ROUTERS[16]  = { 0xff, 0x03, [15] = 0x02 };
static const uint8_t ADDR_SITE_LOCAL_ALL_ROUTERS[16]   = { 0xff, 0x05, [15] = 0x02 };
static const uint8_t ADDR_ALL_MPL_FORWARDERS[16]       = { 0xff, 0x03, [15] = 0xfc };
static const uint8_t ADDR_LINK_LOCAL_ALL_RPL_NODES[16] = { 0xff, 0x02, [15] = 0x1a };
static const uint8_t ADDR_IPV4_MAPPED_PREFIX[12]       = { [10] = 0xff, 0xff };
static const uint8_t ADDR_LOOPBACK[16]                 = { [15] = 1 };
static const uint8_t ADDR_6TO4[16]                     = { 0x20, 0x02 };

#endif
