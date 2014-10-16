/*
 * Copyright (c) 2011,2013-2014 Apple Inc. All Rights Reserved.
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
 *  Copyright (c) 2007-2009,2013-2014 Apple Inc. All Rights Reserved.
 *
 */
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>

#if 0
#include <Security/SecCertificate.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecAsn1Types.h>
#include <Security/oidsalg.h>
#include <Security/SecureTransport.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonDigest.h>
#include <libDER/libDER.h>
#include <stdlib.h>
#include <unistd.h>
#endif


#include "keychain_regressions.h"
#include "utilities/SecCFRelease.h"


#if TARGET_OS_IPHONE
static void testdigestandsignalg(SecKeyRef privKey, SecKeyRef pubKey, const SecAsn1AlgId *algId) {
    uint8_t dataToDigest[256];
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
    for (ix = 0; ix < sizeof(oids) / sizeof(*oids); ++ix) {
        if (oids[ix]) {
            algId.algorithm = *oids[ix];
        } else {
            algId.algorithm.Length = 0;
            algId.algorithm.Data = NULL;
        }

        testdigestandsignalg(privKey, pubKey, &algId);
    }
}
#endif


#if !TARGET_OS_IPHONE
/* This is part of Security.framework on iOS */

enum {
    // kSecKeyKeySizeInBits        = 0, // already exists on osx
    kSecKeySignatureSize        = 101,
    kSecKeyEncryptedDataSize    = 102,
    // More might belong here, but we aren't settled on how
    // to take into account padding and/or digest types.
};

static
size_t SecKeyGetSize(SecKeyRef key, int whichSize)
{
    /* SecKeyGetBlockSize return the signature size on OS X -- smh */
    size_t result = SecKeyGetBlockSize(key);

    result = (result - 2)/2 - 3;

    /* in this test, this is always an ECDSA key */
    switch (whichSize) {
        case kSecKeyEncryptedDataSize:
            result = 0;
            break;
        case kSecKeySignatureSize:
            result = (result >= 66 ? 9 : 8) + 2 * result;
            break;
        case kSecKeyKeySizeInBits:
            if (result >= 66)
                return 521;
    }

    if (whichSize == kSecKeyKeySizeInBits)
        result *= 8;

    return result;

}
#endif


static void testkeygen(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
	size_t keySizeInBytes = (keySizeInBits + 7) / 8;
	CFNumberRef kzib;

	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keySizeInBits);
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

#if TARGET_OS_IPHONE
    testdigestandsign(privKey, pubKey);
#endif

    const void *privkeys[] = {
        kSecValueRef
    };
    const void *privvalues[] = {
        privKey
    };
    CFDictionaryRef privitem = CFDictionaryCreate(NULL, privkeys, privvalues,
                                                  sizeof(privkeys) / sizeof(*privkeys), NULL, NULL);
#if TARGET_OS_IPHONE
    ok_status(SecItemAdd(privitem, NULL), "add private key");
#endif
    ok_status(SecItemDelete(privitem), "delete private key");
    CFReleaseNull(privitem);

    const void *pubkeys[] = {
        kSecValueRef
    };
    const void *pubvalues[] = {
        pubKey
    };
    CFDictionaryRef pubitem = CFDictionaryCreate(NULL, pubkeys, pubvalues,
                                                 sizeof(pubkeys) / sizeof(*pubkeys), NULL, NULL);
#if TARGET_OS_IPHONE
    ok_status(SecItemAdd(pubitem, NULL), "add public key");
#endif
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

    CFUUIDRef ourUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, ourUUID);
    CFMutableStringRef publicName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);
    CFMutableStringRef privateName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);

    CFReleaseNull(ourUUID);
    CFReleaseNull(uuidString);

    CFStringAppend(publicName, CFSTR("-Public-41"));
    CFStringAppend(privateName, CFSTR("-Private-41"));

	CFMutableDictionaryRef pubd = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFMutableDictionaryRef privd = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(pubd, kSecAttrLabel, publicName);
	CFDictionaryAddValue(privd, kSecAttrLabel, privateName);

	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keySizeInBits);
	CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeEC);
	CFDictionaryAddValue(kgp, kSecAttrKeySizeInBits, kzib);
	CFDictionaryAddValue(kgp, kSecAttrIsPermanent, kCFBooleanTrue);
	CFDictionaryAddValue(kgp, kSecPublicKeyAttrs, pubd);
	CFDictionaryAddValue(kgp, kSecPrivateKeyAttrs, privd);

	OSStatus status;
	ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) persistent RSA keypair",
              keySizeInBits, keySizeInBytes);
	CFRelease(kzib);
	CFRelease(kgp);

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

	CFRelease(pubd);
	CFRelease(privd);
}


/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
	testkeygen(192);
#if TARGET_OS_IPHONE
	testkeygen(224);
#endif
	testkeygen(256);
	testkeygen(384);
	testkeygen(521);

    testkeygen2(192);
#if TARGET_OS_IPHONE
    testkeygen2(224);
#endif
	testkeygen2(256);
	testkeygen2(384);
	testkeygen2(521);

}

int kc_41_sececkey(int argc, char *const *argv)
{
#if TARGET_OS_IPHONE
	plan_tests(140);
#else
	plan_tests(88);
#endif

	tests();

	return 0;
}
