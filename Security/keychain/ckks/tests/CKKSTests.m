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

#import "CKKSTests.h"
#import <CloudKit/CloudKit.h>
#import <Foundation/NSXPCConnection_Private.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#include <Security/SecItemPriv.h>
#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemServer.h"
#include <utilities/SecFileLocations.h>
#include "keychain/SecureObjectSync/SOSInternal.h"

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSControlProtocol.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CKKSPeer.h"

#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ot/ObjCImprovements.h"
#import <utilities/SecCoreAnalytics.h>

// break abstraction
@interface CKKSLockStateTracker ()
@property (nullable) NSDate* lastUnlockedTime;
@end

@implementation CloudKitKeychainSyncingTests

#pragma mark - Tests

- (void)testBringupToKeyStateReady {
    // As part of the initial bringup, we should send these 'view ready' notifications
    XCTestExpectation* keychainReadyLibNotifyNotification = [self expectLibNotifyReadyForView:self.keychainZoneID.zoneName];
    XCTestExpectation* keychainReadyDNSNotification = [self expectNSNotificationReadyForView:self.keychainZoneID.zoneName];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self waitForExpectations:@[keychainReadyLibNotifyNotification, keychainReadyDNSNotification] timeout:20];
}

- (void)testActiveTLKs {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    NSError* localError = nil;
    NSArray<CKKSKeychainBackedKey*>* tlks = [[CKKSViewManager manager] currentTLKsFilteredByPolicy:NO error:&localError];
    XCTAssertNil(localError, "Should have no error fetching current TLKs");

    XCTAssertEqual([tlks count], (NSUInteger)1, "Should have one TLK");
    XCTAssertEqualObjects(tlks[0].zoneID.zoneName, @"keychain", "should have a TLK for keychain");

    XCTAssertEqualObjects(tlks[0].uuid, self.keychainZoneKeys.tlk.uuid, "should have the TLK matching cloudkit");
}

- (void)testActiveTLKsWhenMissing {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should have arrived at waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    NSError* localError = nil;
    NSArray<CKKSKeychainBackedKey*>* tlks = [[CKKSViewManager manager] currentTLKsFilteredByPolicy:NO error:&localError];
    XCTAssertNil(localError, "Should have no error fetching current TLKs");

    XCTAssertEqual([tlks count], (NSUInteger)0, "Should have zero TLKs");
}

- (void)testActiveTLKsWhenLocked {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    NSError* localError = nil;
    NSArray<CKKSKeychainBackedKey*>* tlks = [[CKKSViewManager manager] currentTLKsFilteredByPolicy:NO error:&localError];
    XCTAssertNotNil(localError, "Should have an error fetching current TLKs");

    XCTAssertEqual([tlks count], (NSUInteger)0, "Should have zero TLKs");
}

-(void)testReceiveRecordEncryptedv1 {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

    NSError* error = nil;

    // Manually encrypt an item
    NSString* recordName = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.keychainZoneID];
    NSDictionary* item = [self fakeRecordDictionary: @"account-delete-me" zoneID:self.keychainZoneID];
    CKKSItem* cipheritem = [[CKKSItem alloc] initWithUUID:recordID.recordName
                                            parentKeyUUID:self.keychainZoneKeys.classC.uuid
                                                   zoneID:recordID.zoneID];
    CKKSKeychainBackedKey* itemkey = [CKKSKeychainBackedKey randomKeyWrappedByParent:[self.keychainZoneKeys.classC getKeychainBackedKey:&error] error:&error];
    XCTAssertNotNil(itemkey, "Got a key");
    cipheritem.wrappedkey = itemkey.wrappedkey;
    XCTAssertNotNil(cipheritem.wrappedkey, "Got a wrapped key");

    cipheritem.encver = CKKSItemEncryptionVersion1;

    NSMutableDictionary<NSString*, NSData*>* authenticatedData = [[cipheritem makeAuthenticatedDataDictionaryUpdatingCKKSItem: nil encryptionVersion:cipheritem.encver] mutableCopy];

    cipheritem.encitem = [CKKSItemEncrypter encryptDictionary:item key:itemkey.aessivkey authenticatedData:authenticatedData error:&error];
    XCTAssertNil(error, "no error encrypting object");
    XCTAssertNotNil(cipheritem.encitem, "Recieved ciphertext");

    [self.keychainZone addToZone:[cipheritem CKRecordWithZoneID: recordID.zoneID]];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    NSDictionary* query = @{(id)kSecClass: (id)kSecClassGenericPassword,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrAccount: @"account-delete-me",
                            (id)kSecMatchLimit: (id)kSecMatchLimitOne,
                            };
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Found synced item");
    CFReleaseNull(cfresult);

    // Test that if this item is updated, it is encrypted in v2
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self updateGenericPassword:@"different password" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    CKRecord* newRecord = self.keychainZone.currentDatabase[recordID];
    XCTAssertEqualObjects(newRecord[SecCKRecordEncryptionVersionKey], [NSNumber numberWithInteger:(int) CKKSItemEncryptionVersion2], "Uploaded using encv2");
}

- (void)testUploadInitialKeyHierarchy {
    // Test starts with nothing in database. CKKS should get into the "please upload my keys" state, then Octagon should perform the upload

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testDoNotErrorIfNudgedWhileWaitingForTLKUpload {
    // Test starts with nothing in database. CKKS should get into the "please upload my keys" state, then Octagon should perform the upload

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    for(CKKSKeychainViewState* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation' (view %@)", view);
    }
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready')");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];

    // Now that we've kicked them all off, wait for them to resolve (and nudge each one, as if a key was saved)
    for(CKKSKeychainViewState* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation'");
    }

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    NSMutableArray<CKKSCondition*>* processStates = [NSMutableArray array];
    for(CKKSKeychainViewState* viewState in self.ckksViews) {
        [processStates addObject:viewState.keyHierarchyConditions[SecCKKSZoneKeyStateProcess]];
    }

    CKKSCondition* ckksViewProcess = self.defaultCKKS.stateConditions[CKKSStateProcessReceivedKeys];
    [self.defaultCKKS keyStateMachineRequestProcess];

    for(CKKSCondition* processState in processStates) {
         XCTAssertEqual(0, [processState wait:10*NSEC_PER_SEC], "CKKS key state machine should reprocess the key hierarchy when nudged");
    }
    XCTAssertEqual(0, [ckksViewProcess wait:10*NSEC_PER_SEC], "CKKS state machine should reprocess the key hierarchy when nudged");

    for(CKKSKeychainViewState* viewState in self.ckksViews) {
        // Since we do need to leave SecCKKSZoneKeyStateWaitForTLKCreation if a fetch occurs with new keys, make sure we do the right thing
        XCTAssertEqual(0, [viewState.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should re-enter 'waitfortlkcreation'");
    }

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // The views should remain in waitfortlkcreation, and not go through process into an error

    NSMutableArray<CKRecord*>* keyHierarchyRecords = [NSMutableArray array];

    // Wait until finished is usually a bad idea. We could rip this out into an operation if we'd like.
    [keysetOp waitUntilFinished];
    XCTAssertNil(keysetOp.error, "Should be no error fetching keyset from CKKS");

    NSArray<CKRecord*>* records = [self performOctagonKeySetWrites:keysetOp.keysets];
    [keyHierarchyRecords addObjectsFromArray:records];

    // Tell our views about our shiny new records!
    [self.defaultCKKS receiveTLKUploadRecords:keyHierarchyRecords];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}


- (void)testReceiveChangedKeySetFromWaitingForTLKUpload {
    // Test starts with nothing in database. CKKS should get into the "please upload my keys" state

    [self startCKKSSubsystem];

    // After each zone arrives in WaitForTLKCreation, new keys are uploaded
    for(CKKSKeychainViewState* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation' (view %@)", view);
    }
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    for(CKKSKeychainViewState* view in self.ckksViews) {
        [self putFakeKeyHierarchyInCloudKit:view.zoneID];
        [self putFakeDeviceStatusInCloudKit:view.zoneID];
    }

    // If we ask the zones for their keysets, they should return the local set ready for upload
    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keysetOps = [NSMutableArray array];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready')");
    [keysetOps addObject:[self.defaultCKKS findKeySets:NO]];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp in keysetOps) {
        [keysetOp waitUntilFinished];
        XCTAssertNil(keysetOp.error, "Should be no error fetching keyset from CKKS");

        for(CKRecordZoneID* zoneID in keysetOp.intendedZoneIDs) {
            CKKSCurrentKeySet* keyset = keysetOp.keysets[zoneID];
            XCTAssertNotNil(keyset, "Should have provided a keyset for %@", zoneID);

            ZoneKeys* zk = self.keys[zoneID];
            XCTAssertNotNil(zk, "Should have new zone keys for zone %@", zoneID);
            XCTAssertNotEqualObjects(keyset.currentTLKPointer.currentKeyUUID, zk.tlk.uuid, "Fetched TLK and CK TLK should be different");
        }
    }

    // Now, find the keysets again, asking for a fetch this time
    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* fetchedKeysetOps = [NSMutableArray array];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready')");
    [keysetOps addObject:[self.defaultCKKS findKeySets:NO]];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp in fetchedKeysetOps) {
        [keysetOp waitUntilFinished];
        XCTAssertNil(keysetOp.error, "Should be no error fetching keyset from CKKS");

        for(CKRecordZoneID* zoneID in keysetOp.intendedZoneIDs) {
            CKKSCurrentKeySet* keyset = keysetOp.keysets[zoneID];
            XCTAssertNotNil(keyset, "Should have provided a keyset for %@", zoneID);

            ZoneKeys* zk = self.keys[zoneID];
            XCTAssertNotNil(zk, "Should have new zone keys for zone %@", zoneID);
            XCTAssertEqualObjects(keyset.currentTLKPointer.currentKeyUUID, zk.tlk.uuid, "Fetched TLK and CK TLK should now match");
        }
    }
}

- (void)testProvideKeysetFromNoTrust {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust'");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];
    [keysetOp timeout:20*NSEC_PER_SEC];
    [keysetOp waitUntilFinished];

    XCTAssertNil(keysetOp.error, "Should be no error fetching a keyset");
}

- (void)testProvideKeysetFromNoTrustWithRefetch {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust'");

    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:YES];
    [keysetOp timeout:20*NSEC_PER_SEC];
    [keysetOp waitUntilFinished];

    XCTAssertNil(keysetOp.error, "Should be no error fetching a keyset");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testProvideKeysetAfterReceivingTLKInNoTrust {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust'");

    // This isn't necessarily SOS, but perhaps SBD.
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Still ends up in waitfortrust...
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust'");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];
    [keysetOp timeout:20*NSEC_PER_SEC];
    [keysetOp waitUntilFinished];

    XCTAssertNil(keysetOp.error, "Should be no error fetching a keyset");
    CKKSCurrentKeySet* keychainKeyset = keysetOp.keysets[self.keychainZoneID];
    XCTAssertNotNil(keychainKeyset, "Should have a keyset");
    XCTAssertNotNil(keychainKeyset.currentTLKPointer, "keyset should have a current TLK pointer");
    XCTAssertNotNil(keychainKeyset.tlk, "Should have a TLK");
}

- (void)testProvideKeysetWhileActivelyLosingTrust {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateLoseTrust];

    [self startCKKSSubsystem];

    // Intentionally don't check the key state here; the test pause machinery will cause the key state to not be set
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoseTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'lose-trust'");
    XCTAssertTrue([self.defaultCKKS.stateMachine isPaused], @"State machine should be in a test pause");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];

    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateLoseTrust];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust'");

    [keysetOp timeout:20*NSEC_PER_SEC];
    [keysetOp waitUntilFinished];

    XCTAssertNil(keysetOp.error, "Should be no error fetching a keyset");
    CKKSCurrentKeySet* keychainKeyset = keysetOp.keysets[self.keychainZoneID];
    XCTAssertNotNil(keychainKeyset, "Should have a keyset");
    XCTAssertNotNil(keychainKeyset.currentTLKPointer, "keyset should have a current TLK pointer");
    XCTAssertNil(keychainKeyset.tlk, "Should not have a TLK while without trust (and without receiving the tlk bits)");
}

- (void)testUploadInitialKeyHierarchyAfterLockedStart {
    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateProvideKeyHierarchy];
    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateProvideKeyHierarchy] wait:20*NSEC_PER_SEC], "CKKS state machine should enter providekeyhiearchry");
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateProvideKeyHierarchy];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should remain in waitfortlkcreation");

    // After unlock, the key hierarchy should be created.
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    [keysetOp timeout:10 * NSEC_PER_SEC];
    [keysetOp waitUntilFinished];
    XCTAssertNil(keysetOp.error, @"Should be no error performing keyset op");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should remain in 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    NSArray<CKRecord*>* keyHierarchyRecords = [self performOctagonKeySetWrites:keysetOp.keysets];
    [self.defaultCKKS receiveTLKUploadRecords:keyHierarchyRecords];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testExitWaitForTLKUploadIfTLKsCreated {
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];

    [keysetOp timeout:10 * NSEC_PER_SEC];
    [keysetOp waitUntilFinished];
    XCTAssertNil(keysetOp.error, @"Should be no error performing keyset op");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should enter 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // But another device beats us to it!
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should enter 'waitfortlk'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testExitWaitForTLKUploadIfTLKsCreatedWhileNoTrust {
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];

    [keysetOp timeout:10 * NSEC_PER_SEC];
    [keysetOp waitUntilFinished];
    XCTAssertNil(keysetOp.error, @"Should be no error performing keyset op");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should enter 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    // But another device beats us to it!
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should enter 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testLockImmediatelyAfterUploadingInitialKeyHierarchy {

    __weak __typeof(self) weakSelf = self;

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews afterUpload:^{
        __strong __typeof(self) strongSelf = weakSelf;
        [strongSelf holdCloudKitFetches];
    }];

    // Should enter 'ready'
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, lock and allow fetches again
    self.aksLockState = true;
    [self.lockStateTracker recheck];
    [self releaseCloudKitFetchHold];

    CKKSResultOperation* op = [self.defaultCKKS.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting];
    [op waitUntilFinished];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Wait for CKKS to shake itself out...
    [self.defaultCKKS waitForOperationsOfClass:[CKKSProcessReceivedKeysOperation class]];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testReceiveKeyHierarchyAfterLockedStart {
    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Now, another device comes along and creates the hierarchy; we download it; and it and sends us the TLK
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [[self.defaultCKKS.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting] waitUntilFinished];

    self.aksLockState = false;
    [self.lockStateTracker recheck];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should end up in waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"Key state should end up in ready");

    // After unlock, the TLK arrives
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testLoadKeyHierarchyAfterLockedStart {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testUploadAndUseKeyHierarchy {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };
    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not exist");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords:1
                     deletedRecords:0
           currentKeyPointerRecords:1
                             zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]
         expectedOperationGroupName:@"keychain-api-use"];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // now, expect a single class A record to be uploaded
    [self expectCKModifyItemRecords:1
                     deletedRecords:0
           currentKeyPointerRecords:1
                             zoneID:self.keychainZoneID
                          checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]
         expectedOperationGroupName:@"keychain-api-use"];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)@{
                                                                         (id)kSecClass : (id)kSecClassGenericPassword,
                                                                         (id)kSecAttrAccessGroup : @"com.apple.security.sos",
                                                                         (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                                                                         (id)kSecAttrAccount : @"account-class-A",
                                                                         (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                                                         (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                                                         (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                                                         }, NULL), @"Adding class A item");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testUploadInitialKeyHierarchyTriggersBackup {
    // We also expect the view manager's notifyNewTLKsInKeychain call to fire (after some delay)
    OCMExpect([self.mockCKKSViewManager notifyNewTLKsInKeychain]);

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    OCMVerifyAllWithDelay(self.mockCKKSViewManager, 10);
}

- (void)testResetCloudKitZoneFromNoTLK {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // If CKKS sees a zone it's never going to be able to read, it should reset that zone
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // explicitly do not save a fake device status here
    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateResettingZone] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'resetzone'");

    // But then, it'll fire off the reset and reach 'ready', with a little help from octagon
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testResetCloudKitZoneFromNoTLKWithOtherWaitForTLKDevices {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // If CKKS sees a zone it's never going to be able to read, it should reset that zone
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Save a fake device status here, but modify its key state to be 'waitfortlk': it has no idea what the TLK is either
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self putFakeOctagonOnlyDeviceStatusInCloudKit:self.keychainZoneID];

    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType]) {
            record[SecCKRecordKeyState] = CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForTLK);
        }
    }

    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateResettingZone] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'resetzone'");

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    // But then, it'll fire off the reset and reach 'ready'
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testResetCloudKitZoneFromNoTLKIgnoringInactiveDevices {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // If CKKS sees a zone it's never going to be able to read, it should reset that zone
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    // Save a fake device status here, but modify its creation and modification times to be months ago
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self putFakeOctagonOnlyDeviceStatusInCloudKit:self.keychainZoneID];

    // Put a 'in-circle' TLKShare record, but also modify its creation and modification times
    CKKSSOSSelfPeer* untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"untrusted-peer"
                                                                  encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                     signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                       viewList:self.managedViewList];
    [self putTLKShareInCloudKit:self.keychainZoneKeys.tlk from:untrustedPeer to:untrustedPeer zoneID:self.keychainZoneID];

    for(CKRecord* record in self.keychainZone.currentDatabase.allValues) {
        if([record.recordType isEqualToString:SecCKRecordDeviceStateType] || [record.recordType isEqualToString:SecCKRecordTLKShareType]) {
            record.creationDate = [NSDate distantPast];
            record.modificationDate = [NSDate distantPast];
        }
    }

    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateResettingZone] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'resetzone'");

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    // But then, it'll fire off the reset and reach 'ready'
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testDoNotResetCloudKitZoneDuringBadCircleState {
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // This test has stuff in CloudKit, but no TLKs.
    // CKKS should NOT reset the CK zone.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    self.zones[self.keychainZoneID].flag = true;

    [self startCKKSSubsystem];

    // But since we're out of circle, this test needs to initialize the zone itself
    [self.defaultCKKS beginCloudKitOperation];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    FakeCKZone* keychainZone = self.zones[self.keychainZoneID];
    XCTAssertNotNil(keychainZone, "Should still have a keychain zone");
    XCTAssertTrue(keychainZone.flag, "keychain zone should not have been recreated");
}

- (void)testDoNotResetCloudKitZoneFromWaitForTLKDueToRecentDeviceState {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS shouldn't reset this zone, due to a recent device status claiming to have TLKs
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Also, CKKS _should_ be able to return the key hierarchy if asked before it starts
    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = [self.defaultCKKS findKeySets:NO];

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

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortlk'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTAssertTrue(self.keychainZone.flag, "Zone flag should not have been reset to false");

    // And, ensure that the keyset op ran and has results
    CKKSResultOperation* waitOp = [CKKSResultOperation named:@"test op" withBlock:^{}];
    [waitOp addDependency:keysetOp];
    [waitOp timeout:2*NSEC_PER_SEC];
    [self.operationQueue addOperation:waitOp];
    [waitOp waitUntilFinished];

    XCTAssert(keysetOp.finished, "Keyset op should have finished");
    XCTAssertNil(keysetOp.error, "keyset op should not have errored");

    CKKSCurrentKeySet* keychainKeyset = keysetOp.keysets[self.keychainZoneID];
    XCTAssertNotNil(keychainKeyset, "keyset op should have a keyset");
    XCTAssertNotNil(keychainKeyset.currentTLKPointer, "keyset should have a current TLK pointer");
    XCTAssertEqualObjects(keychainKeyset.currentTLKPointer.currentKeyUUID, self.keychainZoneKeys.tlk.uuid, "keyset should match what's in zone");
}

- (void)testDoNotCloudKitZoneFromWaitForTLKDueToRecentButUntrustedDeviceState {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS should reset this zone, even though to a recent device status claiming to have TLKs. The device isn't trusted
    self.silentZoneDeletesAllowed = true;
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self.mockSOSAdapter.trustedPeers removeObject:self.remoteSOSOnlyPeer];

    self.keychainZone.flag = true;
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], @"Key state should become 'waitfortlk'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    XCTAssertTrue(self.keychainZone.flag, "Zone flag should not have been reset to false");

    // And ensure it doesn't go on to 'reset'
    XCTAssertNotEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:100*NSEC_PER_MSEC], @"Key state should not become 'resetzone'");
    XCTAssertNotEqual(0, [self.defaultCKKS.stateConditions[CKKSStateResettingZone] wait:100*NSEC_PER_MSEC], @"CKKS state machine shouldn't have become 'resetzone'");
}

- (void)testResetCloudKitZoneFromWaitForTLKDueToLessRecentAndUntrustedDeviceState {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // CKKS should reset this zone, even though to a recent device status claiming to have TLKs. The device isn't trusted
    self.silentZoneDeletesAllowed = true;
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self.mockSOSAdapter.trustedPeers removeObject:self.remoteSOSOnlyPeer];

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
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateResettingZone] wait:20*NSEC_PER_SEC], @"Key state should become 'resetzone'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateResettingZone] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'resetzone'");

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    // Then we should reset.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // And the zone should have been cleared and re-made
    XCTAssertFalse(self.keychainZone.flag, "Zone flag should have been reset to false");
}

- (void)testAcceptExistingKeyHierarchy {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should only upload its TLK share
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Verify that there are three local keys, and three local current key records
    __weak __typeof(self) weakSelf = self;
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;

        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 3u, "Three keys in local database");

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all: &error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");
    }];
}

- (void)testAcceptExistingAndUseKeyHierarchy {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    // But, CKKS shouldn't ever reset the zone
    self.keychainZone.flag = true;

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:200*NSEC_PER_SEC], "Key state should have become waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS). We'll create a TLK share for ourselves.
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Wait for the key hierarchy to sort itself out, to make it easier on this test; see testOnboardOldItemsWithExistingKeyHierarchy for the other test.
    // The CKKS subsystem should write its TLK share, but nothing else
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertTrue(self.keychainZone.flag, "Keychain zone shouldn't have been reset");
}

- (void)testAcceptExistingKeyHierarchyDespiteLocked {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForUnlock] wait:20*NSEC_PER_SEC], "Key state should have become waitforunlock");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // CKKS will give itself a TLK Share
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // Now that all operations are complete, 'unlock' AKS
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Verify that there are three local keys, and three local current key records
    __weak __typeof(self) weakSelf = self;
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;

        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 3u, "Three keys in local database");

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all: &error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");
    }];
}

- (void)testRestartWhileLocked {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // 'Lock' the keybag
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];

    [self.defaultCKKS beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    self.aksLockState = false;
    [self.lockStateTracker recheck];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testExternalKeyRoll {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    __weak __typeof(self) weakSelf = self;

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    // Make life easy on this test; testAcceptKeyConflictAndUploadReencryptedItem will check the case when we don't receive the notification
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Just in extra case of threading issues, force a reexamination of the key hierarchy
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        [self.defaultCKKS _onqueuePokeKeyStateMachine];
        return CKKSDatabaseTransactionCommit;
    }];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Verify that there are six local keys, and three local current key records
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        NSError* error = nil;
        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:self.keychainZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 6u, "Six keys in local database");

        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all: &error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");

        for(CKKSCurrentKeyPointer* key in currentkeys) {
            if([key.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
                XCTAssertEqualObjects(key.currentKeyUUID, strongSelf.keychainZoneKeys.tlk.uuid);
            } else if([key.keyclass isEqualToString: SecCKKSKeyClassA]) {
                XCTAssertEqualObjects(key.currentKeyUUID, strongSelf.keychainZoneKeys.classA.uuid);
            } else if([key.keyclass isEqualToString: SecCKKSKeyClassC]) {
                XCTAssertEqualObjects(key.currentKeyUUID, strongSelf.keychainZoneKeys.classC.uuid);
            } else {
                XCTFail("Unknown key class: %@", key.keyclass);
            }
        }
    }];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // TODO: remove this by writing code for item reencrypt after key arrival
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAcceptKeyConflictAndUploadReencryptedItem {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Do not trigger a notification here. This should cause a conflict updating the current key records

    // We expect a single record to be uploaded, but that the write will be rejected
    // We then expect that item to be reuploaded with the current key

    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under rolled class C key in hierarchy"]];

    // New key arrives via SOS!
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 10);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testAcceptKeyConflictAndUploadReencryptedItems {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Do not trigger a notification here. This should cause a conflict updating the current key records

    // We expect a single record to be uploaded, but that the write will be rejected
    // We then expect that item to be reuploaded with the current key

    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key"];
    [self addGenericPassword: @"data" account: @"account-delete-me-rolled-key-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under rolled class C key in hierarchy"]];

    // New key arrives via SOS!
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testAcceptKeyHierarchyResetAndUploadReencryptedItem {
    // Test starts with nothing in CloudKit. CKKS uploads a key hierarchy, then it's silently replaced.
    // CKKS should notice the replacement, and reupload the item.

    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // A new peer arrives and resets the world! It sends us a share, though.
    CKKSSOSSelfPeer* remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                                encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                   signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                     viewList:self.managedViewList];
    [self.mockSOSAdapter.trustedPeers addObject:remotePeer1];

    NSString* classCUUID = self.keychainZoneKeys.classC.uuid;

    self.zones[self.keychainZoneID] = [[FakeCKZone alloc] initZone:self.keychainZoneID];
    self.keys[self.keychainZoneID] = nil;
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:remotePeer1 zoneID:self.keychainZoneID];

    XCTAssertNotEqual(classCUUID, self.keychainZoneKeys.classC.uuid, @"Class C UUID should have changed");

    // Upon adding an item, we expect a failed OQO, then another OQO with the two items (encrypted correctly)
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    [self expectCKModifyItemRecords:2
           currentKeyPointerRecords:1
                             zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // We also expect a self share upload, once CKKS figures out the right key hierarchy
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"account-delete-me-after-reset"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromRequestKeyRefetchWithoutRolling {
    // Simply requesting a key state refetch shouldn't roll the key hierarchy.

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Items should upload.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // CKKS should not roll the keys while progressing back to 'ready', but it will fetch once
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    [self.defaultCKKS.stateMachine handleFlag:CKKSFlagFetchRequested];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromIncrementedCurrentKeyPointerEtag {
    // CloudKit sometimes reports the current key pointers have changed (etag mismatch), but their content hasn't.
    // In this case, CKKS shouldn't roll the TLK.

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing]; // just to be sure it's fetched

    // Items should upload.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];

    // Bump the etag on the class C current key record, but don't change any data
    CKRecordID* currentClassCID = [[CKRecordID alloc] initWithRecordName: @"classC" zoneID: self.keychainZoneID];
    CKRecord* currentClassC = self.keychainZone.currentDatabase[currentClassCID];
    XCTAssertNotNil(currentClassC, "Should have the class C current key pointer record");

    [self.keychainZone addCKRecordToZone:[currentClassC copy]];
    XCTAssertNotEqualObjects(currentClassC.etag, self.keychainZone.currentDatabase[currentClassCID].etag, "Etag should have changed");

    // Add another item. This write should fail, then CKKS should recover without rolling the key hierarchy or issuing a fetch.
    self.silentFetchesAllowed = false;
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverMultipleItemsFromIncrementedCurrentKeyPointerEtag {
    // CloudKit sometimes reports the current key pointers have changed (etag mismatch), but their content hasn't.
    // In this case, CKKS shouldn't roll the TLK.
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing]; // just to be sure it's fetched

    // Items should upload.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];

    // Bump the etag on the class C current key record, but don't change any data
    CKRecordID* currentClassCID = [[CKRecordID alloc] initWithRecordName: @"classC" zoneID: self.keychainZoneID];
    CKRecord* currentClassC = self.keychainZone.currentDatabase[currentClassCID];
    XCTAssertNotNil(currentClassC, "Should have the class C current key pointer record");

    [self.keychainZone addCKRecordToZone:[currentClassC copy]];
    XCTAssertNotEqualObjects(currentClassC.etag, self.keychainZone.currentDatabase[currentClassCID].etag, "Etag should have changed");

    // Add another item. This write should fail, then CKKS should recover without rolling the key hierarchy or issuing a fetch.
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    self.defaultCKKS.holdOutgoingQueueOperation = [CKKSGroupOperation named:@"outgoing-hold" withBlock: ^{
        ckksnotice_global("ckks", "releasing outgoing-queue hold");
    }];

    self.silentFetchesAllowed = false;
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    [self.operationQueue addOperation: self.defaultCKKS.holdOutgoingQueueOperation];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsCreatingKeyHierarchy {
    // In this test, we'll check if the CKKS subsystem will pick up a keychain item which existed before the key hierarchy, both with and without a UUID attached at item creation

    // Test starts with nothing in CloudKit, and CKKS blocked. Add one item without a UUID...

    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // We then expect an upload of the added items
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchy {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchyExtantTLK {
    // Test starts key hierarchy in our fake CloudKit, the TLK arrived in the local keychain, and CKKS blocked.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Add one item without a UUID...
    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // Now that we have an item in the keychain, allow CKKS to spin up. It should upload the item, using the key hierarchy already extant
    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testOnboardOldItemsWithExistingKeyHierarchyLateTLK {
    // Test starts key hierarchy in our fake CloudKit, and CKKS blocked.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    self.keychainZone.flag = true;

    // Add one item without a UUID...
    SecCKKSTestSetDisableAutomaticUUID(true);
    [self addGenericPassword: @"data" account: @"account-delete-me-no-UUID" expecting:errSecSuccess message: @"Add item (no UUID) to keychain"];

    // and an item with a UUID...
    SecCKKSTestSetDisableAutomaticUUID(false);
    [self addGenericPassword: @"data" account: @"account-delete-me-with-UUID" expecting:errSecSuccess message: @"Add item (w/ UUID) to keychain"];

    // Now that we have an item in the keychain, allow CKKS to spin up. It should upload the item, using the key hierarchy already extant

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should have become waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Now, save the TLK to the keychain (to simulate it coming in via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertTrue(self.keychainZone.flag, "Keychain zone shouldn't have been reset");
}

- (void)testOnboardOldItemMatchingExistingCKKSItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];

    NSString* itemAccount = @"account-delete-me";
    [self addGenericPassword:@"password" account:itemAccount];

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self findGenericPassword:itemAccount expecting:errSecSuccess];

    // And, the local item should now match the UUID downloaded from CKKS
    [self checkGenericPasswordStoredUUID:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" account:itemAccount];
}

- (void)testResync {
    // We need to set up a desynced situation to test our resync.
    // First, let CKKS start up and send several items to CloudKit (that we'll then desync!)
    __block NSError* error = nil;

    // Test starts with keys in CloudKit (so we can create items later)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    [self addGenericPassword: @"data" account: @"third"];
    [self addGenericPassword: @"data" account: @"fourth"];
    [self addGenericPassword: @"data" account: @"fifth"];
    [self addGenericPassword: @"data" account: @"sixth"];
    NSUInteger passwordCount = 6u;

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
    [self checkGenericPassword: @"data" account: @"third"];
    [self checkGenericPassword: @"data" account: @"fourth"];
    [self checkGenericPassword: @"data" account: @"fifth"];
    [self checkGenericPassword: @"data" account: @"sixth"];

    [self expectCKModifyItemRecords: passwordCount currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    // One TLK share record
    XCTAssertEqual(self.keychainZone.currentDatabase.count, SYSTEM_DB_RECORD_COUNT+passwordCount+1, "Have SYSTEM_DB_RECORD_COUNT+passwordCount+1 objects in cloudkit");

    // Now, corrupt away!
    // Extract all passwordCount items for Corruption
    NSArray<CKRecord*>* items = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    XCTAssertEqual(items.count, passwordCount, "Have %lu Items in cloudkit", (unsigned long)passwordCount);

    // For the first record, delete all traces of it from CKKS. But! it remains in local keychain.
    // Expected outcome: CKKS resyncs; item exists again.
    CKRecord* delete = items[0];
    NSString* deleteAccount = [[self decryptRecord: delete] objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(deleteAccount, "received an account for the local delete object");

    __weak __typeof(self) weakSelf = self;
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        if(ckme) {
            [ckme deleteFromDatabase: &error];
        }
        XCTAssertNil(error, "no error removing CKME");
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        if(oqe) {
            [oqe deleteFromDatabase: &error];
        }
        XCTAssertNil(error, "no error removing OQE");
        CKKSIncomingQueueEntry* iqe = [CKKSIncomingQueueEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        if(iqe) {
            [iqe deleteFromDatabase: &error];
        }
        XCTAssertNil(error, "no error removing IQE");
        return CKKSDatabaseTransactionCommit;
    }];

    // For the second record, delete all traces of it from CloudKit.
    // Expected outcome: deleted locally
    CKRecord* remoteDelete = items[1];
    NSString* remoteDeleteAccount = [[self decryptRecord: remoteDelete] objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(remoteDeleteAccount, "received an account for the remote delete object");

    [self.keychainZone deleteCKRecordIDFromZone: remoteDelete.recordID];
    for(NSMutableDictionary<CKRecordID*, CKRecord*>* database in self.keychainZone.pastDatabases.allValues) {
        [database removeObjectForKey: remoteDelete.recordID];
    }

    // The third record gets modified in CloudKit, but not locally.
    // Expected outcome: use the CloudKit version
    CKRecord* remoteDataChanged = items[2];
    NSMutableDictionary* remoteDataDictionary = [[self decryptRecord: remoteDataChanged] mutableCopy];
    NSString* remoteDataChangedAccount = [remoteDataDictionary objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(remoteDataChangedAccount, "Received an account for the remote-data-changed object");
    remoteDataDictionary[(__bridge id) kSecValueData] = [@"CloudKitWins" dataUsingEncoding: NSUTF8StringEncoding];

    CKRecord* newData = [self newRecord: remoteDataChanged.recordID withNewItemData: remoteDataDictionary];
    [self.keychainZone addToZone: newData];
    for(NSMutableDictionary<CKRecordID*, CKRecord*>* database in self.keychainZone.pastDatabases.allValues) {
        database[remoteDataChanged.recordID] = newData;
    }

    // The fourth record stays in-sync. Good work, everyone!
    // Expected outcome: stays in-sync
    NSString* insyncAccount = [[self decryptRecord: items[3]] objectForKey: (__bridge id) kSecAttrAccount];
    XCTAssertNotNil(insyncAccount, "Received an account for the in-sync object");

    // The fifth record is updated locally, but CKKS didn't get the notification, and so the local CKMirror and CloudKit don't have it
    // Expected outcome: local change should be steamrolled by the cloud version
    CKRecord* localDataChanged = items[4];
    NSMutableDictionary* localDataDictionary = [[self decryptRecord: localDataChanged] mutableCopy];
    NSString* localDataChangedAccount = [localDataDictionary objectForKey: (__bridge id) kSecAttrAccount];
    SecCKKSDisable();
    [self updateGenericPassword:@"newpassword" account:localDataChangedAccount];
    [self checkGenericPassword:@"newpassword"  account:localDataChangedAccount];
    SecCKKSEnable();

    // The sixth record matches what's in CloudKit, but the local UUID has changed (and CKKS didn't notice, for whatever reason)
    CKRecord* uuidMismatch = items[5];
    NSMutableDictionary* uuidMisMatchDictionary = [[self decryptRecord:uuidMismatch] mutableCopy];
    NSString* uuidMismatchAccount = uuidMisMatchDictionary[(__bridge id)kSecAttrAccount];
    NSString* newUUID = @"55463F83-3AAE-462D-B95F-2FA6AD088980";
    SecCKKSDisable();
    [self setGenericPasswordStoredUUID:newUUID account:uuidMismatchAccount];
    [self checkGenericPasswordStoredUUID:newUUID account:uuidMismatchAccount];
    SecCKKSEnable();

    // To make this more challenging, CK returns the refetch in multiple batches. This shouldn't affect the resync...
    CKServerChangeToken* ck1 = self.keychainZone.currentChangeToken;
    self.silentFetchesAllowed = false;
    [self expectCKFetch];
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        CKServerChangeToken* changeToken = frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken;
        if(changeToken && [changeToken isEqual:ck1]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    self.keychainZone.limitFetchTo = ck1;
    self.keychainZone.limitFetchError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:4]}];

    // The seventh record gets magically added to CloudKit, but CKKS has never heard of it
    //  (emulates a lost record on the client, but that CloudKit already believes it's sent the record for)
    // Expected outcome: added to local keychain
    NSString* remoteOnlyAccount = @"remote-only";
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount: remoteOnlyAccount];
    [self.keychainZone addToZone: ckr];
    for(NSMutableDictionary<CKRecordID*, CKRecord*>* database in self.keychainZone.pastDatabases.allValues) {
        database[ckr.recordID] = ckr;
    }

    ckksnotice("ckksresync", self.keychainView, "local delete:        %@ %@", delete.recordID.recordName, deleteAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote deletion:     %@ %@", remoteDelete.recordID.recordName, remoteDeleteAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote data changed: %@ %@", remoteDataChanged.recordID.recordName, remoteDataChangedAccount);
    ckksnotice("ckksresync", self.keychainView, "in-sync:             %@ %@", items[3].recordID.recordName, insyncAccount);
    ckksnotice("ckksresync", self.keychainView, "local update:        %@ %@", items[4].recordID.recordName, localDataChangedAccount);
    ckksnotice("ckksresync", self.keychainView, "uuid mismatch:       %@ %@", items[5].recordID.recordName, uuidMismatchAccount);
    ckksnotice("ckksresync", self.keychainView, "Remote only:         %@ %@", ckr.recordID.recordName, remoteOnlyAccount);

    CKKSSynchronizeOperation* resyncOperation = [self.defaultCKKS resyncWithCloud];
    [resyncOperation waitUntilFinished];

    XCTAssertNil(resyncOperation.error, "No error during the resync operation");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now do some checking. Remember, we don't know which record we corrupted, so use the parsed account variables to check.

    [self findGenericPassword: deleteAccount            expecting: errSecSuccess];
    [self findGenericPassword: remoteDeleteAccount      expecting: errSecItemNotFound];
    [self findGenericPassword: remoteDataChangedAccount expecting: errSecSuccess];
    [self findGenericPassword: insyncAccount            expecting: errSecSuccess];
    [self findGenericPassword: localDataChangedAccount  expecting: errSecSuccess];
    [self findGenericPassword: uuidMismatchAccount      expecting: errSecSuccess];
    [self findGenericPassword: remoteOnlyAccount        expecting: errSecSuccess];

    [self checkGenericPassword: @"data"         account: deleteAccount];
    //[self checkGenericPassword: @"data"         account: remoteDeleteAccount];
    [self checkGenericPassword: @"CloudKitWins" account: remoteDataChangedAccount];
    [self checkGenericPassword: @"data"         account: insyncAccount];
    [self checkGenericPassword:@"data"          account:localDataChangedAccount];
    [self checkGenericPassword:@"data"          account:uuidMismatchAccount];
    [self checkGenericPassword: @"data"         account: remoteOnlyAccount];

    [self checkGenericPasswordStoredUUID:uuidMismatch.recordID.recordName account:uuidMismatchAccount];

    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        CKKSMirrorEntry* ckme = nil;

        ckme = [CKKSMirrorEntry tryFromDatabase:delete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:remoteDelete.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNil(ckme); // deleted!

        ckme = [CKKSMirrorEntry tryFromDatabase:remoteDataChanged.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:items[3].recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:items[4].recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);

        ckme = [CKKSMirrorEntry tryFromDatabase:ckr.recordID.recordName zoneID:strongSelf.keychainZoneID error:&error];
        XCTAssertNil(error);
        XCTAssertNotNil(ckme);
    }];
}

- (void)testResyncItemsMissingFromLocalKeychain {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We want:
    //   one password correctly synced between local keychain and CloudKit
    //   one password incorrectly disappeared from local keychain, but in mirror table
    //   one password sitting in the outgoing queue
    //   one password sitting in the incoming queue

    // Add and sync two passwords
    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Now, place an item in the outgoing queue

    //[self addGenericPassword: @"data" account: @"third"];
    //[self checkGenericPassword: @"data" account: @"third"];

    // Now, corrupt away!
    // Extract all passwordCount items for Corruption
    NSArray<CKRecord*>* items = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    XCTAssertEqual(items.count, 2u, "Have %lu Items in cloudkit", (unsigned long)2u);

    // For the first record, surreptitiously remove from local keychain
    CKRecord* remove = items[0];
    NSString* removeAccount = [[self decryptRecord:remove] objectForKey:(__bridge id)kSecAttrAccount];
    XCTAssertNotNil(removeAccount, "received an account for the local delete object");

    NSURL* kcpath = (__bridge_transfer NSURL*)SecCopyURLForFileInKeychainDirectory((__bridge CFStringRef)@"keychain-2-debug.db");
    sqlite3* db;
    sqlite3_open([[kcpath path] UTF8String], &db);
    NSString* query = [NSString stringWithFormat:@"DELETE FROM genp WHERE uuid=\"%@\"", remove.recordID.recordName];
    char* sqlerror = NULL;
    XCTAssertEqual(SQLITE_OK, sqlite3_exec(db, [query UTF8String], NULL, NULL, &sqlerror), "SQL deletion shouldn't error");
    XCTAssertTrue(sqlerror == NULL, "No error string should have been returned: %s", sqlerror);
    if(sqlerror) {
        sqlite3_free(sqlerror);
        sqlerror = NULL;
    }
    sqlite3_close(db);

    // The second record is kept in-sync

    // Now, add an in-flight change (for record 3)
    [self holdCloudKitModifications];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"data" account:@"third"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // For the fourth, add a new record but prevent incoming queue processing
    self.defaultCKKS.holdIncomingQueueOperation = [CKKSResultOperation named:@"hold-incoming" withBlock:^{}];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"fourth"];
    [self.keychainZone addToZone:ckr];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    [self.defaultCKKS scanLocalItems];
    // This operation will wait for the CKKSOutgoingQueue operation (currently held writing to cloudkit) to finish before beginning

    // Allow everything to proceed
    [self releaseCloudKitModificationHold];

    [self.operationQueue addOperation:self.defaultCKKS.holdIncomingQueueOperation];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");

    // And ensure that all four items are present again
    [self findGenericPassword: @"first"  expecting: errSecSuccess];
    [self findGenericPassword: @"second" expecting: errSecSuccess];
    [self findGenericPassword: @"third"  expecting: errSecSuccess];
    [self findGenericPassword: @"fourth" expecting: errSecSuccess];
}

- (void)testScanItemsChangedInLocalKeychain {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Add and sync two passwords
    NSString* itemAccount = @"first";
    [self addGenericPassword:@"data" account:itemAccount];
    [self addGenericPassword:@"data" account:@"second"];

    [self checkGenericPassword:@"data" account:itemAccount];
    [self checkGenericPassword:@"data" account:@"second"];

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Now, have CKKS miss an update
    SecCKKSDisable();
    [self updateGenericPassword:@"newpassword" account:itemAccount];
    [self checkGenericPassword:@"newpassword"  account:itemAccount];
    SecCKKSEnable();

    // Now, where are we....
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkPasswordBlock:self.keychainZoneID account:itemAccount password:@"newpassword"]];

    [self.defaultCKKS scanLocalItems];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And ensure that all four items are present again
    [self findGenericPassword: @"first"  expecting: errSecSuccess];
    [self findGenericPassword: @"second" expecting: errSecSuccess];
}

