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
#include <Security/SecECKey.h>
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
#include <corecrypto/ccsha2.h>

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
    if (privKey2 != NULL && pubKey2 != NULL) {
        ok_status(SecKeyRawSign(privKey2, kSecPaddingPKCS1,
                                something, sizeof(something), sig, &sigLen), "sign something");
        ok_status(SecKeyRawVerify(pubKey2, kSecPaddingPKCS1,
                                  something, sizeof(something), sig, sigLen), "verify sig on something");
    }

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

    CFErrorRef localError = NULL;

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

const uint8_t EC_P256_KeyDER[]={
    0x30, 0x6b, 0x02, 0x01, 0x01, 0x04, 0x20, 0x86, 0x87, 0x79, 0x59, 0xd1,
    0xc6, 0x3c, 0x50, 0x24, 0x30, 0xa4, 0xaf, 0x89, 0x1d, 0xd1, 0x94, 0x23,
    0x56, 0x79, 0x46, 0x93, 0x72, 0x31, 0x39, 0x24, 0xe6, 0x01, 0x96, 0xc8,
    0xeb, 0xf3, 0x88, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04, 0x8c, 0xfa, 0xd7,
    0x8a, 0xf1, 0xb9, 0xad, 0xd7, 0x3a, 0x33, 0xb5, 0x9a, 0xad, 0x52, 0x0d,
    0x14, 0xd6, 0x6b, 0x35, 0x56, 0x79, 0xd6, 0x74, 0x2a, 0x37, 0x7e, 0x2f,
    0x33, 0xa6, 0xab, 0xee, 0x35, 0x00, 0x70, 0x82, 0x89, 0x9c, 0xfc, 0x97,
    0xc4, 0x89, 0x5c, 0x16, 0x50, 0xad, 0x60, 0x55, 0xa6, 0x70, 0xee, 0x07,
    0x1b, 0xfe, 0xe4, 0xf0, 0xa0, 0x63, 0xc0, 0x73, 0x24, 0x97, 0x92, 0x04,
    0xc7};

const uint8_t EC_P256_SigDER[]={
    0x30, 0x45, 0x02, 0x20, 0x4b, 0x37, 0x7f, 0x45, 0xd0, 0x5d, 0xa6, 0x53,
    0xb3, 0x62, 0x6f, 0x32, 0xdb, 0xfc, 0xf6, 0x3b, 0x84, 0xfa, 0x5a, 0xd9,
    0x17, 0x67, 0x03, 0x73, 0x48, 0x0c, 0xad, 0x89, 0x13, 0x69, 0x61, 0xb3,
    0x02, 0x21, 0x00, 0xd6, 0x23, 0xaf, 0xd9, 0x7d, 0x72, 0xba, 0x3b, 0x90,
    0xc1, 0x23, 0x7d, 0xdb, 0x2c, 0xd1, 0x0d, 0xbb, 0xb4, 0x0f, 0x67, 0x26,
    0xff, 0x3f, 0xa6, 0x47, 0xa4, 0x13, 0x0d, 0xe0, 0x45, 0xd5, 0x6b};

const uint8_t EC_P256_SigRaw[]= {
    0x4b, 0x37, 0x7f, 0x45, 0xd0, 0x5d, 0xa6, 0x53, 0xb3, 0x62, 0x6f, 0x32,
    0xdb, 0xfc, 0xf6, 0x3b, 0x84, 0xfa, 0x5a, 0xd9, 0x17, 0x67, 0x03, 0x73,
    0x48, 0x0c, 0xad, 0x89, 0x13, 0x69, 0x61, 0xb3, 0xd6, 0x23, 0xaf, 0xd9,
    0x7d, 0x72, 0xba, 0x3b, 0x90, 0xc1, 0x23, 0x7d, 0xdb, 0x2c, 0xd1, 0x0d,
    0xbb, 0xb4, 0x0f, 0x67, 0x26, 0xff, 0x3f, 0xa6, 0x47, 0xa4, 0x13, 0x0d,
    0xe0, 0x45, 0xd5, 0x6b};

const uint8_t EC_SigDigest[24] = "012345678912345678901234";

