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
#include <securityd/SecItemServer.h>
#include <LocalAuthentication/LAPublicDefines.h>

#include "secd_regressions.h"

#if USE_KEYSTORE
#include <coreauthd_spi.h>
#include "SecdTestKeychainUtilities.h"
#if TARGET_OS_EMBEDDED
#include <MobileKeyBag/MobileKeyBag.h>
#endif

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

extern void LASetErrorCodeBlock(CFErrorRef (^newCreateErrorBlock)(void));

#if LA_CONTEXT_IMPLEMENTED
static keybag_handle_t test_keybag;
static const char *passcode = "password";

static bool changePasscode(const char *old_passcode, const char *new_passcode)
{
    size_t old_passcode_len = 0;
    size_t new_passcode_len = 0;

    if (old_passcode)
        old_passcode_len = strlen(old_passcode);

    if (new_passcode)
        new_passcode_len = strlen(new_passcode);

    kern_return_t status = aks_change_secret(test_keybag, old_passcode, (int)old_passcode_len, new_passcode, (int)new_passcode_len, NULL, NULL);
    return status == 0;
}
#endif

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

#if LA_CONTEXT_IMPLEMENTED
CF_RETURNS_RETAINED
static CFErrorRef createCFError(CFStringRef message, CFIndex code)
{
    const void* keysPtr[1];
    const void* messagesPtr[1];

    keysPtr[0] = kCFErrorLocalizedDescriptionKey;
    messagesPtr[0] = message;
    return CFErrorCreateWithUserInfoKeysAndValues(kCFAllocatorDefault, CFSTR(kLAErrorDomain), code, keysPtr, messagesPtr, 1);
}
#endif

static void fillItem(CFMutableDictionaryRef item, uint32_t num)
{
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
                char buf[50];
                int len = snprintf(buf, sizeof(buf), "acl-stress-data-%d", num);
                value = CFDataCreate(kCFAllocatorDefault, (const UInt8 *)buf, len);
                break;
            }
            case kDateItemAttr:
                value = NULL; // Don't mess with dates on create.
                break;
            case kAccessabilityItemAttr:
            { break; }
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
}

static void tests(bool isPasscodeSet)
{
    CFErrorRef (^okBlock)(void)  = ^ {
        return (CFErrorRef)NULL;
    };

#if LA_CONTEXT_IMPLEMENTED
    CFErrorRef (^errorNotInteractiveBlock)(void)  = ^ {
        return createCFError(CFSTR(""), kLAErrorNotInteractive);
    };
#endif

    CFArrayRef classArray = CFArrayCreateForCFTypes(kCFAllocatorDefault, kSecClassInternetPassword, kSecClassGenericPassword, kSecClassKey, kSecClassCertificate, NULL);
    CFArrayRef protectionClassArray = CFArrayCreateForCFTypes(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, kSecAttrAccessibleAfterFirstUnlock, kSecAttrAccessibleAlwaysPrivate,
                                                         kSecAttrAccessibleWhenUnlockedThisDeviceOnly, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
                                                         kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, NULL);

    __block uint32_t pass = 0;
    CFArrayForEach(classArray, ^(CFTypeRef itemClass) {
        CFArrayForEach(protectionClassArray, ^(CFTypeRef protectionClass) {
            CFMutableDictionaryRef item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, itemClass, NULL);
            fillItem(item, ++pass);

            LASetErrorCodeBlock(okBlock);
            SecAccessControlRef aclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
            ok(aclRef, "Create SecAccessControlRef");
            ok(SecAccessControlSetProtection(aclRef, protectionClass, NULL), "Set protection");
            ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDecrypt, kCFBooleanTrue, NULL), "Set operation decrypt to true");
            ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDelete, kCFBooleanTrue, NULL), "Set operation delete to true");
            ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpEncrypt, kCFBooleanTrue, NULL), "Set operation encrypt to true");

            LASetErrorCodeBlock(okBlock);
            CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);
            CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
