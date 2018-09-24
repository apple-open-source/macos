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
    NSDictionary *params = @{ (id)kSecAttrNoLegacy: @YES,
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
    params = @{ (id)kSecAttrNoLegacy: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label", };

    // Un-validated app-identifier entitlements must disallow any access to the keychain.
    [self setEntitlements:@{ @"com.apple.application-identifier": @"com.apple.test-app-identifier" } validated:false];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);

    // However, keychain-access-groups entitlements should work even if not validated, AMFI will take care
    // about cases when keychain-access-groups is not correctly used and we have to support cases when
    // process contains application-groups entitlement but that entitlement is not present in provisioned profile, thus
    // failing entitlement validation test.
    [self setEntitlements:@{ @"keychain-access-groups": @[@"com.apple.test-app-identifier"] } validated:false];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecSuccess);
}
#endif // TARGET_OS_OSX

- (void)testValidEntitlementsAppID {
    NSDictionary *params;
    params = @{ (id)kSecAttrNoLegacy: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: @"com.apple.test-app-identifier", };
#if TARGET_OS_OSX
    [self setEntitlements:@{ @"com.apple.application-identifier": @"com.apple.test-app-identifier" } validated:true];
#else
    [self setEntitlements:@{ @"application-identifier": @"com.apple.test-app-identifier" } validated:true];
#endif
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecSuccess);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecSuccess);
}

- (void)testDisallowTokenGroupWrite {
    NSDictionary *params;

    // Explicit com.apple.token agrp is not acceptable for writing operations, but acceptable for reading.
    params = @{ (id)kSecAttrNoLegacy: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken, };
    [self setEntitlements:@{ @"com.apple.application-identifier": (id)kSecAttrAccessGroupToken } validated:true];
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);
}

#if TARGET_OS_OSX
- (void)testInvalidAppGroups {
    NSDictionary *params;
    params = @{ (id)kSecAttrNoLegacy: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label", };
    [self setEntitlements:@{ @"com.apple.security.application-groups": @[@"com.apple.test-app-groups"] } validated:false];

    // Invalid access group entitlement should still allow querying com.apple.token
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);

    // But write-access is forbidden,
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);

    // Similarly as explicitly referring to AG specified in unverified entitlements.
    params = @{ (id)kSecAttrNoLegacy: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: @"com.apple.test-app-groups", };
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
    XCTAssertEqual(SecItemAdd((CFDictionaryRef)params, NULL), errSecMissingEntitlement);
    XCTAssertEqual(SecItemDelete((CFDictionaryRef)params), errSecMissingEntitlement);

    // Explicitly referring to com.apple.token should work fine too.
    params = @{ (id)kSecAttrNoLegacy: @YES,
                (id)kSecClass: (id)kSecClassGenericPassword,
                (id)kSecAttrLabel: @"label",
                (id)kSecAttrAccessGroup: (id)kSecAttrAccessGroupToken, };
    XCTAssertEqual(SecItemCopyMatching((CFDictionaryRef)params, NULL), errSecItemNotFound);
}
#endif // TARGET_OS_OSX

@end

#endif // USE_KEYSTORE

