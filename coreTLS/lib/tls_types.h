/*
 * Copyright (c) 2011 Apple Inc. All Rights Reserved.
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
 * tls_types.h - Common types and constants for coreTLS Handshake and Record layers and SecureTransport.
 */

/* This header should be kernel and libsystem compatible */

#ifndef	_TLS_TYPES_H_
#define _TLS_TYPES_H_ 1

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#define CORETLS_MAX_VERSION 0x0304

typedef enum
{
    /* This value never appears in the actual protocol */
    tls_protocol_version_Undertermined = 0,
    /* actual protocol values */
    tls_protocol_version_SSL_3 = 0x0300,
    tls_protocol_version_TLS_1_0 = 0x0301,		/* TLS 1.0 == SSL 3.1 */
    tls_protocol_version_TLS_1_1 = 0x0302,
    tls_protocol_version_TLS_1_2 = 0x0303,
    tls_protocol_version_TLS_1_3 = 0x0304,
    tls_protocol_version_TLS_1_3_DRAFT = 0x7f12, /* Temporary version number used during pre-standard testing */
    tls_protocol_version_DTLS_1_0 = 0xfeff,
} tls_protocol_version;

/* FIXME: This enum and the SSLRecord are exposed because they
 are used at the interface between the Record and Handshake layer.
 This might not be the best idea */

typedef enum
{
    tls_record_type_ChangeCipher = 20,
    tls_record_type_Alert = 21,
    tls_record_type_Handshake = 22,
    tls_record_type_AppData = 23,
    tls_record_type_SSL2 = 0x80,
} tls_record_type;

/*
 * Server-specified client authentication mechanisms.
 */
typedef enum {
	/* doesn't appear on the wire */
	tls_client_auth_type_None = -1,
	/* RFC 2246 7.4.6 */
	tls_client_auth_type_RSASign = 1,
	tls_client_auth_type_DSSSign = 2,
	tls_client_auth_type_RSAFixedDH = 3,
	tls_client_auth_type_DSS_FixedDH = 4,
	/* RFC 4492 5.5 */
	tls_client_auth_type_ECDSASign = 64,
	tls_client_auth_type_RSAFixedECDH = 65,
	tls_client_auth_type_ECDSAFixedECDH = 66
} tls_client_auth_type;

/* TLS 1.2 Signature Algorithms extension values for hash field. */
typedef enum {
    tls_hash_algorithm_None = 0,
    tls_hash_algorithm_MD5 = 1,
    tls_hash_algorithm_SHA1 = 2,
    tls_hash_algorithm_SHA224 = 3,
    tls_hash_algorithm_SHA256 = 4,
    tls_hash_algorithm_SHA384 = 5,
    tls_hash_algorithm_SHA512 = 6
} tls_hash_algorithm;

/* TLS 1.2 Signature Algorithms extension values for signature field. */
typedef enum {
    tls_signature_algorithm_Anonymous = 0,
    tls_signature_algorithm_RSA = 1,
    tls_signature_algorithm_DSA = 2,
    tls_signature_algorithm_ECDSA = 3
} tls_signature_algorithm;

typedef struct {
    tls_hash_algorithm hash;
    tls_signature_algorithm signature;
} tls_signature_and_hash_algorithm;


/*
 * These are the named curves from RFC 4492
 * section 5.1.1, with the exception of tls_curve_none which means
 * "ECDSA not negotiated".
 */
typedef enum
{
    tls_curve_none = -1,

    tls_curve_sect163k1 = 1,
    tls_curve_sect163r1 = 2,
    tls_curve_sect163r2 = 3,
    tls_curve_sect193r1 = 4,
    tls_curve_sect193r2 = 5,
    tls_curve_sect233k1 = 6,
    tls_curve_sect233r1 = 7,
    tls_curve_sect239k1 = 8,
    tls_curve_sect283k1 = 9,
    tls_curve_sect283r1 = 10,
    tls_curve_sect409k1 = 11,
    tls_curve_sect409r1 = 12,
    tls_curve_sect571k1 = 13,
    tls_curve_sect571r1 = 14,
    tls_curve_secp160k1 = 15,
    tls_curve_secp160r1 = 16,
    tls_curve_secp160r2 = 17,
    tls_curve_secp192k1 = 18,
    tls_curve_secp192r1 = 19,
    tls_curve_secp224k1 = 20,
    tls_curve_secp224r1 = 21,
    tls_curve_secp256k1 = 22,

    /* These are the ones we actually support */
    tls_curve_secp256r1 = 23,
    tls_curve_secp384r1 = 24,
    tls_curve_secp521r1 = 25
} tls_named_curve;

/*
 * An enumeration for the possible return types for a certificate verification call.
 */
typedef enum {
    tls_verify_result_pass,
    tls_verify_result_fail,
    tls_verify_result_pending,
} tls_verify_result;

/*
 * This is the buffer type used internally.
 * TODO: Make this a proper abstraction, so we can use iovec/mbuf as appropriate ?
 */
typedef struct
{   size_t  length;
    uint8_t *data;
} tls_buffer;

/* Buffer list: Used for DER encoded list of DNs and Certificate Chains */
typedef struct tls_buffer_list_s
{
    struct tls_buffer_list_s    *next;
    tls_buffer                  buffer;
} tls_buffer_list_t;


#define TLS_RECORD_HEADER_SIZE 5
#define DTLS_RECORD_HEADER_SIZE 13

/* This is defined in RFC 5246, section 6.2.1 */
#define TLS_MAX_FRAGMENT_SIZE (1<<14)


#endif /* _TLS_TYPES_H_ */
