/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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
 * tls_types_private.h - private types and constants for coreTLS Handshake and Record layers.
 */

/* This header should be kernel and libsystem compatible */

#ifndef	_TLS_TYPES_PRIVATE_H_
#define _TLS_TYPES_PRIVATE_H_ 1

#include <tls_types.h>

enum {
    errSSLRecordParam               = -50,     /* One or more parameters passed to a function were not valid. */

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

#define TLS_AES_GCM_TAG_SIZE 16
#define TLS_AES_GCM_IMPLICIT_IV_SIZE 4
#define TLS_AES_GCM_EXPLICIT_IV_SIZE 8

#endif /* _TLS_TYPES_PRIVATE_H_ */
