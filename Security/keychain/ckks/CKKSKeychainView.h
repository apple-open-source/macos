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

#import "keychain/ckks/CKKSAPSReceiver.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CloudKitDependencies.h"

#include <securityd/SecDbItem.h>
#include <utilities/SecDb.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSIncomingQueueOperation.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSOutgoingQueueOperation.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSProcessReceivedKeysOperation.h"
#import "keychain/ckks/CKKSReencryptOutgoingItemsOperation.h"
#import "keychain/ckks/CKKSScanLocalItemsOperation.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSZone.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSLocalSynchronizeOperation.h"

#include "CKKS.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKey;
@class CKKSAESSIVKey;
@class CKKSSynchronizeOperation;
@class CKKSRateLimiter;
@class CKKSManifest;
@class CKKSEgoManifest;
@class CKKSOutgoingQueueEntry;
@class CKKSZoneChangeFetcher;

@interface CKKSKeychainView : CKKSZone <CKKSZoneUpdateReceiver, CKKSChangeFetcherClient, CKKSPeerUpdateListener>
{
    CKKSZoneKeyState* _keyHierarchyState;
}

@property CKKSCondition* loggedIn;
@property CKKSCondition* loggedOut;
@property CKKSCondition* accountStateKnown;

@property CKKSLockStateTracker* lockStateTracker;

@property CKKSZoneKeyState* keyHierarchyState;
@property (nullable) NSError* keyHierarchyError;
@property (nullable) CKOperationGroup* keyHierarchyOperationGroup;
@property (nullable) NSOperation* keyStateMachineOperation;

// If the key hierarchy isn't coming together, it might be because we're out of sync with cloudkit.
// Use this to track if we've completed a full refetch, so fix-up operations can be done.
@property bool keyStateMachineRefetched;
@property (nullable) CKKSEgoManifest* egoManifest;
@property (nullable) CKKSManifest* latestManifest;
@property (nullable) CKKSResultOperation* keyStateReadyDependency;

// Wait for the key state to become 'nontransient': no pending operation is expected to advance it (at least until user intervenes)
@property (nullable) CKKSResultOperation* keyStateNonTransientDependency;

// True if we believe there's any items in the keychain which haven't been brought up in CKKS yet
@property bool droppedItems;

@property (readonly) NSString* lastActiveTLKUUID;

// Full of condition variables, if you'd like to try to wait until the key hierarchy is in some state
@property NSMutableDictionary<CKKSZoneKeyState*, CKKSCondition*>* keyHierarchyConditions;

@property CKKSZoneChangeFetcher* zoneChangeFetcher;

@property (weak) CKKSNearFutureScheduler* savedTLKNotifier;

/* Used for debugging: just what happened last time we ran this? */
@property CKKSIncomingQueueOperation* lastIncomingQueueOperation;
@property CKKSNewTLKOperation* lastNewTLKOperation;
@property CKKSOutgoingQueueOperation* lastOutgoingQueueOperation;
@property CKKSProcessReceivedKeysOperation* lastProcessReceivedKeysOperation;
@property CKKSReencryptOutgoingItemsOperation* lastReencryptOutgoingItemsOperation;
@property CKKSScanLocalItemsOperation* lastScanLocalItemsOperation;
@property CKKSSynchronizeOperation* lastSynchronizeOperation;
@property CKKSResultOperation* lastFixupOperation;

/* Used for testing: pause operation types by adding operations here */
@property NSOperation* holdReencryptOutgoingItemsOperation;
@property NSOperation* holdOutgoingQueueOperation;
@property NSOperation* holdIncomingQueueOperation;
@property NSOperation* holdLocalSynchronizeOperation;
@property CKKSResultOperation* holdFixupOperation;

/* Trigger this to tell the whole machine that this view has changed */
@property CKKSNearFutureScheduler* notifyViewChangedScheduler;

/* trigger this to request key state machine poking */
@property CKKSNearFutureScheduler* pokeKeyStateMachineScheduler;

// These are available when you're in a dispatchSyncWithAccountKeys call, but at no other time
// These must be pre-fetched before you get on the CKKS queue, otherwise we end up with CKKS<->SQLite<->SOSAccountQueue deadlocks
@property (nonatomic, readonly) CKKSSelves* currentSelfPeers;
@property (nonatomic, readonly) NSError* currentSelfPeersError;
@property (nonatomic, readonly) NSSet<id<CKKSPeer>>* currentTrustedPeers;
@property (nonatomic, readonly) NSError* currentTrustedPeersError;

