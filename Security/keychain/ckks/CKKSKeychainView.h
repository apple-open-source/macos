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
#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CloudKitDependencies.h"

#import "keychain/ot/OctagonFlags.h"
#import "keychain/ot/OctagonStateMachine.h"

#include "keychain/securityd/SecDbItem.h"
#include "utilities/SecDb.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSIncomingQueueOperation.h"
#import "keychain/ckks/CKKSKeychainViewState.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSOutgoingQueueOperation.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ckks/CKKSProcessReceivedKeysOperation.h"
#import "keychain/ckks/CKKSReencryptOutgoingItemsOperation.h"
#import "keychain/ckks/CKKSScanLocalItemsOperation.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSZoneModifier.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSLocalSynchronizeOperation.h"
#import "keychain/ckks/CKKSProvideKeySetOperation.h"
#import "keychain/ckks/CKKSOperationDependencies.h"
#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"

#include "CKKS.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKey;
@class CKKSAESSIVKey;
@class CKKSSynchronizeOperation;
@class CKKSRateLimiter;
@class CKKSOutgoingQueueEntry;
@class CKKSZoneChangeFetcher;
@class CKKSCurrentKeySet;
@class CKKSSecDbAdapter;

@interface CKKSKeychainView : NSObject <CKKSCloudKitAccountStateListener,
                                        CKKSChangeFetcherClient,
                                        CKKSPeerUpdateListener,
                                        CKKSDatabaseProviderProtocol,
                                        OctagonStateMachineEngine>

@property CKKSAccountStatus accountStatus;
@property (readonly) CKContainer* container;
@property CKKSAccountStateTracker* accountTracker;
@property (readonly) CKKSReachabilityTracker* reachabilityTracker;
@property (readonly) CKKSCloudKitClassDependencies* cloudKitClassDependencies;
@property (readonly) dispatch_queue_t queue;

@property CKKSCondition* loggedIn;
@property CKKSCondition* loggedOut;
@property CKKSCondition* accountStateKnown;

@property CKKSAccountStatus trustStatus;
@property CKKSCondition* trustStatusKnown;

@property CKKSLockStateTracker* lockStateTracker;

@property (readonly) OctagonStateMachine* stateMachine;

@property (readonly, nullable) TPSyncingPolicy* syncingPolicy;

// Returns the names of the currently active CKKS-managed views. Used mainly in tests.
@property (readonly) NSSet<NSString*>* viewList;

@property (readonly) NSDate* earliestFetchTime;

// If the key hierarchy isn't coming together, it might be because we're out of sync with cloudkit.
// Use this to track if we've completed a full refetch, so fix-up operations can be done.
@property bool keyStateMachineRefetched;

// Set this to request a key state refetch (tests only)
@property bool keyStateFullRefetchRequested;

// Full of condition variables, if you'd like to try to wait until the key hierarchy is in some state
@property (readonly) NSDictionary<CKKSState*, CKKSCondition*>* stateConditions;

@property CKKSZoneChangeFetcher* zoneChangeFetcher;

@property (nullable) CKKSNearFutureScheduler* suggestTLKUpload;

/* Used for debugging: just what happened last time we ran this? */
@property (nullable) CKKSIncomingQueueOperation* lastIncomingQueueOperation;
@property (nullable) CKKSNewTLKOperation* lastNewTLKOperation;
@property (nullable) CKKSOutgoingQueueOperation* lastOutgoingQueueOperation;
@property (nullable) CKKSProcessReceivedKeysOperation* lastProcessReceivedKeysOperation;
@property (nullable) CKKSReencryptOutgoingItemsOperation* lastReencryptOutgoingItemsOperation;
@property (nullable) CKKSSynchronizeOperation* lastSynchronizeOperation;
@property (nullable) CKKSResultOperation* lastFixupOperation;

/* Used for testing: pause operation types by adding operations here */
@property (nullable) NSOperation* holdOutgoingQueueOperation;
@property (nullable) NSOperation* holdIncomingQueueOperation;
@property (nullable) NSOperation* holdLocalSynchronizeOperation;

