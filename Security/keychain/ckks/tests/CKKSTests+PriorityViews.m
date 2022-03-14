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

#include <Security/SecItemPriv.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CKKSTests+MultiZone.h"

#import "keychain/ot/ObjCImprovements.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSManifest.h"

#import "keychain/ckks/tests/MockCloudKit.h"

@interface CloudKitKeychainSyncingPriorityViewTests : CloudKitKeychainSyncingMultiZoneTestsBase
@property CKKSSOSSelfPeer* remotePeer1;
@end

@implementation CloudKitKeychainSyncingPriorityViewTests

- (void)setUp {
    [super setUp];

    self.remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                    encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                       signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                         viewList:self.managedViewList];

    [self.mockSOSAdapter.trustedPeers addObject:self.remotePeer1];
}

- (void)tearDown {
    self.remotePeer1 = nil;
    [super tearDown];
}

- (void)testPriorityViewDownloadIfTrustArrivesBeforeFetch {
    WEAKIFY(self);

    [self putFakeKeyHierachiesInCloudKit];
    [self putFakeDeviceStatusesInCloudKit];
    [self putAllSelfTLKSharesInCloudKit:self.remotePeer1];
    [self putAllTLKSharesInCloudKitFrom:self.remotePeer1 to:self.mockSOSAdapter.selfPeer];

    // Put sample data in zones, both priority and not, and save off a change token for later fetch shenanigans
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"manatee0"]];
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"manatee1"]];

    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D00" withAccount:@"limited0"]];
    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D01" withAccount:@"limited1"]];

    // Also, add some local items, to check upload time
    [self addGenericPassword:@"data" account:@"account-delete-me-limited-peers" viewHint:(NSString*)kSecAttrViewHintLimitedPeersAllowed];
    [self addGenericPassword:@"data" account:@"account-delete-me-limited-peers-2" viewHint:(NSString*)kSecAttrViewHintLimitedPeersAllowed];
    [self addGenericPassword:@"data" account:@"account-delete-me-manatee-1" viewHint:(NSString*)kSecAttrViewHintManatee];

    // We should see only a fetch for the priority zones first...
    // When we see that fetch, we'll pause the follow-on fetches to allow us to inspect local keychain state
    self.silentFetchesAllowed = false;

    [self expectCKFetchForZones:[NSSet setWithArray:@[self.limitedZoneID, self.homeZoneID]]
              runBeforeFinished:^{
        STRONGIFY(self);
        [self holdCloudKitFetches];
        self.silentFetchesAllowed = true;
    }];

    // As part of the priority view handling, we'll upload TLKShares for the priority zones.
    // This could probably be moved later for less user-wait time.
    [self expectCKKSTLKSelfShareUpload:self.limitedZoneID];
    [self expectCKKSTLKSelfShareUpload:self.homeZoneID];

    // There's a potential race here: if CKKS begins fetching before the test machinery tells it that it's trusted,
    // we could fail. Pause the state machine to win the race.
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateBeginFetch];

    [self startCKKSSubsystem];

    XCTAssertEqual([self.defaultCKKS.stateMachine.stateConditions[CKKSStateBeginFetch] wait:30 * NSEC_PER_SEC], 0, "CKKS should begin a fetch");

    CKKSResultOperation* waitForPriorityView = [self.defaultCKKS rpcWaitForPriorityViewProcessing];

    // Ensure that the API waits sufficiently
    sleep(1);
    XCTAssertFalse(waitForPriorityView.finished, "rpcWaitForPriorityViewProcessing should not have finished");

    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateBeginFetch];

    [waitForPriorityView waitUntilFinished];
    XCTAssertNil(waitForPriorityView.error, "Should be no error waiting for PriorityView processing");

    XCTAssertEqual([self.homeView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:30 * NSEC_PER_SEC], 0, "Home should enter key state ready");
    XCTAssertEqual([self.limitedView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:30 * NSEC_PER_SEC], 0, "LimitedPeersAllowed should enter key state ready");
    XCTAssertNotEqual([self.manateeView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:1 * NSEC_PER_SEC], 0, "Manatee should not be in key state ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual([self.defaultCKKS.stateMachine.stateConditions[CKKSStateFetch] wait:30 * NSEC_PER_SEC], 0, "CKKS should refetch the non-priority views");

    // We should have the priority items, but not the non-priority items
    [self findGenericPassword:@"manatee0" expecting:errSecItemNotFound];
    [self findGenericPassword:@"manatee1" expecting:errSecItemNotFound];
    [self findGenericPassword:@"limited0" expecting:errSecSuccess];
    [self findGenericPassword:@"limited1" expecting:errSecSuccess];

    // Calling the API again should succeed, even without CKKS proceeding
    {
        CKKSResultOperation* nextWait = [self.defaultCKKS rpcWaitForPriorityViewProcessing];
        [nextWait waitUntilFinished];
        XCTAssertNil(nextWait.error, "Should be no error with rpcWaitForPriorityViewProcessing before the non-priority views have finished");
    }

    // expect TLKShare uploads for non-priority views
    for(CKKSKeychainViewState* viewState in self.ckksViews) {
        if(viewState.ckksManagedView && ![self.defaultCKKS.syncingPolicy.priorityViews containsObject:viewState.zoneID.zoneName]) {
            [self expectCKKSTLKSelfShareUpload:viewState.zoneID];
        }
    }

    // And only now should we expect uploads for the local items
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.limitedZoneID];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID];

    [self releaseCloudKitFetchHold];
    [self waitForKeyHierarchyReadinesses];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    [self findGenericPassword:@"manatee0" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee1" expecting:errSecSuccess];
    [self findGenericPassword:@"limited0" expecting:errSecSuccess];
    [self findGenericPassword:@"limited1" expecting:errSecSuccess];

    // And calling the API from a ready state should be fast too
    {
        CKKSResultOperation* nextWait = [self.defaultCKKS rpcWaitForPriorityViewProcessing];
        [nextWait waitUntilFinished];
        XCTAssertNil(nextWait.error, "Should be no error with rpcWaitForPriorityViewProcessing in a ready state");
    }
}

