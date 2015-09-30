//
//  tls_record.h
//  Security
//
//  Created by Fabrice Gautier on 10/25/11.
//  Copyright (c) 2011, 2013 Apple, Inc. All rights reserved.
//


/* This header should be kernel and libsystem compatible */

#ifndef _TLS_RECORD_H_
#define _TLS_RECORD_H_ 1

#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <corecrypto/ccrng.h>

#include "tls_types.h"

/* Types : */
/*=========*/

typedef void *tls_record_ctx_t;
typedef struct _tls_record_s *tls_record_t;

/* Record layer creation functions */
/*=================================*/

tls_record_t
tls_record_create(bool dtls, struct ccrng_state *rng);

void
tls_record_destroy(tls_record_t filter);

/* Record parsing helpers */
/*========================*/

int
tls_record_get_header_size(tls_record_t filter);

int
tls_record_parse_header(tls_record_t filter, tls_buffer input, size_t *len, uint8_t *content_type);

int
tls_record_parse_ssl2_header(tls_record_t filter, tls_buffer input, size_t *len, uint8_t *content_type);

/* decrypt/encrypt */
/*=================*/

size_t
tls_record_decrypted_size(tls_record_t filter,
                                 size_t encrypted_size);

size_t
tls_record_encrypted_size(tls_record_t filter,
                                 uint8_t contentType,
                                 size_t decrypted_size);

int
tls_record_decrypt(tls_record_t filter,
                          const tls_buffer in_data,
                          tls_buffer *out_data,
                          uint8_t *content_type);

int
tls_record_encrypt(tls_record_t filter,
                          const tls_buffer in_data,
                          uint8_t content_type,
                          tls_buffer *out_data);

/* control */
/*=========*/

int
tls_record_init_pending_ciphers(tls_record_t filter,
                                       uint16_t selectedCipher,
                                       bool isServer,
                                       tls_buffer key);

int
tls_record_advance_write_cipher(tls_record_t filter);

int
tls_record_rollback_write_cipher(tls_record_t filter);

int
tls_record_advance_read_cipher(tls_record_t filter);

int
tls_record_set_protocol_version(tls_record_t filter,
                                       tls_protocol_version protocolVersion);

int
tls_record_set_record_splitting(tls_record_t filter,
                                       bool enable);

void
tls_add_debug_logger(void (*logger)(void *, const char *, const char *, const char *), void *ctx);


#endif
