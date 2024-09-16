/*
 * Copyright (c) 2024 Apple Inc. All Rights Reserved.
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
 * SecKyberKey.m - CoreFoundation based Kyber key object
 */

#include "SecKyberKey.h"

#import <Foundation/Foundation.h>

#include <Security/SecBasePriv.h>
#include <Security/SecFramework.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyInternal.h>
#include <Security/SecCFAllocator.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <AssertMacros.h>

#include <corecrypto/cckem.h>
#include <corecrypto/cckyber.h>

static CFIndex SecKyberKeyGetAlgorithmID(SecKeyRef key) {
    return kSecKyberAlgorithmID;
}

static void SecKyberPublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    cckem_pub_ctx_t ctx = key->key;
    if (ctx != NULL) {
        cckem_pub_ctx_clear(ctx->info, ctx);
        free(ctx);
    }
}

static OSStatus SecKyberPublicKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    if (keyDataLength == 0) {
        key->key = (void *)keyData;
        return errSecSuccess;
    }

    const struct cckem_info *info;
    if (keyDataLength == (CFIndex)cckem_pubkey_nbytes_info(cckem_kyber768())) {
        info = cckem_kyber768();
    } else if (keyDataLength == (CFIndex)cckem_pubkey_nbytes_info(cckem_kyber1024())) {
        info = cckem_kyber1024();
    } else {
        secwarning("Kyber pubkey size=%dbytes is invalid", (int)keyDataLength);
        return errSecParam;
    }

    key->key = calloc(1, cckem_sizeof_pub_ctx(info));
    cckem_pub_ctx_t ctx = key->key;
    int err = cckem_import_pubkey(info, keyDataLength, keyData, ctx);
    if (err != 0) {
        secwarning("Kyber pubkey size=%dbytes import failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    return errSecSuccess;
}

static CFDataRef SecKyberPublicKeyCopyData(cckem_pub_ctx_t ctx, CFErrorRef *error) {
    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), cckem_pubkey_nbytes_ctx(ctx)));
    size_t size = data.length;
    int err = cckem_export_pubkey(ctx, &size, data.mutableBytes);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export Kyber pubkey"));
        return NULL;
    }

    data.length = size;
    return CFBridgingRetain(data);
}

static CFStringRef SecKyberPublicKeyCopyKeyDescription(SecKeyRef key) {
    cckem_pub_ctx_t ctx = key->key;
    size_t len = cckem_pubkey_nbytes_ctx(ctx);
    NSMutableString *dump = [NSMutableString stringWithCapacity:len * 2];
    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), cckem_pubkey_nbytes_ctx(ctx)));
    size_t size = data.length;
    int err = cckem_export_pubkey(ctx, &size, data.mutableBytes);
    if (err == CCERR_OK) {
        data.length = size;
        for (size_t byteIndex = 0; byteIndex < len; ++byteIndex) {
            [dump appendFormat:@"%02X", ((const uint8_t *)data.bytes)[byteIndex]];
        }
    }

    NSString *name;
    if (cckem_pubkey_nbytes_ctx(ctx) == cckem_pubkey_nbytes_info(cckem_kyber768())) {
        name = @"Kyber-768-pubKey";
    } else if (cckem_pubkey_nbytes_ctx(ctx) == cckem_pubkey_nbytes_info(cckem_kyber1024())) {
        name = @"Kyber-1024-pubKey";
    } else {
        name = @"Kyber";
    }

    NSString *description = [NSString stringWithFormat:@"<SecKeyRef %@ algorithm id: %lu, key type: %s, version: %d, bytes: %@, addr: %p>",
                             name, (long)SecKyberKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version,
                             dump, (const void *)key];
    return CFBridgingRetain(description);
}

static size_t SecKyberPublicKeyBlockSize(SecKeyRef key) {
    cckem_pub_ctx_t ctx = key->key;
    return cckem_pubkey_nbytes_ctx(ctx);
}

static CFDataRef SecKyberPublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    return SecKyberPublicKeyCopyData(key->key, error);
}