- (void)testEnsureScanOccursOnNextStartIfCancelled {
    // We want to set up a situation where a CKKSScanLocalItemsOperation is cancelled by daemon quitting.
    NSString* itemAccount = @"first";
    [self addGenericPassword:@"data" account:itemAccount];
    [self addGenericPassword:@"data" account:@"second"];

    // We're going to pretend that the scan doesn't happen due to daemon restart
    SecCKKSSetTestSkipScan(true);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // CKKS should perform normally if new items are added
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"found" account:@"after-setup"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, simulate a restart
    SecCKKSSetTestSkipScan(false);

    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    [self.defaultCKKS beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    XCTAssertTrue(self.defaultCKKS.initiatedLocalScan, "Should have intitiated a scan after restart");

    [self findGenericPassword:@"first"  expecting:errSecSuccess];
    [self findGenericPassword:@"second" expecting:errSecSuccess];
}

- (void)testResyncLocal {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    NSUInteger passwordCount = 2u;

    [self expectCKModifyItemRecords: passwordCount currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Local resyncs shouldn't fetch clouds.
    self.silentFetchesAllowed = false;
    SecCKKSDisable();
    [self deleteGenericPassword:@"first"];
    [self deleteGenericPassword:@"second"];
    SecCKKSEnable();

    // And they're gone!
    [self findGenericPassword:@"first" expecting:errSecItemNotFound];
    [self findGenericPassword:@"second" expecting:errSecItemNotFound];

    CKKSLocalSynchronizeOperation* op = [self.defaultCKKS resyncLocal];
    [op waitUntilFinished];
    XCTAssertNil(op.error, "Shouldn't be an error resyncing locally");

    // And they're back!
    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
}

- (void)testPlistRestoreResyncsLocal {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self addGenericPassword: @"data" account: @"second"];
    NSUInteger passwordCount = 2u;

    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];

    [self expectCKModifyItemRecords:passwordCount currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // o no
    // This 'restores' a plist keychain backup
    // That will kick off a local resync in CKKS, so hold that until we're ready...
    self.defaultCKKS.holdLocalSynchronizeOperation = [CKKSResultOperation named:@"hold-local-synchronize" withBlock:^{}];

    // Local resyncs shouldn't fetch clouds.
    self.silentFetchesAllowed = false;

    CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        CFErrorRef cfcferror = NULL;

        bool ret = SecServerImportKeychainInPlist(dbt, SecSecurityClientGet(), KEYBAG_NONE, KEYBAG_NONE,
                                                  (__bridge CFDictionaryRef)@{}, kSecBackupableItemFilter, false, &cfcferror);

        XCTAssertNil(CFBridgingRelease(cfcferror), "Shouldn't error importing a 'backup'");
        XCTAssert(ret, "Importing a 'backup' should have succeeded");
        return true;
    });
    XCTAssertNil(CFBridgingRelease(cferror), "Shouldn't error mucking about in the db");

    // Restore is additive so original items stick around
    [self findGenericPassword:@"first" expecting:errSecSuccess];
    [self findGenericPassword:@"second" expecting:errSecSuccess];

    // Allow the local resync to continue...
    [self.operationQueue addOperation:self.defaultCKKS.holdLocalSynchronizeOperation];
    [self.defaultCKKS waitForOperationsOfClass:[CKKSLocalSynchronizeOperation class]];

    // Items are still here!
    [self checkGenericPassword: @"data" account: @"first"];
    [self checkGenericPassword: @"data" account: @"second"];
}

