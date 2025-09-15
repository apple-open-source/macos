/* srp-decompress.c
 *
 * Copyright (c) 2025 Apple Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * This file contains code to decompress SRP update messages compressed using the OpenThread SRP Coder method.
 */

#ifndef LINUX
#include <netinet/in.h>
#include <net/if.h>
#include <netinet6/in6_var.h>
#include <netinet6/nd6.h>
#include <net/if_media.h>
#include <sys/stat.h>
#else
#define _GNU_SOURCE
#include <netinet/in.h>
#include <fcntl.h>
#include <bsd/stdlib.h>
#include <net/if.h>
#endif
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/route.h>
#include <netinet/icmp6.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stddef.h>
#include <dns_sd.h>
#include <inttypes.h>
#include <signal.h>

#include "srp.h"
#include "dns-msg.h"
#include "ioloop.h"
#include "srp-mdns-proxy.h"
#include "srp-gw.h"
#include "dns-msg.h"
#include "ioloop.h"
#include "srp-proxy.h"
#include "srp-crypto.h"
#include "cti-services.h"
#include "srp-codec.h"
#include "srp-strict.h"

typedef struct srpk_decompress_state {
    dns_label_t *zone;
    dns_label_t *hostname;
    dns_name_pointer_t hostname_pointer;
    dns_name_pointer_t zone_pointer;
    cti_prefix_vec_t *prefixes;
    uint64_t default_ttl;
    uint16_t key_tag;
    bool hostname_emitted;
} srpk_decompress_state_t;

bool
srpk_space_(dns_towire_state_t *NONNULL txn, size_t space, const char *NONNULL file, int line)
{
    if (txn->p + space > txn->lim) {
        ERROR("Need %" PRIxPTR ", have %" PRIxPTR ": no space in transaction at " PUB_S_SRP ":%d",
              space, txn->lim - txn->p, file, line);
        return false;
    }
    return true;
}

#define srpk_integer_from_wire_max(txn, ret, first_mask, left_bit) \
    srpk_integer_from_wire_max_(txn, ret, first_mask, left_bit, __FILE__, __LINE__)

// Encode a compressed integer. val is the integer to encode. first_max must be a power of two, and is one greater than
// the largest value that can be encoded in the first byte. left_or is or'd to the first byte but no other byte.
static bool
srpk_integer_from_wire_max_(dns_towire_state_t *NONNULL txn, uint64_t *ret, uint8_t first_mask, uint8_t left_bit, const char *file, int line)
{
    uint8_t mask = first_mask;
    uint8_t continue_bit = left_bit;
    uint64_t rv = 0;
    bool cont;
    do {
        uint64_t byte = (*txn->p & mask);
        cont = (*txn->p & continue_bit) ? true : false;
        rv = (rv << 7) | byte; // The first time through this is a no-op, and it's always 7 after that.
        SRPK_DEBUG("rv = %" PRIu64 "  byte = %" PRIx64"  mask = %x", rv, byte, mask);

        mask = 0x7f;
        continue_bit = 0x80;
        if (cont && !srpk_space_(txn, 1, file, line)) {
            return false;
        }
        txn->p++;
    } while (cont);
    *ret = rv;
    return true;
}

#define srpk_integer_from_wire(txn, ret) srpk_integer_from_wire_(txn, ret, __FILE__, __LINE__)

static bool
srpk_integer_from_wire_(dns_towire_state_t *NONNULL txn, uint64_t *ret, const char *file, int line)
{
    return srpk_integer_from_wire_max_(txn, ret, 0x7f, 0x80, file, line);
}

// Convert 8-byte binary string to NUL-terminated 16-byte hex string; caller is responsible for validating string and bounds check
#define srpk_hex_from_wire(in, out) srpk_hex_from_wire_(in, out)
static void
srpk_hex_from_wire_(uint8_t *in, char *out)
{
    uint8_t *inp = in;
    char *outp = (char *)out;
    for (int i = 0; i < 8; i++) {
        snprintf(outp, 3, "%02X", *inp);
        outp += 2;
        inp++;
    }
}

static void
srpk_make_name_pointer(dns_towire_state_t *NONNULL out, dns_name_pointer_t *pointer)
{
    memset(pointer, 0, sizeof(*pointer));
    pointer->message_start = (uint8_t *)out->message;
    pointer->name_start = out->p;
}

