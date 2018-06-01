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

#import "CKKSKeychainView.h"



#if OCTAGON
#import "CloudKitDependencies.h"
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#endif

#import "CKKS.h"
#import "CKKSAPSReceiver.h"
#import "CKKSIncomingQueueEntry.h"
#import "CKKSOutgoingQueueEntry.h"
#import "CKKSCurrentKeyPointer.h"
#import "CKKSKey.h"
#import "CKKSMirrorEntry.h"
#import "CKKSZoneStateEntry.h"
#import "CKKSItemEncrypter.h"
#import "CKKSIncomingQueueOperation.h"
#import "CKKSNewTLKOperation.h"
#import "CKKSProcessReceivedKeysOperation.h"
#import "CKKSZone.h"
#import "CKKSFetchAllRecordZoneChangesOperation.h"
#import "CKKSHealKeyHierarchyOperation.h"
#import "CKKSReencryptOutgoingItemsOperation.h"
#import "CKKSScanLocalItemsOperation.h"
#import "CKKSSynchronizeOperation.h"
#import "CKKSRateLimiter.h"
#import "CKKSManifest.h"
#import "CKKSManifestLeafRecord.h"
#import "CKKSZoneChangeFetcher.h"
#import "CKKSAnalytics.h"
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSUpdateCurrentItemPointerOperation.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSHealTLKSharesOperation.h"
#import "keychain/ckks/CKKSLocalSynchronizeOperation.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <securityd/SecDbItem.h>
#include <securityd/SecItemDb.h>
#include <securityd/SecItemSchema.h>
#include <securityd/SecItemServer.h>
#include <utilities/debugging.h>
#include <Security/SecItemPriv.h>
#include <Security/SecureObjectSync/SOSAccountTransaction.h>
#include <utilities/SecADWrapper.h>
#include <utilities/SecPLWrappers.h>
#include <os/transaction_private.h>

#if OCTAGON
@interface CKKSKeychainView()
@property bool keyStateFetchRequested;
@property bool keyStateFullRefetchRequested;
@property bool keyStateProcessRequested;

@property bool keyStateCloudKitDeleteRequested;
@property NSHashTable<CKKSResultOperation*>* cloudkitDeleteZoneOperations;

@property bool keyStateLocalResetRequested;
@property NSHashTable<CKKSResultOperation*>* localResetOperations;

@property (atomic) NSString *activeTLK;

@property (readonly) Class<CKKSNotifier> notifierClass;

@property CKKSNearFutureScheduler* initializeScheduler;

// Slows down all outgoing queue operations
@property CKKSNearFutureScheduler* outgoingQueueOperationScheduler;

@property CKKSResultOperation* processIncomingQueueAfterNextUnlockOperation;

@property NSMutableDictionary<NSString*, SecBoolNSErrorCallback>* pendingSyncCallbacks;

@property id<CKKSPeerProvider> currentPeerProvider;

// An extra queue for semaphore-waiting-based NSOperations
@property NSOperationQueue* waitingQueue;

// Make these readwrite
@property (nonatomic, readwrite) CKKSSelves* currentSelfPeers;
@property (nonatomic, readwrite) NSError* currentSelfPeersError;
@property (nonatomic, readwrite) NSSet<id<CKKSPeer>>* currentTrustedPeers;
@property (nonatomic, readwrite) NSError* currentTrustedPeersError;
@end
#endif

@implementation CKKSKeychainView
#if OCTAGON

- (instancetype)initWithContainer:     (CKContainer*) container
                             zoneName: (NSString*) zoneName
                       accountTracker:(CKKSCKAccountStateTracker*) accountTracker
                     lockStateTracker:(CKKSLockStateTracker*) lockStateTracker
                  reachabilityTracker:(CKKSReachabilityTracker *)reachabilityTracker
                     savedTLKNotifier:(CKKSNearFutureScheduler*) savedTLKNotifier
                         peerProvider:(id<CKKSPeerProvider>)peerProvider
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
           fetchRecordsOperationClass: (Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                  queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
                        notifierClass: (Class<CKKSNotifier>) notifierClass
{

    if(self = [super initWithContainer:container
                              zoneName:zoneName
                        accountTracker:accountTracker
                   reachabilityTracker:reachabilityTracker
  fetchRecordZoneChangesOperationClass:fetchRecordZoneChangesOperationClass
            fetchRecordsOperationClass:fetchRecordsOperationClass
                   queryOperationClass:queryOperationClass
     modifySubscriptionsOperationClass:modifySubscriptionsOperationClass
       modifyRecordZonesOperationClass:modifyRecordZonesOperationClass
                    apsConnectionClass:apsConnectionClass]) {
        __weak __typeof(self) weakSelf = self;

        _loggedIn = [[CKKSCondition alloc] init];
        _loggedOut = [[CKKSCondition alloc] init];
        _accountStateKnown = [[CKKSCondition alloc] init];

        _incomingQueueOperations = [NSHashTable weakObjectsHashTable];
        _outgoingQueueOperations = [NSHashTable weakObjectsHashTable];
        _cloudkitDeleteZoneOperations = [NSHashTable weakObjectsHashTable];
        _localResetOperations = [NSHashTable weakObjectsHashTable];
        _zoneChangeFetcher = [[CKKSZoneChangeFetcher alloc] initWithCKKSKeychainView: self];

        _notifierClass = notifierClass;
        _notifyViewChangedScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-notify-scheduler", self.zoneName]
                                                            initialDelay:250*NSEC_PER_MSEC
                                                         continuingDelay:1*NSEC_PER_SEC
                                                        keepProcessAlive:true
                                                          dependencyDescriptionCode:CKKSResultDescriptionPendingViewChangedScheduling
                                                                   block:^{
                                                                       __strong __typeof(self) strongSelf = weakSelf;
                                                                       [strongSelf.notifierClass post:[NSString stringWithFormat:@"com.apple.security.view-change.%@", strongSelf.zoneName]];

                                                                       // Ugly, but: the Manatee and Engram views need to send a fake 'PCS' view change.
                                                                       // TODO: make this data-driven somehow
                                                                       if([strongSelf.zoneName isEqualToString:@"Manatee"] || [strongSelf.zoneName isEqualToString:@"Engram"]) {
                                                                           [strongSelf.notifierClass post:@"com.apple.security.view-change.PCS"];
                                                                       }
                                                                   }];

        _pendingSyncCallbacks = [[NSMutableDictionary alloc] init];

        _lockStateTracker = lockStateTracker;
        _savedTLKNotifier = savedTLKNotifier;
        _currentPeerProvider = peerProvider;
        [_currentPeerProvider registerForPeerChangeUpdates:self];

        _keyHierarchyConditions = [[NSMutableDictionary alloc] init];
        [CKKSZoneKeyStateMap() enumerateKeysAndObjectsUsingBlock:^(CKKSZoneKeyState * _Nonnull key, NSNumber * _Nonnull obj, BOOL * _Nonnull stop) {
            [self.keyHierarchyConditions setObject: [[CKKSCondition alloc] init] forKey:key];
        }];

        // Use the keyHierarchyState setter to modify the zone key state map
        self.keyHierarchyState = SecCKKSZoneKeyStateLoggedOut;

        _keyHierarchyError = nil;
        _keyHierarchyOperationGroup = nil;
        _keyStateMachineOperation = nil;
        _keyStateFetchRequested = false;
        _keyStateProcessRequested = false;

        _waitingQueue = [[NSOperationQueue alloc] init];
        _waitingQueue.maxConcurrentOperationCount = 5;

        _keyStateReadyDependency = [self createKeyStateReadyDependency: @"Key state has become ready for the first time." ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"initial-key-state-ready-scan"]];

        _keyStateNonTransientDependency = [self createKeyStateNontransientDependency];

        dispatch_time_t initializeDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 600 : NSEC_PER_SEC * 30;
        _initializeScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-zone-initializer", self.zoneName]
                                                                initialDelay:0
                                                             continuingDelay:initializeDelay
                                                            keepProcessAlive:false
                                                   dependencyDescriptionCode:CKKSResultDescriptionPendingZoneInitializeScheduling
                                                                       block:^{}];

        dispatch_time_t initialOutgoingQueueDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 1;
        dispatch_time_t continuingOutgoingQueueDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 30;
        _outgoingQueueOperationScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-outgoing-queue-scheduler", self.zoneName]
                                                                            initialDelay:initialOutgoingQueueDelay
                                                                         continuingDelay:continuingOutgoingQueueDelay
                                                                        keepProcessAlive:false
                                                               dependencyDescriptionCode:CKKSResultDescriptionPendingOutgoingQueueScheduling
                                                                                   block:^{}];


        dispatch_time_t initialKeyHierachyPokeDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 100 : NSEC_PER_MSEC * 500;
        dispatch_time_t continuingKeyHierachyPokeDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 5;
        _pokeKeyStateMachineScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-reprocess-scheduler", self.zoneName]
                                                                         initialDelay:initialKeyHierachyPokeDelay
                                                                      continuingDelay:continuingKeyHierachyPokeDelay
                                                                     keepProcessAlive:true
                                                            dependencyDescriptionCode:CKKSResultDescriptionPendingKeyHierachyPokeScheduling
                                                                                     block:^{
                                                                                         __strong __typeof(self) strongSelf = weakSelf;
                                                                                         [strongSelf dispatchSyncWithAccountKeys: ^bool{
                                                                                             __strong __typeof(weakSelf) strongBlockSelf = weakSelf;

                                                                                             [strongBlockSelf _onqueueAdvanceKeyStateMachineToState:nil withError:nil];
                                                                                             return true;
                                                                                         }];
                                                                                     }];
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<%@: %@ (%@)>", NSStringFromClass([self class]), self.zoneName, self.keyHierarchyState];
}

- (NSString*)debugDescription {
    return [NSString stringWithFormat:@"<%@: %@ (%@) %p>", NSStringFromClass([self class]), self.zoneName, self.keyHierarchyState, self];
}

- (CKKSZoneKeyState*)keyHierarchyState {
    return _keyHierarchyState;
}

- (void)setKeyHierarchyState:(CKKSZoneKeyState *)keyHierarchyState {
    if((keyHierarchyState == nil && _keyHierarchyState == nil) || ([keyHierarchyState isEqualToString:_keyHierarchyState])) {
        // No change, do nothing.
    } else {
        // Fixup the condition variables as part of setting this state
        if(_keyHierarchyState) {
            self.keyHierarchyConditions[_keyHierarchyState] = [[CKKSCondition alloc] init];
        }

        _keyHierarchyState = keyHierarchyState;

        if(keyHierarchyState) {
            [self.keyHierarchyConditions[keyHierarchyState] fulfill];
        }
    }
}

- (NSString *)lastActiveTLKUUID
{
    return self.activeTLK;
}

- (void)_onqueueResetSetup:(CKKSZoneKeyState*)newState resetMessage:(NSString*)resetMessage ckoperationGroup:(CKOperationGroup*)group {
    [super resetSetup];

    self.keyHierarchyState = newState;
    self.keyHierarchyError = nil;

    [self.keyStateMachineOperation cancel];
    self.keyStateMachineOperation = nil;

    self.keyStateFetchRequested = false;
    self.keyStateProcessRequested = false;

    self.keyHierarchyOperationGroup = group;

    NSOperation* oldKSRD = self.keyStateReadyDependency;
    self.keyStateReadyDependency = [self createKeyStateReadyDependency:resetMessage ckoperationGroup:self.keyHierarchyOperationGroup];
    if(oldKSRD) {
        [oldKSRD addDependency:self.keyStateReadyDependency];
        [self.waitingQueue addOperation:oldKSRD];
    }

    NSOperation* oldKSNTD = self.keyStateNonTransientDependency;
    self.keyStateNonTransientDependency = [self createKeyStateNontransientDependency];
    if(oldKSNTD) {
        [oldKSNTD addDependency:self.keyStateNonTransientDependency];
        [self.waitingQueue addOperation:oldKSNTD];
    }
}

- (CKKSResultOperation*)createPendingInitializationOperation {

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* initializationOp = [CKKSGroupOperation named:@"view-initialization" withBlockTakingSelf:^(CKKSGroupOperation * _Nonnull strongOp) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;

        __block CKKSResultOperation* zoneCreationOperation = nil;
        [strongSelf dispatchSync:^bool {
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: self.zoneName];
            zoneCreationOperation = [self handleCKLogin:ckse.ckzonecreated zoneSubscribed:ckse.ckzonesubscribed];
            return true;
        }];

        CKKSResultOperation* viewInitializationOperation = [CKKSResultOperation named:@"view-initialization" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull strongInternalOp) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror("ckks", strongSelf, "received callback for released object");
                return;
            }

            [strongSelf dispatchSyncWithAccountKeys: ^bool {
                ckksnotice("ckks", strongSelf, "Zone setup progress: %@ %d %@ %d %@",
                           [CKKSCKAccountStateTracker stringFromAccountStatus:strongSelf.accountStatus],
                           strongSelf.zoneCreated, strongSelf.zoneCreatedError, strongSelf.zoneSubscribed, strongSelf.zoneSubscribedError);

                NSError* error = nil;
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: strongSelf.zoneName];
                ckse.ckzonecreated = strongSelf.zoneCreated;
                ckse.ckzonesubscribed = strongSelf.zoneSubscribed;

                // Although, if the zone subscribed error says there's no zone, mark down that there's no zone
                if(strongSelf.zoneSubscribedError &&
                   [strongSelf.zoneSubscribedError.domain isEqualToString:CKErrorDomain] && strongSelf.zoneSubscribedError.code == CKErrorPartialFailure) {
                    NSError* subscriptionError = strongSelf.zoneSubscribedError.userInfo[CKPartialErrorsByItemIDKey][strongSelf.zoneID];
                    if(subscriptionError && [subscriptionError.domain isEqualToString:CKErrorDomain] && subscriptionError.code == CKErrorZoneNotFound) {

                        ckkserror("ckks", strongSelf, "zone subscription error appears to say the zone doesn't exist, fixing status: %@", strongSelf.zoneSubscribedError);
                        ckse.ckzonecreated = false;
                    }
                }

                [ckse saveToDatabase: &error];
                if(error) {
                    ckkserror("ckks", strongSelf, "couldn't save zone creation status for %@: %@", strongSelf.zoneName, error);
                }

                if(!strongSelf.zoneCreated || !strongSelf.zoneSubscribed) {
                    // Go into 'zonecreationfailed'
                    strongInternalOp.error = strongSelf.zoneCreatedError ? strongSelf.zoneCreatedError : strongSelf.zoneSubscribedError;
                    [strongSelf _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateZoneCreationFailed withError:strongInternalOp.error];

                    return true;
                } else {
                    [strongSelf _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateInitialized withError:nil];
                }

                return true;
            }];
        }];

        [viewInitializationOperation addDependency:zoneCreationOperation];
        [strongOp runBeforeGroupFinished:viewInitializationOperation];
    }];

    return initializationOp;
}

- (void)_onqueuePerformKeyStateInitialized:(CKKSZoneStateEntry*)ckse {

    // Check if we believe we've synced this zone before.
    if(ckse.changeToken == nil) {
        self.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"initial-setup"];

        ckksnotice("ckks", self, "No existing change token; going to try to match local items with CloudKit ones.");

        // Onboard this keychain: there's likely items in it that we haven't synced yet.
        // But, there might be items in The Cloud that correspond to these items, with UUIDs that we don't know yet.
        // First, fetch all remote items.
        CKKSResultOperation* fetch = [self.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseInitialStart];
        fetch.name = @"initial-fetch";

        // Next, try to process them (replacing local entries)
        CKKSIncomingQueueOperation* initialProcess = [self processIncomingQueue:true after:fetch];
        initialProcess.name = @"initial-process-incoming-queue";

        // If all that succeeds, iterate through all keychain items and find the ones which need to be uploaded
        self.initialScanOperation = [self scanLocalItems:@"initial-scan-operation"
                                        ckoperationGroup:self.keyHierarchyOperationGroup
                                                   after:initialProcess];

    } else {
        // Likely a restart of securityd!

        // First off, are there any in-flight queue entries? If so, put them back into New.
        // If they're truly in-flight, we'll "conflict" with ourselves, but that should be fine.
        NSError* error = nil;
        [self _onqueueResetAllInflightOQE:&error];
        if(error) {
            ckkserror("ckks", self, "Couldn't reset in-flight OQEs, bad behavior ahead: %@", error);
        }

        // Are there any fixups to run first?
        self.lastFixupOperation = [CKKSFixups fixup:ckse.lastFixup for:self];
        if(self.lastFixupOperation) {
            ckksnotice("ckksfixup", self, "We have a fixup to perform: %@", self.lastFixupOperation);
            [self scheduleOperation:self.lastFixupOperation];
        }

        self.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"restart-setup"];

        if ([CKKSManifest shouldSyncManifests]) {
            self.egoManifest = [CKKSEgoManifest tryCurrentEgoManifestForZone:self.zoneName];
        }

        // If it's been more than 24 hours since the last fetch, fetch and process everything.
        // Otherwise, just kick off the local queue processing.

        NSDate* now = [NSDate date];
        NSDateComponents* offset = [[NSDateComponents alloc] init];
        [offset setHour:-24];
        NSDate* deadline = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:now options:0];

        NSOperation* initialProcess = nil;
        if(ckse.lastFetchTime == nil || [ckse.lastFetchTime compare: deadline] == NSOrderedAscending) {
            initialProcess = [self fetchAndProcessCKChanges:CKKSFetchBecauseSecuritydRestart after:self.lastFixupOperation];

            // Also, kick off a scan local items: it'll find any out-of-sync issues in the local keychain
            self.initialScanOperation = [self scanLocalItems:@"24-hr-scan-operation"
                                                ckoperationGroup:self.keyHierarchyOperationGroup
                                                       after:initialProcess];
        } else {
            initialProcess = [self processIncomingQueue:false after:self.lastFixupOperation];
        }

        if([CKKSManifest shouldSyncManifests]) {
            if (!self.egoManifest && !self.initialScanOperation) {
                ckksnotice("ckksmanifest", self, "No ego manifest on restart; rescanning");
                self.initialScanOperation = [self scanLocalItems:@"initial-scan-operation"
                                                ckoperationGroup:self.keyHierarchyOperationGroup
                                                           after:initialProcess];
            }
        }

        // Process outgoing queue after re-start
        [self processOutgoingQueueAfter:self.lastFixupOperation ckoperationGroup:self.keyHierarchyOperationGroup];
    }
}

