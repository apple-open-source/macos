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
#include "keychain/securityd/SecDbItem.h"
#include <coreauthd_spi.h>
#include <corecrypto/ccder.h>
#endif /* TARGET_HAS_KEYSTORE */

#include "Security_regressions.h"

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

    // ACL with protection and kSecAccessControlBiometryCurrentSet
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlBiometryCurrentSet, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlBiometryAny | kSecAccessControlDevicePasscode | kSecAccessControlOr, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlBiometryAny | kSecAccessControlDevicePasscode | kSecAccessControlAnd, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // ACL with protection and flags
#if TARGET_OS_OSX
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlBiometryAny | kSecAccessControlDevicePasscode | kSecAccessControlWatch | kSecAccessControlAnd | kSecAccessControlApplicationPassword, &error);
#else
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlBiometryAny | kSecAccessControlDevicePasscode | kSecAccessControlAnd | kSecAccessControlApplicationPassword, &error);
#endif
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
    acl = SecAccessControlCreateWithFlags(allocator, protection, kSecAccessControlUserPresence | kSecAccessControlBiometryAny, &error);
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

    // Watch constraint
    aclConstraint = SecAccessConstraintCreateWatch(allocator);
    ok(aclConstraint != NULL && isDictionary(aclConstraint), "SecAccessConstraintCreateWatch");
    is(CFDictionaryGetValue(aclConstraint, CFSTR(kACMKeyAclConstraintWatch)), kCFBooleanTrue, "SecAccessConstraintCreateWatch");
    CFReleaseNull(aclConstraint);

    // Passcode constraint
    SecAccessConstraintRef passcode = SecAccessConstraintCreatePasscode(allocator);
    ok(passcode != NULL && isDictionary(passcode), "SecAccessConstraintCreatePasscode");
    is(CFDictionaryGetValue(passcode, CFSTR(kACMKeyAclConstraintUserPasscode)), kCFBooleanTrue, "SecAccessConstraintCreatePasscode");
//    CFReleaseNull(passcode); passcode will be used in later tests

    CFUUIDRef uuid = CFUUIDCreate(allocator);
    CFStringRef uuidString = CFUUIDCreateString(allocator, uuid);
    CFDataRef uuidData = CFStringCreateExternalRepresentation(allocator, uuidString, kCFStringEncodingUTF8, 0);
    SecAccessConstraintRef biometry = SecAccessConstraintCreateBiometryCurrentSet(allocator, uuidData, uuidData);
    // Biometry constraint
    ok(biometry != NULL, "SecAccessConstraintCreateBiometry: %@", error);
    ok(isDictionary(biometry), "SecAccessConstraintCreateBiometry");
    ok(CFDictionaryGetValue(biometry, CFSTR(kACMKeyAclConstraintBio)), "SecAccessConstraintCreateBiometry");
    CFDictionaryRef bioRef = CFDictionaryGetValue(biometry, CFSTR(kACMKeyAclConstraintBio));
    ok(isDictionary(bioRef), "SecAccessConstraintCreateBiometry");
    is(CFDictionaryGetValue(bioRef, CFSTR(kACMKeyAclParamBioCatacombUUID)), uuidData, "SecAccessConstraintCreateBiometry");
    is(CFDictionaryGetValue(bioRef, CFSTR(kACMKeyAclParamBioDatabaseHash)), uuidData, "SecAccessConstraintCreateBiometry");
    CFReleaseNull(error);
    CFReleaseNull(biometry);
    CFReleaseNull(uuidData);
    CFReleaseNull(uuidString);
    CFReleaseNull(uuid);

    uuid = CFUUIDCreate(allocator);
    uuidString = CFUUIDCreateString(allocator, uuid);
    uuidData = CFStringCreateExternalRepresentation(allocator, uuidString, kCFStringEncodingUTF8, 0);
    biometry = SecAccessConstraintCreateBiometryAny(allocator, uuidData);
    // Biometry constraint
    ok(biometry != NULL, "SecAccessConstraintCreateBiometry: %@", error);
    ok(isDictionary(biometry), "SecAccessConstraintCreateBiometry");
    ok(CFDictionaryGetValue(biometry, CFSTR(kACMKeyAclConstraintBio)), "SecAccessConstraintCreateBiometry");
    bioRef = CFDictionaryGetValue(biometry, CFSTR(kACMKeyAclConstraintBio));
    ok(isDictionary(bioRef), "SecAccessConstraintCreateBiometry");
    is(CFDictionaryGetValue(bioRef, CFSTR(kACMKeyAclParamBioCatacombUUID)), uuidData, "SecAccessConstraintCreateBiometry");
    CFReleaseNull(error);
    // CFReleaseNull(biometry); biometry will be used in later tests
    CFReleaseNull(uuidData);
    CFReleaseNull(uuidString);
    CFReleaseNull(uuid);

    // KofN constraint
    CFTypeRef constraints_array[] = { passcode, biometry };
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
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDecrypt, biometry, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    CFReleaseNull(error);

    // Get ACL operation constraint
    SecAccessConstraintRef constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    is(constraint, biometry, "SecAccessControlGetConstraint");
    CFReleaseNull(biometry);

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

int si_77_SecAccessControl(int argc, char *const *argv)
{
    plan_tests(65);

    tests();

    return 0;
}
