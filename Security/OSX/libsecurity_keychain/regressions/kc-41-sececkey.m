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

/*
 * This is to fool os services to not provide the Keychain manager
 * interface that doens't work since we don't have unified headers
 * between iOS and OS X. rdar://23405418/
 */
#define __KEYCHAINCORE__ 1
#import <Foundation/Foundation.h>

#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItemPriv.h>

#include <Security/SecItem.h>
#include <Security/SecKey.h>

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
#include "utilities/array_size.h"


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

	kzib = CFNumberCreate(NULL, kCFNumberSInt64Type, &keySizeInBits);
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

	kzib = CFNumberCreate(NULL, kCFNumberSInt64Type, &keySizeInBits);
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

#if TARGET_OS_IPHONE
// Raw DER Key
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
#else
// Open SSL Private Key
const uint8_t EC_P256_KeyDER_priv[]={
        0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x86, 0x87, 0x79, 0x59, 0xd1,
        0xc6, 0x3c, 0x50, 0x24, 0x30, 0xa4, 0xaf, 0x89, 0x1d, 0xd1, 0x94, 0x23,
        0x56, 0x79, 0x46, 0x93, 0x72, 0x31, 0x39, 0x24, 0xe6, 0x01, 0x96, 0xc8,
        0xeb, 0xf3, 0x88, 0xa0, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
        0x03, 0x01, 0x07, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04, 0x8c, 0xfa, 0xd7,
        0x8a, 0xf1, 0xb9, 0xad, 0xd7, 0x3a, 0x33, 0xb5, 0x9a, 0xad, 0x52, 0x0d,
        0x14, 0xd6, 0x6b, 0x35, 0x56, 0x79, 0xd6, 0x74, 0x2a, 0x37, 0x7e, 0x2f,
        0x33, 0xa6, 0xab, 0xee, 0x35, 0x00, 0x70, 0x82, 0x89, 0x9c, 0xfc, 0x97,
        0xc4, 0x89, 0x5c, 0x16, 0x50, 0xad, 0x60, 0x55, 0xa6, 0x70, 0xee, 0x07,
        0x1b, 0xfe, 0xe4, 0xf0, 0xa0, 0x63, 0xc0, 0x73, 0x24, 0x97, 0x92, 0x04,
        0xc7};

// Open SSL Public Key
const uint8_t EC_P256_KeyDER_pub[]={
        0x30, 0x59, 0x30, 0x13, 0x06, 0x07, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02,
        0x01, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07, 0x03,
        0x42, 0x00, 0x04, 0x8c, 0xfa, 0xd7, 0x8a, 0xf1, 0xb9, 0xad, 0xd7, 0x3a,
        0x33, 0xb5, 0x9a, 0xad, 0x52, 0x0d, 0x14, 0xd6, 0x6b, 0x35, 0x56, 0x79,
        0xd6, 0x74, 0x2a, 0x37, 0x7e, 0x2f, 0x33, 0xa6, 0xab, 0xee, 0x35, 0x00,
        0x70, 0x82, 0x89, 0x9c, 0xfc, 0x97, 0xc4, 0x89, 0x5c, 0x16, 0x50, 0xad,
        0x60, 0x55, 0xa6, 0x70, 0xee, 0x07, 0x1b, 0xfe, 0xe4, 0xf0, 0xa0, 0x63,
        0xc0, 0x73, 0x24, 0x97, 0x92, 0x04, 0xc7};
#endif

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

/* r is 72 bytes longer than it should be */
const uint8_t EC_P256_SigDER_LargeInt[]={
    0x30, 0x81, 0x8D, 0x02, 0x68,
    0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4b, 0x37, 0x7f, 0x45, 0xd0, 0x5d, 0xa6, 0x53,
    0xb3, 0x62, 0x6f, 0x32, 0xdb, 0xfc, 0xf6, 0x3b, 0x84, 0xfa, 0x5a, 0xd9, 0x17, 0x67, 0x03, 0x73,
    0x48, 0x0c, 0xad, 0x89, 0x13, 0x69, 0x61, 0xb3,
    0x02, 0x21,
    0x00, 0xd6, 0x23, 0xaf, 0xd9, 0x7d, 0x72, 0xba, 0x3b, 0x90, 0xc1, 0x23, 0x7d, 0xdb, 0x2c, 0xd1,
    0x0d, 0xbb, 0xb4, 0x0f, 0x67, 0x26, 0xff, 0x3f, 0xa6, 0x47, 0xa4, 0x13, 0x0d, 0xe0, 0x45, 0xd5,
    0x6b};

