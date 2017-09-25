/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#import "CKKSTests.h"
#import <CloudKit/CloudKit.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#include <Security/SecItemPriv.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSControlProtocol.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSAnalyticsLogger.h"
#import "keychain/ckks/CKKSHealKeyHierarchyOperation.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"

#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ckks/tests/CKKSTests.h"

@implementation CloudKitKeychainSyncingTestsBase

- (ZoneKeys*)keychainZoneKeys {
    return self.keys[self.keychainZoneID];
}

// Override our base class
-(NSSet*)managedViewList {
    return [NSSet setWithObject:@"keychain"];
}

+ (void)setUp {
    SecCKKSEnable();
    SecCKKSResetSyncing();
    [super setUp];
}

- (void)setUp {
    [super setUp];

    self.keychainZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"keychain" ownerName:CKCurrentUserDefaultName];
    self.keychainZone = [[FakeCKZone alloc] initZone: self.keychainZoneID];

    SFECKeyPair* keyPair = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    [CKKSManifestInjectionPointHelper registerEgoPeerID:@"MeMyselfAndI" keyPair:keyPair];

    // Wait for the ViewManager to be brought up
    XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:4*NSEC_PER_SEC], "No timeout waiting for SecCKKSInitialize");

    self.keychainView = [[CKKSViewManager manager] findView:@"keychain"];
    XCTAssertNotNil(self.keychainView, "CKKSViewManager created the keychain view");

    // Check that your environment is set up correctly
    XCTAssertFalse([CKKSManifest shouldSyncManifests], "Manifests syncing is disabled");
    XCTAssertFalse([CKKSManifest shouldEnforceManifests], "Manifests enforcement is disabled");
}

+ (void)tearDown {
    [super tearDown];
    SecCKKSResetSyncing();
}

- (void)tearDown {
    // Fetch status, to make sure we can
    NSDictionary* status = [self.keychainView status];
    (void)status;

    self.keychainView = nil;
    self.keychainZoneID = nil;

    [super tearDown];
}

- (FakeCKZone*)keychainZone {
    return self.zones[self.keychainZoneID];
}

- (void)setKeychainZone: (FakeCKZone*) zone {
    self.zones[self.keychainZoneID] = zone;
}

@end

@implementation CloudKitKeychainSyncingTests

#pragma mark - Tests

- (void)testAddItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testActiveTLKS {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    NSDictionary<NSString *,NSString *>* tlks = [[CKKSViewManager manager] activeTLKs];

    XCTAssertEqual([tlks count], (NSUInteger)1, "One TLK");
    XCTAssertNotNil(tlks[@"keychain"], "keychain have a UUID");
}


- (void)testAddMultipleItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testAddItemWithoutUUID {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self.keychainView waitUntilAllOperationsAreFinished];

    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    SecCKKSTestSetDisableAutomaticUUID(false);

    // We then expect an upload of the added item
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And then modified.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testModifyItemImmediately {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testModifyItemPrimaryKey {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And then modified. Since we're changing the "primary key", we expect to delete the old record and upload a new one.
    [self expectCKModifyItemRecords:1 deletedRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:nil];
    [self updateAccountOfGenericPassword: @"new-account-delete-me" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testModifyItemDuringReencrypt {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update, but for the final version of the password
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"third"]];

    // Stop the reencrypt operation from happening
    self.keychainView.holdReencryptOutgoingItemsOperation = [CKKSGroupOperation named:@"reencrypt-hold" withBlock: ^{
        secnotice("ckks", "releasing reencryption hold");
    }];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up the reencryption operation)
    [self releaseCloudKitModificationHold];

    // And wait for this to finish...
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    // And once more to quiesce.
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    // Pause outgoing queue operations to ensure the reencryption operation runs first
    self.keychainView.holdOutgoingQueueOperation = [CKKSGroupOperation named:@"outgoing-hold" withBlock: ^{
        secnotice("ckks", "releasing outgoing-queue hold");
    }];

    [self updateGenericPassword: @"third" account:account];

    // Run the reencrypt items operation to completion.
    [self.operationQueue addOperation: self.keychainView.holdReencryptOutgoingItemsOperation];
    [self.keychainView waitForOperationsOfClass:[CKKSReencryptOutgoingItemsOperation class]];

    [self.operationQueue addOperation: self.keychainView.holdOutgoingQueueOperation];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testModifyItemBeforeReencrypt {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update, but for the final version of the password
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"third"]];

    // Stop the reencrypt operation from happening
    self.keychainView.holdReencryptOutgoingItemsOperation = [CKKSGroupOperation named:@"reencrypt-hold" withBlock: ^{
        secnotice("ckks", "releasing reencryption hold");
    }];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up the reencryption operation)
    [self releaseCloudKitModificationHold];

    // And wait for this to finish...
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    // And once more to quiesce.
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    [self updateGenericPassword: @"third" account:account];

    // Item should upload.
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Run the reencrypt items operation to completion.
    [self.operationQueue addOperation: self.keychainView.holdReencryptOutgoingItemsOperation];
    [self.keychainView waitForOperationsOfClass:[CKKSReencryptOutgoingItemsOperation class]];

    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testModifyItemDuringNetworkFailure {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update, but for the final version of the password
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up uploading the new item)
    [self releaseCloudKitModificationHold];

    // Item should upload.
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testDeleteItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect a single record to be deleted.
    [self expectCKDeleteItemRecords: 1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeleteItemImmediatelyAfterModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now, hold the modify
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Right now, the write in CloudKit is pending. Make the local deletion...
    [self deleteGenericPassword:account];

    // And then schedule the update
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeleteItemAfterFetchAfterModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now, hold the modify
    //[self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Right now, the write in CloudKit is pending. Place a hold on outgoing queue processing
    // Place a hold on processing the outgoing queue.
    CKKSResultOperation* blockOutgoing = [CKKSResultOperation operationWithBlock:^{
        secnotice("ckks", "Outgoing queue hold released.");
    }];
    blockOutgoing.name = @"outgoing-queue-hold";
    CKKSResultOperation* outgoingQueueOperation = [self.keychainView processOutgoingQueueAfter:blockOutgoing ckoperationGroup:nil];

    [self deleteGenericPassword:account];

    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    // Release the CK modification hold
    //[self releaseCloudKitModificationHold];

    // And cause a fetch
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self.operationQueue addOperation:blockOutgoing];
    [outgoingQueueOperation waitUntilFinished];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}


- (void)testReceiveItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };

    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not yet exist");

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");
}

- (void)testReceiveManyItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D01" withAccount:@"account1"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D03" withAccount:@"account3"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D04" withAccount:@"account4"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D05" withAccount:@"account5"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D06" withAccount:@"account6"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D07" withAccount:@"account7"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D08" withAccount:@"account8"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D09" withAccount:@"account9"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D10" withAccount:@"account10"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D11" withAccount:@"account11"]];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];
    [self findGenericPassword: @"account2" expecting:errSecSuccess];
    [self findGenericPassword: @"account3" expecting:errSecSuccess];
    [self findGenericPassword: @"account4" expecting:errSecSuccess];
    [self findGenericPassword: @"account5" expecting:errSecSuccess];
    [self findGenericPassword: @"account6" expecting:errSecSuccess];
    [self findGenericPassword: @"account7" expecting:errSecSuccess];
    [self findGenericPassword: @"account8" expecting:errSecSuccess];
    [self findGenericPassword: @"account9" expecting:errSecSuccess];
    [self findGenericPassword: @"account10" expecting:errSecSuccess];
    [self findGenericPassword: @"account11" expecting:errSecSuccess];
}

