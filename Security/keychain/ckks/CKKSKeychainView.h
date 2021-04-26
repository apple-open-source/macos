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

#import "keychain/analytics/CKKSLaunchSequence.h"
#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CloudKitDependencies.h"

#import "keychain/ot/OctagonFlags.h"
#import "keychain/ot/OctagonStateMachine.h"

#include "keychain/securityd/SecDbItem.h"
#include <utilities/SecDb.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSIncomingQueueOperation.h"
#import "keychain/ckks/CKKSKeychainViewState.h"
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

@interface CKKSKeychainView : NSObject <CKKSCloudKitAccountStateListener,
                                        CKKSChangeFetcherClient,
                                        CKKSPeerUpdateListener,
                                        CKKSDatabaseProviderProtocol,
                                        OctagonStateMachineEngine>

// CKKS is in the middle of a transition period, where this class will take ownership
// of multiple CKKS views and CK zones.
// The following properties are intended for use in that transition, and should eventually disappear
@property (readonly) NSString* zoneName;
@property (readonly) CKRecordZoneID* zoneID;
@property CKKSKeychainViewState* viewState;

// This will hold every view currently active.
@property NSSet<CKKSKeychainViewState*>* viewStates;

@property CKKSAccountStatus accountStatus;
@property (readonly) CKContainer* container;
@property (weak) CKKSAccountStateTracker* accountTracker;
@property (weak) CKKSReachabilityTracker* reachabilityTracker;
@property (readonly) CKKSCloudKitClassDependencies* cloudKitClassDependencies;
@property (readonly) dispatch_queue_t queue;

@property CKKSCondition* loggedIn;
@property CKKSCondition* loggedOut;
@property CKKSCondition* accountStateKnown;

@property CKKSAccountStatus trustStatus;

@property (nullable) CKKSLaunchSequence *launch;

@property CKKSLockStateTracker* lockStateTracker;

// Is this view currently syncing keychain modifications?
@property (readonly) BOOL itemSyncingEnabled;

@property (readonly) OctagonStateMachine* stateMachine;

// If the key hierarchy isn't coming together, it might be because we're out of sync with cloudkit.
// Use this to track if we've completed a full refetch, so fix-up operations can be done.
@property bool keyStateMachineRefetched;

// Set this to request a key state refetch (tests only)
@property bool keyStateFullRefetchRequested;

@property (nullable) CKKSResultOperation* keyStateReadyDependency;

// Full of condition variables, if you'd like to try to wait until the key hierarchy is in some state
@property (readonly) NSDictionary<CKKSZoneKeyState*, CKKSCondition*>* keyHierarchyConditions;

@property CKKSZoneChangeFetcher* zoneChangeFetcher;

@property (nullable) CKKSNearFutureScheduler* suggestTLKUpload;
@property (nullable) CKKSNearFutureScheduler* requestPolicyCheck;

/* Used for debugging: just what happened last time we ran this? */
@property CKKSIncomingQueueOperation* lastIncomingQueueOperation;
@property CKKSNewTLKOperation* lastNewTLKOperation;
@property CKKSOutgoingQueueOperation* lastOutgoingQueueOperation;
@property CKKSProcessReceivedKeysOperation* lastProcessReceivedKeysOperation;
@property CKKSReencryptOutgoingItemsOperation* lastReencryptOutgoingItemsOperation;
@property CKKSSynchronizeOperation* lastSynchronizeOperation;
@property CKKSResultOperation* lastFixupOperation;

/* Used for testing: pause operation types by adding operations here */
@property NSOperation* holdReencryptOutgoingItemsOperation;
@property NSOperation* holdOutgoingQueueOperation;
@property NSOperation* holdIncomingQueueOperation;
@property NSOperation* holdLocalSynchronizeOperation;
@property CKKSResultOperation* holdFixupOperation;

/* Used for testing */
@property BOOL initiatedLocalScan;

@property (readonly) CKKSOperationDependencies* operationDependencies;

- (instancetype)initWithContainer:(CKContainer*)container
                         zoneName:(NSString*)zoneName
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

// If this policy indicates that this view should not sync, this view will no longer sync keychain items,
// but it will continue to particpate in TLK sharing.
// If policyIsFresh is set, any items discovered that do not match this policy will be moved.
- (void)setCurrentSyncingPolicy:(TPSyncingPolicy*)syncingPolicy policyIsFresh:(BOOL)policyIsFresh;

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
                           replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                                hash:(NSData* _Nullable)oldItemSHA1
                            complete:(void (^)(NSError* operror))complete;

