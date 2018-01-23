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
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"

@interface CloudKitKeychainSyncingServerValidationRecoveryTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainSyncingServerValidationRecoveryTests

/* Tests for CKKSServerUnexpectedSyncKeyInChain */

- (void)testRecoverFromWrongClassACurrentKeyPointersOnStartup {
    // The current key pointers in cloudkit should always point directly under the top TLK.

    // Test starts with a broken key hierarchy in our fake CloudKit, and the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKReference* oldClassAKey = self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey];
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    CKReference* newClassAKey = self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey];

    // Break the reference
    self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = oldClassAKey;
    self.keychainZoneKeys.currentClassAPointer.currentKeyUUID = oldClassAKey.recordID.recordName;

    // CKKS should then fix the pointers and give itself a TLK share, but not update any keys
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 1 tlkShareRecords: 1 zoneID:self.keychainZoneID];

    // And then upload the record as normal
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self waitForCKModifications];
    XCTAssertEqualObjects(newClassAKey, self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassAPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey], "CKKS should have fixed up the broken class A key reference");
}

- (void)testRecoverFromWrongClassCCurrentKeyPointersOnStartup {
    // The current key pointers in cloudkit should always point directly under the top TLK.

    // Test starts with a broken key hierarchy in our fake CloudKit, and the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKReference* oldClassCKey = self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey];
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    CKReference* newClassCKey = self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey];

    // Break the reference
    self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = oldClassCKey;
    self.keychainZoneKeys.currentClassCPointer.currentKeyUUID = oldClassCKey.recordID.recordName;

    // CKKS should then fix the pointers and its TLK shares, but not update any keys
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:1 tlkShareRecords:1 zoneID:self.keychainZoneID];

    // And then upload the record as normal
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self waitForCKModifications];
    XCTAssertEqualObjects(newClassCKey, self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey], "CKKS should have fixed up the broken class C key reference");
}

- (void)testRecoverFromWrongClassCCurrentKeyPointersOnNotification {
    // The current key pointers in cloudkit should always point directly under the top TLK.

    // Test starts with a good key hierarchy in our fake CloudKit, and the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    CKReference* oldClassCKey = self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Uploading works
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Break the key hierarchy
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKReference* newClassCKey = self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey];
    self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey] = oldClassCKey;
    self.keychainZoneKeys.currentClassCPointer.currentKeyUUID = oldClassCKey.recordID.recordName;

    [self.keychainView notifyZoneChange:nil];

    // CKKS should then fix the pointers and give itself a new TLK share record, but not update any keys
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:1 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And then upload the item as usual
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self waitForCKModifications];
    XCTAssertEqualObjects(newClassCKey, self.keychainZone.currentDatabase[self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID][SecCKRecordParentKeyRefKey], "CKKS should have fixed up the broken class C key reference");
}

- (void)testRecoverFromWrongClassCCurrentKeyPointersOnNotificationFixRejected {
    // The current key pointers in cloudkit should always point directly under the top TLK.

    // Test starts with a good key hierarchy in our fake CloudKit, and the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    CKRecordID* classCPointerRecordID = self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID;

    CKReference* oldClassCKey = self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Uploading works
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Break the key hierarchy
    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKReference* newClassCKey = self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey];
    CKRecord* classCPointer = self.keychainZone.currentDatabase[classCPointerRecordID];

    CKRecord* brokenClassCPointer = [classCPointer copy];
    brokenClassCPointer[SecCKRecordParentKeyRefKey] = oldClassCKey;
    self.keychainZoneKeys.currentClassCPointer.currentKeyUUID = oldClassCKey.recordID.recordName;
    [self.keychainZone addCKRecordToZone:brokenClassCPointer];

    self.silentFetchesAllowed = false;
    [self expectCKFetchAndRunBeforeFinished: ^{
        // Directly after CKKS fetches, we should fix up the pointers to be right again
        self.keychainZoneKeys.currentClassCPointer.currentKeyUUID = newClassCKey.recordID.recordName;
        [self.keychainZone addToZone: classCPointer];
        XCTAssertEqualObjects(newClassCKey, self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey], "CKKS should have fixed up the broken class C key reference");
        self.silentFetchesAllowed = true;
    }];

    // CKKS should try to fix the pointers, but be rejected (since someone else has already fixed them)
    // It should not try to modify the pointers again, but it should give itself the new TLK
    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // And then use the 'new' key as it should
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self waitForCKModifications];
    XCTAssertEqualObjects(newClassCKey, self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey], "CKKS should have fixed up the broken class C key reference");
}

- (void)testRecoverFromWrongClassCCurrentKeyPointersOnRecordWrite {
    // The current key pointers in cloudkit should always point directly under the top TLK.

    // Test starts with a good but rolled key hierarchy in our fake CloudKit, and the TLK already arrived.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    CKRecordID* classCPointerRecordID = self.keychainZoneKeys.currentClassCPointer.storedCKRecord.recordID;
    CKReference* oldClassCKey = self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey];

    [self rollFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Uploading works
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    // Now, break the class C pointer, but don't tell CKKS
    CKReference* newClassCKey = self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey];
    CKRecord* classCPointer = self.keychainZone.currentDatabase[classCPointerRecordID];

    CKRecord* brokenClassCPointer = [classCPointer copy];
    brokenClassCPointer[SecCKRecordParentKeyRefKey] = oldClassCKey;
    self.keychainZoneKeys.currentClassCPointer.currentKeyUUID = oldClassCKey.recordID.recordName;
    [self.keychainZone addCKRecordToZone:brokenClassCPointer];

    // CKKS should receive a key hierarchy error, since it's wrong in CloudKit
    // It should then fix the pointers and retry the upload
    [self expectCKReceiveSyncKeyHierarchyError:self.keychainZoneID];
    [self expectCKModifyKeyRecords: 0 currentKeyPointerRecords: 1 tlkShareRecords: 0 zoneID:self.keychainZoneID];

    // And then use the 'new' key as it should
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-2"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self waitForCKModifications];
    XCTAssertEqualObjects(newClassCKey, self.keychainZone.currentDatabase[classCPointerRecordID][SecCKRecordParentKeyRefKey], "CKKS should have fixed up the broken class C key reference");
}

@end

#endif // OCTAGON
