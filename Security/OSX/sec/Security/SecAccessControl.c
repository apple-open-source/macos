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

#include <TargetConditionals.h>
#include <AssertMacros.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/der_plist.h>
#include <libaks_acl_cf_keys.h>
#include <ACMDefs.h>
#include <ACMAclDefs.h>

static CFTypeRef kSecAccessControlKeyProtection = CFSTR("prot");
static CFTypeRef kSecAccessControlKeyBound = CFSTR("bound");

struct __SecAccessControl {
    CFRuntimeBase _base;
    CFMutableDictionaryRef dict;
};

static SecAccessConstraintRef SecAccessConstraintCreateValueOfKofN(CFAllocatorRef allocator, size_t numRequired, CFArrayRef constraints, CFErrorRef *error);

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

static CFMutableDictionaryRef SecAccessControlGetMutableConstraints(SecAccessControlRef access_control) {
    CFMutableDictionaryRef constraints = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);

    if (!constraints) {
        CFMutableDictionaryRef newConstraints = CFDictionaryCreateMutableForCFTypes(CFGetAllocator(access_control));
        CFDictionarySetValue(access_control->dict, kAKSKeyAcl, newConstraints);
        CFRelease(newConstraints);

        constraints = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    }

    return constraints;
}

SecAccessControlRef SecAccessControlCreate(CFAllocatorRef allocator, CFErrorRef *error) {
    SecAccessControlRef access_control = CFTypeAllocate(SecAccessControl, struct __SecAccessControl, allocator);
	if (!access_control) {
        SecError(errSecAllocate, error, CFSTR("allocate memory for SecAccessControl"));
        return NULL;
    }

    access_control->dict = CFDictionaryCreateMutableForCFTypes(allocator);
    return access_control;
}
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
static CFDataRef _getEmptyData() {
    static CFMutableDataRef emptyData = NULL;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        emptyData = CFDataCreateMutable(kCFAllocatorDefault, 0);
    });

    return emptyData;
}
#endif

SecAccessControlRef SecAccessControlCreateWithFlags(CFAllocatorRef allocator, CFTypeRef protection,
                                                    SecAccessControlCreateFlags flags, CFErrorRef *error) {
    SecAccessControlRef access_control = NULL;
    CFTypeRef constraint = NULL;
    CFMutableArrayRef constraints = NULL;

    require_quiet(access_control = SecAccessControlCreate(allocator, error), errOut);

    if (!SecAccessControlSetProtection(access_control, protection, error))
        goto errOut;

    if (flags) {
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
        bool or = (flags & kSecAccessControlOr) ? true : false;
        bool and = (flags & kSecAccessControlAnd) ? true : false;

        if (or && and) {
            SecError(errSecParam, error, CFSTR("only one logical operation can be set"));
            goto errOut;
        }

        SecAccessControlCreateFlags maskedFlags = flags & (kSecAccessControlTouchIDAny | kSecAccessControlTouchIDCurrentSet);
        if (maskedFlags && maskedFlags != kSecAccessControlTouchIDAny && maskedFlags != kSecAccessControlTouchIDCurrentSet) {
            SecError(errSecParam, error, CFSTR("only one bio constraint can be set"));
            goto errOut;
        }

        if (flags & kSecAccessControlUserPresence && flags & ~(kSecAccessControlUserPresence | kSecAccessControlApplicationPassword | kSecAccessControlPrivateKeyUsage)) {
#else
        if (flags & kSecAccessControlUserPresence && flags != kSecAccessControlUserPresence) {
#endif
            SecError(errSecParam, error, CFSTR("kSecAccessControlUserPresence can be combined only with kSecAccessControlApplicationPassword and kSecAccessControlPrivateKeyUsage"));
            goto errOut;
        }

        constraints = CFArrayCreateMutable(allocator, 0, &kCFTypeArrayCallBacks);

        if (flags & kSecAccessControlUserPresence) {
            require_quiet(constraint = SecAccessConstraintCreatePolicy(allocator, CFSTR(kACMPolicyDeviceOwnerAuthentication), error), errOut);
            CFArrayAppendValue(constraints, constraint);
            CFReleaseNull(constraint);
        }

        if (flags & kSecAccessControlDevicePasscode) {
            require_quiet(constraint = SecAccessConstraintCreatePasscode(allocator), errOut);
            CFArrayAppendValue(constraints, constraint);
            CFReleaseNull(constraint);
        }

#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
        if (flags & kSecAccessControlTouchIDAny) {
            require_quiet(constraint = SecAccessConstraintCreateTouchIDAny(allocator, _getEmptyData()), errOut);
            CFArrayAppendValue(constraints, constraint);
            CFReleaseNull(constraint);
        }

        if (flags & kSecAccessControlTouchIDCurrentSet) {
            require_quiet(constraint = SecAccessConstraintCreateTouchIDCurrentSet(allocator, _getEmptyData(), _getEmptyData()), errOut);
            CFArrayAppendValue(constraints, constraint);
            CFReleaseNull(constraint);
        }

        if (flags & kSecAccessControlApplicationPassword) {
            SecAccessControlSetRequirePassword(access_control, true);
        }
#endif
        CFIndex constraints_count = CFArrayGetCount(constraints);
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
        if (constraints_count > 1) {
            require_quiet(constraint = SecAccessConstraintCreateValueOfKofN(allocator, or?1:constraints_count, constraints, error), errOut);
            if (flags & kSecAccessControlPrivateKeyUsage) {
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpSign, constraint, error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpComputeKey, constraint, error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpAttest, kCFBooleanTrue, error), errOut);
            }
            else {
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDecrypt, constraint, error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpEncrypt, kCFBooleanTrue, error), errOut);
            }
            require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDelete, kCFBooleanTrue, error), errOut);
            CFReleaseNull(constraint);
        } else
