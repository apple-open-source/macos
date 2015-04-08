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

/*
 * SecAccessControl.c - CoreFoundation based access control object
 */

#include <AssertMacros.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecItem.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/der_plist.h>

#if TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR)
#define USE_KEYSTORE  1
#elif TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#define USE_KEYSTORE  1
#else /* no keystore on this platform */
#define USE_KEYSTORE  0
#endif

#include <libaks_acl_cf_keys.h>

static CFTypeRef kSecAccessControlKeyProtection = CFSTR("prot");

// TODO: Use real name of this policy from SCred/AppleCredentialManager
CFTypeRef kSecAccessControlPolicyUserPresent = CFSTR("DeviceOwnerAuthenticated");

struct __SecAccessControl {
    CFRuntimeBase _base;
    CFMutableDictionaryRef dict;
};

static CFStringRef SecAccessControlCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecAccessControlRef access_control = (SecAccessControlRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<SecAccessControlRef: %p>"), access_control);
}

static Boolean SecAccessControlCompare(CFTypeRef lhs, CFTypeRef rhs) {
    SecAccessControlRef laccess_control = (SecAccessControlRef)lhs;
    SecAccessControlRef raccess_control = (SecAccessControlRef)rhs;
    return (laccess_control == raccess_control) || CFEqual(laccess_control->dict, raccess_control->dict);
}

static void SecAccessControlDestroy(CFTypeRef cf) {
    SecAccessControlRef access_control = (SecAccessControlRef)cf;
    CFReleaseSafe(access_control->dict);
}

CFGiblisWithCompareFor(SecAccessControl);

SecAccessControlRef SecAccessControlCreate(CFAllocatorRef allocator, CFErrorRef *error) {
    SecAccessControlRef access_control = CFTypeAllocate(SecAccessControl, struct __SecAccessControl, allocator);
	if (!access_control) {
        SecError(errSecAllocate, error, CFSTR("allocate memory for SecAccessControl"));
        return NULL;
    }

    access_control->dict = CFDictionaryCreateMutableForCFTypes(allocator);
    return access_control;
}


SecAccessControlRef SecAccessControlCreateWithFlags(CFAllocatorRef allocator, CFTypeRef protection,
                                                    SecAccessControlCreateFlags flags, CFErrorRef *error) {
    SecAccessControlRef access_control = NULL;
    CFTypeRef constraint = NULL;

    require_quiet(access_control = SecAccessControlCreate(allocator, error), errOut);

    if (!SecAccessControlSetProtection(access_control, protection, error))
        goto errOut;

    if (flags & kSecAccessControlUserPresence) {
        require_quiet(constraint = SecAccessConstraintCreatePolicy(kSecAccessControlPolicyUserPresent, error), errOut);
        require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDecrypt,
                                                                constraint, error), errOut);
        CFReleaseNull(constraint);
        require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDelete, kCFBooleanTrue, error), errOut);
    }

    return access_control;

errOut:
    CFReleaseSafe(access_control);
    CFReleaseSafe(constraint);
    return NULL;
}

CFTypeRef SecAccessControlGetProtection(SecAccessControlRef access_control) {
    return CFDictionaryGetValue(access_control->dict, kSecAccessControlKeyProtection);
}

static bool checkItemInArray(CFTypeRef item, const CFTypeRef *values, CFIndex count, CFStringRef errMessage, CFErrorRef *error) {
    for (CFIndex i = 0; i < count; i++) {
        if (CFEqualSafe(item, values[i])) {
            return true;
        }
    }
    return SecError(errSecParam, error, errMessage, item);
}

#define CheckItemInArray(item, values, msg) \
{ \
    const CFTypeRef vals[] = values; \
    if (!checkItemInArray(item, vals, sizeof(vals)/sizeof(*vals), CFSTR(msg), error)) { \
        return false; \
    } \
}

#define ItemArray(...) { __VA_ARGS__ }


