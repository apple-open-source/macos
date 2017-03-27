/*
 * Copyright (c) 2007-2009,2013-2014 Apple Inc. All Rights Reserved.
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


#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Foundation/Foundation.h>
#include <Security/Security.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificatePriv.h>

#include <corecrypto/ccsha1.h>
#include <corecrypto/ccsha2.h>

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


#if !TARGET_OS_IPHONE
#define kEncryptDecryptTestCount 0
#else
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

        uint8_t secret[max_len + 1], encrypted_secret[keySizeInBytes], decrypted_secret[keySizeInBytes];
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
#endif

#define kKeyGenTestCount (12 + (3*kEncryptDecryptTestCount))
static void testkeygen(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
	size_t keySizeInBytes = (keySizeInBits + 7) / 8;
	CFNumberRef kzib;

	kzib = CFNumberCreate(NULL, kCFNumberSInt64Type, &keySizeInBits);
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
		is(SecKeyGetBlockSize(pubKey) * 8, (size_t) keySizeInBits, "public key size is ok");
		is(SecKeyGetBlockSize(privKey) * 8, (size_t) keySizeInBits, "private key size is ok");

		/* Sign something. */
		uint8_t something[keySizeInBytes];
        size_t something_len = keySizeInBytes - 11;
        ok_status(SecRandomCopyBytes(kSecRandomDefault, sizeof(something), something), "rng");
		uint8_t sig[keySizeInBytes];
		size_t sigLen = sizeof(sig);
#if TARGET_OS_IPHONE
        /* TODO: This is returning another error on OS X */
		is_status(SecKeyRawSign(privKey, kSecPaddingPKCS1,
                                something, something_len + 1, sig, &sigLen),
                                errSecParam, "sign overflow");
#endif
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
#if TARGET_OS_IPHONE
        /* Thoses tests are making sure that MD2 and MD5 are NOT supported,
            but they still are on OS X */

        //CC_MD2(something, sizeof(something), digest);
		ok_status(!SecKeyRawSign(privKey, kSecPaddingPKCS1MD2,
			digest, CC_MD2_DIGEST_LENGTH, sig, &sigLen),
                  "don't sign md2 digest");   //FAIL
		ok_status(!SecKeyRawVerify(pubKey, kSecPaddingPKCS1MD2,
			digest, CC_MD2_DIGEST_LENGTH, sig, sigLen),
                  "verify sig on md2 digest fails");  //FAIL

        //CC_MD5(something, sizeof(something), digest);
		sigLen = sizeof(sig);
		ok_status(!SecKeyRawSign(privKey, kSecPaddingPKCS1MD5,
			digest, CC_MD5_DIGEST_LENGTH, sig, &sigLen),
            "don't sign md5 digest"); //FAIL
		ok_status(!SecKeyRawVerify(pubKey, kSecPaddingPKCS1MD5,
			digest, CC_MD5_DIGEST_LENGTH, sig, sigLen),
            "verify sig on md5 digest fails"); //FAIL
#endif

        //CCDigest(kCCDigestSHA1, something, sizeof(something), digest);
		sigLen = sizeof(sig);
		ok_status(SecKeyRawSign(privKey, kSecPaddingPKCS1SHA1,
			digest, CC_SHA1_DIGEST_LENGTH, sig, &sigLen),
            "sign sha1 digest");
		ok_status(SecKeyRawVerify(pubKey, kSecPaddingPKCS1SHA1,
			digest, CC_SHA1_DIGEST_LENGTH, sig, sigLen),
            "verify sig on sha1 digest");

#if TARGET_OS_IPHONE
        /* The assumptions in these tests are just wrong on OS X */
		uint8_t signature[keySizeInBytes], *ptr = signature;
		size_t signature_len = sizeof(signature);
		ok_status(SecKeyEncrypt(pubKey, kSecPaddingNone, sig, sigLen, signature, &signature_len), "inspect signature");
		is(signature_len, keySizeInBytes - 1, "got signature");  // FAIL for 2056
		while(*ptr && ((size_t)(ptr - signature) < signature_len)) ptr++;
		is(signature + signature_len - ptr, 16 /* length(\0 || OID_SHA1) */ + CC_SHA1_DIGEST_LENGTH, "successful decode");
#endif

#if TARGET_OS_IPHONE
        /* Those are not supported on OS X */
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

        test_encrypt_decrypt(pubKey, privKey, kSecPaddingNone, keySizeInBytes);
        test_encrypt_decrypt(pubKey, privKey, kSecPaddingPKCS1, keySizeInBytes);
        test_encrypt_decrypt(pubKey, privKey, kSecPaddingOAEP, keySizeInBytes);

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
        /* OS X: keys are always added to the keychain when generated */
		ok_status(SecItemAdd(privitem, NULL), "add private key"); //FAIL
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
        /* OS X: keys are always added to the keychain when generated */
		ok_status(SecItemAdd(pubitem, NULL), "add public key"); //FAIL
