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
#include <Security/Security.h>
#include <Security/SecRandom.h>
#include <CommonCrypto/CommonDigest.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>

#if 0
#include <Security/SecCertificate.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecKey.h>
#include <Security/SecItem.h>
#include <Security/SecAsn1Types.h>
#include <Security/oidsalg.h>
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
#define kEncryptDecryptTestCount 5
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
        SecRandomCopyBytes(kSecRandomDefault, sizeof(secret), secret);

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
    size_t result = SecKeyGetBlockSize(key);

    /* This is only RSA */
    if (whichSize == kSecKeyKeySizeInBits)
        result *= 8;

    return result;
}
#endif

#define kKeyGenTestCount (11 + (3*kEncryptDecryptTestCount))
static void testkeygen(size_t keySizeInBits) {
	SecKeyRef pubKey = NULL, privKey = NULL;
	size_t keySizeInBytes = (keySizeInBits + 7) / 8;
	CFNumberRef kzib;

	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keySizeInBits);
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
        SecRandomCopyBytes(kSecRandomDefault, sizeof(something), something);
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

	kzib = CFNumberCreate(NULL, kCFNumberSInt32Type, &keySizeInBits);
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

static int testSecKDF(CFStringRef password, CFDataRef salt, CFNumberRef rounds, CFDataRef alg, CFNumberRef dklen, CFDataRef expected, int expected_failure) {
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
    
    if(expectedBytes) CFRelease(expectedBytes);
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


/* Test basic add delete update copy matching stuff. */
#define kTestCount ((2 * kKeyGenTestCount) + kKeyGen2TestCount + (int) (kdfvLen*3))
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
#if !TARGET_OS_IPHONE
    testkeyderivation();
#endif
}

int kc_40_seckey(int argc, char *const *argv)
{
	plan_tests(kTestCount);

	tests();

	return 0;
}