@property (readonly) NSString* zoneName;

/* Used for testing */
@property BOOL initiatedLocalScan;

@property (readonly) CKKSOperationDependencies* operationDependencies;

- (instancetype)initWithContainer:(CKContainer*)container
                   accountTracker:(CKKSAccountStateTracker*)accountTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                    changeFetcher:(CKKSZoneChangeFetcher*)fetcher
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
                 savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies;

/* Trust state management */

// suggestTLKUpload and requestPolicyCheck are essentially callbacks to request certain procedures from the view owner.
// When suggestTLKUpload is triggered, the CKKS view believes it has some new TLKs that need uploading, and Octagon should take care of them.
// When requestPolicyCheck is triggered, the CKKS view would like Octagon to perform a live check on which syncing policy is in effect,
//  successfully retrieving all peer's opinions, and would like -setCurrentSyncingPolicy to be called with the updated policy (even if it is
//  unchanged.)
- (void)beginTrustedOperation:(NSArray<id<CKKSPeerProvider>>*)peerProviders
             suggestTLKUpload:(CKKSNearFutureScheduler*)suggestTLKUpload
           requestPolicyCheck:(CKKSNearFutureScheduler*)requestPolicyCheck;

- (void)endTrustedOperation;

/* CloudKit account management */

- (void)beginCloudKitOperation;

// Call this to set the syncing views+policy that this CKKS instance will use.
// If beginCloudKitOperationOfAllViews has previously been called, then any new views created
// as a result of this call will begin CK operation.
- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy;

// Similar to above, but please only pass policyIsFresh=YES if Octagon has contacted cuttlefish immediately previously
// Returns YES if the view set has changed as part of this set
- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy policyIsFresh:(BOOL)policyIsFresh;

/* Synchronous operations */

- (void)handleKeychainEventDbConnection:(SecDbConnectionRef)dbconn
                                 source:(SecDbTransactionSource)txionSource
                                  added:(SecDbItemRef _Nullable)added
                                deleted:(SecDbItemRef _Nullable)deleted
                            rateLimiter:(CKKSRateLimiter*)rateLimiter;

- (void)setCurrentItemForAccessGroup:(NSData*)newItemPersistentRef
                                hash:(NSData*)newItemSHA1
                         accessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                            viewHint:(NSString*)viewHint
                           replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                                hash:(NSData* _Nullable)oldItemSHA1
                            complete:(void (^)(NSError* operror))complete;

