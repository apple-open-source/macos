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
#import "keychain/ckks/CKKSStates.h"
#import "OctagonAPSReceiver.h"
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
#import "CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSHealKeyHierarchyOperation.h"
#import "CKKSReencryptOutgoingItemsOperation.h"
#import "CKKSScanLocalItemsOperation.h"
#import "CKKSSynchronizeOperation.h"
#import "CKKSRateLimiter.h"
#import "CKKSManifest.h"
#import "CKKSManifestLeafRecord.h"
#import "CKKSZoneChangeFetcher.h"
#import "CKKSAnalytics.h"
#import "keychain/analytics/CKKSLaunchSequence.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSCreateCKZoneOperation.h"
#import "keychain/ckks/CKKSDeleteCKZoneOperation.h"
#import "keychain/ckks/CKKSUpdateCurrentItemPointerOperation.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ckks/CKKSHealTLKSharesOperation.h"
#import "keychain/ckks/CKKSLocalSynchronizeOperation.h"
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ckks/CKKSCheckKeyHierarchyOperation.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ckks/CKKSLocalResetOperation.h"

#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"
#import "keychain/ot/ObjCImprovements.h"

#include <utilities/SecCFWrappers.h>
#include <utilities/SecTrace.h>
#include <utilities/SecDb.h>
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"
#include <Security/SecItemPriv.h>
#include "keychain/SecureObjectSync/SOSAccountTransaction.h"
#include <utilities/SecPLWrappers.h>
#include <os/transaction_private.h>

#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"
#import <Security/SecItemInternal.h>

#if OCTAGON

@interface CKKSKeychainView()

// Slows down all outgoing queue operations
@property CKKSNearFutureScheduler* outgoingQueueOperationScheduler;

@property CKKSResultOperation* processIncomingQueueAfterNextUnlockOperation;
@property CKKSResultOperation* resultsOfNextIncomingQueueOperationOperation;

// An extra queue for semaphore-waiting-based NSOperations
@property NSOperationQueue* waitingQueue;

// Scratch space for resyncs
@property (nullable) NSMutableSet<NSString*>* resyncRecordsSeen;



@property NSOperationQueue* operationQueue;
@property CKKSResultOperation* accountLoggedInDependency;
@property BOOL halted;

// Make these readwrite
@property NSArray<CKKSPeerProviderState*>* currentTrustStates;

@property NSMutableSet<CKKSFetchBecause*>* currentFetchReasons;
@end
#endif

@implementation CKKSKeychainView
#if OCTAGON

- (instancetype)initWithContainer:(CKContainer*)container
                         zoneName:(NSString*)zoneName
                   accountTracker:(CKKSAccountStateTracker*)accountTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                    changeFetcher:(CKKSZoneChangeFetcher*)fetcher
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
                 savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{

    if((self = [super init])) {
        WEAKIFY(self);

        _container = container;
        _accountTracker = accountTracker;
        _reachabilityTracker = reachabilityTracker;
        _lockStateTracker = lockStateTracker;
        _cloudKitClassDependencies = cloudKitClassDependencies;

        _halted = NO;

        _accountStatus = CKKSAccountStatusUnknown;
        _accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in."];

        _queue = dispatch_queue_create([[NSString stringWithFormat:@"CKKSQueue.%@.zone.%@", container.containerIdentifier, zoneName] UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _operationQueue = [[NSOperationQueue alloc] init];
        _waitingQueue = [[NSOperationQueue alloc] init];
        _waitingQueue.maxConcurrentOperationCount = 5;


        _loggedIn = [[CKKSCondition alloc] init];
        _loggedOut = [[CKKSCondition alloc] init];
        _accountStateKnown = [[CKKSCondition alloc] init];

        _initiatedLocalScan = NO;

        _trustStatus = CKKSAccountStatusUnknown;

        _incomingQueueOperations = [NSHashTable weakObjectsHashTable];
        _outgoingQueueOperations = [NSHashTable weakObjectsHashTable];
        _scanLocalItemsOperations = [NSHashTable weakObjectsHashTable];

        _currentTrustStates = @[];

        _currentFetchReasons = [NSMutableSet set];

        _launch = [[CKKSLaunchSequence alloc] initWithRocketName:@"com.apple.security.ckks.launch"];
        [_launch addAttribute:@"view" value:zoneName];

        _resyncRecordsSeen = nil;

        CKKSNearFutureScheduler* notifyViewChangedScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"ckks-%@-notify-scheduler", zoneName]
                                                                                               initialDelay:250*NSEC_PER_MSEC
                                                                                            continuingDelay:1*NSEC_PER_SEC
                                                                                           keepProcessAlive:true
                                                                                  dependencyDescriptionCode:CKKSResultDescriptionPendingViewChangedScheduling
                                                                                                      block:^{
            STRONGIFY(self);
            [self.cloudKitClassDependencies.notifierClass post:[NSString stringWithFormat:@"com.apple.security.view-change.%@", zoneName]];
            [self.cloudKitClassDependencies.notifierClass post:[NSString stringWithUTF8String:kSecServerKeychainChangedNotification]];

            // Ugly, but: the Manatee and Engram views need to send a fake 'PCS' view change.
            // TODO: make this data-driven somehow
            if([zoneName isEqualToString:@"Manatee"] ||
               [zoneName isEqualToString:@"Engram"] ||
               [zoneName isEqualToString:@"ApplePay"] ||
               [zoneName isEqualToString:@"Home"] ||
               [zoneName isEqualToString:@"LimitedPeersAllowed"]) {
                [self.cloudKitClassDependencies.notifierClass post:@"com.apple.security.view-change.PCS"];
            }
        }];

        CKKSNearFutureScheduler* notifyViewReadyScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-ready-scheduler", zoneName]
                                                                                             initialDelay:250*NSEC_PER_MSEC
                                                                                          continuingDelay:120*NSEC_PER_SEC
                                                                                         keepProcessAlive:true
                                                                                dependencyDescriptionCode:CKKSResultDescriptionPendingViewChangedScheduling
                                                                                                    block:^{
            STRONGIFY(self);
            NSDistributedNotificationCenter *center = [self.cloudKitClassDependencies.nsdistributednotificationCenterClass defaultCenter];

            [center postNotificationName:@"com.apple.security.view-become-ready"
                                  object:nil
                                userInfo:@{ @"view" : zoneName ?: @"unknown" }
                                 options:0];
        }];

        _stateMachine = [[OctagonStateMachine alloc] initWithName:[NSString stringWithFormat:@"ckks-%@", zoneName]
                                                           states:[NSSet setWithArray:[CKKSZoneKeyStateMap() allKeys]]
                                                            flags:CKKSAllStateFlags()
                                                     initialState:SecCKKSZoneKeyStateWaitForCloudKitAccountStatus
                                                            queue:self.queue
                                                      stateEngine:self
                                                 lockStateTracker:lockStateTracker
                                              reachabilityTracker:reachabilityTracker];

        _viewState = [[CKKSKeychainViewState alloc] initWithZoneID:[[CKRecordZoneID alloc] initWithZoneName:zoneName ownerName:CKCurrentUserDefaultName]
                                                  viewStateMachine:self.stateMachine
                                        notifyViewChangedScheduler:notifyViewChangedScheduler
                                          notifyViewReadyScheduler:notifyViewReadyScheduler];
        _viewStates = [NSSet setWithObject:_viewState];

        _keyStateReadyDependency = [self createKeyStateReadyDependency: @"Key state has become ready for the first time."];

        dispatch_time_t initialOutgoingQueueDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 1;
        dispatch_time_t continuingOutgoingQueueDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 30;
        _outgoingQueueOperationScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-outgoing-queue-scheduler", zoneName]
                                                                            initialDelay:initialOutgoingQueueDelay
                                                                         continuingDelay:continuingOutgoingQueueDelay
                                                                        keepProcessAlive:false
                                                               dependencyDescriptionCode:CKKSResultDescriptionPendingOutgoingQueueScheduling
                                                                                   block:^{}];

        _operationDependencies = [[CKKSOperationDependencies alloc] initWithViewState:_viewState
                                                                         zoneModifier:zoneModifier
                                                                           ckdatabase:[_container privateCloudDatabase]
                                                                     ckoperationGroup:nil
                                                                          flagHandler:_stateMachine
                                                                       launchSequence:_launch
                                                                  accountStateTracker:accountTracker
                                                                     lockStateTracker:_lockStateTracker
                                                                  reachabilityTracker:reachabilityTracker
                                                                        peerProviders:@[]
                                                                     databaseProvider:self
                                                           notifyViewChangedScheduler:_viewState.notifyViewChangedScheduler
                                                                     savedTLKNotifier:savedTLKNotifier];

        _zoneChangeFetcher = fetcher;
        [fetcher registerClient:self];

        [_stateMachine startOperation];
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
    return self.stateMachine.currentState;
}

- (NSDictionary<CKKSZoneKeyState*, CKKSCondition*>*)keyHierarchyConditions
{
    return self.stateMachine.stateConditions;
}

- (NSString*)zoneName
{
    return self.viewState.zoneName;
}

- (CKRecordZoneID*)zoneID
{
    return self.viewState.zoneID;
}