- (void)testPriorityViewDownloadWhenTrustArrivesMidFetch {
    WEAKIFY(self);

    [self putFakeKeyHierachiesInCloudKit];
    [self putFakeDeviceStatusesInCloudKit];
    [self putAllSelfTLKSharesInCloudKit:self.remotePeer1];
    [self putAllTLKSharesInCloudKitFrom:self.remotePeer1 to:self.mockSOSAdapter.selfPeer];

    // Put sample data in zones, both priority and not, and save off a change token for later fetch shenanigans
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"manatee0"]];
    CKServerChangeToken* manateeChangeToken = self.manateeZone.currentChangeToken;
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"manatee1"]];
    self.manateeZone.limitFetchTo = manateeChangeToken;

    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D00" withAccount:@"limited0"]];
    CKServerChangeToken* limitedChangeToken = self.limitedZone.currentChangeToken;
    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D01" withAccount:@"limited1"]];
    self.limitedZone.limitFetchTo = limitedChangeToken;

    // Also, add some local items, to check upload time
    [self addGenericPassword:@"data" account:@"account-delete-me-limited-peers" viewHint:(NSString*)kSecAttrViewHintLimitedPeersAllowed];
    [self addGenericPassword:@"data" account:@"account-delete-me-limited-peers-2" viewHint:(NSString*)kSecAttrViewHintLimitedPeersAllowed];
    [self addGenericPassword:@"data" account:@"account-delete-me-manatee-1" viewHint:(NSString*)kSecAttrViewHintManatee];

    // Start with no trust
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    // We should see a fetch for all zones, and then we'll have trust arrive.
    // The next fetch should be for the priority zones only.
    // rpcWaitForPriorityViewProcessing must be called after trust is established, so hold off on that.
    // We'll pause the CK fetching for the non-priority fetch, which will simulate downloading many, many non-priority items.
    self.silentFetchesAllowed = false;

    __block CKKSResultOperation* waitForPriorityView = nil;
    XCTestExpectation* trustBegins = [self expectationWithDescription:@"trust begins as part of fetch"];

    [self expectCKFetchForZones:[NSSet setWithArray:self.zones.allKeys]
              runBeforeFinished:^{
        STRONGIFY(self);

        [self holdCloudKitFetches];

        self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
        [self beginSOSTrustedOperationForAllViews];

        waitForPriorityView = [self.defaultCKKS rpcWaitForPriorityViewProcessing];

        // This fetch should be for priority views only
        [self expectCKFetchForZones:[NSSet setWithArray:@[self.limitedZoneID, self.homeZoneID]]
                  runBeforeFinished:^{
            STRONGIFY(self);
            [self holdCloudKitFetches];

            // And this is the fetch finishing up the Manatee view
            [self expectCKFetchForZones:[NSSet setWithArray:self.zones.allKeys] runBeforeFinished:^{}];
        }];

        [trustBegins fulfill];
    }];

    // As part of the priority view handling, we'll upload TLKShares for the priority zones.
    // This could probably be moved later for less user-wait time.
    [self expectCKKSTLKSelfShareUpload:self.limitedZoneID];
    [self expectCKKSTLKSelfShareUpload:self.homeZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual([self.defaultCKKS.stateMachine.stateConditions[CKKSStateBeginFetch] wait:30 * NSEC_PER_SEC], 0, "CKKS should begin a fetch");

    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateBeginFetch];

    [self waitForExpectations:@[trustBegins] timeout:10];

    // Ensure that the API waits sufficiently
    sleep(1);
    XCTAssertFalse(waitForPriorityView.finished, "rpcWaitForPriorityViewProcessing should not have finished");

    [self releaseCloudKitFetchHold];

    XCTAssertNotNil(waitForPriorityView, "Should have an operation showing the results of waitForPriorityView");
    [waitForPriorityView waitUntilFinished];
    XCTAssertNil(waitForPriorityView.error, "Should be no error waiting for PriorityView processing");

    XCTAssertEqual([self.homeView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:30 * NSEC_PER_SEC], 0, "Home should enter key state ready");
    XCTAssertEqual([self.limitedView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:30 * NSEC_PER_SEC], 0, "LimitedPeersAllowed should enter key state ready");
    XCTAssertNotEqual([self.manateeView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:1 * NSEC_PER_SEC], 0, "Manatee should not be in key state ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual([self.defaultCKKS.stateMachine.stateConditions[CKKSStateFetch] wait:30 * NSEC_PER_SEC], 0, "CKKS should refetch the non-priority views");

    // We should have the priority items, but not the non-priority items
    [self findGenericPassword:@"manatee0" expecting:errSecItemNotFound];
    [self findGenericPassword:@"manatee1" expecting:errSecItemNotFound];
    [self findGenericPassword:@"limited0" expecting:errSecSuccess];
    [self findGenericPassword:@"limited1" expecting:errSecSuccess];

    // expect TLKShare uploads for non-priority views
    for(CKKSKeychainViewState* viewState in self.ckksViews) {
        if(viewState.ckksManagedView && ![self.defaultCKKS.syncingPolicy.priorityViews containsObject:viewState.zoneID.zoneName]) {
            [self expectCKKSTLKSelfShareUpload:viewState.zoneID];
        }
    }

    // And only now should we expect uploads for the local items
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.limitedZoneID];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID];

    [self releaseCloudKitFetchHold];
    [self waitForKeyHierarchyReadinesses];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    [self findGenericPassword:@"manatee0" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee1" expecting:errSecSuccess];
    [self findGenericPassword:@"limited0" expecting:errSecSuccess];
    [self findGenericPassword:@"limited1" expecting:errSecSuccess];
}