#endif
        ok_status(SecItemDelete(pubitem), "delete public key");
		CFReleaseNull(pubitem);

		/* Cleanup. */
		CFReleaseNull(pubKey);
		CFReleaseNull(privKey);
	}
}

#define kKeyGen2TestCount 11
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

    CFStringAppend(publicName, CFSTR("-Public-40"));
    CFStringAppend(privateName, CFSTR("-Private-40"));
	CFMutableDictionaryRef pubd = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFMutableDictionaryRef privd = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);

	CFDictionaryAddValue(pubd, kSecAttrLabel, publicName);
	CFDictionaryAddValue(privd, kSecAttrLabel, privateName);

	kzib = CFNumberCreate(NULL, kCFNumberSInt64Type, &keySizeInBits);
	CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
	CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
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
		is(SecKeyGetBlockSize(pubKey) * 8, (size_t) keySizeInBits, "public key size is ok");
		is(SecKeyGetBlockSize(privKey) * 8, (size_t) keySizeInBits, "private key size is ok");

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

#if TARGET_OS_IPHONE
        /* SecKeyEncrypt does not return errSecParam on OS X in that case */
        sigLen = keySizeInBytes;
		is_status(SecKeyEncrypt(pubKey2, kSecPaddingPKCS1SHA1,
			something, sizeof(something), sig, &sigLen), errSecParam,
            "encrypt something with invalid padding");
#endif

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


#if !TARGET_OS_IPHONE
// Only exists currently in MacOSX
typedef struct KDFVector_t {
    char *password;
    char *salt;
    int rounds;
    int alg;
    int dklen;
    char *expectedstr;
    int expected_failure;
} KDFVector;

static KDFVector kdfv[] = {
    // Test Case PBKDF2 - HMACSHA1 http://tools.ietf.org/html/draft-josefsson-pbkdf2-test-vectors-00
    { "password", "salt", 1, 1, 160, "0c60c80f961f0e71f3a9b524af6012062fe037a6", 0 },
    { "password", "salt", 2, 1, 160, "ea6c014dc72d6f8ccd1ed92ace1d41f0d8de8957", 0 },
    { "password", "salt", 4096, 1, 160, "4b007901b765489abead49d926f721d065a429c1", 0 },
    { "password", "salt", 1, 0, 160, NULL, -1} // This crashed
};

static size_t kdfvLen = sizeof(kdfv) / sizeof(KDFVector);