- (void)ensureKeyStateReadyDependency:(NSString*)resetMessage {
    NSOperation* oldKSRD = self.keyStateReadyDependency;
    self.keyStateReadyDependency = [self createKeyStateReadyDependency:resetMessage];
    if(oldKSRD) {
        [oldKSRD addDependency:self.keyStateReadyDependency];
        [self.waitingQueue addOperation:oldKSRD];
    }
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)performInitializedOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"ckks-initialized-operation"
                                        intending:SecCKKSZoneKeyStateBecomeReady
                                       errorState:SecCKKSZoneKeyStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);
        [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            CKKSOutgoingQueueOperation* outgoingOperation = nil;
            CKKSIncomingQueueOperation* initialProcess = nil;
            CKKSScanLocalItemsOperation* initialScan = nil;

            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.zoneName];

            // Check if we believe we've synced this zone before.
            if(ckse.changeToken == nil) {
                self.operationDependencies.ckoperationGroup = [CKOperationGroup CKKSGroupWithName:@"initial-setup"];

                ckksnotice("ckks", self, "No existing change token; going to try to match local items with CloudKit ones.");

                // Onboard this keychain: there's likely items in it that we haven't synced yet.
                // But, there might be items in The Cloud that correspond to these items, with UUIDs that we don't know yet.
                // First, fetch all remote items.

                [self.currentFetchReasons addObject:CKKSFetchBecauseInitialStart];
                op.nextState = SecCKKSZoneKeyStateBeginFetch;

                // Next, try to process them (replacing local entries). This will wait for the key state to be ready.
                initialProcess = [self processIncomingQueue:true after:nil];

                // If all that succeeds, iterate through all keychain items and find the ones which need to be uploaded
                initialScan = [self scanLocalItems:@"initial-scan-operation"
                                  ckoperationGroup:self.operationDependencies.ckoperationGroup
                                             after:initialProcess];

            } else {
                // Likely a restart of securityd!

                // Are there any fixups to run first?
                self.lastFixupOperation = [CKKSFixups fixup:ckse.lastFixup for:self];
                if(self.lastFixupOperation) {
                    ckksnotice("ckksfixup", self, "We have a fixup to perform: %@", self.lastFixupOperation);
                    [self scheduleOperation:self.lastFixupOperation];
                    op.nextState = SecCKKSZoneKeyStateWaitForFixupOperation;
                    return CKKSDatabaseTransactionCommit;
                }

                // First off, are there any in-flight queue entries? If so, put them back into New.
                // If they're truly in-flight, we'll "conflict" with ourselves, but that should be fine.
                NSError* error = nil;
                [self _onqueueResetAllInflightOQE:&error];
                if(error) {
                    ckkserror("ckks", self, "Couldn't reset in-flight OQEs, bad behavior ahead: %@", error);
                }

                // Are there any entries waiting for reencryption? If so, set the flag.
                error = nil;
                NSArray<CKKSOutgoingQueueEntry*>* reencryptOQEs = [CKKSOutgoingQueueEntry allInState:SecCKKSStateReencrypt
                                                                                              zoneID:self.zoneID
                                                                                               error:&error];
                if(error) {
                    ckkserror("ckks", self, "Couldn't load reencrypt OQEs, bad behavior ahead: %@", error);
                }
                if(reencryptOQEs.count > 0) {
                    [self.stateMachine _onqueueHandleFlag:CKKSFlagItemReencryptionNeeded];
                }

                self.operationDependencies.ckoperationGroup = [CKOperationGroup CKKSGroupWithName:@"restart-setup"];

                // If it's been more than 24 hours since the last fetch, fetch and process everything.
                // Or, if we think we were interrupted in the middle of fetching, fetch some more.
                // Otherwise, just kick off the local queue processing.

                NSDate* now = [NSDate date];
                NSDateComponents* offset = [[NSDateComponents alloc] init];
                [offset setHour:-24];
                NSDate* deadline = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:now options:0];

                if(ckse.lastFetchTime == nil ||
                   [ckse.lastFetchTime compare: deadline] == NSOrderedAscending ||
                   ckse.moreRecordsInCloudKit) {

                    op.nextState = SecCKKSZoneKeyStateBeginFetch;

                } else {
                    // Check if we have an existing key hierarchy in keyset
                    CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:self.zoneID];
                    if(keyset.error && !([keyset.error.domain isEqual: @"securityd"] && keyset.error.code == errSecItemNotFound)) {
                        ckkserror("ckkskey", self, "Error examining existing key hierarchy: %@", keyset.error);
                    }

                    if(keyset.tlk && keyset.classA && keyset.classC && !keyset.error) {
                        // This is likely a restart of securityd, and we think we're ready. Double check.
                        op.nextState = SecCKKSZoneKeyStateBecomeReady;

                    } else {
                        ckksnotice("ckkskey", self, "No existing key hierarchy for %@. Check if there's one in CloudKit...", self.zoneID.zoneName);
                        op.nextState = SecCKKSZoneKeyStateBeginFetch;
                    }
                }

                if(ckse.lastLocalKeychainScanTime == nil || [ckse.lastLocalKeychainScanTime compare:deadline] == NSOrderedAscending) {
                    // TODO handle with a state flow
                    ckksnotice("ckksscan", self, "CKKS scan last occurred at %@; beginning a new one", ckse.lastLocalKeychainScanTime);
                    initialScan = [self scanLocalItems:ckse.lastLocalKeychainScanTime == nil ? @"initial-scan-operation" : @"24-hr-scan-operation"
                                      ckoperationGroup:self.operationDependencies.ckoperationGroup
                                                 after:nil];
                }

                // Process outgoing queue after re-start
                outgoingOperation = [self processOutgoingQueueAfter:nil ckoperationGroup:self.operationDependencies.ckoperationGroup];
            }

            /*
             * Launch time is determined by when the zone have:
             *  1. keystate have become ready
             *  2. scan local items (if needed)
             *  3. processed all outgoing item (if needed)
             * TODO: this should move, once queue processing becomes part of the state machine
             */

            WEAKIFY(self);
            NSBlockOperation *seemReady = [NSBlockOperation named:[NSString stringWithFormat:@"seemsReadyForSyncing-%@", self.zoneName] withBlock:^void{
                STRONGIFY(self);
                NSError *error = nil;
                ckksnotice("launch", self, "Launch complete");
                NSNumber *zoneSize = [CKKSMirrorEntry counts:self.zoneID error:&error];
                if (zoneSize) {
                    zoneSize = @(SecBucket1Significant([zoneSize longValue]));
                    [self.launch addAttribute:@"zonesize" value:zoneSize];
                }
                [self.launch launch];

                /*
                 * Since we think we are ready, signal to CK that its to check for PCS identities again, and create the
                 * since before we completed this operation, we would probably have failed with a timeout because
                 * we where busy downloading items from CloudKit and then processing them.
                 */
                [self.viewState.notifyViewReadyScheduler trigger];
            }];

            [seemReady addNullableDependency:self.keyStateReadyDependency];
            [seemReady addNullableDependency:outgoingOperation];
            [seemReady addNullableDependency:initialScan];
            [seemReady addNullableDependency:initialProcess];
            [self scheduleOperation:seemReady];

            return CKKSDatabaseTransactionCommit;
        }];
    }];
}

- (CKKSResultOperation*)resetLocalData {
    ckksnotice("ckksreset", self, "Requesting local data reset");

    return [self.stateMachine doWatchedStateMachineRPC:@"ckks-local-reset"
                                   sourceStates:[NSSet setWithArray:@[
                                       // TODO: possibly every state?
                                       SecCKKSZoneKeyStateReady,
                                       SecCKKSZoneKeyStateWaitForTLK,
                                       SecCKKSZoneKeyStateWaitForTrust,
                                       SecCKKSZoneKeyStateWaitForTLKUpload,
                                       SecCKKSZoneKeyStateLoggedOut,
                                   ]]
                                           path:[OctagonStateTransitionPath pathFromDictionary:@{
                                               SecCKKSZoneKeyStateResettingLocalData: @{
                                                   SecCKKSZoneKeyStateInitializing: @{
                                                       SecCKKSZoneKeyStateInitialized: [OctagonStateTransitionPathStep success],
                                                       SecCKKSZoneKeyStateLoggedOut: [OctagonStateTransitionPathStep success],
                                                   }
                                               }
                                           }]
                                                 reply:^(NSError * _Nonnull error) {}];
}

- (CKKSResultOperation*)resetCloudKitZone:(CKOperationGroup*)operationGroup
{
    [self.accountStateKnown wait:(SecCKKSTestsEnabled() ? 1*NSEC_PER_SEC : 10*NSEC_PER_SEC)];

    // Not overly thread-safe, but a single read is okay
    if(self.accountStatus != CKKSAccountStatusAvailable) {
        // No CK account? goodbye!
        ckksnotice("ckksreset", self, "Requesting reset of CK zone, but no CK account exists");
        CKKSResultOperation* errorOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            op.error = [NSError errorWithDomain:CKKSErrorDomain
                                          code:CKKSNotLoggedIn
                                   description:@"User is not signed into iCloud."];
        }];

        [self scheduleOperationWithoutDependencies:errorOp];
        return errorOp;
    }

    ckksnotice("ckksreset", self, "Requesting reset of CK zone (logged in)");

    NSDictionary* localResetPath = @{
        SecCKKSZoneKeyStateInitializing: @{
            SecCKKSZoneKeyStateInitialized: [OctagonStateTransitionPathStep success],
            SecCKKSZoneKeyStateLoggedOut: [OctagonStateTransitionPathStep success],
        },
    };

    // If the zone delete doesn't work, try it up to two more times

    return [self.stateMachine doWatchedStateMachineRPC:@"ckks-cloud-reset"
                                          sourceStates:[NSSet setWithArray:@[
                                              // TODO: possibly every state?
                                              SecCKKSZoneKeyStateReady,
                                              SecCKKSZoneKeyStateInitialized,
                                              SecCKKSZoneKeyStateFetchComplete,
                                              SecCKKSZoneKeyStateWaitForTLK,
                                              SecCKKSZoneKeyStateWaitForTrust,
                                              SecCKKSZoneKeyStateWaitForTLKUpload,
                                              SecCKKSZoneKeyStateLoggedOut,
                                          ]]
                                                  path:[OctagonStateTransitionPath pathFromDictionary:@{
                                                      SecCKKSZoneKeyStateResettingZone: @{
                                                          SecCKKSZoneKeyStateResettingLocalData: localResetPath,
                                                          SecCKKSZoneKeyStateResettingZone: @{
                                                              SecCKKSZoneKeyStateResettingLocalData: localResetPath,
                                                              SecCKKSZoneKeyStateResettingZone: @{
                                                                 SecCKKSZoneKeyStateResettingLocalData: localResetPath,
                                                              }
                                                          }
                                                      }
                                                  }]
                                                 reply:^(NSError * _Nonnull error) {}];
}

- (void)keyStateMachineRequestProcess {
    [self.stateMachine handleFlag:CKKSFlagKeyStateProcessRequested];
}

- (CKKSResultOperation*)createKeyStateReadyDependency:(NSString*)message {
    WEAKIFY(self);
    CKKSResultOperation* keyStateReadyDependency = [CKKSResultOperation operationWithBlock:^{
        STRONGIFY(self);
        ckksnotice("ckkskey", self, "CKKS became ready: %@", message);
    }];
    keyStateReadyDependency.name = [NSString stringWithFormat: @"%@-key-state-ready", self.zoneName];
    keyStateReadyDependency.descriptionErrorCode = CKKSResultDescriptionPendingKeyReady;
    return keyStateReadyDependency;
}

