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

#import "KeychainXCTest.h"
#import "SecItemInternal.h"
#import "SecItemPriv.h"
#import "keychain/securityd/SecItemDb.h"

#import "ipc/server_security_helpers.h"

#if USE_KEYSTORE

@interface KCSharingSPITests : KeychainXCTest
@end

@implementation KCSharingSPITests

+ (void)setUp
{
    [super setUp];
}

- (void)setUp
{
    [super setUp];
    SecServerSetSharedItemNotifier(CFNotificationCenterGetLocalCenter());
    SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingStateEnabled);
}

- (void)tearDown
{
    SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingStateDisabled);
    SecServerSetSharedItemNotifier(NULL);
    [super tearDown];
}

#pragma mark - Sharing Tests

- (void)testShareWithoutReturnResult {
    NSData* firstKey = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrApplicationTag : [@"first-tag" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrLabel : @"test",
        (id)kSecValueData : firstKey,
    }, NULL), errSecSuccess, "Should add first key");

    NSData* secondKey = [@"ghjk" dataUsingEncoding:NSUTF8StringEncoding];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrApplicationTag : [@"second-tag" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrLabel : @"test",
        (id)kSecValueData : secondKey,
    }, NULL), errSecSuccess, "Should add first key");

    CFErrorRef rawError = NULL;
    XCTestExpectation *firstKeyChangeExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray *firstKeyShareResult = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrApplicationTag : [@"first-tag" dataUsingEncoding:NSUTF8StringEncoding],
    }, CFSTR("example-group"), &rawError));
    XCTAssertNil(firstKeyShareResult, "Sharing a single item without a match limit or a return result should return nil");
    XCTAssertNil(CFBridgingRelease(rawError), "Should successfully share first key");
    [self waitForExpectations:@[firstKeyChangeExpectation] timeout:2.5];

    XCTestExpectation *secondKeyChangeExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    id secondKeyShareResult = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrApplicationTag : [@"second-tag" dataUsingEncoding:NSUTF8StringEncoding],

        (id)kSecMatchLimit: (id)kSecMatchLimitOne,
    }, CFSTR("example-group"), &rawError));
    XCTAssertNil(secondKeyShareResult, "Sharing single item with a match limit and without a return result should return nil");
    XCTAssertNil(CFBridgingRelease(rawError), "Should successfully share second key");
    [self waitForExpectations:@[secondKeyChangeExpectation] timeout:2.5];

    XCTestExpectation *bothKeysChangeExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    id bothKeysShareResult = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"test",
    }, CFSTR("other-group"), &rawError));
    XCTAssertNil(bothKeysShareResult, "Sharing multiple items without a match limit or a return result should return nil");
    XCTAssertNil(CFBridgingRelease(rawError), "Should successfully share both keys with different group");
    [self waitForExpectations:@[bothKeysChangeExpectation] timeout:2.5];
}

- (void)testShareSingleItemWithMultipleGroups {
    NSData* key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* attributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,

        (id)kSecReturnAttributes : @YES,
    };
    CFTypeRef rawNewKeyAttributes = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributes, &rawNewKeyAttributes), errSecSuccess, "Should add key");
    NSDictionary *newKeyAttributes = CFBridgingRelease(rawNewKeyAttributes);

    NSDictionary* queryToShare = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecMatchLimit: (id)kSecMatchLimitOne,

        (id)kSecReturnAttributes : @YES,
    };
    CFErrorRef rawError = NULL;
    XCTestExpectation *shareWithExampleChangeExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSDictionary *sharedWithExample = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with group");
    XCTAssertNotNil(sharedWithExample, "Should return shared copy");
    XCTAssertEqualObjects(sharedWithExample[(id)kSecAttrSharingGroup], @"example-group", "Shared copy should have new group");
    [self waitForExpectations:@[shareWithExampleChangeExpectation] timeout:2.5];

    // Share the same query with a different group.
    rawError = NULL;
    XCTestExpectation *shareWithOtherChangeExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSDictionary* sharedWithOther = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("other-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with another group");
    XCTAssertNotNil(sharedWithOther, "Should return copy shared with other group");
    XCTAssertEqualObjects(sharedWithOther[(id)kSecAttrSharingGroup], @"other-group", "Second shared copy should have different group");
    [self waitForExpectations:@[shareWithOtherChangeExpectation] timeout:2.5];

    CFTypeRef rawItems;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
    }, &rawItems), errSecSuccess);
    NSArray<NSDictionary *> *items = CFBridgingRelease(rawItems);
    XCTAssertEqual(items.count, 3);

    NSSet *expectedGroupIDs = [NSSet setWithArray:@[[NSNull null], @"example-group", @"other-group"]];
    NSSet *actualGroupIDs = [NSSet setWithArray:[items valueForKeyPath:(id)kSecAttrSharingGroup]];
    XCTAssertEqualObjects(actualGroupIDs, expectedGroupIDs, "Should return matches for original and shared copies");

    NSPredicate *dataMatches = [NSPredicate predicateWithFormat:@"ALL %K == %@", kSecValueData, key];
    XCTAssertTrue([dataMatches evaluateWithObject:items], "Original and shared copies should have matching data");

    NSPredicate *modificationDateMatches = [NSPredicate predicateWithFormat:@"ALL %K == %@", kSecAttrModificationDate, newKeyAttributes[(id)kSecAttrModificationDate]];
    XCTAssertTrue([modificationDateMatches evaluateWithObject:items], "Original and shared copies should have matching modification dates");
}

