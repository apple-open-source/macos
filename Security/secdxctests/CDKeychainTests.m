/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import "SecCDKeychain.h"
#import "CKKS.h"
#import "spi.h"
#import "SecItemServer.h"
#import "SecdTestKeychainUtilities.h"
#import "server_security_helpers.h"
#import "SecDbKeychainItemV7.h"
#import "KeychainXCTest.h"
#import "SecCDKeychainManagedItem+CoreDataClass.h"
#import "SecFileLocations.h"
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFKeychain.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#if USE_KEYSTORE

@interface SecCDKeychain (UnitTestingRedeclarations)

@property (readonly, getter=_queue) dispatch_queue_t queue;

- (void)_registerItemTypeForTesting:(SecCDKeychainItemType*)itemType;

- (void)performOnManagedObjectQueue:(void (^)(NSManagedObjectContext* context, NSError* error))block;
- (void)performOnManagedObjectQueueAndWait:(void (^)(NSManagedObjectContext* context, NSError* error))block;
- (SecCDKeychainManagedItem*)fetchManagedItemForPersistentID:(NSUUID*)persistentID withManagedObjectContext:(NSManagedObjectContext*)managedObjectContext error:(NSError**)error;

- (NSData*)_onQueueGetDatabaseKeyDataWithError:(NSError**)error;
- (void)_onQueueDropClassAPersistentStore;

@end

@interface SecCDKeychainItem (UnitTestingRedeclarations)

- (instancetype)initWithManagedItem:(SecCDKeychainManagedItem*)managedItem keychain:(SecCDKeychain*)keychain error:(NSError**)error;

@end

@interface TestItemType : SecCDKeychainItemType
@end

@implementation TestItemType

+ (instancetype)itemType
{
    return [[self alloc] _initWithName:@"TestItem" version:1 primaryKeys:nil syncableKeys:nil];
}

@end

@interface CDKeychainTests : KeychainXCTest
@end

@implementation CDKeychainTests {
    SecCDKeychain* _keychain;
    SFKeychainServerFakeConnection* _connection;
}

- (void)setUp
{
    [super setUp];

    NSURL* persistentStoreURL = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"CDKeychain");
    NSBundle* resourcesBundle = [NSBundle bundleWithPath:@"/System/Library/Keychain/KeychainResources.bundle"];
    NSURL* managedObjectModelURL = [resourcesBundle URLForResource:@"KeychainModel" withExtension:@"momd"];
    _keychain = [[SecCDKeychain alloc] initWithStorageURL:persistentStoreURL modelURL:managedObjectModelURL encryptDatabase:false];
    _connection = [[SFKeychainServerFakeConnection alloc] init];

    self.keychainPartialMock = OCMPartialMock(_keychain);
    [[[[self.keychainPartialMock stub] andCall:@selector(getDatabaseKeyDataithError:) onObject:self] ignoringNonObjectArgs] _onQueueGetDatabaseKeyDataWithError:NULL];

    [_keychain _registerItemTypeForTesting:[TestItemType itemType]];
}

- (void)tearDown
{
    [self.keychainPartialMock stopMocking];
    self.keychainPartialMock = nil;

    [super tearDown];
}

- (NSArray<SecCDKeychainLookupTuple*>*)lookupTuplesForAttributes:(NSDictionary*)attributes
{
    NSMutableArray* lookupTuples = [[NSMutableArray alloc] initWithCapacity:attributes.count];
    [attributes enumerateKeysAndObjectsUsingBlock:^(NSString* key, id value, BOOL* stop) {
        [lookupTuples addObject:[SecCDKeychainLookupTuple lookupTupleWithKey:key value:value]];
    }];
    return lookupTuples;
}

