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

#include <securityd/SecKeybagSupport.h>

#include <securityd/SecItemServer.h>

#if USE_KEYSTORE
#include <IOKit/IOKitLib.h>
#include <libaks.h>
#include <libaks_acl_cf_keys.h>
#include <utilities/der_plist.h>
#include <corecrypto/ccder.h>
#if TARGET_OS_EMBEDDED
#include <MobileKeyBag/MobileKeyBag.h>
#endif
#endif /* USE_KEYSTORE */


/* g_keychain_handle is the keybag handle used for encrypting item in the keychain.
 For testing purposes, it can be set to something other than the default, with SecItemServerSetKeychainKeybag */
#if USE_KEYSTORE
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED
keybag_handle_t g_keychain_keybag = session_keybag_handle;
#else
keybag_handle_t g_keychain_keybag = device_keybag_handle;
#endif
#else /* !USE_KEYSTORE */
keybag_handle_t g_keychain_keybag = 0; /* 0 == device_keybag_handle, constant dictated by AKS */
#endif /* USE_KEYSTORE */

void SecItemServerSetKeychainKeybag(int32_t keybag)
{
    g_keychain_keybag=keybag;
}

void SecItemServerResetKeychainKeybag(void)
{
#if USE_KEYSTORE
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED
    g_keychain_keybag = session_keybag_handle;
#else
    g_keychain_keybag = device_keybag_handle;
#endif
#else /* !USE_KEYSTORE */
    g_keychain_keybag = 0; /* 0 == device_keybag_handle, constant dictated by AKS */
#endif /* USE_KEYSTORE */
}

#if USE_KEYSTORE

static bool hwaes_key_available(void)
{
    keybag_handle_t handle = bad_keybag_handle;
    keybag_handle_t special_handle = bad_keybag_handle;
#if TARGET_OS_MAC && !TARGET_OS_EMBEDDED
    special_handle = session_keybag_handle;
#elif TARGET_OS_EMBEDDED
    special_handle = device_keybag_handle;
#endif
    kern_return_t kr = aks_get_system(special_handle, &handle);
    if (kr != kIOReturnSuccess) {
#if TARGET_OS_EMBEDDED
        /* TODO: Remove this once the kext runs the daemon on demand if
         there is no system keybag. */
        int kb_state = MKBGetDeviceLockState(NULL);
        asl_log(NULL, NULL, ASL_LEVEL_INFO, "AppleKeyStore lock state: %d", kb_state);
#endif
    }
    return true;
}

#else /* !USE_KEYSTORE */

static bool hwaes_key_available(void)
{
	return false;
}

#endif /* USE_KEYSTORE */

/* Wrap takes a 128 - 256 bit key as input and returns output of
 inputsize + 64 bits.
 In bytes this means that a
 16 byte (128 bit) key returns a 24 byte wrapped key
 24 byte (192 bit) key returns a 32 byte wrapped key
 32 byte (256 bit) key returns a 40 byte wrapped key  */
bool ks_crypt(uint32_t operation, keybag_handle_t keybag,
              keyclass_t keyclass, uint32_t textLength, const uint8_t *source, keyclass_t *actual_class, CFMutableDataRef dest, CFErrorRef *error) {
#if USE_KEYSTORE
    kern_return_t kernResult = kIOReturnBadArgument;
    
    int dest_len = (int)CFDataGetLength(dest);
    if (operation == kSecKsWrap) {
        kernResult = aks_wrap_key(source, textLength, keyclass, keybag, CFDataGetMutableBytePtr(dest), &dest_len, actual_class);
    } else if (operation == kSecKsUnwrap) {
        kernResult = aks_unwrap_key(source, textLength, keyclass, keybag, CFDataGetMutableBytePtr(dest), &dest_len);
    }
    
    if (kernResult != KERN_SUCCESS) {
        if ((kernResult == kIOReturnNotPermitted) || (kernResult == kIOReturnNotPrivileged)) {
            /* Access to item attempted while keychain is locked. */
            return SecError(errSecInteractionNotAllowed, error, CFSTR("ks_crypt: %x failed to %s item (class %"PRId32", bag: %"PRId32") Access to item attempted while keychain is locked."),
                            kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag);
        } else if (kernResult == kIOReturnError) {
            /* Item can't be decrypted on this device, ever, so drop the item. */
            return SecError(errSecDecode, error, CFSTR("ks_crypt: %x failed to %s item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                            kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag);
        } else {
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: %x failed to %s item (class %"PRId32", bag: %"PRId32")"),
                            kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag);
        }
    }
    else
        CFDataSetLength(dest, dest_len);    
    return true;
#else /* !USE_KEYSTORE */
    uint32_t dest_len = (uint32_t)CFDataGetLength(dest);
    if (operation == kSecKsWrap) {
        /* The no encryption case. */
        if (dest_len >= textLength + 8) {
            memcpy(CFDataGetMutableBytePtr(dest), source, textLength);
            memset(CFDataGetMutableBytePtr(dest) + textLength, 8, 8);
            CFDataSetLength(dest, textLength + 8);
            *actual_class = keyclass;
        } else
            return SecError(errSecNotAvailable, error, CFSTR("ks_crypt: failed to wrap item (class %"PRId32")"), keyclass);
    } else if (operation == kSecKsUnwrap) {
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
bool ks_crypt_acl(uint32_t operation, keybag_handle_t keybag, keyclass_t keyclass,
                  uint32_t textLength, const uint8_t *source, CFMutableDataRef dest,
                  CFDataRef acl, CFDataRef acm_context, CFDataRef caller_access_groups, CFErrorRef *error) {
    kern_return_t kernResult = kIOReturnBadArgument;
    uint8_t *params = NULL, *der = NULL;
    const uint8_t *access_groups = caller_access_groups?CFDataGetBytePtr(caller_access_groups):NULL;
    size_t params_len = 0, der_len = 0, access_groups_len = caller_access_groups?CFDataGetLength(caller_access_groups):0;
    
    if (operation == kSecKsWrap) {
        aks_operation_optional_params(0, 0, CFDataGetBytePtr(acl), CFDataGetLength(acl), 0, 0, (void**)&params, &params_len);
        kernResult = aks_encrypt(keybag, keyclass, source, textLength, params, params_len, (void**)&der, &der_len);
    } else if (operation == kSecKsUnwrap) {
        aks_operation_optional_params(access_groups, access_groups_len, 0, 0, CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), (void**)&params, &params_len);
        kernResult = aks_decrypt(keybag, source, textLength, params, params_len, (void**)&der, &der_len);
    } else if (operation == kSecKsDelete) {
        aks_operation_optional_params(access_groups, access_groups_len, 0, 0, CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), (void**)&params, &params_len);
        kernResult = aks_delete(keybag, source, textLength, params, params_len);
    }
    
    bool result = false;
    if (kernResult != KERN_SUCCESS) {
        if ((kernResult == kIOReturnNotPermitted) || (kernResult == kIOReturnNotPrivileged)) {
            /* Access to item attempted while keychain is locked. */
            result =  SecError(errSecInteractionNotAllowed, error, CFSTR("ks_crypt_acl: %x failed to %s item (class %"PRId32", bag: %"PRId32") Access to item attempted while keychain is locked."),
                            kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag);
        } else if (kernResult == kIOReturnError) {
            /* Item can't be decrypted on this device, ever, so drop the item. */
            result = SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to %s item (class %"PRId32", bag: %"PRId32") Item can't be decrypted on this device, ever, so drop the item."),
                            kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag);
        } else {
            result = SecError(errSecNotAvailable, error, CFSTR("ks_crypt_acl: %x failed to %s item (class %"PRId32", bag: %"PRId32")"),
                            kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag);
        }
    }
    else {
        if (operation != kSecKsDelete) {
            const uint8_t *value = der;
            if (operation == kSecKsUnwrap) {
                ccder_tag der_tag;
                size_t der_tag_len;
                value = ccder_decode_tag(&der_tag, der, der + der_len);
                value = ccder_decode_len(&der_tag_len, value, der + der_len);
                
                require_action(der_tag == CCDER_OCTET_STRING, out,
                               SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to %s item (class %"PRId32", bag: %"PRId32") Item can't be decrypted due to invalid der tag, so drop the item."),
                                        kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag));
                require_action(der_tag_len == (size_t)((der + der_len) - value), out,
                               SecError(errSecDecode, error, CFSTR("ks_crypt_acl: %x failed to %s item (class %"PRId32", bag: %"PRId32") Item can't be decrypted due to invalid der tag length, so drop the item."),
                                        kernResult, (operation == kSecKsWrap ? "wrap" : "unwrap"), keyclass, keybag));
            }
            
            if(CFDataGetLength(dest) != (der + der_len) - value)
                CFDataSetLength(dest, (der + der_len) - value);
            
            memcpy(CFDataGetMutableBytePtr(dest), value, CFDataGetLength(dest));
        }
        result = true;
    }
    
out:
    if(params)
        free(params);
    if(der)
        free(der);
    return result;
}
#endif

bool use_hwaes(void) {
    static bool use_hwaes;
    static dispatch_once_t check_once;
    dispatch_once(&check_once, ^{
        use_hwaes = hwaes_key_available();
        if (use_hwaes) {
            asl_log(NULL, NULL, ASL_LEVEL_INFO, "using hwaes key");
        } else {
            asl_log(NULL, NULL, ASL_LEVEL_ERR, "unable to access hwaes key");
        }
    });
    return use_hwaes;
}

bool ks_open_keybag(CFDataRef keybag, CFDataRef password, keybag_handle_t *handle, CFErrorRef *error) {
#if USE_KEYSTORE
    kern_return_t kernResult;
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