- (void)testReceiveCollidingItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };

    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not yet exist");

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName: @"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    CKRecord* ckr2 = [self createFakeRecord: self.keychainZoneID recordName: @"F9C58D31-7B59-481E-98AC-5A507ACB2D85"];

    [self.keychainZone addToZone: ckr];
    [self.keychainZone addToZone: ckr2];

    // We expect a delete operation with the "higher" UUID.
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 6);
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");

    [self waitForCKModifications];
    XCTAssertNil(self.keychainZone.currentDatabase[ckr2.recordID], "Correct record was deleted from CloudKit");
}

-(void)testReceiveItemDelete {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };

    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not yet exist");

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName: @"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");
    CFReleaseNull(item);

    // Trigger a delete
    [self.keychainZone deleteCKRecordIDFromZone: [ckr recordID]];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should no longer exist");
}

-(void)testReceiveItemPhantomDelete {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };

    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not yet exist");

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName: @"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");
    CFReleaseNull(item);

    [self.keychainView waitUntilAllOperationsAreFinished];

    // Trigger a delete
    [self.keychainZone deleteCKRecordIDFromZone: [ckr recordID]];

    // and add another, incorrect IQE
    [self.keychainView dispatchSync: ^bool {
        // Inefficient, but hey, it works
        CKRecord* record = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-FFFF-FFFF-5A507ACB2D85"];
        CKKSItem* fakeItem = [[CKKSItem alloc] initWithCKRecord: record];

        CKKSIncomingQueueEntry* iqe = [[CKKSIncomingQueueEntry alloc] initWithCKKSItem:fakeItem
                                                                                action:SecCKKSActionDelete
                                                                                 state:SecCKKSStateNew];
        XCTAssertNotNil(iqe, "could create fake IQE");
        NSError* error = nil;
        XCTAssert([iqe saveToDatabase: &error], "Saved fake IQE to database");
        XCTAssertNil(error, "No error saving fake IQE to database");
        return true;
    }];

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should no longer exist");

    // The incoming queue should be empty
    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        NSArray* iqes = [CKKSIncomingQueueEntry all:&error];
        XCTAssertNil(error, "No error loading IQEs");
        XCTAssertNotNil(iqes, "Could load IQEs");
        XCTAssertEqual(iqes.count, 0u, "Incoming queue is empty");
    }];
}

-(void)testReceiveConflictOnJustAddedItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self.keychainView waitUntilAllOperationsAreFinished];

    // Place a hold on processing the outgoing queue.
    CKKSResultOperation* blockOutgoing = [CKKSResultOperation operationWithBlock:^{
        secnotice("ckks", "Outgoing queue hold released.");
    }];
    blockOutgoing.name = @"outgoing-queue-hold";
    CKKSResultOperation* outgoingQueueOperation = [self.keychainView processOutgoingQueueAfter:blockOutgoing ckoperationGroup:nil];

    CKKSResultOperation* blockIncoming = [CKKSResultOperation operationWithBlock:^{
        secnotice("ckks", "Incoming queue hold released.");
    }];
    blockIncoming.name = @"incoming-queue-hold";
    CKKSResultOperation* incomingQueueOperation = [self.keychainView processIncomingQueue:false after: blockIncoming];

    [self addGenericPassword:@"localchange" account:@"account-delete-me"];

    // Pull out the new item's UUID.
    __block NSString* itemUUID = nil;
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:&error];
        XCTAssertNil(error, "no error fetching uuids");
        XCTAssertEqual(uuids.count, 1u, "There's exactly one outgoing queue entry");
        itemUUID = uuids[0];

        XCTAssertNotNil(itemUUID, "Have a UUID for our new item");
        return false;
    }];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName: itemUUID]];

    [self.keychainView notifyZoneChange:nil];
    [[self.keychainView.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting] waitUntilFinished];

    // Allow the outgoing queue operation to proceed
    [self.operationQueue addOperation:blockOutgoing];
    [outgoingQueueOperation waitUntilFinished];

    // Allow the incoming queue operation to proceed
    [self.operationQueue addOperation:blockIncoming];
    [incomingQueueOperation waitUntilFinished];

    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.keychainView waitUntilAllOperationsAreFinished];
}

-(void)testReceiveUnknownField {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    NSError* error = nil;

    // Manually encrypt an item
    NSString* recordName = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.keychainZoneID];
    NSDictionary* item = [self fakeRecordDictionary: @"account-delete-me" zoneID:self.keychainZoneID];
    CKKSItem* cipheritem = [[CKKSItem alloc] initWithUUID:recordID.recordName
                                            parentKeyUUID:self.keychainZoneKeys.classA.uuid
                                                   zoneID:recordID.zoneID];
    CKKSKey* itemkey = [CKKSKey randomKeyWrappedByParent: self.keychainZoneKeys.classA error:&error];
    XCTAssertNotNil(itemkey, "Got a key");
    cipheritem.wrappedkey = itemkey.wrappedkey;
    XCTAssertNotNil(cipheritem.wrappedkey, "Got a wrapped key");

    NSData* future_data_field = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSString* future_string_field = @"authstring";
    NSString* future_server_field = @"server_can_change_at_any_time";
    NSNumber* future_number_field = [NSNumber numberWithInt:30];

    // Use version 2, so future fields will be authenticated
    cipheritem.encver = CKKSItemEncryptionVersion2;
    NSMutableDictionary<NSString*, NSData*>* authenticatedData = [[cipheritem makeAuthenticatedDataDictionaryUpdatingCKKSItem: nil encryptionVersion:CKKSItemEncryptionVersion2] mutableCopy];

    authenticatedData[@"future_data_field"] = future_data_field;
    authenticatedData[@"future_string_field"] = [future_string_field dataUsingEncoding:NSUTF8StringEncoding];

    uint64_t n = OSSwapHostToLittleConstInt64([future_number_field unsignedLongValue]);
    authenticatedData[@"future_number_field"] = [NSData dataWithBytes:&n length:sizeof(n)];


    cipheritem.encitem = [CKKSItemEncrypter encryptDictionary:item key:itemkey.aessivkey authenticatedData:authenticatedData error:&error];
    XCTAssertNil(error, "no error encrypting object");
    XCTAssertNotNil(cipheritem.encitem, "Recieved ciphertext");

    CKRecord* ckr = [cipheritem CKRecordWithZoneID: recordID.zoneID];
    ckr[@"future_data_field"] = future_data_field;
    ckr[@"future_string_field"] = future_string_field;
    ckr[@"future_number_field"] = future_number_field;
    ckr[@"server_new_server_field"] = future_server_field;
    [self.keychainZone addToZone:ckr];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    NSDictionary* query = @{(id)kSecClass: (id)kSecClassGenericPassword,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrAccount: @"account-delete-me",
                            (id)kSecMatchLimit: (id)kSecMatchLimitOne,
                            };
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Found synced item");

    // Test that if this item is updated, it remains encrypted in v2, and future_field still exists
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self updateGenericPassword:@"different password" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    CKRecord* newRecord = self.keychainZone.currentDatabase[recordID];
    XCTAssertEqualObjects(newRecord[@"future_data_field"], future_data_field, "future_data_field still exists");
    XCTAssertEqualObjects(newRecord[@"future_string_field"], future_string_field, "future_string_field still exists");
    XCTAssertEqualObjects(newRecord[@"future_number_field"], future_number_field, "future_string_field still exists");
    XCTAssertEqualObjects(newRecord[@"server_new_server_field"], future_server_field, "future_server_field stille exists");

    CKKSItem* newItem = [[CKKSItem alloc] initWithCKRecord:newRecord];
    CKKSAESSIVKey* newItemKey = [self.keychainZoneKeys.classA unwrapAESKey:newItem.wrappedkey error:&error];
    XCTAssertNil(error, "No error unwrapping AES key");
    XCTAssertNotNil(newItemKey, "Have an unwrapped AES key for this item");

    NSDictionary* uploadedData = [CKKSItemEncrypter decryptDictionary:newRecord[SecCKRecordDataKey]
                                                                  key:newItemKey
                                                    authenticatedData:authenticatedData
                                                                error:&error];
    XCTAssertNil(error, "No error decrypting dictionary");
    XCTAssertNotNil(uploadedData, "Authenticated re-uploaded data including future_field");
    XCTAssertEqualObjects(uploadedData[@"v_Data"], [@"different password" dataUsingEncoding:NSUTF8StringEncoding], "Passwords match");
}