- (instancetype)initWithContainer:(CKContainer*)container
                                zoneName:(NSString*)zoneName
                          accountTracker:(CKKSCKAccountStateTracker*)accountTracker
                        lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                     reachabilityTracker:(CKKSReachabilityTracker *)reachabilityTracker
                        changeFetcher:(CKKSZoneChangeFetcher*)fetcher
                        savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
                            peerProvider:(id<CKKSPeerProvider>)peerProvider
    fetchRecordZoneChangesOperationClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
              fetchRecordsOperationClass:(Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                     queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
       modifySubscriptionsOperationClass:(Class<CKKSModifySubscriptionsOperation>)modifySubscriptionsOperationClass
         modifyRecordZonesOperationClass:(Class<CKKSModifyRecordZonesOperation>)modifyRecordZonesOperationClass
                      apsConnectionClass:(Class<CKKSAPSConnection>)apsConnectionClass
                           notifierClass:(Class<CKKSNotifier>)notifierClass;

/* Synchronous operations */

- (void)handleKeychainEventDbConnection:(SecDbConnectionRef)dbconn
                                  added:(SecDbItemRef _Nullable)added
                                deleted:(SecDbItemRef _Nullable)deleted
                            rateLimiter:(CKKSRateLimiter*)rateLimiter
                           syncCallback:(SecBoolNSErrorCallback)syncCallback;

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

- (CKKSResultOperation*)waitForFetchAndIncomingQueueProcessing;
- (void)waitForKeyHierarchyReadiness;
- (void)cancelAllOperations;

- (CKKSKey* _Nullable)keyForItem:(SecDbItemRef)item error:(NSError* __autoreleasing*)error;

- (bool)_onqueueWithAccountKeysCheckTLK:(CKKSKey*)proposedTLK error:(NSError* __autoreleasing*)error;

/* Asynchronous kickoffs */

- (CKKSOutgoingQueueOperation*)processOutgoingQueue:(CKOperationGroup* _Nullable)ckoperationGroup;
- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation* _Nullable)after
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

- (CKKSResultOperation*)fetchAndProcessCKChanges:(CKKSFetchBecause*)because;

- (CKKSResultOperation*)resetLocalData;
- (CKKSResultOperation*)resetCloudKitZone:(CKOperationGroup*)operationGroup;

// Call this to tell the key state machine that you think some new data has arrived that it is interested in
- (void)keyStateMachineRequestProcess;

// For our serial queue to work with how handleKeychainEventDbConnection is called from the main thread,
// every block on our queue must have a SecDBConnectionRef available to it before it begins on the queue.
// Use these helper methods to make sure those exist.
- (void)dispatchSync:(bool (^)(void))block;
- (void)dispatchSyncWithAccountKeys:(bool (^)(void))block;

/* Synchronous operations which must be called from inside a dispatchAsyncWithAccountKeys or dispatchSync block */

// Call this to request the key hierarchy state machine to fetch new updates
- (void)_onqueueKeyStateMachineRequestFetch;

// Call this to request the key hierarchy state machine to reprocess
- (void)_onqueueKeyStateMachineRequestProcess;

// Call this from a key hierarchy operation to move the state machine, and record the results of the last move.
- (void)_onqueueAdvanceKeyStateMachineToState:(CKKSZoneKeyState* _Nullable)state withError:(NSError* _Nullable)error;

// Since we might have people interested in the state transitions of objects, please do those transitions via these methods
- (bool)_onqueueChangeOutgoingQueueEntry:(CKKSOutgoingQueueEntry*)oqe
                                 toState:(NSString*)state
                                   error:(NSError* __autoreleasing*)error;
- (bool)_onqueueErrorOutgoingQueueEntry:(CKKSOutgoingQueueEntry*)oqe
                              itemError:(NSError*)itemError
                                  error:(NSError* __autoreleasing*)error;

// Call this if you've done a write and received an error. It'll pull out any new records returned as CKErrorServerRecordChanged and pretend we received them in a fetch
//
// Note that you need to tell this function the records you wanted to save, so it can determine which record failed from its CKRecordID.
// I don't know why CKRecordIDs don't have record types, either.
- (bool)_onqueueCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords;

- (bool)_onqueueCKRecordChanged:(CKRecord*)record resync:(bool)resync;
- (bool)_onqueueCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync;

// For this key, who doesn't yet have a CKKSTLKShare for it, shared to their current Octagon keys?
// Note that we really want a record sharing the TLK to ourselves, so this function might return
// a non-empty set even if all peers have the TLK: it wants us to make a record for ourself.
- (NSSet<id<CKKSPeer>>* _Nullable)_onqueueFindPeersMissingShare:(CKKSKey*)key error:(NSError* __autoreleasing*)error;

// For this key, share it to all trusted peers who don't have it yet
- (NSSet<CKKSTLKShare*>* _Nullable)_onqueueCreateMissingKeyShares:(CKKSKey*)key error:(NSError* __autoreleasing*)error;

- (bool)_onqueueUpdateLatestManifestWithError:(NSError**)error;

- (CKKSDeviceStateEntry* _Nullable)_onqueueCurrentDeviceStateEntry:(NSError* __autoreleasing*)error;

// Please don't use these unless you're an Operation in this package
@property NSHashTable<CKKSIncomingQueueOperation*>* incomingQueueOperations;
@property NSHashTable<CKKSOutgoingQueueOperation*>* outgoingQueueOperations;
@property CKKSScanLocalItemsOperation* initialScanOperation;

// Returns the current state of this view
- (NSDictionary<NSString*, NSString*>*)status;
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
