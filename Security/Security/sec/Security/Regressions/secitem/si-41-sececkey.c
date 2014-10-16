/*
 * Copyright (c) 2011-2014 Apple Inc. All Rights Reserved.
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
 *  si-40-seckey.c
 *  Security
 *
 *  Copyright (c) 2007-2009,2012-2014 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecAsn1Types.h>
#include <Security/oidsalg.h>
#include <Security/SecureTransport.h>
#include <Security/SecRandom.h>
#include <utilities/array_size.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <CommonCrypto/CommonDigest.h>
#include <libDER/libDER.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"

static void testdigestandsignalg(SecKeyRef privKey, SecKeyRef pubKey, const SecAsn1AlgId *algId) {
    uint8_t dataToDigest[256] = {0,};
    size_t dataToDigestLen = sizeof(dataToDigest);
    size_t sigLen = SecKeyGetSize(privKey, kSecKeySignatureSize);
    uint8_t sig[sigLen];

    DERItem oid;
    oid.length = algId->algorithm.Length;
    oid.data = algId->algorithm.Data;

    /* Get the oid in decimal for display purposes. */
    CFStringRef oidStr = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, &oid);
	char oidBuf[40];
    CFStringGetCString(oidStr, oidBuf, sizeof(oidBuf), kCFStringEncodingUTF8);
    CFRelease(oidStr);

SKIP: {
    OSStatus status;

    /* Time to sign. */
    ok_status(status = SecKeyDigestAndSign(privKey, algId, dataToDigest, dataToDigestLen,
                                           sig, &sigLen),
              "digest and sign %s with %ld bit RSA key", oidBuf, sigLen * 8);

    skip("SecKeyDigestAndSign failed", 3, status == errSecSuccess);

    /* Verify the signature we just made. */
    ok_status(SecKeyDigestAndVerify(pubKey, algId, dataToDigest, dataToDigestLen,
                                    sig, sigLen), "digest and verify");
    /* Invalidate the signature. */
    sig[0] ^= 0xff;
    is_status(SecKeyDigestAndVerify(pubKey, algId, dataToDigest, dataToDigestLen,
                                    sig, sigLen), errSSLCrypto, "digest and verify bad sig");
    sig[0] ^= 0xff;
    dataToDigest[0] ^= 0xff;
    is_status(SecKeyDigestAndVerify(pubKey, algId, dataToDigest, dataToDigestLen,
                                    sig, sigLen), errSSLCrypto, "digest and verify bad digest");
}
}

static void testdigestandsign(SecKeyRef privKey, SecKeyRef pubKey) {
    static const SecAsn1Oid *oids[] = {
        &CSSMOID_ECDSA_WithSHA1,
#if 0
        &CSSMOID_ECDSA_WithSHA224,
        &CSSMOID_ECDSA_WithSHA256,
        &CSSMOID_ECDSA_WithSHA384,
        &CSSMOID_ECDSA_WithSHA512,
#endif
    };

    uint32_t ix;
    SecAsn1AlgId algId = {};
    for (ix = 0; ix < array_size(oids); ++ix) {
        if (oids[ix]) {
            algId.algorithm = *oids[ix];
        } else {
            algId.algorithm.Length = 0;
            algId.algorithm.Data = NULL;
        }

        testdigestandsignalg(privKey, pubKey, &algId);
    }
}

static void testkeygen(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
	size_t keySizeInBytes = (keySizeInBits + 7) / 8;
	CFNumberRef kzib;
    int32_t keysz32 = (int32_t)keySizeInBits;

	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
	CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeEC);
	CFDictionaryAddValue(kgp, kSecAttrKeySizeInBits, kzib);

	OSStatus status;
	ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) EC keypair", keySizeInBits,
              keySizeInBytes);
	CFRelease(kzib);
	CFRelease(kgp);

