/*
 * Copyright (c) 2016-2020, Pelion and affiliates.
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

#ifndef SEC_PROT_KEYS_H_
#define SEC_PROT_KEYS_H_
#include <stdint.h>
#include <stdbool.h>
#include "security/protocols/sec_prot.h"
#include "security/protocols/sec_prot_certs.h"

/*
 * Security protocols EAPOL key storage module. This is used by EAPOL protocols to store
 * and update key information. This can be used either as supplicant key storage or as
 * an authenticator key storage for a specific supplicant. Storage can be also used to
 * access global security data (Group Transient Keys and certificate information).
 *
 */

#define PMK_LEN                 32
#define PTK_LEN                 48
#define GTK_LEN                 16
#define GTK_NUM                 4
#define LGTK_NUM                3

#define KCK_LEN                 16
#define KEK_LEN                 16

#define KCK_INDEX               0
#define KEK_INDEX               16

#define PMKID_LEN               16
#define PTKID_LEN               16
#define KEYID_LEN               16

#define GTK_DEFAULT_LIFETIME      (60 * 60 * 24 * 30)     // 30 days
#define GTK_EXPIRE_MISMATCH_TIME  60                      // Supplicant GTK expire time mismatch occurs if GTK expires before this time

#define GTK_STATUS_NEW            0                       // New GTK, can transition to fresh
#define GTK_STATUS_FRESH          1                       // Fresh GTK, live based on hash, can transition to active
#define GTK_STATUS_ACTIVE         2                       // Active GTK, live based on hash, can transition to old
#define GTK_STATUS_OLD            3                       // Old GTK, not to be used for sending

#define GTK_INSTALL_ORDER_FIRST   0                       // Install order runs from 0 to 3, where 0 is the GTK to be installed first
#define GTK_INSTALL_ORDER_SECOND  1                       // Install order runs from 0 to 3, where 1 is the GTK to be installed second

#define GTK_HASH_LEN              8
#define INS_GTK_HASH_LEN          2

// Limit is 60000 (of 65536)
#define PMK_KEY_REPLAY_CNT_LIMIT       60000              // Upper limit for PMK replay counter
#define PMK_KEY_REPLAY_CNT_LIMIT_MASK  0xFFFF             // Upper limit mask

#define SEC_MAXIMUM_LIFETIME   (60 * 60 * 24 * 30 * 24)   // Maximum life time for PMK, PTK, GTKs etc. is two years

typedef struct gtk_key {
    uint8_t                key[GTK_LEN];              /**< Group Transient Key (128 bits) */
    uint32_t               lifetime;                  /**< GTK lifetime in seconds */
    unsigned               status : 2;                /**< Group Transient Key status */
    unsigned               install_order : 2;         /**< Order in which GTK keys are added */
    bool                   set: 1;                    /**< Group Transient Key set (valid value) */
} gtk_key_t;

typedef struct sec_prot_gtk_hash {
    uint8_t                hash[INS_GTK_HASH_LEN];    /**< Inserted GTKs for a PTK hash */
} sec_prot_gtk_hash_t;

typedef struct sec_prot_gtk_keys {
    gtk_key_t              gtk[GTK_NUM];              /**< 4 Group Transient Keys */
    bool                   updated: 1;                /**< Group Transient Keys has been updated */
} sec_prot_gtk_keys_t;

typedef struct sec_prot_gtk {
    sec_prot_gtk_keys_t    *keys;
    sec_prot_gtk_hash_t    ins_gtk_hash[GTK_NUM];     /**< Hashes for inserted GTKs for a PTK */
    uint8_t                gtkl;                      /**< Remote GTKL information */
    int8_t                 gtk_set_index;             /**< Index of GTK to set */
    unsigned               ins_gtk_hash_set: 4;       /**< Hash for inserted GTKs for a PTK set */
} sec_prot_gtk_t;

