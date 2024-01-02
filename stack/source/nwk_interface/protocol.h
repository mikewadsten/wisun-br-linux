/*
 * Copyright (c) 2014-2021, Pelion and affiliates.
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
/**
 *
 * \file protocol.h
 * \brief Protocol support functions.
 *
 *  Protocol core support functions.
 *
 */

#ifndef _NS_PROTOCOL_H
#define _NS_PROTOCOL_H

#include "common/trickle.h"

// Users of protocol.h can assume it includes these headers
#include "nwk_interface/protocol_abstract.h"
#include "core/ns_buffer.h"
#include "core/ns_address_internal.h"

// Headers below this are implementation details - users of protocol.h shouldn't rely on them
#include "6lowpan/iphc_decode/lowpan_context.h"
#include "6lowpan/ws/ws_common.h"
#include "ipv6_stack/ipv6_routing_table.h"

struct mac_neighbor_table;
struct mac_api;
struct eth_mac_api;
struct arm_device_driver_list;
struct load_balance_api;
struct red_info;
enum addrtype;

#define SLEEP_MODE_REQ      0x80
#define SLEEP_PERIOD_ACTIVE 0x40
#define ICMP_ACTIVE         0x08

void set_power_state(uint8_t mode);
void clear_power_state(uint8_t mode);
uint8_t check_power_state(uint8_t mode);

#define BUFFER_DATA_FIXED_SIZE 0
void protocol_push(buffer_t *buf);
void protocol_core_init(void);

#define INTERFACE_BOOTSTRAP_DEFINED     1
#define INTERFACE_SECURITY_DEFINED      2
#define INTERFACE_NETWORK_DRIVER_SETUP_DEFINED      4
#define INTERFACE_ND_BORDER_ROUTER_DEFINED      8


#define INTERFACE_SETUP_MASK        3
#define INTERFACE_SETUP_READY       3
#define INTERFACE_SETUP_NETWORK_DRIVER_MASK         5
#define INTERFACE_SETUP_NETWORK_DRIVER_READY        5

#define INTERFACE_SETUP_BORDER_ROUTER_MASK          11
#define INTERFACE_SETUP_BORDER_ROUTER_READY         11
typedef enum icmp_state {
    ER_ACTIVE_SCAN,
    ER_BOOTSTRAP_DONE,      // State 5 Wi-SUN
    ER_WAIT_RESTART,
} icmp_state_e;

typedef enum {
    INTERFACE_IDLE = 0,
    INTERFACE_UP = 1
} interface_mode_e;

typedef enum arm_internal_event_type {
    ARM_IN_INTERFACE_BOOTSTRAP_CB, /** call net_bootstrap_cb_run */
} arm_internal_event_type_e;

/** Control selection of MPL Seed Identifier for packets we originate */
typedef enum multicast_mpl_seed_id_mode {
    MULTICAST_MPL_SEED_ID_DEFAULT = -256,               /** Default selection (used to make a domain use the interface's default) */
    MULTICAST_MPL_SEED_ID_MAC_SHORT = -1,               /** Use short MAC address if available (eg IEEE 802.15.4 interface's macShortAddress (16-bit)), else full MAC */
    MULTICAST_MPL_SEED_ID_MAC = -2,                     /** Use MAC padded to 64-bit (eg IEEE 802.15.4 interface's macExtendedAddress, or 48-bit Ethernet MAC followed by 2 zero pad bytes) */
    MULTICAST_MPL_SEED_ID_IID_EUI64 = -3,               /** Use 64-bit IPv6 IID based on EUI-64 (eg 02:11:22:ff:fe:00:00:00 for an Ethernet interface with MAC 00:11:22:00:00:00) */
    MULTICAST_MPL_SEED_ID_IID_SLAAC = -4,               /** Use 64-bit IPv6 IID that would be used for SLAAC */
    MULTICAST_MPL_SEED_ID_IPV6_SRC_FOR_DOMAIN = 0,      /** Use IPv6 source address selection to choose 128-bit Seed ID based on MPL Domain Address as destination */
    MULTICAST_MPL_SEED_ID_16_BIT = 2,                   /** Use a manually-specified 16-bit ID */
    MULTICAST_MPL_SEED_ID_64_BIT = 8,                   /** Use a manually-specified 64-bit ID */
    MULTICAST_MPL_SEED_ID_128_BIT = 16,                 /** Use a manually-specified 128-bit ID */
} multicast_mpl_seed_id_mode_e;

#define INTERFACE_NWK_BOOTSTRAP_ACTIVE                   2
#define INTERFACE_NWK_ACTIVE                            8
#define INTERFACE_NWK_ROUTER_DEVICE                     16
#define INTERFACE_NWK_CONF_MAC_RX_OFF_IDLE              64

typedef struct mac_cordinator {
    unsigned cord_adr_mode: 2;
    uint8_t mac_mlme_coord_address[8];
} mac_cordinator_s;

typedef struct arm_15_4_mac_parameters {
    uint16_t mtu;
    /* Security API USE */
    uint8_t mac_default_ffn_key_index;
    uint8_t mac_default_lfn_key_index;
    uint16_t pan_id;
} arm_15_4_mac_parameters_t;

typedef void mac_poll_fail_cb(int8_t nwk_interface_id_e);

typedef struct ipv6_interface_info {
    uint8_t     static_prefix64[8];
} ipv6_interface_info_t;

struct thread_info;
struct ws_info;
struct auth_info;
struct rpl_domain;

struct net_if {
    int8_t id;
    int8_t bootStrapId;
    uint8_t zone_index[16];
    const char *interface_name;
    ns_list_link_t link;
    uint8_t configure_flags;
    uint8_t lowpan_info;
    uint16_t bootstrap_state_machine_cnt;
    icmp_state_e nwk_bootstrap_state;
    if_address_list_t ip_addresses;
    if_group_list_t ip_groups;
    multicast_mpl_seed_id_mode_e mpl_seed_id_mode;
    trickle_params_t mpl_data_trickle_params;
    uint16_t mpl_seed_set_entry_lifetime;
    uint8_t mpl_seed_id[16];
    struct mpl_domain *mpl_domain;
    lowpan_context_list_t lowpan_contexts;
    bool global_address_available : 1;
    bool reallocate_short_address_if_duplicate : 1;
    ipv6_neighbour_cache_t ipv6_neighbour_cache;
    bool is_dhcp_relay_agent_enabled;

    uint16_t icmp_tokens; /* Token bucket for ICMP rate limiting */
    uint8_t iid_eui64[8]; // IID based on EUI-64 - used for link-local address
    uint8_t iid_slaac[8]; // IID to use for SLAAC addresses - may or may not be same as iid_eui64
    uint16_t max_link_mtu;
    bool pan_advert_running: 1;
    bool pan_config_running: 1;
    /* RFC 4861 Host Variables */
    uint8_t cur_hop_limit;
    uint16_t reachable_time_ttl;        // s
    uint32_t base_reachable_time;       // ms
    bool recv_ra_routes : 1;
    bool recv_ra_prefixes: 1;
    bool send_mld: 1;
    bool mpl_seed: 1;
    /* RFC 4861 Router Variables */
    bool ip_forwarding : 1;
    bool ip_multicast_forwarding : 1;
    bool adv_send_advertisements : 1;
    uint8_t rtr_adv_flags;
    uint16_t min_rtr_adv_interval;      // 100ms ticks
    uint16_t max_rtr_adv_interval;      // 100ms ticks
    /* RFC 4862 Node Configuration */
    uint8_t dup_addr_detect_transmits;
    uint16_t pmtu_lifetime;             // s

    /* Link Layer Part */
    uint8_t mac[8]; // MAC address (EUI-64 for LoWPAN, EUI-48 for Ethernet)

    interface_mode_e interface_mode;
    ipv6_interface_info_t ipv6_configure;
    struct red_info *random_early_detection;
    struct red_info *llc_random_early_detection;
    struct red_info *llc_eapol_random_early_detection;
    struct ws_info ws_info;

    struct rcp *rcp;
    arm_15_4_mac_parameters_t mac_parameters;

    void (*if_stack_buffer_handler)(buffer_t *);
    void (*if_common_forwarding_out_cb)(struct net_if *, buffer_t *);
    bool (*if_ns_transmit)(struct net_if *cur, ipv6_neighbour_t *neighCacheEntry, bool unicast, uint8_t seq);
    bool (*if_map_ip_to_link_addr)(struct net_if *cur, const uint8_t *ip_addr, enum addrtype *ll_type, const uint8_t **ll_addr_out);
    buffer_t *(*if_special_forwarding)(struct net_if *cur, buffer_t *buf, const sockaddr_t *ll_src, bool *bounce);
    buffer_t *(*if_snoop)(struct net_if *cur, buffer_t *buf, const sockaddr_t *ll_dst, const sockaddr_t *ll_src, bool *bounce);
    uint8_t (*if_llao_parse)(struct net_if *cur, const uint8_t *opt_in, sockaddr_t *ll_addr_out);
    uint8_t (*if_llao_write)(struct net_if *cur, uint8_t *opt_out, uint8_t opt_type, bool must, const uint8_t *ip_addr);
};

typedef NS_LIST_HEAD(struct net_if, link) protocol_interface_list_t;

extern protocol_interface_list_t protocol_interface_info_list;

void nwk_interface_print_neigh_cache();

//void nwk_interface_dhcp_process_callback(int8_t interfaceID, bool status,uint8_t * routerId,  dhcpv6_client_server_data_t *server, bool reply);

struct net_if *protocol_stack_interface_info_get();
struct net_if *protocol_stack_interface_generate_lowpan(struct rcp *rcp, int mtu, const char *name);
uint32_t protocol_stack_interface_set_reachable_time(struct net_if *cur, uint32_t base_reachable_time);
void net_bootstrap_cb_run(uint8_t event);

int8_t protocol_interface_address_compare(const uint8_t *addr);

void nwk_bootstrap_timer(int ticks);
void icmp_fast_timer(int ticks);
void update_reachable_time(int seconds);

#endif /* _NS_PROTOCOL_H */
