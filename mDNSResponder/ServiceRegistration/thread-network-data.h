/* thread-network-data.h
 *
 * Copyright (c) 2024 Apple Computer, Inc. All rights reserved.
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
 *
 * Structure definitions for parsing thread network data TLVs
 */

#ifndef __THREAD_NETWORK_DATA_H__
#define __THREAD_NETWORK_DATA_H__ 1
#include <stdlib.h>

#include "srp.h"

#define MAX_THREAD_NETDATA_LENGTH                 255
#define MAX_THREAD_NETDATA_TLV_LENGTH             (MAX_THREAD_NETDATA_LENGTH - 2) // -2 to remove type and length bytes
#define THREAD_NETDATA_HAS_ROUTE_ENTRY_LENGTH     3
#define THREAD_NETDATA_BORDER_ROUTER_ENTRY_LENGTH 4
#define THREAD_NETDATA_SERVER_ENTRY_LENGTH        4

#define THREAD_NETDATA_HAS_ROUTE_MAX_ENTRIES (MAX_THREAD_NETDATA_TLV_LENGTH / THREAD_NETDATA_HAS_ROUTE_ENTRY_LENGTH)
#define THREAD_NETDATA_BORDER_ROUTER_MAX_ENTRIES \
    (MAX_THREAD_NETDATA_TLV_LENGTH / THREAD_NETDATA_BORDER_ROUTER_ENTRY_LENGTH)
#define THREAD_NETDATA_SERVER_MAX_ENTRIES (MAX_THREAD_NETDATA_TLV_LENGTH / THREAD_NETDATA_SERVER_ENTRY_LENGTH)

typedef enum thread_netdata_error {
    THREAD_NETDATA_STATUS_SUCCESS = 0,
    THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL = 1,
    THREAD_NETDATA_STATUS_UNKNOWN_TLV_TYPE = 2,
    THREAD_NETDATA_STATUS_UNEXPECTED_TLV_TYPE = 3,
    THREAD_NETDATA_STATUS_MALFORMED_TLV = 4,
    THREAD_NETDATA_STATUS_UNSUPPORTED_THREAD_VERSION = 5,
    THREAD_NETDATA_STATUS_TLV_NOT_FOUND = 6,
} thread_netdata_status_t;

typedef enum thread_netdata_tlv_type {
    THREAD_NETDATA_TLV_HAS_ROUTE = 0, // sub-TLV of the PREFIX TLV
    THREAD_NETDATA_TLV_PREFIX = 1,
    THREAD_NETDATA_TLV_BORDER_ROUTER = 2, // sub-TLV of the PREFIX TLV
    THREAD_NETDATA_TLV_6LOWPAN_ID = 3,    // sub-TLV of the PREFIX TLV
    THREAD_NETDATA_TLV_COMMISSIONING = 4,
    THREAD_NETDATA_TLV_SERVICE = 5,
    THREAD_NETDATA_TLV_SERVER = 6 // sub-TLV of the SERVER TLV
} thread_netdata_tlv_type_t;

typedef enum thread_netdata_preference {
    THREAD_NETDATA_PREFERENCE_MEDIUM = 0,
    THREAD_NETDATA_PREFERENCE_HIGH = 1,
    THREAD_NETDATA_PREFERENCE_RESERVED = 2,
    THREAD_NETDATA_PREFERENCE_LOW = 3,
} thread_netdata_preference_t;

typedef enum thread_specification_version {
    THREAD_VERSION_1_DOT_2 = 3, // defined in Thread 1.2 specification document
    THREAD_VERSION_1_DOT_3 = 4, // defined in Thread 1.3 specification document
    THREAD_VERSION_1_DOT_4 = 5, // defined in Thread 1.4 specification document
} thread_version_t;

typedef struct thread_netdata_tlv_iterator {
    const uint8_t *NONNULL buf;
    uint16_t buf_length;
    uint16_t offset;
    thread_version_t thread_version;
} thread_netdata_tlv_iterator_t;

// format described in Thread 1.3 specification, Section 5.18.1
typedef struct thread_netdata_has_route_tlv_entry {
    uint16_t rloc;                          // R_border_router_16
    thread_netdata_preference_t preference; // Prf
    bool nat64_prefix;                      // NP
} thread_netdata_has_route_tlv_entry_t;

typedef struct thread_netdata_has_route_tlv {
    size_t n_entries;
    thread_netdata_has_route_tlv_entry_t entries[THREAD_NETDATA_HAS_ROUTE_MAX_ENTRIES];
} thread_netdata_has_route_tlv_t;

// format described in Thread 1.3 specification, Section 5.18.2
typedef struct thread_netdata_prefix_tlv {
    uint8_t domain_id;
    uint8_t prefix_bit_length;
    uint8_t prefix_octet_length;
    struct in6_addr prefix;
    thread_netdata_tlv_iterator_t sub_tlvs_iterator;
} thread_netdata_prefix_tlv_t;

typedef uint16_t thread_netdata_prefix_flags_t;

