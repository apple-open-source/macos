/*
 * Copyright (c) 2007-2009,2012-2014 Apple Inc. All Rights Reserved.
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
#include <utilities/SecCFWrappers.h>
#include <CommonCrypto/CommonDigest.h>
#include <libDER/libDER.h>
#include <stdlib.h>
#include <unistd.h>
#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

#include "Security_regressions.h"

#include "utilities/SecCFRelease.h"

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
            sig, &sigLen), "digest and sign %s with %ld bit RSA key", oidBuf, sigLen * 8);

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
        &CSSMOID_SHA1WithRSA,
        &CSSMOID_SHA224WithRSA,
        &CSSMOID_SHA256WithRSA,
        &CSSMOID_SHA384WithRSA,
        &CSSMOID_SHA512WithRSA,
#if 0
        &CSSMOID_SHA1WithRSA_OIW,
        &CSSMOID_SHA1WithDSA,		// BSAFE
        &CSSMOID_SHA1WithDSA_CMS,	// X509/CMS
        &CSSMOID_SHA1WithDSA_JDK,	// JDK 1.1
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

#if 0
static void dump_bytes(uint8_t* bytes, size_t amount)
{
    while (amount > 0) {
        printf("0x%02x ", *bytes);
        ++bytes;
        --amount;
    }
}
#endif

#define kEncryptDecryptTestCount 6
static void test_encrypt_decrypt(SecKeyRef pubKey, SecKeyRef privKey, uint32_t padding, size_t keySizeInBytes)
{
    SKIP: {
        size_t max_len = keySizeInBytes;
        switch (padding) {
            case kSecPaddingNone: max_len = keySizeInBytes; break;
            case kSecPaddingOAEP: max_len = keySizeInBytes - 2 - 2 * CC_SHA1_DIGEST_LENGTH; break;
            case kSecPaddingPKCS1: max_len = keySizeInBytes - 11; break;
            default: skip("what is the max_len for this padding?", 5, false);
        }

        uint8_t secret[max_len + 2], encrypted_secret[keySizeInBytes], decrypted_secret[keySizeInBytes];
        uint8_t *secret_ptr = secret;
        size_t secret_len = max_len;
        size_t encrypted_secret_len = sizeof(encrypted_secret);
        size_t decrypted_secret_len = sizeof(decrypted_secret);
        memset(decrypted_secret, 0xff, decrypted_secret_len);
        ok_status(SecRandomCopyBytes(kSecRandomDefault, sizeof(secret), secret),"rng");

        // zero pad, no accidental second zero byte
        if (padding == kSecPaddingNone) {
            secret[0] = 0;
            secret[1] = 128;
        }

        is_status(SecKeyEncrypt(pubKey, padding,
                                secret, sizeof(secret),
                                encrypted_secret, &encrypted_secret_len), errSecParam, "encrypt secret (overflow)");
        ok_status(SecKeyEncrypt(pubKey, padding,
                                secret, secret_len,
                                encrypted_secret, &encrypted_secret_len), "encrypt secret");

        ok_status(SecKeyDecrypt(privKey, padding,
                                encrypted_secret, encrypted_secret_len,
                                decrypted_secret, &decrypted_secret_len), "decrypt secret");

        // zero padding is removed on decode
        if (padding == kSecPaddingNone) {
            secret_len--;
            secret_ptr++;
        }

        ok(decrypted_secret_len == secret_len, "correct length");
        ok_status(memcmp(secret_ptr, decrypted_secret, secret_len), "verify secret");
    }
}

#define kKeyGenTestCount (50 + (3*kEncryptDecryptTestCount))
static void testkeygen(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
	size_t keySizeInBytes = (keySizeInBits + 7) / 8;
	CFNumberRef kzib;

    int32_t iKeySizeInBits = (int32_t) keySizeInBits;
    kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &iKeySizeInBits);
	CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
	CFDictionaryAddValue(kgp, kSecAttrKeySizeInBits, kzib);

	OSStatus status;
	ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair", keySizeInBits,
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
		uint8_t something[keySizeInBytes];
        size_t something_len = keySizeInBytes - 11;
        ok_status(SecRandomCopyBytes(kSecRandomDefault, sizeof(something), something),"rng");
		uint8_t sig[keySizeInBytes];
		size_t sigLen = sizeof(sig);
        if (privKey != NULL && pubKey != NULL) {
            is_status(SecKeyRawSign(privKey, kSecPaddingPKCS1,
                                    something, something_len + 1, sig, &sigLen),
                      errSecParam, "sign overflow");
            ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1,
                                    something, something_len, sig, &sigLen), "sign something");
            ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1,
                                      something, something_len, sig, sigLen), "verify sig on something");

            // Torture test ASN.1 encoder by setting high bit to 1.
            uint8_t digest[CC_SHA512_DIGEST_LENGTH] = {
                0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
                0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,
            };
            //CC_MD2(something, sizeof(something), digest);
            ok_status(!SecKeyRawSign(privKey, kSecPaddingPKCS1MD2,
                                     digest, CC_MD2_DIGEST_LENGTH, sig, &sigLen),
                      "don't sign md2 digest");
            ok_status(!SecKeyRawVerify(pubKey, kSecPaddingPKCS1MD2,
                                       digest, CC_MD2_DIGEST_LENGTH, sig, sigLen),
                      "verify sig on md2 digest fails");

            //CC_MD5(something, sizeof(something), digest);
            sigLen = sizeof(sig);
            ok_status(!SecKeyRawSign(privKey, kSecPaddingPKCS1MD5,
                                     digest, CC_MD5_DIGEST_LENGTH, sig, &sigLen),
                      "don't sign md5 digest");
            ok_status(!SecKeyRawVerify(pubKey, kSecPaddingPKCS1MD5,
                                       digest, CC_MD5_DIGEST_LENGTH, sig, sigLen),
                      "verify sig on md5 digest fails");

            //CCDigest(kCCDigestSHA1, something, sizeof(something), digest);
            sigLen = sizeof(sig);
            ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1SHA1,
                                    digest, CC_SHA1_DIGEST_LENGTH, sig, &sigLen),
                      "sign sha1 digest");
            ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1SHA1,
                                      digest, CC_SHA1_DIGEST_LENGTH, sig, sigLen),
                      "verify sig on sha1 digest");

            uint8_t signature[keySizeInBytes], *ptr = signature;
            size_t signature_len = sizeof(signature);
            ok_status(SecKeyDecrypt(pubKey, kSecPaddingNone, sig, sigLen, signature, &signature_len), "inspect signature");
            is(signature_len, keySizeInBytes - 1, "got signature");
            while(*ptr && ((size_t)(ptr - signature) < signature_len)) ptr++;
            is(signature + signature_len - ptr, 16 /* length(\0 || OID_SHA1) */ + CC_SHA1_DIGEST_LENGTH, "successful decode");

            /* PKCS1 padding is 00 01 PAD * 8 or more 00 data.
             data is SEQ { SEQ { OID NULL } BIT STRING 00 DIGEST }
             So min data + pad overhead is 11 + 9 + oidlen
             oidlen = 11 for the sha2 family of oids, so we have 29 bytes; or
             232 bits of minimum overhead.  */
            const size_t pkcs1Overhead = 232;
            if (keySizeInBits > 224 + pkcs1Overhead) {
                //CC_SHA224(something, sizeof(something), digest);
                sigLen = sizeof(sig);
                ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1SHA224,
                                        digest, CC_SHA224_DIGEST_LENGTH, sig, &sigLen),
                          "sign sha224 digest");
                ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1SHA224,
                                          digest, CC_SHA224_DIGEST_LENGTH, sig, sigLen),
                          "verify sig on sha224 digest");
            }

            if (keySizeInBits > 256 + pkcs1Overhead) {
                //CC_SHA256(something, sizeof(something), digest);
                sigLen = sizeof(sig);
                ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1SHA256,
                                        digest, CC_SHA256_DIGEST_LENGTH, sig, &sigLen),
                          "sign sha256 digest");
                ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1SHA256,
                                          digest, CC_SHA256_DIGEST_LENGTH, sig, sigLen),
                          "verify sig on sha256 digest");
            }

            if (keySizeInBits > 384 + pkcs1Overhead) {
                //CC_SHA384(something, sizeof(something), digest);
                sigLen = sizeof(sig);
                ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1SHA384,
                                        digest, CC_SHA384_DIGEST_LENGTH, sig, &sigLen),
                          "sign sha384 digest");
                ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1SHA384,
                                          digest, CC_SHA384_DIGEST_LENGTH, sig, sigLen),
                          "verify sig on sha384 digest");
            }

            if (keySizeInBits > 512 + pkcs1Overhead) {
                //CC_SHA512(something, sizeof(something), digest);
                sigLen = sizeof(sig);
                ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1SHA512,
                                        digest, CC_SHA512_DIGEST_LENGTH, sig, &sigLen),
                          "sign sha512 digest");
                ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1SHA512,
                                          digest, CC_SHA512_DIGEST_LENGTH, sig, sigLen),
                          "verify sig on sha512 digest");
            }
        }

        test_encrypt_decrypt(pubKey, privKey, kSecPaddingNone, keySizeInBytes);
        test_encrypt_decrypt(pubKey, privKey, kSecPaddingPKCS1, keySizeInBytes);
        test_encrypt_decrypt(pubKey, privKey, kSecPaddingOAEP, keySizeInBytes);

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
        ok_status(SecItemDelete(privitem), "delete private key");
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

