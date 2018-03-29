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
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFDigestOperation.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ot/OTDefines.h"

@interface CloudKitKeychainSyncingTLKSharingTests : CloudKitKeychainSyncingTestsBase
@property CKKSSOSSelfPeer* remotePeer1;
@property CKKSSOSPeer* remotePeer2;


@property CKKSSOSSelfPeer* untrustedPeer;

@property (nullable) NSMutableSet<id<CKKSSelfPeer>>* pastSelfPeers;

// Used to test a single code path. If true, no past self peers will be valid
@property bool breakLoadSelfPeerEncryptionKey;
@end

@implementation CloudKitKeychainSyncingTLKSharingTests

- (void)setUp {
    [super setUp];

    self.pastSelfPeers = [NSMutableSet set];

    // Use the upsetting old-style mocks so we can ignore the enum
    [[[[self.mockCKKSViewManager stub] andCall:@selector(fakeLoadRestoredBottledKeysOfType:error:)
                                      onObject:self] ignoringNonObjectArgs]
     loadRestoredBottledKeysOfType:0 error:[OCMArg anyObjectRef]];

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
    self.pastSelfPeers = nil;
    self.remotePeer1 = nil;
    self.remotePeer2 = nil;
    self.untrustedPeer = nil;

    [super tearDown];
}


- (NSArray<NSDictionary *>* _Nullable)fakeLoadRestoredBottledKeysOfType:(OctagonKeyType)keyType error:(NSError**)error {
    if(self.aksLockState) {
        if(error) {
            *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecInteractionNotAllowed userInfo:nil];
        }
        return nil;
    } else {
        if(self.breakLoadSelfPeerEncryptionKey && keyType == OctagonEncryptionKey) {
            if(error) {
                *error = [NSError errorWithDomain:(__bridge NSString*)kSecErrorDomain code:errSecItemNotFound userInfo:nil];
            }
            return nil;
        }

        // Convert self.pastSelfPeers into an array of dictionaries
        NSMutableArray<NSDictionary*>* keys = [NSMutableArray array];

        for(id<CKKSSelfPeer> peer in self.pastSelfPeers) {
            SFECKeyPair* key = nil;

            switch(keyType) {
                case OctagonSigningKey:
                    key = peer.signingKey;
                    break;
                case OctagonEncryptionKey:
                    key = peer.encryptionKey;
                    break;
            }

            XCTAssertNotNil(key, "Should have a key at this point");

            NSData* signingPublicKeyHashBytes = [SFSHA384DigestOperation digest:peer.signingKey.publicKey.keyData];
            NSString* signingPublicKeyHash = [signingPublicKeyHashBytes base64EncodedStringWithOptions:0];

            NSDictionary* dict = @{
                                   (id)kSecAttrAccount : peer.peerID,
                                   (id)kSecAttrLabel : signingPublicKeyHash,
                                   (id)kSecValueData : key.keyData,
                                   };
            [keys addObject:dict];
        }

        return keys;
    }
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

- (void)testAcceptExistingTLKSharedKeyHierarchyForPastSelf {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // Self rolls its keys and ID...
    [self.pastSelfPeers addObject:self.currentSelfPeer];
    self.currentSelfPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"new-local-peer"
                                                        encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                           signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];

    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.currentSelfPeer.peerID, "Receiver peerID on TLKShare should match current self");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.currentSelfPeer.publicEncryptionKey, "Receiver encryption key on TLKShare should match current self");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   return TRUE;
               }];
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

- (void)testDontCrashOnHalfBottle {
    self.breakLoadSelfPeerEncryptionKey = true;

    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // Self rolls its keys and ID...
    [self.pastSelfPeers addObject:self.currentSelfPeer];
    self.currentSelfPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"new-local-peer"
                                                        encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                           signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];

    // CKKS should enter 'waitfortlk' without crashing
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:10*NSEC_PER_SEC], "Key state should become waitfortlk");
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
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

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
    // Bring up CKKS. It'll upload a few TLK Shares, but we'll delete them to get into state
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Now, delete all the TLK Shares, so CKKS will upload them again
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        [CKKSTLKShare deleteAll:self.keychainZoneID error:&error];
        XCTAssertNil(error, "Shouldn't be an error deleting all TLKShares");

        NSArray<CKRecord*>* records = [self.zones[self.keychainZoneID].currentDatabase allValues];
        for(CKRecord* record in records) {
            if([record.recordType isEqualToString:SecCKRecordTLKShareType]) {
                [self.zones[self.keychainZoneID] deleteFromHistory:record.recordID];
            }
        }

        return true;
    }];

    // Restart. We expect an upload of 3 TLK shares.
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
    [self waitForCKModifications];

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
    // The remote peer should also have given the TLK to a non-TLKShare peer (which is also offline)
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Test also starts with the key hierarchy shared from a non-trusted peer
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.untrustedPeer zoneID:self.keychainZoneID];

    // The CKKS subsystem should go into waitfortlk, since it doesn't trust this peer, but the peer is active
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testAcceptSharedTLKOnTrustSetAdditionOfSharer {
    // Test starts with nothing in database, but key hierarchy in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Test also starts with the key hierarchy shared from a non-trusted peer
    // note that it would share it itself too
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.untrustedPeer zoneID:self.keychainZoneID];
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:self.untrustedPeer to:self.untrustedPeer zoneID:self.keychainZoneID];

    // The CKKS subsystem should go into waitfortlk, since it doesn't trust this peer, but the peer is active
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should become waitfortlk");

    // Wait to be sure we really get into that state
    [self.keychainView waitForOperationsOfClass:[CKKSProcessReceivedKeysOperation class]];

    // Now, trust the previously-untrusted peer
    [self.currentPeers addObject: self.untrustedPeer];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    // The CKKS subsystem should now accept the key, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], "Key state should become ready");

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

