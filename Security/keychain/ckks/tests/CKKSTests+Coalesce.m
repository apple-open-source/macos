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
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CloudKitMockXCTest.h"

// Break abstraction.
@interface CKKSKeychainView(test)
@property NSOperationQueue* operationQueue;
@end

@implementation CloudKitKeychainSyncingTests (CoalesceTests)
// These tests check that, if CKKS doesn't start processing an item before a new update comes in,
// each case is properly handled.

- (void)testCoalesceAddModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];
    [self updateGenericPassword: @"otherdata" account:account];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
}

- (void)testCoalesceAddModifyModifyItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];
    [self updateGenericPassword: @"otherdata" account:account];
    [self updateGenericPassword: @"again" account:account];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
}

- (void)testCoalesceAddModifyDeleteItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];
    [self updateGenericPassword: @"otherdata" account:account];
    [self deleteGenericPassword: account];

    // We expect no uploads.
    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
}

- (void)testCoalesceDeleteAddItem {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    NSString* account = @"account-delete-me";

    [self addGenericPassword: @"data" account: account];

    // We expect a single record to be uploaded.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
    [self waitForCKModifications];

    // Okay, now the delete/add. Note that this is not a coalescing operation, since the new item
    // will have a completely different UUID, and so will delete the old record and upload the new one.

    [self.keychainView.operationQueue waitUntilAllOperationsAreFinished];
    self.keychainView.operationQueue.suspended = YES;
    [self deleteGenericPassword: account];
    [self addGenericPassword: @"data" account: account];

    [self expectCKModifyRecords:@{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: 1],
                                  SecCKRecordCurrentKeyType: [NSNumber numberWithUnsignedInteger: 1],
                                  SecCKRecordDeviceStateType: [NSNumber numberWithUnsignedInteger: 1],
                                  }
        deletedRecordTypeCounts:@{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: 1]}
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
    self.keychainView.operationQueue.suspended = NO;
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
}

@end

#endif
