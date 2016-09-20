//
//  sec_acl_stress.c
//  Security
//
//  Created by Vratislav Kužela on 20/05/15.
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

#include "testlist.h"

#if TARGET_OS_MAC && !(TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR)
#define USE_KEYSTORE  1
#elif TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#define USE_KEYSTORE  1
#else /* no keystore on this platform */
#define USE_KEYSTORE  0
#endif

#if USE_KEYSTORE
#include <coreauthd_spi.h>
#endif

#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
#include <MobileKeyBag/MobileKeyBag.h>
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

#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
CFStringRef temporaryPasscode = CFSTR("1111");
static bool changePasscode(CFStringRef oldPasscode, CFStringRef newPasscode)
{
    CFDataRef oldPasscodeData = NULL;
    CFDataRef newPasscodeData= NULL;
    if (oldPasscode) {
        oldPasscodeData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, oldPasscode, kCFStringEncodingUTF8, 0x00);
    }

    if (newPasscode) {
        newPasscodeData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, newPasscode, kCFStringEncodingUTF8, 0x00);
    }

    int status = MKBKeyBagChangeSystemSecret(oldPasscodeData, newPasscodeData, NULL);

    CFReleaseSafe(oldPasscodeData);
    CFReleaseSafe(newPasscodeData);
    return status == kMobileKeyBagSuccess;
}
#endif

#if !TARGET_IPHONE_SIMULATOR
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
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrAccount,             kStringItemAttr,
                       kSecAttrService,             kStringItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassInternetPassword)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
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
                       kSecAttrAccessible,          kAccessabilityItemAttr,
                       kSecAttrAccessGroup,         kAccessGroupItemAttr,
                       kSecAttrCertificateType,     kNumberItemAttr,
                       kSecAttrIssuer,              kDataItemAttr,
                       kSecAttrSerialNumber,        kDataItemAttr,
                       kSecAttrSynchronizable,      kBoolItemAttr,
                       NULL);
    } else if (CFEqual(iclass, kSecClassKey)) {
        WithEachString(each,
                       kSecAttrAccessible,          kAccessabilityItemAttr,
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
                       kSecAttrAccessible,          kAccessabilityItemAttr,
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
    for (int num = 0 ; num < 8; ++num) {
        __block CFTypeRef protection = kSecAttrAccessibleWhenUnlocked;
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
                    value = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("acl-stress-string-%d"), num);
                    break;
                case kDataItemAttr:
                {
                    char buf[10];
                    int len = snprintf(buf, sizeof(buf), "acl-stress-data-%d", num);
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
                        kSecAttrAccessibleAlwaysPrivate,
                        kSecAttrAccessibleWhenUnlockedThisDeviceOnly,
                        kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                        kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate,
                    };
                    protection = accessabilites[num % array_size(accessabilites)];
                    break;
                }
                case kAccessGroupItemAttr:
                {
                    CFStringRef accessGroups[] = {
                        NULL,
                        CFSTR("com.apple.security.sos"),          // Secd internally uses this
                    };
                    value = accessGroups[num % array_size(accessGroups)];
                    break;
                }
            }
            if (value)
                CFDictionarySetValue(item, attr, value);
            CFReleaseSafe(value);
        });

        SecAccessControlRef aclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
        ok(aclRef, "Create SecAccessControlRef");
        ok(SecAccessControlSetProtection(aclRef, protection, NULL), "Set protection");
        ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDecrypt, kCFBooleanTrue, NULL), "Set operation decrypt to true");
        ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDelete, kCFBooleanTrue, NULL), "Set operation delete to true");
        ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpEncrypt, kCFBooleanTrue, NULL), "Set operation encrypt to true");

        SecAccessControlRef invalidAclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
        ok(invalidAclRef, "Create invalid SecAccessControlRef");
        ok(SecAccessControlSetProtection(invalidAclRef, protection, NULL), "Set protection");
        CFTypeRef constraint = SecAccessConstraintCreatePolicy(kCFAllocatorDefault, CFSTR("invalidPolicy"), NULL);
        ok(constraint, "Create invalid constraint");
        ok(SecAccessControlAddConstraintForOperation(invalidAclRef, kAKSKeyOpDecrypt, constraint, NULL), "Add invalid constraint");
        CFReleaseSafe(constraint);

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