- (void)testRestartWithoutRefetch {
    // Restarting the CKKS operation should check that it's been 15 minutes since the last fetch before it fetches again. Simulate this.
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // Tear down the CKKS object and disallow fetches
    [self.defaultCKKS halt];
    self.silentFetchesAllowed = false;

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:@"keychain"];

    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertFalse(self.defaultCKKS.initiatedLocalScan, "Should not have initiated a local items scan due to a restart with a recent fetch");

    // Okay, cool, rad, now let's set the date to be very long ago and check that there's positively a fetch
    [self.defaultCKKS halt];
    self.silentFetchesAllowed = false;

    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];

        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastFetchTime = [NSDate distantPast];
        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    [self expectCKFetch];

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:@"keychain"];

    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    [self.defaultCKKS waitForKeyHierarchyReadiness];
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertFalse(self.defaultCKKS.initiatedLocalScan, "Should not have initiated a local items scan due to a restart (when we haven't fetched in a while, but did scan recently)");

    // Now restart again, but cause a scan to occur
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];
        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastLocalKeychainScanTime = [NSDate distantPast];
        [ckse saveToDatabase:&error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    [self.defaultCKKS halt];
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:@"keychain"];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertTrue(self.defaultCKKS.initiatedLocalScan, "Should have initiated a local items scan due to 24-hr notification");
}

- (void)testFetchAndScanOn24HrNotification {
    // Every 24 hrs, CKKS should fetch if there hasn't been a fetch in a while.
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // We now get the 24-hr notification. Set this bit for later checking
    XCTAssertTrue(self.defaultCKKS.initiatedLocalScan, "Should have initiated a local items scan during bringup");
    self.defaultCKKS.initiatedLocalScan = NO;

    self.silentFetchesAllowed = false;

    CKKSResultOperation *promiseReadyAfterBringup = [self promiseEnteredReadyWithName:@"no-local-scan-after-bringup" blockTakingWatcherResult:^(CKKSResultOperation *watcherResult) {
        XCTAssertNil(watcherResult.error, @"CKKS state machine should follow path to ready state");
        XCTAssertEqual(self.keychainView.viewKeyHierarchyState, SecCKKSZoneKeyStateReady, @"key state should arrive at ready");
        XCTAssertEqual(self.defaultCKKS.stateMachine.currentState, CKKSStateReady, @"CKKS state machine should enter ready");
    }];

    [self.defaultCKKS xpc24HrNotification];
    [[promiseReadyAfterBringup timeout:20*NSEC_PER_SEC] waitUntilFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertFalse(self.defaultCKKS.initiatedLocalScan, "Should not have initiated a local items scan due to a 24-hr notification with a recent fetch");

    // Okay, cool, rad, now let's set the last local-keychain-scan date to be very long ago and retry
    // This shouldn't fetch, but it should scan the local keychain
    self.silentFetchesAllowed = false;

    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];
        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastLocalKeychainScanTime = [NSDate distantPast];
        [ckse saveToDatabase:&error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    CKKSResultOperation *promiseLocalScanOp = [self promiseEnteredReadyWithName:@"local-scan" blockTakingWatcherResult:^(CKKSResultOperation *watcherResult) {
        XCTAssertNil(watcherResult.error, @"CKKS state machine should follow path to ready state");
        XCTAssertEqual(self.keychainView.viewKeyHierarchyState, SecCKKSZoneKeyStateReady, @"key state should arrive at ready");
        XCTAssertEqual(self.defaultCKKS.stateMachine.currentState, CKKSStateReady, @"CKKS state machine should enter ready");
    }];

    [self.defaultCKKS xpc24HrNotification];
    [[promiseLocalScanOp timeout:20*NSEC_PER_SEC] waitUntilFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertTrue(self.defaultCKKS.initiatedLocalScan, "Should have initiated a local items scan due to 24-hr notification");
    self.defaultCKKS.initiatedLocalScan = false;

    // And check that the fetch occurs as well
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.keychainZoneID.zoneName error:&error];
        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.lastFetchTime = [NSDate distantPast];
        [ckse saveToDatabase:&error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    [self expectCKFetch];

    CKKSResultOperation *promiseNoLocalScanAfterRecentOp = [self promiseEnteredReadyWithName:@"no-local-scan-after-recent-scan" blockTakingWatcherResult:^(CKKSResultOperation *watcherResult) {
        XCTAssertNil(watcherResult.error, @"CKKS state machine should follow path to ready state");
        XCTAssertEqual(self.keychainView.viewKeyHierarchyState, SecCKKSZoneKeyStateReady, @"key state should arrive at ready");
        XCTAssertEqual(self.defaultCKKS.stateMachine.currentState, CKKSStateReady, @"CKKS state machine should enter ready");
    }];

    [self.defaultCKKS xpc24HrNotification];
    [[promiseNoLocalScanAfterRecentOp timeout:20*NSEC_PER_SEC] waitUntilFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertFalse(self.defaultCKKS.initiatedLocalScan, "Should not have initiated a local items scan due to 24-hr notification (if we've done one recently)");
}

- (void)testRecoverFromZoneCreationFailure {
    // Fail the zone creation.
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone
    [self failNextZoneCreation:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // CKKS should figure it out, and fix it
    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertNil(self.zones[self.keychainZoneID].creationError, "Creation error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromZoneSubscriptionFailure {
    // Fail the zone subscription.
    [self failNextZoneSubscription:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it before Octagon uploads its items
    [self performOctagonTLKUpload:self.ckksViews];

    [self.defaultCKKS waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertNil(self.zones[self.keychainZoneID].subscriptionError, "Subscription error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromZoneSubscriptionFailureDueToZoneNotExisting {
    // This is different from testRecoverFromZoneSubscriptionFailure, since the zone is gone. CKKS must attempt to re-create the zone.

    // Silently fail the zone creation
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone
    [self failNextZoneCreationSilently:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertFalse(self.zones[self.keychainZoneID].flag, "Zone flag was reset");
    XCTAssertNil(self.zones[self.keychainZoneID].subscriptionError, "Subscription error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromZoneDuplicateSubscriptionFailure {
    // Subscription creations are supposed to be idempotent.
    // But, if they aren't, be resilient.

    /*
     The error taken from a sysdiagnose is:

     <CKError 0x13f63e520: "Partial Failure" (2/1011); "Failed to modify some subscriptions"; uuid = 9174968C-225F-4CF6-8D17-3D1D9B36B6EC; container ID = "com.apple.security.keychain"; partial errors: {
         zone:ProtectedCloudStorage = <CKError 0x13f62fd40: "Server Rejected Request" (15/2032); server message = "subscription is duplicate of 'zone:ProtectedCloudStorage'"; uuid = 9174968C-225F-4CF6-8D17-3D1D9B36B6EC>
         zone:CreditCards = <CKError 0x13f6441a0: "Server Rejected Request" (15/2032); server message = "subscription is duplicate of 'zone:CreditCards'"; uuid = 9174968C-225F-4CF6-8D17-3D1D9B36B6EC>
         zone:LimitedPeersAllowed = <CKError 0x13f669d70: "Server Rejected Request" (15/2032); server message = "subscription is duplicate of 'zone:LimitedPeersAllowed'"; uuid = 9174968C-225F-4CF6-8D17-3D1D9B36B6EC <decode: missing data>
     */

    NSError* duplicateSubscriptionError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                           code:CKErrorServerRejectedRequest
                                                                       userInfo:@{
        NSUnderlyingErrorKey: [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                               code:CKErrorInternalDuplicateSubscription
                                                           userInfo:@{
            NSLocalizedDescriptionKey: @"subscription is duplicate of 'zone:keychain''",
        }]
    }];

    NSError* partialError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                             code:CKErrorPartialFailure
                                                         userInfo:@{
        CKErrorRetryAfterKey: @(0.2),
        CKPartialErrorsByItemIDKey:
            @{@"zone:keychain":duplicateSubscriptionError},
    }];
    NSLog(@"test error: %@", partialError);
    [self persistentlyFailNextZoneSubscription:self.keychainZoneID withError:partialError];

    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertNil(self.zones[self.keychainZoneID].subscriptionError, "Subscription error was unset (and so CKKS probably dealt with the error");
}

- (void)testRecoverFromDeletedTLKWithStashedTLK {
    // We need to handle the case where our syncable TLKs are deleted for some reason. The device that has them might resurrect them

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    NSError* error = nil;

    // Stash the TLKs.
    [self.keychainZoneKeys.tlk saveKeyMaterialToKeychain:true error:&error];
    XCTAssertNil(error, "Should have received no error stashing the new TLK in the keychain");

    // And delete the non-stashed version
    [self.keychainZoneKeys.tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "Should have received no error deleting the new TLK from the keychain");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self.defaultCKKS waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // CKKS should recreate the syncable TLK.
    [self checkNSyncableTLKsInKeychain: 1];
}

- (void)testRecoverFromDeletedTLKWithStashedTLKUponRestart {
    // We need to handle the case where our syncable TLKs are deleted for some reason. The device that has them might resurrect them

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

    // Tear down the CKKS object
    [self.defaultCKKS halt];

    NSError* error = nil;

    // Stash the TLKs.
    [self.keychainZoneKeys.tlk saveKeyMaterialToKeychain:true error:&error];
    XCTAssertNil(error, "Should have received no error stashing the new TLK in the keychain");

    [self.keychainZoneKeys.tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "Should have received no error deleting the new TLK from the keychain");

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];

    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    [self.defaultCKKS waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // CKKS should recreate the syncable TLK.
    [self checkNSyncableTLKsInKeychain: 1];
}

/*
 // <rdar://problem/49024967> Octagon: tests for CK exceptions out of cuttlefish
- (void)testRecoverFromTLKWriteFailure {
    // We need to handle the case where a device's first TLK write doesn't go through (due to whatever reason).
    // Test starts with nothing in CloudKit, and will fail the first TLK write.
    NSError* noNetwork = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}];
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:nil withError:noNetwork];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    [self.defaultCKKS waitForKeyHierarchyReadiness];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // A network failure creating new TLKs shouldn't delete the 'failed' syncable one.
    [self checkNSyncableTLKsInKeychain: 2];
}
 */

// This test needs to be moved and rewritten now that Octagon handles TLK uploads
// <rdar://problem/49024967> Octagon: tests for CK exceptions out of cuttlefish
/*
- (void)testRecoverFromTLKRace {
    // We need to handle the case where a device's first TLK write doesn't go through (due to whatever reason).
    // Test starts with nothing in CloudKit, and will fail the first TLK write.
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject: ^{
        [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    }];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The first TLK write should fail, and then our fake TLKs should be there in CloudKit.
    // It shouldn't write anything back up to CloudKit.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now the TLKs arrive from the other device...
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // A race failure creating new TLKs should delete the old syncable one.
    [self checkNSyncableTLKsInKeychain: 1];
}
 */

- (void)testRecoverFromNullCurrentKeyPointers {
    // The current key pointers in cloudkit shouldn't ever not exist if keys do. But, if they don't, CKKS must recover.

    // Test starts with a broken key hierarchy in our fake CloudKit, but the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* zonekeys = self.keys[self.keychainZoneID];
    FakeCKZone* ckzone = self.zones[self.keychainZoneID];
    ckzone.currentDatabase[zonekeys.currentTLKPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = nil;
    ckzone.currentDatabase[zonekeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = nil;
    ckzone.currentDatabase[zonekeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = nil;

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];

    [self.defaultCKKS waitForKeyHierarchyReadiness];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromNoCurrentKeyPointers {
    // The current key pointers in cloudkit shouldn't ever point to nil. But, if they do, CKKS must recover.

    // Test starts with a broken key hierarchy in our fake CloudKit, but the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* zonekeys = self.keys[self.keychainZoneID];
    XCTAssertNil([self.zones[self.keychainZoneID] deleteCKRecordIDFromZone: zonekeys.currentTLKPointer.storedCKRecord.recordID],    "Deleted TLK pointer from zone");
    XCTAssertNil([self.zones[self.keychainZoneID] deleteCKRecordIDFromZone: zonekeys.currentClassAPointer.storedCKRecord.recordID], "Deleted class a pointer from zone");
    XCTAssertNil([self.zones[self.keychainZoneID] deleteCKRecordIDFromZone: zonekeys.currentClassCPointer.storedCKRecord.recordID], "Deleted class c pointer from zone");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it.
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

/*
 // <rdar://problem/49024967> Octagon: tests for CK exceptions out of cuttlefish
- (void)testRecoverFromBadChangeTag {
    // We received a report where a machine appeared to have an up-to-date change tag, but was continuously attempting to create a new TLK hierarchy. No idea why.

    // Test starts with a broken key hierarchy in our fake CloudKit, but a (incorrectly) up-to-date change tag stored locally.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    SecCKKSTestSetDisableKeyNotifications(true); // Don't tell CKKS about this key material; we're pretending like this is a securityd restart
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    SecCKKSTestSetDisableKeyNotifications(false);

    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.keychainZoneID.zoneName];
        XCTAssertNotNil(ckse, "should have received a ckse");

        ckse.ckzonecreated = true;
        ckse.ckzonesubscribed = true;
        ckse.changeToken = self.keychainZone.currentChangeToken;

        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "shouldn't have gotten an error saving to database");
        return true;
    }];

    // The CKKS subsystem should try to write TLKs, but fail. It'll then upload a TLK share for the keys already in CloudKit
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // CKKS should then happily use the keys in CloudKit
    [self createClassCItemAndWaitForUpload:self.keychainZoneID account:@"account-delete-me"];
    [self createClassAItemAndWaitForUpload:self.keychainZoneID account:@"account-delete-me-class-a"];
}
 */

- (void)testRecoverFromDeletedKeysNewItem {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForCKModifications];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    // Now, delete the local keys from the keychain (but leave the synced TLK)
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                         (id)kSecClass : (id)kSecClassInternetPassword,
                                                                         (id)kSecUseDataProtectionKeychain : @YES,
                                                                         (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                         (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                                                                         }), @"Deleting local keys");
    SecCKKSTestSetDisableKeyNotifications(false);

    NSError* error = nil;
    [self.keychainZoneKeys.classC loadKeyMaterialFromKeychain:&error];
    XCTAssertNotNil(error, "Error loading class C key material from keychain");

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"datadata" account: @"account-no-keys"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // We expect a single class A record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromDeletedKeysReceive {
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self waitForCKModifications];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];

    // Now, delete the local keys from the keychain (but leave the synced TLK)
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecUseDataProtectionKeychain : @YES,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
                                                                            }), @"Deleting local keys");
    SecCKKSTestSetDisableKeyNotifications(false);

    [self.keychainZone addToZone: ckr];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
}

- (void)testRecoverDeletedTLK {
    // If the TLK disappears halfway through, well, that's no good. But we should recover using TLK sharing

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account0"];
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // Now, delete the local keys from the keychain
    SecCKKSTestSetDisableKeyNotifications(true);
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassInternetPassword,
                                                                            (id)kSecUseDataProtectionKeychain : @YES,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
                                                                            }), @"Deleting CKKS keys");
    SecCKKSTestSetDisableKeyNotifications(false);

    // Trigger a notification (with hilariously fake data)
    [self.keychainZone addToZone: ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:200*NSEC_PER_SEC], "Key state should return to 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should return to 'ready'");

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing]; // Do this again, to allow for non-atomic key state machinery switching

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
}