- (bool)_onqueueResetLocalData: (NSError * __autoreleasing *) error {
    dispatch_assert_queue(self.queue);

    NSError* localerror = nil;
    bool setError = false; // Ugly, but this is the only way to return the first error given

    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: self.zoneName];
    ckse.ckzonecreated = false;
    ckse.ckzonesubscribed = false; // I'm actually not sure about this: can you be subscribed to a non-existent zone?
    ckse.changeToken = NULL;
    [ckse saveToDatabase: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't reset zone status for %@: %@", self.zoneName, localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSMirrorEntry deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSMirrorEntry: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSOutgoingQueueEntry deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSOutgoingQueueEntry: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSIncomingQueueEntry deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSIncomingQueueEntry: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSKey deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSKey: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSTLKShare deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSTLKShare: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSCurrentKeyPointer deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSCurrentKeyPointer: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSCurrentItemPointer deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSCurrentItemPointer: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    [CKKSDeviceStateEntry deleteAll:self.zoneID error:&localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSDeviceStateEntry: %@", localerror);
        if(error && !setError) {
            *error = localerror; setError = true;
        }
    }

    return (localerror == nil && !setError);
}

- (CKKSResultOperation*)createPendingResetLocalDataOperation {
    @synchronized(self.localResetOperations) {
        CKKSResultOperation* pendingResetLocalOperation = (CKKSResultOperation*) [self findFirstPendingOperation:self.localResetOperations];
        if(!pendingResetLocalOperation) {
            __weak __typeof(self) weakSelf = self;
            pendingResetLocalOperation = [CKKSResultOperation named:@"reset-local" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull strongOp) {
                __strong __typeof(self) strongSelf = weakSelf;
                __block NSError* error = nil;

                [strongSelf dispatchSync: ^bool{
                    [strongSelf _onqueueResetLocalData: &error];
                    return true;
                }];

                strongOp.error = error;
            }];
            [pendingResetLocalOperation linearDependencies:self.localResetOperations];
        }
        return pendingResetLocalOperation;
    }
}

- (CKKSResultOperation*)resetLocalData {
    // Not overly thread-safe, but a single read is okay
    CKKSAccountStatus accountStatus = self.accountStatus;
    ckksnotice("ckksreset", self, "Requesting local data reset");

    // If we're currently signed in, the reset operation will be handled by the CKKS key state machine, and a reset should end up in 'ready'
    if(accountStatus == CKKSAccountStatusAvailable) {
        __block CKKSResultOperation* resetOperation = nil;
        [self dispatchSyncWithAccountKeys:^bool {
            self.keyStateLocalResetRequested = true;
            resetOperation = [self createPendingResetLocalDataOperation];
            [self _onqueueAdvanceKeyStateMachineToState:nil withError:nil];
            return true;
        }];

        __weak __typeof(self) weakSelf = self;
        CKKSGroupOperation* viewReset = [CKKSGroupOperation named:@"local-data-reset" withBlockTakingSelf:^(CKKSGroupOperation *strongOp) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            // Now that the local reset finished, wait for the key hierarchy state machine to churn
            ckksnotice("ckksreset", strongSelf, "waiting for key hierarchy to become ready (after local reset)");
            CKKSResultOperation* waitOp = [CKKSResultOperation named:@"waiting-for-local-reset" withBlock:^{}];
            [waitOp timeout: 60*NSEC_PER_SEC];
            [waitOp addNullableDependency:strongSelf.keyStateReadyDependency];

            [strongOp runBeforeGroupFinished:waitOp];
        }];
        [viewReset addSuccessDependency:resetOperation];

        [self scheduleOperationWithoutDependencies:viewReset];
        return viewReset;
    } else {
        // Since we're logged out, we must run the reset ourselves
        __weak __typeof(self) weakSelf = self;
        CKKSResultOperation* pendingResetLocalOperation = [CKKSResultOperation named:@"reset-local"
                                                                 withBlockTakingSelf:^(CKKSResultOperation * _Nonnull strongOp) {
            __strong __typeof(self) strongSelf = weakSelf;
            __block NSError* error = nil;

            [strongSelf dispatchSync: ^bool{
                [strongSelf _onqueueResetLocalData: &error];
                return true;
            }];

            strongOp.error = error;
        }];
        [self scheduleOperationWithoutDependencies:pendingResetLocalOperation];
        return pendingResetLocalOperation;
    }
}

- (CKKSResultOperation*)createPendingDeleteZoneOperation:(CKOperationGroup*)operationGroup {
    @synchronized(self.cloudkitDeleteZoneOperations) {
        CKKSResultOperation* pendingDeleteOperation = (CKKSResultOperation*) [self findFirstPendingOperation:self.cloudkitDeleteZoneOperations];
        if(!pendingDeleteOperation) {
            pendingDeleteOperation = [self deleteCloudKitZoneOperation:operationGroup];
            [pendingDeleteOperation linearDependencies:self.cloudkitDeleteZoneOperations];
        }
        return pendingDeleteOperation;
    }
}

- (CKKSResultOperation*)resetCloudKitZone:(CKOperationGroup*)operationGroup {
    // Not overly thread-safe, but a single read is okay
    if(self.accountStatus == CKKSAccountStatusAvailable) {
        // Actually running the delete operation will be handled by the CKKS key state machine
        ckksnotice("ckksreset", self, "Requesting reset of CK zone (logged in)");

        __block CKKSResultOperation* deleteOperation = nil;
        [self dispatchSyncWithAccountKeys:^bool {
            self.keyStateCloudKitDeleteRequested = true;
            deleteOperation = [self createPendingDeleteZoneOperation:operationGroup];
            [self _onqueueAdvanceKeyStateMachineToState:nil withError:nil];
            return true;
        }];

        __weak __typeof(self) weakSelf = self;
        CKKSGroupOperation* viewReset = [CKKSGroupOperation named:[NSString stringWithFormat:@"cloudkit-view-reset-%@", self.zoneName]
                                              withBlockTakingSelf:^(CKKSGroupOperation *strongOp) {
            __strong __typeof(self) strongSelf = weakSelf;
            // Now that the delete finished, wait for the key hierarchy state machine
            ckksnotice("ckksreset", strongSelf, "waiting for key hierarchy to become ready (after cloudkit reset)");
            CKKSResultOperation* waitOp = [CKKSResultOperation named:@"waiting-for-reset" withBlock:^{}];
            [waitOp timeout: 60*NSEC_PER_SEC];
            [waitOp addNullableDependency:strongSelf.keyStateReadyDependency];

            [strongOp runBeforeGroupFinished:waitOp];
        }];

        [viewReset addDependency:deleteOperation];
        [self.waitingQueue addOperation:viewReset];

        return viewReset;
    } else {
        // Since we're logged out, we just need to run this ourselves
        ckksnotice("ckksreset", self, "Requesting reset of CK zone (logged out)");
        CKKSResultOperation* deleteOperation = [self createPendingDeleteZoneOperation:operationGroup];
        [self scheduleOperationWithoutDependencies:deleteOperation];
        return deleteOperation;
    }
}

- (void)_onqueueKeyStateMachineRequestFetch {
    dispatch_assert_queue(self.queue);

    // We're going to set this flag, then nudge the key state machine.
    // If it was idle, then it should launch a fetch. If there was an active process, this flag will stay high
    // and the fetch will be launched later.

    self.keyStateFetchRequested = true;
    [self _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
}

- (void)keyStateMachineRequestProcess {
    // Since bools are atomic, we don't need to get on-queue here
    // Just set the flag high and hope
    self.keyStateProcessRequested = true;
    [self.pokeKeyStateMachineScheduler trigger];
}

- (void)_onqueueKeyStateMachineRequestProcess {
    dispatch_assert_queue(self.queue);

    // Set the request flag, then nudge the key state machine.
    // If it was idle, then it should launch a process. If there was an active process, this flag will stay high
    // and the process will be launched later.

    self.keyStateProcessRequested = true;
    [self _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
}

- (CKKSResultOperation*)createKeyStateReadyDependency:(NSString*)message ckoperationGroup:(CKOperationGroup*)group {
    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* keyStateReadyDependency = [CKKSResultOperation operationWithBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            return;
        }
        ckksnotice("ckkskey", strongSelf, "%@", message);

        [strongSelf dispatchSync:^bool {
            if(strongSelf.droppedItems) {
                // While we weren't in 'ready', keychain modifications might have come in and were dropped on the floor. Find them!
                ckksnotice("ckkskey", strongSelf, "Launching scan operation for missed items");
                [self scanLocalItems:@"ready-again-scan" ckoperationGroup:group after:nil];
            }
            return true;
        }];
    }];
    keyStateReadyDependency.name = [NSString stringWithFormat: @"%@-key-state-ready", self.zoneName];
    keyStateReadyDependency.descriptionErrorCode = CKKSResultDescriptionPendingKeyReady;
    return keyStateReadyDependency;
}

- (CKKSResultOperation*)createKeyStateNontransientDependency {
    __weak __typeof(self) weakSelf = self;
    return [CKKSResultOperation named:[NSString stringWithFormat: @"%@-key-state-nontransient", self.zoneName] withBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;
        ckksnotice("ckkskey", strongSelf, "Key state is now non-transient");
    }];
}

// The operations suggested by this state machine should call _onqueueAdvanceKeyStateMachineToState once they are complete.
// At no other time should keyHierarchyState be modified.

// Note that this function cannot rely on doing any database work; it might get rolled back, especially in an error state
- (void)_onqueueAdvanceKeyStateMachineToState: (CKKSZoneKeyState*) state withError: (NSError*) error {
    dispatch_assert_queue(self.queue);
    __weak __typeof(self) weakSelf = self;

    // Resetting back to 'loggedout' takes all precedence.
    if([state isEqual:SecCKKSZoneKeyStateLoggedOut]) {
        ckksnotice("ckkskey", self, "Resetting the key hierarchy state machine back to '%@'", state);

        [self _onqueueResetSetup:SecCKKSZoneKeyStateLoggedOut
                    resetMessage:@"Key state has become ready for the first time (after reset)."
                ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"key-state-after-logout"]];

        [self _onqueueHandleKeyStateNonTransientDependency];
        return;
    }

    // Resetting back to 'initialized' also takes precedence
    if([state isEqual:SecCKKSZoneKeyStateInitializing]) {
        ckksnotice("ckkskey", self, "Resetting the key hierarchy state machine back to '%@'", state);

        [self _onqueueResetSetup:SecCKKSZoneKeyStateInitializing
                    resetMessage:@"Key state has become ready for the first time (after re-initializing)."
                ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"key-state-reset-to-initializing"]];

        // Begin initialization, but rate-limit it
        self.keyStateMachineOperation = [self createPendingInitializationOperation];
        [self.keyStateMachineOperation addNullableDependency:self.initializeScheduler.operationDependency];
        [self.initializeScheduler trigger];
        [self scheduleOperation:self.keyStateMachineOperation];

        [self _onqueueHandleKeyStateNonTransientDependency];
        return;
    }

    // Cancels and error states take precedence
    if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateError] ||
       [self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateCancelled] ||
       self.keyHierarchyError != nil) {
        // Error state: nowhere to go. Early-exit.
        ckkserror("ckkskey", self, "Asked to advance state machine from non-exit state %@ (to %@): %@", self.keyHierarchyState, state, self.keyHierarchyError);
        return;
    }

    if([state isEqual: SecCKKSZoneKeyStateError]) {
        // But wait! Is this a "we're locked" error?
        if(error && [self.lockStateTracker isLockedError:error]) {
            ckkserror("ckkskey", self, "advised of 'keychain locked' error, ignoring: coming from state (%@): %@", self.keyHierarchyState, error);
            // After the next unlock, fake that we received the last zone transition
            CKKSZoneKeyState* lastState = self.keyHierarchyState;
            self.keyStateMachineOperation = [NSBlockOperation named:@"key-state-after-unlock" withBlock:^{
                __strong __typeof(self) strongSelf = weakSelf;
                if(!strongSelf) {
                    return;
                }
                [strongSelf dispatchSync:^bool{
                    [strongSelf _onqueueAdvanceKeyStateMachineToState:lastState withError:nil];
                    return true;
                }];
            }];
            state = nil;

            self.keyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;

            [self.keyStateMachineOperation addNullableDependency:self.lockStateTracker.unlockDependency];
            [self scheduleOperation:self.keyStateMachineOperation];

            [self _onqueueHandleKeyStateNonTransientDependency];
            return;

        } else {
            // Error state: record the error and exit early
            ckkserror("ckkskey", self, "advised of error: coming from state (%@): %@", self.keyHierarchyState, error);

            [[CKKSAnalytics logger] logUnrecoverableError:error
                                                 forEvent:CKKSEventStateError
                                                   inView:self
                                           withAttributes:@{ @"previousKeyHierarchyState" : self.keyHierarchyState }];


            self.keyHierarchyState = SecCKKSZoneKeyStateError;
            self.keyHierarchyError = error;

            [self _onqueueHandleKeyStateNonTransientDependency];
            return;
        }
    }

    if([state isEqual: SecCKKSZoneKeyStateCancelled]) {
        ckkserror("ckkskey", self, "advised of cancel: coming from state (%@): %@", self.keyHierarchyState, error);
        self.keyHierarchyState = SecCKKSZoneKeyStateCancelled;
        self.keyHierarchyError = error;

        // Cancel the key ready dependency. Strictly Speaking, this will cause errors down the line, but we're in a cancel state: those operations should be canceled anyway.
        self.keyHierarchyOperationGroup = nil;
        [self.keyStateReadyDependency cancel];
        self.keyStateReadyDependency = nil;

        [self.keyStateNonTransientDependency cancel];
        self.keyStateNonTransientDependency = nil;
        return;
    }

    // Now that the current or new state isn't an error or a cancel, proceed.
    if(self.keyStateMachineOperation && ![self.keyStateMachineOperation isFinished]) {
        if(state == nil) {
            // we started this operation to move the state machine. Since you aren't asking for a state transition, and there's an active operation, no need to do anything
            ckksnotice("ckkskey", self, "Not advancing state machine: waiting for %@", self.keyStateMachineOperation);
            return;
        }
    }

    if(state) {
        ckksnotice("ckkskey", self, "Preparing to advance key hierarchy state machine from %@ to %@", self.keyHierarchyState, state);
        self.keyStateMachineOperation = nil;
    } else {
        ckksnotice("ckkskey", self, "Key hierarchy state machine is being poked; currently %@", self.keyHierarchyState);
        state = self.keyHierarchyState;
    }

#if DEBUG
    // During testing, keep the developer honest: this function should always have the self identities
    if(self.currentSelfPeersError) {
        NSAssert(self.currentSelfPeersError.code != CKKSNoPeersAvailable, @"Must have viable (or errored) self peers to advance key state");
    }
#endif

    // Do any of these state transitions below want to change which state we're in?
    CKKSZoneKeyState* nextState = nil;
    NSError* nextError = nil;

    // Many of our decisions below will be based on what keys exist. Help them out.
    CKKSCurrentKeySet* keyset = [[CKKSCurrentKeySet alloc] initForZone:self.zoneID];
    NSError* localerror = nil;
    NSArray<CKKSKey*>* localKeys = [CKKSKey localKeys:self.zoneID error:&localerror];
    NSArray<CKKSKey*>* remoteKeys = [CKKSKey remoteKeys:self.zoneID error: &localerror];

    // We also are checking for OutgoingQueueEntries in the reencrypt state; this is a sign that our key hierarchy is out of date.
    NSInteger outdatedOQEs = [CKKSOutgoingQueueEntry countByState:SecCKKSStateReencrypt zone:self.zoneID error:&localerror];

    SecADSetValueForScalarKey((__bridge CFStringRef) SecCKKSAggdViewKeyCount, [localKeys count]);

    if(localerror) {
        ckkserror("ckkskey", self, "couldn't fetch keys and OQEs from local database, entering error state: %@", localerror);
        self.keyHierarchyState = SecCKKSZoneKeyStateError;
        self.keyHierarchyError = localerror;
        [self _onqueueHandleKeyStateNonTransientDependency];
        return;
    }

#if !defined(NDEBUG)
    NSArray<CKKSKey*>* allKeys = [CKKSKey allKeys:self.zoneID error:&localerror];
    ckksdebug("ckkskey", self, "All keys: %@", allKeys);
