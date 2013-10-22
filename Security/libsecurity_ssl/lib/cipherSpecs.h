/*
 * Copyright (c) 1999-2001,2005-2007,2010-2012 Apple Inc. All Rights Reserved.
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
 * cipherSpecs.h - SSLCipherSpec declarations
 */

#ifndef	_CIPHER_SPECS_H_
#define _CIPHER_SPECS_H_

#include <stdint.h>
#include "CipherSuite.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Cipher Algorithm */
typedef enum {
    SSL_CipherAlgorithmNull,
    SSL_CipherAlgorithmRC2_128,
    SSL_CipherAlgorithmRC4_128,
    SSL_CipherAlgorithmDES_CBC,
    SSL_CipherAlgorithm3DES_CBC,
    SSL_CipherAlgorithmAES_128_CBC,
    SSL_CipherAlgorithmAES_256_CBC,
    SSL_CipherAlgorithmAES_128_GCM,
    SSL_CipherAlgorithmAES_256_GCM,
} SSL_CipherAlgorithm;

/* The HMAC algorithms we support */
typedef enum {
    HA_Null = 0,		// i.e., uninitialized
    HA_SHA1,
    HA_MD5,
    HA_SHA256,
    HA_SHA384
} HMAC_Algs;

typedef enum
{   SSL_NULL_auth,
    SSL_RSA,
    SSL_RSA_EXPORT,
    SSL_DH_DSS,
    SSL_DH_DSS_EXPORT,
    SSL_DH_RSA,
    SSL_DH_RSA_EXPORT,
    SSL_DHE_DSS,
    SSL_DHE_DSS_EXPORT,
    SSL_DHE_RSA,
    SSL_DHE_RSA_EXPORT,
    SSL_DH_anon,
    SSL_DH_anon_EXPORT,
    SSL_Fortezza,

    /* ECDSA addenda, RFC 4492 */
    SSL_ECDH_ECDSA,
    SSL_ECDHE_ECDSA,
    SSL_ECDH_RSA,
    SSL_ECDHE_RSA,
    SSL_ECDH_anon,

    /* PSK, RFC 4279 */
    TLS_PSK,
    TLS_DHE_PSK,
    TLS_RSA_PSK,
    
} KeyExchangeMethod;


HMAC_Algs sslCipherSuiteGetMacAlgorithm(SSLCipherSuite cipherSuite);
SSL_CipherAlgorithm sslCipherSuiteGetSymmetricCipherAlgorithm(SSLCipherSuite cipherSuite);
KeyExchangeMethod sslCipherSuiteGetKeyExchangeMethod(SSLCipherSuite cipherSuite);

uint8_t sslCipherSuiteGetMacSize(SSLCipherSuite cipherSuite);
uint8_t sslCipherSuiteGetSymmetricCipherKeySize(SSLCipherSuite cipherSuite);
uint8_t sslCipherSuiteGetSymmetricCipherBlockIvSize(SSLCipherSuite cipherSuite);

/*
 * Determine if an SSLCipherSuite is SSLv2 only.
 */
#define CIPHER_SUITE_IS_SSLv2(suite)	((suite & 0xff00) == 0xff00)

#ifdef __cplusplus
}
#endif

#endif	/* _CIPHER_SPECS_H_ */