-(void)testReceiveRecordEncryptedv1 {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    NSError* error = nil;

    // Manually encrypt an item
    NSString* recordName = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.keychainZoneID];
    NSDictionary* item = [self fakeRecordDictionary: @"account-delete-me" zoneID:self.keychainZoneID];
    CKKSItem* cipheritem = [[CKKSItem alloc] initWithUUID:recordID.recordName
                                            parentKeyUUID:self.keychainZoneKeys.classC.uuid
                                                   zoneID:recordID.zoneID];
    CKKSKey* itemkey = [CKKSKey randomKeyWrappedByParent: self.keychainZoneKeys.classC error:&error];
    XCTAssertNotNil(itemkey, "Got a key");
    cipheritem.wrappedkey = itemkey.wrappedkey;
    XCTAssertNotNil(cipheritem.wrappedkey, "Got a wrapped key");

    cipheritem.encver = CKKSItemEncryptionVersion1;

    NSMutableDictionary<NSString*, NSData*>* authenticatedData = [[cipheritem makeAuthenticatedDataDictionaryUpdatingCKKSItem: nil encryptionVersion:cipheritem.encver] mutableCopy];

    cipheritem.encitem = [CKKSItemEncrypter encryptDictionary:item key:itemkey.aessivkey authenticatedData:authenticatedData error:&error];
    XCTAssertNil(error, "no error encrypting object");
    XCTAssertNotNil(cipheritem.encitem, "Recieved ciphertext");

    [self.keychainZone addToZone:[cipheritem CKRecordWithZoneID: recordID.zoneID]];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    NSDictionary* query = @{(id)kSecClass: (id)kSecClassGenericPassword,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrAccount: @"account-delete-me",
                            (id)kSecMatchLimit: (id)kSecMatchLimitOne,
                            };
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Found synced item");
    CFReleaseNull(cfresult);

    // Test that if this item is updated, it is encrypted in v2
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self updateGenericPassword:@"different password" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 4);
    [self waitForCKModifications];

    CKRecord* newRecord = self.keychainZone.currentDatabase[recordID];
    XCTAssertEqualObjects(newRecord[SecCKRecordEncryptionVersionKey], [NSNumber numberWithInteger:(int) CKKSItemEncryptionVersion2], "Uploaded using encv2");
}

- (void)testUploadPagination {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    for(size_t count = 0; count < 250; count++) {
        [self addGenericPassword: @"data" account: [NSString stringWithFormat:@"account-delete-me-%03lu", count]];
    }

    [self startCKKSSubsystem];

    [self expectCKModifyItemRecords: SecCKKSOutgoingQueueItemsAtOnce currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: SecCKKSOutgoingQueueItemsAtOnce currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: 50 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 160);
}

- (void)testUploadInitialKeyHierarchy {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testUploadInitialKeyHierarchyAfterLockedStart {
    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    while(!([self.keychainView.keyStateMachineOperation isPending] && [self.keychainView.keyStateMachineOperation.dependencies containsObject:self.lockStateTracker.unlockDependency])) {
        sleep(0.1);
    }

    // After unlock, the key hierarchy should be created.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testReceiveKeyHierarchyAfterLockedStart {
    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    while(!([self.keychainView.keyStateMachineOperation isPending] && [self.keychainView.keyStateMachineOperation.dependencies containsObject:self.lockStateTracker.unlockDependency])) {
        sleep(0.1);
    }

    // Now, another device comes along and creates the hierarchy; we download it; and it and sends us the TLK
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    [[self.keychainView.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting] waitUntilFinished];

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    // After unlock, the TLK arrives
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testUploadAndUseKeyHierarchy {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };
    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not exist");

    OCMVerifyAllWithDelay(self.mockDatabase, 1);

    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // now, expect a single class A record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)@{
                                                                         (id)kSecClass : (id)kSecClassGenericPassword,
                                                                         (id)kSecAttrAccessGroup : @"com.apple.security.sos",
                                                                         (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                                                                         (id)kSecAttrAccount : @"account-class-A",
                                                                         (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                                                         (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                                                         }, NULL), @"Adding class A item");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testUploadInitialKeyHierarchyTriggersBackup {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    // We also expect the view manager's notifyNewTLKsInKeychain call to fire (after some delay)
    id mockVM = OCMPartialMock(self.injectedManager);
    OCMExpect([mockVM notifyNewTLKsInKeychain]);

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    OCMVerifyAllWithDelay(mockVM, 10);

    [mockVM stopMocking];
}

- (void)testAcceptExistingKeyHierarchy {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database while it's accepting the keys
    [self.keychainView waitForKeyHierarchyReadiness];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Verify that there are three local keys, and three local current key records
    __weak __typeof(self) weakSelf = self;
    [self.keychainView dispatchSync: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;

        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 3u, "Three keys in local database");

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all: &error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");

        return false;
    }];
}

