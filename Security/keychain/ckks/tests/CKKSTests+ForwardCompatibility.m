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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import <TrustedPeers/TrustedPeers.h>
#import <TrustedPeers/TPPBPolicyKeyViewMapping.h>
#import <TrustedPeers/TPDictionaryMatchingRules.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/MockCloudKit.h"

@interface CloudKitKeychainForwardCompatibilityTests : CloudKitKeychainSyncingTestsBase
@property CKRecordZoneID* unknownZoneID;
@property CKRecordZoneID* passwordsZoneID;
@property CKKSKeychainView* passwordsView;

@property TPSyncingPolicy* originalPolicy;
@property TPSyncingPolicy* originalPolicyPlusUnknownVwht;
@property TPSyncingPolicy* allItemsToPasswordsPolicy;
@end

@implementation CloudKitKeychainForwardCompatibilityTests

- (void)setUp {
    [super setUp];

    self.passwordsZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Passwords" ownerName:CKCurrentUserDefaultName];
    self.unknownZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"unknown-zone" ownerName:CKCurrentUserDefaultName];

    self.zones[self.passwordsZoneID] = [[FakeCKZone alloc] initZone:self.passwordsZoneID];

    self.originalPolicy = self.viewSortingPolicyForManagedViewList;


    NSMutableArray<TPPBPolicyKeyViewMapping*>* newRules = [self.originalPolicy.keyViewMapping mutableCopy];
    TPPBPolicyKeyViewMapping* unknownVwhtMapping = [[TPPBPolicyKeyViewMapping alloc] init];
    unknownVwhtMapping.view = self.keychainZoneID.zoneName;
    unknownVwhtMapping.matchingRule = [TPDictionaryMatchingRule fieldMatch:@"vwht"
                                                                fieldRegex:[NSString stringWithFormat:@"^%@$", self.unknownZoneID.zoneName]];
    [newRules insertObject:unknownVwhtMapping atIndex:0];

    self.originalPolicyPlusUnknownVwht = [[TPSyncingPolicy alloc] initWithModel:@"test-policy"
                                                                        version:[[TPPolicyVersion alloc] initWithVersion:2 hash:@"fake-policy-for-views-with-unknown-view"]
                                                                       viewList:self.originalPolicy.viewList
                                                                  priorityViews:self.originalPolicy.priorityViews
                                                          userControllableViews:self.originalPolicy.userControllableViews
                                                      syncUserControllableViews:self.originalPolicy.syncUserControllableViews
                                                           viewsToPiggybackTLKs:self.originalPolicy.viewsToPiggybackTLKs
                                                                 keyViewMapping:newRules];

    TPPBPolicyKeyViewMapping* passwordsVwhtMapping = [[TPPBPolicyKeyViewMapping alloc] init];
    passwordsVwhtMapping.view = self.passwordsZoneID.zoneName;
    passwordsVwhtMapping.matchingRule = [TPDictionaryMatchingRule trueMatch];

    self.allItemsToPasswordsPolicy = [[TPSyncingPolicy alloc] initWithModel:@"test-policy"
                                                                    version:[[TPPolicyVersion alloc] initWithVersion:2 hash:@"fake-policy-for-views-with-passwords-view"]
                                                                   viewList:[NSSet setWithArray:@[self.keychainView.zoneName, self.passwordsZoneID.zoneName]]
                                                              priorityViews:[NSSet set]
                                                      userControllableViews:self.originalPolicy.userControllableViews
                                                  syncUserControllableViews:self.originalPolicy.syncUserControllableViews
                                                       viewsToPiggybackTLKs:self.originalPolicy.viewsToPiggybackTLKs
                                                             keyViewMapping:@[passwordsVwhtMapping]];
}

- (void)setPolicyAndWaitForQuiescence:(TPSyncingPolicy*)policy policyIsFresh:(BOOL)policyIsFresh {
    [self.injectedManager setCurrentSyncingPolicy:policy policyIsFresh:policyIsFresh];
    self.ckksViews = [NSMutableSet setWithArray:[[self.injectedManager views] allValues]];
    [self beginSOSTrustedOperationForAllViews];

    // And wait for everything to enter a resting state
    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
        [view waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
        [view waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    }
}

- (void)testReceiveItemForWrongView {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    NSString* wrongZoneAccount = @"wrong-zone";
    NSDictionary* item = [self fakeRecordDictionary:wrongZoneAccount zoneID:self.unknownZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:wrongZoneAccount expecting:errSecItemNotFound];

    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);

    NSError* zoneError = nil;
    NSInteger count = [CKKSIncomingQueueEntry countByState:SecCKKSStateMismatchedView zone:self.keychainView.zoneID error:&zoneError];
    XCTAssertNil(zoneError, "should be no error counting all IQEs");
    XCTAssertEqual(count, 1, "Should be one mismatched IQE");
}

