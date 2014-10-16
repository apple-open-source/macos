//
//  si-81-item-acl.c
//  sec
//
//  Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
//
//

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecBase.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>
#include <Security/SecAccessControl.h>
#include <Security/SecAccessControlPriv.h>
#include <libaks_acl_cf_keys.h>
#include <LocalAuthentication/LAPublicDefines.h>
#include <LocalAuthentication/LAPrivateDefines.h>

#include "Security_regressions.h"

#if TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_IPHONE_SIMULATOR)
#define USE_KEYSTORE  1
#elif TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
#define USE_KEYSTORE  1
#else /* no keystore on this platform */
#define USE_KEYSTORE  0
#endif

#if USE_KEYSTORE
#include <coreauthd_spi.h>
#endif

enum ItemAttrType {
    kBoolItemAttr,
    kNumberItemAttr,
    kStringItemAttr,
    kDataItemAttr,
    kBlobItemAttr,
    kDateItemAttr,
    kAccessabilityItemAttr,
    kAccessGroupItemAttr,
};

static void WithEachString(void(^each)(CFStringRef attr, enum ItemAttrType atype), ...) {
    va_list ap;
    va_start(ap, each);
    CFStringRef attr;
    while((attr = va_arg(ap, CFStringRef)) != NULL) {
        enum ItemAttrType atype = va_arg(ap, enum ItemAttrType);
        each(attr, atype);
    }
    va_end(ap);
}

static void ItemForEachPKAttr(CFMutableDictionaryRef item, void(^each)(CFStringRef attr, enum ItemAttrType atype)) {
    CFStringRef iclass = CFDictionaryGetValue(item, kSecClass);
    if (!iclass) {
        return;
    } else if (CFEqual(iclass, kSecClassGenericPassword)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrService,             kStringItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassInternetPassword)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrSecurityDomain,      kStringItemAttr,
                       kSecAttrServer,              kStringItemAttr,
                       kSecAttrProtocol,            kNumberItemAttr,
                       kSecAttrAuthenticationType,  kNumberItemAttr,
                       kSecAttrPort,                kNumberItemAttr,
                       kSecAttrPath,                kStringItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassCertificate)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassKey)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrKeyClass,            kStringItemAttr, // kNumberItemAttr on replies
                       kSecAttrApplicationLabel,    kDataItemAttr,
                       kSecAttrApplicationTag,      kDataItemAttr,
                       kSecAttrKeyType,             kNumberItemAttr,
                       kSecAttrKeySizeInBits,       kNumberItemAttr,
                       kSecAttrEffectiveKeySize,    kNumberItemAttr,
                       kSecAttrStartDate,           kDateItemAttr,
                       kSecAttrEndDate,             kDateItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassIdentity)) {
        WithEachString(each,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       kSecAttrKeyClass,            kStringItemAttr, // kNumberItemAttr on replies
                       kSecAttrApplicationLabel,    kDataItemAttr,
                       kSecAttrApplicationTag,      kDataItemAttr,
                       kSecAttrKeyType,             kNumberItemAttr,
                       kSecAttrKeySizeInBits,       kNumberItemAttr,
                       kSecAttrEffectiveKeySize,    kNumberItemAttr,
                       kSecAttrStartDate,           kDateItemAttr,
                       kSecAttrEndDate,             kDateItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    }
}

static CFMutableDictionaryRef ItemCreate(int num) {
    CFStringRef iclass = NULL;
    switch (num % 4) {
        case 0:
            iclass = kSecClassInternetPassword;
            break;
        case 1:
            iclass = kSecClassGenericPassword;
            break;
        case 2:
            iclass = kSecClassKey;
            break;
        case 3:
            iclass = kSecClassCertificate;
            break;
    }
    return CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, iclass, NULL);
}

