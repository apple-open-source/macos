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

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSViewManager.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"

@interface CloudKitKeychainSyncingTLKSharingTests : CloudKitKeychainSyncingTestsBase
@property CKKSSOSSelfPeer* remotePeer1;
@property CKKSSOSPeer* remotePeer2;


@property CKKSSOSSelfPeer* untrustedPeer;
@end

@implementation CloudKitKeychainSyncingTLKSharingTests

- (void)setUp {
    [super setUp];
    SecCKKSSetShareTLKs(true);

    self.remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                    encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                       signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];

    self.remotePeer2 = [[CKKSSOSPeer alloc] initWithSOSPeerID:@"remote-peer2"
                                              encryptionPublicKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]].publicKey
                                                 signingPublicKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]].publicKey];

    // Local SOS trusts these peers
    [self.currentPeers addObject:self.remotePeer1];
    [self.currentPeers addObject:self.remotePeer2];

    self.untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"untrusted-peer"
                                                      encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                         signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];
}

- (void)tearDown {
    self.remotePeer1 = nil;
    self.remotePeer2 = nil;
    self.untrustedPeer = nil;

    [super tearDown];

    SecCKKSSetShareTLKs(false);
}

- (void)testAcceptExistingTLKSharedKeyHierarchy {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

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

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all:&error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");

        return false;
    }];
}

- (void)testAcceptExistingTLKSharedKeyHierarchyAndUse {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

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

- (void)testNewTLKSharesHaveChangeTags {
    // Since there's currently no flow for CKKS to ever update a TLK share when things are working properly, do some hackery

    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    [self saveTLKSharesInLocalDatabase:self.keychainZoneID];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Verify that making a new share will have the old share's change tag
    __weak __typeof(self) weakSelf = self;
    [self.keychainView dispatchSyncWithAccountKeys: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;
        CKKSTLKShare* share = [CKKSTLKShare share:strongSelf.keychainZoneKeys.tlk
                                               as:strongSelf.currentSelfPeer
                                               to:strongSelf.currentSelfPeer
                                            epoch:-1
                                         poisoned:0
                                            error:&error];
        XCTAssertNil(error, "Shouldn't be an error creating a share");
        XCTAssertNotNil(share, "Should be able to create share");

        CKRecord* newRecord = [share CKRecordWithZoneID:strongSelf.keychainZoneID];
        XCTAssertNotNil(newRecord, "Should be able to create a CKRecord");

        CKRecord* cloudKitRecord = strongSelf.keychainZone.currentDatabase[newRecord.recordID];
        XCTAssertNotNil(cloudKitRecord, "Should have found existing CKRecord in cloudkit");
        XCTAssertNotNil(cloudKitRecord.recordChangeTag, "Existing record should have a change tag");

        XCTAssertEqualObjects(cloudKitRecord.recordChangeTag, newRecord.recordChangeTag, "Change tags on existing and new records should match");

        return false;
    }];
}

- (void)testReceiveTLKShareRecordsAndDeletes {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    // The CKKS subsystem should not try to write anything to the CloudKit database while it's accepting the keys
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);


    // Make another share, but from an untrusted peer to some other peer. local shouldn't necessarily care.
    NSError* error = nil;
    CKKSTLKShare* share = [CKKSTLKShare share:self.keychainZoneKeys.tlk
                                           as:self.untrustedPeer
                                           to:self.remotePeer1
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");
    XCTAssertNotNil(share, "Should be able to create a share");

    CKRecord* shareCKRecord = [share CKRecordWithZoneID: self.keychainZoneID];
    XCTAssertNotNil(shareCKRecord, "Should have been able to create a CKRecord");
    [self.keychainZone addToZone:shareCKRecord];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self.keychainView dispatchSync:^bool {
        NSError* blockerror = nil;
        CKKSTLKShare* localshare = [CKKSTLKShare tryFromDatabaseFromCKRecordID:shareCKRecord.recordID error:&blockerror];
        XCTAssertNil(blockerror, "Shouldn't error finding TLKShare record in database");
        XCTAssertNotNil(localshare, "Should be able to find a TLKShare record in database");
        return true;
    }];

    // Delete the record in CloudKit...
    [self.keychainZone deleteCKRecordIDFromZone:shareCKRecord.recordID];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Should be gone now.
    [self.keychainView dispatchSync:^bool {
        NSError* blockerror = nil;
        CKKSTLKShare* localshare = [CKKSTLKShare tryFromDatabaseFromCKRecordID:shareCKRecord.recordID error:&blockerror];

        XCTAssertNil(blockerror, "Shouldn't error trying to find non-existent TLKShare record in database");
        XCTAssertNil(localshare, "Shouldn't be able to find a TLKShare record in database");

        return true;
    }];
}