- (void)testFillInMissingPeerSharesAfterUnlock {
    // step 1: add a new peer; we should share the TLK with them
    // start with no trusted peers
    [self.currentPeers removeAllObjects];

    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], @"Key state should become 'ready'");

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now, lock.
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    // New peer arrives! This can't actually happen (since we have to be unlocked to accept a new peer), but this will exercise CKKS
    [self.currentPeers addObject:self.remotePeer1];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    // CKKS should notice that it has things to do...
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReadyPendingUnlock] wait:8*NSEC_PER_SEC], @"Key state should become 'readypendingunlock'");

    // And do them.
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // and return to ready
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], @"Key state should become 'ready'");
}

- (void)testAddItemDuringNewTLKSharesOnTrustSetAddition {
    // step 1: add a new peer; we should share the TLK with them
    // start with no trusted peers
    [self.currentPeers removeAllObjects];

    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Hold the TLK share modification
    [self holdCloudKitModifications];

    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self.currentPeers addObject:self.remotePeer1];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // While CloudKit is hanging the write, add an item
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    // After that returns, release the write. CKKS should upload the new item
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self releaseCloudKitModificationHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testSendNewTLKSharesOnTrustSetRemoval {
    // Not implemented. Trust set removal demands a key roll, but let's not get ahead of ourselves...
}

- (void)testWaitForTLKWithMissingKeys {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // self no longer has that key pair, but it does have a new one with the same peer ID....
    self.currentSelfPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:self.currentSelfPeer.peerID
                                                        encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                           signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];
    self.pastSelfPeers = [NSMutableSet set];

    // CKKS should become very upset, and enter waitfortlk.
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:4000*NSEC_PER_SEC], "Key state should become waitfortlk");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testSendNewTLKShareToPeerOnPeerEncryptionKeyChange {
    // If a peer changes its keys, CKKS should send it a new TLK share with the right keys
    // This recovers from the remote peer losing its Octagon keys and making new ones

    // step 1: add a new peer; we should share the TLK with them
    // start with no trusted peers
    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:1000*NSEC_PER_SEC], "Key state should become ready");

    // Remote peer rolls its encryption key...
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.remotePeer1.peerID, "Receiver peerID on TLKShare should match remote peer");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.remotePeer1.publicEncryptionKey, "Receiver encryption key on TLKShare should match remote peer");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   return TRUE;
               }];

    self.remotePeer1.encryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testRecoverFromBrokenSignatureOnTLKShareDuetoSignatureKeyChange {
    // If a peer changes its signature key, CKKS shouldn't necessarily enter 'error': it should enter 'waitfortlk'.
    // The peer should then send us another TLKShare
    // This recovers from the remote peer losing its Octagon keys and making new ones

    // For this test, only have one peer
    self.currentPeers = [NSMutableSet setWithObject:self.remotePeer1];

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Test also starts with the TLK shared to all trusted peers from remotePeer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];

    // BUT, remotePeer1 has rolled its signing key
    self.remotePeer1.signingKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:10*NSEC_PER_SEC], "Key state should become waitfortlk");

    // Remote peer discovers its error and sends a new TLKShare! CKKS should recover and share itself a TLKShare
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.currentSelfPeer.peerID, "Receiver peerID on TLKShare should match self peer");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.currentSelfPeer.publicEncryptionKey, "Receiver encryption key on TLKShare should match self peer");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   return TRUE;
               }];

    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
}

