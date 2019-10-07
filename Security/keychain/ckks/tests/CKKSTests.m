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
#include <securityd/SecItemDb.h>
#include <securityd/SecItemServer.h>
#include <utilities/SecFileLocations.h>
#include "keychain/SecureObjectSync/SOSInternal.h"

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
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSHealKeyHierarchyOperation.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSPeer.h"

#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ckks/tests/CKKSTests.h"
#import <utilities/SecCoreAnalytics.h>

// break abstraction
@interface CKKSLockStateTracker ()
@property (nullable) NSDate* lastUnlockedTime;
@end

@implementation CloudKitKeychainSyncingTests

#pragma mark - Tests

- (void)testBringupToKeyStateReady {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
}

- (void)testAddItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testActiveTLKS {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    NSDictionary<NSString *,NSString *>* tlks = [[CKKSViewManager manager] activeTLKs];

    XCTAssertEqual([tlks count], (NSUInteger)1, "One TLK");
    XCTAssertNotNil(tlks[@"keychain"], "keychain have a UUID");
}


- (void)testAddMultipleItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAddItemWithoutUUID {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];

    [self.keychainView waitUntilAllOperationsAreFinished];

    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    SecCKKSTestSetDisableAutomaticUUID(false);

    // We then expect an upload of the added item
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And then modified.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testModifyItemPrimaryKey {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
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
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

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

    // Run the reencrypt items operation to completion.
    [self.operationQueue addOperation: self.keychainView.holdReencryptOutgoingItemsOperation];
    [self.keychainView waitForOperationsOfClass:[CKKSReencryptOutgoingItemsOperation class]];

    [self.operationQueue addOperation: self.keychainView.holdOutgoingQueueOperation];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Right now, the write in CloudKit is pending. Make the local modification...
    [self updateGenericPassword: @"otherdata" account:account];

    // And then schedule the update, but for the final version of the password
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up uploading the new item)
    [self releaseCloudKitModificationHold];

    // Item should upload.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testOutgoingQueueRecoverFromStaleInflightEntry {
    // CKKS is restarting with an existing in-flight OQE
    // Note that this test is incomplete, and doesn't re-add the item to the local keychain
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"fake-account";

    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;

        CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];

        CKKSItem* item = [self newItem:ckrid withNewItemData:[self fakeRecordDictionary:account zoneID:self.keychainZoneID] key:self.keychainZoneKeys.classC];
        XCTAssertNotNil(item, "Should be able to create a new fake item");

        CKKSOutgoingQueueEntry* oqe = [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:item action:SecCKKSActionAdd state:SecCKKSStateInFlight waitUntil:nil accessGroup:@"ckks"];
        XCTAssertNotNil(oqe, "Should be able to create a new fake OQE");
        [oqe saveToDatabase:&error];

        XCTAssertNil(error, "Shouldn't error saving new OQE to database");
        return true;
    }];

    NSError *error = NULL;
    XCTAssertEqual([CKKSOutgoingQueueEntry countByState:SecCKKSStateInFlight zone:self.keychainZoneID error:&error], 1,
                   "Expected on inflight entry in outgoing queue: %@", error);

    // When CKKS restarts, it should find and re-upload this item
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];

    [self startCKKSSubsystem];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data"]];

    // The cloudkit operation finishes, letting the next OQO proceed (and set up uploading the new item)
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
}

- (void)testDeleteItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single record to be deleted.
    [self expectCKDeleteItemRecords: 1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteItemImmediatelyAfterModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, hold the modify
    [self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
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

- (void)testDeleteItemAfterFetchAfterModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, hold the modify
    //[self holdCloudKitModifications];

    // We expect a single record to be uploaded, but need to hold the operation from finishing until we can modify the item locally
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"otherdata"]];

    [self updateGenericPassword: @"otherdata" account:account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteItemWithoutTombstones {
    // The keychain API allows a client to ask for an inconsistent sync state:
    // They can ask for a local item deletion without propagating the deletion off-device.
    // This is the only halfway reasonable way to do keychain item deletions on account signout with the current API

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    [self deleteGenericPasswordWithoutTombstones:account];
    [self findGenericPassword:account expecting:errSecItemNotFound];

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    // Ensure nothing is in the outgoing queue
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID
                                                               error:&error];
        XCTAssertNil(error, "should be no error fetching uuids");
        XCTAssertEqual(uuids.count, 0u, "There should be zero OQEs");
        return false;
    }];

    // And a simple fetch doesn't bring it back
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:account expecting:errSecItemNotFound];

    // but a resync does
    CKKSSynchronizeOperation* resyncOperation = [self.keychainView resyncWithCloud];
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

    for(int i = 12; i < 100; i++) {
        @autoreleasepool {
            NSString* recordName = [NSString stringWithFormat:@"7B598D31-F9C5-481E-98AC-%012d", i];
            NSString* account = [NSString stringWithFormat:@"account%d", i];

            [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:recordName withAccount:account]];
        }
    }

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
    [self.keychainView notifyZoneChange:nil];;

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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

    [self.keychainView waitForKeyHierarchyReadiness];
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
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID ?: [[CKRecordZoneID alloc] initWithZoneName:@"keychain"
                                                                                                                           ownerName:CKCurrentUserDefaultName]
                                                               error:&error];
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

- (void)testReceiveCloudKitConflictOnJustAddedItems {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // Place a hold on processing the outgoing queue.
    self.keychainView.holdOutgoingQueueOperation = [CKKSResultOperation named:@"outgoing-queue-hold" withBlock:^{
        secnotice("ckks", "Outgoing queue hold released.");
    }];

    [self addGenericPassword:@"localchange" account:@"account-delete-me"];

    // Pull out the new item's UUID.
    __block NSString* itemUUID = nil;
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        NSArray<NSString*>* uuids = [CKKSOutgoingQueueEntry allUUIDs:self.keychainZoneID ?: [[CKRecordZoneID alloc] initWithZoneName:@"keychain"
                                                                                                                           ownerName:CKCurrentUserDefaultName]
                                                               error:&error];
        XCTAssertNil(error, "no error fetching uuids");
        XCTAssertEqual(uuids.count, 1u, "There's exactly one outgoing queue entry");
        itemUUID = uuids[0];

        XCTAssertNotNil(itemUUID, "Have a UUID for our new item");
        return false;
    }];

    // Add a second item: this item should be uploaded after the failure of the first item
    [self addGenericPassword:@"localchange" account:@"account-delete-me-2"];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName: itemUUID]];

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
    [self.operationQueue addOperation:self.keychainView.holdOutgoingQueueOperation];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self.keychainView waitUntilAllOperationsAreFinished];

    [self checkGenericPassword:@"data" account:@"account-delete-me"];
    [self checkGenericPassword:@"localchange" account:@"account-delete-me-2"];
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

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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

    OCMVerifyAllWithDelay(self.mockDatabase, 40);
}

- (void)testUploadInitialKeyHierarchy {
    // Test starts with nothing in database. CKKS should get into the "please upload my keys" state, then Octagon should perform the upload

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
}

- (void)testDoNotErrorIfNudgedWhileWaitingForTLKUpload {
    // Test starts with nothing in database. CKKS should get into the "please upload my keys" state, then Octagon should perform the upload

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keysetOps = [NSMutableArray array];

    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:40*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation' (view %@)", view);
        [keysetOps addObject: [view findKeySet]];
    }

    // Now that we've kicked them all off, wait for them to resolve (and nudge each one, as if a key was saved)
    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKUpload] wait:40*NSEC_PER_SEC], @"key state should enter 'waitfortlkupload'");

        CKKSCondition* viewProcess = view.keyHierarchyConditions[SecCKKSZoneKeyStateProcess];
        [view keyStateMachineRequestProcess];
        XCTAssertNotEqual(0, [viewProcess wait:500*NSEC_PER_MSEC], "CKKS should not reprocess the key hierarchy, even if nudged");
    }

    // The views should remain in waitfortlkcreation, and not go through process into an error

    NSMutableArray<CKRecord*>* keyHierarchyRecords = [NSMutableArray array];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp in keysetOps) {
        // Wait until finished is usually a bad idea. We could rip this out into an operation if we'd like.
        [keysetOp waitUntilFinished];
        XCTAssertNil(keysetOp.error, "Should be no error fetching keyset from CKKS");

        NSArray<CKRecord*>* records = [self putKeySetInCloudKit:keysetOp.keyset];
        [keyHierarchyRecords addObjectsFromArray:records];
    }

    // Tell our views about our shiny new records!
    for(CKKSKeychainView* view in self.ckksViews) {
        [view receiveTLKUploadRecords: keyHierarchyRecords];
    }
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
}

