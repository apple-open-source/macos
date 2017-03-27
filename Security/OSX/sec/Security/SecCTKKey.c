/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#include <Security/SecFramework.h>
#include <Security/SecKeyPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecBasePriv.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <ctkclient.h>
#include <libaks_acl_cf_keys.h>

#include "SecECKey.h"
#include "SecRSAKey.h"
#include "SecCTKKeyPriv.h"

const CFStringRef kSecUseToken = CFSTR("u_Token");
const CFStringRef kSecUseTokenObjectID = CFSTR("u_TokenOID");

typedef struct {
    TKTokenRef token;
    CFStringRef token_id;
    CFDataRef objectID;
    SecCFDictionaryCOW auth_params;
    SecCFDictionaryCOW attributes;
    CFMutableDictionaryRef params;
} SecCTKKeyData;

static void SecCTKKeyDestroy(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    CFReleaseSafe(kd->token);
    CFReleaseSafe(kd->token_id);
    CFReleaseSafe(kd->objectID);
    CFReleaseSafe(kd->auth_params.mutable_dictionary);
    CFReleaseSafe(kd->attributes.mutable_dictionary);
    CFReleaseSafe(kd->params);
}

static CFIndex SecCTKGetAlgorithmID(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    if (CFEqualSafe(CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeyType), kSecAttrKeyTypeECSECPrimeRandom)) {
        return kSecECDSAAlgorithmID;
    }
    return kSecRSAAlgorithmID;
}

static SecItemAuthResult SecCTKProcessError(CFStringRef operation, TKTokenRef token, CFDataRef object_id, CFArrayRef *ac_pairs, CFErrorRef *error) {
    if (CFEqualSafe(CFErrorGetDomain(*error), CFSTR(kTKErrorDomain)) &&
        CFErrorGetCode(*error) == kTKErrorCodeAuthenticationFailed) {
        CFDataRef access_control = TKTokenCopyObjectAccessControl(token, object_id, error);
        if (access_control != NULL) {
            CFArrayRef ac_pair = CFArrayCreateForCFTypes(NULL, access_control, operation, NULL);
            CFAssignRetained(*ac_pairs, CFArrayCreateForCFTypes(NULL, ac_pair, NULL));

            CFReleaseNull(*error);
            CFRelease(ac_pair);
            CFRelease(access_control);
            return kSecItemAuthResultNeedAuth;
        }
    }
    return kSecItemAuthResultError;
}

static const CFTypeRef *aclOperations[] = {
    [kSecKeyOperationTypeSign] = &kAKSKeyOpSign,
    [kSecKeyOperationTypeDecrypt] = &kAKSKeyOpDecrypt,
    [kSecKeyOperationTypeKeyExchange] = &kAKSKeyOpComputeKey,
};

static CFTypeRef SecCTKKeyCopyOperationResult(SecKeyRef key, SecKeyOperationType operation, SecKeyAlgorithm algorithm,
                                              CFArrayRef algorithms, SecKeyOperationMode mode,
                                              CFTypeRef in1, CFTypeRef in2, CFErrorRef *error) {
    SecCTKKeyData *kd = key->key;
    __block SecCFDictionaryCOW auth_params = { kd->auth_params.dictionary };
    __block TKTokenRef token = CFRetainSafe(kd->token);
    __block CFTypeRef result = kCFNull;

    CFErrorRef localError = NULL;
    SecItemAuthDo(&auth_params, &localError, ^SecItemAuthResult(CFDictionaryRef ap, CFArrayRef *ac_pairs, CFErrorRef *error) {
        if (auth_params.mutable_dictionary != NULL || token == NULL || kd->params != NULL) {
            // token was not connected yet or auth_params were modified, so reconnect the token in order to update the attributes.
            SecCFDictionaryCOW attributes = { ap };
            if (kd->params && CFDictionaryGetCount(kd->params) > 0) {
                CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&attributes), CFSTR(kTKTokenCreateAttributeAuxParams), kd->params);
            }
            CFAssignRetained(token, SecTokenCreate(kd->token_id, attributes.dictionary, error));
            CFReleaseSafe(attributes.mutable_dictionary);
            if (token == NULL) {
                return kSecItemAuthResultError;
            }
        }

        result = TKTokenCopyOperationResult(token, kd->objectID, operation, algorithms, mode, in1, in2, error);
        return (result != NULL) ? kSecItemAuthResultOK : SecCTKProcessError(*aclOperations[operation], token,
                                                                            kd->objectID, ac_pairs, error);
    });

    CFErrorPropagate(localError, error);
    CFReleaseSafe(auth_params.mutable_dictionary);
    CFReleaseSafe(token);
    return result;
}

