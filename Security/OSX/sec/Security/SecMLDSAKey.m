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

/*
 * SecMLDSAKey.m - CoreFoundation based ML-DSA key object
 */

#include "SecMLDSAKey.h"

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

#include <corecrypto/ccmldsa.h>

static CFIndex SecMLDSAKeyGetAlgorithmID(SecKeyRef key) {
    return kSecMLDSAAlgorithmID;
}

static void SecMLDSAKeyPublicKeyDestroy(SecKeyRef key) {
    // Zero out the public key
    ccmldsa_pub_ctx_t ctx = key->key;
    if (ctx != NULL) {
        ccmldsa_pub_ctx_clear(ctx->params, ctx);
        free(ctx);
    }
}

static OSStatus SecMLDSAPublicKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    if (keyDataLength == 0) {
        if (keyData != NULL) {
            key->key = (void *)keyData;
            return errSecSuccess;
        } else {
            return errSecParam;
        }
    }

    const struct ccmldsa_params *params;
    if (keyDataLength == (CFIndex)ccmldsa_pubkey_nbytes_params(ccmldsa65())) {
        params = ccmldsa65();
    } else if (keyDataLength == (CFIndex)ccmldsa_pubkey_nbytes_params(ccmldsa87())) {
        params = ccmldsa87();
    } else {
        secwarning("ML-DSA pubkey size=%dbytes is invalid", (int)keyDataLength);
        return errSecParam;
    }

    key->key = calloc(1, ccmldsa_sizeof_pub_ctx(params));
    ccmldsa_pub_ctx_t ctx = key->key;
    int err = ccmldsa_import_pubkey(params, keyDataLength, keyData, ctx);
    if (err != 0) {
        secwarning("ML-DSA pubkey size=%dbytes import failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    return errSecSuccess;
}

static CFDataRef SecMLDSAPublicKeyCopyData(ccmldsa_pub_ctx_t ctx, CFErrorRef *error) {
    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(kCFAllocatorDefault, ccmldsa_pubkey_nbytes_ctx(ctx)));
    size_t size = data.length;
    int err = ccmldsa_export_pubkey(ctx, size, data.mutableBytes);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export ML-DSA pubkey"));
        return NULL;
    }
    return CFBridgingRetain(data);
}

static CFStringRef SecMLDSAPublicKeyCopyKeyDescription(SecKeyRef key) {
    ccmldsa_pub_ctx_t ctx = key->key;
    size_t len = ccmldsa_pubkey_nbytes_ctx(ctx);
    NSMutableString *dump = [NSMutableString stringWithCapacity:len * 2];
    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(kCFAllocatorDefault, ccmldsa_pubkey_nbytes_ctx(ctx)));
    size_t size = data.length;
    int err = ccmldsa_export_pubkey(ctx, size, data.mutableBytes);
    if (err == CCERR_OK) {
        for (size_t byteIndex = 0; byteIndex < len; ++byteIndex) {
            [dump appendFormat:@"%02X", ((const uint8_t *)data.bytes)[byteIndex]];
        }
    }

    NSString *name;
    if (ccmldsa_pubkey_nbytes_ctx(ctx) == ccmldsa_pubkey_nbytes_params(ccmldsa65())) {
        name = @"ML-DSA-65-pubKey";
    } else if (ccmldsa_pubkey_nbytes_ctx(ctx) == ccmldsa_pubkey_nbytes_params(ccmldsa87())) {
        name = @"ML-DSA-87-pubKey";
    } else {
        name = @"ML-DSA";
    }

    NSString *description = [NSString stringWithFormat:@"<SecKeyRef %@ algorithm id: %lu, key type: %s, version: %d, bytes: %@, addr: %p>",
                             name, (long)SecMLDSAKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version,
                             dump, (const void *)key];
    return CFBridgingRetain(description);
}

static size_t SecMLDSAPublicKeyBlockSize(SecKeyRef key) {
    ccmldsa_pub_ctx_t ctx = key->key;
    return ccmldsa_pubkey_nbytes_ctx(ctx);
}

static CFDataRef SecMLDSAPublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    return SecMLDSAPublicKeyCopyData(key->key, error);
}

