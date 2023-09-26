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
 * _SECURITY_SECCURVE25519_H_.m - CoreFoundation based Ed25519 and X25519 key objects
 */

#include "SecKeyCurve25519.h"
#include "SecKeyCurve25519Priv.h"

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
#include <corecrypto/ccec25519.h>
#include <corecrypto/ccec25519_priv.h>
#include <corecrypto/ccsha2.h>

static os_log_t _SECKEY_LOG(void) {
    static dispatch_once_t once;
    static os_log_t log;
    dispatch_once(&once, ^{ log = os_log_create("com.apple.security", "seckey"); });
    return log;
};
#define SECKEY_LOG _SECKEY_LOG()

static bool isEd25519Key(SecKeyRef key);

static CFIndex SecCurve25519KeyGetAlgorithmID(SecKeyRef key) {
    return isEd25519Key(key) ? kSecEd25519AlgorithmID : kSecX25519AlgorithmID;
}

/*
 *
 * Public Key
 *
 */

/* Public key static functions. */
static void SecCurve25519PublicKeyDestroy(SecKeyRef key) {
    /* Zero out the public key */
    uint8_t* pubkey = key->key;
    cc_clear(sizeof(ccec25519pubkey), pubkey);
}

static OSStatus SecCurve25519PublicKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    uint8_t *pubkey = key->key;
    OSStatus err = errSecParam;
    
    switch (encoding) {
    case kSecKeyEncodingBytes:
    {
        require_action_quiet(keyDataLength == sizeof(ccec25519pubkey), errOut, err = errSecDecode);
        memcpy(pubkey, keyData, keyDataLength);
        err = errSecSuccess;
        break;
    }
    case kSecExtractPublicFromPrivate:
    {
        const uint8_t *privatekey = keyData;
        require_action_quiet(keyDataLength == sizeof(ccec25519secretkey), errOut, err = errSecDecode);
        if (isEd25519Key(key)) {
            int errcc = cced25519_make_pub_with_rng(ccsha512_di(), ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, errOut, err = errSecDecode; os_log_error(SECKEY_LOG, "cced25519_make_pub_with_rng() failed, error %d", (int)errcc););
        } else {
            int errcc = cccurve25519_make_pub_with_rng(ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, errOut, err = errSecDecode; os_log_error(SECKEY_LOG, "cccurve25519_make_pub_with_rng() failed, error %d", (int)errcc););
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

static CFTypeRef SecCurve25519PublicKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                   CFArrayRef algorithms, SecKeyOperationMode mode,
                                                   CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    if (!isEd25519Key(key)) {
        // X25519 public key does not support anything
        return kCFNull;
    }
    if (operation != kSecKeyOperationTypeVerify || !CFEqual(algorithm, kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512)) {
        // Ed25519 public key supports only signature verification with EdDSA algorithm.
        return kCFNull;
    }

    if (mode == kSecKeyOperationModePerform) {
        int err = -1;
        size_t sigLen = CFDataGetLength(in2);
        const uint8_t *sig = CFDataGetBytePtr(in2);
        size_t messageLen = CFDataGetLength(in1);
        const uint8_t *message = CFDataGetBytePtr(in1);
        const uint8_t *pubkey = key->key;

        if (sigLen != sizeof(ccec25519signature)) {
            SecError(errSecVerifyFailed, error, CFSTR("Ed25519 signature verification failed (invalid signature length)"));
            return NULL;
        }
        
        err = cced25519_verify(ccsha512_di(), messageLen, message, sig, pubkey);
        if (err != 0) {
            SecError(errSecVerifyFailed, error, CFSTR("Ed25519 signature verification failed (ccerr %d)"), err);
            return NULL;
        } else {
            return kCFBooleanTrue;
        }
    } else {
        // Algorithm is supported.
        return kCFBooleanTrue;
    }
}

static size_t SecCurve25519PublicKeyBlockSize(SecKeyRef key) {
    /* Get key size in octets */
    return sizeof(ccec25519pubkey);
}

static CFDataRef SecCurve25519KeyExport(CFAllocatorRef allocator, const ccec25519key key) {
    return CFDataCreate(allocator, key, sizeof(ccec25519key));
}

static CFDataRef SecCurve25519PublicKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    const uint8_t* pubkey = key->key;
    return SecCurve25519KeyExport(CFGetAllocator(key), pubkey);
}

static OSStatus SecCurve25519PublicKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization) {
    const uint8_t* pubkey = key->key;

    *serialization = SecCurve25519KeyExport(CFGetAllocator(key), pubkey);

    if (NULL == *serialization)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDictionaryRef SecCurve25519PublicKeyCopyAttributeDictionary(SecKeyRef key) {
    CFDictionaryRef dict = SecKeyGeneratePublicAttributeDictionary(key, isEd25519Key(key) ? kSecAttrKeyTypeEd25519 : kSecAttrKeyTypeX25519);
    CFMutableDictionaryRef mutableDict = CFDictionaryCreateMutableCopy(NULL, 0, dict);
    CFDictionarySetValue(mutableDict, kSecAttrCanDerive, kCFBooleanFalse);
    CFAssignRetained(dict, mutableDict);
    return dict;
}

static const char * getCurveName(SecKeyRef key) {
    return isEd25519Key(key) ? "kSecEd25519" : "kSecX25519";
}

static CFStringRef SecCurve25519PublicKeyCopyKeyDescription(SecKeyRef key)
{
    const char* curve = getCurveName(key);
    NSString *description = [NSString stringWithFormat:@"<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>",
                             curve, (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version,
                             8 * SecKeyGetBlockSize(key), key];
    return CFBridgingRetain(description);
}

static CFDataRef SecCurve25519KeyCopyWrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef unwrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
    return NULL;
}

static SecKeyDescriptor kSecEd25519PublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "Ed25519PublicKey",
    .extraBytes = sizeof(ccec25519pubkey),
    
    .init = SecCurve25519PublicKeyInit,
    .destroy = SecCurve25519PublicKeyDestroy,
    .blockSize = SecCurve25519PublicKeyBlockSize,
    .copyDictionary = SecCurve25519PublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecCurve25519PublicKeyCopyExternalRepresentation,
    .describe = SecCurve25519PublicKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve25519KeyGetAlgorithmID,
    .copyPublic = SecCurve25519PublicKeyCopyPublicOctets,
    .copyWrapKey = SecCurve25519KeyCopyWrapKey,
    .copyOperationResult = SecCurve25519PublicKeyCopyOperationResult,
};