- (void)testConflictingItemInWrongView {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];
    self.passwordsView = [self.injectedManager findView:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    NSDictionary* item = [self fakeRecordDictionary:@"account-delete-me" zoneID:self.passwordsZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.passwordsZoneID];
    CKRecordID* ckr2id = [[CKRecordID alloc] initWithRecordName:@"FFFF8D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];

    NSMutableDictionary* item2 = [item mutableCopy];
    item2[@"v_Data"] = @"wrongview";
    CKRecord* ckr2 = [self newRecord:ckr2id withNewItemData:item2];

    // Receive the passwords item first
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.passwordsView waitForFetchAndIncomingQueueProcessing];
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.zones[self.keychainZoneID] addToZone:ckr2];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // THe view should ask for an update, and receive one
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    // And we have ignored the change in the other view
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the item is then deleted
    [self.zones[self.keychainZoneID] deleteCKRecordIDFromZone:ckr2id];
    OCMExpect([self.requestPolicyCheck trigger]);
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    // The password should still exist
    [self checkGenericPassword:@"data" account:@"account-delete-me"];
}

- (void)testConflictingItemInWrongViewWithLowerUUID {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];
    self.passwordsView = [self.injectedManager findView:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    NSDictionary* item = [self fakeRecordDictionary:@"account-delete-me" zoneID:self.passwordsZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.passwordsZoneID];
    CKRecordID* ckr2id = [[CKRecordID alloc] initWithRecordName:@"00008D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];

    NSMutableDictionary* item2 = [item mutableCopy];
    item2[@"v_Data"] = @"wrongview";
    CKRecord* ckr2 = [self newRecord:ckr2id withNewItemData:item2];

    // Receive the passwords item first
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.passwordsView waitForFetchAndIncomingQueueProcessing];
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.zones[self.keychainZoneID] addToZone:ckr2];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // THe view should ask for an update, and receive one
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    // And the item is then deleted
    [self.zones[self.keychainZoneID] deleteCKRecordIDFromZone:ckr2id];
    OCMExpect([self.requestPolicyCheck trigger]);
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    // The password should still exist
    [self checkGenericPassword:@"data" account:@"account-delete-me"];
}

- (void)testConflictingItemInWrongViewWithSameUUID {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];
    self.passwordsView = [self.injectedManager findView:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    NSDictionary* item = [self fakeRecordDictionary:@"account-delete-me" zoneID:self.passwordsZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.passwordsZoneID];
    CKRecordID* ckr2id = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];

    NSMutableDictionary* item2 = [item mutableCopy];
    item2[@"v_Data"] = @"wrongview";
    CKRecord* ckr2 = [self newRecord:ckr2id withNewItemData:item2];

    // Receive the passwords item first
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.passwordsView waitForFetchAndIncomingQueueProcessing];
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.zones[self.keychainZoneID] addToZone:ckr2];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // THe view should ask for a policy update, and receive one
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    // And the item is then deleted
    [self.zones[self.keychainZoneID] deleteCKRecordIDFromZone:ckr2id];
    OCMExpect([self.requestPolicyCheck trigger]);
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    // The password should still exist
    [self checkGenericPassword:@"data" account:@"account-delete-me"];
}

- (void)testReceiveItemForFuturePolicy {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    NSString* wrongZoneAccount = @"wrong-zone";
    NSDictionary* item = [self fakeRecordDictionary:wrongZoneAccount zoneID:self.unknownZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:wrongZoneAccount expecting:errSecItemNotFound];

    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);

    // Now, Octagon discovers that there's a new policy that allows this item in the keychain view
    TPSyncingPolicy* currentPolicy = self.injectedManager.policy;
    XCTAssertNotNil(currentPolicy, "should have a current policy");

    [self setPolicyAndWaitForQuiescence:self.originalPolicyPlusUnknownVwht policyIsFresh:YES];

    [self findGenericPassword:wrongZoneAccount expecting:errSecSuccess];
}

