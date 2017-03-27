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
#include <Security/SecItemPriv.h>
#include <Security/SecItemInternal.h>
#include <Security/SecRandom.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <utilities/SecCFCCWrappers.h>

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
static CFTypeRef kc_copy_protection_from(const uint8_t *der, const uint8_t *der_end);
static CF_RETURNS_RETAINED CFMutableDictionaryRef s3dl_item_v2_decode(CFDataRef plain, CFErrorRef *error);
static CF_RETURNS_RETAINED CFMutableDictionaryRef s3dl_item_v3_decode(CFDataRef plain, CFErrorRef *error);
#if USE_KEYSTORE
static CFDataRef kc_create_auth_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes);
static bool kc_attribs_key_encrypted_data_from_blob(keybag_handle_t keybag, const SecDbClass *class, const void *blob_data, size_t blob_data_len, SecAccessControlRef access_control, uint32_t version,
                                                    CFMutableDictionaryRef *authenticated_attributes, aks_ref_key_t *ref_key, CFDataRef *encrypted_data, CFErrorRef *error);
static CFDataRef kc_copy_access_groups_data(CFArrayRef access_groups, CFErrorRef *error);
#endif

static const uint8_t* der_decode_plist_with_repair(CFAllocatorRef pl, CFOptionFlags mutability, CFPropertyListRef* cf, CFErrorRef *error,
                                                   const uint8_t* der, const uint8_t *der_end,
                                                   const uint8_t* (^repairBlock)(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* pl, CFErrorRef *error,
                                                                                 const uint8_t* der, const uint8_t *der_end));
static const uint8_t* der_decode_dictionary_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability, CFDictionaryRef* dictionary, CFErrorRef *error,
                                                        const uint8_t* der, const uint8_t *der_end,
                                                        const uint8_t* (^repairBlock)(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* pl, CFErrorRef *error,
                                                                                      const uint8_t* der, const uint8_t *der_end));
static const uint8_t* der_decode_key_value_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* key, CFPropertyListRef* value, CFErrorRef *error,
                                                       const uint8_t* der, const uint8_t *der_end,
                                                       const uint8_t* (^repairBlock)(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* pl, CFErrorRef *error,
                                                                                     const uint8_t* der, const uint8_t *der_end));
static const uint8_t* der_decode_array_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability, CFArrayRef* array, CFErrorRef *error,
                                                   const uint8_t* der, const uint8_t *der_end,
                                                   const uint8_t* (^repairBlock)(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* pl, CFErrorRef *error,
                                                                                 const uint8_t* der, const uint8_t *der_end));
static const uint8_t* der_decode_set_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability, CFSetRef* set, CFErrorRef *error,
                                                 const uint8_t* der, const uint8_t *der_end,
                                                 const uint8_t* (^repairBlock)(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* pl, CFErrorRef *error,
                                                                               const uint8_t* der, const uint8_t *der_end));

const uint32_t kUseDefaultIVMask =  1<<31;
const int16_t  kIVSizeAESGCM = 12;

// echo "keychainblobstaticiv" | openssl dgst -sha256 | cut -c1-24 | xargs -I {} echo "0x{}" | xxd -r | xxd -p  -i
static const uint8_t gcmIV[kIVSizeAESGCM] = {
    0x1e, 0xa0, 0x5c, 0xa9, 0x98, 0x2e, 0x87, 0xdc, 0xf1, 0x45, 0xe8, 0x24
};

/* Given plainText create and return a CFDataRef containing:
 BULK_KEY = RandomKey()
 version || keyclass|ACL || KeyStore_WRAP(keyclass, BULK_KEY) ||
 AES(BULK_KEY, NULL_IV, plainText || padding)
 */
bool ks_encrypt_data(keybag_handle_t keybag, SecAccessControlRef access_control, CFDataRef acm_context,
                     CFDictionaryRef attributes, CFDictionaryRef authenticated_attributes, CFDataRef *pBlob, bool useDefaultIV, CFErrorRef *error) {
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
    size_t ivLen = 0;
    const uint8_t *iv = NULL;
    const uint8_t *aad = NULL;  // Additional Authenticated Data
    ptrdiff_t aadLen = 0;

#if USE_KEYSTORE
    CFDataRef auth_data = NULL;
#endif

    /* If access_control specifies only protection and no ACL, use legacy blob format version 3,
     which has better support for sync/backup.  Otherwise, force new format v6 unless useDefaultIV is set. */
    bool hasACLConstraints = SecAccessControlGetConstraints(access_control);
    const uint32_t version = (hasACLConstraints ? 6 : 3);
    CFDataRef plainText = NULL;
    if (version < 4) {
        CFMutableDictionaryRef attributes_dict = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
        if (authenticated_attributes) {
            CFDictionaryForEach(authenticated_attributes, ^(const void *key, const void *value) {
                CFDictionaryAddValue(attributes_dict, key, value);
            });
        }

        if (attributes_dict) {
            // Drop the accc attribute for non v6 items during encode.
            CFDictionaryRemoveValue(attributes_dict, kSecAttrAccessControl);
            plainText = CFPropertyListCreateDERData(kCFAllocatorDefault, attributes_dict, error);
            CFRelease(attributes_dict);
        }
    } else {
#if USE_KEYSTORE
        if (attributes) {
            plainText = CFPropertyListCreateDERData(kCFAllocatorDefault, attributes, error);
        }
#else
        CFMutableDictionaryRef attributes_dict = CFDictionaryCreateMutableCopy(NULL, 0, attributes);
        if (authenticated_attributes) {
            CFDictionaryForEach(authenticated_attributes, ^(const void *key, const void *value) {
                CFDictionaryAddValue(attributes_dict, key, value);
            });
        }

        if (attributes_dict) {
            plainText = CFPropertyListCreateDERData(kCFAllocatorDefault, attributes_dict, error);
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
    keyclass_t actual_class = 0;
    
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
        auth_data = kc_create_auth_data(access_control, authenticated_attributes);
        require_quiet(ok = ks_encrypt_acl(keybag, keyclass, bulkKeySize, bulkKey, bulkKeyWrapped, auth_data, acm_context, access_control, error), out);
    } else
#endif
    {
        /* Encrypt bulkKey. */
        require_quiet(ok = ks_crypt(kAKSKeyOpEncrypt, keybag,
                                    keyclass, bulkKeySize, bulkKey,
                                    &actual_class, bulkKeyWrapped,
                                    error), out);
    }

    key_wrapped_size = (uint32_t)CFDataGetLength(bulkKeyWrapped);
    UInt8 *cursor;
    size_t blobLen = sizeof(version);
    uint32_t prot_length = 0;

    if (!hasACLConstraints) {
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

    *((uint32_t *)cursor) = useDefaultIV ? (version | kUseDefaultIVMask) : version;
    cursor += sizeof(version);

    //secerror("class: %d actual class: %d", keyclass, actual_class);
    if (!hasACLConstraints) {
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

    if (useDefaultIV) {
        iv = gcmIV;
        ivLen = kIVSizeAESGCM;
        // AAD is (version || ac_data || key_wrapped_size)
        aad = CFDataGetMutableBytePtr(blob);
        aadLen = cursor - aad;
    }

    memcpy(cursor, CFDataGetBytePtr(bulkKeyWrapped), key_wrapped_size);
    cursor += key_wrapped_size;

    /* Encrypt the plainText with the bulkKey. */
    CCCryptorStatus ccerr = CCCryptorGCM(kCCEncrypt, kCCAlgorithmAES128,
                                         bulkKey, bulkKeySize,
                                         iv, ivLen,     /* iv */
                                         aad, aadLen,   /* auth data */
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
    CFReleaseSafe(auth_data);
#endif
    return ok;
}

/* Given cipherText containing:
 version || keyclass || KeyStore_WRAP(keyclass, BULK_KEY) ||
 AES(BULK_KEY, NULL_IV, plainText || padding)
 return the plainText. */
bool ks_decrypt_data(keybag_handle_t keybag, CFTypeRef cryptoOp, SecAccessControlRef *paccess_control, CFDataRef acm_context,
                     CFDataRef blob, const SecDbClass *db_class, CFArrayRef caller_access_groups,
                     CFMutableDictionaryRef *attributes_p, uint32_t *version_p, CFErrorRef *error) {
    const uint32_t v0KeyWrapOverHead = 8;
    CFMutableDataRef bulkKey = CFDataCreateMutable(0, 32); /* Use 256 bit AES key for bulkKey. */
    CFDataSetLength(bulkKey, 32); /* Use 256 bit AES key for bulkKey. */
    bool ok = true;
    SecAccessControlRef access_control = NULL;

    if (attributes_p)
        *attributes_p = NULL;
    if (version_p)
        *version_p = 0;
    
    CFMutableDataRef plainText = NULL;
    CFMutableDictionaryRef attributes = NULL;
    uint32_t version = 0;
    size_t ivLen = 0;
    const uint8_t *iv = NULL;
    const uint8_t *aad = NULL;  // Additional Authenticated Data
    ptrdiff_t aadLen = 0;

#if USE_KEYSTORE
    CFMutableDictionaryRef authenticated_attributes = NULL;
    CFDataRef caller_access_groups_data = NULL;
    CFDataRef ed_data = NULL;
    aks_ref_key_t ref_key = NULL;
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

    if (blobLen < sizeof(version)) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (length)"));
        goto out;
    }

    version = *((uint32_t *)cursor);
    if (version & kUseDefaultIVMask) {
        version &= ~kUseDefaultIVMask;
        iv = gcmIV;
        ivLen = kIVSizeAESGCM;
    }

    cursor += sizeof(version);
    blobLen -= sizeof(version);

    bool hasProtectionData = (version >= 4);

    if (hasProtectionData) {
        /* Deserialize SecAccessControl object from the blob. */
        uint32_t prot_length;

        /*
         * Parse proto length
         */

        if (blobLen < sizeof(prot_length)) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (prot_length)"));
            goto out;
        }

        prot_length = *((uint32_t *)cursor);
        cursor += sizeof(prot_length);
        blobLen -= sizeof(prot_length);

        /*
         * Parse proto itself
         */

        if (blobLen < prot_length) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (prot)"));
            goto out;
        }

        CFTypeRef protection = kc_copy_protection_from(cursor, cursor + prot_length);
        if (!protection) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid ACL"));
            goto out;
        } else {
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
        blobLen -= prot_length;

        /*
         * Get numeric value of keyclass from the access_control.
         */
        keyclass = kc_parse_keyclass(SecAccessControlGetProtection(access_control), error);
        if (!keyclass) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid ACL"));
            goto out;
        }
    } else {
        if (blobLen < sizeof(keyclass)) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (keyclass)"));
            goto out;
        }

        keyclass = *((keyclass_t *)cursor);

#if USE_KEYSTORE
        CFTypeRef protection = kc_encode_keyclass(keyclass & key_class_last); // mask out generation
#else
        CFTypeRef protection = kc_encode_keyclass(keyclass);
#endif
        require_action_quiet(protection, out, ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid keyclass detected")));
        require_action_quiet(access_control = SecAccessControlCreate(kCFAllocatorDefault, error), out,
                             ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: SecAccessControlCreate failed")));
        require_action_quiet(SecAccessControlSetProtection(access_control, protection, error), out,
                             ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: SecAccessControlSetProtection failed")));

        cursor += sizeof(keyclass);
        blobLen -= sizeof(keyclass);
    }

    size_t tagLen = 0;
    uint32_t wrapped_key_size = 0;

    switch (version) {
        case 0:
            wrapped_key_size = (uint32_t)CFDataGetLength(bulkKey) + v0KeyWrapOverHead;
            break;
        case 2:
        case 3:
            /* DROPTHROUGH */
            /* v2 and v3 have the same crypto, just different dictionary encodings. */
            /* Difference between v3 and v6 is already handled above, so treat v3 as v6. */
        case 4:
        case 5:
        case 6:
            tagLen = 16;
            /* DROPTHROUGH */
        case 1:
            if (blobLen < sizeof(wrapped_key_size)) {
                ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (wrapped_key_size)"));
                goto out;
            }
            wrapped_key_size = *((uint32_t *)cursor);

            cursor += sizeof(wrapped_key_size);
            blobLen -= sizeof(wrapped_key_size);

            break;
        default:
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid version %d"), version);
            goto out;
    }

    if (blobLen < tagLen + wrapped_key_size) {
        ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (wrapped_key/taglen)"));
        goto out;
    }

    size_t ctLen = blobLen - tagLen - wrapped_key_size;

    /*
     * Pre-version 2 have some additial constraints since it use AES in CBC mode
     */
    if (version < 2) {
        if (ctLen < kCCBlockSizeAES128) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: Check for underflow (CBC check)"));
            goto out;
        }
        if ((ctLen & 0xF) != 0) {
            ok = SecError(errSecDecode, error, CFSTR("ks_decrypt_data: invalid length on CBC data"));
            goto out;
        }
    }

#if USE_KEYSTORE
    if (hasProtectionData) {
        if (caller_access_groups) {
            caller_access_groups_data = kc_copy_access_groups_data(caller_access_groups, error);
            require_quiet(ok = (caller_access_groups_data != NULL), out);
        }

        require_quiet(ok = kc_attribs_key_encrypted_data_from_blob(keybag, db_class, cursor, wrapped_key_size, access_control, version,
                                                                   &authenticated_attributes, &ref_key, &ed_data, error), out);
        if (CFEqual(cryptoOp, kAKSKeyOpDecrypt)) {
            require_quiet(ok = ks_decrypt_acl(ref_key, ed_data, bulkKey, acm_context, caller_access_groups_data, access_control, error), out);
        } else if (CFEqual(cryptoOp, kAKSKeyOpDelete)) {
            require_quiet(ok = ks_delete_acl(ref_key, ed_data, acm_context, caller_access_groups_data, access_control, error), out);
            attributes = CFRetainSafe(authenticated_attributes);
            goto out;
        } else {
            ok = SecError(errSecInternal, error, CFSTR("ks_decrypt_data: invalid operation"));
            goto out;
        }
    } else
#endif
    {
        /* Now unwrap the bulk key using a key in the keybag. */
        require_quiet(ok = ks_crypt(cryptoOp, keybag,
            keyclass, wrapped_key_size, cursor, NULL, bulkKey, error), out);
    }

    if (iv) {
        // AAD is (version || ... [|| key_wrapped_size ])
        aad = CFDataGetBytePtr(blob);
        aadLen = cursor - aad;
    }

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
                             iv, ivLen,     /* iv */
                             aad, aadLen,   /* auth data */
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
        if (timingsafe_bcmp(tag, cursor, tagLen)) {
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
    }
    
    require_action_quiet(attributes, out, { ok = false; secerror("decode v%d failed: %@ [item: %@]", version, error ? *error : NULL, plainText); });

#if USE_KEYSTORE
    if (version >= 4 && authenticated_attributes != NULL) {
        CFDictionaryForEach(authenticated_attributes, ^(const void *key, const void *value) {
            CFDictionaryAddValue(attributes, key, value);
        });
    }
#endif

out:
    memset(CFDataGetMutableBytePtr(bulkKey), 0, CFDataGetLength(bulkKey));
    CFReleaseNull(bulkKey);
    CFReleaseNull(plainText);
    
    // Always copy access control data (if present), because if we fail it may indicate why.
    if (paccess_control)
        *paccess_control = access_control;
    else
        CFReleaseNull(access_control);
    
    if (ok) {
        if (attributes_p)
            CFRetainAssign(*attributes_p, attributes);
        if (version_p)
            *version_p = version;
	}
    CFReleaseNull(attributes);
#if USE_KEYSTORE
    CFReleaseNull(authenticated_attributes);
    CFReleaseNull(caller_access_groups_data);
    CFReleaseNull(ed_data);
    if (ref_key) aks_ref_key_free(&ref_key);
#endif
    return ok;
}

static keyclass_t kc_parse_keyclass(CFTypeRef value, CFErrorRef *error) {
    if (!isString(value)) {
        SecError(errSecParam, error, CFSTR("accessible attribute %@ not a string"), value);
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlocked)) {
        return key_class_ak;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlock)) {
        return key_class_ck;
    } else if (CFEqual(value, kSecAttrAccessibleAlwaysPrivate)) {
        return key_class_dk;
    } else if (CFEqual(value, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)) {
        return key_class_aku;
    } else if (CFEqual(value, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)) {
        return key_class_cku;
    } else if (CFEqual(value, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate)) {
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
            return kSecAttrAccessibleAlwaysPrivate;
        case key_class_aku:
            return kSecAttrAccessibleWhenUnlockedThisDeviceOnly;
        case key_class_cku:
            return kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly;
        case key_class_dku:
            return kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate;
        case key_class_akpu:
            return kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly;
        default:
            return 0;
    }
}

#if USE_KEYSTORE
static bool kc_attribs_key_encrypted_data_from_blob(keybag_handle_t keybag, const SecDbClass *class, const void *blob_data, size_t blob_data_len, SecAccessControlRef access_control, uint32_t version,
                                             CFMutableDictionaryRef *authenticated_attributes, aks_ref_key_t *ref_key, CFDataRef *encrypted_data, CFErrorRef *error)
{
    CFMutableDictionaryRef acl = NULL;
    CFDictionaryRef blob_dict = NULL;
    aks_ref_key_t tmp_ref_key = NULL;
    CFDataRef key_data = NULL;
    CFDataRef ed = NULL;
    bool ok = false;

    der_decode_plist(NULL, kCFPropertyListImmutable, (CFPropertyListRef*)&blob_dict, NULL, blob_data, blob_data + blob_data_len);
    require_action_quiet(blob_dict, out, SecError(errSecDecode, error, CFSTR("kc_attribs_key_encrypted_data_from_blob: failed to decode 'blob data'")));

    if (!ks_separate_data_and_key(blob_dict, &ed, &key_data)) {
        ed = CFDataCreate(kCFAllocatorDefault, blob_data, blob_data_len);
        key_data = CFRetain(ed);
    }
    require_action_quiet(ed, out, SecError(errSecDecode, error, CFSTR("kc_attribs_key_encrypted_data_from_blob: failed to decode 'encrypted data'")));
    require_action_quiet(key_data, out, SecError(errSecDecode, error, CFSTR("kc_attribs_key_encrypted_data_from_blob: failed to decode 'key data'")));

    const void *external_data = NULL;
    size_t external_data_len = 0;
    require_quiet(external_data = ks_ref_key_get_external_data(keybag, key_data, &tmp_ref_key, &external_data_len, error), out);

    CFPropertyListRef external_data_dict = NULL;
    der_decode_plist(NULL, kCFPropertyListImmutable, &external_data_dict, NULL, external_data, external_data + external_data_len);
    require_action_quiet(external_data_dict, out, SecError(errSecDecode, error, CFSTR("kc_attribs_key_encrypted_data_from_blob: failed to decode 'encrypted data dictionary'")));
    acl = CFDictionaryCreateMutableCopy(NULL, 0, external_data_dict);
    SecDbForEachAttrWithMask(class, attr_desc, kSecDbInAuthenticatedDataFlag) {
        CFDictionaryRemoveValue(acl, attr_desc->name);
        CFTypeRef value = CFDictionaryGetValue(external_data_dict, attr_desc->name);
        if (value) {
            if (!*authenticated_attributes)
                *authenticated_attributes = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

            CFDictionaryAddValue(*authenticated_attributes, attr_desc->name, value);
        }
    }
    CFReleaseSafe(external_data_dict);

    if (acl) {
        /* v4 data format used wrong ACL placement, for backward compatibility we have to support both formats */
        if (version == 4) {
            SecAccessControlSetConstraints(access_control, acl);
        } else {
            CFDictionaryRef constraints = CFDictionaryGetValue(acl, kAKSKeyAcl);
            require_action_quiet(isDictionary(constraints), out,
                                 SecError(errSecDecode, error, CFSTR("kc_attribs_key_encrypted_data_from_blob: acl missing")));
            SecAccessControlSetConstraints(access_control, constraints);
        }

        /* v4/v5 data format usualy does not contain kAKSKeyOpEncrypt, so add kAKSKeyOpEncrypt if is missing */
        if (version < 6) {
            SecAccessConstraintRef encryptConstraint = SecAccessControlGetConstraint(access_control, kAKSKeyOpEncrypt);
            if (!encryptConstraint)
                SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpEncrypt, kCFBooleanTrue, NULL);
        }

    }

    if (encrypted_data)
        *encrypted_data = CFRetain(ed);

    if (ref_key) {
        *ref_key = tmp_ref_key;
        tmp_ref_key = NULL;
    }

    ok = true;

out:
    if (tmp_ref_key)
        aks_ref_key_free(&tmp_ref_key);
    CFReleaseSafe(blob_dict);
    CFReleaseSafe(key_data);
    CFReleaseSafe(ed);
    CFReleaseSafe(acl);


    return ok;
}

static CFDataRef kc_create_auth_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes) {
    CFDictionaryRef constraints = SecAccessControlGetConstraints(access_control);
    CFMutableDictionaryRef auth_data = CFDictionaryCreateMutableCopy(NULL, 0, auth_attributes);
    CFDictionarySetValue(auth_data, kAKSKeyAcl, constraints);
    CFDataRef encoded = CFPropertyListCreateDERData(kCFAllocatorDefault, auth_data, NULL);
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

static CFTypeRef kc_copy_protection_from(const uint8_t *der, const uint8_t *der_end)
{
    CFTypeRef result = NULL;
    der_decode_plist(NULL, kCFPropertyListImmutable, &result, NULL, der, der_end);
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

static const uint8_t* (^s3dl_item_v3_decode_repair_date)(CFAllocatorRef, CFOptionFlags, CFPropertyListRef*, CFErrorRef*, const uint8_t*, const uint8_t*) =
    ^const uint8_t*(CFAllocatorRef allocator, CFOptionFlags mutability, CFPropertyListRef* pl, CFErrorRef *error, const uint8_t* der, const uint8_t *der_end) {
        if (error && CFEqualSafe(CFErrorGetDomain(*error), sSecDERErrorDomain) && CFErrorGetCode(*error) == kSecDERErrorUnknownEncoding) {
            CFAbsoluteTime date = 0;
            CFCalendarRef calendar = CFCalendarCreateWithIdentifier(allocator, kCFGregorianCalendar);
            CFTimeZoneRef tz = CFTimeZoneCreateWithTimeIntervalFromGMT(allocator, 0);
            CFCalendarSetTimeZone(calendar, tz);
            CFCalendarComposeAbsoluteTime(calendar, &date, "yMd", 2001, 3, 24); // random date for <rdar://problem/20458954> 15A143: can't recover keychain
            CFReleaseSafe(tz);
            CFReleaseSafe(calendar);

            *pl = CFDateCreate(allocator, date);
            if (NULL != *pl) {
                CFReleaseNull(*error);
                return der_end;
            }
        }
        return NULL;
};

static CF_RETURNS_RETAINED
CFMutableDictionaryRef s3dl_item_v3_decode(CFDataRef plain, CFErrorRef *error) {
    CFPropertyListRef item = NULL;
    const uint8_t *der_beg = CFDataGetBytePtr(plain);
    const uint8_t *der_end = der_beg + CFDataGetLength(plain);
    const uint8_t *der = der_decode_plist(0, kCFPropertyListMutableContainers, &item, error, der_beg, der_end);
    if (!der && error && CFEqualSafe(CFErrorGetDomain(*error), sSecDERErrorDomain) && CFErrorGetCode(*error) == kSecDERErrorUnknownEncoding) {
        CFReleaseNull(*error);
        der = der_decode_plist_with_repair(0, kCFPropertyListMutableContainers, &item, error, der_beg, der_end, s3dl_item_v3_decode_repair_date);
    }
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
    require_quiet((ok = ks_decrypt_data(q->q_keybag, kAKSKeyOpDecrypt, &ac, q->q_use_cred_handle, edata, q->q_class,
                                        q->q_caller_access_groups, item, &version, error)), out);
    if (version < 2) {
        goto out;
    }

    ac_data = SecAccessControlCopyData(ac);
    if (!itemInAccessGroup(*item, accessGroups)) {
        secerror("items accessGroup %@ not in %@",
                 CFDictionaryGetValue(*item, kSecAttrAccessGroup),
                 accessGroups);
        ok = SecError(errSecDecode, error, CFSTR("items accessGroup %@ not in %@"),
                                                 CFDictionaryGetValue(*item, kSecAttrAccessGroup),
                                                 accessGroups);
        CFReleaseNull(*item);
    }

    /* AccessControl attribute does not exist in the db, so synthesize it. */
    if (version > 3)
        CFDictionarySetValue(*item, kSecAttrAccessControl, ac_data);

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
    if (SecDbItemGetClass(item) == &genp_class && CFEqual(accessible, kSecAttrAccessibleAlwaysPrivate)) {
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
    } else if (SecDbItemGetClass(item) == &inet_class && CFEqual(accessible, kSecAttrAccessibleAlwaysPrivate)) {
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

bool SecDbItemDecrypt(SecDbItemRef item, CFDataRef edata, CFErrorRef *error) {
    bool ok = true;
    CFMutableDictionaryRef dict = NULL;
    SecAccessControlRef access_control = NULL;
    uint32_t version = 0;

    require_quiet(ok = ks_decrypt_data(SecDbItemGetKeybag(item), item->cryptoOp, &access_control, item->credHandle, edata,
                                       item->class, item->callerAccessGroups, &dict, &version, error), out);

    if (version < 2) {
        /* Old V4 style keychain backup being imported. */
        ok = SecDbItemSetValueWithName(item, CFSTR("v_Data"), CFDictionaryGetValue(dict, CFSTR("v_Data")), error) &&
        SecDbItemImportMigrate(item, error);
    } else {
        ok = dict && SecDbItemSetValues(item, dict, error);
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

out:
    // If we got protection back from ks_decrypt_data, update the appropriate attribute even if anything else
    // (incl. actual decryption) failed.  We need to access the protection type even if we are not able to actually
    // decrypt the data.
    ok = SecDbItemSetAccessControl(item, access_control, NULL) && ok;

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

static CFDataRef SecDbItemCopyDERWithMask(SecDbItemRef item, CFOptionFlags mask, CFErrorRef *error) {
    CFDataRef der = NULL;
    CFMutableDictionaryRef dict = SecDbItemCopyPListWithMask(item, mask, error);
    if (dict) {
        der = CFPropertyListCreateDERData(kCFAllocatorDefault, dict, error);
        CFRelease(dict);
    }
    return der;
}

static CFTypeRef SecDbItemCopyDigestWithMask(SecDbItemRef item, CFOptionFlags mask, CFErrorRef *error) {
    CFDataRef digest = NULL;
    CFDataRef der = SecDbItemCopyDERWithMask(item, mask, error);
    if (der) {
        digest = CFDataCopySHA1Digest(der, error);
        CFRelease(der);
    }
    return digest;
}

CFTypeRef SecDbKeychainItemCopyPrimaryKey(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error) {
    return SecDbItemCopyDigestWithMask(item, kSecDbPrimaryKeyFlag, error);
}

CFTypeRef SecDbKeychainItemCopySHA1(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error) {
    return SecDbItemCopyDigestWithMask(item, kSecDbInHashFlag, error);
}

CFTypeRef SecDbKeychainItemCopyEncryptedData(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error) {
    CFDataRef edata = NULL;
    CFMutableDictionaryRef attributes = SecDbItemCopyPListWithMask(item, kSecDbInCryptoDataFlag, error);
    CFMutableDictionaryRef auth_attributes = SecDbItemCopyPListWithMask(item, kSecDbInAuthenticatedDataFlag, error);
    if (attributes || auth_attributes) {
        SecAccessControlRef access_control = SecDbItemCopyAccessControl(item, error);
        if (access_control) {
            if (ks_encrypt_data(item->keybag, access_control, item->credHandle, attributes, auth_attributes, &edata, true, error)) {
                item->_edataState = kSecDbItemEncrypting;
            } else if (!error || !*error || CFErrorGetCode(*error) != errSecAuthNeeded || !CFEqualSafe(CFErrorGetDomain(*error), kSecErrorDomain) ) {
                seccritical("ks_encrypt_data (db): failed: %@", error ? *error : (CFErrorRef)CFSTR(""));
            }
            CFRelease(access_control);
        }
        CFReleaseSafe(attributes);
        CFReleaseSafe(auth_attributes);
    }

    return edata;
}

CFTypeRef SecDbKeychainItemCopyCurrentDate(SecDbItemRef item, const SecDbAttr *attr, CFErrorRef *error) {
    CFTypeRef value = NULL;
    switch (attr->kind) {
        case kSecDbDateAttr:
            value = CFDateCreate(kCFAllocatorDefault, 0.0);
            break;
        case kSecDbCreationDateAttr:
        case kSecDbModificationDateAttr:
            value = CFDateCreate(kCFAllocatorDefault, CFAbsoluteTimeGetCurrent());
            break;
        default:
            SecError(errSecInternal, error, CFSTR("attr %@ has no default value"), attr->name);
            value = NULL;
    }

    return value;
}

SecAccessControlRef SecDbItemCopyAccessControl(SecDbItemRef item, CFErrorRef *error) {
    SecAccessControlRef accc = NULL, pdmn = NULL, result = NULL;
    CFTypeRef acccData = SecDbItemGetValue(item, SecDbClassAttrWithKind(item->class, kSecDbAccessControlAttr, error), error);
    CFTypeRef pdmnValue = SecDbItemGetValue(item, SecDbClassAttrWithKind(item->class, kSecDbAccessAttr, error), error);

    if (!acccData || !pdmnValue)
        return NULL;
    if (!CFEqual(acccData, kCFNull))
        require_quiet(accc = SecAccessControlCreateFromData(CFGetAllocator(item), acccData, error), out);

    if (!CFEqual(pdmnValue, kCFNull)) {
        require_quiet(pdmn = SecAccessControlCreate(CFGetAllocator(item), error), out);
        require_quiet(SecAccessControlSetProtection(pdmn, pdmnValue, error), out);
    }

    if (accc && pdmn) {
        CFTypeRef acccProt = SecAccessControlGetProtection(accc);
        CFTypeRef pdmnProt = SecAccessControlGetProtection(pdmn);
        if (!acccProt || !pdmnProt || !CFEqual(acccProt, pdmnProt)) {
            secerror("SecDbItemCopyAccessControl accc %@ != pdmn %@, setting pdmn to accc value", acccProt, pdmnProt);
            __security_simulatecrash(CFSTR("Corrupted item on decrypt accc != pdmn"), __sec_exception_code_CorruptItem);
            // Setting pdmn to accc prot value.
            require_quiet(SecDbItemSetValue(item, SecDbClassAttrWithKind(item->class, kSecDbAccessAttr, error), acccProt, error), out);
        }
    }

    if (accc)
        CFRetainAssign(result, accc);
    else if(pdmn)
       CFRetainAssign(result, pdmn);

out:
    CFReleaseSafe(accc);
    CFReleaseSafe(pdmn);

    return result;
}

static const uint8_t* der_decode_plist_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                   CFPropertyListRef* pl, CFErrorRef *error,
                                                   const uint8_t* der, const uint8_t *der_end,
                                                   const uint8_t* (^repairBlock)(CFAllocatorRef, CFOptionFlags, CFPropertyListRef*, CFErrorRef*,
                                                                                 const uint8_t*, const uint8_t*))
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Null DER"), NULL, error);
        return NULL;
    }

    ccder_tag tag;
    if (NULL == ccder_decode_tag(&tag, der, der_end)) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding"), NULL, error);
        return NULL;
    }

    switch (tag) {
        case CCDER_NULL:
            return der_decode_null(allocator, mutability, (CFNullRef*)pl, error, der, der_end);
        case CCDER_BOOLEAN:
            return der_decode_boolean(allocator, mutability, (CFBooleanRef*)pl, error, der, der_end);
        case CCDER_OCTET_STRING:
            return der_decode_data(allocator, mutability, (CFDataRef*)pl, error, der, der_end);
        case CCDER_GENERALIZED_TIME: {
                const uint8_t* der_result = der_decode_date(allocator, mutability, (CFDateRef*)pl, error, der, der_end);
                if (!der_result) {
                    der_result = repairBlock(allocator, mutability, pl, error, der, der_end);
                }
                return der_result;
            }
        case CCDER_CONSTRUCTED_SEQUENCE:
            return der_decode_array_with_repair(allocator, mutability, (CFArrayRef*)pl, error, der, der_end, repairBlock);
        case CCDER_UTF8_STRING:
            return der_decode_string(allocator, mutability, (CFStringRef*)pl, error, der, der_end);
        case CCDER_INTEGER:
            return der_decode_number(allocator, mutability, (CFNumberRef*)pl, error, der, der_end);
        case CCDER_CONSTRUCTED_SET:
            return der_decode_dictionary_with_repair(allocator, mutability, (CFDictionaryRef*)pl, error, der, der_end, repairBlock);
        case CCDER_CONSTRUCTED_CFSET:
            return der_decode_set_with_repair(allocator, mutability, (CFSetRef*)pl, error, der, der_end, repairBlock);
        default:
            SecCFDERCreateError(kSecDERErrorUnsupportedDERType, CFSTR("Unsupported DER Type"), NULL, error);
            return NULL;
    }
}

static const uint8_t* der_decode_dictionary_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                        CFDictionaryRef* dictionary, CFErrorRef *error,
                                                        const uint8_t* der, const uint8_t *der_end,
                                                        const uint8_t* (^repairBlock)(CFAllocatorRef, CFOptionFlags, CFPropertyListRef*, CFErrorRef*,
                                                                                      const uint8_t*, const uint8_t*))
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Null DER"), NULL, error);
        return NULL;
    }

    const uint8_t *payload_end = 0;
    const uint8_t *payload = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SET, &payload_end, der, der_end);

    if (NULL == payload) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding, expected CCDER_CONSTRUCTED_SET"), NULL, error);
        return NULL;
    }


    CFMutableDictionaryRef dict = CFDictionaryCreateMutable(allocator, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    if (NULL == dict) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("Failed to create dictionary"), NULL, error);
        payload = NULL;
        goto exit;
    }

    while (payload != NULL && payload < payload_end) {
        CFTypeRef key = NULL;
        CFTypeRef value = NULL;

        payload = der_decode_key_value_with_repair(allocator, mutability, &key, &value, error, payload, payload_end, repairBlock);

        if (payload) {
            CFDictionaryAddValue(dict, key, value);
        }

        CFReleaseNull(key);
        CFReleaseNull(value);
    }


exit:
    if (payload == payload_end) {
        *dictionary = dict;
        dict = NULL;
    }

    CFReleaseNull(dict);

    return payload;
}

static const uint8_t* der_decode_key_value_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                       CFPropertyListRef* key, CFPropertyListRef* value, CFErrorRef *error,
                                                       const uint8_t* der, const uint8_t *der_end,
                                                       const uint8_t* (^repairBlock)(CFAllocatorRef, CFOptionFlags, CFPropertyListRef*, CFErrorRef*,
                                                                                     const uint8_t*, const uint8_t*))
{
    const uint8_t *payload_end = 0;
    const uint8_t *payload = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, &payload_end, der, der_end);

    if (NULL == payload) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding, expected CCDER_CONSTRUCTED_SEQUENCE"), NULL, error);
        return NULL;
    }

    CFTypeRef keyObject = NULL;
    CFTypeRef valueObject = NULL;


    payload = der_decode_plist_with_repair(allocator, mutability, &keyObject, error, payload, payload_end, repairBlock);
    payload = der_decode_plist_with_repair(allocator, mutability, &valueObject, error, payload, payload_end, repairBlock);

    if (payload != NULL) {
        *key = keyObject;
        *value = valueObject;
    } else {
        CFReleaseNull(keyObject);
        CFReleaseNull(valueObject);
    }
    return payload;
}