static size_t SecCTKKeyBlockSize(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    CFTypeRef keySize = CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeySizeInBits);
    if (CFGetTypeID(keySize) == CFNumberGetTypeID()) {
        CFIndex bitSize;
        if (CFNumberGetValue(keySize, kCFNumberCFIndexType, &bitSize))
            return (bitSize + 7) / 8;
    }

    return 0;
}

static OSStatus SecCTKKeyCopyPublicOctets(SecKeyRef key, CFDataRef *data) {
    OSStatus status = errSecSuccess;
    CFErrorRef error = NULL;
    CFDataRef publicData = NULL;

    SecCTKKeyData *kd = key->key;
    require_action_quiet(publicData = TKTokenCopyPublicKeyData(kd->token, kd->objectID, &error), out,
                         status = SecErrorGetOSStatus(error));
    *data = publicData;

out:
    CFReleaseSafe(error);
    return status;
}

static CFStringRef SecCTKKeyCopyKeyDescription(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecKeyRef:('%@') %p>"),
                                    CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrTokenID), key);
}

// Attributes allowed to be exported from all internal key attributes.
static const CFStringRef *kSecExportableCTKKeyAttributes[] = {
    &kSecClass,
    &kSecAttrTokenID,
    &kSecAttrKeyClass,
    &kSecAttrIsPermanent,
    &kSecAttrIsPrivate,
    &kSecAttrIsModifiable,
    &kSecAttrKeyType,
    &kSecAttrKeySizeInBits,
    &kSecAttrEffectiveKeySize,
    &kSecAttrIsSensitive,
    &kSecAttrWasAlwaysSensitive,
    &kSecAttrIsExtractable,
    &kSecAttrWasNeverExtractable,
    &kSecAttrCanEncrypt,
    &kSecAttrCanDecrypt,
    &kSecAttrCanDerive,
    &kSecAttrCanSign,
    &kSecAttrCanVerify,
    &kSecAttrCanSignRecover,
    &kSecAttrCanVerifyRecover,
    &kSecAttrCanWrap,
    &kSecAttrCanUnwrap,
    NULL
};

static CFDictionaryRef SecCTKKeyCopyAttributeDictionary(SecKeyRef key) {
    CFMutableDictionaryRef attrs = NULL;
    CFErrorRef error = NULL;
    CFDataRef publicData = NULL, digest = NULL;
    SecCTKKeyData *kd = key->key;

    // Encode ApplicationLabel as SHA1 digest of public key bytes.
    require_quiet(publicData = TKTokenCopyPublicKeyData(kd->token, kd->objectID, &error), out);

    /* Calculate the digest of the public key. */
    require(digest = SecSHA1DigestCreate(NULL, CFDataGetBytePtr(publicData), CFDataGetLength(publicData)), out);
    attrs = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(key));
    CFDictionarySetValue(attrs, kSecAttrApplicationLabel, digest);

    for (const CFStringRef **attrKey = &kSecExportableCTKKeyAttributes[0]; *attrKey != NULL; attrKey++) {
        CFTypeRef value = CFDictionaryGetValue(kd->attributes.dictionary, **attrKey);
        if (value != NULL) {
            CFDictionarySetValue(attrs, **attrKey, value);
        }
    }

out:
    CFReleaseSafe(error);
    CFReleaseSafe(publicData);
    CFReleaseSafe(digest);
    return attrs;
}

static SecKeyRef SecCTKKeyCreateDuplicate(SecKeyRef key);