- (void)testShareMultipleItemsWithMultipleGroups {
    NSData* firstKey = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* firstKeyAttributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : firstKey,
        (id)kSecAttrApplicationLabel: @"first-key",

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)firstKeyAttributes, NULL), errSecSuccess, "Should add first key");

    NSData* secondKey = [@"ghjk" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary *secondKeyAttributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : secondKey,
        (id)kSecAttrApplicationLabel: @"second-key",

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)secondKeyAttributes, NULL), errSecSuccess, "Should add second key");

    NSDictionary* queryToShare = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        // Omitting `kSecMatchLimit` implies `kSecMatchLimitAll`.

        (id)kSecReturnAttributes : @YES,
    };
    CFErrorRef rawError = NULL;
    XCTestExpectation *shareWithExampleExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray *sharedWithExample = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share multiple items with group");
    XCTAssertEqual(sharedWithExample.count, 2, "Should return array of copies shared with group");
    [self waitForExpectations:@[shareWithExampleExpectation] timeout:2.5];

    NSPredicate *exampleGroupsMatch = [NSPredicate predicateWithFormat:@"ALL %K == %@", kSecAttrSharingGroup, @"example-group"];
    XCTAssertTrue([exampleGroupsMatch evaluateWithObject:sharedWithExample], "Shared copies should have matching groups");

    rawError = NULL;
    XCTestExpectation *shareWithOtherExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray* sharedWithOther = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("other-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share multiple items with another group");
    XCTAssertEqual(sharedWithOther.count, 2, "Should return array of copies shared with other group");
    [self waitForExpectations:@[shareWithOtherExpectation] timeout:2.5];

    NSPredicate *otherGroupsMatch = [NSPredicate predicateWithFormat:@"ALL %K == %@", kSecAttrSharingGroup, @"other-group"];
    XCTAssertTrue([otherGroupsMatch evaluateWithObject:sharedWithOther], "Copies shared with other groups should have matching groups");
}

- (void)testUpdateOriginalItemAndSharedClone {
    NSData* key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* attributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributes, NULL), errSecSuccess, "Should add key");

    NSDictionary* queryToShare = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecReturnAttributes : @YES,
    };
    CFErrorRef rawError = NULL;
    XCTestExpectation *shareWithExampleExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray *sharedWithExample = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with group");
    XCTAssertEqual(sharedWithExample.count, 1, "Should return array of shared copies");
    [self waitForExpectations:@[shareWithExampleExpectation] timeout:2.5];

    NSData* newKey = [@"ghjk" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* queryToUpdate = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
    };
    NSDictionary* attributesToUpdate = @{
        (id)kSecValueData : newKey,
    };
    XCTestExpectation *updateExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)queryToUpdate, (__bridge CFDictionaryRef)attributesToUpdate), errSecSuccess, "Should update key");
    [self waitForExpectations:@[updateExpectation] timeout:2.5];

    CFTypeRef rawUpdatedItems;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecMatchLimit : (id)kSecMatchLimitAll,

        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData : @YES,
    }, &rawUpdatedItems), errSecSuccess);
    NSArray<NSDictionary *> *updatedItems = CFBridgingRelease(rawUpdatedItems);
    XCTAssertEqual(updatedItems.count, 2);

    NSPredicate *newDataMatches = [NSPredicate predicateWithFormat:@"ALL %K == %@", kSecValueData, newKey];
    XCTAssertTrue([newDataMatches evaluateWithObject:updatedItems], "Original and shared copies should have new matching data");
}