#endif
        if (constraints_count == 1) {
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
            if (flags & kSecAccessControlPrivateKeyUsage) {
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpSign, CFArrayGetValueAtIndex(constraints, 0), error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpComputeKey, CFArrayGetValueAtIndex(constraints, 0), error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpAttest, kCFBooleanTrue, error), errOut);
            }
            else {
#endif
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDecrypt, CFArrayGetValueAtIndex(constraints, 0), error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpEncrypt, kCFBooleanTrue, error), errOut);
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
            }
#endif
            require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDelete, kCFBooleanTrue, error), errOut);
        } else {
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
            if (flags & kSecAccessControlPrivateKeyUsage) {
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpSign, kCFBooleanTrue, error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpComputeKey, kCFBooleanTrue, error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpAttest, kCFBooleanTrue, error), errOut);
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDelete, kCFBooleanTrue, error), errOut);
            }
            else {
#endif
                require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDefaultAcl, kCFBooleanTrue, error), errOut);
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
            }
#endif
        }

        CFReleaseNull(constraints);
    }
    else {
        require_quiet(SecAccessControlAddConstraintForOperation(access_control, kAKSKeyOpDefaultAcl, kCFBooleanTrue, error), errOut);
    }

    return access_control;

errOut:
    CFReleaseSafe(access_control);
    CFReleaseSafe(constraints);
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
    if (!protection || CFGetTypeID(protection) != CFDictionaryGetTypeID()) {
        // Verify protection type.
        CheckItemInArray(protection, ItemArray(kSecAttrAccessibleAlwaysPrivate, kSecAttrAccessibleAfterFirstUnlock,
                                               kSecAttrAccessibleWhenUnlocked, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate,
                                               kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                                               kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                                               kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly),
                         "SecAccessControl: invalid protection %@");
    }

    // Protection valid, use it.
    CFDictionarySetValue(access_control->dict, kSecAccessControlKeyProtection, protection);
    return true;
}

SecAccessConstraintRef SecAccessConstraintCreatePolicy(CFAllocatorRef allocator, CFTypeRef policy, CFErrorRef *error) {
    return CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclConstraintPolicy), policy, NULL);
}

SecAccessConstraintRef SecAccessConstraintCreatePasscode(CFAllocatorRef allocator) {
    return CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclConstraintUserPasscode), kCFBooleanTrue, NULL);
}

SecAccessConstraintRef SecAccessConstraintCreateTouchIDAny(CFAllocatorRef allocator, CFDataRef catacombUUID) {
    CFMutableDictionaryRef bioDict = CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclParamBioCatacombUUID), catacombUUID, NULL);
    SecAccessConstraintRef constraint = CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclConstraintBio), bioDict, NULL);
    CFReleaseSafe(bioDict);
    return constraint;
}

SecAccessConstraintRef SecAccessConstraintCreateTouchIDCurrentSet(CFAllocatorRef allocator, CFDataRef catacombUUID, CFDataRef bioDbHash) {
    CFMutableDictionaryRef bioDict = CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclParamBioCatacombUUID), catacombUUID, NULL);
    CFDictionarySetValue(bioDict, CFSTR(kACMKeyAclParamBioDatabaseHash), bioDbHash);
    SecAccessConstraintRef constraint = CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclConstraintBio), bioDict, NULL);
    CFReleaseSafe(bioDict);
    return constraint;
}

static SecAccessConstraintRef SecAccessConstraintCreateValueOfKofN(CFAllocatorRef allocator, size_t numRequired, CFArrayRef constraints, CFErrorRef *error) {
    CFNumberRef k = CFNumberCreateWithCFIndex(allocator, numRequired);
    CFMutableDictionaryRef kofn = CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclParamKofN), k, NULL);
    CFRelease(k);

    /* Populate kofn dictionary with constraint keys from the array. note that for now we just ignore any additional
       constraint parameters, but we might err-out if some parameter is found, since we cannot propagate parameteres
       into k-of-n dictionary. */
    const CFTypeRef keysToCopy[] = { CFSTR(kACMKeyAclConstraintBio), CFSTR(kACMKeyAclConstraintPolicy),
        CFSTR(kACMKeyAclConstraintUserPasscode) };
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

    return kofn;

