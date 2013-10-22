/*
 * Copyright (c) 1999-2001,2005-2008,2012 Apple Inc. All Rights Reserved.
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
 * cryptType.h - CipherSpec and CipherContext structures
 */

#ifndef _CRYPTTYPE_H_
#define _CRYPTTYPE_H_ 1

// #include <Security/CipherSuite.h>
#include "tls_hmac.h"
#include "tls_hashhmac.h"
#include "symCipher.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * An SSLRecordContext contains four of these - one for each of {read,write} and for
 * {current, pending}.
 */
typedef struct CipherContext
{
    const HashHmacReference   	*macRef;			/* HMAC (TLS) or digest (SSL) */
    const SSLSymmetricCipher  	*symCipher;

    /* this is a context which is reused once per record */
    HashHmacContext				macCtx;
    /*
     * Crypto context (eg: for CommonCrypto-based symmetric ciphers, this will be a CCCryptorRef)
     */
    SymCipherContext            cipherCtx;

    /* encrypt or decrypt. needed in CDSASymmInit, may not be needed anymore (TODO)*/
    uint8_t						encrypting;

    uint64_t          			sequenceNum;
    uint8_t            			ready;

    /* in SSL2 mode, the macSecret is the same size as the
     * cipher key - which is 24 bytes in the 3DES case. */
    uint8_t						macSecret[48 /* SSL_MAX_DIGEST_LEN */];
} CipherContext;

#ifdef __cplusplus
}
#endif

#endif /* _CRYPTTYPE_H_ */
