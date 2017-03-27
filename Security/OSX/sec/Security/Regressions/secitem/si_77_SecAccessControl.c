/*
 * Copyright (c) 2014 Apple Inc. All Rights Reserved.
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


#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAKSWrappers.h>
#include <utilities/array_size.h>
#include <utilities/der_plist.h>
#include <libaks_acl_cf_keys.h>
#include <ACMDefs.h>
#include <ACMAclDefs.h>

#if TARGET_HAS_KEYSTORE
#include <Security/SecRandom.h>
#include <securityd/SecDbItem.h>
#include <coreauthd_spi.h>
#include <corecrypto/ccder.h>
#endif /* TARGET_HAS_KEYSTORE */

#include "Security_regressions.h"

#if LA_CONTEXT_IMPLEMENTED && TARGET_HAS_KEYSTORE
static bool aks_consistency_test(bool currentAuthDataFormat, kern_return_t expectedAksResult, SecAccessControlRef access_control, CFDataRef acm_context);
static CFDataRef kc_create_auth_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes);
static CFDataRef kc_copy_constraints_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes);
static int aks_crypt_acl(CFTypeRef operation, keybag_handle_t keybag,
                                   keyclass_t keyclass, uint32_t textLength, const uint8_t *source,
                                   CFMutableDataRef dest, CFDataRef auth_data, CFDataRef acm_context, CFDataRef caller_access_groups);
#endif

static void tests(void)
{
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFTypeRef protection = kSecAttrAccessibleAlwaysPrivate;
    CFErrorRef error = NULL;

    // Simple API tests:

    // ACL with protection only
    SecAccessControlRef acl = SecAccessControlCreateWithFlags(allocator, protection, 0, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with flags only (not allowed)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // NULL passed as 'protection' newly generates a warning, we need to suppress it in order to compile
    acl = SecAccessControlCreateWithFlags(allocator, NULL, kSecAccessControlUserPresence, &error);
#pragma clang diagnostic pop
    ok(acl == NULL, "SecAccessControlCreateWithFlags");
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and kSecAccessControlTouchIDCurrentSet
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlTouchIDCurrentSet, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlTouchIDAny | kSecAccessControlDevicePasscode | kSecAccessControlOr, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlTouchIDAny | kSecAccessControlDevicePasscode | kSecAccessControlAnd, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlTouchIDAny | kSecAccessControlDevicePasscode | kSecAccessControlAnd | kSecAccessControlApplicationPassword, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and kSecAccessControlApplicationPassword
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlApplicationPassword, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and, kSecAccessControlUserPresence can be in combination with kSecAccessControlApplicationPassword and kSecAccessControlPrivateKeyUsage
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlUserPresence | kSecAccessControlApplicationPassword | kSecAccessControlPrivateKeyUsage, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // negative test of ACL with protection and, kSecAccessControlUserPresence can be in combination with kSecAccessControlApplicationPassword and kSecAccessControlPrivateKeyUsage
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlUserPresence | kSecAccessControlTouchIDAny, &error);
    ok(acl == NULL, "SecAccessControlCreateWithFlag wrong combination of flags");
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlUserPresence, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);

    // Extended API tests:

    // Check created ACL
    CFTypeRef aclProtection = SecAccessControlGetProtection(acl);
    is(aclProtection, protection, "SecAccessControlGetProtection");

    SecAccessConstraintRef aclConstraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    // Check created ACL
    ok(aclConstraint != NULL && isDictionary(aclConstraint), "SecAccessControlGetConstraint");
    eq_cf(CFDictionaryGetValue(aclConstraint, CFSTR(kACMKeyAclConstraintPolicy)), CFSTR(kACMPolicyDeviceOwnerAuthentication), "SecAccessControlGetConstraint");

    CFReleaseNull(acl);

    // Simple SPI tests:

    // Empty ACL
    acl = SecAccessControlCreate(allocator, &error);
    ok(acl != NULL, "SecAccessControlCreate: %@", error);
    CFReleaseNull(error);

    // ACL protection
    bool result = SecAccessControlSetProtection(acl, protection, &error);
    ok(result, "SecAccessControlSetProtection: %@", error);
    CFReleaseNull(error);

    aclProtection = SecAccessControlGetProtection(acl);
    // ACL protection
    is(aclProtection, protection, "SecAccessControlGetProtection");

    // Policy constraint
    SecAccessConstraintRef policy = SecAccessConstraintCreatePolicy(allocator, CFSTR(kACMPolicyDeviceOwnerAuthentication), &error);
    ok(policy != NULL, "SecAccessConstraintCreatePolicy: %@", error);
    ok(isDictionary(policy), "SecAccessConstraintCreatePolicy");
    is(CFDictionaryGetValue(policy, CFSTR(kACMKeyAclConstraintPolicy)), CFSTR(kACMPolicyDeviceOwnerAuthentication), "SecAccessConstraintCreatePolicy");
    CFReleaseNull(error);
    CFReleaseNull(policy);

    // Passcode constraint
    SecAccessConstraintRef passcode = SecAccessConstraintCreatePasscode(allocator);
    ok(passcode != NULL && isDictionary(passcode), "SecAccessConstraintCreatePasscode");
    is(CFDictionaryGetValue(passcode, CFSTR(kACMKeyAclConstraintUserPasscode)), kCFBooleanTrue, "SecAccessConstraintCreatePasscode");
