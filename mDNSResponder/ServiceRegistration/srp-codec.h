/* srp-codec.h
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
 * SRP Coder header definitions.
 */

#ifndef __SRP_CODEC_H__
#define __SRP_CODEC_H__

// Label compression codes
// For these, the left two bits are enough to signify
#define SRPK_LABEL_2BIT_MASK          0xc0 // Compression types that start with two bits
#define SRPK_LABEL_LITERAL            0x00
#define SRPK_LABEL_UNDERLINE          0x40
#define SRPK_LABEL_POINTER            0x80

// For these it's the left three bits
#define SRPK_LABEL_3BIT_MASK           0xe0
#define SRPK_LABEL_ABBREV_MARKER       0xc0 // 0b110.....
#define SRPK_LABEL_GENERATIVE_PATTERN  0xe0 // 0b111.....

// Well-known abbreviations
#define SRPK_LABEL_UDP                0xc0
#define SRPK_LABEL_TCP                0xc1
#define SRPK_LABEL_MATTER             0xc2
#define SRPK_LABEL_MATTERC            0xc3
#define SRPK_LABEL_MATTERD            0xc4
#define SRPK_LABEL_HAP                0xc5
#define SRPK_LABEL_SINGLE_HEX         0xe0
#define SRPK_LABEL_DOUBLE_HEX         0xe1
#define SRPK_LABEL_UNDERLINE_CHAR_HEX 0xe2
#define SRPK_LABEL_UNDERLINE_CHAR_PTR 0xe3


// Back pointers for SRP label compression
typedef enum {
    bptr_type_single_hex,
    bptr_type_double_hex,
    bptr_type_underline_char_hex,
    bptr_type_underline_label,
    bptr_type_literal_label,
    bptr_type_txt_record,
} bptr_type_t;

struct srpk_back_pointer {
    srpk_back_pointer_t *NULLABLE next;
    dns_label_t *NULLABLE label;
    dns_rr_t *NULLABLE txt_record;
    uint8_t *NULLABLE data;
    const uint8_t *NULLABLE txt_data;
    uint64_t txt_len;
    int line;
    uint16_t offset;
    bptr_type_t type;
};

// Host dispatch block codes
#define SRPK_HOST_DISPATCH_BLOCK 0x80
#define SRPK_HOST_DISPATCH_AT    0x20
#define SRPK_HOST_DISPATCH_ADR   0x10
#define SRPK_HOST_DISPATCH_KT    0x08
#define SRPK_HOST_DISPATCH_KEY   0x04

// Address dispatch block codes
#define SRPK_ADDRESS_DISPATCH_CC       0x80
#define SRPK_ADDRESS_DISPATCH_MORE     0x40
#define SRPK_ADDRESS_DISPATCH_CID_MASK 0x0f

// Remove service dispatch block; no notes.
#define SRPK_REMOVE_SERVICE_DISPATCH_BLOCK 0x40

// Add service dispatch block
#define SRPK_SERVICE_ADD_DISPATCH_BLOCK 0x0
#define SRPK_SERVICE_ADD_DISPATCH_PT    0x20
#define SRPK_SERVICE_ADD_DISPATCH_ST    0x10
#define SRPK_SERVICE_ADD_DISPATCH_SUB   0x08
#define SRPK_SERVICE_ADD_DISPATCH_PRI   0x04
#define SRPK_SERVICE_ADD_DISPATCH_WGT   0x02
#define SRPK_SERVICE_ADD_DISPATCH_TXT   0x01

// TXT record dispatch
#define SRPK_TXT_DISPATCH_OFFSET        0x80

// Compression dispatch byte
#define SRPK_COMPRESSION_DISPATCH_MASK 0xfc // Bits to compare with dispatch code
#define SRPK_COMPRESSION_DISPATCH_CODE 0x2c // Marks this as a compressed SRP update
#define SRPK_COMPRESSION_DISPATCH_TP   0x02 // Default TTL present (not 7200)
#define SRPK_COMPRESSION_DISPATCH_ZP   0x01 // Update zone present (not default.service.arpa)

// Non-compression dispatch byte mask
#define SRPK_COMPRESSION_BLOCK_MASK 0xc0

// Compression footer dispatch byte
#define SRPK_COMPRESSION_FOOTER_MASK 0xe0
#define SRPK_COMPRESSION_FOOTER_MARK 0xc0
#define SRPK_COMPRESSION_FOOTER_LP   0x10 // Lease present
#define SRPK_COMPRESSION_FOOTER_KLP  0x08 // Key lease present
#define SRPK_COMPRESSION_FOOTER_TRP  0x04 // Signature Time Range present
#define SRPK_COMPRESSION_FOOTER_SIGM 0x03 // Signature code mask
#define SRPK_COMPRESSION_FOOTER_SIGE 0x00 // Signature elided
#define SRPK_COMPRESSION_FOOTER_SIGF 0x01 // Full signature
#define SRPK_COMPRESSION_FOOTER_SIGS 0x02 // Short signature

#if SRP_COMPRESSION_DEBUGGING
#  define SRPK_DEBUG(fmt, ...) DEBUG(fmt, ##__VA_ARGS__)
#else
#  define SRPK_DEBUG(fmt, ...)
#endif

#define SRPK_LABEL_DEBUG(fmt, ...) SRPK_DEBUG(PRI_S_SRP ": " fmt, dns_name_print(label, label_buf, sizeof(label_buf)), ##__VA_ARGS__)

#define srpk_hex_dump(label, offset, bytes, length) \
    srpk_hex_dump_(label, offset, bytes, length, __FILE__, __LINE__)
void srpk_hex_dump_(const char *NONNULL label, size_t offset, const uint8_t *NONNULL bytes, ssize_t length,
                    const char *NONNULL file, int line);
#define srpk_space(txn, space) srpk_space_(txn, space, __FILE__, __LINE__)
bool srpk_space_(dns_towire_state_t *NONNULL txn, size_t space, const char *NONNULL file, int line);

#define srpk_label_cache(txn, label, txt_record, bptr_type) \
    srpk_label_cache_(txn, label, txt_record, bptr_type, __LINE__)
srpk_back_pointer_t *NULLABLE srpk_label_cache_(dns_towire_state_t *NONNULL txn, dns_label_t *NULLABLE label,
                                                dns_rr_t *NULLABLE txt_record, bptr_type_t bptr_type, int line);
void srpk_message_to_wire(dns_towire_state_t *NONNULL txn,
                          message_t *NONNULL raw_message, cti_prefix_vec_t *NULLABLE prefixes);
message_t *NULLABLE srpk_message_decompress(message_t *NONNULL txn, cti_prefix_vec_t *NULLABLE prefixes);
#endif // __SRP_CODEC_H__

// Local Variables:
// mode: C
// tab-width: 4
// c-file-style: "bsd"
// c-basic-offset: 4
// fill-column: 120
// indent-tabs-mode: nil
// End:
