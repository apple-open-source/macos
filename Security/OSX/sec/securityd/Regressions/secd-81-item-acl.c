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
#include <ACMLib.h>
#include <coreauthd_spi.h>
#include "SecdTestKeychainUtilities.h"
#if TARGET_OS_IPHONE
#include <MobileKeyBag/MobileKeyBag.h>
#endif

#if LA_CONTEXT_IMPLEMENTED
static keybag_handle_t test_keybag;
static const char *passcode1 = "passcode1";
static const char *passcode2 = "passcode2";

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

#if TARGET_OS_IPHONE
static void set_app_password(ACMContextRef acmContext)
{
    CFDataRef appPwdData = CFStringCreateExternalRepresentation(kCFAllocatorDefault, CFSTR("Application password"), kCFStringEncodingUTF8, 0);
    ACMCredentialRef acmCredential = NULL;
    ok_status(ACMCredentialCreate(kACMCredentialTypePassphraseEntered, &acmCredential), "Create ACM credential");
    ACMPassphrasePurpose purpose = kACMPassphrasePurposeGeneral;
    ok_status(ACMCredentialSetProperty(acmCredential, kACMCredentialPropertyPassphrase, CFDataGetBytePtr(appPwdData), CFDataGetLength(appPwdData)), "Set ACM credential property - passphrase");
    ok_status(ACMCredentialSetProperty(acmCredential, kACMCredentialPropertyPassphrasePurpose, &purpose, sizeof(purpose)), "Set ACM credential property - purpose");
    ok_status(ACMContextAddCredentialWithScope(acmContext, acmCredential, kACMScopeContext), "aad ACM credential to ACM context");
    ACMCredentialDelete(acmCredential);
    CFReleaseSafe(appPwdData);
}
#endif