//    CFReleaseNull(passcode); passcode will be used in later tests

    CFUUIDRef uuid = CFUUIDCreate(allocator);
    CFStringRef uuidString = CFUUIDCreateString(allocator, uuid);
    CFDataRef uuidData = CFStringCreateExternalRepresentation(allocator, uuidString, kCFStringEncodingUTF8, 0);
    SecAccessConstraintRef touchID = SecAccessConstraintCreateTouchIDCurrentSet(allocator, uuidData, uuidData);
    // TouchID constraint
    ok(touchID != NULL, "SecAccessConstraintCreateTouchID: %@", error);
    ok(isDictionary(touchID), "SecAccessConstraintCreateTouchID");
    ok(CFDictionaryGetValue(touchID, CFSTR(kACMKeyAclConstraintBio)), "SecAccessConstraintCreateTouchID");
    CFDictionaryRef bioRef = CFDictionaryGetValue(touchID, CFSTR(kACMKeyAclConstraintBio));
    ok(isDictionary(bioRef), "SecAccessConstraintCreateTouchID");
    is(CFDictionaryGetValue(bioRef, CFSTR(kACMKeyAclParamBioCatacombUUID)), uuidData, "SecAccessConstraintCreateTouchID");
    is(CFDictionaryGetValue(bioRef, CFSTR(kACMKeyAclParamBioDatabaseHash)), uuidData, "SecAccessConstraintCreateTouchID");
    CFReleaseNull(error);
    CFReleaseNull(touchID);
    CFReleaseNull(uuidData);
    CFReleaseNull(uuidString);
    CFReleaseNull(uuid);

    uuid = CFUUIDCreate(allocator);
    uuidString = CFUUIDCreateString(allocator, uuid);
    uuidData = CFStringCreateExternalRepresentation(allocator, uuidString, kCFStringEncodingUTF8, 0);
    touchID = SecAccessConstraintCreateTouchIDAny(allocator, uuidData);
    // TouchID constraint
    ok(touchID != NULL, "SecAccessConstraintCreateTouchID: %@", error);
    ok(isDictionary(touchID), "SecAccessConstraintCreateTouchID");
    ok(CFDictionaryGetValue(touchID, CFSTR(kACMKeyAclConstraintBio)), "SecAccessConstraintCreateTouchID");
    bioRef = CFDictionaryGetValue(touchID, CFSTR(kACMKeyAclConstraintBio));
    ok(isDictionary(bioRef), "SecAccessConstraintCreateTouchID");
    is(CFDictionaryGetValue(bioRef, CFSTR(kACMKeyAclParamBioCatacombUUID)), uuidData, "SecAccessConstraintCreateTouchID");
    CFReleaseNull(error);
    // CFReleaseNull(touchID); touchID will be used in later tests
    CFReleaseNull(uuidData);
    CFReleaseNull(uuidString);
    CFReleaseNull(uuid);

    // KofN constraint
    CFTypeRef constraints_array[] = { passcode, touchID };
    CFArrayRef constraintsArray = CFArrayCreate(allocator, constraints_array, array_size(constraints_array), &kCFTypeArrayCallBacks);
    SecAccessConstraintRef kofn = SecAccessConstraintCreateKofN(allocator, 1, constraintsArray, &error);
    ok(kofn != NULL, "SecAccessConstraintCreateKofN: %@", error);
    ok(isDictionary(kofn), "SecAccessConstraintCreateKofN");
    CFTypeRef kofnConstraint = CFDictionaryGetValue(kofn, CFSTR(kACMKeyAclConstraintKofN));
    ok(kofnConstraint != NULL && isDictionary(kofnConstraint), "SecAccessConstraintCreateKofN");
    CFNumberRef required = CFNumberCreateWithCFIndex(allocator, 1);
    is(CFDictionaryGetValue(kofnConstraint, CFSTR(kACMKeyAclParamKofN)), required, "SecAccessConstraintCreateKofN");
    ok(CFDictionaryGetValue(kofnConstraint, CFSTR(kACMKeyAclConstraintBio)), "SecAccessConstraintCreateKofN");
    is(CFDictionaryGetValue(kofnConstraint, CFSTR(kACMKeyAclConstraintUserPasscode)), kCFBooleanTrue, "SecAccessConstraintCreateKofN");
    CFReleaseNull(error);
    CFReleaseNull(kofn);
    CFReleaseNull(required);
    CFReleaseNull(constraintsArray);
    CFReleaseNull(passcode);

    // Add ACL constraint for operation
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDecrypt, touchID, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    CFReleaseNull(error);

    // Get ACL operation constraint
    SecAccessConstraintRef constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    is(constraint, touchID, "SecAccessControlGetConstraint");
    CFReleaseNull(touchID);

    // Add ACL constraint for operation (kCFBooleanTrue)
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDecrypt, kCFBooleanTrue, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    CFReleaseNull(error);

    // Get ACL operation constraint (kCFBooleanTrue)
    constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    is(constraint, kCFBooleanTrue, "SecAccessControlGetConstraint");

    // Get ACL constraints
    CFDictionaryRef constraints = SecAccessControlGetConstraints(acl);
    ok(constraints != NULL && isDictionary(constraints), "SecAccessControlGetConstraints");
    // Get ACL constraints
    is(CFDictionaryGetValue(constraints, kAKSKeyOpDecrypt), kCFBooleanTrue, "SecAccessControlGetConstraints");

    // ACL export and import
    CFDataRef aclData = SecAccessControlCopyData(acl);
    ok(aclData != NULL, "SecAccessControlCopyData");
    SecAccessControlRef aclCopy = SecAccessControlCreateFromData(allocator, aclData, &error);
    ok(aclCopy != NULL, "SecAccessControlCopyData: %@", error);
    ok(CFEqual(aclCopy, acl), "SecAccessControlCopyData");
    CFReleaseNull(error);
    CFReleaseNull(aclCopy);
    CFReleaseNull(aclData);

    // Extended SPI tests:

    // kAKSKeyDefaultOpAcl
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDefaultAcl, kCFBooleanTrue, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    is(constraint, kCFBooleanTrue, "SecAccessControlRemoveConstraintForOperation");
    CFReleaseNull(error);

    CFReleaseNull(acl);