- (void)testRecoverMissingRolledKey {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    NSString* accountShouldExist = @"under-rolled-key";
    NSString* accountWillExist = @"under-rolled-key-later";
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountShouldExist];
    [self.keychainZone addToZone: ckr];

    CKRecord* ckrAddedLater = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountWillExist];
    CKKSKey* pastClassCKey = self.keychainZoneKeys.classC;

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecItemNotFound];

    // Now, find and delete the class C key that ckrAddedLater is under
    NSError* error = nil;
    XCTAssertTrue([pastClassCKey deleteKeyMaterialFromKeychain:&error], "Should be able to delete old key material from keychain");
    XCTAssertNil(error, "Should be no error deleting old key material from keychain");

    [self.keychainZone addToZone:ckrAddedLater];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecSuccess];

    XCTAssertTrue([pastClassCKey loadKeyMaterialFromKeychain:&error], "Class C key should be back in the keychain");
    XCTAssertNil(error, "Should be no error loading key from keychain");
}

- (void)testRecoverMissingRolledClassAKeyWhileLocked {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    NSString* accountShouldExist = @"under-rolled-key";
    NSString* accountWillExist = @"under-rolled-key-later";
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountShouldExist key:self.keychainZoneKeys.classA];
    [self.keychainZone addToZone: ckr];

    CKRecord* ckrAddedLater = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:accountWillExist key:self.keychainZoneKeys.classA];
    CKKSKey* pastClassAKey = self.keychainZoneKeys.classA;

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have returned to ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecItemNotFound];

    // Now, find and delete the class C key that ckrAddedLater is under
    NSError* error = nil;
    XCTAssertTrue([pastClassAKey deleteKeyMaterialFromKeychain:&error], "Should be able to delete old key material from keychain");
    XCTAssertNil(error, "Should be no error deleting old key material from keychain");

    // now, lock the keychain
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self.keychainZone addToZone:ckrAddedLater];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Item should still not exist due to the lock state....
    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecItemNotFound];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should returned to ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    NSOperation* results = [self.defaultCKKS resultsOfNextProcessIncomingQueueOperation];
    self.aksLockState = false;
    [self.lockStateTracker recheck];

    // And now it does
    [results waitUntilFinished];
    [self findGenericPassword:accountShouldExist expecting:errSecSuccess];
    [self findGenericPassword:accountWillExist expecting:errSecSuccess];

    XCTAssertTrue([pastClassAKey loadKeyMaterialFromKeychain:&error], "Class A key should be back in the keychain");
    XCTAssertNil(error, "Should be no error loading key from keychain");
}

- (void)testRecoverFromBadCurrentKeyPointer {
    // The current key pointers in cloudkit shouldn't ever point to missing entries. But, if they do, CKKS must recover.

    // Test starts with a broken key hierarchy in our fake CloudKit, but the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* zonekeys = self.keys[self.keychainZoneID];
    FakeCKZone* ckzone = self.zones[self.keychainZoneID];
    ckzone.currentDatabase[zonekeys.currentTLKPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey]    = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: @"not a real tlk"         zoneID: self.keychainZoneID] action: CKReferenceActionNone];
    ckzone.currentDatabase[zonekeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: @"not a real class a key" zoneID: self.keychainZoneID] action: CKReferenceActionNone];
    ckzone.currentDatabase[zonekeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: @"not a real class c key" zoneID: self.keychainZoneID] action: CKReferenceActionNone];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it (while uploading itself a TLK Share)
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 3 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromIncorrectCurrentTLKPointer {
    // The current key pointers in cloudkit shouldn't ever point to wrong entries. But, if they do, CKKS must recover.

    // Test starts with a rolled hierarchy, and CKPs pointing to the wrong items
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKKSCurrentKeyPointer* oldTLKCKP = self.keychainZoneKeys.currentTLKPointer;
    CKRecord* oldTLKPointer = [self.keychainZone.currentDatabase[self.keychainZoneKeys.currentTLKPointer.storedCKRecord.recordID] copy];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    ZoneKeys* newZoneKeys = [self.keychainZoneKeys copy];

    // And put the oldTLKPointer back
    [self.zones[self.keychainZoneID] addToZone:oldTLKPointer];
    self.keychainZoneKeys.currentTLKPointer = oldTLKCKP;

    // Make sure it stuck:
    XCTAssertNotEqualObjects(self.keychainZoneKeys.currentTLKPointer,
                             newZoneKeys.currentTLKPointer,
                             "current TLK pointer should now not point to proper TLK");

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // The CKKS subsystem should figure out the issue, and fix it (while uploading itself a TLK Share)
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:3 tlkShareRecords:1 zoneID:self.keychainZoneID];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    XCTAssertEqualObjects(self.keychainZoneKeys.currentTLKPointer,
                          newZoneKeys.currentTLKPointer,
                          "current TLK pointer should now point to proper TLK");
    XCTAssertEqualObjects(self.keychainZoneKeys.currentClassAPointer,
                          newZoneKeys.currentClassAPointer,
                          "current Class A pointer should now point to proper Class A key");
    XCTAssertEqualObjects(self.keychainZoneKeys.currentClassCPointer,
                          newZoneKeys.currentClassCPointer,
                          "current Class C pointer should now point to proper Class C key");
}

- (void)testRecoverFromDesyncedKeyRecordsViaResync {
    // We need to set up a desynced situation to test our resync.
    // First, let CKKS start up and send several items to CloudKit (that we'll then desync!)
    __block NSError* error = nil;

    // Test starts with keys in CloudKit (so we can create items later)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"first"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];

    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Now, delete most of the key records are from on-disk, but the change token is not changed
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:self.keychainZoneID];

        XCTAssertNotNil(keyset.currentTLKPointer, @"should be a TLK pointer");
        XCTAssertNotNil(keyset.currentClassAPointer, @"should be a class A pointer");
        XCTAssertNotNil(keyset.currentClassCPointer, @"should be a class C pointer");

        [keyset.currentTLKPointer deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting TLK pointer from database");
        [keyset.currentClassAPointer deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting class A pointer from database");

        XCTAssertNotNil(keyset.tlk, @"should be a TLK");
        XCTAssertNotNil(keyset.classA, @"should be a classA key");
        XCTAssertNotNil(keyset.classC, @"should be a classC key");

        [keyset.tlk deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting TLK from database");

        [keyset.classA deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting classA from database");

        [keyset.classC deleteFromDatabase:&error];
        XCTAssertNil(error, "Should be no error deleting classC from database");

        return CKKSDatabaseTransactionCommit;
    }];

    // A restart should realize there's an issue, and pause for help
    // Ideally we'd try a refetch here to see if we're very wrong, but that's hard to avoid making into an infinite loop
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    [self.defaultCKKS beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // But, a resync should fix you back up
    CKKSSynchronizeOperation* resyncOperation = [self.defaultCKKS resyncWithCloud];
    [resyncOperation waitUntilFinished];
    XCTAssertNil(resyncOperation.error, "No error during the resync operation");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testRecoverFromCloudKitFetchFail {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // The first two CKRecordZoneChanges should fail with a 'network unavailable' error.
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}]];
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}]];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitFetchNetworkFailAfterReady {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Network is unavailable
    [self.reachabilityTracker setNetworkReachability:false];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    // Say network is available
    [self.reachabilityTracker setNetworkReachability:true];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];
}

- (void)testRecoverFromCloudKitFetchNetworkFailBeforeReady {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    // Network is unavailable
    [self.reachabilityTracker setNetworkReachability:false];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateInitializing] wait:20*NSEC_PER_SEC], "CKKS entered initializing");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateInitializing] wait:20*NSEC_PER_SEC], "CKKS state machine should enter initializing");

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    // Say network is available
    self.silentFetchesAllowed = false;
    [self expectCKFetch];
    [self.reachabilityTracker setNetworkReachability:true];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:8*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];
}

