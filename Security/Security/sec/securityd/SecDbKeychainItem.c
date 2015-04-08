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
 *  SecDbKeychainItem.c - CoreFoundation-based constants and functions for
    access to Security items (certificates, keys, identities, and
    passwords.)
 */

#include <securityd/SecDbKeychainItem.h>

#include <securityd/SecItemSchema.h>
#include <CommonCrypto/CommonCryptor.h>
#include <CommonCrypto/CommonCryptorSPI.h>
#include <Security/SecBasePriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemInternal.h>
#include <Security/SecRandom.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <utilities/der_plist.h>

#if USE_KEYSTORE
#include <LocalAuthentication/LAPublicDefines.h>
#include <LocalAuthentication/LAPrivateDefines.h>
#include <coreauthd_spi.h>
#include <libaks_acl_cf_keys.h>
#include <securityd/spi.h>
#endif /* USE_KEYSTORE */

pthread_key_t CURRENT_CONNECTION_KEY;

// From SecItemServer, should be a acl-check block
bool itemInAccessGroup(CFDictionaryRef item, CFArrayRef accessGroups);

static keyclass_t kc_parse_keyclass(CFTypeRef value, CFErrorRef *error);
static CFTypeRef kc_encode_keyclass(keyclass_t keyclass);
static CFDataRef kc_copy_protection_data(SecAccessControlRef access_control);
static CFTypeRef kc_copy_protection_from_data(CFDataRef data);
static CFMutableDictionaryRef s3dl_item_v2_decode(CFDataRef plain, CFErrorRef *error);
static CFMutableDictionaryRef s3dl_item_v3_decode(CFDataRef plain, CFErrorRef *error);
#if USE_KEYSTORE
static CFDataRef kc_copy_constraints_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes);
static void kc_dict_from_auth_data(const SecDbClass *class, const uint8_t *der, const uint8_t *der_end, CFMutableDictionaryRef *authenticated_attributes, CFMutableDictionaryRef *acl);
static CFDataRef kc_copy_access_groups_data(CFArrayRef access_groups, CFErrorRef *error);
#endif
    
/* Given plainText create and return a CFDataRef containing:
 BULK_KEY = RandomKey()
 version || keyclass|ACL || KeyStore_WRAP(keyclass, BULK_KEY) ||
 AES(BULK_KEY, NULL_IV, plainText || padding)
 */
bool ks_encrypt_data(keybag_handle_t keybag, SecAccessControlRef access_control, CFTypeRef *cred_handle,
                     CFDictionaryRef attributes, CFDictionaryRef authenticated_attributes, CFDataRef *pBlob, CFErrorRef *error) {
    CFMutableDataRef blob = NULL;
    CFDataRef ac_data = NULL;
    bool ok = true;
    //check(keybag >= 0);

    /* Precalculate output blob length. */
    const uint32_t bulkKeySize = 32; /* Use 256 bit AES key for bulkKey. */
    const uint32_t maxKeyWrapOverHead = 8 + 32;
    uint8_t bulkKey[bulkKeySize];
    CFMutableDataRef bulkKeyWrapped = CFDataCreateMutable(NULL, 0);
    CFDataSetLength(bulkKeyWrapped, bulkKeySize + maxKeyWrapOverHead);
    uint32_t key_wrapped_size;
    
#if USE_KEYSTORE
    CFDataRef access_control_data = NULL;
    CFDataRef constraint_data = NULL;
    CFDataRef acm_context = NULL;
#endif
    
    /* If access_control specifies only protection and no ACL, use legacy blob format version 3,
     which has better support for sync/backup.  Otherwise, force new format v4. */
    const uint32_t version = SecAccessControlGetConstraints(access_control) ? 4 : 3;
    CFDataRef plainText = NULL;
    if (version < 4) {
        CFMutableDictionaryRef attributes_dict = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
        if (authenticated_attributes) {
            CFDictionaryForEach(authenticated_attributes, ^(const void *key, const void *value) {
                CFDictionaryAddValue(attributes_dict, key, value);
            });
        }
        
        if (attributes_dict) {
            // Drop the accc attribute for non v4 items during encode.
            CFDictionaryRemoveValue(attributes_dict, kSecAttrAccessControl);
            plainText = kc_plist_copy_der(attributes_dict, error);
            CFRelease(attributes_dict);
        }
    } else {
#if USE_KEYSTORE
        if (attributes) {
            plainText = kc_plist_copy_der(attributes, error);
        }
#else
        CFMutableDictionaryRef attributes_dict = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
        if (authenticated_attributes) {
            CFDictionaryForEach(authenticated_attributes, ^(const void *key, const void *value) {
                CFDictionaryAddValue(attributes_dict, key, value);
            });
        }
        
        if (attributes_dict) {
            plainText = kc_plist_copy_der(attributes_dict, error);
            CFRelease(attributes_dict);
        }
#endif
    }
    
    if (!plainText || CFGetTypeID(plainText) != CFDataGetTypeID()
        || access_control == 0) {
        ok = SecError(errSecParam, error, CFSTR("ks_encrypt_data: invalid plain text"));
        goto out;
    }

    size_t ptLen = CFDataGetLength(plainText);
    size_t ctLen = ptLen;
    size_t tagLen = 16;
    keyclass_t actual_class;
    
    if (SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey)) {
        ok = SecError(errSecAllocate, error, CFSTR("ks_encrypt_data: SecRandomCopyBytes failed"));
        goto out;
    }

    /* Extract keyclass from access control. */
    keyclass_t keyclass = kc_parse_keyclass(SecAccessControlGetProtection(access_control), error);
    if (!keyclass)
        goto out;

#if USE_KEYSTORE
    if (version >= 4) {
        if (*cred_handle == NULL || isData(*cred_handle)) {
            CFTypeRef auth_ref = VRCreateNewReferenceWithACMContext(*cred_handle, error);
            if (!auth_ref) {
                ok = false;
                goto out;
            }
            CFReleaseSafe(*cred_handle);
            *cred_handle = auth_ref;
        }
        
        access_control_data = SecAccessControlCopyData(access_control);
        CFErrorRef authError = NULL;
        ok = VRValidateACL(*cred_handle, access_control_data, &authError);
        if (!ok) {
            ok = SecCFCreateError(errSecParam, kSecErrorDomain, CFSTR("Invalid ACL"), authError, error);
            CFReleaseSafe(authError);
            goto out;
        }

        constraint_data = kc_copy_constraints_data(access_control, authenticated_attributes);
        require_quiet(ok = ks_crypt_acl(kSecKsWrap, keybag,
                                        keyclass, bulkKeySize, bulkKey, bulkKeyWrapped, constraint_data, NULL, NULL, error), out);
    }
    else {
#endif
    /* Encrypt bulkKey. */
    require_quiet(ok = ks_crypt(kSecKsWrap, keybag,
                                keyclass, bulkKeySize, bulkKey,
                                &actual_class, bulkKeyWrapped,
                                error), out);
#if USE_KEYSTORE
    }
#endif
    
    key_wrapped_size = (uint32_t)CFDataGetLength(bulkKeyWrapped);
    UInt8 *cursor;
    size_t blobLen = sizeof(version);
    uint32_t prot_length;

    if (version == 3) {
        blobLen += sizeof(actual_class);
    } else {
        require_quiet(ac_data = kc_copy_protection_data(access_control), out);
        prot_length = (uint32_t)CFDataGetLength(ac_data);
        blobLen += sizeof(prot_length) + prot_length;
    }

    blobLen += sizeof(key_wrapped_size) + key_wrapped_size + ctLen + tagLen;
    require_quiet(blob = CFDataCreateMutable(NULL, blobLen), out);
    CFDataSetLength(blob, blobLen);
    cursor = CFDataGetMutableBytePtr(blob);

    *((uint32_t *)cursor) = version;
    cursor += sizeof(version);

    //secerror("class: %d actual class: %d", keyclass, actual_class);
    if (version == 3) {
        *((keyclass_t *)cursor) = actual_class;
        cursor += sizeof(keyclass);
    } else {
        *((uint32_t *)cursor) = prot_length;
        cursor += sizeof(prot_length);

        CFDataGetBytes(ac_data, CFRangeMake(0, prot_length), cursor);
        cursor += prot_length;
    }

    *((uint32_t *)cursor) = key_wrapped_size;
    cursor += sizeof(key_wrapped_size);

    memcpy(cursor, CFDataGetBytePtr(bulkKeyWrapped), key_wrapped_size);
    cursor += key_wrapped_size;

    /* Encrypt the plainText with the bulkKey. */
    CCCryptorStatus ccerr = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128,
                                         bulkKey, bulkKeySize,
                                         NULL, 0,  /* iv */
                                         NULL, 0,  /* auth data */
                                         CFDataGetBytePtr(plainText), ptLen,
                                         cursor,
                                         cursor + ctLen, &tagLen);
    if (ccerr) {
        ok = SecError(errSecInternal, error, CFSTR("ks_encrypt_data: CCCryptorGCM failed: %d"), ccerr);
        goto out;
    }
    if (tagLen != 16) {
        ok = SecError(errSecInternal, error, CFSTR("ks_encrypt_data: CCCryptorGCM expected: 16 got: %ld byte tag"), tagLen);
        goto out;
    }

out:
    memset(bulkKey, 0, sizeof(bulkKey));
    CFReleaseSafe(ac_data);
    CFReleaseSafe(bulkKeyWrapped);
    CFReleaseSafe(plainText);
	if (!ok) {
		CFReleaseSafe(blob);
	} else {
		*pBlob = blob;
	}
    
#if USE_KEYSTORE
    CFReleaseSafe(access_control_data);
    CFReleaseSafe(constraint_data);
    CFReleaseSafe(acm_context);
#endif
    return ok;
}

/* Given cipherText containing:
 version || keyclass || KeyStore_WRAP(keyclass, BULK_KEY) ||
 AES(BULK_KEY, NULL_IV, plainText || padding)
 return the plainText. */
bool ks_decrypt_data(keybag_handle_t keybag, enum SecKsCryptoOp cryptoOp, SecAccessControlRef *paccess_control, CFTypeRef *cred_handle,
                     CFDataRef blob, const SecDbClass *db_class, CFArrayRef caller_access_groups,
                     CFMutableDictionaryRef *attributes_p, uint32_t *version_p, CFErrorRef *error) {
    const uint32_t v0KeyWrapOverHead = 8;
    CFMutableDataRef bulkKey = CFDataCreateMutable(0, 32); /* Use 256 bit AES key for bulkKey. */
    CFDataSetLength(bulkKey, 32); /* Use 256 bit AES key for bulkKey. */
    bool ok = true;
    SecAccessControlRef access_control = NULL;
    
    CFMutableDataRef plainText = NULL;
    CFMutableDictionaryRef attributes = NULL;
    uint32_t version = 0;
    CFDataRef access_control_data = NULL;
    
#if USE_KEYSTORE
    CFDataRef acm_context = NULL;
    CFMutableDictionaryRef authenticated_attributes = NULL;
    CFDataRef caller_access_groups_data = NULL;
    
#if TARGET_OS_IPHONE
    check(keybag >= 0);
#else
    check((keybag >= 0) || (keybag == session_keybag_handle));
#endif
#endif

    if (!blob) {
        ok = SecError(errSecParam, error, CFSTR("ks_decrypt_data: invalid blob"));
        goto out;
    }

    size_t blobLen = CFDataGetLength(blob);
    const uint8_t *cursor = CFDataGetBytePtr(blob);
    keyclass_t keyclass;
    uint32_t wrapped_key_size;

    /* Check for underflow, ensuring we have at least one full AES block left. */
    if (blobLen < sizeof(version) + sizeof(keyclass) +
        CFDataGetLength(bulkKey) + v0KeyWrapOverHead + 16) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow"));
        goto out;
    }

    version = *((uint32_t *)cursor);
    cursor += sizeof(version);

    size_t minimum_blob_len = sizeof(version) + 16;
    size_t ctLen = blobLen - sizeof(version);

    if (version == 4) {
        /* Deserialize SecAccessControl object from the blob. */
        uint32_t prot_length = *((uint32_t *)cursor);
        cursor += sizeof(prot_length);

        CFDataRef protection_data = CFDataCreate(kCFAllocatorDefault, cursor, prot_length);
        CFTypeRef protection = kc_copy_protection_from_data(protection_data);
        CFRelease(protection_data);
        if (!protection) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid ACL"));
            goto out;
        }
        else {
            access_control = SecAccessControlCreate(NULL, NULL);
            require_quiet(access_control, out);
            ok = SecAccessControlSetProtection(access_control, protection, NULL);
            CFRelease(protection);
            if (!ok) {
                SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid ACL"));
                goto out;
            }
        }
        
        cursor += prot_length;

        minimum_blob_len += sizeof(prot_length) + prot_length;
        ctLen -= sizeof(prot_length) + prot_length;

        /* Get numeric value of keyclass from the access_control. */
        keyclass = kc_parse_keyclass(SecAccessControlGetProtection(access_control), error);
        if (!keyclass) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid ACL"));
            goto out;
        }
    } else {
        keyclass = *((keyclass_t *)cursor);
	//secerror("class: %d keyclass: %d", keyclass, keyclass & key_class_last);
#if USE_KEYSTORE
        CFTypeRef protection = kc_encode_keyclass(keyclass & key_class_last); // mask out generation
#else
        CFTypeRef protection = kc_encode_keyclass(keyclass);
#endif
        if (protection) {
            access_control = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protection, 0, error);
        }
        if (!access_control) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid keyclass detected"));
            goto out;
        }
        cursor += sizeof(keyclass);

        minimum_blob_len += sizeof(keyclass);
        ctLen -= sizeof(keyclass);
    }

    size_t tagLen = 0;
    switch (version) {
        case 0:
            wrapped_key_size = (uint32_t)CFDataGetLength(bulkKey) + v0KeyWrapOverHead;
            break;
        case 2:
        case 3:
            /* DROPTHROUGH */
            /* v2 and v3 have the same crypto, just different dictionary encodings. */
            /* Difference between v3 and v4 is already handled above, so treat v3 as v4. */
        case 4:
            tagLen = 16;
            minimum_blob_len -= 16; // Remove PKCS7 padding block requirement
            ctLen -= tagLen;        // Remove tagLen from ctLen
            /* DROPTHROUGH */
        case 1:
            wrapped_key_size = *((uint32_t *)cursor);
            cursor += sizeof(wrapped_key_size);
            minimum_blob_len += sizeof(wrapped_key_size);
            ctLen -= sizeof(wrapped_key_size);
            break;
        default:
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid version %d"), version);
            goto out;
    }

    /* Validate key wrap length against total length */
    require(blobLen - minimum_blob_len - tagLen >= wrapped_key_size, out);
    ctLen -= wrapped_key_size;
    if (version < 2 && (ctLen & 0xF) != 0) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid version"));
        goto out;
    }