static Boolean SecCTKKeySetParameter(SecKeyRef key, CFStringRef name, CFPropertyListRef value, CFErrorRef *error) {
    SecCTKKeyData *kd = key->key;
    CFTypeRef acm_reference = NULL;

    static const CFStringRef *const knownUseFlags[] = {
        &kSecUseOperationPrompt,
        &kSecUseAuthenticationContext,
        &kSecUseAuthenticationUI,
        &kSecUseCallerName,
        &kSecUseCredentialReference,
    };

    // Check, whether name is part of known use flags.
    bool isUseFlag = false;
    for (size_t i = 0; i < array_size(knownUseFlags); i++) {
        if (CFEqual(*knownUseFlags[i], name)) {
            isUseFlag = true;
            break;
        }
    }

    if (CFEqual(name, kSecUseAuthenticationContext)) {
        // Preprocess LAContext to ACMRef value.
        if (value != NULL) {
            require_quiet(acm_reference = SecItemAttributesCopyPreparedAuthContext(value, error), out);
            value = acm_reference;
        }
        name = kSecUseCredentialReference;
    }

    if (isUseFlag) {
        // Release existing token connection to enforce creation of new connection with new auth params.
        CFReleaseNull(kd->token);
        if (value != NULL) {
            CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->auth_params), name, value);
        } else {
            CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->auth_params), name);
        }
    } else {
        if (kd->params == NULL) {
            kd->params = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        if (value != NULL) {
            CFDictionarySetValue(kd->params, name, value);
        } else {
            CFDictionaryRemoveValue(kd->params, name);
        }
    }

out:
    CFReleaseSafe(acm_reference);
    return TRUE;
}

static SecKeyDescriptor kSecCTKKeyDescriptor = {
    .version = kSecKeyDescriptorVersion,
    .name = "CTKKey",
    .extraBytes = sizeof(SecCTKKeyData),

    .destroy = SecCTKKeyDestroy,
    .blockSize = SecCTKKeyBlockSize,
    .copyDictionary = SecCTKKeyCopyAttributeDictionary,
    .describe = SecCTKKeyCopyKeyDescription,
    .getAlgorithmID = SecCTKGetAlgorithmID,
    .copyPublic = SecCTKKeyCopyPublicOctets,
    .copyOperationResult = SecCTKKeyCopyOperationResult,
    .createDuplicate = SecCTKKeyCreateDuplicate,
    .setParameter = SecCTKKeySetParameter,
};

static SecKeyRef SecCTKKeyCreateDuplicate(SecKeyRef key) {
    SecKeyRef result = SecKeyCreate(CFGetAllocator(key), &kSecCTKKeyDescriptor, 0, 0, 0);
    SecCTKKeyData *kd = key->key, *rd = result->key;
    rd->token = CFRetainSafe(kd->token);
    rd->objectID = CFRetainSafe(kd->objectID);
    rd->token_id = CFRetainSafe(kd->token_id);
    if (kd->attributes.dictionary != NULL) {
        rd->attributes.dictionary = kd->attributes.dictionary;
        SecCFDictionaryCOWGetMutable(&rd->attributes);
    }
    if (kd->auth_params.dictionary != NULL) {
        rd->auth_params.dictionary = kd->auth_params.dictionary;
        SecCFDictionaryCOWGetMutable(&rd->auth_params);
    }
    return result;
}

SecKeyRef SecKeyCreateCTKKey(CFAllocatorRef allocator, CFDictionaryRef refAttributes, CFErrorRef *error) {
    SecKeyRef key = SecKeyCreate(allocator, &kSecCTKKeyDescriptor, 0, 0, 0);
    SecCTKKeyData *kd = key->key;
    kd->token = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecUseToken));
    kd->objectID = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecUseTokenObjectID));
    kd->token_id = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecAttrTokenID));
    kd->attributes.dictionary = refAttributes;
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecUseToken);
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecUseTokenObjectID);
    SecItemAuthCopyParams(&kd->auth_params, &kd->attributes);
    if (CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrIsPrivate) == NULL) {
        CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecAttrIsPrivate, kCFBooleanTrue);
    }

    // Convert some attributes which are stored as numbers in iOS keychain but a lot of code counts that the values
    // are actually strings as specified by kSecAttrXxx constants.
    static const CFStringRef *numericAttributes[] = {
        &kSecAttrKeyType,
        &kSecAttrKeyClass,
        NULL,
    };

    for (const CFStringRef **attrName = &numericAttributes[0]; *attrName != NULL; attrName++) {
        CFTypeRef value = CFDictionaryGetValue(kd->attributes.dictionary, **attrName);
        if (value != NULL && CFGetTypeID(value) == CFNumberGetTypeID()) {
            CFIndex number;
            if (CFNumberGetValue(value, kCFNumberCFIndexType, &number)) {
                CFStringRef newValue = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%ld"), (long)number);
                if (newValue != NULL) {
                    CFDictionarySetValue(SecCFDictionaryCOWGetMutable(&kd->attributes), **attrName, newValue);
                    CFRelease(newValue);
                }
            }
        }
    }

    return key;
}