- (void)testPriorityViewDownloadInWaitForTrust {
    [self putFakeKeyHierachiesInCloudKit];
    [self putFakeDeviceStatusesInCloudKit];
    [self putAllSelfTLKSharesInCloudKit:self.remotePeer1];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    CKKSResultOperation* waitForPriorityViewBeforeStart = [self.defaultCKKS rpcWaitForPriorityViewProcessing];

    // CKKS should download the entire CK zones before entering waitForTrust, as there's no requirement for priority downloads until trust is established

    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"manatee0"]];
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"manatee1"]];
    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D00" withAccount:@"limited0"]];
    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D01" withAccount:@"limited1"]];

    self.silentFetchesAllowed = NO;
    [self expectCKFetchForZones:[NSSet setWithArray:self.zones.allKeys]
              runBeforeFinished:^{}];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitfortrust'");

    [waitForPriorityViewBeforeStart waitUntilFinished];
    XCTAssertNotNil(waitForPriorityViewBeforeStart.error, "Should be an error waiting for priority views before CKKS even starts");

    CKKSResultOperation* waitForPriorityViewsWhileInWaitForTrust = [self.defaultCKKS rpcWaitForPriorityViewProcessing];
    [waitForPriorityViewsWhileInWaitForTrust waitUntilFinished];
    XCTAssertNotNil(waitForPriorityViewsWhileInWaitForTrust.error, "Should be an error waiting for priority views in waitfortrust");

    // Check that all entries were downloaded in the single fetch
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* loadError = nil;
        NSSet<NSString*>* mirrors = [CKKSMirrorEntry allUUIDsInZones:[NSSet setWithArray:self.zones.allKeys] error:&loadError];
        XCTAssertNotNil(mirrors, "Should have a bag of UUIDs");
        XCTAssertNil(loadError, "Should be no error finding UUIDs");

        XCTAssertEqual(mirrors.count, 4, "Should have 4 UUIDs");

        XCTAssert([mirrors containsObject:@"7B598D31-0000-0000-0000-5A507ACB2D00"], "Should have known UUID #1");
        XCTAssert([mirrors containsObject:@"7B598D31-0000-0000-0000-5A507ACB2D01"], "Should have known UUID #2");
        XCTAssert([mirrors containsObject:@"7B598D31-0000-0000-FFFF-5A507ACB2D00"], "Should have known UUID #3");
        XCTAssert([mirrors containsObject:@"7B598D31-0000-0000-FFFF-5A507ACB2D01"], "Should have known UUID #4");
    }];

    // Now, if trust arrives, CKKS should process all of the priority items first

    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateExpandToHandleAllViews];

    [self expectCKKSTLKSelfShareUploads];
    [self putAllTLKSharesInCloudKitFrom:self.remotePeer1 to:self.mockSOSAdapter.selfPeer];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self beginSOSTrustedOperationForAllViews];

    // CKKS will refetch as part of bringup, to get the TLKShares. There will be two: one for priority views and one for the rest.
    [self expectCKFetch];
    [self expectCKFetch];

    CKKSResultOperation* waitForPriorityViewOp = [self.defaultCKKS rpcWaitForPriorityViewProcessing];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateExpandToHandleAllViews] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'CKKSStateExpandToHandleAllViews'");

    [waitForPriorityViewOp waitUntilFinished];
    XCTAssertNil(waitForPriorityViewOp.error, "Should be no error waiting for priority views");

    // We should have the priority items, but not the non-priority items
    [self findGenericPassword:@"manatee0" expecting:errSecItemNotFound];
    [self findGenericPassword:@"manatee1" expecting:errSecItemNotFound];
    [self findGenericPassword:@"limited0" expecting:errSecSuccess];
    [self findGenericPassword:@"limited1" expecting:errSecSuccess];

    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateExpandToHandleAllViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    [self findGenericPassword:@"manatee0" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee1" expecting:errSecSuccess];
    [self findGenericPassword:@"limited0" expecting:errSecSuccess];
    [self findGenericPassword:@"limited1" expecting:errSecSuccess];
}

