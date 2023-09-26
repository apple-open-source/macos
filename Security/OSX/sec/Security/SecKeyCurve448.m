/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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
 * _SECURITY_SECCURVE448_H_.m - CoreFoundation based Ed448 and X448 key objects
 */

#include "SecKeyCurve448.h"
#include "SecKeyCurve448Priv.h"

#import <Foundation/Foundation.h>

#include <Security/SecKeyInternal.h>
#include <Security/SecItem.h>
#include <Security/SecBasePriv.h>
#include <AssertMacros.h>
#include <Security/SecureTransport.h> /* For error codes. */
#include <CoreFoundation/CFData.h> /* For error codes. */
#include <CoreFoundation/CFNumber.h>
#include <Security/SecFramework.h>
#include <Security/SecRandom.h>
#include <utilities/debugging.h>
#include <Security/SecItemPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecCFAllocator.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <corecrypto/ccec.h>
#include <corecrypto/ccec448.h>
#include <corecrypto/ccec448_priv.h>
#include <corecrypto/ccsha2.h>

static os_log_t _SECKEY_LOG(void) {
    static dispatch_once_t once;
    static os_log_t log;
    dispatch_once(&once, ^{ log = os_log_create("com.apple.security", "seckey"); });
    return log;
};
#define SECKEY_LOG _SECKEY_LOG()

static bool isEd448Key(SecKeyRef key);

static CFIndex SecCurve448KeyGetAlgorithmID(SecKeyRef key) {
    return isEd448Key(key) ? kSecEd448AlgorithmID : kSecX448AlgorithmID;
}

/*
 *
 * Public Key
 *
 */

/* Public key static functions. */
static void SecCurve448PublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    uint8_t* pubkey = key->key;
    cc_clear(isEd448Key(key) ? sizeof(cced448pubkey) : sizeof(ccec448pubkey), pubkey);
}

static OSStatus SecCurve448PublicKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    uint8_t *pubkey = key->key;
    OSStatus err = errSecParam;
    
    switch (encoding) {
    case kSecKeyEncodingBytes:
    {
        const CFIndex expectedKeyLength = isEd448Key(key) ? sizeof(cced448pubkey) : sizeof(ccec448pubkey);
        require_action_quiet(keyDataLength == expectedKeyLength, errOut, err = errSecDecode);
        memcpy(pubkey, keyData, keyDataLength);
        err = errSecSuccess;
        break;
    }
    case kSecExtractPublicFromPrivate:
    {
        const uint8_t *privatekey = keyData;
        const CFIndex expectedKeyLength = isEd448Key(key) ? sizeof(cced448secretkey) : sizeof(ccec448secretkey);
        require_action_quiet(keyDataLength == expectedKeyLength, errOut, err = errSecDecode);
        if (isEd448Key(key)) {
            int errcc = cced448_make_pub(ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, errOut, err = errSecDecode; os_log_error(SECKEY_LOG, "cced448_make_pub() failed, error %d", (int)errcc););
        } else {
            int errcc = cccurve448_make_pub(ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, errOut, err = errSecDecode; os_log_error(SECKEY_LOG, "cccurve448_make_pub() failed, error %d", (int)errcc););
        }
        err = errSecSuccess;
        break;
    }
    default:
        require_action_quiet(0, errOut, err = errSecParam);
        break;
    }

errOut:
    return err;
}

static CFTypeRef SecCurve448PublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                   CFArrayRef algorithms, SecKeyOperationMode mode,
                                                   CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (!isEd448Key(key)) {
        // X448 public key does not support anything
        return kCFNull;
    }
    if (operation != kSecKeyOperationTypeVerify || !CFEqual(algorithm, kSecKeyAlgorithmEdDSASignatureMessageCurve448SHAKE256)) {
        // Ed448 public key supports only signature verification with EdDSA algorithm.
        return kCFNull;
    }

    if (mode == kSecKeyOperationModePerform) {
        int err = -1;
        size_t sigLen = CFDataGetLength(in2);
        const uint8_t *sig = CFDataGetBytePtr(in2);
        size_t messageLen = CFDataGetLength(in1);
        const uint8_t *message = CFDataGetBytePtr(in1);
        const uint8_t *pubkey = key->key;

        if (sigLen != sizeof(cced448signature)) {
            SecError(errSecVerifyFailed, error, CFSTR("Ed448 signature verification failed (invalid signature length)"));
            return NULL;
        }
        
        err = cced448_verify(messageLen, message, sig, pubkey);
        if (err != 0) {
            SecError(errSecVerifyFailed, error, CFSTR("Ed448 signature verification failed (ccerr %d)"), err);
            return NULL;
        } else {
            return kCFBooleanTrue;
        }
    } else {
        // Algorithm is supported.
        return kCFBooleanTrue;
    }
}