- (void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                     fetchCloudValue:(bool)fetchCloudValue
                            complete:(void (^)(NSString* uuid, NSError* operror))complete;

- (bool)outgoingQueueEmpty:(NSError* __autoreleasing*)error;

- (CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*)findKeySet:(BOOL)refetchBeforeReturningKeySet;
- (void)receiveTLKUploadRecords:(NSArray<CKRecord*>*)records;

// Returns true if this zone would like a new TLK to be uploaded
- (BOOL)requiresTLKUpload;

- (void)waitForKeyHierarchyReadiness;
- (void)cancelAllOperations;

/* Asynchronous kickoffs */

- (CKKSOutgoingQueueOperation*)processOutgoingQueue:(CKOperationGroup* _Nullable)ckoperationGroup;
- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation* _Nullable)after
                                        ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup;
- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation* _Nullable)after
                                           requiredDelay:(uint64_t)requiredDelay
                                        ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup;

- (CKKSIncomingQueueOperation*)processIncomingQueue:(bool)failOnClassA;
- (CKKSIncomingQueueOperation*)processIncomingQueue:(bool)failOnClassA after:(CKKSResultOperation* _Nullable)after;

- (CKKSScanLocalItemsOperation*)scanLocalItems:(NSString*)name;

// Schedules a process queueoperation to happen after the next device unlock. This may be Immediately, if the device is unlocked.
- (void)processIncomingQueueAfterNextUnlock;

// This operation will complete directly after the next ProcessIncomingQueue, and should supply that IQO's result. Used mainly for testing; otherwise you'd just kick off a IQO directly.
- (CKKSResultOperation*)resultsOfNextProcessIncomingQueueOperation;

// Schedules an operation to update this device's state record in CloudKit
// If rateLimit is true, the operation will abort if it's updated the record in the past 3 days
- (CKKSUpdateDeviceStateOperation*)updateDeviceState:(bool)rateLimit
                   waitForKeyHierarchyInitialization:(uint64_t)timeout
                                    ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup;

- (CKKSSynchronizeOperation*)resyncWithCloud;
- (CKKSLocalSynchronizeOperation*)resyncLocal;

- (CKKSResultOperation*)resetLocalData;
- (CKKSResultOperation*)resetCloudKitZone:(CKOperationGroup*)operationGroup;

// Call this to tell the key state machine that you think some new data has arrived that it is interested in
- (void)keyStateMachineRequestProcess;

// For our serial queue to work with how handleKeychainEventDbConnection is called from the main thread,
// every block on our queue must have a SecDBConnectionRef available to it before it begins on the queue.
// Use these helper methods to make sure those exist.
- (void)dispatchSyncWithSQLTransaction:(CKKSDatabaseTransactionResult (^)(void))block;
- (void)dispatchSyncWithReadOnlySQLTransaction:(void (^)(void))block;

/* Synchronous operations which must be called from inside a dispatchAsyncWithAccountKeys or dispatchSync block */

- (bool)_onqueueCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords;
- (bool)_onqueueCKRecordChanged:(CKRecord*)record resync:(bool)resync;
- (bool)_onqueueCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync;

- (CKKSDeviceStateEntry* _Nullable)_onqueueCurrentDeviceStateEntry:(NSError* __autoreleasing*)error;

// Please don't use these unless you're an Operation in this package
@property NSHashTable<CKKSIncomingQueueOperation*>* incomingQueueOperations;
@property NSHashTable<CKKSOutgoingQueueOperation*>* outgoingQueueOperations;

@property NSHashTable<CKKSScanLocalItemsOperation*>* scanLocalItemsOperations;

// Returns the current state of this view, fastStatus is the same, but as name promise, no expensive calculations
- (NSDictionary<NSString*, NSString*>*)status;
- (NSDictionary<NSString*, NSString*>*)fastStatus;

- (void)xpc24HrNotification;

// NSOperation Helpers
- (void)scheduleOperation:(NSOperation*)op;
@end

@interface CKKSKeychainView (Testing)

// Call this to just nudge the state machine (without a request)
// This is used internally, but you should only call it if you're a test.
- (void)_onqueuePokeKeyStateMachine;

/* NSOperation helpers */
- (void)cancelAllOperations;
- (void)waitUntilAllOperationsAreFinished;
- (void)waitForOperationsOfClass:(Class)operationClass;

- (void)waitForFetchAndIncomingQueueProcessing;

- (void)halt;

- (void)handleCKLogout;

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