- (void)testAcceptExistingAndUseKeyHierarchy {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database.
    sleep(1);

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Wait for the key hierarchy to sort itself out, to make it easier on this test; see testOnboardOldItemsWithExistingKeyHierarchy for the other test.
    [self.keychainView waitForKeyHierarchyReadiness];

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.sos",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                                    (id)kSecAttrAccount : @"account-class-A",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    }, NULL), @"Adding class A item");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}



- (void)testAcceptExistingKeyHierarchyDespiteLocked {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)
    // However, the CKKSKeychainView's "checkTLK" method should return a keychain error the first time through, indicating that the keybag is locked
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    id partialKVMock = OCMPartialMock(self.keychainView);
    OCMExpect([partialKVMock checkTLK: [OCMArg any] error: [OCMArg setTo:[[NSError alloc] initWithDomain:@"securityd" code:errSecInteractionNotAllowed userInfo:nil]]]).andReturn(false);

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(partialKVMock, 4);

    // Now that all operations are complete, 'unlock' AKS
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);

    // Verify that there are three local keys, and three local current key records
    __weak __typeof(self) weakSelf = self;
    [self.keychainView dispatchSync: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;

        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 3u, "Three keys in local database");

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all: &error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");

        return false;
    }];

    [partialKVMock stopMocking];
}

- (void)testReceiveClassCWhileALocked {
    // Test starts with a key hierarchy already existing.
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self findGenericPassword:@"classCItem" expecting:errSecItemNotFound];
    [self findGenericPassword:@"classAItem" expecting:errSecItemNotFound];

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    XCTAssertNotNil(self.keychainZoneKeys, "Have zone keys for zone");
    XCTAssertNotNil(self.keychainZoneKeys.classA, "Have class A key for zone");
    XCTAssertNotNil(self.keychainZoneKeys.classC, "Have class C key for zone");

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"classCItem" key:self.keychainZoneKeys.classC]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-FFFF-FFFF-FFFF-5A507ACB2D85" withAccount:@"classAItem" key:self.keychainZoneKeys.classA]];

    CKKSResultOperation* op = [self.keychainView waitForFetchAndIncomingQueueProcessing];
    // The processing op should NOT error, even though it didn't manage to process the classA item
    XCTAssertNil(op.error, "no error while failing to process a class A item");

    CKKSResultOperation* erroringOp = [self.keychainView processIncomingQueue:true];
    [erroringOp waitUntilFinished];
    XCTAssertNotNil(erroringOp.error, "error exists while processing a class A item");

    [self findGenericPassword:@"classCItem" expecting:errSecSuccess];
    [self findGenericPassword:@"classAItem" expecting:errSecItemNotFound];

    self.aksLockState = false;
    [self.lockStateTracker recheck];
    [self.keychainView waitUntilAllOperationsAreFinished];

    [self findGenericPassword:@"classCItem" expecting:errSecSuccess];
    [self findGenericPassword:@"classAItem" expecting:errSecSuccess];
}