- (void)testPriorityViewDownloadWithMidflightLossOfTrust {
    WEAKIFY(self);

    [self putFakeKeyHierachiesInCloudKit];
    [self putFakeDeviceStatusesInCloudKit];
    [self putAllSelfTLKSharesInCloudKit:self.remotePeer1];
    [self putAllTLKSharesInCloudKitFrom:self.remotePeer1 to:self.mockSOSAdapter.selfPeer];

    // Put sample data in zones, both priority and not, and save off a change token for later fetch shenanigans
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"manatee0"]];
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"manatee1"]];

    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D00" withAccount:@"limited0"]];
    [self.limitedZone addToZone:[self createFakeRecord:self.limitedZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D01" withAccount:@"limited1"]];

    // Also, add some local items, to check upload time
    [self addGenericPassword:@"data" account:@"account-delete-me-limited-peers" viewHint:(NSString*)kSecAttrViewHintLimitedPeersAllowed];
    [self addGenericPassword:@"data" account:@"account-delete-me-limited-peers-2" viewHint:(NSString*)kSecAttrViewHintLimitedPeersAllowed];
    [self addGenericPassword:@"data" account:@"account-delete-me-manatee-1" viewHint:(NSString*)kSecAttrViewHintManatee];

    // We should see only a fetch for the priority zones first...
    // When we see that fetch, we'll pause the follow-on fetches to allow us to inspect local keychain state
    self.silentFetchesAllowed = false;

    [self expectCKFetchForZones:[NSSet setWithArray:@[self.limitedZoneID, self.homeZoneID]]
              runBeforeFinished:^{
        STRONGIFY(self);

        [self endSOSTrustedOperationForAllViews];

        // But we'll get another fetch, for the non-priority views
        [self expectCKFetch];

        // And yet another fetch, due to not-trusted startup behavior. This is unideal and should be changed.
        [self expectCKFetch];
    }];

    // No uploads should occur

    // Pause the state machine at fetch time, to win the API race with rpcWaitForPriorityViewProcessing
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateBeginFetch];
    [self startCKKSSubsystem];
    XCTAssertEqual([self.defaultCKKS.stateMachine.stateConditions[CKKSStateBeginFetch] wait:30 * NSEC_PER_SEC], 0, "CKKS should begin a fetch");

    CKKSResultOperation* waitForPriorityView = [self.defaultCKKS rpcWaitForPriorityViewProcessing];

    // Ensure that the API waits sufficiently
    sleep(1);
    XCTAssertFalse(waitForPriorityView.finished, "rpcWaitForPriorityViewProcessing should not have finished");

    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateBeginFetch];

    [waitForPriorityView waitUntilFinished];
    XCTAssertNotNil(waitForPriorityView.error, "Should be an error waiting for PriorityView processing when there's a trust loss");

    XCTAssertEqual([self.homeView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:30 * NSEC_PER_SEC], 0, "Home should enter key state WaitForTrust");
    XCTAssertEqual([self.limitedView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:30 * NSEC_PER_SEC], 0, "LimitedPeersAllowed should enter key state WaitForTrust");
    XCTAssertEqual([self.manateeView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:1 * NSEC_PER_SEC], 0, "Manatee should enter key state WaitForTrust");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual([self.defaultCKKS.stateMachine.stateConditions[CKKSStateWaitForTrust] wait:30 * NSEC_PER_SEC], 0, "CKKS should eventually realize it should lose trust");

    // Calling the API again should fail, even without CKKS proceeding
    {
        CKKSResultOperation* nextWait = [self.defaultCKKS rpcWaitForPriorityViewProcessing];
        [nextWait waitUntilFinished];
        XCTAssertNotNil(nextWait.error, "Should be an error with rpcWaitForPriorityViewProcessing before the non-priority views have finished");
    }
}

@end

#endif // OCTAGON