#define srpk_label_from_wire(txn, ret) srpk_label_from_wire_(txn, ret, __FILE__, __LINE__)
static bool
srpk_label_from_wire_(dns_towire_state_t *NONNULL txn, dns_label_t *NONNULL *NULLABLE ret, const char *file, int line)
{
    char label_buf[128];
    size_t label_len = 0;
    const char *label_pointer = label_buf;
    srpk_back_pointer_t *back_pointer = NULL;

    // Don't proceed if we already hit an error.
    if (txn->error) {
        return false;
    }
    // We need at least a dispatch byte.
    if (!srpk_space(txn, 1)) {
        return false;
    }

    // First look for labels we abbreviate directly (0b110*) since we will never use a back pointer for these
    if ((*txn->p & SRPK_LABEL_3BIT_MASK) == SRPK_LABEL_ABBREV_MARKER) {
        uint8_t code = *txn->p++;
        switch(code) {
        case SRPK_LABEL_UDP:
            label_len = 4;
            label_pointer = "_udp";
            break;
        case SRPK_LABEL_TCP:
            label_len = 4;
            label_pointer = "_tcp";
            break;
        case SRPK_LABEL_HAP:
            label_len = 4;
            label_pointer = "_hap";
            break;
        case SRPK_LABEL_MATTER:
            label_len = 7;
            label_pointer = "_matter";
            break;
        case SRPK_LABEL_MATTERC:
            label_len = 8;
            label_pointer = "_matterc";
            break;
        case SRPK_LABEL_MATTERD:
            label_len = 8;
            label_pointer = "_matterd";
            break;
        default:
            ERROR(PUB_S_SRP ":%d: unrecognized direct abbreviation %x", file, line, code);
            txn->error = EINVAL;
            txn->line = line;
            return false;
        }
        SRPK_DEBUG("abbreviation: " PUB_S_SRP, label_pointer);
        goto return_label;
    }

    // Next look for a straight pointer back to a previous label.
    if ((*txn->p & SRPK_LABEL_2BIT_MASK) == SRPK_LABEL_POINTER) {
        uint64_t offset;
        SRPK_DEBUG("pointer to label: %x", *txn->p);
        if (!srpk_integer_from_wire_max(txn, &offset, 63, 64)) {
            return false;
        }
        uint64_t pre_offset = txn->lim - (((uint8_t *)&txn->message) + DNS_HEADER_SIZE);
        uint64_t next_offset = 0;
        for (srpk_back_pointer_t *bptr = txn->back_pointers; bptr != NULL; bptr = bptr->next) {
            if (bptr->offset == offset) {
                if (!dns_label_create(ret, bptr->label->data, bptr->label->len)) {
                    txn->error = ENOMEM;
                    txn->line = line;
                    return false;
                }
                SRPK_DEBUG("back pointer match");
                return true;
            }
            // bptrs are in reverse order by offset, so if this offset should be next, but isn't, it's an error.
            if (bptr->next != NULL) {
                next_offset = bptr->next->offset;
            } else {
                next_offset = 0;
            }
            pre_offset = bptr->offset;
            if (pre_offset > offset && next_offset < offset) {
            missing:
                ERROR(PUB_S_SRP ":%d: missing back pointer %" PRIu64
                      " (between %" PRIu64 " and %" PRIu64 ") doesn't point to a label.",
                      file, line, offset, pre_offset, next_offset);
                txn->error = EINVAL;
                txn->line = line;
                return false;
            }
        }
        SRPK_DEBUG("back pointer missing for label pointer");
        goto missing; // If we get here the pointer is missing, which is always an error.
    }

    // A literal label with the underline excluded
    bool underline_label = false;
    if ((*txn->p & SRPK_LABEL_2BIT_MASK) == SRPK_LABEL_UNDERLINE) {
        underline_label = true;
    }

    if (underline_label || (*txn->p & SRPK_LABEL_2BIT_MASK) == SRPK_LABEL_LITERAL) {
        back_pointer = srpk_label_cache(txn, NULL, NULL,
                                        underline_label ? bptr_type_underline_label : bptr_type_literal_label);
        if (back_pointer == NULL) {
            txn->error = ENOMEM;
            txn->line = line;
            return false;
        }

        SRPK_DEBUG(PUB_S_SRP " %x", underline_label ? "underline label" : "normal label", *txn->p);

        // Get label length
        uint64_t len;
        if (!srpk_integer_from_wire_max(txn, &len, 31, 32)) {
            return false;
        }

        if (len > DNS_MAX_LABEL_SIZE) {
            ERROR("length > 63 (%" PRIu64 ") at %d", len, line);
            txn->error = E2BIG;
            txn->line = line;
            return false;
        }

        if (!srpk_space(txn, len)) {
            return false;
        }

        char *buf = label_buf;
        if (underline_label) {
            *buf++ = '_';
            memcpy(buf, txn->p, len);
            txn->p += len;
            len++;
        } else {
            memcpy(buf, txn->p, len);
            txn->p += len;
        }
        label_pointer = (char *)label_buf;
        label_len = len;
        SRPK_DEBUG("literal label");
        goto return_label;
    }

    if ((*txn->p & SRPK_LABEL_3BIT_MASK) == SRPK_LABEL_GENERATIVE_PATTERN) {
        int gen_type = *txn->p;

        if (gen_type != SRPK_LABEL_UNDERLINE_CHAR_PTR) {
            back_pointer = srpk_label_cache(txn, NULL, NULL, underline_label ? bptr_type_underline_label : bptr_type_literal_label);
            if (back_pointer == NULL) {
                txn->error = ENOMEM;
                txn->line = line;
                return false;
            }
        }
        txn->p++;

        uint8_t len, inlen;
        uint64_t offset = 0;
        switch(gen_type) {
        case SRPK_LABEL_SINGLE_HEX:
            len = 16;
            inlen = 8;
            break;
        case SRPK_LABEL_DOUBLE_HEX:
            len = 33;
            inlen = 16;
            break;
        case SRPK_LABEL_UNDERLINE_CHAR_HEX:
            inlen = 9;
            len = 18;
            break;
        case SRPK_LABEL_UNDERLINE_CHAR_PTR:
            inlen = 1;
            len = 18;
            break;
        default:
            ERROR("Unexpected generative pattern type %d", gen_type & ~SRPK_LABEL_3BIT_MASK);
            txn->error = EINVAL;
            txn->line = line;
            return false;
        }

        if (!srpk_space(txn, inlen)) {
            txn->error = ENOSPC;
            txn->line = line;
            return false;
        }
        char *bufp = label_buf;
        if (gen_type == SRPK_LABEL_UNDERLINE_CHAR_PTR) {
            SRPK_DEBUG("underline char ptr %x", *txn->p);
            char the_char = *txn->p++;
            if (!srpk_integer_from_wire(txn, &offset)) {
                txn->error = EINVAL;
                txn->line = line;
                return false;
            }
            uint8_t *hex_pointer = ((uint8_t *)txn->message + offset);
            if (hex_pointer + 8 >= txn->p) {
                ERROR("bogus underline-char-ptr offset %" PRIu64, offset);
                txn->error = EINVAL;
                txn->line = line;
                return false;
            }
            *bufp++ = '_';
            *bufp++ = the_char;
            srpk_hex_from_wire(hex_pointer, bufp); // The hex string
        } else {
            if (gen_type == SRPK_LABEL_UNDERLINE_CHAR_HEX) {
                SRPK_DEBUG("underline char hex %x", *txn->p);
                *bufp++ = '_';
                *bufp++ = *txn->p++; // The character.
            }
            srpk_hex_from_wire(txn->p, bufp); // The hex string
            bufp += 16;
            txn->p += 8;
            if (gen_type == SRPK_LABEL_DOUBLE_HEX) {
                *bufp++ = '-';
                srpk_hex_from_wire(txn->p, bufp); // The hex string
                txn->p += 8;
            }
        }
        label_pointer = label_buf;
        label_len = len;
        SRPK_DEBUG("generative pattern inlen %d len %d", inlen, len);
        goto return_label;
    }
    SRPK_DEBUG("falling through");
return_label:
    if (label_pointer != NULL) {
        dns_label_t *label;
        if (!dns_label_create(&label, label_pointer, label_len)) {
            txn->error = ENOMEM;
            txn->line = line;
            return false;
        }
        *ret = label;
        if (back_pointer != NULL) {
            back_pointer->label = label;
        }
        SRPK_DEBUG("resulting label is " PRI_S_SRP, label->data);
    } else {
        SRPK_DEBUG("no resulting label");
    }
    return true;
}