- (void)_onqueuePokeKeyStateMachine
{
    dispatch_assert_queue(self.queue);
    [self.stateMachine _onqueuePokeStateMachine];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextStateMachineTransition:(OctagonState*)currentState
                                                                                                        flags:(OctagonFlags*)flags
                                                                                                 pendingFlags:(id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler
{
    dispatch_assert_queue(self.queue);

    // Resetting back to 'loggedout' takes all precedence.
    if([flags _onqueueContains:CKKSFlagCloudKitLoggedOut]) {
        [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedOut];
        ckksnotice("ckkskey", self, "CK account is not present");

        [self ensureKeyStateReadyDependency:@"cloudkit-account-not-present"];
        return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                       intendedState:SecCKKSZoneKeyStateLoggedOut
                                                          errorState:SecCKKSZoneKeyStateError];
    }

    if([flags _onqueueContains:CKKSFlagCloudKitZoneMissing]) {
        [flags _onqueueRemoveFlag:CKKSFlagCloudKitZoneMissing];

        [self ensureKeyStateReadyDependency:@"cloudkit-zone-missing"];
        // The zone is gone! Let's reset our local state, which will feed into recreating the zone
        return [OctagonStateTransitionOperation named:@"ck-zone-missing"
                                             entering:SecCKKSZoneKeyStateResettingLocalData];
    }

    if([flags _onqueueContains:CKKSFlagChangeTokenExpired]) {
        [flags _onqueueRemoveFlag:CKKSFlagChangeTokenExpired];

        [self ensureKeyStateReadyDependency:@"cloudkit-change-token-expired"];
        // Our change token is invalid! We'll have to refetch the world, so let's delete everything locally.
        return [OctagonStateTransitionOperation named:@"ck-token-expired"
                                             entering:SecCKKSZoneKeyStateResettingLocalData];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateLoggedOut]) {
        if([flags _onqueueContains:CKKSFlagCloudKitLoggedIn] || self.accountStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedIn];

            ckksnotice("ckkskey", self, "CloudKit account now present");
            return [OctagonStateTransitionOperation named:@"ck-sign-in"
                                                 entering:SecCKKSZoneKeyStateInitializing];
        }

        if([flags _onqueueContains:CKKSFlag24hrNotification]) {
            [flags _onqueueRemoveFlag:CKKSFlag24hrNotification];
        }
        return nil;
    }

    if([currentState isEqualToString: SecCKKSZoneKeyStateWaitForCloudKitAccountStatus]) {
        if([flags _onqueueContains:CKKSFlagCloudKitLoggedIn] || self.accountStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedIn];

            ckksnotice("ckkskey", self, "CloudKit account now present");
            return [OctagonStateTransitionOperation named:@"ck-sign-in"
                                                 entering:SecCKKSZoneKeyStateInitializing];
        }

        if([flags _onqueueContains:CKKSFlagCloudKitLoggedOut]) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedOut];
            ckksnotice("ckkskey", self, "No account available");

            return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:SecCKKSZoneKeyStateLoggedOut
                                                              errorState:SecCKKSZoneKeyStateError];
        }
        return nil;
    }

    [self.launch addEvent:currentState];

    if([currentState isEqual:SecCKKSZoneKeyStateInitializing]) {
        if(self.accountStatus == CKKSAccountStatusNoAccount) {
            ckksnotice("ckkskey", self, "CloudKit account is missing. Departing!");
            return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:SecCKKSZoneKeyStateLoggedOut
                                                              errorState:SecCKKSZoneKeyStateError];
        }

        // Begin zone creation, but rate-limit it
        CKKSCreateCKZoneOperation* pendingInitializeOp = [[CKKSCreateCKZoneOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                   intendedState:SecCKKSZoneKeyStateInitialized
                                                                                                      errorState:SecCKKSZoneKeyStateZoneCreationFailed];
        [pendingInitializeOp addNullableDependency:self.operationDependencies.zoneModifier.cloudkitRetryAfter.operationDependency];
        [self.operationDependencies.zoneModifier.cloudkitRetryAfter trigger];

        return pendingInitializeOp;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateWaitForFixupOperation]) {
        // TODO: fixup operations should become part of the state machine
        ckksnotice("ckkskey", self, "Waiting for the fixup operation: %@", self.lastFixupOperation);
        OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"wait-for-fixup" entering:SecCKKSZoneKeyStateInitialized];
        [op addNullableDependency:self.lastFixupOperation];
        return op;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateInitialized]) {
        // We're initialized and CloudKit is ready. If we're trusted, see what needs done. Otherwise, wait.
        return [self performInitializedOperation];
    }

    // In error? You probably aren't getting out.
    if([currentState isEqualToString:SecCKKSZoneKeyStateError]) {
        if([flags _onqueueContains:CKKSFlagCloudKitLoggedIn]) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedIn];

            // Worth one last shot. Reset everything locally, and try again.
            return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:SecCKKSZoneKeyStateInitializing
                                                              errorState:SecCKKSZoneKeyStateError];
        }

        ckkserror("ckkskey", self, "Staying in error state %@", currentState);
        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateResettingZone]) {
        ckksnotice("ckkskey", self, "Deleting the CloudKit Zone");

        [self ensureKeyStateReadyDependency:@"ck-zone-reset"];
        return [[CKKSDeleteCKZoneOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:SecCKKSZoneKeyStateResettingLocalData
                                                            errorState:SecCKKSZoneKeyStateResettingZone];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateResettingLocalData]) {
        ckksnotice("ckkskey", self, "Resetting local data");

        [self ensureKeyStateReadyDependency:@"local-data-reset"];
        return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                       intendedState:SecCKKSZoneKeyStateInitializing
                                                          errorState:SecCKKSZoneKeyStateError];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateZoneCreationFailed]) {
        //Prepare to go back into initializing, as soon as the cloudkitRetryAfter is happy
        OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"recover-from-cloudkit-failure" entering:SecCKKSZoneKeyStateInitializing];

        [op addNullableDependency:self.operationDependencies.zoneModifier.cloudkitRetryAfter.operationDependency];
        [self.operationDependencies.zoneModifier.cloudkitRetryAfter trigger];

        return op;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateLoseTrust]) {
        if([flags _onqueueContains:CKKSFlagBeginTrustedOperation]) {
            [flags _onqueueRemoveFlag:CKKSFlagBeginTrustedOperation];
            // This was likely a race between some operation and the beginTrustedOperation call! Skip changing state and try again.
            return [OctagonStateTransitionOperation named:@"begin-trusted-operation" entering:SecCKKSZoneKeyStateInitialized];
        }

        // If our current state is "trusted", fall out
        if(self.trustStatus == CKKSAccountStatusAvailable) {
            self.trustStatus = CKKSAccountStatusUnknown;
        }
        return [OctagonStateTransitionOperation named:@"trust-loss" entering:SecCKKSZoneKeyStateWaitForTrust];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateWaitForTrust]) {
        if(self.trustStatus == CKKSAccountStatusAvailable) {
            ckksnotice("ckkskey", self, "Beginning trusted state machine operation");
            return [OctagonStateTransitionOperation named:@"begin-trusted-operation" entering:SecCKKSZoneKeyStateInitialized];
        }

        if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchRequested];
            return [OctagonStateTransitionOperation named:@"fetch-requested" entering:SecCKKSZoneKeyStateBeginFetch];
        }

        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            return [OctagonStateTransitionOperation named:@"begin-trusted-operation" entering:SecCKKSZoneKeyStateProcess];
        }

        if([flags _onqueueContains:CKKSFlagKeySetRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
            return [OctagonStateTransitionOperation named:@"process" entering:SecCKKSZoneKeyStateProcess];
        }

        if([flags _onqueueContains:CKKSFlag24hrNotification]) {
            [flags _onqueueRemoveFlag:CKKSFlag24hrNotification];
        }

        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateBecomeReady]) {
        return [[CKKSCheckKeyHierarchyOperation alloc] initWithDependencies:self.operationDependencies
                                                              intendedState:SecCKKSZoneKeyStateReady
                                                                 errorState:SecCKKSZoneKeyStateError];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateReady]) {
        // If we're ready, we can ignore the begin trusted flag
        [flags _onqueueRemoveFlag:CKKSFlagBeginTrustedOperation];

        if(self.keyStateFullRefetchRequested) {
            // In ready, but something has requested a full refetch.
            ckksnotice("ckkskey", self, "Kicking off a full key refetch based on request:%d", self.keyStateFullRefetchRequested);
            [self ensureKeyStateReadyDependency:@"key-state-full-refetch"];
            return [OctagonStateTransitionOperation named:@"full-refetch" entering:SecCKKSZoneKeyStateNeedFullRefetch];
        }

        if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchRequested];
            ckksnotice("ckkskey", self, "Kicking off a key refetch based on request");
            [self ensureKeyStateReadyDependency:@"key-state-fetch"];
            return [OctagonStateTransitionOperation named:@"fetch-requested" entering:SecCKKSZoneKeyStateBeginFetch];
        }

        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            ckksnotice("ckkskey", self, "Kicking off a key reprocess based on request");
            [self ensureKeyStateReadyDependency:@"key-state-process"];
            return [OctagonStateTransitionOperation named:@"key-process" entering:SecCKKSZoneKeyStateProcess];
        }

        if([flags _onqueueContains:CKKSFlagKeySetRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
            [self ensureKeyStateReadyDependency:@"key-state-process"];
            return [OctagonStateTransitionOperation named:@"provide-key-set" entering:SecCKKSZoneKeyStateProcess];
        }

        if(self.trustStatus != CKKSAccountStatusAvailable) {
            ckksnotice("ckkskey", self, "In ready, but there's no trust; going into waitfortrust");
            [self ensureKeyStateReadyDependency:@"trust loss"];
            return [OctagonStateTransitionOperation named:@"trust-gone" entering:SecCKKSZoneKeyStateLoseTrust];
        }

        if([flags _onqueueContains:CKKSFlagTrustedPeersSetChanged]) {
            [flags _onqueueRemoveFlag:CKKSFlagTrustedPeersSetChanged];
            ckksnotice("ckkskey", self, "Received a nudge that the trusted peers set might have changed! Reprocessing.");
            [self ensureKeyStateReadyDependency:@"Peer set changed"];
            return [OctagonStateTransitionOperation named:@"trusted-peers-changed" entering:SecCKKSZoneKeyStateProcess];
        }

        if([flags _onqueueContains:CKKSFlag24hrNotification]) {
            [flags _onqueueRemoveFlag:CKKSFlag24hrNotification];

            // We'd like to trigger our 24-hr backup fetch and scan.
            // That's currently part of the Initialized state, so head that way
            return [OctagonStateTransitionOperation named:@"24-hr-check" entering:SecCKKSZoneKeyStateInitialized];
        }

        if([flags _onqueueContains:CKKSFlagItemReencryptionNeeded]) {
            [flags _onqueueRemoveFlag:CKKSFlagItemReencryptionNeeded];

            // TODO: this should be part of the state machine
            CKKSReencryptOutgoingItemsOperation* op = [[CKKSReencryptOutgoingItemsOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                                   ckks:self
                                                                                                          intendedState:SecCKKSZoneKeyStateReady
                                                                                                             errorState:SecCKKSZoneKeyStateError];
            [self scheduleOperation:op];
            // fall through.
        }

        if([flags _onqueueContains:CKKSFlagProcessIncomingQueue]) {
            [flags _onqueueRemoveFlag:CKKSFlagProcessIncomingQueue];
            // TODO: this should be part of the state machine

            [self processIncomingQueue:true];
            //return [OctagonStateTransitionOperation named:@"process-outgoing" entering:SecCKKSZoneKeyStateProcessIncomingQueue];
        }

        if([flags _onqueueContains:CKKSFlagScanLocalItems]) {
            [flags _onqueueRemoveFlag:CKKSFlagScanLocalItems];
            ckksnotice("ckkskey", self, "Launching a scan operation to find dropped items");

            // TODO: this should be a state flow
            [self scanLocalItems:@"per-request"];
            // fall through
        }

        if([flags _onqueueContains:CKKSFlagProcessOutgoingQueue]) {
            [flags _onqueueRemoveFlag:CKKSFlagProcessOutgoingQueue];

            [self processOutgoingQueue:nil];
            // TODO: this should be a state flow.
            //return [OctagonStateTransitionOperation named:@"process-outgoing" entering:SecCKKSZoneKeyStateProcessOutgoingQueue];
            // fall through
        }

        // TODO: kick off a key roll if one has been requested


        // If we reach this point, we're in ready, and will stay there.
        // Tell the launch and the viewReadyScheduler about that.

        [self.launch launch];

        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastKeystateReady zoneName:self.zoneName];
        if(self.keyStateReadyDependency) {
            [self scheduleOperation:self.keyStateReadyDependency];
            self.keyStateReadyDependency = nil;
        }

        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock]) {
        if([flags _onqueueContains:CKKSFlagDeviceUnlocked]) {
            [flags _onqueueRemoveFlag:CKKSFlagDeviceUnlocked];
            [self ensureKeyStateReadyDependency:@"Device unlocked"];
            return [OctagonStateTransitionOperation named:@"key-state-ready-after-unlock" entering:SecCKKSZoneKeyStateBecomeReady];
        }

        if([flags _onqueueContains:CKKSFlagProcessOutgoingQueue]) {
            [flags _onqueueRemoveFlag:CKKSFlagProcessOutgoingQueue];
            [self processOutgoingQueue:nil];
            // TODO: this should become part of the key state hierarchy
        }

        // Ready enough!

        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastKeystateReady zoneName:self.zoneName];
        if(self.keyStateReadyDependency) {
            [self scheduleOperation:self.keyStateReadyDependency];
            self.keyStateReadyDependency = nil;
        }

        OctagonPendingFlag* unlocked = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagDeviceUnlocked
                                                                     conditions:OctagonPendingConditionsDeviceUnlocked];
        [pendingFlagHandler _onqueueHandlePendingFlag:unlocked];
        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateBeginFetch]) {
        ckksnotice("ckkskey", self, "Starting a key hierarchy fetch");
        [flags _onqueueRemoveFlag:CKKSFlagFetchComplete];

        WEAKIFY(self);

        NSSet<CKKSFetchBecause*>* fetchReasons = self.currentFetchReasons ?
            [self.currentFetchReasons setByAddingObject:CKKSFetchBecauseKeyHierarchy] :
            [NSSet setWithObject:CKKSFetchBecauseKeyHierarchy];

        CKKSResultOperation* fetchOp = [self.zoneChangeFetcher requestSuccessfulFetchForManyReasons:fetchReasons];
        CKKSResultOperation* flagOp = [CKKSResultOperation named:@"post-fetch"
                                                       withBlock:^{
            STRONGIFY(self);
            [self.stateMachine handleFlag:CKKSFlagFetchComplete];
        }];
        [flagOp addDependency:fetchOp];
        [self scheduleOperation:flagOp];

        return [OctagonStateTransitionOperation named:@"waiting-for-fetch" entering:SecCKKSZoneKeyStateFetch];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateFetch]) {
        if([flags _onqueueContains:CKKSFlagFetchComplete]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchComplete];
            return [OctagonStateTransitionOperation named:@"fetch-complete" entering:SecCKKSZoneKeyStateFetchComplete];
        }

        // The flags CKKSFlagCloudKitZoneMissing and CKKSFlagChangeTokenOutdated are both handled at the top of this function
        // So, we don't need to handle them here.

        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateNeedFullRefetch]) {
         ckksnotice("ckkskey", self, "Starting a key hierarchy full refetch");

         //TODO use states here instead of flags
         self.keyStateMachineRefetched = true;
         self.keyStateFullRefetchRequested = false;

         return [OctagonStateTransitionOperation named:@"fetch-complete" entering:SecCKKSZoneKeyStateResettingLocalData];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateFetchComplete]) {
        [self.launch addEvent:@"fetch-complete"];
        [self.currentFetchReasons removeAllObjects];

        return [OctagonStateTransitionOperation named:@"post-fetch-process" entering:SecCKKSZoneKeyStateProcess];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateWaitForTLKCreation]) {
        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            ckksnotice("ckkskey", self, "We believe we need to create TLKs but we also received a key nudge; moving to key state Process.");
            return [OctagonStateTransitionOperation named:@"wait-for-tlk-creation-process" entering:SecCKKSZoneKeyStateProcess];

        } else if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchRequested];
            return [OctagonStateTransitionOperation named:@"fetch-requested" entering:SecCKKSZoneKeyStateBeginFetch];

        } else if([flags _onqueueContains:CKKSFlagTLKCreationRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagTLKCreationRequested];

            // It's very likely that we're already untrusted at this point. But, sometimes we will be trusted right now, and can lose trust while waiting for the upload.
            // This probably should be handled by a state increase.
            [flags _onqueueRemoveFlag:CKKSFlagEndTrustedOperation];

            ckksnotice("ckkskey", self, "TLK creation requested; kicking off operation");
            return [[CKKSNewTLKOperation alloc] initWithDependencies:self.operationDependencies
                                                                ckks:self];
        } else if(self.lastNewTLKOperation.keyset) {
            // This means that we _have_ created new TLKs, and should wait for them to be uploaded. This is ugly and should probably be done with more states.
            return [OctagonStateTransitionOperation named:@"" entering:SecCKKSZoneKeyStateWaitForTLKUpload];

        } else {
            ckksnotice("ckkskey", self, "We believe we need to create TLKs; waiting for Octagon (via %@)", self.suggestTLKUpload);
            [self.suggestTLKUpload trigger];
        }
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateWaitForTLKUpload]) {
        ckksnotice("ckkskey", self, "We believe we have TLKs that need uploading");

        if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            ckksnotice("ckkskey", self, "Received a nudge to refetch CKKS");
            return [OctagonStateTransitionOperation named:@"tlk-upload-refetch" entering:SecCKKSZoneKeyStateBeginFetch];
        }

        if([flags _onqueueContains:CKKSFlagKeyStateTLKsUploaded]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateTLKsUploaded];

            return [OctagonStateTransitionOperation named:@"wait-for-tlk-upload-process" entering:SecCKKSZoneKeyStateProcess];
        }

        if([flags _onqueueContains:CKKSFlagEndTrustedOperation]) {
            [flags _onqueueRemoveFlag:CKKSFlagEndTrustedOperation];

            return [OctagonStateTransitionOperation named:@"trust-loss" entering:SecCKKSZoneKeyStateLoseTrust];
        }

        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            return [OctagonStateTransitionOperation named:@"wait-for-tlk-fetch-process" entering:SecCKKSZoneKeyStateProcess];
        }

        // This is quite the hack, but it'll do for now.
        [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
        [self.operationDependencies provideKeySet:self.lastNewTLKOperation.keyset];

        ckksnotice("ckkskey", self, "Notifying Octagon again, just in case");
        [self.suggestTLKUpload trigger];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateTLKMissing]) {
        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            return [OctagonStateTransitionOperation named:@"wait-for-tlk-process" entering:SecCKKSZoneKeyStateProcess];
        }

        return [self tlkMissingOperation:SecCKKSZoneKeyStateWaitForTLK];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateWaitForTLK]) {
        // We're in a hold state: waiting for the TLK bytes to arrive.

        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            // Someone has requsted a reprocess! Go to the correct state.
            ckksnotice("ckkskey", self, "Received a nudge that our TLK might be here! Reprocessing.");
            return [OctagonStateTransitionOperation named:@"wait-for-tlk-process" entering:SecCKKSZoneKeyStateProcess];

        } else if([flags _onqueueContains:CKKSFlagTrustedPeersSetChanged]) {
            [flags _onqueueRemoveFlag:CKKSFlagTrustedPeersSetChanged];

            // Hmm, maybe this trust set change will cause us to recover this TLK (due to a previously-untrusted share becoming trusted). Worth a shot!
            ckksnotice("ckkskey", self, "Received a nudge that the trusted peers set might have changed! Reprocessing.");
            return [OctagonStateTransitionOperation named:@"wait-for-tlk-peers" entering:SecCKKSZoneKeyStateProcess];
        }

        if([flags _onqueueContains:CKKSFlagKeySetRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
            return [OctagonStateTransitionOperation named:@"provide-key-set" entering:SecCKKSZoneKeyStateProcess];
        }

        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateWaitForUnlock]) {
        ckksnotice("ckkskey", self, "Requested to enter waitforunlock");

        if([flags _onqueueContains:CKKSFlagDeviceUnlocked ]) {
            [flags _onqueueRemoveFlag:CKKSFlagDeviceUnlocked];
            return [OctagonStateTransitionOperation named:@"key-state-after-unlock" entering:SecCKKSZoneKeyStateInitialized];
        }

        OctagonPendingFlag* unlocked = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagDeviceUnlocked
                                                                     conditions:OctagonPendingConditionsDeviceUnlocked];
        [pendingFlagHandler _onqueueHandlePendingFlag:unlocked];

        return nil;
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateBadCurrentPointers]) {
        // The current key pointers are broken, but we're not sure why.
        ckksnotice("ckkskey", self, "Our current key pointers are reported broken. Attempting a fix!");
        return [[CKKSHealKeyHierarchyOperation alloc] initWithDependencies:self.operationDependencies
                                                                      ckks:self
                                                                 intending:SecCKKSZoneKeyStateBecomeReady
                                                                errorState:SecCKKSZoneKeyStateError];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateNewTLKsFailed]) {
        ckksnotice("ckkskey", self, "Creating new TLKs didn't work. Attempting to refetch!");
        return [OctagonStateTransitionOperation named:@"new-tlks-failed" entering:SecCKKSZoneKeyStateBeginFetch];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateHealTLKSharesFailed]) {
        ckksnotice("ckkskey", self, "Creating new TLK shares didn't work. Attempting to refetch!");
        return [OctagonStateTransitionOperation named:@"heal-tlks-failed" entering:SecCKKSZoneKeyStateBeginFetch];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateUnhealthy]) {
        if(self.trustStatus != CKKSAccountStatusAvailable) {
            ckksnotice("ckkskey", self, "Looks like the key hierarchy is unhealthy, but we're untrusted.");
            return [OctagonStateTransitionOperation named:@"unhealthy-lacking-trust" entering:SecCKKSZoneKeyStateLoseTrust];

        } else {
            ckksnotice("ckkskey", self, "Looks like the key hierarchy is unhealthy. Launching fix.");
            return [[CKKSHealKeyHierarchyOperation alloc] initWithDependencies:self.operationDependencies
                                                                          ckks:self
                                                                     intending:SecCKKSZoneKeyStateBecomeReady
                                                                    errorState:SecCKKSZoneKeyStateError];
        }
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateHealTLKShares]) {
        ckksnotice("ckksshare", self, "Key hierarchy is okay, but not shared appropriately. Launching fix.");
        return [[CKKSHealTLKSharesOperation alloc] initWithOperationDependencies:self.operationDependencies
                                                                            ckks:self];
    }

    if([currentState isEqualToString:SecCKKSZoneKeyStateProcess]) {
        [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];

        ckksnotice("ckksshare", self, "Launching key state process");
        return [[CKKSProcessReceivedKeysOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:SecCKKSZoneKeyStateBecomeReady
                                                                   errorState:SecCKKSZoneKeyStateError];
    }

    return nil;
}