- (void)testDeleteSharedClone {
    NSData* key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* attributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributes, NULL), errSecSuccess, "Should add key");

    NSDictionary* queryToShare = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecReturnAttributes : @YES,
    };
    CFErrorRef rawError = NULL;
    XCTestExpectation *shareWithExampleExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray *sharedWithExample = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with group");
    XCTAssertEqual(sharedWithExample.count, 1, "Should return array with one shared copy");
    [self waitForExpectations:@[shareWithExampleExpectation] timeout:2.5];

    rawError = NULL;
    XCTestExpectation *shareWithOtherExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray* sharedWithOther = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("other-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with another group");
    XCTAssertEqual(sharedWithOther.count, 1, "Should return array with one copy shared with other group");
    [self waitForExpectations:@[shareWithOtherExpectation] timeout:2.5];

    NSDictionary *queryToDelete = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecAttrSharingGroup : @"example-group",
    };
    XCTestExpectation *deleteExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)queryToDelete), errSecSuccess, "Should delete first shared copy");
    [self waitForExpectations:@[deleteExpectation] timeout:2.5];

    CFTypeRef rawItems;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecMatchLimit : (id)kSecMatchLimitAll,

        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData : @YES,
    }, &rawItems), errSecSuccess);
    NSArray<NSDictionary *> *items = CFBridgingRelease(rawItems);
    XCTAssertEqual(items.count, 2);

    NSSet *expectedGroupIDs = [NSSet setWithArray:@[[NSNull null], @"other-group"]];
    NSSet *actualGroupIDs = [NSSet setWithArray:[items valueForKeyPath:(id)kSecAttrSharingGroup]];
    XCTAssertEqualObjects(actualGroupIDs, expectedGroupIDs, "Should omit deleted shared copy from results");
}

- (void)testShareAlreadySharedItem {
    NSData* key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* attributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributes, NULL), errSecSuccess, "Should add key");

    NSDictionary* queryToShare = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecReturnAttributes : @YES,
    };
    CFErrorRef rawError = NULL;
    NSArray *sharedWithExample = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with group");
    XCTAssertEqual(sharedWithExample.count, 1, "Should return array with one shared copy");

    rawError = NULL;
    XCTAssertNil(CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError)), "Should not return array on conflict");
    NSError *duplicateError = CFBridgingRelease(rawError);
    XCTAssertEqual(duplicateError.code, errSecDuplicateItem, "Should return duplicate item error if item is already shared with group");
}

- (void)testShareExistingCloneWithNewGroup {
    NSData* key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSDictionary* attributes = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributes, NULL), errSecSuccess, "Should add key");

    NSDictionary* queryToShare = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecReturnAttributes : @YES,
    };
    CFErrorRef rawError = NULL;
    XCTestExpectation *shareWithExampleExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray *sharedWithExample = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShare, CFSTR("example-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share item with group");
    XCTAssertEqual(sharedWithExample.count, 1, "Should return array with one shared copy");
    [self waitForExpectations:@[shareWithExampleExpectation] timeout:2.5];

    NSDictionary* queryToShareExistingSharedCopy = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecAttrSharingGroup : @"example-group",

        (id)kSecReturnAttributes : @YES,
    };
    rawError = NULL;
    XCTestExpectation *shareWithOtherExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    NSArray* sharedWithOther = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)queryToShareExistingSharedCopy, CFSTR("other-group"), &rawError));
    XCTAssertNil(CFBridgingRelease(rawError), "Should share existing shared copy with another group");
    XCTAssertEqual(sharedWithOther.count, 1, "Should return array with one copy shared with other group");
    [self waitForExpectations:@[shareWithOtherExpectation] timeout:2.5];

    CFTypeRef rawItems;
    XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
        (id)kSecAttrLabel : @"origin.example.com",

        (id)kSecMatchLimit : (id)kSecMatchLimitAll,

        (id)kSecReturnAttributes : @YES,
        (id)kSecReturnData : @YES,
    }, &rawItems), errSecSuccess);
    NSArray<NSDictionary *> *items = CFBridgingRelease(rawItems);
    XCTAssertEqual(items.count, 3);

    NSSet *expectedGroupIDs = [NSSet setWithArray:@[[NSNull null], @"example-group", @"other-group"]];
    NSSet *actualGroupIDs = [NSSet setWithArray:[items valueForKeyPath:(id)kSecAttrSharingGroup]];
    XCTAssertEqualObjects(actualGroupIDs, expectedGroupIDs, "Should include original and both shared copies in matches");
}