static OSStatus SecMLDSAPublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    *serialization = SecMLDSAPublicKeyCopyData(key->key, NULL);
    return *serialization != NULL ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecMLDSAPublicKeyCopyAttributeDictionary(SecKeyRef key) {
    ccmldsa_pub_ctx_t ctx = key->key;
    id sizeType;
    if (ccmldsa_pubkey_nbytes_ctx((void *)ctx) == ccmldsa_pubkey_nbytes_params(ccmldsa65())) {
        sizeType = (id)kSecAttrKeySizeMLDSA65;
    } else if (ccmldsa_pubkey_nbytes_ctx((void *)ctx) == ccmldsa_pubkey_nbytes_params(ccmldsa87())) {
        sizeType = (id)kSecAttrKeySizeMLDSA87;
    } else {
        secerror("unknown ML-DSA type detected");
        sizeType = @0;
    }

    NSData *pubKeyBlob = CFBridgingRelease(SecMLDSAPublicKeyCopyData(ctx, NULL));
    NSData *pubKeyDigest = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, pubKeyBlob.bytes, pubKeyBlob.length));

    return CFBridgingRetain(@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeySizeInBits: sizeType,
        (id)kSecAttrEffectiveKeySize: sizeType,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPublic,
        (id)kSecAttrApplicationLabel: pubKeyDigest,
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
        (id)kSecAttrCanVerify: @YES,
        (id)kSecAttrCanSignRecover: @NO,
        (id)kSecAttrCanVerifyRecover: @NO,
        (id)kSecAttrCanWrap: @NO,
        (id)kSecAttrCanUnwrap: @NO,
        (id)kSecValueData: pubKeyBlob,
    });
}

static CFTypeRef SecMLDSAPublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                      CFArrayRef algorithms, SecKeyOperationMode mode,
                                                      CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    // Even though SEP keys are prehashed inside CryptoTokenKit,
    // the signature verification can still happen without prehashing (eg. we don't do any SecKeyAdaptors passes).
    // Reason being that `ccmldsa_verify` expects original message (not the prehash/digest) in both cases,
    // even when the signature was created by `ccmldsa_sign_prehashed`
    if (operation != kSecKeyOperationTypeVerify || !CFEqual(algorithm, kSecKeyAlgorithmMLDSASignatureMessage)) {
        // MLDSA public key supports only signature verification.
        return kCFNull;
    }

    switch (mode) {
        case kSecKeyOperationModePerform: {
            if (CFGetTypeID(in1) != CFDataGetTypeID() || CFGetTypeID(in2) != CFDataGetTypeID()) {
                SecError(errSecVerifyFailed, error, CFSTR("ML-DSA signature verification failed (invalid data inputs)"));
                return NULL;
            }

            int err = -1;
            size_t signatureLength = CFDataGetLength((CFDataRef)in2);
            const uint8_t *signature = CFDataGetBytePtr((CFDataRef)in2);
            size_t messageLength = CFDataGetLength((CFDataRef)in1);
            const uint8_t *message = CFDataGetBytePtr((CFDataRef)in1);
            ccmldsa_pub_ctx_t ctx = key->key;
            
            if (signatureLength != ccmldsa_signature_nbytes_ctx(ctx)) {
                SecError(errSecVerifyFailed, error, CFSTR("ML-DSA signature verification failed (invalid signature length)"));
                return NULL;
            }

            err = ccmldsa_verify(ctx, signatureLength, signature, messageLength, message);

            if (err != CCERR_OK) {
                SecError(errSecVerifyFailed, error, CFSTR("Signature verification failed, err=%d for key %@"), err, key);
                return NULL;
            } else {
                return kCFBooleanTrue;
            }
        }

        case kSecKeyOperationModeCheckIfSupported:
            return kCFBooleanTrue;

        default:
            return kCFNull;
    }
}

static SecKeyDescriptor kSecMLDSAPublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "MLDSAPublicKey",
    .init = SecMLDSAPublicKeyInit,
    .destroy = SecMLDSAKeyPublicKeyDestroy,
    .blockSize = SecMLDSAPublicKeyBlockSize,
    .copyDictionary = SecMLDSAPublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecMLDSAPublicKeyCopyExternalRepresentation,
    .describe = SecMLDSAPublicKeyCopyKeyDescription,
    .getAlgorithmID = SecMLDSAKeyGetAlgorithmID,
    .copyPublic = SecMLDSAPublicKeyCopyPublicOctets,
    .copyOperationResult = SecMLDSAPublicKeyCopyOperationResult,
};