- (OctagonStateTransitionOperation*)tlkMissingOperation:(CKKSZoneKeyState*)newState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"tlk-missing"
                                        intending:newState
                                       errorState:SecCKKSZoneKeyStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);

        NSArray<CKKSPeerProviderState*>* trustStates = self.operationDependencies.currentTrustStates;

        [self.operationDependencies.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
            CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:self.zoneID];

            if(keyset.error) {
                ckkserror("ckkskey", self, "Unable to load keyset: %@", keyset.error);
                op.nextState = newState;

                [self.operationDependencies provideKeySet:keyset];
                return;
            }

            if(!keyset.currentTLKPointer.currentKeyUUID) {
                // In this case, there's no current TLK at all. Go into "wait for tlkcreation";
                op.nextState = SecCKKSZoneKeyStateWaitForTLKCreation;
                [self.operationDependencies provideKeySet:keyset];
                return;
            }

            if(self.trustStatus != CKKSAccountStatusAvailable) {
                ckksnotice("ckkskey", self, "TLK is missing, but no trust is present.");
                op.nextState = SecCKKSZoneKeyStateLoseTrust;

                [self.operationDependencies provideKeySet:keyset];
                return;
            }

            bool otherDevicesPresent = [self _onqueueOtherDevicesReportHavingTLKs:keyset
                                                                      trustStates:trustStates];
            if(otherDevicesPresent) {
                // We expect this keyset to continue to exist. Send it to our listeners.
                [self.operationDependencies provideKeySet:keyset];

                op.nextState = newState;
            } else {
                ckksnotice("ckkskey", self, "No other devices claim to have the TLK. Resetting zone...");
                op.nextState = SecCKKSZoneKeyStateResettingZone;
            }
            return;
        }];
    }];
}