- (void)testInsertAndRetrieveItemWithPersistentID
{
    SecCDKeychainAccessControlEntity* owner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"com.apple.token"];

    NSUUID* persistentID = [NSUUID UUID];
    SecCDKeychainItem* item = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:persistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MySecret"} owner:owner keyclass:key_class_ak];

    XCTestExpectation* insertExpectation = [self expectationWithDescription:@"insert item"];
    [_keychain insertItems:@[item] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should have been able to insert item");
        XCTAssertNil(error, @"should not have gotten an error inserting item");
        [insertExpectation fulfill];
    }];
    [self waitForExpectations:@[insertExpectation] timeout:5.0];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch item"];
    [_keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNotNil(fetchedItem, @"should have been able to fetch item");
        XCTAssertNil(error, @"should not have gotten an error fetching item");
        XCTAssertEqualObjects(fetchedItem, item, @"fetched item should match inserted item");
        [fetchExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchExpectation] timeout:5.0];
}

- (SecCDKeychainItem*)fullItemForItemMetadata:(SecCDKeychainItemMetadata*)metadata
{
    dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);
    __block SecCDKeychainItem* result = nil;
    [metadata fetchFullItemWithKeychain:_keychain withConnection:_connection completionHandler:^(SecCDKeychainItem* item, NSError* error) {
        result = item;
        dispatch_semaphore_signal(semaphore);
    }];
    dispatch_semaphore_wait(semaphore, dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC));
    return result;
}

- (void)testLookupItemByAttribute
{
    NSUUID* firstItemID = [NSUUID UUID];
    NSUUID* secondItemID = [NSUUID UUID];
    NSDictionary* firstItemAttributes = @{@"attribute1" : @"gotIt", @"sharedAttribute" : @"first"};
    NSDictionary* secondItemAttributes = @{@"attribute2" : @"gotIt", @"sharedAttribute" : @"second"};
    NSArray* firstItemLookupAttributes = [self lookupTuplesForAttributes:firstItemAttributes];
    NSArray* secondItemLookupAttributes = [self lookupTuplesForAttributes:secondItemAttributes];

    SecCDKeychainAccessControlEntity* owner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"com.apple.token"];
    SecCDKeychainItem* firstItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:firstItemID attributes:firstItemAttributes lookupAttributes:firstItemLookupAttributes secrets:@{@"data" : @"I'm the first"} owner:owner keyclass:key_class_ak];
    SecCDKeychainItem* secondItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:secondItemID attributes:secondItemAttributes lookupAttributes:secondItemLookupAttributes secrets:@{@"data" : @"I'm the second"} owner:owner keyclass:key_class_ak];

    XCTestExpectation* insertExpection = [self expectationWithDescription:@"insert items"];
    [_keychain insertItems:@[firstItem, secondItem] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"failed to successfully insert items with error: %@", error);
        XCTAssertNil(error, @"encountered error inserting items: %@", error);
        [insertExpection fulfill];
    }];
    [self waitForExpectations:@[insertExpection] timeout:5.0];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch items"];
    [_keychain fetchItemsWithValue:@"gotIt" forLookupKey:@"attribute1" ofType:SecCDKeychainLookupValueTypeString withConnection:_connection completionHandler:^(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error) {
        XCTAssertEqual(items.count, (unsigned long)1, @"did not get the expected number of fetched items; expected 1, but got %d", (int)items.count);
        SecCDKeychainItem* fetchedItem = [self fullItemForItemMetadata:items.firstObject];
        XCTAssertNotNil(fetchedItem, @"failed to fetch item we just inserted with error: %@", error);
        XCTAssertNil(error, @"encountered error fetching item: %@", error);
        XCTAssertEqualObjects(fetchedItem, firstItem, @"fetched item does not match the item we expected to fetch");
        [fetchExpectation fulfill];
    }];

    XCTestExpectation* secondFetchExpectation = [self expectationWithDescription:@"second fetch"];
    [_keychain fetchItemsWithValue:@"first" forLookupKey:@"sharedAttribute" ofType:SecCDKeychainLookupValueTypeString withConnection:_connection completionHandler:^(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error) {
        XCTAssertEqual(items.count, (unsigned long)1, @"did not get the expected number of fetched items; expected 1, but got %d", (int)items.count);
        SecCDKeychainItem* fetchedItem = [self fullItemForItemMetadata:items.firstObject];
        XCTAssertNotNil(fetchedItem, @"failed to fetch item we just inserted with error: %@", error);
        XCTAssertNil(error, @"encountered error fetching item: %@", error);
        XCTAssertEqualObjects(fetchedItem, firstItem, @"fetched item does not match the item we expected to fetch");
        [secondFetchExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchExpectation, secondFetchExpectation] timeout:5.0];
}

