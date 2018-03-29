/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
#import "keychain/ckks/CKKSViewManager.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"

// break abstraction
@interface CKKSLockStateTracker ()
@property (nullable) NSDate* lastUnlockedTime;
@end


@interface CloudKitKeychainSyncingDeviceStateUploadTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainSyncingDeviceStateUploadTests

- (void)testDeviceStateUploadGood {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateReady), "Device is in ready");

                    XCTAssertEqualObjects([record[SecCKRecordCurrentTLK]    recordID].recordName, zoneKeys.tlk.uuid, "Correct TLK uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassA] recordID].recordName, zoneKeys.classA.uuid, "Correct class A uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassC] recordID].recordName, zoneKeys.classC.uuid, "Correct class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:2*NSEC_PER_SEC ckoperationGroup:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadRateLimited {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateReady), "Device is in ready");

                    XCTAssertEqualObjects([record[SecCKRecordCurrentTLK]    recordID].recordName, zoneKeys.tlk.uuid, "Correct TLK uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassA] recordID].recordName, zoneKeys.classA.uuid, "Correct class A uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassC] recordID].recordName, zoneKeys.classC.uuid, "Correct class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    CKKSUpdateDeviceStateOperation* op = [self.keychainView updateDeviceState:true waitForKeyHierarchyInitialization:2*NSEC_PER_SEC ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [op waitUntilFinished];

    // Check that an immediate rate-limited retry doesn't upload anything
    op = [self.keychainView updateDeviceState:true waitForKeyHierarchyInitialization:2*NSEC_PER_SEC ckoperationGroup:nil];
    [op waitUntilFinished];

    // But not rate-limiting works just fine!
    [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
    op = [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:2*NSEC_PER_SEC ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [op waitUntilFinished];

    // And now, if the update is old enough, that'll work too
    [self.keychainView dispatchSync:^bool {
        NSError* error = nil;
        CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry fromDatabase:self.accountStateTracker.ckdeviceID zoneID:self.keychainZoneID error:&error];
        XCTAssertNil(error, "No error fetching device state entry");
        XCTAssertNotNil(cdse, "Fetched device state entry");

        CKRecord* record = cdse.storedCKRecord;

        NSDate* m = record.modificationDate;
        XCTAssertNotNil(m, "Have modification date");

        // Four days ago!
        NSDateComponents* offset = [[NSDateComponents alloc] init];
        [offset setHour:-4 * 24];
        NSDate* m2 = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:m options:0];

        XCTAssertNotNil(m2, "Made modification date");

        record.modificationDate = m2;
        [cdse setStoredCKRecord:record];

        [cdse saveToDatabase:&error];
        XCTAssertNil(error, "No error saving device state entry");

        return true;
    }];

    // And now the rate-limiting doesn't get in the way
    [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
    op = [self.keychainView updateDeviceState:true waitForKeyHierarchyInitialization:2*NSEC_PER_SEC ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 12);
    [op waitUntilFinished];
}

- (void)testDeviceStateUploadRateLimitedAfterNormalUpload {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self addGenericPassword:@"password" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Check that an immediate rate-limited retry doesn't upload anything
    CKKSUpdateDeviceStateOperation* op = [self.keychainView updateDeviceState:true waitForKeyHierarchyInitialization:2*NSEC_PER_SEC ckoperationGroup:nil];
    [op waitUntilFinished];
}