- (void)testProvideKeysetFromNoTrust {

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortlkcreation'");
    // I'm not sure how CKKS ends up in 'waitfortrust' without a keyset, so force that state
    // In 52301278, it occurred with some complex interaction of zone deletions, fetches, and trust operations
    [self.keychainView dispatchSyncWithAccountKeys:^bool{
        [self.keychainView _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForTrust withError:nil];
        return true;
    }];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.keychainView findKeySet];
    [keysetOp timeout:20*NSEC_PER_SEC];
    [keysetOp waitUntilFinished];

    XCTAssertNil(keysetOp.error, "Should be no error fetching a keyset");
}

// This test no longer is very interesting, since Octagon needs to handle lock states, not CKKS...
- (void)testUploadInitialKeyHierarchyAfterLockedStart {
    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");

    // After unlock, the key hierarchy should be created.
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    [self performOctagonTLKUpload:self.ckksViews];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testLockImmediatelyAfterUploadingInitialKeyHierarchy {

    __weak __typeof(self) weakSelf = self;

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews afterUpload:^{
        __strong __typeof(self) strongSelf = weakSelf;
        [strongSelf holdCloudKitFetches];
    }];

    // Should enter 'ready'
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, lock and allow fetches again
    self.aksLockState = true;
    [self.lockStateTracker recheck];
    [self releaseCloudKitFetchHold];

    CKKSResultOperation* op = [self.keychainView.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting];
    [op waitUntilFinished];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Wait for CKKS to shake itself out...
    [self.keychainView waitForOperationsOfClass:[CKKSProcessReceivedKeysOperation class]];

    // Should  be in ReadyPendingUnlock
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReadyPendingUnlock] wait:20*NSEC_PER_SEC], @"Key state should become 'readypendingunlock'");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testReceiveKeyHierarchyAfterLockedStart {
    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");

    // Now, another device comes along and creates the hierarchy; we download it; and it and sends us the TLK
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    [[self.keychainView.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting] waitUntilFinished];

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    // After unlock, the TLK arrives
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testLoadKeyHierarchyAfterLockedStart {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReadyPendingUnlock] wait:20*NSEC_PER_SEC], @"Key state should become 'readypendingunlock'");

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testUploadAndUseKeyHierarchy {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };
    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not exist");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testUploadInitialKeyHierarchyTriggersBackup {
    // We also expect the view manager's notifyNewTLKsInKeychain call to fire (after some delay)
    OCMExpect([self.mockCKKSViewManager notifyNewTLKsInKeychain]);

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    OCMVerifyAllWithDelay(self.mockCKKSViewManager, 10);
}

- (void)testResetCloudKitZoneFromNoTLK {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // If CKKS sees a zone it's never going to be able to read, it should reset that zone
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // explicitly do not save a fake device status here
    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");

    // But then, it'll fire off the reset and reach 'ready', with a little help from octagon
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testResetCloudKitZoneFromNoTLKWithOtherWaitForTLKDevices {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // If CKKS sees a zone it's never going to be able to read, it should reset that zone
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Save a fake device status here, but modify its key state to be 'waitfortlk': it has no idea what the TLK is either
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self putFakeOctagonOnlyDeviceStatusInCloudKit:self.keychainZoneID];

    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType]) {
            record[SecCKRecordKeyState] = CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForTLK);
        }
    }

    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    // But then, it'll fire off the reset and reach 'ready'
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testResetCloudKitZoneFromNoTLKIgnoringInactiveDevices {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // If CKKS sees a zone it's never going to be able to read, it should reset that zone
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Save a fake device status here, but modify its creation and modification times to be months ago
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self putFakeOctagonOnlyDeviceStatusInCloudKit:self.keychainZoneID];

    // Put a 'in-circle' TLKShare record, but also modify its creation and modification times
    CKKSSOSSelfPeer* untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"untrusted-peer"
                                                                  encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                     signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                       viewList:self.managedViewList];
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:untrustedPeer to:untrustedPeer zoneID:self.keychainZoneID];

    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType] || [record.recordType isEqualToString:SecCKRecordTLKShareType]) {
            record.creationDate = [NSDate distantPast];
            record.modificationDate = [NSDate distantPast];
        }
    }

    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    // But then, it'll fire off the reset and reach 'ready'
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testDoNotResetCloudKitZoneDuringBadCircleState {
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // This test has stuff in CloudKit, but no TLKs.
    // CKKS should NOT reset the CK zone.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    self.zones[self.keychainZoneID].flag = true;

    [self startCKKSSubsystem];

    // But since we're out of circle, this test needs to initialize the zone itself
    [self.keychainView beginCloudKitOperation];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    FakeCKZone* keychainZone = self.zones[self.keychainZoneID];
    XCTAssertNotNil(keychainZone, "Should still have a keychain zone");
    XCTAssertTrue(keychainZone.flag, "keychain zone should not have been recreated");
}

- (void)testDoNotResetCloudKitZoneFromWaitForTLKDueToRecentDeviceState {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS shouldn't reset this zone, due to a recent device status claiming to have TLKs
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Also, CKKS _should_ be able to return the key hierarchy if asked before it starts
    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.keychainView findKeySet];

    NSDateComponents* offset = [[NSDateComponents alloc] init];
    [offset setDay:-5];
    NSDate* updateTime = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:[NSDate date] options:0];
    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType] || [record.recordType isEqualToString:SecCKRecordTLKShareType]) {
            record.creationDate = updateTime;
            record.modificationDate = updateTime;
        }
    }

    self.keychainZone.flag = true;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortlk'");

    XCTAssertTrue(self.keychainZone.flag, "Zone flag should not have been reset to false");

    // And, ensure that the keyset op ran and has results
    CKKSResultOperation* waitOp = [CKKSResultOperation named:@"test op" withBlock:^{}];
    [waitOp addDependency:keysetOp];
    [waitOp timeout:2*NSEC_PER_SEC];
    [self.operationQueue addOperation:waitOp];
    [waitOp waitUntilFinished];

    XCTAssert(keysetOp.finished, "Keyset op should have finished");
    XCTAssertNil(keysetOp.error, "keyset op should not have errored");
    XCTAssertNotNil(keysetOp.keyset, "keyset op should have a keyset");
    XCTAssertNotNil(keysetOp.keyset.currentTLKPointer, "keyset should have a current TLK pointer");
    XCTAssertEqualObjects(keysetOp.keyset.currentTLKPointer.currentKeyUUID, self.keychainZoneKeys.tlk.uuid, "keyset should match what's in zone");
}

- (void)testDoNotCloudKitZoneFromWaitForTLKDueToRecentButUntrustedDeviceState {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS should reset this zone, even though to a recent device status claiming to have TLKs. The device isn't trusted
    self.silentZoneDeletesAllowed = true;
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self.mockSOSAdapter.trustedPeers removeObject:self.remoteSOSOnlyPeer];

    self.keychainZone.flag = true;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortlk'");
    XCTAssertTrue(self.keychainZone.flag, "Zone flag should not have been reset to false");

    // And ensure it doesn't go on to 'reset'
    XCTAssertNotEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:100*NSEC_PER_MSEC], @"Key state should not become 'resetzone'");
}