static const uint8_t* der_decode_array_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                   CFArrayRef* array, CFErrorRef *error,
                                                   const uint8_t* der, const uint8_t *der_end,
                                                   const uint8_t* (^repairBlock)(CFAllocatorRef, CFOptionFlags, CFPropertyListRef*, CFErrorRef*,
                                                                                 const uint8_t*, const uint8_t*))
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Null DER"), NULL, error);
        return NULL;
    }

    CFMutableArrayRef result = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

    const uint8_t *elements_end;
    const uint8_t *current_element = ccder_decode_sequence_tl(&elements_end, der, der_end);

    while (current_element != NULL && current_element < elements_end) {
        CFPropertyListRef element = NULL;
        current_element = der_decode_plist_with_repair(allocator, mutability, &element, error, current_element, elements_end, repairBlock);
        if (current_element) {
            CFArrayAppendValue(result, element);
            CFReleaseNull(element);
        }
    }

    if (current_element) {
        *array = result;
        result = NULL;
    }

    CFReleaseNull(result);
    return current_element;
}

static const uint8_t* der_decode_set_with_repair(CFAllocatorRef allocator, CFOptionFlags mutability,
                                                 CFSetRef* set, CFErrorRef *error,
                                                 const uint8_t* der, const uint8_t *der_end,
                                                 const uint8_t* (^repairBlock)(CFAllocatorRef, CFOptionFlags, CFPropertyListRef*, CFErrorRef*,
                                                                               const uint8_t*, const uint8_t*))
{
    if (NULL == der) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Null DER"), NULL, error);
        return NULL;
    }

    const uint8_t *payload_end = 0;
    const uint8_t *payload = ccder_decode_constructed_tl(CCDER_CONSTRUCTED_CFSET, &payload_end, der, der_end);

    if (NULL == payload) {
        SecCFDERCreateError(kSecDERErrorUnknownEncoding, CFSTR("Unknown data encoding, expected CCDER_CONSTRUCTED_CFSET"), NULL, error);
        return NULL;
    }

    CFMutableSetRef theSet = (set && *set) ? CFSetCreateMutableCopy(allocator, 0, *set)
                                           : CFSetCreateMutable(allocator, 0, &kCFTypeSetCallBacks);

    if (NULL == theSet) {
        SecCFDERCreateError(kSecDERErrorAllocationFailure, CFSTR("Failed to create set"), NULL, error);
        payload = NULL;
        goto exit;
    }

    while (payload != NULL && payload < payload_end) {
        CFTypeRef value = NULL;

        payload = der_decode_plist_with_repair(allocator, mutability, &value, error, payload, payload_end, repairBlock);

        if (payload) {
            CFSetAddValue(theSet, value);
        }
        CFReleaseNull(value);
    }
    
    
exit:
    if (set && payload == payload_end) {
        CFTransferRetained(*set, theSet);
    }
    
    CFReleaseNull(theSet);
    
    return payload;
}
