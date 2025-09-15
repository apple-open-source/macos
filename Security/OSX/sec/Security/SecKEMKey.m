/*
 * Copyright (c) 2025 Apple Inc. All Rights Reserved.
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

#import "SecKEMKeyPriv.h"

#include <Security/SecBase.h>
#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCFAllocator.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

#import <Foundation/Foundation.h>

#include <corecrypto/cckem.h>

void SecKEMPublicKeyDestroy(SecKeyRef key) {
    // Zero out the public key
    cckem_pub_ctx_t ctx = key->key;
    if (ctx != NULL) {
        cckem_pub_ctx_clear(ctx->info, ctx);
        free(ctx);
    }
}

CFDataRef SecKEMPublicKeyCopyData(cckem_pub_ctx_t ctx, CFErrorRef *error) {
    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(kCFAllocatorDefault, cckem_pubkey_nbytes_ctx(ctx)));
    size_t size = data.length;
    int err = cckem_export_pubkey(ctx, &size, data.mutableBytes);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export ML-KEM pubkey"));
        return NULL;
    }

    data.length = size;
    return CFBridgingRetain(data);
}

size_t SecKEMPublicKeyBlockSize(SecKeyRef key) {
    cckem_pub_ctx_t ctx = key->key;
    if (ctx == NULL) {
        secerror("Invalid key data: The provided key does not contain a valid KEM context.");
        return 0;
    }

    return cckem_pubkey_nbytes_ctx(ctx);
}

void SecKEMPrivateKeyDestroy(SecKeyRef key) {
    // Zero out the private key
    cckem_full_ctx_t ctx = key->key;
    if (ctx != NULL) {
        cckem_full_ctx_clear(ctx->info, ctx);
        free(ctx);
    }
}

size_t SecKEMPrivateKeyBlockSize(SecKeyRef key) {
    cckem_full_ctx_t ctx = key->key;
    if (ctx == NULL) {
        secerror("Invalid key data: The provided key does not contain a valid KEM context.");
        return 0;
    }

    return cckem_privkey_nbytes_ctx(cckem_public_ctx(ctx));
}

CFDataRef SecKEMPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    cckem_full_ctx_t ctx = key->key;
    if (ctx == NULL) {
        SecError(errSecParam, error, CFSTR("Invalid key data: The provided key does not contain a valid KEM context."));
        return NULL;
    }

    size_t pubKeySize = cckem_pubkey_nbytes_ctx(cckem_public_ctx(ctx));
    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), pubKeySize + cckem_privkey_nbytes_ctx((void *)ctx)));
    size_t pubSize = data.length;
    int err = cckem_export_pubkey(cckem_public_ctx(ctx), &pubSize, data.mutableBytes);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export public part of %@"), key);
        return NULL;
    }

    size_t privSize = data.length - pubSize;
    err = cckem_export_privkey(ctx, &privSize, data.mutableBytes + pubSize);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export %@"), key);
        return NULL;
    }

    data.length = pubSize + privSize;
    return CFBridgingRetain(data);
}

NSString *SecKEMGenerateHexDump(cckem_pub_ctx_t ctx) {
    size_t len = cckem_pubkey_nbytes_ctx(ctx);
    NSMutableString *dump = [NSMutableString stringWithCapacity:len * 2];

    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(kCFAllocatorDefault, cckem_pubkey_nbytes_ctx(ctx)));

    size_t size = data.length;
    int err = cckem_export_pubkey(ctx, &size, data.mutableBytes);
    if (err != CCERR_OK) {
        secwarning("Failed to export public key: error code %d.", err);
        return @"";
    }

    data.length = size;
    for (size_t byteIndex = 0; byteIndex < len; ++byteIndex) {
        [dump appendFormat:@"%02X", ((const uint8_t *)data.bytes)[byteIndex]];
    }

    return dump;
}

NSDictionary *SecKEMCreateKeyAttributeDictionary(id keyType,
                                                 id keySizeType,
                                                 id keyClass,
                                                 NSData *applicationLabel,
                                                 NSData *valueData) {
    return @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: keyType,
        (id)kSecAttrKeySizeInBits: keySizeType,
        (id)kSecAttrEffectiveKeySize: keySizeType,
        (id)kSecAttrKeyClass: keyClass,
        (id)kSecAttrApplicationLabel: applicationLabel ?: NSData.data,
        (id)kSecAttrIsPermanent: @YES,
        (id)kSecAttrIsPrivate: @YES,
        (id)kSecAttrIsModifiable: @YES,
        (id)kSecAttrIsSensitive: @NO,
        (id)kSecAttrWasAlwaysSensitive: @NO,
        (id)kSecAttrIsExtractable: @YES,
        (id)kSecAttrWasNeverExtractable: @NO,
        (id)kSecAttrCanEncrypt: @NO,
        (id)kSecAttrCanDecrypt: @NO,
        (id)kSecAttrCanDerive: @NO,
        (id)kSecAttrCanSign: @NO,
        (id)kSecAttrCanVerify: @NO,
        (id)kSecAttrCanSignRecover: @NO,
        (id)kSecAttrCanVerifyRecover: @NO,
        (id)kSecAttrCanWrap: @NO,
        (id)kSecAttrCanUnwrap: @NO,
        (id)kSecValueData: valueData ?: NSData.data,
    };
}

NSData * SecKEMDecapsulateSharedKey(SecKeyRef key,
                                    CFDataRef encapsulatedKey,
                                    CFErrorRef *error) {
    cckem_full_ctx_t ctx = key->key;
    if (ctx == NULL) {
        SecError(errSecParam, error, CFSTR("Invalid key data: The provided key does not contain a valid KEM context."));
        return NULL;
    }

    NSData *ek = (__bridge NSData *)encapsulatedKey;

    if (![ek isKindOfClass:NSData.class] || ek.length != cckem_encapsulated_key_nbytes_info(ctx->info)) {
        SecError(errSecParam,
                 error,
                 CFSTR("KEM decapsulation failed: expecting input data of size %dbytes"),
                 (int)cckem_encapsulated_key_nbytes_info(ctx->info));
        return NULL;
    }
    
    NSMutableData *sharedKey = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(),
                                                                                cckem_shared_key_nbytes_info(ctx->info)));
    int err = cckem_decapsulate(ctx,
                                ek.length,
                                ek.bytes,
                                sharedKey.length,
                                sharedKey.mutableBytes);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("KEM Key decapsulation failed, err=%d for key %@"), err, key);
    }

    return sharedKey;
}
