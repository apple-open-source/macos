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

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <XCTest/XCTest.h>

#import <Foundation/Foundation.h>
#import <SystemConfiguration/SystemConfiguration.h>

#import "keychain/ckks/tests/CKKSMockSOSPresentAdapter.h"
#import "keychain/ckks/tests/CKKSMockOctagonAdapter.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ot/OTManager.h"
#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKey;
@class CKKSCKRecordHolder;
@class CKKSKeychainView;
@class CKKSViewManager;
@class FakeCKZone;
@class CKKSLockStateTracker;
@class CKKSReachabilityTracker;
@class SOSCKKSPeerAdapter;

@interface CKKSTestFailureLogger : NSObject <XCTestObservation>
@end

@interface CloudKitMockXCTest : XCTestCase

@property CKRecordZoneID* testZoneID;

@property (nullable) id mockDatabase;
@property (nullable) id mockDatabaseExceptionCatcher;
@property (nullable) id mockContainer;
@property (nullable) id mockContainerExpectations;
@property (nullable) id mockFakeCKModifyRecordZonesOperation;
@property (nullable) id mockFakeCKModifySubscriptionsOperation;
@property (nullable) id mockFakeCKFetchRecordZoneChangesOperation;
@property (nullable) id mockFakeCKFetchRecordsOperation;
@property (nullable) id mockFakeCKQueryOperation;

@property (nullable) id mockAccountStateTracker;

@property (nullable) id mockTTR;
@property BOOL isTTRRatelimited;
@property (nullable) XCTestExpectation *ttrExpectation;

// The CloudKit account status
@property CKAccountStatus accountStatus;

// The current HSA2-ness of the world
// Set to 'unknown' to not inject any answer into
@property CKKSAccountStatus fakeHSA2AccountStatus;

@property BOOL iCloudHasValidCredentials;

@property (readonly) NSString* ckDeviceID;
@property (readonly) CKKSAccountStateTracker* accountStateTracker;


@property NSString* apsEnvironment;

@property bool aksLockState;  // The current 'AKS lock state'
@property (readonly) CKKSLockStateTracker* lockStateTracker;
@property (nullable) id mockLockStateTracker;

@property (readonly) CKKSReachabilityTracker *reachabilityTracker;

@property (nullable) NSMutableDictionary<CKRecordZoneID*, FakeCKZone*>* zones;

@property NSOperationQueue* operationQueue;
@property (nullable) NSBlockOperation* ckaccountHoldOperation;

@property (nullable) NSBlockOperation* ckModifyHoldOperation;
@property (nullable) NSBlockOperation* ckFetchHoldOperation;
@property (nullable) NSBlockOperation* ckModifyRecordZonesHoldOperation;
@property (nullable) NSBlockOperation* ckModifySubscriptionsHoldOperation;

@property bool silentFetchesAllowed;
@property bool silentZoneDeletesAllowed;

@property CKKSMockSOSPresentAdapter* mockSOSAdapter;
@property (nullable) CKKSMockOctagonAdapter *mockOctagonAdapter;

- (NSSet<NSString*>*)managedViewList;
- (TPSyncingPolicy*)viewSortingPolicyForManagedViewList;
- (TPSyncingPolicy*)viewSortingPolicyForManagedViewListWithUserControllableViews:(NSSet<NSString*>*)ucv
                                                       syncUserControllableViews:(TPPBPeerStableInfo_UserControllableViewStatus)syncUserControllableViews;

@property (nullable) id mockCKKSViewManager;
@property (nullable) CKKSViewManager* injectedManager;

// Injected into CKKSViewManager using OCMock
@property BOOL overrideUseCKKSViewsFromPolicy;

// Set this to true before calling -setup if you're going to configure the ckks view manager yourself
// !disable format used because this will be initialized to false, and so will happen unless subclasses take positive action
@property BOOL disableConfigureCKKSViewManagerWithViews;

// Set this to true to override CKKSViewsFromPolicy to NO instead of the default YES
// !disable format used because this will be initialized to false, and so will happen unless subclasses take positive action
@property BOOL setCKKSViewsFromPolicyToNo;

@property (nullable) OTManager* injectedOTManager;

// Fill this in to fail the next modifyzones operation
@property (nullable) NSError* nextModifyRecordZonesError;

// Used to track the test failure logger (for test teardown purposes)
@property (class) CKKSTestFailureLogger* testFailureLogger;

- (CKKSKey*)fakeTLK:(CKRecordZoneID*)zoneID;

- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID;
- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID
                        checkItem:(BOOL (^_Nullable)(CKRecord*))checkItem;

- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfModifiedRecords
                   deletedRecords:(NSUInteger)expectedNumberOfDeletedRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID
                        checkItem:(BOOL (^_Nullable)(CKRecord*))checkItem;

- (void)expectCKDeleteItemRecords:(NSUInteger)expectedNumberOfRecords zoneID:(CKRecordZoneID*)zoneID;

- (void)expectCKModifyKeyRecords:(NSUInteger)expectedNumberOfRecords
        currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                 tlkShareRecords:(NSUInteger)expectedTLKShareRecords
                          zoneID:(CKRecordZoneID*)zoneID;
- (void)expectCKModifyKeyRecords:(NSUInteger)expectedNumberOfRecords
        currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                 tlkShareRecords:(NSUInteger)expectedTLKShareRecords
                          zoneID:(CKRecordZoneID*)zoneID
             checkModifiedRecord:(BOOL (^_Nullable)(CKRecord*))checkModifiedRecord;

- (void)expectCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*)expectedRecordTypeCounts
      deletedRecordTypeCounts:(NSDictionary<NSString*, NSNumber*>* _Nullable)expectedDeletedRecordTypeCounts
                       zoneID:(CKRecordZoneID*)zoneID
          checkModifiedRecord:(BOOL (^_Nullable)(CKRecord*))checkRecord
         runAfterModification:(void (^_Nullable)(void))afterModification;

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID;
- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID
                                      blockAfterReject:(void (^_Nullable)(void))blockAfterReject;
- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID
                                      blockAfterReject:(void (^_Nullable)(void))blockAfterReject
                                             withError:(NSError* _Nullable)error;
- (void)expectCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID;

- (void)failNextZoneCreation:(CKRecordZoneID*)zoneID;
- (void)failNextZoneCreationSilently:(CKRecordZoneID*)zoneID;
- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID;
- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID withError:(NSError*)error;

- (NSError* _Nullable)shouldFailModifyRecordZonesOperation;
- (void)ensureZoneDeletionAllowed:(FakeCKZone*)zone;

// Override this to interpose on how an OTManager is created
// This will be called during setUp, after the CloudKit mocks are in-place
// This needs to fill in the injectedOTManager variable on this class
- (OTManager*)setUpOTManager:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies;

// Use this to assert that a fetch occurs (especially if silentFetchesAllowed = false)
- (void)expectCKFetch;

// Use this to 1) assert that a fetch occurs and 2) cause a block to run _after_ all changes have been delivered but _before_ the fetch 'completes'.
// This way, you can modify the CK zone to cause later collisions.
- (void)expectCKFetchAndRunBeforeFinished:(void (^_Nullable)(void))blockAfterFetch;

// Introspect the fetch object before allowing it to proceed
- (void)expectCKFetchWithFilter:(BOOL (^)(FakeCKFetchRecordZoneChangesOperation*))operationMatch
              runBeforeFinished:(void (^)(void))blockAfterFetch;

// Use this to assert that a FakeCKFetchRecordsOperation occurs.
- (void)expectCKFetchByRecordID;

// Use this to assert that a FakeCKQueryOperation occurs.
- (void)expectCKFetchByQuery;

// Wait until all scheduled cloudkit operations are reflected in the currentDatabase
- (void)waitForCKModifications;

// Unblocks the CKAccount mock subsystem. Until this is called, the tests believe cloudd hasn't returned any account status yet.
- (void)startCKAccountStatusMock;

// Starts everything.
- (void)startCKKSSubsystem;

// Blocks the completion (partial or full) of CloudKit modifications
- (void)holdCloudKitModifications;

// Unblocks the hold you've added with holdCloudKitModifications; CloudKit modifications will finish
- (void)releaseCloudKitModificationHold;

// Blocks the CloudKit fetches from beginning (similar to network latency)
- (void)holdCloudKitFetches;
// Unblocks the hold you've added with holdCloudKitFetches; CloudKit fetches will finish
- (void)releaseCloudKitFetchHold;

- (void)holdCloudKitModifyRecordZones;
- (void)releaseCloudKitModifyRecordZonesHold;

- (void)holdCloudKitModifySubscription;
- (void)releaseCloudKitModifySubscriptionHold;


// Make a CK internal server extension error with a given code and description.
- (NSError*)ckInternalServerExtensionError:(NSInteger)code description:(NSString*)desc;

// Schedule an operation for execution (and failure), with some existing record errors.
// Other records in the operation but not in failedRecords will have CKErrorBatchRequestFailed errors created.
- (void)rejectWrite:(CKModifyRecordsOperation*)op failedRecords:(NSMutableDictionary<CKRecordID*, NSError*>*)failedRecords;

@end

NS_ASSUME_NONNULL_END

#endif /* OCTAGON */
