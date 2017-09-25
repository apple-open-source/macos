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

#import <XCTest/XCTest.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import <Foundation/Foundation.h>

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/CKKSCKAccountStateTracker.h"

@class CKKSKey;
@class CKKSCKRecordHolder;
@class CKKSKeychainView;
@class CKKSViewManager;
@class FakeCKZone;
@class CKKSLockStateTracker;

@interface CloudKitMockXCTest : XCTestCase

#if OCTAGON

@property CKRecordZoneID* testZoneID;

@property id mockDatabase;
@property id mockContainer;
@property id mockFakeCKModifyRecordZonesOperation;
@property id mockFakeCKModifySubscriptionsOperation;
@property id mockFakeCKFetchRecordZoneChangesOperation;

@property id mockAccountStateTracker;

@property CKAccountStatus accountStatus;
@property BOOL supportsDeviceToDeviceEncryption;
@property SOSCCStatus circleStatus;
@property (readonly) NSString* ckDeviceID;
@property (readonly) CKKSCKAccountStateTracker* accountStateTracker;

@property NSString* circlePeerID;

@property bool aksLockState; // The current 'AKS lock state'
@property (readonly) CKKSLockStateTracker* lockStateTracker;
@property id mockLockStateTracker;

@property NSMutableDictionary<CKRecordZoneID*, FakeCKZone*>* zones;

@property NSOperationQueue* operationQueue;
@property NSBlockOperation* ckksHoldOperation;
@property NSBlockOperation* ckaccountHoldOperation;

@property NSBlockOperation* ckModifyHoldOperation;

@property bool silentFetchesAllowed;

@property id mockCKKSViewManager;
@property CKKSViewManager* injectedManager;

- (CKKSKey*) fakeTLK: (CKRecordZoneID*)zoneID;

- (void)expectCKModifyItemRecords: (NSUInteger) expectedNumberOfRecords
         currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords
                           zoneID: (CKRecordZoneID*) zoneID;
- (void)expectCKModifyItemRecords: (NSUInteger) expectedNumberOfRecords
         currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords
                           zoneID: (CKRecordZoneID*) zoneID
                        checkItem: (BOOL (^)(CKRecord*)) checkItem;

- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfModifiedRecords
                   deletedRecords:(NSUInteger)expectedNumberOfDeletedRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID
                        checkItem:(BOOL (^)(CKRecord*))checkItem;

- (void)expectCKDeleteItemRecords: (NSUInteger) expectedNumberOfRecords zoneID: (CKRecordZoneID*) zoneID;

- (void)expectCKModifyKeyRecords: (NSUInteger) expectedNumberOfRecords
        currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords
                          zoneID: (CKRecordZoneID*) zoneID;

- (void)expectCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*) expectedRecordTypeCounts
      deletedRecordTypeCounts:(NSDictionary<NSString*, NSNumber*>*) expectedDeletedRecordTypeCounts
                       zoneID:(CKRecordZoneID*) zoneID
          checkModifiedRecord:(BOOL (^)(CKRecord*)) checkRecord
         runAfterModification:(void (^) ())afterModification;

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID;
- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID blockAfterReject: (void (^)())blockAfterReject;
- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID blockAfterReject: (void (^)())blockAfterReject withError:(NSError*)error;
- (void)expectCKAtomicModifyItemRecordsUpdateFailure: (CKRecordZoneID*) zoneID;

- (void)failNextZoneCreation:(CKRecordZoneID*)zoneID;
- (void)failNextZoneCreationSilently:(CKRecordZoneID*)zoneID;
- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID;
- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID withError:(NSError*)error;

// Use this to assert that a fetch occurs (especially if silentFetchesAllowed = false)
- (void)expectCKFetch;

// Wait until all scheduled cloudkit operations are reflected in the currentDatabase
- (void)waitForCKModifications;

// Unblocks the CKKS subsystem only.
- (void)startCKKSSubsystemOnly;

// Unblocks the CKAccount mock subsystem. Until this is called, the tests believe cloudd hasn't returned any account status yet.
- (void)startCKAccountStatusMock;

// Starts everything.
- (void)startCKKSSubsystem;

// Blocks the completion (partial or full) of CloudKit modifications
-(void)holdCloudKitModifications;

// Unblocks the hold you've added with holdCloudKitModifications; CloudKit modifications will finish
-(void)releaseCloudKitModificationHold;

#endif // OCTAGON
@end