static OSStatus SecKyberPublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    *serialization = SecKyberPublicKeyCopyData(key->key, NULL);
    return *serialization != NULL ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecKyberPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    cckem_pub_ctx_t ctx = key->key;
    id sizeType;
    if (cckem_pubkey_nbytes_ctx((void *)ctx) == cckem_pubkey_nbytes_info(cckem_kyber768())) {
        sizeType = (id)kSecAttrKeySizeKyber768;
    } else if (cckem_pubkey_nbytes_ctx((void *)ctx) == cckem_pubkey_nbytes_info(cckem_kyber1024())) {
        sizeType = (id)kSecAttrKeySizeKyber1024;
    } else {
        secerror("unknown Kyber type detected");
        sizeType = @0;
    }

    NSData *pubKeyBlob = CFBridgingRelease(SecKyberPublicKeyCopyData(ctx, NULL));
    NSData *pubKeyDigest = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, pubKeyBlob.bytes, pubKeyBlob.length));

    return CFBridgingRetain(@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeySizeInBits: sizeType,
        (id)kSecAttrEffectiveKeySize: sizeType,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
        (id)kSecAttrApplicationLabel: pubKeyDigest,
        (id)kSecAttrIsPermanent: [NSNumber numberWithBool:YES],
        (id)kSecAttrIsPrivate: [NSNumber numberWithBool:YES],
        (id)kSecAttrIsModifiable: [NSNumber numberWithBool:YES],
        (id)kSecAttrIsSensitive: [NSNumber numberWithBool:NO],
        (id)kSecAttrWasAlwaysSensitive: [NSNumber numberWithBool:NO],
        (id)kSecAttrIsExtractable: [NSNumber numberWithBool:YES],
        (id)kSecAttrWasNeverExtractable: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanEncrypt: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanDecrypt: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanDerive: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanSign: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanVerify: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanSignRecover: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanVerifyRecover: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanWrap: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanUnwrap: [NSNumber numberWithBool:NO],
        (id)kSecValueData: pubKeyBlob,
    });
}

static CFTypeRef SecKyberPublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                      CFArrayRef algorithms, SecKeyOperationMode mode,
                                                      CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (operation != kSecKeyOperationTypeEncapsulate || !CFEqual(algorithm, kSecKeyAlgorithmKEMKyber)) {
        // Kyber public key supports only key encapsulation.
        return kCFNull;
    }

    if (mode == kSecKeyOperationModePerform) {
        cckem_pub_ctx_t ctx = key->key;
        NSMutableData *sk = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), cckem_shared_key_nbytes_ctx(ctx)));
        NSMutableData *ek = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(),  cckem_encapsulated_key_nbytes_ctx(ctx)));
        int err = cckem_encapsulate(ctx, ek.length, ek.mutableBytes, sk.length, sk.mutableBytes, ccrng_seckey());
        if (err != CCERR_OK) {
            SecError(errSecDecode, error, CFSTR("Key encapsulation failed, err=%d for key %@"), err, key);
        }
        return CFBridgingRetain(@[ek, sk]);
    } else {
        // Algorithm is supported.
        return kCFBooleanTrue;
    }
}

static SecKeyDescriptor kSecKyberPublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "KyberPublicKey",
    .init = SecKyberPublicKeyInit,
    .destroy = SecKyberPublicKeyDestroy,
    .blockSize = SecKyberPublicKeyBlockSize,
    .copyDictionary = SecKyberPublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecKyberPublicKeyCopyExternalRepresentation,
    .describe = SecKyberPublicKeyCopyKeyDescription,
    .getAlgorithmID = SecKyberKeyGetAlgorithmID,
    .copyPublic = SecKyberPublicKeyCopyPublicOctets,
    .copyOperationResult = SecKyberPublicKeyCopyOperationResult,
};

SecKeyRef SecKeyCreateKyberPublicKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength) {
    return SecKeyCreate(allocator, &kSecKyberPublicKeyDescriptor, keyData, keyDataLength, 0);
}


static void SecKyberPrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    cckem_full_ctx_t ctx = key->key;
    if (ctx != NULL) {
        cckem_full_ctx_clear(ctx->info, ctx);
        free(ctx);
    }
}