static int testSecKDF(CFStringRef password, CFDataRef salt, CFNumberRef rounds, CFStringRef alg, CFNumberRef dklen, CFDataRef expected, int expected_failure) {
    CFMutableDictionaryRef parameters = CFDictionaryCreateMutable(kCFAllocatorDefault, 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    int retval = 0;
    
    CFDictionaryAddValue(parameters, kSecAttrSalt, salt);
    CFDictionaryAddValue(parameters, kSecAttrKeySizeInBits, dklen);
    CFDictionaryAddValue(parameters, kSecAttrPRF, alg);
    CFDictionaryAddValue(parameters, kSecAttrRounds, rounds);
    
    SecKeyRef derivedKey = SecKeyDeriveFromPassword(password, parameters, NULL);
    if(derivedKey == NULL && expected_failure) {
        ok(1, "Correctly failed to produce a key");
        goto errOut;
    } else if(derivedKey == NULL) {
        ok(0, "Could not generate a key when we should have");
        goto errOut;
    }
    ok(1, "Made a new key");
    retval = 1;
    // NEEDS Fix -- ok(status = expectedEqualsComputed(expected, derivedKey), "Derived key is as expected");
errOut:
    if(parameters) CFRelease(parameters);
    if(derivedKey) CFRelease(derivedKey);
    return retval;
}

static CFDataRef CFDataCreateFromHexBytes(char *s) {
    if(!s) return NULL;
    size_t len = strlen(s);
    if(len%2) return NULL;
    len /= 2;
    uint8_t buf[len];
    for(size_t i=0; i<len; i++) {
        buf[i] = s[i*2] * 16 + s[i*2+1];
    }
    CFDataRef retval = CFDataCreate(NULL, buf, len);
    return retval;
}


static int
PBKDF2Test(KDFVector *kdfvec)
{
    CFDataRef expectedBytes = CFDataCreateFromHexBytes(kdfvec->expectedstr);
    CFStringRef password = CFStringCreateWithCString(NULL, kdfvec->password, kCFStringEncodingUTF8);
    CFDataRef salt = CFDataCreate(NULL, (const UInt8 *)kdfvec->salt, strlen(kdfvec->salt));
    CFNumberRef rounds = CFNumberCreate(NULL, kCFNumberIntType, &kdfvec->rounds);
    CFNumberRef dklen = CFNumberCreate(NULL, kCFNumberIntType, &kdfvec->dklen);
    int status = 1;
    
    ok(testSecKDF(password, salt, rounds, kSecAttrPRFHmacAlgSHA1, dklen, expectedBytes, kdfvec->expected_failure), "Test SecKeyDeriveFromPassword PBKDF2");

    CFReleaseNull(expectedBytes);
    CFReleaseNull(password);
    CFReleaseNull(salt);
    CFReleaseNull(rounds);
    CFReleaseNull(dklen);

    return status;
}


static void testkeyderivation() {
    for(size_t testcase = 0; testcase < kdfvLen; testcase++) {
        // diag("Test %lu\n", testcase + 1);
        ok(PBKDF2Test(&kdfv[testcase]), "Successful full test of KDF Vector");
    }
}

#else
static size_t kdfvLen = 0; // no kdf functions in Sec for iphone
#endif /* !TARGET_OS_IPHONE */

static void delete_key(SecKeyRef *key) {
    CFMutableDictionaryRef query = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                             &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(query, kSecValueRef, *key);
    SecItemDelete(query);
    CFReleaseNull(query);
    CFReleaseNull(*key);
}

static const int kTestSupportedCount = 3 + (4 * 12) + 2 + (4 * 10) + 2;
static void testsupportedalgos(size_t keySizeInBits)
{
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
              "Generate %ld bit (%ld byte) persistent RSA keypair",
              keySizeInBits, keySizeInBytes);
    CFRelease(kzib);
    CFRelease(kgp);

    is(SecKeyGetBlockSize(pubKey) * 8, (size_t) keySizeInBits, "public key size is ok");
    is(SecKeyGetBlockSize(privKey) * 8, (size_t) keySizeInBits, "private key size is ok");

    const SecKeyAlgorithm sign[] = {
        kSecKeyAlgorithmRSASignatureRaw,
        kSecKeyAlgorithmRSASignatureDigestPKCS1v15Raw,
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
        SecKeyAlgorithm algorithm = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), sign[i]);
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeSign, algorithm),
           "privKey supports sign algorithm %@", algorithm);
        ok(SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeVerify, algorithm),
           "pubKey supports verify algorithm %@", algorithm);
        // Since private key supports RSA decryption, our verify adapters happily accepts it.
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeVerify, algorithm),
           "privKey supports verify algorithm %@", algorithm);
        ok(!SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeSign, algorithm),
           "pubKey doesn't support sign algorithm %@", algorithm);
        CFReleaseNull(algorithm);
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
        kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA1AESGCM,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA224AESGCM,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA256AESGCM,
        kSecKeyAlgorithmRSAEncryptionOAEPSHA384AESGCM,
//        kSecKeyAlgorithmRSAEncryptionOAEPSHA512,
    };
    for (size_t i = 0; i < array_size(crypt); i++) {
        SecKeyAlgorithm algorithm = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@"), crypt[i]);
        ok(SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeDecrypt, algorithm),
           "privKey supports decrypt algorithm %@", algorithm);
        ok(SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeEncrypt, algorithm),
           "pubKey supports encrypt algorithm %@", algorithm);
        ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeEncrypt, algorithm),
           "privKey doesn't supports encrypt algorithm %@", algorithm);
        ok(!SecKeyIsAlgorithmSupported(pubKey, kSecKeyOperationTypeDecrypt, algorithm),
           "pubKey doesn't support decrypt algorithm %@", algorithm);
        CFReleaseNull(algorithm);
    }
    ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeDecrypt, kSecKeyAlgorithmRSAEncryptionOAEPSHA512),
       "privKey doesn't support decrypt algorithm %@", kSecKeyAlgorithmRSAEncryptionOAEPSHA512);
    ok(!SecKeyIsAlgorithmSupported(privKey, kSecKeyOperationTypeDecrypt, kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM),
       "privKey doesn't support decrypt algorithm %@", kSecKeyAlgorithmRSAEncryptionOAEPSHA512AESGCM);

    /* Cleanup. */
    delete_key(&pubKey);
    delete_key(&privKey);
}