- (void)testLookupMultipleItems
{
    NSUUID* firstItemID = [NSUUID UUID];
    NSUUID* secondItemID = [NSUUID UUID];
    NSUUID* thirdUUID = [NSUUID UUID];
    NSDictionary* firstItemAttributes = @{@"attribute1" : @"gotIt", @"sharedAttribute" : @"bothHaveThis"};
    NSDictionary* secondItemAttributes = @{@"attribute2" : @"gotIt", @"sharedAttribute" : @"bothHaveThis"};
    NSDictionary* thirdItemAttributes = @{@"attribute3" : @"gotIt", @"sharedAttribute" : @"somethingDifferent"};
    NSArray* firstItemLookupAttributes = [self lookupTuplesForAttributes:firstItemAttributes];
    NSArray* secondItemLookupAttributes = [self lookupTuplesForAttributes:secondItemAttributes];
    NSArray* thirdItemLookupAttributes = [self lookupTuplesForAttributes:thirdItemAttributes];

    SecCDKeychainAccessControlEntity* owner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"com.apple.token"];
    SecCDKeychainItem* firstItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:firstItemID attributes:firstItemAttributes lookupAttributes:firstItemLookupAttributes secrets:@{@"data" : @"I'm the first"} owner:owner keyclass:key_class_ak];
    SecCDKeychainItem* secondItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:secondItemID attributes:secondItemAttributes lookupAttributes:secondItemLookupAttributes secrets:@{@"data" : @"I'm the second"} owner:owner keyclass:key_class_ak];
    SecCDKeychainItem* thirdItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:thirdUUID attributes:thirdItemAttributes lookupAttributes:thirdItemLookupAttributes secrets:@{@"data" : @"I'm the third"} owner:owner keyclass:key_class_ak];

    XCTestExpectation* insertExpectation = [self expectationWithDescription:@"insert items"];
    [_keychain insertItems:@[firstItem, secondItem] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"failed to successfully insert items with error: %@", error);
        XCTAssertNil(error, @"encountered error inserting items: %@", error);
        [insertExpectation fulfill];
    }];
    [self waitForExpectations:@[insertExpectation] timeout:5.0];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch items"];
    [_keychain fetchItemsWithValue:@"bothHaveThis" forLookupKey:@"sharedAttribute" ofType:SecCDKeychainLookupValueTypeString withConnection:_connection completionHandler:^(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error) {
        XCTAssertEqual(items.count, (unsigned long)2, @"did not get the expected number of fetched items; expected 2, but got %d", (int)items.count);
        XCTAssertNil(error, @"encountered error fetching item: %@", error);
        if (items.count >= 2) {
            SecCDKeychainItem* firstFetchedObject = [self fullItemForItemMetadata:items[0]];
            SecCDKeychainItem* secondFetchedObject = [self fullItemForItemMetadata:items[1]];
            XCTAssertTrue([firstFetchedObject isEqual:firstItem] || [firstFetchedObject isEqual:secondItem], @"first fetched object does not match either of the items we expected to fetch");
            XCTAssertTrue([secondFetchedObject isEqual:firstItem] || [secondFetchedObject isEqual:secondItem], @"second fetched object does not match either of the items we expected to fetch");
            XCTAssertFalse([firstFetchedObject isEqual:secondFetchedObject], @"the objects we got back from our query are unexpectedly equal to each other");
            XCTAssertFalse([items containsObject:thirdItem.metadata], @"found an unexpected item in our fetch results");
        }
        [fetchExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchExpectation] timeout:5.0];
}