- (void)testResetCloudKitZoneFromWaitForTLKDueToLessRecentAndUntrustedDeviceState {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS should reset this zone, even though to a recent device status claiming to have TLKs. The device isn't trusted
    self.silentZoneDeletesAllowed = true;
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self.mockSOSAdapter.trustedPeers removeObject:self.remoteSOSOnlyPeer];

    NSDateComponents* offset = [[NSDateComponents alloc] init];
    [offset setDay:-5];
    NSDate* updateTime = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:[NSDate date] options:0];
    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType] || [record.recordType isEqualToString:SecCKRecordTLKShareType]) {
            record.creationDate = updateTime;
            record.modificationDate = updateTime;
        }
    }

    self.keychainZone.flag = true;
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    // Then we should reset.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testAcceptExistingKeyHierarchy {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should only upload its TLK share
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    // But, CKKS shouldn't ever reset the zone
    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:200*NSEC_PER_SEC], "Key state should have become waitfortlk");

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS). We'll create a TLK share for ourselves.
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Wait for the key hierarchy to sort itself out, to make it easier on this test; see testOnboardOldItemsWithExistingKeyHierarchy for the other test.
    // The CKKS subsystem should write its TLK share, but nothing else
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertTrue(self.keychainZone.flag, "Keychain zone shouldn't have been reset");
}

- (void)testAcceptExistingKeyHierarchyDespiteLocked {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForUnlock] wait:20*NSEC_PER_SEC], "Key state should have become waitforunlock");

    // CKKS will give itself a TLK Share
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // Now that all operations are complete, 'unlock' AKS
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");

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

- (void)testReceiveClassCWhileALocked {
    // Test starts with a key hierarchy already existing.
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"classCItem" expecting:errSecItemNotFound];
    [self findGenericPassword:@"classAItem" expecting:errSecItemNotFound];

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    XCTAssertNotNil(self.keychainZoneKeys, "Have zone keys for zone");
    XCTAssertNotNil(self.keychainZoneKeys.classA, "Have class A key for zone");
    XCTAssertNotNil(self.keychainZoneKeys.classC, "Have class C key for zone");

    [self.keychainView dispatchSyncWithAccountKeys: ^bool {
        [self.keychainView _onqueueKeyStateMachineRequestProcess];
        return true;
    }];
    // And ensure we end up back in 'readypendingunlock': we have the keys, we're just locked now
    [self.keychainView waitForOperationsOfClass:[CKKSProcessReceivedKeysOperation class]];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReadyPendingUnlock] wait:20*NSEC_PER_SEC], @"Key state should become 'readypendingunlock'");

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

- (void)testRestartWhileLocked {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self.keychainView halt];
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self.keychainView beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReadyPendingUnlock] wait:20*NSEC_PER_SEC], @"Key state should become 'readypendingunlock'");

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
}

