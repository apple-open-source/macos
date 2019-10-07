/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "KeychainXCTest.h"

#if USE_KEYSTORE
@interface KeychainEntitlementsTest : KeychainXCTest
@end

@implementation KeychainEntitlementsTest

- (void)testNoEntitlements {
    NSDictionary *params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                              (id)kSecClass: (id)kSecClassGenericPassword, (id)kSecAttrLabel: @"label", };

    // Application with no keychain-related entitlements at all, but CopyMatching must work in order to support
    // backward compatibility with smart-card-enabled macos applications (com.apple.token AG is added automatically in this case).
    [self setEntitlements:@{} validated:false];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);

    // However, write access is declined for such application.
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);
}

#if TARGET_OS_OSX
- (void)testInvalidEntitlementsAppID {
    NSDictionary *params;
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label", };

    // Un-validated app-identifier entitlements must disallow any access to the keychain.
    [self setEntitlements:@{ @"com.apple.application-identifier": @"com.apple.test-app-identifier" } validated:NO];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);

    // However, keychain-access-groups entitlements should work even if not validated, AMFI will take care
    // about cases when keychain-access-groups is not correctly used and we have to support cases when
    // process contains application-groups entitlement but that entitlement is not present in provisioned profile, thus
    // failing entitlement validation test.
    [self setEntitlements:@{ @"keychain-access-groups": @[@"com.apple.test-app-identifier"] } validated:NO];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecSuccess);
}
#endif // TARGET_OS_OSX

- (void)testValidEntitlementsAppID {
    NSDictionary *params;
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: @"com.apple.test-app-identifier", };
#if TARGET_OS_OSX
    [self setEntitlements:@{ @"com.apple.application-identifier": @"com.apple.test-app-identifier" } validated:YES];
#else
    [self setEntitlements:@{ @"application-identifier": @"com.apple.test-app-identifier" } validated:YES];
#endif
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecSuccess);
}

- (void)testEntitlementsAssociatedAppID {
    NSMutableDictionary *params = [@{(id)kSecUseDataProtectionKeychain: @YES,
                                     (id)kSecClass: (id)kSecClassGenericPassword,
                                     (id)kSecAttrLabel: @"label" } mutableCopy];

    [self setEntitlements:@{
#if TARGET_OS_OSX
                             @"com.apple.application-identifier": @"com.apple.test-app-identifier",
#else
                             @"application-identifier": @"com.apple.test-app-identifier",
#endif
                             @"com.apple.developer.associated-application-identifier": @[ @"com.apple.test-associated-app-identifier" ]
                             } validated:YES];
    
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    
    // The associated app identifier is preferred over the 'regular' app identifier (in practice this is only relevant on macOS)
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecSuccess);
    params[(id)kSecReturnAttributes] = @(YES);
    CFTypeRef result = NULL;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)params, &result), errSecSuccess);
    XCTAssertTrue(CFEqual(CFDictionaryGetValue(result, kSecAttrAccessGroup), CFSTR("com.apple.test-associated-app-identifier")));
    CFReleaseNull(result);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecSuccess);
}

- (void)testDisallowTokenGroupWrite {
    NSDictionary *params;

    // Explicit com.apple.token agrp is not acceptable for writing operations, but acceptable for reading.
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken, };
    [self setEntitlements:@{ @"com.apple.application-identifier": (id)kSecAttrAccessGroupToken } validated:YES];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);
}

#if TARGET_OS_OSX
- (void)testInvalidAppGroups {
    NSDictionary *params;
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label", };
    [self setEntitlements:@{ @"com.apple.security.application-groups": @[@"com.apple.test-app-groups"] } validated:NO];

    // Invalid access group entitlement should still allow querying com.apple.token
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);

    // But write-access is forbidden,
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);

    // Similarly as explicitly referring to AG specified in unverified entitlements.
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: @"com.apple.test-app-groups", };
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);

    // Explicitly referring to com.apple.token should work fine too.
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken, };
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
}
#endif // TARGET_OS_OSX