#if USE_KEYSTORE
    if (version >= 4) {
        /* Verify before we try to unwrap the key. */
        if (*cred_handle == NULL || isData(*cred_handle)) {
            CFTypeRef auth_ref = VRCreateNewReferenceWithACMContext(*cred_handle, error);
            if (!auth_ref) {
                ok = false;
                goto out;
            }
            CFReleaseSafe(*cred_handle);
            *cred_handle = auth_ref;
        }
        
        CFMutableDictionaryRef acl = NULL;
        kc_dict_from_auth_data(db_class, cursor, cursor + wrapped_key_size, &authenticated_attributes, &acl);
        SecAccessControlSetConstraints(access_control, acl);
        CFReleaseSafe(acl);
        access_control_data = SecAccessControlCopyData(access_control);
        
        static CFDictionaryRef hints = NULL;
        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            CFNumberRef noninteractiveKey = CFNumberCreateWithCFIndex(kCFAllocatorDefault, kLAOptionNotInteractive);
            hints = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, noninteractiveKey, kCFBooleanTrue, NULL);
            CFRelease(noninteractiveKey);
        });

        CFErrorRef authError = NULL;
        ok = VREvaluateACL(*cred_handle, access_control_data, (cryptoOp == kSecKsDelete)?kAKSKeyOpDelete:kAKSKeyOpDecrypt, hints, &authError);
        if (!ok) {
            if (CFEqual(CFErrorGetDomain(authError), CFSTR(kLAErrorDomain)) &&
                CFErrorGetCode(authError) == kLAErrorNotInteractive) {
                /* UI is needed, but this is not really an error, just leave with no output data. */
                ok = true;
            } else {
                ok = SecCFCreateError(errSecAuthFailed, kSecErrorDomain, CFSTR("CoreAuthentication failed"), authError, error);
            }
            CFReleaseSafe(authError);
            goto out;
        }
        
        acm_context = VRCopyACMContext(*cred_handle, error);
        
        if (caller_access_groups) {
            caller_access_groups_data = kc_copy_access_groups_data(caller_access_groups, error);
            require_quiet(ok = (caller_access_groups_data != NULL), out);
        }
        require_quiet(ok = ks_crypt_acl(cryptoOp, keybag,
                                        keyclass, wrapped_key_size, cursor, bulkKey, NULL, acm_context, caller_access_groups_data, error), out);
        if (cryptoOp == kSecKsDelete) {
            attributes = CFRetainSafe(authenticated_attributes);
            goto out;
        }
    }
    else {
#endif
        /* Now unwrap the bulk key using a key in the keybag. */
        require_quiet(ok = ks_crypt(kSecKsUnwrap, keybag,
            keyclass, wrapped_key_size, cursor, NULL, bulkKey, error), out);
#if USE_KEYSTORE
    }
#endif
    
    cursor += wrapped_key_size;

    plainText = CFDataCreateMutable(NULL, ctLen);
    if (!plainText) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: failed to allocate data for plain text"));
        goto out;
    }
    CFDataSetLength(plainText, ctLen);

    /* Decrypt the cipherText with the bulkKey. */
    CCCryptorStatus ccerr;
    if (tagLen) {
        uint8_t tag[tagLen];
        ccerr = CCCryptorGCM(kCCDecrypt, kCCAlgorithmAES128,
                             CFDataGetBytePtr(bulkKey), CFDataGetLength(bulkKey),
                             NULL, 0,  /* iv */
                             NULL, 0,  /* auth data */
                             cursor, ctLen,
                             CFDataGetMutableBytePtr(plainText),
                             tag, &tagLen);
        if (ccerr) {
            /* TODO: Should this be errSecDecode once AppleKeyStore correctly
             identifies uuid unwrap failures? */
            /* errSecInteractionNotAllowed; */
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: CCCryptorGCM failed: %d"), ccerr);
            goto out;
        }
        if (tagLen != 16) {
            ok = SecError(errSecInternal, error, CFSTR("ks_decrypt_data: CCCryptorGCM expected: 16 got: %ld byte tag"), tagLen);
            goto out;
        }
        cursor += ctLen;
        if (memcmp(tag, cursor, tagLen)) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: CCCryptorGCM computed tag not same as tag in blob"));
            goto out;
        }
    } else {
        size_t ptLen;
        ccerr = CCCrypt(kCCDecrypt, kCCAlgorithmAES128, kCCOptionPKCS7Padding,
                        CFDataGetBytePtr(bulkKey), CFDataGetLength(bulkKey), NULL, cursor, ctLen,
                        CFDataGetMutableBytePtr(plainText), ctLen, &ptLen);
        if (ccerr) {
            /* TODO: Should this be errSecDecode once AppleKeyStore correctly
             identifies uuid unwrap failures? */
            /* errSecInteractionNotAllowed; */
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: CCCrypt failed: %d"), ccerr);
            goto out;
        }
        CFDataSetLength(plainText, ptLen);
    }
    
    if (version < 2) {
        attributes = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFDictionaryAddValue(attributes, CFSTR("v_Data"), plainText);
    } else if (version < 3) {
        attributes = s3dl_item_v2_decode(plainText, error);
    } else {
        attributes = s3dl_item_v3_decode(plainText, error);
        
#if USE_KEYSTORE
        if (version >= 4 && authenticated_attributes != NULL) {
            CFDictionaryForEach(authenticated_attributes, ^(const void *key, const void *value) {
                CFDictionaryAddValue(attributes, key, value);
            });
        }
#endif
    }
    
    if (!attributes) {
        secerror("decode v%d failed: %@ [item: %@]", version, error ? *error : NULL, plainText);
        ok = false;
    }