static void testsignformat(void)
{
    SecKeyRef pkey = NULL;
    SecKeyRef pubkey = NULL;
    CFDataRef pubdata = NULL;
    uint8_t EC_signature_DER[72];
    uint8_t EC_signature_RAW[64];
    size_t EC_signature_DER_size=sizeof(EC_signature_DER);
    size_t EC_signature_RAW_size=sizeof(EC_signature_RAW);

    ok((pkey = SecKeyCreateECPrivateKey(kCFAllocatorDefault,
                                       EC_P256_KeyDER, sizeof(EC_P256_KeyDER),
                                       kSecKeyEncodingPkcs1)) != NULL, "import privkey");

    ok_status(SecKeyCopyPublicBytes(pkey, &pubdata), "pub key from priv key");

    ok((pubkey = SecKeyCreateECPublicKey(kCFAllocatorDefault,
                                        CFDataGetBytePtr(pubdata), CFDataGetLength(pubdata),
                                        kSecKeyEncodingBytes))!=NULL,
       "recreate seckey");

    if (pubkey != NULL && pkey != NULL) {
        // Verify fixed signature
        ok_status(SecKeyRawVerify(pubkey, kSecPaddingPKCS1,
                                  EC_SigDigest, sizeof(EC_SigDigest), EC_P256_SigDER, sizeof(EC_P256_SigDER)), "verify DER sig on something");

        ok_status(SecKeyRawVerify(pubkey, kSecPaddingSigRaw,
                                  EC_SigDigest, sizeof(EC_SigDigest), EC_P256_SigRaw, sizeof(EC_P256_SigRaw)), "verify RAW sig on something");

        // Verify signature with mismatching format
        ok_status(!SecKeyRawVerify(pubkey, kSecPaddingSigRaw,
                                   EC_SigDigest, sizeof(EC_SigDigest), EC_P256_SigDER, sizeof(EC_P256_SigDER)), "verify DER sig with RAW option");

        ok_status(!SecKeyRawVerify(pubkey, kSecPaddingPKCS1,
                                   EC_SigDigest, sizeof(EC_SigDigest), EC_P256_SigRaw, sizeof(EC_P256_SigRaw)), "verify RAW sig with DER something");

        // Sign something in each format
        ok_status(SecKeyRawSign(pkey, kSecPaddingPKCS1,
                                EC_SigDigest, sizeof(EC_SigDigest), EC_signature_DER, &EC_signature_DER_size), "sign DER sig on something");

        ok_status(SecKeyRawSign(pkey, kSecPaddingSigRaw,
                                EC_SigDigest, sizeof(EC_SigDigest), EC_signature_RAW, &EC_signature_RAW_size), "sign RAW sig on something");

        // Verify expecting that verification does the right thing.
        ok_status(SecKeyRawVerify(pubkey, kSecPaddingPKCS1,
                                  EC_SigDigest, sizeof(EC_SigDigest), EC_signature_DER, EC_signature_DER_size), "verify DER sig on something");

        ok_status(SecKeyRawVerify(pubkey, kSecPaddingSigRaw,
                                  EC_SigDigest, sizeof(EC_SigDigest), EC_signature_RAW, EC_signature_RAW_size), "verify RAW sig on something");
    }

    CFReleaseNull(pkey);
    CFReleaseNull(pubkey);
    CFReleaseNull(pubdata);
}

