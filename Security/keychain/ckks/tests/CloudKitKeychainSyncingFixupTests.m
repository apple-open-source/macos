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
    [[[mockFixups reject] ignoringNonObjectArgs] fixup:0 for:[OCMArg any]];

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [mockFixups verify];
    [mockFixups stopMocking];
}

- (void)testImmediateRestartUsesLatestFixup {
    id mockFixups = OCMClassMock([CKKSFixups class]);
    OCMExpect([mockFixups fixup:CKKSCurrentFixupNumber for:[OCMArg any]]);

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Tear down the CKKS object
    [self.keychainView halt];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];
    [self.keychainView waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [mockFixups verify];
    [mockFixups stopMocking];
}

- (void)testFixupRefetchAllCurrentItemPointers {
    // Due to <rdar://problem/34916549> CKKS: current item pointer CKRecord resurrection,
    // CKKS needs to refetch all current item pointers if it restarts and hasn't yet.

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Add some current item pointers. They don't necessarily need to point to anything...

    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                    currentItemUUID:@"50184A35-4480-E8BA-769B-567CF72F1EC0"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord = self.keychainZone.currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found record in CloudKit at expected UUID");

    CKKSCurrentItemPointer* cip2 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice2"
                                                                     currentItemUUID:@"10E76B80-CE1C-A52A-B0CB-462A2EBA05AF"
                                                                               state:SecCKKSProcessedStateRemote
                                                                              zoneID:self.keychainZoneID
                                                                     encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip2 CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID2 = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice2" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord2 = self.keychainZone.currentDatabase[currentPointerRecordID2];
    XCTAssertNotNil(currentPointerRecord2, "Found record in CloudKit at expected UUID");

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Tear down the CKKS object
    [self.keychainView halt];

    [self.keychainView dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
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
                                                                         currentItemUUID:@"10E76B80-CE1C-A52A-B0CB-462A2EBA05AF"
                                                                                   state:SecCKKSProcessedStateLocal
                                                                                  zoneID:self.keychainZoneID
                                                                         encodedCKRecord:nil];
        cip3.storedCKRecord = [cip3 CKRecordWithZoneID:self.keychainZoneID];
        XCTAssertEqual(cip3.identifier, @"garbage", "Identifier is what we thought it was");
        [cip3 saveToDatabase:&error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    self.silentFetchesAllowed = false;
    [self expectCKFetchByRecordID];
    [self expectCKFetchByQuery]; // and one for the TLKShare fixup

    // Change one of the CIPs while CKKS is offline
    cip2.currentItemUUID = @"changed-by-cloudkit";
    [self.keychainZone addToZone: [cip2 CKRecordWithZoneID:self.keychainZoneID]];

    // Bring CKKS back up
    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView dispatchSyncWithReadOnlySQLTransaction:^{
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
    }];
}

- (void)setFixupNumber:(CKKSFixup)newFixup ckks:(CKKSKeychainView*)ckks {
    [ckks dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        // Edit the zone state entry to have no fixups
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:ckks.zoneID.zoneName error:&error];

        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastFixup = newFixup;
        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)testFixupFetchAllTLKShareRecords {
    // In <rdar://problem/34901306> CKKSTLK: TLKShare CloudKit upload/download on TLK change, trust set addition,
    // we added the TLKShare CKRecord type. Upgrading devices must fetch all such records when they come online for the first time.

    // Note that this already does TLK sharing, and so technically doesn't need to do the fixup, but we'll fix that later.
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    // Tear down the CKKS object
    [self.keychainView halt];
    [self setFixupNumber:CKKSFixupRefetchCurrentItemPointers ckks:self.keychainView];

    // Also, create a TLK share record that CKKS should find
    // Make another share, but from an untrusted peer to some other peer. local shouldn't necessarily care.
    NSError* error = nil;

    CKKSSOSSelfPeer* remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                                encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                   signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                     viewList:self.managedViewList];

    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:self.keychainZoneKeys.tlk
                                           as:remotePeer1
                                           to:self.mockSOSAdapter.selfPeer
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
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForFixupOperation] wait:20*NSEC_PER_SEC], "Key state should become waitforfixup");

    [self releaseCloudKitFetchHold];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView.lastFixupOperation waitUntilFinished];
    XCTAssertNil(self.keychainView.lastFixupOperation.error, "Shouldn't have been any error performing fixup");

    // and check that the share made it
    [self.keychainView dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* blockerror = nil;
        CKKSTLKShareRecord* localshare = [CKKSTLKShareRecord tryFromDatabaseFromCKRecordID:shareCKRecord.recordID error:&blockerror];
        XCTAssertNil(blockerror, "Shouldn't error finding new TLKShare record in database");
        XCTAssertNotNil(localshare, "Should be able to find a new TLKShare record in database");
    }];
}