#endif

    NSError* hierarchyError = nil;

    if(self.keyStateCloudKitDeleteRequested || [state isEqualToString:SecCKKSZoneKeyStateResettingZone]) {
        // CloudKit reset requests take precedence over all other state transitions
        ckksnotice("ckkskey", self, "Deleting the CloudKit Zone");
        CKKSGroupOperation* op = [[CKKSGroupOperation alloc] init];

        CKKSResultOperation* deleteOp = [self createPendingDeleteZoneOperation:self.keyHierarchyOperationGroup];
        [op runBeforeGroupFinished: deleteOp];

        NSOperation* nextStateOp = [self operationToEnterState:SecCKKSZoneKeyStateResettingLocalData keyStateError:nil named:@"state-resetting-local"];
        [nextStateOp addDependency:deleteOp];
        [op runBeforeGroupFinished:nextStateOp];

        self.keyStateMachineOperation = op;
        self.keyStateCloudKitDeleteRequested = false;

        // Also, pending operations should be cancelled
        [self cancelPendingOperations];

    } else if(self.keyStateLocalResetRequested || [state isEqualToString:SecCKKSZoneKeyStateResettingLocalData]) {
        // Local reset requests take precedence over all other state transitions
        ckksnotice("ckkskey", self, "Resetting local data");
        CKKSGroupOperation* op = [[CKKSGroupOperation alloc] init];

        CKKSResultOperation* resetOp = [self createPendingResetLocalDataOperation];
        [op runBeforeGroupFinished: resetOp];

        NSOperation* nextStateOp = [self operationToEnterState:SecCKKSZoneKeyStateInitializing keyStateError:nil named:@"state-resetting-initialize"];
        [nextStateOp addDependency:resetOp];
        [op runBeforeGroupFinished:nextStateOp];

        self.keyStateMachineOperation = op;
        self.keyStateLocalResetRequested = false;


    } else if([state isEqualToString:SecCKKSZoneKeyStateZoneCreationFailed]) {
        //Prepare to go back into initializing, as soon as the initializeScheduler is happy
        self.keyStateMachineOperation = [self operationToEnterState:SecCKKSZoneKeyStateInitializing keyStateError:nil named:@"recover-from-cloudkit-failure"];
        [self.keyStateMachineOperation addNullableDependency:self.initializeScheduler.operationDependency];
        [self.initializeScheduler trigger];

    } else if([state isEqualToString: SecCKKSZoneKeyStateReady]) {
        if(self.keyStateProcessRequested || [remoteKeys count] > 0) {
            // We've either received some remote keys from the last fetch, or someone has requested a reprocess.
            ckksnotice("ckkskey", self, "Kicking off a key reprocess based on request:%d and remote key count %lu", self.keyStateProcessRequested, (unsigned long)[remoteKeys count]);
            [self _onqueueKeyHierarchyProcess];
            // Stay in state 'ready': this reprocess might not change anything. If it does, cleanup code elsewhere will
            // reencode items that arrive during this ready

        } else if(self.keyStateFullRefetchRequested) {
            // In ready, but someone has requested a full fetch. Kick it off.
            ckksnotice("ckkskey", self, "Kicking off a full key refetch based on request:%d", self.keyStateFullRefetchRequested);
            nextState = SecCKKSZoneKeyStateNeedFullRefetch;

        } else if(self.keyStateFetchRequested) {
            // In ready, but someone has requested a fetch. Kick it off.
            ckksnotice("ckkskey", self, "Kicking off a key refetch based on request:%d", self.keyStateFetchRequested);
            nextState = SecCKKSZoneKeyStateFetch; // Don't go to 'ready', go to 'initialized', since we want to fetch again
        }
        // TODO: kick off a key roll if one has been requested

        if(!self.keyStateMachineOperation) {
            // We think we're ready. Double check.
            CKKSZoneKeyState* checkedstate = [self _onqueueEnsureKeyHierarchyHealth:keyset error:&hierarchyError];
            if(![checkedstate isEqualToString:SecCKKSZoneKeyStateReady] || hierarchyError) {
                // Things is bad. Kick off a heal to fix things up.
                ckksnotice("ckkskey", self, "Thought we were ready, but the key hierarchy is %@: %@", checkedstate, hierarchyError);
                nextState = checkedstate;
                if([nextState isEqualToString:SecCKKSZoneKeyStateError]) {
                    nextError = hierarchyError;
                }
            }
        }

    } else if([state isEqualToString: SecCKKSZoneKeyStateInitialized]) {
        // We're initialized and CloudKit is ready. See what needs done...

        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.zoneName];
        [self _onqueuePerformKeyStateInitialized:ckse];

        // We need to either:
        //  Wait for the fixup operation to occur
        //  Go into 'ready'
        //  Or start a key state fetch
        if(self.lastFixupOperation && ![self.lastFixupOperation isFinished]) {
            nextState = SecCKKSZoneKeyStateWaitForFixupOperation;
        } else {
            // Check if we have an existing key hierarchy in keyset
            if(keyset.error && !([keyset.error.domain isEqual: @"securityd"] && keyset.error.code == errSecItemNotFound)) {
                ckkserror("ckkskey", self, "Error examining existing key hierarchy: %@", error);
            }

            if(keyset.tlk && keyset.classA && keyset.classC && !keyset.error) {
                // This is likely a restart of securityd, and we think we're ready. Double check.

                CKKSZoneKeyState* checkedstate = [self _onqueueEnsureKeyHierarchyHealth:keyset error:&hierarchyError];
                if([checkedstate isEqualToString:SecCKKSZoneKeyStateReady] && !hierarchyError) {
                    ckksnotice("ckkskey", self, "Already have existing key hierarchy for %@; using it.", self.zoneID.zoneName);
                } else {
                    ckksnotice("ckkskey", self, "Initial scan shows key hierarchy is %@: %@", checkedstate, hierarchyError);
                }
                nextState = checkedstate;

            } else {
                // We have no local key hierarchy. One might exist in CloudKit, or it might not.
                ckksnotice("ckkskey", self, "No existing key hierarchy for %@. Check if there's one in CloudKit...", self.zoneID.zoneName);
                nextState = SecCKKSZoneKeyStateFetch;
            }
        }

    } else if([state isEqualToString:SecCKKSZoneKeyStateFetch]) {
        ckksnotice("ckkskey", self, "Starting a key hierarchy fetch");
        [self _onqueueKeyHierarchyFetch];

    } else if([state isEqualToString: SecCKKSZoneKeyStateNeedFullRefetch]) {
        ckksnotice("ckkskey", self, "Starting a key hierarchy full refetch");
        [self _onqueueKeyHierarchyRefetch];

    } else if([state isEqualToString:SecCKKSZoneKeyStateWaitForFixupOperation]) {
        // We should enter 'initialized' when the fixup operation completes
        ckksnotice("ckkskey", self, "Waiting for the fixup operation: %@", self.lastFixupOperation);

        self.keyStateMachineOperation = [NSBlockOperation named:@"key-state-after-fixup" withBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;
            [strongSelf dispatchSyncWithAccountKeys:^bool{
                ckksnotice("ckkskey", self, "Fixup operation complete! Restarting key hierarchy machinery");
                [strongSelf _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateInitialized withError:nil];
                return true;
            }];
        }];
        [self.keyStateMachineOperation addNullableDependency:self.lastFixupOperation];

    } else if([state isEqualToString: SecCKKSZoneKeyStateFetchComplete]) {
        // We've just completed a fetch of everything. Are there any remote keys?
        if(remoteKeys.count > 0u) {
            // Process the keys we received.
            self.keyStateMachineOperation = [[CKKSProcessReceivedKeysOperation alloc] initWithCKKSKeychainView: self];
        } else if( (keyset.currentTLKPointer || keyset.currentClassAPointer || keyset.currentClassCPointer) &&
                  !(keyset.tlk && keyset.classA && keyset.classC)) {
            // Huh. We appear to have current key pointers, but the keys themselves don't exist. That's weird.
            // Transfer to the "unhealthy" state to request a fix
            ckksnotice("ckkskey", self, "We appear to have current key pointers but no keys to match them. Moving to 'unhealthy'");
            nextState = SecCKKSZoneKeyStateUnhealthy;
        } else {
            // No remote keys, and the pointers look sane? Do we have an existing key hierarchy?
            CKKSZoneKeyState* checkedstate = [self _onqueueEnsureKeyHierarchyHealth:keyset error:&hierarchyError];
            if([checkedstate isEqualToString:SecCKKSZoneKeyStateReady] && !hierarchyError) {
                ckksnotice("ckkskey", self, "After fetch, everything looks good.");
                nextState = checkedstate;
            } else if(localKeys.count == 0 && remoteKeys.count == 0) {
                ckksnotice("ckkskey", self, "After fetch, we don't have any key hierarchy. Making a new one: %@", hierarchyError);
                self.keyStateMachineOperation = [[CKKSNewTLKOperation alloc] initWithCKKSKeychainView: self ckoperationGroup:self.keyHierarchyOperationGroup];
            } else {
                ckksnotice("ckkskey", self, "After fetch, we have a possibly unhealthy key hierarchy. Moving to %@: %@", checkedstate, hierarchyError);
                nextState = checkedstate;
            }
        }

    } else if([state isEqualToString: SecCKKSZoneKeyStateWaitForTLK]) {
        // We're in a hold state: waiting for the TLK bytes to arrive.

        if(self.keyStateProcessRequested) {
            // Someone has requsted a reprocess! Run a ProcessReceivedKeysOperation.
            ckksnotice("ckkskey", self, "Received a nudge that our TLK might be here! Starting operation to check.");
            [self _onqueueKeyHierarchyProcess];
        } else {
            // Should we nuke this zone?
            if([self _onqueueOtherDevicesReportHavingTLKs:keyset]) {
                ckksnotice("ckkskey", self, "Other devices report having TLK(%@). Entering a waiting state", keyset.currentTLKPointer);
            } else {
                ckksnotice("ckkskey", self, "No other devices have TLK(%@). Beginning zone reset...", keyset.currentTLKPointer);
                nextState = SecCKKSZoneKeyStateResettingZone;
            }
        }

    } else if([state isEqualToString: SecCKKSZoneKeyStateWaitForUnlock]) {
        ckksnotice("ckkskey", self, "Requested to enter waitforunlock");
        self.keyStateMachineOperation = [self operationToEnterState:SecCKKSZoneKeyStateInitialized keyStateError:nil named:@"key-state-after-unlock"];
        [self.keyStateMachineOperation addNullableDependency: self.lockStateTracker.unlockDependency];

    } else if([state isEqualToString: SecCKKSZoneKeyStateReadyPendingUnlock]) {
        ckksnotice("ckkskey", self, "Believe we're ready, but rechecking after unlock");
        self.keyStateMachineOperation = [self operationToEnterState:SecCKKSZoneKeyStateInitialized keyStateError:nil named:@"key-state-after-unlock"];
        [self.keyStateMachineOperation addNullableDependency: self.lockStateTracker.unlockDependency];

    } else if([state isEqualToString: SecCKKSZoneKeyStateBadCurrentPointers]) {
        // The current key pointers are broken, but we're not sure why.
        ckksnotice("ckkskey", self, "Our current key pointers are reported broken. Attempting a fix!");
        self.keyStateMachineOperation = [[CKKSHealKeyHierarchyOperation alloc] initWithCKKSKeychainView: self ckoperationGroup:self.keyHierarchyOperationGroup];

    } else if([state isEqualToString: SecCKKSZoneKeyStateNewTLKsFailed]) {
        ckksnotice("ckkskey", self, "Creating new TLKs didn't work. Attempting to refetch!");
        [self _onqueueKeyHierarchyFetch];

    } else if([state isEqualToString: SecCKKSZoneKeyStateHealTLKSharesFailed]) {
        ckksnotice("ckkskey", self, "Creating new TLK shares didn't work. Attempting to refetch!");
        [self _onqueueKeyHierarchyFetch];

    } else if([state isEqualToString:SecCKKSZoneKeyStateUnhealthy]) {
        ckksnotice("ckkskey", self, "Looks like the key hierarchy is unhealthy. Launching fix.");
        self.keyStateMachineOperation = [[CKKSHealKeyHierarchyOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:self.keyHierarchyOperationGroup];

    } else if([state isEqualToString:SecCKKSZoneKeyStateHealTLKShares]) {
        ckksnotice("ckksshare", self, "Key hierarchy is okay, but not shared appropriately. Launching fix.");
        self.keyStateMachineOperation = [[CKKSHealTLKSharesOperation alloc] initWithCKKSKeychainView:self
                                                                                    ckoperationGroup:self.keyHierarchyOperationGroup];

    } else {
        ckkserror("ckks", self, "asked to advance state machine to unknown state: %@", state);
        self.keyHierarchyState = state;
        [self _onqueueHandleKeyStateNonTransientDependency];
        return;
    }

    // Handle the key state ready dependency
    // If we're in ready and not entering a non-ready state, we should activate the ready dependency. Otherwise, we should create it.
    if(([state isEqualToString:SecCKKSZoneKeyStateReady] || [state isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock]) &&
       (nextState == nil || [nextState isEqualToString:SecCKKSZoneKeyStateReady] || [nextState isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock])) {

        // Ready enough!
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastKeystateReady inView:self];
        if(self.keyStateReadyDependency) {
            [self scheduleOperation: self.keyStateReadyDependency];
            self.keyStateReadyDependency = nil;
        }

        // If there are any OQEs waiting to be encrypted, launch an op to fix them
        if(outdatedOQEs > 0) {
            ckksnotice("ckksreencrypt", self, "Reencrypting outgoing items as the key hierarchy is ready");
            CKKSReencryptOutgoingItemsOperation* op = [[CKKSReencryptOutgoingItemsOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:self.keyHierarchyOperationGroup];
            [self scheduleOperation:op];
        }
    } else {
        // Not in ready: we need a key state ready dependency
        if(self.keyStateReadyDependency == nil || [self.keyStateReadyDependency isFinished]) {
            self.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"key-state-broken"];
            self.keyStateReadyDependency = [self createKeyStateReadyDependency:@"Key state has become ready again." ckoperationGroup:self.keyHierarchyOperationGroup];
        }
    }

    NSAssert(!((self.keyStateMachineOperation != nil) &&
               (nextState != nil)),
             @"Should have a machine operation or a next state, not both");

    // Start any operations, or log that we aren't
    if(self.keyStateMachineOperation) {
        [self scheduleOperation: self.keyStateMachineOperation];
        ckksnotice("ckkskey", self, "Now in key state: %@", state);
        self.keyHierarchyState = state;

    } else if([state isEqualToString:SecCKKSZoneKeyStateError]) {
        ckksnotice("ckkskey", self, "Entering key state 'error'");
        self.keyHierarchyState = state;

    } else if(nextState == nil) {
        ckksnotice("ckkskey", self, "Entering key state: %@", state);
        self.keyHierarchyState = state;

    } else if(![state isEqualToString: nextState]) {
        ckksnotice("ckkskey", self, "Staying in state %@, but proceeding to %@ as soon as possible", self.keyHierarchyState, nextState);
        self.keyStateMachineOperation = [self operationToEnterState:nextState keyStateError:nextError named:@"next-key-state"];
        [self scheduleOperation: self.keyStateMachineOperation];

    } else {
        // Nothing to do and not in a waiting state? This is likely a bug, but, hey: pretend to be in ready!
        if(!([state isEqualToString:SecCKKSZoneKeyStateReady] || [state isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock])) {
            ckkserror("ckkskey", self, "No action to take in state %@; BUG, but: maybe we're ready?", state);
            nextState = SecCKKSZoneKeyStateReady;
            self.keyStateMachineOperation = [self operationToEnterState:nextState keyStateError:nil named:@"next-key-state"];
            [self scheduleOperation: self.keyStateMachineOperation];
        }
    }

    [self _onqueueHandleKeyStateNonTransientDependency];
}

- (void)_onqueueHandleKeyStateNonTransientDependency {
    dispatch_assert_queue(self.queue);

    if(CKKSKeyStateTransient(self.keyHierarchyState)) {
        if(self.keyStateNonTransientDependency == nil || [self.keyStateNonTransientDependency isFinished]) {
            self.keyStateNonTransientDependency = [self createKeyStateNontransientDependency];
        }
    } else {
        // Nontransient: go for it
        if(self.keyStateNonTransientDependency) {
            [self scheduleOperation: self.keyStateNonTransientDependency];
            self.keyStateNonTransientDependency = nil;
        }
    }
}

- (NSOperation*)operationToEnterState:(CKKSZoneKeyState*)state keyStateError:(NSError* _Nullable)keyStateError named:(NSString*)name {
    __weak __typeof(self) weakSelf = self;

    return [NSBlockOperation named:name withBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            return;
        }
        [strongSelf dispatchSyncWithAccountKeys:^bool{
            [strongSelf _onqueueAdvanceKeyStateMachineToState:state withError:keyStateError];
            return true;
        }];
    }];
}

- (bool)_onqueueOtherDevicesReportHavingTLKs:(CKKSCurrentKeySet*)keyset
{
    dispatch_assert_queue(self.queue);

    //Has there been any activity indicating that other trusted devices have keys in the past 45 days, or untrusted devices in the past 4?
    // (We chose 4 as devices attempt to upload their device state every 3 days. If a device is unceremoniously kicked out of circle, we normally won't immediately reset.)
    NSDate* now = [NSDate date];
    NSDateComponents* trustedOffset = [[NSDateComponents alloc] init];
    [trustedOffset setDay:-45];
    NSDate* trustedDeadline = [[NSCalendar currentCalendar] dateByAddingComponents:trustedOffset toDate:now options:0];

    NSDateComponents* untrustedOffset = [[NSDateComponents alloc] init];
    [untrustedOffset setDay:-4];
    NSDate* untrustedDeadline = [[NSCalendar currentCalendar] dateByAddingComponents:untrustedOffset toDate:now options:0];

    NSMutableSet<NSString*>* trustedPeerIDs = [NSMutableSet set];
    for(id<CKKSPeer> peer in self.currentTrustedPeers) {
        [trustedPeerIDs addObject:peer.peerID];
    }

    NSError* localerror = nil;

    NSArray<CKKSDeviceStateEntry*>* allDeviceStates = [CKKSDeviceStateEntry allInZone:self.zoneID error:&localerror];
    if(localerror) {
        ckkserror("ckkskey", self, "Error fetching device states: %@", localerror);
        localerror = nil;
        return true;
    }
    for(CKKSDeviceStateEntry* device in allDeviceStates) {
        if([trustedPeerIDs containsObject:device.circlePeerID]) {
            // Is this a recent DSE? If it's older than the deadline, skip it
            if([device.storedCKRecord.modificationDate compare:trustedDeadline] == NSOrderedAscending) {
                ckksnotice("ckkskey", self, "Trusted device state (%@) is too old; ignoring", device);
                continue;
            }
        } else {
            // Device is untrusted. How does it fare with the untrustedDeadline?
            if([device.storedCKRecord.modificationDate compare:untrustedDeadline] == NSOrderedAscending) {
                ckksnotice("ckkskey", self, "Device (%@) is not trusted and from too long ago; ignoring device state (%@)", device.circlePeerID, device);
                continue;
            } else {
                ckksnotice("ckkskey", self, "Device (%@) is not trusted, but very recent. Including in heuristic: %@", device.circlePeerID, device);
            }
        }

        if([device.keyState isEqualToString:SecCKKSZoneKeyStateReady] ||
           [device.keyState isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock]) {
            ckksnotice("ckkskey", self, "Other device (%@) has keys; it should send them to us", device);
            return true;
        }
    }

    NSArray<CKKSTLKShare*>* tlkShares = [CKKSTLKShare allForUUID:keyset.currentTLKPointer.currentKeyUUID
                                                          zoneID:self.zoneID
                                                           error:&localerror];
    if(localerror) {
        ckkserror("ckkskey", self, "Error fetching device states: %@", localerror);
        localerror = nil;
        return false;
    }

    for(CKKSTLKShare* tlkShare in tlkShares) {
        if([trustedPeerIDs containsObject:tlkShare.senderPeerID] &&
           [tlkShare.storedCKRecord.modificationDate compare:trustedDeadline] == NSOrderedDescending) {
            ckksnotice("ckkskey", self, "Trusted TLK Share (%@) created recently; other devices have keys and should send them to us", tlkShare);
            return true;
        }
    }

    // Okay, how about the untrusted deadline?
    for(CKKSTLKShare* tlkShare in tlkShares) {
        if([tlkShare.storedCKRecord.modificationDate compare:untrustedDeadline] == NSOrderedDescending) {
            ckksnotice("ckkskey", self, "Untrusted TLK Share (%@) created very recently; other devices might have keys and should rejoin the circle (and send them to us)", tlkShare);
            return true;
        }
    }

    return false;
}