- (void)testDeviceStateUploadWaitsForKeyHierarchyReady {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Ask to wait for quite a while if we don't become ready
    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:20*NSEC_PER_SEC ckoperationGroup:nil];

    __weak __typeof(self) weakSelf = self;
    // Expect a ready upload
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateReady), "Device is in ready");

                    XCTAssertEqualObjects([record[SecCKRecordCurrentTLK]    recordID].recordName, zoneKeys.tlk.uuid, "Correct TLK uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassA] recordID].recordName, zoneKeys.classA.uuid, "Correct class A uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassC] recordID].recordName, zoneKeys.classC.uuid, "Correct class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    // And allow the key state to progress
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadWaitsForKeyHierarchyWaitForTLK {
    // This test has stuff in CloudKit, but no TLKs. It should become very sad.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Ask to wait for the key state to enter a state if we don't become ready
    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:20*NSEC_PER_SEC ckoperationGroup:nil];

    __weak __typeof(self) weakSelf = self;
    // Expect a waitfortlk upload
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID should matche what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device should be in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForTLK), "Device should be in waitfortlk");

                    XCTAssertNil([record[SecCKRecordCurrentTLK]    recordID].recordName, "Should have no TLK uuid");
                    XCTAssertNil([record[SecCKRecordCurrentClassA] recordID].recordName, "Should have no class A uuid");
                    XCTAssertNil([record[SecCKRecordCurrentClassC] recordID].recordName, "Should have no class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    // And allow the key state to progress
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], "CKKS entered waitfortlk");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateReceive {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    ZoneKeys* zoneKeys = self.keys[self.keychainZoneID];
    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", self.keychainZoneID);

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    NSDate* date = [[NSCalendar currentCalendar] startOfDayForDate:[NSDate date]];
    CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initForDevice:@"otherdevice"
                                                                   osVersion:@"fake-version"
                                                              lastUnlockTime:date
                                                                circlePeerID:@"asdfasdf"
                                                                circleStatus:kSOSCCInCircle
                                                                    keyState:SecCKKSZoneKeyStateReady
                                                              currentTLKUUID:zoneKeys.tlk.uuid
                                                           currentClassAUUID:zoneKeys.classA.uuid
                                                           currentClassCUUID:zoneKeys.classC.uuid
                                                                      zoneID:self.keychainZoneID
                                                             encodedCKRecord:nil];
    CKRecord* record = [cdse CKRecordWithZoneID:self.keychainZoneID];
    [self.keychainZone addToZone:record];

    CKKSDeviceStateEntry* oldcdse = [[CKKSDeviceStateEntry alloc] initForDevice:@"olderotherdevice"
                                                                      osVersion:nil // old-style, no OSVersion or lastUnlockTime
                                                                 lastUnlockTime:nil
                                                                   circlePeerID:@"olderasdfasdf"
                                                                   circleStatus:kSOSCCInCircle
                                                                       keyState:SecCKKSZoneKeyStateReady
                                                                 currentTLKUUID:zoneKeys.tlk.uuid
                                                              currentClassAUUID:zoneKeys.classA.uuid
                                                              currentClassCUUID:zoneKeys.classC.uuid
                                                                         zoneID:self.keychainZoneID
                                                                encodedCKRecord:nil];
    [self.keychainZone addToZone:[oldcdse CKRecordWithZoneID:self.keychainZoneID]];

    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        NSArray<CKKSDeviceStateEntry*>* cdses = [CKKSDeviceStateEntry allInZone:self.keychainZoneID error:&error];
        XCTAssertNil(error, "No error fetching CDSEs");
        XCTAssertNotNil(cdses, "An array of CDSEs was returned");
        XCTAssert(cdses.count >= 1u, "At least one CDSE came back");

        CKKSDeviceStateEntry* item = nil;
        CKKSDeviceStateEntry* olderotherdevice = nil;
        for(CKKSDeviceStateEntry* dbcdse in cdses) {
            if([dbcdse.device isEqualToString:@"otherdevice"]) {
                item = dbcdse;
            } else if([dbcdse.device isEqualToString:@"olderotherdevice"]) {
                olderotherdevice = dbcdse;
            }
        }
        XCTAssertNotNil(item, "Found a cdse for otherdevice");

        XCTAssertEqualObjects(cdse, item, "Saved item matches pre-cloudkit item");

        XCTAssertEqualObjects(item.osVersion,         @"fake-version",      "correct osVersion");
        XCTAssertEqualObjects(item.lastUnlockTime,    date,                 "correct date");
        XCTAssertEqualObjects(item.circlePeerID,      @"asdfasdf",          "correct peer id");
        XCTAssertEqualObjects(item.keyState, SecCKKSZoneKeyStateReady,      "correct key state");
        XCTAssertEqualObjects(item.currentTLKUUID,    zoneKeys.tlk.uuid,    "correct tlk uuid");
        XCTAssertEqualObjects(item.currentClassAUUID, zoneKeys.classA.uuid, "correct classA uuid");
        XCTAssertEqualObjects(item.currentClassCUUID, zoneKeys.classC.uuid, "correct classC uuid");


        XCTAssertNotNil(olderotherdevice, "Should have found a cdse for olderotherdevice");
        XCTAssertEqualObjects(oldcdse, olderotherdevice, "Saved item should match pre-cloudkit item");

        XCTAssertNil(olderotherdevice.osVersion,                                        "osVersion should be nil");
        XCTAssertNil(olderotherdevice.lastUnlockTime,                                   "lastUnlockTime should be nil");
        XCTAssertEqualObjects(olderotherdevice.circlePeerID,      @"olderasdfasdf",     "correct peer id");
        XCTAssertEqualObjects(olderotherdevice.keyState, SecCKKSZoneKeyStateReady,      "correct key state");
        XCTAssertEqualObjects(olderotherdevice.currentTLKUUID,    zoneKeys.tlk.uuid,    "correct tlk uuid");
        XCTAssertEqualObjects(olderotherdevice.currentClassAUUID, zoneKeys.classA.uuid, "correct classA uuid");
        XCTAssertEqualObjects(olderotherdevice.currentClassCUUID, zoneKeys.classC.uuid, "correct classC uuid");

        return false;
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadBadKeyState {
    // This test has stuff in CloudKit, but no TLKs. It should become very sad.
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateWaitForTLK, "CKKS entered waitfortlk");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForTLK), "Device is in waitfortlk");

                    XCTAssertNil(record[SecCKRecordCurrentTLK]   , "No TLK");
                    XCTAssertNil(record[SecCKRecordCurrentClassA], "No class A key");
                    XCTAssertNil(record[SecCKRecordCurrentClassC], "No class C key");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:500*NSEC_PER_MSEC ckoperationGroup:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadWaitForUnlockKeyState {
    // Starts with everything in keychain, but locked
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    NSDateComponents *dateComponents = [[NSDateComponents alloc] init];
    [dateComponents setDay:-3];
    NSDate* threeDaysAgo = [[NSCalendar currentCalendar] dateByAddingComponents:dateComponents toDate:[NSDate date] options:0];

    self.aksLockState = true;
    [self.lockStateTracker recheck];
    self.lockStateTracker.lastUnlockedTime = threeDaysAgo;
    XCTAssertTrue([self.utcCalendar isDate:self.lockStateTracker.lastUnlockTime
                               equalToDate:threeDaysAgo
                         toUnitGranularity:NSCalendarUnitSecond],
                  "last unlock date (%@) similar to threeDaysAgo (%@)", self.lockStateTracker.lastUnlockTime, threeDaysAgo);

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForUnlock] wait:8*NSEC_PER_SEC], "CKKS entered waitforunlock");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:threeDaysAgo toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to three days ago (%@)", record[SecCKSRecordLastUnlockTime], threeDaysAgo);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForUnlock), "Device is in waitforunlock");

                    XCTAssertNil(record[SecCKRecordCurrentTLK]   , "No TLK");
                    XCTAssertNil(record[SecCKRecordCurrentClassA], "No class A key");
                    XCTAssertNil(record[SecCKRecordCurrentClassC], "No class C key");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:500*NSEC_PER_MSEC ckoperationGroup:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (void)testDeviceStateUploadBadKeyStateAfterRestart {
    // This test has stuff in CloudKit, but no TLKs. It should become very sad.
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateWaitForTLK, "CKKS entered waitfortlk");

    // And restart CKKS...
    self.keychainView = [[CKKSViewManager manager] restartZone: self.keychainZoneID.zoneName];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:8*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateWaitForTLK, "CKKS entered waitfortlk");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateWaitForTLK), "Device is in waitfortlk");

                    XCTAssertNil(record[SecCKRecordCurrentTLK]   , "No TLK");
                    XCTAssertNil(record[SecCKRecordCurrentClassA], "No class A key");
                    XCTAssertNil(record[SecCKRecordCurrentClassC], "No class C key");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:500*NSEC_PER_MSEC ckoperationGroup:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}