OSStatus SecCTKKeyGeneratePair(CFDictionaryRef parameters, SecKeyRef *publicKey, SecKeyRef *privateKey) {
    OSStatus status;
    CFMutableDictionaryRef attrs = NULL;
    CFDictionaryRef keyAttrs = NULL;
    CFDataRef publicData = NULL;

    require_action_quiet(publicKey != NULL, out, status = errSecParam);
    require_action_quiet(privateKey != NULL, out, status = errSecParam);

    // Simply adding key on the token without value will cause the token to generate the key and automatically
    // add it to the keychain.  Prepare dictionary specifying item to add.
    keyAttrs = CFDictionaryGetValue(parameters, kSecPrivateKeyAttrs);
    attrs = (keyAttrs == NULL) ? CFDictionaryCreateMutableForCFTypes(NULL) : CFDictionaryCreateMutableCopy(NULL, 0, keyAttrs);

    CFDictionaryForEach(parameters, ^(const void *key, const void *value) {
        if (!CFEqual(key, kSecPrivateKeyAttrs) && !CFEqual(key, kSecPublicKeyAttrs)) {
            CFDictionarySetValue(attrs, key, value);
        }
    });
    CFDictionaryRemoveValue(attrs, kSecValueData);
    CFDictionarySetValue(attrs, kSecClass, kSecClassKey);
    CFDictionarySetValue(attrs, kSecAttrKeyClass, kSecAttrKeyClassPrivate);
    CFDictionarySetValue(attrs, kSecReturnRef, kCFBooleanTrue);

    // Add key from given attributes to the token (having no data will cause the token to actually generate the key).
    require_noerr_quiet(status = SecItemAdd(attrs, (CFTypeRef *)privateKey), out);

    // Create non-token public key.
    require_noerr_quiet(status = SecCTKKeyCopyPublicOctets(*privateKey, &publicData), out);
    if (CFEqualSafe(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeEC)) {
        *publicKey = SecKeyCreateECPublicKey(NULL, CFDataGetBytePtr(publicData), CFDataGetLength(publicData),
                                             kSecKeyEncodingBytes);
    } else if (CFEqualSafe(CFDictionaryGetValue(parameters, kSecAttrKeyType), kSecAttrKeyTypeRSA)) {
        *publicKey = SecKeyCreateRSAPublicKey(NULL, CFDataGetBytePtr(publicData), CFDataGetLength(publicData),
                                              kSecKeyEncodingBytes);
    }

    if (*publicKey != NULL) {
        status = errSecSuccess;
    } else {
        status = errSecInvalidKey;
        CFReleaseNull(*privateKey);
    }

out:
    CFReleaseSafe(attrs);
    CFReleaseSafe(publicData);
    return status;
}

