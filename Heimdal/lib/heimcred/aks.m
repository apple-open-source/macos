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

#import "aks.h"
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecRandom.h>
#import <CommonCrypto/CommonCryptor.h>
#import <CommonCrypto/CommonCryptorSPI.h>
#import <TargetConditionals.h>
#import "gssoslog.h"
#import "heimbase.h"
#import <os/transaction_private.h>
#import <os/overflow.h>
#define PLATFORM_SUPPORT_CLASS_F !TARGET_OS_SIMULATOR

#import <AssertMacros.h>
#if PLATFORM_SUPPORT_CLASS_F
#import <libaks.h>
#endif
#import "HeimCredCoder.h"
#import "common.h"
#import "roken.h"

// Use the AKS maximum wrapped key length macro
#ifndef AKS_WRAP_KEY_MAX_WRAPPED_KEY_LEN
#define AKS_WRAP_KEY_MAX_WRAPPED_KEY_LEN 128
#endif

/*
 * stored as
 *  ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐─ ─ ─ ─ ─ ─ ─ ─ ─ ┐─ ─ ─ ─ ─ ─ ─ ─ ─ ┐─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐─ ─ ─ ─
 *          32          wrapped_key_len           16                 variable           16
 * ┌──────────────────┼──────────────────┼──────────────────┼────────────────────────┼───────┐
 * │ wrapped_key_len  │   wrapped_key    │        iv        │         ctData         │  tag  │
 * └──────────────────┴──────────────────┴──────────────────┴────────────────────────┴───────┘
 */
static const size_t ivSize = 16;
#if PLATFORM_SUPPORT_CLASS_F
static os_transaction_t keyNotReadyTransaction = NULL;
#endif

NSData *
ksEncryptData(NSData *plainText)
{
    NSMutableData *blob = NULL;
    
    const size_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    uint8_t bulkKey[bulkKeySize];
    uint8_t iv[ivSize];
    uint8_t bulkKeyWrapped[AKS_WRAP_KEY_MAX_WRAPPED_KEY_LEN];
    uint32_t key_wrapped_size;
    CCCryptorStatus ccerr;

    heim_assert([plainText isKindOfClass:[NSData class]], "input is not NSData");
    
    size_t ctLen = [plainText length];
    size_t tagLen = 16;

    if (SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey)) {
	abort();
    }
    if (SecRandomCopyBytes(kSecRandomDefault, ivSize, iv)) {
	abort();
    }

    int bulkKeyWrappedSize;
#if PLATFORM_SUPPORT_CLASS_F
    kern_return_t error;

    bulkKeyWrappedSize = sizeof(bulkKeyWrapped);

    error = aks_wrap_key(bulkKey, sizeof(bulkKey), key_class_f, bad_keybag_handle, bulkKeyWrapped, &bulkKeyWrappedSize, NULL);
    if (error) {
	os_log_error(GSSOSLog(), "Error with wrap key: %d", error);
	// When there is a key error, start an os transaction instead of aborting.  This will keep the service running for the users. The risk is that if the service exits, then all the credentials are lost.  While not ideal, it is better than the service crashing when it tries to save the credentials.
	if (!keyNotReadyTransaction) {
	    keyNotReadyTransaction = os_transaction_create("com.apple.Heimdal.GSSCred.keyError");
	}
	return NULL;
    }
    //complete the transaction, if present
    if (keyNotReadyTransaction) {
	keyNotReadyTransaction = NULL;
    }
    if (bulkKeyWrappedSize <= 0 || (size_t)bulkKeyWrappedSize > sizeof(bulkKeyWrapped)) {
	os_log_error(GSSOSLog(), "Wrapped key size error: %d", bulkKeyWrappedSize);
	return NULL;
    }

#else
    bulkKeyWrappedSize = bulkKeySize;
    memcpy(bulkKeyWrapped, bulkKey, bulkKeySize);