#if LA_CONTEXT_IMPLEMENTED && TARGET_HAS_KEYSTORE
    // AKS consistency test:

    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlUserPresence, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);

    SKIP: {
        skip("SecAccessControlCreateWithFlags failed", 7, acl != NULL);

        CFDataRef acm_context = NULL;
        CFTypeRef auth_handle = NULL;

        auth_handle = LACreateNewContextWithACMContext(NULL, &error);
        ok(auth_handle != NULL, "LACreateNewContextWithACMContext: %@", error);
        CFReleaseNull(error);

        SKIP: {
            skip("LACreateNewContextWithACMContext failed", 6, auth_handle != NULL);

            acm_context = LACopyACMContext(auth_handle, &error);
            ok(acm_context != NULL, "LACopyACMContext: %@", error);
            CFReleaseNull(error);

            CFReleaseNull(auth_handle);

            SKIP: {
                skip("LACopyACMContext failed", 5, acm_context != NULL);

                ok(aks_consistency_test(true, kAKSReturnPolicyError, acl, acm_context), "AKS consistency negative test (current auth_data format)");
                ok(aks_consistency_test(false, kAKSReturnPolicyError, acl, acm_context), "AKS consistency negative test (old auth_data format)");

                bool decrypt_enabled = false;
                CFDictionaryRef constraints = SecAccessControlGetConstraints(acl);
                if (constraints) {
                    CFMutableDictionaryRef new_constraints = CFDictionaryCreateMutableCopy(NULL, 0, constraints);
                    if (new_constraints) {
                        CFDictionarySetValue(new_constraints, kAKSKeyOpDecrypt, kCFBooleanTrue);
                        SecAccessControlSetConstraints(acl, new_constraints);
                        CFReleaseSafe(new_constraints);
                        decrypt_enabled = true;
                    }
                }
                ok(decrypt_enabled, "Cannot enable decrypt operation for AKS consistency positive tests");

                ok(aks_consistency_test(true, kAKSReturnSuccess, acl, acm_context), "AKS consistency positive test (current auth_data format)");
                ok(aks_consistency_test(false, kAKSReturnSuccess, acl, acm_context), "AKS consistency positive test (old auth_data format)");

                CFReleaseNull(acm_context);
            }
        }

        CFReleaseNull(acl);
    }