// For this key, who doesn't yet have a valid CKKSTLKShare for it?
// Note that we really want a record sharing the TLK to ourselves, so this function might return
// a non-empty set even if all peers have the TLK: it wants us to make a record for ourself.
- (NSSet<id<CKKSPeer>>*)_onqueueFindPeersMissingShare:(CKKSKey*)key error:(NSError* __autoreleasing*)error {
    dispatch_assert_queue(self.queue);

    if(!key) {
        ckkserror("ckksshare", self, "Attempting to find missing shares for nil key");
        return [NSSet set];
    }

    if(self.currentTrustedPeersError) {
        ckkserror("ckksshare", self, "Couldn't find missing shares because trusted peers aren't available: %@", self.currentTrustedPeersError);
        if(error) {
            *error = self.currentTrustedPeersError;
        }
        return [NSSet set];
    }
    if(self.currentSelfPeersError) {
        ckkserror("ckksshare", self, "Couldn't find missing shares because self peers aren't available: %@", self.currentSelfPeersError);
        if(error) {
            *error = self.currentSelfPeersError;
        }
        return [NSSet set];
    }

    NSMutableSet<id<CKKSPeer>>* peersMissingShares = [NSMutableSet set];

    NSMutableSet<NSString*>* trustedPeerIDs = [NSMutableSet set];
    for(id<CKKSPeer> peer in self.currentTrustedPeers) {
        [trustedPeerIDs addObject:peer.peerID];
    }

    for(id<CKKSPeer> peer in self.currentTrustedPeers) {
        NSError* peerError = nil;
        // Find all the shares for this peer for this key
        NSArray<CKKSTLKShare*>* currentPeerShares = [CKKSTLKShare allFor:peer.peerID
                                                                 keyUUID:key.uuid
                                                                  zoneID:self.zoneID
                                                                   error:&peerError];

        if(peerError) {
            ckkserror("ckksshare", self, "Couldn't load shares for peer %@: %@", peer, peerError);
            if(error) {
                *error = peerError;
            }
            return nil;
        }

        // Determine if we think this peer has enough things shared to them
        bool alreadyShared = false;
        for(CKKSTLKShare* existingPeerShare in currentPeerShares) {
            // If an SOS Peer sent this share, is its signature still valid? Or did the signing key change?
            if([existingPeerShare.senderPeerID hasPrefix:CKKSSOSPeerPrefix]) {
                NSError* signatureError = nil;
                if(![existingPeerShare signatureVerifiesWithPeerSet:self.currentTrustedPeers error:&signatureError]) {
                    ckksnotice("ckksshare", self, "Existing TLKShare's signature doesn't verify with current peer set: %@ %@", signatureError, existingPeerShare);
                    continue;
                }
            }

            if([existingPeerShare.tlkUUID isEqualToString: key.uuid] && [trustedPeerIDs containsObject:existingPeerShare.senderPeerID]) {

                // Was this shared to us?
                if([peer.peerID isEqualToString: self.currentSelfPeers.currentSelf.peerID]) {
                    // We only count this as 'found' if we did the sharing and it's to our current keys
                    if([existingPeerShare.senderPeerID isEqualToString:self.currentSelfPeers.currentSelf.peerID] &&
                       [existingPeerShare.receiver.publicEncryptionKey isEqual:self.currentSelfPeers.currentSelf.publicEncryptionKey]) {
                        ckksnotice("ckksshare", self, "Local peer %@ is shared %@ via self: %@", peer, key, existingPeerShare);
                        alreadyShared = true;
                    } else {
                        ckksnotice("ckksshare", self, "Local peer %@ is shared %@ via trusted %@, but that's not good enough", peer, key, existingPeerShare);
                    }

                } else {
                    // Was this shared to the remote peer's current keys?
                    if([peer.publicEncryptionKey isEqual: existingPeerShare.receiver.publicEncryptionKey]) {
                        // Some other peer has a trusted share. Cool!
                        ckksnotice("ckksshare", self, "Peer %@ is shared %@ via trusted %@", peer, key, existingPeerShare);
                        alreadyShared = true;
                    } else {
                        ckksnotice("ckksshare", self, "Peer %@ has a share for %@, but to old keys: %@", peer, key, existingPeerShare);
                    }
                }
            }
        }

        if(!alreadyShared) {
            // Add this peer to our set, if it has an encryption key to receive the share
            if(peer.publicEncryptionKey) {
                [peersMissingShares addObject:peer];
            }
        }
    }

    if(peersMissingShares.count > 0u) {
        // Log each and every one of the things
        ckksnotice("ckksshare", self, "Missing TLK shares for %lu peers: %@", (unsigned long)peersMissingShares.count, peersMissingShares);
        ckksnotice("ckksshare", self, "Self peers are (%@) %@", self.currentSelfPeersError ?: @"no error", self.currentSelfPeers);
        ckksnotice("ckksshare", self, "Trusted peers are (%@) %@", self.currentTrustedPeersError ?: @"no error", self.currentTrustedPeers);
    }

    return peersMissingShares;
}

- (NSSet<CKKSTLKShare*>*)_onqueueCreateMissingKeyShares:(CKKSKey*)key error:(NSError* __autoreleasing*)error {
    dispatch_assert_queue(self.queue);

    if(self.currentTrustedPeersError) {
        ckkserror("ckksshare", self, "Couldn't create missing shares because trusted peers aren't available: %@", self.currentTrustedPeersError);
        if(error) {
            *error = self.currentTrustedPeersError;
        }
        return nil;
    }
    if(self.currentSelfPeersError) {
        ckkserror("ckksshare", self, "Couldn't create missing shares because self peers aren't available: %@", self.currentSelfPeersError);
        if(error) {
            *error = self.currentSelfPeersError;
        }
        return nil;
    }

    NSSet<id<CKKSPeer>>* remainingPeers = [self _onqueueFindPeersMissingShare:key error:error];
    NSMutableSet<CKKSTLKShare*>* newShares = [NSMutableSet set];

    if(!remainingPeers) {
        return nil;
    }

    NSError* localerror = nil;

    if(![key ensureKeyLoaded:error]) {
        return nil;
    }

    for(id<CKKSPeer> peer in remainingPeers) {
        if(!peer.publicEncryptionKey) {
            ckksnotice("ckksshare", self, "No need to make TLK for %@; they don't have any encryption keys", peer);
            continue;
        }

        // Create a share for this peer.
        ckksnotice("ckksshare", self, "Creating share of %@ as %@ for %@", key, self.currentSelfPeers.currentSelf, peer);
        CKKSTLKShare* newShare = [CKKSTLKShare share:key
                                                  as:self.currentSelfPeers.currentSelf
                                                  to:peer
                                               epoch:-1
                                            poisoned:0
                                               error:&localerror];

        if(localerror) {
            ckkserror("ckksshare", self, "Couldn't create new share for %@: %@", peer, localerror);
            if(error) {
                *error = localerror;
            }
            return nil;
        }

        [newShares addObject: newShare];
    }

    return newShares;
}

- (CKKSZoneKeyState*)_onqueueEnsureKeyHierarchyHealth:(CKKSCurrentKeySet*)set error:(NSError* __autoreleasing *)error {
    dispatch_assert_queue(self.queue);

    // Check keyset
    if(!set.tlk || !set.classA || !set.classC) {
        ckkserror("ckkskey", self, "Error examining existing key hierarchy: %@", set);
        if(error) {
            *error = set.error;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }

    NSError* localerror = nil;
    bool probablyOkIfUnlocked = false;

    // keychain being locked is not a fatal error here
    [set.tlk loadKeyMaterialFromKeychain:&localerror];
    if(localerror && !([localerror.domain isEqual: @"securityd"] && localerror.code == errSecInteractionNotAllowed)) {
        ckkserror("ckkskey", self, "Error loading TLK(%@): %@", set.tlk, localerror);
        if(error) {
            *error = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    } else if(localerror) {
        ckkserror("ckkskey", self, "Soft error loading TLK(%@), maybe locked: %@", set.tlk, localerror);
        probablyOkIfUnlocked = true;
    }
    localerror = nil;

    // keychain being locked is not a fatal error here
    [set.classA loadKeyMaterialFromKeychain:&localerror];
    if(localerror && !([localerror.domain isEqual: @"securityd"] && localerror.code == errSecInteractionNotAllowed)) {
        ckkserror("ckkskey", self, "Error loading classA key(%@): %@", set.classA, localerror);
        if(error) {
            *error = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    } else if(localerror) {
        ckkserror("ckkskey", self, "Soft error loading classA key(%@), maybe locked: %@", set.classA, localerror);
        probablyOkIfUnlocked = true;
    }
    localerror = nil;

    // keychain being locked is a fatal error here, since this is class C
    [set.classC loadKeyMaterialFromKeychain:&localerror];
    if(localerror) {
        ckkserror("ckkskey", self, "Error loading classC(%@): %@", set.classC, localerror);
        if(error) {
            *error = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }

    // Check that the classA and classC keys point to the current TLK
    if(![set.classA.parentKeyUUID isEqualToString: set.tlk.uuid]) {
        localerror = [NSError errorWithDomain:CKKSServerExtensionErrorDomain
                                         code:CKKSServerUnexpectedSyncKeyInChain
                                     userInfo:@{
                                                NSLocalizedDescriptionKey: @"Current class A key does not wrap to current TLK",
                                               }];
        ckkserror("ckkskey", self, "Key hierarchy unhealthy: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }
    if(![set.classC.parentKeyUUID isEqualToString: set.tlk.uuid]) {
        localerror = [NSError errorWithDomain:CKKSServerExtensionErrorDomain
                                         code:CKKSServerUnexpectedSyncKeyInChain
                                     userInfo:@{
                                                NSLocalizedDescriptionKey: @"Current class C key does not wrap to current TLK",
                                               }];
        ckkserror("ckkskey", self, "Key hierarchy unhealthy: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return SecCKKSZoneKeyStateUnhealthy;
    }

    self.activeTLK = [set.tlk uuid];

    // Now that we're pretty sure we have the keys, are they shared appropriately?
    // Check that every trusted peer has at least one TLK share
    NSSet<id<CKKSPeer>>* missingShares = [self _onqueueFindPeersMissingShare:set.tlk error:&localerror];
    if(localerror && [self.lockStateTracker isLockedError: localerror]) {
        ckkserror("ckkskey", self, "Couldn't find missing TLK shares due to lock state: %@", localerror);
        probablyOkIfUnlocked = true;
    } else if([localerror.domain isEqualToString:CKKSErrorDomain] && localerror.code == CKKSNoPeersAvailable) {
        ckkserror("ckkskey", self, "Couldn't find missing TLK shares due to missing peers, likely due to lock state: %@", localerror);
        probablyOkIfUnlocked = true;

    } else if(localerror) {
        if(error) {
            *error = localerror;
        }
        ckkserror("ckkskey", self, "Error finding missing TLK shares: %@", localerror);
        return SecCKKSZoneKeyStateError;
    }

    if(!missingShares || missingShares.count != 0u) {
        localerror = [NSError errorWithDomain:CKKSErrorDomain code:CKKSMissingTLKShare
                                  description:[NSString stringWithFormat:@"Missing shares for %lu peers", (unsigned long)missingShares.count]];
        if(error) {
            *error = localerror;
        }
        return SecCKKSZoneKeyStateHealTLKShares;
    } else {
        ckksnotice("ckksshare", self, "TLK (%@) is shared correctly", set.tlk);
    }

    // Got to the bottom? Cool! All keys are present and accounted for.
    return probablyOkIfUnlocked ? SecCKKSZoneKeyStateReadyPendingUnlock : SecCKKSZoneKeyStateReady;
}

- (void)_onqueueKeyHierarchyFetch {
    dispatch_assert_queue(self.queue);

    __weak __typeof(self) weakSelf = self;
    self.keyStateMachineOperation = [NSBlockOperation blockOperationWithBlock: ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return;
        }

        [strongSelf dispatchSyncWithAccountKeys: ^bool{
            [strongSelf _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateFetchComplete withError: nil];
            return true;
        }];
    }];
    self.keyStateMachineOperation.name = @"waiting-for-fetch";

    NSOperation* fetchOp = [self.zoneChangeFetcher requestSuccessfulFetch: CKKSFetchBecauseKeyHierarchy];
    [self.keyStateMachineOperation addDependency: fetchOp];

    self.keyStateFetchRequested = false;
}

- (void)_onqueueKeyHierarchyRefetch {
    dispatch_assert_queue(self.queue);

    __weak __typeof(self) weakSelf = self;
    self.keyStateMachineOperation = [NSBlockOperation blockOperationWithBlock: ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return;
        }

        [strongSelf dispatchSyncWithAccountKeys: ^bool{
            [strongSelf _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateFetchComplete withError: nil];
            return true;
        }];
    }];
    self.keyStateMachineOperation.name = @"waiting-for-refetch";

    NSOperation* fetchOp = [self.zoneChangeFetcher requestSuccessfulResyncFetch: CKKSFetchBecauseKeyHierarchy];
    [self.keyStateMachineOperation addDependency: fetchOp];

    self.keyStateMachineRefetched = true;
    self.keyStateFullRefetchRequested = false;
    self.keyStateFetchRequested = false;
}


- (void)_onqueueKeyHierarchyProcess {
    dispatch_assert_queue(self.queue);

    self.keyStateMachineOperation = [[CKKSProcessReceivedKeysOperation alloc] initWithCKKSKeychainView: self];

    // Since we're starting a reprocess, this is answering all previous requests.
    self.keyStateProcessRequested = false;
}

- (void) handleKeychainEventDbConnection: (SecDbConnectionRef) dbconn
                                   added: (SecDbItemRef) added
                                 deleted: (SecDbItemRef) deleted
                             rateLimiter: (CKKSRateLimiter*) rateLimiter
                            syncCallback: (SecBoolNSErrorCallback) syncCallback {
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckks", self, "Skipping handleKeychainEventDbConnection due to disabled CKKS");
        return;
    }

    __block NSError* error = nil;

    // Tombstones come in as item modifications or item adds. Handle modifications here.
    bool addedTombstone   = added   && SecDbItemIsTombstone(added);
    bool deletedTombstone = deleted && SecDbItemIsTombstone(deleted);

    bool addedSync   = added   && SecDbItemIsSyncable(added);
    bool deletedSync = deleted && SecDbItemIsSyncable(deleted);

    bool isAdd    = ( added && !deleted) || (added && deleted && !addedTombstone &&  deletedTombstone) || (added && deleted &&  addedSync && !deletedSync);
    bool isDelete = (!added &&  deleted) || (added && deleted &&  addedTombstone && !deletedTombstone) || (added && deleted && !addedSync &&  deletedSync);
    bool isModify = ( added &&  deleted) && (!isAdd) && (!isDelete);

    // On an update that changes an item's primary key, SecDb modifies the existing item, then adds a new tombstone to replace the old primary key.
    // Therefore, we might receive an added tombstone here with no deleted item to accompany it. This should be considered a deletion.
    if(addedTombstone && !deleted) {
        isAdd = false;
        isDelete = true;
        isModify = false;

        // Passed to withItem: below
        deleted = added;
    }

    // If neither item is syncable, don't proceed further in the syncing system
    bool proceed = addedSync || deletedSync;

    if(!proceed) {
        ckksnotice("ckks", self, "skipping sync of non-sync item (%d, %d)", addedSync, deletedSync);
        return;
    }

    // Only synchronize items which can transfer between devices
    NSString* protection = (__bridge NSString*)SecDbItemGetCachedValueWithName(added ? added : deleted, kSecAttrAccessible);
    if(! ([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleWhenUnlocked] ||
          [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAfterFirstUnlock] ||
          [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAlwaysPrivate])) {
        ckksnotice("ckks", self, "skipping sync of device-bound(%@) item", protection);
        return;
    }

    // Our caller gave us a database connection. We must get on the local queue to ensure atomicity
    // Note that we're at the mercy of the surrounding db transaction, so don't try to rollback here
    [self dispatchSyncWithConnection: dbconn block: ^bool {
        // Schedule a "view changed" notification
        [self.notifyViewChangedScheduler trigger];

        if(self.accountStatus == CKKSAccountStatusNoAccount) {
            // No account; CKKS shouldn't attempt anything.
            self.droppedItems = true;

            if(syncCallback) {
                // We're positively not logged into CloudKit, and therefore don't expect this item to be synced anytime particularly soon.
                [self callSyncCallbackWithErrorNoAccount: syncCallback];
            }
            return true;
        }

        // Always record the callback, even if we can't encrypt the item right now. Maybe we'll get to it soon!
        if(syncCallback) {
            CFErrorRef cferror = NULL;
            NSString* uuid = (__bridge_transfer NSString*) CFRetain(SecDbItemGetValue(added, &v10itemuuid, &cferror));
            if(!cferror && uuid) {
                self.pendingSyncCallbacks[uuid] = syncCallback;
            }
            CFReleaseNull(cferror);
        }

        CKKSOutgoingQueueEntry* oqe = nil;
        if       (isAdd) {
            oqe = [CKKSOutgoingQueueEntry withItem: added   action: SecCKKSActionAdd    ckks:self error: &error];
        } else if(isDelete) {
            oqe = [CKKSOutgoingQueueEntry withItem: deleted action: SecCKKSActionDelete ckks:self error: &error];
        } else if(isModify) {
            oqe = [CKKSOutgoingQueueEntry withItem: added   action: SecCKKSActionModify ckks:self error: &error];
        } else {
            ckkserror("ckks", self, "processKeychainEventItemAdded given garbage: %@ %@", added, deleted);
            return true;
        }

        CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName:@"keychain-api-use"];

        if(error) {
            ckkserror("ckks", self, "Couldn't create outgoing queue entry: %@", error);
            self.droppedItems = true;

            // If the problem is 'no UUID', launch a scan operation to find and fix it
            // We don't want to fix it up here, in the closing moments of a transaction
            if([error.domain isEqualToString:CKKSErrorDomain] && error.code == CKKSNoUUIDOnItem) {
                ckksnotice("ckks", self, "Launching scan operation to find UUID");
                [self scanLocalItems:@"uuid-find-scan" ckoperationGroup:operationGroup after:nil];
            }

            // If the problem is 'couldn't load key', tell the key hierarchy state machine to fix it
            if([error.domain isEqualToString:CKKSErrorDomain] && error.code == errSecItemNotFound) {
                [self.pokeKeyStateMachineScheduler trigger];
            }

            return true;
        }

        if(rateLimiter) {
            NSDate* limit = nil;
            NSInteger value = [rateLimiter judge:oqe at:[NSDate date] limitTime:&limit];
            if(limit) {
                oqe.waitUntil = limit;
                SecPLLogRegisteredEvent(@"CKKSSyncing", @{ @"ratelimit" : @(value), @"accessgroup" : oqe.accessgroup});
            }
        }

        [oqe saveToDatabaseWithConnection: dbconn error: &error];
        if(error) {
            ckkserror("ckks", self, "Couldn't save outgoing queue entry to database: %@", error);
            return true;
        } else {
            ckksnotice("ckks", self, "Saved %@ to outgoing queue", oqe);
        }

        // This update supercedes all other local modifications to this item (_except_ those in-flight).
        // Delete all items in reencrypt or error.
        CKKSOutgoingQueueEntry* reencryptOQE = [CKKSOutgoingQueueEntry tryFromDatabase:oqe.uuid state:SecCKKSStateReencrypt zoneID:self.zoneID error:&error];
        if(error) {
            ckkserror("ckks", self, "Couldn't load reencrypt OQE sibling for %@: %@", oqe, error);
        }
        if(reencryptOQE) {
            [reencryptOQE deleteFromDatabase:&error];
            if(error) {
                ckkserror("ckks", self, "Couldn't delete reencrypt OQE sibling(%@) for %@: %@", reencryptOQE, oqe, error);
            }
            error = nil;
        }

        CKKSOutgoingQueueEntry* errorOQE = [CKKSOutgoingQueueEntry tryFromDatabase:oqe.uuid state:SecCKKSStateError zoneID:self.zoneID error:&error];
        if(error) {
            ckkserror("ckks", self, "Couldn't load error OQE sibling for %@: %@", oqe, error);
        }
        if(errorOQE) {
            [errorOQE deleteFromDatabase:&error];
            if(error) {
                ckkserror("ckks", self, "Couldn't delete error OQE sibling(%@) for %@: %@", reencryptOQE, oqe, error);
            }
        }

        [self processOutgoingQueue:operationGroup];

        return true;
    }];
}

-(void)setCurrentItemForAccessGroup:(NSData* _Nonnull)newItemPersistentRef
                               hash:(NSData*)newItemSHA1
                        accessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                          replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                               hash:(NSData*)oldItemSHA1
                           complete:(void (^) (NSError* operror)) complete
{
    if(accessGroup == nil || identifier == nil) {
        NSError* error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:errSecParam
                                      description:@"No access group or identifier given"];
        ckkserror("ckkscurrent", self, "Cancelling request: %@", error);
        complete(error);
        return;
    }

    // Not being in a CloudKit account is an automatic failure.
    // But, wait a good long while for the CloudKit account state to be known (in the case of daemon startup)
    [self.accountStateKnown wait:(SecCKKSTestsEnabled() ? 1*NSEC_PER_SEC : 30*NSEC_PER_SEC)];

    if(self.accountStatus != CKKSAccountStatusAvailable) {
        NSError* error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNotLoggedIn
                                      description:@"User is not signed into iCloud."];
        ckksnotice("ckkscurrent", self, "Rejecting current item pointer set since we don't have an iCloud account.");
        complete(error);
        return;
    }

    ckksnotice("ckkscurrent", self, "Starting change current pointer operation for %@-%@", accessGroup, identifier);
    CKKSUpdateCurrentItemPointerOperation* ucipo = [[CKKSUpdateCurrentItemPointerOperation alloc] initWithCKKSKeychainView:self
                                                                                                                   newItem:newItemPersistentRef
                                                                                                                      hash:newItemSHA1
                                                                                                               accessGroup:accessGroup
                                                                                                                identifier:identifier
                                                                                                                 replacing:oldCurrentItemPersistentRef
                                                                                                                      hash:oldItemSHA1
                                                                                                          ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"currentitem-api"]];

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* returnCallback = [CKKSResultOperation operationWithBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;

        if(ucipo.error) {
            ckkserror("ckkscurrent", strongSelf, "Failed setting a current item pointer for %@ with %@", ucipo.currentPointerIdentifier, ucipo.error);
        } else {
            ckksnotice("ckkscurrent", strongSelf, "Finished setting a current item pointer for %@", ucipo.currentPointerIdentifier);
        }
        complete(ucipo.error);
    }];
    returnCallback.name = @"setCurrentItem-return-callback";
    [returnCallback addDependency: ucipo];
    [self scheduleOperation: returnCallback];

    // Now, schedule ucipo. It modifies the CloudKit zone, so it should insert itself into the list of OutgoingQueueOperations.
    // Then, we won't have simultaneous zone-modifying operations.
    [ucipo linearDependencies:self.outgoingQueueOperations];

    // If this operation hasn't started within 60 seconds, cancel it and return a "timed out" error.
    [ucipo timeout:60*NSEC_PER_SEC];

    [self scheduleOperation:ucipo];
    return;
}