static void
srpk_name_to_wire(dns_towire_state_t *out, dns_label_t *name)
{
    // Emit all the labels in the name. We do not emit a root label here--that's up to the caller.
    for (dns_label_t *label = name; label != NULL; label = label->next) {
        dns_u8_to_wire(out, label->len);
        dns_rdata_raw_data_to_wire(out, label->data, label->len);
    }
}

static void
srpk_hostname_to_wire(dns_towire_state_t *out, srpk_decompress_state_t *state)
{
    if (state->hostname_emitted) {
        dns_pointer_to_wire(NULL, out, &state->hostname_pointer);
    } else {
        srpk_make_name_pointer(out, &state->hostname_pointer);
        srpk_name_to_wire(out, state->hostname);
        dns_pointer_to_wire(NULL, out, &state->zone_pointer);
        state->hostname_emitted = true;
    }
}

// See if this is a host instruction; if so, emit a host block and return the number of records consumed.
static int
srpk_decompress_host_block(dns_towire_state_t *txn, dns_towire_state_t *out, uint8_t dispatch, srpk_decompress_state_t *state)
{
    char name_buf_1[DNS_MAX_NAME_SIZE_ESCAPED];
    uint64_t host_ttl = state->default_ttl;
    uint64_t key_ttl = state->default_ttl; // Don't panic: this is the TTL, not the lease.
    int records_emitted = 0;
    dns_name_pointer_t hostname_pointer;

    srpk_space(txn, 1);
    srpk_make_name_pointer(out, &hostname_pointer);
    srpk_hostname_to_wire(out, state);

    // Emit the "delete all records on name" update instruction
    dns_u16_to_wire(out, dns_rrtype_any);
    dns_u16_to_wire(out, dns_qclass_any);
    dns_ttl_to_wire(out, 0);
    dns_u16_to_wire(out, 0);
    records_emitted++;

    // Get the AAAA record TTL?
    if (dispatch & SRPK_HOST_DISPATCH_AT) {
        // This is an error, but is not fatal. Worth logging, at least.
        if (!(dispatch & SRPK_HOST_DISPATCH_ADR)) {
            ERROR(PRI_S_SRP ": host TTL provided when there are no address records.",
                  dns_name_print(state->hostname, name_buf_1, sizeof(name_buf_1)));
        }
        if (!srpk_integer_from_wire(txn, &host_ttl)) {
            ERROR(PRI_S_SRP ": malformed host ttl", dns_name_print(state->hostname, name_buf_1, sizeof(name_buf_1)));
            txn->error = EINVAL;
            txn->line = __LINE__;
            return -1;
        }
    }

    // Are there addresses?
    if (dispatch & SRPK_HOST_DISPATCH_ADR) {
        do {
            // Get space for address dispatch byte
            if (!srpk_space(txn, 1)) {
                return -1;
            }
            uint8_t address_dispatch = *txn->p++;
            struct in6_addr addr;
            bool got_prefix = false;
            if (address_dispatch & SRPK_ADDRESS_DISPATCH_CC) {
                if (!srpk_space(txn, 8)) {
                    return -1;
                }
                uint8_t context_id = address_dispatch & SRPK_ADDRESS_DISPATCH_CID_MASK;
                if (state->prefixes == NULL) {
                    ERROR("Context ID %d present but no prefixes", context_id);
                    return -1;
                }
                for (size_t i = 0; i < state->prefixes->num; i++) {
                    cti_prefix_t *prefix = state->prefixes->prefixes[i];
                    if (prefix != NULL && prefix->has_6lowpan_context &&
                        prefix->thread_6lowpan_context.cid == context_id) {
                        memcpy(&addr, &prefix->prefix, 8);
                        memcpy(((uint8_t *)&addr) + 8, txn->p, 8);
                        txn->p += 8;
                        got_prefix = true;
                        break;
                    }
                }
                if (!got_prefix) {
                    ERROR("invalid prefix context id %d", context_id);
                    txn->error = ENOENT;
                    txn->line = __LINE__;
                    return -1;
                }
            } else {
                if (!srpk_space(txn, 16)) {
                    return -1;
                }
                memcpy(&addr, &txn->p, 16);
                txn->p += 16;
            }
            // Write an AAAA RR.
            dns_pointer_to_wire(NULL, out, &state->hostname_pointer);
            dns_u16_to_wire(out, dns_rrtype_aaaa);
            dns_u16_to_wire(out, dns_qclass_in);
            dns_ttl_to_wire(out, (uint32_t)host_ttl);
            dns_u16_to_wire(out, 16);
            dns_rdata_raw_data_to_wire(out, (uint8_t *)&addr, 16);
            records_emitted++;

            if (!(address_dispatch & SRPK_ADDRESS_DISPATCH_MORE)) {
                break;
            }
        } while (true);
    }

    // Get the KEY TTL if provided
    if (dispatch & SRPK_HOST_DISPATCH_KT) {
        if (!srpk_integer_from_wire(txn, &key_ttl)) {
            ERROR(PRI_S_SRP ": malformed key ttl", dns_name_print(state->hostname, name_buf_1, sizeof(name_buf_1)));
            txn->error = EINVAL;
            txn->line = __LINE__;
            return -1;
        }
    }

    // construct the KEY RR
    dns_pointer_to_wire(NULL, out, &hostname_pointer);
    dns_u16_to_wire(out, dns_rrtype_key);
    dns_u16_to_wire(out, dns_qclass_in);
    dns_ttl_to_wire(out, (uint32_t)key_ttl);
    dns_rdlength_begin(out);
    uint8_t *rdata = out->p;
    if (!srpk_space(out, ECDSA_KEY_SIZE + 4)) {
        return -1;
    }
    if (!srpk_space(txn, ECDSA_KEY_SIZE)) {
        return -1;
    }
    *out->p++ = 2; // name type is 2, key type is 0
    *out->p++ = 1; // signatory is 1
    *out->p++ = 3; // protocol type is always 3
    *out->p++ = dnssec_keytype_ecdsa; // Always ECDSA
    dns_rdata_raw_data_to_wire(out, txn->p, ECDSA_KEY_SIZE); // Copy the key
    txn->p += ECDSA_KEY_SIZE;
    state->key_tag = dns_key_tag_compute(out->p - rdata, rdata);
    dns_rdlength_end(out);

    records_emitted++;
    return records_emitted;
}

