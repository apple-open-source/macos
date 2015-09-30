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

/***
 *** Each of {TLS, SSLv3} implements each of these functions.
 ***/

/* decrypt, validate one record */
typedef int (*decryptRecordFcn) (
	uint8_t type,
	tls_buffer *input,
	tls_record_t ctx);

/* pack, encrypt, mac, queue one outgoing record */
typedef int (*writeRecordFcn) (
	SSLRecord rec,
	tls_record_t ctx);

/* initialize a per-CipherContext HashHmacContext for use in MACing each record */
typedef int (*initMacFcn) (
    CipherContext *cipherCtx		// macRef, macSecret valid on entry
									// macCtx valid on return
);

/* free per-CipherContext HashHmacContext */
typedef int (*freeMacFcn) (
	CipherContext *cipherCtx);

/* compute MAC on one record */
typedef int (*computeMacFcn) (
	uint8_t type,
	tls_buffer data,
	tls_buffer mac, 					// caller mallocs data
	CipherContext *cipherCtx,		// assumes macCtx, macRef
	uint64_t seqNo,
	tls_record_t ctx);


typedef struct _SslRecordCallouts {
	decryptRecordFcn			decryptRecord;
	initMacFcn					initMac;
	freeMacFcn					freeMac;
	computeMacFcn				computeMac;
} SslRecordCallouts;


/* From ssl3RecordCallouts.c and tls1RecordCallouts.c */
extern const SslRecordCallouts  Ssl3RecordCallouts;
extern const SslRecordCallouts	Tls1RecordCallouts;

/* one callout routine used in common (for now) */
int ssl3WriteRecord(
	SSLRecord rec,
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

typedef struct {
    const HashHmacReference     *macAlgorithm;
    const SSLSymmetricCipher          *cipher;
} SSLRecordCipherSpec;



struct _tls_record_s
{
    /* ciphers */
    uint16_t            selectedCipher;			/* currently selected */
    SSLRecordCipherSpec selectedCipherSpec;     /* ditto */
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
    const SslRecordCallouts   *sslTslCalls;
    struct ccrng_state * rng; /* corecrypto rng interface */
};

/* Function called from the ssl3/tls1 callouts */

int SSLVerifyMac(
                 uint8_t type,
                 tls_buffer *data,
                 uint8_t *compareMAC,
                 tls_record_t ctx);

#ifdef	__cplusplus
}
#endif

#endif 	/* _TLS_SSL_H_ */