- (void)testExternalKeyRoll {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    __weak __typeof(self) weakSelf = self;

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Trigger a notification
    [self.keychainView notifyZoneChange:nil];

    // Make life easy on this test; testAcceptKeyConflictAndUploadReencryptedItem will check the case when we don't receive the notification
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Just in extra case of threading issues, force a reexamination of the key hierarchy
    [self.keychainView dispatchSyncWithAccountKeys: ^bool {
        [self.keychainView _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
        return true;
    }];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

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

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAcceptKeyConflictAndUploadReencryptedItem {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Do not trigger a notification here. This should cause a conflict updating the current key records

    // We expect a single record to be uploaded, but that the write will be rejected
    // We then expect that item to be reuploaded with the current key

    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under rolled class C key in hierarchy"]];

    // New key arrives via SOS!
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAcceptKeyConflictAndUploadReencryptedItems {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Do not trigger a notification here. This should cause a conflict updating the current key records

    // We expect a single record to be uploaded, but that the write will be rejected
    // We then expect that item to be reuploaded with the current key

    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under rolled class C key in hierarchy"]];

    // New key arrives via SOS!
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromRequestKeyRefetchWithoutRolling {
    // Simply requesting a key state refetch shouldn't roll the key hierarchy.

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Items should upload.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];


    // CKKS should not roll the keys while progressing back to 'ready', but it will fetch once
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    [self.keychainView dispatchSyncWithAccountKeys: ^bool {
        [self.keychainView _onqueueKeyStateMachineRequestFetch];
        return true;
    }];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromIncrementedCurrentKeyPointerEtag {
    // CloudKit sometimes reports the current key pointers have changed (etag mismatch), but their content hasn't.
    // In this case, CKKS shouldn't roll the TLK.

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self.keychainView waitForFetchAndIncomingQueueProcessing]; // just to be sure it's fetched

    // Items should upload.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];

    // Bump the etag on the class C current key record, but don't change any data
    CKRecordID* currentClassCID = [[CKRecordID alloc] initWithRecordName: @"classC" zoneID: self.keychainZoneID];
    CKRecord* currentClassC = self.keychainZone.currentDatabase[currentClassCID];
    XCTAssertNotNil(currentClassC, "Should have the class C current key pointer record");

    [self.keychainZone addCKRecordToZone:[currentClassC copy]];
    XCTAssertNotEqualObjects(currentClassC.etag, self.keychainZone.currentDatabase[currentClassCID].etag, "Etag should have changed");

    // Add another item. This write should fail, then CKKS should recover without rolling the key hierarchy or issuing a fetch.
    self.silentFetchesAllowed = false;
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverMultipleItemsFromIncrementedCurrentKeyPointerEtag {
    // CloudKit sometimes reports the current key pointers have changed (etag mismatch), but their content hasn't.
    // In this case, CKKS shouldn't roll the TLK.
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self.keychainView waitForFetchAndIncomingQueueProcessing]; // just to be sure it's fetched

    // Items should upload.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];

    // Bump the etag on the class C current key record, but don't change any data
    CKRecordID* currentClassCID = [[CKRecordID alloc] initWithRecordName: @"classC" zoneID: self.keychainZoneID];
    CKRecord* currentClassC = self.keychainZone.currentDatabase[currentClassCID];
    XCTAssertNotNil(currentClassC, "Should have the class C current key pointer record");

    [self.keychainZone addCKRecordToZone:[currentClassC copy]];
    XCTAssertNotEqualObjects(currentClassC.etag, self.keychainZone.currentDatabase[currentClassCID].etag, "Etag should have changed");

    // Add another item. This write should fail, then CKKS should recover without rolling the key hierarchy or issuing a fetch.
    self.keychainView.holdOutgoingQueueOperation = [CKKSGroupOperation named:@"outgoing-hold" withBlock: ^{
        secnotice("ckks", "releasing outgoing-queue hold");
    }];

    self.silentFetchesAllowed = false;
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.operationQueue addOperation: self.keychainView.holdOutgoingQueueOperation];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsCreatingKeyHierarchy {
    // In this test, we'll check if the CKKS subsystem will pick up a keychain item which existed before the key hierarchy, both with and without a UUID attached at item creation

    // Test starts with nothing in CloudKit, and CKKS blocked. Add one item without a UUID...

    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // We then expect an upload of the added items
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchy {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchyExtantTLK {
    // Test starts key hierarchy in our fake CloudKit, the TLK arrived in the local keychain, and CKKS blocked.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
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

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchyLateTLK {
    // Test starts key hierarchy in our fake CloudKit, and CKKS blocked.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    self.keychainZone.flag = true;

    // Add one item without a UUID...
    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // Now that we have an item in the keychain, allow CKKS to spin up. It should upload the item, using the key hierarchy already extant

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should have become waitfortlk");

    // Now, save the TLK to the keychain (to simulate it coming in via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertTrue(self.keychainZone.flag, "Keychain zone shouldn't have been reset");
}

- (void)testResync {
    // We need to set up a desynced situation to test our resync.
    // First, let CKKS start up and send several items to CloudKit (that we'll then desync!)
    __block NSError* error = nil;

    // Test starts with keys in CloudKit (so we can create items later)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    [self addGenericPassword: @"data" account: @"third"];
    [self addGenericPassword: @"data" account: @"fourth"];
    [self addGenericPassword: @"data" account: @"fifth"];
    NSUInteger passwordCount = 5u;

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
    [self checkGenericPassword: @"data" account: @"third"];
    [self checkGenericPassword: @"data" account: @"fourth"];
    [self checkGenericPassword: @"data" account: @"fifth"];

    [self expectCKModifyItemRecords: passwordCount currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    // One TLK share record
    XCTAssertEqual(self.keychainZone.currentDatabase.count, SYSTEM_DB_RECORD_COUNT+passwordCount+1, "Have SYSTEM_DB_RECORD_COUNT+passwordCount+1 objects in cloudkit");

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

    // The fifth record is updated locally, but CKKS didn't get the notification, and so the local CKMirror and CloudKit don't have it
    // Expected outcome: local change should be steamrolled by the cloud version
    CKRecord* localDataChanged = items[4];
    NSMutableDictionary* localDataDictionary = [[self decryptRecord: localDataChanged] mutableCopy];
    NSString* localDataChangedAccount = [localDataDictionary objectForKey: (__bridge id) kSecAttrAccount];
    SecCKKSDisable();
    [self updateGenericPassword:@"newpassword" account:localDataChangedAccount];
    [self checkGenericPassword:@"newpassword"  account:localDataChangedAccount];
    SecCKKSEnable();

    // The sixth record gets magically added to CloudKit, but CKKS has never heard of it
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
    ckksnotice("ckksresync", self.keychainView, "local update:        %@ %@", items[4].recordID.recordName, localDataChangedAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote only:         %@ %@", ckr.recordID.recordName, remoteOnlyAccount);

    CKKSSynchronizeOperation* resyncOperation = [self.keychainView resyncWithCloud];
    [resyncOperation waitUntilFinished];

    XCTAssertNil(resyncOperation.error, "No error during the resync operation");

    // Now do some checking. Remember, we don't know which record we corrupted, so use the parsed account variables to check.

    [self findGenericPassword: deleteAccount            expecting: errSecSuccess];
    [self findGenericPassword: remoteDeleteAccount      expecting: errSecItemNotFound];
    [self findGenericPassword: remoteDataChangedAccount expecting: errSecSuccess];
    [self findGenericPassword: insyncAccount            expecting: errSecSuccess];
    [self findGenericPassword: localDataChangedAccount  expecting: errSecSuccess];
    [self findGenericPassword: remoteOnlyAccount        expecting: errSecSuccess];

    [self checkGenericPassword: @"data"         account: deleteAccount];
    //[self checkGenericPassword: @"data"         account: remoteDeleteAccount];
    [self checkGenericPassword: @"CloudKitWins" account: remoteDataChangedAccount];
    [self checkGenericPassword: @"data"         account: insyncAccount];
    [self checkGenericPassword:@"data"          account:localDataChangedAccount];
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

        ckme = [CKKSMirrorEntry tryFromDatabase:items[4].recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:ckr.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);
        return true;
    }];
}

- (void)testResyncItemsMissingFromLocalKeychain {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We want:
    //   one password correctly synced between local keychain and CloudKit
    //   one password incorrectly disappeared from local keychain, but in mirror table
    //   one password sitting in the outgoing queue
    //   one password sitting in the incoming queue

    // Add and sync two passwords
    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Now, place an item in the outgoing queue

    //[self addGenericPassword: @"data" account: @"third"];
    //[self checkGenericPassword: @"data" account: @"third"];

    // Now, corrupt away!
    // Extract all passwordCount items for Corruption
    NSArray<CKRecord*>* items = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    XCTAssertEqual(items.count, 2u, "Have %lu Items in cloudkit", (unsigned long)2u);

    // For the first record, surreptitiously remove from local keychain
    CKRecord* remove = items[0];
    NSString* removeAccount = [[self decryptRecord:remove] objectForKey:(__bridge id)kSecAttrAccount];
    XCTAssertNotNil(removeAccount, "received an account for the local delete object");

    NSURL* kcpath = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"keychain-2-debug.db");
    sqlite3* db;
    sqlite3_open([[kcpath path] UTF8String], &db);
    NSString* query = [NSString stringWithFormat:@"DELETE FROM genp WHERE uuid=\"%@\"", remove.recordID.recordName];
    char* sqlerror = NULL;
    XCTAssertEqual(SQLITE_OK, sqlite3_exec(db, [query UTF8String], NULL, NULL, &sqlerror), "SQL deletion shouldn't error");
    XCTAssertTrue(sqlerror == NULL, "No error string should have been returned: %s", sqlerror);
    if(sqlerror) {
        sqlite3_free(sqlerror);
        sqlerror = NULL;
    }
    sqlite3_close(db);

    // The second record is kept in-sync

    // Now, add an in-flight change (for record 3)
    [self holdCloudKitModifications];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"data" account:@"third"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // For the fourth, add a new record but prevent incoming queue processing
    self.keychainView.holdIncomingQueueOperation = [CKKSResultOperation named:@"hold-incoming" withBlock:^{}];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"fourth"];
    [self.keychainZone addToZone:ckr];
    [self.keychainView notifyZoneChange:nil];

    // Now, where are we....
    CKKSScanLocalItemsOperation* scanLocal = [self.keychainView scanLocalItems:@"test-scan"];
    [scanLocal waitUntilFinished];

    XCTAssertEqual(scanLocal.missingLocalItemsFound, 1u, "Should have found one missing item");

    // Allow everything to proceed
    [self releaseCloudKitModificationHold];
    [self.operationQueue addOperation:self.keychainView.holdIncomingQueueOperation];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    // And ensure that all four items are present again
    [self findGenericPassword: @"first"  expecting: errSecSuccess];
    [self findGenericPassword: @"second" expecting: errSecSuccess];
    [self findGenericPassword: @"third"  expecting: errSecSuccess];
    [self findGenericPassword: @"fourth" expecting: errSecSuccess];
}

- (void)testScanItemsChangedInLocalKeychain {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Add and sync two passwords
    NSString* itemAccount = @"first";
    [self addGenericPassword:@"data" account:itemAccount];
    [self addGenericPassword:@"data" account:@"second"];

    [self checkGenericPassword:@"data" account:itemAccount];
    [self checkGenericPassword:@"data" account:@"second"];

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self.keychainView waitForOperationsOfClass:[CKKSScanLocalItemsOperation class]];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    // Now, have CKKS miss an update
    SecCKKSDisable();
    [self updateGenericPassword:@"newpassword" account:itemAccount];
    [self checkGenericPassword:@"newpassword"  account:itemAccount];
    SecCKKSEnable();

    // Now, where are we....
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:itemAccount password:@"newpassword"]];

    CKKSScanLocalItemsOperation* scanLocal = [self.keychainView scanLocalItems:@"test-scan"];
    [scanLocal waitUntilFinished];

    XCTAssertEqual(scanLocal.recordsAdded, 1u, "Should have added a single record");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And ensure that all four items are present again
    [self findGenericPassword: @"first"  expecting: errSecSuccess];
    [self findGenericPassword: @"second" expecting: errSecSuccess];
}

- (void)testResyncLocal {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    NSUInteger passwordCount = 2u;

    [self expectCKModifyItemRecords: passwordCount currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Local resyncs shouldn't fetch clouds.
    self.silentFetchesAllowed = false;
    SecCKKSDisable();
    [self deleteGenericPassword:@"first"];
    [self deleteGenericPassword:@"second"];
    SecCKKSEnable();

    // And they're gone!
    [self findGenericPassword:@"first" expecting:errSecItemNotFound];
    [self findGenericPassword:@"second" expecting:errSecItemNotFound];

    CKKSLocalSynchronizeOperation* op = [self.keychainView resyncLocal];
    [op waitUntilFinished];
    XCTAssertNil(op.error, "Shouldn't be an error resyncing locally");

    // And they're back!
    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
}

- (void)testPlistRestoreResyncsLocal {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    NSUInteger passwordCount = 2u;

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];

    [self expectCKModifyItemRecords:passwordCount currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // o no
    // This 'restores' a plist keychain backup
    // That will kick off a local resync in CKKS, so hold that until we're ready...
    self.keychainView.holdLocalSynchronizeOperation = [CKKSResultOperation named:@"hold-local-synchronize" withBlock:^{}];

    // Local resyncs shouldn't fetch clouds.
    self.silentFetchesAllowed = false;

    CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        CFErrorRef cfcferror = NULL;

        bool ret = SecServerImportKeychainInPlist(dbt, SecSecurityClientGet(), KEYBAG_NONE, KEYBAG_NONE,
                                                  (__bridge CFDictionaryRef)@{}, kSecBackupableItemFilter, false, &cfcferror);

        XCTAssertNil(CFBridgingRelease(cfcferror), "Shouldn't error importing a 'backup'");
        XCTAssert(ret, "Importing a 'backup' should have succeeded");
        return true;
    });
    XCTAssertNil(CFBridgingRelease(cferror), "Shouldn't error mucking about in the db");

    // Restore is additive so original items stick around
    [self findGenericPassword:@"first" expecting:errSecSuccess];
    [self findGenericPassword:@"second" expecting:errSecSuccess];

    // Allow the local resync to continue...
    [self.operationQueue addOperation:self.keychainView.holdLocalSynchronizeOperation];
    [self.keychainView waitForOperationsOfClass:[CKKSLocalSynchronizeOperation class]];

    // Items are still here!
    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
}