errOut:
    SecError(errSecParam, error, CFSTR("SecAccessControl: invalid constraint for k-of-n"));
    CFReleaseSafe(kofn);
    return NULL;
}

SecAccessConstraintRef SecAccessConstraintCreateKofN(CFAllocatorRef allocator, size_t numRequired, CFArrayRef constraints, CFErrorRef *error) {
    SecAccessConstraintRef valueOfKofN =  SecAccessConstraintCreateValueOfKofN(allocator, numRequired, constraints, error);
    require_quiet(valueOfKofN, errOut);

    SecAccessConstraintRef constraint = CFDictionaryCreateMutableForCFTypesWith(allocator, CFSTR(kACMKeyAclConstraintKofN), valueOfKofN, NULL);
    CFReleaseSafe(valueOfKofN);
    return constraint;

errOut:
    return NULL;
}

bool SecAccessControlAddConstraintForOperation(SecAccessControlRef access_control, CFTypeRef operation, CFTypeRef constraint, CFErrorRef *error) {
    CheckItemInArray(operation, ItemArray(kAKSKeyOpEncrypt, kAKSKeyOpDecrypt,
#if TARGET_OS_IPHONE || (!RC_HIDE_J79 && !RC_HIDE_J80)
                                          kAKSKeyOpSign, kAKSKeyOpAttest, kAKSKeyOpComputeKey,
#endif
                                          kAKSKeyOpSync, kAKSKeyOpDefaultAcl, kAKSKeyOpDelete),
                     "SecAccessControl: invalid operation %@");
    if (!isDictionary(constraint) && !CFEqual(constraint, kCFBooleanTrue) && !CFEqual(constraint, kCFBooleanFalse) ) {
        return SecError(errSecParam, error, CFSTR("invalid constraint"));
    }

    CFMutableDictionaryRef constraints = SecAccessControlGetMutableConstraints(access_control);
    CFDictionarySetValue(constraints, operation, constraint);
    return true;
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

CFDataRef SecAccessControlCopyConstraintData(SecAccessControlRef access_control, CFTypeRef operation) {
    SecAccessConstraintRef constraint = SecAccessControlGetConstraint(access_control, operation);

    size_t len = der_sizeof_plist(constraint, NULL);
    CFMutableDataRef encoded = CFDataCreateMutable(0, len);
    CFDataSetLength(encoded, len);
    uint8_t *der_end = CFDataGetMutableBytePtr(encoded);
    const uint8_t *der = der_end;
    der_end += len;
    der_end = der_encode_plist(constraint, NULL, der, der_end);
    if (!der_end) {
        CFReleaseNull(encoded);
    }
    return encoded;
}

CFDictionaryRef SecAccessControlGetConstraints(SecAccessControlRef access_control) {
    return CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
}

void SecAccessControlSetConstraints(SecAccessControlRef access_control, CFDictionaryRef constraints) {
    CFMutableDictionaryRef mutableConstraints = CFDictionaryCreateMutableCopy(CFGetAllocator(access_control), 0, constraints);
    CFDictionarySetValue(access_control->dict, kAKSKeyAcl, mutableConstraints);
    CFReleaseSafe(mutableConstraints);
}

void SecAccessControlSetRequirePassword(SecAccessControlRef access_control, bool require) {
    CFMutableDictionaryRef constraints = SecAccessControlGetMutableConstraints(access_control);
    CFDictionarySetValue(constraints, kAKSKeyAclParamRequirePasscode, require?kCFBooleanTrue:kCFBooleanFalse);
}

bool SecAccessControlGetRequirePassword(SecAccessControlRef access_control) {
    CFMutableDictionaryRef acl = (CFMutableDictionaryRef)CFDictionaryGetValue(access_control->dict, kAKSKeyAcl);
    if (acl) {
        return CFEqualSafe(CFDictionaryGetValue(acl, kAKSKeyAclParamRequirePasscode), kCFBooleanTrue);
    }

    return false;
}

void SecAccessControlSetBound(SecAccessControlRef access_control, bool bound) {
    CFDictionarySetValue(access_control->dict, kSecAccessControlKeyBound, bound ? kCFBooleanTrue : kCFBooleanFalse);
}

bool SecAccessControlIsBound(SecAccessControlRef access_control) {
    CFTypeRef bound = CFDictionaryGetValue(access_control->dict, kSecAccessControlKeyBound);
    return bound != NULL && CFEqualSafe(bound, kCFBooleanTrue);
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