- (void)testReceiveSharedTLKWhileInWaitForTLK {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database, but it should enter waitfortlk
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should become waitfortlk");

    // peer1 arrives to save the day
    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

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

- (void)testReceiveTLKShareWhileLocked {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // Because 33710924 didn't make it backwards in time, this test is fragile on Chipmunk/Cinar.
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database, but it should enter waitforunlock
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForUnlock] wait:10*NSEC_PER_SEC], "Key state should become waitforunlock");

    // Now unlock things. We expect a TLKShare upload.
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testUploadTLKSharesForExistingHierarchy {
    // Test starts with key material locally and in CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testUploadTLKSharesForExistingHierarchyOnRestart {
    // Turn off TLK sharing, and get situated
    SecCKKSSetShareTLKs(false);

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

    // Turn TLK sharing back on, and restart. We expect an upload of 3 TLK shares.
    SecCKKSSetShareTLKs(true);
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];
    self.keychainView = [self.injectedManager restartZone: self.keychainZoneID.zoneName];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testHandleExternalSharedTLKRoll {
    // Test starts with key material locally and in CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now the external peer rolls the TLK and updates the shares
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // CKKS will share the TLK back to itself
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    // Trigger a notification
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testUploadTLKSharesForExternalTLKRollWithoutShares {
    // Test starts with key material locally and in CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now, an old (Tigris) peer rolls the TLK, but doesn't share it
    // CKKS should get excited and throw 3 new share records up
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];

    // Wait for that modification to finish before changing CK data
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Trigger a notification
    [self.keychainView notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testRecoverFromTLKShareUploadFailure {
    // Test starts with key material locally and in CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    __weak __typeof(self) weakSelf = self;
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:^{
        __strong __typeof(self) strongSelf = weakSelf;
        [strongSelf expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];
    }];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testFillInMissingPeerShares {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to just the local peer from peer1
    // We expect the local peer to send it to peer2
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

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

- (void)testDontAcceptTLKFromUntrustedPeer {
    // Test starts with nothing in database, but key hierarchy in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the key hierarchy shared from a non-trusted peer
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.untrustedPeer zoneID:self.keychainZoneID];

    // The CKKS subsystem should go into waitfortlk, since it doesn't trust this peer
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testAcceptSharedTLKOnTrustSetAdditionOfSharer {
    // Test starts with nothing in database, but key hierarchy in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the key hierarchy shared from a non-trusted peer
    // note that it would share it itself too
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.untrustedPeer zoneID:self.keychainZoneID];
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:self.untrustedPeer to:self.untrustedPeer zoneID:self.keychainZoneID];

    // The CKKS subsystem should go into waitfortlk, since it doesn't trust this peer
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should become waitfortlk");

    // Wait to be sure we really get into that state
    [self.keychainView waitForOperationsOfClass:[CKKSProcessReceivedKeysOperation class]];

    // Now, trust the previously-untrusted peer
    [self.currentPeers addObject: self.untrustedPeer];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    // The CKKS subsystem should now accept the key, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:500*NSEC_PER_SEC], "Key state should become ready");

    // And use it as well
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testSendNewTLKSharesOnTrustSetAddition {
    // step 1: add a new peer; we should share the TLK with them
    // start with no trusted peers
    [self.currentPeers removeAllObjects];

    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Cool! New peer arrives!
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self.currentPeers addObject:self.remotePeer1];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // step 2: add a new peer who already has a share; no share should be created
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 to:self.remotePeer2 zoneID:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // CKKS should not upload a tlk share for this peer
    [self.currentPeers addObject:self.remotePeer2];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    [self.keychainView waitUntilAllOperationsAreFinished];
}

- (void)testSendNewTLKSharesOnTrustSetRemoval {
    // Not implemented. Trust set removal demands a key roll, but let's not get ahead of ourselves...
}


@end

#endif // OCTAGON
