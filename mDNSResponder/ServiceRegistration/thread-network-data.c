/* thread-network-data.c
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

#include "dns-msg.h"
#include "thread-network-data.h"

#define REQUIRE_OR_EXIT(condition, exit_status_code) \
    if (!(condition)) { \
        exit_status = (exit_status_code); \
        goto exit; \
    }

#define REQUIRE_OR_EXIT_MSG(condition, exit_status_code, fmt, ...) \
    if (!(condition)) { \
        ERROR(fmt, ##__VA_ARGS__); \
        exit_status = (exit_status_code); \
        goto exit; \
    }

#define SUCCESS_OR_EXIT(status_code) \
    if ((status_code) != THREAD_NETDATA_STATUS_SUCCESS) { \
        exit_status = (status_code); \
        goto exit; \
    }

#define SUCCESS_OR_EXIT_MSG(status_code, fmt, ...) \
    if ((status_code) != THREAD_NETDATA_STATUS_SUCCESS) { \
        ERROR(fmt, ##__VA_ARGS__); \
        exit_status = (status_code); \
        goto exit; \
    }

typedef union tlv_with_entries {
    thread_netdata_has_route_tlv_t *NONNULL has_route;
    thread_netdata_border_router_tlv_t *NONNULL border_router;
} tlv_with_entries_t;

static char *NONNULL
thread_netdata_tlv_type_to_string(thread_netdata_tlv_type_t type)
{
    switch (type) {
        case THREAD_NETDATA_TLV_HAS_ROUTE:
            return "has_route";
        case THREAD_NETDATA_TLV_PREFIX:
            return "prefix";
        case THREAD_NETDATA_TLV_BORDER_ROUTER:
            return "border_router";
        case THREAD_NETDATA_TLV_6LOWPAN_ID:
            return "6lowpan_id";
        case THREAD_NETDATA_TLV_COMMISSIONING:
            return "commissioning_data";
        case THREAD_NETDATA_TLV_SERVICE:
            return "service";
        case THREAD_NETDATA_TLV_SERVER:
            return "server";
        default:
            return "unknown";
    }
}

thread_netdata_status_t
thread_netdata_decode_thread_version(uint16_t version, thread_version_t *NONNULL decoded)
{
    thread_netdata_status_t status;
    switch (version) {
        case THREAD_VERSION_1_DOT_2:
        case THREAD_VERSION_1_DOT_3:
        case THREAD_VERSION_1_DOT_4:
            *decoded = (thread_version_t)version;
            status = THREAD_NETDATA_STATUS_SUCCESS;
            break;
        default:
            status = THREAD_NETDATA_STATUS_UNSUPPORTED_THREAD_VERSION;
    }
    return status;
}

thread_netdata_status_t
thread_netdata_tlv_iterator_init(thread_netdata_tlv_iterator_t *NONNULL iterator, const uint8_t *NONNULL buf,
                                 uint16_t buf_length, thread_version_t thread_version)
{
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;

    // We consider an iterator on a buf with length 0 to be valid, the iterator is just empty.
    // A buffer of length 1 is however invalid since it can't contain both the type and length of a TLV.
    // The length must therefore either be 0 or >= 2
    REQUIRE_OR_EXIT(buf_length != 1, THREAD_NETDATA_STATUS_MALFORMED_TLV);
    iterator->buf = buf;
    iterator->buf_length = buf_length;
    iterator->offset = 0;
    iterator->thread_version = thread_version;
exit:
    return exit_status;
}

bool
thread_netdata_tlv_iterator_has_next(thread_netdata_tlv_iterator_t *NONNULL iterator)
{
    // Ensure we can at least read a type and a length from the next TLV
    return iterator->offset < iterator->buf_length && (iterator->buf_length - iterator->offset) > 1;
}

void
thread_netdata_tlv_iterator_restart(thread_netdata_tlv_iterator_t *NONNULL iterator)
{
    iterator->offset = 0;
}

/*
 * Decodes a has_route entry and assigns it to `entry`.
 * Sets `consumed` to the amount of bytes consumed in the buffer in order to decode the entry.
 *
 *  buf points to the start of a route entry
 *  buf_length must be at least the size of a route entry
 *
 */
static thread_netdata_status_t
thread_netdata_decode_has_route_entry(const uint8_t *NONNULL buf, uint16_t buf_length,
                                      thread_netdata_has_route_tlv_entry_t *NONNULL entry, unsigned *NONNULL consumed)
{
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;
    REQUIRE_OR_EXIT(buf_length >= THREAD_NETDATA_HAS_ROUTE_ENTRY_LENGTH, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);
    *consumed = 0;

    REQUIRE_OR_EXIT(dns_u16_parse(buf, buf_length, consumed, &entry->rloc), THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);
    entry->preference = (buf[*consumed] >> 6) & 0x03;
    entry->nat64_prefix = (buf[*consumed] >> 5) & 0x01;
    // reseved bits are ignored
    *consumed += 1;
exit:
    return exit_status;
}