static void testkeyexchange(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    CFNumberRef kzib;
    int32_t keysz32 = (int32_t)keySizeInBits;

    kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    CFReleaseNull(kzib);

    SecKeyRef pubKey1 = NULL, privKey1 = NULL;
    SecKeyRef pubKey2 = NULL, privKey2 = NULL;

    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey1, &privKey1),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey2, &privKey2),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    const SecKeyAlgorithm algos[] = {
        kSecKeyAlgorithmECDHKeyExchangeStandard,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA224,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA256,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA384,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA512,
        kSecKeyAlgorithmECDHKeyExchangeCofactor,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA1,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA224,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA256,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA384,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA512,
    };

    // Strange size to test borderline conditions.
    CFIndex rs = 273;
    CFNumberRef requestedSize = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &rs);
    CFDataRef sharedInfo = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)"sharedInput", 11);
    CFDictionaryRef params = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                          kSecKeyKeyExchangeParameterRequestedSize, requestedSize,
                                                          kSecKeyKeyExchangeParameterSharedInfo, sharedInfo,
                                                          NULL);
    CFRelease(requestedSize);
    CFRelease(sharedInfo);

    for (size_t ix = 0; ix < array_size(algos); ++ix) {
        CFErrorRef error = NULL;

        CFDataRef secret1 = SecKeyCopyKeyExchangeResult(privKey1, algos[ix], pubKey2, params, &error);
        ok(secret1 != NULL && CFGetTypeID(secret1) == CFDataGetTypeID());
        CFReleaseNull(error);

        CFDataRef secret2 = SecKeyCopyKeyExchangeResult(privKey2, algos[ix], pubKey1, params, &error);
        ok(secret2 != NULL && CFGetTypeID(secret1) == CFDataGetTypeID());
        CFReleaseNull(error);

        eq_cf(secret1, secret2, "results of key exchange are equal");
        if (algos[ix] != kSecKeyAlgorithmECDHKeyExchangeCofactor && algos[ix] != kSecKeyAlgorithmECDHKeyExchangeStandard) {
            is(CFDataGetLength(secret1), rs, "generated response has expected length");
        }

        CFReleaseNull(secret1);
        CFReleaseNull(secret2);
    }

    CFReleaseNull(privKey1);
    CFReleaseNull(pubKey1);
    CFReleaseNull(privKey2);
    CFReleaseNull(pubKey2);
    CFReleaseNull(params);
}

static void testsupportedalgos(size_t keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    CFNumberRef kzib;
    int32_t keysz32 = (int32_t)keySizeInBits;

    kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    CFReleaseNull(kzib);

    SecKeyRef pubKey = NULL, privKey = NULL;

    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);

    const SecKeyAlgorithm sign[] = {
        kSecKeyAlgorithmECDSASignatureRFC4754,
        kSecKeyAlgorithmECDSASignatureDigestX962,
        kSecKeyAlgorithmECDSASignatureDigestX962SHA1,
        kSecKeyAlgorithmECDSASignatureDigestX962SHA224,
        kSecKeyAlgorithmECDSASignatureDigestX962SHA256,
        kSecKeyAlgorithmECDSASignatureDigestX962SHA384,
        kSecKeyAlgorithmECDSASignatureDigestX962SHA512,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA1,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA512,
    };

    for (size_t i = 0; i < array_size(sign); i++) {
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeSign, sign[i]),
           "privKey supports sign algorithm %@", sign[i]);
        ok(SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeVerify, sign[i]),
           "pubKey supports verify algorithm %@", sign[i]);
        ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeVerify, sign[i]),
           "privKey doesn't supports verify algorithm %@", sign[i]);
        ok(!SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeSign, sign[i]),
           "pubKey doesn't support verify algorithm %@", sign[i]);
    }

    const SecKeyAlgorithm keyexchange[] = {
        kSecKeyAlgorithmECDHKeyExchangeStandard,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA1,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA224,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA256,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA384,
        kSecKeyAlgorithmECDHKeyExchangeStandardX963SHA512,
        kSecKeyAlgorithmECDHKeyExchangeCofactor,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA1,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA224,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA256,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA384,
        kSecKeyAlgorithmECDHKeyExchangeCofactorX963SHA512,
    };
    for (size_t i = 0; i < array_size(crypt); i++) {
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeKeyExchange, keyexchange[i]),
           "privKey supports keyexchange algorithm %@", keyexchange[i]);
        ok(!SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeKeyExchange, keyexchange[i]),
           "pubKey doesn't support keyexchange algorithm %@", keyexchange[i]);
    }

    /* Cleanup. */
    CFReleaseNull(kgp);
    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