// Security key data
typedef struct sec_prot_keys {
    uint64_t               pmk_key_replay_cnt;        /**< Pairwise Master Key replay counter */
    uint8_t                pmk[PMK_LEN];              /**< Pairwise Master Key (256 bits) */
    uint8_t                ptk[PTK_LEN];              /**< Pairwise Transient Key (384 bits) */
    uint8_t                ptk_eui_64[8];             /**< Remote EUI-64 used to derive PTK or NULL */
    sec_prot_gtk_t         gtks;                      /**< Group Transient Keys */
    sec_prot_gtk_t         lgtks;                     /**< Group Transient Keys */
    const sec_prot_certs_t *certs;                    /**< Certificates */
    uint32_t               pmk_lifetime;              /**< PMK lifetime in seconds */
    uint32_t               ptk_lifetime;              /**< PTK lifetime in seconds */
    uint8_t                node_role;                 /**< Remote Node Role */
    bool                   pmk_set: 1;                /**< Pairwise Master Key set */
    bool                   ptk_set: 1;                /**< Pairwise Transient Key set */
    bool                   pmk_key_replay_cnt_set: 1; /**< Pairwise Master Key replay counter set */
    bool                   updated: 1;                /**< Keys has been updated */
    bool                   ptk_eui_64_set: 1;         /**< Remote EUI-64 used to derive PTK is set */
    bool                   pmk_mismatch: 1;           /**< Remote PMK mismatch reported */
    bool                   ptk_mismatch: 1;           /**< Remote PTK mismatch reported */
} sec_prot_keys_t;

// Frame counter data
typedef struct frame_counter {
    uint8_t gtk[GTK_LEN];                             /**< GTK of the frame counter */
    uint32_t frame_counter;                           /**< Current frame counter */
    bool set : 1;                                     /**< Value has been set */
} frame_counter_t;

typedef struct frame_counters {
    frame_counter_t counter[GTK_NUM];                 /**< Frame counter for each GTK key */
} frame_counters_t;

// Authenticator supplicant security key data
typedef struct sec_prot_keys_storage {
    uint8_t                pmk[PMK_LEN];              /**< Pairwise Master Key (256 bits) */
    uint8_t                ptk[PTK_LEN];              /**< Pairwise Transient Key (384 bits) */
    uint8_t                ptk_eui_64[8];             /**< Remote EUI-64 used to derive PTK or NULL */
    gtkhash_t              ins_gtk_hash[GTK_NUM];     /**< Hashes for inserted GTKs for a PTK */
    gtkhash_t              ins_lgtk_hash[GTK_NUM];     /**< Hashes for inserted GTKs for a PTK */
    uint16_t               pmk_key_replay_cnt;        /**< Pairwise Master Key replay counter */
    uint16_t               pmk_lifetime;              /**< PMK lifetime (short time format) */
    uint16_t               ptk_lifetime;              /**< PTK lifetime (short time format) */
    unsigned               ins_gtk_hash_set: 4;       /**< Hash for inserted GTKs for a PTK set */
    unsigned               ins_lgtk_hash_set: 4;       /**< Hash for inserted GTKs for a PTK set */
    bool                   pmk_set: 1;                /**< Pairwise Master Key set */
    bool                   ptk_set: 1;                /**< Pairwise Transient Key set */
    bool                   pmk_key_replay_cnt_set: 1; /**< Pairwise Master Key replay counter set */
    bool                   ptk_eui_64_set: 1;         /**< Remote EUI-64 used to derive PTK is set */
} sec_prot_keys_storage_t;

// Security keys (GTKs) and needed network information
typedef struct sec_prot_keys_nw_info {
    char network_name[33];                                 /**< Network name for keys */
    sec_prot_gtk_keys_t *gtks;                             /**< Link to GTKs */
    sec_prot_gtk_keys_t *lgtks;                            /**< Link to LGTKs */
    bool updated : 1;                                      /**< Network info has been updated */
} sec_prot_keys_nw_info_t;

/*
 * GTK mismatch types, list is ordered according to priority of mismatch i.e. if there
 * are both hash and lifetime mismatch, hash has greater priority
 */
typedef enum {
    GTK_NO_MISMATCH = 0,
    GTK_LIFETIME_MISMATCH,
    GTK_HASH_MISMATCH,
} gtk_mismatch_e;

/**
 * sec_prot_keys_init initialises security keys
 *
 * \param sec_keys security keys
 * \param gtks GTK keys
 * \param cert_chain certificates
 *
 */
void sec_prot_keys_init(sec_prot_keys_t *sec_keys, sec_prot_gtk_keys_t *gtks, sec_prot_gtk_keys_t *lgtks, const sec_prot_certs_t *certs);

/**
 * sec_prot_keys_pmk_write writes Pairwise Master Key
 *
 * \param sec_keys security keys
 * \param pmk Pairwise Master Key
 * \param pmk_lifetime PMK lifetime
 *
 */