out:
    memset(CFDataGetMutableBytePtr(bulkKey), 0, CFDataGetLength(bulkKey));
    CFReleaseSafe(bulkKey);
    CFReleaseSafe(plainText);
    
    // Always copy access control data (if present), because if we fail it may indicate why.
    if (paccess_control)
        *paccess_control = access_control;
    else
        CFReleaseSafe(access_control);
    
    if (ok) {
        if (attributes_p)
            *attributes_p = CFRetainSafe(attributes);
        if (version_p)
            *version_p = version;
	}
    CFReleaseSafe(attributes);
    CFReleaseSafe(access_control_data);
#if USE_KEYSTORE
    CFReleaseSafe(acm_context);
    CFReleaseSafe(authenticated_attributes);
    CFReleaseSafe(caller_access_groups_data);
#endif
    return ok;
}

// TODO: Move to utilities - CFPropertyListCopyDERData()
CFDataRef kc_plist_copy_der(CFPropertyListRef plist, CFErrorRef *error) {
    size_t len = der_sizeof_plist(plist, error);
    CFMutableDataRef encoded = CFDataCreateMutable(0, len);
    CFDataSetLength(encoded, len);
    uint8_t *der_end = CFDataGetMutableBytePtr(encoded);
    const uint8_t *der = der_end;
    der_end += len;
    der_end = der_encode_plist(plist, error, der, der_end);
    if (!der_end) {
        CFReleaseNull(encoded);
    } else {
        assert(!der_end || der_end == der);
    }
    return encoded;
}

static CFDataRef kc_copy_digest(const struct ccdigest_info *di, size_t len,
                                const void *data, CFErrorRef *error) {
    CFMutableDataRef digest = CFDataCreateMutable(0, di->output_size);
    CFDataSetLength(digest, di->output_size);
    ccdigest(di, len, data, CFDataGetMutableBytePtr(digest));
    return digest;
}

CFDataRef kc_copy_sha1(size_t len, const void *data, CFErrorRef *error) {
    return kc_copy_digest(ccsha1_di(), len, data, error);
}

CFDataRef kc_copy_plist_sha1(CFPropertyListRef plist, CFErrorRef *error) {
    CFDataRef der = kc_plist_copy_der(plist, error);
    CFDataRef digest = NULL;
    if (der) {
        digest = kc_copy_sha1(CFDataGetLength(der), CFDataGetBytePtr(der), error);
        CFRelease(der);
    }
    return digest;
}

static keyclass_t kc_parse_keyclass(CFTypeRef value, CFErrorRef *error) {
    if (!isString(value)) {
        SecError(errSecParam, error, CFSTR("accessible attribute %@ not a string"), value);
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlocked)) {
        return key_class_ak;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlock)) {
        return key_class_ck;
    } else if (CFEqual(value, kSecAttrAccessibleAlways)) {
        return key_class_dk;
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)) {
        return key_class_aku;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)) {
        return key_class_cku;
    } else if (CFEqual(value, kSecAttrAccessibleAlwaysThisDeviceOnly)) {
        return key_class_dku;
    } else if (CFEqual(value, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly)) {
        return key_class_akpu;
    } else {
        SecError(errSecParam, error, CFSTR("accessible attribute %@ unknown"), value);
    }
    return 0;
}

static CFTypeRef kc_encode_keyclass(keyclass_t keyclass) {
    switch (keyclass) {
        case key_class_ak:
            return kSecAttrAccessibleWhenUnlocked;
        case key_class_ck:
            return kSecAttrAccessibleAfterFirstUnlock;
        case key_class_dk:
            return kSecAttrAccessibleAlways;
        case key_class_aku:
            return kSecAttrAccessibleWhenUnlockedThisDeviceOnly;
        case key_class_cku:
            return kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
        case key_class_dku:
            return kSecAttrAccessibleAlwaysThisDeviceOnly;
        case key_class_akpu:
            return kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly;
        default:
            return 0;
    }
}