static void testcreatewithdata(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    CFMutableDictionaryRef kcwd = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                          kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                                          kSecAttrKeySizeInBits, kzib,
                                                                          kSecAttrIsPermanent, kCFBooleanFalse,
                                                                          NULL);
    CFReleaseNull(kzib);

    CFErrorRef error = NULL;
    CFDataRef privExternalData = NULL, pubExternalData = NULL;
    SecKeyRef dataKey = NULL;

    { // privKey
        privExternalData = SecKeyCopyExternalRepresentation(privKey, &error);
        ok(privExternalData && CFGetTypeID(privExternalData) == CFDataGetTypeID(),
           "priv key SecKeyCopyExternalRepresentation failed");
        CFReleaseNull(error);

        SKIP: {
            skip("invalid priv key external data", 4, privExternalData);

            CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
            dataKey = SecKeyCreateWithData(privExternalData, kcwd, &error);
            ok(dataKey, "priv key SecKeyCreateWithData failed");
            CFReleaseNull(error);

            eq_cf(privKey, dataKey, "priv keys differ");
            CFReleaseNull(dataKey);

            CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPublic);
            dataKey = SecKeyCreateWithData(privExternalData, kcwd, &error);
            ok(!dataKey, "priv key SecKeyCreateWithData succeeded with invalid kSecAttrKeyClass");
            CFReleaseNull(error);
            CFReleaseNull(dataKey);

            CFMutableDataRef modifiedExternalData = CFDataCreateMutable(kCFAllocatorDefault, 0);
            CFDataAppend(modifiedExternalData, privExternalData);
            *CFDataGetMutableBytePtr(modifiedExternalData) ^= 0xff;

            CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
            dataKey = SecKeyCreateWithData(modifiedExternalData, kcwd, &error);
            ok(!dataKey, "priv key SecKeyCreateWithData succeeded with invalid external data");
            CFReleaseNull(error);
            CFReleaseNull(dataKey);

            CFReleaseNull(modifiedExternalData);
        }
    }

    { // pubKey
        pubExternalData = SecKeyCopyExternalRepresentation(pubKey, &error);
        ok(pubExternalData && CFGetTypeID(pubExternalData) == CFDataGetTypeID(),
           "pub key SecKeyCopyExternalRepresentation failed");
        CFReleaseNull(error);

        SKIP: {
            skip("invalid pub key external data", 4, pubExternalData);

            CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPublic);
            dataKey = SecKeyCreateWithData(pubExternalData, kcwd, &error);
            ok(dataKey, "pub key SecKeyCreateWithData failed");
            CFReleaseNull(error);

            eq_cf(pubKey, dataKey, "pub keys differ");
            CFReleaseNull(dataKey);

            CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
            dataKey = SecKeyCreateWithData(pubExternalData, kcwd, &error);
            ok(!dataKey, "pub key SecKeyCreateWithData succeeded with invalid kSecAttrKeyClass");
            CFReleaseNull(error);
            CFReleaseNull(dataKey);

            CFMutableDataRef modifiedExternalData = CFDataCreateMutable(kCFAllocatorDefault, 0);
            CFDataAppend(modifiedExternalData, pubExternalData);
            *CFDataGetMutableBytePtr(modifiedExternalData) ^= 0xff;

            CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPublic);
            dataKey = SecKeyCreateWithData(modifiedExternalData, kcwd, &error);
            ok(!dataKey, "pub key SecKeyCreateWithData succeeded with invalid external data");
            CFReleaseNull(error);
            CFReleaseNull(dataKey);

            CFReleaseNull(modifiedExternalData);
        }
    }

    SKIP: {
        skip("invalid pub key external data", 1, pubExternalData);

        CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
        dataKey = SecKeyCreateWithData(pubExternalData, kcwd, &error);
        ok(!dataKey, "priv key SecKeyCreateWithData succeeded with public external data");
        CFReleaseNull(error);
        CFReleaseNull(dataKey);

        CFReleaseNull(pubExternalData);
    }

    SKIP: {
        skip("invalid priv key external data", 1, privExternalData);

        CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPublic);
        dataKey = SecKeyCreateWithData(privExternalData, kcwd, &error);
        ok(!dataKey, "pub key SecKeyCreateWithData succeeded with private external data");
        CFReleaseNull(error);
        CFReleaseNull(dataKey);

        CFReleaseNull(privExternalData);
    }

    CFReleaseNull(kcwd);
    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