#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
        assert(protection);
        SecAccessControlRef privateKeyUsageAclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protection, kSecAccessControlPrivateKeyUsage, NULL);
        ok(privateKeyUsageAclRef, "Create SecAccessControlRef for kSecAccessControlPrivateKeyUsage");
        CFDictionarySetValue(item, kSecAttrAccessControl, privateKeyUsageAclRef);
        is_status(SecItemAdd(item, NULL), errSecAuthFailed, "add local - kSecAccessControlPrivateKeyUsage without constraint");
        CFReleaseNull(privateKeyUsageAclRef);

        if(isPasscodeSet) {
            privateKeyUsageAclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protection, kSecAccessControlDevicePasscode | kSecAccessControlPrivateKeyUsage, NULL);
            ok(privateKeyUsageAclRef, "Create SecAccessControlRef for kSecAccessControlPrivateKeyUsage");
            CFDictionarySetValue(item, kSecAttrAccessControl, privateKeyUsageAclRef);
            is_status(SecItemAdd(item, NULL), errSecAuthFailed, "add local - kSecAccessControlPrivateKeyUsage with constraint");
            CFReleaseSafe(privateKeyUsageAclRef);
            SecAccessControlRef aclWithUIRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protection, kSecAccessControlUserPresence, NULL);
            ok(aclWithUIRef, "Create SecAccessControlRef which require UI interaction");

            CFDictionarySetValue(item, kSecAttrAccessControl, aclWithUIRef);
            ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
            CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
            is_status(SecItemCopyMatching(item, NULL), errSecInteractionNotAllowed, "find local  - acl with authentication UI");
            CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUISkip);
            is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "find local  - acl with authentication UI");
            CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow);
            ok_status(SecItemDelete(item), "delete local  - acl with authentication UI");
            is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local  - acl with authentication UI");

            CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
            ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
            is_status(SecItemCopyMatching(item, NULL), errSecInteractionNotAllowed, "find local  - acl with authentication UI");
            ok_status(SecItemDelete(item), "delete local  - acl with authentication UI");
            is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local  - acl with authentication UI");
            CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow);
            CFReleaseSafe(aclWithUIRef);

            aclWithUIRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlUserPresence, NULL);
            ok(aclWithUIRef, "Create SecAccessControlRef which require UI interaction");

            CFDictionarySetValue(item, kSecAttrAccessControl, aclWithUIRef);
            ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
            changePasscode(temporaryPasscode, NULL);
            ok_status(SecItemDelete(item), "delete local - AKPU");
            changePasscode(NULL, temporaryPasscode);
            CFReleaseSafe(aclWithUIRef);
        }
#endif

        CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUISkip);
        is_status(SecItemAdd(item, NULL), errSecParam, "add local - invalid kSecUseAuthenticationUISkip");
        is_status(SecItemDelete(item), errSecParam, "delete local - invalid kSecUseAuthenticationUISkip");

        CFRelease(item);
        CFReleaseSafe(aclRef);
        CFReleaseSafe(invalidAclRef);
    }
}
#endif

int sec_acl_stress(int argc, char *const *argv)
{
#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
    bool removeTemporaryPasscode = false;
    bool isPasscodeSet = false;
    if (MKBGetDeviceLockState(NULL) == kMobileKeyBagDisabled) {
        removeTemporaryPasscode = changePasscode(NULL, temporaryPasscode);
        isPasscodeSet = MKBGetDeviceLockState(NULL) != kMobileKeyBagDisabled;
    }

    plan_tests(isPasscodeSet?288:168);
    tests(isPasscodeSet);

    if (removeTemporaryPasscode) {
        changePasscode(temporaryPasscode, NULL);
    }
#elif TARGET_OS_MAC && !TARGET_IPHONE_SIMULATOR
    plan_tests(152);
    tests(false);
#else
    plan_tests(1);
    ok(true);
#endif


    return 0;
}
