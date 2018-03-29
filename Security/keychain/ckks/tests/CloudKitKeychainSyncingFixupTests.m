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
#import <Foundation/NSKeyedArchiver_Private.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSFixups.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSIncomingQueueOperation.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CKKSTests+API.h"


@interface CloudKitKeychainSyncingFixupTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainSyncingFixupTests

- (void)testNoFixupOnInitialStart {
    id mockFixups = OCMClassMock([CKKSFixups class]);
    OCMReject([[[mockFixups stub] ignoringNonObjectArgs] fixup:0 for:[OCMArg any]]);

    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [mockFixups verify];
    [mockFixups stopMocking];
}

- (void)testImmediateRestartUsesLatestFixup {
    id mockFixups = OCMClassMock([CKKSFixups class]);
    OCMExpect([mockFixups fixup:CKKSCurrentFixupNumber for:[OCMArg any]]);

    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Tear down the CKKS object
    [self.keychainView halt];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [mockFixups verify];
    [mockFixups stopMocking];
}

- (void)testFixupRefetchAllCurrentItemPointers {
    // Due to <rdar://problem/34916549> CKKS: current item pointer CKRecord resurrection,
    // CKKS needs to refetch all current item pointers if it restarts and hasn't yet.

    // Test starts with no keys in database. We expect some sort of TLK/key hierarchy upload.
    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Add some current item pointers. They don't necessarily need to point to anything...

    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                    currentItemUUID:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord = self.keychainZone.currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found record in CloudKit at expected UUID");

    CKKSCurrentItemPointer* cip2 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice2"
                                                                     currentItemUUID:@"3AB8E78D-75AF-CFEF-F833-FA3E3E90978A"
                                                                               state:SecCKKSProcessedStateRemote
                                                                              zoneID:self.keychainZoneID
                                                                     encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip2 CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID2 = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice2" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord2 = self.keychainZone.currentDatabase[currentPointerRecordID2];
    XCTAssertNotNil(currentPointerRecord2, "Found record in CloudKit at expected UUID");

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Tear down the CKKS object
    [self.keychainView halt];

    [self.keychainView dispatchSync: ^bool {
        // Edit the zone state entry to have no fixups
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];

        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastFixup = CKKSFixupNever;
        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "no error saving to database");

        // And add a garbage CIP
        CKKSCurrentItemPointer* cip3 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"garbage"
                                                                         currentItemUUID:@"3AB8E78D-75AF-CFEF-F833-FA3E3E90978A"
                                                                                   state:SecCKKSProcessedStateLocal
                                                                                  zoneID:self.keychainZoneID
                                                                         encodedCKRecord:nil];
        cip3.storedCKRecord = [cip3 CKRecordWithZoneID:self.keychainZoneID];
        XCTAssertEqual(cip3.identifier, @"garbage", "Identifier is what we thought it was");
        [cip3 saveToDatabase:&error];
        XCTAssertNil(error, "no error saving to database");
        return true;
    }];

    self.silentFetchesAllowed = false;
    [self expectCKFetchByRecordID];
    [self expectCKFetchByQuery]; // and one for the TLKShare fixup

    // Change one of the CIPs while CKKS is offline
    cip2.currentItemUUID = @"changed-by-cloudkit";
    [self.keychainZone addToZone: [cip2 CKRecordWithZoneID:self.keychainZoneID]];

    // Bring CKKS back up
    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self.keychainView dispatchSync: ^bool {
        // The zone state entry should be up the most recent fixup level
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];
        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");
        XCTAssertEqual(ckse.lastFixup, CKKSCurrentFixupNumber, "CKSE should have the current fixup number stored");

        // The garbage CIP should be gone, and CKKS should have caught up to the CIP change
        NSArray<CKKSCurrentItemPointer*>* allCIPs = [CKKSCurrentItemPointer allInZone:self.keychainZoneID error:&error];
        XCTAssertNil(error, "no error loading all CIPs from database");

        XCTestExpectation* foundCIP2 = [self expectationWithDescription: @"found CIP2"];
        for(CKKSCurrentItemPointer* loaded in allCIPs) {
            if([loaded.identifier isEqualToString: cip2.identifier]) {
                [foundCIP2 fulfill];
                XCTAssertEqualObjects(loaded.currentItemUUID, @"changed-by-cloudkit", "Fixup should have fixed UUID to new value, not pre-shutdown value");
            }
            XCTAssertNotEqualObjects(loaded.identifier, @"garbage", "Garbage CIP shouldn't exist anymore");
        }

        [self waitForExpectations:@[foundCIP2] timeout:0.1];
        return true;
    }];
}

- (void)setFixupNumber:(CKKSFixup)newFixup ckks:(CKKSKeychainView*)ckks {
    [ckks dispatchSync: ^bool {
        // Edit the zone state entry to have no fixups
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:ckks.zoneID.zoneName error:&error];

        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastFixup = newFixup;
        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "no error saving to database");
        return true;
    }];
}