static void testcopyattributes(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    CFDictionaryRef attributes;
    CFTypeRef attrValue = NULL, privAppLabel = NULL, pubAppLabel = NULL;

    { // privKey
        attributes = SecKeyCopyAttributes(privKey);
        ok(attributes && CFGetTypeID(attributes) == CFDictionaryGetTypeID(),
           "priv key SecKeyCopyAttributes failed");

        SKIP: {
            skip("invalid attributes", 8, attributes);

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanEncrypt);
            eq_cf(attrValue, kCFBooleanFalse, "invalid priv key kSecAttrCanEncrypt");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDecrypt);
            eq_cf(attrValue, kCFBooleanTrue, "invalid priv key kSecAttrCanDecrypt");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDerive);
            eq_cf(attrValue, kCFBooleanTrue, "invalid priv key kSecAttrCanDerive");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanSign);
            eq_cf(attrValue, kCFBooleanTrue, "invalid priv key kSecAttrCanSign");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanVerify);
            eq_cf(attrValue, kCFBooleanFalse, "invalid priv key kSecAttrCanVerify");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyClass);
            eq_cf(attrValue, kSecAttrKeyClassPrivate, "priv key invalid kSecAttrKeyClass");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyType);
            eq_cf(attrValue, kSecAttrKeyTypeEC, "invalid priv key kSecAttrKeyType");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeySizeInBits);
            eq_cf(attrValue, kzib, "invalid priv key kSecAttrKeySizeInBits");

            privAppLabel = CFDictionaryGetValue(attributes, kSecAttrApplicationLabel);
            CFRetainSafe(privAppLabel);

            CFReleaseNull(attributes);
        }
    }

    { // pubKey
        attributes = SecKeyCopyAttributes(pubKey);
        ok(attributes && CFGetTypeID(attributes) == CFDictionaryGetTypeID(),
           "pub key SecKeyCopyAttributes failed");

        SKIP: {
            skip("invalid attributes", 8, attributes);

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanEncrypt);
            eq_cf(attrValue, kCFBooleanTrue, "pub key invalid kSecAttrCanEncrypt");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDecrypt);
            eq_cf(attrValue, kCFBooleanFalse, "pub key invalid kSecAttrCanDecrypt");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDerive);
            eq_cf(attrValue, kCFBooleanFalse, "pub key invalid kSecAttrCanDerive");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanSign);
            eq_cf(attrValue, kCFBooleanFalse, "pub key invalid kSecAttrCanSign");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanVerify);
            eq_cf(attrValue, kCFBooleanTrue, "pub key invalid kSecAttrCanVerify");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyClass);
            eq_cf(attrValue, kSecAttrKeyClassPublic, "pub key invalid kSecAttrKeyClass");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyType);
            eq_cf(attrValue, kSecAttrKeyTypeEC, "pub key invalid kSecAttrKeyType");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeySizeInBits);
            eq_cf(attrValue, kzib, "pub key invalid kSecAttrKeySizeInBits");

            pubAppLabel = CFDictionaryGetValue(attributes, kSecAttrApplicationLabel);
            CFRetainSafe(pubAppLabel);

            CFReleaseNull(attributes);
        }
    }

    eq_cf(privAppLabel, pubAppLabel, "priv key and pub key kSecAttrApplicationLabel differ");

    CFReleaseNull(privAppLabel);
    CFReleaseNull(pubAppLabel);
    CFReleaseNull(kzib);
    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