SKIP: {
    skip("keygen failed", 8, status == errSecSuccess);
    ok(pubKey, "pubkey returned");
    ok(privKey, "privKey returned");
    is(SecKeyGetSize(pubKey, kSecKeyKeySizeInBits), (size_t) keySizeInBits, "public key size is ok");
    is(SecKeyGetSize(privKey, kSecKeyKeySizeInBits), (size_t) keySizeInBits, "private key size is ok");

    /* Sign something. */
    uint8_t something[20] = {0x80, 0xbe, 0xef, 0xba, 0xd0, };
    uint8_t sig[8+2*keySizeInBytes];
    size_t sigLen = sizeof(sig);
    ok_status(SecKeyRawSign(privKey, kSecPaddingNone,
                            something, sizeof(something), sig, &sigLen), "sign something");
    ok_status(SecKeyRawVerify(pubKey, kSecPaddingNone,
                              something, sizeof(something), sig, sigLen), "verify sig on something");

    testdigestandsign(privKey, pubKey);

    const void *privkeys[] = {
        kSecValueRef
    };
    const void *privvalues[] = {
        privKey
    };
    CFDictionaryRef privitem = CFDictionaryCreate(NULL, privkeys, privvalues,
                                                  array_size(privkeys), NULL, NULL);
    ok_status(SecItemAdd(privitem, NULL), "add private key");
    ok_status(SecItemDelete(privitem), "delete public key");
    CFReleaseNull(privitem);

    const void *pubkeys[] = {
        kSecValueRef
    };
    const void *pubvalues[] = {
        pubKey
    };
    CFDictionaryRef pubitem = CFDictionaryCreate(NULL, pubkeys, pubvalues,
                                                 array_size(pubkeys), NULL, NULL);
    ok_status(SecItemAdd(pubitem, NULL), "add public key");
    ok_status(SecItemDelete(pubitem), "delete public key");
    CFReleaseNull(pubitem);

    /* Cleanup. */
    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}
}


static void testkeygen2(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
	size_t keySizeInBytes = (keySizeInBits + 7) / 8;
	CFNumberRef kzib;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFUUIDRef ourUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, ourUUID);
    CFMutableStringRef publicName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);
    CFMutableStringRef privateName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);

    CFReleaseNull(ourUUID);
    CFReleaseNull(uuidString);

    CFStringAppend(publicName, CFSTR("-Public-41"));
    CFStringAppend(privateName, CFSTR("-Private-41"));

    CFMutableDictionaryRef pubd = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                          kSecAttrLabel, publicName,
                                                                          NULL);
    CFMutableDictionaryRef privd = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                           kSecAttrLabel, privateName,
                                                                           NULL);

	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanTrue,
                                                       kSecPublicKeyAttrs, pubd,
                                                       kSecPrivateKeyAttrs, privd,
                                                       NULL);

    CFReleaseNull(kzib);

	OSStatus status;
	ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) persistent RSA keypair",
              keySizeInBits, keySizeInBytes);

    CFReleaseNull(kgp);

SKIP: {
    skip("keygen failed", 8, status == errSecSuccess);
    ok(pubKey, "pubkey returned");
    ok(privKey, "privKey returned");
    is(SecKeyGetSize(pubKey, kSecKeyKeySizeInBits), (size_t) keySizeInBits, "public key size is ok");
    is(SecKeyGetSize(privKey, kSecKeyKeySizeInBits), (size_t) keySizeInBits, "private key size is ok");

    SecKeyRef pubKey2, privKey2;
    CFDictionaryAddValue(pubd, kSecClass, kSecClassKey);
    CFDictionaryAddValue(pubd, kSecReturnRef, kCFBooleanTrue);
    CFDictionaryAddValue(privd, kSecClass, kSecClassKey);
    CFDictionaryAddValue(privd, kSecReturnRef, kCFBooleanTrue);
    CFDictionaryAddValue(privd, kSecAttrCanSign, kCFBooleanTrue);
    ok_status(SecItemCopyMatching(pubd, (CFTypeRef *)&pubKey2),
              "retrieve pub key by label");
    ok(pubKey2, "got valid object");
    ok_status(SecItemCopyMatching(privd, (CFTypeRef *)&privKey2),
              "retrieve priv key by label and kSecAttrCanSign");
    ok(privKey2, "got valid object");

    /* Sign something. */
    uint8_t something[20] = {0x80, 0xbe, 0xef, 0xba, 0xd0, };
    size_t sigLen = SecKeyGetSize(privKey2, kSecKeySignatureSize);
    uint8_t sig[sigLen];
    ok_status(SecKeyRawSign(privKey2, kSecPaddingPKCS1,
                            something, sizeof(something), sig, &sigLen), "sign something");
    ok_status(SecKeyRawVerify(pubKey2, kSecPaddingPKCS1,
                              something, sizeof(something), sig, sigLen), "verify sig on something");

    /* Cleanup. */
    CFReleaseNull(pubKey2);
    CFReleaseNull(privKey2);
}

    /* delete from keychain - note: do it before releasing publicName and privateName
       because pubd and privd have no retain/release callbacks */
    ok_status(SecItemDelete(pubd), "delete generated pub key");
    ok_status(SecItemDelete(privd), "delete generated priv key");

	/* Cleanup. */
	CFReleaseNull(pubKey);
	CFReleaseNull(privKey);

    CFReleaseNull(publicName);
    CFReleaseNull(privateName);

	CFReleaseNull(pubd);
	CFReleaseNull(privd);
}