- (void)testMultipleZoneAdd {
    // Bring up a new zone: we expect a key hierarchy upload.
    CKKSKeychainView* atvView = [self.injectedManager findOrCreateView:(id)kSecAttrViewHintAppleTV];
    [self.ckksViews addObject:atvView];
    CKRecordZoneID* appleTVZoneID = [[CKRecordZoneID alloc] initWithZoneName:(__bridge NSString*) kSecAttrViewHintAppleTV ownerName:CKCurrentUserDefaultName];

    // We also expect the view manager's notifyNewTLKsInKeychain call to fire once (after some delay)
    OCMExpect([self.mockCKKSViewManager notifyNewTLKsInKeychain]);

    // Let the horses loose
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    // We expect a single record to be uploaded to the 'keychain' view
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single record to be uploaded to the 'atv' view
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:appleTVZoneID];
    [self addGenericPassword: @"atv"
                     account: @"tvaccount"
                    viewHint:(__bridge NSString*) kSecAttrViewHintAppleTV
                      access:(id)kSecAttrAccessibleAfterFirstUnlock
                   expecting:errSecSuccess message:@"AppleTV view-hinted object"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    OCMVerifyAllWithDelay(self.mockCKKSViewManager, 10);
}

- (void)testMultipleZoneDelete {
    [self startCKKSSubsystem];

    // Bring up a new zone: we expect a key hierarchy and an item.
    CKKSKeychainView* atvView = [self.injectedManager findOrCreateView:(id)kSecAttrViewHintAppleTV];
    XCTAssertNotNil(atvView, "Should have a new ATV view");
    [self.ckksViews addObject:atvView];
    [self beginSOSTrustedViewOperation:atvView];
    CKRecordZoneID* appleTVZoneID = [[CKRecordZoneID alloc] initWithZoneName:(__bridge NSString*) kSecAttrViewHintAppleTV ownerName:CKCurrentUserDefaultName];

    [self performOctagonTLKUpload:self.ckksViews];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:appleTVZoneID];
    [self addGenericPassword: @"atv"
                     account: @"tvaccount"
                    viewHint:(__bridge NSString*) kSecAttrViewHintAppleTV
                      access:(id)kSecAttrAccessibleAfterFirstUnlock
                   expecting:errSecSuccess
                     message:@"AppleTV view-hinted object"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single record to be deleted from the ATV zone
    [self expectCKDeleteItemRecords: 1 zoneID:appleTVZoneID];
    [self deleteGenericPassword:@"tvaccount"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now we expect a single record to be deleted from the test zone
    [self expectCKDeleteItemRecords: 1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRestartWithoutRefetch {
    // Restarting the CKKS operation should check that it's been 15 minutes since the last fetch before it fetches again. Simulate this.
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");

    // Tear down the CKKS object and disallow fetches
    [self.keychainView halt];
    self.silentFetchesAllowed = false;

    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Okay, cool, rad, now let's set the date to be very long ago and check that there's positively a fetch
    [self.keychainView halt];
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
    [self beginSOSTrustedViewOperation:self.keychainView];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromZoneCreationFailure {
    // Fail the zone creation.
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone
    [self failNextZoneCreation:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // CKKS should figure it out, and fix it
    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertNil(self.zones[self.keychainZoneID].creationError, "Creation error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromZoneSubscriptionFailure {
    // Fail the zone subscription.
    [self failNextZoneSubscription:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it before Octagon uploads its items
    [self performOctagonTLKUpload:self.ckksViews];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertFalse(self.zones[self.keychainZoneID].flag, "Zone flag was reset");
    XCTAssertNil(self.zones[self.keychainZoneID].subscriptionError, "Subscription error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromDeletedTLKWithStashedTLK {
    // We need to handle the case where our syncable TLKs are deleted for some reason. The device that has them might resurrect them

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSError* error = nil;

    // Stash the TLKs.
    [self.keychainZoneKeys.tlk saveKeyMaterialToKeychain:true error:&error];
    XCTAssertNil(error, "Should have received no error stashing the new TLK in the keychain");

    // And delete the non-stashed version
    [self.keychainZoneKeys.tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "Should have received no error deleting the new TLK from the keychain");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    [self.keychainView halt];

    NSError* error = nil;

    // Stash the TLKs.
    [self.keychainZoneKeys.tlk saveKeyMaterialToKeychain:true error:&error];
    XCTAssertNil(error, "Should have received no error stashing the new TLK in the keychain");

    [self.keychainZoneKeys.tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "Should have received no error deleting the new TLK from the keychain");

    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // CKKS should recreate the syncable TLK.
    [self checkNSyncableTLKsInKeychain: 1];
}

/*
 // <rdar://problem/49024967> Octagon: tests for CK exceptions out of cuttlefish
- (void)testRecoverFromTLKWriteFailure {
    // We need to handle the case where a device's first TLK write doesn't go through (due to whatever reason).
    // Test starts with nothing in CloudKit, and will fail the first TLK write.
    NSError* noNetwork = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}];
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:nil withError:noNetwork];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // A network failure creating new TLKs shouldn't delete the 'failed' syncable one.
    [self checkNSyncableTLKsInKeychain: 2];
}
 */

// This test needs to be moved and rewritten now that Octagon handles TLK uploads
// <rdar://problem/49024967> Octagon: tests for CK exceptions out of cuttlefish
/*
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now the TLKs arrive from the other device...
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // A race failure creating new TLKs should delete the old syncable one.
    [self checkNSyncableTLKsInKeychain: 1];
}
 */

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
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];

    [self.keychainView waitForKeyHierarchyReadiness];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

/*
 // <rdar://problem/49024967> Octagon: tests for CK exceptions out of cuttlefish
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

    // The CKKS subsystem should try to write TLKs, but fail. It'll then upload a TLK share for the keys already in CloudKit
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // CKKS should then happily use the keys in CloudKit
    [self createClassCItemAndWaitForUpload:self.keychainZoneID account:@"account-delete-me"];
    [self createClassAItemAndWaitForUpload:self.keychainZoneID account:@"account-delete-me-class-a"];
}
 */

- (void)testRecoverFromDeletedKeysNewItem {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // Now, delete the local keys from the keychain (but leave the synced TLK)
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                         (id)kSecClass : (id)kSecClassInternetPassword,
                                                                         (id)kSecUseDataProtectionKeychain : @YES,
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single class A record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromDeletedKeysReceive {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");

    [self waitForCKModifications];
    [self.keychainView waitUntilAllOperationsAreFinished];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];

    // Now, delete the local keys from the keychain (but leave the synced TLK)
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecUseDataProtectionKeychain : @YES,
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

- (void)testRecoverDeletedTLK {
    // If the TLK disappears halfway through, well, that's no good. But we should recover using TLK sharing

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];
    [self.keychainView waitUntilAllOperationsAreFinished];

    // Now, delete the local keys from the keychain
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecUseDataProtectionKeychain : @YES,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
                                                                            }), @"Deleting CKKS keys");
    SecCKKSTestSetDisableKeyNotifications(false);

    // Trigger a notification (with hilariously fake data)
    [self.keychainZone addToZone: ckr];
    [self.keychainView notifyZoneChange:nil];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should return to 'ready'");

    [self.keychainView waitForFetchAndIncomingQueueProcessing]; // Do this again, to allow for non-atomic key state machinery switching

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
}

- (void)testRecoverMissingRolledKey {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    NSString* accountShouldExist = @"under-rolled-key";
    NSString* accountWillExist = @"under-rolled-key-later";
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountShouldExist];
    [self.keychainZone addToZone: ckr];

    CKRecord* ckrAddedLater = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountWillExist];
    CKKSKey* pastClassCKey = self.keychainZoneKeys.classC;

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecItemNotFound];

    // Now, find and delete the class C key that ckrAddedLater is under
    NSError* error = nil;
    XCTAssertTrue([pastClassCKey deleteKeyMaterialFromKeychain:&error], "Should be able to delete old key material from keychain");
    XCTAssertNil(error, "Should be no error deleting old key material from keychain");

    [self.keychainZone addToZone:ckrAddedLater];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecSuccess];

    XCTAssertTrue([pastClassCKey loadKeyMaterialFromKeychain:&error], "Class C key should be back in the keychain");
    XCTAssertNil(error, "Should be no error loading key from keychain");
}