#define kKeyGen2TestCount 12
static void testkeygen2(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
    int32_t keySizeInBytes = (int32_t)((keySizeInBits + 7) / 8);
	CFNumberRef kzib;

    CFUUIDRef ourUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFStringRef uuidString = CFUUIDCreateString(kCFAllocatorDefault, ourUUID);
    CFMutableStringRef publicName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);
    CFMutableStringRef privateName = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, uuidString);

    CFReleaseNull(ourUUID);
    CFReleaseNull(uuidString);

    CFStringAppend(publicName, CFSTR("-Public-40"));
    CFStringAppend(privateName, CFSTR("-Private-40"));
	CFMutableDictionaryRef pubd = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFMutableDictionaryRef privd = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

	CFDictionaryAddValue(pubd, kSecAttrLabel, publicName);
	CFDictionaryAddValue(privd, kSecAttrLabel, privateName);

    int32_t iKeySizeInBits = (int32_t) keySizeInBits;
	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &iKeySizeInBits);
	CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
	CFDictionaryAddValue(kgp, kSecAttrKeySizeInBits, kzib);
	CFDictionaryAddValue(kgp, kSecAttrIsPermanent, kCFBooleanTrue);
	CFDictionaryAddValue(kgp, kSecPublicKeyAttrs, pubd);
	CFDictionaryAddValue(kgp, kSecPrivateKeyAttrs, privd);

	OSStatus status;
	ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %d bit (%d byte) persistent RSA keypair",
              (int)keySizeInBits, (int)keySizeInBytes);
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
		ok_status(SecItemCopyMatching(privd, (CFTypeRef *)&privKey2),
			"retrieve priv key by label and kSecAttrCanSign");

		/* Sign something. */
		uint8_t something[50] = {0x80, 0xbe, 0xef, 0xba, 0xd0, };
		uint8_t sig[keySizeInBytes];
		size_t sigLen = keySizeInBytes;
		ok_status(SecKeyRawSign(privKey2, kSecPaddingPKCS1,
			something, sizeof(something), sig, &sigLen), "sign something");
		ok_status(SecKeyRawVerify(pubKey2, kSecPaddingPKCS1,
			something, sizeof(something), sig, sigLen), "verify sig on something");

        sigLen = keySizeInBytes;
		is_status(SecKeyEncrypt(pubKey2, kSecPaddingPKCS1SHA1,
			something, sizeof(something), sig, &sigLen), errSecParam,
            "encrypt something with invalid padding");

		/* Cleanup. */
		CFReleaseNull(pubKey2);
		CFReleaseNull(privKey2);

        /* delete from keychain - note: do it before releasing publicName and privateName
         because pubd and privd have no retain/release callbacks */
        ok_status(SecItemDelete(pubd), "delete generated pub key");
        ok_status(SecItemDelete(privd), "delete generated priv key");
	}

	/* Cleanup. */
	CFReleaseNull(pubKey);
	CFReleaseNull(privKey);

    CFReleaseNull(publicName);
    CFReleaseNull(privateName);

	CFRelease(pubd);
	CFRelease(privd);
}

