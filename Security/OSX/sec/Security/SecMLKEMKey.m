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
 * SecMLKEMKey.m - CoreFoundation based ML-KEM key object
 */

#include "SecMLKEMKey.h"
#include "SecKEMKeyPriv.h"

#import <Foundation/Foundation.h>

#include <Security/SecBasePriv.h>
#include <Security/SecFramework.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecKeyInternal.h>
#include <Security/SecCFAllocator.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <AssertMacros.h>

#include <corecrypto/cckem.h>
#include <corecrypto/ccmlkem.h>

static CFIndex SecMLKEMKeyGetAlgorithmID(SecKeyRef key) {
    return kSecMLKEMAlgorithmID;
}

static void SecMLKEMPublicKeyDestroy(SecKeyRef key) {
    return SecKEMPublicKeyDestroy(key);
}

static OSStatus SecMLKEMPublicKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    if (keyDataLength == 0) {
        if (keyData != NULL) {
            key->key = (void *)keyData;
            return errSecSuccess;
        } else {
            return errSecParam;
        }
    }

    const struct cckem_info *info;
    if (keyDataLength == (CFIndex)cckem_pubkey_nbytes_info(cckem_mlkem768())) {
        info = cckem_mlkem768();
    } else if (keyDataLength == (CFIndex)cckem_pubkey_nbytes_info(cckem_mlkem1024())) {
        info = cckem_mlkem1024();
    } else {
        secwarning("ML-KEM pubkey size=%dbytes is invalid", (int)keyDataLength);
        return errSecParam;
    }

    key->key = calloc(1, cckem_sizeof_pub_ctx(info));
    cckem_pub_ctx_t ctx = key->key;
    int err = cckem_import_pubkey(info, keyDataLength, keyData, ctx);
    if (err != 0) {
        secwarning("ML-KEM pubkey size=%dbytes import failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    return errSecSuccess;
}

static CFDataRef SecMLKEMPublicKeyCopyData(cckem_pub_ctx_t ctx, CFErrorRef *error) {
    return SecKEMPublicKeyCopyData(ctx, error);
}

static CFStringRef SecMLKEMPublicKeyCopyKeyDescription(SecKeyRef key) {
    cckem_pub_ctx_t ctx = key->key;
    if (ctx == NULL) {
        secerror("Invalid key data: The provided key does not contain a valid KEM context.");
        return CFBridgingRetain(@"");
    }

    NSString *dump = SecKEMGenerateHexDump(ctx);

    NSString *name;
    if (cckem_pubkey_nbytes_ctx(ctx) == cckem_pubkey_nbytes_info(cckem_mlkem768())) {
        name = @"ML-KEM-768-pubKey";
    } else if (cckem_pubkey_nbytes_ctx(ctx) == cckem_pubkey_nbytes_info(cckem_mlkem1024())) {
        name = @"ML-KEM-1024-pubKey";
    } else {
        name = @"ML-KEM";
    }

    NSString *description = [NSString stringWithFormat:@"<SecKeyRef %@ algorithm id: %lu, key type: %s, version: %d, bytes: %@, addr: %p>",
                             name, (long)SecMLKEMKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version,
                             dump, (const void *)key];
    return CFBridgingRetain(description);
}

static size_t SecMLKEMPublicKeyBlockSize(SecKeyRef key) {
    return SecKEMPublicKeyBlockSize(key);
}

static CFDataRef SecMLKEMPublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    return SecMLKEMPublicKeyCopyData(key->key, error);
}

static OSStatus SecMLKEMPublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    *serialization = SecMLKEMPublicKeyCopyData(key->key, NULL);
    return *serialization != NULL ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecMLKEMPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    cckem_pub_ctx_t ctx = key->key;
    id sizeType;
    if (cckem_pubkey_nbytes_ctx(ctx) == cckem_pubkey_nbytes_info(cckem_mlkem768())) {
        sizeType = (id)kSecAttrKeySizeMLKEM768;
    } else if (cckem_pubkey_nbytes_ctx(ctx) == cckem_pubkey_nbytes_info(cckem_mlkem1024())) {
        sizeType = (id)kSecAttrKeySizeMLKEM1024;
    } else {
        secerror("unknown ML-KEM type detected");
        sizeType = @0;
    }

    NSData *pubKeyBlob = CFBridgingRelease(SecMLKEMPublicKeyCopyData(ctx, NULL));
    NSData *pubKeyDigest = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, pubKeyBlob.bytes, pubKeyBlob.length));

    return CFBridgingRetain(SecKEMCreateKeyAttributeDictionary((id)kSecAttrKeyTypeMLKEM,
                                                               sizeType,
                                                               (id)kSecAttrKeyClassPublic,
                                                               pubKeyDigest,
                                                               pubKeyBlob));
}