- (void)testWaitAfterCloudKitNetworkFailDuringOutgoingQueueOperation {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:8*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Network is now unavailable
    [self.reachabilityTracker setNetworkReachability:false];

    NSError* noNetwork = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{
                                                                                                                       CKErrorRetryAfterKey: @(0.2),
                                                                                                                       }];
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID blockAfterReject:nil withError:noNetwork];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:8*NSEC_PER_SEC], "CKKS state machine should enter ready");

    sleep(2);

    // Once network is available again, the write should happen
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.reachabilityTracker setNetworkReachability:true];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitFetchFailWithDelay {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // The first  CKRecordZoneChanges should fail with a 'delay' error.
    self.silentFetchesAllowed = false;
    [self.keychainZone failNextFetchWith:[[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorRequestRateLimited userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:4]}]];
    [self expectCKFetch];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Ensure it doesn't fetch within these three seconds (if it does, an exception will throw).
    sleep(3);

    // Okay, you can fetch again.
    [self expectCKFetch];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testHandleZoneDeletedWhileFetching {
    // Test starts with no keys in database, a key hierarchy in our fake CloudKit, and the TLK safely in the local keychain.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // The first CKRecordZoneChanges should fail with a 'zone not found' error (race between zone creation as part of initalization and zone deletion from another device)
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorZoneNotFound userInfo:@{}]];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testRecoverFromCloudKitOldChangeToken {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Delete all old database states, to destroy the change tag validity
    [self.keychainZone.pastDatabases removeAllObjects];

    // We expect a total local flush and refetch
    self.silentFetchesAllowed = false;
    [self expectCKFetch]; // one to fail with a CKErrorChangeTokenExpired error
    [self expectCKFetch]; // and one to succeed

    // Trigger a fake change notification
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And check that a new upload happens just fine.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitUnknownDeviceStateRecord {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Save a new device state record with some fake etag
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:self.ckDeviceID
                                                                       osVersion:@"fake-record"
                                                                  lastUnlockTime:[NSDate date]
                                                                   octagonPeerID:nil
                                                                   octagonStatus:nil
                                                                    circlePeerID:self.mockSOSAdapter.selfPeer.peerID
                                                                    circleStatus:kSOSCCInCircle
                                                                        keyState:SecCKKSZoneKeyStateWaitForTLK
                                                                  currentTLKUUID:nil
                                                               currentClassAUUID:nil
                                                               currentClassCUUID:nil
                                                                          zoneID:self.keychainZoneID
                                                                 encodedCKRecord:nil];
        XCTAssertNotNil(cdse, "Should have created a fake CDSE");
        CKRecord* record = [cdse CKRecordWithZoneID:self.keychainZoneID];
        XCTAssertNotNil(record, "Should have created a fake CDSE CKRecord");
        record.etag = @"fake etag";
        cdse.storedCKRecord = record;

        NSError* error = nil;
        [cdse saveToDatabase:&error];
        XCTAssertNil(error, @"No error saving cdse to database");

        return CKKSDatabaseTransactionCommit;
    }];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a record failure, since the device state record is broke
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    // And then we expect a clean write
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitUnknownItemRecord {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone:ckr];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    // Delete the record from CloudKit, but miss the notification
    XCTAssertNil([self.keychainZone deleteCKRecordIDFromZone: ckr.recordID], "Deleting the record from fake CloudKit should succeed");

    // Expect a failed upload when we modify the item
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self updateGenericPassword:@"never seen again" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // And the item should be disappeared from the local keychain
    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];
}

- (void)testRecoverFromCloudKitUserDeletedZone {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // The first CKRecordZoneChanges should fail with a 'CKErrorUserDeletedZone' error. This will cause a local reset, ending up with zone re-creation.
    self.zones[self.keychainZoneID] = nil; // delete the zone
    self.keys[self.keychainZoneID] = nil;
    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorUserDeletedZone userInfo:@{}]];

    // We expect CKKS to recreate the zone, then have octagon reupload the keys, and then the class C item upload
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And check that a new upload occurs.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitZoneNotFoundWithoutZoneDeletion {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Now, save the TLK to the keychain (to simulate it coming in later via SOS).
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    [self waitForCKModifications];

    // The next CKRecordZoneChanges will fail with a 'zone not found' error.
    self.zones[self.keychainZoneID] = nil; // delete the zone
    self.keys[self.keychainZoneID] = nil;

    // We expect CKKS to reset itself and recover, then have octagon upload the keys, and then the class C item upload
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // And check that a new upload occurs.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRecoverFromCloudKitZoneNotFoundFetchBeforeSigninOccurs {
    self.zones[self.keychainZoneID] = nil; // delete the autocreated zone

    // Before CKKS sign-in, it receives a fetch rpc
    XCTestExpectation *fetchReturns = [self expectationWithDescription:@"fetch returned"];
    [self.injectedManager rpcFetchAndProcessChanges:nil classA:false onlyIfNoRecentFetch:false reply:^(NSError *result) {
        XCTAssertNotNil(result, "Should be an error fetching and processing changes before processing has occurred");
        XCTAssertEqual(result.domain, CKKSResultErrorDomain, "Domain should be CKKSResultErrorDomain");
        XCTAssertEqual(result.code, CKKSResultSubresultError, "Code should be CKKSResultSubresultError");
        [fetchReturns fulfill];
    }];

    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // The fetch should have come back by now
    [self waitForExpectations: @[fetchReturns] timeout:5];
}

- (void)testNoCloudKitAccount {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    // simulate a NSNotification callback (but still logged out)
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Test that there are no items in the database (since we never logged in)
    [self checkNoCKKSDataForView:self.keychainView];
}

- (void)testSACloudKitAccount {
    // Test starts with nothing in database and the user logged into CloudKit and in circle, but the account is not HSA2.
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;

    self.accountStatus = CKAccountStatusAvailable;

    self.silentFetchesAllowed = false;

    // Octagon does not initialize the ckks views when not in an HSA2 account
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    [self startCKKSSubsystem];

    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS should enter 'waitforcloudkitaccount'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitforcloudkitaccount'");

    // There should be no uploads, even when we save keychain items and enter/exit circle
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Test that there are no items in the database (since we never were in an HSA2 account)
    [self checkNoCKKSDataForView:self.keychainView];
}

- (void)testEarlyLogin
{
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    // Octagon should initialize these views
    self.automaticallyBeginCKKSViewCloudKitOperation = true;

    self.accountStatus = CKAccountStatusAvailable;
    //[self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    [self startCKKSSubsystem];

    // CKKS should end up in 'waitfortlkcreation', as there's no trust and no TLKs
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    // Now, renotify the account status, and ensure that CKKS doesn't reenter 'initializing'
    CKKSCondition* keyMachineInitializing = self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateInitializing];
    CKKSCondition* ckksInitializing = self.defaultCKKS.stateConditions[CKKSStateInitializing];

    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    XCTAssertNotEqual(0, [keyMachineInitializing wait:500*NSEC_PER_MSEC], "CKKS key state machine should not enter initializing when the device HSA status changes");
    XCTAssertNotEqual(0, [ckksInitializing wait:500*NSEC_PER_MSEC], "CKKS state machine should not enter initializing when the device HSA status changes");
}

- (void)testNoCircle {
    // Test starts with nothing in database and the user logged into CloudKit, but out of Circle.
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    self.accountStatus = CKAccountStatusAvailable;

    [self startCKKSSubsystem];

    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    // simulate a NSNotification callback (but still logged out)
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'loggedout'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'loggedout'");

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Test that there are no items in the database (since we never logged in)
    [self checkNoCKKSDataForView:self.keychainView];
}

- (void)testCircleDepartAndRejoin {
    // Test starts with CKKS in ready
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    XCTAssertEqual(0, [self.defaultCKKS.trustStatusKnown wait:20*NSEC_PER_SEC], @"CKKS should know the trust status");

    // But then, trust departs
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.trustStatusKnown wait:20*NSEC_PER_SEC], @"CKKS should still know the trust status");

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    // Then trust returns. We expect two uploads
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testCloudKitLogin {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;

    // Before we inform CKKS of its account state....
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");

    [self startCKKSSubsystem];

    XCTAssertEqual(0,    [self.defaultCKKS.loggedOut wait:500*NSEC_PER_MSEC], "Should have been told of a 'logout' event on startup");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedIn wait:100*NSEC_PER_MSEC], "'login' event shouldn't have happened");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // simulate a cloudkit login and NSNotification callback
    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // No writes yet, since we're not in circle
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");
    XCTAssertEqual(0, [self.defaultCKKS.trustStatusKnown wait:20*NSEC_PER_SEC], @"CKKS should know the trust status");

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedOperationForAllViews];

    [self performOctagonTLKUpload:self.ckksViews];

    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
}

- (void)testCloudKitLogoutLogin {
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];
    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // simulate a cloudkit logout and NSNotification callback
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.defaultCKKS.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'logged out'");
    [self checkNoCKKSDataForView:self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'logged out'");

    // simulate a cloudkit login
    // We should expect CKKS to re-find the key hierarchy it already uploaded, and then send up the two records we added during the pause
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Let everything settle...
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    [self waitForCKModifications];

    // Logout again
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.defaultCKKS.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    [self checkNoCKKSDataForView:self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-5"];
    [self addGenericPassword: @"data" account: @"account-delete-me-6"];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'CKKSStateLoggedOut'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // simulate a cloudkit login
    // We should expect CKKS to re-find the key hierarchy it already uploaded, and then send up the two records we added during the pause
    [self expectCKModifyItemRecords: 2 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Let everything settle...
    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    // Logout again
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.defaultCKKS.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'logged out'");
    [self checkNoCKKSDataForView:self.keychainView];

    // Force state machine into error state
    OctagonStateTransitionOperation* transitionOp = [OctagonStateTransitionOperation named:@"enter" entering:CKKSStateError];
    OctagonStateTransitionRequest* request = [[OctagonStateTransitionRequest alloc] init:@"enter-wait-for-trust"
                                                                            sourceStates:CKKSAllStates()
                                                                             serialQueue:self.defaultCKKS.queue
                                                                                 timeout:10 * NSEC_PER_SEC
                                                                            transitionOp:transitionOp];
    [self.defaultCKKS.stateMachine handleExternalRequest:request];
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateError] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'error'");
    self.keychainView.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateError] wait:20*NSEC_PER_SEC], "CKKS entered 'error'");

    self.accountStatus = CKAccountStatusAvailable;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
}

- (void)testCloudKitLogoutDueToGreyMode {
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];
    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:20*NSEC_PER_SEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:50*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // simulate a cloudkit grey mode switch and NSNotification callback. CKKS should treat this as a logout
    self.iCloudHasValidCredentials = false;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    // Test that there are no items in the database after logout
    XCTAssertEqual(0,    [self.defaultCKKS.loggedOut wait:20*NSEC_PER_SEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedIn wait:50*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered 'logged out'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'logged out'");
    [self checkNoCKKSDataForView:self.keychainView];

    // There should be no further uploads, even when we save keychain items
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    [self addGenericPassword: @"data" account: @"account-delete-me-3"];

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'CKKSStateLoggedOut'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Also, fetches shouldn't occur
    self.silentFetchesAllowed = false;
    NSOperation* op = [self.defaultCKKS.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseTesting];
    CKKSResultOperation* timeoutOp = [CKKSResultOperation named:@"timeout" withBlock:^{}];
    [timeoutOp addDependency:op];
    [timeoutOp timeout:4*NSEC_PER_SEC];
    [self.operationQueue addOperation:timeoutOp];
    [timeoutOp waitUntilFinished];

    // CloudKit figures its life out. We expect the two passwords from before to be uploaded
    [self expectCKModifyItemRecords:2 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    self.silentFetchesAllowed = true;
    self.iCloudHasValidCredentials = true;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:20*NSEC_PER_SEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:50*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And fetching still works!
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D00" withAccount:@"account0"]];
    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
}

- (void)testCloudKitLoginRace {
    // Test starts with nothing in database, and 'in circle', but securityd hasn't received notification if we're logged into CloudKit.
    // CKKS should call handleLogout, as the CK account is not present.

    // note: don't unblock the ck account state object yet...

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // Add a keychain item, and make sure it doesn't upload yet.
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitforcloudkitaccountstatus'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitforcloudkitaccountstatus'");

    XCTAssertNotEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:1*NSEC_PER_SEC], "CKKS state machine shouldn't have entered 'loggedout'");
    XCTAssertNotEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:1*NSEC_PER_SEC], "CKKS state machine shouldn't have entered 'loggedout'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now that we're here (and logged out), bring the account up

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    // simulate another NSNotification callback
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Make sure new items upload too
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.defaultCKKS waitUntilAllOperationsAreFinished];
    [self waitForCKModifications];
    [self.defaultCKKS halt];
}

