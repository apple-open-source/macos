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
} SecCTKKeyData;

static void SecCTKKeyDestroy(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    CFReleaseSafe(kd->token);
    CFReleaseSafe(kd->token_id);
    CFReleaseSafe(kd->objectID);
    CFReleaseSafe(kd->auth_params.mutable_dictionary);
    CFReleaseSafe(kd->attributes.mutable_dictionary);
}

static CFIndex SecCTKGetAlgorithmID(SecKeyRef key) {
    SecCTKKeyData *kd = key->key;
    if (CFEqualSafe(CFDictionaryGetValue(kd->attributes.dictionary, kSecAttrKeyType), kSecAttrKeyTypeEC))
        return kSecECDSAAlgorithmID;
    else
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

static OSStatus SecCTKKeyRawSign(SecKeyRef key, SecPadding padding,
                                 const uint8_t *dataToSign, size_t dataToSignLen,
                                 uint8_t *sig, size_t *sigLen) {
    OSStatus status = errSecSuccess;
    CFDataRef digest = CFDataCreateWithBytesNoCopy(NULL, dataToSign, dataToSignLen, kCFAllocatorNull);

    SecCTKKeyData *kd = key->key;
    __block SecCFDictionaryCOW sign_auth_params = { kd->auth_params.dictionary };
    __block TKTokenRef token = CFRetainSafe(kd->token);

    status = SecOSStatusWith(^bool(CFErrorRef *error) {
        return SecItemAuthDo(&sign_auth_params, error, ^SecItemAuthResult(CFDictionaryRef auth_params, CFArrayRef *ac_pairs, CFErrorRef *error) {
            CFDataRef signature = NULL;
            SecItemAuthResult auth_result = kSecItemAuthResultOK;

            if (sign_auth_params.mutable_dictionary != NULL) {
                // auth_params were modified, so reconnect the token in order to update the attributes.
                TKTokenRef new_token = NULL;
                require_quiet(new_token = SecTokenCreate(kd->token_id, auth_params, error), out);
                CFAssignRetained(token, new_token);
            }

            require_action_quiet(signature = TKTokenCopySignature(token, kd->objectID, padding, digest, error), out,
                                 auth_result = SecCTKProcessError(kAKSKeyOpSign, token, kd->objectID, ac_pairs, error));
            require_action_quiet((CFIndex)*sigLen >= CFDataGetLength(signature), out,
                                 SecError(errSecParam, error, CFSTR("signature buffer too small (%ulb required)"),
                                          (unsigned long)CFDataGetLength(signature)));
            *sigLen = CFDataGetLength(signature);
            CFDataGetBytes(signature, CFRangeMake(0, *sigLen), sig);
            *sigLen = CFDataGetLength(signature);

        out:
            CFReleaseSafe(signature);
            return auth_result;
        });
    });

    CFReleaseSafe(sign_auth_params.mutable_dictionary);
    CFReleaseSafe(digest);
    CFReleaseSafe(token);
    return status;
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

SecKeyDescriptor kSecCTKKeyDescriptor = {
    kSecKeyDescriptorVersion,
    "CTKKey",
    sizeof(SecCTKKeyData),
    NULL, // SecKeyInit
    SecCTKKeyDestroy,
    SecCTKKeyRawSign,
    NULL, // SecKeyRawVerifyMethod
    NULL, // SecKeyEncryptMethod
    NULL, // SecKeyRawDecrypt
    NULL, // SecKeyComputeMethod
    SecCTKKeyBlockSize,
    SecCTKKeyCopyAttributeDictionary,
    SecCTKKeyCopyKeyDescription,
    SecCTKGetAlgorithmID,
    SecCTKKeyCopyPublicOctets,
    NULL, // SecKeyCopyWrapKey
    NULL, // SecKeyCopyUnwrapKey
};

SecKeyRef SecKeyCreateCTKKey(CFAllocatorRef allocator, CFDictionaryRef refAttributes) {
    SecKeyRef key = SecKeyCreate(allocator, &kSecCTKKeyDescriptor, 0, 0, 0);
    SecCTKKeyData *kd = key->key;
    kd->token = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecUseToken));
    kd->objectID = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecUseTokenObjectID));
    kd->token_id = CFRetainSafe(CFDictionaryGetValue(refAttributes, kSecAttrTokenID));
    kd->attributes.dictionary = refAttributes;
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecUseToken);
    CFDictionaryRemoveValue(SecCFDictionaryCOWGetMutable(&kd->attributes), kSecUseTokenObjectID);
    SecItemAuthCopyParams(&kd->auth_params, &kd->attributes);
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