- (void)testRecoverMissingRolledClassAKeyWhileLocked {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    NSString* accountShouldExist = @"under-rolled-key";
    NSString* accountWillExist = @"under-rolled-key-later";
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountShouldExist key:self.keychainZoneKeys.classA];
    [self.keychainZone addToZone: ckr];

    CKRecord* ckrAddedLater = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountWillExist key:self.keychainZoneKeys.classA];
    CKKSKey* pastClassAKey = self.keychainZoneKeys.classA;

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecItemNotFound];

    // Now, find and delete the class C key that ckrAddedLater is under
    NSError* error = nil;
    XCTAssertTrue([pastClassAKey deleteKeyMaterialFromKeychain:&error], "Should be able to delete old key material from keychain");
    XCTAssertNil(error, "Should be no error deleting old key material from keychain");

    // now, lock the keychain
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self.keychainZone addToZone:ckrAddedLater];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Item should still not exist due to the lock state....
    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecItemNotFound];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReadyPendingUnlock] wait:20*NSEC_PER_SEC], "Key state should have returned to readypendingunlock");

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    // And now it does
    [self.keychainView waitUntilAllOperationsAreFinished];
    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecSuccess];

    XCTAssertTrue([pastClassAKey loadKeyMaterialFromKeychain:&error], "Class A key should be back in the keychain");
    XCTAssertNil(error, "Should be no error loading key from keychain");
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

    // The CKKS subsystem should figure out the issue, and fix it (while uploading itself a TLK Share)
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 3 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromIncorrectCurrentTLKPointer {
    // The current key pointers in cloudkit shouldn't ever point to wrong entries. But, if they do, CKKS must recover.

    // Test starts with a rolled hierarchy, and CKPs pointing to the wrong items
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKKSCurrentKeyPointer* oldTLKCKP = self.keychainZoneKeys.currentTLKPointer;
    CKRecord* oldTLKPointer = [self.keychainZone.currentDatabase[self.keychainZoneKeys.currentTLKPointer.storedCKRecord.recordID] copy];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* newZoneKeys = [self.keychainZoneKeys copy];

    // And put the oldTLKPointer back
    [self.zones[self.keychainZoneID] addToZone:oldTLKPointer];
    self.keychainZoneKeys.currentTLKPointer = oldTLKCKP;

    // Make sure it stuck:
    XCTAssertNotEqualObjects(self.keychainZoneKeys.currentTLKPointer,
                             newZoneKeys.currentTLKPointer,
                             "current TLK pointer should now not point to proper TLK");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it (while uploading itself a TLK Share)
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    XCTAssertEqualObjects(self.keychainZoneKeys.currentTLKPointer,
                          newZoneKeys.currentTLKPointer,
                          "current TLK pointer should now point to proper TLK");
    XCTAssertEqualObjects(self.keychainZoneKeys.currentClassAPointer,
                          newZoneKeys.currentClassAPointer,
                          "current Class A pointer should now point to proper Class A key");
    XCTAssertEqualObjects(self.keychainZoneKeys.currentClassCPointer,
                          newZoneKeys.currentClassCPointer,
                          "current Class C pointer should now point to proper Class C key");
}

- (void)testRecoverFromDesyncedKeyRecordsViaResync {
    // We need to set up a desynced situation to test our resync.
    // First, let CKKS start up and send several items to CloudKit (that we'll then desync!)
    __block NSError* error = nil;

    // Test starts with keys in CloudKit (so we can create items later)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Now, delete most of the key records are from on-disk, but the change token is not changed
    [self.keychainView dispatchSync:^bool{
        CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:self.keychainZoneID];

        XCTAssertNotNil(keyset.currentTLKPointer, @"should be a TLK pointer");
        XCTAssertNotNil(keyset.currentClassAPointer, @"should be a class A pointer");
        XCTAssertNotNil(keyset.currentClassCPointer, @"should be a class C pointer");

        [keyset.currentTLKPointer deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting TLK pointer from database");
        [keyset.currentClassAPointer deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting class A pointer from database");

        XCTAssertNotNil(keyset.tlk, @"should be a TLK");
        XCTAssertNotNil(keyset.classA, @"should be a classA key");
        XCTAssertNotNil(keyset.classC, @"should be a classC key");

        [keyset.tlk deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting TLK from database");

        [keyset.classA deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting classA from database");

        [keyset.classC deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting classC from database");

        return true;
    }];

    // A restart should realize there's an issue, and pause for help
    // Ideally we'd try a refetch here to see if we're very wrong, but that's hard to avoid making into an infinite loop
    [self.keychainView halt];
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self.keychainView beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation'");

    // But, a resync should fix you back up
    CKKSSynchronizeOperation* resyncOperation = [self.keychainView resyncWithCloud];
    [resyncOperation waitUntilFinished];
    XCTAssertNil(resyncOperation.error, "No error during the resync operation");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
}

- (void)testRecoverFromCloudKitFetchFail {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // The first two CKRecordZoneChanges should fail with a 'network unavailable' error.
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}]];
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}]];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitFetchNetworkFailAfterReady {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateReady, "CKKS entered ready");

    // Network is unavailable
    [self.reachabilityTracker setNetworkReachability:false];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    // Say network is available
    [self.reachabilityTracker setNetworkReachability:true];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];
}

- (void)testRecoverFromCloudKitFetchNetworkFailBeforeReady {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    // Network is unavailable
    [self.reachabilityTracker setNetworkReachability:false];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateInitializing] wait:20*NSEC_PER_SEC], "CKKS entered initializing");
    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateInitializing, "CKKS entered initializing");

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    // Say network is available
    [self.reachabilityTracker setNetworkReachability:true];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];
}

- (void)testWaitAfterCloudKitNetworkFailDuringOutgoingQueueOperation {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], "CKKS entered ready");

    // Network is now unavailable
    [self.reachabilityTracker setNetworkReachability:false];

    NSError* noNetwork = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{
                                                                                                                       CKErrorRetryAfterKey: @(0.2),
                                                                                                                       }];
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:nil withError:noNetwork];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    sleep(2);

    // Once network is available again, the write should happen
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.reachabilityTracker setNetworkReachability:true];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitFetchFailWithDelay {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

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
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testHandleZoneDeletedWhileFetching {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // The first CKRecordZoneChanges should fail with a 'zone not found' error (race between zone creation as part of initalization and zone deletion from another device)
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorZoneNotFound userInfo:@{}]];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"Key state should become 'ready'");
}