static CFTypeRef SecMLKEMPublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                      CFArrayRef algorithms, SecKeyOperationMode mode,
                                                      CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (operation != kSecKeyOperationTypeEncapsulate || !CFEqual(algorithm, kSecKeyAlgorithmKEMMLKEM)) {
        // ML-KEM public key supports only key encapsulation.
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

static SecKeyDescriptor kSecMLKEMPublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "MLKEMPublicKey",
    .init = SecMLKEMPublicKeyInit,
    .destroy = SecMLKEMPublicKeyDestroy,
    .blockSize = SecMLKEMPublicKeyBlockSize,
    .copyDictionary = SecMLKEMPublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecMLKEMPublicKeyCopyExternalRepresentation,
    .describe = SecMLKEMPublicKeyCopyKeyDescription,
    .getAlgorithmID = SecMLKEMKeyGetAlgorithmID,
    .copyPublic = SecMLKEMPublicKeyCopyPublicOctets,
    .copyOperationResult = SecMLKEMPublicKeyCopyOperationResult,
};

SecKeyRef SecKeyCreateMLKEMPublicKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength) {
    return SecKeyCreate(allocator, &kSecMLKEMPublicKeyDescriptor, keyData, keyDataLength, 0);
}

static void SecMLKEMPrivateKeyDestroy(SecKeyRef key) {
    return SecKEMPrivateKeyDestroy(key);
}

static OSStatus SecMLKEMPrivateKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    if (keyDataLength == 0) {
        if (keyData != NULL) {
            key->key = (void *)keyData;
            return errSecSuccess;
        } else {
            return errSecParam;
        }
    }

    const struct cckem_info *info;
    if ((size_t)keyDataLength == cckem_pubkey_nbytes_info(cckem_mlkem768()) + cckem_privkey_nbytes_info(cckem_mlkem768())) {
        info = cckem_mlkem768();
    } else if ((size_t)keyDataLength == cckem_pubkey_nbytes_info(cckem_mlkem1024()) + cckem_privkey_nbytes_info(cckem_mlkem1024())) {
        info = cckem_mlkem1024();
    } else {
        secwarning("ML-KEM pubkey size=%dbytes is invalid", (int)keyDataLength);
        return errSecParam;
    }

    key->key = calloc(1, cckem_sizeof_full_ctx(info));
    cckem_full_ctx_t ctx = key->key;
    size_t pubKeySize = cckem_pubkey_nbytes_info(info);

    int err = cckem_import_privkey(info, keyDataLength - pubKeySize, keyData + pubKeySize, ctx);
    if (err != 0) {
        secwarning("ML-KEM privkey size=%dbytes import of priv part failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    err = cckem_import_pubkey(info, pubKeySize, keyData, cckem_public_ctx(ctx));
    if (err != CCERR_OK) {
        secwarning("ML-KEM privkey size=%dbytes import of pub part failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    return errSecSuccess;
}

static CFStringRef SecMLKEMPrivateKeyCopyKeyDescription(SecKeyRef key) {
    cckem_full_ctx_t ctx = key->key;

    NSString *name;
    if (cckem_privkey_nbytes_ctx(cckem_public_ctx(ctx)) == cckem_privkey_nbytes_info(cckem_mlkem768())) {
        name = @"ML-KEM-768-privKey";
    } else if (cckem_privkey_nbytes_ctx(cckem_public_ctx(ctx)) == cckem_privkey_nbytes_info(cckem_mlkem1024())) {
        name = @"ML-KEM-1024-privKey";
    } else {
        name = @"ML-KEM";
    }

    return CFBridgingRetain([NSString stringWithFormat:@"<SecKeyRef %@ algorithm id: %lu, key type: %s, version: %d, addr: %p>",
                             name, (long)SecMLKEMKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, key]);
}

static size_t SecMLKEMPrivateKeyBlockSize(SecKeyRef key) {
    return SecKEMPrivateKeyBlockSize(key);
}

static CFDataRef SecMLKEMPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    return SecKEMPrivateKeyCopyExternalRepresentation(key, error);
}

static OSStatus SecMLKEMPrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    cckem_pub_ctx_t ctx = cckem_public_ctx(key->key);
    *serialization = SecMLKEMPublicKeyCopyData(ctx, NULL);
    return *serialization != NULL ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecMLKEMPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
    cckem_full_ctx_t ctx = key->key;
    id sizeType;
    if (cckem_privkey_nbytes_ctx(cckem_public_ctx(ctx)) == cckem_privkey_nbytes_info(cckem_mlkem768())) {
        sizeType = (id)kSecAttrKeySizeMLKEM768;
    } else if (cckem_privkey_nbytes_ctx(cckem_public_ctx(ctx)) == cckem_privkey_nbytes_info(cckem_mlkem1024())) {
        sizeType = (id)kSecAttrKeySizeMLKEM1024;
    } else {
        secerror("unknown ML-KEM type detected");
        sizeType = @0;
    }

    NSData *pubKeyDigest;
    CFDataRef pubCFKeyBlob;
    if (SecMLKEMPrivateKeyCopyPublicOctets(key, &pubCFKeyBlob) == errSecSuccess) {
        NSData *pubKeyBlob = CFBridgingRelease(pubCFKeyBlob);
        pubKeyDigest = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, pubKeyBlob.bytes, pubKeyBlob.length));
    }

    return CFBridgingRetain(SecKEMCreateKeyAttributeDictionary((id)kSecAttrKeyTypeMLKEM,
                                                               sizeType,
                                                               (id)kSecAttrKeyClassPrivate,
                                                               pubKeyDigest ?: NSData.data,
                                                               CFBridgingRelease(SecMLKEMPrivateKeyCopyExternalRepresentation(key, NULL)) ?: NSData.data));
}