- (void)testExternalKeyRoll {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database.
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    __weak __typeof(self) weakSelf = self;

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Trigger a notification
    [self.keychainView notifyZoneChange:nil];

    // Make life easy on this test; testAcceptKeyConflictAndUploadReencryptedItem will check the case when we don't receive the notification
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Verify that there are six local keys, and three local current key records
    [self.keychainView dispatchSync: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;
        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:self.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 6u, "Six keys in local database");

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all: &error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");

        for(CKKSCurrentKeyPointer* key in currentkeys) {
            if([key.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
                XCTAssertEqualObjects(key.currentKeyUUID, strongSelf.keychainZoneKeys.tlk.uuid);
            } else if([key.keyclass isEqualToString: SecCKKSKeyClassA]) {
                XCTAssertEqualObjects(key.currentKeyUUID, strongSelf.keychainZoneKeys.classA.uuid);
            } else if([key.keyclass isEqualToString: SecCKKSKeyClassC]) {
                XCTAssertEqualObjects(key.currentKeyUUID, strongSelf.keychainZoneKeys.classC.uuid);
            } else {
                XCTFail("Unknown key class: %@", key.keyclass);
            }
        }

        return false;
    }];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // TODO: remove this by writing code for item reencrypt after key arrival
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testAcceptKeyConflictAndUploadReencryptedItem {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Do not trigger a notification here. This should cause a conflict updating the current key records

    // We expect a single record to be uploaded, but that the write will be rejected
    // We then expect that item to be reuploaded with the current key

    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under rolled class C key in hierarchy"]];

    // New key arrives via SOS!
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testOnboardOldItems {
    // In this test, we'll check if the CKKS subsystem will pick up a keychain item which existed before the key hierarchy, both with and without a UUID attached at item creation

    // Test starts with nothing in CloudKit, and CKKS blocked. Add one item without a UUID...

    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // We expect an upload of the key hierarchy
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    // We then expect an upload of the added items
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}


- (void)testOnboardOldItemsWithExistingKeyHierarchyExtantTLK {
    // Test starts key hierarchy in our fake CloudKit, the TLK arrived in the local keychain, and CKKS blocked.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Add one item without a UUID...
    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // Now that we have an item in the keychain, allow CKKS to spin up. It should upload the item, using the key hierarchy already extant
    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchyLateTLK {
    // Test starts key hierarchy in our fake CloudKit, and CKKS blocked.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Add one item without a UUID...
    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // Now that we have an item in the keychain, allow CKKS to spin up. It should upload the item, using the key hierarchy already extant

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // No write yet...
    sleep(0.5);
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now, save the TLK to the keychain (to simulate it coming in via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testResync {
    // We need to set up a desynced situation to test our resync.
    // First, let CKKS start up and send several items to CloudKit (that we'll then desync!)
    __block NSError* error = nil;

    // Test starts with keys in CloudKit (so we can create items later)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    [self addGenericPassword: @"data" account: @"third"];
    [self addGenericPassword: @"data" account: @"fourth"];
    NSUInteger passwordCount = 4u;

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
    [self checkGenericPassword: @"data" account: @"third"];
    [self checkGenericPassword: @"data" account: @"fourth"];

    [self expectCKModifyItemRecords: passwordCount currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
    XCTAssertEqual(self.keychainZone.currentDatabase.count, SYSTEM_DB_RECORD_COUNT+passwordCount, "Have 6+passwordCount objects in cloudkit");

    // Now, corrupt away!
    // Extract all passwordCount items for Corruption
    NSArray<CKRecord*>* items = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    XCTAssertEqual(items.count, passwordCount, "Have %lu Items in cloudkit", (unsigned long)passwordCount);

    // For the first record, delete all traces of it from CKKS. But! it remains in local keychain.
    // Expected outcome: CKKS resyncs; item exists again.
    CKRecord* delete = items[0];
    NSString* deleteAccount = [[self decryptRecord: delete] objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(deleteAccount, "received an account for the local delete object");

    __weak __typeof(self) weakSelf = self;
    [self.keychainView dispatchSync:^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        if(ckme) {
            [ckme deleteFromDatabase: &error];
        }
        XCTAssertNil(error, "no error removing CKME");
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        if(oqe) {
            [oqe deleteFromDatabase: &error];
        }
        XCTAssertNil(error, "no error removing OQE");
        CKKSIncomingQueueEntry* iqe = [CKKSIncomingQueueEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        if(iqe) {
            [iqe deleteFromDatabase: &error];
        }
        XCTAssertNil(error, "no error removing IQE");
        return true;
    }];

    // For the second record, delete all traces of it from CloudKit.
    // Expected outcome: deleted locally
    CKRecord* remoteDelete = items[1];
    NSString* remoteDeleteAccount = [[self decryptRecord: remoteDelete] objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(remoteDeleteAccount, "received an account for the remote delete object");

    [self.keychainZone deleteCKRecordIDFromZone: remoteDelete.recordID];
    for(NSMutableDictionary<CKRecordID*, CKRecord*>* database in self.keychainZone.pastDatabases.allValues) {
        [database removeObjectForKey: remoteDelete.recordID];
    }

    // The third record gets modified in CloudKit, but not locally.
    // Expected outcome: use the CloudKit version
    CKRecord* remoteDataChanged = items[2];
    NSMutableDictionary* remoteDataDictionary = [[self decryptRecord: remoteDataChanged] mutableCopy];
    NSString* remoteDataChangedAccount = [remoteDataDictionary objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(remoteDataChangedAccount, "Received an account for the remote-data-changed object");
    remoteDataDictionary[(__bridge id) kSecValueData] = [@"CloudKitWins" dataUsingEncoding: NSUTF8StringEncoding];

    CKRecord* newData = [self newRecord: remoteDataChanged.recordID withNewItemData: remoteDataDictionary];
    [self.keychainZone addToZone: newData];
    for(NSMutableDictionary<CKRecordID*, CKRecord*>* database in self.keychainZone.pastDatabases.allValues) {
        database[remoteDataChanged.recordID] = newData;
    }

    // The fourth record stays in-sync. Good work, everyone!
    // Expected outcome: stays in-sync
    NSString* insyncAccount = [[self decryptRecord: items[3]] objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(insyncAccount, "Received an account for the in-sync object");

    // The fifth record gets magically added to CloudKit, but CKKS has never heard of it
    //  (emulates a lost record on the client, but that CloudKit already believes it's sent the record for)
    // Expected outcome: added to local keychain
    NSString* remoteOnlyAccount = @"remote-only";
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount: remoteOnlyAccount];
    [self.keychainZone addToZone: ckr];
    for(NSMutableDictionary<CKRecordID*, CKRecord*>* database in self.keychainZone.pastDatabases.allValues) {
        database[ckr.recordID] = ckr;
    }

    ckksnotice("ckksresync", self.keychainView, "local delete:        %@ %@", delete.recordID.recordName, deleteAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote deletion:     %@ %@", remoteDelete.recordID.recordName, remoteDeleteAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote data changed: %@ %@", remoteDataChanged.recordID.recordName, remoteDataChangedAccount);
    ckksnotice("ckksresync", self.keychainView, "in-sync:             %@ %@", items[3].recordID.recordName, insyncAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote only:         %@ %@", ckr.recordID.recordName, remoteOnlyAccount);

    CKKSSynchronizeOperation* resyncOperation = [self.keychainView resyncWithCloud];
    [resyncOperation waitUntilFinished];

    XCTAssertNil(resyncOperation.error, "No error during the resync operation");

    // Now do some checking. Remember, we don't know which record we corrupted, so use the parsed account variables to check.

    [self findGenericPassword: deleteAccount            expecting: errSecSuccess];
    [self findGenericPassword: remoteDeleteAccount      expecting: errSecItemNotFound];
    [self findGenericPassword: remoteDataChangedAccount expecting: errSecSuccess];
    [self findGenericPassword: insyncAccount            expecting: errSecSuccess];
    [self findGenericPassword: remoteOnlyAccount        expecting: errSecSuccess];

    [self checkGenericPassword: @"data"         account: deleteAccount];
    //[self checkGenericPassword: @"data"         account: remoteDeleteAccount];
    [self checkGenericPassword: @"CloudKitWins" account: remoteDataChangedAccount];
    [self checkGenericPassword: @"data"         account: insyncAccount];
    [self checkGenericPassword: @"data"         account: remoteOnlyAccount];

    [self.keychainView dispatchSync:^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        CKKSMirrorEntry* ckme = nil;

        ckme = [CKKSMirrorEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:remoteDelete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNil(ckme); // deleted!

        ckme = [CKKSMirrorEntry tryFromDatabase:remoteDataChanged.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:items[3].recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:ckr.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);
        return true;
    }];
}

- (void)testMultipleZoneAdd {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    // Bring up a new zone: we expect a key hierarchy upload.
    [self.injectedManager findOrCreateView:(id)kSecAttrViewHintAppleTV];
    CKRecordZoneID* appleTVZoneID = [[CKRecordZoneID alloc] initWithZoneName:(__bridge NSString*) kSecAttrViewHintAppleTV ownerName:CKCurrentUserDefaultName];
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:appleTVZoneID];

    // We also expect the view manager's notifyNewTLKsInKeychain call to fire once (after some delay)
    id mockVM = OCMPartialMock(self.injectedManager);
    OCMExpect([mockVM notifyNewTLKsInKeychain]);

    // Let the horses loose
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded to the 'keychain' view
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect a single record to be uploaded to the 'atv' view
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:appleTVZoneID];
    [self addGenericPassword: @"atv"
                     account: @"tvaccount"
                    viewHint:(__bridge NSString*) kSecAttrViewHintAppleTV
                      access:(id)kSecAttrAccessibleAfterFirstUnlock
                   expecting:errSecSuccess message:@"AppleTV view-hinted object"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    OCMVerifyAllWithDelay(mockVM, 10);
    [mockVM stopMocking];
}

- (void)testMultipleZoneDelete {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Bring up a new zone: we expect a key hierarchy and an item.
    [self.injectedManager findOrCreateView:(id)kSecAttrViewHintAppleTV];
    CKRecordZoneID* appleTVZoneID = [[CKRecordZoneID alloc] initWithZoneName:(__bridge NSString*) kSecAttrViewHintAppleTV ownerName:CKCurrentUserDefaultName];
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:appleTVZoneID];
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:appleTVZoneID];

    [self addGenericPassword: @"atv"
                     account: @"tvaccount"
                    viewHint:(__bridge NSString*) kSecAttrViewHintAppleTV
                      access:(id)kSecAttrAccessibleAfterFirstUnlock
                   expecting:errSecSuccess
                     message:@"AppleTV view-hinted object"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect a single record to be deleted from the ATV zone
    [self expectCKDeleteItemRecords: 1 zoneID:appleTVZoneID];
    [self deleteGenericPassword:@"tvaccount"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now we expect a single record to be deleted from the test zone
    [self expectCKDeleteItemRecords: 1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRestartWithoutRefetch {
    // Restarting the CKKS operation should check that it's been 15 minutes since the last fetch before it fetches again. Simulate this.

    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Tear down the CKKS object and disallos fetches
    [self.keychainView cancelAllOperations];
    self.silentFetchesAllowed = false;

    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Okay, cool, rad, now let's set the date to be very long ago and check that there's positively a fetch
    [self.keychainView cancelAllOperations];
    self.silentFetchesAllowed = false;

    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];

        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastFetchTime = [NSDate distantPast];
        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "no error saving to database");
        return true;
    }];

    [self expectCKFetch];
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromZoneCreationFailure {
    // Fail the zone creation.
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone
    [self failNextZoneCreation:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    XCTAssertNil(self.zones[self.keychainZoneID].creationError, "Creation error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromZoneSubscriptionFailure {
    // Fail the zone subscription.
    [self failNextZoneSubscription:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    XCTAssertNil(self.zones[self.keychainZoneID].subscriptionError, "Subscription error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromZoneSubscriptionFailureDueToZoneNotExisting {
    // This is different from testRecoverFromZoneSubscriptionFailure, since the zone is gone. CKKS must attempt to re-create the zone.

    // Silently fail the zone creation
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone
    [self failNextZoneCreationSilently:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    XCTAssertFalse(self.zones[self.keychainZoneID].flag, "Zone flag was reset");
    XCTAssertNil(self.zones[self.keychainZoneID].subscriptionError, "Subscription error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromDeletedTLKWithStashedTLK {
    // We need to handle the case where our syncable TLKs are deleted for some reason. The device that has them might resurrect them

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSError* error = nil;
    [self.keychainZoneKeys.tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "Should have received no error deleting the new TLK from the keychain");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // CKKS should recreate the syncable TLK.
    [self checkNSyncableTLKsInKeychain: 1];
}

- (void)testRecoverFromDeletedTLKWithStashedTLKUponRestart {
    // We need to handle the case where our syncable TLKs are deleted for some reason. The device that has them might resurrect them

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    // Tear down the CKKS object
    [self.keychainView cancelAllOperations];

    NSError* error = nil;
    [self.keychainZoneKeys.tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "Should have received no error deleting the new TLK from the keychain");

    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // CKKS should recreate the syncable TLK.
    [self checkNSyncableTLKsInKeychain: 1];
}

- (void)testRecoverFromTLKWriteFailure {
    // We need to handle the case where a device's first TLK write doesn't go through (due to whatever reason).
    // Test starts with nothing in CloudKit, and will fail the first TLK write.
    NSError* noNetwork = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}];
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:nil withError:noNetwork];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // A network failure creating new TLKs shouldn't delete the 'failed' syncable one.
    [self checkNSyncableTLKsInKeychain: 2];
}

- (void)testRecoverFromTLKRace {
    // We need to handle the case where a device's first TLK write doesn't go through (due to whatever reason).
    // Test starts with nothing in CloudKit, and will fail the first TLK write.
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject: ^{
        [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    }];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The first TLK write should fail, and then our fake TLKs should be there in CloudKit.
    // It shouldn't write anything back up to CloudKit.
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now the TLKs arrive from the other device...
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // A race failure creating new TLKs should delete the old syncable one.
    [self checkNSyncableTLKsInKeychain: 1];
}

- (void)testRecoverFromNullCurrentKeyPointers {
    // The current key pointers in cloudkit shouldn't ever not exist if keys do. But, if they don't, CKKS must recover.

    // Test starts with a broken key hierarchy in our fake CloudKit, but the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* zonekeys = self.keys[self.keychainZoneID];
    FakeCKZone* ckzone = self.zones[self.keychainZoneID];
    ckzone.currentDatabase[zonekeys.currentTLKPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = nil;
    ckzone.currentDatabase[zonekeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = nil;
    ckzone.currentDatabase[zonekeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = nil;

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromNoCurrentKeyPointers {
    // The current key pointers in cloudkit shouldn't ever point to nil. But, if they do, CKKS must recover.

    // Test starts with a broken key hierarchy in our fake CloudKit, but the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* zonekeys = self.keys[self.keychainZoneID];
    XCTAssertNil([self.zones[self.keychainZoneID] deleteCKRecordIDFromZone: zonekeys.currentTLKPointer.storedCKRecord.recordID],    "Deleted TLK pointer from zone");
    XCTAssertNil([self.zones[self.keychainZoneID] deleteCKRecordIDFromZone: zonekeys.currentClassAPointer.storedCKRecord.recordID], "Deleted class a pointer from zone");
    XCTAssertNil([self.zones[self.keychainZoneID] deleteCKRecordIDFromZone: zonekeys.currentClassCPointer.storedCKRecord.recordID], "Deleted class c pointer from zone");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromBadChangeTag {
    // We received a report where a machine appeared to have an up-to-date change tag, but was continuously attempting to create a new TLK hierarchy. No idea why.

    // Test starts with a broken key hierarchy in our fake CloudKit, but a (incorrectly) up-to-date change tag stored locally.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    SecCKKSTestSetDisableKeyNotifications(true); // Don't tell CKKS about this key material; we're pretending like this is a securityd restart
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    SecCKKSTestSetDisableKeyNotifications(false);

    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.keychainZoneID.zoneName];
        XCTAssertNotNil(ckse, "should have received a ckse");

        ckse.ckzonecreated = true;
        ckse.ckzonesubscribed = true;
        ckse.changeToken = self.keychainZone.currentChangeToken;

        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "shouldn't have gotten an error saving to database");
        return true;
    }];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should try to write TLKs, but fail
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    OCMVerifyAllWithDelay(self.mockDatabase, 16);

    // CKKS should then happily use the keys in CloudKit
    [self createClassCItemAndWaitForUpload:self.keychainZoneID account:@"account-delete-me"];
    [self createClassAItemAndWaitForUpload:self.keychainZoneID account:@"account-delete-me-class-a"];
}

- (void)testRecoverFromDeletedKeysNewItem {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self waitForCKModifications];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // Now, delete the local keys from the keychain (but leave the synced TLK)
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                         (id)kSecClass : (id)kSecClassInternetPassword,
                                                                         (id)kSecAttrNoLegacy : @YES,
                                                                         (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                         (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                                                                         }), @"Deleting local keys");
    SecCKKSTestSetDisableKeyNotifications(false);

    NSError* error = nil;
    [self.keychainZoneKeys.classC loadKeyMaterialFromKeychain:&error];
    XCTAssertNotNil(error, "Error loading class C key material from keychain");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"datadata" account: @"account-no-keys"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect a single class A record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromDeletedKeysReceive {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self waitForCKModifications];
    [self.keychainView waitUntilAllOperationsAreFinished];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];

    // Now, delete the local keys from the keychain (but leave the synced TLK)
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecAttrNoLegacy : @YES,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                                                                            }), @"Deleting local keys");
    SecCKKSTestSetDisableKeyNotifications(false);

    // Trigger a notification (with hilariously fake data)
    [self.keychainZone addToZone: ckr];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
}

- (void)disabledtestRecoverDeletedTLKAndPause {
    // If the TLK disappears halfway through, well, that's no good. But we should make it into waitfortlk.

    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // Now, delete the local keys from the keychain
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecAttrNoLegacy : @YES,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                                                                            }), @"Deleting local keys");
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecAttrNoLegacy : @YES,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                                                            }), @"Deleting TLK");
    SecCKKSTestSetDisableKeyNotifications(false);

    // Trigger a notification (with hilariously fake data)
    [self.keychainZone addToZone: ckr];
    [self.keychainView notifyZoneChange:nil];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self.keychainView waitForOperationsOfClass:[CKKSHealKeyHierarchyOperation class]];

    XCTAssertEqual(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateWaitForTLK, "CKKS re-entered waitfortlk");
}

- (void)testRecoverFromBadCurrentKeyPointer {
    // The current key pointers in cloudkit shouldn't ever point to missing entries. But, if they do, CKKS must recover.

    // Test starts with a broken key hierarchy in our fake CloudKit, but the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* zonekeys = self.keys[self.keychainZoneID];
    FakeCKZone* ckzone = self.zones[self.keychainZoneID];
    ckzone.currentDatabase[zonekeys.currentTLKPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey]    = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: @"not a real tlk"         zoneID: self.keychainZoneID] action: CKReferenceActionNone];
    ckzone.currentDatabase[zonekeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: @"not a real class a key" zoneID: self.keychainZoneID] action: CKReferenceActionNone];
    ckzone.currentDatabase[zonekeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: @"not a real class c key" zoneID: self.keychainZoneID] action: CKReferenceActionNone];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromCloudKitFetchFail {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // The first two CKRecordZoneChanges should fail with a 'network unavailable' error.
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}]];
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}]];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 12);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromCloudKitFetchFailWithDelay {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // The first  CKRecordZoneChanges should fail with a 'delay' error.
    self.silentFetchesAllowed = false;
    [self.keychainZone failNextFetchWith:[[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorRequestRateLimited userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:4]}]];
    [self expectCKFetch];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Ensure it doesn't fetch within these three seconds (if it does, an exception will throw).
    sleep(3);

    // Okay, you can fetch again.
    [self expectCKFetch];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromCloudKitOldChangeToken {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Delete all old database states, to destroy the change tag validity
    [self.keychainZone.pastDatabases removeAllObjects];

    // We expect a total local flush and refetch
    self.silentFetchesAllowed = false;
    [self expectCKFetch]; // one to fail with a CKErrorChangeTokenExpired error
    [self expectCKFetch]; // and one to succeed

    // Trigger a fake change notification
    [self.keychainView notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And check that a new upload happens just fine.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromCloudKitUnknownDeviceStateRecord {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Save a new device state record with some fake etag
    [self.keychainView dispatchSync: ^bool {
        CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:self.ckDeviceID
                                                                    circlePeerID:self.circlePeerID
                                                                    circleStatus:kSOSCCInCircle
                                                                        keyState:SecCKKSZoneKeyStateWaitForTLK
                                                                  currentTLKUUID:nil
                                                               currentClassAUUID:nil
                                                               currentClassCUUID:nil
                                                                          zoneID:self.keychainZoneID
                                                                 encodedCKRecord:nil];
        XCTAssertNotNil(cdse, "Should have created a fake CDSE");
        CKRecord* record = [cdse CKRecordWithZoneID:self.keychainZoneID];
        XCTAssertNotNil(record, "Should have created a fake CDSE CKRecord");
        record.etag = @"fake etag";
        cdse.storedCKRecord = record;

        NSError* error = nil;
        [cdse saveToDatabase:&error];
        XCTAssertNil(error, @"No error saving cdse to database");

        return true;
    }];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a record failure, since the device state record is broke
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    // And then we expect a clean write
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromCloudKitUnknownItemRecord {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    // Delete the record from CloudKit, but miss the notification
    XCTAssertNil([self.keychainZone deleteCKRecordIDFromZone: ckr.recordID], "Deleting the record from fake CloudKit should succeed");

    // Expect a failed upload when we modify the item
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self updateGenericPassword:@"never seen again" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self.keychainView waitUntilAllOperationsAreFinished];

    // And the item should be disappeared from the local keychain
    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];
}