#if 0
#define kTestSupportedCount 15
static void testsupportedalgos(size_t keySizeInBits)
{
    SecKeyRef pubKey = NULL, privKey = NULL;
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    CFNumberRef kzib;

    int32_t iKeySizeInBits = (int32_t) keySizeInBits;
    kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &iKeySizeInBits);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    CFDictionaryAddValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionaryAddValue(kgp, kSecAttrKeySizeInBits, kzib);
    CFDictionaryAddValue(kgp, kSecAttrLabel, CFSTR("sectests:testsupportedalgos"));

    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) persistent RSA keypair",
              keySizeInBits, keySizeInBytes);
    CFRelease(kzib);
    CFRelease(kgp);

    is(SecKeyGetBlockSize(pubKey) * 8, (size_t) keySizeInBits, "public key size is ok");
    is(SecKeyGetBlockSize(privKey) * 8, (size_t) keySizeInBits, "private key size is ok");

    CFSetRef keySet, expectedSet;

    CFIndex value = kSecKeyAlgorithmECDSASignatureDigestX962;
    CFNumberRef ECDSAX962 = CFNumberCreate(NULL, kCFNumberCFIndexType, &value);
    value = kSecKeyAlgorithmRSAEncryptionRaw;
    CFNumberRef RSARaw = CFNumberCreate(NULL, kCFNumberCFIndexType, &value);

    { // privkey
        keySet = SecKeyCopySupportedAlgorithms(privKey, kSecKeyOperationTypeSign);
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
        expectedSet = createAlgorithmSet(sign, array_size(sign));
        ok(CFSetIsSubset(expectedSet, keySet), "privkey contains expecting algos for signing");
        ok(!CFSetContainsValue(keySet, ECDSAX962));
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);

        keySet = SecKeyCopySupportedAlgorithms(privKey, kSecKeyOperationTypeVerify);
        expectedSet = createAlgorithmSet(sign, array_size(sign));
        ok(CFSetIsSubset(expectedSet, keySet), "privkey contains expecting algos for verification");
        ok(!CFSetContainsValue(keySet, ECDSAX962));
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);

        keySet = SecKeyCopySupportedAlgorithms(privKey, kSecKeyOperationTypeDecrypt);
        const SecKeyAlgorithm decrypt[] = {
            kSecKeyAlgorithmRSAEncryptionRaw,
            kSecKeyAlgorithmRSAEncryptionPKCS1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA512,
        };
        expectedSet = createAlgorithmSet(decrypt, array_size(decrypt));
        ok(CFSetIsSubset(expectedSet, keySet), "privkey contains expecting algos for decryption");
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);

        keySet = SecKeyCopySupportedAlgorithms(privKey, kSecKeyOperationTypeEncrypt);
        expectedSet = CFSetCreate(NULL, NULL, 0, &kCFTypeSetCallBacks);
        is(CFSetGetCount(keySet), 0, "privkey contains no algos for encryption");
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);
    }

    { // pubkey
        keySet = SecKeyCopySupportedAlgorithms(pubKey, kSecKeyOperationTypeVerify);
        const SecKeyAlgorithm verify[] = {
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
        expectedSet = createAlgorithmSet(verify, array_size(verify));
        ok(CFSetIsSubset(expectedSet, keySet), "pubkey contains expecting algos for verification");
        ok(!CFSetContainsValue(keySet, ECDSAX962),
           "pubkey does not contain ECDSA algorithms for verification");
        ok(!CFSetContainsValue(keySet, RSARaw),
           "pubkey does not contain encryption-specific algorithm for verification");
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);

        keySet = SecKeyCopySupportedAlgorithms(pubKey, kSecKeyOperationTypeSign);
        expectedSet = CFSetCreate(NULL, NULL, 0, &kCFTypeSetCallBacks);
        is(CFSetGetCount(keySet), 0, "pubkey contains no algos for signing");
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);

        const SecKeyAlgorithm crypt[] = {
            kSecKeyAlgorithmRSAEncryptionRaw,
            kSecKeyAlgorithmRSAEncryptionPKCS1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA1,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA224,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA256,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA384,
            kSecKeyAlgorithmRSAEncryptionOAEPSHA512,
        };
        expectedSet = createAlgorithmSet(crypt, array_size(crypt));
        keySet = SecKeyCopySupportedAlgorithms(pubKey, kSecKeyOperationTypeDecrypt);
#if TARGET_OS_IPHONE
        ok(CFSetIsSubset(expectedSet, keySet), "pubkey contains expecting algos for decryption");
#else
        ok(CFSetGetCount(keySet) == 0, "pubkey cannot decrypt");
#endif
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);

        keySet = SecKeyCopySupportedAlgorithms(pubKey, kSecKeyOperationTypeEncrypt);
        expectedSet = createAlgorithmSet(crypt, array_size(crypt));
        ok(CFSetIsSubset(expectedSet, keySet), "pubkey contains expecting algos for encryption");
        CFReleaseNull(keySet);
        CFReleaseNull(expectedSet);
    }

    /* Cleanup. */
    CFReleaseNull(RSARaw);
    CFReleaseNull(ECDSAX962);
    delete_key(&pubKey);
    delete_key(&privKey);
}
#endif