- (void)testDuplicates
{
    // TODO: test some duplicate-rejection scenarios, including the case where no primary keys are explicitly defined
}

- (void)testAccessGroups
{
    [_connection setFakeAccessGroups:[NSArray arrayWithObject:@"TestAccessGroup"]];

    // first we'll try inserting in an access group that should be rejected
    SecCDKeychainAccessControlEntity* badOwner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"BadAccessGroup"];
    NSUUID* persistentID = [NSUUID UUID];
    SecCDKeychainItem* badItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:persistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MySecret"} owner:badOwner keyclass:key_class_ak];

    XCTestExpectation* badAccessGroupInsertExpectation = [self expectationWithDescription:@"items insert with bad access group"];
    [_keychain insertItems:@[badItem] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertFalse(success, @"should have rejected insert of item for a bad access group");
        XCTAssertNotNil(error, @"should have encountered an error for inserting with bad access group");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"should have gotten an error in the SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorInvalidAccessGroup, @"should have gotten the SFKeychainErrorInvalidAccessGroup error code");
        [badAccessGroupInsertExpectation fulfill];
    }];
    [self waitForExpectations:@[badAccessGroupInsertExpectation] timeout:5.0];

    // ok, now try an insertion that should succeed
    SecCDKeychainAccessControlEntity* goodOwner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"TestAccessGroup"];
    SecCDKeychainItem* item = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:persistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MySecret"} owner:goodOwner keyclass:key_class_ak];

    XCTestExpectation* insertExpectation = [self expectationWithDescription:@"items inserted"];
    [_keychain insertItems:@[item] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should have been able to insert item");
        XCTAssertNil(error, @"should not have gotten an error inserting item");
        [insertExpectation fulfill];
    }];
    [self waitForExpectations:@[insertExpectation] timeout:5.0];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch item"];
    [_keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNotNil(fetchedItem, @"should have been able to fetch item");
        XCTAssertNil(error, @"should not have gotten error fetching item");
        XCTAssertEqualObjects(fetchedItem, item, @"fetched item should match the inserted item");
        [fetchExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchExpectation] timeout:5.0];

    // now change my access group and see if I get the expected error trying to fetch the item
    [_connection setFakeAccessGroups:[NSArray arrayWithObject:@"NotYourAccessGroup"]];

    XCTestExpectation* badFetchExpectation = [self expectationWithDescription:@"fetch item with bad access group"];
    [_keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNil(fetchedItem, @"should not have gotten item when fetching with a bad access group");
        XCTAssertNotNil(error, @"should have gotten an error fetching item");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"should have gotten an error in the SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorItemNotFound, @"should have gotten the SFKeychainErrorItemNotFound error code");
        [badFetchExpectation fulfill];
    }];
    [self waitForExpectations:@[badFetchExpectation] timeout:5.0];

    // now try to delete the thing to confirm we cannot

    XCTestExpectation* badDeleteExpectation = [self expectationWithDescription:@"delete item with bad access group"];
    [_keychain deleteItemWithPersistentID:persistentID withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertFalse(success, @"should not have succeeded at deleting item we don't have access to");
        XCTAssertNotNil(error, @"should have gotten an error deleting item we don't have access to");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"should have gotten an error in the SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorItemNotFound, @"should have gotten the SFKeychainErrorItemNotFound error code");
        [badDeleteExpectation fulfill];
    }];
    [self waitForExpectations:@[badDeleteExpectation] timeout:5.0];

    // switch to the good access group to make sure it's still there
    [_connection setFakeAccessGroups:[NSArray arrayWithObject:@"TestAccessGroup"]];

    XCTestExpectation* fetchAgainExpecation = [self expectationWithDescription:@"fetch item again"];
    [_keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNotNil(fetchedItem, @"should have been able to fetch item");
        XCTAssertNil(error, @"should not have gotten error fetching item");
        XCTAssertEqualObjects(fetchedItem, item, @"fetched item should match the inserted item");
        [fetchAgainExpecation fulfill];
    }];
    [self waitForExpectations:@[fetchAgainExpecation] timeout:5.0];

    // and finally delete with proper permissions

    XCTestExpectation* deleteExpectation = [self expectationWithDescription:@"delete item"];
    [_keychain deleteItemWithPersistentID:persistentID withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should have succeeded deleting item");
        XCTAssertNil(error, @"should not have gotten error deleting item: %@", error);
        [deleteExpectation fulfill];
    }];
    [self waitForExpectations:@[deleteExpectation] timeout:5.0];

    XCTestExpectation* postDeleteFetchExpectation = [self expectationWithDescription:@"fetch item after delete"];
    [_keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNil(fetchedItem, @"should not have gotten item when fetching with a bad access group");
        XCTAssertNotNil(error, @"should have gotten error for fetch of deleted item");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"should have gotten an error in the SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorItemNotFound, @"should have gotten the SFKeychainErrorItemNotFound error code");
        [postDeleteFetchExpectation fulfill];
    }];
    [self waitForExpectations:@[postDeleteFetchExpectation] timeout:5.0];
}