- (bool)_onqueueOtherDevicesReportHavingTLKs:(CKKSCurrentKeySet*)keyset
                                 trustStates:(NSArray<CKKSPeerProviderState*>*)trustStates
{
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
    for(CKKSPeerProviderState* trustState in trustStates) {
        for(id<CKKSPeer> peer in trustState.currentTrustedPeers) {
            [trustedPeerIDs addObject:peer.peerID];
        }
    }

    NSError* localerror = nil;

    NSArray<CKKSDeviceStateEntry*>* allDeviceStates = [CKKSDeviceStateEntry allInZone:keyset.currentTLKPointer.zoneID error:&localerror];
    if(localerror) {
        ckkserror("ckkskey", self, "Error fetching device states: %@", localerror);
        localerror = nil;
        return true;
    }
    for(CKKSDeviceStateEntry* device in allDeviceStates) {
        // The peerIDs in CDSEs aren't written with the peer prefix. Make sure we match both.
        NSString* sosPeerID = device.circlePeerID ? [CKKSSOSPeerPrefix stringByAppendingString:device.circlePeerID] : nil;

        if([trustedPeerIDs containsObject:device.circlePeerID] ||
           [trustedPeerIDs containsObject:sosPeerID] ||
           [trustedPeerIDs containsObject:device.octagonPeerID]) {
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

    NSArray<CKKSTLKShareRecord*>* tlkShares = [CKKSTLKShareRecord allForUUID:keyset.currentTLKPointer.currentKeyUUID
                                                                      zoneID:keyset.currentTLKPointer.zoneID
                                                                       error:&localerror];
    if(localerror) {
        ckkserror("ckkskey", self, "Error fetching device states: %@", localerror);
        localerror = nil;
        return false;
    }

    for(CKKSTLKShareRecord* tlkShare in tlkShares) {
        if([trustedPeerIDs containsObject:tlkShare.senderPeerID] &&
           [tlkShare.storedCKRecord.modificationDate compare:trustedDeadline] == NSOrderedDescending) {
            ckksnotice("ckkskey", self, "Trusted TLK Share (%@) created recently; other devices have keys and should send them to us", tlkShare);
            return true;
        }
    }

    // Okay, how about the untrusted deadline?
    for(CKKSTLKShareRecord* tlkShare in tlkShares) {
        if([tlkShare.storedCKRecord.modificationDate compare:untrustedDeadline] == NSOrderedDescending) {
            ckksnotice("ckkskey", self, "Untrusted TLK Share (%@) created very recently; other devices might have keys and should rejoin the circle (and send them to us)", tlkShare);
            return true;
        }
    }

    return false;
}

- (void)handleKeychainEventDbConnection:(SecDbConnectionRef) dbconn
                                 source:(SecDbTransactionSource)txionSource
                                  added:(SecDbItemRef) added
                                deleted:(SecDbItemRef) deleted
                            rateLimiter:(CKKSRateLimiter*) rateLimiter
{
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

    bool isTombstoneModification = addedTombstone && deletedTombstone;
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

    if(isTombstoneModification) {
        ckksnotice("ckks", self, "skipping syncing update of tombstone item (%d, %d)", addedTombstone, deletedTombstone);
        return;
    }

    // It's possible to ask for an item to be deleted without adding a corresponding tombstone.
    // This is arguably a bug, as it generates an out-of-sync state, but it is in the API contract.
    // CKKS should ignore these, but log very upset messages.
    if(isDelete && !addedTombstone) {
        ckksnotice("ckks", self, "Client has asked for an item deletion to not sync. Keychain is now out of sync with account");
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

    if(txionSource == kSecDbSOSTransaction) {
        NSString* addedUUID = (__bridge NSString*)SecDbItemGetValue(added, &v10itemuuid, NULL);
        ckksnotice("ckks", self, "Received an incoming %@ from SOS (%@)",
                   isAdd ? @"addition" : (isModify ? @"modification" : @"deletion"),
                   addedUUID);
    }

    // Our caller gave us a database connection. We must get on the local queue to ensure atomicity
    // Note that we're at the mercy of the surrounding db transaction, so don't try to rollback here
    [self dispatchSyncWithConnection:dbconn
                      readWriteTxion:YES
                               block:^CKKSDatabaseTransactionResult {
        // Schedule a "view changed" notification
        [self.viewState.notifyViewChangedScheduler trigger];

        if(self.accountStatus == CKKSAccountStatusNoAccount) {
            // No account; CKKS shouldn't attempt anything.
            [self.stateMachine _onqueueHandleFlag:CKKSFlagScanLocalItems];
            ckksnotice("ckks", self, "Dropping sync item modification due to CK account state; will scan to find changes later");

            // We're positively not logged into CloudKit, and therefore don't expect this item to be synced anytime particularly soon.
            NSString* uuid = (__bridge NSString*)SecDbItemGetValue(added ? added : deleted, &v10itemuuid, NULL);

            SecBoolNSErrorCallback syncCallback = [[CKKSViewManager manager] claimCallbackForUUID:uuid];
            if(syncCallback) {
                [CKKSViewManager callSyncCallbackWithErrorNoAccount: syncCallback];
            }

            return CKKSDatabaseTransactionCommit;
        }

        CKKSMemoryKeyCache* keyCache = [[CKKSMemoryKeyCache alloc] init];

        CKKSOutgoingQueueEntry* oqe = nil;
        if       (isAdd) {
            oqe = [CKKSOutgoingQueueEntry withItem: added   action: SecCKKSActionAdd    zoneID:self.zoneID keyCache:keyCache error: &error];
        } else if(isDelete) {
            oqe = [CKKSOutgoingQueueEntry withItem: deleted action: SecCKKSActionDelete zoneID:self.zoneID keyCache:keyCache error: &error];
        } else if(isModify) {
            oqe = [CKKSOutgoingQueueEntry withItem: added   action: SecCKKSActionModify zoneID:self.zoneID keyCache:keyCache error: &error];
        } else {
            ckkserror("ckks", self, "processKeychainEventItemAdded given garbage: %@ %@", added, deleted);
            return CKKSDatabaseTransactionCommit;
        }

        if(!self.itemSyncingEnabled) {
            // Call any callback now; they're not likely to get the sync they wanted
            SecBoolNSErrorCallback syncCallback = [[CKKSViewManager manager] claimCallbackForUUID:oqe.uuid];
            if(syncCallback) {
                syncCallback(false, [NSError errorWithDomain:CKKSErrorDomain
                                                        code:CKKSErrorViewIsPaused
                                                 description:@"View is paused; item is not expected to sync"]);
            }
        }

        CKOperationGroup* operationGroup = txionSource == kSecDbSOSTransaction
            ? [CKOperationGroup CKKSGroupWithName:@"sos-incoming-item"]
            : [CKOperationGroup CKKSGroupWithName:@"keychain-api-use"];

        if(error) {
            ckkserror("ckks", self, "Couldn't create outgoing queue entry: %@", error);
            [self.stateMachine _onqueueHandleFlag:CKKSFlagScanLocalItems];

            // If the problem is 'couldn't load key', tell the key hierarchy state machine to fix it
            if([error.domain isEqualToString:CKKSErrorDomain] && error.code == errSecItemNotFound) {
                [self.stateMachine _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];
            }

            return CKKSDatabaseTransactionCommit;
        } else if(!oqe) {
            ckkserror("ckks", self, "Decided that no operation needs to occur for %@", error);
            return CKKSDatabaseTransactionCommit;
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
            return CKKSDatabaseTransactionCommit;
        } else {
            ckksnotice("ckks", self, "Saved %@ to outgoing queue", oqe);
        }

        // This update supercedes all other local modifications to this item (_except_ those in-flight).
        // Delete all items in reencrypt or error.
        NSArray<CKKSOutgoingQueueEntry*>* siblings = [CKKSOutgoingQueueEntry allWithUUID:oqe.uuid
                                                                                  states:@[SecCKKSStateReencrypt, SecCKKSStateError]
                                                                                  zoneID:self.zoneID
                                                                                   error:&error];
        if(error) {
            ckkserror("ckks", self, "Couldn't load OQE siblings for %@: %@", oqe, error);
        }

        for(CKKSOutgoingQueueEntry* oqeSibling in siblings) {
            NSError* deletionError = nil;
            [oqeSibling deleteFromDatabase:&deletionError];
            if(deletionError) {
                ckkserror("ckks", self, "Couldn't delete OQE sibling(%@) for %@: %@", oqeSibling, oqe.uuid, deletionError);
            }
        }

        // This update also supercedes any remote changes that are pending.
        NSError* iqeError = nil;
        CKKSIncomingQueueEntry* iqe = [CKKSIncomingQueueEntry tryFromDatabase:oqe.uuid zoneID:self.zoneID error:&iqeError];
        if(iqeError) {
            ckkserror("ckks", self, "Couldn't find IQE matching %@: %@", oqe.uuid, error);
        } else if(iqe) {
            [iqe deleteFromDatabase:&iqeError];
            if(iqeError) {
                ckkserror("ckks", self, "Couldn't delete IQE matching %@: %@", oqe.uuid, error);
            } else {
                ckksnotice("ckks", self, "Deleted IQE matching changed item %@", oqe.uuid);
            }
        }

        [self processOutgoingQueue:operationGroup];

        return CKKSDatabaseTransactionCommit;
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

    WEAKIFY(self);
    CKKSResultOperation* returnCallback = [CKKSResultOperation operationWithBlock:^{
        STRONGIFY(self);

        if(ucipo.error) {
            ckkserror("ckkscurrent", self, "Failed setting a current item pointer for %@ with %@", ucipo.currentPointerIdentifier, ucipo.error);
        } else {
            ckksnotice("ckkscurrent", self, "Finished setting a current item pointer for %@", ucipo.currentPointerIdentifier);
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

    WEAKIFY(self);
    CKKSResultOperation* getCurrentItem = [CKKSResultOperation named:@"get-current-item-pointer" withBlock:^{
        if(fetchAndProcess.error) {
            ckksnotice("ckkscurrent", self, "Rejecting current item pointer get since fetch failed: %@", fetchAndProcess.error);
            complete(NULL, fetchAndProcess.error);
            return;
        }

        STRONGIFY(self);

        [self dispatchSyncWithReadOnlySQLTransaction:^{
            NSError* error = nil;
            NSString* currentIdentifier = [NSString stringWithFormat:@"%@-%@", accessGroup, identifier];

            CKKSCurrentItemPointer* cip = [CKKSCurrentItemPointer fromDatabase:currentIdentifier
                                                                         state:SecCKKSProcessedStateLocal
                                                                        zoneID:self.zoneID
                                                                         error:&error];
            if(!cip || error) {
                if([error.domain isEqualToString:@"securityd"] && error.code == errSecItemNotFound) {
                    // This error is common and very, very noisy. Shorten it and don't log here (the framework should log for us)
                    ckksinfo("ckkscurrent", self, "No current item pointer for %@", currentIdentifier);
                    error = [NSError errorWithDomain:@"securityd" code:errSecItemNotFound description:[NSString stringWithFormat:@"No current item pointer found for %@", currentIdentifier]];
                } else {
                    ckkserror("ckkscurrent", self, "No current item pointer for %@", currentIdentifier);
                }
                complete(nil, error);
                return;
            }

            if(!cip.currentItemUUID) {
                ckkserror("ckkscurrent", self, "Current item pointer is empty %@", cip);
                complete(nil, [NSError errorWithDomain:CKKSErrorDomain
                                                  code:errSecInternalError
                                           description:@"Current item pointer is empty"]);
                return;
            }

            ckksinfo("ckkscurrent", self, "Retrieved current item pointer: %@", cip);
            complete(cip.currentItemUUID, NULL);
            return;
        }];
    }];

    [getCurrentItem addNullableDependency:fetchAndProcess];
    [self scheduleOperation: getCurrentItem];
}

- (CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*)findKeySet:(BOOL)refetchBeforeReturningKeySet
{
    __block CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = nil;

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        keysetOp = (CKKSProvideKeySetOperation*)[self findFirstPendingOperation:self.operationDependencies.keysetProviderOperations];
        if(!keysetOp) {
            keysetOp = [[CKKSProvideKeySetOperation alloc] initWithZoneName:self.zoneName];
            [self.operationDependencies.keysetProviderOperations addObject:keysetOp];

            // This is an abuse of operations: they should generally run when added to a queue, not wait, but this allows recipients to set timeouts
            [self scheduleOperationWithoutDependencies:keysetOp];
        }

        if(refetchBeforeReturningKeySet) {
            ckksnotice("ckks", self, "Refetch requested before returning key set!");

            [self.stateMachine _onqueueHandleFlag:CKKSFlagFetchRequested];
            [self.stateMachine _onqueueHandleFlag:CKKSFlagTLKCreationRequested];
            [self.stateMachine _onqueueHandleFlag:CKKSFlagKeySetRequested];

            return;
        }

        CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:self.zoneID];
        if(keyset.currentTLKPointer.currentKeyUUID &&
           (keyset.tlk.uuid ||
            [self.stateMachine.currentState isEqualToString:SecCKKSZoneKeyStateWaitForTrust] ||
            [self.stateMachine.currentState isEqualToString:SecCKKSZoneKeyStateWaitForTLK])) {
            ckksnotice("ckks", self, "Already have keyset %@", keyset);

            [keysetOp provideKeySet:keyset];
            return;

        } else {
            // The key state machine will know what to do.
            [self.stateMachine _onqueueHandleFlag:CKKSFlagTLKCreationRequested];
            [self.stateMachine _onqueueHandleFlag:CKKSFlagKeySetRequested];
        };
    }];

    return keysetOp;
}

- (void)receiveTLKUploadRecords:(NSArray<CKRecord*>*)records
{
    // First, filter for records matching this zone
    NSMutableArray<CKRecord*>* zoneRecords = [NSMutableArray array];
    for(CKRecord* record in records) {
        if([record.recordID.zoneID isEqual:self.zoneID]) {
            [zoneRecords addObject:record];
        }
    }

    ckksnotice("ckkskey", self, "Received a set of %lu TLK upload records", (unsigned long)zoneRecords.count);

    if(!zoneRecords || zoneRecords.count == 0) {
        return;
    }

    [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        for(CKRecord* record in zoneRecords) {
            [self _onqueueCKRecordChanged:record resync:false];
        }

        [self.stateMachine _onqueueHandleFlag:CKKSFlagKeyStateTLKsUploaded];

        return CKKSDatabaseTransactionCommit;
    }];
}

- (BOOL)requiresTLKUpload
{
    __block BOOL requiresUpload = NO;
    dispatch_sync(self.queue, ^{
        // We want to return true only if we're in a state that immediately requires an upload.
        if(([self.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLKUpload] ||
            [self.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLKCreation])) {
            requiresUpload = YES;
        }
    });

    return requiresUpload;
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

- (CKKSOutgoingQueueOperation*)processOutgoingQueue:(CKOperationGroup* _Nullable)ckoperationGroup {
    return [self processOutgoingQueueAfter:nil ckoperationGroup:ckoperationGroup];
}

- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation* _Nullable)after
                                        ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup {
    return [self processOutgoingQueueAfter:after requiredDelay:DISPATCH_TIME_FOREVER ckoperationGroup:ckoperationGroup];
}

- (CKKSOutgoingQueueOperation*)processOutgoingQueueAfter:(CKKSResultOperation* _Nullable)after
                                           requiredDelay:(uint64_t)requiredDelay
                                        ckoperationGroup:(CKOperationGroup* _Nullable)ckoperationGroup
{
    if(ckoperationGroup) {
        if(self.operationDependencies.currentOutgoingQueueOperationGroup) {
            ckkserror("ckks", self, "Throwing away CKOperationGroup(%@) in favor of (%@)", ckoperationGroup.name, self.operationDependencies.ckoperationGroup.name);
        } else {
            self.operationDependencies.currentOutgoingQueueOperationGroup = ckoperationGroup;
        }
    }

    CKKSOutgoingQueueOperation* outgoingop =
            (CKKSOutgoingQueueOperation*) [self findFirstPendingOperation:self.outgoingQueueOperations
                                                                  ofClass:[CKKSOutgoingQueueOperation class]];
    if(outgoingop) {
        if(after) {
            [outgoingop addDependency: after];
        }
        if([outgoingop isPending]) {
            // Will log any pending dependencies as well
            ckksinfo("ckksoutgoing", self, "Returning existing %@", outgoingop);

            // Shouldn't be necessary, but can't hurt
            [self.outgoingQueueOperationScheduler triggerAt:requiredDelay];
            return outgoingop;
        }
    }

    CKKSOutgoingQueueOperation* op = [[CKKSOutgoingQueueOperation alloc] initWithDependencies:self.operationDependencies
                                                                                    intending:SecCKKSZoneKeyStateReady
                                                                                   errorState:SecCKKSZoneKeyStateUnhealthy];

    [op addNullableDependency:self.holdOutgoingQueueOperation];
    [op addNullableDependency:self.keyStateReadyDependency];
    [op linearDependencies:self.outgoingQueueOperations];

    op.name = @"outgoing-queue-operation";
    [op addNullableDependency:after];
    [op addNullableDependency:self.outgoingQueueOperationScheduler.operationDependency];

    [self.outgoingQueueOperationScheduler triggerAt:requiredDelay];

    [self scheduleOperation: op];
    ckksnotice("ckksoutgoing", self, "Scheduled %@", op);

    WEAKIFY(self);
    CKKSResultOperation* bookkeeping = [CKKSResultOperation named:@"bookkeping-oqo" withBlock:^{
        STRONGIFY(self);
        self.lastOutgoingQueueOperation = op;
    }];

    [bookkeeping addDependency:op];
    [self scheduleOperation:bookkeeping];

    return op;
}

- (void)processIncomingQueueAfterNextUnlock {
    // Thread races aren't so important here; we might end up with two or three copies of this operation, but that's okay.
    if(![self.processIncomingQueueAfterNextUnlockOperation isPending]) {
        WEAKIFY(self);

        CKKSResultOperation* restartIncomingQueueOperation = [CKKSResultOperation operationWithBlock:^{
            STRONGIFY(self);
            // This IQO shouldn't error if the keybag has locked again. It will simply try again later.
            [self processIncomingQueue:false];
        }];

        restartIncomingQueueOperation.name = @"reprocess-incoming-queue-after-unlock";
        self.processIncomingQueueAfterNextUnlockOperation = restartIncomingQueueOperation;

        [restartIncomingQueueOperation addNullableDependency:self.lockStateTracker.unlockDependency];
        [self scheduleOperation: restartIncomingQueueOperation];
    }
}

- (CKKSResultOperation*)resultsOfNextProcessIncomingQueueOperation {
    if(self.resultsOfNextIncomingQueueOperationOperation && [self.resultsOfNextIncomingQueueOperationOperation isPending]) {
        return self.resultsOfNextIncomingQueueOperationOperation;
    }

    // Else, make a new one.
    self.resultsOfNextIncomingQueueOperationOperation = [CKKSResultOperation named:[NSString stringWithFormat:@"wait-for-next-incoming-queue-operation-%@", self.zoneName] withBlock:^{}];
    return self.resultsOfNextIncomingQueueOperationOperation;
}

- (CKKSIncomingQueueOperation*)processIncomingQueue:(bool)failOnClassA {
    return [self processIncomingQueue:failOnClassA after: nil];
}

- (CKKSIncomingQueueOperation*) processIncomingQueue:(bool)failOnClassA after: (CKKSResultOperation*) after {
    return [self processIncomingQueue:failOnClassA after:after policyConsideredAuthoritative:false];
}

- (CKKSIncomingQueueOperation*)processIncomingQueue:(bool)failOnClassA
                                              after:(CKKSResultOperation*)after
                      policyConsideredAuthoritative:(bool)policyConsideredAuthoritative
{
    CKKSIncomingQueueOperation* incomingop = (CKKSIncomingQueueOperation*) [self findFirstPendingOperation:self.incomingQueueOperations];
    if(incomingop) {
        ckksinfo("ckks", self, "Skipping processIncomingQueue due to at least one pending instance");
        if(after) {
            [incomingop addNullableDependency: after];
        }

        // check (again) for race condition; if the op has started we need to add another (for the dependency)
        if([incomingop isPending]) {
            incomingop.errorOnClassAFailure |= failOnClassA;
            incomingop.handleMismatchedViewItems |= policyConsideredAuthoritative;
            return incomingop;
        }
    }

    CKKSIncomingQueueOperation* op = [[CKKSIncomingQueueOperation alloc] initWithDependencies:self.operationDependencies
                                                                                    intending:SecCKKSZoneKeyStateReady
                                                                                   errorState:SecCKKSZoneKeyStateUnhealthy
                                                                         errorOnClassAFailure:failOnClassA
                                                                        handleMismatchedViewItems:policyConsideredAuthoritative];
    op.name = @"incoming-queue-operation";

    // Can't process unless we have a reasonable key hierarchy.
    [op addNullableDependency:self.keyStateReadyDependency];
    [op addNullableDependency:self.holdIncomingQueueOperation];
    [op linearDependencies:self.incomingQueueOperations];

    if(after != nil) {
        [op addSuccessDependency: after];
    }

    if(self.resultsOfNextIncomingQueueOperationOperation) {
        [self.resultsOfNextIncomingQueueOperationOperation addSuccessDependency:op];
        [self scheduleOperation:self.resultsOfNextIncomingQueueOperationOperation];
        self.resultsOfNextIncomingQueueOperationOperation = nil;
    }

    [self scheduleOperation: op];

    WEAKIFY(self);
    CKKSResultOperation* bookkeeping = [CKKSResultOperation named:@"bookkeping-oqo" withBlock:^{
        STRONGIFY(self);
        self.lastIncomingQueueOperation = op;
    }];

    [bookkeeping addDependency:op];
    [self scheduleOperation:bookkeeping];
    return op;
}

- (CKKSScanLocalItemsOperation*)scanLocalItems:(NSString*)operationName {
    return [self scanLocalItems:operationName ckoperationGroup:nil after:nil];
}

- (CKKSScanLocalItemsOperation*)scanLocalItems:(NSString*)operationName
                              ckoperationGroup:(CKOperationGroup*)operationGroup
                                         after:(NSOperation*)after
{
    CKKSScanLocalItemsOperation* scanOperation = (CKKSScanLocalItemsOperation*)[self findFirstPendingOperation:self.scanLocalItemsOperations];

    if(scanOperation) {
        [scanOperation addNullableDependency:after];

        // check (again) for race condition; if the op has started we need to add another (for the dependency)
        if([scanOperation isPending]) {
            if(operationGroup) {
                scanOperation.ckoperationGroup = operationGroup;
            }

            scanOperation.name = [NSString stringWithFormat:@"%@::%@", scanOperation.name, operationName];
            return scanOperation;
        }
    }

    scanOperation = [[CKKSScanLocalItemsOperation alloc] initWithDependencies:self.operationDependencies
                                                                         ckks:self
                                                                    intending:SecCKKSZoneKeyStateReady
                                                                   errorState:SecCKKSZoneKeyStateError
                                                             ckoperationGroup:operationGroup];
    scanOperation.name = operationName;

    [scanOperation addNullableDependency:self.lastFixupOperation];
    [scanOperation addNullableDependency:self.lockStateTracker.unlockDependency];
    [scanOperation addNullableDependency:self.keyStateReadyDependency];
    [scanOperation addNullableDependency:after];

    [scanOperation linearDependencies:self.scanLocalItemsOperations];

    // This might generate items for upload. Make sure that any uploads wait until the scan is complete, so we know what to upload
    [scanOperation linearDependencies:self.outgoingQueueOperations];

    [self scheduleOperation:scanOperation];
    self.initiatedLocalScan = YES;
    return scanOperation;
}

- (CKKSUpdateDeviceStateOperation*)updateDeviceState:(bool)rateLimit
                   waitForKeyHierarchyInitialization:(uint64_t)timeout
                                    ckoperationGroup:(CKOperationGroup*)ckoperationGroup {
    // If securityd just started, the key state might be in some transient early state. Wait a bit.
    OctagonStateMultiStateArrivalWatcher* waitForTransient = [[OctagonStateMultiStateArrivalWatcher alloc] initNamed:@"rpc-watcher"
                                                                                                         serialQueue:self.queue
                                                                                                              states:CKKSKeyStateNonTransientStates()];
    [waitForTransient timeout:timeout];
    [self.stateMachine registerMultiStateArrivalWatcher:waitForTransient];

    CKKSUpdateDeviceStateOperation* op = [[CKKSUpdateDeviceStateOperation alloc] initWithCKKSKeychainView:self rateLimit:rateLimit ckoperationGroup:ckoperationGroup];
    op.name = @"device-state-operation";

    [op addDependency:waitForTransient.result];

    // op modifies the CloudKit zone, so it should insert itself into the list of OutgoingQueueOperations.
    // Then, we won't have simultaneous zone-modifying operations and confuse ourselves.
    // However, since we might have pending OQOs, it should try to insert itself at the beginning of the linearized list
    [op linearDependenciesWithSelfFirst:self.outgoingQueueOperations];

    // CKKSUpdateDeviceStateOperations are special: they should fire even if we don't believe we're in an iCloud account.
    // They also shouldn't block or be blocked by any other operation; our wait operation above will handle that
    [self scheduleOperationWithoutDependencies:op];
    return op;
}

- (void)xpc24HrNotification
{
    // Called roughly once every 24hrs
    [self.stateMachine handleFlag:CKKSFlag24hrNotification];
}

// There are some errors which won't be reported but will be reflected in the CDSE; any error coming out of here is fatal
- (CKKSDeviceStateEntry*)_onqueueCurrentDeviceStateEntry: (NSError* __autoreleasing*)error {
    return [CKKSDeviceStateEntry intransactionCreateDeviceStateForView:self.viewState
                                                        accountTracker:self.accountTracker
                                                      lockStateTracker:self.lockStateTracker
                                                                 error:error];
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

- (CKKSResultOperation*)fetchAndProcessCKChanges:(CKKSFetchBecause*)because
{
     if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping fetchAndProcessCKChanges due to disabled CKKS");
        return nil;
    }

     // We fetched some changes; try to process them!
    return [self processIncomingQueue:false after:[self.zoneChangeFetcher requestSuccessfulFetch:because]];
}

- (bool)_onqueueCKWriteFailed:(NSError*)ckerror attemptedRecordsChanged:(NSDictionary<CKRecordID*, CKRecord*>*)savedRecords
{
    return [self.operationDependencies intransactionCKWriteFailed:ckerror attemptedRecordsChanged:savedRecords];
}

- (bool)_onqueueCKRecordChanged:(CKRecord*)record resync:(bool)resync
{
    return [self.operationDependencies intransactionCKRecordChanged:record resync:resync];
}

- (bool)_onqueueCKRecordDeleted:(CKRecordID*)recordID recordType:(NSString*)recordType resync:(bool)resync
{
    return [self.operationDependencies intransactionCKRecordDeleted:recordID recordType:recordType resync:resync];
}

- (bool)_onqueueResetAllInflightOQE:(NSError**)error {
    dispatch_assert_queue(self.queue);
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
            [oqe intransactionMoveToState:SecCKKSStateNew viewState:self.viewState error:&localError];

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

- (bool)dispatchSyncWithConnection:(SecDbConnectionRef _Nonnull)dbconn
                    readWriteTxion:(BOOL)readWriteTxion
                             block:(CKKSDatabaseTransactionResult (^)(void))block
{
    CFErrorRef cferror = NULL;

    // Take the DB transaction, then get on the local queue.
    // In the case of exclusive DB transactions, we don't really _need_ the local queue, but, it's here for future use.

    SecDbTransactionType txtionType = readWriteTxion ? kSecDbExclusiveRemoteCKKSTransactionType : kSecDbNormalTransactionType;
    bool ret = kc_transaction_type(dbconn, txtionType, &cferror, ^bool{
        __block CKKSDatabaseTransactionResult result = CKKSDatabaseTransactionRollback;

        CKKSSQLInTransaction = true;
        if(readWriteTxion) {
            CKKSSQLInWriteTransaction = true;
        }

        dispatch_sync(self.queue, ^{
            result = block();
        });

        if(readWriteTxion) {
            CKKSSQLInWriteTransaction = false;
        }
        CKKSSQLInTransaction = false;
        return result == CKKSDatabaseTransactionCommit;
    });

    if(cferror) {
        ckkserror("ckks", self, "error doing database transaction, major problems ahead: %@", cferror);
    }
    return ret;
}

- (void)dispatchSyncWithSQLTransaction:(CKKSDatabaseTransactionResult (^)(void))block
{
    // important enough to block this thread. Must get a connection first, though!

    // Please don't jetsam us...
    os_transaction_t transaction = os_transaction_create([[NSString stringWithFormat:@"com.apple.securityd.ckks.%@", self.zoneName] UTF8String]);

    CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        return [self dispatchSyncWithConnection:dbt
                                 readWriteTxion:YES
                                          block:block];

    });
    if(cferror) {
        ckkserror("ckks", self, "error getting database connection, major problems ahead: %@", cferror);
    }

    (void)transaction;
}

- (void)dispatchSyncWithReadOnlySQLTransaction:(void (^)(void))block
{
    // Please don't jetsam us...
    os_transaction_t transaction = os_transaction_create([[NSString stringWithFormat:@"com.apple.securityd.ckks.%@", self.zoneName] UTF8String]);

    CFErrorRef cferror = NULL;

    // Note: we are lying to kc_with_dbt here about whether we're read-and-write or read-only.
    // This is because the SOS engine's queue are broken: SOSEngineSetNotifyPhaseBlock attempts
    // to take the SOS engine's queue while a SecDb transaction is still ongoing. But, in
    // SOSEngineCopyPeerConfirmedDigests, SOS takes the engine queue, then calls dsCopyManifestWithViewNameSet()
    // which attempts to get a read-only SecDb connection.
    //
    // The issue manifests when many CKKS read-only transactions are in-flight, and starve out
    // the pool of read-only connections. Then, a deadlock forms.
    //
    // By claiming to be a read-write connection here, we'll contend on the pool of writer threads,
    // and shouldn't starve SOS of its read thread.
    //
    // But, since we pass NO to readWriteTxion, the SQLite transaction will be of type
    // kSecDbNormalTransactionType, which won't block other readers.

    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        return [self dispatchSyncWithConnection:dbt
                                 readWriteTxion:NO
                                          block:^CKKSDatabaseTransactionResult {
            block();
            return CKKSDatabaseTransactionCommit;
        }];

    });
    if(cferror) {
        ckkserror("ckks", self, "error getting database connection, major problems ahead: %@", cferror);
    }

    (void)transaction;
}

- (BOOL)insideSQLTransaction
{
    return CKKSSQLInTransaction;
}

#pragma mark - CKKSZone operations

- (void)beginCloudKitOperation
{
    [self.accountTracker registerForNotificationsOfCloudKitAccountStatusChange:self];
}

- (CKKSResultOperation*)createAccountLoggedInDependency:(NSString*)message
{
    WEAKIFY(self);
    CKKSResultOperation* accountLoggedInDependency = [CKKSResultOperation named:@"account-logged-in-dependency" withBlock:^{
        STRONGIFY(self);
        ckksnotice("ckkszone", self, "%@", message);
    }];
    accountLoggedInDependency.descriptionErrorCode = CKKSResultDescriptionPendingAccountLoggedIn;
    return accountLoggedInDependency;
}

#pragma mark - CKKSZoneUpdateReceiverProtocol

- (CKKSAccountStatus)accountStatusFromCKAccountInfo:(CKAccountInfo*)info
{
    if(!info) {
        return CKKSAccountStatusUnknown;
    }
    if(info.accountStatus == CKAccountStatusAvailable &&
       info.hasValidCredentials) {
        return CKKSAccountStatusAvailable;
    } else {
        return CKKSAccountStatusNoAccount;
    }
}

- (void)cloudkitAccountStateChange:(CKAccountInfo* _Nullable)oldAccountInfo to:(CKAccountInfo*)currentAccountInfo
{
    ckksnotice("ckkszone", self, "%@ Received notification of CloudKit account status change, moving from %@ to %@",
               self.zoneID.zoneName,
               oldAccountInfo,
               currentAccountInfo);

    // Filter for device2device encryption and cloudkit grey mode
    CKKSAccountStatus oldStatus = [self accountStatusFromCKAccountInfo:oldAccountInfo];
    CKKSAccountStatus currentStatus = [self accountStatusFromCKAccountInfo:currentAccountInfo];

    if(oldStatus == currentStatus) {
        ckksnotice("ckkszone", self, "Computed status of new CK account info is same as old status: %@", [CKKSAccountStateTracker stringFromAccountStatus:currentStatus]);
        return;
    }

    switch(currentStatus) {
        case CKKSAccountStatusAvailable: {
            ckksnotice("ckkszone", self, "Logged into iCloud.");
            [self handleCKLogin];

            if(self.accountLoggedInDependency) {
                [self.operationQueue addOperation:self.accountLoggedInDependency];
                self.accountLoggedInDependency = nil;
            };
        }
            break;

        case CKKSAccountStatusNoAccount: {
            ckksnotice("ckkszone", self, "Logging out of iCloud. Shutting down.");

            if(!self.accountLoggedInDependency) {
                self.accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in again."];
            }

            [self handleCKLogout];
        }
            break;

        case CKKSAccountStatusUnknown: {
            // We really don't expect to receive this as a notification, but, okay!
            ckksnotice("ckkszone", self, "Account status has become undetermined. Pausing for %@", self.zoneID.zoneName);

            if(!self.accountLoggedInDependency) {
                self.accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in again."];
            }

            [self handleCKLogout];
        }
            break;
    }
}

- (void)handleCKLogin
{
    ckksnotice("ckks", self, "received a notification of CK login");
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckks", self, "Skipping CloudKit initialization due to disabled CKKS");
        return;
    }

    dispatch_sync(self.queue, ^{
        ckksinfo("ckkszone", self, "received a notification of CK login");

        // Change our condition variables to reflect that we think we're logged in
        self.accountStatus = CKKSAccountStatusAvailable;
        self.loggedOut = [[CKKSCondition alloc] initToChain:self.loggedOut];
        [self.loggedIn fulfill];
    });

    [self.stateMachine handleFlag:CKKSFlagCloudKitLoggedIn];

    [self.accountStateKnown fulfill];
}

