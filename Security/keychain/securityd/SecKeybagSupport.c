/*
 * Copyright (c) 2006-2014 Apple Inc. All Rights Reserved.
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
 *  SecKeybagSupport.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include "keychain/securityd/SecKeybagSupport.h"
#include <TargetConditionals.h>

#include "keychain/securityd/SecItemServer.h"

#if USE_KEYSTORE
#include <IOKit/IOKitLib.h>
#include <libaks_acl_cf_keys.h>
#include <utilities/der_plist.h>
#include <corecrypto/ccder.h>
#include <ACMLib.h>
#else /* !USE_KEYSTORE */
#include <utilities/SecInternalReleasePriv.h>
#endif /* USE_KEYSTORE */

#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>

#include "OSX/utilities/SecAKSWrappers.h"
#include <os/feature_private.h>
#include <utilities/der_plist.h>

/* g_keychain_handle is the keybag handle used for encrypting item in the keychain.
 For testing purposes, it can be set to something other than the default, with SecItemServerSetKeychainKeybag */
#if USE_KEYSTORE
static const keybag_handle_t kDefault_keychain_keybag = user_only_keybag_handle;
#else /* !USE_KEYSTORE */
static const keybag_handle_t kDefault_keychain_keybag = 0; /* 0 == device_keybag_handle, constant dictated by AKS */
#endif /* USE_KEYSTORE */

keybag_handle_t g_keychain_keybag = kDefault_keychain_keybag;

const CFStringRef kSecKSKeyData1 = CFSTR("d1");
const CFStringRef kSecKSKeyData2 = CFSTR("d2");

void SecItemServerSetKeychainKeybag(int32_t keybag)
{
    g_keychain_keybag=keybag;
}

void SecItemServerSetKeychainKeybagToDefault(void)
{
    g_keychain_keybag = kDefault_keychain_keybag;
}

static bool merge_der_in_to_data(const void *ed_blob, size_t ed_blob_len, const void *key_blob, size_t key_blob_len, CFMutableDataRef mergedData)
{
    bool ok = false;
    CFDataRef ed_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, ed_blob, ed_blob_len, kCFAllocatorNull);
    CFDataRef key_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, key_blob, key_blob_len, kCFAllocatorNull);

    if (ed_data && key_data) {
        CFDictionaryRef result_dict = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kSecKSKeyData1, ed_data, kSecKSKeyData2, key_data, NULL);

        CFDataSetLength(mergedData, 0);
        CFDataRef der_data = CFPropertyListCreateDERData(kCFAllocatorDefault, result_dict, NULL);
        if (der_data) {
            CFDataAppend(mergedData, der_data);
            CFRelease(der_data);
            ok = CFDataGetLength(mergedData) > 0;
        }
        CFRelease(result_dict);
    }

    CFReleaseSafe(ed_data);
    CFReleaseSafe(key_data);
    return ok;
}