void sec_prot_keys_pmk_write(sec_prot_keys_t *sec_keys, uint8_t *pmk, uint32_t pmk_lifetime);

/**
 * sec_prot_keys_pmk_delete deletes PMK
 *
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_pmk_delete(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_pmk_get gets Pairwise Master Key
 *
 * \param sec_keys security keys
 *
 * \return PMK or NULL
 *
 */
uint8_t *sec_prot_keys_pmk_get(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_pmk_replay_cnt_get gets PMK replay counter value
 *
 * \param sec_keys security keys
 *
 * \return replay counter value
 *
 */
uint64_t sec_prot_keys_pmk_replay_cnt_get(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_pmk_replay_cnt_increment increments PMK replay counter value by one
 *
 * \param sec_keys security keys
 *
 * \return TRUE counter was incremented
 * \return FALSE counter increment failed
 *
 */
bool sec_prot_keys_pmk_replay_cnt_increment(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_pmk_mismatch_reset reset PMK mismatch
 *
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_pmk_mismatch_reset(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_pmk_lifetime_decrement decrements PMK lifetime
 *
 * \param sec_keys security keys
 * \param seconds elapsed seconds
 *
 * \return true PMK expired and deleted both PMK and PTK
 * \return false PMK not expired
 *
 */
bool sec_prot_keys_pmk_lifetime_decrement(sec_prot_keys_t *sec_keys, uint8_t seconds);

/**
 * sec_prot_keys_ptk_write writes Pairwise Transient Key
 *
 * \param sec_keys security keys
 * \param ptk Pairwise Transient Key
 * \param ptk_lifetime PTK lifetime
 *
 */
void sec_prot_keys_ptk_write(sec_prot_keys_t *sec_keys, uint8_t *ptk, uint32_t ptk_lifetime);

/**
 * sec_prot_keys_ptk_delete deletes PTK
 *
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_ptk_delete(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_ptk_get gets Pairwise Transient Key
 *
 * \param sec_keys security keys
 *
 * \return PTK or NULL
 *
 */
uint8_t *sec_prot_keys_ptk_get(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_ptk_mismatch_reset reset PTK mismatch
 *
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_ptk_mismatch_reset(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_ptk_eui_64_write writes PTK EUI-64
 *
 * \param sec_keys security keys
 * \param eui64 EUI-64
 *
 */
void sec_prot_keys_ptk_eui_64_write(sec_prot_keys_t *sec_keys, const uint8_t *eui_64);

/**
 * sec_prot_keys_ptk_eui_64_get gets PTK EUI-64
 *
 * \param sec_keys security keys
 *
 * \return EUI-64 OR NULL
 *
 */
uint8_t *sec_prot_keys_ptk_eui_64_get(sec_prot_keys_t *sec_keys);

/**
 * sec_prot_keys_ptk_lifetime_decrement decrements PTK lifetime
 *
 * \param sec_keys security keys
 * \param seconds elapsed seconds
 *
 * \return true PTK expired and deleted
 * \return false PTK not expired
 *
 */
bool sec_prot_keys_ptk_lifetime_decrement(sec_prot_keys_t *sec_keys, uint8_t seconds);

/**
 * sec_prot_keys_fresh_gtkl_get gets GTK liveness based on GTK status fields
 *
 * \param gtks GTK keys
 *
 * \return bit field indicating GTK liveness
 *
 */
uint8_t sec_prot_keys_fresh_gtkl_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtkl_set checks whether GTK is live on GTK liveness storage
 *
 * \param sec_keys security keys
 * \param index index of the GTK which liveness is returned
 *
 * \return TRUE GTK is live, FALSE GTK is not live
 *
 */
bool sec_prot_keys_gtkl_gtk_is_live(sec_prot_gtk_t *sec_gtks, uint8_t index);

/**
 * sec_prot_keys_gtkl_gtk_live_set sets that GTK is live to GTK liveness storage
 *
 * \param sec_keys security keys
 * \param index index of the GTK which is set live
 *
 * \return < 0 failure
 * \return >= 0 success
 *
 */
int8_t sec_prot_keys_gtkl_gtk_live_set(sec_prot_gtk_t *sec_gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_insert_index_set sets index of GTK to be inserted
 *
 * \param sec_keys security keys
 * \param index GTK index
 *
 * \return < 0 failure
 * \return >= 0 success
 *
 */
int8_t sec_prot_keys_gtk_insert_index_set(sec_prot_gtk_t *sec_gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_insert_index_get gets index of GTK to be inserted
 *
 * \param sec_keys security keys
 *
 * \return >= 0 GTK index
 * \return < 0 no GTK to be inserted
 *
 */
int8_t sec_prot_keys_gtk_insert_index_get(sec_prot_gtk_t *sec_gtks);

/**
 * sec_prot_keys_gtk_insert_index_clear clears the index of GTK to be inserted
 *
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_gtk_insert_index_clear(sec_prot_gtk_t *sec_gtks);

/**
 * sec_prot_keys_gtkl_from_gtk_insert_index_set sets inserted GTK as live to GTK liveness storage
 *
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_gtkl_from_gtk_insert_index_set(sec_prot_gtk_t *sec_gtks);

/**
 * sec_prot_keys_gtk_insert_index_from_gtkl_get gets inserted GTK based on GTK liveness storage
 *
 * \param sec_keys security keys
 *
 * \return >= 0 GTK index
 * \return < 0 no GTK to be inserted
 *
 */
int8_t sec_prot_keys_gtk_insert_index_from_gtkl_get(sec_prot_gtk_t *sec_gtks);

/**
 * sec_prot_keys_get_gtk_to_insert gets GTK that is marked to be inserted
 *
 * \param sec_keys security keys
 * \param index index of the returned GTK
 *
 * \return GTK or NULL
 *
 */
uint8_t *sec_prot_keys_get_gtk_to_insert(sec_prot_gtk_t *sec_gtks, uint8_t *index);

/**
 * sec_prot_keys_gtk_set sets Group Transient Key
 *
 * \param gtks GTK keys
 * \param index index
 * \param gtk gtk value
 * \param lifetime GTK lifetime
 *
 * \return < 0 failure
 * \return >= 0 success
 *
 */
int8_t sec_prot_keys_gtk_set(sec_prot_gtk_keys_t *gtks, uint8_t index, const uint8_t gtk[GTK_LEN], uint32_t lifetime);

/**
 * sec_prot_keys_gtk_clear clears Group Transient Key
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return < 0 failure
 * \return >= 0 success
 *
 */
int8_t sec_prot_keys_gtk_clear(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_is_set checks if Group Transient Key is set
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return TRUE GTK is set, FALSE GTK is not set
 *
 */
bool sec_prot_keys_gtk_is_set(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_gets gets Group Transient Key
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return GTK or NULL
 *
 */
uint8_t *sec_prot_keys_gtk_get(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_lifetime_get gets GTK lifetime
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return GTK lifetime
 *
 */
uint32_t sec_prot_keys_gtk_lifetime_get(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_lifetime_decrement decrements GTK lifetime
 *
 * \param gtks GTK keys
 * \param index index for GTK
 * \param current_time current timestamp
 * \param seconds elapsed seconds
 * \param gtk_update_enable enable GTK status to be updated
 *
 * \return new GTK lifetime
 *
 */
uint32_t sec_prot_keys_gtk_lifetime_decrement(sec_prot_gtk_keys_t *gtks, uint8_t index, uint64_t current_time, uint32_t seconds, bool gtk_update_enable);

/**
 * sec_prot_keys_gtks_are_updated returns GTKs have been updated flag
 *
 * \param gtks GTK keys
 *
 * \return TRUE GTKs have been updated, FALSE GTKs have not been updated
 *
 */
bool sec_prot_keys_gtks_are_updated(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtks_updated_set resets GTKs have been updated flag
 *
 * \param gtks GTK keys
 *
 *
 */
void sec_prot_keys_gtks_updated_reset(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_status_fresh_set sets GTK as status fresh (if GTK status new)
 *
 * \param gtks GTK keys
 * \param index index
 *
 */
void sec_prot_keys_gtk_status_fresh_set(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_status_fresh_set sets all GTK statuses fresh (if GTK status new)
 *
 * \param gtks GTK keys
 *
 */
void sec_prot_keys_gtk_status_all_fresh_set(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_status_active_set sets fresh GTK to active, and currently active as old
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return < 0 failure
 * \return >= 0 success
 *
 */
int8_t sec_prot_keys_gtk_status_active_set(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_status_active_get gets active GTK
 *
 * \param gtks GTK keys
 *
 * \return >= 0 GTK index
 * \return < 0 no active GTK
 *
 */
int8_t sec_prot_keys_gtk_status_active_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_status_is_live checks whether GTK is active
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return TRUE active, FALSE non active
 *
 */
bool sec_prot_keys_gtk_status_is_live(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtk_status_get gets GTK status
 *
 * \param gtks GTK keys
 * \param index index
 *
 * \return GTK status
 *
 */
uint8_t sec_prot_keys_gtk_status_get(sec_prot_gtk_keys_t *gtks, uint8_t index);

/**
 * sec_prot_keys_gtks_hash_generate generate GTK hash based on all GTKs
 *
 * \param gtks GTK keys
 * \param gtk_hash GTK hash
 *
 */
void sec_prot_keys_gtks_hash_generate(sec_prot_gtk_keys_t *gtks, gtkhash_t *gtk_hash);

/**
 * sec_prot_keys_gtk_hash_generate generate GTK hash for a GTK
 *
 * \param gtk GTK key
 * \param gtk_hash GTK hash for a GTK
 *
 * \return < 0 failure
 * \return >= 0 success
 */
int8_t sec_prot_keys_gtk_hash_generate(uint8_t *gtk, uint8_t *gtk_hash);

/**
 * sec_prot_keys_gtk_valid_check check if GTK is valid
 *
 * \param gtk GTK key
 *
 * \return < 0 failure
 * \return >= 0 success
 */
int8_t sec_prot_keys_gtk_valid_check(uint8_t *gtk);

/**
 * sec_prot_keys_gtk_install_order_last_get gets install order of the last GTK to be installd
 *
 * \param gtks GTK keys
 *
 * \return install order
 *
 */
int8_t sec_prot_keys_gtk_install_order_last_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_install_order_last_get gets GTK that is to be installed last
 *
 * \param gtks GTK keys
 *
 * \return GTK index
 *
 */
int8_t sec_prot_keys_gtk_install_order_last_index_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_install_order_last_lifetime_get gets lifetime of the GTK that is to be installed last
 *
 * \param gtks GTK keys
 *
 * \return lifetime
 *
 */
uint32_t sec_prot_keys_gtk_install_order_last_lifetime_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_install_order_first_index_get gets index of the GTK that is to be installed first
 *
 * \param gtks GTK keys
 *
 * \return GTK index
 *
 */
int8_t sec_prot_keys_gtk_install_order_first_index_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_install_order_second_index_get gets index of the GTK that is to be installed second first
 *
 * \param gtks GTK keys
 *
 * \return GTK index
 *
 */
int8_t sec_prot_keys_gtk_install_order_second_index_get(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_install_order_update updates install order (if GTKs are removed set the order back)
 *
 * \param gtks GTK keys
 *
 */
void sec_prot_keys_gtk_install_order_update(sec_prot_gtk_keys_t *gtks);

/**
 * sec_prot_keys_gtk_install_index_get gets a free index for GTK to be installed
 *
 * \param gtks GTK keys
 *
 * \return GTK index
 *
 */
int8_t sec_prot_keys_gtk_install_index_get(sec_prot_gtk_keys_t *gtks, bool is_lgtk);

/**
 * sec_prot_keys_ptk_installed_gtk_hash_clear_all clear GTK hashes of the GTKs that has been installed
 *                                                to supplicant using the PTK
 * \param sec_keys security keys
 *
 */
void sec_prot_keys_ptk_installed_gtk_hash_clear_all(sec_prot_gtk_t *sec_gtks);

/**
 * sec_prot_keys_ptk_installed_gtk_hash_set set GTK hash of the GTK that has been installed
 *                                          to supplicant using the current PTK
 *
 * \param sec_keys security keys
 * \param is_4wh set by 4WH
 *
 */
void sec_prot_keys_ptk_installed_gtk_hash_set(sec_prot_gtk_t *sec_gtks, bool is_4wh);

/**
 * sec_prot_keys_ptk_installed_gtk_hash_set check if PTK is being used to store new GTK for the index
 *                                          for the supplicant i.e. GTK hash would change
 *
 * \param sec_keys security keys
 *
 */
bool sec_prot_keys_ptk_installed_gtk_hash_mismatch_check(sec_prot_gtk_t *sec_gtks, uint8_t gtk_index);

#endif
