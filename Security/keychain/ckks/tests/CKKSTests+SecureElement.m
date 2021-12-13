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
#import "keychain/ckks/CKKSExternalTLKClient.h"
#import "keychain/ckks/CKKSExternalTLKClient+Categories.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"

@interface CloudKitKeychainSecureElementTests : CloudKitKeychainSyncingTestsBase
@property CKRecordZoneID* ptaZoneID;
@property CKRecordZoneID* ptcZoneID;
@end

@implementation CloudKitKeychainSecureElementTests

- (void)setUp {
    self.ptaZoneID = [[CKRecordZoneID alloc] initWithZoneName:CKKSSEViewPTA
                                                    ownerName:CKCurrentUserDefaultName];
    self.ptcZoneID = [[CKRecordZoneID alloc] initWithZoneName:CKKSSEViewPTC
                                                    ownerName:CKCurrentUserDefaultName];

    [super setUp];
}

- (TPSyncingPolicy*)viewSortingPolicyForManagedViewList
{
    return [self viewSortingPolicyForManagedViewListWithUserControllableViews:[NSSet set]
                                                    syncUserControllableViews:TPPBPeerStableInfoUserControllableViewStatus_ENABLED];
}

- (void)testProposeTLKAndUploadShares {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    CKKSExternalTLKShare* tlkShare2 = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                         tlkUUID:newTLK.uuid
                                                                  receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZmFzZGYK" options:0]
                                                                    senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                       signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:0
          currentKeyPointerRecords:0
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* updateExpectation = [self expectationWithDescription:@"update should arrive"];
    [self.ckksControl modifyTLKSharesForSEView:CKKSSEViewPTA
                                        adding:@[tlkShare2]
                                      deleting:@[]
                                         reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error uploading a TLK");
        [updateExpectation fulfill];
    }];

    [self waitForExpectations:@[updateExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    self.silentFetchesAllowed = NO;
    [self expectCKFetch];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newTLK, "Current TLK should match proposed TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 2, "Should be 2 tlkShares");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testProposeTLKAndReceiveConflict {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // Stricly speaking, this will be a CKKS hierarchy and not an SE one. But it should all look the same!
    [self putFakeKeyHierarchyInCloudKit:self.ptaZoneID];

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNotNil(error, "Should be an error proposing a TLK when there already is one");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFetchViewKeysBeforeInitialCKFetch {
    [self holdCloudKitFetches];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateFetch] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'fetch'");

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:NO
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNotNil(error, "Should be an error fetching the view hierarchy");
        XCTAssertNil(currentTLK, "Should not have a current TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 0, "Should be no tlkShares");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self releaseCloudKitFetchHold];
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    XCTestExpectation* fetchExpectation2 = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:NO
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNil(currentTLK, "Should not have a current TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 0, "Should be no tlkShares");
        [fetchExpectation2 fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation2] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFetchWihoutCreatingZone {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    self.silentFetchesAllowed = NO;
    [self expectCKFetch];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNil(currentTLK, "Should not have a current TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 0, "Should be no tlkShares");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFetchWihoutCKAccountStatusKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];

    self.silentFetchesAllowed = NO;

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNotNil(error, "Should be an error fetching the view hierarchy with no CK account");

        XCTAssertEqualObjects(error.domain, CKKSErrorDomain, "Error should be from the CKKSErrorDomain");
        XCTAssertEqual(error.code, CKKSErrorAccountStatusUnknown, "Error should be CKKSErrorAccountStatusUnknown");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];

    // Start CKKS, to end the test properly
    self.silentFetchesAllowed = YES;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFetchWithCKAccountLoggedOut {
    self.accountStatus = CKAccountStatusNoAccount;
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self startCKKSSubsystem];

    self.silentFetchesAllowed = NO;

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNotNil(error, "Should be an error fetching the view hierarchy with no CK account");

        XCTAssertEqualObjects(error.domain, CKKSErrorDomain, "Error should be from the CKKSErrorDomain");
        XCTAssertEqual(error.code, CKKSNotLoggedIn, "Error should be CKKSNotLoggedIn");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
}

- (void)testFindNewShareOnPositiveFetch {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, someone else uploads a share

    CKKSExternalTLKShare* tlkShare2 = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                         tlkUUID:newTLK.uuid
                                                                  receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZmFzZGYK" options:0]
                                                                    senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZmFzZGYK" options:0]
                                                                      wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                       signature:[[NSData alloc] init]];

    FakeCKZone* zone = self.zones[self.ptaZoneID];
    XCTAssertNotNil(zone, "Should already have a fakeckzone before putting a keyset in it");

    CKRecord* tlkShareCKRecord = [[tlkShare2 makeTLKShareRecord:self.ptaZoneID] CKRecordWithZoneID:self.ptaZoneID];
    [zone addToZone:tlkShareCKRecord];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newTLK, "Current TLK should match proposed TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 2, "Should be 2 tlkShares");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // now, someone else removes that share
    [zone deleteCKRecordIDFromZone:tlkShareCKRecord.recordID];

    XCTestExpectation* fetch2Expectation = [self expectationWithDescription:@"fetch2 should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newTLK, "Current TLK should match proposed TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 1, "Should be 1 tlkShare");
        [fetch2Expectation fulfill];
    }];

    [self waitForExpectations:@[fetch2Expectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFindNewShareAfterNotification {
    // When the initial download is complete, CKKS should send the 'zone ready' notification for pta/ptc
    XCTestExpectation* ptaReadyNotificationExpectation = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];
    XCTestExpectation* ptcReadyNotificationExpectation = [self expectLibNotifyReadyForView:self.ptcZoneID.zoneName];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self waitForExpectations:@[ptaReadyNotificationExpectation, ptcReadyNotificationExpectation] timeout:10];

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, someone else uploads a share

    CKKSExternalTLKShare* tlkShare2 = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                         tlkUUID:newTLK.uuid
                                                                  receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZmFzZGYK" options:0]
                                                                    senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZmFzZGYK" options:0]
                                                                      wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                       signature:[[NSData alloc] init]];

    FakeCKZone* zone = self.zones[self.ptaZoneID];
    XCTAssertNotNil(zone, "Should already have a fakeckzone before putting a keyset in it");

    CKRecord* tlkShareCKRecord = [[tlkShare2 makeTLKShareRecord:self.ptaZoneID] CKRecordWithZoneID:self.ptaZoneID];
    [zone addToZone:tlkShareCKRecord];

    // When this TLKShare arrives, we should send a change notification for PTA, but not PTC
    XCTestExpectation* ptaChanged = [self expectChangeForView:self.ptaZoneID.zoneName];
    XCTestExpectation* ptcChanged = [self expectChangeForView:self.ptcZoneID.zoneName];
    ptcChanged.inverted = YES;

    // It should _not_ send a zone ready notification (we only want to send those after zone resets)
    XCTestExpectation* ptaReadyNotification = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];
    ptaReadyNotification.inverted = YES;

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self waitForExpectations:@[ptaChanged, ptcChanged, ptaReadyNotification] timeout:5];

    // Calling the cached API after the PTA-Changed notification shouldn't cause a fetch
    self.silentFetchesAllowed = false;

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:NO
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newTLK, "Current TLK should match proposed TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 2, "Should be 2 tlkShares");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // now, someone else removes that share
    [zone deleteCKRecordIDFromZone:tlkShareCKRecord.recordID];

    [self expectCKFetch];
    XCTestExpectation* ptaChanged2 = [self expectChangeForView:self.ptaZoneID.zoneName];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self waitForExpectations:@[ptaChanged2] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTestExpectation* fetch2Expectation = [self expectationWithDescription:@"fetch2 should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:NO
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newTLK, "Current TLK should match proposed TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 1, "Should be 1 tlkShare");
        [fetch2Expectation fulfill];
    }];

    [self waitForExpectations:@[fetch2Expectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testDeleteShare {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, we delete a share
    [self expectCKModifyRecords:nil
        deletedRecordTypeCounts:@{SecCKRecordTLKShareType: [NSNumber numberWithUnsignedInteger: 1]}
                         zoneID:self.ptaZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    XCTestExpectation* deleteExpectation = [self expectationWithDescription:@"deleteExpectation should pccir"];
    [self.ckksControl modifyTLKSharesForSEView:CKKSSEViewPTA
                                        adding:@[]
                                      deleting:@[tlkShare] reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error deleting TLK Share");
        [deleteExpectation fulfill];
    }];
    [self waitForExpectations:@[deleteExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And refetch:

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newTLK, "Current TLK should match proposed TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 0, "Should be 0 tlkShares");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testReset {
    XCTestExpectation* ptaInitialReadyNotificationExpectation = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    [self waitForExpectations:@[ptaInitialReadyNotificationExpectation] timeout:20];

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    FakeCKZone* zone = self.zones[self.ptaZoneID];
    XCTAssertNotNil(zone, "Should already have a fakeckzone before putting a keyset in it");
    zone.flag = true;
    self.silentZoneDeletesAllowed = true;

    // After the delete, CKKS should remake the zone, and send this notification once that process is complete
    XCTestExpectation* ptaReadyNotificationExpectation = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];

    XCTestExpectation* resetExpectation = [self expectationWithDescription:@"reset"];
    [self.ckksControl deleteSEView:CKKSSEViewPTA
                             reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error resetting");
        [resetExpectation fulfill];
    }];
    [self waitForExpectations:@[resetExpectation, ptaReadyNotificationExpectation] timeout:20];

    NSData* newerData = [[NSData alloc] initWithBase64EncodedString:@"Rs68q+9xcbv0qCDKx8UbA3qxriMw7rhZAcr5yQy+tTxfa6055r8TEW1vtJA1s7b85gXZAEozFvBCt1ogDEyKdzHeIhrfxDuAg/6wXFP7+aQ=" options:0];
    CKKSExternalKey* newerTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                                 uuid:[[NSUUID UUID] UUIDString]
                                                        parentTLKUUID:nil
                                                              keyData:newerData];

    CKKSExternalTLKShare* tlkShare2 = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                         tlkUUID:newerTLK.uuid
                                                                  receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZmFzZGYK" options:0]
                                                                    senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                       signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* newerProposeExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newerTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare2]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [newerProposeExpectation fulfill];
    }];
    [self waitForExpectations:@[newerProposeExpectation] timeout:20];

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNotNil(currentTLK, "Should have a current TLK");
        XCTAssertEqualObjects(currentTLK, newerTLK, "Current TLK should match the newer TLK");
        XCTAssertNotEqualObjects(currentTLK, newTLK, "Current TLK should not match the origina TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 1, "Should be 1 tlkShare");
        [fetchExpectation fulfill];
    }];

    [self waitForExpectations:@[fetchExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromZoneMissing {
    XCTestExpectation* initialPTAReadyNotificationExpectation = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self waitForExpectations:@[initialPTAReadyNotificationExpectation] timeout:20];

    NSData* keyData = [[NSData alloc] initWithBase64EncodedString:@"1TQNQAZh0Gguq/FvUhisRh9Hf0LEB8f4ZOKAJgUB6eWYbKGhh47+qpgRagwtXPSa7aq6NSPfBDYqsMQFlJjHojXD/HJud9nAHDljTTkbSBfriGftBA+HO4Z5aAJZm/SArpicHlXRWQmmEYwimTnxUcXRR7gg7DrH" options:0];
    CKKSExternalKey* newTLK = [[CKKSExternalKey alloc] initWithView:CKKSSEViewPTA
                                                               uuid:[[NSUUID UUID] UUIDString]
                                                      parentTLKUUID:nil
                                                            keyData:keyData];

    CKKSExternalTLKShare* tlkShare = [[CKKSExternalTLKShare alloc] initWithView:CKKSSEViewPTA
                                                                        tlkUUID:newTLK.uuid
                                                                 receiverPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                   senderPeerID:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                     wrappedTLK:[[NSData alloc] initWithBase64EncodedString:@"YXNkZgo=" options:0]
                                                                      signature:[[NSData alloc] init]];

    [self expectCKModifyKeyRecords:3
          currentKeyPointerRecords:3
                   tlkShareRecords:1
                            zoneID:self.ptaZoneID];

    XCTestExpectation* replyExpectation = [self expectationWithDescription:@"reply should arrive"];
    [self.ckksControl proposeTLKForSEView:CKKSSEViewPTA
                              proposedTLK:newTLK
                            wrappedOldTLK:nil
                                tlkShares:@[tlkShare]
                                    reply:^(NSError* _Nullable error) {
        XCTAssertNil(error, "Should be no error proposing a TLK");
        [replyExpectation fulfill];
    }];

    [self waitForExpectations:@[replyExpectation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // Now, another client deletes the zone
    self.zones[self.ptaZoneID] = nil;

    // On the next fetch, CKKS should notice the deletion, remake the zone, and resend the 'ready' notification
    XCTestExpectation* ptaReadyNotificationExpectation = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];

    ckksnotice_global("asdf", "sending notification");
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    [self waitForExpectations:@[ptaReadyNotificationExpectation] timeout:20];
    XCTAssertNotNil(self.zones[self.ptaZoneID], "After the notification is sent, PTA zone should be remade");

    XCTestExpectation* fetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                    forceFetch:NO
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNil(currentTLK, "Should not have a current TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 0, "Should be 0 tlkShare");
        [fetchExpectation fulfill];
    }];
    [self waitForExpectations:@[fetchExpectation] timeout:20];

    // And does the recovery work via API?
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    self.zones[self.ptaZoneID] = nil;

    XCTestExpectation* ptaReadyNotificationExpectation2 = [self expectLibNotifyReadyForView:self.ptaZoneID.zoneName];
    XCTestExpectation* forceFetchExpectation = [self expectationWithDescription:@"fetch should arrive"];
    [self.ckksControl fetchSEViewKeyHierarchy:CKKSSEViewPTA
                                   forceFetch:YES
                                        reply:^(CKKSExternalKey * _Nullable currentTLK,
                                                NSArray<CKKSExternalKey *> * _Nullable pastTLKs,
                                                NSArray<CKKSExternalTLKShare *> * _Nullable currentTLKShares,
                                                NSError * _Nullable error) {
        XCTAssertNil(error, "Should be no error fetching the view hierarchy");
        XCTAssertNil(currentTLK, "Should not have a current TLK");
        XCTAssertEqual(pastTLKs.count, 0, "Should be no past TLKs");
        XCTAssertEqual(currentTLKShares.count, 0, "Should be 0 tlkShare");
        [forceFetchExpectation fulfill];
    }];
    [self waitForExpectations:@[ptaReadyNotificationExpectation2, forceFetchExpectation] timeout:20];
}

@end

#endif