static thread_netdata_status_t
thread_netdata_decode_prefix_payload(const uint8_t *NONNULL tlv_value, uint16_t tlv_length,
                                     thread_netdata_prefix_tlv_t *NONNULL tlv,
                                     thread_version_t thread_version)
{
    memset(tlv, 0, sizeof(thread_netdata_prefix_tlv_t));
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;
    REQUIRE_OR_EXIT_MSG(tlv_length >= 2, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
                        "prefix TLV payload must be at least 2 bytes long (was %u bytes long)", tlv_length);

    size_t offset = 0;
    tlv->domain_id = tlv_value[offset];
    offset += 1;
    tlv->prefix_bit_length = tlv_value[offset];

    // + 7 ensures we have enough bytes to store the prefix (e.g. ensure we have 1 byte if prefix length is 3 bits)
    tlv->prefix_octet_length = (tlv->prefix_bit_length + 7) / 8;
    REQUIRE_OR_EXIT_MSG(
        tlv->prefix_octet_length <= sizeof(tlv->prefix), THREAD_NETDATA_STATUS_MALFORMED_TLV,
        "prefix TLV's prefix length is too large to fit in an IPv6 address (prefix is %d bits / %d bytes long)",
        tlv->prefix_bit_length, tlv->prefix_octet_length);
    offset += 1;
    REQUIRE_OR_EXIT_MSG(
        tlv_length >= offset + tlv->prefix_octet_length, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
        "prefix TLV payload cannot contain the IPv6 prefix of length %d (%d bytes) (only %lu bytes remaining)",
        tlv->prefix_bit_length, tlv->prefix_octet_length, tlv_length - offset);
    memcpy(&tlv->prefix.s6_addr, &tlv_value[offset], tlv->prefix_octet_length);
    offset += tlv->prefix_octet_length;
    SUCCESS_OR_EXIT_MSG(thread_netdata_tlv_iterator_init(&tlv->sub_tlvs_iterator, &tlv_value[offset],
                                                         tlv_length - offset, thread_version),
                        "could not init the Prefix TLV's sub-TLV iterator");
exit:
    return exit_status;
}

/*
 * Decodes a border router entry and assigns it to `entry`.
 * Sets `consumed` to the amount of bytes consumed in the buffer in order to decode the entry.
 *
 *  buf points to the start of a border router entry
 *  buf_length must be at least the size of a border router entry
 *
 */
static thread_netdata_status_t
thread_netdata_decode_border_router_entry(const uint8_t *NONNULL buf, uint16_t buf_length,
                                          thread_netdata_border_router_entry_t *NONNULL entry, unsigned *NONNULL consumed)
{
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;
    REQUIRE_OR_EXIT(buf_length >= THREAD_NETDATA_BORDER_ROUTER_ENTRY_LENGTH, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);
    *consumed = 0;

    REQUIRE_OR_EXIT(dns_u16_parse(buf, buf_length, consumed, &entry->rloc), THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);
    REQUIRE_OR_EXIT(dns_u16_parse(buf, buf_length, consumed, &entry->flags), THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);

exit:
    return exit_status;
}

/*
 * common function whose purpose is to parse TLVs that contain a limited number of sub-entries entries that are not
 * TLVs themselves
 */
static thread_netdata_status_t
thread_netdata_decode_tlv_with_entries(const uint8_t *NONNULL tlv_value, uint16_t tlv_length,
                                       tlv_with_entries_t abstract_tlv, thread_netdata_tlv_type_t type)
{
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;
    size_t total_read = 0;
    // add new entries one by one
    while (total_read < tlv_length) {
        unsigned consumed = 0;
        thread_netdata_status_t status;
        switch (type) {
            case THREAD_NETDATA_TLV_HAS_ROUTE: {
                thread_netdata_has_route_tlv_t *tlv = abstract_tlv.has_route;
                status = thread_netdata_decode_has_route_entry(&tlv_value[total_read], tlv_length - total_read,
                                                               &tlv->entries[tlv->n_entries], &consumed);
                REQUIRE_OR_EXIT_MSG(status == THREAD_NETDATA_STATUS_SUCCESS, THREAD_NETDATA_STATUS_MALFORMED_TLV,
                                    "could not parse has_route TLV entry: error code %d", status);
                tlv->n_entries++;
                break;
            }
            case THREAD_NETDATA_TLV_BORDER_ROUTER: {
                thread_netdata_border_router_tlv_t *tlv = abstract_tlv.border_router;
                status = thread_netdata_decode_border_router_entry(&tlv_value[total_read], tlv_length - total_read,
                                                                   &tlv->entries[tlv->n_entries], &consumed);
                REQUIRE_OR_EXIT_MSG(status == THREAD_NETDATA_STATUS_SUCCESS, THREAD_NETDATA_STATUS_MALFORMED_TLV,
                                    "could not parse border_router TLV entry: error code %d", status);
                tlv->n_entries++;
                break;
            }
            default:
                // we should never reach that
                exit_status = THREAD_NETDATA_STATUS_UNEXPECTED_TLV_TYPE;
                ERROR("TLV of type %d is not supposed to be a TLV with entries", type);
                goto exit;
        }
        total_read += consumed;
    }

exit:
    return exit_status;
}