#if USE_KEYSTORE
static void kc_dict_from_auth_data(const SecDbClass *class, const uint8_t *der, const uint8_t *der_end, CFMutableDictionaryRef *authenticated_attributes, CFMutableDictionaryRef *acl)
{
    CFPropertyListRef aks_data = NULL;
    der = der_decode_plist(NULL, kCFPropertyListImmutable, &aks_data, NULL, der, der_end);
    if(der == der_end) {
        CFDictionaryRef authenticated_data = CFDictionaryGetValue(aks_data, kAKSKeyAuthData);
        if (authenticated_data) {
            *acl = CFDictionaryCreateMutableCopy(NULL, 0, authenticated_data);
            SecDbForEachAttrWithMask(class, attr_desc, kSecDbInAuthenticatedDataFlag) {
                CFDictionaryRemoveValue(*acl, attr_desc->name);
                CFTypeRef value = CFDictionaryGetValue(authenticated_data, attr_desc->name);
                if (value) {
                    if (!*authenticated_attributes)
                        *authenticated_attributes = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                    
                    CFDictionaryAddValue(*authenticated_attributes, attr_desc->name, value);
                }
            }
        }
    }
    
    CFReleaseSafe(aks_data);
}

static CFDataRef kc_copy_constraints_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes) {
    CFDictionaryRef constraints = SecAccessControlGetConstraints(access_control);
    CFMutableDictionaryRef auth_data = CFDictionaryCreateMutableCopy(NULL, 0, constraints);
    if (auth_attributes) {
        CFDictionaryForEach(auth_attributes, ^(const void *key, const void *value) {
            CFDictionaryAddValue(auth_data, key, value);
        });
    }
    
    CFDataRef encoded = kc_plist_copy_der(auth_data, NULL);
    CFReleaseSafe(auth_data);
    return encoded;
}

static CFDataRef kc_copy_access_groups_data(CFArrayRef access_groups, CFErrorRef *error)
{
    size_t ag_size = der_sizeof_plist(access_groups, error);
    CFMutableDataRef result = CFDataCreateMutable(kCFAllocatorDefault, 0);
    CFDataSetLength(result, ag_size);
    if (!der_encode_plist(access_groups, error, CFDataGetMutableBytePtr(result), CFDataGetMutableBytePtr(result) + ag_size)) {
        CFRelease(result);
        return NULL;
    }
    else
        return result;
}

#endif /* USE_KEYSTORE */

static CFDataRef kc_copy_protection_data(SecAccessControlRef access_control)
{
    CFTypeRef protection = SecAccessControlGetProtection(access_control);
    size_t protection_size = der_sizeof_plist(protection, NULL);
    CFMutableDataRef result = CFDataCreateMutable(NULL, 0);
    CFDataSetLength(result, protection_size);
    if (!der_encode_plist(protection, NULL, CFDataGetMutableBytePtr(result), CFDataGetMutableBytePtr(result) + protection_size)) {
        CFRelease(result);
        return NULL;
    }
    else
        return result;
}

static CFTypeRef kc_copy_protection_from_data(CFDataRef data)
{
    CFTypeRef result = NULL;
    der_decode_plist(NULL, kCFPropertyListImmutable, &result, NULL, CFDataGetBytePtr(data), CFDataGetBytePtr(data) + CFDataGetLength(data));
    return result;
}

/* Return a (mutable) dictionary if plist is a dictionary, return NULL and set error otherwise.  Does nothing if plist is already NULL. */
static CF_RETURNS_RETAINED
CFMutableDictionaryRef dictionaryFromPlist(CFPropertyListRef plist CF_CONSUMED, CFErrorRef *error) {
    if (plist && !isDictionary(plist)) {
        CFStringRef typeName = CFCopyTypeIDDescription(CFGetTypeID((CFTypeRef)plist));
        SecError(errSecDecode, error, CFSTR("plist is a %@, expecting a dictionary"), typeName);
        CFReleaseSafe(typeName);
        CFReleaseNull(plist);
    }
    return (CFMutableDictionaryRef)plist;
}

static CF_RETURNS_RETAINED
CFMutableDictionaryRef s3dl_item_v2_decode(CFDataRef plain, CFErrorRef *error) {
    CFPropertyListRef item;
    item = CFPropertyListCreateWithData(0, plain, kCFPropertyListMutableContainers, NULL, error);
    return dictionaryFromPlist(item, error);
}

static CF_RETURNS_RETAINED
CFMutableDictionaryRef s3dl_item_v3_decode(CFDataRef plain, CFErrorRef *error) {
    CFPropertyListRef item = NULL;
    const uint8_t *der = CFDataGetBytePtr(plain);
    const uint8_t *der_end = der + CFDataGetLength(plain);
    der = der_decode_plist(0, kCFPropertyListMutableContainers, &item, error, der, der_end);
    if (der && der != der_end) {
        SecCFCreateError(errSecDecode, kSecErrorDomain, CFSTR("trailing garbage at end of decrypted item"), NULL, error);
        CFReleaseNull(item);
    }
    return dictionaryFromPlist(item, error);
}