- (void)testTokenItemsGroup {
    NSDictionary *params;

    // Add token items for testing into the keychain.
    NSArray *tokenItems = @[ @{
                                 (id)kSecClass: (id)kSecClassGenericPassword,
                                 (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken,
                                 (id)kSecAttrLabel: @"label",
                                 } ];
    XCTAssertEqual(SecItemUpdateTokenItems(@"com.apple.testtoken", (__bridge CFArrayRef)tokenItems), errSecSuccess);

    [self setEntitlements:@{ @"com.apple.application-identifier": @"com.apple.test-app-identifier" } validated:YES];

    // Query without explicit access group, should find item on macOS and not find it on iOS.
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label", };
#if TARGET_OS_IPHONE
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
#else
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecSuccess);
#endif

    // Query with explicit AG should work the same on both platforms.
    params = @{ (id)kSecUseDataProtectionKeychain: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken, };
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecSuccess);

    // Delete all test token items.
    SecItemUpdateTokenItems(@"com.apple.testtoken", (__bridge CFArrayRef)@[]);
}

- (void)testEntitlementForExplicitAccessGroupLacking {
    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier", @"application-identifier": @"com.apple.test-app-identifier"} validated:YES];

    NSMutableDictionary* query = [@{(id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccount : @"TestAccount",
                                    (id)kSecAttrLabel : @"TestLabel",
                                    (id)kSecAttrAccessGroup : @"com.apple.test.myaccessgroup",
                                    (id)kSecValueData : [@"passwd" dataUsingEncoding:NSUTF8StringEncoding],
                                   } mutableCopy];
    NSMutableDictionary* update = [@{(id)kSecAttrLabel : @"NewLabel"} mutableCopy];

    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecMissingEntitlement);
    query[(id)kSecValueData] = nil;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecMissingEntitlement);

    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier", @"application-identifier": @"com.apple.test-app-identifier", @"keychain-access-groups": @[@"com.apple.test.myaccessgroup"]} validated:YES];
    query[(id)kSecValueData] = [@"secret" dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    query[(id)kSecValueData] = nil;

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier"} validated:YES];
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecMissingEntitlement);

    query[(id)kSecAttrAccessGroup] = nil;
    update[(id)kSecAttrAccessGroup] = @"com.apple.test.myotheraccessgroup";
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecMissingEntitlement);
 
    query[(id)kSecAttrAccessGroup] = @"com.apple.test.myaccessgroup";
    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier", @"application-identifier": @"com.apple.test-app-identifier", @"keychain-access-groups": @[@"com.apple.test.myaccessgroup"]} validated:YES];
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess, @"keychain item not deleted after querying without entitlements");
}

- (void)testEntitlementForImplicitAccessGroupLacking {
    NSMutableDictionary* query = [@{(id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccount : @"TestAccount",
                                    (id)kSecAttrLabel : @"TestLabel",
                                    (id)kSecAttrAccessGroup : @"com.apple.test.myaccessgroup",
                                    (id)kSecValueData : [@"passwd" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];
    NSDictionary* update = @{(id)kSecAttrLabel : @"NewLabel"};

    // Have to use explicit access group here or we just get the app identifier
    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier", @"application-identifier": @"com.apple.test-app-identifier", @"keychain-access-groups": @[@"com.apple.test.myaccessgroup"]} validated:YES];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess);

    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier", @"application-identifier": @"com.apple.test-app-identifier"} validated:YES];
    query[(id)kSecValueData] = nil;
    query[(id)kSecAttrAccessGroup] = nil;

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecItemNotFound);
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecItemNotFound);

    [self setEntitlements:@{@"com.apple.application-identifier": @"com.apple.test-app-identifier", @"application-identifier": @"com.apple.test-app-identifier", @"keychain-access-groups": @[@"com.apple.test.myaccessgroup"]} validated:YES];
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update), errSecSuccess);
    query[(id)kSecAttrLabel] = @"NewLabel";

    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)query, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)query), errSecSuccess, @"keychain item not deleted after querying without entitlements");
}

@end

#endif // USE_KEYSTORE