static SecKeyDescriptor kSecX25519PublicKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "X25519PublicKey",
    .extraBytes = sizeof(ccec25519pubkey),
    
    .init = SecCurve25519PublicKeyInit,
    .destroy = SecCurve25519PublicKeyDestroy,
    .blockSize = SecCurve25519PublicKeyBlockSize,
    .copyDictionary = SecCurve25519PublicKeyCopyAttributeDictionary,
    .copyExternalRepresentation = SecCurve25519PublicKeyCopyExternalRepresentation,
    .describe = SecCurve25519PublicKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve25519KeyGetAlgorithmID,
    .copyPublic = SecCurve25519PublicKeyCopyPublicOctets,
    .copyWrapKey = SecCurve25519KeyCopyWrapKey,
    .copyOperationResult = SecCurve25519PublicKeyCopyOperationResult,
};

/* Public Key functions called from SecKey.m */

SecKeyRef SecKeyCreateEd25519PublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecEd25519PublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}

SecKeyRef SecKeyCreateX25519PublicKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecX25519PublicKeyDescriptor, keyData,
                        keyDataLength, encoding);
}

/*
 *
 * Private Key
 *
 */

/* Private key static functions. */
static void SecCurve25519PrivateKeyDestroy(SecKeyRef key) {
    /* Zero out the private key */
    uint8_t *privatekey = key->key;
    cc_clear(sizeof(ccec25519secretkey), privatekey);
}