static void testkeywrap(unsigned long keySizeInBits, CFTypeRef alg)
{
    SecKeyRef pubKey = NULL, privKey = NULL;
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    CFNumberRef kzib;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFUUIDRef ourUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, ourUUID);
    CFMutableStringRef publicName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);
    CFMutableStringRef privateName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);

    CFReleaseNull(ourUUID);
    CFReleaseNull(uuidString);

    CFStringAppend(publicName, CFSTR("-Public-41"));
    CFStringAppend(privateName, CFSTR("-Private-41"));

    CFDictionaryRef pubd = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                        kSecAttrLabel, publicName,
                                                        NULL);
    CFDictionaryRef privd = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         kSecAttrLabel, privateName,
                                                         NULL);

    CFReleaseNull(publicName);
    CFReleaseNull(privateName);

    kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       kSecPublicKeyAttrs, pubd,
                                                       kSecPrivateKeyAttrs, privd,
                                                       NULL);
    CFReleaseNull(pubd);
    CFReleaseNull(privd);
    CFReleaseNull(kzib);

    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) persistent RSA keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    CFErrorRef localError;

    CFDataRef secret = CFDataCreate(NULL, (void *)"0123456789012345", 16);
    ok(secret, "secret");

    CFDataRef fp = CFDataCreate(NULL, (void *)"01234567890123456789", 20);
    ok(fp, "fingerprint");


    int8_t sym_alg_data = 8;
    CFNumberRef symalg = CFNumberCreate(NULL, kCFNumberSInt8Type, &sym_alg_data);
    CFDictionaryRef param = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                         _kSecKeyWrapPGPWrapAlg, alg,
                                                         _kSecKeyWrapPGPSymAlg, symalg,
                                                         _kSecKeyWrapPGPFingerprint, fp,
                                                         NULL);

    CFDataRef wrapped = _SecKeyCopyWrapKey(pubKey, kSecKeyWrapPublicKeyPGP, secret, param, NULL, &localError);
    ok(wrapped, "wrap key: %@", localError);

    CFDataRef unwrapped = _SecKeyCopyUnwrapKey(privKey, kSecKeyWrapPublicKeyPGP, wrapped, param, NULL, &localError);
    ok(unwrapped, "unwrap key: %@", localError);

    CFReleaseNull(symalg);

    ok(CFEqual(unwrapped, secret), "keys still same");

    CFReleaseNull(fp);
    CFReleaseNull(secret);
    CFReleaseNull(unwrapped);
    CFReleaseNull(wrapped);
    CFReleaseNull(param);
    CFReleaseNull(privKey);
    CFReleaseNull(pubKey);
}




/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
	testkeygen(192);
	testkeygen(224);
	testkeygen(256);
	testkeygen(384);
	testkeygen(521);

    testkeygen2(192);
    testkeygen2(224);
	testkeygen2(256);
	testkeygen2(384);
	testkeygen2(521);

    testkeywrap(256, _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128);
    testkeywrap(521, _kSecKeyWrapRFC6637WrapDigestSHA256KekAES128);
    testkeywrap(256, _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256);
    testkeywrap(521, _kSecKeyWrapRFC6637WrapDigestSHA512KekAES256);

}

int si_41_sececkey(int argc, char *const *argv)
{
	plan_tests(164);

	tests();

	return 0;
}