static bool
srpk_decompress_instance_labels(dns_towire_state_t *txn, dns_towire_state_t *out, srpk_decompress_state_t *state,
                                bool remove, dns_name_pointer_t *service_pointer_ret,
                                dns_name_pointer_t *instance_pointer_ret, uint64_t ptr_ttl)
{
    dns_label_t *service_name = NULL, *instance_name = NULL;
    // Read the first label of the service instance name
    if (!srpk_label_from_wire(txn, &instance_name)) {
        return false;
    }
    // Read the service name (two labels)
    if (!srpk_label_from_wire(txn, &service_name)) {
        return false;
    }
    if (!srpk_label_from_wire(txn, &service_name->next)) {
        return false;
    }

    // Emit the service name
    dns_name_pointer_t service_name_pointer;

    srpk_make_name_pointer(out, &service_name_pointer);
    if (service_pointer_ret != NULL) {
        *service_pointer_ret = service_name_pointer;
    }

    // Emit the service name for the PTR record
    srpk_name_to_wire(out, service_name);
    dns_pointer_to_wire(NULL, out, &state->zone_pointer);

    // Emit the "delete all records on name" update instruction
    dns_u16_to_wire(out, dns_rrtype_ptr);
    dns_u16_to_wire(out, remove ? dns_qclass_none : dns_qclass_in);
    dns_ttl_to_wire(out, (uint32_t)ptr_ttl);
    dns_rdlength_begin(out);

    // We'll need a pointer to the service instance name
    dns_name_pointer_t instance_name_pointer;
    srpk_make_name_pointer(out, &instance_name_pointer);
    if (instance_pointer_ret != NULL) {
        *instance_pointer_ret = instance_name_pointer;
    }

    // Emit the service instance name label, followed by a pointer back to the service name we just emitted for the PTR
    // record delete update.
    srpk_name_to_wire(out, instance_name);
    dns_pointer_to_wire(NULL, out, &service_name_pointer);
    dns_rdlength_end(out);

    return out->error ? false : true;
}