SecKeyRef SecKeyCreateMLDSAPublicKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength) {
    return SecKeyCreate(allocator, &kSecMLDSAPublicKeyDescriptor, keyData, keyDataLength, 0);
}

static void SecMLDSAPrivateKeyDestroy(SecKeyRef key) {
    // Zero out the public key
    ccmldsa_full_ctx_t ctx = key->key;
    if (ctx != NULL) {
        ccmldsa_full_ctx_clear(ctx->params, ctx);
        free(ctx);
    }
}

static OSStatus SecMLDSAPrivateKeyInit(SecKeyRef key, const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    if (keyDataLength == 0) {
        if (keyData != NULL) {
            key->key = (void *)keyData;
            return errSecSuccess;
        } else {
            return errSecParam;
        }
    }

    const struct ccmldsa_params *params;
    if ((size_t)keyDataLength == ccmldsa_pubkey_nbytes_params(ccmldsa65()) + ccmldsa_privkey_nbytes_params(ccmldsa65())) {
        params = ccmldsa65();
    } else if ((size_t)keyDataLength == ccmldsa_pubkey_nbytes_params(ccmldsa87()) + ccmldsa_privkey_nbytes_params(ccmldsa87())) {
        params = ccmldsa87();
    } else {
        secwarning("ML-DSA priv size=%dbytes is invalid", (int)keyDataLength);
        return errSecParam;
    }

    key->key = calloc(1, ccmldsa_sizeof_full_ctx(params));
    ccmldsa_full_ctx_t ctx = key->key;
    size_t pubKeySize = ccmldsa_pubkey_nbytes_params(params);

    int err = ccmldsa_import_privkey(params, keyDataLength - pubKeySize, keyData + pubKeySize, ctx);
    if (err != CCERR_OK) {
        secwarning("ML-DSA privkey size=%dbytes import of priv part failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    err = ccmldsa_import_pubkey(params, pubKeySize, keyData, ccmldsa_public_ctx(ctx));
    if (err != CCERR_OK) {
        secwarning("ML-DSA privkey size=%dbytes import of pub part failed: %d", (int)keyDataLength, err);
        return errSecParam;
    }

    return errSecSuccess;
}

static CFStringRef SecMLDSAPrivateKeyCopyKeyDescription(SecKeyRef key) {
    ccmldsa_full_ctx_t ctx = key->key;

    NSString *name;
    if (ccmldsa_privkey_nbytes_ctx((void *)ctx) == ccmldsa_privkey_nbytes_params(ccmldsa65())) {
        name = @"ML-DSA-65-privKey";
    } else if (ccmldsa_privkey_nbytes_ctx((void *)ctx) == ccmldsa_privkey_nbytes_params(ccmldsa87())) {
        name = @"ML-DSA-87-privKey";
    } else {
        name = @"ML-DSA";
    }

    return CFBridgingRetain([NSString stringWithFormat:@"<SecKeyRef %@ algorithm id: %lu, key type: %s, version: %d, addr: %p>",
                             name, (long)SecMLDSAKeyGetAlgorithmID(key), key->key_class->name, key->key_class->version, key]);
}

static size_t SecMLDSAPrivateKeyBlockSize(SecKeyRef key) {
    ccmldsa_full_ctx_t ctx = key->key;
    return ccmldsa_privkey_nbytes_ctx(ccmldsa_public_ctx(ctx));
}

static CFDataRef SecMLDSAPrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    ccmldsa_full_ctx_t ctx = key->key;

    size_t pubKeySize = ccmldsa_pubkey_nbytes_ctx(ccmldsa_public_ctx(ctx));
    size_t privKeySize = ccmldsa_privkey_nbytes_ctx(ccmldsa_public_ctx(ctx));

    NSMutableData *data = CFBridgingRelease(CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(),
                                                                           pubKeySize + privKeySize));
    if (!data) {
        SecError(errSecAllocate, error, CFSTR("Failed to allocate key data"));
        return NULL;
    }

    int err = ccmldsa_export_pubkey(ccmldsa_public_ctx(ctx), pubKeySize, data.mutableBytes);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export public part of %@, err=%d"), key, err);
        return NULL;
    }

    err = ccmldsa_export_privkey(ctx, privKeySize, (uint8_t *)data.mutableBytes + pubKeySize);
    if (err != CCERR_OK) {
        SecError(errSecDecode, error, CFSTR("Failed to export private part of %@, err=%d"), key, err);
        return NULL;
    }

    return CFBridgingRetain(data);
}

