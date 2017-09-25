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


#ifndef CKKSKeychainView_h
#define CKKSKeychainView_h

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#if OCTAGON
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSAPSReceiver.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#endif

#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSIncomingQueueOperation.h"
#import "keychain/ckks/CKKSOutgoingQueueOperation.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSNewTLKOperation.h"
#import "keychain/ckks/CKKSProcessReceivedKeysOperation.h"
#import "keychain/ckks/CKKSReencryptOutgoingItemsOperation.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSScanLocalItemsOperation.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSZone.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ckks/CKKSNotifier.h"

#include "CKKS.h"

# if !OCTAGON
@interface CKKSKeychainView : NSObject {
    NSString* _containerName;
}
@end
#else // OCTAGON

@class CKKSKey;
@class CKKSAESSIVKey;
@class CKKSSynchronizeOperation;
@class CKKSRateLimiter;
@class CKKSManifest;
@class CKKSEgoManifest;
@class CKKSOutgoingQueueEntry;
@class CKKSZoneChangeFetcher;

@interface CKKSKeychainView : CKKSZone <CKKSZoneUpdateReceiver, CKKSChangeFetcherErrorOracle> {
    CKKSZoneKeyState* _keyHierarchyState;
}

@property CKKSLockStateTracker* lockStateTracker;

@property CKKSZoneKeyState* keyHierarchyState;
@property NSError* keyHierarchyError;
@property CKOperationGroup* keyHierarchyOperationGroup;
@property NSOperation* keyStateMachineOperation;

// If the key hierarchy isn't coming together, it might be because we're out of sync with cloudkit.
// Use this to track if we've completed a full refetch, so fix-up operations can be done.
@property bool keyStateMachineRefetched;
@property CKKSEgoManifest* egoManifest;
@property CKKSManifest* latestManifest;
@property CKKSResultOperation* keyStateReadyDependency;

@property (readonly) NSString *lastActiveTLKUUID;

// Full of condition variables, if you'd like to try to wait until the key hierarchy is in some state
@property NSMutableDictionary<CKKSZoneKeyState*, CKKSCondition*>* keyHierarchyConditions;

@property CKKSZoneChangeFetcher* zoneChangeFetcher;

@property (weak) CKKSNearFutureScheduler* savedTLKNotifier;

// Differs from the zonesetupoperation: zoneSetup is only for CK modifications, viewSetup handles local db changes too
@property CKKSGroupOperation* viewSetupOperation;

/* Used for debugging: just what happened last time we ran this? */
@property CKKSIncomingQueueOperation*             lastIncomingQueueOperation;
@property CKKSNewTLKOperation*                    lastNewTLKOperation;
@property CKKSOutgoingQueueOperation*             lastOutgoingQueueOperation;
@property CKKSProcessReceivedKeysOperation*       lastProcessReceivedKeysOperation;
@property CKKSFetchAllRecordZoneChangesOperation* lastRecordZoneChangesOperation;
@property CKKSReencryptOutgoingItemsOperation*    lastReencryptOutgoingItemsOperation;
@property CKKSScanLocalItemsOperation*            lastScanLocalItemsOperation;
@property CKKSSynchronizeOperation*               lastSynchronizeOperation;

/* Used for testing: pause operation types by adding operations here */
@property NSOperation* holdReencryptOutgoingItemsOperation;
@property NSOperation* holdOutgoingQueueOperation;

/* Trigger this to tell the whole machine that this view has changed */
@property CKKSNearFutureScheduler* notifyViewChangedScheduler;

- (instancetype)initWithContainer:     (CKContainer*) container
                             zoneName: (NSString*) zoneName
                       accountTracker:(CKKSCKAccountStateTracker*) accountTracker
                     lockStateTracker:(CKKSLockStateTracker*) lockStateTracker
                     savedTLKNotifier:(CKKSNearFutureScheduler*) savedTLKNotifier
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
                        notifierClass: (Class<CKKSNotifier>) notifierClass;

/* Synchronous operations */

- (void) handleKeychainEventDbConnection:(SecDbConnectionRef) dbconn
                                   added:(SecDbItemRef) added
                                 deleted:(SecDbItemRef) deleted
                             rateLimiter:(CKKSRateLimiter*) rateLimiter
                            syncCallback:(SecBoolNSErrorCallback) syncCallback;

-(void)setCurrentItemForAccessGroup:(SecDbItemRef)newItem
                               hash:(NSData*)newItemSHA1
                        accessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                          replacing:(SecDbItemRef)oldItem
                               hash:(NSData*)oldItemSHA1
                           complete:(void (^) (NSError* operror)) complete;