-(void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                    fetchCloudValue:(bool)fetchCloudValue
                           complete:(void (^) (NSString* uuid, NSError* operror)) complete
{
    if(accessGroup == nil || identifier == nil) {
        ckksnotice("ckkscurrent", self, "Rejecting current item pointer get since no access group(%@) or identifier(%@) given", accessGroup, identifier);
        complete(NULL, [NSError errorWithDomain:CKKSErrorDomain
                                           code:errSecParam
                                    description:@"No access group or identifier given"]);
        return;
    }

    // Not being in a CloudKit account is an automatic failure.
    // But, wait a good long while for the CloudKit account state to be known (in the case of daemon startup)
    [self.accountStateKnown wait:(SecCKKSTestsEnabled() ? 1*NSEC_PER_SEC : 30*NSEC_PER_SEC)];

    if(self.accountStatus != CKKSAccountStatusAvailable) {
        ckksnotice("ckkscurrent", self, "Rejecting current item pointer get since we don't have an iCloud account.");
        complete(NULL, [NSError errorWithDomain:CKKSErrorDomain
                                           code:CKKSNotLoggedIn
                                    description:@"User is not signed into iCloud."]);
        return;
    }

    CKKSResultOperation* fetchAndProcess = nil;
    if(fetchCloudValue) {
        fetchAndProcess = [self fetchAndProcessCKChanges:CKKSFetchBecauseCurrentItemFetchRequest];
    }

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* getCurrentItem = [CKKSResultOperation named:@"get-current-item-pointer" withBlock:^{
        if(fetchAndProcess.error) {
            ckksnotice("ckkscurrent", self, "Rejecting current item pointer get since fetch failed: %@", fetchAndProcess.error);
            complete(NULL, fetchAndProcess.error);
            return;
        }

        __strong __typeof(self) strongSelf = weakSelf;

        [strongSelf dispatchSync: ^bool {
            NSError* error = nil;
            NSString* currentIdentifier = [NSString stringWithFormat:@"%@-%@", accessGroup, identifier];

            CKKSCurrentItemPointer* cip = [CKKSCurrentItemPointer fromDatabase:currentIdentifier
                                                                         state:SecCKKSProcessedStateLocal
                                                                        zoneID:strongSelf.zoneID
                                                                         error:&error];
            if(!cip || error) {
                ckkserror("ckkscurrent", strongSelf, "No current item pointer for %@", currentIdentifier);
                complete(nil, error);
                return false;
            }

            if(!cip.currentItemUUID) {
                ckkserror("ckkscurrent", strongSelf, "Current item pointer is empty %@", cip);
                complete(nil, [NSError errorWithDomain:CKKSErrorDomain
                                                  code:errSecInternalError
                                           description:@"Current item pointer is empty"]);
                return false;
            }

            ckksinfo("ckkscurrent", strongSelf, "Retrieved current item pointer: %@", cip);
            complete(cip.currentItemUUID, NULL);
            return true;
        }];
    }];

    [getCurrentItem addNullableDependency:fetchAndProcess];
    [self scheduleOperation: getCurrentItem];
}

- (CKKSKey*) keyForItem: (SecDbItemRef) item error: (NSError * __autoreleasing *) error {
    CKKSKeyClass* class = nil;

    NSString* protection = (__bridge NSString*)SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);
    if([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleWhenUnlocked]) {
        class = SecCKKSKeyClassA;
    } else if([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAlwaysPrivate] ||
              [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAfterFirstUnlock]) {
        class = SecCKKSKeyClassC;
    } else {
        NSError* localError = [NSError errorWithDomain:CKKSErrorDomain
                                                  code:CKKSInvalidKeyClass
                                           description:[NSString stringWithFormat:@"can't pick key class for protection %@", protection]];
        ckkserror("ckks", self, "can't pick key class: %@ %@", localError, item);
        if(error) {
            *error = localError;
        }

        return nil;
    }

    NSError* currentKeyError = nil;
    CKKSKey* key = [CKKSKey currentKeyForClass: class zoneID:self.zoneID error:&currentKeyError];
    if(!key || currentKeyError) {
        ckkserror("ckks", self, "Couldn't find current key for %@: %@", class, currentKeyError);

        if(error) {
            *error = currentKeyError;
        }
        return nil;
    }

    // and make sure it's unwrapped.
    NSError* loadedError = nil;
    if(![key ensureKeyLoaded:&loadedError]) {
        ckkserror("ckks", self, "Couldn't load key(%@): %@", key, loadedError);
        if(error) {
            *error = loadedError;
        }
        return nil;
    }

    return key;
}

// Use the following method to find the first pending operation in a weak collection
- (NSOperation*)findFirstPendingOperation: (NSHashTable*) table {
    return [self findFirstPendingOperation:table ofClass:nil];
}

// Use the following method to find the first pending operation in a weak collection
- (NSOperation*)findFirstPendingOperation: (NSHashTable*) table ofClass:(Class)class {
    @synchronized(table) {
        for(NSOperation* op in table) {
            if(op != nil && [op isPending] && (class == nil || [op isKindOfClass: class])) {
                return op;
            }
        }
        return nil;
    }
}

// Use the following method to count the pending operations in a weak collection
- (int64_t)countPendingOperations: (NSHashTable*) table {
    @synchronized(table) {
        int count = 0;
        for(NSOperation* op in table) {
            if(op != nil && !([op isExecuting] || [op isFinished])) {
                count++;
            }
        }
        return count;
    }
}

- (CKKSOutgoingQueueOperation*)processOutgoingQueue:(CKOperationGroup*)ckoperationGroup {
    return [self processOutgoingQueueAfter:nil ckoperationGroup:ckoperationGroup];
}

- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation*)after ckoperationGroup:(CKOperationGroup*)ckoperationGroup {
    CKKSOutgoingQueueOperation* outgoingop =
            (CKKSOutgoingQueueOperation*) [self findFirstPendingOperation:self.outgoingQueueOperations
                                                                  ofClass:[CKKSOutgoingQueueOperation class]];
    if(outgoingop) {
        if(after) {
            [outgoingop addDependency: after];
        }
        if([outgoingop isPending]) {
            if(!outgoingop.ckoperationGroup && ckoperationGroup) {
                outgoingop.ckoperationGroup = ckoperationGroup;
            } else if(ckoperationGroup) {
                ckkserror("ckks", self, "Throwing away CKOperationGroup(%@) in favor of (%@)", ckoperationGroup.name, outgoingop.ckoperationGroup.name);
            }

            // Will log any pending dependencies as well
            ckksnotice("ckksoutgoing", self, "Returning existing %@", outgoingop);

            // Shouldn't be necessary, but can't hurt
            [self.outgoingQueueOperationScheduler trigger];
            return outgoingop;
        }
    }

    CKKSOutgoingQueueOperation* op = [[CKKSOutgoingQueueOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:ckoperationGroup];
    op.name = @"outgoing-queue-operation";
    [op addNullableDependency:after];
    [op addNullableDependency:self.outgoingQueueOperationScheduler.operationDependency];
    [self.outgoingQueueOperationScheduler trigger];

    [self scheduleOperation: op];
    ckksnotice("ckksoutgoing", self, "Scheduled %@", op);
    return op;
}

- (void)processIncomingQueueAfterNextUnlock {
    // Thread races aren't so important here; we might end up with two or three copies of this operation, but that's okay.
    if(![self.processIncomingQueueAfterNextUnlockOperation isPending]) {
        __weak __typeof(self) weakSelf = self;

        CKKSResultOperation* restartIncomingQueueOperation = [CKKSResultOperation operationWithBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;
            // This IQO shouldn't error if the keybag has locked again. It will simply try again later.
            [strongSelf processIncomingQueue:false];
        }];

        restartIncomingQueueOperation.name = @"reprocess-incoming-queue-after-unlock";
        self.processIncomingQueueAfterNextUnlockOperation = restartIncomingQueueOperation;

        [restartIncomingQueueOperation addNullableDependency:self.lockStateTracker.unlockDependency];
        [self scheduleOperation: restartIncomingQueueOperation];
    }
}

- (CKKSIncomingQueueOperation*)processIncomingQueue:(bool)failOnClassA {
    return [self processIncomingQueue:failOnClassA after: nil];
}

- (CKKSIncomingQueueOperation*) processIncomingQueue:(bool)failOnClassA after: (CKKSResultOperation*) after {
    CKKSIncomingQueueOperation* incomingop = (CKKSIncomingQueueOperation*) [self findFirstPendingOperation:self.incomingQueueOperations];
    if(incomingop) {
        ckksinfo("ckks", self, "Skipping processIncomingQueue due to at least one pending instance");
        if(after) {
            [incomingop addNullableDependency: after];
        }
        // check (again) for race condition; if the op has started we need to add another (for the dependency)
        if([incomingop isPending]) {
            incomingop.errorOnClassAFailure |= failOnClassA;
            return incomingop;
        }
    }

    CKKSIncomingQueueOperation* op  = [[CKKSIncomingQueueOperation alloc] initWithCKKSKeychainView:self errorOnClassAFailure:failOnClassA];
    op.name = @"incoming-queue-operation";
    if(after != nil) {
        [op addSuccessDependency: after];
    }

    [self scheduleOperation: op];
    return op;
}

- (CKKSScanLocalItemsOperation*)scanLocalItems:(NSString*)operationName {
    return [self scanLocalItems:operationName ckoperationGroup:nil after:nil];
}

- (CKKSScanLocalItemsOperation*)scanLocalItems:(NSString*)operationName ckoperationGroup:(CKOperationGroup*)operationGroup after:(NSOperation*)after {
    CKKSScanLocalItemsOperation* scanOperation = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:operationGroup];
    scanOperation.name = operationName;

    [scanOperation addNullableDependency:self.lastFixupOperation];
    [scanOperation addNullableDependency:self.lockStateTracker.unlockDependency];
    [scanOperation addNullableDependency:self.keyStateReadyDependency];
    [scanOperation addNullableDependency:after];

    [self scheduleOperation: scanOperation];
    return scanOperation;
}

- (CKKSUpdateDeviceStateOperation*)updateDeviceState:(bool)rateLimit
                   waitForKeyHierarchyInitialization:(uint64_t)timeout
                                    ckoperationGroup:(CKOperationGroup*)ckoperationGroup {

    __weak __typeof(self) weakSelf = self;

    // If securityd just started, the key state might be in some transient early state. Wait a bit.
    CKKSResultOperation* waitForKeyReady = [CKKSResultOperation named:@"device-state-wait" withBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;
        ckksnotice("ckksdevice", strongSelf, "Finished waiting for key hierarchy transient state, currently %@", strongSelf.keyHierarchyState);
    }];

    [waitForKeyReady addNullableDependency:self.keyStateNonTransientDependency];
    [waitForKeyReady timeout:timeout];
    [self.waitingQueue addOperation:waitForKeyReady];

    CKKSUpdateDeviceStateOperation* op = [[CKKSUpdateDeviceStateOperation alloc] initWithCKKSKeychainView:self rateLimit:rateLimit ckoperationGroup:ckoperationGroup];
    op.name = @"device-state-operation";

    [op addDependency: waitForKeyReady];

    // op modifies the CloudKit zone, so it should insert itself into the list of OutgoingQueueOperations.
    // Then, we won't have simultaneous zone-modifying operations and confuse ourselves.
    // However, since we might have pending OQOs, it should try to insert itself at the beginning of the linearized list
    [op linearDependenciesWithSelfFirst:self.outgoingQueueOperations];

    // CKKSUpdateDeviceStateOperations are special: they should fire even if we don't believe we're in an iCloud account.
    // They also shouldn't block or be blocked by any other operation; our wait operation above will handle that
    [self scheduleOperationWithoutDependencies:op];
    return op;
}