static void
srpk_write_instance_delete(dns_towire_state_t *out, dns_name_pointer_t *instance_pointer)
{
    // Now emit a "delete all records on a name" update for the service instance name
    dns_pointer_to_wire(NULL, out, instance_pointer);
    dns_u16_to_wire(out, dns_rrtype_any);
    dns_u16_to_wire(out, dns_qclass_any);
    dns_ttl_to_wire(out, 0);
    dns_u16_to_wire(out, 0);
}

static int
srpk_decompress_service_remove_block(dns_towire_state_t *txn, dns_towire_state_t *out, srpk_decompress_state_t *state)
{
    dns_name_pointer_t instance_pointer;

    if (txn->error || out->error) {
        return -1;
    }

    // Parse the service instance name label and the service name labels.
    if (!srpk_decompress_instance_labels(txn, out, state, true, NULL, &instance_pointer, 0)) {
        return -1;
    }

    srpk_write_instance_delete(out, &instance_pointer);
    if (out->error) {
        return -1;
    }
    return 2;
}

static bool
srpk_decompress_txt_data(dns_towire_state_t *txn, dns_towire_state_t *out)
{
    srpk_space(txn, 1); // Dispatch byte
    if (*txn->p & SRPK_TXT_DISPATCH_OFFSET) {
        uint64_t offset;
        if (!srpk_integer_from_wire_max(txn, &offset, 63, 64)) {
            return false;
        }
        // Offset should be the offset of an actual back pointer.
        for (srpk_back_pointer_t *bptr = txn->back_pointers; bptr != NULL; bptr = bptr->next) {
            SRPK_DEBUG("txt record back-pointer offset: %u %" PRIu64, bptr->offset, offset);
            if (bptr->offset == offset) {
                if (bptr->txt_data == NULL) {
                    ERROR("txt record back pointer is not to a txt record.");
                    txn->error = EINVAL;
                    txn->line = __LINE__;
                    return false;
                }
                dns_rdata_raw_data_to_wire(out, bptr->txt_data, bptr->txt_len);
                return true;
            }
        }
        ERROR("no match.");
        txn->error = ENOENT;
        txn->line = __LINE__;
        return false;
    }
    SRPK_DEBUG("literal txt record");
    srpk_back_pointer_t *bptr = srpk_label_cache(txn, NULL, NULL, bptr_type_txt_record);
    uint64_t length;
    if (!srpk_integer_from_wire_max(txn, &length, 63, 64)) {
        return false;
    }
    if (!srpk_space(txn, length)) {
        return false;
    }
    bptr->txt_len = length;
    bptr->txt_data = txn->p;
    dns_rdata_raw_data_to_wire(out, txn->p, length);
    txn->p += length;
    return true;
}

