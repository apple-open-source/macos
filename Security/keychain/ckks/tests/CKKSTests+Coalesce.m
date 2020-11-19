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

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"

// Break abstraction.
@interface CKKSKeychainView(test)
@property NSOperationQueue* operationQueue;
@end

@implementation CloudKitKeychainSyncingTests (CoalesceTests)
// These tests check that, if CKKS doesn't start processing an item before a new update comes in,
// each case is properly handled.

- (void)testCoalesceAddModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];
    [self updateGenericPassword: @"otherdata" account:account];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCoalesceAddModifyModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];
    [self updateGenericPassword: @"otherdata" account:account];
    [self updateGenericPassword: @"again" account:account];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCoalesceAddModifyDeleteItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];
    [self updateGenericPassword: @"otherdata" account:account];
    [self deleteGenericPassword: account];

    // We expect no uploads.
    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCoalesceDeleteAddItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Okay, now the delete/add. Note that this is not a coalescing operation, since the new item
    // has different contents. (This test used to upload the item to a different UUID, but no longer).

    self.keychainView.holdOutgoingQueueOperation = [CKKSResultOperation named:@"hold-outgoing-queue"
                                                                    withBlock:^{}];

    [self deleteGenericPassword: account];
    [self addGenericPassword: @"data_new_contents" account: account];

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:account password:@"data_new_contents"]];

    [self.operationQueue addOperation:self.keychainView.holdOutgoingQueueOperation];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCoalesceReceiveModifyWhileDeletingItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    NSString* account = @"account-delete-me";

    [self addGenericPassword:@"data" account:account];

    // We expect a single record to be uploaded.
    __block CKRecord* itemRecord = nil;
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:^BOOL(CKRecord * _Nonnull record) {
        itemRecord = record;
        return YES;
    }];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Now, we receive a modification from CK, but then delete the item locally before processing the IQE.

    XCTAssertNotNil(itemRecord, "Should have a record for the uploaded item");
    NSMutableDictionary* contents = [[self decryptRecord:itemRecord] mutableCopy];
    contents[@"v_Data"] = [@"updated" dataUsingEncoding:NSUTF8StringEncoding];

    CKRecord* recordUpdate = [self newRecord:itemRecord.recordID withNewItemData:contents];
    [self.keychainZone addCKRecordToZone:recordUpdate];

    self.keychainView.holdIncomingQueueOperation = [NSBlockOperation blockOperationWithBlock:^{}];

    // Ensure we wait for the whole fetch
    NSOperation* fetchOp = [self.keychainView.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    [fetchOp waitUntilFinished];

    // now, delete the item
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:account];

    [self.operationQueue addOperation:self.keychainView.holdIncomingQueueOperation];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the item shouldn't be present, since it was deleted via API after the item was fetched
    [self findGenericPassword:account expecting:errSecItemNotFound];
}

- (void)testCoalesceReceiveDeleteWhileModifyingItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    NSString* account = @"account-delete-me";

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");

    __block CKRecord* itemRecord = nil;
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:^BOOL(CKRecord * _Nonnull record) {
        itemRecord = record;
        return YES;
    }];

    [self addGenericPassword:@"data" account:account];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Ensure we fetch again, to prime the delete (due to insufficient mock CK)
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];

    // Now, we receive a delete from CK, but after we modify the item locally
    self.keychainView.holdOutgoingQueueOperation = [NSBlockOperation blockOperationWithBlock:^{}];

    XCTAssertNotNil(itemRecord, "Should have an item record from the upload");
    [self.keychainZone deleteCKRecordIDFromZone:itemRecord.recordID];
    [self updateGenericPassword:@"new-password" account:account];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:account expecting:errSecItemNotFound];

    [self.operationQueue addOperation:self.keychainView.holdOutgoingQueueOperation];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the item shouldn't be present, since it was deleted via CK after the API change
    [self findGenericPassword:account expecting:errSecItemNotFound];
}

@end

#endif