static const int kTestSupportedCount = 3 + (4 * 11) + 2 + (4 * 5);
static void testsupportedalgos(size_t keySizeInBits)
{
    SecKeyRef pubKey = NULL, privKey = NULL;
    int32_t keySizeInBytes = (int)((keySizeInBits + 7) / 8);
    CFNumberRef kzib;

    int32_t iKeySizeInBits = (int32_t) keySizeInBits;
    kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &iKeySizeInBits);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionaryAddValue(kgp, kSecAttrKeySizeInBits, kzib);

    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %d bit (%d byte) persistent RSA keypair",
              (int)keySizeInBits, (int)keySizeInBytes);
    CFRelease(kzib);
    CFRelease(kgp);

    is(SecKeyGetSize(pubKey, kSecKeyKeySizeInBits), (size_t) keySizeInBits, "public key size is ok");
    is(SecKeyGetSize(privKey, kSecKeyKeySizeInBits), (size_t) keySizeInBits, "private key size is ok");

    const SecKeyAlgorithm sign[] = {
        kSecKeyAlgorithmRSASignatureRaw,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
    };

    for (size_t i = 0; i < array_size(sign); i++) {
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeSign, sign[i]),
           "privKey supports sign algorithm %@", sign[i]);
        ok(SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeVerify, sign[i]),
           "pubKey supports verify algorithm %@", sign[i]);
        // Our privKey actually supports even verify operation because it is adapter over decrypt...
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeVerify, sign[i]),
           "privKey supports verify algorithm %@", sign[i]);
        ok(!SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeSign, sign[i]),
           "pubKey doesn't support verify algorithm %@", sign[i]);
    }
    ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeSign, kSecKeyAlgorithmECDSASignatureDigestX962),
       "RSA privKey does not support ECDSA algorithm");
    ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeVerify, kSecKeyAlgorithmECDSASignatureDigestX962),
       "RSA pubKey does not support ECDSA algorithm");

    const SecKeyAlgorithm crypt[] = {
        kSecKeyAlgorithmRSAEncryptionRaw,
        kSecKeyAlgorithmRSAEncryptionPKCS1,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
//        kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
//        kSecKeyAlgorithmRSAEncryptionOAEPSHA512,
    };
    for (size_t i = 0; i < array_size(crypt); i++) {
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeDecrypt, crypt[i]),
           "privKey supports decrypt algorithm %@", crypt[i]);
        ok(SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeEncrypt, crypt[i]),
           "pubKey supports encrypt algorithm %@", crypt[i]);
        ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeEncrypt, crypt[i]),
           "privKey doesn't supports encrypt algorithm %@", crypt[i]);
        ok(SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeDecrypt, crypt[i]),
           "pubKey supports decrypt algorithm %@", crypt[i]);
    }

    /* Cleanup. */
    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