static size_t SecCurve448PublicKeyBlockSize(SecKeyRef key) {
    /* Get key size in octets */
    return isEd448Key(key) ? sizeof(cced448pubkey) : sizeof(ccec448pubkey);
}

static CFDataRef SecCurve448PublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    const uint8_t* pubkey = key->key;
    return CFDataCreate(CFGetAllocator(key), pubkey, SecCurve448PublicKeyBlockSize(key));
}

static OSStatus SecCurve448PublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    *serialization = SecCurve448PublicKeyCopyExternalRepresentation(key, NULL);
    return *serialization ? errSecSuccess : errSecDecode;
}

static CFDictionaryRef SecCurve448PublicKeyCopyAttributeDictionary(SecKeyRef key) {
    CFDictionaryRef dict = SecKeyGeneratePublicAttributeDictionary(key, isEd448Key(key) ? kSecAttrKeyTypeEd448 : kSecAttrKeyTypeX448);
    CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(mutableDict, kSecAttrCanDerive, kCFBooleanFalse);
    CFAssignRetained(dict, mutableDict);
    return dict;
}

static const char * getCurveName(SecKeyRef key) {
    return isEd448Key(key) ? "kSecEd448" : "kSecX448";
}

static CFStringRef SecCurve448PublicKeyCopyKeyDescription(SecKeyRef key)
{
    const char* curve = getCurveName(key);
    NSString *description = [NSString stringWithFormat:@"<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>",
                             curve, (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version,
                             8 * SecKeyGetBlockSize(key), key];
    return CFBridgingRetain(description);
}

static CFDataRef SecCurve448KeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
    return NULL;
}

static SecKeyDescriptor kSecEd448PublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "Ed448PublicKey",
    .extraBytes = sizeof(cced448pubkey),
    
    .init = SecCurve448PublicKeyInit,
    .destroy = SecCurve448PublicKeyDestroy,
    .blockSize = SecCurve448PublicKeyBlockSize,
    .copyDictionary = SecCurve448PublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecCurve448PublicKeyCopyExternalRepresentation,
    .describe = SecCurve448PublicKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve448KeyGetAlgorithmID,
    .copyPublic = SecCurve448PublicKeyCopyPublicOctets,
    .copyWrapKey = SecCurve448KeyCopyWrapKey,
    .copyOperationResult = SecCurve448PublicKeyCopyOperationResult,
};

static SecKeyDescriptor kSecX448PublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "X448PublicKey",
    .extraBytes = sizeof(ccec448pubkey),
    
    .init = SecCurve448PublicKeyInit,
    .destroy = SecCurve448PublicKeyDestroy,
    .blockSize = SecCurve448PublicKeyBlockSize,
    .copyDictionary = SecCurve448PublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecCurve448PublicKeyCopyExternalRepresentation,
    .describe = SecCurve448PublicKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve448KeyGetAlgorithmID,
    .copyPublic = SecCurve448PublicKeyCopyPublicOctets,
    .copyWrapKey = SecCurve448KeyCopyWrapKey,
    .copyOperationResult = SecCurve448PublicKeyCopyOperationResult,
};

/* Public Key functions called from SecKey.m */

SecKeyRef SecKeyCreateEd448PublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecEd448PublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}

SecKeyRef SecKeyCreateX448PublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecX448PublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}

/*
 *
 * Private Key
 *
 */

/* Private key static functions. */
static void SecCurve448PrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the private key */
    uint8_t *privatekey = key->key;
    cc_clear(isEd448Key(key) ? sizeof(cced448secretkey) : sizeof(ccec448secretkey), privatekey);
}

static OSStatus SecCurve448PrivateKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    uint8_t *privatekey = key->key;
    OSStatus err = errSecInternalError;

    switch (encoding) {
    case kSecKeyEncodingBytes:
    {
        const CFIndex expectKeyLength = isEd448Key(key) ? sizeof(cced448secretkey) : sizeof(ccec448secretkey);
        require_action_quiet(keyDataLength == expectKeyLength, exit, err = errSecDecode);
        memcpy(privatekey, keyData, keyDataLength);
        err = errSecSuccess;
        break;
    }
    case kSecGenerateKey:
    {
        if (isEd448Key(key)) {
            cced448pubkey pubkey = {};
            int errcc = cced448_make_key_pair(ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, exit, err = errSecDecode; os_log_error(SECKEY_LOG, "cced448_make_key_pair() failed, error %d", (int)errcc););
            cc_clear(sizeof(pubkey), pubkey);
        } else {
            ccec448pubkey pubkey = {};
            int errcc = cccurve448_make_key_pair(ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, exit, err = errSecDecode; os_log_error(SECKEY_LOG, "ccec448_make_key_pair() failed, error %d", (int)errcc););
            cc_clear(sizeof(pubkey), pubkey);
        }
        err = errSecSuccess;
        break;
    }

    default:
        break;
    }
