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
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecInternal.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <libaks_acl_cf_keys.h>

#include "Security_regressions.h"

static CFTypeRef kSecAccessControlKeyProtection = CFSTR("prot");

// TODO: Use real name of this policy from SCred/AppleCredentialManager
CFTypeRef kSecAccessControlPolicyUserPresent = CFSTR("DeviceOwnerAuthenticated");

static void tests(void)
{
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFTypeRef protection = kSecAttrAccessibleAlways;
    SecAccessControlCreateFlags flags = kSecAccessControlUserPresence;
    CFErrorRef error = NULL;

    // Simple API tests:

    // 1: ACL with protection only
    SecAccessControlRef acl = SecAccessControlCreateWithFlags(allocator, protection, 0, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // 2: ACL with flags only (not allowed)
    acl = SecAccessControlCreateWithFlags(allocator, NULL, flags, &error);
    ok(acl == NULL, "SecAccessControlCreateWithFlags");
    CFReleaseNull(error);
    CFReleaseNull(acl);

    // 3: ACL with protection and flags
    acl = SecAccessControlCreateWithFlags(allocator, protection, flags, &error);
    ok(acl != NULL, "SecAccessControlCreateWithFlags: %@", error);
    CFReleaseNull(error);

    // Extended API tests:

    // 4: Check created ACL
    CFTypeRef aclProtection = SecAccessControlGetProtection(acl);
    is(aclProtection, protection, "SecAccessControlGetProtection");

    SecAccessConstraintRef aclConstraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    // 5: Check created ACL
    ok(aclConstraint != NULL && isDictionary(aclConstraint), "SecAccessControlGetConstraint");
    // 6: Check created ACL
    eq_string(CFDictionaryGetValue(aclConstraint, kAKSKeyAclConstraintPolicy), kSecAccessControlPolicyUserPresent, "SecAccessControlGetConstraint");

    CFReleaseNull(acl);

    // Simple SPI tests:

    // 7: Empty ACL
    acl = SecAccessControlCreate(allocator, &error);
    ok(acl != NULL, "SecAccessControlCreate: %@", error);
    CFReleaseNull(error);

    // 8: ACL protection
    bool result = SecAccessControlSetProtection(acl, protection, &error);
    ok(result, "SecAccessControlSetProtection: %@", error);
    CFReleaseNull(error);

    aclProtection = SecAccessControlGetProtection(acl);
    // 9: ACL protection
    is(aclProtection, protection, "SecAccessControlGetProtection");

    // 10: Policy constraint
    SecAccessConstraintRef policy = SecAccessConstraintCreatePolicy(kSecAccessControlPolicyUserPresent, &error);
    ok(policy != NULL, "SecAccessConstraintCreatePolicy: %@", error);
    // 11: Policy constraint
    ok(isDictionary(policy), "SecAccessConstraintCreatePolicy");
    // 12: Policy constraint
    is(CFDictionaryGetValue(policy, kAKSKeyAclConstraintPolicy), kSecAccessControlPolicyUserPresent, "SecAccessConstraintCreatePolicy");
    CFReleaseNull(error);
    CFReleaseNull(policy);

    // 13: Passcode constraint
    SecAccessConstraintRef passcode = SecAccessConstraintCreatePasscode(true);
    ok(passcode != NULL && isDictionary(passcode), "SecAccessConstraintCreatePasscode");
    // 14: Passcode constraint
    is(CFDictionaryGetValue(passcode, kAKSKeyAclConstraintUserPasscode), kCFBooleanTrue, "SecAccessConstraintCreatePasscode");
//    CFReleaseNull(passcode); passcode will be used in later tests
    
    // 15: TouchID constraint
    CFUUIDRef uuid = CFUUIDCreate(allocator);
    CFStringRef uuidString = CFUUIDCreateString(allocator, uuid);
    CFDataRef uuidData = CFStringCreateExternalRepresentation(allocator, uuidString, kCFStringEncodingUTF8, 0);
    SecAccessConstraintRef touchID = SecAccessConstraintCreateTouchID(uuidData, &error);
    ok(touchID != NULL, "SecAccessConstraintCreateTouchID: %@", error);
    // 16: TouchID constraint
    ok(isDictionary(touchID), "SecAccessConstraintCreateTouchID");
    // 17: TouchID constraint
    is(CFDictionaryGetValue(touchID, kAKSKeyAclConstraintBio), uuidData, "SecAccessConstraintCreateTouchID");
    CFReleaseNull(error);
    CFReleaseNull(touchID);
    CFReleaseNull(uuidData);
    CFReleaseNull(uuidString);
    CFReleaseNull(uuid);

    touchID = SecAccessConstraintCreateTouchID(NULL, &error);
    // 18: TouchID constraint
    ok(touchID != NULL, "SecAccessConstraintCreateTouchID: %@", error);
    // 19: TouchID constraint
    ok(isDictionary(touchID), "SecAccessConstraintCreateTouchID");
    // 20: TouchID constraint
    is(CFDictionaryGetValue(touchID, kAKSKeyAclConstraintBio), kCFBooleanTrue, "SecAccessConstraintCreateTouchID");
    CFReleaseNull(error);
    // CFReleaseNull(touchID); touchID will be used in later tests

    // 21: KofN constraint
    CFTypeRef constraints_array[] = { passcode, touchID };
    CFArrayRef constraintsArray = CFArrayCreate(allocator, constraints_array, array_size(constraints_array), &kCFTypeArrayCallBacks);
    SecAccessConstraintRef kofn = SecAccessConstraintCreateKofN(1, constraintsArray, &error);
    ok(kofn != NULL, "SecAccessConstraintCreateKofN: %@", error);
    // 22: KofN constraint
    ok(isDictionary(kofn), "SecAccessConstraintCreateKofN");
    CFTypeRef kofnConstraint = CFDictionaryGetValue(kofn, kAKSKeyAclConstraintKofN);
    // 23: KofN constraint
    ok(kofnConstraint != NULL && isDictionary(kofnConstraint), "SecAccessConstraintCreateKofN");
    CFNumberRef required = CFNumberCreateWithCFIndex(allocator, 1);
    // 24: KofN constraint
    is(CFDictionaryGetValue(kofnConstraint, kAKSKeyAclParamKofN), required, "SecAccessConstraintCreateKofN");
    // 25: KofN constraint
    is(CFDictionaryGetValue(kofnConstraint, kAKSKeyAclConstraintBio), kCFBooleanTrue, "SecAccessConstraintCreateKofN");
    // 26: KofN constraint
    is(CFDictionaryGetValue(kofnConstraint, kAKSKeyAclConstraintUserPasscode), kCFBooleanTrue, "SecAccessConstraintCreateKofN");
    CFReleaseNull(error);
    CFReleaseNull(kofn);
    CFReleaseNull(required);
    CFReleaseNull(constraintsArray);
    CFReleaseNull(passcode);

    // 27: Set constraint option
    CFTypeRef accesss_groups_array[] = { CFSTR("wheel"), CFSTR("staff") };
    CFArrayRef accessGroupsArray = CFArrayCreate(allocator, accesss_groups_array, array_size(accesss_groups_array), &kCFTypeArrayCallBacks);
    result = SecAccessConstraintSetOption(touchID, kAKSKeyAclConstraintAccessGroups, accessGroupsArray, &error);
    ok(result, "SecAccessConstraintSetOption: %@", error);
    // 28: Set constraint option
    is(CFDictionaryGetValue(touchID, kAKSKeyAclConstraintAccessGroups), accessGroupsArray, "SecAccessConstraintSetOption");
    CFReleaseNull(error);
    // CFReleaseNull(accessGroupsArray); accessGroupsArray will be used in later tests

    // 29: Add ACL constraint for operation
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDecrypt, touchID, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    CFReleaseNull(error);

    // 30: Get ACL operation constraint
    SecAccessConstraintRef constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    is(constraint, touchID, "SecAccessControlGetConstraint");
    CFReleaseNull(touchID);

    // 31: Add ACL constraint for operation (kCFBooleanTrue)
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDecrypt, kCFBooleanTrue, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    CFReleaseNull(error);

    // 32: Get ACL operation constraint (kCFBooleanTrue)
    constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    is(constraint, kCFBooleanTrue, "SecAccessControlGetConstraint");

    // 33: ACL access groups
    SecAccessControlSetAccessGroups(acl, accessGroupsArray);
    CFArrayRef aclAccessGroups = SecAccessControlGetAccessGroups(acl, kAKSKeyOpDecrypt);
    is(aclAccessGroups, accessGroupsArray, "SecAccessControlGetAccessGroups");

    // 34: Get ACL constraints
    CFDictionaryRef constraints = SecAccessControlGetConstraints(acl);
    ok(constraints != NULL && isDictionary(constraints), "SecAccessControlGetConstraints");
    // 35: Get ACL constraints
    is(CFDictionaryGetValue(constraints, kAKSKeyOpDecrypt), kCFBooleanTrue, "SecAccessControlGetConstraints");
    // 36: Get ACL constraints
    is(CFDictionaryGetValue(constraints, kAKSKeyAccessGroups), accessGroupsArray, "SecAccessControlGetConstraints");
    CFReleaseNull(accessGroupsArray);

    // 37: Remove ACL operation constraint
    SecAccessControlRemoveConstraintForOperation(acl, kAKSKeyOpDecrypt);
SKIP: {
        // All constraints are removed so any operation should be allowed.
        constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
        skip("ACL access groups are considered as a constraint which blocks ACL operations", 1, constraint == kCFBooleanTrue);
        is(constraint, kCFBooleanTrue, "SecAccessControlRemoveConstraintForOperation");
    }

    // 38: ACL export and import
    CFDataRef aclData = SecAccessControlCopyData(acl);
    ok(aclData != NULL, "SecAccessControlCopyData");

    SecAccessControlRef aclCopy = SecAccessControlCreateFromData(allocator, aclData, &error);
    // 39: ACL export and import
    ok(aclCopy != NULL, "SecAccessControlCopyData: %@", error);
    // 40: ACL export and import
    ok(CFEqual(aclCopy, acl), "SecAccessControlCopyData");
    CFReleaseNull(error);
    CFReleaseNull(aclCopy);
    CFReleaseNull(aclData);

    // Extended SPI tests:

    // 41: kAKSKeyDefaultOpAcl
    result = SecAccessControlAddConstraintForOperation(acl, kAKSKeyOpDefaultAcl, kCFBooleanTrue, &error);
    ok(result, "SecAccessControlAddConstraintForOperation: %@", error);
    constraint = SecAccessControlGetConstraint(acl, kAKSKeyOpDecrypt);
    // 42: kAKSKeyDefaultOpAcl
    is(constraint, kCFBooleanTrue, "SecAccessControlRemoveConstraintForOperation");
    CFReleaseNull(error);

    CFReleaseNull(acl);
}

int si_77_SecAccessControl(int argc, char *const *argv)
{
    plan_tests(42);

    tests();

    return 0;
}
