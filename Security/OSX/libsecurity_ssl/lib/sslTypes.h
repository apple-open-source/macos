/*
 * Copyright (c) 2011-2012,2014 Apple Inc. All Rights Reserved.
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
 * sslTypes.h - internal ssl types
 */

/* This header should be kernel compatible */

#ifndef	_SSLTYPES_H_
#define _SSLTYPES_H_ 1

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <tls_types.h>

enum {
    errSSLRecordInternal            = -10000,
    errSSLRecordWouldBlock          = -10001,
    errSSLRecordProtocol            = -10002,
    errSSLRecordNegotiation         = -10003,
    errSSLRecordClosedAbort         = -10004,
	errSSLRecordConnectionRefused   = -10005,	/* peer dropped connection before responding */
	errSSLRecordDecryptionFail      = -10006,	/* decryption failure */
	errSSLRecordBadRecordMac        = -10007,	/* bad MAC */
	errSSLRecordRecordOverflow      = -10008,	/* record overflow */
	errSSLRecordUnexpectedRecord    = -10009,	/* unexpected (skipped) record in DTLS */
};

typedef enum
{
    /* This value never appears in the actual protocol */
    SSL_Version_Undetermined = 0,
    /* actual protocol values */
    SSL_Version_2_0 = 0x0002,
    SSL_Version_3_0 = 0x0300,
    TLS_Version_1_0 = 0x0301,		/* TLS 1.0 == SSL 3.1 */
    TLS_Version_1_1 = 0x0302,
    TLS_Version_1_2 = 0x0303,
    DTLS_Version_1_0 = 0xfeff,
} SSLProtocolVersion;

/* FIXME: This enum and the SSLRecord are exposed because they
 are used at the interface between the Record and Handshake layer.
 This might not be the best idea */

enum
{   SSL_RecordTypeV2_0,
    SSL_RecordTypeV3_Smallest = 20,
    SSL_RecordTypeChangeCipher = 20,
    SSL_RecordTypeAlert = 21,
    SSL_RecordTypeHandshake = 22,
    SSL_RecordTypeAppData = 23,
    SSL_RecordTypeV3_Largest = 23
};

typedef enum
{
    kSSLRecordOptionSendOneByteRecord = 0,
} SSLRecordOption;

/*
 * This is the buffer type used internally.
 */
typedef tls_buffer SSLBuffer;

/*
struct
{   size_t  length;
    uint8_t *data;
} SSLBuffer;
*/

typedef struct
{
    uint8_t                 contentType;
    SSLBuffer               contents;
} SSLRecord;


/*
 * We should remove this and use uint64_t all over.
 */
typedef uint64_t sslUint64;


/* Opaque reference to a Record Context */
typedef void * SSLRecordContextRef;


typedef int
(*SSLRecordReadFunc)                (SSLRecordContextRef    ref,
                                     SSLRecord              *rec);

typedef int
(*SSLRecordWriteFunc)               (SSLRecordContextRef    ref,
                                     SSLRecord              rec);

typedef int
(*SSLRecordInitPendingCiphersFunc)  (SSLRecordContextRef    ref,
                                     uint16_t               selectedCipher,
                                     bool                   server,
                                     SSLBuffer              key);

typedef int
(*SSLRecordAdvanceWriteCipherFunc)  (SSLRecordContextRef    ref);

typedef int
(*SSLRecordRollbackWriteCipherFunc) (SSLRecordContextRef    ref);

typedef int
(*SSLRecordAdvanceReadCipherFunc)   (SSLRecordContextRef    ref);

typedef int
(*SSLRecordSetProtocolVersionFunc)  (SSLRecordContextRef    ref,
                                     SSLProtocolVersion     protocolVersion);

typedef int
(*SSLRecordFreeFunc)                (SSLRecordContextRef    ref,
                                     SSLRecord              rec);

typedef int
(*SSLRecordServiceWriteQueueFunc)   (SSLRecordContextRef    ref);

typedef int
(*SSLRecordSetOptionFunc)           (SSLRecordContextRef    ref,
                                     SSLRecordOption        option,
                                     bool                   value);

struct SSLRecordFuncs
{
    SSLRecordReadFunc                   read;
    SSLRecordWriteFunc                  write;
    SSLRecordInitPendingCiphersFunc     initPendingCiphers;
    SSLRecordAdvanceWriteCipherFunc     advanceWriteCipher;
    SSLRecordRollbackWriteCipherFunc    rollbackWriteCipher;
    SSLRecordAdvanceReadCipherFunc      advanceReadCipher;
    SSLRecordSetProtocolVersionFunc     setProtocolVersion;
    SSLRecordFreeFunc                   free;
    SSLRecordServiceWriteQueueFunc      serviceWriteQueue;
    SSLRecordSetOptionFunc              setOption;
};

#endif /* _SSLTYPES_H_ */