#if !TARGET_OS_IPHONE
static inline bool CFEqualSafe(CFTypeRef left, CFTypeRef right)
{
    if (left == NULL || right == NULL)
        return left == right;
    else
        return CFEqual(left, right);
}
#endif

#define kCreateWithDataTestCount 13
static void testcreatewithdata(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionarySetValue(kgp, kSecAttrKeySizeInBits, kzib);
    CFDictionarySetValue(kgp, kSecAttrIsPermanent, kCFBooleanFalse);
    CFDictionarySetValue(kgp, kSecAttrLabel, CFSTR("sectests:createwithdata"));

    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);

    CFMutableDictionaryRef kcwd = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                            &kCFTypeDictionaryValueCallBacks);;
    CFDictionarySetValue(kcwd, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionarySetValue(kcwd, kSecAttrKeySizeInBits, kzib);
    CFDictionarySetValue(kcwd, kSecAttrIsPermanent, kCFBooleanFalse);
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

        CFDataRef external = SecKeyCopyExternalRepresentation(dataKey, NULL);
        eq_cf(privExternalData, external, "priv keys differ");
        CFReleaseNull(external);
        CFReleaseNull(dataKey);

        CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPublic);
        dataKey = SecKeyCreateWithData(privExternalData, kcwd, &error);
        ok(!dataKey, "priv key SecKeyCreateWithData succeeded with invalid kSecAttrKeyClass");
        CFReleaseNull(error);
        CFReleaseNull(dataKey);

        CFMutableDataRef modifiedExternalData = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, privExternalData);
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

        CFDataRef external = SecKeyCopyExternalRepresentation(dataKey, NULL);
        eq_cf(pubExternalData, external, "pub keys differ");
        CFReleaseNull(external);
        CFReleaseNull(dataKey);

        CFDictionarySetValue(kcwd, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
        dataKey = SecKeyCreateWithData(pubExternalData, kcwd, &error);
        ok(!dataKey, "pub key SecKeyCreateWithData succeeded with invalid kSecAttrKeyClass");
        CFReleaseNull(error);
        CFReleaseNull(dataKey);

        CFMutableDataRef modifiedExternalData = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, pubExternalData);
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
    delete_key(&pubKey);
    delete_key(&privKey);
}

#define kCopyAttributesTestCount 20
static void testcopyattributes(unsigned long keySizeInBits, bool extractable)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionarySetValue(kgp, kSecAttrKeySizeInBits, kzib);
    CFDictionarySetValue(kgp, kSecAttrIsExtractable, extractable ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(kgp, kSecAttrLabel, CFSTR("sectests:copyattributes"));
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
        eq_cf(attrValue, kCFBooleanTrue, "invalid priv key kSecAttrCanDerive");

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

        attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDecrypt);
        eq_cf(attrValue, kCFBooleanFalse, "pub key invalid kSecAttrCanDecrypt");

        attrValue = CFDictionaryGetValue(attributes, kSecAttrCanDerive);
        eq_cf(attrValue, kCFBooleanTrue, "pub key invalid kSecAttrCanDerive");

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
    delete_key(&pubKey);
    delete_key(&privKey);
}

#define kCopyPublicKeyTestCount 5
static void testcopypublickey(unsigned long keySizeInBits, bool extractable, bool permanent) {
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionarySetValue(kgp, kSecAttrKeySizeInBits, kzib);
    CFDictionarySetValue(kgp, kSecAttrIsPermanent, permanent ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(kgp, kSecAttrIsExtractable, extractable ? kCFBooleanTrue : kCFBooleanFalse);
    CFDictionarySetValue(kgp, kSecAttrLabel, CFSTR("sectests:copypublickey"));
    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    if (permanent) {
        ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
                  "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
                  keySizeInBits, keySizeInBytes, (int)status);
    } else {
        NSError *error = nil;
        privKey = SecKeyCreateRandomKey(kgp, (void *)&error);
        pubKey = SecKeyCopyPublicKey(privKey);
        ok(privKey != NULL && pubKey != NULL, "Generate %ld bit (%ld byte) RSA keypair (error %@)",
           keySizeInBits, keySizeInBytes, error);
    }
    CFReleaseNull(kgp);
    CFReleaseNull(kzib);

    CFDataRef pubKeyData = SecKeyCopyExternalRepresentation(pubKey, NULL);

    SecKeyRef pubKeyCopy = NULL;

    { // privKey
        pubKeyCopy = SecKeyCopyPublicKey(privKey);
        ok(pubKeyCopy, "priv key SecKeyCopyPublicKey failed");
        CFDataRef pubKeyCopyData = SecKeyCopyExternalRepresentation(pubKeyCopy, NULL);
        eq_cf(pubKeyCopyData, pubKeyData, "pub key from priv key SecKeyCopyPublicKey and pub key differ");
        CFReleaseNull(pubKeyCopy);
        CFReleaseNull(pubKeyCopyData);
    }

    { // pubKey
        pubKeyCopy = SecKeyCopyPublicKey(pubKey);
        ok(pubKeyCopy, "pub key SecKeyCopyPublicKey failed");
        CFDataRef pubKeyCopyData = SecKeyCopyExternalRepresentation(pubKeyCopy, NULL);
        eq_cf(pubKeyCopyData, pubKeyData, "pub key from pub key SecKeyCopyPublicKey and pub key differ");
        CFReleaseNull(pubKeyCopy);
        CFReleaseNull(pubKeyCopyData);
    }

    CFReleaseNull(pubKeyData);
    if (permanent) {
        delete_key(&pubKey);
        delete_key(&privKey);
    } else {
        CFReleaseSafe(pubKey);
        CFReleaseSafe(privKey);
    }
}