- (void)testRecoverFromCloudKitOldChangeToken {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Delete all old database states, to destroy the change tag validity
    [self.keychainZone.pastDatabases removeAllObjects];

    // We expect a total local flush and refetch
    self.silentFetchesAllowed = false;
    [self expectCKFetch]; // one to fail with a CKErrorChangeTokenExpired error
    [self expectCKFetch]; // and one to succeed

    // Trigger a fake change notification
    [self.keychainView notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And check that a new upload happens just fine.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitUnknownDeviceStateRecord {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Save a new device state record with some fake etag
    [self.keychainView dispatchSync: ^bool {
        CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:self.ckDeviceID
                                                                       osVersion:@"fake-record"
                                                                  lastUnlockTime:[NSDate date]
                                                                   octagonPeerID:nil
                                                                   octagonStatus:nil
                                                                    circlePeerID:self.mockSOSAdapter.selfPeer.peerID
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // The first CKRecordZoneChanges should fail with a 'CKErrorUserDeletedZone' error. This will cause a local reset, ending up with zone re-creation.
    self.zones[self.keychainZoneID] = nil; // delete the zone
    self.keys[self.keychainZoneID] = nil;
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorUserDeletedZone userInfo:@{}]];

    // We expect CKKS to recreate the zone, then have octagon reupload the keys, and then the class C item upload
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.keychainView notifyZoneChange:nil];

    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And check that a new upload occurs.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitZoneNotFoundWithoutZoneDeletion {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter 'ready'");

    [self waitForCKModifications];
    [self.keychainView waitForOperationsOfClass:[CKKSScanLocalItemsOperation class]];

    // The next CKRecordZoneChanges will fail with a 'zone not found' error.
    self.zones[self.keychainZoneID] = nil; // delete the zone
    self.keys[self.keychainZoneID] = nil;

    // We expect CKKS to reset itself and recover, then have octagon upload the keys, and then the class C item upload
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.keychainView notifyZoneChange:nil];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // And check that a new upload occurs.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitZoneNotFoundFetchBeforeSigninOccurs {
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone

    // Before CKKS sign-in, it receives a fetch rpc
    XCTestExpectation *fetchReturns = [self expectationWithDescription:@"fetch returned"];
    [self.injectedManager rpcFetchAndProcessChanges:nil reply:^(NSError *result) {
        XCTAssertNil(result, "Should be no error fetching and processing changes");
        [fetchReturns fulfill];
    }];

    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter 'ready'");

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // The fetch should have come back by now
    [self waitForExpectations: @[fetchReturns] timeout:5];
}

- (void)testNoCloudKitAccount {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    // simulate a NSNotification callback (but still logged out)
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Test that there are no items in the database (since we never logged in)
    [self checkNoCKKSData: self.keychainView];
}

- (void)testSACloudKitAccount {
    // Test starts with nothing in database and the user logged into CloudKit and in circle, but the account is not HSA2.
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;

    self.accountStatus = CKAccountStatusAvailable;

    self.silentFetchesAllowed = false;

    // Octagon does not initialize the ckks views when not in an HSA2 account
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    [self startCKKSSubsystem];

    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS should enter 'loggedout'");

    // There should be no uploads, even when we save keychain items and enter/exit circle
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Test that there are no items in the database (since we never were in an HSA2 account)
    [self checkNoCKKSData: self.keychainView];
}

- (void)testEarlyLogin
{
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    // Octagon should initialize these views
    self.automaticallyBeginCKKSViewCloudKitOperation = true;

    self.accountStatus = CKAccountStatusAvailable;
    //[self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    [self startCKKSSubsystem];

    // CKKS should end up in 'waitfortlkcreation', as there's no trust and no TLKs
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");

    // Now, renotify the account status, and ensure that CKKS doesn't reenter 'initializing'
    CKKSCondition* initializing = self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateInitializing];

    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    XCTAssertNotEqual(0, [initializing wait:500*NSEC_PER_MSEC], "CKKS should not enter initializing when the device HSA status changes");
}

- (void)testNoCircle {
    // Test starts with nothing in database and the user logged into CloudKit, but out of Circle.
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    self.accountStatus = CKAccountStatusAvailable;

    [self startCKKSSubsystem];

    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");

    // simulate a NSNotification callback (but still logged out)
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'loggedout'");

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Test that there are no items in the database (since we never logged in)
    [self checkNoCKKSData: self.keychainView];
}

- (void)testCircleDepartAndRejoin {
    // Test starts with CKKS in ready
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    // But then, trust departs
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortrust'");

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    // Then trust returns. We expect two uploads
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCloudKitLogin {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    // Before we inform CKKS of its account state....
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");

    [self startCKKSSubsystem];

    XCTAssertEqual(0,    [self.keychainView.loggedOut wait:500*NSEC_PER_MSEC], "Should have been told of a 'logout' event on startup");
    XCTAssertNotEqual(0, [self.keychainView.loggedIn wait:100*NSEC_PER_MSEC], "'login' event shouldn't have happened");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // simulate a cloudkit login and NSNotification callback
    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // No writes yet, since we're not in circle
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedOperationForAllViews];

    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
}

- (void)testCloudKitLogoutLogin {
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];
    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

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
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.keychainView.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.keychainView.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    [self checkNoCKKSData: self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");

    // simulate a cloudkit login
    // We should expect CKKS to re-find the key hierarchy it already uploaded, and then send up the two records we added during the pause
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Let everything settle...
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    [self waitForCKModifications];

    // Logout again
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.keychainView.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.keychainView.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    [self checkNoCKKSData: self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-5"];
    [self addGenericPassword: @"data" account: @"account-delete-me-6"];

    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // simulate a cloudkit login
    // We should expect CKKS to re-find the key hierarchy it already uploaded, and then send up the two records we added during the pause
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Let everything settle...
    [self.keychainView waitUntilAllOperationsAreFinished];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");

    // Logout again
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.keychainView.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.keychainView.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");
    [self checkNoCKKSData: self.keychainView];

    // Force zone into error state
    self.keychainView.keyHierarchyState = SecCKKSZoneKeyStateError;

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTestExpectation *operationRun = [self expectationWithDescription:@"operation run"];
    NSOperation* op = [NSBlockOperation named:@"test" withBlock:^{
        [operationRun fulfill];
    }];

    [op addDependency:self.keychainView.keyStateReadyDependency];
    [self.operationQueue addOperation:op];

    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations: @[operationRun] timeout:10];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
}

- (void)testCloudKitLogoutDueToGreyMode {
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];
    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:20*NSEC_PER_SEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:50*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // simulate a cloudkit grey mode switch and NSNotification callback. CKKS should treat this as a logout
    self.iCloudHasValidCredentials = false;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.keychainView.loggedOut wait:20*NSEC_PER_SEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.keychainView.loggedIn wait:50*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    [self checkNoCKKSData: self.keychainView];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Also, fetches shouldn't occur
    self.silentFetchesAllowed = false;
    NSOperation* op = [self.keychainView.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting];
    CKKSResultOperation* timeoutOp = [CKKSResultOperation named:@"timeout" withBlock:^{}];
    [timeoutOp addDependency:op];
    [timeoutOp timeout:4*NSEC_PER_SEC];
    [self.operationQueue addOperation:timeoutOp];
    [timeoutOp waitUntilFinished];

    // CloudKit figures its life out. We expect the two passwords from before to be uploaded
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    self.silentFetchesAllowed = true;
    self.iCloudHasValidCredentials = true;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:20*NSEC_PER_SEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:50*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And fetching still works!
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
}