- (void)handleCKLogout
{
    dispatch_sync(self.queue, ^{
        ckksinfo("ckkszone", self, "received a notification of CK logout");

        self.accountStatus = CKKSAccountStatusNoAccount;
        self.loggedIn = [[CKKSCondition alloc] initToChain:self.loggedIn];
        [self.loggedOut fulfill];
    });

    [self.stateMachine handleFlag:CKKSFlagCloudKitLoggedOut];

    [self.accountStateKnown fulfill];
}

#pragma mark - Trust operations

- (void)beginTrustedOperation:(NSArray<id<CKKSPeerProvider>>*)peerProviders
             suggestTLKUpload:(CKKSNearFutureScheduler*)suggestTLKUpload
           requestPolicyCheck:(CKKSNearFutureScheduler*)requestPolicyCheck
{
    for(id<CKKSPeerProvider> peerProvider in peerProviders) {
        [peerProvider registerForPeerChangeUpdates:self];
    }

    [self.launch addEvent:@"beginTrusted"];

    dispatch_sync(self.queue, ^{
        ckksnotice("ckkstrust", self, "Beginning trusted operation");
        self.operationDependencies.peerProviders = peerProviders;
        self.operationDependencies.requestPolicyCheck = requestPolicyCheck;

        CKKSAccountStatus oldTrustStatus = self.trustStatus;

        self.suggestTLKUpload = suggestTLKUpload;

        self.trustStatus = CKKSAccountStatusAvailable;
        [self.stateMachine _onqueueHandleFlag:CKKSFlagBeginTrustedOperation];

        // Re-process the key hierarchy, just in case the answer is now different
        [self.stateMachine _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];

        if(oldTrustStatus == CKKSAccountStatusNoAccount) {
            ckksnotice("ckkstrust", self, "Moving from an untrusted status; we need to process incoming queue and scan for any new items");

            [self.stateMachine _onqueueHandleFlag:CKKSFlagProcessIncomingQueue];
            [self.stateMachine _onqueueHandleFlag:CKKSFlagScanLocalItems];
        }
    });
}