- (void)testDontLogOutIfBeforeFirstUnlock {
    /*
    // test starts as if a previously logged-in device has just rebooted
    self.aksLockState = true;
    self.accountStatus = CKAccountStatusAvailable;

    // This is the original state of the account tracker
    self.circleStatus = [[SOSAccountStatus alloc] init:kSOSCCError error:nil];
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // And this is what the first circle status fetch will actually return
    self.circleStatus = [[SOSAccountStatus alloc] init:kSOSCCError error:[NSError errorWithDomain:(__bridge id)kSOSErrorDomain code:kSOSErrorNotReady description:@"fake error: device is locked, so SOS doesn't know if it's in-circle"]];
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    XCTAssertEqual(self.accountStateTracker.currentComputedAccountStatus, CKKSAccountStatusUnknown, "Account tracker status should just be 'unknown'");
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKKS should not yet know the CK account state");

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.loggedIn wait:8*NSEC_PER_SEC], "'login' event should have happened");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:10*NSEC_PER_MSEC], "Should not have been told of a CK 'logout' event on startup");
    XCTAssertEqual(0, [self.defaultCKKS.accountStateKnown wait:1*NSEC_PER_SEC], "CKKS should know the account state");

    // And assume another CK status change
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    XCTAssertEqual(self.accountStateTracker.currentComputedAccountStatus, CKKSAccountStatusUnknown, "Account tracker status should just be 'no account'");
    XCTAssertEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKKS should know the CK account state");

    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    self.aksLockState = false;

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0,    [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We expect a single class C record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];*/
}

- (void)testSyncableItemsAddedWhileLoggedOut {
    // Test that once CKKS is up and 'logged out', nothing happens when syncable items are added
    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];

    XCTAssertEqual([self.defaultCKKS.loggedOut wait:500*NSEC_PER_MSEC], 0, "CKKS should be told that it's logged out");

    // CKKS shouldn't decide to poke its state machine, but it should still send the notification
    XCTestExpectation* viewChangeNotification = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    [self waitForExpectations:@[viewChangeNotification] timeout:8];
}

- (void)testUploadSyncableItemsAddedWhileUntrusted {
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual([self.defaultCKKS.loggedIn wait:500*NSEC_PER_MSEC], 0, "CKKS should be told that it's logged in");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self addGenericPassword: @"data" account: @"account-delete-me-2"];

    NSError* error = nil;
    NSDictionary* currentOQEs = [CKKSOutgoingQueueEntry countsByStateInZone:self.keychainZoneID error:&error];
    XCTAssertNil(error, "Should be no error counting OQEs");
    XCTAssertEqual(0, currentOQEs.count, "Should be no OQEs");

    // Now, insert a restart to simulate securityd restarting (and throwing away all pending operations), then a real sign in
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    [self endSOSTrustedViewOperation:self.defaultCKKS];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    // Okay! Upon sign in, this item should be uploaded
    [self expectCKModifyItemRecords:1
                     deletedRecords:0
           currentKeyPointerRecords:1
                             zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]
         expectedOperationGroupName:@"restart-setup"];

    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testSyncableItemAddedOnDaemonRestartBeforePolicyLoaded {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSyncWithoutSettingPolicy:self.defaultCKKS];

    // This item addition shouldn't be uploaded yet, or in any queues
    [self addGenericPassword:@"data" account:@"account-delete-me-2"];

    NSError* error = nil;
    NSDictionary* currentOQEs = [CKKSOutgoingQueueEntry countsByStateInZone:self.keychainZoneID error:&error];
    XCTAssertNil(error, "Should be no error counting OQEs");
    XCTAssertEqual(0, currentOQEs.count, "Should be no OQEs");

    [self.defaultCKKS setCurrentSyncingPolicy:self.viewSortingPolicyForManagedViewList];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    // end of daemon restart

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    [self.defaultCKKS beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

// Note that this test assumes that the keychainView object was created at daemon restart.
// I don't really know how to write a test for that...
- (void)testSyncableItemAddedOnDaemonRestartBeforeCloudKitAccountKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];

    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:100*NSEC_PER_MSEC], "CKKS should still have no idea what the account state is");
    XCTAssertEqual(self.defaultCKKS.accountStatus, CKKSAccountStatusUnknown, "Account status should be unknown");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS entered 'waitforcloudkitaccount'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitforcloudkitaccount'");

    [self.defaultCKKS beginCloudKitOperation];

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testSyncableItemModifiedOnDaemonRestartBeforeCloudKitAccountKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    [self updateGenericPassword:@"newdata" account: @"account-delete-me-2"];
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:100*NSEC_PER_MSEC], "CKKS should still have no idea what the account state is");
    XCTAssertEqual(self.defaultCKKS.accountStatus, CKKSAccountStatusUnknown, "Account status should be unknown");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS entered 'waitforcloudkitaccount'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitforcloudkitaccount'");

    [self.defaultCKKS beginCloudKitOperation];

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testSyncableItemDeletedOnDaemonRestartBeforeCloudKitAccountKnown {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Daemon restarts
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS.operationDependencies viewStateForName:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    [self deleteGenericPassword:@"account-delete-me-2"];
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:100*NSEC_PER_MSEC], "CKKS should still have no idea what the account state is");
    XCTAssertEqual(self.defaultCKKS.accountStatus, CKKSAccountStatusUnknown, "Account status should be unknown");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS entered 'waitforcloudkitaccount'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForCloudKitAccountStatus] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitforcloudkitaccount'");

    [self.defaultCKKS beginCloudKitOperation];

    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testNotStuckAfterReset {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // And handle a spurious logout
    [self.defaultCKKS handleCKLogout];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
}

- (void)testCKKSControlBringup {
    NSXPCInterface *interface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    XCTAssertNotNil(interface, "Received a configured CKKS interface");
}

- (void)testMetricsUpload {

    XCTestExpectation *upload = [self expectationWithDescription:@"CAMetrics"];
    XCTestExpectation *collection = [self expectationWithDescription:@"CAMetrics"];

    id saMock = OCMClassMock([SecCoreAnalytics class]);
    OCMStub([saMock sendEvent:[OCMArg any] event:[OCMArg any]]).andDo(^(NSInvocation* invocation) {
        [upload fulfill];
    });

    NSString *sampleSampler = @"stuff";

    [[CKKSAnalytics logger] AddMultiSamplerForName:sampleSampler withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
        [collection fulfill];
        return @{ @"hej" : @1 };
    }];


    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];

    [self.injectedManager xpc24HrNotification];

    [self waitForExpectations: @[upload, collection] timeout:10];
    [[CKKSAnalytics logger] removeMultiSamplerForName:sampleSampler];
}

- (void)testSaveManyTLKShares {
    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self performOctagonTLKUpload:self.ckksViews];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    NSMutableArray<CKKSSOSSelfPeer*>* peers = [NSMutableArray array];

    for(int i = 0; i < 20; i++) {
        CKKSSOSSelfPeer* untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:[NSString stringWithFormat:@"untrusted-peer-%d", i]
                                                                      encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                         signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                                           viewList:self.managedViewList];

        [peers addObject:untrustedPeer];
    }

    NSMutableArray<CKRecord*>* tlkShareRecords = [NSMutableArray array];

    for(CKKSSOSSelfPeer* peer1 in peers) {
        for(CKKSSOSSelfPeer* peer2 in peers) {
            NSError* error = nil;
            CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:[self.keychainZoneKeys.tlk getKeychainBackedKey:&error]
                                                               as:peer1
                                                               to:peer2
                                                            epoch:-1
                                                         poisoned:0
                                                            error:&error];
            XCTAssertNil(error, "Should have been no error sharing a CKKSKey");
            XCTAssertNotNil(share, "Should be able to create a share");

            CKRecord* shareRecord = [share CKRecordWithZoneID:self.keychainZoneID];
            [tlkShareRecords addObject:shareRecord];
        }
    }

    [self measureBlock:^{
        [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            for(CKRecord* record in tlkShareRecords) {
                [self.defaultCKKS.operationDependencies intransactionCKRecordChanged:record resync:false];
            }
            return CKKSDatabaseTransactionCommit;
        }];
    }];
}

- (void)testReceiveNotificationDuringLaunch {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    [self holdCloudKitModifyRecordZones];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    CKKSCondition* fetcherCondition = self.defaultCKKS.zoneChangeFetcher.fetchScheduler.liveRequestReceived;

    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    [self.injectedManager.zoneChangeFetcher notifyZoneChange:nil];

    XCTAssertNotEqual(0, [fetcherCondition wait:(3 * NSEC_PER_SEC)], "not supposed to get a fetch data");

    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    self.silentFetchesAllowed = false;
    [self expectCKFetch];
    [self releaseCloudKitModifyRecordZonesHold];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testEnsureCloudKitDoesNotReceiveUUIDPersistentReference {

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should have arrived at ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter ready");

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    NSArray<CKRecord*>* items = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    XCTAssertEqual(items.count, 1, "Have %d Items in cloudkit", 1);

    CKRecord* delete = items[0];
    NSDictionary* item = [self decryptRecord: delete];

    NSString* deleteAccount = item[(id) kSecAttrAccount];
    XCTAssertNotNil(deleteAccount, "received an account for the local delete object");
    XCTAssertEqualObjects(deleteAccount, @"account-delete-me", "accounts should be the same");

    NSData* pref = item[(id) kSecAttrPersistentReference];
    XCTAssertNil(pref, "persistent reference uuid should be nil");

    NSData* prefValue = item[(id) kSecValuePersistentRef];
    XCTAssertNil(prefValue, "persistent reference value should be nil");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

#pragma mark - Helpers

/// Returns an operation with the given name and block that runs when the CKKS
/// state machine enters the "ready" state. Tests use this helper to ensure that
/// the state machine runs a full cycle when poked, instead of relying on its
/// cached (and asynchronously invalidated) current state.
- (CKKSResultOperation *)promiseEnteredReadyWithName:(NSString *)name blockTakingWatcherResult:(void(^)(CKKSResultOperation *))block {
    OctagonStateTransitionPath *path = [OctagonStateTransitionPath pathFromDictionary:@{
        CKKSStateReady: [OctagonStateTransitionPathStep success],
    }];

    OctagonStateTransitionWatcher *watcher = [[OctagonStateTransitionWatcher alloc] initNamed:name
                                                                                  serialQueue:self.defaultCKKS.queue
                                                                                         path:path
                                                                               initialRequest:nil];
    [watcher timeout:20*NSEC_PER_SEC];
    [self.defaultCKKS.stateMachine registerStateTransitionWatcher:watcher];

    CKKSResultOperation *finishOp = [CKKSResultOperation named:[name stringByAppendingString:@"-finish"] withBlock:^{
        block(watcher.result);
    }];
    [finishOp addDependency:watcher.result];
    [self.operationQueue addOperation:finishOp];

    return finishOp;
}

@end

#endif // OCTAGON
