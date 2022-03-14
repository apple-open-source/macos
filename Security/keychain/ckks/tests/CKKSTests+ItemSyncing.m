/*
 * Copyright (c) 2021 Apple Inc. All Rights Reserved.
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

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"

#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemServer.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/MockCloudKit.h"

@interface CloudKitKeychainSyncingItemSyncingTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainSyncingItemSyncingTests

- (void)testAddItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self addGenericPassword:@"data" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

     [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
     [self addCertificateWithLabel:@"certificate" serialNumber:[@"1234" dataUsingEncoding:NSUTF8StringEncoding]];
     OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAddMultipleItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAddItemWithoutUUID {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // We expect an upload of the added item, once CKKS finds the UUID-less item and fixes it
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];
    SecCKKSTestSetDisableAutomaticUUID(false);

    [self findGenericPassword:@"account-delete-me-no-UUID" expecting:errSecSuccess];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And then modified.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItemImmediately {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItemPrimaryKey {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And then modified. Since we're changing the "primary key", we expect to delete the old record and upload a new one.
    [self expectCKModifyItemRecords:1 deletedRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:nil];
    [self updateAccountOfGenericPassword: @"new-account-delete-me" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItemDuringReencrypt {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    // Stop the reencrypt operation from happening
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateReencryptOutgoingItems];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up the reencryption operation)
    [self releaseCloudKitModificationHold];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReencryptOutgoingItems] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'CKKSStateReencryptOutgoingItems'");

    // Run the reencrypt items operation to completion.
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateReencryptOutgoingItems];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    [self waitForCKModifications];
}

- (void)testModifyItemBeforeReencrypt {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update, but for the final version of the password
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"third"]];

    // Stop the reencrypt operation from happening
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateReencryptOutgoingItems];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up the reencryption operation)
    [self releaseCloudKitModificationHold];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReencryptOutgoingItems] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'CKKSStateReencryptOutgoingItems'");

    [self updateGenericPassword: @"third" account:account];

    // Release the state machine.
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateReencryptOutgoingItems];

    // Item should upload.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testOutgoingQueueRecoverFromStaleInflightEntry {
    // CKKS is restarting with an existing in-flight OQE
    // Note that this test is incomplete, and doesn't re-add the item to the local keychain
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"fake-account";

    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;

        CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];

        CKKSItem* item = [self newItem:ckrid withNewItemData:[self fakeRecordDictionary:account zoneID:self.keychainZoneID] key:self.keychainZoneKeys.classC];
        XCTAssertNotNil(item, "Should be able to create a new fake item");

        CKKSOutgoingQueueEntry* oqe = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:item action:SecCKKSActionAdd state:SecCKKSStateInFlight waitUntil:nil accessGroup:@"ckks"];
        XCTAssertNotNil(oqe, "Should be able to create a new fake OQE");
        [oqe saveToDatabase:&error];

        XCTAssertNil(error, "Shouldn't error saving new OQE to database");
        return CKKSDatabaseTransactionCommit;
    }];

    NSError *error = NULL;
    XCTAssertEqual([CKKSOutgoingQueueEntry countByState:SecCKKSStateInFlight zone:self.keychainZoneID error:&error], 1,
                   "Expected on inflight entry in outgoing queue: %@", error);

    // When CKKS restarts, it should find and re-upload this item
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];

    [self startCKKSSubsystem];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    [self.defaultCKKS waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItemDuringNetworkFailure {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update, but for the final version of the password
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up uploading the new item)
    [self releaseCloudKitModificationHold];

    // Item should upload.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testOutgoingQueueRecoverFromNetworkFailure {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally

    NSError* greyMode = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNotAuthenticated userInfo:@{
        CKErrorRetryAfterKey: @(0.2),
    }];
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:nil withError:greyMode];

    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And then schedule the retried update
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up uploading the new item)
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testDeleteItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single record to be deleted.
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

     [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
     [self addCertificateWithLabel:@"certificate" serialNumber:[@"1234" dataUsingEncoding:NSUTF8StringEncoding]];
     OCMVerifyAllWithDelay(self.mockDatabase, 20);

     [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
     [self deleteCertificateWithSerialNumber:[@"1234" dataUsingEncoding:NSUTF8StringEncoding]];
     OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteItemAndReaddAtSameUUID {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    __block CKRecordID* itemRecordID = nil;
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        itemRecordID = record.recordID;
        return YES;
    }];
    [self addGenericPassword:@"data" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single record to be deleted.
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the item is readded. It should come back to its previous UUID.
    XCTAssertNotNil(itemRecordID, "Should have an item record ID");
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        XCTAssertEqualObjects(itemRecordID.recordName, record.recordID.recordName, "Uploaded item UUID should match previous upload");
        return YES;
    }];
    [self addGenericPassword:@"data" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteItemImmediatelyAfterModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, hold the modify
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local deletion...
    [self deleteGenericPassword:account];

    // And then schedule the update
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteItemDuringAddUpload {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single record to be uploaded. But, while that's happening, delete it via the API.

    XCTestExpectation *deleteBlock = [self expectationWithDescription:@"delete block called"];

    WEAKIFY(self);
    self.keychainZone.blockBeforeWriteOperation = ^() {
        STRONGIFY(self);
        [self deleteGenericPassword:account];
        self.keychainZone.blockBeforeWriteOperation = nil;
        [deleteBlock fulfill];
    };

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    // This should cause a deletion
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"data" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForExpectations: @[deleteBlock] timeout:5];
}

- (void)testDeleteItemDuringModificationUpload {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single modification record to be uploaded, and want to delete the item while the upload is ongoing
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    XCTestExpectation *deleteBlock = [self expectationWithDescription:@"delete block called"];

    WEAKIFY(self);
    self.keychainZone.blockBeforeWriteOperation = ^() {
        STRONGIFY(self);
        [self deleteGenericPassword:account];
        self.keychainZone.blockBeforeWriteOperation = nil;
        [deleteBlock fulfill];
    };

    [self updateGenericPassword:@"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForExpectations: @[deleteBlock] timeout:5];
}

- (void)testDeleteItemAfterFetchAfterModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, hold the modify
    //[self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Place a hold on outgoing queue processing
    // Place a hold on processing the outgoing queue.
    self.defaultCKKS.holdOutgoingQueueOperation = [CKKSResultOperation named:@"outgoing-queue-hold"
                                                                   withBlock:^{
        ckksnotice_global("ckks", "Outgoing queue hold released.");
    }];

    [self deleteGenericPassword:account];
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    // Release the CK modification hold
    //[self releaseCloudKitModificationHold];

    // And cause a fetch
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS.zoneChangeFetcher.inflightFetch waitUntilFinished];
    [self.operationQueue addOperation:self.defaultCKKS.holdOutgoingQueueOperation];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteItemWithoutTombstones {
    // The keychain API allows a client to ask for an inconsistent sync state:
    // They can ask for a local item deletion without propagating the deletion off-device.
    // This is the only halfway reasonable way to do keychain item deletions on account signout with the current API

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self deleteGenericPasswordWithoutTombstones:account];
    [self findGenericPassword:account expecting:errSecItemNotFound];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure nothing is in the outgoing queue
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID
                                                               error:&error];
        XCTAssertNil(error, "should be no error fetching uuids");
        XCTAssertEqual(uuids.count, 0u, "There should be zero OQEs");
    }];

    // And a simple fetch doesn't bring it back
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:account expecting:errSecItemNotFound];

    // but a resync does
    CKKSSynchronizeOperation* resyncOperation = [self.defaultCKKS resyncWithCloud];
    [resyncOperation waitUntilFinished];
    XCTAssertNil(resyncOperation.error, "No error during the resync operation");

    [self findGenericPassword:account expecting:errSecSuccess];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    CKRecord* ckrCert = [self createFakeRecord:self.keychainZoneID
                                    recordName:@"E025708D-F288-4068-A016-792E82B0BF53"
                                itemDictionary:[self fakeCertificateRecordDictionary:[@"1234" dataUsingEncoding:NSUTF8StringEncoding]
                                                                              zoneID:self.keychainZoneID]
                                           key:nil];
    [self.keychainZone addToZone:ckrCert];

    // Trigger a notification (with hilariously fake data)
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");

    [self findCertificateWithSerialNumber:[@"1234" dataUsingEncoding:NSUTF8StringEncoding] expecting:errSecSuccess];
}

- (void)testReceiveItemAndGeneratedUUIDBasedPersistentRef {
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSMutableDictionary *query = [@{(id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccount : @"account-delete-me",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                                  } mutableCopy];

    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not yet exist");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    // Trigger a notification (with hilariously fake data)
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    query[(id)kSecReturnPersistentRef] = @YES;

    CFTypeRef pref = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &pref), "item should exist now");
    XCTAssertNotNil((__bridge id)pref, "persistent ref should not be nil");
    XCTAssertEqual([(__bridge id)pref length], 20, "persistent reference should be 20 bytes");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

- (void)testReceiveManyItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D01" withAccount:@"account1"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D03" withAccount:@"account3"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D04" withAccount:@"account4"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D05" withAccount:@"account5"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D06" withAccount:@"account6"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D07" withAccount:@"account7"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D08" withAccount:@"account8"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D09" withAccount:@"account9"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D10" withAccount:@"account10"]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D11" withAccount:@"account11"]];

    for(int i = 12; i < 100; i++) {
        @autoreleasepool {
            NSString* recordName = [NSString stringWithFormat:@"7B598D31-F9C5-481E-98AC-%012d", i];
            NSString* account = [NSString stringWithFormat:@"account%d", i];

            [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:recordName withAccount:account]];
        }
    }

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName: @"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    CKRecord* ckr2 = [self createFakeRecord:self.keychainZoneID recordName: @"F9C58D31-7B59-481E-98AC-5A507ACB2D85"];

    [self.keychainZone addToZone:ckr];
    [self.keychainZone addToZone:ckr2];

    // We expect a delete operation with the "higher" UUID.
    [self expectCKDeleteItemRecords:1
                             zoneID:self.keychainZoneID
         expectedOperationGroupName:@"incoming-queue-response"];

    // Trigger a notification (with hilariously fake data)
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");

    [self waitForCKModifications];
    XCTAssertNil(self.keychainZone.currentDatabase[ckr2.recordID], "Correct record was deleted from CloudKit");

    // And the local item should have ckr's UUID
    [self checkGenericPasswordStoredUUID:ckr.recordID.recordName account:@"account-delete-me"];
}

- (void)testReceiveCorruptedItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];

    // I don't know of any codepaths that cause this, but it apparently has happened.
    id key = ckr[SecCKRecordWrappedKeyKey];

    ckr[SecCKRecordWrappedKeyKey] = nil;
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // The item still shouldn't exist, because it was corrupted in flight
    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<CKKSIncomingQueueEntry*>* iqes = [CKKSIncomingQueueEntry all:&error];
        XCTAssertNil(error, "No error loading IQEs");
        XCTAssertNotNil(iqes, "Could load IQEs");
        XCTAssertEqual(iqes.count, 1u, "Incoming queue has one item");
        XCTAssertEqualObjects(iqes[0].state, SecCKKSStateError, "Item state should be 'error'");
    }];

    CKRecord* ckrNext = [ckr copy];
    ckrNext[SecCKRecordWrappedKeyKey] = key;
    [self.keychainZone addToZone:ckrNext];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<CKKSIncomingQueueEntry*>* iqes = [CKKSIncomingQueueEntry all:&error];
        XCTAssertNil(error, "No error loading IQEs");
        XCTAssertNotNil(iqes, "Could load IQEs");
        XCTAssertEqual(iqes.count, 0u, "Incoming queue has zero items");
    }];
}

- (void)testReceiveItemDelete {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecReturnAttributes : @YES,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
    };

    CFTypeRef cfitem = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfitem), "item should not yet exist");

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName: @"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    CKRecord* ckrCert = [self createFakeRecord:self.keychainZoneID
                                    recordName:@"E025708D-F288-4068-A016-792E82B0BF53"
                                itemDictionary:[self fakeCertificateRecordDictionary:[@"1234" dataUsingEncoding:NSUTF8StringEncoding]
                                                                              zoneID:self.keychainZoneID]
                                           key:nil];
    [self.keychainZone addToZone:ckrCert];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfitem), "item should exist now");

    NSDictionary* item = (NSDictionary*) CFBridgingRelease(cfitem);
    cfitem = NULL;
    NSDate* itemModificationDate = item[(id)kSecAttrModificationDate];
    XCTAssertNotNil(itemModificationDate, "Should have a modification date");

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass : (id)kSecClassCertificate,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecReturnAttributes : @YES,
        (id)kSecMatchLimit : (id)kSecMatchLimitOne,
    }, &cfitem), "certificate should exist now");

    NSDictionary* certificate = (NSDictionary*)CFBridgingRelease(cfitem);
    cfitem = NULL;
    NSDate* certificateModificationDate = certificate[(id)kSecAttrModificationDate];
    XCTAssertNotNil(certificateModificationDate, "Should have a modification date");

    // Trigger a delete
    [self.keychainZone deleteCKRecordIDFromZone:[ckr recordID]];
    [self.keychainZone deleteCKRecordIDFromZone:ckrCert.recordID];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfitem), "item should no longer exist");
    CFReleaseNull(cfitem);

    [self findCertificateWithSerialNumber:[@"1234" dataUsingEncoding:NSUTF8StringEncoding] expecting:errSecItemNotFound];

    {
        // Now, double-check the tombstone. Its modification date should be derived from the item's mdat.
        NSDictionary *tombquery = @{(id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccount : @"account-delete-me",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrTombstone : @YES,
                                    (id)kSecReturnAttributes : @YES,
                                    (id)kSecMatchLimit : (id)kSecMatchLimitOne,};

        CFTypeRef cfref = NULL;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)tombquery, &cfref);
        XCTAssertEqual(status, errSecSuccess, "Should have found a tombstone");

        NSDictionary* tombstone = (NSDictionary*)CFBridgingRelease(cfref);
        XCTAssertNotNil(tombstone, "Should have found a tombstone");

        NSDate* tombstoneModificationDate = tombstone[(id)kSecAttrModificationDate];
        XCTAssertEqual([tombstoneModificationDate compare:itemModificationDate], NSOrderedDescending, "tombstone should be later than item");

        NSTimeInterval tombestoneDelta = [tombstoneModificationDate timeIntervalSinceDate:itemModificationDate];
        XCTAssertGreaterThan(tombestoneDelta, 0, "Delta should be positive");
        XCTAssertLessThan(tombestoneDelta, 5, "tombstone mdat should be no later than 5s after item mdat");

        // And just to be sure, mdat should be far ago. (It should be near the date of test data creation.)
        NSTimeInterval itemDelta = [[NSDate date] timeIntervalSinceDate:itemModificationDate];
        XCTAssertGreaterThan(itemDelta, 10, "item mdat should at least 10s in the past");
    }

    {
        // Now, double-check the tombstone. Its modification date should be derived from the item's mdat.
        NSDictionary *tombquery = @{
            (id)kSecClass : (id)kSecClassCertificate,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
            (id)kSecAttrTombstone : @YES,
            (id)kSecReturnAttributes : @YES,
            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
        };

        CFTypeRef cfref = NULL;
        OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)tombquery, &cfref);
        XCTAssertEqual(status, errSecSuccess, "Should have found a tombstone");

        NSDictionary* tombstone = (NSDictionary*)CFBridgingRelease(cfref);
        XCTAssertNotNil(tombstone, "Should have found a tombstone");

        NSDate* tombstoneModificationDate = tombstone[(id)kSecAttrModificationDate];
        XCTAssertEqual([tombstoneModificationDate compare:certificateModificationDate], NSOrderedDescending, "tombstone should be later than item");

        NSTimeInterval tombstoneDelta = [tombstoneModificationDate timeIntervalSinceDate:certificateModificationDate];
        XCTAssertGreaterThan(tombstoneDelta, 0, "Delta should be positive");
        XCTAssertLessThan(tombstoneDelta, 5, "tombstone mdat should be no later than 5s after item mdat");

        // And just to be sure, mdat should be far ago. (It should be near the date of test data creation.)
        NSTimeInterval itemDelta = [[NSDate date] timeIntervalSinceDate:certificateModificationDate];
        XCTAssertGreaterThan(itemDelta, 10, "item mdat should at least 10s in the past");
    }
}

- (void)testReceiveTombstoneItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSString* account = @"account-delete-me";

    CKRecord* ckr = [self createFakeTombstoneRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" account:account];
    [self.keychainZone addToZone:ckr];

    // This device should delete the tombstone entry
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // The tombstone shouldn't exist
    NSDictionary *tombquery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccount : account,
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecReturnAttributes : @YES,
        (id)kSecAttrTombstone : @YES,
    };

    CFTypeRef cftype = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef)tombquery, &cftype), "item should not exist now");
    XCTAssertNil((__bridge id)cftype, "Should have found no tombstones");

    // And the delete should occur
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSIncomingQueueEntry allUUIDs:self.keychainZoneID
                                                               error:&error];
        XCTAssertNil(error, "should be no error fetching uuids");
        XCTAssertEqual(uuids.count, 0u, "There should be zero IQEs");
    }];
}

#if TARGET_OS_IOS

- (void)testAddMultiuserItem {
    // CKKS should not create or send any musr items, even if they're marked as syncable
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSUUID* musrUUID = [NSUUID UUID];
    SecSecuritySetPersonaMusr((__bridge CFStringRef)musrUUID.UUIDString);

    [self addGenericPassword:@"data" account:@"account-delete-me"];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching uuids");
        XCTAssertEqual(uuids.count, 0u, "There's exactly zero outgoing queue entries");
    }];


    SecSecuritySetPersonaMusr(NULL);

    // And resyncing doesn't upload it, right?
    CKKSResultOperation* resync = [self.defaultCKKS resyncWithCloud];
    [resync waitUntilFinished];

    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching uuids");
        XCTAssertEqual(uuids.count, 0u, "There's exactly zero outgoing queue entries");
    }];
}

#endif // TARGET_OS_IOS

- (void)testReceiveMultiuserItem {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    NSString* account = @"account-delete-me";

    NSUUID* musrUUID = [NSUUID UUID];
    CKRecord* ckr = [self createFakeMultiuserRecord:self.keychainZoneID musr:musrUUID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" account:account];
    [self.keychainZone addToZone:ckr];

    // This device should delete the tombstone entry
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // On platforms that don't support musr, we cannot fully finish this test
#if TARGET_OS_IOS
    SecSecuritySetPersonaMusr((__bridge CFStringRef)musrUUID.UUIDString);
#endif // TARGET_OS_IOS
    [self findGenericPassword:account expecting:errSecItemNotFound];
}

- (void)testReceiveItemDeleteAndReaddAtDifferentUUIDInSameFetch {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];
    [self startCKKSSubsystem];

    NSString* itemAccount = @"account-delete-me";
    [self findGenericPassword:itemAccount expecting:errSecItemNotFound];

    NSString* uuidOriginalItem = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    NSString* uuidGreater      = @"7B598D31-FFFF-FFFF-98AC-5A507ACB2D85";

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:uuidOriginalItem];
    CKRecord* ckrGreater = [self createFakeRecord:self.keychainZoneID recordName:uuidGreater];

    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:itemAccount expecting:errSecSuccess];
    [self checkGenericPasswordStoredUUID:uuidOriginalItem account:itemAccount];

    // Now, the item is deleted and re-added with a greater UUID
    [self.keychainZone deleteCKRecordIDFromZone:[ckr recordID]];
    [self.keychainZone addToZone:ckrGreater];

    // This node should not upload anything.
    [[self.mockDatabase reject] addOperation:[OCMArg any]];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Item should still exist.
    [self findGenericPassword:itemAccount expecting:errSecSuccess];
    [self checkGenericPasswordStoredUUID:uuidGreater account:itemAccount];
}

- (void)testReceiveItemPhantomDelete {
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

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName: @"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should exist now");
    CFReleaseNull(item);

    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // Trigger a delete
    [self.keychainZone deleteCKRecordIDFromZone:[ckr recordID]];

    // and add another, incorrect IQE
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        // Inefficient, but hey, it works
        CKRecord* record = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-FFFF-FFFF-5A507ACB2D85"];
        CKKSItem* fakeItem = [[CKKSItem alloc] initWithCKRecord: record];

        CKKSIncomingQueueEntry* iqe = [[CKKSIncomingQueueEntry alloc] initWithCKKSItem:fakeItem
                                                                                action:SecCKKSActionDelete
                                                                                 state:SecCKKSStateNew];
        XCTAssertNotNil(iqe, "could create fake IQE");
        NSError* error = nil;
        XCTAssert([iqe saveToDatabase: &error], "Saved fake IQE to database");
        XCTAssertNil(error, "No error saving fake IQE to database");
        return CKKSDatabaseTransactionCommit;
    }];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should no longer exist");

    // The incoming queue should be empty
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray* iqes = [CKKSIncomingQueueEntry all:&error];
        XCTAssertNil(error, "No error loading IQEs");
        XCTAssertNotNil(iqes, "Could load IQEs");
        XCTAssertEqual(iqes.count, 0u, "Incoming queue is empty");
    }];
}

- (void)testReceiveConflictOnJustAddedItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Place a hold on processing the outgoing queue.
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateProcessOutgoingQueue];

    [self addGenericPassword:@"localchange" account:@"account-delete-me"];

    // Pull out the new item's UUID.
    __block NSString* itemUUID = nil;
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID ?: [[CKRecordZoneID alloc] initWithZoneName:@"keychain"
                                                                                                                           ownerName:CKCurrentUserDefaultName]
                                                               error:&error];
        XCTAssertNil(error, "no error fetching uuids");
        XCTAssertEqual(uuids.count, 1u, "There's exactly one outgoing queue entry");
        itemUUID = uuids[0];

        XCTAssertNotNil(itemUUID, "Have a UUID for our new item");
    }];

    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName: itemUUID]];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [[self.defaultCKKS.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting] waitUntilFinished];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateProcessOutgoingQueue] wait:20*NSEC_PER_SEC], "CKKS state machine should enter processoutgoing queue");

    // Allow the outgoing queue operation to proceed
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateProcessOutgoingQueue];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.defaultCKKS waitUntilAllOperationsAreFinished];
}

- (void)testReceiveCloudKitConflictOnJustAddedItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self.defaultCKKS waitForKeyHierarchyReadiness];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Place a hold on processing the outgoing queue.
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    self.defaultCKKS.holdOutgoingQueueOperation = [CKKSResultOperation named:@"outgoing-queue-hold" withBlock:^{
        ckksnotice_global("ckks", "Outgoing queue hold released.");
    }];

    [self addGenericPassword:@"localchange" account:@"account-delete-me"];

    // Pull out the new item's UUID.
    __block NSString* itemUUID = nil;
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID ?: [[CKRecordZoneID alloc] initWithZoneName:@"keychain"
                                                                                                                           ownerName:CKCurrentUserDefaultName]
                                                               error:&error];
        XCTAssertNil(error, "no error fetching uuids");
        XCTAssertEqual(uuids.count, 1u, "There's exactly one outgoing queue entry");
        itemUUID = uuids[0];

        XCTAssertNotNil(itemUUID, "Have a UUID for our new item");
    }];

    // Add a second item: this item should be uploaded after the failure of the first item
    [self addGenericPassword:@"localchange" account:@"account-delete-me-2"];

    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName: itemUUID]];

    // Also, this write will increment the class C current pointer's etag
    CKRecordID* currentClassCID = [[CKRecordID alloc] initWithRecordName: @"classC" zoneID: self.keychainZoneID];
    CKRecord* currentClassC = self.keychainZone.currentDatabase[currentClassCID];
    XCTAssertNotNil(currentClassC, "Should have the class C current key pointer record");
    [self.keychainZone addCKRecordToZone:[currentClassC copy]];
    XCTAssertNotEqualObjects(currentClassC.etag, self.keychainZone.currentDatabase[currentClassCID].etag, "Etag should have changed");

    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // Allow the outgoing queue operation to proceed
    [self.operationQueue addOperation:self.defaultCKKS.holdOutgoingQueueOperation];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self checkGenericPassword:@"data" account:@"account-delete-me"];
    [self checkGenericPassword:@"localchange" account:@"account-delete-me-2"];
}

- (void)testReceiveUnknownField {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

    NSError* error = nil;

    // Manually encrypt an item
    NSString* recordName = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.keychainZoneID];
    NSDictionary* item = [self fakeRecordDictionary: @"account-delete-me" zoneID:self.keychainZoneID];
    CKKSItem* cipheritem = [[CKKSItem alloc] initWithUUID:recordID.recordName
                                            parentKeyUUID:self.keychainZoneKeys.classA.uuid
                                                   zoneID:recordID.zoneID];
    CKKSKeychainBackedKey* itemkey = [CKKSKeychainBackedKey randomKeyWrappedByParent:[self.keychainZoneKeys.classA getKeychainBackedKey:&error] error:&error];
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

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    NSDictionary* query = @{(id)kSecClass: (id)kSecClassGenericPassword,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrAccount: @"account-delete-me",
                            (id)kSecMatchLimit: (id)kSecMatchLimitOne,
    };
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Found synced item");

    // Test that if this item is updated, it remains encrypted in v2, and future_field still exists
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self updateGenericPassword:@"different password" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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

- (void)testLocalUpdateToTombstoneItem {
    // Some CKKS clients may accidentally upload entries with tomb=1.
    // We should delete these items with extreme predjudice.
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single record to be deleted.
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, SOS comes along and updates the tombstone
    // CKKS should _not_ try to upload a tombstone
    NSDictionary *tombquery = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccount : @"account-delete-me",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrTombstone : @YES,
    };

    NSDictionary* update = @{
        (id)kSecAttrModificationDate : [NSDate date],
    };

    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        bool ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteSOSTransactionType, &cferror, ^bool {
            OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)tombquery, (__bridge CFDictionaryRef)update);
            XCTAssertEqual(status, errSecSuccess, "Should have been able to update a tombstone");

            return true;
        });
        return ok;
    });

    XCTAssertNil((__bridge NSError*)cferror, "Should be no error updating a tombstone");

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testIgnoreUpdateToModificationDateItem {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Nothing more should be uploaded
    NSDictionary *query = @{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccount : @"account-delete-me",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
    };

    NSDictionary* update = @{
        (id)kSecAttrModificationDate : [NSDate date],
    };

    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        bool ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteSOSTransactionType, &cferror, ^bool {
            OSStatus status = SecItemUpdate((__bridge CFDictionaryRef)query, (__bridge CFDictionaryRef)update);
            XCTAssertEqual(status, errSecSuccess, "Should have been able to update the item");

            return true;
        });
        return ok;
    });

    XCTAssertNil((__bridge NSError*)cferror, "Should be no error updating just the mdat");

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testUploadPagination {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    for(size_t count = 0; count < 250; count++) {
        [self addGenericPassword: @"data" account: [NSString stringWithFormat:@"account-delete-me-%03lu", count]];
    }

    [self expectCKModifyItemRecords: SecCKKSOutgoingQueueItemsAtOnce currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: SecCKKSOutgoingQueueItemsAtOnce currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: 50 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // For the next 5 seconds, try to add and then find an item. Each attempt should be fairly quick: no long multisecond pauses while CKKS Scans
    NSTimeInterval elapsed = 0;
    uint64_t count = 0;
    while(elapsed < 10) {
        NSDate* begin = [NSDate now];

        NSString* account = [NSString stringWithFormat:@"non-syncable-%d", (int)count];

        NSDictionary* query = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrAccount : account,
            (id)kSecAttrSynchronizable : @NO,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecValueData : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        };

        XCTAssertEqual(SecItemAdd((__bridge CFDictionaryRef)query, NULL), errSecSuccess, @"Should be able to add nonsyncable item");
        ckksnotice("ckkstest", self.keychainView, "SecItemAdd of %@ successful", account);

        NSDictionary *findQuery = @{
            (id)kSecClass : (id)kSecClassGenericPassword,
            (id)kSecAttrAccount : account,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
        };
        XCTAssertEqual(SecItemCopyMatching((__bridge CFDictionaryRef)findQuery, NULL), errSecSuccess, "Finding item %@", account);
        ckksnotice("ckkstest", self.keychainView, "SecItemCopyMatching of %@ successful", account);

        NSDate* end = [NSDate now];
        NSTimeInterval delta = [end timeIntervalSinceDate:begin];

        //XCTAssertLessThan(delta, 2, @"Keychain API should respond in two seconds");
        ckksnotice("ckkstest", self.keychainView, "SecItemAdd/SecItemCopyMatching pair of %@ took %.4fs", account, delta);

        usleep(10000); // sleep for 10ms, to let some other things get done

        // And retake the time elasped for the overall count
        elapsed += [[NSDate now] timeIntervalSinceDate:begin];
        count += 1;
    }

    OCMVerifyAllWithDelay(self.mockDatabase, 40);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self measureBlock:^{
        CKKSScanLocalItemsOperation* scan = [[CKKSScanLocalItemsOperation alloc] initWithDependencies:self.defaultCKKS.operationDependencies
                                                                                            intending:CKKSStateBecomeReady
                                                                                           errorState:CKKSStateError
                                                                                     ckoperationGroup:nil];
        [self.operationQueue addOperation:scan];
        [scan waitUntilFinished];
    }];
}

- (void)testReceiveClassCItemWhileALocked {
    // Test starts with a key hierarchy already existing.
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"classCItem" expecting:errSecItemNotFound];
    [self findGenericPassword:@"classAItem" expecting:errSecItemNotFound];

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    XCTAssertNotNil(self.keychainZoneKeys, "Have zone keys for zone");
    XCTAssertNotNil(self.keychainZoneKeys.classA, "Have class A key for zone");
    XCTAssertNotNil(self.keychainZoneKeys.classC, "Have class C key for zone");

    [self.defaultCKKS.stateMachine handleFlag:CKKSFlagKeyStateProcessRequested];

    // And ensure we end up back in 'ready': we have the keys, we're just locked now
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"classCItem" key:self.keychainZoneKeys.classC]];
    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-FFFF-FFFF-FFFF-5A507ACB2D85" withAccount:@"classAItem" key:self.keychainZoneKeys.classA]];

    CKKSResultOperation* op = self.defaultCKKS.resultsOfNextProcessIncomingQueueOperation;
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // If you're looking at CKKS internals, you'd better expect this to error
    XCTAssertNotNil(op.error, "results should be an error while failing to process a class A item");

    // During this operation, the keychain view should never go through 'fetching'
    CKKSCondition* keychainFetchState = self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateFetch];

    CKKSResultOperation* erroringOp = [self.defaultCKKS rpcFetchAndProcessIncomingQueue:nil
                                                                                because:CKKSFetchBecauseTesting
                                                                   errorOnClassAFailure:true];
    [erroringOp waitUntilFinished];
    XCTAssertNotNil(erroringOp.error, "error exists while processing a class A item");

    // But the API should swallow lock state errors.
    CKKSResultOperation* nonErroringOp = [self.defaultCKKS rpcFetchAndProcessIncomingQueue:nil
                                                                                   because:CKKSFetchBecauseTesting
                                                                      errorOnClassAFailure:false];
    [nonErroringOp waitUntilFinished];
    XCTAssertNil(nonErroringOp.error, "no error should while processing only class C items");

    [self findGenericPassword:@"classCItem" expecting:errSecSuccess];
    [self findGenericPassword:@"classAItem" expecting:errSecItemNotFound];

    XCTAssertNotEqual(0, [keychainFetchState wait:10*NSEC_PER_MSEC], "Should not have gone through 'fetching' in the view in response to an API fetch");

    NSOperation* results = [self.defaultCKKS resultsOfNextProcessIncomingQueueOperation];
    self.aksLockState = false;
    [self.lockStateTracker recheck];
    [results waitUntilFinished];

    [self findGenericPassword:@"classCItem" expecting:errSecSuccess];
    [self findGenericPassword:@"classAItem" expecting:errSecSuccess];
}

@end

#endif // OCTAGON