static int
srpk_decompress_service_add_block(dns_towire_state_t *txn, dns_towire_state_t *out, uint8_t dispatch,
                                  srpk_decompress_state_t *state)
{
    int num_emitted = 2;
    uint64_t ptr_ttl = state->default_ttl;
    uint64_t srv_txt_ttl = state->default_ttl;
    dns_name_pointer_t service_pointer, instance_pointer;

    // Get all the flags for stuff that might or might not be present.
    bool ptr_ttl_present = (dispatch & SRPK_SERVICE_ADD_DISPATCH_PT) ? true : false;
    bool srv_txt_ttl_present = (dispatch & SRPK_SERVICE_ADD_DISPATCH_ST) ? true : false;
    bool subtypes_present = (dispatch & SRPK_SERVICE_ADD_DISPATCH_SUB) ? true : false;
    bool priority_present = (dispatch & SRPK_SERVICE_ADD_DISPATCH_PRI) ? true : false;
    bool weight_present = (dispatch & SRPK_SERVICE_ADD_DISPATCH_WGT) ? true : false;
    bool txt_present = (dispatch & SRPK_SERVICE_ADD_DISPATCH_TXT) ? true : false;

    SRPK_DEBUG("dispatch: %x", dispatch);

    // Fetch TTLs if present
    if (ptr_ttl_present) {
        srpk_integer_from_wire(txn, &ptr_ttl);
    }

    // Emit SRV/TXT TTL
    if (srv_txt_ttl_present) {
        srpk_integer_from_wire(txn, &srv_txt_ttl);
    }

    // Emit the PTR record and get a pointer to the instance name
    srpk_decompress_instance_labels(txn, out, state, false, &service_pointer, &instance_pointer, ptr_ttl);

    if (subtypes_present) {
        if (!srpk_space(txn, 1)) {
            return -1;
        }
        do {
            dns_label_t *subtype = NULL;
            if (!srpk_label_from_wire(txn, &subtype)) {
                return -1;
            }

            // Write out the name
            srpk_name_to_wire(out, subtype);
            dns_name_to_wire(NULL, out, "_sub");
            dns_pointer_to_wire(NULL, out, &service_pointer);

            // Write out the PTR record
            dns_u16_to_wire(out, dns_rrtype_ptr);
            dns_u16_to_wire(out, dns_qclass_in);
            dns_ttl_to_wire(out, (uint32_t)ptr_ttl);
            dns_rdlength_begin(out);
            dns_pointer_to_wire(NULL, out, &instance_pointer);
            dns_rdlength_end(out);
            num_emitted++;

            if (!srpk_space(txn, 1)) {
                return -1;
            }
        } while (*txn->p != 0);
        txn->p++; // Consume the root label
    }
    uint64_t port, priority = 0, weight = 0;
    if (!srpk_integer_from_wire(txn, &port)) {
        return -1;
    }
    if (priority_present && !srpk_integer_from_wire(txn, &priority)) {
        return -1;
    }
    if (weight_present && !srpk_integer_from_wire(txn, &weight)) {
        return -1;
    }

    // Emit delete all records on name
    dns_pointer_to_wire(NULL, out, &instance_pointer);
    dns_u16_to_wire(out, dns_rrtype_any);
    dns_u16_to_wire(out, dns_qclass_any);
    dns_ttl_to_wire(out, 0);
    dns_u16_to_wire(out, 0); // length 0

    // Emit the SRV record.
    dns_pointer_to_wire(NULL, out, &instance_pointer);
    dns_u16_to_wire(out, dns_rrtype_srv);
    dns_u16_to_wire(out, dns_qclass_in);
    dns_ttl_to_wire(out, (uint32_t)srv_txt_ttl);
    dns_rdlength_begin(out);
    dns_u16_to_wire(out, (uint16_t)priority);
    dns_u16_to_wire(out, (uint16_t)weight);
    dns_u16_to_wire(out, (uint16_t)port);
    srpk_hostname_to_wire(out, state);
    dns_rdlength_end(out);
    num_emitted++;

    // Emit the TXT record.
    dns_pointer_to_wire(NULL, out, &instance_pointer);
    dns_u16_to_wire(out, dns_rrtype_txt);
    dns_u16_to_wire(out, dns_qclass_in);
    dns_ttl_to_wire(out, (uint32_t)srv_txt_ttl);
    dns_rdlength_begin(out);
    // If there is no TXT data present, we output a zero-length TXT record.
    if (txt_present) {
        srpk_decompress_txt_data(txn, out);
    }
    dns_rdlength_end(out);
    num_emitted++;

    if (txn->error || out->error) {
        return -1;
    }
    return num_emitted;
}