static OSStatus SecKyberPrivateKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    if (keyDataLength == 0) {
        key->key = (void *)keyData;
        return errSecSuccess;
    }

    const struct cckem_info *info;
    if ((size_t)keyDataLength == cckem_pubkey_nbytes_info(cckem_kyber768()) + cckem_privkey_nbytes_info(cckem_kyber768())) {
        info = cckem_kyber768();
    } else if ((size_t)keyDataLength == cckem_pubkey_nbytes_info(cckem_kyber1024()) + cckem_privkey_nbytes_info(cckem_kyber1024())) {
        info = cckem_kyber1024();
    } else {
        secwarning("Kyber pubkey size=%dbytes is invalid", (int)keyDataLength);
        return errSecParam;
    }

    key->key = calloc(1, cckem_sizeof_full_ctx(info));
    cckem_full_ctx_t ctx = key->key;
    size_t pubKeySize = cckem_pubkey_nbytes_info(info);

    int err = cckem_import_privkey(info, keyDataLength - pubKeySize, keyData + pubKeySize, ctx);
    if (err != 0) {
        secwarning("Kyber privkey size=%dbytes import or priv part failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    err = cckem_import_pubkey(info, pubKeySize, keyData, cckem_public_ctx(ctx));
    if (err != CCERR_OK) {
        secwarning("Kyber privkey size=%dbytes import of pub part failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    return errSecSuccess;
}

static CFStringRef SecKyberPrivateKeyCopyKeyDescription(SecKeyRef key) {
    cckem_full_ctx_t ctx = key->key;

    NSString *name;
    if (cckem_privkey_nbytes_ctx((void *)ctx) == cckem_privkey_nbytes_info(cckem_kyber768())) {
        name = @"Kyber-768-privKey";
    } else if (cckem_privkey_nbytes_ctx((void *)ctx) == cckem_privkey_nbytes_info(cckem_kyber1024())) {
        name = @"Kyber-1024-privKey";
    } else {
        name = @"Kyber";
    }

    return CFBridgingRetain([NSString stringWithFormat:@"<SecKeyRef %@ algorithm id: %lu, key type: %s, version: %d, addr: %p>",
                             name, (long)SecKyberKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, key]);
}

static size_t SecKyberPrivateKeyBlockSize(SecKeyRef key) {
    cckem_full_ctx_t ctx = key->key;
    return cckem_privkey_nbytes_ctx((void *)ctx);
}

static CFDataRef SecKyberPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    cckem_full_ctx_t ctx = key->key;
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

static OSStatus SecKyberPrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    cckem_pub_ctx_t ctx = cckem_public_ctx(key->key);
    *serialization = SecKyberPublicKeyCopyData(ctx, NULL);
    return *serialization != NULL ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecKyberPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
    cckem_full_ctx_t ctx = key->key;
    id sizeType;
    if (cckem_privkey_nbytes_ctx((void *)ctx) == cckem_privkey_nbytes_info(cckem_kyber768())) {
        sizeType = (id)kSecAttrKeySizeKyber768;
    } else if (cckem_privkey_nbytes_ctx((void *)ctx) == cckem_privkey_nbytes_info(cckem_kyber1024())) {
        sizeType = (id)kSecAttrKeySizeKyber1024;
    } else {
        secerror("unknown Kyber type detected");
        sizeType = @0;
    }

    NSData *pubKeyDigest;
    CFDataRef pubCFKeyBlob;
    if (SecKyberPrivateKeyCopyPublicOctets(key, &pubCFKeyBlob) == errSecSuccess) {
        NSData *pubKeyBlob = CFBridgingRelease(pubCFKeyBlob);
        pubKeyDigest = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, pubKeyBlob.bytes, pubKeyBlob.length));
    }

    return CFBridgingRetain(@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeKyber,
        (id)kSecAttrKeySizeInBits: sizeType,
        (id)kSecAttrEffectiveKeySize: sizeType,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrApplicationLabel: pubKeyDigest ?: NSData.data,
        (id)kSecAttrIsPermanent: [NSNumber numberWithBool:YES],
        (id)kSecAttrIsPrivate: [NSNumber numberWithBool:YES],
        (id)kSecAttrIsModifiable: [NSNumber numberWithBool:YES],
        (id)kSecAttrIsSensitive: [NSNumber numberWithBool:NO],
        (id)kSecAttrWasAlwaysSensitive: [NSNumber numberWithBool:NO],
        (id)kSecAttrIsExtractable: [NSNumber numberWithBool:YES],
        (id)kSecAttrWasNeverExtractable: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanEncrypt: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanDecrypt: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanDerive: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanSign: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanVerify: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanSignRecover: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanVerifyRecover: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanWrap: [NSNumber numberWithBool:NO],
        (id)kSecAttrCanUnwrap: [NSNumber numberWithBool:NO],
        (id)kSecValueData: CFBridgingRelease(SecKyberPrivateKeyCopyExternalRepresentation(key, NULL)) ?: NSMutableData.data,
    });
}

