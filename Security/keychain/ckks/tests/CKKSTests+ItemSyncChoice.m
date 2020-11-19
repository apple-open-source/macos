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

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/MockCloudKit.h"

@interface CloudKitKeychainSyncingItemSyncChoiceTests : CloudKitKeychainSyncingTestsBase
@property CKKSSOSSelfPeer* remotePeer1;
@end

@implementation CloudKitKeychainSyncingItemSyncChoiceTests

- (size_t)outgoingQueueSize:(CKKSKeychainView*)view {
    __block size_t result = 0;

    [view dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* zoneError = nil;
        NSArray<CKKSOutgoingQueueEntry*>* entries = [CKKSOutgoingQueueEntry all:view.zoneID error:&zoneError];
        XCTAssertNil(zoneError, "should be no error fetching all OQEs");

        result = (size_t)entries.count;
    }];
    return result;
}

- (size_t)incomingQueueSize:(CKKSKeychainView*)view {
    __block size_t result = 0;

    [view dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* zoneError = nil;
        NSArray<CKKSIncomingQueueEntry*>* entries = [CKKSIncomingQueueEntry all:view.zoneID error:&zoneError];
        XCTAssertNil(zoneError, "should be no error fetching all IQEs");

        result = (size_t)entries.count;
    }];
    return result;
}

- (void)setUp {
    [super setUp];

    self.remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                    encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                       signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                         viewList:self.managedViewList];
}

- (void)testAddItemToPausedView {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_DISABLED]
                                 policyIsFresh:NO];

    [self addGenericPassword:@"data" account:@"account-delete-me"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    XCTAssertEqual(1, [self outgoingQueueSize:self.keychainView], "There should be one pending item in the outgoing queue");

    // and again
    [self addGenericPassword:@"data" account:@"account-delete-me-2"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    XCTAssertEqual(2, [self outgoingQueueSize:self.keychainView], "There should be two pending item in the outgoing queue");

    // When syncing is enabled, these items should sync
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_ENABLED]
                                 policyIsFresh:NO];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testReceiveItemToPausedView {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");

    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_DISABLED]
                                 policyIsFresh:NO];

    [self findGenericPassword: @"account0" expecting:errSecItemNotFound];

    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D00" withAccount:@"account0"]];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    XCTAssertEqual(1, [self incomingQueueSize:self.keychainView], "There should be one pending item in the incoming queue");

    [self.keychainZone addToZone:[self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-0000-5A507ACB2D00" withAccount:@"account1"]];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    XCTAssertEqual(2, [self incomingQueueSize:self.keychainView], "There should be two pending item in the incoming queue");

    [self findGenericPassword:@"account0" expecting:errSecItemNotFound];
    [self findGenericPassword:@"account1" expecting:errSecItemNotFound];

    // When syncing is enabled, these items should sync
    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_ENABLED]
                                 policyIsFresh:NO];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self findGenericPassword:@"account0" expecting:errSecSuccess];
    [self findGenericPassword:@"account1" expecting:errSecSuccess];
}

- (void)testAcceptKeyHierarchyWhilePaused {
    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_DISABLED]
                                 policyIsFresh:NO];

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
}

- (void)testUploadSelfTLKShare {
    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_DISABLED]
                                 policyIsFresh:NO];

    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self.mockSOSAdapter.trustedPeers addObject:self.remotePeer1];
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testSendNewTLKSharesOnTrustSetAddition {
    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_DISABLED]
                                 policyIsFresh:NO];

    // step 1: add a new peer; we should share the TLK with them
    // start with no trusted peers
    [self.mockSOSAdapter.trustedPeers removeAllObjects];

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self.mockSOSAdapter.trustedPeers addObject:self.remotePeer1];
    [self.mockSOSAdapter sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // and just double-check that no syncing is occurring
    [self addGenericPassword:@"data" account:@"account-delete-me"];
    [self.keychainView waitForOperationsOfClass:[CKKSOutgoingQueueOperation class]];
    XCTAssertEqual(1, [self outgoingQueueSize:self.keychainView], "There should be one pending item in the outgoing queue");
}

- (void)testAddAndNotifyOnSyncDuringPausedOperation {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView setCurrentSyncingPolicy:[self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet setWithObject:self.keychainView.zoneName]
                                                                                        syncUserControllableViews:TPPBPeerStableInfo_UserControllableViewStatus_DISABLED]
                                 policyIsFresh:NO];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef cferror) {
        XCTAssertFalse(didSync, "Item did not sync");

        NSError* error = (__bridge NSError*)cferror;
        XCTAssertNotNil(error, "Error syncing item");
        XCTAssertEqual(error.domain, CKKSErrorDomain, "Error domain was CKKSErrorDomain");
        XCTAssertEqual(error.code, CKKSErrorViewIsPaused, "Error code is 'view is paused'");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 10);

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
}

@end

#endif // OCTAGON