bool s3dl_item_from_data(CFDataRef edata, Query *q, CFArrayRef accessGroups,
                         CFMutableDictionaryRef *item, SecAccessControlRef *access_control, CFErrorRef *error) {
    SecAccessControlRef ac = NULL;
    CFDataRef ac_data = NULL;
    bool ok = false;

    /* Decrypt and decode the item and check the decoded attributes against the query. */
    uint32_t version = 0;
    require_quiet((ok = ks_decrypt_data(q->q_keybag, q->q_crypto_op, &ac, &q->q_use_cred_handle, edata, q->q_class, q->q_caller_access_groups, item, &version, error)), out);
    if (version < 2) {
        goto out;
    }

    ac_data = SecAccessControlCopyData(ac);
    if (!*item) {
        /* Item cannot be decrypted, because interactive authentication is needed. */
        if (!q->q_required_access_controls) {
            ok = SecError(errSecInteractionNotAllowed, error, CFSTR("item would need ui for decrypting"));
            goto out;
        }
        CFArrayAppendValue(q->q_required_access_controls, ac_data);
        *item = NULL;
        goto out;
    }

    if (*item && !itemInAccessGroup(*item, accessGroups)) {
        secerror("items accessGroup %@ not in %@",
                 CFDictionaryGetValue(*item, kSecAttrAccessGroup),
                 accessGroups);
        ok = SecError(errSecDecode, error, CFSTR("items accessGroup %@ not in %@"),
                                                 CFDictionaryGetValue(*item, kSecAttrAccessGroup),
                                                 accessGroups);
        CFReleaseNull(*item);
    }

    /* AccessControl attribute does not exist in the db, so synthesize it. */
    if (*item) {
        CFDictionarySetValue(*item, kSecAttrAccessControl, ac_data);
    }

    /* TODO: Validate access_control attribute. */

out:
    if (access_control)
        *access_control = CFRetainSafe(ac);
    CFReleaseSafe(ac);
    CFReleaseSafe(ac_data);
    return ok;
}

/* Infer accessibility and access group for pre-v2 (iOS4.x and earlier) items
 being imported from a backup.  */
static bool SecDbItemImportMigrate(SecDbItemRef item, CFErrorRef *error) {
    bool ok = true;
    CFStringRef agrp = SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup);
    CFStringRef accessible = SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);

    if (!isString(agrp) || !isString(accessible))
        return ok;
    if (SecDbItemGetClass(item) == &genp_class && CFEqual(accessible, kSecAttrAccessibleAlways)) {
        CFStringRef svce = SecDbItemGetCachedValueWithName(item, kSecAttrService);
        if (!isString(svce)) return ok;
        if (CFEqual(agrp, CFSTR("apple"))) {
            if (CFEqual(svce, CFSTR("AirPort"))) {
                ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, error);
            } else if (CFEqual(svce, CFSTR("com.apple.airplay.password"))) {
                ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error);
            } else if (CFEqual(svce, CFSTR("YouTube"))) {
                ok = (SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error) &&
                      SecDbItemSetValueWithName(item, kSecAttrAccessGroup, CFSTR("com.apple.youtube.credentials"), error));
            } else {
                CFStringRef desc = SecDbItemGetCachedValueWithName(item, kSecAttrDescription);
                if (!isString(desc)) return ok;
                if (CFEqual(desc, CFSTR("IPSec Shared Secret")) || CFEqual(desc, CFSTR("PPP Password"))) {
                    ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleAfterFirstUnlock, error);
                }
            }
        }
    } else if (SecDbItemGetClass(item) == &inet_class && CFEqual(accessible, kSecAttrAccessibleAlways)) {
        if (CFEqual(agrp, CFSTR("PrintKitAccessGroup"))) {
            ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error);
        } else if (CFEqual(agrp, CFSTR("apple"))) {
            CFTypeRef ptcl = SecDbItemGetCachedValueWithName(item, kSecAttrProtocol);
            bool is_proxy = false;
            if (isNumber(ptcl)) {
                SInt32 iptcl;
                CFNumberGetValue(ptcl, kCFNumberSInt32Type, &iptcl);
                is_proxy = (iptcl == FOUR_CHAR_CODE('htpx') ||
                            iptcl == FOUR_CHAR_CODE('htsx') ||
                            iptcl == FOUR_CHAR_CODE('ftpx') ||
                            iptcl == FOUR_CHAR_CODE('rtsx') ||
                            iptcl == FOUR_CHAR_CODE('xpth') ||
                            iptcl == FOUR_CHAR_CODE('xsth') ||
                            iptcl == FOUR_CHAR_CODE('xptf') ||
                            iptcl == FOUR_CHAR_CODE('xstr'));
            } else if (isString(ptcl)) {
                is_proxy = (CFEqual(ptcl, kSecAttrProtocolHTTPProxy) ||
                            CFEqual(ptcl, kSecAttrProtocolHTTPSProxy) ||
                            CFEqual(ptcl, kSecAttrProtocolRTSPProxy) ||
                            CFEqual(ptcl, kSecAttrProtocolFTPProxy));
            }
            if (is_proxy)
                ok = SecDbItemSetValueWithName(item, kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked, error);
        }
    }
    return ok;
}