// There are some errors which won't be reported but will be reflected in the CDSE; any error coming out of here is fatal
- (CKKSDeviceStateEntry*)_onqueueCurrentDeviceStateEntry: (NSError* __autoreleasing*)error {
    NSError* localerror = nil;

    CKKSCKAccountStateTracker* accountTracker = self.accountTracker;

    // We must have an iCloud account (with d2de on) to even create one of these
    if(accountTracker.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable || accountTracker.currentCKAccountInfo.supportsDeviceToDeviceEncryption != YES) {
        ckkserror("ckksdevice", self, "No iCloud account active: %@", accountTracker.currentCKAccountInfo);
        localerror = [NSError errorWithDomain:@"securityd"
                                         code:errSecInternalError
                                     userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No active HSA2 iCloud account: %@", accountTracker.currentCKAccountInfo]}];
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSDeviceStateEntry* oldcdse = [CKKSDeviceStateEntry tryFromDatabase:accountTracker.ckdeviceID zoneID:self.zoneID error:&localerror];
    if(localerror) {
        ckkserror("ckksdevice", self, "Couldn't read old CKKSDeviceStateEntry from database: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    // Find out what we think the current keys are
    CKKSCurrentKeyPointer* currentTLKPointer    = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassTLK zoneID:self.zoneID error:&localerror];
    CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassA   zoneID:self.zoneID error:&localerror];
    CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer tryFromDatabase: SecCKKSKeyClassC   zoneID:self.zoneID error:&localerror];
    if(localerror) {
        // Things is broken, but the whole point of this record is to share the brokenness. Continue.
        ckkserror("ckksdevice", self, "Couldn't read current key pointers from database: %@; proceeding", localerror);
        localerror = nil;
    }

    CKKSKey* suggestedTLK       = currentTLKPointer.currentKeyUUID    ? [CKKSKey tryFromDatabase:currentTLKPointer.currentKeyUUID    zoneID:self.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassAKey = currentClassAPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:currentClassAPointer.currentKeyUUID zoneID:self.zoneID error:&localerror] : nil;
    CKKSKey* suggestedClassCKey = currentClassCPointer.currentKeyUUID ? [CKKSKey tryFromDatabase:currentClassCPointer.currentKeyUUID zoneID:self.zoneID error:&localerror] : nil;

    if(localerror) {
        // Things is broken, but the whole point of this record is to share the brokenness. Continue.
        ckkserror("ckksdevice", self, "Couldn't read keys from database: %@; proceeding", localerror);
        localerror = nil;
    }

    // Check if we posess the keys in the keychain
    [suggestedTLK ensureKeyLoaded:&localerror];
    if(localerror && [self.lockStateTracker isLockedError:localerror]) {
        ckkserror("ckksdevice", self, "Device is locked; couldn't read TLK from keychain. Assuming it is present and continuing; error was %@", localerror);
        localerror = nil;
    } else if(localerror) {
        ckkserror("ckksdevice", self, "Couldn't read TLK from keychain. We do not have a current TLK. Error was %@", localerror);
        suggestedTLK = nil;
    }

    [suggestedClassAKey ensureKeyLoaded:&localerror];
    if(localerror && [self.lockStateTracker isLockedError:localerror]) {
        ckkserror("ckksdevice", self, "Device is locked; couldn't read ClassA key from keychain. Assuming it is present and continuing; error was %@", localerror);
        localerror = nil;
    } else if(localerror) {
        ckkserror("ckksdevice", self, "Couldn't read ClassA key from keychain. We do not have a current ClassA key. Error was %@", localerror);
        suggestedClassAKey = nil;
    }

    [suggestedClassCKey ensureKeyLoaded:&localerror];
    // class C keys are stored class C, so uh, don't check lock state.
    if(localerror) {
        ckkserror("ckksdevice", self, "Couldn't read ClassC key from keychain. We do not have a current ClassC key. Error was %@", localerror);
        suggestedClassCKey = nil;
    }

    // We'd like to have the circle peer ID. Give the account state tracker a fighting chance, but not having it is not an error
    if([accountTracker.accountCirclePeerIDInitialized wait:500*NSEC_PER_MSEC] != 0 && !accountTracker.accountCirclePeerID) {
        ckkserror("ckksdevice", self, "No peer ID available");
    }

    // Reset the last unlock time to 'day' granularity in UTC
    NSCalendar* calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierISO8601];
    calendar.timeZone = [NSTimeZone timeZoneWithAbbreviation:@"UTC"];
    NSDate* lastUnlockDay = self.lockStateTracker.lastUnlockTime;
    lastUnlockDay = lastUnlockDay ? [calendar startOfDayForDate:lastUnlockDay] : nil;

    // We only really want the oldcdse for its encodedCKRecord, so make a new cdse here
    CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initForDevice:accountTracker.ckdeviceID
                                                                      osVersion:SecCKKSHostOSVersion()
                                                                 lastUnlockTime:lastUnlockDay
                                                                   circlePeerID:accountTracker.accountCirclePeerID
                                                                   circleStatus:accountTracker.currentCircleStatus
                                                                       keyState:self.keyHierarchyState
                                                                 currentTLKUUID:suggestedTLK.uuid
                                                              currentClassAUUID:suggestedClassAKey.uuid
                                                              currentClassCUUID:suggestedClassCKey.uuid
                                                                         zoneID:self.zoneID
                                                                encodedCKRecord:oldcdse.encodedCKRecord];
    return newcdse;
}

- (CKKSSynchronizeOperation*) resyncWithCloud {
    CKKSSynchronizeOperation* op = [[CKKSSynchronizeOperation alloc] initWithCKKSKeychainView: self];
    [self scheduleOperation: op];
    return op;
}

- (CKKSLocalSynchronizeOperation*)resyncLocal {
    CKKSLocalSynchronizeOperation* op = [[CKKSLocalSynchronizeOperation alloc] initWithCKKSKeychainView:self];
    [self scheduleOperation: op];
    return op;
}

- (CKKSResultOperation*)fetchAndProcessCKChanges:(CKKSFetchBecause*)because {
    return [self fetchAndProcessCKChanges:because after:nil];
}

- (CKKSResultOperation*)fetchAndProcessCKChanges:(CKKSFetchBecause*)because after:(CKKSResultOperation*)after {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping fetchAndProcessCKChanges due to disabled CKKS");
        return nil;
    }

    if(after) {
        [self.zoneChangeFetcher holdFetchesUntil:after];
    }

    // We fetched some changes; try to process them!
    return [self processIncomingQueue:false after:[self.zoneChangeFetcher requestSuccessfulFetch:because]];
}

// Lets the view know about a failed CloudKit write. If the error is "already have one of these records", it will
// store the new records and kick off the new processing
//
// Note that you need to tell this function the records you wanted to save, so it can determine what needs deletion
- (bool)_onqueueCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords {
    dispatch_assert_queue(self.queue);

    NSDictionary<CKRecordID*,NSError*>* partialErrors = ckerror.userInfo[CKPartialErrorsByItemIDKey];
    if([ckerror.domain isEqual:CKErrorDomain] && ckerror.code == CKErrorPartialFailure && partialErrors) {
        // Check if this error was "you're out of date"
        bool recordChanged = true;

        for(NSError* error in partialErrors.allValues) {
            if((![error.domain isEqual:CKErrorDomain]) || (error.code != CKErrorBatchRequestFailed && error.code != CKErrorServerRecordChanged && error.code != CKErrorUnknownItem)) {
                // There's an error in there that isn't CKErrorServerRecordChanged, CKErrorBatchRequestFailed, or CKErrorUnknownItem. Don't handle nicely...
                recordChanged = false;
            }
        }

        if(recordChanged) {
            ckksnotice("ckks", self, "Received a ServerRecordChanged error, attempting to update new records and delete unknown ones");

            bool updatedRecord = false;

            for(CKRecordID* recordID in partialErrors.allKeys) {
                NSError* error = partialErrors[recordID];
                if([error.domain isEqual:CKErrorDomain] && error.code == CKErrorServerRecordChanged) {
                    CKRecord* newRecord = error.userInfo[CKRecordChangedErrorServerRecordKey];
                    ckksnotice("ckks", self, "On error: updating our idea of: %@", newRecord);

                    updatedRecord |= [self _onqueueCKRecordChanged:newRecord resync:true];
                } else if([error.domain isEqual:CKErrorDomain] && error.code == CKErrorUnknownItem) {
                    CKRecord* record = savedRecords[recordID];
                    ckksnotice("ckks", self, "On error: handling an unexpected delete of: %@ %@", recordID, record);

                    updatedRecord |= [self _onqueueCKRecordDeleted:recordID recordType:record.recordType resync:true];
                }
            }

            if(updatedRecord) {
                [self processIncomingQueue:false];
                return true;
            }
        }

        // Check if this error was the CKKS server extension rejecting the write
        for(CKRecordID* recordID in partialErrors.allKeys) {
            NSError* error = partialErrors[recordID];

            NSError* underlyingError = error.userInfo[NSUnderlyingErrorKey];
            NSError* thirdLevelError = underlyingError.userInfo[NSUnderlyingErrorKey];
            ckksnotice("ckks", self, "Examining 'write failed' error: %@ %@ %@", error, underlyingError, thirdLevelError);

            if([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorServerRejectedRequest &&
               underlyingError && [underlyingError.domain isEqualToString:CKInternalErrorDomain] && underlyingError.code == CKErrorInternalPluginError &&
               thirdLevelError && [thirdLevelError.domain isEqualToString:@"CloudkitKeychainService"]) {

                if(thirdLevelError.code == CKKSServerUnexpectedSyncKeyInChain) {
                    // The server thinks the classA/C synckeys don't wrap directly the to top TLK, but we don't (otherwise, we would have fixed it).
                    // Issue a key hierarchy fetch and see what's what.
                    ckkserror("ckks", self, "CKKS Server extension has told us about %@ for record %@; requesting refetch and reprocess of key hierarchy", thirdLevelError, recordID);
                    [self _onqueueKeyStateMachineRequestFetch];
                } else {
                    ckkserror("ckks", self, "CKKS Server extension has told us about %@ for record %@, but we don't currently handle this error", thirdLevelError, recordID);
                }
            }
        }
    }

    return false;
}

- (bool)_onqueueCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    // TODO: resync doesn't really mean much here; what does it mean for a record to be 'deleted' if you're fetching from scratch?

    if([recordType isEqual: SecCKRecordItemType]) {
        ckksinfo("ckks", self, "CloudKit notification: deleted record(%@): %@", recordType, recordID);
        NSError* error = nil;
        NSError* iqeerror = nil;
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase: [recordID recordName] zoneID:self.zoneID error: &error];

        // Deletes always succeed, not matter the generation count
        if(ckme) {
            [ckme deleteFromDatabase:&error];

            CKKSIncomingQueueEntry* iqe = [[CKKSIncomingQueueEntry alloc] initWithCKKSItem:ckme.item action:SecCKKSActionDelete state:SecCKKSStateNew];
            [iqe saveToDatabase:&iqeerror];
            if(iqeerror) {
                ckkserror("ckks", self, "Couldn't save incoming queue entry: %@", iqeerror);
            }
        }
        ckksinfo("ckks", self, "CKKSMirrorEntry was deleted: %@ %@ error: %@", recordID, ckme, error);
        // TODO: actually pass error back up
        return (error == nil);

    } else if([recordType isEqual: SecCKRecordCurrentItemType]) {
        ckksinfo("ckks", self, "CloudKit notification: deleted current item pointer(%@): %@", recordType, recordID);
        NSError* error = nil;

        [[CKKSCurrentItemPointer tryFromDatabase:[recordID recordName] state:SecCKKSProcessedStateRemote zoneID:self.zoneID error:&error] deleteFromDatabase:&error];
        [[CKKSCurrentItemPointer fromDatabase:[recordID recordName]    state:SecCKKSProcessedStateLocal  zoneID:self.zoneID error:&error] deleteFromDatabase:&error];

        ckksinfo("ckks", self, "CKKSCurrentItemPointer was deleted: %@ error: %@", recordID, error);
        return (error == nil);

    } else if([recordType isEqual: SecCKRecordIntermediateKeyType]) {
        // TODO: handle in some interesting way
        return true;
    } else if([recordType isEqual: SecCKRecordTLKShareType]) {
        NSError* error = nil;
        ckksinfo("ckks", self, "CloudKit notification: deleted tlk share record(%@): %@", recordType, recordID);
        CKKSTLKShare* share = [CKKSTLKShare tryFromDatabaseFromCKRecordID:recordID error:&error];
        [share deleteFromDatabase:&error];

        if(error) {
            ckkserror("ckks", self, "CK notification: Couldn't delete deleted TLKShare: %@ %@", recordID,  error);
        }
        return (error == nil);

    } else if([recordType isEqual: SecCKRecordDeviceStateType]) {
        NSError* error = nil;
        ckksinfo("ckks", self, "CloudKit notification: deleted device state record(%@): %@", recordType, recordID);

        CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry tryFromDatabaseFromCKRecordID:recordID error:&error];
        [cdse deleteFromDatabase: &error];
        ckksinfo("ckks", self, "CKKSCurrentItemPointer(%@) was deleted: %@ error: %@", cdse, recordID, error);

        return (error == nil);

    } else if ([recordType isEqualToString:SecCKRecordManifestType]) {
        ckksinfo("ckks", self, "CloudKit notification: deleted manifest record (%@): %@", recordType, recordID);
        
        NSError* error = nil;
        CKKSManifest* manifest = [CKKSManifest manifestForRecordName:recordID.recordName error:&error];
        if (manifest) {
            [manifest deleteFromDatabase:&error];
        }
        
        ckksinfo("ckks", self, "CKKSManifest was deleted: %@ %@ error: %@", recordID, manifest, error);
        // TODO: actually pass error back up
        return error == nil;
    }

    else {
        ckkserror("ckksfetch", self, "unknown record type: %@ %@", recordType, recordID);
        return false;
    }
}

- (bool)_onqueueCKRecordChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    ckksinfo("ckksfetch", self, "Processing record modification(%@): %@", record.recordType, record);

    if([[record recordType] isEqual: SecCKRecordItemType]) {
        [self _onqueueCKRecordItemChanged:record resync:resync];
        return true;
    } else if([[record recordType] isEqual: SecCKRecordCurrentItemType]) {
        [self _onqueueCKRecordCurrentItemPointerChanged:record resync:resync];
        return true;
    } else if([[record recordType] isEqual: SecCKRecordIntermediateKeyType]) {
        [self _onqueueCKRecordKeyChanged:record resync:resync];
        return true;
    } else if ([[record recordType] isEqual: SecCKRecordTLKShareType]) {
        [self _onqueueCKRecordTLKShareChanged:record resync:resync];
        return true;
    } else if([[record recordType] isEqualToString: SecCKRecordCurrentKeyType]) {
        [self _onqueueCKRecordCurrentKeyPointerChanged:record resync:resync];
        return true;
    } else if ([[record recordType] isEqualToString:SecCKRecordManifestType]) {
        [self _onqueueCKRecordManifestChanged:record resync:resync];
        return true;
    } else if ([[record recordType] isEqualToString:SecCKRecordManifestLeafType]) {
        [self _onqueueCKRecordManifestLeafChanged:record resync:resync];
        return true;
    } else if ([[record recordType] isEqualToString:SecCKRecordDeviceStateType]) {
        [self _onqueueCKRecordDeviceStateChanged:record resync:resync];
        return true;
    } else {
        ckkserror("ckksfetch", self, "unknown record type: %@ %@", [record recordType], record);
        return false;
    }
}