- (void)testRecoverFromCloudKitUserDeletedZone {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // The first  CKRecordZoneChanges should fail with a 'CKErrorUserDeletedZone' error.
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorUserDeletedZone userInfo:@{}]];

    // We expect a key hierarchy upload, and then the class C item upload
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.keychainView notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And check that a new upload occurs.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromCloudKitZoneNotFound {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // The next CKRecordZoneChanges should fail with a 'zone not found' error.
    NSError* zoneNotFoundError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                  code:CKErrorZoneNotFound
                                                              userInfo:@{}];
    NSError* error = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                      code:CKErrorPartialFailure
                                                  userInfo:@{CKPartialErrorsByItemIDKey: @{self.keychainZoneID:zoneNotFoundError}}];
    [self.keychainZone failNextFetchWith:error];

    // We expect a key hierarchy upload, and then the class C item upload
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.keychainView notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And check that a new upload occurs.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testNoCloudKitAccount {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // simulate a NSNotification callback (but still logged out)
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Test that there are no items in the database (since we never logged in)
    [self checkNoCKKSData: self.keychainView];
}

- (void)testSACloudKitAccount {
    // Test starts with nothing in database and the user logged into CloudKit and in circle, but the account is not HSA2.
    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.accountStatus = CKAccountStatusAvailable;
    self.supportsDeviceToDeviceEncryption = NO;

    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // There should be no uploads, even when we save keychain items and enter/exit circle
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitUntilAllOperationsAreFinished];

    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Test that there are no items in the database (since we never were in an HSA2 account)
    [self checkNoCKKSData: self.keychainView];
}

- (void)testNoCircle {
    // Test starts with nothing in database and the user logged into CloudKit, but out of Circle.
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.accountStatus = CKAccountStatusAvailable;

    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // simulate a NSNotification callback (but still logged out)
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Test that there are no items in the database (since we never logged in)
    [self checkNoCKKSData: self.keychainView];
}

- (void)testCloudKitLogin {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self startCKKSSubsystem];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // simulate a cloudkit login and NSNotification callback
    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // No writes yet, since we're not in circle
    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect some sort of TLK/key hierarchy upload once we are notified of entering the circle.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
}