static bool encrypted_data_from_blob(keybag_handle_t keybag, const void *blob_data, size_t blob_data_len, aks_ref_key_t *ref_key, CFDataRef *encrypted_data, CFErrorRef *error)
{
    CFDictionaryRef blob_dict = NULL;
    CFDataRef key_data = NULL;
    CFDataRef ed = NULL;
    bool ok = false;
    int aks_return = kAKSReturnSuccess;
    const uint8_t *der_end = blob_data_len + blob_data;

    blob_data = der_decode_plist(NULL, (CFPropertyListRef*)&blob_dict, NULL, blob_data, der_end);
    if (blob_data == NULL || blob_data != der_end) {
        SecError(errSecDecode, error, CFSTR("encrypted_data_from_blob: NULL 'blob data'"));
        goto out;
    }
    if (CFGetTypeID(blob_dict) != CFDictionaryGetTypeID()) {
        CFStringRef description = CFCopyTypeIDDescription(CFGetTypeID(blob_dict));
        CFStringRef reason = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("encrypted_data_from_blob: Bad object type %@ for 'blob data'"), description);
        SecError(errSecInternal, error, CFSTR("%@"), reason);
        __security_simulatecrash(reason, __sec_exception_code_CorruptItem);
        CFReleaseSafe(description);
        CFReleaseSafe(reason);
        goto out;
    }
    if (!ks_separate_data_and_key(blob_dict, &ed, &key_data)) {
        ed = CFDataCreate(kCFAllocatorDefault, blob_data, blob_data_len);
        key_data = CFRetain(ed);
    }
    require_action_quiet(ed, out, SecError(errSecDecode, error, CFSTR("encrypted_data_from_blob: failed to decode 'encrypted data'")));
    require_action_quiet(key_data, out, SecError(errSecDecode, error, CFSTR("encrypted_data_from_blob: failed to decode 'key data'")));
    require_noerr_action_quiet(aks_return = aks_ref_key_create_with_blob(keybag, CFDataGetBytePtr(key_data), CFDataGetLength(key_data), ref_key), out, SecError(errSecDecode, error, CFSTR("aks_ref_key: failed to create ref key with blob: %x (bag: %"PRId32")"), aks_return, keybag));

    if (encrypted_data)
        *encrypted_data = CFRetain(ed);
    ok = true;

out:
    CFReleaseSafe(blob_dict);
    CFReleaseSafe(key_data);
    CFReleaseSafe(ed);

    return ok;
}

bool ks_crypt_diversify(CFTypeRef operation, keybag_handle_t keybag,
              keyclass_t keyclass, uint32_t textLength, const uint8_t *source, keyclass_t *actual_class, CFMutableDataRef dest, const void *personaId, size_t personaIdLength, CFErrorRef *error) {
#if USE_KEYSTORE
    aks_params_t aks_params = NULL;
    aks_params = aks_params_create(NULL, 0);
    aks_ref_key_t refKey = NULL;
    CFDataRef ed_data = NULL;
    int aksResult = kAKSReturnBadArgument;
    bool result = false;
    uint8_t *der_params = NULL;
    size_t der_params_len = 0;

    if (!aks_params) {
        return SecError(errSecAllocate, error, CFSTR("ks_crypt_diversify: failed to create aks_params"));
    }
    aks_params_set_data(aks_params, aks_params_key_persona_uuid, personaId, personaIdLength);
    aks_params_get_der(aks_params, &der_params, &der_params_len);

    size_t outDerLen = 0;
    void *outDer = NULL;

    if (CFEqual(operation, kAKSKeyOpEncrypt)) {
        require_noerr_quiet(aksResult = aks_ref_key_create(keybag, keyclass, key_type_sym, der_params, der_params_len, &refKey), out);
        require_noerr_quiet(aksResult = aks_ref_key_wrap(refKey, der_params, der_params_len, source, textLength, &outDer, &outDerLen), out);

        size_t key_blob_len;
        const void *key_blob = aks_ref_key_get_blob(refKey, &key_blob_len);
        require_quiet(key_blob, out);

        result = merge_der_in_to_data(outDer, outDerLen, key_blob, key_blob_len, dest);
        if (!result) {
            if (der_params)
                free(der_params);
            aks_params_free(&aks_params);
            return SecError(errSecDecode, error, CFSTR("ks_crypt_diversify: failed to '%s' item (class %"PRId32", bag: %"PRId32") Item can't be encrypted due to merge failed, so drop the item."), "encrypt", keyclass, keybag);
        }
        *actual_class = keyclass;
    } else if (CFEqual(operation, kAKSKeyOpDecrypt) || CFEqual(operation, kAKSKeyOpDelete)) {
        result = encrypted_data_from_blob(keybag, source, textLength, &refKey, &ed_data, error);
        if (!result) {
            if(der_params)
                free(der_params);
            aks_params_free(&aks_params);
            return SecError(errSecDecode, error, CFSTR("ks_crypt_diversify: failed to create ref key with blob because bad data (bag: %"PRId32")"), keybag);
        }
        require_noerr_quiet(aksResult = aks_ref_key_unwrap(refKey, der_params, der_params_len, CFDataGetBytePtr(ed_data), CFDataGetLength(ed_data), &outDer, &outDerLen), out);
        CFPropertyListRef unwrappedKeyData = NULL;
        der_decode_plist(NULL, &unwrappedKeyData, error, outDer, outDer + outDerLen);
        if (CFGetTypeID(unwrappedKeyData) == CFDataGetTypeID()) {
            CFDataSetLength(dest, 0);
            CFDataAppend(dest, unwrappedKeyData);
        }
        CFReleaseSafe(unwrappedKeyData);
    }
out:
    if (der_params) {
        free(der_params);
    }
    if (aks_params) {
        aks_params_free(&aks_params);
    }
    if (outDer) {
        aks_dealloc(outDer, outDerLen);
    }
    if (refKey) {
        aks_ref_key_free(&refKey);
    }
    CFReleaseSafe(ed_data);
    if (aksResult != kAKSReturnSuccess) {
        if ((aksResult == kAKSReturnNoPermission) || (aksResult == kAKSReturnNotPrivileged)) {
            const char *substatus = "";
            if (keyclass == key_class_ck || keyclass == key_class_cku)
                substatus = " (hibernation?)";
            /* Access to item attempted while keychain is locked. */
            return SecError(errSecInteractionNotAllowed, error, CFSTR("ks_crypt_diversify: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") Access to item attempted while keychain is locked%s."),
                            aksResult, operation, keyclass, keybag, substatus);
        } else if (aksResult == kAKSReturnNotFound) {
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt_diversify: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") No key available for class."), aksResult, operation, keyclass, keybag);
        } else if (aksResult == kAKSReturnError || aksResult == kAKSReturnDecodeError) {
            /* Item can't be decrypted on this device, ever, so drop the item. */
            return SecError(errSecDecode, error, CFSTR("ks_crypt_diversify: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                            aksResult, operation, keyclass, keybag);
        } else if (aksResult == kAKSReturnInvalidPersona) {
            /* Persona doesn't exist, item can no longer be encrypted or decrypted on this device, so drop the item. */
            return SecError(errSecDecode, error, CFSTR("ks_crypt_diversify: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") Persona doesn't exist, so drop the item."),
                            aksResult, operation, keyclass, keybag);
        } else {
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt_diversify: %x failed to '%@' item (class %"PRId32", bag: %"PRId32")"),
                            aksResult, operation, keyclass, keybag);
        }
    }
    return true;