static void item_with_application_password(uint32_t *item_num)
{
#if TARGET_OS_IPHONE
    CFErrorRef (^okBlock)(void)  = ^ {
        return (CFErrorRef)NULL;
    };

    CFErrorRef (^authFailedBlock)(void)  = ^ {
        return createCFError(CFSTR(""), kLAErrorAuthenticationFailed);
    };

    CFMutableDictionaryRef item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, kSecClassInternetPassword, NULL);
    fillItem(item, (*item_num)++);

    LASetErrorCodeBlock(okBlock);
    SecAccessControlRef aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlApplicationPassword, NULL);
    ok(aclRef, "Create SecAccessControlRef");

    ACMContextRef acmContext = NULL;
    ok_status(ACMContextCreate(&acmContext), "Create ACM context");
    set_app_password(acmContext);

    __block CFDataRef credRefData = NULL;
    ACMContextGetExternalForm(acmContext, ^(const void *externalForm, size_t dataBufferLength) {
        credRefData = CFDataCreate(kCFAllocatorDefault, externalForm, dataBufferLength);
    });

    CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);
    CFDictionarySetValue(item, kSecUseCredentialReference, credRefData);
    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
    ok_status(SecItemAdd(item, NULL), "add local - acl with application password");
    ok_status(SecItemCopyMatching(item, NULL), "find local - acl with application password");
    ok_status(SecItemDelete(item), "delete local - acl with application password");

    CFReleaseSafe(aclRef);

    LASetErrorCodeBlock(okBlock);
    aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlUserPresence, NULL);
    SecAccessControlSetRequirePassword(aclRef, true);
    ok(aclRef, "Create SecAccessControlRef");

    CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);
    CFDictionarySetValue(item, kSecUseCredentialReference, credRefData);
    ok_status(SecItemAdd(item, NULL), "add local - acl with application password and user present");
    LASetErrorCodeBlock(authFailedBlock);
    is_status(SecItemCopyMatching(item, NULL), errSecAuthFailed, "find local - acl with application password and user present");
    LASetErrorCodeBlock(okBlock);
    set_app_password(acmContext);
    ok_status(SecItemDelete(item), "delete local - acl with application password and user present");
    CFReleaseSafe(aclRef);

    LASetErrorCodeBlock(okBlock);
    aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlUserPresence, NULL);
    SecAccessControlSetRequirePassword(aclRef, true);
    SecAccessConstraintRef constraint = SecAccessConstraintCreatePolicy(kCFAllocatorDefault, CFSTR(kACMPolicyDeviceOwnerAuthentication), NULL);
    SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDelete, constraint, NULL);
    CFRelease(constraint);
    ok(aclRef, "Create SecAccessControlRef");

    CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);
    CFDictionarySetValue(item, kSecUseCredentialReference, credRefData);
    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
    ok_status(SecItemAdd(item, NULL), "add local - acl with application password and user present");
    LASetErrorCodeBlock(authFailedBlock);
    is_status(SecItemCopyMatching(item, NULL), errSecAuthFailed, "find local - acl with application password and user present");
    set_app_password(acmContext);
    is_status(SecItemDelete(item), errSecAuthFailed, "delete local - acl with application password and user present");

    CFRelease(item);
    CFReleaseSafe(aclRef);

    // Update tests for item with application password:

    // Prepare query for item without ACL.
    item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, kSecClassInternetPassword, NULL);
    fillItem(item, (*item_num)++);
    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);

    // Add test item without ACL and check that it can be found.
    ok_status(SecItemAdd(item, NULL), "add local - no acl");
    ok_status(SecItemCopyMatching(item, NULL), "find local - no acl");

    // Update test item by adding ACL with application password flag.
    CFMutableDictionaryRef update = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAlwaysPrivate, kSecAccessControlApplicationPassword, NULL);
    CFDictionarySetValue(update, kSecAttrAccessControl, aclRef);
    set_app_password(acmContext);
    CFDictionarySetValue(item, kSecUseCredentialReference, credRefData);
    LASetErrorCodeBlock(okBlock);
    ok_status(SecItemUpdate(item, update), "update local - acl with application password");

    LASetErrorCodeBlock(authFailedBlock);
    ok_status(SecItemCopyMatching(item, NULL), "find local - acl with application password");
    CFDictionaryRemoveValue(item, kSecUseCredentialReference);
    is_status(SecItemCopyMatching(item, NULL), errSecAuthFailed, "find local - acl with application password (without ACM context)");
    CFDictionarySetValue(item, kSecUseCredentialReference, credRefData);
    ok_status(SecItemCopyMatching(item, NULL), "find local - acl with application password (with ACM context)");

    // Try to update item with ACL with application password with the same password (it will fail because ACM context is not allowd for update attributes).
    CFDictionarySetValue(update, kSecUseCredentialReference, credRefData);
    LASetErrorCodeBlock(okBlock);
    is_status(SecItemUpdate(item, update), errSecNoSuchAttr, "update local - add application password");

    CFDictionaryRemoveValue(update, kSecUseCredentialReference);
    LASetErrorCodeBlock(okBlock);
    ok_status(SecItemUpdate(item, update), "update local - updated with the same application password");
    LASetErrorCodeBlock(authFailedBlock);
    ok_status(SecItemCopyMatching(item, NULL), "find local - updated with the same application password"); // LA authFailedBlock is not called.

    CFReleaseSafe(aclRef);
    // Update item with ACL without application password.
    aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleAlwaysPrivate, 0, NULL);
    CFDictionarySetValue(update, kSecAttrAccessControl, aclRef);

    LASetErrorCodeBlock(okBlock);
    ok_status(SecItemUpdate(item, update), "update local - remove application password");

    CFDictionaryRemoveValue(item, kSecUseCredentialReference);
    LASetErrorCodeBlock(authFailedBlock);
    ok_status(SecItemCopyMatching(item, NULL), "find local - acl without application password"); // LA authFailedBlock is not called.

    ok_status(SecItemDelete(item), "delete local - acl without application password");

    CFRelease(update);
    CFRelease(item);
    CFReleaseSafe(aclRef);

    ACMContextDelete(acmContext, true);
    CFReleaseSafe(credRefData);
#endif
}

static void item_with_invalid_acl(uint32_t *item_num)
{
    CFErrorRef (^errorParamBlock)(void)  = ^ {
        return createCFError(CFSTR(""), kLAErrorParameter);
    };

    CFMutableDictionaryRef item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, kSecClassInternetPassword, NULL);
    fillItem(item, (*item_num)++);

    SecAccessControlRef invalidAclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
    ok(invalidAclRef, "Create invalid SecAccessControlRef");
    ok(SecAccessControlSetProtection(invalidAclRef, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, NULL), "Set protection");
    CFTypeRef constraint = SecAccessConstraintCreatePolicy(kCFAllocatorDefault, CFSTR("invalidPolicy"), NULL);
    ok(constraint, "Create invalid constraint");
    ok(SecAccessControlAddConstraintForOperation(invalidAclRef, kAKSKeyOpDecrypt, constraint, NULL), "Add invalid constraint");
    CFReleaseSafe(constraint);

    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionarySetValue(item, kSecAttrAccessControl, invalidAclRef);

    LASetErrorCodeBlock(errorParamBlock);
    is_status(SecItemAdd(item, NULL), errSecParam, "do not add local with invalid acl");
    is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find after add failed");

    CFReleaseSafe(invalidAclRef);
    CFRelease(item);
}