static bool
srpk_full_name_from_wire(dns_towire_state_t *txn, dns_label_t **ret)
{
    dns_label_t **progress = ret;
    if (!srpk_space(txn, 1)) {
        return false;
    }
    while (*txn->p != 0) {
        if (!srpk_label_from_wire(txn, progress)) {
            return false;
        }
        progress = &((*progress)->next);
        if (!srpk_space(txn, 1)) {
            return false;
        }
    }
    txn->p++;
    return true;
}
// Take a parsed SRP message and try to write it as a compressed message.
message_t *
srpk_message_decompress(message_t *compressed, cti_prefix_vec_t *prefixes)
{
    dns_towire_state_t out, txn;
#define BIG_MESSAGE 10000

    // Not long enough to check for compression flag
    if (compressed->length < 3) {
        return NULL;
    }
    // Not a compressed update
    if ((((uint8_t *)&compressed->wire)[2] & SRPK_COMPRESSION_DISPATCH_MASK) != SRPK_COMPRESSION_DISPATCH_CODE) {
        return NULL;
    }

    memset(&txn, 0, sizeof(txn));
    txn.message = &compressed->wire;
    txn.p = (uint8_t *)txn.message;
    txn.lim = txn.p + compressed->length;

    uint8_t *message_buf = srp_strict_malloc(BIG_MESSAGE); // Lots of space, hopefully not necessary.
    message_t *ret = NULL;
    dns_wire_t *message;

    if (message_buf == NULL) {
        goto out;
    }

    memset(&out, 0, sizeof(out));
    out.message = message = (dns_wire_t *)message_buf;
    out.p = &message->data[0];               // We start storing RR data here.
    out.lim = message_buf + BIG_MESSAGE;

    // Caller has to have ensured that the xid and the dispatch byte are present.
    // Set up the message header
    if (!srpk_space(&txn, 3)) {
        goto out;
    }
    message->id = txn.message->id;
    message->bitfield = 0;
    dns_qr_set(message, dns_qr_query);
    dns_opcode_set(message, dns_opcode_update);
    message->qdcount = htons(1); // The zone

    uint8_t header_dispatch = txn.p[2];
    txn.p += 3;

    srpk_decompress_state_t state;
    memset(&state, 0, sizeof (state));
    state.default_ttl = 7200;
    state.prefixes = prefixes;

    // Read in the zone
    if (header_dispatch & SRPK_COMPRESSION_DISPATCH_ZP) {
        if (!srpk_full_name_from_wire(&txn, &state.zone)) {
            goto out;
        }
    } else {
        state.zone = dns_pres_name_parse("default.service.arpa.");
    }

    if (header_dispatch & SRPK_COMPRESSION_DISPATCH_TP) {
        if (!srpk_integer_from_wire(&txn, &state.default_ttl)) {
            goto out;
        }
    }

    if (!srpk_full_name_from_wire(&txn, &state.hostname)) {
        goto out;
    }

    // Emit the zone (it's formed as a question, so no TTL or length or data.
    srpk_make_name_pointer(&out, &state.zone_pointer);
    srpk_name_to_wire(&out, state.zone);
    dns_u16_to_wire(&out, dns_rrtype_soa);
    dns_u16_to_wire(&out, dns_qclass_in);

    if (!srpk_space(&txn, 1)) {
        goto out;
    }

    int records_emitted = 0;
    while ((*txn.p & SRPK_COMPRESSION_FOOTER_MASK) != SRPK_COMPRESSION_FOOTER_MARK) {
        uint8_t block_dispatch = *txn.p++;
        int records_uncompressed = 0;

        switch (block_dispatch & SRPK_COMPRESSION_BLOCK_MASK) {
        case SRPK_HOST_DISPATCH_BLOCK:
            records_uncompressed = srpk_decompress_host_block(&txn, &out, block_dispatch, &state);
            break;

        case SRPK_REMOVE_SERVICE_DISPATCH_BLOCK:
            records_uncompressed = srpk_decompress_service_remove_block(&txn, &out, &state);
            break;

        case SRPK_SERVICE_ADD_DISPATCH_BLOCK:
            records_uncompressed = srpk_decompress_service_add_block(&txn, &out, block_dispatch, &state);
            break;

        default:
            ERROR("Unknown dispatch byte %x", block_dispatch);
            goto out;
        }

        // If there was a host instruction, but it was in the wrong order, we can't compress.
        if (records_uncompressed < 0) {
            goto out;
        }

        // We successfully emitted something, don't care what, advance past it.
        records_emitted += records_uncompressed;

        if (!srpk_space(&txn, 1)) {
            goto out;
        }
    }
    message->ancount = 0; // No prerequisites
    message->nscount = htons(records_emitted);

    // Footer dispatch byte, we already accounted for it before exiting the loop.
    uint8_t footer_dispatch = *txn.p++;

    // We only support the full signature.
    if ((footer_dispatch & SRPK_COMPRESSION_FOOTER_SIGM) != SRPK_COMPRESSION_FOOTER_SIGF) {
        ERROR("Unsupported signature type: %d", footer_dispatch & SRPK_COMPRESSION_FOOTER_SIGM);
        goto out;
    }

    // For now we always include full signature, since we haven't defined short signature.
    uint64_t host_lease = 0;
    if (footer_dispatch & SRPK_COMPRESSION_FOOTER_LP) {
        if (!srpk_integer_from_wire(&txn, &host_lease)) {
            goto out;
        }
    }
    uint64_t key_lease = 0;
    if (footer_dispatch & SRPK_COMPRESSION_FOOTER_KLP) {
        if (!srpk_integer_from_wire(&txn, &key_lease)) {
            goto out;
        }
    }

    // Emit an EDNS0 RR with the update lease option
    dns_edns0_header_to_wire(&out, DNS_MAX_UDP_PAYLOAD, 0, 0, 1);
    dns_rdlength_begin(&out);
    dns_u16_to_wire(&out, dns_opt_update_lease);
    dns_edns0_option_begin(&out);
    dns_u32_to_wire(&out, (uint32_t)host_lease);
    dns_u32_to_wire(&out, (uint32_t)key_lease);
    dns_edns0_option_end(&out);
    dns_rdlength_end(&out);

    // Emit the signature.
    dns_u8_to_wire(&out, 0);	// root label
    dns_u16_to_wire(&out, dns_rrtype_sig);
    dns_u16_to_wire(&out, dns_qclass_any); // class
    dns_ttl_to_wire(&out, 0); // SIG RR TTL
    dns_rdlength_begin(&out);
    dns_u16_to_wire(&out, 0); // type = 0 for transaction signature
    dns_u8_to_wire(&out, dnssec_keytype_ecdsa); // Algorithm agility? HAH!
    dns_u8_to_wire(&out, 0); // labels field doesn't apply for transaction signature
    dns_ttl_to_wire(&out, 0); // original ttl doesn't apply
    if (footer_dispatch & SRPK_COMPRESSION_FOOTER_TRP) {
        uint32_t start, end;
        if (!srpk_space(&txn, 4)) {
            goto out;
        }
        unsigned offset = 0;
        dns_u32_parse(txn.p, (unsigned)(txn.lim - txn.p), &offset, &start);
        dns_u32_parse(txn.p, (unsigned)(txn.lim - txn.p), &offset, &end);
        txn.p += offset;
        dns_u32_to_wire(&out, (uint32_t)start);
        dns_u32_to_wire(&out, (uint32_t)end);
    } else {
        dns_u32_to_wire(&out, 0); // Indicate that we have no clock: set expiry and inception times to zero
        dns_u32_to_wire(&out, 0);
    }
    dns_u16_to_wire(&out, state.key_tag);
    dns_pointer_to_wire(NULL, &out, &state.hostname_pointer);
    if (!srpk_space(&txn, ECDSA_SHA256_SIG_SIZE) || !srpk_space(&out, ECDSA_SHA256_SIG_SIZE)) {
        goto out;
    }
    dns_rdata_raw_data_to_wire(&out, txn.p, ECDSA_SHA256_SIG_SIZE);
    dns_rdlength_end(&out);

    message->arcount = htons(2); // EDNS0 and SIG(0)

#ifdef SRP_TEST_SERVER
    for (srpk_back_pointer_t *bptr = out.back_pointers; bptr != NULL; bptr = bptr->next) {
        const char *label = bptr->label != NULL ? bptr->label->data : "<no label>";
        INFO("bptr label %s  txt %p  line %d  type %d  data %p  offset %d", label, bptr->txt_record, bptr->line, bptr->type, bptr->data, bptr->offset);
    }
#endif

    // We now have a fully decompressed message. Allocate a message_t
    ret = ioloop_message_create(out.p - message_buf);
    if (ret == NULL) {
        ERROR("No memory for message!");
        goto out;
    }
    memcpy(&ret->wire, message_buf, out.p - message_buf);

out:
    srp_strict_free(&message_buf);
    return ret;
}

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 120
// indent-tabs-mode: nil
// End:
