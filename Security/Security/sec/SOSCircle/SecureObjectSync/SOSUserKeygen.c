/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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


#include <SecureObjectSync/SOSUserKeygen.h>
#include <stdio.h>
#include <corecrypto/ccrng.h>
#include <corecrypto/ccrng_pbkdf2_prng.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
#include <CommonCrypto/CommonRandomSPI.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecFramework.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFRelease.h>
#include <utilities/debugging.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSInternal.h>
#include <Security/SecFramework.h>
#include <Security/SecItem.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>

#include <corecrypto/ccder.h>
#include <Security/oidsalg.h>

// A.2   PBKDF2
//
// The object identifier id-PBKDF2 identifies the PBKDF2 key derivation
// function (Section 5.2).
//
// id-PBKDF2 OBJECT IDENTIFIER ::= {pkcs-5 12}
//
// The parameters field associated with this OID in an
// AlgorithmIdentifier shall have type PBKDF2-params:
//
// PBKDF2-params ::= SEQUENCE {
//    salt CHOICE {
//        specified OCTET STRING,
//        otherSource AlgorithmIdentifier {{PBKDF2-SaltSources}}
//    },
//    iterationCount INTEGER (1..MAX),
//    keyLength INTEGER (1..MAX) OPTIONAL,
//    prf AlgorithmIdentifier {{PBKDF2-PRFs}} DEFAULT
//    algid-hmacWithSHA1 }
//
// The fields of type PKDF2-params have the following meanings:


static size_t der_sizeof_SecAsn1Oid(const SecAsn1Oid* secasn_oid)
{
    return ccder_sizeof(CCDER_OBJECT_IDENTIFIER, secasn_oid->Length);
}

static uint8_t *der_encode_SecAsn1Oid(const SecAsn1Oid* secasn_oid, const uint8_t *der, uint8_t *der_end)
{
    return ccder_encode_tl(CCDER_OBJECT_IDENTIFIER, secasn_oid->Length, der,
           ccder_encode_body(secasn_oid->Length, secasn_oid->Data, der, der_end));
}

static const uint8_t *der_expect_SecAsn1Oid(const SecAsn1Oid* secasn_oid, const uint8_t *der, const uint8_t *der_end)
{
    size_t len = 0;
    der = ccder_decode_tl(CCDER_OBJECT_IDENTIFIER, &len,
                                          der, der_end);

    if (secasn_oid->Length != len || memcmp(secasn_oid->Data, der, len) != 0)
        der = NULL;
    else
        der += len;

    return der;
}

static size_t der_sizeof_pbkdf2_params(size_t saltLen, const uint8_t *salt,
                                       unsigned long iterationCount,
                                       unsigned long keyLength)
{
    size_t body_size = ccder_sizeof_raw_octet_string(saltLen)
                     + ccder_sizeof_uint64(iterationCount)
                     + ccder_sizeof_uint64(keyLength)
                     + der_sizeof_SecAsn1Oid(&CSSMOID_PKCS5_HMAC_SHA1);

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE, body_size);
}

static uint8_t *der_encode_pbkdf2_params(size_t saltLen, const uint8_t *salt,
                                         unsigned long iterationCount,
                                         unsigned long keyLength,
                                         const uint8_t *der, uint8_t *der_end)
{
    uint8_t* original_der_end = der_end;

    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, original_der_end, der,
           ccder_encode_raw_octet_string(saltLen, salt, der,
           ccder_encode_uint64(iterationCount, der,
           ccder_encode_uint64(keyLength, der,
           der_encode_SecAsn1Oid(&CSSMOID_PKCS5_HMAC_SHA1, der, der_end)))));
}

static const uint8_t *der_decode_pbkdf2_params(size_t *saltLen, const uint8_t **salt,
                                               unsigned long *iterationCount,
                                               unsigned long *keyLength,
                                               const uint8_t *der, const uint8_t *der_end)
{
    const uint8_t * body_end = NULL;
    der = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &body_end, der, der_end);

    if (body_end != der_end)
        der = NULL;

    size_t salt_size = 0;
    const uint8_t *salt_bytes = NULL;

    der = ccder_decode_tl(CCDER_OCTET_STRING, &salt_size, der, body_end);
    salt_bytes = der;
    der += salt_size;

    uint64_t iteration_count = 0;
    uint64_t key_len = 0;
    der = ccder_decode_uint64(&iteration_count, der, body_end);
    if (iteration_count > UINT32_MAX)
        der = NULL;

    der = ccder_decode_uint64(&key_len, der, body_end);
    if (key_len > UINT32_MAX)
        der = NULL;

    der = der_expect_SecAsn1Oid(&CSSMOID_PKCS5_HMAC_SHA1, der, body_end);

    if (der) {
        if (salt)
            *salt = salt_bytes;
        if (saltLen)
            *saltLen = salt_size;
        if (iterationCount)
            *iterationCount = (unsigned long)iteration_count;
        if (keyLength)
            *keyLength = (unsigned long) key_len;
    }

    return der;
}