// wrapper functions that fallback on thread_netdata_decode_tlv_with_entries
static thread_netdata_status_t
thread_netdata_decode_has_route_payload(const uint8_t *NONNULL tlv_value, uint16_t tlv_length,
                                        thread_netdata_has_route_tlv_t *NONNULL tlv)
{
    memset(tlv, 0, sizeof(thread_netdata_has_route_tlv_t));
    tlv_with_entries_t abstract_tlv = { .has_route = tlv };
    return thread_netdata_decode_tlv_with_entries(tlv_value, tlv_length, abstract_tlv, THREAD_NETDATA_TLV_HAS_ROUTE);
}

static thread_netdata_status_t
thread_netdata_decode_border_router_payload(const uint8_t *NONNULL tlv_value, uint16_t tlv_length,
                                            thread_netdata_border_router_tlv_t *NONNULL tlv)
{
    memset(tlv, 0, sizeof(thread_netdata_border_router_tlv_t));
    tlv_with_entries_t abstract_tlv = { .border_router = tlv };
    return thread_netdata_decode_tlv_with_entries(tlv_value, tlv_length, abstract_tlv,
                                                  THREAD_NETDATA_TLV_BORDER_ROUTER);
}

static thread_netdata_status_t
thread_netdata_decode_server_payload(const uint8_t *NONNULL tlv_value, uint16_t tlv_length,
                                     thread_netdata_server_tlv_t *NONNULL tlv)
{
    memset(tlv, 0, sizeof(thread_netdata_server_tlv_t));
    thread_netdata_status_t exit_status;

    REQUIRE_OR_EXIT_MSG(tlv_length >= 2, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
                        "Server TLV value must be at least 2 bytes long (was %u bytes long)", tlv_length);
    unsigned consumed = 0;
    REQUIRE_OR_EXIT(dns_u16_parse(tlv_value, tlv_length, &consumed, &tlv->server_rloc), THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);
    tlv->data_length = tlv_length - consumed;
    tlv->data = &tlv_value[consumed];
    exit_status = THREAD_NETDATA_STATUS_SUCCESS;
exit:
    return exit_status;
}

static thread_netdata_status_t
thread_netdata_decode_6lowpan_id_payload(const uint8_t *NONNULL tlv_value, size_t tlv_length,
                                         thread_netdata_6lowpan_id_tlv_t *NONNULL tlv)
{
    memset(tlv, 0, sizeof(thread_netdata_6lowpan_id_tlv_t));
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;

    // The 6LoWPAN TLV is an odd case since it is always 2-bytes long. The actual length field of the TLV must always be
    // set to 2
    REQUIRE_OR_EXIT_MSG(tlv_length == 2, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
                        "6LoWPAN ID TLV value must be exactly 2 bytes long (was %lu bytes long)", tlv_length);
    tlv->c = (tlv_value[0] >> 4) & 0x01;
    tlv->cid = (tlv_value[0]) & 0x0F;
    tlv->context_length = tlv_value[1];
exit:
    return exit_status;
}

// there is no decode_commissionning_data_payload function since there is nothing to parse

static thread_netdata_status_t
thread_netdata_decode_service_payload(const uint8_t *NONNULL tlv_value, uint16_t tlv_length,
                                      thread_netdata_service_tlv_t *NONNULL tlv,
                                      thread_version_t thread_version)
{
    memset(tlv, 0, sizeof(thread_netdata_service_tlv_t));
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_SUCCESS;

    REQUIRE_OR_EXIT_MSG(tlv_length >= 2, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
                        "service TLV value must be at least 2 bytes long (was %u bytes long)", tlv_length);

    unsigned offset = 0;
    bool is_thread_enterprise_number = tlv_value[offset] >> 7;
    tlv->s_id = tlv_value[offset] & 0x0F;
    INFO("s_id = %d", tlv->s_id);
    offset += 1;
    if (is_thread_enterprise_number) {
        tlv->enterprise_number = THREAD_ENTERPRISE_NUMBER;
    } else {
        REQUIRE_OR_EXIT_MSG(dns_u32_parse(tlv_value, tlv_length, &offset, &tlv->enterprise_number), THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
                            "service TLV value must be at least 6 bytes long (was %u bytes long)", tlv_length);
    }
    tlv->data_length = tlv_value[offset];
    offset += 1;
    REQUIRE_OR_EXIT_MSG(tlv_length - offset >= tlv->data_length, THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL,
                        "not enough space in the TLV buffer to contain the %u bytes of service data", tlv->data_length);
    tlv->data = &tlv_value[offset];
    offset += tlv->data_length;

    SUCCESS_OR_EXIT(thread_netdata_tlv_iterator_init(&tlv->sub_tlvs_iterator, &tlv_value[offset], tlv_length - offset,
                                                     thread_version));
exit:
    return exit_status;
}