#endif

    // kSecAccessControlPrivateKeyUsage
    acl = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protection, kSecAccessControlPrivateKeyUsage | kSecAccessControlDevicePasscode, NULL);
    ok(acl, "kSecAccessControlPrivateKeyUsage ACL create with constraint");
    ok(!SecAccessControlGetConstraint(acl, kAKSKeyOpEncrypt), "kAKSKeyOpEncrypt constraint");
    ok(!SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt), "kAKSKeyOpDecrypt constraint");
    ok(constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDelete), "kAKSKeyOpDelete constraint");
    is(constraint, kCFBooleanTrue, "kAKSKeyOpDelete constraint value");
    ok(constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpSign), "kAKSKeyOpSign constraint");
    ok(constraint && isDictionary(constraint), "kAKSKeyOpSign constraint value");
    CFReleaseNull(acl);

    acl = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protection, kSecAccessControlPrivateKeyUsage, NULL);
    ok(acl, "kSecAccessControlPrivateKeyUsage ACL create without constraint");
    ok(!SecAccessControlGetConstraint(acl, kAKSKeyOpEncrypt), "kAKSKeyOpEncrypt constraint");
    ok(!SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt), "kAKSKeyOpDecrypt constraint");
    ok(constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDelete), "kAKSKeyOpDelete constraint");
    is(constraint, kCFBooleanTrue, "kAKSKeyOpDelete constraint value");
    ok(constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpSign), "kAKSKeyOpSign constraint");
    is(constraint, kCFBooleanTrue, "kAKSKeyOpSign constraint value");
    CFReleaseNull(acl);
}

#if LA_CONTEXT_IMPLEMENTED && TARGET_HAS_KEYSTORE