-(void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                    fetchCloudValue:(bool)fetchCloudValue
                           complete:(void (^) (NSString* uuid, NSError* operror)) complete;

- (bool) outgoingQueueEmpty: (NSError * __autoreleasing *) error;

- (CKKSResultOperation*)waitForFetchAndIncomingQueueProcessing;
- (void) waitForKeyHierarchyReadiness;
- (void) cancelAllOperations;

- (CKKSKey*) keyForItem: (SecDbItemRef) item error: (NSError * __autoreleasing *) error;

- (bool)checkTLK: (CKKSKey*) proposedTLK error: (NSError * __autoreleasing *) error;

/* Asynchronous kickoffs */

- (void) initializeZone;

- (CKKSOutgoingQueueOperation*)processOutgoingQueue:(CKOperationGroup*)ckoperationGroup;
- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation*)after ckoperationGroup:(CKOperationGroup*)ckoperationGroup;

- (CKKSIncomingQueueOperation*) processIncomingQueue:(bool)failOnClassA;
- (CKKSIncomingQueueOperation*) processIncomingQueue:(bool)failOnClassA after: (CKKSResultOperation*) after;

// Schedules a process queueoperation to happen after the next device unlock. This may be Immediately, if the device is unlocked.
- (void)processIncomingQueueAfterNextUnlock;

// Schedules an operation to update this device's state record in CloudKit
// If rateLimit is true, the operation will abort if it's updated the record in the past 3 days
- (CKKSUpdateDeviceStateOperation*)updateDeviceState:(bool)rateLimit ckoperationGroup:(CKOperationGroup*)ckoperationGroup;

- (CKKSSynchronizeOperation*) resyncWithCloud;

- (CKKSResultOperation*)fetchAndProcessCKChanges:(CKKSFetchBecause*)because;

- (CKKSResultOperation*)resetLocalData;
- (CKKSResultOperation*)resetCloudKitZone;

// Call this to pick and start the next key hierarchy operation for the zone
- (void)advanceKeyStateMachine;

// Call this to tell the key state machine that you think some new data has arrived that it is interested in
- (void)keyStateMachineRequestProcess;

// For our serial queue to work with how handleKeychainEventDbConnection is called from the main thread,
// every block on our queue must have a SecDBConnectionRef available to it before it begins on the queue.
// Use these helper methods to make sure those exist.
- (void) dispatchAsync: (bool (^)(void)) block;
- (void) dispatchSync: (bool (^)(void)) block;
- (void)dispatchSyncWithAccountQueue:(bool (^)(void))block;

/* Synchronous operations which must be called from inside a dispatchAsync or dispatchSync block */

// Call this to request the key hierarchy state machine to fetch new updates
- (void)_onqueueKeyStateMachineRequestFetch;

// Call this to request the key hierarchy state machine to refetch everything in Cloudkit
- (void)_onqueueKeyStateMachineRequestFullRefetch;

// Call this to request the key hierarchy state machine to reprocess
- (void)_onqueueKeyStateMachineRequestProcess;

// Call this from a key hierarchy operation to move the state machine, and record the results of the last move.
- (void)_onqueueAdvanceKeyStateMachineToState: (CKKSZoneKeyState*) state withError: (NSError*) error;

// Since we might have people interested in the state transitions of objects, please do those transitions via these methods
- (bool)_onqueueChangeOutgoingQueueEntry: (CKKSOutgoingQueueEntry*) oqe toState: (NSString*) state error: (NSError* __autoreleasing*) error;
- (bool)_onqueueErrorOutgoingQueueEntry: (CKKSOutgoingQueueEntry*) oqe itemError: (NSError*) itemError error: (NSError* __autoreleasing*) error;

// Call this if you've done a write and received an error. It'll pull out any new records returned as CKErrorServerRecordChanged and pretend we received them in a fetch
//
// Note that you need to tell this function the records you wanted to save, so it can determine which record failed from its CKRecordID.
// I don't know why CKRecordIDs don't have record types, either.
- (bool)_onqueueCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*,CKRecord*>*)savedRecords;

- (bool) _onqueueCKRecordChanged:(CKRecord*)record resync:(bool)resync;
- (bool) _onqueueCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync;

- (bool)_onQueueUpdateLatestManifestWithError:(NSError**)error;

- (CKKSDeviceStateEntry*)_onqueueCurrentDeviceStateEntry: (NSError* __autoreleasing*)error;

// Called by the CKKSZoneChangeFetcher
- (bool) isFatalCKFetchError: (NSError*) error;

// Please don't use these unless you're an Operation in this package
@property NSHashTable<CKKSIncomingQueueOperation*>* incomingQueueOperations;
@property NSHashTable<CKKSOutgoingQueueOperation*>* outgoingQueueOperations;
@property CKKSScanLocalItemsOperation* initialScanOperation;

// Returns the current state of this view
-(NSDictionary<NSString*, NSString*>*)status;
@end
#endif // OCTAGON


#define SecTranslateError(nserrorptr, cferror) \
    if(nserrorptr) { \
       *nserrorptr = (__bridge_transfer NSError*) cferror; \
    } else { \
       CFReleaseNull(cferror); \
    }

#endif /* CKKSKeychainView_h */