static OSStatus SecCurve25519PrivateKeyInit(SecKeyRef key,
    const uint8_t *keyData, CFIndex keyDataLength, SecKeyEncoding encoding) {
    uint8_t *privatekey = key->key;
    OSStatus err = errSecInternalError;

    switch (encoding) {
    case kSecKeyEncodingBytes:
    {
        require_action_quiet(keyDataLength == sizeof(ccec25519secretkey), exit, err = errSecDecode);
        memcpy(privatekey, keyData, keyDataLength);
        err = errSecSuccess;
        break;
    }
    case kSecGenerateKey:
    {
        ccec25519pubkey pubkey = {};
        if (isEd25519Key(key)) {
            int errcc = cced25519_make_key_pair(ccsha512_di(), ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, exit, err = errSecDecode; os_log_error(SECKEY_LOG, "cced25519_make_key_pair() failed, error %d", (int)errcc););
        } else {
            int errcc = cccurve25519_make_key_pair(ccrng_seckey(), pubkey, privatekey);
            require_noerr_action_quiet(errcc, exit, err = errSecDecode; os_log_error(SECKEY_LOG, "cccurve25519_make_key_pair() failed, error %d", (int)errcc););
        }
        cc_clear(sizeof(pubkey), pubkey);
        err = errSecSuccess;
        break;
    }

    default:
        break;
    }
exit:
    return err;
}