- (void)_onqueueCKRecordItemChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    NSError* error = nil;
    // Find if we knew about this record in the past
    bool update = false;
    CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase: [[record recordID] recordName] zoneID:self.zoneID error:&error];

    if(error) {
        ckkserror("ckks", self, "error loading a CKKSMirrorEntry from database: %@", error);
        // TODO: quit?
    }

    if(resync) {
        if(!ckme) {
            ckkserror("ckksresync", self, "BUG: No local item matching resynced CloudKit record: %@", record);
        } else if(![ckme matchesCKRecord:record]) {
            ckkserror("ckksresync", self, "BUG: Local item doesn't match resynced CloudKit record: %@ %@", ckme, record);
        } else {
            ckksnotice("ckksresync", self, "Already know about this item record, updating anyway: %@", record.recordID);
        }
    }

    if(ckme && ckme.item && ckme.item.generationCount > [record[SecCKRecordGenerationCountKey] unsignedLongLongValue]) {
        ckkserror("ckks", self, "received a record from CloudKit with a bad generation count: %@ (%ld > %@)", ckme.uuid,
                 (long) ckme.item.generationCount,
                 record[SecCKRecordGenerationCountKey]);
        // Abort processing this record.
        return;
    }

    // If we found an old version in the database; this might be an update
    if(ckme) {
        if([ckme matchesCKRecord:record] && !resync) {
            // This is almost certainly a record we uploaded; CKFetchChanges sends them back as new records
            ckksnotice("ckks", self, "CloudKit has told us of record we already know about; skipping update");
            return;
        }

        update = true;
        // Set the CKKSMirrorEntry's fields to be whatever this record holds
        [ckme setFromCKRecord: record];
    } else {
        // Have to make a new CKKSMirrorEntry
        ckme = [[CKKSMirrorEntry alloc] initWithCKRecord: record];
    }

    [ckme saveToDatabase: &error];

    if(error) {
        ckkserror("ckks", self, "couldn't save new CKRecord to database: %@ %@", record, error);
    } else {
        ckksdebug("ckks", self, "CKKSMirrorEntry was created: %@", ckme);
    }

    NSError* iqeerror = nil;
    CKKSIncomingQueueEntry* iqe = [[CKKSIncomingQueueEntry alloc] initWithCKKSItem:ckme.item
                                                                            action:(update ? SecCKKSActionModify : SecCKKSActionAdd)
                                                                             state:SecCKKSStateNew];
    [iqe saveToDatabase:&iqeerror];
    if(iqeerror) {
        ckkserror("ckks", self, "Couldn't save modified incoming queue entry: %@", iqeerror);
    } else {
        ckksdebug("ckks", self, "CKKSIncomingQueueEntry was created: %@", iqe);
    }

    // A remote change has occured for this record. Delete any pending local changes; they will be overwritten.
    CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:ckme.uuid state: SecCKKSStateNew zoneID:self.zoneID error: &error];
    if(error) {
        ckkserror("ckks", self, "Couldn't load OutgoingQueueEntry: %@", error);
    }
    if(oqe) {
        [self _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateDeleted error:&error];
    }

    // Reencryptions are pending changes too
    oqe = [CKKSOutgoingQueueEntry tryFromDatabase:ckme.uuid state: SecCKKSStateReencrypt zoneID:self.zoneID error: &error];
    if(error) {
        ckkserror("ckks", self, "Couldn't load reencrypted OutgoingQueueEntry: %@", error);
    }
    if(oqe) {
        [oqe deleteFromDatabase:&error];
        if(error) {
            ckkserror("ckks", self, "Couldn't delete reencrypted oqe(%@): %@", oqe, error);
        }
    }
}

- (void)_onqueueCKRecordKeyChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    NSError* error = nil;

    if(resync) {
        NSError* resyncerror = nil;

        CKKSKey* key = [CKKSKey tryFromDatabaseAnyState:record.recordID.recordName zoneID:self.zoneID error:&resyncerror];
        if(resyncerror) {
            ckkserror("ckksresync", self, "error loading key: %@", resyncerror);
        }
        if(!key) {
            ckkserror("ckksresync", self, "BUG: No sync key matching resynced CloudKit record: %@", record);
        } else if(![key matchesCKRecord:record]) {
            ckkserror("ckksresync", self, "BUG: Local sync key doesn't match resynced CloudKit record(s): %@ %@", key, record);
        } else {
            ckksnotice("ckksresync", self, "Already know about this sync key, skipping update: %@", record);
            return;
        }
    }

    CKKSKey* remotekey = [[CKKSKey alloc] initWithCKRecord: record];

    // Do we already know about this key?
    CKKSKey* possibleLocalKey = [CKKSKey tryFromDatabase:remotekey.uuid zoneID:self.zoneID error:&error];
    if(error) {
        ckkserror("ckkskey", self, "Error findibg exsiting local key for %@: %@", remotekey, error);
        // Go on, assuming there isn't a local key
    } else if(possibleLocalKey && [possibleLocalKey matchesCKRecord:record]) {
        // Okay, nothing new here. Update the CKRecord and move on.
        // Note: If the new record doesn't match the local copy, we have to go through the whole dance below
        possibleLocalKey.storedCKRecord = record;
        [possibleLocalKey saveToDatabase:&error];

        if(error) {
            ckkserror("ckkskey", self, "Couldn't update existing key: %@: %@", possibleLocalKey, error);
        }
        return;
    }

    // Drop into the synckeys table as a 'remote' key, then ask for a rekey operation.
    remotekey.state = SecCKKSProcessedStateRemote;
    remotekey.currentkey = false;

    [remotekey saveToDatabase:&error];
    if(error) {
        ckkserror("ckkskey", self, "Couldn't save key record to database: %@: %@", remotekey, error);
        ckksinfo("ckkskey", self, "CKRecord was %@", record);
    }

    // We've saved a new key in the database; trigger a rekey operation.
    [self _onqueueKeyStateMachineRequestProcess];
}

- (void)_onqueueCKRecordTLKShareChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    NSError* error = nil;
    if(resync) {
        // TODO fill in
    }

    // CKKSTLKShares get saved with no modification
    CKKSTLKShare* share = [[CKKSTLKShare alloc] initWithCKRecord:record];
    [share saveToDatabase:&error];
    if(error) {
        ckkserror("ckksshare", self, "Couldn't save new TLK share to database: %@ %@", share, error);
    }

    [self _onqueueKeyStateMachineRequestProcess];
}

- (void)_onqueueCKRecordCurrentKeyPointerChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    // Pull out the old CKP, if it exists
    NSError* ckperror = nil;
    CKKSCurrentKeyPointer* oldckp = [CKKSCurrentKeyPointer tryFromDatabase:((CKKSKeyClass*) record.recordID.recordName) zoneID:self.zoneID error:&ckperror];
    if(ckperror) {
        ckkserror("ckkskey", self, "error loading ckp: %@", ckperror);
    }

    if(resync) {
        if(!oldckp) {
            ckkserror("ckksresync", self, "BUG: No current key pointer matching resynced CloudKit record: %@", record);
        } else if(![oldckp matchesCKRecord:record]) {
            ckkserror("ckksresync", self, "BUG: Local current key pointer doesn't match resynced CloudKit record: %@ %@", oldckp, record);
        } else {
            ckksnotice("ckksresync", self, "Current key pointer has 'changed', but it matches our local copy: %@", record);
        }
    }

    NSError* error = nil;
    CKKSCurrentKeyPointer* currentkey = [[CKKSCurrentKeyPointer alloc] initWithCKRecord: record];

    [currentkey saveToDatabase: &error];
    if(error) {
        ckkserror("ckkskey", self, "Couldn't save current key pointer to database: %@: %@", currentkey, error);
        ckksinfo("ckkskey", self, "CKRecord was %@", record);
    }

    if([oldckp matchesCKRecord:record]) {
        ckksnotice("ckkskey", self, "Current key pointer modification doesn't change anything interesting; skipping reprocess: %@", record);
    } else {
        // We've saved a new key in the database; trigger a rekey operation.
        [self _onqueueKeyStateMachineRequestProcess];
    }
}

- (void)_onqueueCKRecordCurrentItemPointerChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    if(resync) {
        NSError* ciperror = nil;
        CKKSCurrentItemPointer* localcip  = [CKKSCurrentItemPointer tryFromDatabase:record.recordID.recordName state:SecCKKSProcessedStateLocal  zoneID:self.zoneID error:&ciperror];
        CKKSCurrentItemPointer* remotecip = [CKKSCurrentItemPointer tryFromDatabase:record.recordID.recordName state:SecCKKSProcessedStateRemote zoneID:self.zoneID error:&ciperror];
        if(ciperror) {
            ckkserror("ckksresync", self, "error loading cip: %@", ciperror);
        }
        if(!(localcip || remotecip)) {
            ckkserror("ckksresync", self, "BUG: No current item pointer matching resynced CloudKit record: %@", record);
        } else if(! ([localcip matchesCKRecord:record] || [remotecip matchesCKRecord:record]) ) {
            ckkserror("ckksresync", self, "BUG: Local current item pointer doesn't match resynced CloudKit record(s): %@ %@ %@", localcip, remotecip, record);
        } else {
            ckksnotice("ckksresync", self, "Already know about this current item pointer, skipping update: %@", record);
            return;
        }
    }

    NSError* error = nil;
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initWithCKRecord: record];
    cip.state = SecCKKSProcessedStateRemote;

    [cip saveToDatabase: &error];
    if(error) {
        ckkserror("currentitem", self, "Couldn't save current item pointer to database: %@: %@ %@", cip, error, record);
    }
}

- (void)_onqueueCKRecordManifestChanged:(CKRecord*)record resync:(bool)resync
{
    NSError* error = nil;
    CKKSPendingManifest* manifest = [[CKKSPendingManifest alloc] initWithCKRecord:record];
    [manifest saveToDatabase:&error];
    if (error) {
        ckkserror("CKKS", self, "Failed to save fetched manifest record to database: %@: %@", manifest, error);
        ckksinfo("CKKS", self, "manifest CKRecord was %@", record);
    }
}

- (void)_onqueueCKRecordManifestLeafChanged:(CKRecord*)record resync:(bool)resync
{
    NSError* error = nil;
    CKKSManifestLeafRecord* manifestLeaf = [[CKKSManifestPendingLeafRecord alloc] initWithCKRecord:record];
    [manifestLeaf saveToDatabase:&error];
    if (error) {
        ckkserror("CKKS", self, "Failed to save fetched manifest leaf record to database: %@: %@", manifestLeaf, error);
        ckksinfo("CKKS", self, "manifest leaf CKRecord was %@", record);
    }
}

- (void)_onqueueCKRecordDeviceStateChanged:(CKRecord*)record resync:(bool)resync {
    if(resync) {
        NSError* dserror = nil;
        CKKSDeviceStateEntry* cdse  = [CKKSDeviceStateEntry tryFromDatabase:record.recordID.recordName zoneID:self.zoneID error:&dserror];
        if(dserror) {
            ckkserror("ckksresync", self, "error loading cdse: %@", dserror);
        }
        if(!cdse) {
            ckkserror("ckksresync", self, "BUG: No current device state entry matching resynced CloudKit record: %@", record);
        } else if(![cdse matchesCKRecord:record]) {
            ckkserror("ckksresync", self, "BUG: Local current device state entry doesn't match resynced CloudKit record(s): %@ %@", cdse, record);
        } else {
            ckksnotice("ckksresync", self, "Already know about this current item pointer, skipping update: %@", record);
            return;
        }
    }

    NSError* error = nil;
    CKKSDeviceStateEntry* cdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record];
    [cdse saveToDatabase:&error];
    if (error) {
        ckkserror("ckksdevice", self, "Failed to save device record to database: %@: %@ %@", cdse, error, record);
    }
}

- (bool)_onqueueResetAllInflightOQE:(NSError**)error {
    NSError* localError = nil;

    while(true) {
        NSArray<CKKSOutgoingQueueEntry*> * inflightQueueEntries = [CKKSOutgoingQueueEntry fetch:SecCKKSOutgoingQueueItemsAtOnce
                                                                                          state:SecCKKSStateInFlight
                                                                                         zoneID:self.zoneID
                                                                                          error:&localError];

        if(localError != nil) {
            ckkserror("ckks", self, "Error finding inflight outgoing queue records: %@", localError);
            if(error) {
                *error = localError;
            }
            return false;
        }

        if([inflightQueueEntries count] == 0u) {
            break;
        }

        for(CKKSOutgoingQueueEntry* oqe in inflightQueueEntries) {
            [self _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateNew error:&localError];

            if(localError) {
                ckkserror("ckks", self, "Error fixing up inflight OQE(%@): %@", oqe, localError);
                if(error) {
                    *error = localError;
                }
                return false;
            }
        }
    }

    return true;
}

- (bool)_onqueueChangeOutgoingQueueEntry: (CKKSOutgoingQueueEntry*) oqe toState: (NSString*) state error: (NSError* __autoreleasing*) error {
    dispatch_assert_queue(self.queue);

    NSError* localerror = nil;

    if([state isEqualToString: SecCKKSStateDeleted]) {
        // Hurray, this must be a success
        SecBoolNSErrorCallback callback = self.pendingSyncCallbacks[oqe.uuid];
        if(callback) {
            callback(true, nil);
            self.pendingSyncCallbacks[oqe.uuid] = nil;
        }

        [oqe deleteFromDatabase: &localerror];
        if(localerror) {
            ckkserror("ckks", self, "Couldn't delete %@: %@", oqe, localerror);
        }

    } else if([oqe.state isEqualToString:SecCKKSStateInFlight] && [state isEqualToString:SecCKKSStateNew]) {
        // An in-flight OQE is moving to new? See if it's been superceded
        CKKSOutgoingQueueEntry* newOQE = [CKKSOutgoingQueueEntry tryFromDatabase:oqe.uuid state:SecCKKSStateNew zoneID:self.zoneID error:&localerror];
        if(localerror) {
            ckkserror("ckksoutgoing", self, "Couldn't fetch an overwriting OQE, assuming one doesn't exist: %@", localerror);
            newOQE = nil;
        }

        if(newOQE) {
            ckksnotice("ckksoutgoing", self, "New modification has come in behind inflight %@; dropping failed change", oqe);
            // recurse for that lovely code reuse
            [self _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateDeleted error:&localerror];
            if(localerror) {
                ckkserror("ckksoutgoing", self, "Couldn't delete in-flight OQE: %@", localerror);
                if(error) {
                    *error = localerror;
                }
            }
        } else {
            oqe.state = state;
            [oqe saveToDatabase: &localerror];
            if(localerror) {
                ckkserror("ckks", self, "Couldn't save %@ as %@: %@", oqe, state, localerror);
            }
        }

    } else {
        oqe.state = state;
        [oqe saveToDatabase: &localerror];
        if(localerror) {
            ckkserror("ckks", self, "Couldn't save %@ as %@: %@", oqe, state, localerror);
        }
    }

    if(error && localerror) {
        *error = localerror;
    }
    return localerror == nil;
}

- (bool)_onqueueErrorOutgoingQueueEntry: (CKKSOutgoingQueueEntry*) oqe itemError: (NSError*) itemError error: (NSError* __autoreleasing*) error {
    dispatch_assert_queue(self.queue);

    SecBoolNSErrorCallback callback = self.pendingSyncCallbacks[oqe.uuid];
    if(callback) {
        callback(false, itemError);
        self.pendingSyncCallbacks[oqe.uuid] = nil;
    }
    NSError* localerror = nil;

    // Now, delete the OQE: it's never coming back
    [oqe deleteFromDatabase:&localerror];
    if(localerror) {
        ckkserror("ckks", self, "Couldn't delete %@ (due to error %@): %@", oqe, itemError, localerror);
    }

    if(error && localerror) {
        *error = localerror;
    }
    return localerror == nil;
}

- (bool)_onqueueUpdateLatestManifestWithError:(NSError**)error
{
    dispatch_assert_queue(self.queue);
    CKKSManifest* manifest = [CKKSManifest latestTrustedManifestForZone:self.zoneName error:error];
    if (manifest) {
        self.latestManifest = manifest;
        return true;
    }
    else {
        return false;
    }
}

- (bool)_onqueueWithAccountKeysCheckTLK:(CKKSKey*)proposedTLK error:(NSError* __autoreleasing *)error {
    dispatch_assert_queue(self.queue);
    // First, if we have a local identity, check for any TLK shares
    NSError* localerror = nil;

    if(![proposedTLK wrapsSelf]) {
        ckkserror("ckksshare", self, "Potential TLK %@ does not wrap self; skipping TLK share checking", proposedTLK);
    } else {
        bool tlkShares = [self _onqueueWithAccountKeysCheckTLKFromShares:proposedTLK error:&localerror];
        // We only want to error out if a positive error occurred. "No shares" is okay.
        if(!tlkShares || localerror) {
            bool noTrustedTLKShares = [localerror.domain isEqualToString:CKKSErrorDomain] && localerror.code == CKKSNoTrustedTLKShares;
            bool noSelfPeer = [localerror.domain isEqualToString:CKKSErrorDomain] && localerror.code == CKKSNoEncryptionKey;

            // If this error was something worse than 'couldn't unwrap for reasons including there not being data', report it
            if(!(noTrustedTLKShares || noSelfPeer)) {
                if(error) {
                    *error = localerror;
                }
                ckkserror("ckksshare", self, "Errored unwrapping TLK with TLKShares: %@", localerror);
                return false;
            } else {
                ckkserror("ckksshare", self, "Non-fatal error unwrapping TLK with TLKShares: %@", localerror);
            }
        }
    }

    if([proposedTLK loadKeyMaterialFromKeychain:error]) {
        // Hurray!
        return true;
    } else {
        return false;
    }
}