- (void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                            viewHint:(NSString*)viewHint
                     fetchCloudValue:(bool)fetchCloudValue
                            complete:(void (^)(NSString* uuid, NSError* operror))complete;

- (bool)outgoingQueueEmpty:(NSError* __autoreleasing*)error;

- (CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*)findKeySets:(BOOL)refetchBeforeReturningKeySet;
- (void)receiveTLKUploadRecords:(NSArray<CKRecord*>*)records;

// Returns true if this zone would like a new TLK to be uploaded
- (NSSet<CKKSKeychainViewState*>*)viewsRequiringTLKUpload;

- (void)cancelAllOperations;

/* Asynchronous kickoffs */

- (CKKSResultOperation*)rpcProcessOutgoingQueue:(NSSet<NSString*>* _Nullable)viewNames operationGroup:(CKOperationGroup* _Nullable)ckoperationGroup;

- (CKKSResultOperation*)rpcFetchBecause:(CKKSFetchBecause*)why;
- (CKKSResultOperation*)rpcFetchAndProcessIncomingQueue:(NSSet<NSString*>* _Nullable)viewNames
                                                because:(CKKSFetchBecause*)why
                                   errorOnClassAFailure:(bool)failOnClassA;

// This will wait for the next incoming queue operation to occur. If the zone is in a bad state, this will time out.
- (CKKSResultOperation*)rpcProcessIncomingQueue:(NSSet<NSString*>* _Nullable)viewNames
                           errorOnClassAFailure:(bool)failOnClassA;

- (CKKSResultOperation*)rpcWaitForPriorityViewProcessing;

- (void)scanLocalItems;

// This operation will complete directly after the next ProcessIncomingQueue, and should supply that IQO's result. Used mainly for testing; otherwise you'd just kick off a IQO directly.
- (CKKSResultOperation*)resultsOfNextProcessIncomingQueueOperation;

// Schedules an operation to update this device's state record in CloudKit
// If rateLimit is true, the operation will abort if it's updated the record in the past 3 days
- (CKKSUpdateDeviceStateOperation*)updateDeviceState:(bool)rateLimit
                   waitForKeyHierarchyInitialization:(uint64_t)timeout
                                    ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup;

- (CKKSSynchronizeOperation*)resyncWithCloud;
- (CKKSLocalSynchronizeOperation*)resyncLocal;

// Call this to tell the key state machine that you think some new data has arrived that it is interested in
- (void)keyStateMachineRequestProcess;

/* Client RPC calls */

- (CKKSResultOperation*)rpcResetLocal:(NSSet<NSString*>* _Nullable)viewNames reply:(void(^)(NSError* _Nullable result))reply;
- (CKKSResultOperation*)rpcResetCloudKit:(NSSet<NSString*>* _Nullable)viewNames reply:(void(^)(NSError* _Nullable result))reply;

// Returns the current state of this view, fastStatus is the same, but as name promises, no expensive calculations

- (void)rpcStatus:(NSString* _Nullable)viewName
        fast:(BOOL)fast
        waitForNonTransientState:(dispatch_time_t)nonTransientStateTimeout
        reply:(void(^)(NSArray<NSDictionary*>* _Nullable result, NSError* _Nullable error))reply;

- (BOOL)waitUntilReadyForRPCForOperation:(NSString*)opName
                                    fast:(BOOL)fast
                errorOnNoCloudKitAccount:(BOOL)errorOnNoCloudKitAccount
                    errorOnPolicyMissing:(BOOL)errorOnPolicyMissing
                                   error:(NSError**)error;

- (void)xpc24HrNotification;

- (void)toggleHavoc:(void (^)(BOOL havoc, NSError* _Nullable error))reply;

- (void)pcsMirrorKeysForServices:(NSDictionary<NSNumber*,NSArray<NSData*>*>*)services
                           reply:(void (^)(NSDictionary<NSNumber*,NSArray<NSData*>*>* _Nullable result,
                                           NSError* _Nullable error))reply;

// NSOperation Helpers
- (void)scheduleOperation:(NSOperation*)op;

- (NSArray<NSString*>*)viewsForPeerID:(NSString*)peerID error:(NSError**)error;

@end

@interface CKKSKeychainView (Testing)

// If set, any set passed to setCurrentSyncingPolicy will be intersected with this set
@property (readonly, nullable) NSSet<NSString*>* viewAllowList;
- (void)setSyncingViewsAllowList:(NSSet<NSString*>* _Nullable)viewNames;

- (CKKSKeychainViewState* _Nullable)viewStateForName:(NSString*)viewName NS_SWIFT_NAME(viewState(name:));

// Call this to just nudge the state machine (without a request)
// This is used internally, but you should only call it if you're a test.
- (void)_onqueuePokeKeyStateMachine;

/* NSOperation helpers */
- (void)cancelAllOperations;
- (BOOL)waitForKeyHierarchyReadiness;
- (BOOL)waitUntilAllOperationsAreFinished;
- (void)waitForOperationsOfClass:(Class)operationClass;

- (BOOL)waitForFetchAndIncomingQueueProcessing;

- (void)halt;

- (void)handleCKLogout;


@property (readonly) CKKSSecDbAdapter* databaseProvider;
@end

NS_ASSUME_NONNULL_END
#else   // !OCTAGON
#import <Foundation/Foundation.h>
@interface CKKSKeychainView : NSObject
{
    NSString* _containerName;
}
@end
#endif  // OCTAGON