static void testcopypublickey(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    CFReleaseNull(kzib);

    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    SecKeyRef pubKeyCopy = NULL;

    { // privKey
        pubKeyCopy = SecKeyCopyPublicKey(privKey);
        ok(pubKeyCopy, "priv key SecKeyCopyPublicKey failed");
        eq_cf(pubKeyCopy, pubKey, "pub key from priv key SecKeyCopyPublicKey and pub key differ");
        CFReleaseNull(pubKeyCopy);
    }

    { // pubKey
        pubKeyCopy = SecKeyCopyPublicKey(pubKey);
        ok(pubKeyCopy, "pub key SecKeyCopyPublicKey failed");
        eq_cf(pubKeyCopy, pubKey, "pub key from pub key SecKeyCopyPublicKey and pub key differ");
        CFReleaseNull(pubKeyCopy);
    }

    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

static void testsignverify(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeEC,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    CFReleaseNull(kzib);

    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    SecKeyAlgorithm algorithms[] = {
        kSecKeyAlgorithmECDSASignatureRFC4754,
        kSecKeyAlgorithmECDSASignatureDigestX962,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA1,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA224,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA256,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA384,
        kSecKeyAlgorithmECDSASignatureMessageX962SHA512
    };

    CFDataRef testData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("test"), kCFStringEncodingUTF8, 0);
    ok(testData, "creating test data failed");

    SKIP: {
        skip("invalid test data", 51, status == errSecSuccess && testData);

        CFErrorRef error = NULL;

        for (uint32_t ix = 0; ix < array_size(algorithms); ++ix) {
            SecKeyAlgorithm algorithm = algorithms[ix];
            SecKeyAlgorithm incompatibleAlgorithm = CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureRFC4754) ?
            kSecKeyAlgorithmECDSASignatureDigestX962 : kSecKeyAlgorithmECDSASignatureRFC4754;

            CFDataRef dataToSign = NULL;
            if (CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureRFC4754) ||
                CFEqual(algorithm, kSecKeyAlgorithmECDSASignatureDigestX962)) {
                dataToSign = CFDataCreateWithHash(kCFAllocatorDefault, ccsha256_di(),
                                                  CFDataGetBytePtr(testData), CFDataGetLength(testData));
                ok(dataToSign, "creating digest failed for algorithm %d", (int)algorithm);
                CFReleaseNull(error);
            }
            else {
                CFRetainAssign(dataToSign, testData);
            }

            SKIP: {
                skip("invalid data to sign", 7, dataToSign != NULL);

                CFDataRef signature = SecKeyCreateSignature(pubKey, algorithm, dataToSign, &error);
                ok(!signature, "SecKeyCopySignature succeeded with pub key for algorithm %d", (int)algorithm);
                CFReleaseNull(error);
                CFReleaseNull(signature);

                signature = SecKeyCreateSignature(privKey, algorithm, dataToSign, &error);
                ok(signature, "SecKeyCopySignature failed for algorithm %d", (int)algorithm);
                CFReleaseNull(error);

                SKIP: {
                    skip("invalid signature", 5, signature != NULL);

                    ok(!SecKeyVerifySignature(privKey, algorithm, dataToSign, signature, &error),
                       "SecKeyVerifySignature succeeded with priv key for %d", (int)algorithm);
                    CFReleaseNull(error);

                    ok(!SecKeyVerifySignature(pubKey, incompatibleAlgorithm, dataToSign, signature, &error),
                       "SecKeyVerifySignature succeeded with wrong algorithm for %d", (int)algorithm);
                    CFReleaseNull(error);

                    ok(SecKeyVerifySignature(pubKey, algorithm, dataToSign, signature, &error),
                       "SecKeyVerifySignature failed for algorithm %d", (int)algorithm);
                    CFReleaseNull(error);

                    CFMutableDataRef modifiedSignature = CFDataCreateMutable(kCFAllocatorDefault, 0);
                    CFDataAppend(modifiedSignature, signature);
                    *CFDataGetMutableBytePtr(modifiedSignature) ^= 0xff;

                    ok(!SecKeyVerifySignature(pubKey, algorithm, dataToSign, modifiedSignature, &error),
                       "SecKeyVerifySignature succeeded with bad signature for algorithm %d", (int)algorithm);
                    CFReleaseNull(error);

                    CFMutableDataRef modifiedDataToSign = CFDataCreateMutable(kCFAllocatorDefault, 0);
                    CFDataAppend(modifiedDataToSign, dataToSign);
                    *CFDataGetMutableBytePtr(modifiedDataToSign) ^= 0xff;

                    ok(!SecKeyVerifySignature(pubKey, algorithm, modifiedDataToSign, signature, &error),
                       "SecKeyVerifySignature succeeded with bad data for %d", (int)algorithm);
                    CFReleaseNull(error);

                    CFReleaseNull(modifiedDataToSign);
                    CFReleaseNull(modifiedSignature);

                    CFReleaseNull(signature);
                }
                CFReleaseNull(dataToSign);
            }
        }
    }

    CFReleaseNull(testData);
    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
    testsignformat();

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

    testkeyexchange(192);
    testkeyexchange(224);
    testkeyexchange(256);
    testkeyexchange(384);
    testkeyexchange(521);

    testsupportedalgos(192);
    testcreatewithdata(192);
    testcopyattributes(192);
    testcopypublickey(192);
    testsignverify(192);
}

int si_41_sececkey(int argc, char *const *argv)
{
	plan_tests(557);

	tests();

	return 0;
}