- (void)testSharePersistentRefs {
    NSData* originalKey = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    CFTypeRef addResult = NULL;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrApplicationTag : [@"first-tag" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : originalKey,

        (id)kSecReturnPersistentRef: @YES,
    }, &addResult), errSecSuccess, "Should add original item");
    id originalPersistentRef = CFBridgingRelease(addResult);
    XCTAssertTrue([originalPersistentRef isKindOfClass:[NSData class]], "Original item should have valid persistent ref");

    CFErrorRef rawError = NULL;
    XCTestExpectation *sharePersistentRefExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    id sharedPersistentRef = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrApplicationTag : [@"first-tag" dataUsingEncoding:NSUTF8StringEncoding],

        (id)kSecMatchLimit: (id)kSecMatchLimitOne,
        (id)kSecReturnPersistentRef: @YES,
    }, CFSTR("example-group"), &rawError));
    XCTAssertTrue([sharedPersistentRef isKindOfClass:[NSData class]], "Shared item should have valid persistent ref");
    XCTAssertNotEqualObjects(originalPersistentRef, sharedPersistentRef, "Original and shared persistent refs should be different");
    XCTAssertNil(CFBridgingRelease(rawError), "Should successfully share original key");
    [self waitForExpectations:@[sharePersistentRefExpectation] timeout:2.5];
}

- (void)testGroupSharingAttribute {
    NSData* key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];

    NSDictionary* attributesToAdd = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,
        (id)kSecAttrSharingGroup : @"example-group",
        (id)kSecReturnAttributes : @YES,
    };
    XCTestExpectation *addExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributesToAdd, NULL), errSecSuccess, "Should allow adding an item with a sharing group");
    [self waitForExpectations:@[addExpectation] timeout:2.5];

    NSDictionary* queryToDeleteInGroup = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecAttrSharingGroup : @"example-group",
    };
    XCTestExpectation *deleteExpectation = [self expectationForNotification:@(kSecServerSharedItemsChangedNotification) object:nil handler:nil];
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)queryToDeleteInGroup), errSecSuccess, "Should allow deleting items shared with a group");
    [self waitForExpectations:@[deleteExpectation] timeout:2.5];

    NSMutableDictionary* attributesToAddWithoutGroup = [attributesToAdd mutableCopy];
    attributesToAddWithoutGroup[(id)kSecAttrSharingGroup] = nil;
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributesToAddWithoutGroup, NULL), errSecSuccess, "Should add an item without a sharing group");

    NSDictionary* queryToUpdate = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
    };
    NSDictionary* attributesToUpdate = @{
        (id)kSecAttrSharingGroup : @"example-group",
    };
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)queryToUpdate, (__bridge CFDictionaryRef)attributesToUpdate), errSecParam, "Should not allow updating an item's sharing group");

    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)queryToDeleteInGroup), errSecItemNotFound, "Should fail deleting item in a group when it doesn't exist");
}

