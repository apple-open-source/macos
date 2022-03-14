/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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

#import <Security/Security.h>
#import <Security/SecItemPriv.h>
#include <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>

#import "KeychainXCTest.h"

#if USE_KEYSTORE
@interface KeychainAppClipTests : KeychainXCTest
@end

@implementation KeychainAppClipTests {
    // App Clips are only permitted to store items with agrp == appID, so we set and track it
    NSString* _applicationIdentifier;
}

+ (void)setUp {
    [super setUp];
}

- (void)setUp {
    [super setUp];
    SecSecurityClientAppClipToRegular();
    _applicationIdentifier = @"com.apple.security.appcliptests";
    SecSecurityClientSetApplicationIdentifier((__bridge CFStringRef)_applicationIdentifier);
}

+ (void)tearDown {
    SecSecurityClientAppClipToRegular();
    SecSecurityClientSetApplicationIdentifier(NULL);
    [super tearDown];
}

# pragma mark - Test App Clip API Restrictions (SecItemAdd)

- (void)testAppclipCanAddItem {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
}

- (void)testAppClipAddNoSyncAllowed {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecRestrictedAPI);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

- (void)testAppClipAddNoAgrpAllowed {
    SecSecurityClientRegularToAppClip();
    // By not explicitly setting entitlements we get the default set which is not permitted for an app clip
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecRestrictedAPI);

    SecSecurityClientAppClipToRegular();
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

# pragma mark - Test App Clip API Restrictions (SecItemUpdate)

- (void)testAppClipCanUpdateItem {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];
    SecItemAdd((__bridge CFDictionaryRef)query, NULL);

    NSDictionary* update = @{
        (id)kSecValueData : [@"different" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrService : @"DifferentAppClipTestService",
    };

    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecSuccess);
    query[(id)kSecAttrService] = @"DifferentAppClipTestService";
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
}

- (void)testAppClipUpdateNoSyncAllowed {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];
    SecItemAdd((__bridge CFDictionaryRef)query, NULL);

    NSDictionary* update = @{
        (id)kSecValueData : [@"different" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrSynchronizable : @YES,
    };

    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecRestrictedAPI);
    query[(id)kSecAttrSynchronizable] = @YES;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

- (void)testAppClipUpdateNoAgrpAllowed {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];
    SecItemAdd((__bridge CFDictionaryRef)query, NULL);

    NSDictionary* update = @{
        (id)kSecValueData : [@"different" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccessGroup : @"someotheraccessgroup",
    };

    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecMissingEntitlement);
    query[(id)kSecAttrAccessGroup] = @"someotheraccessgroup";
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecMissingEntitlement);
}

# pragma mark - Test App Clip API Restrictions (SecItemCopyMatching)
// For now SICM doesn't care about sync, because there shouldn't be any items where (clip == 1 && sync == 1)

- (void)testAppClipCopyMatchingNoAgrpsAllowed {
    SecSecurityClientRegularToAppClip();

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnAttributes : @YES,
    };

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecRestrictedAPI);
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

# pragma mark - Test App Clip API Restrictions (SecItemDelete)

- (void)testAppClipDeleteNoAgrpsAllowed {
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
    };

    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecItemNotFound);
    SecSecurityClientRegularToAppClip();
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecRestrictedAPI);
}

# pragma mark - Test App Clip API Restrictions (Misc)

- (void)testAppClipNoSecItemUpdateTokenItemsAllowed {
    SecSecurityClientRegularToAppClip();
    // Don't bother setting it up, app clips aren't even welcome at the door.
    XCTAssertEqual(SecItemUpdateTokenItemsForAccessGroups(NULL, (__bridge CFArrayRef)@[(id)kSecAttrAccessGroupToken], NULL), errSecRestrictedAPI);
}

- (void)testAppClipCanPassAppIDAccessGroup {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"AppClipTestService",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnAttributes : @YES,
        (id)kSecAttrAccessGroup : _applicationIdentifier,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    NSDictionary* update = @{
        (id)kSecValueData : [@"betterpassword" dataUsingEncoding:NSUTF8StringEncoding],
    };

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    query[(id)kSecValueData] = nil;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    query[(id)kSecReturnAttributes] = nil;
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecSuccess);
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess);
}

# pragma mark - Test Item Deletion SPI

- (void)testDeletionSPINoEntitlement {
    XCTAssertEqual(SecItemDeleteKeychainItemsForAppClip((__bridge CFStringRef)@"nonexistent"), errSecMissingEntitlement);
}

- (void)testDeletionSPINoItems {
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);
    XCTAssertEqual(SecItemDeleteKeychainItemsForAppClip((__bridge CFStringRef)@"nonexistent"), errSecSuccess);
}

- (void)testDeletionSPIDeleteAppClipItem {
    // The SPI does not check if app clip, and the API does not check private entitlement
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnAttributes : @YES,
    };

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDeleteKeychainItemsForAppClip((__bridge CFStringRef)_applicationIdentifier), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