// Bits in the prefix flags
#define kThreadNetdataPriorityShift     14
#define kThreadNetdataPreferredShift    13
#define kThreadNetdataSLAACShift        12
#define kThreadNetdataDHCPShift         11
#define kThreadNetdataConfigureShift    10
#define kThreadNetdataDefaultRouteShift 9
#define kThreadNetdataOnMeshShift       8
#define kThreadNetdataDNSShift          7
#define kThreadNetdataDPShift           6

// Macros to fetch values from the prefix flags
#define THREAD_NETDATA_PREFIX_FLAGS_PRIORITY(flags)      (((flags) >> kThreadNetdataPriorityShift) & 3)
#define THREAD_NETDATA_PREFIX_FLAGS_PREFERRED(flags)     (((flags) >> kThreadNetdataPreferredShift) & 1)
#define THREAD_NETDATA_PREFIX_FLAGS_SLAAC(flags)         (((flags) >> kThreadNetdataSLAACShift) & 1)
#define THREAD_NETDATA_PREFIX_FLAGS_DHCP(flags)          (((flags) >> kThreadNetdataDHCPShift) & 1)
#define THREAD_NETDATA_PREFIX_FLAGS_CONFIGURE(flags)     (((flags) >> kThreadNetdataConfigureShift) & 1)
#define THREAD_NETDATA_PREFIX_FLAGS_DEFAULT_ROUTE(flags) (((flags) >> kThreadNetdataDefaultRouteShift) & 1)
#define THREAD_NETDATA_PREFIX_FLAGS_ON_MESH(flags)       (((flags) >> kThreadNetdataOnMeshShift) & 1)
#define THREAD_NETDATA_PREFIX_FLAGS_DNS(flags)           (((flags) >> kThreadNetdataDNSShift) & 1)
#define THREAD_NETDATA_PREFIX_DLAGS_DP(flags)            (((flags) >> kThreadNetdataDPShift) & 1)

// Macros to set values in the prefix flags
#define THREAD_NETDATA_PREFIX_FLAGS_PRIORITY_SET(flags, value) \
    ((flags) = (((flags) & ~(3 << kThreadNetdataPriorityShift)) | (((value) & 3) << kThreadNetdataPriorityShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_PREFERRED_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataPreferredShift)) | (((value) & 1) << kThreadNetdataPreferredShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_SLAAC_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataSLAACShift)) | (((value) & 1) << kThreadNetdataSLAACShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_DHCP_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataDHCPShift)) | (((value) & 1) << kThreadNetdataDHCPShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_CONFIGURE_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataConfigureShift)) | (((value) & 1) << kThreadNetdataConfigureShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_DEFAULT_ROUTE_SET(flags, value) \
    ((flags) \
     = (((flags) & ~(1 << kThreadNetdataDefaultRouteShift)) | (((value) & 1) << kThreadNetdataDefaultRouteShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_ON_MESH_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataOnMeshShift)) | (((value) & 1) << kThreadNetdataOnMeshShift)))
#define THREAD_NETDATA_PREFIX_FLAGS_DNS_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataDNSShift)) | (((value) & 1) << kThreadNetdataDNSShift)))
#define THREAD_NETDATA_PREFIX_DLAGS_DP_SET(flags, value) \
    ((flags) = (((flags) & ~(1 << kThreadNetdataDPShift)) | (((value) & 1) << kThreadNetdataDPShift)))

// format described in Thread 1.3 specification, Section 5.18.3
typedef struct thread_netdata_border_router_entry {
    uint16_t rloc; // P_border_router_16
    thread_netdata_prefix_flags_t
        flags; // prefix flags, can be accessed and set using THREAD_NETDATA_PREFIX_FLAGS_* macros
} thread_netdata_border_router_entry_t;

typedef struct thread_netdata_border_router_tlv {
    size_t n_entries;
    thread_netdata_border_router_entry_t entries[THREAD_NETDATA_BORDER_ROUTER_MAX_ENTRIES];
} thread_netdata_border_router_tlv_t;

// format described in Thread 1.3 specification, Section 5.18.4
typedef struct thread_netdata_6lowpan_id_tlv {
    bool c;
    uint8_t cid;
    uint8_t context_length;
} thread_netdata_6lowpan_id_tlv_t;

// format described in Thread 1.3 specification, Section 5.18.5
typedef struct thread_netdata_commissioning_data_tlv {
    uint16_t length;               // COM_length
    const uint8_t *NONNULL data; // COM_data
} thread_netdata_commissioning_data_tlv_t;

// format described in Thread 1.3 specification, Section 5.18.6
typedef struct thread_netdata_service_tlv {
    uint32_t enterprise_number;  // S_enterprise_number
    uint8_t s_id;                // S_id
    uint8_t data_length;         // S_service_data_Length
    const uint8_t *NONNULL data; // S_service_data
    thread_netdata_tlv_iterator_t sub_tlvs_iterator;
} thread_netdata_service_tlv_t;

// format described in Thread 1.3 specification, Section 5.18.6
typedef struct thread_netdata_server_tlv {
    uint16_t server_rloc;
    uint16_t data_length;
    const uint8_t *NONNULL data;
} thread_netdata_server_tlv_t;