- (void)testDifferentOwnersWithSimilarItems
{
    [_connection setFakeAccessGroups:[NSArray arrayWithObject:@"FirstAccessGroup"]];
    SecCDKeychainAccessControlEntity* firstOwner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"FirstAccessGroup"];

    NSUUID* firstPersistentID = [NSUUID UUID];
    SecCDKeychainItem* firstItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:firstPersistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MyFirstSecret"} owner:firstOwner keyclass:key_class_ak];

    XCTestExpectation* firstInsertExpectation = [self expectationWithDescription:@"insert first item"];
    [_keychain insertItems:@[firstItem] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should have been able to insert item");
        XCTAssertNil(error, @"should not have gotten an error inserting item");
        [firstInsertExpectation fulfill];
    }];
    [self waitForExpectations:@[firstInsertExpectation] timeout:5.0];

    [_connection setFakeAccessGroups:[NSArray arrayWithObject:@"SecondAccessGroup"]];
    SecCDKeychainAccessControlEntity* secondOwner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"SecondAccessGroup"];

    NSUUID* secondPersistentID = [NSUUID UUID];
    SecCDKeychainItem* secondItem = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:secondPersistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MySecondSecret"} owner:secondOwner keyclass:key_class_ak];

    XCTestExpectation* secondInsertExpectation = [self expectationWithDescription:@"insert item"];
    [_keychain insertItems:@[secondItem] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should have been able to insert item");
        XCTAssertNil(error, @"should not have gotten an error inserting item");
        [secondInsertExpectation fulfill];
    }];
    [self waitForExpectations:@[secondInsertExpectation] timeout:5.0];

    // now try fetching the first and second items
    // we should have access to the second but not the first

    XCTestExpectation* firstFetchExpectation = [self expectationWithDescription:@"fetch first item"];
    [_keychain fetchItemForPersistentID:firstPersistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNil(fetchedItem, @"should not have been able to fetch item from first access group");
        XCTAssertNotNil(error, @"should have gotten an error fetching item");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"should have gotten an error in the SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorItemNotFound, @"should have gotten the SFKeychainErrorItemNotFound error code");
        [firstFetchExpectation fulfill];
    }];
    [self waitForExpectations:@[firstFetchExpectation] timeout:5.0];

    XCTestExpectation* secondFetchExpectation = [self expectationWithDescription:@"fetch second item"];
    [_keychain fetchItemForPersistentID:secondPersistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNotNil(fetchedItem, @"should have been able to fetch item");
        XCTAssertNil(error, @"should not have gotten an error fetching item");
        XCTAssertEqualObjects(fetchedItem, secondItem, @"fetched item should match inserted item");
        [secondFetchExpectation fulfill];
    }];
    [self waitForExpectations:@[secondFetchExpectation] timeout:5.0];

    // finally, do a search; make sure it returns exactly one result
    XCTestExpectation* searchExpectation = [self expectationWithDescription:@"search items"];
    [_keychain fetchItemsWithValue:@"value" forLookupKey:@"key" ofType:SecCDKeychainLookupValueTypeString withConnection:_connection completionHandler:^(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error) {
        XCTAssertEqual(items.count, (unsigned long)1, @"should have gotten 1 search result, but got %d", (int)items.count);
        SecCDKeychainItem* fetchedItem = [self fullItemForItemMetadata:items.firstObject];
        XCTAssertNotNil(fetchedItem, @"should have been able to fetch full item");
        XCTAssertNil(error, @"should not have gotten error looking up item");
        XCTAssertEqualObjects(fetchedItem, secondItem, @"item we looked up should match the one we inserted");
        [searchExpectation fulfill];
    }];
    [self waitForExpectations:@[searchExpectation] timeout:5.0];
}