#define kCreateWithDataTestCount 13
static void testcreatewithdata(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeRSA,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    CFMutableDictionaryRef kcwd = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                                                          kSecAttrKeyType, kSecAttrKeyTypeRSA,
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

#define kCopyAttributesTestCount 20
static void testcopyattributes(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeRSA,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
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
            eq_cf(attrValue, kCFBooleanFalse, "invalid priv key kSecAttrCanDerive");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanSign);
            eq_cf(attrValue, kCFBooleanTrue, "invalid priv key kSecAttrCanSign");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanVerify);
            eq_cf(attrValue, kCFBooleanFalse, "invalid priv key kSecAttrCanVerify");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyClass);
            eq_cf(attrValue, kSecAttrKeyClassPrivate, "priv key invalid kSecAttrKeyClass");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyType);
            eq_cf(attrValue, kSecAttrKeyTypeRSA, "invalid priv key kSecAttrKeyType");

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

            // Although unusual, our RSA public key can even decrypt.
            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDecrypt);
            eq_cf(attrValue, kCFBooleanTrue, "pub key invalid kSecAttrCanDecrypt");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDerive);
            eq_cf(attrValue, kCFBooleanFalse, "pub key invalid kSecAttrCanDerive");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanSign);
            eq_cf(attrValue, kCFBooleanFalse, "pub key invalid kSecAttrCanSign");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrCanVerify);
            eq_cf(attrValue, kCFBooleanTrue, "pub key invalid kSecAttrCanVerify");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyClass);
            eq_cf(attrValue, kSecAttrKeyClassPublic, "pub key invalid kSecAttrKeyClass");

            attrValue = CFDictionaryGetValue(attributes, kSecAttrKeyType);
            eq_cf(attrValue, kSecAttrKeyTypeRSA, "pub key invalid kSecAttrKeyType");

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

