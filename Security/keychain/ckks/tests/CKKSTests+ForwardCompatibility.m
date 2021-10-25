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
@property CKKSKeychainViewState* passwordsView;

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
                                                                 keyViewMapping:newRules
                                                             isInheritedAccount:self.originalPolicy.isInheritedAccount];

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
                                                             keyViewMapping:@[passwordsVwhtMapping]
                                                         isInheritedAccount:self.originalPolicy.isInheritedAccount];
}

- (void)setPolicyAndWaitForQuiescence:(TPSyncingPolicy*)policy policyIsFresh:(BOOL)policyIsFresh {
    [self.defaultCKKS setCurrentSyncingPolicy:policy policyIsFresh:policyIsFresh];
    self.ckksViews = [self.defaultCKKS.operationDependencies.allCKKSManagedViews mutableCopy];
    [self beginSOSTrustedOperationForAllViews];

    // And wait for everything to enter a resting state
    for(CKKSKeychainViewState* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    }
    XCTAssertEqual(0, [self.defaultCKKS.stateMachine.paused wait:20*NSEC_PER_SEC], "CKKS state machine should quiesce");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
}

- (void)testReceiveItemForWrongView {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.requestPolicyCheck trigger]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSString* wrongZoneAccount = @"wrong-zone";
    NSDictionary* item = [self fakeRecordDictionary:wrongZoneAccount zoneID:self.unknownZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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
    self.passwordsView = [self.defaultCKKS.operationDependencies viewStateForName:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

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
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.zones[self.keychainZoneID] addToZone:ckr2];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
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
    self.passwordsView = [self.defaultCKKS.operationDependencies viewStateForName:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

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
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.zones[self.keychainZoneID] addToZone:ckr2];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // THe view should ask for an update, and receive one
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    // And the item is then deleted
    [self.zones[self.keychainZoneID] deleteCKRecordIDFromZone:ckr2id];
    OCMExpect([self.requestPolicyCheck trigger]);
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
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
    self.passwordsView = [self.defaultCKKS.operationDependencies viewStateForName:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

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
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    [self.zones[self.keychainZoneID] addToZone:ckr2];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // THe view should ask for a policy update, and receive one
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    // And the item is then deleted
    [self.zones[self.keychainZoneID] deleteCKRecordIDFromZone:ckr2id];
    OCMExpect([self.requestPolicyCheck trigger]);
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
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
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSString* wrongZoneAccount = @"wrong-zone";
    NSDictionary* item = [self fakeRecordDictionary:wrongZoneAccount zoneID:self.unknownZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:wrongZoneAccount expecting:errSecItemNotFound];

    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);

    // Now, Octagon discovers that there's a new policy that allows this item in the keychain view
    TPSyncingPolicy* currentPolicy = self.defaultCKKS.syncingPolicy;
    XCTAssertNotNil(currentPolicy, "should have a current policy");

    [self setPolicyAndWaitForQuiescence:self.originalPolicyPlusUnknownVwht policyIsFresh:YES];

    [self findGenericPassword:wrongZoneAccount expecting:errSecSuccess];
}

- (void)testHandleItemMovedBetweenViewsBeforePolicyChange {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

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
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Another device shows up, changes the item sync policy, and moves the item over into the passwords view.

    NSDictionary* itemContents = [self decryptRecord:itemCKRecord];
    XCTAssertNotNil(itemContents, "should have some item contents");

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:itemCKRecord.recordID.recordName zoneID:self.passwordsZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:itemContents];
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    TPSyncingPolicy* currentPolicy = self.defaultCKKS.syncingPolicy;
    XCTAssertNotNil(currentPolicy, "should have a current policy");

    // In this test, we receive the deletion, and then the policy change adding the Passwords view
    [self.keychainZone deleteCKRecordIDFromZone:itemCKRecord.recordID];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
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
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

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
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Another device shows up, changes the item sync policy, and moves the item over into the Passwords view.
    // But, in this case, we receive the item delete in the Keychain view after we've synced the item in the Passwords view

    NSDictionary* itemContents = [self decryptRecord:itemCKRecord];
    XCTAssertNotNil(itemContents, "should have some item contents");

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:itemCKRecord.recordID.recordName zoneID:self.passwordsZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:itemContents];
    [self.zones[self.passwordsZoneID] addToZone:ckr];

    TPSyncingPolicy* currentPolicy = self.defaultCKKS.syncingPolicy;
    XCTAssertNotNil(currentPolicy, "should have a current policy");

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];

    CKKSKeychainViewState* passwordsView = [self.defaultCKKS.operationDependencies viewStateForName:@"Passwords"];
    XCTAssertNotNil(passwordsView, @"Should have a passwords view");

    // And once Passwords syncs, the item should appear again
    [self findGenericPassword:itemAccount expecting:errSecSuccess];

    // And now we receive the delete in the keychain view. The item should still exist!
    [self.keychainZone deleteCKRecordIDFromZone:itemCKRecord.recordID];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:itemAccount expecting:errSecSuccess];

    // Keychain View should have asked for a policy set
    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);
    [self.defaultCKKS setCurrentSyncingPolicy:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
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
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];

    // Now, someone uploads an item to the keychain view. We should move it to the passwords view
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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

    [self.defaultCKKS setCurrentSyncingPolicy:self.allItemsToPasswordsPolicy policyIsFresh:YES];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the keychain item should still exist
    [self findGenericPassword:@"account0" expecting:errSecSuccess];
}