#else /* !USE_KEYSTORE */

    uint32_t dest_len = (uint32_t)CFDataGetLength(dest);
    if (CFEqual(operation, kAKSKeyOpEncrypt)) {
        /* The no encryption case. */
        if (dest_len >= textLength + 8) {
            memcpy(CFDataGetMutableBytePtr(dest), source, textLength);
            memset(CFDataGetMutableBytePtr(dest) + textLength, 8, 8);
            CFDataSetLength(dest, textLength + 8);
            *actual_class = keyclass;
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to wrap item (class %"PRId32")"), keyclass);
    } else if (CFEqual(operation, kAKSKeyOpDecrypt) || CFEqual(operation, kAKSKeyOpDelete)) {
        if (dest_len + 8 >= textLength) {
            memcpy(CFDataGetMutableBytePtr(dest), source, textLength - 8);
            CFDataSetLength(dest, textLength - 8);
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to unwrap item (class %"PRId32")"), keyclass);
    }
    return true;
#endif /* USE_KEYSTORE */
}

/* Wrap takes a 128 - 256 bit key as input and returns output of
 inputsize + 64 bits.
 In bytes this means that a
 16 byte (128 bit) key returns a 24 byte wrapped key
 24 byte (192 bit) key returns a 32 byte wrapped key
 32 byte (256 bit) key returns a 40 byte wrapped key  */
bool ks_crypt(CFTypeRef operation, keybag_handle_t keybag, struct backup_keypair* bkp,
              keyclass_t keyclass, uint32_t textLength, const uint8_t *source, keyclass_t *actual_class, CFMutableDataRef dest,
              bool useNewBackupBehavior, CFErrorRef *error) {
#if USE_KEYSTORE
    kern_return_t kernResult = kAKSReturnBadArgument;
    
    int dest_len = (int)CFDataGetLength(dest);
    if (CFEqual(operation, kAKSKeyOpEncrypt)) {
        bool doOldWrapping = true;
        if (useNewBackupBehavior) {
            doOldWrapping = false;
            uint8_t* outBuf = CFDataGetMutableBytePtr(dest);
            size_t outBufLen = dest_len;
            kernResult = aks_kc_backup_wrap_key(keybag, source, textLength, outBuf, &outBufLen);
            if (kernResult == KERN_SUCCESS) {
                dest_len = (int)outBufLen;
                if (actual_class) {
                    // lie & say the actual class matches the input class, even though backup wrapping has no classes
                    *actual_class = keyclass;
                }
            } else {
                secerror("ks_crypt: aks_kc_backup_wrap_key returned %d", kernResult);
            }
        }

        if (doOldWrapping) {
            kernResult = aks_wrap_key(source, textLength, keyclass, keybag, CFDataGetMutableBytePtr(dest), &dest_len, actual_class);
        }
    } else if (CFEqual(operation, kAKSKeyOpDecrypt)) {
        if (bkp != NULL) {
            uint8_t* outBuf = CFDataGetMutableBytePtr(dest);
            size_t outBufLen = dest_len;
            secnotice("ks_crypt", "have a backup_keypair, attempting to use");
            kernResult = aks_kc_backup_unwrap_key(bkp, source, textLength, outBuf, &outBufLen);
            if (kernResult == KERN_SUCCESS) {
                dest_len = (int)outBufLen;
            }
        } else {
            kernResult = aks_unwrap_key(source, textLength, keyclass, keybag, CFDataGetMutableBytePtr(dest), &dest_len);
        }
    } else if (CFEqual(operation, kAKSKeyOpDelete)) {
        kernResult = aks_unwrap_key(source, textLength, keyclass, keybag, CFDataGetMutableBytePtr(dest), &dest_len);
    }
    
    if (kernResult != KERN_SUCCESS) {
        if ((kernResult == kAKSReturnNoPermission) || (kernResult == kAKSReturnNotPrivileged)) {
            const char *substatus = "";
            if (keyclass == key_class_ck || keyclass == key_class_cku)
                substatus = " (hibernation?)";
            /* Access to item attempted while keychain is locked. */
            return SecError(errSecInteractionNotAllowed, error, CFSTR("ks_crypt: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") Access to item attempted while keychain is locked%s."),
                            kernResult, operation, keyclass, keybag, substatus);
        } else if (kernResult == kAKSReturnNotFound) {
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") No key available for class."), kernResult, operation, keyclass, keybag);
        } else if (kernResult == kAKSReturnError || kernResult == kAKSReturnDecodeError) {
            /* Item can't be decrypted on this device, ever, so drop the item. */
            return SecError(errSecDecode, error, CFSTR("ks_crypt: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                            kernResult, operation, keyclass, keybag);
        } else {
            /* Check if it is v8 item with a valid ref key in this logic */
            if (CFEqual(operation, kAKSKeyOpDecrypt) || CFEqual(operation, kAKSKeyOpDelete)) {
                aks_ref_key_t refKey = NULL;
                CFDataRef ed_data = NULL;
                bool result = encrypted_data_from_blob(keybag, source, textLength, &refKey, &ed_data, error);
                CFReleaseSafe(ed_data);
                aks_ref_key_free(&refKey);
                if (result) {
                    /* this is a persona based blob and is incorrectly being processed by ks_crypt */
                    return SecError(errSecDecode, error, CFSTR("ks_crypt: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                                    kernResult, operation, keyclass, keybag);
                }
            }
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: %x failed to '%@' item (class %"PRId32", bag: %"PRId32")"),
                            kernResult, operation, keyclass, keybag);
        }
    }
    else
        CFDataSetLength(dest, dest_len);
    return true;
#else /* !USE_KEYSTORE */

    uint32_t dest_len = (uint32_t)CFDataGetLength(dest);
    if (CFEqual(operation, kAKSKeyOpEncrypt)) {
        /* The no encryption case. */
        if (dest_len >= textLength + 8) {
            memcpy(CFDataGetMutableBytePtr(dest), source, textLength);
            memset(CFDataGetMutableBytePtr(dest) + textLength, 8, 8);
            CFDataSetLength(dest, textLength + 8);
            *actual_class = keyclass;
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to wrap item (class %"PRId32")"), keyclass);
    } else if (CFEqual(operation, kAKSKeyOpDecrypt) || CFEqual(operation, kAKSKeyOpDelete)) {
        if (dest_len + 8 >= textLength) {
            memcpy(CFDataGetMutableBytePtr(dest), source, textLength - 8);
            CFDataSetLength(dest, textLength - 8);
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to unwrap item (class %"PRId32")"), keyclass);
    }
    return true;
#endif /* USE_KEYSTORE */
}

#if USE_KEYSTORE
bool ks_access_control_needed_error(CFErrorRef *error, CFDataRef access_control_data, CFTypeRef operation) {
    if (error == NULL)
        return false;

    if (*error && CFErrorGetCode(*error) != errSecAuthNeeded) {
        // If we already had an error there, just leave it, no access_control specific error is needed here.
        return false;
    }

    // Create new error instance which adds new access control data appended to existing
    CFMutableDictionaryRef user_info;
    if (*error) {
        CFDictionaryRef old_user_info = CFErrorCopyUserInfo(*error);
        user_info = CFDictionaryCreateMutableCopy(NULL, 0, old_user_info);
        CFRelease(old_user_info);
        CFReleaseNull(*error);
    } else {
        user_info = CFDictionaryCreateMutableForCFTypes(NULL);
    }

    if (access_control_data) {
        CFNumberRef key = CFNumberCreateWithCFIndex(NULL, errSecAuthNeeded);
        CFMutableArrayRef acls;
        CFArrayRef old_acls = CFDictionaryGetValue(user_info, key);
        if (old_acls)
            acls = CFArrayCreateMutableCopy(NULL, 0, old_acls);
        else
            acls = CFArrayCreateMutableForCFTypes(NULL);

        CFArrayRef pair = CFArrayCreateForCFTypes(NULL, access_control_data, operation, NULL);
        CFArrayAppendValue(acls, pair);
        CFRelease(pair);

        CFDictionarySetValue(user_info, key, acls);
        CFRelease(key);
        CFRelease(acls);

        *error = CFErrorCreate(NULL, kSecErrorDomain, errSecAuthNeeded, user_info);
    }
    else
        *error = CFErrorCreate(NULL, kSecErrorDomain, errSecAuthNeeded, NULL);

    CFReleaseSafe(user_info);
    return false;
}

bool ks_separate_data_and_key(CFDictionaryRef blob_dict, CFDataRef *ed_data, CFDataRef *key_data)
{
    bool ok = false;
    CFDataRef tmp_ed_data = CFDictionaryGetValue(blob_dict, kSecKSKeyData1);
    CFDataRef tmp_key_data = CFDictionaryGetValue(blob_dict, kSecKSKeyData2);

    if (tmp_ed_data && tmp_key_data &&
        CFDataGetTypeID() == CFGetTypeID(tmp_ed_data) &&
        CFDataGetTypeID() == CFGetTypeID(tmp_key_data)) {
        *ed_data = CFRetain(tmp_ed_data);
        *key_data = CFRetain(tmp_key_data);
        ok = true;
    }

    return ok;
}

bool create_cferror_from_aks(int aks_return, CFTypeRef operation, keybag_handle_t keybag, keyclass_t keyclass, CFDataRef access_control_data, CFDataRef acm_context_data, CFErrorRef *error)
{
    const char *operation_string = "";
    if (CFEqual(operation, kAKSKeyOpDecrypt)) {
        operation_string = "decrypt";
    } else if (CFEqual(operation, kAKSKeyOpEncrypt)) {
        operation_string = "encrypt";
    } if (CFEqual(operation, kAKSKeyOpDelete)) {
        operation_string = "delete";
    }

    if (aks_return == kAKSReturnNoPermission) {
        /* Keychain is locked. */
        SecError(errSecInteractionNotAllowed, error, CFSTR("aks_ref_key: %x failed to '%s' item (class %"PRId32", bag: %"PRId32") Access to item attempted while keychain is locked."),
                           aks_return, operation_string, keyclass, keybag);
    } else if (aks_return == kAKSReturnPolicyError || aks_return == kAKSReturnBadPassword) {
        if (aks_return == kAKSReturnBadPassword) {
            if (acm_context_data) {
                ACMContextRef acm_context_ref = NULL;
                acm_context_ref = ACMContextCreateWithExternalForm(CFDataGetBytePtr(acm_context_data), CFDataGetLength(acm_context_data));
                if (acm_context_ref) {
                    ACMContextRemovePassphraseCredentialsByPurposeAndScope(acm_context_ref, kACMPassphrasePurposeGeneral, kACMScopeContext);
                    ACMContextDelete(acm_context_ref, false);
                }
            }
        }

        /* Item needed authentication. */
        ks_access_control_needed_error(error, access_control_data, operation);
    } else if (aks_return == kAKSReturnError || aks_return == kAKSReturnPolicyInvalid || aks_return == kAKSReturnDecodeError) {
        /* Item can't be decrypted on this device, ever, so drop the item. */
        SecError(errSecDecode, error, CFSTR("aks_ref_key: %x failed to '%s' item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                          aks_return, operation_string, keyclass, keybag);

    } else if (aks_return == kAKSReturnNotFound) {
        return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: %x failed to '%@' item (class %"PRId32", bag: %"PRId32") No key available for class."), aks_return, operation, keyclass, keybag);
    } else {
        SecError(errSecNotAvailable, error, CFSTR("aks_ref_key: %x failed to '%s' item (class %"PRId32", bag: %"PRId32")"),
                          aks_return, operation_string, keyclass, keybag);
    }

    return false;
}

bool ks_encrypt_acl(keybag_handle_t keybag, keyclass_t keyclass, uint32_t textLength, const uint8_t *source,
                    CFMutableDataRef dest, CFDataRef auth_data, CFDataRef acm_context,
                    SecAccessControlRef access_control, CFErrorRef *error) {
    void *params = NULL, *der = NULL;
    size_t params_len = 0, der_len = 0;
    CFDataRef access_control_data = SecAccessControlCopyData(access_control);
    int aks_return = kAKSReturnSuccess;
    aks_ref_key_t key_handle = NULL;

    /* Verify that we have credential handle, otherwise generate proper error containing ACL and operation requested. */
    bool ok = false;
    if (!acm_context || !SecAccessControlIsBound(access_control)) {
        require_quiet(ok = ks_access_control_needed_error(error, access_control_data, SecAccessControlIsBound(access_control) ? kAKSKeyOpEncrypt : CFSTR("")), out);
    }

    aks_operation_optional_params(0, 0, CFDataGetBytePtr(auth_data), CFDataGetLength(auth_data), CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), &params, &params_len);
    require_noerr_action_quiet(aks_return = aks_ref_key_create(keybag, keyclass, key_type_sym, params, params_len, &key_handle), out,
                               create_cferror_from_aks(aks_return, kAKSKeyOpEncrypt, keybag, keyclass, access_control_data, acm_context, error));
    require_noerr_action_quiet(aks_return = aks_ref_key_encrypt(key_handle, params, params_len, source, textLength, &der, &der_len), out,
                               create_cferror_from_aks(aks_return, kAKSKeyOpEncrypt, keybag, keyclass, access_control_data, acm_context, error));
    size_t key_blob_len;
    const void *key_blob = aks_ref_key_get_blob(key_handle, &key_blob_len);
    require_action_quiet(key_blob, out, SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to '%s' item (class %"PRId32", bag: %"PRId32") Item can't be encrypted due to invalid key data, so drop the item."),
                                           aks_return, "encrypt", keyclass, keybag));

    require_action_quiet(merge_der_in_to_data(der, der_len, key_blob, key_blob_len, dest), out,
                   SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to '%s' item (class %"PRId32", bag: %"PRId32") Item can't be encrypted due to merge failed, so drop the item."),
                            aks_return, "encrypt", keyclass, keybag));

    ok = true;

out:
    if (key_handle)
        aks_ref_key_free(&key_handle);
    if(params)
        free(params);
    if(der)
        free(der);
    CFReleaseSafe(access_control_data);
    return ok;
}

bool ks_decrypt_acl(aks_ref_key_t ref_key, CFDataRef encrypted_data, CFMutableDataRef dest,
                  CFDataRef acm_context, CFDataRef caller_access_groups,
                  SecAccessControlRef access_control, CFErrorRef *error) {
    void *params = NULL, *der = NULL;
    const uint8_t *access_groups = caller_access_groups?CFDataGetBytePtr(caller_access_groups):NULL;
    size_t params_len = 0, der_len = 0, access_groups_len = caller_access_groups?CFDataGetLength(caller_access_groups):0;
    CFDataRef access_control_data = SecAccessControlCopyData(access_control);
    int aks_return = kAKSReturnSuccess;

    /* Verify that we have credential handle, otherwise generate proper error containing ACL and operation requested. */
    bool ok = false;
    if (!acm_context) {
        require_quiet(ok = ks_access_control_needed_error(error, NULL, NULL), out);
    }

    aks_operation_optional_params(access_groups, access_groups_len, 0, 0, CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), &params, &params_len);
    require_noerr_action_quiet(aks_return = aks_ref_key_decrypt(ref_key, params, params_len, CFDataGetBytePtr(encrypted_data), CFDataGetLength(encrypted_data), &der, &der_len), out,
                               create_cferror_from_aks(aks_return, kAKSKeyOpDecrypt, 0, 0, access_control_data, acm_context, error));
    require_action_quiet(der, out, SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to '%s' item, Item can't be decrypted due to invalid der data, so drop the item."),
                                                  aks_return, "decrypt"));

    CFPropertyListRef decoded_data = NULL;
    der_decode_plist(kCFAllocatorDefault, &decoded_data, NULL, der, der + der_len);
    require_action_quiet(decoded_data, out, SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to '%s' item, Item can't be decrypted due to failed decode der, so drop the item."),
                                                           aks_return, "decrypt"));
    if (CFGetTypeID(decoded_data) == CFDataGetTypeID()) {
        CFDataSetLength(dest, 0);
        CFDataAppend(dest, decoded_data);
        CFRelease(decoded_data);
    }
    else {
        CFRelease(decoded_data);
        require_action_quiet(false, out, SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to '%s' item, Item can't be decrypted due to wrong data, so drop the item."),
                                                        aks_return, "decrypt"));
    }

    ok = true;