bool SecAccessControlSetProtection(SecAccessControlRef access_control, CFTypeRef protection, CFErrorRef *error) {
    // Verify protection type.
    CheckItemInArray(protection, ItemArray(kSecAttrAccessibleAlways, kSecAttrAccessibleAfterFirstUnlock,
                                           kSecAttrAccessibleWhenUnlocked, kSecAttrAccessibleAlwaysThisDeviceOnly,
                                           kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                                           kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                                           kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly),
                     "SecAccessControl: invalid protection %@");

    // Protection valid, use it.
    CFDictionarySetValue(access_control->dict, kSecAccessControlKeyProtection, protection);
    return true;
}

SecAccessConstraintRef SecAccessConstraintCreatePolicy(CFTypeRef policy, CFErrorRef *error) {
    return CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kAKSKeyAclConstraintPolicy, policy, NULL);
}

SecAccessConstraintRef SecAccessConstraintCreatePasscode(bool systemPasscode) {
    return CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kAKSKeyAclConstraintUserPasscode, kCFBooleanTrue, NULL);
}

SecAccessConstraintRef SecAccessConstraintCreateTouchID(CFDataRef uuid, CFErrorRef *error) {
    return CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kAKSKeyAclConstraintBio,
                                                   uuid ? uuid : (const void *)kCFBooleanTrue, NULL);
}

SecAccessConstraintRef SecAccessConstraintCreateKofN(size_t numRequired, CFArrayRef constraints, CFErrorRef *error) {
    CFNumberRef k = CFNumberCreateWithCFIndex(kCFAllocatorDefault, numRequired);
    CFMutableDictionaryRef kofn = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kAKSKeyAclParamKofN, k, NULL);
    CFRelease(k);

    /* Populate kofn dictionary with constraint keys from the array. note that for now we just ignore any additional
       constraint parameters, but we might err-out if some parameter is found, since we cannot propagate parameteres
       into k-of-n dictionary. */
    const CFTypeRef keysToCopy[] = { kAKSKeyAclConstraintBio, kAKSKeyAclConstraintPolicy,
        kAKSKeyAclConstraintUserPasscode };
    SecAccessConstraintRef constraint;
    CFArrayForEachC(constraints, constraint) {
        require_quiet(isDictionary(constraint), errOut);
        bool found = false;
        for (CFIndex i = 0; i < (CFIndex)(sizeof(keysToCopy) / sizeof(keysToCopy[0])); i++) {
            CFTypeRef value = CFDictionaryGetValue(constraint, keysToCopy[i]);
            if (value) {
                CFDictionarySetValue(kofn, keysToCopy[i], value);
                found = true;
                break;
            }
        }
        require_quiet(found, errOut);
    }

    constraint = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kAKSKeyAclConstraintKofN, kofn, NULL);
    CFRelease(kofn);
    return constraint;

errOut:
    SecError(errSecParam, error, CFSTR("SecAccessControl: invalid constraint for k-of-n"));
    CFReleaseSafe(kofn);
    return NULL;
}

bool SecAccessConstraintSetOption(SecAccessConstraintRef constraint, CFTypeRef option, CFTypeRef value, CFErrorRef *error) {
    CheckItemInArray(option, ItemArray(kAKSKeyAclConstraintAccessGroups, kAKSKeyAclParamCredentialMaxAge),
                     "SecAccessControl: invalid constraint option %@");
    CFDictionarySetValue((CFMutableDictionaryRef)constraint, option, value);
    return true;
}

bool SecAccessControlAddConstraintForOperation(SecAccessControlRef access_control, CFTypeRef operation, CFTypeRef constraint, CFErrorRef *error) {
    CheckItemInArray(operation, ItemArray(kAKSKeyOpEncrypt, kAKSKeyOpDecrypt,
                                          kAKSKeyOpSync, kAKSKeyOpDefaultAcl, kAKSKeyOpDelete),
                     "SecAccessControl: invalid operation %@");
    if (!isDictionary(constraint) && !CFEqual(constraint, kCFBooleanTrue) && !CFEqual(constraint, kCFBooleanFalse) ) {
        return SecError(errSecParam, error, CFSTR("invalid constraint"));
    }

    CFMutableDictionaryRef ops = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    if (!ops) {
        ops = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(access_control));
        CFDictionarySetValue(access_control->dict, kAKSKeyAcl, ops);
    }
    CFDictionarySetValue(ops, operation, constraint);
    return true;
}