static OSStatus SecMLDSAPrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    ccmldsa_pub_ctx_t ctx = ccmldsa_public_ctx(key->key);
    *serialization = SecMLDSAPublicKeyCopyData(ctx, NULL);
    return *serialization != NULL ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecMLDSAPrivateKeyCopyAttributeDictionary(SecKeyRef key) {
    ccmldsa_full_ctx_t ctx = key->key;
    id sizeType;
    if (ccmldsa_privkey_nbytes_ctx((void *)ctx) == ccmldsa_privkey_nbytes_params(ccmldsa65())) {
        sizeType = (id)kSecAttrKeySizeMLDSA65;
    } else if (ccmldsa_privkey_nbytes_ctx((void *)ctx) == ccmldsa_privkey_nbytes_params(ccmldsa87())) {
        sizeType = (id)kSecAttrKeySizeMLDSA87;
    } else {
        secerror("Unknown ML-DSA type detected");
        sizeType = @0;
    }

    NSData *pubKeyDigest;
    CFDataRef pubCFKeyBlob;
    if (SecMLDSAPrivateKeyCopyPublicOctets(key, &pubCFKeyBlob) == errSecSuccess) {
        NSData *pubKeyBlob = CFBridgingRelease(pubCFKeyBlob);
        pubKeyDigest = CFBridgingRelease(SecSHA1DigestCreate(kCFAllocatorDefault, pubKeyBlob.bytes, pubKeyBlob.length));
    }
    CFDataRef privateKeyBlob = SecMLDSAPrivateKeyCopyExternalRepresentation(key, NULL);

    return CFBridgingRetain(@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeMLDSA,
        (id)kSecAttrKeySizeInBits: sizeType,
        (id)kSecAttrEffectiveKeySize: sizeType,
        (id)kSecAttrKeyClass: (id)kSecAttrKeyClassPrivate,
        (id)kSecAttrApplicationLabel: pubKeyDigest ?: NSData.data,
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
        (id)kSecAttrCanSign: privateKeyBlob ? @YES : @NO,
        (id)kSecAttrCanVerify: @NO,
        (id)kSecAttrCanSignRecover: @NO,
        (id)kSecAttrCanVerifyRecover: @NO,
        (id)kSecAttrCanWrap: @NO,
        (id)kSecAttrCanUnwrap: @NO,
        (id)kSecValueData: CFBridgingRelease(privateKeyBlob) ?: NSMutableData.data,
    });
}

static CFTypeRef SecMLDSAPrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                       CFArrayRef algorithms, SecKeyOperationMode mode,
                                                       CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (operation != kSecKeyOperationTypeSign || !CFEqual(algorithm, kSecKeyAlgorithmMLDSASignatureMessage)) {
        // ML-DSA private key supports only signing.
        return kCFNull;
    }

    switch (mode) {
        case kSecKeyOperationModePerform: {
            ccmldsa_full_ctx_t ctx = key->key;

            // Message data
            NSData *inputData = (__bridge NSData *)in1;
            if (![inputData isKindOfClass:NSData.class]) {
                SecError(errSecParam, error, CFSTR("Expected input data of type NSData for signing operation"));
                return NULL;
            }

            // We only support context as a parameter
            NSData *context = nil;
            if (in2 != NULL && [(__bridge id)in2 isKindOfClass:NSDictionary.class]) {
                NSDictionary *parameters = (__bridge NSDictionary *)in2;
                id contextObject = parameters[(id)kSecKeySignatureParameterContext];
                if (contextObject && [contextObject isKindOfClass:NSData.class]) {
                    context = (NSData *)contextObject;
                }
            }

            const uint8_t *data = inputData.bytes;
            size_t data_nbytes = inputData.length;

            size_t sig_nbytes = ccmldsa_signature_nbytes_params(ctx->params);
            NSMutableData *signatureData = CFBridgingRelease(CFDataCreateMutableWithScratch(kCFAllocatorDefault, sig_nbytes));
            uint8_t *sig = signatureData.mutableBytes;

            int err = -1;

            if (context && context.length > 0) {
                const uint8_t *contextData = context.bytes;
                size_t contextData_nbytes = context.length;

                err = ccmldsa_sign_with_context(ctx, sig_nbytes, sig, data_nbytes, data, contextData_nbytes, contextData, ccrng_seckey());
            } else {
                err = ccmldsa_sign(ctx, sig_nbytes, sig, data_nbytes, data, ccrng_seckey());
            }

            if (err != CCERR_OK) {
                SecError(errSecParam, error, CFSTR("Signing failed, err=%d"), err);
                return NULL;
            }
            return CFBridgingRetain(signatureData);
        }
        case kSecKeyOperationModeCheckIfSupported:
            return kCFBooleanTrue;
        default:
            return kCFNull;
    }
}