- (void)testCloudKitLogoutLogin {
    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // simulate a cloudkit logout and NSNotification callback
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // Test that there are no items in the database after logout
    [self.keychainView waitUntilAllOperationsAreFinished];
    [self checkNoCKKSData: self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // simulate a cloudkit login
    // We should expect CKKS to re-find the key hierarchy it already uploaded, and then send up the two records we added during the pause
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Let everything settle...
    [self.keychainView waitUntilAllOperationsAreFinished];

    // Logout again
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // Test that there are no items in the database after logout
    [self.keychainView waitUntilAllOperationsAreFinished];
    [self checkNoCKKSData: self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-5"];
    [self addGenericPassword: @"data" account: @"account-delete-me-6"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // simulate a cloudkit login
    // We should expect CKKS to re-find the key hierarchy it already uploaded, and then send up the two records we added during the pause
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCloudKitLoginRace {
    // Test starts with nothing in database, and 'in circle', but securityd hasn't received notification if we're logged into CloudKit.
    // CKKS should not call handleLogout.

    id partialKVMock = OCMPartialMock(self.keychainView);
    OCMReject([partialKVMock handleCKLogout]);

    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self startCKKSSubsystemOnly]; // note: don't unblock the ck account state object yet...

    // Add a keychain item, but make sure it doesn't upload yet.
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now that we're here (and handleCKLogout hasn't been called), bring the account up

    // We expect some sort of TLK/key hierarchy upload once we are notified of entering the circle.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    self.accountStatus = CKAccountStatusAvailable;
    [self startCKAccountStatusMock];

    // simulate another NSNotification callback
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Make sure new items upload too
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
    [self.keychainView cancelAllOperations];

    [partialKVMock stopMocking];
}

- (void)testDeviceStateUploadGood {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], @"fake-circle-id", "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateReady), "Device is in ready");

                    XCTAssertEqualObjects([record[SecCKRecordCurrentTLK]    recordID].recordName, zoneKeys.tlk.uuid, "Correct TLK uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassA] recordID].recordName, zoneKeys.classA.uuid, "Correct class A uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassC] recordID].recordName, zoneKeys.classC.uuid, "Correct class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    [self.keychainView updateDeviceState:false ckoperationGroup:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadRateLimited {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], @"fake-circle-id", "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateReady), "Device is in ready");

                    XCTAssertEqualObjects([record[SecCKRecordCurrentTLK]    recordID].recordName, zoneKeys.tlk.uuid, "Correct TLK uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassA] recordID].recordName, zoneKeys.classA.uuid, "Correct class A uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassC] recordID].recordName, zoneKeys.classC.uuid, "Correct class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    CKKSUpdateDeviceStateOperation* op = [self.keychainView updateDeviceState:true ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [op waitUntilFinished];

    // Check that an immediate rate-limited retry doesn't upload anything
    op = [self.keychainView updateDeviceState:true ckoperationGroup:nil];
    [op waitUntilFinished];

    // But not rate-limiting works just fine!
    [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
    op = [self.keychainView updateDeviceState:false ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [op waitUntilFinished];

    // And now, if the update is old enough, that'll work too
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry fromDatabase:self.accountStateTracker.ckdeviceID zoneID:self.keychainZoneID error:&error];
        XCTAssertNil(error, "No error fetching device state entry");
        XCTAssertNotNil(cdse, "Fetched device state entry");

        CKRecord* record = cdse.storedCKRecord;

        NSDate* m = record.modificationDate;
        XCTAssertNotNil(m, "Have modification date");

        // Four days ago!
        NSDateComponents* offset = [[NSDateComponents alloc] init];
        [offset setHour:-4 * 24];
        NSDate* m2 = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:m options:0];

        XCTAssertNotNil(m2, "Made modification date");

        record.modificationDate = m2;
        [cdse setStoredCKRecord:record];

        [cdse saveToDatabase:&error];
        XCTAssertNil(error, "No error saving device state entry");

        return true;
    }];

    // And now the rate-limiting doesn't get in the way
    [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
    op = [self.keychainView updateDeviceState:true ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 12);
    [op waitUntilFinished];
}

- (void)testDeviceStateUploadRateLimitedAfterNormalUpload {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"password" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Check that an immediate rate-limited retry doesn't upload anything
    CKKSUpdateDeviceStateOperation* op = [self.keychainView updateDeviceState:true ckoperationGroup:nil];
    [op waitUntilFinished];
}

- (void)testDeviceStateReceive {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    ZoneKeys* zoneKeys = self.keys[self.keychainZoneID];
    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", self.keychainZoneID);

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:@"otherdevice"
                                                                circlePeerID:@"asdfasdf"
                                                                circleStatus:kSOSCCInCircle
                                                                    keyState:SecCKKSZoneKeyStateReady
                                                              currentTLKUUID:zoneKeys.tlk.uuid
                                                           currentClassAUUID:zoneKeys.classA.uuid
                                                           currentClassCUUID:zoneKeys.classC.uuid
                                                                      zoneID:self.keychainZoneID
                                                             encodedCKRecord:nil];
    CKRecord* record = [cdse CKRecordWithZoneID:self.keychainZoneID];
    [self.keychainZone addToZone:record];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        NSArray<CKKSDeviceStateEntry*>* cdses = [CKKSDeviceStateEntry allInZone:self.keychainZoneID error:&error];
        XCTAssertNil(error, "No error fetching CDSEs");
        XCTAssertNotNil(cdses, "An array of CDSEs was returned");
        XCTAssert(cdses.count >= 1u, "At least one CDSE came back");

        CKKSDeviceStateEntry* item = nil;
        for(CKKSDeviceStateEntry* dbcdse in cdses) {
            if([dbcdse.device isEqualToString:@"otherdevice"]) {
                item = dbcdse;
            }
        }
        XCTAssertNotNil(item, "Found a cdse for otherdevice");

        XCTAssertEqualObjects(cdse, item, "Saved item matches pre-cloudkit item");

        XCTAssertEqualObjects(item.circlePeerID,      @"asdfasdf",          "correct peer id");
        XCTAssertEqualObjects(item.keyState, SecCKKSZoneKeyStateReady,      "correct key state");
        XCTAssertEqualObjects(item.currentTLKUUID,    zoneKeys.tlk.uuid,    "correct tlk uuid");
        XCTAssertEqualObjects(item.currentClassAUUID, zoneKeys.classA.uuid, "correct classA uuid");
        XCTAssertEqualObjects(item.currentClassCUUID, zoneKeys.classC.uuid, "correct classC uuid");

        return false;
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadBadKeyState {
    // This test has stuff in CloudKit, but no TLKs. It should become very sad.
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateWaitForTLK, "CKKS entered waitfortlk");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], @"fake-circle-id", "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForTLK), "Device is in waitfortlk");

                    XCTAssertNil(record[SecCKRecordCurrentTLK]   , "No TLK");
                    XCTAssertNil(record[SecCKRecordCurrentClassA], "No class A key");
                    XCTAssertNil(record[SecCKRecordCurrentClassC], "No class C key");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    [self.keychainView updateDeviceState:false ckoperationGroup:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadBadCircleState {
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // This test has stuff in CloudKit, but no TLKs.
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];

    [self startCKKSSubsystem];

    // Since CKKS should start up enough to get back into the error state and then back into initializing state, wait for that to happen.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateError] wait:8*NSEC_PER_SEC], "CKKS entered error");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateInitializing] wait:8*NSEC_PER_SEC], "CKKS entered initializing");
    XCTAssertEqual(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateInitializing, "CKKS entered intializing");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    XCTAssertNil(record[SecCKRecordCirclePeerID], "no peer ID if device is not in circle");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCNotInCircle], "device is not in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateInitializing), "Device is in keystate:initializing");

                    XCTAssertNil(record[SecCKRecordCurrentTLK]   , "No TLK");
                    XCTAssertNil(record[SecCKRecordCurrentClassA], "No class A key");
                    XCTAssertNil(record[SecCKRecordCurrentClassC], "No class C key");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    CKKSUpdateDeviceStateOperation* op = [self.keychainView updateDeviceState:false ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [op waitUntilFinished];
    XCTAssertNil(op.error, "No error uploading 'out of circle' device state");
}

- (void)testCKKSControlBringup {
    xpc_endpoint_t endpoint = SecServerCreateCKKSEndpoint();
    XCTAssertNotNil(endpoint, "Received endpoint");

    NSXPCInterface *interface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    XCTAssertNotNil(interface, "Received a configured CKKS interface");

    NSXPCListenerEndpoint *listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];
    [listenerEndpoint _setEndpoint:endpoint];

    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];
    XCTAssertNotNil(connection , "Received an active connection");

    connection.remoteObjectInterface = interface;
}

@end

#endif // OCTAGON