- (void)testFixupFetchAllTLKShareRecords {
    // In <rdar://problem/34901306> CKKSTLK: TLKShare CloudKit upload/download on TLK change, trust set addition,
    // we added the TLKShare CKRecord type. Upgrading devices must fetch all such records when they come online for the first time.

    // Test starts with nothing in database. We expect some sort of TLK/key hierarchy upload.
    // Note that this already does TLK sharing, and so technically doesn't need to do the fixup, but we'll fix that later.
    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Tear down the CKKS object
    [self.keychainView halt];
    [self setFixupNumber:CKKSFixupRefetchCurrentItemPointers ckks:self.keychainView];

    // Also, create a TLK share record that CKKS should find
    // Make another share, but from an untrusted peer to some other peer. local shouldn't necessarily care.
    NSError* error = nil;

    CKKSSOSSelfPeer* remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                                encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                   signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];

    CKKSTLKShare* share = [CKKSTLKShare share:self.keychainZoneKeys.tlk
                                           as:remotePeer1
                                           to:self.currentSelfPeer
                                        epoch:-1
                                     poisoned:0
                                        error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");
    XCTAssertNotNil(share, "Should be able to create a share");

    CKRecord* shareCKRecord = [share CKRecordWithZoneID: self.keychainZoneID];
    XCTAssertNotNil(shareCKRecord, "Should have been able to create a CKRecord");
    [self.keychainZone addToZone:shareCKRecord];

    // Now, restart CKKS
    self.silentFetchesAllowed = false;
    [self expectCKFetchByQuery];

    // We want to ensure that the key hierarchy state machine doesn't progress past fixup until we let this go
    [self holdCloudKitFetches];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForFixupOperation] wait:8*NSEC_PER_SEC], "Key state should become waitforfixup");
    [self releaseCloudKitFetchHold];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], "Key state should become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self.keychainView.lastFixupOperation waitUntilFinished];
    XCTAssertNil(self.keychainView.lastFixupOperation.error, "Shouldn't have been any error performing fixup");

    // and check that the share made it
    [self.keychainView dispatchSync:^bool {
        NSError* blockerror = nil;
        CKKSTLKShare* localshare = [CKKSTLKShare tryFromDatabaseFromCKRecordID:shareCKRecord.recordID error:&blockerror];
        XCTAssertNil(blockerror, "Shouldn't error finding new TLKShare record in database");
        XCTAssertNotNil(localshare, "Should be able to find a new TLKShare record in database");
        return true;
    }];
}

- (void)testFixupLocalReload {
    // In <rdar://problem/35540228> Server Generated CloudKit "Manatee Identity Lost"
    // items could be deleted from the local keychain after CKKS believed they were already synced, and therefore wouldn't resync

    [self expectCKModifyKeyRecords:3 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], @"Key state should become 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    [self addGenericPassword: @"data" account: @"first"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Add another record to mock up early CKKS record saving
    __block CKRecordID* secondRecordID = nil;
    CKKSCondition* secondRecordIDFilled = [[CKKSCondition alloc] init];
    [self addGenericPassword: @"data" account: @"second"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        secondRecordID = record.recordID;
        [secondRecordIDFilled fulfill];
        return TRUE;
    }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
    XCTAssertNotNil(secondRecordID, "Should have filled in secondRecordID");
    XCTAssertEqual(0, [secondRecordIDFilled wait:8*NSEC_PER_SEC], "Should have filled in secondRecordID within enough time");

    // Tear down the CKKS object
    [self.keychainView halt];
    [self setFixupNumber:CKKSFixupFetchTLKShares ckks:self.keychainView];

    // Delete items from keychain
    [self deleteGenericPassword:@"first"];

    // Corrupt the second item's CKMirror entry to only contain system fields in the CKRecord portion (to emulate early CKKS behavior)
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:secondRecordID.recordName zoneID:self.keychainZoneID error:&error];
        XCTAssertNil(error, "Should have no error pulling second CKKSMirrorEntry from database");

        NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
        [ckme.item.storedCKRecord encodeSystemFieldsWithCoder:archiver];
        ckme.item.encodedCKRecord = archiver.encodedData;

        [ckme saveToDatabase:&error];
        XCTAssertNil(error, "No error saving system-fielded CKME back to database");
        return true;
    }];

    // Now, restart CKKS, but place a hold on the fixup operation
    self.silentFetchesAllowed = false;
    self.accountStatus = CKAccountStatusCouldNotDetermine;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    self.keychainView.holdFixupOperation = [CKKSResultOperation named:@"hold-fixup" withBlock:^{}];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForFixupOperation] wait:8*NSEC_PER_SEC], "Key state should become waitforfixup");
    [self.operationQueue addOperation: self.keychainView.holdFixupOperation];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], "Key state should become ready");

    [self.keychainView.lastFixupOperation waitUntilFinished];
    XCTAssertNil(self.keychainView.lastFixupOperation.error, "Shouldn't have been any error performing fixup");

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    // And the item should be back!
    [self checkGenericPassword: @"data" account: @"first"];
}

@end

#endif // OCTAGON