- (void)testMissingKeychainSharingClientEntitlement {
    SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingStateDisabled);

    // We should reject all queries from unentitled clients
    // that explicitly specify a `kSecAttrSharingGroup`.
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],

        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrSharingGroup : @"some-group",
    }, NULL);
    XCTAssertEqual(status, errSecMissingEntitlement);

    status = SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrSharingGroup : @"some-group",
    }, NULL);
    XCTAssertEqual(status, errSecMissingEntitlement);

    status = SecItemUpdate((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",

        (id)kSecAttrSharingGroup : @"some-group",
    }, (__bridge CFDictionaryRef)@{
        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
    });
    XCTAssertEqual(status, errSecMissingEntitlement);

    status = SecItemDelete((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrSharingGroup : @"some-group",
    });
    XCTAssertEqual(status, errSecMissingEntitlement);

    CFErrorRef shareError = NULL;
    CFTypeRef shareResult = SecItemShareWithGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,

        (id)kSecAttrAccount : @"TestAccount",
    }, (__bridge CFStringRef)@"some-group", &shareError);
    XCTAssertNil((__bridge id)shareResult);
    XCTAssertEqual(CFErrorGetCode(shareError), errSecMissingEntitlement);
    CFReleaseNull(shareResult);
    CFReleaseNull(shareError);

    // Add an item, relax the entitlement to allow sharing it, then deny the
    // entitlement again.
    status = SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,

        (id)kSecValueData : [@"password" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccount : @"TestAccount",
        (id)kSecAttrService : @"TestService",

        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
    }, NULL);
    XCTAssertEqual(status, errSecSuccess);

    SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingStateEnabled);
    NSDictionary *sharedItem = CFBridgingRelease(SecItemShareWithGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,

        (id)kSecAttrAccount : @"TestAccount",

        (id)kSecReturnAttributes : @YES,
    }, (__bridge CFStringRef)@"some-group", NULL));
    XCTAssertNotNil(sharedItem);
    SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingStateDisabled);

    NSDictionary *queryForAllTestAccount = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,

        (id)kSecAttrAccount : @"TestAccount",

        (id)kSecReturnAttributes : @YES,
        (id)kSecMatchLimit : (id)kSecMatchLimitAll,
    };

    CFTypeRef rawMatchingItemsWithoutEntitlement = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)queryForAllTestAccount, &rawMatchingItemsWithoutEntitlement);
    XCTAssertEqual(status, errSecSuccess);
    NSArray *matchingItemsWithoutEntitlement = CFBridgingRelease(rawMatchingItemsWithoutEntitlement);
    XCTAssertEqual(matchingItemsWithoutEntitlement.count, 1);
    XCTAssertEqualObjects(matchingItemsWithoutEntitlement.firstObject[(id)kSecAttrAccount], @"TestAccount");
    XCTAssertNil(matchingItemsWithoutEntitlement.firstObject[(id)kSecAttrSharingGroup]);

    SecSecurityClientSetKeychainSharingState(SecSecurityClientKeychainSharingStateEnabled);
    CFTypeRef rawMatchingItemsWithEntitlement = NULL;
    status = SecItemCopyMatching((__bridge CFDictionaryRef)queryForAllTestAccount, &rawMatchingItemsWithEntitlement);
    XCTAssertEqual(status, errSecSuccess);
    NSArray *matchingItemsWithEntitlement = CFBridgingRelease(rawMatchingItemsWithEntitlement);
    XCTAssertEqual(matchingItemsWithEntitlement.count, 2);
    XCTAssertEqualObjects(matchingItemsWithEntitlement.firstObject[(id)kSecAttrAccount], @"TestAccount");
    XCTAssertNil(matchingItemsWithEntitlement.firstObject[(id)kSecAttrSharingGroup]);

    NSPredicate *isTestAccount = [NSPredicate predicateWithFormat:@"ALL %K == 'TestAccount'", kSecAttrAccount];
    XCTAssertTrue([isTestAccount evaluateWithObject:matchingItemsWithEntitlement]);

    NSSet *expectedGroups = [NSSet setWithArray:@[[NSNull null], @"some-group"]];
    NSSet *actualGroups = [NSSet setWithArray:[matchingItemsWithEntitlement valueForKey:(id)kSecAttrSharingGroup]];
    XCTAssertEqualObjects(actualGroups, expectedGroups);
}

- (void)testSharingGroupAttribute {
    NSData *key = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];

    NSDictionary *attributesToAdd = @{
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,

        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : key,
        (id)kSecAttrSharingGroup : @"example-group",

        (id)kSecReturnAttributes : @YES,
    };
    XCTAssertEqual(SecItemAdd((__bridge  CFDictionaryRef)attributesToAdd, NULL), errSecSuccess, "Should allow adding an item with a sharing group");

    NSDictionary *queryToUpdate = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
    };
    NSDictionary *attributesToUpdate = @{
        (id)kSecAttrSharingGroup : @"updated-group",
    };
    XCTAssertEqual(SecItemUpdate((__bridge CFDictionaryRef)queryToUpdate, (__bridge CFDictionaryRef)attributesToUpdate), errSecParam, "Should not allow updating an item's sharing group");

    NSDictionary *queryToDelete = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassKey,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecAttrSharingGroup : @"example-group",
    };
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)queryToDelete), errSecSuccess, "Should allow deleting items with a sharing group");
}