/*
 * Represent a Thread network data TLV. The TLV structure depends on the indicated type.
 */
typedef struct thread_netdata_tlv {
    thread_netdata_tlv_type_t type;
    bool stable;
    union {
        thread_netdata_has_route_tlv_t has_route;                   //  THREAD_NETDATA_TLV_HAS_ROUTE
        thread_netdata_prefix_tlv_t prefix;                         //  THREAD_NETDATA_TLV_PREFIX
        thread_netdata_border_router_tlv_t border_router;           //  THREAD_NETDATA_TLV_BORDER_ROUTER
        thread_netdata_6lowpan_id_tlv_t sixlowpan_id;               //  THREAD_NETDATA_TLV_6LOWPAN_ID
        thread_netdata_commissioning_data_tlv_t commissioning_data; //  THREAD_NETDATA_TLV_COMMISSIONING
        thread_netdata_service_tlv_t service;                       //  THREAD_NETDATA_TLV_SERVICE
        thread_netdata_server_tlv_t server;                         //  THREAD_NETDATA_TLV_SERVER
    };
} thread_netdata_tlv_t;

/*
 * thread_netdata_decode_thread_version:
 *
 *  Decodes the thread specification version number.
 *
 *  version: the raw integer representing the Thread version. Must already be in host endianness.
 *
 *  decoded: a non-NULL address to store the decoded thread version.
 *
 *  supported Thread specification version number.
 */
thread_netdata_status_t
thread_netdata_decode_thread_version(uint16_t version, thread_version_t *NONNULL decoded);

/*
 * thread_netdata_tlv_iterator_init:
 *
 * Initializes a Thread network data TLV iterator. The iterator iterates TLVs over the provided buf
 * and MUST NOT be used after the provided buffer has been discarded. No memory allocation is performed
 * in this function, there is therefore no destroy/release function.
 * An iterator initialized over a buffer of size 0 is valid, it will be considered as an empty iterator.
 *
 *  iterator: a reference to a new iterator to initialize
 *
 *  buf: the buffer containing the TLVs to parse and iterate on
 *
 *  buf_length: the length of the provided buffer.
 *
 *  thread_version:  the thread specification version defining the format of network data TLVs
 *
 *  On success, returns THREAD_NETDATA_STATUS_SUCCESS. Returns an error code otherwise. If an error is returned,
 *  iterator is left unmodified.
 */
thread_netdata_status_t
thread_netdata_tlv_iterator_init(thread_netdata_tlv_iterator_t *NONNULL iterator, const uint8_t *NONNULL buf,
                                 uint16_t buf_length, thread_version_t thread_version);

/*
 * thread_netdata_tlv_iterator_has_next:
 *
 * Returns true then there is a next TLV to be parsed, false otherwise.
 * Returning true does not mean that the next TLV will necessarily be a valid TLV.
 * It could be a malformed TLV, see thread_netdata_tlv_iterator_next.
 *
 *  iterator: the current iterator
 *
 */
bool
thread_netdata_tlv_iterator_has_next(thread_netdata_tlv_iterator_t *NONNULL iterator);

/*
 * thread_netdata_tlv_iterator_next:
 *
 * Provides the next TLV of the iterator.
 *
 *  iterator: the iterator
 *
 *  buf: the buffer containing the TLVs to parse and iterate on
 *
 *  buf_length: the length of the provided buffer.
 *
 *  On success, returns THREAD_NETDATA_STATUS_SUCCESS. Returns an error code otherwise (e.g. tried to decode a malformed
 * TLV).
 */
thread_netdata_status_t
thread_netdata_tlv_iterator_next(thread_netdata_tlv_iterator_t *NONNULL iterator, thread_netdata_tlv_t *NONNULL next);

/*
 * thread_netdata_tlv_iterator_restart:
 *
 * Resets the iterator back to the start of the provided buffer.
 *
 *  iterator: the iterator
 */
void
thread_netdata_tlv_iterator_restart(thread_netdata_tlv_iterator_t *NONNULL iterator);

/*
 * thread_netdata_tlv_iterator_find:
 *
 * Searches through the iterator for the next TLV of the specified type.
 *
 *  iterator: the iterator
 *
 *  type: the type of the TLV to find
 *
 *  ret: the address to store the result to, if a TLV of the specified type was found
 *
 *  If a TLV of the specified type was found, THREAD_NETDATA_STATUS_SUCCESS is returned and the
 *  TLV is stored in ret. Of no TLV of the specified type was found, THREAD_NETDATA_STATUS_TLV_NOT_FOUND is returned.
 *  any return value other than THREAD_NETDATA_STATUS_SUCCESS and THREAD_NETDATA_STATUS_TLV_NOT_FOUND indicates
 *  an error.
 *
 */
thread_netdata_status_t
thread_netdata_tlv_iterator_find(thread_netdata_tlv_iterator_t *NONNULL iterator, thread_netdata_tlv_type_t type,
                                 thread_netdata_tlv_t *NONNULL ret);
#endif // __THREAD_NETWORK_DATA_H__