- (void)testHandleItemMovedBetweenViewsBeforePolicyChange {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    // A password is created and uploaded out of the keychain view.
    __block CKRecord* itemCKRecord = nil;
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        itemCKRecord = record;
        return YES;
    }];
    NSString* itemAccount = @"account-delete-me";
    [self addGenericPassword:@"data" account:itemAccount viewHint:self.keychainZoneID.zoneName];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertNotNil(itemCKRecord, "Should have some CKRecord for the added item");

    // Update etag as well
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Another device shows up, changes the item sync policy, and moves the item over into the passwords view.

    NSDictionary* itemContents = [self decryptRecord:itemCKRecord];
    XCTAssertNotNil(itemContents, "should have some item contents");

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:itemCKRecord.recordID.recordName zoneID:self.passwordsZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:itemContents];
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    TPSyncingPolicy* currentPolicy = self.injectedManager.policy;
    XCTAssertNotNil(currentPolicy, "should have a current policy");

    // In this test, we receive the deletion, and then the policy change adding the Passwords view
    [self.keychainZone deleteCKRecordIDFromZone:itemCKRecord.recordID];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:itemAccount expecting:errSecItemNotFound];

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];

    // And once passwords syncs, the item should appear again
    [self findGenericPassword:itemAccount expecting:errSecSuccess];
}

- (void)testHandleItemMovedBetweenViewsAfterPolicyChange {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    // A password is created and uploaded out of the keychain view.
    __block CKRecord* itemCKRecord = nil;
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        itemCKRecord = record;
        return YES;
    }];
    NSString* itemAccount = @"account-delete-me";
    [self addGenericPassword:@"data" account:itemAccount viewHint:self.keychainZoneID.zoneName];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertNotNil(itemCKRecord, "Should have some CKRecord for the added item");

    // Update etag as well
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Another device shows up, changes the item sync policy, and moves the item over into the Passwords view.
    // But, in this case, we receive the item delete in the Keychain view after we've synced the item in the Passwords view

    NSDictionary* itemContents = [self decryptRecord:itemCKRecord];
    XCTAssertNotNil(itemContents, "should have some item contents");

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:itemCKRecord.recordID.recordName zoneID:self.passwordsZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:itemContents];
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    TPSyncingPolicy* currentPolicy = self.injectedManager.policy;
    XCTAssertNotNil(currentPolicy, "should have a current policy");

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];

    CKKSKeychainView* passwordsView = [self.injectedManager findView:self.passwordsZoneID.zoneName];
    XCTAssertNotNil(passwordsView, @"Should have a passwords view");

    // And once Passwords syncs, the item should appear again
    [self findGenericPassword:itemAccount expecting:errSecSuccess];

    // And now we receive the delete in the keychain view. The item should still exist!
    [self.keychainZone deleteCKRecordIDFromZone:itemCKRecord.recordID];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:itemAccount expecting:errSecSuccess];

    // Keychain View should have asked for a policy set
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self.injectedManager setCurrentSyncingPolicy:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:itemAccount expecting:errSecSuccess];

    NSError* zoneError = nil;
    NSInteger count = [CKKSIncomingQueueEntry countByState:SecCKKSStateMismatchedView zone:self.keychainView.zoneID error:&zoneError];
    XCTAssertNil(zoneError, "should be no error counting all IQEs");
    XCTAssertEqual(count, 0, "Should be no remaining mismatched IQEs");
}

- (void)testMoveItemUploadedToOldZone {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];

    // Now, someone uploads an item to the keychain view. We should move it to the passwords view
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // The keychain view should request a policy refetch
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);

    // Note that ideally, we'd remove the old item from CloudKit. But, other devices which participate in CKKS4All
    // might not have this forward-compatiblity change, and will treat this as a deletion. If they process this deletion,
    // then sync the resulting tombstone to a third SOS device, then receive the addition in the 'right' view, and then the
    // tombstone syncs back to the CKKS4All devices, then we might end up deleting the item across the account.
    // Until enough internal folk have moved onto builds with this forward-compat change, we can't issue these deletes.
    //[self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    // The item should be reuploaded to Passwords, though.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.passwordsZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        return [record.recordID.recordName isEqualToString:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    }];

    [self.injectedManager setCurrentSyncingPolicy:self.allItemsToPasswordsPolicy policyIsFresh:YES];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the keychain item should still exist
    [self findGenericPassword:@"account0" expecting:errSecSuccess];
}

@end

#endif // OCTAGON