- (void)testFixupLocalReload {
    // In <rdar://problem/35540228> Server Generated CloudKit "Manatee Identity Lost"
    // items could be deleted from the local keychain after CKKS believed they were already synced, and therefore wouldn't resync

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self addGenericPassword: @"data" account: @"first"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    XCTAssertNotNil(secondRecordID, "Should have filled in secondRecordID");
    XCTAssertEqual(0, [secondRecordIDFilled wait:20*NSEC_PER_SEC], "Should have filled in secondRecordID within enough time");

    // Tear down the CKKS object
    [self.keychainView halt];
    [self setFixupNumber:CKKSFixupFetchTLKShares ckks:self.keychainView];

    // Delete items from keychain
    [self deleteGenericPassword:@"first"];

    // Corrupt the second item's CKMirror entry to only contain system fields in the CKRecord portion (to emulate early CKKS behavior)
    [self.keychainView dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:secondRecordID.recordName zoneID:self.keychainZoneID error:&error];
        XCTAssertNil(error, "Should have no error pulling second CKKSMirrorEntry from database");

        NSKeyedArchiver *archiver = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
        [ckme.item.storedCKRecord encodeSystemFieldsWithCoder:archiver];
        ckme.item.encodedCKRecord = archiver.encodedData;

        [ckme saveToDatabase:&error];
        XCTAssertNil(error, "No error saving system-fielded CKME back to database");
        return CKKSDatabaseTransactionCommit;
    }];

    // Now, restart CKKS, but place a hold on the fixup operation
    self.silentFetchesAllowed = false;
    self.accountStatus = CKAccountStatusCouldNotDetermine;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];
    self.keychainView.holdFixupOperation = [CKKSResultOperation named:@"hold-fixup" withBlock:^{}];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForFixupOperation] wait:20*NSEC_PER_SEC], "Key state should become waitforfixup");
    [self.operationQueue addOperation: self.keychainView.holdFixupOperation];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    [self.keychainView.lastFixupOperation waitUntilFinished];
    XCTAssertNil(self.keychainView.lastFixupOperation.error, "Shouldn't have been any error performing fixup");

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    // And the item should be back!
    [self checkGenericPassword: @"data" account: @"first"];
}

- (void)testFixupResaveDeviceStateEntries {
    // In <rdar://problem/50612776>, we introduced a new field to DeviceStateEntries. But, Peace couldn't change the DB schema to match newer Yukons
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeOctagonOnlyDeviceStatusInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];


    // Modify the sqlite DB to simulate how earlier verions would save these records
    [self.keychainView dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry fromDatabase:self.remoteSOSOnlyPeer.peerID zoneID:self.keychainZoneID error:&error];
        XCTAssertNil(error, "Should have no error pulling CKKSDeviceStateEntry from database");

        XCTAssertNotNil(cdse.octagonPeerID, "CDSE should have an octagon peer ID");
        XCTAssertNotNil(cdse.octagonStatus, "CDSE should have an octagon status");
        cdse.octagonPeerID = nil;
        cdse.octagonStatus = nil;

        [cdse saveToDatabase:&error];
        XCTAssertNil(error, "No error saving modified CDSE back to database");
        return CKKSDatabaseTransactionCommit;
    }];

    // Tear down the CKKS object
    [self.keychainView halt];
    [self setFixupNumber:CKKSFixupFetchTLKShares ckks:self.keychainView];

    // Now, restart CKKS
    self.silentFetchesAllowed = false;
    self.accountStatus = CKAccountStatusCouldNotDetermine;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    [self.keychainView.lastFixupOperation waitUntilFinished];
    XCTAssertNil(self.keychainView.lastFixupOperation.error, "Shouldn't have been any error performing fixup");

    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    // And all CDSEs should have an octagon peer ID again!
    [self.keychainView dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry fromDatabase:self.remoteSOSOnlyPeer.peerID zoneID:self.keychainZoneID error:&error];
        XCTAssertNil(error, "Should have no error pulling CKKSDeviceStateEntry from database");

        XCTAssertNotNil(cdse.octagonPeerID, "CDSE should have an octagon peer ID");
        XCTAssertNotNil(cdse.octagonStatus, "CDSE should have an octagon status");
    }];
}

- (void)testFixupDeletesTombstoneEntries {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // The CKKS stack now rejects tombstone items. So, let's inject one out of band.

    [self.keychainView halt];

    [self setFixupNumber:CKKSFixupResaveDeviceStateEntries ckks:self.keychainView];

    CKRecord* ckr = [self createFakeTombstoneRecord:self.keychainZoneID
                                         recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"
                                            account:@"account-delete-me"];
    [self.keychainZone addToZone:ckr];

    [self.keychainView dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        CKKSMirrorEntry* ckme = [[CKKSMirrorEntry alloc] initWithCKRecord:ckr];
        NSError* error = nil;
        [ckme saveToDatabase:&error];

        XCTAssertNil(error, "Should be no error saving the CKME to the database");
        return CKKSDatabaseTransactionCommit;
    }];

    // Now, restart CKKS. The bad record should be deleted.
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];

    self.keychainView = [[CKKSViewManager manager] restartZone:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.keychainView];

    // Deletions should occur
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.keychainView.lastFixupOperation waitUntilFinished];
    XCTAssertNil(self.keychainView.lastFixupOperation.error, "Shouldn't have been any error performing fixup");

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];
}

@end

#endif // OCTAGON
