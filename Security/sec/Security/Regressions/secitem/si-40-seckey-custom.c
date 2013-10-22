/*
 *  si-40-seckey.c
 *  Security
 *
 *  Created by Michael Brouwer on 1/29/07.
 *  Copyright (c) 2007-2008,2010 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecKeyPriv.h>

#include "Security_regressions.h"

#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); if (_cf) {  (CF) = NULL; CFRelease(_cf); } }

static SecKeyRef customKey;
static SecKeyRef initedCustomKey;

static OSStatus CustomKeyInit(SecKeyRef key, const uint8_t *key_data,
    CFIndex key_len, SecKeyEncoding encoding)
{
    ok(key, "CustomKeyInit");
    is(key->key, NULL, "key->key is NULL");
    initedCustomKey = key;
    return errSecSuccess;
}

static void CustomKeyDestroy(SecKeyRef key)
{
    is(customKey, key, "CustomKeyDestroy");
}

static OSStatus CustomKeyRawSign(SecKeyRef key, SecPadding padding,
	const uint8_t *dataToSign, size_t dataToSignLen,
	uint8_t *sig, size_t *sigLen)
{
    is(customKey, key, "CustomKeyRawSign");
    return errSecSuccess;
}

static OSStatus CustomKeyRawVerify(
    SecKeyRef key, SecPadding padding, const uint8_t *signedData,
    size_t signedDataLen, const uint8_t *sig, size_t sigLen)
{
    is(customKey, key, "CustomKeyRawVerify");
    return errSecSuccess;
}

static OSStatus CustomKeyEncrypt(SecKeyRef key, SecPadding padding,
    const uint8_t *plainText, size_t plainTextLen,
	uint8_t *cipherText, size_t *cipherTextLen)
{
    is(customKey, key, "CustomKeyEncrypt");
    return errSecSuccess;
}

static OSStatus CustomKeyDecrypt(SecKeyRef key, SecPadding padding,
    const uint8_t *cipherText, size_t cipherTextLen,
    uint8_t *plainText, size_t *plainTextLen)
{
    is(customKey, key, "CustomKeyDecrypt");
    return errSecSuccess;
}

static OSStatus CustomKeyCompute(SecKeyRef key,
    const uint8_t *pub_key, size_t pub_key_len,
    uint8_t *computed_key, size_t *computed_key_len)
{
    is(customKey, key, "CustomKeyCompute");
    return errSecSuccess;
}

static size_t CustomKeyBlockSize(SecKeyRef key)
{
    is(customKey, key, "CustomKeyBlockSize");
    return 42;
}

static CFDictionaryRef CustomKeyCopyAttributeDictionary(SecKeyRef key)
{
    is(customKey, key, "CustomKeyCopyAttributeDictionary");
    CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL,
        0, NULL, NULL);
    return dict;
}

SecKeyDescriptor kCustomKeyDescriptor = {
    kSecKeyDescriptorVersion,
    "CustomKey",
    0, /* extraBytes */
    CustomKeyInit,
    CustomKeyDestroy,
    CustomKeyRawSign,
    CustomKeyRawVerify,
    CustomKeyEncrypt,
    CustomKeyDecrypt,
    CustomKeyCompute,
    CustomKeyBlockSize,
	CustomKeyCopyAttributeDictionary,
};

/* Test basic add delete update copy matching stuff. */
static void tests(void)
{
    const uint8_t *keyData = (const uint8_t *)"abc";
    CFIndex keyDataLength = 3;
    SecKeyEncoding encoding = kSecKeyEncodingRaw;
    ok(customKey = SecKeyCreate(kCFAllocatorDefault,
        &kCustomKeyDescriptor, keyData, keyDataLength, encoding),
        "create custom key");
    is(customKey, initedCustomKey, "CustomKeyInit got the right key");

    SecPadding padding = kSecPaddingPKCS1;
    const uint8_t *src = NULL;
    size_t srcLen = 3;
    uint8_t *dst = NULL;
    size_t dstLen = 3;

    ok_status(SecKeyDecrypt(customKey, padding, src, srcLen, dst, &dstLen),
        "SecKeyDecrypt");
    ok_status(SecKeyEncrypt(customKey, padding, src, srcLen, dst, &dstLen),
        "SecKeyEncrypt");
    ok_status(SecKeyRawSign(customKey, padding, src, srcLen, dst, &dstLen),
        "SecKeyRawSign");
    ok_status(SecKeyRawVerify(customKey, padding, src, srcLen, dst, dstLen),
        "SecKeyRawVerify");
    is(SecKeyGetSize(customKey, kSecKeyKeySizeInBits), (size_t)42*8, "SecKeyGetSize");

    CFDictionaryRef attrDict = NULL;
    ok(attrDict = SecKeyCopyAttributeDictionary(customKey),
        "SecKeyCopyAttributeDictionary");
    CFReleaseNull(attrDict);

    //ok(SecKeyGeneratePair(customKey, ), "SecKeyGeneratePair");
    ok(SecKeyGetTypeID() != 0, "SecKeyGetTypeID works");

    if (customKey) {
        CFRelease(customKey);
        customKey = NULL;
    }
}

int si_40_seckey_custom(int argc, char *const *argv)
{
	plan_tests(18);


	tests();

	return 0;
}