- (void)endTrustedOperation
{
    [self.launch addEvent:@"endTrusted"];

    dispatch_sync(self.queue, ^{
        ckksnotice("ckkstrust", self, "Ending trusted operation");

        self.operationDependencies.peerProviders = @[];

        self.suggestTLKUpload = nil;

        self.trustStatus = CKKSAccountStatusNoAccount;
        [self.stateMachine _onqueueHandleFlag:CKKSFlagEndTrustedOperation];
    });
}

- (BOOL)itemSyncingEnabled
{
    if(!self.operationDependencies.syncingPolicy) {
        ckksnotice("ckks", self, "No syncing policy loaded; item syncing is disabled");
        return NO;
    } else {
        return [self.operationDependencies.syncingPolicy isSyncingEnabledForView:self.zoneName];
    }
}

- (void)setCurrentSyncingPolicy:(TPSyncingPolicy*)syncingPolicy policyIsFresh:(BOOL)policyIsFresh
{
    dispatch_sync(self.queue, ^{
        BOOL oldEnabled = [self itemSyncingEnabled];

        self.operationDependencies.syncingPolicy = syncingPolicy;

        BOOL enabled = [self itemSyncingEnabled];
        if(enabled != oldEnabled) {
            ckksnotice("ckks", self, "Syncing for this view is now %@ (policy: %@)", enabled ? @"enabled" : @"paused", self.operationDependencies.syncingPolicy);
        }

        if(enabled) {
            CKKSResultOperation* incomingOp = [self processIncomingQueue:false after:nil policyConsideredAuthoritative:policyIsFresh];
            [self processOutgoingQueueAfter:incomingOp ckoperationGroup:nil];
        }
    });
}

#pragma mark - CKKSChangeFetcherClient

- (BOOL)zoneIsReadyForFetching
{
    __block BOOL ready = NO;

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        ready = (bool)[self _onQueueZoneIsReadyForFetching];
    }];

    return ready;
}

- (BOOL)_onQueueZoneIsReadyForFetching
{
    dispatch_assert_queue(self.queue);
    if(self.accountStatus != CKKSAccountStatusAvailable) {
        ckksnotice("ckksfetch", self, "Not participating in fetch: not logged in");
        return NO;
    }

    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.operationDependencies.zoneID.zoneName];

    if(!ckse.ckzonecreated) {
        ckksnotice("ckksfetch", self, "Not participating in fetch: zone not created yet");
        return NO;
    }
    return YES;
}

- (CKKSCloudKitFetchRequest*)participateInFetch
{
    __block CKKSCloudKitFetchRequest* request = [[CKKSCloudKitFetchRequest alloc] init];

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        if (![self _onQueueZoneIsReadyForFetching]) {
            ckksnotice("ckksfetch", self, "skipping fetch since zones are not ready");
            return;
        }

        request.participateInFetch = true;
        [self.launch addEvent:@"fetch"];

        if([self.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateNeedFullRefetch]) {
            // We want to return a nil change tag (to force a resync)
            ckksnotice("ckksfetch", self, "Beginning refetch");
            request.changeToken = nil;
            request.resync = true;
        } else {
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.zoneName];
            if(!ckse) {
                ckkserror("ckksfetch", self, "couldn't fetch zone change token for %@", self.zoneName);
                return;
            }
            request.changeToken = ckse.changeToken;
        }
    }];

    if (request.changeToken == nil) {
        self.launch.firstLaunch = true;
    }

    return request;
}