- (void)testSendNewTLKShareToSelfOnPeerSigningKeyChange {
    // If a CKKS peer rolls its own keys, but has the TLK, it should write a new TLK share to itself with its new Octagon keys
    // This recovers from the local peer losing its Octagon keys and making new ones

    // For this test, only have one peer
    self.currentPeers = [NSMutableSet setWithObject:self.remotePeer1];

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:100*NSEC_PER_SEC], "Key state should become ready");

    // Remote peer rolls its signing key, but hasn't updated its TLKShare. We should send it one.
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.remotePeer1.peerID, "Receiver peerID on TLKShare should match remote peer");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.remotePeer1.publicEncryptionKey, "Receiver encryption key on TLKShare should match remote peer");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   return TRUE;
               }];

    self.remotePeer1.signingKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testSendNewTLKShareToPeerOnDisappearanceOfPeerKeys {
    // If a CKKS peer deletes its own octagon keys (BUT WHY), local CKKS should be able to respond

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:100*NSEC_PER_SEC], "Key state should become ready");

    // Now, peer 1 updates its keys (to be nil). Local peer should re-send TLKShares to peer2.

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.remotePeer2.peerID, "Receiver peerID on TLKShare should match remote peer");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.remotePeer2.publicEncryptionKey, "Receiver encryption key on TLKShare should match remote peer");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   return TRUE;
               }];

    CKKSSOSPeer* brokenRemotePeer1 = [[CKKSSOSPeer alloc] initWithSOSPeerID:self.remotePeer1.peerID encryptionPublicKey:nil signingPublicKey:nil];
    [self.currentPeers removeObject:self.remotePeer1];
    [self.currentPeers addObject:brokenRemotePeer1];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testSendNewTLKShareToPeerOnDisappearanceOfPeerSigningKey {
    // If a CKKS peer rolls its own keys, but has the TLK, it should write a new TLK share to itself with its new Octagon keys
    // This recovers from the local peer losing its Octagon keys and making new ones

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:100*NSEC_PER_SEC], "Key state should become ready");

    // Now, peer 1 updates its signing key (to be nil). Local peer should re-send TLKShares to peer1 and peer2.
    // Both should be sent because both peers don't have a signed TLKShare that gives them the TLK

    XCTestExpectation *peer1Share = [self expectationWithDescription:@"share uploaded for peer1"];
    XCTestExpectation *peer2Share = [self expectationWithDescription:@"share uploaded for peer2"];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:2 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   if([share.receiver.peerID isEqualToString:self.remotePeer1.peerID]) {
                       [peer1Share fulfill];
                       XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.remotePeer1.publicEncryptionKey, "Receiver encryption key on TLKShare should match remote peer1");
                   }
                   if([share.receiver.peerID isEqualToString:self.remotePeer2.peerID]) {
                       [peer2Share fulfill];
                       XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.remotePeer2.publicEncryptionKey, "Receiver encryption key on TLKShare should match remote peer2");
                   }

                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   return TRUE;
               }];

    CKKSSOSPeer* brokenRemotePeer1 = [[CKKSSOSPeer alloc] initWithSOSPeerID:self.remotePeer1.peerID
                                                        encryptionPublicKey:self.remotePeer1.publicEncryptionKey
                                                           signingPublicKey:nil];
    [self.currentPeers removeObject:self.remotePeer1];
    [self.currentPeers addObject:brokenRemotePeer1];
    [self.injectedManager sendTrustedPeerSetChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
    [self waitForExpectations:@[peer1Share, peer2Share] timeout:5];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testSendNewTLKShareToSelfOnSelfKeyChanges {
    // If a CKKS peer rolls its own keys, but has the TLK, it should write a new TLK share to itself with its new Octagon keys
    // This recovers from the local peer losing its Octagon keys and making new ones

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

    // Local peer rolls its encryption key (and loses the old ones)
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.currentSelfPeer.peerID, "Receiver peerID on TLKShare should match current self");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.currentSelfPeer.publicEncryptionKey, "Receiver encryption key on TLKShare should match current self");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   NSError* signatureVerifyError = nil;
                   XCTAssertTrue([share verifySignature:share.signature verifyingPeer:self.currentSelfPeer error:&signatureVerifyError], "New share's signature should verify");
                   XCTAssertNil(signatureVerifyError, "Should be no error verifying signature on new TLKShare");
                   return TRUE;
               }];

    self.currentSelfPeer.encryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    self.pastSelfPeers = [NSMutableSet set];
    [self.injectedManager sendSelfPeerChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");

    // Now, local peer loses and rolls its signing key (and loses the old one)
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID
               checkModifiedRecord:^BOOL(CKRecord* _Nonnull record) {
                   CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
                   XCTAssertEqualObjects(share.receiver.peerID, self.currentSelfPeer.peerID, "Receiver peerID on TLKShare should match current self");
                   XCTAssertEqualObjects(share.receiver.publicEncryptionKey, self.currentSelfPeer.publicEncryptionKey, "Receiver encryption key on TLKShare should match current self");
                   XCTAssertEqualObjects(share.senderPeerID, self.currentSelfPeer.peerID, "Sender of TLKShare should match current self");
                   NSError* signatureVerifyError = nil;
                   XCTAssertTrue([share verifySignature:share.signature verifyingPeer:self.currentSelfPeer error:&signatureVerifyError], "New share's signature should verify");
                   XCTAssertNil(signatureVerifyError, "Should be no error verifying signature on new TLKShare");
                   return TRUE;
               }];

    self.currentSelfPeer.signingKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    self.pastSelfPeers = [NSMutableSet set];
    [self.injectedManager sendSelfPeerChangedUpdate];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], "Key state should become ready");
}