#define kCopyPublicKeyTestCount 5
static void testcopypublickey(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeRSA,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    CFReleaseNull(kzib);

    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
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

#define kSignAndVerifyTestCount 84
static void testsignverify(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFDictionaryRef kgp = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                                       kSecAttrKeyType, kSecAttrKeyTypeRSA,
                                                       kSecAttrKeySizeInBits, kzib,
                                                       kSecAttrIsPermanent, kCFBooleanFalse,
                                                       NULL);
    CFReleaseNull(kzib);

    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    SecKeyAlgorithm algorithms[] = {
        kSecKeyAlgorithmRSASignatureRaw,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA224,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA256,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA384,
        kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA512,
    };

    CFDataRef testData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("test"), kCFStringEncodingUTF8, 0);
    ok(testData, "creating test data failed");

    SKIP: {
        skip("invalid test data", 71, testData);

        CFErrorRef error = NULL;

        for (uint32_t ix = 0; ix < array_size(algorithms); ++ix) {
            SecKeyAlgorithm algorithm = algorithms[ix];
            SecKeyAlgorithm incompatibleAlgorithm = (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureRaw)) ?
            kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1 : kSecKeyAlgorithmRSASignatureRaw;

            CFDataRef dataToSign = NULL;
            if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA1)) {
                dataToSign = CFDataCreateWithHash(kCFAllocatorDefault, ccsha1_di(),
                                                  CFDataGetBytePtr(testData), CFDataGetLength(testData));
                ok(dataToSign, "creating digest failed for algorithm %@", algorithm);
            }
            else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA224)) {
                dataToSign = CFDataCreateWithHash(kCFAllocatorDefault, ccsha224_di(),
                                                  CFDataGetBytePtr(testData), CFDataGetLength(testData));
                ok(dataToSign, "creating digest failed for algorithm %@", algorithm);
            }
            else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA256)) {
                dataToSign = CFDataCreateWithHash(kCFAllocatorDefault, ccsha256_di(),
                                                  CFDataGetBytePtr(testData), CFDataGetLength(testData));
                ok(dataToSign, "creating digest failed for algorithm %@", algorithm);
            }
            else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA384)) {
                dataToSign = CFDataCreateWithHash(kCFAllocatorDefault, ccsha384_di(),
                                                  CFDataGetBytePtr(testData), CFDataGetLength(testData));
                ok(dataToSign, "creating digest failed for algorithm %@", algorithm);
            }
            else if (CFEqual(algorithm, kSecKeyAlgorithmRSASignatureDigestPKCS1v15SHA512)) {
                dataToSign = CFDataCreateWithHash(kCFAllocatorDefault, ccsha512_di(),
                                                  CFDataGetBytePtr(testData), CFDataGetLength(testData));
                ok(dataToSign, "creating digest failed for algorithm %@", algorithm);
            }
            else {
                CFRetainAssign(dataToSign, testData);
            }
            CFReleaseNull(error);

            SKIP: {
                skip("invalid data to sign", 7, dataToSign);

                CFDataRef signature = SecKeyCreateSignature(pubKey, algorithm, dataToSign, &error);
                ok(!signature, "SecKeyCopySignature succeeded with pub key for algorithm %@", algorithm);
                CFReleaseNull(error);
                CFReleaseNull(signature);

                signature = SecKeyCreateSignature(privKey, algorithm, dataToSign, &error);
                ok(signature, "SecKeyCopySignature failed for algorithm %@", algorithm);
                CFReleaseNull(error);

                SKIP: {
                    skip("invalid signature", 4, signature);

                    ok(!SecKeyVerifySignature(privKey, algorithm, dataToSign, signature, &error),
                       "SecKeyVerifySignature succeeded with priv key for algorithm %@", algorithm);
                    CFReleaseNull(error);

                    ok(!SecKeyVerifySignature(pubKey, incompatibleAlgorithm, dataToSign, signature, &error),
                       "SecKeyVerifySignature succeeded with wrong algorithm for algorithm %@", algorithm);
                    CFReleaseNull(error);

                    ok(SecKeyVerifySignature(pubKey, algorithm, dataToSign, signature, &error),
                       "SecKeyVerifySignature failed for algorithm %@", algorithm);
                    CFReleaseNull(error);

                    CFMutableDataRef modifiedSignature = CFDataCreateMutable(kCFAllocatorDefault, 0);
                    CFDataAppend(modifiedSignature, signature);
                    *CFDataGetMutableBytePtr(modifiedSignature) ^= 0xff;

                    ok(!SecKeyVerifySignature(pubKey, algorithm, dataToSign, modifiedSignature, &error),
                       "SecKeyVerifySignature succeeded with bad signature for algorithm %@", algorithm);
                    CFReleaseNull(error);

                    CFMutableDataRef modifiedDataToSign = CFDataCreateMutable(kCFAllocatorDefault, 0);
                    CFDataAppend(modifiedDataToSign, dataToSign);
                    *CFDataGetMutableBytePtr(modifiedDataToSign) ^= 0xff;

                    ok(!SecKeyVerifySignature(pubKey, algorithm, modifiedDataToSign, signature, &error),
                       "SecKeyVerifySignature succeeded with bad data for algorithm %@", algorithm);
                    CFReleaseNull(error);

                    CFReleaseNull(modifiedDataToSign);
                    CFReleaseNull(modifiedSignature);
                    CFReleaseNull(signature);
                }
                CFReleaseNull(dataToSign);
            }
        }
        CFReleaseNull(testData);
    }

    CFReleaseNull(pubKey);
    CFReleaseNull(privKey);
}

/* Test basic add delete update copy matching stuff. */
#define kTestCount ((3 * kKeyGenTestCount) + kKeyGen2TestCount + kTestSupportedCount + kCreateWithDataTestCount \
    + kCopyAttributesTestCount + kCopyPublicKeyTestCount + kSignAndVerifyTestCount)
static void tests(void)
{
	/* Comment out lines below for testing generating all common key sizes,
	   disabled now for speed reasons. */
	//testkeygen(512);
	testkeygen(768);
	testkeygen(1024);
	testkeygen(2056); // Stranged sized for edge cases in padding.
	//testkeygen(2048);
	//testkeygen(4096);

    testkeygen2(768);

    testsupportedalgos(768);
    testcreatewithdata(768);
    testcopyattributes(768);
    testcopypublickey(768);
    testsignverify(768);
}

int si_40_seckey(int argc, char *const *argv)
{
	plan_tests(kTestCount);

	tests();

	return 0;
}