static void tests(bool isPasscodeSet)
{
    SecAccessControlRef aclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
    ok(aclRef, "Create SecAccessControlRef");
    ok(SecAccessControlSetProtection(aclRef, kSecAttrAccessibleWhenUnlocked, NULL), "Set protection");
    ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDecrypt, kCFBooleanTrue, NULL), "Set operation decrypt to true");
    ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDelete, kCFBooleanTrue, NULL), "Set operation delete to true");

    SecAccessControlRef invalidAclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
    ok(invalidAclRef, "Create invalid SecAccessControlRef");
    ok(SecAccessControlSetProtection(invalidAclRef, kSecAttrAccessibleWhenUnlocked, NULL), "Set protection");
    CFTypeRef constraint = SecAccessConstraintCreatePolicy(CFSTR("invalidPolicy"), NULL);
    ok(constraint, "Create invalid constraint");
    ok(SecAccessControlAddConstraintForOperation(invalidAclRef, kAKSKeyOpDecrypt, constraint, NULL), "Add invalid constraint");
    CFReleaseSafe(constraint);
    
    SecAccessControlRef aclWithUIRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, kSecAccessControlUserPresence, NULL);
    ok(invalidAclRef, "Create SecAccessControlRef which require UI interaction");


    for (int num = 0 ; num < 8; ++num) {
        CFMutableDictionaryRef item = ItemCreate(num);
        ItemForEachPKAttr(item, ^(CFStringRef attr, enum ItemAttrType atype) {
            CFTypeRef value = NULL;
            switch (atype) {
                case kBoolItemAttr:
                    value = (num % 2 == 0 ? kCFBooleanTrue : kCFBooleanFalse);
                    CFRetain(value);
                    break;
                case kNumberItemAttr:
                    value = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &num);
                    break;
                case kStringItemAttr:
                case kBlobItemAttr:
                    value = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("string-%d"), num);
                    break;
                case kDataItemAttr:
                {
                    char buf[10];
                    int len = snprintf(buf, sizeof(buf), "data-%d", num);
                    value = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)buf, len);
                    break;
                }
                case kDateItemAttr:
                    value = NULL; // Don't mess with dates on create.
                    break;
                case kAccessabilityItemAttr:
                {
                    CFStringRef accessabilites[] = {
                        kSecAttrAccessibleWhenUnlocked,
                        kSecAttrAccessibleAfterFirstUnlock,
                        kSecAttrAccessibleAlways,
                        kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                        kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                        kSecAttrAccessibleAlwaysThisDeviceOnly,
                    };
                    value = accessabilites[num % array_size(accessabilites)];
                    break;
                }
                case kAccessGroupItemAttr:
                {
                    CFStringRef accessGroups[] = {
                        NULL,
#if 0
#if NO_SERVER
                        CFSTR("test"),
                        CFSTR("apple"),
                        CFSTR("lockdown-identities"),
#else
                        CFSTR("sync"),
#endif
                        CFSTR("com.apple.security.sos"),          // Secd internally uses this
                        
                        CFSTR("com.apple.security.regressions"),  // SecurityTestApp is in this group.
#endif
                    };
                    value = accessGroups[num % array_size(accessGroups)];
                    break;
                }
            }
            if (value)
                CFDictionarySetValue(item, attr, value);
            CFReleaseSafe(value);
        });

        CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
        CFDictionarySetValue(item, kSecAttrAccessControl, invalidAclRef);
        is_status(SecItemAdd(item, NULL), errSecParam, "do not add local with invalid acl");
        is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after add failed");

        CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);
        ok_status(SecItemAdd(item, NULL), "add local ");
        ok_status(SecItemCopyMatching(item, NULL), "find local");
        ok_status(SecItemDelete(item), "delete local");
        is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local");

        CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanTrue);
        is_status(SecItemAdd(item, NULL), errSecParam, "add sync");
        is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find sync");
        CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
        
        if(isPasscodeSet) {
            CFDictionarySetValue(item, kSecAttrAccessControl, aclWithUIRef);
            CFDictionarySetValue(item, kSecUseNoAuthenticationUI, kCFBooleanTrue);
            ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
            is_status(SecItemCopyMatching(item, NULL), errSecInteractionNotAllowed, "find local  - acl with authentication UI");
            ok_status(SecItemDelete(item), "delete local  - acl with authentication UI");
            is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local  - acl with authentication UI");
        }

        CFRelease(item);
    }

    CFReleaseSafe(aclRef);
    CFReleaseSafe(invalidAclRef);
    CFReleaseSafe(aclWithUIRef);
}

int si_81_item_acl_stress(int argc, char *const *argv)
{
#if USE_KEYSTORE
    CFErrorRef error = NULL;
    SecAccessControlRef aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, kSecAccessControlUserPresence, NULL);
    CFTypeRef authRef = VRCreateNewReferenceWithACMContext(NULL, NULL);
    CFDataRef aclData = SecAccessControlCopyData(aclRef);
    VRValidateACL(authRef, aclData, &error);
    
    bool isPasscodeSet = true;
    if(error && CFEqual(CFErrorGetDomain(error), CFSTR(kLAErrorDomain)) && CFErrorGetCode(error) == kLAErrorPasscodeNotSet)
       isPasscodeSet = false;

    CFReleaseSafe(aclRef);
    CFReleaseSafe(aclData);
    CFReleaseSafe(authRef);
    CFReleaseSafe(error);
#else
    bool isPasscodeSet = false;
#endif
    
    plan_tests(isPasscodeSet?105:73);
    
    tests(isPasscodeSet);
    
    return 0;
}