- (void)testReceiveItemConflictInWrongView {
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self createAndSaveFakeKeyHierarchy:self.passwordsZoneID];

    [self startCKKSSubsystem];
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:NO];
    self.passwordsView = [self.defaultCKKS.operationDependencies viewStateForName:@"Passwords"];
    XCTAssertNotNil(self.passwordsView, @"Policy created a passwords view");

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.passwordsView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.passwordsZoneID];
    [self addGenericPassword:@"data" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // So far, so good. But, someone uploads a version of this record to this other zone?

    OCMExpect([self.requestPolicyCheck trigger]);

    NSDictionary* item = [self fakeRecordDictionary:@"account-delete-me" password:@"updated_data" zoneID:self.keychainZoneID];
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* ckr = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // We should not have updated the password yet
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.requestPolicyCheck, 10);

    // When CKKS resolves the issue, it should accept the update and push a new record to Passwords,
    // and also delete the record from 'keychain'.

    NSError* zoneError = nil;
    NSInteger count = [CKKSIncomingQueueEntry countByState:SecCKKSStateMismatchedView zone:self.keychainView.zoneID error:&zoneError];
    XCTAssertNil(zoneError, "should be no error counting all IQEs");
    XCTAssertEqual(count, 1, "Should be one mismatched IQE");

    // But note that CKKS should _not_ re-request the policy; that way lies infinite loops
    [self.requestPolicyCheck stopMocking];
    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMReject([self.requestPolicyCheck trigger]);

    // see testMoveItemUploadedToOldZone for a note on why we don't perform this delete.
    //[self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self setPolicyAndWaitForQuiescence:self.allItemsToPasswordsPolicy policyIsFresh:YES];

    NSInteger postFixCount = [CKKSIncomingQueueEntry countByState:SecCKKSStateMismatchedView zone:self.keychainView.zoneID error:&zoneError];
    XCTAssertNil(zoneError, "should be no error counting all IQEs");
    XCTAssertEqual(postFixCount, 0, "Should be zero mismatched IQEs");

    // We should not have taken the update
    [self checkGenericPassword:@"data" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    // And, receiving a new item to the right view does _not_ trigger a sync policy check, or a CKKSScan to fix up the mismatched item we already processed,
    self.defaultCKKS.initiatedLocalScan = false;

    NSDictionary* item2 = [self fakeRecordDictionary:@"account-delete-me-2" password:@"new_item_data" zoneID:self.passwordsZoneID];
    CKRecordID* ckr2id = [[CKRecordID alloc] initWithRecordName:@"50184A35-4480-E8BA-769B-567CF7200000" zoneID:self.passwordsZoneID];
    CKRecord* ckr2 = [self newRecord:ckr2id withNewItemData:item2];
    [self.zones[self.passwordsZoneID] addToZone:ckr2];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"account-delete-me-2" expecting:errSecSuccess];
    XCTAssertFalse(self.defaultCKKS.initiatedLocalScan, "Should not have initiated a local scan");
}

- (void)testHistoricalKeyRoll {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // An item is uploaded under the old key
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account-delete-me"];
    [self.keychainZone addToZone:ckr];

    // and the keys are rolled
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];
}

- (void)testLossOfHistoricalKeyRecord {
    // This test examines what a device does if it doesn't have a synckey for an item. I don't know how a device enters this state other than
    // CK sync dropping the item. So, simulate that.

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // An item is uploaded under the old key
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account-delete-me"];
    [self.keychainZone addToZone:ckr];

    // Cause our fake CK to not send the record down
    CKRecordID* classCRecordID = [self.keychainZoneKeys.classC CKRecordWithZoneID:self.keychainZoneID].recordID;
    [self.keychainZone.recordIDsToSkip addObject:classCRecordID];

    // and the keys are rolled
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // After the first fetch, let's make the key available again

    self.silentFetchesAllowed = NO;
    [self expectCKFetchAndRunBeforeFinished:^{
        [self.keychainZone.recordIDsToSkip removeAllObjects];

        // and CKKS should refetch
        [self expectCKFetch];
    }];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // After we refetch, we should be able to process the item
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];
}


@end

#endif // OCTAGON
