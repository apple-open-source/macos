/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
 * secCrypto.c - interface between SSL and SecKey/SecDH interfaces.
 */

#include "secCrypto.h"

#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <AssertMacros.h>

/* Private Key operations */
static
SecAsn1Oid oidForSSLHash(SSL_HashAlgorithm hash)
{
    switch (hash) {
        case SSL_HashAlgorithmSHA1:
            return CSSMOID_SHA1WithRSA;
        case SSL_HashAlgorithmSHA256:
            return CSSMOID_SHA256WithRSA;
        case SSL_HashAlgorithmSHA384:
            return CSSMOID_SHA384WithRSA;
        default:
            break;
    }
    // Internal error
    assert(0);
    // This guarantee failure down the line
    return CSSMOID_MD5WithRSA;
}

static
int mySSLPrivKeyRSA_sign(void *key, SSL_HashAlgorithm hash, const uint8_t *plaintext, size_t plaintextLen, uint8_t *sig, size_t *sigLen)
{
    SecKeyRef keyRef = key;

    if(hash == SSL_HashAlgorithmNone) {
        return SecKeyRawSign(keyRef, kSecPaddingPKCS1, plaintext, plaintextLen, sig, sigLen);
    } else {
        SecAsn1AlgId  algId;
        algId.algorithm = oidForSSLHash(hash);
        return SecKeySignDigest(keyRef, &algId, plaintext, plaintextLen, sig, sigLen);
    }
}

static
int mySSLPrivKeyRSA_decrypt(void *key, const uint8_t *ciphertext, size_t ciphertextLen, uint8_t *plaintext, size_t *plaintextLen)
{
    SecKeyRef keyRef = key;

    return SecKeyDecrypt(keyRef, kSecPaddingPKCS1, ciphertext, ciphertextLen, plaintext, plaintextLen);
}