- (void)testTamperedMetadata
{
    SecCDKeychainAccessControlEntity* owner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"com.apple.token"];

    NSUUID* persistentID = [NSUUID UUID];
    SecCDKeychainItem* item = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:persistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MyFirstSecret"} owner:owner keyclass:key_class_ak];

    XCTestExpectation* insertExpectation = [self expectationWithDescription:@"insert item"];
    [_keychain insertItems:@[item] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should have been able to insert item");
        XCTAssertNil(error, @"should not have gotten an error inserting item");
        [insertExpectation fulfill];
    }];
    [self waitForExpectations:@[insertExpectation] timeout:5.0];

    XCTestExpectation* tamperExpectation = [self expectationWithDescription:@"tamper with item"];
    [_keychain performOnManagedObjectQueue:^(NSManagedObjectContext* managedObjectContext, NSError* managedObjectError) {
        XCTAssertNotNil(managedObjectContext, @"should have gotten a managed object to perform work with");
        XCTAssertNil(managedObjectError, @"should not get error performing a block on the managed object context");

        NSError* error = nil;
        SecCDKeychainManagedItem* managedItem = [self->_keychain fetchManagedItemForPersistentID:persistentID withManagedObjectContext:managedObjectContext error:&error];
        XCTAssertNotNil(managedItem, @"should have been able to fetch the managed item we just inserted");
        XCTAssertNil(error, @"should not have gotten an error fetching the managed item");

        // ok, now let's do something nefarious, like change the metadata
        NSDictionary* fakeMetadata = @{@"key" : @"differentValue"};
        managedItem.metadata = [NSPropertyListSerialization dataWithPropertyList:fakeMetadata format:NSPropertyListXMLFormat_v1_0 options:9 error:&error];
        XCTAssertNotNil(managedItem.metadata, @"should have been able to write modified metadata");
        XCTAssertNil(error, @"should not have gotten an error writing modified metadata");

        SecCDKeychainItem* brokenItem = [[SecCDKeychainItem alloc] initWithManagedItem:managedItem keychain:self->_keychain error:&error];
        XCTAssertNil(brokenItem, @"should not have been able to create item with tampered metadata");
        XCTAssertNotNil(error, @"should have gotten an error attempting to create an item with tampered metadata");
        XCTAssertEqualObjects(error.domain, SFKeychainErrorDomain, @"should have gotten an error in the SFKeychainErrorDomain");
        XCTAssertEqual(error.code, SFKeychainErrorItemDecryptionFailed, @"should have gotten the SFKeychainErrorItemDecryptionFailed error");

        [tamperExpectation fulfill];
    }];
    [self waitForExpectations:@[tamperExpectation] timeout:5.0];
}

@end

// these tests do not wish to have a keychain already setup
@interface CDKeychainSetupTests : KeychainXCTest
@end

@implementation CDKeychainSetupTests {
    SFKeychainServerFakeConnection* _connection;
}