out:
    if(params)
        free(params);
    if(der)
        free(der);
    CFReleaseSafe(access_control_data);
    return ok;
}

bool ks_delete_acl(aks_ref_key_t ref_key, CFDataRef encrypted_data, 
                  CFDataRef acm_context, CFDataRef caller_access_groups,
                  SecAccessControlRef access_control, CFErrorRef *error) {
    void *params = NULL;
    CFDataRef access_control_data = NULL;
    int aks_return = kAKSReturnSuccess;
    bool ok = false;

    nrequire_action_quiet(CFEqualSafe(SecAccessControlGetConstraint(access_control, kAKSKeyOpDelete), kCFBooleanTrue), out, ok = true);

    /* Verify that we have credential handle, otherwise generate proper error containing ACL and operation requested. */
    if (!acm_context) {
        require_quiet(ok = ks_access_control_needed_error(error, NULL, NULL), out);
    }

    access_control_data = SecAccessControlCopyData(access_control);
    const uint8_t *access_groups = caller_access_groups?CFDataGetBytePtr(caller_access_groups):NULL;
    size_t params_len = 0, access_groups_len = caller_access_groups?CFDataGetLength(caller_access_groups):0;
    aks_operation_optional_params(access_groups, access_groups_len, 0, 0, CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), &params, &params_len);
    require_noerr_action_quiet(aks_return = aks_ref_key_delete(ref_key, params, params_len), out,
                               create_cferror_from_aks(aks_return, kAKSKeyOpDelete, 0, 0, access_control_data, acm_context, error));

    ok = true;