static bool aks_consistency_test(bool currentAuthDataFormat, kern_return_t expectedAksResult, SecAccessControlRef access_control, CFDataRef acm_context)
{
    bool result = false;

    const uint32_t bulkKeySize = 32;
    const uint32_t maxKeyWrapOverHead = 8 + 32;
    uint8_t bulkKey[bulkKeySize];
    CFMutableDataRef bulkKeyWrapped = CFDataCreateMutable(NULL, 0);
    CFDataSetLength(bulkKeyWrapped, bulkKeySize + maxKeyWrapOverHead);
    CFMutableDataRef bulkKeyUnwrapped = CFDataCreateMutable(NULL, 0);
    CFDataSetLength(bulkKeyUnwrapped, bulkKeySize);

    CFDataRef auth_data = NULL;
    CFMutableDictionaryRef auth_attribs = NULL;

    require_noerr_string(SecRandomCopyBytes(kSecRandomDefault, bulkKeySize, bulkKey), out, "SecRandomCopyBytes failed");

    auth_attribs = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (currentAuthDataFormat) {
        auth_data = kc_create_auth_data(access_control, auth_attribs);
    }
    else {
        auth_data = kc_copy_constraints_data(access_control, auth_attribs);
    }

    require_string(aks_crypt_acl(kAKSKeyOpEncrypt, KEYBAG_DEVICE, key_class_dk, bulkKeySize, bulkKey, bulkKeyWrapped,
                                 auth_data, acm_context, NULL) == kAKSReturnSuccess, out, "kAKSKeyOpEncrypt failed");

    uint32_t blobLenWrapped = (uint32_t)CFDataGetLength(bulkKeyWrapped);
    const uint8_t *cursor = CFDataGetBytePtr(bulkKeyWrapped);

    require_string(aks_crypt_acl(kAKSKeyOpDecrypt, KEYBAG_DEVICE, key_class_dk, blobLenWrapped, cursor, bulkKeyUnwrapped,
                                 auth_data, acm_context, NULL) == expectedAksResult, out, "kAKSKeyOpDecrypt finished with unexpected result");

    result = true;

out:
    CFReleaseSafe(bulkKeyUnwrapped);
    CFReleaseSafe(bulkKeyWrapped);
    CFReleaseSafe(auth_data);
    CFReleaseSafe(auth_attribs);

    return result;
}

static bool merge_der_in_to_data(const void *der1, size_t der1_len, const void *der2, size_t der2_len, CFMutableDataRef mergedData)
{
    bool result = false;
    CFPropertyListRef dict1 = NULL;
    CFPropertyListRef dict2 = NULL;

    der_decode_plist(NULL, kCFPropertyListImmutable, &dict1, NULL, der1, der1 + der1_len);
    der_decode_plist(NULL, kCFPropertyListImmutable, &dict2, NULL, der2, der2 + der2_len);
    if (dict1 && dict2) {
        CFMutableDictionaryRef result_dict = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, dict1);
        CFDictionaryForEach(dict2, ^(const void *key, const void *value) {
            CFDictionaryAddValue(result_dict, key, value);
        });

        CFDataSetLength(mergedData, 0);
        CFDataRef der_data = CFPropertyListCreateDERData(kCFAllocatorDefault, result_dict, NULL);
        if (der_data) {
            CFDataAppend(mergedData, der_data);
            CFRelease(der_data);
            result = CFDataGetLength(mergedData) > 0;
        }
        CFRelease(result_dict);
    }

    CFReleaseSafe(dict1);
    CFReleaseSafe(dict2);
    return result;
}

