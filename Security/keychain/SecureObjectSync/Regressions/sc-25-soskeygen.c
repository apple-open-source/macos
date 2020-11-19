//
//  sc-25-soskeygen.c
//  sec
//
//  Created by Richard Murphy on 6/1/15.
//
//

/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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



#include <Security/SecBase.h>
#include <Security/SecItem.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>

#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSUserKeygen.h"

#include <utilities/SecCFWrappers.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>
#include <unistd.h>
#include "SOSCircle_regressions.h"

#include "SOSRegressionUtilities.h"

#if SOS_ENABLED

#include <corecrypto/ccrng.h>
#include <corecrypto/ccrng_pbkdf2_prng.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccdigest.h>
#include <corecrypto/ccsha2.h>
#include <corecrypto/ccder.h>
#include <Security/oidsalg.h>


#if TARGET_OS_WATCH
#define NPARMS 3
#define NKEYS 3
#else
#define NPARMS 10
#define NKEYS 10
#endif


#define TESTCOMPAT 1
#if TESTCOMPAT == 1

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
#define SALTMAX 16
#define ITERATIONMIN 50000

static SecKeyRef ccec2SecKey(ccec_full_ctx_t fk)
{
    size_t export_size = ccec_x963_export_size(1, ccec_ctx_pub(fk));
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
    cc_clear(export_size, export_keybytes);
    return retval;
}

static SecKeyRef SOSOldUserKeygen(CFDataRef password, CFDataRef parameters, CFErrorRef *error)
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

    debugDumpUserParameters(CFSTR("sc_25_soskeygen: Generating key for:"), parameters);

    if (ccrng_pbkdf2_prng_init(&pbkdf2_prng, maxbytes,
                               password_length, password_bytes,
                               saltlen, salt,
                               iterations)) {
        SOSCreateError(kSOSErrorProcessingFailure, CFSTR("prng init failed"), NULL, error);
        return NULL;
    }

    if (ccec_generate_key_legacy(cp, (struct ccrng_state *)&pbkdf2_prng, tmpkey)) {
        SOSCreateError(kSOSErrorProcessingFailure, CFSTR("Keygen failed"), NULL, error);
        return NULL;
    }

    return ccec2SecKey(tmpkey);
}


static SecKeyRef createOldTestKey(CFDataRef cfpassword, CFDataRef parameters, CFErrorRef *error) {
    SecKeyRef user_privkey = SOSOldUserKeygen(cfpassword, parameters, error);
    ok(user_privkey, "No key!");
    ok(*error == NULL, "Error: (%@)", *error);
    CFReleaseNull(*error);
    return user_privkey;
}

#endif /* TESTCOMPAT */

static SecKeyRef createTestKey(CFDataRef cfpassword, CFDataRef parameters, CFErrorRef *error) {
    SecKeyRef user_privkey = SOSUserKeygen(cfpassword, parameters, error);
    ok(user_privkey, "No key!");
    ok(*error == NULL, "Error: (%@)", *error);
    CFReleaseNull(*error);
    return user_privkey;
}

static void tests(void) {
    CFErrorRef error = NULL;
    CFDataRef cfpassword = CFDataCreate(NULL, (uint8_t *) "FooFooFoo", 10);
    
    for(int j=0; j < NPARMS; j++) {
        CFDataRef parameters = SOSUserKeyCreateGenerateParameters(&error);
        ok(parameters, "No parameters!");
        ok(error == NULL, "Error: (%@)", error);
        CFReleaseNull(error);

        SecKeyRef baseline_privkey = createTestKey(cfpassword, parameters, &error);
        ok(baseline_privkey != NULL, "Failed to create baseline privkey");
        if(baseline_privkey) {
            SecKeyRef baseline_pubkey = SecKeyCreatePublicFromPrivate(baseline_privkey);

            for(int i = 0; i < NKEYS; i++) {
                SecKeyRef user_privkey = createTestKey(cfpassword, parameters, &error);
                SecKeyRef user_pubkey = SecKeyCreatePublicFromPrivate(user_privkey);
                ok(CFEqualSafe(baseline_privkey, user_privkey), "Private Keys Don't Match");
                ok(CFEqualSafe(baseline_pubkey, user_pubkey), "Public Keys Don't Match");
#if TESTCOMPAT == 1
                SecKeyRef old_privkey = createOldTestKey(cfpassword, parameters, &error);
                SecKeyRef old_pubkey = SecKeyCreatePublicFromPrivate(old_privkey);
                ok(CFEqualSafe(old_privkey, user_privkey), "Old/New Private Keys Don't Match");
                ok(CFEqualSafe(old_pubkey, user_pubkey), "Old/New Public Keys Don't Match");
                CFReleaseNull(old_privkey);
                CFReleaseNull(old_pubkey);
#endif /* TESTCOMPAT */
                CFReleaseNull(error);
                CFReleaseNull(user_privkey);
                CFReleaseNull(user_pubkey);
            }
            CFReleaseNull(baseline_pubkey);
        }
        CFReleaseNull(baseline_privkey);
        CFReleaseNull(parameters);
    }
    CFReleaseNull(cfpassword);
}

#endif

int sc_25_soskeygen(int argc, char *const *argv)
{
#if SOS_ENABLED
    plan_tests(850);
    tests();
#else
    plan_tests(0);
#endif
    return 0;
}