- (void)testDeletionSPILeaveRegularItemsAlone {
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);
    NSString* agrp = @"com.apple.keychain.test.notanappclip";
    [self setEntitlements:@{ @"keychain-access-groups": @[agrp] } validated:YES];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessGroup : agrp,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding]
    };

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDeleteKeychainItemsForAppClip((__bridge CFStringRef)agrp), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
}

- (void)testDeletionSPILeaveOtherAppClipItemsAlone {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    NSString* otherAppID = @"not-the-same-application-identifier";
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    [self setEntitlements:@{@"com.apple.application-identifier" : otherAppID} validated:YES];
    SecSecurityClientSetApplicationIdentifier((__bridge CFStringRef)otherAppID);
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    XCTAssertEqual(SecItemDeleteKeychainItemsForAppClip((__bridge CFStringRef)_applicationIdentifier), errSecSuccess);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    SecSecurityClientSetApplicationIdentifier((__bridge CFStringRef)_applicationIdentifier);
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

#pragma mark - Item Promotion SPI

- (void)testPromotionSPINoEntitlement {
    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)@"appClipAppID", (__bridge CFStringRef)@"parentAppID"), errSecMissingEntitlement);
}

- (void)testPromotionSPINoItems {
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);
    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)_applicationIdentifier, (__bridge CFStringRef)@"com.apple.security.parentapp"), errSecSuccess);
}

- (void)testPromotionSPIPromoteAppClipItems {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)_applicationIdentifier, (__bridge CFStringRef)@"com.apple.security.parentapp"), errSecSuccess);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);

    SecSecurityClientAppClipToRegular();
    [self setEntitlements:@{@"com.apple.application-identifier" : @"com.apple.security.parentapp"} validated:YES];

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
}

- (void)testPromotionSPILeaveRegularItemsAlone {
    [self setEntitlements:@{@"com.apple.application-identifier" : @"RegularAppID"} validated:YES];
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)@"RegularAppID", (__bridge CFStringRef)@"OtherAppID"), errSecSuccess);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    [self setEntitlements:@{@"com.apple.application-identifier" : @"OtherAppID"} validated:YES];

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

- (void)testPromotionSPILeaveOtherAppClipItemsAlone {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    SecSecurityClientSetApplicationIdentifier((__bridge CFStringRef)@"DifferentAppClipAppID");
    [self setEntitlements:@{@"com.apple.application-identifier" : @"DifferentAppClipAppID"} validated:YES];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)_applicationIdentifier, (__bridge CFStringRef)@"UnusedParentAppID"), errSecSuccess);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
}

- (void)disabledtestPromotionSPIDeleteAppClipItemsWithACLs {
    // in-memory DBs don't support ACLs, so this is disabled
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    id acl = CFBridgingRelease(SecAccessControlCreateWithFlags(kCFAllocatorDefault, kSecAttrAccessibleWhenUnlocked, kSecAccessControlUserPresence, NULL));
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnAttributes : @YES,
        (id)kSecAttrAccessControl : acl,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)_applicationIdentifier, (__bridge CFStringRef)@"ParentAppID"), errSecSuccess);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);

    [self setEntitlements:@{@"com.apple.application-identifier" : @"ParentAppID"} validated:YES];
    SecSecurityClientAppClipToRegular();
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
}

- (void)testPromotionSPIKeychainLocked {
    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    self.lockState = LockStateLockedAndDisallowAKS;
    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)_applicationIdentifier, (__bridge CFStringRef)@"com.apple.security.parentapp"), errSecInteractionNotAllowed);
}

- (void)testPromotionSPIDuplicateItem {
    [self setEntitlements:@{@"com.apple.application-identifier" : @"ParentAppID"} validated:YES];

    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrService : @"helloworld",
        (id)kSecAttrComment : @"regular",
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    SecSecurityClientRegularToAppClip();
    [self setEntitlements:@{@"com.apple.application-identifier" : _applicationIdentifier} validated:YES];

    NSDictionary* query2 = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrService : @"helloworld",
        (id)kSecAttrComment : @"clip",
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query2, NULL), errSecSuccess);

    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateAppClipDeletion, kCFBooleanTrue);

    // This should replace the old item with the new one, to be less surprising.
    XCTAssertEqual(SecItemPromoteAppClipItemsToParentApp((__bridge CFStringRef)_applicationIdentifier, (__bridge CFStringRef)@"ParentAppID"), errSecSuccess);

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query2, NULL), errSecItemNotFound);

    [self setEntitlements:@{@"com.apple.application-identifier" : @"ParentAppID"} validated:YES];
    SecSecurityClientAppClipToRegular();

    CFTypeRef result = NULL;
    NSDictionary* query3 = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrService : @"helloworld",
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query3, &result), errSecSuccess);
    if (result) {
        XCTAssertEqual(CFGetTypeID(result), CFDictionaryGetTypeID());
        XCTAssertEqualObjects([(__bridge NSDictionary*)result valueForKey:(id)kSecAttrComment], query2[(id)kSecAttrComment]);
        CFReleaseNull(result);
    }
}

@end
#endif