#if LA_CONTEXT_IMPLEMENTED
            ok_status(SecItemAdd(item, NULL), "add local ");
            ok_status(SecItemCopyMatching(item, NULL), "find local");
            ok_status(SecItemDelete(item), "delete local");
            is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local");
            CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanTrue);
            is_status(SecItemAdd(item, NULL), errSecParam, "add sync");
            is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find sync");
            CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);

            if(isPasscodeSet) {
                SecAccessControlRef aclWithUIRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protectionClass, kSecAccessControlUserPresence, NULL);
                ok(aclWithUIRef, "Create SecAccessControlRef which require UI interaction");

                CFDictionarySetValue(item, kSecAttrAccessControl, aclWithUIRef);
                ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
                CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
                LASetErrorCodeBlock(errorNotInteractiveBlock);
                is_status(SecItemCopyMatching(item, NULL), errSecInteractionNotAllowed, "find local  - acl with authentication UI");
                CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUISkip);
                is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "find local  - acl with authentication UI");
                CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow);
                LASetErrorCodeBlock(okBlock);
                ok_status(SecItemDelete(item), "delete local  - acl with authentication UI");
                is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local  - acl with authentication UI");

                CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
                ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
                LASetErrorCodeBlock(errorNotInteractiveBlock);
                is_status(SecItemCopyMatching(item, NULL), errSecInteractionNotAllowed, "find local  - acl with authentication UI");
                LASetErrorCodeBlock(okBlock);
                ok_status(SecItemDelete(item), "delete local  - acl with authentication UI");
                is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after delete local  - acl with authentication UI");
                CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow);

                SecAccessControlRef aclWithDeleteConstraintRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, protectionClass, kSecAccessControlUserPresence, NULL);
                ok(aclWithDeleteConstraintRef, "Create SecAccessControlRef which require UI interaction for Delete operation");
                CFTypeRef constraint = SecAccessConstraintCreatePolicy(kCFAllocatorDefault, CFSTR("DeviceOwnerAuthentication"), NULL);
                ok(constraint);
                ok(SecAccessControlAddConstraintForOperation(aclWithDeleteConstraintRef, kAKSKeyOpDelete, constraint, NULL), "Add constraint for operation delete");
                CFReleaseSafe(constraint);

                CFDictionarySetValue(item, kSecAttrAccessControl, aclWithDeleteConstraintRef);
                ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
                CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIFail);
                LASetErrorCodeBlock(errorNotInteractiveBlock);
                is_status(SecItemDelete(item), errSecInteractionNotAllowed, "delete local  - acl with authentication UI");

                if (CFEqual(protectionClass, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly)) {
                    CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUIAllow);
                    ok(changePasscode(passcode, NULL));
                    LASetErrorCodeBlock(okBlock);
                    ok_status(SecItemDelete(item), "delete local  - acl with authentication UI");
                    ok(changePasscode(NULL, passcode));

                    CFReleaseSafe(aclWithUIRef);
                    CFReleaseSafe(aclWithDeleteConstraintRef);

                    aclWithUIRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlUserPresence, NULL);
                    ok(aclWithUIRef, "Create SecAccessControlRef which require UI interaction");

                    CFDictionarySetValue(item, kSecAttrAccessControl, aclWithUIRef);
                    ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
                    changePasscode(passcode, NULL);
                    ok_status(SecItemDelete(item), "delete local - AKPU");
                    changePasscode(NULL, passcode);

                    aclWithDeleteConstraintRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlUserPresence, NULL);
                    ok(aclWithDeleteConstraintRef, "Create SecAccessControlRef which require UI interaction for Delete operation");
                    constraint = SecAccessConstraintCreatePolicy(kCFAllocatorDefault, CFSTR("DeviceOwnerAuthentication"), NULL);
                    ok(constraint);
                    ok(SecAccessControlAddConstraintForOperation(aclWithDeleteConstraintRef, kAKSKeyOpDelete, constraint, NULL), "Add constraint for operation delete");
                    CFReleaseSafe(constraint);

                    CFDictionarySetValue(item, kSecAttrAccessControl, aclWithDeleteConstraintRef);
                    ok_status(SecItemAdd(item, NULL), "add local - acl with authentication UI");
                    changePasscode(passcode, NULL);
                    ok_status(SecItemDelete(item), "delete local - AKPU + prot odel");
                    changePasscode(NULL, passcode);
                    
                    CFReleaseNull(aclWithUIRef);
                    CFReleaseNull(aclWithDeleteConstraintRef);
                }
                CFReleaseNull(aclWithUIRef);
                CFReleaseNull(aclWithDeleteConstraintRef);
            }
#endif
            CFReleaseNull(item);
            CFReleaseNull(aclRef);
        });
    });

    CFRelease(classArray);
    CFRelease(protectionClassArray);
}

int secd_81_item_acl_stress(int argc, char *const *argv)
{
#if LA_CONTEXT_IMPLEMENTED
    secd_test_setup_temp_keychain(__FUNCTION__, ^{
        keybag_state_t state;
        int passcode_len=(int)strlen(passcode);

        ok(kIOReturnSuccess==aks_create_bag(passcode, passcode_len, kAppleKeyStoreDeviceBag, &test_keybag), "create keybag");
        ok(kIOReturnSuccess==aks_get_lock_state(test_keybag, &state), "get keybag state");
        ok(!(state&keybag_state_locked), "keybag unlocked");
        SecItemServerSetKeychainKeybag(test_keybag);
    });

    bool isPasscodeSet = true;
#else
    bool isPasscodeSet = false;
#endif
    
    plan_tests(isPasscodeSet?776:140);

    tests(isPasscodeSet);

#if LA_CONTEXT_IMPLEMENTED
    SecItemServerResetKeychainKeybag();
#endif

    return 0;
}
