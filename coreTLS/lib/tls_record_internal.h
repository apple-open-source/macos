/*
 * Copyright (c) 2002,2005-2007,2010-2011 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 * tls_record.h - Declarations of record layer callout struct to provide indirect calls to
 *     SSLv3 and TLS routines.
 */

#ifndef	_TLS_RECORD_INTERNAL_H_
#define _TLS_RECORD_INTERNAL_H_

#ifdef	__cplusplus
extern "C" {
#endif

#include <tls_record.h>
#include "tls_types_private.h"
#include "cryptType.h"
#include "sslMemory.h"

struct SSLRecordInternalContext;

typedef struct
{
    uint8_t                 contentType;
    tls_protocol_version      protocolVersion;
    uint64_t                seqNum;
    tls_buffer               contents;
} SSLRecord;



/* From tls_record_crypto.c */
uint8_t muxb(bool s, uint8_t a, uint8_t b);
uint32_t mux32(bool s, uint32_t a, uint32_t b);
void mem_extract(uint8_t *dst, const uint8_t *src, size_t offset, size_t dst_len, size_t src_len);

int SSLComputeMac(uint8_t type,
                  tls_buffer *data,
                  size_t padLen,
                  uint8_t *outputMAC,
                  CipherContext *cipherCtx,
                  tls_protocol_version pv);


int SSLDecryptRecord(uint8_t type,
                     tls_buffer *payload,
                     tls_record_t ctx);


typedef struct WaitingRecord
{   struct WaitingRecord    *next;
    size_t                  sent;
    /*
     * These two fields replace a dynamically allocated tls_buffer;
     * the payload to write is contained in the variable-length
     * array data[].
     */
    size_t					length;
    uint8_t					data[1];
} WaitingRecord;

struct _tls_record_s
{
    /* ciphers */
    uint16_t            selectedCipher;			/* currently selected */
    CipherContext       readCipher;
    CipherContext       writeCipher;
    CipherContext       readPending;
    CipherContext       writePending;
    CipherContext       prevCipher;             /* previous write cipher context, used for retransmit */
    
    /* protocol */
    bool                isDTLS;
    bool                splitEnabled;
    bool                firstDataRecordEncrypted;         /* flag set to true after the first app data record is sent, after a (re)negotiation */
    tls_protocol_version  negProtocolVersion;	/* negotiated */
    struct ccrng_state * rng; /* corecrypto rng interface */
};

#ifdef	__cplusplus
}
#endif

#endif 	/* _TLS_SSL_H_ */