void SecAccessControlRemoveConstraintForOperation(SecAccessControlRef access_control, CFTypeRef operation) {
    CFMutableDictionaryRef ops = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    if (ops)
        CFDictionaryRemoveValue(ops, operation);
}

SecAccessConstraintRef SecAccessControlGetConstraint(SecAccessControlRef access_control, CFTypeRef operation) {
    CFMutableDictionaryRef ops = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    if (!ops || CFDictionaryGetCount(ops) == 0)
        // No ACL is present, this means that everything is allowed.
        return kCFBooleanTrue;

    SecAccessConstraintRef constraint = CFDictionaryGetValue(ops, operation);
    if (!constraint) {
        constraint = CFDictionaryGetValue(ops, kAKSKeyOpDefaultAcl);
    }
    return constraint;
}

CFDictionaryRef SecAccessControlGetConstraints(SecAccessControlRef access_control) {
    return CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
}

void SecAccessControlSetConstraints(SecAccessControlRef access_control, CFDictionaryRef constraints) {
    CFMutableDictionaryRef mutableConstraints = CFDictionaryCreateMutableCopy(NULL, 0, constraints);
    CFDictionarySetValue(access_control->dict, kAKSKeyAcl, mutableConstraints);
    CFReleaseSafe(mutableConstraints);
}

void SecAccessControlSetAccessGroups(SecAccessControlRef access_control, CFArrayRef access_groups) {
    CFMutableDictionaryRef ops = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    if (!ops) {
        ops = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(access_control));
        CFDictionarySetValue(access_control->dict, kAKSKeyAcl, ops);
    }
    CFDictionarySetValue(ops, kAKSKeyAccessGroups, access_groups);
}

CFArrayRef SecAccessControlGetAccessGroups(SecAccessControlRef access_control, CFTypeRef operation) {
    CFMutableDictionaryRef ops = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    if (!ops)
        return NULL;

    CFArrayRef access_groups = NULL;
    SecAccessConstraintRef constraint = CFDictionaryGetValue(ops, operation);
    if (!constraint) {
        constraint = CFDictionaryGetValue(ops, kAKSKeyOpDefaultAcl);
    }
    if (constraint && isDictionary(constraint)) {
        access_groups = CFDictionaryGetValue(constraint, kAKSKeyAclConstraintAccessGroups);
    }
    if (!access_groups) {
        access_groups = CFDictionaryGetValue(ops, kAKSKeyAccessGroups);
    }
    return access_groups;
}

CFDataRef SecAccessControlCopyData(SecAccessControlRef access_control) {
    size_t len = der_sizeof_plist(access_control->dict, NULL);
    CFMutableDataRef encoded = CFDataCreateMutable(0, len);
    CFDataSetLength(encoded, len);
    uint8_t *der_end = CFDataGetMutableBytePtr(encoded);
    const uint8_t *der = der_end;
    der_end += len;
    der_end = der_encode_plist(access_control->dict, NULL, der, der_end);
    if (!der_end) {
        CFReleaseNull(encoded);
    }
    return encoded;
}

SecAccessControlRef SecAccessControlCreateFromData(CFAllocatorRef allocator, CFDataRef data, CFErrorRef *error) {
    SecAccessControlRef access_control;
    require_quiet(access_control = SecAccessControlCreate(allocator, error), errOut);

    CFPropertyListRef plist;
    const uint8_t *der = CFDataGetBytePtr(data);
    const uint8_t *der_end = der + CFDataGetLength(data);
    require_quiet(der = der_decode_plist(0, kCFPropertyListMutableContainers, &plist, error, der, der_end), errOut);
    if (der != der_end) {
        SecError(errSecDecode, error, CFSTR("trailing garbage at end of SecAccessControl data"));
        goto errOut;
    }

    CFReleaseSafe(access_control->dict);
    access_control->dict = (CFMutableDictionaryRef)plist;
    return access_control;

errOut:
    CFReleaseSafe(access_control);
    return NULL;
}