- (void)changesFetched:(NSArray<CKRecord*>*)changedRecords
      deletedRecordIDs:(NSArray<CKKSCloudKitDeletion*>*)deletedRecords
        newChangeToken:(CKServerChangeToken*)newChangeToken
            moreComing:(BOOL)moreComing
                resync:(BOOL)resync
{
    [self.launch addEvent:@"changes-fetched"];

    if(changedRecords.count == 0 && deletedRecords.count == 0 && !moreComing && !resync) {
        // Early-exit, so we don't pick up the account keys or kick off an IncomingQueue operation for no changes
        [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            ckksinfo("ckksfetch", self, "No record changes in this fetch");

            NSError* error = nil;
            CKKSZoneStateEntry* state = [CKKSZoneStateEntry state:self.zoneName];
            state.lastFetchTime = [NSDate date]; // The last fetch happened right now!
            state.changeToken = newChangeToken;
            state.moreRecordsInCloudKit = moreComing;
            [state saveToDatabase:&error];
            if(error) {
                ckkserror("ckksfetch", self, "Couldn't save new server change token: %@", error);
            }
            return CKKSDatabaseTransactionCommit;
        }];
        return;
    }

    [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        for (CKRecord* record in changedRecords) {
            [self _onqueueCKRecordChanged:record resync:resync];
        }

        for (CKKSCloudKitDeletion* deletion in deletedRecords) {
            [self _onqueueCKRecordDeleted:deletion.recordID recordType:deletion.recordType resync:resync];
        }

        NSError* error = nil;
        if(resync) {
            // If we're performing a resync, we need to keep track of everything that's actively in
            // CloudKit during the fetch, (so that we can find anything that's on-disk and not in CloudKit).
            // Please note that if, during a resync, the fetch errors, we won't be notified. If a record is in
            // the first refetch but not the second, it'll be added to our set, and the second resync will not
            // delete the record (which is a consistency violation, but only with actively changing records).
            // A third resync should correctly delete that record.

            if(self.resyncRecordsSeen == nil) {
                self.resyncRecordsSeen = [NSMutableSet set];
            }
            for(CKRecord* r in changedRecords) {
                [self.resyncRecordsSeen addObject:r.recordID.recordName];
            }

            // Is there More Coming? If not, self.resyncRecordsSeen contains everything in CloudKit. Inspect for anything extra!
            if(moreComing) {
                ckksnotice("ckksresync", self, "In a resync, but there's More Coming. Waiting to scan for extra items.");

            } else {
                // Scan through all CKMirrorEntries and determine if any exist that CloudKit didn't tell us about
                ckksnotice("ckksresync", self, "Comparing local UUIDs against the CloudKit list");
                NSMutableArray<NSString*>* uuids = [[CKKSMirrorEntry allUUIDs:self.zoneID error:&error] mutableCopy];

                for(NSString* uuid in uuids) {
                    if([self.resyncRecordsSeen containsObject:uuid]) {
                        ckksnotice("ckksresync", self, "UUID %@ is still in CloudKit; carry on.", uuid);
                    } else {
                        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid zoneID:self.zoneID error:&error];
                        if(error != nil) {
                            ckkserror("ckksresync", self, "Couldn't read an item from the database, but it used to be there: %@ %@", uuid, error);
                            continue;
                        }
                        if(!ckme) {
                            ckkserror("ckksresync", self, "Couldn't read ckme(%@) from database; continuing", uuid);
                            continue;
                        }

                        ckkserror("ckksresync", self, "BUG: Local item %@ not found in CloudKit, deleting", uuid);
                        [self _onqueueCKRecordDeleted:ckme.item.storedCKRecord.recordID recordType:ckme.item.storedCKRecord.recordType resync:resync];
                    }
                }

                // Now that we've inspected resyncRecordsSeen, reset it for the next time through
                self.resyncRecordsSeen = nil;
            }
        }

        CKKSZoneStateEntry* state = [CKKSZoneStateEntry state:self.zoneName];
        state.lastFetchTime = [NSDate date]; // The last fetch happened right now!
        state.changeToken = newChangeToken;
        state.moreRecordsInCloudKit = moreComing;
        [state saveToDatabase:&error];
        if(error) {
            ckkserror("ckksfetch", self, "Couldn't save new server change token: %@", error);
        }

        if(!moreComing) {
            // Might as well kick off a IQO!
            [self processIncomingQueue:false];
            ckksnotice("ckksfetch", self, "Beginning incoming processing for %@", self.zoneID);
        }

        ckksnotice("ckksfetch", self, "Finished processing changes for %@", self.zoneID);

        return CKKSDatabaseTransactionCommit;
    }];
}

- (bool)ckErrorOrPartialError:(NSError *)error isError:(CKErrorCode)errorCode
{
    if((error.code == errorCode) && [error.domain isEqualToString:CKErrorDomain]) {
        return true;
    } else if((error.code == CKErrorPartialFailure) && [error.domain isEqualToString:CKErrorDomain]) {
        NSDictionary* partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];

        NSError* partialError = partialErrors[self.zoneID];
        if ((partialError.code == errorCode) && [partialError.domain isEqualToString:CKErrorDomain]) {
            return true;
        }
    }
    return false;
}

- (bool)shouldRetryAfterFetchError:(NSError*)error {

    bool isChangeTokenExpiredError = [self ckErrorOrPartialError:error isError:CKErrorChangeTokenExpired];
    if(isChangeTokenExpiredError) {
        ckkserror("ckks", self, "Received notice that our change token is out of date (for %@). Resetting local data...", self.zoneID);

        [self.stateMachine handleFlag:CKKSFlagChangeTokenExpired];
        return true;
    }

    bool isDeletedZoneError = [self ckErrorOrPartialError:error isError:CKErrorZoneNotFound];
    if(isDeletedZoneError) {
        ckkserror("ckks", self, "Received notice that our zone(%@) does not exist. Resetting local data.", self.zoneID);

        [self.stateMachine handleFlag:CKKSFlagCloudKitZoneMissing];
        return false;
    }

    if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorBadContainer)) {
        ckkserror("ckks", self, "Received notice that our container does not exist. Nothing to do.");
        return false;
    }

    return true;
}

#pragma mark CKKSPeerUpdateListener

- (void)selfPeerChanged:(id<CKKSPeerProvider>)provider
{
    // Currently, we have no idea what to do with this. Kick off a key reprocess?
    ckkserror("ckks", self, "Received update that our self identity has changed");
    [self keyStateMachineRequestProcess];
}

- (void)trustedPeerSetChanged:(id<CKKSPeerProvider>)provider
{
    // We might need to share the TLK to some new people, or we might now trust the TLKs we have.
    // The key state machine should handle that, so poke it.
    ckkserror("ckks", self, "Received update that the trust set has changed");

    [self.stateMachine handleFlag:CKKSFlagTrustedPeersSetChanged];
}

#pragma mark - Test Support

- (bool) outgoingQueueEmpty: (NSError * __autoreleasing *) error {
    __block bool ret = false;
    [self dispatchSyncWithReadOnlySQLTransaction:^{
        NSArray* queueEntries = [CKKSOutgoingQueueEntry all: error];
        ret = queueEntries && ([queueEntries count] == 0);
    }];

    return ret;
}

- (void)waitForFetchAndIncomingQueueProcessing
{
    [[self.zoneChangeFetcher inflightFetch] waitUntilFinished];
    [self waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
}

- (void)waitForKeyHierarchyReadiness {
    if(self.keyStateReadyDependency) {
        [self.keyStateReadyDependency waitUntilFinished];
    }
}

#pragma mark - NSOperation assistance

- (void)scheduleOperation:(NSOperation*)op
{
    if(self.halted) {
        ckkserror("ckkszone", self, "attempted to schedule an operation on a halted zone, ignoring");
        return;
    }

    [op addNullableDependency:self.accountLoggedInDependency];
    [self.operationQueue addOperation: op];
}

// to be used rarely, if at all
- (bool)scheduleOperationWithoutDependencies:(NSOperation*)op
{
    if(self.halted) {
        ckkserror("ckkszone", self, "attempted to schedule an non-dependent operation on a halted zone, ignoring");
        return false;
    }

    [self.operationQueue addOperation: op];
    return true;
}

- (void)waitUntilAllOperationsAreFinished
{
    [self.operationQueue waitUntilAllOperationsAreFinished];
}

- (void)waitForOperationsOfClass:(Class)operationClass
{
    NSArray* operations = [self.operationQueue.operations copy];
    for(NSOperation* op in operations) {
        if([op isKindOfClass:operationClass]) {
            [op waitUntilFinished];
        }
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

    @synchronized(self.scanLocalItemsOperations) {
        for(NSOperation* op in self.scanLocalItemsOperations) {
            [op cancel];
        }
        [self.scanLocalItemsOperations removeAllObjects];
    }
}

- (void)cancelAllOperations {
    [self.keyStateReadyDependency cancel];
    // We don't own the zoneChangeFetcher, so don't cancel it
    [self.viewState.notifyViewChangedScheduler cancel];

    [self cancelPendingOperations];
    [self.operationQueue cancelAllOperations];
}

- (void)halt {
    [self.stateMachine haltOperation];

    // Synchronously set the 'halted' bit
    dispatch_sync(self.queue, ^{
        self.halted = true;
    });

    // Bring all operations down, too
    [self cancelAllOperations];

    // And now, wait for all operations that are running
    for(NSOperation* op in self.operationQueue.operations) {
        if(op.isExecuting) {
            [op waitUntilFinished];
        }
    }

    // Don't send any more notifications, either
    [self.viewState.notifyViewChangedScheduler cancel];
    [self.viewState.notifyViewReadyScheduler cancel];
}

- (NSDictionary*)status {
#define stringify(obj) CKKSNilToNSNull([obj description])
#define boolstr(obj) (!!(obj) ? @"yes" : @"no")
    __block NSMutableDictionary* ret = nil;
    __block NSError* error = nil;

    ret = [[self fastStatus] mutableCopy];

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:self.zoneID];
        if(keyset.error) {
            error = keyset.error;
        }

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

        NSArray* tlkShares = [CKKSTLKShareRecord allForUUID:keyset.currentTLKPointer.currentKeyUUID zoneID:self.zoneID error:&error];
        NSMutableArray<NSString*>* mutTLKShares = [[NSMutableArray alloc] init];
        [tlkShares enumerateObjectsUsingBlock:^(id  _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [mutTLKShares addObject: [obj description]];
        }];

        [ret addEntriesFromDictionary:@{
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
                 @"itemsyncing":         self.itemSyncingEnabled ? @"enabled" : @"paused",
            }];
    }];
    return ret;
}

- (NSDictionary*)fastStatus {

    __block NSDictionary* ret = nil;

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.zoneName];

        ret = @{
            @"view":                CKKSNilToNSNull(self.zoneName),
            @"ckaccountstatus":     self.accountStatus == CKAccountStatusCouldNotDetermine ? @"could not determine" :
                self.accountStatus == CKAccountStatusAvailable         ? @"logged in" :
                self.accountStatus == CKAccountStatusRestricted        ? @"restricted" :
                self.accountStatus == CKAccountStatusNoAccount         ? @"logged out" : @"unknown",
            @"accounttracker":      stringify(self.accountTracker),
            @"fetcher":             stringify(self.zoneChangeFetcher),
            @"zoneCreated":         boolstr(ckse.ckzonecreated),
            @"zoneSubscribed":      boolstr(ckse.ckzonesubscribed),
            @"keystate":            CKKSNilToNSNull(self.keyHierarchyState),
            @"statusError":         [NSNull null],
            @"launchSequence":      CKKSNilToNSNull([self.launch eventsByTime]),

            @"lastIncomingQueueOperation":         stringify(self.lastIncomingQueueOperation),
            @"lastNewTLKOperation":                stringify(self.lastNewTLKOperation),
            @"lastOutgoingQueueOperation":         stringify(self.lastOutgoingQueueOperation),
            @"lastProcessReceivedKeysOperation":   stringify(self.lastProcessReceivedKeysOperation),
            @"lastReencryptOutgoingItemsOperation":stringify(self.lastReencryptOutgoingItemsOperation),
        };
    }];

    return ret;
}

#endif /* OCTAGON */
@end
