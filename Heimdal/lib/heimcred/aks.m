/*-
 * Copyright (c) 2013 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Portions Copyright (c) 2013, 2016 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecRandom.h>
#import <CommonCrypto/CommonCryptor.h>
#import <CommonCrypto/CommonCryptorSPI.h>
#import <TargetConditionals.h>
#import <syslog.h>

#define PLATFORM_SUPPORT_CLASS_F TARGET_OS_EMBEDDED


#import <AssertMacros.h>
#if PLATFORM_SUPPORT_CLASS_F
#import <libaks.h>
#endif
#import "HeimCredCoder.h"
#import "common.h"
#import "roken.h"


#if PLATFORM_SUPPORT_CLASS_F

static keybag_handle_t
get_keybag(void)
{
    static keybag_handle_t handle;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        kern_return_t kr = aks_get_system(device_keybag_handle, &handle);
	if (kr != KERN_SUCCESS)
	    abort();
    });
    return handle;
}
#endif


/*
 * stored as [32:wrapped_key_len][wrapped_key_len:wrapped_key][iv:ivSize][variable:ctdata][16:tag]
 */

static const size_t ivSize = 16;

NSData *
ksEncryptData(NSData *plainText)
{
    NSMutableData *blob = NULL;
    
    const size_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    const uint32_t maxKeyWrapOverHead = 8 + 32;
    uint8_t bulkKey[bulkKeySize];
    uint8_t iv[ivSize];
    uint8_t bulkKeyWrapped[bulkKeySize + maxKeyWrapOverHead];
    uint32_t key_wrapped_size;
    CCCryptorStatus ccerr;

    if (![plainText isKindOfClass:[NSData class]]) abort();
    
    size_t ctLen = [plainText length];
    size_t tagLen = 16;

    if (SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey))
	abort();
    if (SecRandomCopyBytes(kSecRandomDefault, ivSize, iv))
	abort();

    int bulkKeyWrappedSize;
#if PLATFORM_SUPPORT_CLASS_F
    kern_return_t error;

    bulkKeyWrappedSize = sizeof(bulkKeyWrapped);

    error = aks_wrap_key(bulkKey, sizeof(bulkKey), key_class_f, get_keybag(), bulkKeyWrapped, &bulkKeyWrappedSize, NULL);
    if (error)
	abort();
    if ((unsigned long)bulkKeyWrappedSize > sizeof(bulkKeyWrapped))
	abort();

#else
    bulkKeyWrappedSize = bulkKeySize;
    memcpy(bulkKeyWrapped, bulkKey, bulkKeySize);
#endif
    key_wrapped_size = (uint32_t)bulkKeyWrappedSize;
    
    size_t blobLen = sizeof(key_wrapped_size) + key_wrapped_size + ivSize + ctLen + tagLen;
    
    blob = [[NSMutableData alloc] initWithLength:blobLen];
    if (blob == NULL)
	return NULL;

    UInt8 *cursor = [blob mutableBytes];


    memcpy(cursor, &key_wrapped_size, sizeof(key_wrapped_size));
    cursor += sizeof(key_wrapped_size);
    
    memcpy(cursor, bulkKeyWrapped, key_wrapped_size);
    cursor += key_wrapped_size;

    memcpy(cursor, iv, ivSize);
    cursor += ivSize;

    ccerr = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128,
			 bulkKey, bulkKeySize,
			 iv, ivSize,  /* iv */
			 NULL, 0,  /* auth data */
			 [plainText bytes], ctLen,
			 cursor,
			 cursor + ctLen, &tagLen);
    memset_s(bulkKey, 0, sizeof(bulkKey), sizeof(bulkKey));
    if (ccerr || tagLen != 16) {
	[blob release];
	return NULL;
    }

    return blob;
}

NSData *
ksDecryptData(NSData * blob)
{
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    uint8_t bulkKey[bulkKeySize];
    int error = EINVAL;
    CCCryptorStatus ccerr;
    uint8_t *tag = NULL;
    const uint8_t *iv = NULL;
    NSMutableData *clear = NULL, *plainText = NULL;

    size_t blobLen = [blob length];
    const uint8_t *cursor = [blob bytes];

    uint32_t wrapped_key_size;
    
    size_t ctLen = blobLen;
    
    /* tag is stored after the plain text data */
    size_t tagLen = 16;
    if (ctLen < tagLen)
	return NULL;
    ctLen -= tagLen;

    if (ctLen < sizeof(wrapped_key_size))
	return NULL;

    memcpy(&wrapped_key_size, cursor, sizeof(wrapped_key_size));

    cursor += sizeof(wrapped_key_size);
    ctLen -= sizeof(wrapped_key_size);

    /* Validate key wrap length against total length */
    if (ctLen < wrapped_key_size)
	return NULL;

    int keySize = sizeof(bulkKey);
#if PLATFORM_SUPPORT_CLASS_F

    error = aks_unwrap_key(cursor, wrapped_key_size, key_class_f, get_keybag(), bulkKey, &keySize);
    if (error != KERN_SUCCESS)
	goto out;
#else
    if (bulkKeySize != wrapped_key_size) {
	error = EINVAL;
	goto out;
    }
    memcpy(bulkKey, cursor, bulkKeySize);
    keySize = 32;
#endif

    if (keySize != 32) {
	error = EINVAL;
	goto out;
    }

    cursor += wrapped_key_size;
    ctLen -= wrapped_key_size;

    if (ctLen < ivSize) {
	error = EINVAL;
	goto out;
    }

    iv = cursor;
    cursor += ivSize;
    ctLen -= ivSize;

    plainText = [NSMutableData dataWithLength:ctLen];
    if (!plainText) {
        goto out;
    }

    tag = malloc(tagLen);
    if (tag == NULL) {
        goto out;
    }

    ccerr = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128,
			 bulkKey, bulkKeySize,
			 iv, ivSize,  /* iv */
			 NULL, 0,  /* auth data */
			 cursor, ctLen,
			 [plainText mutableBytes],
			 tag, &tagLen);
    /* Decrypt the cipherText with the bulkKey. */
    if (ccerr) {
	goto out;
    }
    if (tagLen != 16) {
	goto out;
    }

    /* check that tag stored after the plaintext is correct */
    cursor += ctLen;
    if (ct_memcmp(tag, cursor, tagLen) != 0) {
	syslog(LOG_ERR, "incorrect tag on credential data");
	goto out;
    }

    clear = [plainText retain];
out:
    memset_s(bulkKey, 0, bulkKeySize, bulkKeySize);
    free(tag);

    return clear;
}

