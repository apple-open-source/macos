/* srp-compression.c
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
 * This file contains code to compress SRP update messages using the OpenThread
 * SRP Coder method.
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

#define SRPK_PUT(func, towire, ...)      \
    do {                                 \
      const uint8_t *put_start = txn->p; \
      func(towire, ##__VA_ARGS__);       \
      srpk_hex_dump(#func ": ", put_start - (uint8_t *)&towire->message, put_start, towire->p - put_start); \
    } while (0)

void
srpk_hex_dump_(const char *UNUSED label, size_t offset, const uint8_t *bytes, ssize_t length,
               const char *file, int line)
{
	char buf[200];
	for (ssize_t i = 0; i < length; i += 32) {
		char *bp = buf;
		char *lim = buf + sizeof(buf);
		for (ssize_t j = 0; j < 32 && i + j < length; j += 4) {
			if (bp < lim && j != 0) {
				*bp++ = ' ';
			}
			for (ssize_t k = 0; k < 4 && i + j + k < length; k++) {
				snprintf(bp, lim - bp, "%02x", bytes[i + j + k]);
                bp += 2;
			}
		}
		if (bp == lim) {
			--bp;
		}
		*bp = 0;
        const char *fonly = strrchr(file, '/');
        if (fonly == NULL) {
            fonly = file;
        } else {
            fonly++;
        }
		INFO("%s:%d: %03" PRIxPTR ": " PUB_S_SRP, fonly, line, i + offset, buf);
	}
}

// Flow:
// Check header: is bit five set and opcode = 5? If no, no decompression to do
// If yes:
// -- Outer dispatch byte check:
//    -- Add service check -> parse add service block
//    -- Remvoe service check -> parse remove service block
//    -- Host check -> parse host block
//    -- Footer check -> parsefooter block
//
//    -- Parse add service block:
//       if PTR TTL, decode compact integer -> PTR TTL, else 7200 -> PTR TTL
//       if SRV/TXT TTL, decode compact integer -> SRV/TXT TTL, else 7200 -> PTR TTL
//       parse labels ending in 0 label -> service instance name
//       parse service name labels
//       if subtype, parse labels, stopping at 0 label, constructing list of subtype PTRs:
//          each subtype has owner name of <parsed label>._sub.<service instance name> and target
//          of <service instance name>
//       parse compact integer -> SRV port
//       if priority flag, parse compact integer -> SRV priority else 0->priority
//       if weight flag, parse compact integer -> SRV weight
//       if TXT flag, parse TXT data block
//       -> one or more PTR records, one of which will be on the service, all the rest of which will be subtypes
//       -> one SRV record, on the service instance
//       -> one TXT record, on the service instance
//
//    -- Parse remove service block:
//       parse labels to 0 label -> service instance name
//       second label of service instance name -> service name
//
//    -- Parse host block:
//

// Emit a compressed integer
//
// - Works with unsigned integer of different length (`uint16`, `uint32`)
// - Number is encoded as one or more segment(s).
// - Segments are one byte (8-bit) long except for the first segment which may have fewer bits (2-7 bits).
// - First (MSB) bit in each segment is the “continuation bit” indicating whether there are more segments to follow
//   (`1`) or if it is the last segment (`0`).
// - The remaining bits after the MSB provide the numerical `uint` bit values in big-endian order.

#define srpk_integer_to_wire_max(txn, val, first_max, left_or) \
    srpk_integer_to_wire_max_(txn, val, first_max, left_or, __LINE__)

// Encode a compressed integer. val is the integer to encode. first_max must be a power of two, and is one greater than
// the largest value that can be encoded in the first byte. left_or is or'd to the first byte but no other byte.
static void
srpk_integer_to_wire_max_(dns_towire_state_t *NONNULL txn, uint64_t val,
                          unsigned first_max, unsigned left_or, int line)
{
    unsigned len = 1;
    for (uint64_t shift = val; shift > first_max; shift >>= 7) {
        ++len;
    }
    if (!txn->error) {
        if (txn->p + len >= txn->lim) {
            txn->error = ENOBUFS;
            txn->truncated = true;
            txn->line = line;
            return;
        }
        uint64_t shift = val;
        // Go from LSB to MSB (7-bit Bytes), setting bit 7 on all but the first and last byte; on the
        // first byte we use left_or, which is 128 for normal compressed integers, but can have other values.
        // On the last byte we use 0 to indicate that it's the last.
        for (unsigned i = 0; i < len; i++) {
            unsigned offset = len - i - 1;
            unsigned left = (offset == 0
                             ? (left_or | (val >= first_max ? first_max : 0))
                             : ((i == 0)
                                ? 0
                                : 128));
            SRPK_DEBUG("offset %d  byte: %" PRIx64 " left %x", offset, (shift & 127), left);
            txn->p[offset] = (shift & 127) | left;
            shift >>= 7;
        }
        txn->p += len;
    }
    SRPK_DEBUG("integer value to write is %" PRId64 " (%" PRIx64 "; %d bytes written:", val, val, len);
}

#define srpk_integer_to_wire(txn, val) srpk_integer_to_wire_(txn, val, __LINE__)

static void
srpk_integer_to_wire_(dns_towire_state_t *NONNULL txn, uint64_t val, int line)
{
    return srpk_integer_to_wire_max_(txn, val, 128, 0, line);
}

srpk_back_pointer_t *
srpk_label_cache_(dns_towire_state_t *NONNULL txn, dns_label_t *label, dns_rr_t *txt_record,
                  bptr_type_t bptr_type, int line)
{
    if (txn->error != 0) {
        return NULL;
    }
    if (txn->p - (uint8_t *)txn->message > 65536) {
        txn->error = E2BIG;
        txn->truncated = true;
        txn->line = line;
        return NULL;
    }
    srpk_back_pointer_t *bptr = srp_strict_calloc(1, sizeof(*bptr));
    if (bptr == NULL) {
        txn->error = ENOMEM;
        txn->truncated = true;
        txn->line = line;
        return NULL;
    }
    bptr->label = label;
    bptr->txt_record = txt_record;
    bptr->line = line;
    bptr->type = bptr_type;
    bptr->offset = txn->p - (uint8_t *)txn->message;
    bptr->data = txn->p;
    bptr->next = txn->back_pointers;
    txn->back_pointers = bptr; // we want the list backwards for locality of reference.
    return bptr;
}

// Check for sixteen bytes of hexadecimal (caller is responsible for bounds check)
static bool
srpk_is_hex_16(const char *data)
{
    for (int i = 0; i < 16; i++) {
        if (!((data[i] >= '0' && data[i] <= '9') ||
              (data[i] >= 'a' && data[i] <= 'f') ||
              (data[i] >= 'A' && data[i] <= 'F')))
        {
            return false;
        }
    }
    return true;
}

// Convert 16-byte hex string to 8-byte binary data; caller is responsible for validating string and bounds check
#define srpk_hex_to_wire(txn, data) srpk_hex_to_wire_(txn, data, __LINE__)
static void
srpk_hex_to_wire_(dns_towire_state_t *NONNULL txn, const char *data, int line)
{
    for (int i = 0; i < 16; i += 2) {
        uint8_t byte = 0;
        for (int j = 0; j < 2; j++) {
            uint8_t nybble = data[i + j];
            // Nybble is already validated to be a hex char.
            if (nybble <= '9') {
                nybble -= '0';
            } else if (nybble <= 'F') {
                nybble = (nybble - 'A') + 10;
            } else {
                nybble = (nybble - 'a') + 10;
            }
            byte = (byte << 4) | nybble;
        }
        dns_u8_to_wire_(txn, byte, line);
    }
}

//
// - `00 <6-bit len>`: Standard label encoding, where the lower 6 bits indicate the label's length (number of
//       characters). This is followed by the label characters, totaling `len` in number.
// - `01 <6-bit len>`: Similar to the above, but the label begins with an underscore `'_'`. The `len` value here doesn't
//       include the underscore itself, which isn't explicitly encoded after the dispatch byte.
// - `10 <6-bit offset first seg>`: This references a previously encoded label within the message. The 6-bit value in
//       the dispatch byte serves as the first segment of a compact `uint` representing the offset to that label. This
//       offset must point to the label dispatch byte of the previous label.
//   - Note: This mechanism applies to a single label only. Any subsequent labels are encoded directly after the current
//     one. This differs from traditional DNS pointer name compression, where a pointer indicates that the remaining
//     labels should be read from the specified offset.
// - `110 <5-bit code>`: Commonly used constant labels. Codes are:
//   - `0`: `_udp`
//   - `1`: `_tcp`
//   - `2`: `_matter`
//   - `3`: `_matterc`
//   - `4`: `_matterd`
//   - `5`: `_hap`
// - `111 <5-bit code>`: Commonly Used Generative Patterns
//   - `0`: `<hex_value>` - 16-character uppercase hexadecimal 64-bit value.
//     - Example: `DAAFF10F39B00F32`
//     - Encoding: After the label dispatch byte, the 64-bit value is encoded as 8-byte binary format (big-endian
//       order).
//   - `1`: `<hex_value_1>-<hex_value_2>` - Two 16-character uppercase hexadecimals, separated by a hyphen `-`
//     - Example `AABBCCDDEEFF0011-1122334455667788`.
//     - Encoding: After the label dispatch byte, the two 64-bit values are encoded (each as 8-byte binary format
//       big-endian).
//   - `2`: Subtype label `_<char><hex_value>` - starting with an underscore `_`, followed by a single character
//          `<char>`, ending with a 16-character uppercase hexadecimal 64-bit value `<hex_value>`.
//     - Example: `_IAA557733CC00EE11`
//     - Encoding: After the label dispatch byte, `<char>` is encoded as a single byte, followed by the 8-byte
//       big-endian binary representation of the 64-bit value `<hex_value>`.
//   - `3`: Subtype Label - same as the previous case.
//     - Encoding: Instead of directly encoding the 8-byte value, an offset pointing to an earlier occurrence of the
//       same byte sequence within the message is encoded. The offset is encoded using a compact `uint` format.

static void
srpk_label_to_wire(dns_towire_state_t *NONNULL txn, dns_label_t *label)
{
    int len = label->len;
    const char *data = label->data;
    char label_buf[120];

    // Don't proceed if we already hit an error.
    if (txn->error) {
        return;
    }

    // First look for labels we abbreviate directly (0b110*) since we will never use a back pointer for these
    if (len > 3 && data[0] == '_') {
        if (len == 4) {
            if (data[3] == 'p') {
                if (data[1] == 'u' && data[2] == 'd') {
                    dns_u8_to_wire(txn, SRPK_LABEL_UDP);
                    SRPK_LABEL_DEBUG("abbrev: %x", SRPK_LABEL_UDP);
                    return;
                } else if (data[1] == 't' && data[2] == 'c') {
                    dns_u8_to_wire(txn, SRPK_LABEL_TCP);
                    SRPK_LABEL_DEBUG("abbrev: %x", SRPK_LABEL_TCP);
                    return;
                } else if (data[1] == 'h' && data[2] == 'a') {
                    dns_u8_to_wire(txn, SRPK_LABEL_HAP);
                    SRPK_LABEL_DEBUG("abbrev: %x", SRPK_LABEL_HAP);
                    return;
                }
            }
        } else if (len == 7 && memcmp(data + 1, "matter", 6)) {
            SRPK_LABEL_DEBUG("abbrev: %x", SRPK_LABEL_MATTER);
            dns_u8_to_wire(txn, SRPK_LABEL_MATTER);
            return;
        } else if (len == 8 && !memcmp(data + 1, "matter", 6)) {
            if (data[7] == 'c') {
                SRPK_LABEL_DEBUG("abbrev: %x", SRPK_LABEL_MATTERC);
                dns_u8_to_wire(txn, SRPK_LABEL_MATTERC);
                return;
            } else if (data[7] == 'd') {
                SRPK_LABEL_DEBUG("abbrev: %x", SRPK_LABEL_MATTERD);
                dns_u8_to_wire(txn, SRPK_LABEL_MATTERD);
                return;
            }
        }
    }

    // Now see if we can use a back pointer to the same label
    for (srpk_back_pointer_t *bptr = txn->back_pointers; bptr != NULL; bptr = bptr->next) {
        if (bptr->label != NULL && bptr->label->len == len && !memcmp(bptr->label, data, len)) {
            SRPK_LABEL_DEBUG("straight offset: %u", bptr->offset);
            srpk_integer_to_wire_max(txn, bptr->offset, 32, SRPK_LABEL_POINTER);
            return;
        }
    }
    // Now check for _C<hex> back pointer
    if (len == 18 && data[0] == '_' && srpk_is_hex_16(&data[2])) {
        for (srpk_back_pointer_t *bptr = txn->back_pointers; bptr != NULL; bptr = bptr->next) {
            if (bptr->label == NULL) {
                continue; // not a label, so can't match.
            }
            switch(bptr->type) {
                // We can treat these the same because we only ever care about the first 8 bytes of the binary data.
                case bptr_type_single_hex:
                case bptr_type_double_hex:
                if (!memcmp(bptr->label->data, &data[2], 16)) {
                    SRPK_LABEL_DEBUG("subtype offset to single hex: %u", bptr->offset);
                    dns_u8_to_wire(txn, SRPK_LABEL_UNDERLINE_CHAR_PTR);
                    dns_u8_to_wire(txn, data[1]);
                    srpk_integer_to_wire(txn, bptr->offset + 1);
                    return;
                }
                break;
                case bptr_type_underline_char_hex:
                if (!memcmp(&bptr->label->data[2], &data[2], 16)) {
                    SRPK_LABEL_DEBUG("subtype offset to underline char single hex: %u", bptr->offset);
                    dns_u8_to_wire(txn, SRPK_LABEL_UNDERLINE_CHAR_PTR);
                    dns_u8_to_wire(txn, data[1]);
                    srpk_integer_to_wire(txn, bptr->offset + 3);
                    return;
                }
                break;
            default:
                break;
            }
        }
        // No back pointer, so just write it out.
        SRPK_LABEL_DEBUG("underline_char_hex");
        srpk_label_cache(txn, label, NULL, bptr_type_underline_char_hex);
        dns_u8_to_wire(txn, SRPK_LABEL_UNDERLINE_CHAR_HEX);
        dns_u8_to_wire(txn, data[1]);
        srpk_hex_to_wire(txn, &data[2]);
        return;
    }

    // Look for hex hunks
    if (len == 16 || len == 33) {
        // Starts with 16 bytes of hex?
        if (srpk_is_hex_16(data)) {
            if (len == 16) {
                SRPK_LABEL_DEBUG("single_hex");
                srpk_label_cache(txn, label, NULL, bptr_type_single_hex);
                dns_u8_to_wire(txn, SRPK_LABEL_SINGLE_HEX);
                srpk_hex_to_wire(txn, data);
                return;
            } else if (data[16] == '-' && srpk_is_hex_16(&data[17])) {
                SRPK_LABEL_DEBUG("double_hex");
                srpk_label_cache(txn, label, NULL, bptr_type_double_hex);
                dns_u8_to_wire(txn, SRPK_LABEL_DOUBLE_HEX);
                srpk_hex_to_wire(txn, data);
                srpk_hex_to_wire(txn, &data[17]);
                return;
            }
        }
    }
    if (data[0] == '_') {
        SRPK_LABEL_DEBUG("underline_label");
        srpk_label_cache(txn, label, NULL, bptr_type_underline_label);
        srpk_integer_to_wire_max(txn, len - 1, 32, SRPK_LABEL_UNDERLINE);
        dns_rdata_raw_data_to_wire(txn, data + 1, len - 1);
        return;
    }
    SRPK_LABEL_DEBUG("literal_label");
    srpk_label_cache(txn, label, NULL, bptr_type_literal_label);
    srpk_integer_to_wire_max(txn, len, 32, SRPK_LABEL_LITERAL);
    dns_rdata_raw_data_to_wire(txn, data, len);
    return;
}

static void
srpk_address_to_wire(dns_towire_state_t *txn, host_addr_t *addr, cti_prefix_vec_t *prefixes)
{
    bool use_context_id = false;
    int context_id = -1;
    size_t i;

    // Scan the prefix list looking for a prefix that matches this address.
    if (prefixes != NULL) {
        for (i = 0; i < prefixes->num; i++) {
            if (prefixes->prefixes == NULL || prefixes->prefixes[i] == NULL) {
                continue;
            }
            cti_prefix_t *prefix = prefixes->prefixes[i];
            // If we find a matching prefix, record its context ID.
            if (prefix->has_6lowpan_context && prefix->thread_6lowpan_context.compression_allowed &&
                !in6prefix_compare(&prefix->prefix, &addr->rr.data.aaaa, 8)) {
                use_context_id = true;
                context_id = prefix->thread_6lowpan_context.cid;
                break;
            }
        }
    }
    // Write the address dispatch byte
    dns_u8_to_wire(txn, ((use_context_id ? SRPK_ADDRESS_DISPATCH_CC | context_id : 0) |
                         (addr->next == NULL ? 0 : SRPK_ADDRESS_DISPATCH_MORE)));
    // If we don't have a context ID, write out the 64-bit prefix
    if (!use_context_id) {
        dns_rdata_raw_data_to_wire(txn, &addr->rr.data.aaaa, 8);
    }
    // Always write out the host identifier
    dns_rdata_raw_data_to_wire(txn, (uint8_t *)(&addr->rr.data.aaaa) + 8, 8);
}

// See if this is a host instruction; if so, emit a host block and return the number of records consumed.
static unsigned
srpk_emit_host_block(dns_towire_state_t *txn, dns_message_t *message, unsigned cur, client_update_t *update,
                     uint32_t default_ttl, cti_prefix_vec_t *prefixes)
{
    char name_buf_1[DNS_MAX_NAME_SIZE_ESCAPED];
    char name_buf_2[DNS_MAX_NAME_SIZE_ESCAPED];
    dns_rr_t *delete = &message->authority[cur];
    int num_addresses = 0;
    bool have_key = false;
    unsigned i = 0, last_record_consumed = 0;

    // A host record will start with a "Delete All Records on a Name" update, so see if that's the next
    // record in the message. The purpose of this is to make sure that the records we saw when parsing
    // are consecutive; if they are not, the SRP update is valid but not compressible.
    if (!(delete->type == dns_rrtype_any && delete->qclass == dns_qclass_any && delete->ttl == 0)) {
        return 0; // Not a delete.
    }

    bool can_elide_key_ttl = false;
    uint32_t host_ttl = default_ttl;

    // It's a delete. Is it followed by zero or more address records on the same name, and then a key record?
    for (i = cur + 1; i < message->nscount; i++) {
        dns_rr_t *rr = &message->authority[i];
        // Address record?
        if (rr->type == dns_rrtype_aaaa) {
            // Name has to be the same.
            if (!dns_names_equal(rr->name, delete->name)) {
                SRPK_DEBUG("can't compress: host name " PRI_S_SRP " doesn't match preceding delete name " PRI_S_SRP,
                           dns_name_print(rr->name, name_buf_1, sizeof(name_buf_1)),
                           dns_name_print(delete->name, name_buf_2, sizeof(name_buf_2)));
                return -1;
            }
            if (rr->ttl != host_ttl) {
                if (i == cur + 1) {
                    host_ttl = rr->ttl;
                } else {
                    SRPK_DEBUG("can't compress: host " PRI_S_SRP " has multiple address TTLs ",
                               dns_name_print(rr->name, name_buf_1, sizeof(name_buf_1)));
                }
            }
            num_addresses++;
            continue;
        }
        if (rr->type == dns_rrtype_key) {
            if (!dns_names_equal(rr->name, delete->name)) {
                SRPK_DEBUG("can't compress: key name " PRI_S_SRP " doesn't match preceding delete name " PRI_S_SRP,
                           dns_name_print(rr->name, name_buf_1, sizeof(name_buf_1)),
                           dns_name_print(delete->name, name_buf_2, sizeof(name_buf_2)));
                return -1;
            }
            have_key = true;
            if (rr->ttl == default_ttl) {
                can_elide_key_ttl = true;
            }

            // Possible success: we saw zero or more address records followed by a key record.
            ++i;
            break;
        }
        SRPK_DEBUG("can't compress: host " PRI_S_SRP " record set ends with rrtype %d rather than a key",
                   dns_name_print(rr->name, name_buf_1, sizeof(name_buf_1)), rr->type);
        return -1;
    }

    last_record_consumed = i;

    // This shouldn't be possible if the parse succeeded--the key is required for a successful SRP parse.
    if (!have_key) {
        FAULT("can't compress: no key following host records for " PRI_S_SRP,
              dns_name_print(delete->name, name_buf_1, sizeof(name_buf_1)));
        return -1;
    }
    // It is possible that the key could come before one or more address records, resulting in a low address count.
    int num_host_addresses = 0;
    for (host_addr_t *addr = update->host->addrs; addr != NULL; addr = addr->next) {
        num_host_addresses++;
    }
    if (num_host_addresses != num_addresses) {
        SRPK_DEBUG("can't compress: host " PRI_S_SRP " record set is not properly sequential",
                   dns_name_print(delete->name, name_buf_1, sizeof(name_buf_1)));
        return -1;
    }
    SRPK_DEBUG("can compress host " PRI_S_SRP ".",
               dns_name_print(delete->name, name_buf_1, sizeof(name_buf_1)));

    // ## Host Block
    //
    // ### Host Dispatch
    //
    // ```
    //     0      1     2     3     4     5     6     7
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    //   |  1  |  0  | AT  | ADR | KT  | KEY | (unused)  |
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    // ```
    //
    // - `AT` flag (Address TTL) - `0` AAAA TTL elided (use default from header), `1`: TTL encoded.
    // - `ADR` flag (Address list) - `0`: Address list elided (no addresses), `1`: Address list encoded.
    // - 'KT flag (Key TTL) - `0`: Key TTL elided (use default from header), `1`: TTL encoded
    // - `KEY` flag (Key) - `0`: Key elided, `1`: Key encoded.
    //

    // Write the host dispatch byte.
    SRPK_PUT(dns_u8_to_wire, txn, (SRPK_HOST_DISPATCH_BLOCK |
                                   (host_ttl == default_ttl ? 0 : SRPK_HOST_DISPATCH_AT) |
                                   (num_host_addresses == 0 ? 0 : SRPK_HOST_DISPATCH_ADR) |
                                   (can_elide_key_ttl ? 0 : SRPK_HOST_DISPATCH_KT) |
                                   SRPK_HOST_DISPATCH_KEY));

    // ### Host Block Format
    //
    // - "Host Dispatch" byte
    // - AAAA record TTL (if not elided)
    // - Address list (if not empty based on `ADR` flag) - Starts with Address Dispatch byte
    // - Key TTL (if not elided)
    // - Key (if not elided) - 64 bytes
    //
    // TTL fields (when not elided) are encoded using the compact integer format.
    //
    // When the address list is not empty, each address begins with an Address Dispatch byte, followed by the specific
    // information indicated by that dispatch byte.
    //
    // ### Address Dispatch
    //
    // ```
    //     0      1     2     3     4     5     6     7
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    //   |  C  |  M  | (unused)  |    Context ID         |
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    // ```
    //
    // - `C` flag (Context compression)
    //   - `0`: IPv6 address fully encoded (16 bytes) after dispatch.
    //   - `1`: Context compression used. Context ID provided in lower 4 bits. Address IID (8 bytes) encoded after
    //          dispatch.
    // - `M` flag (More addresses)
    //   - `0`: This is the last address in the list.
    //   - `1`: There are more addresses in the list.
    //
    // When context compression is used, the corresponding prefix is determined using Thread Network Data and the
    // specified Context ID.

    if (host_ttl != default_ttl) {
        SRPK_PUT(srpk_integer_to_wire, txn, host_ttl);
    }

    for (host_addr_t *addr = update->host->addrs; addr != NULL; addr = addr->next) {
        SRPK_PUT(srpk_address_to_wire, txn, addr, prefixes);
    }

    // Emit the key TTL if necessary
    if (!can_elide_key_ttl) {
        SRPK_PUT(srpk_integer_to_wire, txn, update->host->key->ttl);
    }
    // Write out the public key
    SRPK_PUT(dns_rdata_raw_data_to_wire, txn, update->host->key->data.key.key, ECDSA_KEY_SIZE);

    // Return the number of records consumed.
    return last_record_consumed - cur;
}

static void
srpk_write_instance_labels(dns_towire_state_t *txn, dns_label_t *instance_name, dns_label_t *service_name)
{
    // Write out the first label of the service instance name
    SRPK_PUT(srpk_label_to_wire, txn, instance_name);
    // Write out the service name (two labels)
    SRPK_PUT(srpk_label_to_wire, txn, service_name);
    SRPK_PUT(srpk_label_to_wire, txn, service_name->next);
}

static bool
srpk_two_labels_present(message_t *raw_message, unsigned name_offset)
{
    uint8_t *ptr_rr = &raw_message->wire.data[0] + name_offset;
    // raw message is already validated
    // If this is a pointer, it's fine.
    if (*ptr_rr > 63) {
        return true;
    }
    uint8_t *next_label = ptr_rr + *ptr_rr + 1;
    // What we are trying to ensure is that the second label is not a pointer.
    if (*next_label > 63) {
        return false;
    }
    return true;
}

// ## Remove Service Block
//
// ### Remove Service Dispatch
//
//     0      1     2     3     4     5     6     7
//   +-----+-----+-----+-----+-----+-----+-----+-----+
//   |  0  |  1  |     (unused)                      |
//   +-----+-----+-----+-----+-----+-----+-----+-----+
//
// ### Remove Service Block Format
//
// - “Remove Service Dispatch” byte
// - Service instance label
// - Service name labels (excludes zone name)

static int
srpk_emit_service_remove_block(dns_towire_state_t *txn, dns_message_t *message, message_t *raw_message, unsigned cur)
{
    // A service remove looks like a delete for the PTR record pointing to the service instance name, plus a
    // delete for all records on the service instance name.
    dns_rr_t *ptr_delete = &message->authority[cur];
    unsigned ptr_offset = message->offsets[cur + message->qdcount + message->ancount];

    // A service remove will start with a "Delete PTR Records on a Name" update, so see if that's the next
    // record in the message. The purpose of this is to make sure that the records we saw when parsing
    // are consecutive; if they are not, the SRP update is valid but not compressible.
    if (!(ptr_delete->type == dns_rrtype_ptr && ptr_delete->qclass == dns_qclass_none && ptr_delete->ttl == 0)) {
        return 0; // Not a "Delete PTR with value on name" update.
    }
    if (cur + 1 >= message->nscount) {
        return 0; // No "delete all records on a name following.
    }

    dns_rr_t *instance_delete = &message->authority[cur + 1];
    if (!(instance_delete->type == dns_rrtype_any && instance_delete->qclass == dns_qclass_any && instance_delete->ttl == 0)) {
        return 0; // Not a "Delete all RRs on a name" update.
    }
    if (!dns_names_equal(instance_delete->name, ptr_delete->data.ptr.name)) {
        return 0; // Delete all records on a name was not for the service instance name.
    }
    // Invalid service name?
    if (ptr_delete->name->next == NULL) {
        return 0;
    }
    if (!srpk_two_labels_present(raw_message, ptr_offset)) {
        return 0;
    }

    // Write the remove service dispatch byte.
    dns_u8_to_wire(txn, (SRPK_REMOVE_SERVICE_DISPATCH_BLOCK));
    srpk_write_instance_labels(txn, instance_delete->name, ptr_delete->name);

    return 2; // Delete always consumes two records.
}

// #### TXT Data Dispatch
//
// - `0 <7-bit len - first segment>`:    TXT data is encoded directly. The data size is encoded using the compact
//                                       integer format, with the first segment utilizing the 7 bits in the dispatch
//                                       byte. This is followed by the TXT data bytes of the specified length.
// - `1 <7-bit offset - first segment>`: Refers to previously encoded TXT data within the message. The offset is encoded
//                                       using the compact integer format, with the first segment utilizing the 7 bits
//                                       in the dispatch byte. The offset MUST point to a previous existing TXT data
//                                       block within the message.

static void
srpk_emit_txt_record(dns_towire_state_t *txn, dns_rr_t *txt)
{
    // Now see if we can use a back pointer to the same label
    for (srpk_back_pointer_t *bptr = txn->back_pointers; bptr != NULL; bptr = bptr->next) {
        if (bptr->txt_record != NULL && bptr->txt_record->data.txt.len == txt->data.txt.len &&
            !memcmp(txt->data.txt.data, bptr->txt_record->data.txt.data, txt->data.txt.len))
        {
            SRPK_DEBUG("txt record back-pointer offset: %u", bptr->offset);
            srpk_integer_to_wire_max(txn, bptr->offset, 64, SRPK_TXT_DISPATCH_OFFSET);
            return;
        }
    }
    SRPK_DEBUG("literal txt record");
    srpk_label_cache(txn, NULL, txt, bptr_type_txt_record);
    srpk_integer_to_wire_max(txn, txt->data.txt.len, 64, 0);
    dns_rdata_raw_data_to_wire(txn, txt->data.txt.data, txt->data.txt.len);
}

static int
srpk_emit_service_block(dns_towire_state_t *txn, dns_message_t *message, message_t *raw_message,
                        unsigned cur, client_update_t *update, uint32_t default_ttl)
{
    dns_rr_t *service_ptr = &message->authority[cur];
    unsigned ptr_offset = message->offsets[message->qdcount + message->ancount + cur];
    unsigned i = cur + 1;
    char name_buf_1[DNS_MAX_NAME_SIZE_ESCAPED];
    char name_buf_2[DNS_MAX_NAME_SIZE_ESCAPED];
    bool can_elide_ptr_ttl = true;
    bool can_elide_srv_txt_ttl = true;
    bool have_subtypes = false;

    // Find the next as-yet-unemitted service. Note that base types should always come first, so we should always
    // encounter the base type before any subtypes, and we can then consume those at the same time.
    service_t *next_service;
    for (next_service = update->services; (next_service != NULL &&
                                           next_service->consumed); next_service = next_service->next) {
    }

    // next_service is now either NULL or the next service to emit.
    if (next_service == NULL) {
        SRPK_DEBUG("no more services");
        return 0;
    }

    // See if the next RR in the dns message is this service instance's PTR record. Note that this is not a delete
    // because we don't need to delete the PTR before adding it, since we're adding a PTR specific to this service
    // instance.
    if (service_ptr != next_service->rr) {
        SRPK_DEBUG("next service is not the next rr");
        return 0;
    }

    if (service_ptr->ttl != default_ttl) {
        can_elide_ptr_ttl = false;
    }

    // Make sure the PTR RR name doesn't have a pointer as the second label.
    if (!srpk_two_labels_present(raw_message, ptr_offset)) {
        SRPK_DEBUG("two labels not present for service " PUB_S_SRP,
              dns_name_print(service_ptr->name, name_buf_1, sizeof(name_buf_1)));
        return 0;
    }


    // Okay, so this is the right service. Make sure the records show up in the right sequence.
    // First all of the subtype PTR records (if any)
    for (service_t *subtype = next_service->next; (subtype != NULL &&
                                                   subtype->base_type == next_service); subtype = subtype->next) {
        // This shouldn't be possible: the update parsed successfully, so there must be the records in the message that
        // were parsed.
        if (i >= message->nscount) {
            FAULT("missing expected subtype " PRI_S_SRP,
                  dns_name_print(subtype->rr->name, name_buf_1, sizeof(name_buf_1)));
            goto fail;
        }
        dns_rr_t *subtype_rr = &message->authority[i];
        // Next service should be the next subtype.
        if (subtype->rr != subtype_rr) {
            SRPK_DEBUG("subtype " PRI_S_SRP " out of order for " PRI_S_SRP,
                       dns_name_print(subtype->rr->name, name_buf_1, sizeof(name_buf_1)),
                       dns_name_print(subtype_rr->name, name_buf_2, sizeof(name_buf_2)));
            goto fail;
        }
        // If some PTR RRs have different TTLs, we can't compress.
        if (subtype_rr->ttl != default_ttl && can_elide_ptr_ttl) {
            SRPK_DEBUG("subtype " PRI_S_SRP " ttl (%d) differs from " PRI_S_SRP " (%d)",
                       dns_name_print(subtype->rr->name, name_buf_1, sizeof(name_buf_1)), subtype_rr->ttl,
                       dns_name_print(service_ptr->name, name_buf_2, sizeof(name_buf_2)), service_ptr->ttl);

            goto fail;
        }
        have_subtypes = true;
        ++i;
    }

    // All of the subtypes are in the right sequence. Find the matching service instance that the service pointer points to.
    service_instance_t *instance;
    for (instance = update->instances; instance != NULL && instance->service != next_service; instance = instance->next) {
    }
    // Shouldn't be possible
    if (instance == NULL) {
        FAULT("missing service_instance_t for service " PUB_S_SRP,
              dns_name_print(service_ptr->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }

    // See if the SRV and (optional) TXT records are present
    if (instance->srv == NULL) {
        FAULT("missing expected SRV pointer for " PRI_S_SRP,
              dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }

    // This shouldn't be possible: the update parsed successfully, so there must be the records in the message that were parsed.
    if (i >= message->nscount) {
        FAULT("missing expected Delete All Records on Name for " PRI_S_SRP,
              dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }
    dns_rr_t *instance_delete = &message->authority[i];
    if (instance_delete->type != dns_rrtype_any || instance_delete->qclass != dns_qclass_any ||
        !dns_names_equal(instance->name, instance_delete->name))
    {
        SRPK_DEBUG("instance " PRI_S_SRP " delete all records on a name does not follow PTR add",
                   dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }
    ++i;

    if (i >= message->nscount) {
        FAULT("missing expected SRV record for " PRI_S_SRP,
              dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }
    // Out of order?
    if (instance->srv != &message->authority[i]) {
        SRPK_DEBUG("instance " PRI_S_SRP " SRV record is out of order",
                   dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }
    if (instance->srv->ttl != default_ttl) {
        can_elide_srv_txt_ttl = false;
    }
    ++i;

    // TXT record can be missing.
    if (instance->txt != NULL) {
        if (i >= message->nscount) {
            FAULT("missing expected TXT record for " PRI_S_SRP,
                  dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
            goto fail;
        }
        // Out of order?
        if (instance->txt != &message->authority[i]) {
            SRPK_DEBUG("instance " PRI_S_SRP " SRV record is out of order",
                       dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
            goto fail;
        }
        if (instance->txt->ttl != instance->srv->ttl) {
            SRPK_DEBUG("instance " PRI_S_SRP " SRV and TXT TTLS differ",
                       dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
            goto fail;
        }
        ++i;
    } else {
        SRPK_DEBUG("instance " PRI_S_SRP " TXT record is entirely omitted",
                   dns_name_print(instance->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }

    // Everything validates, so emit the service block.
    // ## Add Service Block
    //
    // ### Add Service Dispatch
    //
    // ```
    //     0      1     2     3     4     5     6     7
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    //   |  0  |  0  | PT  | ST  | SUB | PRI | WGT | TXT |
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    // ```
    //
    // - `PT` flag (PTR TTL)     - `0`: Elide PTR TTL (use default TTL from header), `1`: TTL encoded.
    // - `ST` flag (SRV/TXT TTL) - `0`: Elide SRV/TXT TTL (use default TTL from header), `1`: TTL encoded.
    // - `SUB` flag (sub-type)   - `0`: No sub-types, `1`: Sub-types labels encoded.
    // - `PRI` flag (priority)   - `0`: Elide priority (use zero), `1`: Priority encoded.
    // - `WGT` flag (weight)     - `0`: Elide weight (use zero), `1`: Weight encoded.
    // - `TXT` flag (TXT data)   - `0`: No TXT data (elided), `1`: TXT data encoded.
    //

    // Write the add service dispatch byte.
    dns_u8_to_wire(txn, (SRPK_SERVICE_ADD_DISPATCH_BLOCK |
                         (can_elide_ptr_ttl ? 0 : SRPK_SERVICE_ADD_DISPATCH_PT) |
                         (can_elide_srv_txt_ttl ? 0 : SRPK_SERVICE_ADD_DISPATCH_ST) |
                         (have_subtypes ? SRPK_SERVICE_ADD_DISPATCH_SUB : 0) |
                         (instance->srv->data.srv.priority == 0 ? 0 : SRPK_SERVICE_ADD_DISPATCH_PRI) |
                         (instance->srv->data.srv.weight == 0 ? 0 : SRPK_SERVICE_ADD_DISPATCH_WGT) |
                         (instance->txt == NULL ? 0 : SRPK_SERVICE_ADD_DISPATCH_TXT)));

    // ### Add Service Block Format
    //
    // - "Add Service Dispatch" byte
    // - PTR TTL (if not elided)
    // - SRV/TXT TTL (if not elided)
    // - Service instance label
    // - Service name labels (excluding the zone name).
    // - SubType labels (if not elided) - A series of labels, one per subtype, terminated by a single empty label (i.e.,
    //                                    single `0x00` byte).
    // - Port
    // - Priority (if not elided)
    // - Weight (if not elided)
    // - TXT Data block - if not elided.
    //
    // TTLs, port, priority, and weight fields (when not elided) are encoded using the compact integer format.
    //
    // TXT Data block (when not elided) starts with a TXT Data Dispatch byte.

    // Emit PTR TTL
    if (!can_elide_ptr_ttl) {
        SRPK_PUT(srpk_integer_to_wire, txn, service_ptr->ttl);
    }
    // Emit SRV/TXT TTL
    if (!can_elide_srv_txt_ttl) {
        SRPK_PUT(srpk_integer_to_wire, txn, instance->txt->ttl);
    }

    // Invalid service name?
    if (service_ptr->name->next == NULL) {
        FAULT("service name too short " PRI_S_SRP, dns_name_print(service_ptr->name, name_buf_1, sizeof(name_buf_1)));
        goto fail;
    }
    srpk_write_instance_labels(txn, instance->name, service_ptr->name);
    next_service->consumed = true;

    if (have_subtypes) {
        for (service_t *subtype = next_service->next; (subtype != NULL &&
                                                       subtype->base_type == next_service); subtype = subtype->next) {
            subtype->consumed = true;
            SRPK_PUT(srpk_label_to_wire, txn, subtype->rr->name);
        }
        // End with null label
        SRPK_PUT(dns_u8_to_wire, txn, 0);
    }
    srpk_integer_to_wire(txn, instance->srv->data.srv.port);
    if (instance->srv->data.srv.priority != 0) {
        SRPK_PUT(srpk_integer_to_wire, txn, instance->srv->data.srv.priority);
    }
    if (instance->srv->data.srv.weight != 0) {
        SRPK_PUT(srpk_integer_to_wire, txn, instance->srv->data.srv.weight);
    }
    if (instance->txt != NULL && instance->txt->data.txt.len != 0) {
        SRPK_PUT(srpk_emit_txt_record, txn, instance->txt);
    }
    SRPK_DEBUG("successfully consumed %d records", i - cur);
    return i - cur; // records consumed

fail:
    txn->error = EINVAL;
    return -1;
}

static void
srpk_name_to_wire(dns_towire_state_t *txn, dns_name_t *name, dns_label_t *lim)
{
    for (dns_label_t *label = name; label != NULL && label != lim; label = label->next) {
        srpk_label_to_wire(txn, label);
    }
    dns_u8_to_wire(txn, 0); // End label.
}

// Take a parsed SRP message and try to write it as a compressed message.
void
srpk_message_to_wire(dns_towire_state_t *txn, message_t *raw_message, cti_prefix_vec_t *prefixes)
{
    char name_buf_1[DNS_MAX_NAME_SIZE_ESCAPED];
    char name_buf_2[DNS_MAX_NAME_SIZE_ESCAPED];

    // First parse the message into a client_update_t. This validates that the message is actually
    // a valid SRP update that we could conceivably compress.
    client_update_t *update = srp_evaluate(NULL, NULL, raw_message, 0);
    if (update == NULL) {
        txn->error = EINVAL;
        return;
    }
    dns_message_t *message = update->parsed_message;

    // There should be one question
    if (message->qdcount != 1) {
        // More than one question
        SRPK_DEBUG("wrong qdcount %d", message->qdcount);
        txn->error = EINVAL;
        return;
    }

    // Which should be the update zone.
    if (!dns_names_equal(message->questions[0].name, update->update_zone)) {
        SRPK_DEBUG("question 0 name " PRI_S_SRP " does not match update zone " PRI_S_SRP,
                   dns_name_print(message->questions[0].name, name_buf_1, sizeof(name_buf_1)),
                   dns_name_print(update->update_zone, name_buf_2, sizeof(name_buf_2)));
        txn->error = EINVAL;
        return;
    }

    // There should be a signature, and the length should be ECDSA_SHA256_SIG_SIZE. If it's not, we can't compress, doesn't mean
    // the message is invalid.
    if (update->signature == NULL) {
        SRPK_DEBUG("missing signatured");
        txn->error = EINVAL;
        return;    } else if (update->signature->data.sig.len != ECDSA_SHA256_SIG_SIZE) {
        SRPK_DEBUG("wrong signature length %d", update->signature->data.sig.len);
        txn->error = EINVAL;
        return;
    }

    // There should be a key, and it should be of type dnssec_keytype_ecdsa.
    if (update->host->key->data.key.algorithm != dnssec_keytype_ecdsa) {
        SRPK_DEBUG("wrong key algorithm %d", update->host->key->data.key.algorithm);
        txn->error = EINVAL;
        return;
    }

    // There should be a key, and it should be of type dnssec_keytype_ecdsa.
    if (update->host->key->data.key.len != ECDSA_KEY_SIZE) {
        SRPK_DEBUG("wrong key length %d, should be %d", update->host->key->data.key.len, ECDSA_KEY_SIZE);
        txn->error = EINVAL;
        return;
    }

    // There should be no records in the answer section, since that's prerequisites in DNS update.
    if (message->ancount != 0) {
        SRPK_DEBUG("wrong ancount %d", message->ancount);
        txn->error = EINVAL;
        return;
    }

    // There should be at least one record in the updates section.
    if (message->nscount < 1) {
        SRPK_DEBUG("not enough updates %d", message->nscount);
        txn->error = EINVAL;
        return;
    }

    // Emit the header and, if necessary, zone and TTL.
    // Note that the header includes the hostname, of which There Can Be Only One.

    // See if we have a consistent TTL.
    struct ttl_counts {
        uint32_t ttl;
        uint32_t count;
    } *ttl_counts = srp_strict_calloc(message->nscount, sizeof(*ttl_counts));
    int num_ttls = 0;
    int highest_ttl = 0;
    for (unsigned i = 0; i < message->nscount; i++) {
        int j;
        for (j = 0; j < num_ttls; j++) {
            if (ttl_counts[j].ttl == message->authority[i].ttl) {
                ttl_counts[j].count++;
                break;
            }
        }
        if (j == num_ttls) {
            ttl_counts[j].ttl = message->authority[i].ttl;
            ttl_counts[j].count = 1;
            num_ttls++;
        }
        // Remember which TTL got the most votes.
        if (ttl_counts[j].count > ttl_counts[highest_ttl].count) {
            highest_ttl = j;
        }
    }

    // There may be more than one TTL that has the same count. Prefer TTL 7200 in this case.
    for (int i = 0; i < num_ttls; i++) {
        if (i != highest_ttl && ttl_counts[i].ttl == 7200 && ttl_counts[i].count == ttl_counts[highest_ttl].count) {
            highest_ttl = i;
            break; // There Can Be Only One
        }
    }

    uint32_t default_ttl = ttl_counts[highest_ttl].ttl;
    srp_strict_free(&ttl_counts);

    //     0      1     2     3     4     5     6     7
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    //   |  0  |  0  |  1  |  0  |  1  |  1  |  Z  |  T  |
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    // ```
    //
    // - The initial bits in dispatch byte are fixed to differentiate it from the general DNS header used in SRP
    //   updates.

    // - `Z` flag (Zone)
    //   - `0`: Zone name is elided - use the default `default.service.arpa`.
    //   - `1`: Zone name is encoded within the message.


    // - `T` flag (Default TTL)
    //   - `0`: Default TTL is elided - use `7200` (2 hours).
    //   - `1`: Default TTL is encoded in the message using compact integer format.

    uint8_t ttl_elided = false; // bit 6 of header dispatch
    if (default_ttl == 7200) {
        ttl_elided = true;
    }

    // Check to see if the zone name is default.service.arpa
    uint8_t default_elided = false;
    if (dns_names_equal_text(update->update_zone, "default.service.arpa.")) {
        default_elided = true;
    }

    SRPK_PUT(dns_rdata_raw_data_to_wire, txn, (uint8_t *)(&raw_message->wire.id), 2);
    SRPK_PUT(dns_u8_to_wire,txn, (SRPK_COMPRESSION_DISPATCH_CODE |
                                  (ttl_elided ? 0 : SRPK_COMPRESSION_DISPATCH_TP) |
                                  (default_elided ? 0 : SRPK_COMPRESSION_DISPATCH_ZP)));

    if (!default_elided) {
        SRPK_PUT(srpk_name_to_wire, txn, update->update_zone, NULL);
    }
    if (!ttl_elided) {
        SRPK_PUT(srpk_integer_to_wire, txn, default_ttl);
    }

    // Host labels
    dns_label_t *lim = dns_name_subdomain_of(update->host->name, update->update_zone);
    SRPK_PUT(srpk_name_to_wire, txn, update->host->name, lim);

    // At this point we're going to assume that the message is valid, although we could be wrong.
    // So we start going through the records in the authority section, which are the updates. Each
    // SRP instruction in the authority section has a specific format, and we will insist also that
    // the ordering be reversible: that everything in the source message is in the same order that
    // we would emit it in the compressed message, so that when we uncompress it will come out in
    // the right order, and that there are no keys that could be elided, since we can only represent
    // the key RR that's attached to the host.
    for (unsigned i = 0; i < message->nscount; ) {
        uint8_t *start = txn->p;
        int records_consumed = srpk_emit_host_block(txn, message, i, update, default_ttl, prefixes);
        if (records_consumed > 0) {
            srpk_hex_dump("host block: ", start - (uint8_t *)&txn->message, start, txn->p - start);
        } else {
            // Check for a service remove block
            if (records_consumed == 0) {
                records_consumed = srpk_emit_service_remove_block(txn, message, raw_message, i);
            }
            if (records_consumed > 0) {
                srpk_hex_dump("service remove block: ", start - (uint8_t *)&txn->message, start, txn->p - start);
            } else {
                if (records_consumed == 0) {
                    records_consumed = srpk_emit_service_block(txn, message, raw_message, i, update, default_ttl);
                }
                if (records_consumed > 0) {
                    srpk_hex_dump("service add block: ", start - (uint8_t *)&txn->message, start, txn->p - start);
                }
            }
        }

        if (records_consumed == 0) {
            SRPK_DEBUG("update %d on name " PRI_S_SRP " RRtype %d class %d ttl %d isn't recognized as part of a block",
                       i, dns_name_print(message->authority[i].name, name_buf_1, sizeof(name_buf_1)),
                       message->authority[i].type, message->authority[i].qclass, message->authority[i].ttl);
            txn->error = EINVAL;
            return;
        }

        // If there was a host instruction, but it was in the wrong order, we can't compress.
        if (records_consumed < 0) {
            if (txn->error == 0) {
                txn->error = EINVAL;
                txn->line = __LINE__;
            }
            return;
        }

        // We successfully emitted something, don't care what, advance past it.
        i += records_consumed;
    }

    // If we get here, we were able to emit all of the updates in some sort of block, so now we should be able
    // to emit the footer.

    // ### Footer Dispatch
    //
    //     0      1     2     3     4     5     6     7
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    //   |  1  |  0  |  0  | LS  | KLS |     |   SIG     |
    //   +-----+-----+-----+-----+-----+-----+-----+-----+
    //
    // - `LS` flag (Lease) - `0`: Lease elided (assume zero). `1`: Lease encoded
    // - `KLS` flag (Key lease) - `0`: Key lease elided (assume zero). `1`: Key lease encoded
    // - `SIG` code (2 bit value)
    //   - `00` - signature elided
    //   - `01` - Full signature (64 bytes)
    //   - `10` - Short signature (TBD)
    //   - `11` - Unused

    // For now we always include full signature, since we haven't defined short signature.
    bool host_lease_elided = false;
    if (update->host_lease == 0) {
        host_lease_elided = true;
    }
    bool key_lease_elided = false;
    if (update->key_lease == 0) {
        key_lease_elided = true;
    }
    bool time_range_present = false;
    if (update->signature->data.sig.expiry != 0 || update->signature->data.sig.inception != 0) {
        time_range_present = true;
    }
    uint8_t signature_type = SRPK_COMPRESSION_FOOTER_SIGF; // full signature is the only kind we currently support
    SRPK_PUT(dns_u8_to_wire, txn, (SRPK_COMPRESSION_FOOTER_MARK | (host_lease_elided ? 0 : SRPK_COMPRESSION_FOOTER_LP ) |
                                   (key_lease_elided ? 0 : SRPK_COMPRESSION_FOOTER_KLP) |
                                   (time_range_present ? SRPK_COMPRESSION_FOOTER_TRP : 0) |signature_type));

    if (!host_lease_elided) {
        SRPK_PUT(srpk_integer_to_wire, txn, update->host_lease);
    }
    if (!key_lease_elided) {
        SRPK_PUT(srpk_integer_to_wire, txn, update->key_lease);
    }
    if (time_range_present) {
        SRPK_PUT(dns_u32_to_wire, txn, update->signature->data.sig.expiry);
        SRPK_PUT(dns_u32_to_wire, txn, update->signature->data.sig.inception);
    }
    SRPK_PUT(dns_rdata_raw_data_to_wire, txn, update->signature->data.sig.signature, ECDSA_SHA256_SIG_SIZE);

    return;
}


// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 120
// indent-tabs-mode: nil
// End:
