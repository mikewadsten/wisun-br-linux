/*
 * Copyright (c) 2015-2018, 2020, Pelion and affiliates.
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

/*
 * \file protocol_6lowpan.h
 *
 */

#ifndef PROTOCOL_6LOWPAN_H_
#define PROTOCOL_6LOWPAN_H_
#include <stdint.h>
#include <stdbool.h>

struct net_if;
struct ns_sockaddr;
struct rpl_domain;
struct rpl_dodag;
enum addrtype;

void protocol_6lowpan_interface_common_init(struct net_if *cur);
void protocol_6lowpan_configure_core(struct net_if *cur);

int8_t protocol_6lowpan_neighbor_address_state_synch(struct net_if *cur, const uint8_t eui64[8], const uint8_t iid[8]);

#endif /* PROTOCOL_6LOWPAN_H_ */