static SecKeyRef ccec2SecKey(ccec_full_ctx_t fk)
{
    size_t export_size = ccec_x963_export_size(1, fk);
    uint8_t export_keybytes[export_size];
    ccec_x963_export(1, export_keybytes, fk);
    CFDataRef exportedkey = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, export_keybytes, export_size, kCFAllocatorNull);

    CFDictionaryRef keyattributes = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                                 kSecValueData, exportedkey,
                                                                 kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                 kSecAttrKeyClass, kSecAttrKeyClassPrivate,
                                                                 NULL);

    SecKeyRef retval = SecKeyCreateFromAttributeDictionary(keyattributes);

    CFRelease(keyattributes);
    CFRelease(exportedkey);
    bzero(export_keybytes, 0);
    return retval;
}

#define SALTMAX 16
#define ITERATIONMIN 50000

CFDataRef SOSUserKeyCreateGenerateParameters(CFErrorRef *error) {
    size_t saltlen = SALTMAX;
    uint8_t salt[saltlen];

    size_t iterations = ITERATIONMIN;
    size_t keysize = 256;

    if(CCRandomCopyBytes(kCCRandomDefault, salt, sizeof(salt)) != kCCSuccess) {
        SOSCreateError(kSOSErrorProcessingFailure, CFSTR("CCRandomCopyBytes failed"), NULL, error);
        return NULL;
    }

    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataSetLength(result, der_sizeof_pbkdf2_params(saltlen, salt, iterations, keysize));

    uint8_t * encode = der_encode_pbkdf2_params(saltlen, salt, iterations, keysize,
                                                CFDataGetBytePtr(result),
                                                CFDataGetMutableBytePtr(result) + CFDataGetLength(result));

    if (!encode)
        CFReleaseNull(result);

    if (result) {
        secnotice("keygen", "Created new parameters: iterations %zd, keysize %zd: %@", iterations, keysize, result);
    }

    return result;
}

SecKeyRef SOSUserKeygen(CFDataRef password, CFDataRef parameters, CFErrorRef *error)
{
    size_t saltlen;
    const uint8_t *salt = NULL;

    size_t iterations = 0;
    size_t keysize = 0;

    const uint8_t *der = CFDataGetBytePtr(parameters);
    const uint8_t *der_end = der + CFDataGetLength(parameters);

    der = der_decode_pbkdf2_params(&saltlen, &salt, &iterations, &keysize, der, der_end);

    if (der == NULL) {
        SOSCreateErrorWithFormat(kSOSErrorDecodeFailure, NULL, error, NULL,
                                 CFSTR("Bad paramter encoding, got: %@"), parameters);
        return NULL;
    }
    if (keysize != 256) {
        SOSCreateErrorWithFormat(kSOSErrorUnsupported, NULL, error, NULL,
                                 CFSTR("Key size not supported, requested %zd."), keysize);
        return NULL;
    }
    if (saltlen < 4) {
        SOSCreateErrorWithFormat(kSOSErrorUnsupported, NULL, error, NULL,
                                 CFSTR("Salt length not supported, requested %zd."), saltlen);
        return NULL;
    }
    if (iterations < ITERATIONMIN) {
        SOSCreateErrorWithFormat(kSOSErrorUnsupported, NULL, error, NULL,
                                 CFSTR("Too few iterations, params suggested %zd."), iterations);
        return NULL;
    }

    const uint8_t *password_bytes = CFDataGetBytePtr(password);
    size_t password_length = CFDataGetLength(password);

    const size_t maxbytes = 128;

    ccec_const_cp_t cp = ccec_get_cp(keysize);
    struct ccrng_pbkdf2_prng_state pbkdf2_prng;

    ccec_full_ctx_decl_cp(cp, tmpkey);

    secnotice("keygen", "Generating key for: iterations %zd, keysize %zd: %@", iterations, keysize, parameters);

    if (ccrng_pbkdf2_prng_init(&pbkdf2_prng, maxbytes,
                                password_length, password_bytes,
                                saltlen, salt,
                                iterations)) {
        SOSCreateError(kSOSErrorProcessingFailure, CFSTR("prng init failed"), NULL, error);
        return NULL;
    }

    if (ccec_generate_key(cp, (struct ccrng_state *)&pbkdf2_prng, tmpkey)) {
        SOSCreateError(kSOSErrorProcessingFailure, CFSTR("Keygen failed"), NULL, error);
        return NULL;
    }


    return ccec2SecKey(tmpkey);
}

void debugDumpUserParameters(CFStringRef message, CFDataRef parameters)
{
    size_t saltlen = 0;
    const uint8_t *salt = NULL;
    
    size_t iterations = 0;
    size_t keysize = 0;
    
    const uint8_t *der = CFDataGetBytePtr(parameters);
    const uint8_t *der_end = der + CFDataGetLength(parameters);
    
    der = der_decode_pbkdf2_params(&saltlen, &salt, &iterations, &keysize, der, der_end);
    if (der == NULL) {
        secnotice("keygen", "failed to decode pbkdf2 params");
        return;
    }
    
    BufferPerformWithHexString(salt, saltlen, ^(CFStringRef saltHex) {
        CFDataPerformWithHexString(parameters, ^(CFStringRef parametersHex) {
            secnotice("keygen", "%@ <Params: count: %zd, keysize: %zd, salt: %@, raw: %@>]", message, iterations, keysize, saltHex, parametersHex);
        });
    });
}