#endif
    key_wrapped_size = (uint32_t)bulkKeyWrappedSize;
    
    // Calculate the total blob size and verify it doesn't overflow
    size_t blobLen = 0;
    if (os_add_overflow(sizeof(key_wrapped_size), key_wrapped_size, &blobLen) ||
        os_add_overflow(blobLen, ivSize, &blobLen) ||
        os_add_overflow(blobLen, ctLen, &blobLen) ||
        os_add_overflow(blobLen, tagLen, &blobLen)) {
        os_log_error(GSSOSLog(), "Blob size calculation would overflow");
        return NULL;
    }
    
    blob = [[NSMutableData alloc] initWithLength:blobLen];
    if (blob == NULL) {
	os_log_error(GSSOSLog(), "Failed to allocate memory for blob");
	return NULL;
    }

    UInt8 *cursor = [blob mutableBytes];


    memcpy(cursor, &key_wrapped_size, sizeof(key_wrapped_size));
    cursor += sizeof(key_wrapped_size);
    
    memcpy(cursor, bulkKeyWrapped, key_wrapped_size);
    cursor += key_wrapped_size;

    memcpy(cursor, iv, ivSize);
    cursor += ivSize;

    ccerr = CCCryptorGCMOneshotEncrypt(kCCAlgorithmAES,       // algorithm
				       bulkKey,               // key bytes
				       bulkKeySize,           // key length
				       iv,                    // IV/nonce bytes
				       ivSize,                // IV/nonce length
				       NULL,                  // additional bytes
				       0,                     // additional bytes length
				       plainText.bytes,       // plaintext bytes
				       ctLen,                 // plaintext length
				       cursor,                // ciphertext bytes
				       cursor + ctLen,        // authentication tag bytes
				       tagLen);               // authentication tag length
    memset_s(bulkKey, 0, sizeof(bulkKey), sizeof(bulkKey));
    if (ccerr || tagLen != 16) {
	os_log_error(GSSOSLog(), "Encryption error: %d", ccerr);
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

    // Validate input
    if (blob == NULL) {
        os_log_error(GSSOSLog(), "Decryption failed: Input blob is NULL");
        return NULL;
    }
    
    if (![blob isKindOfClass:[NSData class]]) {
        os_log_error(GSSOSLog(), "Decryption failed: Input is not NSData");
        return NULL;
    }

    size_t blobLen = [blob length];
    const uint8_t *cursor = [blob bytes];

    uint32_t wrapped_key_size;
    
    size_t ctLen = blobLen;
    
    /* tag is stored after the plain text data */
    size_t tagLen = 16;
    if (ctLen < tagLen) {
        os_log_error(GSSOSLog(), "Decryption failed: Blob too small for authentication tag");
        return NULL;
    }
    ctLen -= tagLen;

    if (ctLen < sizeof(wrapped_key_size)) {
        os_log_error(GSSOSLog(), "Decryption failed: Blob too small for wrapped key size");
        return NULL;
    }

    memcpy(&wrapped_key_size, cursor, sizeof(wrapped_key_size));

    cursor += sizeof(wrapped_key_size);
    ctLen -= sizeof(wrapped_key_size);

    /* Validate key wrap length against total length */
    if (ctLen < wrapped_key_size) {
        os_log_error(GSSOSLog(), "Decryption failed: Blob too small for wrapped key data (need %u bytes, have %zu)", 
                     wrapped_key_size, ctLen);
        return NULL;
    }
    
    if (wrapped_key_size > AKS_WRAP_KEY_MAX_WRAPPED_KEY_LEN) {
        os_log_error(GSSOSLog(), "Decryption failed: Invalid wrapped key size: %u (maximum allowed: %d)", 
                     wrapped_key_size, AKS_WRAP_KEY_MAX_WRAPPED_KEY_LEN);
        return NULL;
    }

    int keySize = sizeof(bulkKey);
#if PLATFORM_SUPPORT_CLASS_F

    error = aks_unwrap_key(cursor, wrapped_key_size, key_class_f, bad_keybag_handle, bulkKey, &keySize);
    if (error != KERN_SUCCESS) {
        os_log_error(GSSOSLog(), "Decryption failed: Error unwrapping key: %d", error);
        goto out;
    }
#else
    if (bulkKeySize != wrapped_key_size) {
        os_log_error(GSSOSLog(), "Decryption failed: Key size mismatch in simulator mode");
        error = EINVAL;
        goto out;
    }
    memcpy(bulkKey, cursor, bulkKeySize);
    keySize = 32;
#endif

    if (keySize != 32) {
        os_log_error(GSSOSLog(), "Decryption failed: Invalid unwrapped key size: %d (expected 32)", keySize);
        error = EINVAL;
        goto out;
    }

    cursor += wrapped_key_size;
    ctLen -= wrapped_key_size;

    if (ctLen < ivSize) {
        os_log_error(GSSOSLog(), "Decryption failed: Not enough data for IV (need %zu bytes, have %zu)", 
                     ivSize, ctLen);
        error = EINVAL;
        goto out;
    }

    iv = cursor;
    cursor += ivSize;
    ctLen -= ivSize;

    plainText = [NSMutableData dataWithLength:ctLen];
    if (!plainText) {
        os_log_error(GSSOSLog(), "Decryption failed: Failed to allocate memory for plaintext");
        error = ENOMEM;
        goto out;
    }

    tag = malloc(tagLen);
    if (tag == NULL) {
        os_log_error(GSSOSLog(), "Decryption failed: Failed to allocate memory for tag");
        error = ENOMEM;
        goto out;
    }

    ccerr = CCCryptorGCMOneshotDecrypt(kCCAlgorithmAES,           // algorithm
                                       bulkKey,                   // key bytes
                                       bulkKeySize,               // key length
                                       iv,                        // IV/nonce bytes
                                       ivSize,                    // IV/nonce length
                                       NULL,                      // additional bytes
                                       0,                         // additional bytes length
                                       cursor,                    // ciphertext bytes
                                       ctLen,                     // ciphertext length
                                       plainText.mutableBytes,    // plaintext bytes
                                       cursor + ctLen,            // authentication tag bytes
                                       tagLen);                   // authentication tag length
    /* Decrypt the cipherText with the bulkKey. */
    if (ccerr) {
        os_log_error(GSSOSLog(), "Decryption failed: CCCryptorGCMOneshotDecrypt error: %d", ccerr);
        goto out;
    }

    clear = plainText;
out:
    memset_s(bulkKey, 0, bulkKeySize, bulkKeySize);
    free(tag);

    return clear;
}