static const char *kCertWithPubK = "\
MIIELjCCAxagAwIBAgIJALJlcYRBqZlZMA0GCSqGSIb3DQEBBQUAMIGUMQswCQYDVQQGEwJVUzELMAkG\
A1UECBMCQ0ExEjAQBgNVBAcTCUN1cGVydGlubzETMBEGA1UEChMKQXBwbGUgSW5jLjEPMA0GA1UECxMG\
Q29yZU9TMRwwGgYDVQQDExNBcHBsZSBUZXN0IENBMSBDZXJ0MSAwHgYJKoZIhvcNAQkBFhF2a3V6ZWxh\
QGFwcGxlLmNvbTAeFw0xNTA0MjkwODMyMDBaFw0yNTA0MjYwODMyMDBaMIGPMQswCQYDVQQGEwJVUzEL\
MAkGA1UECBMCQ0ExEjAQBgNVBAcTCUN1cGVydGlubzETMBEGA1UEChMKQXBwbGUgSW5jLjEQMA4GA1UE\
CxMHQ29yZSBPUzEWMBQGA1UEAxMNRmlsaXAgU3Rva2xhczEgMB4GCSqGSIb3DQEJARYRc3Rva2xhc0Bh\
cHBsZS5jb20wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDZcPMvjpu7i/2SkNDCrSC4Wa8m\
j3r6Lgn0crL4AgU+g3apptyy1eFf4RpNRJTGJ9ZMApbRZ0b7wX87Dq6UlCJUI9RJPOy+/TW0FM6mUVaF\
VSY+P+KMdRYGIOMLVI+LR6lRTf8MWbxZ238cqAIVnLHaE9HrXjyIrgX2IufJjt69WhwsJZuan7jmeXJS\
0AnESB31wS5NOn0tFDtzNAAQmoP8N8q6ZNC85tPVWBM61YLNjwSYl74y14QfX401P2pQRvwxTortRImk\
xjN4DBprG23e59UW2IBxYsqUA61jhA0yVy8gxYpCGa4bEBslhrnkAoSv+Zlyk7u2GyO13AC1dfRxAgMB\
AAGjgYUwgYIwPwYDVR0RBDgwNqAhBgorBgEEAYI3FAIDoBMMEXN0b2tsYXNAYXBwbGUuY29tgRFzdG9r\
bGFzQGFwcGxlLmNvbTA/BgNVHREEODA2oCEGCisGAQQBgjcUAgOgEwwRc3Rva2xhc0BhcHBsZS5jb22B\
EXN0b2tsYXNAYXBwbGUuY29tMA0GCSqGSIb3DQEBBQUAA4IBAQB87bZdl4XEDFA7UdPouhR3dKRl6evS\
MfC9/0jVcdB+P1mNJ/vIZdOZMY0asieOXhsI91nEcHUjbBCnu18mu2jR6SGiJsS/zr6enkpQMcztulMU\
kcjuSjT1hEzRv0LvEgWPOK+VpVqk6N0ZhybBQYVH2ECf7OU48CkFQFg9eLv6VaSK9+FqcBWpq8fXyOa7\
bL58bO5A3URHcmMWibv9/j+lpVeQBxt1UUwqBZT7DSLPw3QCj/zXfAGEu3izvEYaWwsQDhItwQJ6g6pp\
DLO741C7K8eKgvGs8ptna4RSosQda9bdnhZwT+g0UcorsVTUo+sR9+LW7INJ1zovRCL7NXit";