- (void)setUp
{
    [super setUp];
    
    _connection = [[SFKeychainServerFakeConnection alloc] init];
}

- (void)testKeychainLocking
{
    self.lockState = LockStateLockedAndDisallowAKS;

    NSURL* persistentStoreURL = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"CDKeychain");
    NSBundle* resourcesBundle = [NSBundle bundleWithPath:@"/System/Library/Keychain/KeychainResources.bundle"];
    NSURL* managedObjectModelURL = [resourcesBundle URLForResource:@"KeychainModel" withExtension:@"momd"];
    SecCDKeychain* keychain = [[SecCDKeychain alloc] initWithStorageURL:persistentStoreURL modelURL:managedObjectModelURL encryptDatabase:false];
    XCTAssertNotNil(keychain, @"should have been able to create a keychain instance");

    self.keychainPartialMock = OCMPartialMock(keychain);
    [[[[self.keychainPartialMock stub] andCall:@selector(getDatabaseKeyDataithError:) onObject:self] ignoringNonObjectArgs] _onQueueGetDatabaseKeyDataWithError:NULL];

    SecCDKeychainAccessControlEntity* owner = [SecCDKeychainAccessControlEntity accessControlEntityWithType:SecCDKeychainAccessControlEntityTypeAccessGroup stringRepresentation:@"com.apple.token"];

    NSUUID* persistentID = [NSUUID UUID];
    SecCDKeychainItem* item = [[SecCDKeychainItem alloc] initItemType:[TestItemType itemType] withPersistentID:persistentID attributes:@{@"key" : @"value"} lookupAttributes:nil secrets:@{@"data" : @"MySecret"} owner:owner keyclass:key_class_ak];

    XCTestExpectation* insertExpectation = [self expectationWithDescription:@"insert item"];
    [keychain insertItems:@[item] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertFalse(success, @"should not be able to insert item while locked");
        XCTAssertNotNil(error, @"should get error inserting item while locked");

        // in the future, check for proper error
        // // <rdar://problem/38972671> add SFKeychainErrorDeviceLocked

        [insertExpectation fulfill];
    }];
    [self waitForExpectations:@[insertExpectation] timeout:5.0];

    self.lockState = LockStateUnlocked;

    [keychain _registerItemTypeForTesting:[TestItemType itemType]];

    XCTestExpectation* insertAgainExpectation = [self expectationWithDescription:@"insert item again"];
    [keychain insertItems:@[item] withConnection:_connection completionHandler:^(bool success, NSError* error) {
        XCTAssertTrue(success, @"should be able to insert item after unlock");
        XCTAssertNil(error, @"should not get error inserting after unlock");
        [insertAgainExpectation fulfill];
    }];
    [self waitForExpectations:@[insertAgainExpectation] timeout:5.0];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch item"];
    [keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNotNil(fetchedItem, @"should have been able to fetch item");
        XCTAssertNil(error, @"should not have gotten an error fetching item");
        XCTAssertEqualObjects(fetchedItem, item, @"fetched item should match inserted item");
        [fetchExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchExpectation] timeout:5.0];

    self.lockState = LockStateLockedAndDisallowAKS;
    dispatch_sync(keychain.queue, ^{
        [keychain _onQueueDropClassAPersistentStore];
    });

    XCTestExpectation* fetchWhileLockedExpectation = [self expectationWithDescription:@"fetch item while locked"];
    [keychain fetchItemForPersistentID:persistentID withConnection:_connection completionHandler:^(SecCDKeychainItem* fetchedItem, NSError* error) {
        XCTAssertNil(fetchedItem, @"should not be able to fetch item while locked");
        XCTAssertNotNil(error, @"should get an error fetching item while locked");

        // in the future, check for proper error
        // // <rdar://problem/38972671> add SFKeychainErrorDeviceLocked

        [fetchWhileLockedExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchWhileLockedExpectation] timeout:5.0];
}

@end

#endif