static int aks_crypt_acl(CFTypeRef operation, keybag_handle_t keybag,
                                   keyclass_t keyclass, uint32_t textLength, const uint8_t *source,
                                   CFMutableDataRef dest, CFDataRef auth_data, CFDataRef acm_context, CFDataRef caller_access_groups)
{
    int aks_return = kAKSReturnSuccess;
    void *params = NULL, *der = NULL;
    const uint8_t *access_groups = caller_access_groups?CFDataGetBytePtr(caller_access_groups):NULL;
    size_t params_len = 0, der_len = 0, access_groups_len = caller_access_groups?CFDataGetLength(caller_access_groups):0;
    aks_ref_key_t key_handle = NULL;

    if (CFEqual(operation, kAKSKeyOpEncrypt)) {
        aks_operation_optional_params(0, 0, CFDataGetBytePtr(auth_data), CFDataGetLength(auth_data),
                                      CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), &params, &params_len);

        require_noerr_quiet(aks_return = aks_ref_key_create(keybag, keyclass, key_type_sym, params, params_len, &key_handle), out);
        require_noerr_quiet(aks_return = aks_ref_key_encrypt(key_handle, params, params_len, source, textLength, &der, &der_len), out);
        size_t key_blob_len;
        const void *key_blob = aks_ref_key_get_blob(key_handle, &key_blob_len);
        require_action_string(key_blob, out, aks_return = kAKSReturnError, "aks_ref_key_get_blob failed");
        require_action_string(merge_der_in_to_data(der, der_len, key_blob, key_blob_len, dest), out, aks_return = kAKSReturnError, "merge_der_in_to_data failed");

    } else if (CFEqual(operation, kAKSKeyOpDecrypt)) {
        aks_operation_optional_params(access_groups, access_groups_len, 0, 0,
                                      CFDataGetBytePtr(acm_context), (int)CFDataGetLength(acm_context), (void**)&params, &params_len);
        require_noerr_quiet(aks_return = aks_ref_key_create_with_blob(keybag, source, textLength, &key_handle), out);
        require_noerr_quiet(aks_return = aks_ref_key_decrypt(key_handle, params, params_len, source, textLength, &der, &der_len), out);
        require_action_string(der, out, aks_return = kAKSReturnError, "aks_ref_key_decrypt failed");

        CFPropertyListRef decoded_data = NULL;
        der_decode_plist(kCFAllocatorDefault, kCFPropertyListImmutable, &decoded_data, NULL, der, der + der_len);
        require_action_string(decoded_data, out, aks_return = kAKSReturnError, "der_decode_plist failed");
        if (CFGetTypeID(decoded_data) == CFDataGetTypeID()) {
            CFDataSetLength(dest, 0);
            CFDataAppend(dest, decoded_data);
            CFRelease(decoded_data);
        }
        else {
            CFRelease(decoded_data);
            require_action_string(false, out, aks_return = kAKSReturnError, "wrong decoded data type");
        }
    }

out:
    if (key_handle) {
        aks_ref_key_free(&key_handle);
    }
    if (params) {
        free(params);
    }
    if (der) {
        free(der);
    }
    return aks_return;
}

static CFDataRef kc_create_auth_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes)
{
    CFDictionaryRef constraints = SecAccessControlGetConstraints(access_control);
    CFMutableDictionaryRef auth_data = CFDictionaryCreateMutableCopy(NULL, 0, auth_attributes);
    CFDictionarySetValue(auth_data, kAKSKeyAcl, constraints);
    CFDataRef encoded = CFPropertyListCreateDERData(kCFAllocatorDefault, auth_data, NULL);
    CFReleaseSafe(auth_data);
    return encoded;
}

static CFDataRef kc_copy_constraints_data(SecAccessControlRef access_control, CFDictionaryRef auth_attributes)
{
    CFDictionaryRef constraints = SecAccessControlGetConstraints(access_control);
    CFMutableDictionaryRef auth_data = CFDictionaryCreateMutableCopy(NULL, 0, constraints);
    if (auth_attributes) {
        CFDictionaryForEach(auth_attributes, ^(const void *key, const void *value) {
            CFDictionaryAddValue(auth_data, key, value);
        });
    }

    CFDataRef encoded = CFPropertyListCreateDERData(kCFAllocatorDefault, auth_data, NULL);
    CFReleaseSafe(auth_data);
    return encoded;
}

#endif

int si_77_SecAccessControl(int argc, char *const *argv)
{
#if LA_CONTEXT_IMPLEMENTED && TARGET_HAS_KEYSTORE
    plan_tests(71);
#else
    plan_tests(63);
#endif

    tests();

    return 0;
}