out:
    if(params)
        free(params);
    CFReleaseSafe(access_control_data);
    return ok;
}

const void* ks_ref_key_get_external_data(keybag_handle_t keybag, CFDataRef key_data, aks_ref_key_t *ref_key, size_t *external_data_len, CFErrorRef *error) {
    int aks_return = aks_ref_key_create_with_blob(keybag, CFDataGetBytePtr(key_data), CFDataGetLength(key_data), ref_key);
    if (aks_return == kAKSReturnBadArgument) {
        SecError(errSecDecode, error, CFSTR("aks_ref_key: failed to create ref key with blob because bad data (bag: %"PRId32")"), keybag);
        return NULL;
    // As of this writing the only other error code is kAKSReturnInternalError but we don't want to rely on that
    } else if (aks_return != kAKSReturnSuccess) {
        SecError(errSecDecode, error, CFSTR("aks_ref_key: failed to create ref key with blob: %x (bag: %"PRId32")"), aks_return, keybag);
        return NULL;
    }
    return aks_ref_key_get_external_data(*ref_key, external_data_len);
}
#endif

bool use_hwaes(void) {
#if !TARGET_OS_BRIDGE
    static bool use_hwaes;
    static dispatch_once_t check_once;
    dispatch_once(&check_once, ^{
        use_hwaes = hwaes_key_available();
        if (use_hwaes) {
            secinfo("aks", "using hwaes key");
        } else {
            secerror("unable to access hwaes key");
        }
    });
    return use_hwaes;
#else
    return false;
#endif // TARGET_OS_BRIDGE
}