static CFTypeRef SecCurve25519PrivateKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                                    CFArrayRef allAlgorithms, SecKeyOperationMode mode,
                                                    CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    // Default answer is 'unsupported', unless we find out that we can support it.
    CFTypeRef result = kCFNull;

    const uint8_t *privatekey = key->key;
    switch (operation) {
        case kSecKeyOperationTypeSign: {
            if (isEd25519Key(key) && CFEqual(algorithm, kSecKeyAlgorithmEdDSASignatureMessageCurve25519SHA512)) {
                if (mode == kSecKeyOperationModePerform) {
                    // Perform EdDSA mode of signature.
                    int err = 0;
                    ccec25519pubkey pubkey = {};
                    err = cced25519_make_pub_with_rng(ccsha512_di(), ccrng_seckey(), pubkey, privatekey);
                    require_noerr_action_quiet(err, out, SecError(errSecInternalComponent, error, CFSTR("%@: Failed to get public key from private key"), key));
                    size_t size = sizeof(ccec25519signature);
                    result = CFDataCreateMutableWithScratch(NULL, size);
                    require_action_quiet(result, out, SecError(errSecAllocate, error, CFSTR("%@: Failed to create buffer for a signature"), key));
                    size_t msgLen = CFDataGetLength(in1);
                    const uint8_t *msg = CFDataGetBytePtr(in1);
                    uint8_t *signaturePtr = CFDataGetMutableBytePtr((CFMutableDataRef)result);
                    
                    err = cced25519_sign_with_rng(ccsha512_di(), ccrng_seckey(), signaturePtr, msgLen, msg, pubkey, privatekey);
                    cc_clear(sizeof(pubkey), pubkey);
                    require_action_quiet(err == 0, out, (CFReleaseNull(result),
                                                         SecError(errSecParam, error, CFSTR("%@: Ed25519 signing failed (ccerr %d)"),
                                                                  key, err)));
                } else {
                    // Operation is supported.
                    result = kCFBooleanTrue;
                }
            }
            break;
        }
        case kSecKeyOperationTypeKeyExchange:
            if (!isEd25519Key(key) && (CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeStandard) ||
                CFEqual(algorithm, kSecKeyAlgorithmECDHKeyExchangeCofactor))) {
                if (mode == kSecKeyOperationModePerform) {
                    const uint8_t *hispub = CFDataGetBytePtr(in1);
                    size_t hislen = CFDataGetLength(in1);
                    require_action_quiet(hislen == sizeof(ccec25519pubkey), out,
                                         SecError(errSecParam, error, CFSTR("X25519priv sharedsecret: bad public key")));
                    
                    size_t size = sizeof(ccec25519key);
                    result = CFDataCreateMutableWithScratch(SecCFAllocatorZeroize(), size);
                    int err = cccurve25519_with_rng(ccrng_seckey(), CFDataGetMutableBytePtr((CFMutableDataRef)result), privatekey, hispub);
                    require_action_quiet(err == 0, out, (CFReleaseNull(result),
                                                         SecError(errSecParam, error, CFSTR("%@: X25519 DH failed (ccerr %d)"),
                                                                  key, err)));
                    CFDataSetLength((CFMutableDataRef)result, size);
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

static size_t SecCurve25519PrivateKeyBlockSize(SecKeyRef key) {
    return sizeof(ccec25519secretkey);
}

static OSStatus SecCurve25519PrivateKeyCopyPublicOctets(SecKeyRef key, CFDataRef *serialization)
{
    const uint8_t *privatekey = key->key;
    ccec25519pubkey pubkey = {};
    if (isEd25519Key(key)) {
        int err = cced25519_make_pub_with_rng(ccsha512_di(), ccrng_seckey(), pubkey, privatekey);
        if (err != 0) {
            return errSecInternal;
        }
    } else {
        int err = cccurve25519_make_pub_with_rng(ccrng_seckey(), pubkey, privatekey);
        if (err != 0) {
            return errSecInternal;
        }
    }

	CFAllocatorRef allocator = CFGetAllocator(key);
    *serialization = SecCurve25519KeyExport(allocator, pubkey);
    cc_clear(sizeof(pubkey), pubkey);

    if (NULL == *serialization)
        return errSecDecode;
    else
        return errSecSuccess;
}

static CFDataRef SecCurve25519PrivateKeyCopyExternalRepresentation(SecKeyRef key, CFErrorRef *error) {
    const uint8_t* privatekey = key->key;
    return SecCurve25519KeyExport(CFGetAllocator(key), privatekey);
}

static CFDictionaryRef SecCurve25519PrivateKeyCopyAttributeDictionary(SecKeyRef key) {
    /* Export the full ec key pair. */
    CFDataRef fullKeyBlob = SecCurve25519PrivateKeyCopyExternalRepresentation(key, NULL);

    CFDictionaryRef dict = SecKeyGeneratePrivateAttributeDictionary(key, isEd25519Key(key) ? kSecAttrKeyTypeEd25519 : kSecAttrKeyTypeX25519, fullKeyBlob);
	CFReleaseSafe(fullKeyBlob);
	return dict;
}
static CFStringRef SecCurve25519PrivateKeyCopyKeyDescription(SecKeyRef key) {
    const char* curve = getCurveName(key);
	return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR( "<SecKeyRef curve type: %s, algorithm id: %lu, key type: %s, version: %d, block size: %zu bits, addr: %p>"), curve, (long)SecKeyGetAlgorithmId(key), key->key_class->name, key->key_class->version, (8*SecKeyGetBlockSize(key)), key);
}

static CFDataRef SecCurve25519KeyCopyUnwrapKey(SecKeyRef key, SecKeyWrapType type, CFDataRef wrappedKey, CFDictionaryRef parameters, CFDictionaryRef *outParam, CFErrorRef *error)
{
    SecError(errSecUnsupportedOperation, error, CFSTR("unsupported curve"));
    return NULL;
}

static SecKeyDescriptor kSecEd25519PrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "Ed25519PrivateKey",
    .extraBytes = sizeof(ccec25519secretkey),

    .init = SecCurve25519PrivateKeyInit,
    .destroy = SecCurve25519PrivateKeyDestroy,
    .blockSize = SecCurve25519PrivateKeyBlockSize,
    .copyDictionary = SecCurve25519PrivateKeyCopyAttributeDictionary,
    .describe = SecCurve25519PrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve25519KeyGetAlgorithmID,
    .copyPublic = SecCurve25519PrivateKeyCopyPublicOctets,
    .copyExternalRepresentation = SecCurve25519PrivateKeyCopyExternalRepresentation,
    .copyWrapKey = SecCurve25519KeyCopyWrapKey,
    .copyUnwrapKey = SecCurve25519KeyCopyUnwrapKey,
    .copyOperationResult = SecCurve25519PrivateKeyCopyOperationResult,
};