- (void)testCloudKitLoginRace {
    // Test starts with nothing in database, and 'in circle', but securityd hasn't received notification if we're logged into CloudKit.
    // CKKS should call handleLogout, as the CK account is not present.

    // note: don't unblock the ck account state object yet...

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // Add a keychain item, and make sure it doesn't upload yet.
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'loggedout'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now that we're here (and logged out), bring the account up

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    // simulate another NSNotification callback
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Make sure new items upload too
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
    [self.keychainView halt];
}

- (void)testDontLogOutIfBeforeFirstUnlock {
    /*
    // test starts as if a previously logged-in device has just rebooted
    self.aksLockState = true;
    self.accountStatus = CKAccountStatusAvailable;

    // This is the original state of the account tracker
    self.circleStatus = [[SOSAccountStatus alloc] init:kSOSCCError error:nil];
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // And this is what the first circle status fetch will actually return
    self.circleStatus = [[SOSAccountStatus alloc] init:kSOSCCError error:[NSError errorWithDomain:(__bridge id)kSOSErrorDomain code:kSOSErrorNotReady description:@"fake error: device is locked, so SOS doesn't know if it's in-circle"]];
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    XCTAssertEqual(self.accountStateTracker.currentComputedAccountStatus, CKKSAccountStatusUnknown, "Account tracker status should just be 'unknown'");
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKKS should not yet know the CK account state");

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.loggedIn wait:8*NSEC_PER_SEC], "'login' event should have happened");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:10*NSEC_PER_MSEC], "Should not have been told of a CK 'logout' event on startup");
    XCTAssertEqual(0, [self.keychainView.accountStateKnown wait:1*NSEC_PER_SEC], "CKKS should know the account state");

    // And assume another CK status change
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    XCTAssertEqual(self.accountStateTracker.currentComputedAccountStatus, CKKSAccountStatusUnknown, "Account tracker status should just be 'no account'");
    XCTAssertEqual(0, [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKKS should know the CK account state");

    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    self.aksLockState = false;

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0,    [self.keychainView.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.keychainView.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.keychainView.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];*/
}

- (void)testSyncableItemsAddedWhileLoggedOut {
    // Test that once CKKS is up and 'logged out', nothing happens when syncable items are added
    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];

    XCTAssertEqual([self.keychainView.loggedOut wait:500*NSEC_PER_MSEC], 0, "CKKS should be told that it's logged out");

    // CKKS shouldn't decide to poke its state machine, but it should still send the notification
    XCTestExpectation* viewChangeNotification = [self expectChangeForView:self.keychainZoneID.zoneName];

    // Reject all attempts to trigger a state machine update
    id pokeKeyStateMachineScheduler = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMReject([pokeKeyStateMachineScheduler trigger]);
    self.keychainView.pokeKeyStateMachineScheduler = pokeKeyStateMachineScheduler;

    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    [self waitForExpectations:@[viewChangeNotification] timeout:8];
    [pokeKeyStateMachineScheduler stopMocking];
}

- (void)testUploadSyncableItemsAddedWhileUntrusted {
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual([self.keychainView.loggedIn wait:500*NSEC_PER_MSEC], 0, "CKKS should be told that it's logged in");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    sleep(2);

    NSError* error = nil;
    NSDictionary* currentOQEs = [CKKSOutgoingQueueEntry countsByStateInZone:self.keychainZoneID error:&error];
    XCTAssertNil(error, "Should be no error coutning OQEs");
    XCTAssertEqual(0, currentOQEs.count, "Should be no OQEs");

    // Now, insert a restart to simulate securityd restarting (and throwing away all pending operations), then a real sign in
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self endSOSTrustedViewOperation:self.keychainView];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");

    // Okay! Upon sign in, this item should be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

// Note that this test assumes that the keychainView object was created at daemon restart.
// I don't really know how to write a test for that...
- (void)testSyncableItemAddedOnDaemonRestartBeforeCloudKitAccountKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];

    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:100*NSEC_PER_MSEC], "CKKS should still have no idea what the account state is");
    XCTAssertEqual(self.keychainView.accountStatus, CKKSAccountStatusUnknown, "Account status should be unknown");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");

    [self.keychainView beginCloudKitOperation];

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testSyncableItemModifiedOnDaemonRestartBeforeCloudKitAccountKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];

    [self updateGenericPassword:@"newdata" account: @"account-delete-me-2"];
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:100*NSEC_PER_MSEC], "CKKS should still have no idea what the account state is");
    XCTAssertEqual(self.keychainView.accountStatus, CKKSAccountStatusUnknown, "Account status should be unknown");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");

    [self.keychainView beginCloudKitOperation];

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testSyncableItemDeletedOnDaemonRestartBeforeCloudKitAccountKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];

    [self deleteGenericPassword:@"account-delete-me-2"];
    XCTAssertNotEqual(0, [self.keychainView.accountStateKnown wait:100*NSEC_PER_MSEC], "CKKS should still have no idea what the account state is");
    XCTAssertEqual(self.keychainView.accountStatus, CKKSAccountStatusUnknown, "Account status should be unknown");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");

    [self.keychainView beginCloudKitOperation];

    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testNotStuckAfterReset {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    XCTestExpectation *operationRun = [self expectationWithDescription:@"operation run"];
    NSOperation* op = [NSBlockOperation named:@"test" withBlock:^{
        [operationRun fulfill];
    }];

    [op addDependency:self.keychainView.keyStateReadyDependency];
    [self.operationQueue addOperation:op];

    // And handle a spurious logout
    [self.keychainView handleCKLogout];

    [self startCKKSSubsystem];

    [self waitForExpectations: @[operationRun] timeout:20];
}

- (void)testCKKSControlBringup {
    NSXPCInterface *interface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    XCTAssertNotNil(interface, "Received a configured CKKS interface");
}

- (void)testMetricsUpload {

    XCTestExpectation *upload = [self expectationWithDescription:@"CAMetrics"];
    XCTestExpectation *collection = [self expectationWithDescription:@"CAMetrics"];

    id saMock = OCMClassMock([SecCoreAnalytics class]);
    OCMStub([saMock sendEvent:[OCMArg any] event:[OCMArg any]]).andDo(^(NSInvocation* invocation) {
        [upload fulfill];
    });

    NSString *sampleSampler = @"stuff";

    [[CKKSAnalytics logger] AddMultiSamplerForName:sampleSampler withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
        [collection fulfill];
        return @{ @"hej" : @1 };
    }];


    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];

    [self.injectedManager xpc24HrNotification];

    [self waitForExpectations: @[upload, collection] timeout:10];
    [[CKKSAnalytics logger] removeMultiSamplerForName:sampleSampler];
}

- (void)testSaveManyTLKShares {
    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");

    NSMutableArray<CKKSSOSSelfPeer*>* peers = [NSMutableArray array];

    for(int i = 0; i < 20; i++) {
        CKKSSOSSelfPeer* untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:[NSString stringWithFormat:@"untrusted-peer-%d", i]
                                                                      encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                         signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                           viewList:self.managedViewList];

        [peers addObject:untrustedPeer];
    }

    NSMutableArray<CKRecord*>* tlkShareRecords = [NSMutableArray array];

    for(CKKSSOSSelfPeer* peer1 in peers) {
        for(CKKSSOSSelfPeer* peer2 in peers) {
            NSError* error = nil;
            CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.keychainZoneKeys.tlk
                                                               as:peer1
                                                               to:peer2
                                                            epoch:-1
                                                         poisoned:0
                                                            error:&error];
            XCTAssertNil(error, "Should have been no error sharing a CKKSKey");
            XCTAssertNotNil(share, "Should be able to create a share");

            CKRecord* shareRecord = [share CKRecordWithZoneID:self.keychainZoneID];
            [tlkShareRecords addObject:shareRecord];
        }
    }

    [self measureBlock:^{
        [self.keychainView dispatchSyncWithAccountKeys:^bool{
            for(CKRecord* record in tlkShareRecords) {
                [self.keychainView _onqueueCKRecordChanged:record resync:false];
            }
            return true;
        }];
    }];
}

@end

#endif // OCTAGON