thread_netdata_status_t
thread_netdata_tlv_iterator_next(thread_netdata_tlv_iterator_t *NONNULL iterator, thread_netdata_tlv_t *NONNULL next)
{
    thread_netdata_status_t exit_status;
    size_t tlv_length = 0;
    REQUIRE_OR_EXIT(thread_netdata_tlv_iterator_has_next(iterator), THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL);

    const uint8_t *current_buf = &iterator->buf[iterator->offset];
    next->type = current_buf[0] >> 1;         // the type is stored in the 7 msb of the first byte
    next->stable = (current_buf[0] & 1) != 0; // the stable bit is stored in the lsb of the first byte
    tlv_length = current_buf[1];
    if (iterator->buf_length - (iterator->offset + 2) < tlv_length) {
        ERROR("the provided buffer length is smaller than the decoded TLV length");
        exit_status = THREAD_NETDATA_STATUS_BUFFER_TOO_SMALL;
        goto exit;
    }
    const uint8_t *tlv_value = &current_buf[2];
    switch (next->type) {
        case THREAD_NETDATA_TLV_HAS_ROUTE:
            exit_status = thread_netdata_decode_has_route_payload(tlv_value, tlv_length, &next->has_route);
            break;
        case THREAD_NETDATA_TLV_PREFIX:
            exit_status
                = thread_netdata_decode_prefix_payload(tlv_value, tlv_length, &next->prefix, iterator->thread_version);
            break;
        case THREAD_NETDATA_TLV_BORDER_ROUTER:
            exit_status = thread_netdata_decode_border_router_payload(tlv_value, tlv_length, &next->border_router);
            break;
        case THREAD_NETDATA_TLV_6LOWPAN_ID:
            exit_status = thread_netdata_decode_6lowpan_id_payload(tlv_value, tlv_length, &next->sixlowpan_id);
            break;
        case THREAD_NETDATA_TLV_COMMISSIONING:
            next->commissioning_data.length = tlv_length;
            next->commissioning_data.data = tlv_value;
            exit_status = THREAD_NETDATA_STATUS_SUCCESS;
            break;
        case THREAD_NETDATA_TLV_SERVICE:
            exit_status = thread_netdata_decode_service_payload(tlv_value, tlv_length, &next->service,
                                                                iterator->thread_version);
            break;
        case THREAD_NETDATA_TLV_SERVER:
            exit_status = thread_netdata_decode_server_payload(tlv_value, tlv_length, &next->server);
            break;
        default:
            // we should never reach that
            exit_status = THREAD_NETDATA_STATUS_UNKNOWN_TLV_TYPE;
            goto exit;
    }
    if (exit_status == THREAD_NETDATA_STATUS_SUCCESS) {
        iterator->offset += 2 + tlv_length;
        INFO("decoded %s TLV, length: %lu", thread_netdata_tlv_type_to_string(next->type), tlv_length);
    }
exit:
    if (exit_status != THREAD_NETDATA_STATUS_SUCCESS) {
        ERROR("could not decode TLV of type %s, status code: %d", thread_netdata_tlv_type_to_string(next->type),
              exit_status);
    }
    return exit_status;
}

thread_netdata_status_t
thread_netdata_tlv_iterator_find(thread_netdata_tlv_iterator_t *NONNULL iterator, thread_netdata_tlv_type_t type,
                                 thread_netdata_tlv_t *NONNULL ret) {
    thread_netdata_status_t exit_status = THREAD_NETDATA_STATUS_TLV_NOT_FOUND;
    while (thread_netdata_tlv_iterator_has_next(iterator)) {
        thread_netdata_status_t iteration_status = thread_netdata_tlv_iterator_next(iterator, ret);
        if (iteration_status != THREAD_NETDATA_STATUS_SUCCESS) {
            // an error occured
            exit_status = iteration_status;
            break;
        }
        if (ret->type == type) {
            // found it
            exit_status = THREAD_NETDATA_STATUS_SUCCESS;
            break;
        }
    }
    return exit_status;
}