exit:
    return err;
}

static CFTypeRef SecCurve448PrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                    CFArrayRef allAlgorithms, SecKeyOperationMode mode,
                                                    CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    // Default answer is 'unsupported', unless we find out that we can support it.
    CFTypeRef result = kCFNull;

    const uint8_t *privatekey = key->key;
    switch (operation) {
        case kSecKeyOperationTypeSign: {
            if (isEd448Key(key) && CFEqual(algorithm, kSecKeyAlgorithmEdDSASignatureMessageCurve448SHAKE256)) {
                if (mode == kSecKeyOperationModePerform) {
                    // Perform EdDSA mode of signature.
                    int err = 0;
                    cced448pubkey pubkey = {};
                    err = cced448_make_pub(ccrng_seckey(), pubkey, privatekey);
                    require_noerr_action_quiet(err, out, SecError(errSecInternalComponent, error, CFSTR("%@: Failed to get public key from private key"), key));
                    size_t size = sizeof(cced448signature);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    require_action_quiet(result, out, SecError(errSecAllocate, error, CFSTR("%@: Failed to create buffer for a signature"), key));
                    size_t msgLen = CFDataGetLength(in1);
                    const uint8_t *msg = CFDataGetBytePtr(in1);
                    uint8_t *signaturePtr = CFDataGetMutableBytePtr((CFMutableDataRef)result);
                    
                    err = cced448_sign(ccrng_seckey(), signaturePtr, msgLen, msg, pubkey, privatekey);
                    cc_clear(sizeof(pubkey), pubkey);
                    require_action_quiet(err == 0, out, (CFReleaseNull(result),
                                                         SecError(errSecParam, error, CFSTR("%@: Ed448 signing failed (ccerr %d)"),
                                                                  key, err)));
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        }
        case kSecKeyOperationTypeKeyExchange:
            if (!isEd448Key(key) && (CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeStandard) ||
                CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeCofactor))) {
                if (mode == kSecKeyOperationModePerform) {
                    const uint8_t *hispub = CFDataGetBytePtr(in1);
                    size_t hislen = CFDataGetLength(in1);
                    require_action_quiet(hislen == sizeof(ccec448pubkey), out,
                                         SecError(errSecParam, error, CFSTR("X448priv sharedsecret: bad public key")));
                    
                    size_t size = sizeof(ccec448key);
                    result = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), size);
                    CFDataSetLength((CFMutableDataRef)result, size);
                    int err = cccurve448(ccrng_seckey(), CFDataGetMutableBytePtr((CFMutableDataRef)result), privatekey, hispub);
                    require_action_quiet(err == 0, out, (CFReleaseNull(result),
                                                         SecError(errSecParam, error, CFSTR("%@: X448 DH failed (ccerr %d)"),
                                                                  key, err)));
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        default:
            break;
    }

out:
    return result;
}

static size_t SecCurve448PrivateKeyBlockSize(SecKeyRef key) {
    return isEd448Key(key) ? sizeof(cced448secretkey) : sizeof(ccec448secretkey);
}

static OSStatus SecCurve448PrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization)
{
    const uint8_t *privatekey = key->key;
    if (isEd448Key(key)) {
        cced448pubkey pubkey = {};
        int err = cced448_make_pub(ccrng_seckey(), pubkey, privatekey);
        if (err != 0) {
            return errSecInternal;
        }
        *serialization = CFDataCreate(CFGetAllocator(key), pubkey, sizeof(pubkey));
        cc_clear(sizeof(pubkey), pubkey);
    } else {
        ccec448pubkey pubkey = {};
        int err = cccurve448_make_pub(ccrng_seckey(), pubkey, privatekey);
        if (err != 0) {
            return errSecInternal;
        }
        *serialization = CFDataCreate(CFGetAllocator(key), pubkey, sizeof(pubkey));
        cc_clear(sizeof(pubkey), pubkey);
    }

    if (NULL == *serialization)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDataRef SecCurve448PrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    const uint8_t* privatekey = key->key;
    return CFDataCreate(CFGetAllocator(key), privatekey, SecCurve448PrivateKeyBlockSize(key));
}

static CFDictionaryRef SecCurve448PrivateKeyCopyAttributeDictionary(SecKeyRef key) {
    /* Export the full ec key pair. */
    CFDataRef fullKeyBlob = SecCurve448PrivateKeyCopyExternalRepresentation(key, NULL);

    CFDictionaryRef dict = SecKeyGeneratePrivateAttributeDictionary(key, isEd448Key(key) ? kSecAttrKeyTypeEd448 : kSecAttrKeyTypeX448, fullKeyBlob);
	CFReleaseSafe(fullKeyBlob);
	return dict;
}
static CFStringRef SecCurve448PrivateKeyCopyKeyDescription(SecKeyRef key) {
    const char* curve = getCurveName(key);
	return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR( "<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), curve, (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);
}

static CFDataRef SecCurve448KeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
    return NULL;
}