static void testsignformat(void)
{
    SecKeyRef pkey = NULL;
    SecKeyRef pubkey = NULL;
    CFArrayRef KeyArrayPub=NULL;
    CFArrayRef KeyArrayPriv=NULL;
    uint8_t EC_signature_DER[72];
    uint8_t EC_signature_RAW[64];
    size_t EC_signature_DER_size=sizeof(EC_signature_DER);
    size_t EC_signature_RAW_size=sizeof(EC_signature_RAW);

#if TARGET_OS_IPHONE
    // Key import for iOS
    {
        ok((pkey = SecKeyCreateECPrivateKey(kCFAllocatorDefault,
                                            EC_P256_KeyDER, sizeof(EC_P256_KeyDER),
                                            kSecKeyEncodingPkcs1)) != NULL, "import privkey");
        CFDataRef pubdata = NULL;
        ok_status(SecKeyCopyPublicBytes(pkey, &pubdata), "pub key from priv key");

        ok((pubkey = SecKeyCreateECPublicKey(kCFAllocatorDefault,
                                             CFDataGetBytePtr(pubdata), CFDataGetLength(pubdata),
                                             kSecKeyEncodingBytes))!=NULL,
           "recreate seckey");
        CFReleaseNull(pubdata);
    }
#else
    // Key import for MacOS
    {
        CFDataRef DER_key;
        SecExternalFormat sef=kSecFormatOpenSSL;
        SecExternalItemType seit;

        // Public key
        DER_key = CFDataCreate(kCFAllocatorDefault, EC_P256_KeyDER_pub, sizeof(EC_P256_KeyDER_pub));
        seit=kSecItemTypePublicKey;
        ok_status(SecItemImport(DER_key,NULL,&sef,&seit,0,NULL,NULL,&KeyArrayPub), "Import DER key");
        ok((!(KeyArrayPub==NULL) && CFArrayGetCount(KeyArrayPub)==1), "One key imported");
        pubkey=(SecKeyRef)CFArrayGetValueAtIndex(KeyArrayPub,0);
        CFReleaseNull(DER_key);

        // Private key
        DER_key = CFDataCreate(kCFAllocatorDefault, EC_P256_KeyDER_priv, sizeof(EC_P256_KeyDER_priv));
        seit=kSecItemTypePrivateKey;
        ok_status(SecItemImport(DER_key,NULL,&sef,&seit,0,NULL,NULL,&KeyArrayPriv), "Import DER key");
        ok((!(KeyArrayPriv==NULL) && CFArrayGetCount(KeyArrayPriv)==1), "One key imported");
        pkey=(SecKeyRef)CFArrayGetValueAtIndex(KeyArrayPriv,0);
        CFReleaseNull(DER_key);
    }
#endif

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

    // Verify signature with one integer larger than it should be
    ok_status(!SecKeyRawVerify(pubkey, kSecPaddingPKCS1,
                               EC_SigDigest, sizeof(EC_SigDigest), EC_P256_SigDER_LargeInt, sizeof(EC_P256_SigDER_LargeInt)),
              "verify DER sig with large integer");

    CFReleaseNull(KeyArrayPub);
    CFReleaseNull(KeyArrayPriv);
}

#if !TARGET_OS_IPHONE
static inline bool CFEqualSafe(CFTypeRef left, CFTypeRef right)
{
    if (left == NULL || right == NULL)
        return left == right;
    else
        return CFEqual(left, right);
}
#endif

static void testkeyexchange(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    OSStatus status;

    SecKeyRef pubKey1 = NULL, privKey1 = NULL;
    NSDictionary *kgp1 = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeEC,
        (id)kSecAttrKeySizeInBits: @(keySizeInBits),
        (id)kSecAttrIsPermanent: @NO,
        (id)kSecAttrLabel: @"sectests:kc-41-sececkey:testkeyexchange",
        (id)kSecAttrNoLegacy: @YES,
    };
    ok_status(status = SecKeyGeneratePair((CFDictionaryRef)kgp1, &pubKey1, &privKey1),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);

    SecKeyRef pubKey2 = NULL, privKey2 = NULL;
    NSDictionary *kgp2 = @{
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeEC,
        (id)kSecAttrKeySizeInBits: @(keySizeInBits),
        (id)kSecAttrIsPermanent: @NO,
        (id)kSecAttrLabel: @"sectests:kc-41-sececkey:testkeyexchange",
        (id)kSecAttrNoLegacy: @NO,
    };
    ok_status(status = SecKeyGeneratePair((CFDictionaryRef)kgp2, &pubKey2, &privKey2),
              "Generate %ld bit (%ld byte) EC keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    
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
    const CFIndex requestedSize = 273;
    NSDictionary *params = @{
        (id)kSecKeyKeyExchangeParameterRequestedSize: @(requestedSize),
        (id)kSecKeyKeyExchangeParameterSharedInfo: [NSData dataWithBytes:"shared" length:5],
    };

    for (size_t ix = 0; ix < array_size(algos); ++ix) {
        CFErrorRef error = NULL;

        NSData *secret1 = (__bridge_transfer NSData *)SecKeyCopyKeyExchangeResult(privKey1, algos[ix], pubKey2, (CFDictionaryRef)params, &error);
        ok(secret1 != NULL && CFGetTypeID((__bridge CFTypeRef) secret1) == CFDataGetTypeID());
        CFReleaseNull(error);

        NSData *secret2 = (__bridge_transfer NSData *)SecKeyCopyKeyExchangeResult(privKey2, algos[ix], pubKey1, (CFDictionaryRef)params, &error);
        ok(secret2 != NULL && CFGetTypeID((__bridge CFTypeRef) secret2) == CFDataGetTypeID());
        CFReleaseNull(error);

        eq_cf((__bridge CFTypeRef) secret1, (__bridge CFTypeRef) secret2, "results of key exchange are equal");
        if (algos[ix] != kSecKeyAlgorithmECDHKeyExchangeCofactor && algos[ix] != kSecKeyAlgorithmECDHKeyExchangeStandard) {
            is(secret1.length, requestedSize, "generated response has expected length");
        }
    }

    CFReleaseNull(privKey1);
    CFReleaseNull(pubKey1);
    CFReleaseNull(privKey2);
    CFReleaseNull(pubKey2);

    SecItemDelete((CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrLabel: @"sectests:kc-41-sececkey:testkeyexchange",
    });
}

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
    testsignformat();

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

    testkeyexchange(256);
    testkeyexchange(384);
    testkeyexchange(521);
}

int kc_41_sececkey(int argc, char *const *argv)
{
	plan_tests(245);

	tests();

	return 0;
}