static const char *kPubK = "\
MIIBCgKCAQEA2XDzL46bu4v9kpDQwq0guFmvJo96+i4J9HKy+AIFPoN2qabcstXhX+EaTUSUxifWTAKW\
0WdG+8F/Ow6ulJQiVCPUSTzsvv01tBTOplFWhVUmPj/ijHUWBiDjC1SPi0epUU3/DFm8Wdt/HKgCFZyx\
2hPR6148iK4F9iLnyY7evVocLCWbmp+45nlyUtAJxEgd9cEuTTp9LRQ7czQAEJqD/DfKumTQvObT1VgT\
OtWCzY8EmJe+MteEH1+NNT9qUEb8MU6K7USJpMYzeAwaaxtt3ufVFtiAcWLKlAOtY4QNMlcvIMWKQhmu\
GxAbJYa55AKEr/mZcpO7thsjtdwAtXX0cQIDAQAB";

static const int kTestCountCopyPubKFromCert = 2;
static void testcopypubkfromcert() {
    NSData *certData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:kCertWithPubK]
                                                           options:NSDataBase64DecodingIgnoreUnknownCharacters];
    NSData *pubKData = [[NSData alloc] initWithBase64EncodedString:[NSString stringWithUTF8String:kPubK]
                                                           options:NSDataBase64DecodingIgnoreUnknownCharacters];
    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);
    SecKeyRef pubKey = NULL;
    ok_status(SecCertificateCopyPublicKey(cert, &pubKey), "export public key from certificate");
    NSData *pubKeyData = (__bridge_transfer NSData *)SecKeyCopyExternalRepresentation(pubKey, NULL);
    eq_cf( (__bridge CFTypeRef) pubKeyData, (__bridge CFTypeRef) pubKData, "public key exports itself into expected data");
    CFReleaseNull(pubKey);
    CFReleaseNull(cert);
}

static inline CFDataRef CFDataCreateWithHash(CFAllocatorRef allocator, const struct ccdigest_info *di, const uint8_t *buffer, const uint8_t length) {
    CFMutableDataRef result = CFDataCreateMutable(allocator, di->output_size);
    CFDataSetLength(result, di->output_size);

    ccdigest(di, length, buffer, CFDataGetMutableBytePtr(result));

    return result;
}

#define kSignAndVerifyTestCount 130
static void testsignverify(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionarySetValue(kgp, kSecAttrKeySizeInBits, kzib);
    CFDictionarySetValue(kgp, kSecAttrIsPermanent, kCFBooleanFalse);
    CFDictionarySetValue(kgp, kSecAttrLabel, CFSTR("sectests:signverify"));
    SecKeyRef pubKey = NULL, privKey = NULL, pubKeyIOS = NULL, privKeyIOS = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);
    CFReleaseNull(kzib);

    CFDataRef privKeyData = SecKeyCopyExternalRepresentation(privKey, NULL);
    CFDictionaryRef privKeyAttrs = SecKeyCopyAttributes(privKey);
    privKeyIOS = SecKeyCreateWithData(privKeyData, privKeyAttrs, NULL);
    CFReleaseNull(privKeyData);
    CFReleaseNull(privKeyAttrs);
    ok(privKeyIOS, "create IOS version of the private key");

    CFDataRef pubKeyData = SecKeyCopyExternalRepresentation(pubKey, NULL);
    CFDictionaryRef pubKeyAttrs = SecKeyCopyAttributes(pubKey);
    pubKeyIOS = SecKeyCreateWithData(pubKeyData, pubKeyAttrs, NULL);
    CFReleaseNull(pubKeyData);
    CFReleaseNull(pubKeyAttrs);
    ok(pubKeyIOS, "create IOS version of the public key");

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

        CFDataRef iosSignature = SecKeyCreateSignature(privKeyIOS, algorithm, dataToSign, &error);
        ok(iosSignature, "SecKeyCopySignature(ios) failed for algorithm %@", algorithm);
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
           "SecKeyVerifySignature(osx) failed osx-signature for algorithm %@", algorithm);
        CFReleaseNull(error);

        ok(SecKeyVerifySignature(pubKeyIOS, algorithm, dataToSign, signature, &error),
           "SecKeyVerifySignature(ios) failed for osx-signature for algorithm %@", algorithm);

        ok(SecKeyVerifySignature(pubKey, algorithm, dataToSign, iosSignature, &error),
           "SecKeyVerifySignature(osx) failed for ios-signature for algorithm %@", algorithm);
        CFReleaseNull(error);

        ok(SecKeyVerifySignature(pubKeyIOS, algorithm, dataToSign, iosSignature, &error),
           "SecKeyVerifySignature(ios) failed for ios-signature for algorithm %@", algorithm);

        CFMutableDataRef modifiedSignature = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, signature);
        *CFDataGetMutableBytePtr(modifiedSignature) ^= 0xff;

        ok(!SecKeyVerifySignature(pubKey, algorithm, dataToSign, modifiedSignature, &error),
           "SecKeyVerifySignature succeeded with bad signature for algorithm %@", algorithm);
        CFReleaseNull(error);

        CFMutableDataRef modifiedDataToSign = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, dataToSign);
        *CFDataGetMutableBytePtr(modifiedDataToSign) ^= 0xff;

        ok(!SecKeyVerifySignature(pubKey, algorithm, modifiedDataToSign, signature, &error),
           "SecKeyVerifySignature succeeded with bad data for algorithm %@", algorithm);
        CFReleaseNull(error);

        CFReleaseNull(modifiedDataToSign);
        CFReleaseNull(modifiedSignature);
        CFReleaseNull(signature);
        CFReleaseNull(iosSignature);
    }
        CFReleaseNull(dataToSign);
    }
    }
    CFReleaseNull(testData);
}

    delete_key(&pubKey);
    delete_key(&privKey);
    CFReleaseNull(pubKeyIOS);
    CFReleaseNull(privKeyIOS);
}