static CFTypeRef SecMLKEMPrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                       CFArrayRef algorithms, SecKeyOperationMode mode,
                                                       CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (operation != kSecKeyOperationTypeDecapsulate || !CFEqual(algorithm, kSecKeyAlgorithmKEMMLKEM)) {
        // ML-KEM private key supports only key decapsulation.
        return kCFNull;
    }

    switch (mode) {
        case kSecKeyOperationModePerform: {
            return CFBridgingRetain(SecKEMDecapsulateSharedKey(key, in1, error));
        }
        case kSecKeyOperationModeCheckIfSupported:
            return kCFBooleanTrue;
        default:
            return kCFNull;
    }
}

static SecKeyDescriptor kSecMLKEMPrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "MLKEMPrivateKey",
    .init = SecMLKEMPrivateKeyInit,
    .destroy = SecMLKEMPrivateKeyDestroy,
    .blockSize = SecMLKEMPrivateKeyBlockSize,
    .copyDictionary = SecMLKEMPrivateKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecMLKEMPrivateKeyCopyExternalRepresentation,
    .describe = SecMLKEMPrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecMLKEMKeyGetAlgorithmID,
    .copyPublic = SecMLKEMPrivateKeyCopyPublicOctets,
    .copyOperationResult = SecMLKEMPrivateKeyCopyOperationResult,
};

SecKeyRef SecKeyCreateMLKEMPrivateKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength) {
    return SecKeyCreate(allocator, &kSecMLKEMPrivateKeyDescriptor, keyData, keyDataLength, 0);
}

OSStatus SecMLKEMKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    const struct cckem_info *info = cckem_mlkem768();
    id mlkemType = ((__bridge NSDictionary *)parameters)[(id)kSecAttrKeySizeInBits];
    if (mlkemType != nil) {
        if ([mlkemType integerValue] == [(id)kSecAttrKeySizeMLKEM768 integerValue]) {
            info = cckem_mlkem768();
        } else if ([mlkemType integerValue] == [(id)kSecAttrKeySizeMLKEM1024 integerValue]) {
            info = cckem_mlkem1024();
        } else {
            secwarning("Invalid ML-KEM type %@ requested for ML-KEM key generation", mlkemType);
            return errSecParam;
        }
    }

    cckem_full_ctx_t fullctx = calloc(1, cckem_sizeof_full_ctx(info));
    cckem_full_ctx_init(fullctx, info);
    int err = cckem_generate_key(fullctx, ccrng_seckey());
    if (err != CCERR_OK) {
        free(fullctx);
        secwarning("Failed to generate ML-KEM key: err %d", (int)err);
        return errSecDecode;
    }

    id privKey = CFBridgingRelease(SecKeyCreate(SecCFAllocatorZeroize(), &kSecMLKEMPrivateKeyDescriptor, (const void *)fullctx, 0, 0));
    if (privKey == nil) {
        return errSecParam;
    }

    cckem_pub_ctx_t pubctx = calloc(1, cckem_sizeof_pub_ctx(info));
    memcpy(pubctx, cckem_public_ctx(fullctx), cckem_sizeof_pub_ctx(info));
    id pubKey = CFBridgingRelease(SecKeyCreate(kCFAllocatorDefault, &kSecMLKEMPublicKeyDescriptor, (const void *)pubctx, 0, 0));
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