static void item_with_acl_caused_maxauth(uint32_t *item_num)
{
    CFErrorRef (^okBlock)(void)  = ^ {
        return (CFErrorRef)NULL;
    };

    CFMutableDictionaryRef item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, kSecClassInternetPassword, NULL);
    fillItem(item, (*item_num)++);

    SecAccessControlRef aclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
    ok(aclRef, "Create SecAccessControlRef");
    ok(SecAccessControlSetProtection(aclRef, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, NULL));
    ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpEncrpyt, kCFBooleanFalse, NULL));

    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);

    __security_simulatecrash_enable(false);

    LASetErrorCodeBlock(okBlock);
    diag("this will cause an internal assert - on purpose");
    is_status(SecItemAdd(item, NULL), errSecAuthFailed, "max auth attempts failed");

    is(__security_simulatecrash_enable(true), 1, "Expecting simcrash max auth threshold passed");

    CFReleaseSafe(aclRef);
    CFRelease(item);
}

static void item_with_akpu(uint32_t *item_num)
{
    CFErrorRef (^okBlock)(void)  = ^ {
        return (CFErrorRef)NULL;
    };

    CFMutableDictionaryRef item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, kSecClassGenericPassword, NULL);
    fillItem(item, (*item_num)++);

    SecAccessControlRef aclRef = SecAccessControlCreate(kCFAllocatorDefault, NULL);
    ok(aclRef, "Create SecAccessControlRef");
    ok(SecAccessControlSetProtection(aclRef, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, NULL));
    ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpEncrpyt, kCFBooleanTrue, NULL));
    ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDecrypt, kCFBooleanTrue, NULL));
    ok(SecAccessControlAddConstraintForOperation(aclRef, kAKSKeyOpDelete, kCFBooleanTrue, NULL));

    CFDictionarySetValue(item, kSecAttrSynchronizable, kCFBooleanFalse);
    CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);

    LASetErrorCodeBlock(okBlock);
    ok_status(SecItemAdd(item, NULL), "add item with akpu");
    ok_status(SecItemCopyMatching(item, NULL), "find item with akpu");
    changePasscode(passcode1, NULL);
    is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find item with akpu");
    is_status(SecItemAdd(item, NULL), errSecAuthFailed, "cannot add item with akpu without passcode");
    changePasscode(NULL, passcode2);
    is_status(SecItemCopyMatching(item, NULL), errSecItemNotFound, "do not find item with akpu");
    ok_status(SecItemAdd(item, NULL), "add item with akpu");

    changePasscode(passcode2, passcode1);
    CFReleaseSafe(aclRef);
    CFRelease(item);
}
#endif

static void item_with_skip_auth_ui(uint32_t *item_num)
{
    CFMutableDictionaryRef item = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSecClass, kSecClassInternetPassword, NULL);
    fillItem(item, (*item_num)++);

    SecAccessControlRef aclRef = SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, kSecAccessControlDevicePasscode, NULL);
    ok(aclRef, "Create SecAccessControlRef");

    CFDictionarySetValue(item, kSecAttrAccessControl, aclRef);
    CFDictionarySetValue(item, kSecUseAuthenticationUI, kSecUseAuthenticationUISkip);
    is_status(SecItemAdd(item, NULL), errSecParam, "add local - invalid kSecUseAuthenticationUISkip");
    is_status(SecItemDelete(item), errSecParam, "delete local - invalid kSecUseAuthenticationUISkip");

    CFReleaseNull(aclRef);
    CFRelease(item);
}

int secd_81_item_acl(int argc, char *const *argv)
{
    uint32_t item_num = 1;
#if LA_CONTEXT_IMPLEMENTED
    secd_test_setup_temp_keychain(__FUNCTION__, ^{
        keybag_state_t state;
        int passcode_len=(int)strlen(passcode1);

        ok(kIOReturnSuccess==aks_create_bag(passcode1, passcode_len, kAppleKeyStoreDeviceBag, &test_keybag), "create keybag");
        ok(kIOReturnSuccess==aks_get_lock_state(test_keybag, &state), "get keybag state");
        ok(!(state&keybag_state_locked), "keybag unlocked");
        SecItemServerSetKeychainKeybag(test_keybag);
    });
#if TARGET_OS_IPHONE
    plan_tests(70);
#else
    plan_tests(29);
#endif
    item_with_skip_auth_ui(&item_num);
    item_with_invalid_acl(&item_num);
    item_with_application_password(&item_num);
    item_with_acl_caused_maxauth(&item_num);
    item_with_akpu(&item_num);
#else
    plan_tests(3);
    item_with_skip_auth_ui(&item_num);
#endif

#if LA_CONTEXT_IMPLEMENTED
    SecItemServerResetKeychainKeybag();
#endif

    return 0;
}