static SecKeyDescriptor kSecEd448PrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "Ed448PrivateKey",
    .extraBytes = sizeof(cced448secretkey),

    .init = SecCurve448PrivateKeyInit,
    .destroy = SecCurve448PrivateKeyDestroy,
    .blockSize = SecCurve448PrivateKeyBlockSize,
    .copyDictionary = SecCurve448PrivateKeyCopyAttributeDictionary,
    .describe = SecCurve448PrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve448KeyGetAlgorithmID,
    .copyPublic = SecCurve448PrivateKeyCopyPublicOctets,
    .copyExternalRepresentation = SecCurve448PrivateKeyCopyExternalRepresentation,
    .copyWrapKey = SecCurve448KeyCopyWrapKey,
    .copyUnwrapKey = SecCurve448KeyCopyUnwrapKey,
    .copyOperationResult = SecCurve448PrivateKeyCopyOperationResult,
};

static SecKeyDescriptor kSecX448PrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "X448PrivateKey",
    .extraBytes = sizeof(ccec448secretkey),

    .init = SecCurve448PrivateKeyInit,
    .destroy = SecCurve448PrivateKeyDestroy,
    .blockSize = SecCurve448PrivateKeyBlockSize,
    .copyDictionary = SecCurve448PrivateKeyCopyAttributeDictionary,
    .describe = SecCurve448PrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve448KeyGetAlgorithmID,
    .copyPublic = SecCurve448PrivateKeyCopyPublicOctets,
    .copyExternalRepresentation = SecCurve448PrivateKeyCopyExternalRepresentation,
    .copyWrapKey = SecCurve448KeyCopyWrapKey,
    .copyUnwrapKey = SecCurve448KeyCopyUnwrapKey,
    .copyOperationResult = SecCurve448PrivateKeyCopyOperationResult,
};

/* Private Key functions called from SecKey.m */
SecKeyRef SecKeyCreateEd448PrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecEd448PrivateKeyDescriptor, keyData,
        keyDataLength, encoding);
}

SecKeyRef SecKeyCreateX448PrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecX448PrivateKeyDescriptor, keyData,
        keyDataLength, encoding);
}

static OSStatus curve448KeyGeneratePair(CFDictionaryRef parameters,
                                          SecKeyRef *publicKey, SecKeyRef *privateKey, CFIndex algorithm) {
    OSStatus status = errSecParam;

    CFAllocatorRef allocator = SecCFAllocatorZeroize(); /* @@@ get from parameters. */
    SecKeyRef pubKey = NULL;
    
    const SecKeyDescriptor *privKeyDescriptor = (algorithm == kSecEd448AlgorithmID) ? &kSecEd448PrivateKeyDescriptor : &kSecX448PrivateKeyDescriptor;

    const SecKeyDescriptor *pubKeyDescriptor = (algorithm == kSecEd448AlgorithmID) ? &kSecEd448PublicKeyDescriptor : &kSecX448PublicKeyDescriptor;

    SecKeyRef privKey = SecKeyCreate(allocator, privKeyDescriptor, (const void*) parameters, 0, kSecGenerateKey);
    require_quiet(privKey, errOut);

    /* Create SecKeyRef's from the private key. */
    pubKey = SecKeyCreate(allocator, pubKeyDescriptor,
                          privKey->key, (algorithm == kSecEd448AlgorithmID) ? sizeof(cced448secretkey) : sizeof(ccec448secretkey), kSecExtractPublicFromPrivate);

    require_quiet(pubKey, errOut);

    if (publicKey) {
        *publicKey = pubKey;
        pubKey = NULL;
    }
    if (privateKey) {
        *privateKey = privKey;
        privKey = NULL;
    }

    status = errSecSuccess;

errOut:
    CFReleaseSafe(pubKey);
    CFReleaseSafe(privKey);

    return status;
}

OSStatus SecEd448KeyGeneratePair(CFDictionaryRef parameters,
                              SecKeyRef *publicKey, SecKeyRef *privateKey) {
    return curve448KeyGeneratePair(parameters, publicKey, privateKey, kSecEd448AlgorithmID);
}

OSStatus SecX448KeyGeneratePair(CFDictionaryRef parameters,
                                  SecKeyRef *publicKey, SecKeyRef *privateKey) {
    return curve448KeyGeneratePair(parameters, publicKey, privateKey, kSecX448AlgorithmID);
}

static bool isEd448Key(SecKeyRef key) {
    return key->key_class == &kSecEd448PublicKeyDescriptor || key->key_class == &kSecEd448PrivateKeyDescriptor;
}