#define kNonExtractableTestCount 6
static void testnonextractable(unsigned long keySizeInBits)
{
    size_t keySizeInBytes = (keySizeInBits + 7) / 8;
    int32_t keysz32 = (int32_t)keySizeInBits;

    CFNumberRef kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keysz32);
    CFMutableDictionaryRef kgp = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
                                                           &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(kgp, kSecAttrKeyType, kSecAttrKeyTypeRSA);
    CFDictionarySetValue(kgp, kSecAttrKeySizeInBits, kzib);
    CFDictionarySetValue(kgp, kSecAttrIsPermanent, kCFBooleanTrue);
    CFDictionarySetValue(kgp, kSecAttrIsExtractable, kCFBooleanFalse);
    CFStringRef label = CFSTR("sectests:nonextractable");
    CFDictionarySetValue(kgp, kSecAttrLabel, label);
    SecKeyRef pubKey = NULL, privKey = NULL;
    OSStatus status;
    ok_status(status = SecKeyGeneratePair(kgp, &pubKey, &privKey),
              "Generate %ld bit (%ld byte) RSA keypair (status = %d)",
              keySizeInBits, keySizeInBytes, (int)status);
    CFReleaseNull(kgp);
    CFReleaseNull(kzib);

    // Get attributes, verify that ApplicationLabel is set and equals for both keys.
    CFDictionaryRef privAttrs = SecKeyCopyAttributes(privKey);
    CFDictionaryRef pubAttrs = SecKeyCopyAttributes(pubKey);

    ok(privAttrs, "able to get private key attributes");
    ok(pubAttrs, "able to get public key attributes");

    CFDataRef privLabel = CFDictionaryGetValue(privAttrs, kSecAttrApplicationLabel);
    CFDataRef pubLabel = CFDictionaryGetValue(pubAttrs, kSecAttrApplicationLabel);

    ok(privLabel && CFGetTypeID(privLabel) == CFDataGetTypeID() && CFDataGetLength(privLabel) == 20,
       "priv appLabel is present and of correct type");
    ok(pubLabel && CFGetTypeID(pubLabel) == CFDataGetTypeID() && CFDataGetLength(pubLabel) == 20,
       "priv appLabel is present and of correct type");
    eq_cf(privLabel, pubLabel, "applabels of pub and priv keys are equal");

    CFReleaseNull(pubAttrs);
    CFReleaseNull(privAttrs);

    delete_key(&pubKey);
    delete_key(&privKey);
}

#define kTestCount ((2 * kKeyGenTestCount) + kKeyGen2TestCount + (int) (kdfvLen*3)) + \
    kTestSupportedCount + kCreateWithDataTestCount + (kCopyAttributesTestCount * 2) + (kCopyPublicKeyTestCount * 4) + \
    kSignAndVerifyTestCount + kNonExtractableTestCount + \
    kTestCountCopyPubKFromCert

static void tests(void)
{
	/* Comment out lines below for testing generating all common key sizes,
	   disabled now for speed reasons. */
	//testkeygen(512);
	//testkeygen(768);
	testkeygen(1024);
	testkeygen(2056); // Stranged sized for edge cases in padding.
	//testkeygen(2048);
	//testkeygen(4096);

    testkeygen2(1024); // lots of FAIL!

    testsupportedalgos(1024);
    testcreatewithdata(1024);
    testcopyattributes(1024, true);
    testcopyattributes(1024, false);
    testcopypublickey(1024, true, true);
    testcopypublickey(1024, false, true);
    testcopypublickey(1024, true, false);
    testcopypublickey(1024, false, false);
    testsignverify(1024);
    testnonextractable(1024);

#if !TARGET_OS_IPHONE
    testkeyderivation();
#endif

    testcopypubkfromcert();
}

int kc_40_seckey(int argc, char *const *argv)
{
	plan_tests(kTestCount);

	tests();

	return 0;
}