- (void)testSharingBackupRestore {
    NSArray* previousKeychainAccessGroups = (__bridge NSArray*)SecAccessGroupsGetCurrent();
    NSMutableArray* newKeychainAccessGroups = [previousKeychainAccessGroups mutableCopy];
    [newKeychainAccessGroups addObject:@"com.apple.cfnetwork"];
    SecAccessGroupsSetCurrent((__bridge CFArrayRef)newKeychainAccessGroups);

    // Add a shared item with a ggrp
    NSDictionary* attributesToAdd = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecValueData : [@"swordfish" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccessGroup : @"com.apple.cfnetwork",
        (id)kSecAttrSharingGroup : @"example-group",
        (id)kSecAttrServer : @"example.com",
        (id)kSecAttrProtocol : (id)kSecAttrProtocolHTTPS,
        (id)kSecAttrAuthenticationType : (id)kSecAttrAuthenticationTypeHTMLForm,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributesToAdd, NULL), errSecSuccess, "Should allow adding an inet with a sharing group");

    // add a not-synced and not-shared item (no ggrp), which should have its sync bit flipped to 1 by restoring from backup
    attributesToAdd = @{
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel : @"destination.example.com",
        (id)kSecValueData : [@"swordfish" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrAccessGroup : @"com.apple.cfnetwork",
        (id)kSecAttrServer : @"example.com",
        (id)kSecAttrProtocol : (id)kSecAttrProtocolHTTPS,
        (id)kSecAttrAuthenticationType : (id)kSecAttrAuthenticationTypeHTMLForm,
        (id)kSecAttrSynchronizable: @NO,
    };
    XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)attributesToAdd, NULL), errSecSuccess, "Should allow adding an unshared+unsynced inet");

    CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        CFErrorRef cfcferror = NULL;

        keybag_handle_t keybag_none = KEYBAG_NONE;
        NSDictionary* backup = (__bridge_transfer NSDictionary*)SecServerCopyKeychainPlist(dbt, SecSecurityClientGet(), &keybag_none, kSecBackupableItemFilter, &cfcferror);
        XCTAssertNil(CFBridgingRelease(cfcferror), "Shouldn't error creating a 'backup'");
        XCTAssertNotNil(backup, "Creating a 'backup' should have succeeded");

        bool ret = SecServerImportKeychainInPlist(dbt, SecSecurityClientGet(), KEYBAG_NONE, NULL, KEYBAG_DEVICE,
                                                  (__bridge CFDictionaryRef)backup, kSecBackupableItemFilter, false, &cfcferror);

        XCTAssertNil(CFBridgingRelease(cfcferror), "Shouldn't error importing a 'backup'");
        XCTAssert(ret, "Importing a 'backup' should have succeeded");
        return true;
    });
    XCTAssertNil(CFBridgingRelease(cferror), "Shouldn't error mucking about in the db");

    // Make sure we can delete the shared item, asking for _not_ synchronizable, as that bit should not be flipped for KCSharing items
    NSDictionary *queryToDelete = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecAttrLabel : @"origin.example.com",
        (id)kSecAttrSharingGroup : @"example-group",
        (id)kSecAttrSynchronizable: @NO,
    };
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)queryToDelete), errSecSuccess, "Should have found & deleted shared inet w/o sync bit");

    // Make sure we can delete the not-shared-but-now-synced item, asking for synchronizable, as that bit _should_ have be flipped
    queryToDelete = @{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecClass : (id)kSecClassInternetPassword,
        (id)kSecAttrLabel : @"destination.example.com",
        (id)kSecAttrSynchronizable: @YES,
    };
    XCTAssertEqual(SecItemDelete((__bridge CFDictionaryRef)queryToDelete), errSecSuccess, "Should have found & deleted now sync=1 inet");

    SecAccessGroupsSetCurrent((__bridge CFArrayRef)previousKeychainAccessGroups);
}
@end

#endif