static SecKeyDescriptor kSecMLDSAPrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "MLDSAPrivateKey",
    .init = SecMLDSAPrivateKeyInit,
    .destroy = SecMLDSAPrivateKeyDestroy,
    .blockSize = SecMLDSAPrivateKeyBlockSize,
    .copyDictionary = SecMLDSAPrivateKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecMLDSAPrivateKeyCopyExternalRepresentation,
    .describe = SecMLDSAPrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecMLDSAKeyGetAlgorithmID,
    .copyPublic = SecMLDSAPrivateKeyCopyPublicOctets,
    .copyOperationResult = SecMLDSAPrivateKeyCopyOperationResult,
};

SecKeyRef SecKeyCreateMLDSAPrivateKey(CFAllocatorRef allocator, const uint8_t *keyData, CFIndex keyDataLength) {
    return SecKeyCreate(allocator, &kSecMLDSAPrivateKeyDescriptor, keyData, keyDataLength, 0);
}

OSStatus SecMLDSAKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    const struct ccmldsa_params *params = ccmldsa65();

    id mldsaType = ((__bridge NSDictionary *)parameters)[(id)kSecAttrKeySizeInBits];
    if (mldsaType != nil) {
        if ([mldsaType integerValue] == [(id)kSecAttrKeySizeMLDSA65 integerValue]) {
            params = ccmldsa65();
        } else if ([mldsaType integerValue] == [(id)kSecAttrKeySizeMLDSA87 integerValue]) {
            params = ccmldsa87();
        } else {
            secwarning("Invalid ML-DSA type %@ requested for ML-DSA key generation", mldsaType);
            return errSecParam;
        }
    }

    ccmldsa_full_ctx_t fullctx = calloc(1, ccmldsa_sizeof_full_ctx(params));
    if (!fullctx) {
        return errSecAllocate;
    }
    ccmldsa_full_ctx_init(fullctx, params);

    int err = ccmldsa_generate_key(fullctx, ccrng_seckey());
    if (err != CCERR_OK) {
        free(fullctx);
        secwarning("Failed to generate ML-DSA key: err %d", (int)err);
        return errSecDecode;
    }

    id privKey = CFBridgingRelease(SecKeyCreate(SecCFAllocatorZeroize(), &kSecMLDSAPrivateKeyDescriptor, (const void *)fullctx, 0, 0));
    if (privKey == nil) {
        return errSecParam;
    }

    ccmldsa_pub_ctx_t pubctx = calloc(1, ccmldsa_sizeof_pub_ctx(params));
    memcpy(pubctx, ccmldsa_public_ctx(fullctx), ccmldsa_sizeof_pub_ctx(params));

    id pubKey = CFBridgingRelease(SecKeyCreate(kCFAllocatorDefault, &kSecMLDSAPublicKeyDescriptor, (const void *)pubctx, 0, 0));
    if (pubKey == nil) {
        return errSecParam;
    }

    if (publicKey != NULL) {
        *publicKey = (SecKeyRef)CFBridgingRetain(pubKey);
        pubKey = NULL;
    }
    if (privateKey != NULL) {
        *privateKey = (SecKeyRef)CFBridgingRetain(privKey);
        privKey = NULL;
    }

    return errSecSuccess;
}