static SecKeyDescriptor kSecX25519PrivateKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "X25519PrivateKey",
    .extraBytes = sizeof(ccec25519secretkey),

    .init = SecCurve25519PrivateKeyInit,
    .destroy = SecCurve25519PrivateKeyDestroy,
    .blockSize = SecCurve25519PrivateKeyBlockSize,
    .copyDictionary = SecCurve25519PrivateKeyCopyAttributeDictionary,
    .describe = SecCurve25519PrivateKeyCopyKeyDescription,
    .getAlgorithmID = SecCurve25519KeyGetAlgorithmID,
    .copyPublic = SecCurve25519PrivateKeyCopyPublicOctets,
    .copyExternalRepresentation = SecCurve25519PrivateKeyCopyExternalRepresentation,
    .copyWrapKey = SecCurve25519KeyCopyWrapKey,
    .copyUnwrapKey = SecCurve25519KeyCopyUnwrapKey,
    .copyOperationResult = SecCurve25519PrivateKeyCopyOperationResult,
};

/* Private Key functions called from SecKey.m */
SecKeyRef SecKeyCreateEd25519PrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecEd25519PrivateKeyDescriptor, keyData,
        keyDataLength, encoding);
}

SecKeyRef SecKeyCreateX25519PrivateKey(CFAllocatorRef allocator,
    const uint8_t *keyData, CFIndex keyDataLength,
    SecKeyEncoding encoding) {
    return SecKeyCreate(allocator, &kSecX25519PrivateKeyDescriptor, keyData,
        keyDataLength, encoding);
}

static OSStatus curve25519KeyGeneratePair(CFDictionaryRef parameters,
                                          SecKeyRef *publicKey, SecKeyRef *privateKey, CFIndex algorithm) {
    OSStatus status = errSecParam;

    CFAllocatorRef allocator = SecCFAllocatorZeroize(); /* @@@ get from parameters. */
    SecKeyRef pubKey = NULL;
    
    const SecKeyDescriptor *privKeyDescriptor = (algorithm == kSecEd25519AlgorithmID) ? &kSecEd25519PrivateKeyDescriptor : &kSecX25519PrivateKeyDescriptor;

    const SecKeyDescriptor *pubKeyDescriptor = (algorithm == kSecEd25519AlgorithmID) ? &kSecEd25519PublicKeyDescriptor : &kSecX25519PublicKeyDescriptor;

    SecKeyRef privKey = SecKeyCreate(allocator, privKeyDescriptor, (const void*) parameters, 0, kSecGenerateKey);
    require_quiet(privKey, errOut);

    /* Create SecKeyRef's from the private key. */
    pubKey = SecKeyCreate(allocator, pubKeyDescriptor,
                          privKey->key, sizeof(ccec25519secretkey), kSecExtractPublicFromPrivate);

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

OSStatus SecEd25519KeyGeneratePair(CFDictionaryRef parameters,
                              SecKeyRef *publicKey, SecKeyRef *privateKey) {
    return curve25519KeyGeneratePair(parameters, publicKey, privateKey, kSecEd25519AlgorithmID);
}

OSStatus SecX25519KeyGeneratePair(CFDictionaryRef parameters,
                                  SecKeyRef *publicKey, SecKeyRef *privateKey) {
    return curve25519KeyGeneratePair(parameters, publicKey, privateKey, kSecX25519AlgorithmID);
}

static bool isEd25519Key(SecKeyRef key) {
    return key->key_class == &kSecEd25519PublicKeyDescriptor || key->key_class == &kSecEd25519PrivateKeyDescriptor;
}