// This version only examines if this TLK is recoverable from TLK shares
- (bool)_onqueueWithAccountKeysCheckTLKFromShares:(CKKSKey*)proposedTLK error:(NSError* __autoreleasing *)error {
    NSError* localerror = NULL;
    if(!self.currentSelfPeers.currentSelf || self.currentSelfPeersError) {
        ckkserror("ckksshare", self, "Couldn't fetch self peers: %@", self.currentSelfPeersError);
        if(error) {
            if([self.lockStateTracker isLockedError:self.currentSelfPeersError]) {
                // Locked error should propagate
                *error = self.currentSelfPeersError;
            } else {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNoEncryptionKey
                                      description:@"No current self peer"
                                       underlying:self.currentSelfPeersError];
            }
        }
        return false;
    }

    if(!self.currentTrustedPeers || self.currentTrustedPeersError) {
        ckkserror("ckksshare", self, "Couldn't fetch trusted peers: %@", self.currentTrustedPeersError);
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoPeersAvailable
                                  description:@"No trusted peers"
                                   underlying:self.currentTrustedPeersError];
        }
        return false;
    }

    NSError* lastShareError = nil;

    for(id<CKKSSelfPeer> selfPeer in self.currentSelfPeers.allSelves) {
        NSArray<CKKSTLKShare*>* possibleShares = [CKKSTLKShare allFor:selfPeer.peerID
                                                              keyUUID:proposedTLK.uuid
                                                               zoneID:self.zoneID
                                                                error:&localerror];
        if(localerror) {
            ckkserror("ckksshare", self, "Error fetching CKKSTLKShares for %@: %@", selfPeer, localerror);
        }

        if(possibleShares.count == 0) {
            ckksnotice("ckksshare", self, "No CKKSTLKShares to %@ for %@", selfPeer, proposedTLK);
            continue;
        }

        for(CKKSTLKShare* possibleShare in possibleShares) {
            NSError* possibleShareError = nil;
            ckksnotice("ckksshare", self, "Checking possible TLK share %@ as %@", possibleShare, selfPeer);

            CKKSKey* possibleKey = [possibleShare recoverTLK:selfPeer
                                                trustedPeers:self.currentTrustedPeers
                                                       error:&possibleShareError];

            if(possibleShareError) {
                ckkserror("ckksshare", self, "Unable to unwrap TLKShare(%@) as %@: %@",
                          possibleShare, selfPeer, possibleShareError);
                ckkserror("ckksshare", self, "Current trust set: %@", self.currentTrustedPeers);
                lastShareError = possibleShareError;
                continue;
            }

            bool result = [proposedTLK trySelfWrappedKeyCandidate:possibleKey.aessivkey error:&possibleShareError];
            if(possibleShareError) {
                ckkserror("ckksshare", self, "Unwrapped TLKShare(%@) does not unwrap proposed TLK(%@) as %@: %@",
                          possibleShare, proposedTLK, self.currentSelfPeers.currentSelf, possibleShareError);
                lastShareError = possibleShareError;
                continue;
            }

            if(result) {
                ckksnotice("ckksshare", self, "TLKShare(%@) unlocked TLK(%@) as %@",
                           possibleShare, proposedTLK, selfPeer);

                // The proposed TLK is trusted key material. Persist it as a "trusted" key.
                [proposedTLK saveKeyMaterialToKeychain:true error:&possibleShareError];
                if(possibleShareError) {
                    ckkserror("ckksshare", self, "Couldn't store the new TLK(%@) to the keychain: %@", proposedTLK, possibleShareError);
                    if(error) {
                        *error = possibleShareError;
                    }
                    return false;
                }

                return true;
            }
        }
    }

    if(error) {
        *error = [NSError errorWithDomain:CKKSErrorDomain
                                     code:CKKSNoTrustedTLKShares
                              description:[NSString stringWithFormat:@"No trusted TLKShares for %@", proposedTLK]
                               underlying:lastShareError];
    }
    return false;
}

- (bool)dispatchSyncWithConnection:(SecDbConnectionRef _Nonnull)dbconn block:(bool (^)(void))block {
    CFErrorRef cferror = NULL;

    // Take the DB transaction, then get on the local queue.
    // In the case of exclusive DB transactions, we don't really _need_ the local queue, but, it's here for future use.
    bool ret = kc_transaction_type(dbconn, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, ^bool{
        __block bool ok = false;

        dispatch_sync(self.queue, ^{
            ok = block();
        });

        return ok;
    });

    if(cferror) {
        ckkserror("ckks", self, "error doing database transaction, major problems ahead: %@", cferror);
    }
    return ret;
}

- (void)dispatchSync: (bool (^)(void)) block {
    // important enough to block this thread. Must get a connection first, though!

    // Please don't jetsam us...
    os_transaction_t transaction = os_transaction_create([[NSString stringWithFormat:@"com.apple.securityd.ckks.%@", self.zoneName] UTF8String]);

    CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        return [self dispatchSyncWithConnection:dbt block:block];
    });
    if(cferror) {
        ckkserror("ckks", self, "error getting database connection, major problems ahead: %@", cferror);
    }

    (void)transaction;
}

- (void)dispatchSyncWithAccountKeys:(bool (^)(void))block
{
    [SOSAccount performOnQuietAccountQueue: ^{
        NSError* selfPeersError = nil;
        CKKSSelves* currentSelfPeers = [self.currentPeerProvider fetchSelfPeers:&selfPeersError];

        NSError* trustedPeersError = nil;
        NSSet<id<CKKSPeer>>* currentTrustedPeers = [self.currentPeerProvider fetchTrustedPeers:&trustedPeersError];

        [self dispatchSync:^bool{
            self.currentSelfPeers = currentSelfPeers;
            self.currentSelfPeersError = selfPeersError;

            self.currentTrustedPeers = currentTrustedPeers;
            self.currentTrustedPeersError = trustedPeersError;

            __block bool result = false;
            [SOSAccount performWhileHoldingAccountQueue:^{ // so any calls through SOS account will know they can perform their work without dispatching to the account queue, which we already hold
                result = block();
            }];

            // Forget the peers; they might have class A key material
            self.currentSelfPeers = nil;
            self.currentSelfPeersError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoPeersAvailable description:@"No current self peer available"];
            self.currentTrustedPeers = nil;
            self.currentTrustedPeersError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoPeersAvailable description:@"No current trusted peers available"];

            return result;
        }];
    }];
}

#pragma mark - CKKSZoneUpdateReceiver

- (void)notifyZoneChange: (CKRecordZoneNotification*) notification {
    ckksnotice("ckks", self, "received a zone change notification for %@ %@", self, notification);

    [self fetchAndProcessCKChanges:CKKSFetchBecauseAPNS];
}

- (void)superHandleCKLogin {
    [super handleCKLogin];
}

- (void)handleCKLogin {
    ckksnotice("ckks", self, "received a notification of CK login");
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckks", self, "Skipping CloudKit initialization due to disabled CKKS");
        return;
    }

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* login = [CKKSResultOperation named:@"ckks-login" withBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;

        [strongSelf dispatchSyncWithAccountKeys:^bool{
            [strongSelf superHandleCKLogin];

            // Reset key hierarchy state machine to initializing
            [strongSelf _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateInitializing withError:nil];
            return true;
        }];

        // Change our condition variables to reflect that we think we're logged in
        strongSelf.loggedOut = [[CKKSCondition alloc] initToChain:strongSelf.loggedOut];
        [strongSelf.loggedIn fulfill];
        [strongSelf.accountStateKnown fulfill];
    }];

    [self scheduleAccountStatusOperation:login];
}

- (void)superHandleCKLogout {
    [super handleCKLogout];
}

- (void)handleCKLogout {
    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* logout = [CKKSResultOperation named:@"ckks-logout" withBlock: ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            return;
        }
        [strongSelf dispatchSync:^bool {
            ckksnotice("ckks", strongSelf, "received a notification of CK logout");
            [strongSelf superHandleCKLogout];

            NSError* error = nil;
            [strongSelf _onqueueResetLocalData: &error];
            if(error) {
                ckkserror("ckks", strongSelf, "error while resetting local data: %@", error);
            }

            [self _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateLoggedOut withError:nil];

            strongSelf.loggedIn = [[CKKSCondition alloc] initToChain: strongSelf.loggedIn];
            [strongSelf.loggedOut fulfill];
            [strongSelf.accountStateKnown fulfill];

            // Tell all pending sync clients that we don't expect to ever sync
            for(NSString* callbackUUID in strongSelf.pendingSyncCallbacks.allKeys) {
                [strongSelf callSyncCallbackWithErrorNoAccount:strongSelf.pendingSyncCallbacks[callbackUUID]];
                strongSelf.pendingSyncCallbacks[callbackUUID] = nil;
            }

            return true;
        }];
    }];

    [self scheduleAccountStatusOperation: logout];
}

- (void)callSyncCallbackWithErrorNoAccount:(SecBoolNSErrorCallback)syncCallback {
    CKKSAccountStatus accountStatus = self.accountStatus;
    dispatch_async(self.queue, ^{
        syncCallback(false, [NSError errorWithDomain:@"securityd"
                                                code:errSecNotLoggedIn
                                            userInfo:@{NSLocalizedDescriptionKey:
                                                           [NSString stringWithFormat: @"No iCloud account available(%d); item is not expected to sync", (int)accountStatus]}]);
    });
}

#pragma mark - CKKSChangeFetcherErrorOracle

- (bool) isFatalCKFetchError: (NSError*) error {
    __weak __typeof(self) weakSelf = self;

    // Again, note that this handles exactly one zone. Mutli-zone errors are not supported.
    bool isChangeTokenExpiredError = false;
    if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorChangeTokenExpired)) {
        isChangeTokenExpiredError = true;
    } else if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorPartialFailure)) {
        NSDictionary* partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];
        for(NSError* partialError in partialErrors.allValues) {
            if([partialError.domain isEqualToString:CKErrorDomain] && (partialError.code == CKErrorChangeTokenExpired)) {
                isChangeTokenExpiredError = true;
            }
        }
    }

    if(isChangeTokenExpiredError) {
        ckkserror("ckks", self, "Received notice that our change token is out of date. Resetting local data...");
        CKKSResultOperation* resetOp = [self resetLocalData];
        CKKSResultOperation* resetHandler = [CKKSResultOperation named:@"local-reset-handler" withBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror("ckks", strongSelf, "received callback for released object");
                return;
            }

            if(resetOp.error) {
                ckksnotice("ckksreset", strongSelf, "CloudKit-inspired local reset of %@ ended with error: %@", strongSelf.zoneID, error);
            } else {
                ckksnotice("ckksreset", strongSelf, "CloudKit-inspired local reset of %@ ended successfully", strongSelf.zoneID);
            }
        }];

        [resetHandler addDependency:resetOp];
        [self scheduleOperation:resetHandler];
        return true;
    }

    bool isDeletedZoneError = false;
    if([error.domain isEqualToString:CKErrorDomain] && ((error.code == CKErrorUserDeletedZone) || (error.code == CKErrorZoneNotFound))) {
        isDeletedZoneError = true;
    } else if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorPartialFailure)) {
        NSDictionary* partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];
        for(NSError* partialError in partialErrors.allValues) {
            if([partialError.domain isEqualToString:CKErrorDomain] && ((partialError.code == CKErrorUserDeletedZone) || (partialError.code == CKErrorZoneNotFound))) {
                isDeletedZoneError = true;
            }
        }
    }

    if(isDeletedZoneError) {
        ckkserror("ckks", self, "Received notice that our zone does not exist. Resetting local data.");
        CKKSResultOperation* resetOp = [self resetLocalData];
        CKKSResultOperation* resetHandler = [CKKSResultOperation named:@"reset-handler" withBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror("ckksreset", strongSelf, "received callback for released object");
                return;
            }

            if(resetOp.error) {
                ckksnotice("ckksreset", strongSelf, "CloudKit-inspired local reset of %@ ended with error: %@", strongSelf.zoneID, resetOp.error);
            } else {
                ckksnotice("ckksreset", strongSelf, "CloudKit-inspired local reset of %@ ended successfully", strongSelf.zoneID);
            }
        }];

        [resetHandler addDependency:resetOp];
        [self scheduleOperation:resetHandler];
        return true;
    }

    if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorBadContainer)) {
        ckkserror("ckks", self, "Received notice that our container does not exist. Nothing to do.");
        return true;
    }

    return false;
}

#pragma mark CKKSPeerUpdateListener

- (void)selfPeerChanged {
    // Currently, we have no idea what to do with this. Kick off a key reprocess?
    ckkserror("ckks", self, "Received update that our self identity has changed");
    [self keyStateMachineRequestProcess];
}

- (void)trustedPeerSetChanged {
    // We might need to share the TLK to some new people, or we might now trust the TLKs we have.
    // The key state machine should handle that, so poke it.
    ckkserror("ckks", self, "Received update that the trust set has changed");
    [self keyStateMachineRequestProcess];
}

#pragma mark - Test Support

- (bool) outgoingQueueEmpty: (NSError * __autoreleasing *) error {
    __block bool ret = false;
    [self dispatchSync: ^bool{
        NSArray* queueEntries = [CKKSOutgoingQueueEntry all: error];
        ret = queueEntries && ([queueEntries count] == 0);
        return true;
    }];

    return ret;
}

- (CKKSResultOperation*)waitForFetchAndIncomingQueueProcessing {
    CKKSResultOperation* op = [self fetchAndProcessCKChanges:CKKSFetchBecauseTesting];
    [op waitUntilFinished];
    return op;
}

- (void)waitForKeyHierarchyReadiness {
    if(self.keyStateReadyDependency) {
        [self.keyStateReadyDependency waitUntilFinished];
    }
}

- (void)cancelPendingOperations {
    @synchronized(self.outgoingQueueOperations) {
        for(NSOperation* op in self.outgoingQueueOperations) {
            [op cancel];
        }
        [self.outgoingQueueOperations removeAllObjects];
    }

    @synchronized(self.incomingQueueOperations) {
        for(NSOperation* op in self.incomingQueueOperations) {
            [op cancel];
        }
        [self.incomingQueueOperations removeAllObjects];
    }

    [super cancelAllOperations];
}

- (void)cancelAllOperations {
    [self.zoneSetupOperation cancel];
    [self.keyStateMachineOperation cancel];
    [self.keyStateReadyDependency cancel];
    [self.keyStateNonTransientDependency cancel];
    [self.zoneChangeFetcher cancel];
    [self.notifyViewChangedScheduler cancel];

    [self cancelPendingOperations];

    [self dispatchSync:^bool{
        [self _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateCancelled withError: nil];
        return true;
    }];
}

- (void)halt {
    [super halt];

    // Don't send any more notifications, either
    _notifierClass = nil;
}

- (NSDictionary*)status {
#define stringify(obj) CKKSNilToNSNull([obj description])
#define boolstr(obj) (!!(obj) ? @"yes" : @"no")
    __block NSDictionary* ret = nil;
    __block NSError* error = nil;
    CKKSManifest* manifest = [CKKSManifest latestTrustedManifestForZone:self.zoneName error:&error];
    [self dispatchSync: ^bool {

        CKKSCurrentKeySet* keyset = [[CKKSCurrentKeySet alloc] initForZone:self.zoneID];
        if(keyset.error) {
            error = keyset.error;
        }

        NSString* manifestGeneration = manifest ? [NSString stringWithFormat:@"%lu", (unsigned long)manifest.generationCount] : nil;

        if(error) {
            ckkserror("ckks", self, "error during status: %@", error);
        }
        // We actually don't care about this error, especially if it's "no current key pointers"...
        error = nil;

        // Map deviceStates to strings to avoid NSXPC issues. Obj-c, why is this so hard?
        NSArray* deviceStates = [CKKSDeviceStateEntry allInZone:self.zoneID error:&error];
        NSMutableArray<NSString*>* mutDeviceStates = [[NSMutableArray alloc] init];
        [deviceStates enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [mutDeviceStates addObject: [obj description]];
        }];

        NSArray* tlkShares = [CKKSTLKShare allForUUID:keyset.currentTLKPointer.currentKeyUUID zoneID:self.zoneID error:&error];
        NSMutableArray<NSString*>* mutTLKShares = [[NSMutableArray alloc] init];
        [tlkShares enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [mutTLKShares addObject: [obj description]];
        }];


        ret = @{
                 @"view":                CKKSNilToNSNull(self.zoneName),
                 @"ckaccountstatus":     self.accountStatus == CKAccountStatusCouldNotDetermine ? @"could not determine" :
                                         self.accountStatus == CKAccountStatusAvailable         ? @"logged in" :
                                         self.accountStatus == CKAccountStatusRestricted        ? @"restricted" :
                                         self.accountStatus == CKAccountStatusNoAccount         ? @"logged out" : @"unknown",
                 @"lockstatetracker":    stringify(self.lockStateTracker),
                 @"accounttracker":      stringify(self.accountTracker),
                 @"fetcher":             stringify(self.zoneChangeFetcher),
                 @"zoneCreated":         boolstr(self.zoneCreated),
                 @"zoneCreatedError":    stringify(self.zoneCreatedError),
                 @"zoneSubscribed":      boolstr(self.zoneSubscribed),
                 @"zoneSubscribedError": stringify(self.zoneSubscribedError),
                 @"zoneInitializeScheduler": stringify(self.initializeScheduler),
                 @"keystate":            CKKSNilToNSNull(self.keyHierarchyState),
                 @"keyStateError":       stringify(self.keyHierarchyError),
                 @"statusError":         stringify(error),
                 @"oqe":                 CKKSNilToNSNull([CKKSOutgoingQueueEntry countsByStateInZone:self.zoneID error:&error]),
                 @"iqe":                 CKKSNilToNSNull([CKKSIncomingQueueEntry countsByStateInZone:self.zoneID error:&error]),
                 @"ckmirror":            CKKSNilToNSNull([CKKSMirrorEntry        countsByParentKey:self.zoneID error:&error]),
                 @"devicestates":        CKKSNilToNSNull(mutDeviceStates),
                 @"tlkshares":           CKKSNilToNSNull(mutTLKShares),
                 @"keys":                CKKSNilToNSNull([CKKSKey countsByClass:self.zoneID error:&error]),
                 @"currentTLK":          CKKSNilToNSNull(keyset.tlk.uuid),
                 @"currentClassA":       CKKSNilToNSNull(keyset.classA.uuid),
                 @"currentClassC":       CKKSNilToNSNull(keyset.classC.uuid),
                 @"currentTLKPtr":       CKKSNilToNSNull(keyset.currentTLKPointer.currentKeyUUID),
                 @"currentClassAPtr":    CKKSNilToNSNull(keyset.currentClassAPointer.currentKeyUUID),
                 @"currentClassCPtr":    CKKSNilToNSNull(keyset.currentClassCPointer.currentKeyUUID),
                 @"currentManifestGen":   CKKSNilToNSNull(manifestGeneration),


                 @"zoneSetupOperation":                 stringify(self.zoneSetupOperation),
                 @"keyStateOperation":                  stringify(self.keyStateMachineOperation),
                 @"lastIncomingQueueOperation":         stringify(self.lastIncomingQueueOperation),
                 @"lastNewTLKOperation":                stringify(self.lastNewTLKOperation),
                 @"lastOutgoingQueueOperation":         stringify(self.lastOutgoingQueueOperation),
                 @"lastRecordZoneChangesOperation":     stringify(self.lastRecordZoneChangesOperation),
                 @"lastProcessReceivedKeysOperation":   stringify(self.lastProcessReceivedKeysOperation),
                 @"lastReencryptOutgoingItemsOperation":stringify(self.lastReencryptOutgoingItemsOperation),
                 @"lastScanLocalItemsOperation":        stringify(self.lastScanLocalItemsOperation),
                 };
        return false;
    }];
    return ret;
}



#endif /* OCTAGON */
@end