- (void)testDeviceStateUploadBadCircleState {
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // This test has stuff in CloudKit, but no TLKs.
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:8*NSEC_PER_SEC], "CKKS entered logged out");
    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateLoggedOut, "CKKS thinks it's logged out");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertNil(record[SecCKRecordCirclePeerID], "no peer ID if device is not in circle");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCNotInCircle], "device is not in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateLoggedOut), "Device is in keystate:loggedout");

                    XCTAssertNil(record[SecCKRecordCurrentTLK]   , "No TLK");
                    XCTAssertNil(record[SecCKRecordCurrentClassA], "No class A key");
                    XCTAssertNil(record[SecCKRecordCurrentClassC], "No class C key");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];

    CKKSUpdateDeviceStateOperation* op = [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:500*NSEC_PER_MSEC ckoperationGroup:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [op waitUntilFinished];
    XCTAssertNil(op.error, "No error uploading 'out of circle' device state");
}

- (void)testDeviceStateUploadWithTardyNetworkAfterRestart {
    // Test starts with a key hierarchy in cloudkit and the TLK having arrived
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self holdCloudKitFetches];

    [self startCKKSSubsystem];

    // we should be stuck in fetch
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateFetch] wait:8*NSEC_PER_SEC], "Key state should become fetch");

    __weak __typeof(self) weakSelf = self;
    [self expectCKModifyRecords: @{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordDeviceStateType]) {
                    // Check that all the things matches
                    __strong __typeof(weakSelf) strongSelf = weakSelf;
                    XCTAssertNotNil(strongSelf, "self exists");

                    ZoneKeys* zoneKeys = strongSelf.keys[strongSelf.keychainZoneID];
                    XCTAssertNotNil(zoneKeys, "Have zone keys for %@", strongSelf.keychainZoneID);

                    XCTAssertEqualObjects(record[SecCKSRecordOSVersionKey], SecCKKSHostOSVersion(), "os version string should match current OS version");
                    XCTAssertTrue([self.utcCalendar isDate:record[SecCKSRecordLastUnlockTime] equalToDate:[NSDate date] toUnitGranularity:NSCalendarUnitDay],
                                  "last unlock date (%@) similar to Now (%@)", record[SecCKSRecordLastUnlockTime], [NSDate date]);

                    XCTAssertEqualObjects(record[SecCKRecordCirclePeerID], strongSelf.circlePeerID, "peer ID matches what we gave it");
                    XCTAssertEqualObjects(record[SecCKRecordCircleStatus], [NSNumber numberWithInt:kSOSCCInCircle], "device is in circle");
                    XCTAssertEqualObjects(record[SecCKRecordKeyState], CKKSZoneKeyToNumber(SecCKKSZoneKeyStateReady), "Device is in ready");

                    XCTAssertEqualObjects([record[SecCKRecordCurrentTLK]    recordID].recordName, zoneKeys.tlk.uuid, "Correct TLK uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassA] recordID].recordName, zoneKeys.classA.uuid, "Correct class A uuid");
                    XCTAssertEqualObjects([record[SecCKRecordCurrentClassC] recordID].recordName, zoneKeys.classC.uuid, "Correct class C uuid");
                    return YES;
                } else {
                    return NO;
                }
            }
           runAfterModification:nil];


    [self.keychainView updateDeviceState:false waitForKeyHierarchyInitialization:8*NSEC_PER_SEC ckoperationGroup:nil];

    XCTAssertEqualObjects(self.keychainView.keyHierarchyState, SecCKKSZoneKeyStateFetch, "CKKS re-entered fetch");
    [self releaseCloudKitFetchHold];

    OCMVerifyAllWithDelay(self.mockDatabase, 16);
}



@end

#endif // OCTAGON