bool ks_open_keybag(CFDataRef keybag, CFDataRef password, keybag_handle_t *handle, CFErrorRef *error) {
#if USE_KEYSTORE
    kern_return_t kernResult;
    if (!asData(keybag, error)) return false;
    kernResult = aks_load_bag(CFDataGetBytePtr(keybag), (int)CFDataGetLength(keybag), handle);
    if (kernResult)
        return SecKernError(kernResult, error, CFSTR("aks_load_bag failed: %@"), keybag);

    if (password) {
        kernResult = aks_unlock_bag(*handle, CFDataGetBytePtr(password), (int)CFDataGetLength(password));
        if (kernResult) {
            aks_unload_bag(*handle);
            return SecKernError(kernResult, error, CFSTR("aks_unlock_bag failed"));
        }
    }
    return true;
#else /* !USE_KEYSTORE */
    *handle = KEYBAG_NONE;
    return true;
#endif /* USE_KEYSTORE */
}

bool ks_close_keybag(keybag_handle_t keybag, CFErrorRef *error) {
#if USE_KEYSTORE
	IOReturn kernResult = aks_unload_bag(keybag);
    if (kernResult) {
        return SecKernError(kernResult, error, CFSTR("aks_unload_bag failed"));
    }
#endif /* USE_KEYSTORE */
    return true;
}