- (void)testDoNotResetCloudKitZoneFromWaitForTLKDueToRecentTLKShare {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS shouldn't reset this zone, due to a recent TLK Share from a trusted peer (indicating the presence of TLKs)
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 to:self.remotePeer1 zoneID:self.keychainZoneID];

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

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], @"Key state should become 'waitfortlk'");

    XCTAssertTrue(self.keychainZone.flag, "Zone flag should not have been reset to false");
}

- (void)testDoNotResetCloudKitZoneFromWaitForTLKDueToVeryRecentUntrustedTLKShare {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS shouldn't reset this zone, due to a very recent (but untrusted) TLK Share. You can hit this getting a circle reset; the device with the TLKs will have a CFU.
    CKKSSOSSelfPeer* untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"untrusted-peer"
                                                                  encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                     signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:untrustedPeer to:untrustedPeer zoneID:self.keychainZoneID];

    NSDateComponents* offset = [[NSDateComponents alloc] init];
    [offset setDay:-2];
    NSDate* updateTime = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:[NSDate date] options:0];
    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType] || [record.recordType isEqualToString:SecCKRecordTLKShareType]) {
            record.creationDate = updateTime;
            record.modificationDate = updateTime;
        }
    }

    self.keychainZone.flag = true;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], @"Key state should become 'waitfortlk'");
    XCTAssertTrue(self.keychainZone.flag, "Zone flag should not have been reset to false");

    // And ensure it doesn't go on to 'reset'
    XCTAssertNotEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:100*NSEC_PER_MSEC], @"Key state should not become 'resetzone'");
}

- (void)testResetCloudKitZoneFromWaitForTLKDueToUntustedTLKShareNotRecentEnough {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS shouldn't reset this zone, due to a recent TLK Share (indicating the presence of TLKs)
    CKKSSOSSelfPeer* untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"untrusted-peer"
                                                                  encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                     signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:untrustedPeer to:untrustedPeer zoneID:self.keychainZoneID];

    NSDateComponents* offset = [[NSDateComponents alloc] init];
    [offset setDay:-5];
    NSDate* updateTime = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:[NSDate date] options:0];
    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType] || [record.recordType isEqualToString:SecCKRecordTLKShareType]) {
            record.creationDate = updateTime;
            record.modificationDate = updateTime;
        }
    }

    self.silentZoneDeletesAllowed = true;
    self.keychainZone.flag = true;
    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:8*NSEC_PER_SEC], @"Key state should become 'resetzone'");

    // Then we should reset.
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], @"Key state should become 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testNoSelfEncryptionKeys {
    // If you lose your local encryption keys, CKKS should do something reasonable

    // Test also starts with the TLK shared to all trusted peers from peer1
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:self.remotePeer1 zoneID:self.keychainZoneID];
    [self saveTLKSharesInLocalDatabase:self.keychainZoneID];

    // But, we lost our local keys :(
    id<CKKSSelfPeer> oldSelfPeer = self.currentSelfPeer;

    self.currentSelfPeer = nil;
    self.currentSelfPeerError = [NSError errorWithDomain:NSOSStatusErrorDomain code:errSecParam description:@"injected test failure"];

    // CKKS subsystem should realize that it can't read the shares it has, and enter waitfortlk
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:10*NSEC_PER_SEC], "Key state should become 'waitfortlk'");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Fetching status should be quick
    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcStatus:@"keychain" reply:^(NSArray<NSDictionary*>* result, NSError* error) {
        XCTAssertNil(error, "should be no error fetching status for keychain");
        [callbackOccurs fulfill];
    }];
    [self waitForExpectations:@[callbackOccurs] timeout:1.0];

    // But, if by some miracle those keys come back, CKKS should be able to recover
    // It'll also upload itself a TLK share
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    self.currentSelfPeer = oldSelfPeer;
    self.currentSelfPeerError = nil;

    [self.injectedManager sendSelfPeerChangedUpdate];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:100*NSEC_PER_SEC], "Key state should become 'ready''");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
}

@end

#endif // OCTAGON