static CFTypeRef SecKyberPrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                       CFArrayRef algorithms, SecKeyOperationMode mode,
                                                       CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (operation != kSecKeyOperationTypeDecapsulate || !CFEqual(algorithm, kSecKeyAlgorithmKEMKyber)) {
        // Kyber private key supports only key decapsulation.
        return kCFNull;
    }

    if (mode == kSecKeyOperationModePerform) {
        cckem_full_ctx_t ctx = key->key;
        NSData *ek = (__bridge NSData *)in1;
        if (![ek isKindOfClass:NSData.class] || ek.length != cckem_encapsulated_key_nbytes_info(ctx->info)) {
            SecError(errSecParam, error, CFSTR("Kyber decapsulation failed: expecting input data of size %dbytes"), (int)cckem_encapsulated_key_nbytes_info(ctx->info));
            return NULL;
        }

        NSMutableData *sk = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), cckem_shared_key_nbytes_info(ctx->info)));
        int err = cckem_decapsulate(ctx, ek.length, ek.bytes, sk.length, sk.mutableBytes);
        if (err != CCERR_OK) {
            SecError(errSecDecode, error, CFSTR("Key decapsulation failed, err=%d for key %@"), err, key);
        }
        return CFBridgingRetain(sk);
    } else {
        // Algorithm is supported.
        return kCFBooleanTrue;
    }
}

static SecKeyDescriptor kSecKyberPrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "KyberPrivateKey",
    .init = SecKyberPrivateKeyInit,
    .destroy = SecKyberPrivateKeyDestroy,
    .blockSize = SecKyberPrivateKeyBlockSize,
    .copyDictionary = SecKyberPrivateKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecKyberPrivateKeyCopyExternalRepresentation,
    .describe = SecKyberPrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecKyberKeyGetAlgorithmID,
    .copyPublic = SecKyberPrivateKeyCopyPublicOctets,
    .copyOperationResult = SecKyberPrivateKeyCopyOperationResult,
};

SecKeyRef SecKeyCreateKyberPrivateKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength) {
    return SecKeyCreate(allocator, &kSecKyberPrivateKeyDescriptor, keyData, keyDataLength, 0);
}

OSStatus SecKyberKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    const struct cckem_info *info = cckem_kyber768();
    id kyberType = ((__bridge NSDictionary *)parameters)[(id)kSecAttrKeySizeInBits];
    if (kyberType != nil) {
        if ([kyberType integerValue] == [(id)kSecAttrKeySizeKyber768 integerValue]) {
            info = cckem_kyber768();
        } else if ([kyberType integerValue] == [(id)kSecAttrKeySizeKyber1024 integerValue]) {
            info = cckem_kyber1024();
        } else {
            secwarning("Invalid kyber type %@ requested for Kyber key generation", kyberType);
            return errSecParam;
        }
    }

    cckem_full_ctx_t fullctx = calloc(1, cckem_sizeof_full_ctx(info));
    cckem_full_ctx_init(fullctx, info);
    int err = cckem_generate_key(fullctx, ccrng_seckey());
    if (err != CCERR_OK) {
        free(fullctx);
        secwarning("Failed to generate Kyber key: err %d", (int)err);
        return errSecDecode;
    }

    id privKey = CFBridgingRelease(SecKeyCreate(SecCFAllocatorZeroize(), &kSecKyberPrivateKeyDescriptor, (const void *)fullctx, 0, 0));
    if (privKey == nil) {
        return errSecParam;
    }

    cckem_pub_ctx_t pubctx = calloc(1, cckem_sizeof_pub_ctx(info));
    memcpy(pubctx, cckem_public_ctx(fullctx), cckem_sizeof_pub_ctx(info));
    id pubKey = CFBridgingRelease(SecKeyCreate(SecCFAllocatorZeroize(), &kSecKyberPublicKeyDescriptor, (const void *)pubctx, 0, 0));
    if (pubKey == nil) {
        return errSecParam;
    }

    if (publicKey != NULL) {
        *publicKey = (SecKeyRef)CFBridgingRetain(pubKey);
    }
    if (privateKey != NULL) {
        *privateKey = (SecKeyRef)CFBridgingRetain(privKey);
    }

    return errSecSuccess;
}