bool SecDbItemDecrypt(SecDbItemRef item, CFDataRef edata, CFDataRef *neededAuth, CFErrorRef *error) {
    bool ok = true;
    CFMutableDictionaryRef dict = NULL;
    SecAccessControlRef access_control = NULL;
    uint32_t version = 0;

    if (!ks_decrypt_data(SecDbItemGetKeybag(item), item->cryptoOp, &access_control, &item->credHandle, edata, item->class, item->callerAccessGroups, &dict, &version, error)) {
        // Copy access control data, which might indicate why decryption failed.
        if (access_control && neededAuth)
            *neededAuth = SecAccessControlCopyData(access_control);
        ok = false;
        goto out;
    }

    if (!dict) {
        if (access_control && neededAuth)
            *neededAuth = SecAccessControlCopyData(access_control);
        else
            require_quiet(ok = SecError(errSecInteractionNotAllowed, error, CFSTR("auth needed, but caller does not provide it")), out);
    } else {
        if (neededAuth)
            *neededAuth = NULL;
        if (version < 2) {
            /* Old V4 style keychain backup being imported. */
            ok = SecDbItemSetValueWithName(item, CFSTR("v_Data"), CFDictionaryGetValue(dict, CFSTR("v_Data")), error) &&
            SecDbItemImportMigrate(item, error);
        } else {
            ok = dict && SecDbItemSetValues(item, dict, error);
        }
    }

    SecAccessControlRef my_access_control = SecDbItemCopyAccessControl(item, error);
    if (!my_access_control) {
        ok = false;
        goto out;
    }

    /* Make sure that the protection of ACL in the dictionary (read from DB) matched what we got
     back from decoding the data blob. */
    if (!CFEqual(SecAccessControlGetProtection(my_access_control), SecAccessControlGetProtection(access_control))) {
        ok = SecError(errSecDecode, error, CFSTR("ACL protection doesn't match the one in blob (%@ : %@)"),
                      SecAccessControlGetProtection(my_access_control),
                      SecAccessControlGetProtection(access_control));
    }
    CFRelease(my_access_control);

    // Update real protection used for decrypting in the item.
    ok = ok && SecDbItemSetAccessControl(item, access_control, error);

out:
    CFReleaseSafe(dict);
    CFReleaseSafe(access_control);
    return ok;
}

/* Automagically make a item syncable, based on various attributes. */
bool SecDbItemInferSyncable(SecDbItemRef item, CFErrorRef *error)
{
    CFStringRef agrp = SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup);

    if (!isString(agrp))
        return true;

    if (CFEqual(agrp, CFSTR("com.apple.cfnetwork")) && SecDbItemGetClass(item) == &inet_class) {
        CFTypeRef srvr = SecDbItemGetCachedValueWithName(item, kSecAttrServer);
        CFTypeRef ptcl = SecDbItemGetCachedValueWithName(item, kSecAttrProtocol);
        CFTypeRef atyp = SecDbItemGetCachedValueWithName(item, kSecAttrAuthenticationType);

        if (isString(srvr) && isString(ptcl) && isString(atyp)) {
            /* This looks like a Mobile Safari Password,  make syncable */
            secnotice("item", "Make this item syncable: %@", item);
            return SecDbItemSetSyncable(item, true, error);
        }
    }

    return true;
}

/* This create a SecDbItem from the item dictionnary that are exported for backups.
 Item are stored in the backup as a dictionary containing two keys:
 - v_Data: the encrypted data blob
 - v_PersistentRef: a persistent Ref.
 src_keybag is normally the backup keybag.
 dst_keybag is normally the device keybag.
 */
SecDbItemRef SecDbItemCreateWithBackupDictionary(CFAllocatorRef allocator, const SecDbClass *dbclass, CFDictionaryRef dict, keybag_handle_t src_keybag, keybag_handle_t dst_keybag, CFErrorRef *error)
{
    CFDataRef edata = CFDictionaryGetValue(dict, CFSTR("v_Data"));
    SecDbItemRef item = NULL;

    if (edata) {
        item = SecDbItemCreateWithEncryptedData(kCFAllocatorDefault, dbclass, edata, src_keybag, error);
        if (item)
            if (!SecDbItemSetKeybag(item, dst_keybag, error))
                CFReleaseNull(item);
    } else {
        SecError(errSecDecode, error, CFSTR("No v_Data in backup dictionary %@"), dict);
    }

    return item;
}

bool SecDbItemExtractRowIdFromBackupDictionary(SecDbItemRef item, CFDictionaryRef dict, CFErrorRef *error) {
    CFDataRef ref = CFDictionaryGetValue(dict, CFSTR("v_PersistentRef"));
    if (!ref)
        return SecError(errSecDecode, error, CFSTR("No v_PersistentRef in backup dictionary %@"), dict);

    CFStringRef className;
    sqlite3_int64 rowid;
    if (!_SecItemParsePersistentRef(ref, &className, &rowid))
        return SecError(errSecDecode, error, CFSTR("v_PersistentRef %@ failed to decode"), ref);

    if (!CFEqual(SecDbItemGetClass(item)->name, className))
        return SecError(errSecDecode, error, CFSTR("v_PersistentRef has unexpected class %@"), className);

    return SecDbItemSetRowId(item, rowid, error);
}