SecKeyRef SecKeyCopyAttestationKey(SecKeyAttestationKeyType keyType, CFErrorRef *error) {
    if (keyType != kSecKeyAttestationKeyTypeSIK && keyType != kSecKeyAttestationKeyTypeGID) {
        SecError(errSecParam, error, CFSTR("unexpected attestation key type %u"), (unsigned)keyType);
        return NULL;
    }

    // [NSKeyedArchiver archivedDataWithRootObject:[@"com.apple.setoken.sik" dataUsingEncoding:NSUTF8StringEncoding]];
    static const uint8_t sikObjectIDBytes[] = {
        0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30, 0xd4, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x14,
        0x15, 0x58, 0x24, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x58, 0x24, 0x6f, 0x62, 0x6a, 0x65,
        0x63, 0x74, 0x73, 0x59, 0x24, 0x61, 0x72, 0x63, 0x68, 0x69, 0x76, 0x65, 0x72, 0x54, 0x24, 0x74,
        0x6f, 0x70, 0x12, 0x00, 0x01, 0x86, 0xa0, 0xa3, 0x07, 0x08, 0x0d, 0x55, 0x24, 0x6e, 0x75, 0x6c,
        0x6c, 0xd2, 0x09, 0x0a, 0x0b, 0x0c, 0x57, 0x4e, 0x53, 0x2e, 0x64, 0x61, 0x74, 0x61, 0x56, 0x24,
        0x63, 0x6c, 0x61, 0x73, 0x73, 0x4f, 0x10, 0x15, 0x63, 0x6f, 0x6d, 0x2e, 0x61, 0x70, 0x70, 0x6c,
        0x65, 0x2e, 0x73, 0x65, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x2e, 0x73, 0x69, 0x6b, 0x80, 0x02, 0xd2,
        0x0e, 0x0f, 0x10, 0x11, 0x5a, 0x24, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x6e, 0x61, 0x6d, 0x65, 0x58,
        0x24, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x65, 0x73, 0x5d, 0x4e, 0x53, 0x4d, 0x75, 0x74, 0x61, 0x62,
        0x6c, 0x65, 0x44, 0x61, 0x74, 0x61, 0xa3, 0x10, 0x12, 0x13, 0x56, 0x4e, 0x53, 0x44, 0x61, 0x74,
        0x61, 0x58, 0x4e, 0x53, 0x4f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x5f, 0x10, 0x0f, 0x4e, 0x53, 0x4b,
        0x65, 0x79, 0x65, 0x64, 0x41, 0x72, 0x63, 0x68, 0x69, 0x76, 0x65, 0x72, 0xd1, 0x16, 0x17, 0x54,
        0x72, 0x6f, 0x6f, 0x74, 0x80, 0x01, 0x08, 0x11, 0x1a, 0x23, 0x2d, 0x32, 0x37, 0x3b, 0x41, 0x46,
        0x4e, 0x55, 0x6d, 0x6f, 0x74, 0x7f, 0x88, 0x96, 0x9a, 0xa1, 0xaa, 0xbc, 0xbf, 0xc4, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6
    };

    // [NSKeyedArchiver archivedDataWithRootObject:[@"com.apple.setoken.gid" dataUsingEncoding:NSUTF8StringEncoding]];
    static const uint8_t gidObjectIDBytes[] = {
        0x62, 0x70, 0x6c, 0x69, 0x73, 0x74, 0x30, 0x30, 0xd4, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x14,
        0x15, 0x58, 0x24, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0x58, 0x24, 0x6f, 0x62, 0x6a, 0x65,
        0x63, 0x74, 0x73, 0x59, 0x24, 0x61, 0x72, 0x63, 0x68, 0x69, 0x76, 0x65, 0x72, 0x54, 0x24, 0x74,
        0x6f, 0x70, 0x12, 0x00, 0x01, 0x86, 0xa0, 0xa3, 0x07, 0x08, 0x0d, 0x55, 0x24, 0x6e, 0x75, 0x6c,
        0x6c, 0xd2, 0x09, 0x0a, 0x0b, 0x0c, 0x57, 0x4e, 0x53, 0x2e, 0x64, 0x61, 0x74, 0x61, 0x56, 0x24,
        0x63, 0x6c, 0x61, 0x73, 0x73, 0x4f, 0x10, 0x15, 0x63, 0x6f, 0x6d, 0x2e, 0x61, 0x70, 0x70, 0x6c,
        0x65, 0x2e, 0x73, 0x65, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x2e, 0x67, 0x69, 0x64, 0x80, 0x02, 0xd2,
        0x0e, 0x0f, 0x10, 0x11, 0x5a, 0x24, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x6e, 0x61, 0x6d, 0x65, 0x58,
        0x24, 0x63, 0x6c, 0x61, 0x73, 0x73, 0x65, 0x73, 0x5d, 0x4e, 0x53, 0x4d, 0x75, 0x74, 0x61, 0x62,
        0x6c, 0x65, 0x44, 0x61, 0x74, 0x61, 0xa3, 0x10, 0x12, 0x13, 0x56, 0x4e, 0x53, 0x44, 0x61, 0x74,
        0x61, 0x58, 0x4e, 0x53, 0x4f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x5f, 0x10, 0x0f, 0x4e, 0x53, 0x4b,
        0x65, 0x79, 0x65, 0x64, 0x41, 0x72, 0x63, 0x68, 0x69, 0x76, 0x65, 0x72, 0xd1, 0x16, 0x17, 0x54,
        0x72, 0x6f, 0x6f, 0x74, 0x80, 0x01, 0x08, 0x11, 0x1a, 0x23, 0x2d, 0x32, 0x37, 0x3b, 0x41, 0x46,
        0x4e, 0x55, 0x6d, 0x6f, 0x74, 0x7f, 0x88, 0x96, 0x9a, 0xa1, 0xaa, 0xbc, 0xbf, 0xc4, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc6
    };

    CFDataRef objectID = (keyType == kSecKeyAttestationKeyTypeSIK) ?
                         CFDataCreate(kCFAllocatorDefault, sikObjectIDBytes, sizeof(sikObjectIDBytes)) :
                         CFDataCreate(kCFAllocatorDefault, gidObjectIDBytes, sizeof(gidObjectIDBytes)) ;

    const void *keys[] = { kSecUseToken, kSecUseTokenObjectID, kSecAttrTokenID };
    const void *values[] = { kCFNull, objectID, CFSTR("com.apple.setoken.attest") };

    CFDictionaryRef attributes = CFDictionaryCreate(kCFAllocatorDefault,
                                                    keys, values, sizeof(keys) / sizeof(*keys),
                                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    return SecKeyCreateCTKKey(kCFAllocatorDefault, attributes, error);
}

CFDataRef SecKeyCreateAttestation(SecKeyRef key, SecKeyRef keyToAttest, CFErrorRef *error) {
    if (!key || !keyToAttest) {
        SecError(errSecParam, error, CFSTR("attestation key(s) is NULL"));
        return NULL;
    }

    SecCTKKeyData *attestingKeyData = key->key;
    SecCTKKeyData *keyToAttestData = keyToAttest->key;

    if (key->key_class != &kSecCTKKeyDescriptor) {
        SecError(errSecUnsupportedOperation, error, CFSTR("attestation not supported by key %@"), key);
        return NULL;
    }
    if (keyToAttest->key_class != &kSecCTKKeyDescriptor || CFEqual(keyToAttestData->token, kCFNull)) {
        SecError(errSecUnsupportedOperation, error, CFSTR("attestation not supported for key %@"), keyToAttest);
        return NULL;
    }

    const void *keys[] = {
        CFSTR(kTKTokenControlAttribAttestingKey),
        CFSTR(kTKTokenControlAttribKeyToAttest),
    };
    const void *values[] = {
        attestingKeyData->objectID,
        keyToAttestData->objectID
    };

    CFDictionaryRef attributes = NULL;
    __block CFDictionaryRef outputAttributes = NULL;
    CFDataRef attestationData = NULL;
    __block SecCFDictionaryCOW sign_auth_params = { keyToAttestData->auth_params.dictionary };

    attributes = CFDictionaryCreate(kCFAllocatorDefault, keys, values, sizeof(keys) / sizeof(*keys),
                                    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    SecItemAuthDo(&sign_auth_params, error, ^SecItemAuthResult(CFDictionaryRef auth_params, CFArrayRef *ac_pairs, CFErrorRef *error) {
        outputAttributes = TKTokenControl(keyToAttestData->token, attributes, error);
        return outputAttributes ? kSecItemAuthResultOK : SecCTKProcessError(kAKSKeyOpAttest, keyToAttestData->token, keyToAttestData->objectID, ac_pairs, error);
    });
    require(outputAttributes, out);

    attestationData = CFDictionaryGetValue(outputAttributes, CFSTR(kTKTokenControlAttribAttestationData));
    require_action(attestationData, out, SecError(errSecInternal, error, CFSTR("could not get attestation data")));

    if (CFGetTypeID(attestationData) != CFDataGetTypeID()) {
        SecError(errSecInternal, error, CFSTR("unexpected attestation object type"));
        attestationData = NULL;
    }

    CFRetainSafe(attestationData);

out:
    CFReleaseSafe(attributes);
    CFReleaseSafe(outputAttributes);
    return attestationData;
}
