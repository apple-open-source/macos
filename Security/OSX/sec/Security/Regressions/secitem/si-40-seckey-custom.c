/*
 * Copyright (c) 2007-2008,2010,2012-2014 Apple Inc. All Rights Reserved.
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
#include <Security/SecKeyPriv.h>

#include "Security_regressions.h"

#define CFReleaseNull(CF) { CFTypeRef _cf = (CF); if (_cf) {  (CF) = NULL; CFRelease(_cf); } }

static SecKeyRef customKey;
static SecKeyRef initedCustomKey;

static OSStatus CustomKeyInit(SecKeyRef key, const uint8_t *key_data,
    CFIndex key_len, SecKeyEncoding encoding)
{
    ok(key, "CustomKeyInit");
    ok(key && key->key == NULL, "key->key is NULL");
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
    return 5;
}

static CFDictionaryRef CustomKeyCopyAttributeDictionary(SecKeyRef key)
{
    is(customKey, key, "CustomKeyCopyAttributeDictionary");
    CFDictionaryRef dict = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL,
        0, NULL, NULL);
    return dict;
}

static CFStringRef CustomKeyCopyDescribe(SecKeyRef key)
{
    return CFStringCreateWithFormat(NULL, NULL, CFSTR("%s"), key->key_class->name);
}


SecKeyDescriptor kCustomKeyDescriptor_version0 = {
    0,
    "CustomKeyVersion0",
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
    CustomKeyCopyDescribe,
    (void *)abort,
    (void *)abort,
    (void *)abort,
    (void *)abort,
};

SecKeyDescriptor kCustomKeyDescriptor_version1 = {
    1,
    "CustomKeyVersion1",
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
    CustomKeyCopyDescribe,
    NULL,
    (void *)abort,
    (void *)abort,
    (void *)abort,
};

SecKeyDescriptor kCustomKeyDescriptor_version2 = {
    2,
    "CustomKeyVersion2",
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
    CustomKeyCopyDescribe,
    NULL,
    NULL,
    (void *)abort,
    (void *)abort,
};

SecKeyDescriptor kCustomKeyDescriptor_version3 = {
    3,
    "CustomKeyVersion3",
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
    CustomKeyCopyDescribe,
    NULL,
    NULL,
    NULL,
    NULL,
};

/* Test basic add delete update copy matching stuff. */
static void tests(SecKeyDescriptor *descriptor)
{
    const uint8_t *keyData = (const uint8_t *)"abc";
    CFIndex keyDataLength = 3;
    SecKeyEncoding encoding = kSecKeyEncodingRaw;
    ok(customKey = SecKeyCreate(kCFAllocatorDefault,
        descriptor, keyData, keyDataLength, encoding),
        "create custom key");
    is(customKey, initedCustomKey, "CustomKeyInit got the right key");

    SecPadding padding = kSecPaddingPKCS1;
    const uint8_t *src = (const uint8_t *)"defgh";
    size_t srcLen = 5;
    uint8_t dst[5];
    size_t dstLen = 5;

    ok_status(SecKeyDecrypt(customKey, padding, src, srcLen, dst, &dstLen),
        "SecKeyDecrypt");
    ok_status(SecKeyEncrypt(customKey, padding, src, srcLen, dst, &dstLen),
        "SecKeyEncrypt");
    ok_status(SecKeyRawSign(customKey, padding, src, srcLen, dst, &dstLen),
        "SecKeyRawSign");
    ok_status(SecKeyRawVerify(customKey, padding, src, srcLen, dst, dstLen),
        "SecKeyRawVerify");
    is(SecKeyGetSize(customKey, kSecKeyKeySizeInBits), (size_t)5*8, "SecKeyGetSize");

    CFDictionaryRef attrDict = NULL;
    ok(attrDict = SecKeyCopyAttributeDictionary(customKey),
        "SecKeyCopyAttributeDictionary");
    CFReleaseNull(attrDict);

    CFDataRef pubdata = NULL;
    ok(SecKeyCopyPublicBytes(customKey, &pubdata) != 0, "SecKeyCopyPublicBytes");
    CFReleaseNull(pubdata);

    CFDataRef wrapped;
    wrapped = _SecKeyCopyWrapKey(customKey, kSecKeyWrapPublicKeyPGP, pubdata, NULL, NULL, NULL);
    ok(wrapped == NULL, "_SecKeyCopyWrapKey");
    CFReleaseNull(wrapped);

    wrapped = _SecKeyCopyUnwrapKey(customKey, kSecKeyWrapPublicKeyPGP, pubdata, NULL, NULL, NULL);
    ok(wrapped == NULL, "_SecKeyCopyUnwrapKey");
    CFReleaseNull(wrapped);

    //ok(SecKeyGeneratePair(customKey, ), "SecKeyGeneratePair");
    ok(SecKeyGetTypeID() != 0, "SecKeyGetTypeID works");

    if (customKey) {
        CFRelease(customKey);
        customKey = NULL;
    }
}

int si_40_seckey_custom(int argc, char *const *argv)
{
	plan_tests(20 * 4);

	tests(&kCustomKeyDescriptor_version0);
	tests(&kCustomKeyDescriptor_version1);
	tests(&kCustomKeyDescriptor_version2);
	tests(&kCustomKeyDescriptor_version3);

	return 0;
}
