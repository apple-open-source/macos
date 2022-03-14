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
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSSecDbAdapter.h"
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

#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"
#import <Security/SecItemInternal.h>
#import <Security/CKKSExternalTLKClient.h>

#if OCTAGON

@interface CKKSKeychainView()

@property CKKSCondition* policyLoaded;
@property (nullable) OctagonStateMultiStateArrivalWatcher* priorityViewsProcessed;

@property BOOL itemModificationsBeforePolicyLoaded;

// Slows down all outgoing queue operations
@property CKKSNearFutureScheduler* outgoingQueueOperationScheduler;

@property CKKSResultOperation* resultsOfNextIncomingQueueOperationOperation;

// Please don't use these unless you're an Operation in this package
@property NSHashTable<CKKSOutgoingQueueOperation*>* outgoingQueueOperations;

// Scratch space for resyncs
@property (nullable) NSMutableSet<NSString*>* resyncRecordsSeen;

@property CKKSSecDbAdapter* databaseProvider;

@property NSOperationQueue* operationQueue;
@property CKKSResultOperation* accountLoggedInDependency;

@property BOOL halted;

// Make these readwrite
@property NSArray<CKKSPeerProviderState*>* currentTrustStates;

// For testing
@property (nullable) NSSet<NSString*>* viewAllowList;

@property BOOL havoc;
@end
#endif

@implementation CKKSKeychainView
#if OCTAGON

- (instancetype)initWithContainer:(CKContainer*)container
                   accountTracker:(CKKSAccountStateTracker*)accountTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                    changeFetcher:(CKKSZoneChangeFetcher*)fetcher
                     zoneModifier:(CKKSZoneModifier*)zoneModifier
                 savedTLKNotifier:(CKKSNearFutureScheduler*)savedTLKNotifier
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if((self = [super init])) {
        _container = container;
        _accountTracker = accountTracker;
        _reachabilityTracker = reachabilityTracker;
        _lockStateTracker = lockStateTracker;
        _cloudKitClassDependencies = cloudKitClassDependencies;

        _zoneName = @"all";

        _havoc = NO;
        _halted = NO;

        _accountStatus = CKKSAccountStatusUnknown;
        _accountLoggedInDependency = [self createAccountLoggedInDependency:@"CloudKit account logged in."];

        _queue = dispatch_queue_create([[NSString stringWithFormat:@"CKKSQueue.%@", container.containerIdentifier] UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _operationQueue = [[NSOperationQueue alloc] init];

        _databaseProvider = [[CKKSSecDbAdapter alloc] initWithQueue:_queue];

        _loggedIn = [[CKKSCondition alloc] init];
        _loggedOut = [[CKKSCondition alloc] init];
        _accountStateKnown = [[CKKSCondition alloc] init];

        _initiatedLocalScan = NO;

        _trustStatus = CKKSAccountStatusUnknown;
        _trustStatusKnown = [[CKKSCondition alloc] init];

        _outgoingQueueOperations = [NSHashTable weakObjectsHashTable];

        _currentTrustStates = @[];

        _resyncRecordsSeen = nil;

        _stateMachine = [[OctagonStateMachine alloc] initWithName:@"ckks"
                                                           states:CKKSAllStates()
                                                            flags:CKKSAllStateFlags()
                                                     initialState:CKKSStateWaitForCloudKitAccountStatus
                                                            queue:self.queue
                                                      stateEngine:self
                                                 lockStateTracker:lockStateTracker
                                              reachabilityTracker:reachabilityTracker];

        WEAKIFY(self);

        dispatch_time_t initialOutgoingQueueDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 1;
        dispatch_time_t continuingOutgoingQueueDelay = SecCKKSReduceRateLimiting() ? NSEC_PER_MSEC * 200 : NSEC_PER_SEC * 30;
        _outgoingQueueOperationScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"outgoing-queue-scheduler"]
                                                                            initialDelay:initialOutgoingQueueDelay
                                                                         continuingDelay:continuingOutgoingQueueDelay
                                                                        keepProcessAlive:false
                                                               dependencyDescriptionCode:CKKSResultDescriptionPendingOutgoingQueueScheduling
                                                                                   block:^{
            STRONGIFY(self);
            [self.stateMachine handleFlag:CKKSFlagOutgoingQueueOperationRateToken];
        }];

        _policyLoaded = [[CKKSCondition alloc] init];
        _operationDependencies = [[CKKSOperationDependencies alloc] initWithViewStates:[NSSet set]
                                                                          zoneModifier:zoneModifier
                                                                            ckdatabase:[_container privateCloudDatabase]
                                                             cloudKitClassDependencies:_cloudKitClassDependencies
                                                                      ckoperationGroup:nil
                                                                           flagHandler:_stateMachine
                                                                   accountStateTracker:accountTracker
                                                                      lockStateTracker:_lockStateTracker
                                                                   reachabilityTracker:reachabilityTracker
                                                                         peerProviders:@[]
                                                                     databaseProvider:_databaseProvider
                                                                      savedTLKNotifier:savedTLKNotifier];

        _zoneChangeFetcher = fetcher;

        [_stateMachine startOperation];
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<%@: %@ %@>", NSStringFromClass([self class]),
            self.stateMachine.currentState,
            self.operationDependencies.views];
}

- (NSString*)debugDescription {
    return [NSString stringWithFormat:@"<%@: %@ %@ %p>", NSStringFromClass([self class]),
            self.stateMachine.currentState,
            self.operationDependencies.views,
            self];
}

- (NSDictionary<CKKSState*, CKKSCondition*>*)stateConditions
{
    return self.stateMachine.stateConditions;
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>*)performInitializedOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"ckks-initialized-operation"
                                        intending:CKKSStateBecomeReady
                                       errorState:CKKSStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);
        [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{

            NSMutableArray<CKKSZoneStateEntry*>* ckses = [NSMutableArray array];
            for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneID.zoneName];
                [ckses addObject:ckse];
            }

            // Check if we believe we've synced all zones before.
            bool needInitialFetch = false;
            bool moreComing = false;

            for(CKKSZoneStateEntry* ckse in ckses) {
                if(ckse.changeToken == nil) {
                    needInitialFetch = true;
                }

                if(ckse.moreRecordsInCloudKit) {
                    ckksnotice("ckks", self, "CloudKit reports there's more records to fetch!");
                    moreComing = true;
                }
            }

            if(needInitialFetch) {
                self.operationDependencies.ckoperationGroup = [CKOperationGroup CKKSGroupWithName:@"initial-setup"];

                ckksnotice("ckks", self, "No existing change token; going to try to match local items with CloudKit ones.");

                // Onboard this keychain: there's likely items in it that we haven't synced yet.
                // But, there might be items in The Cloud that correspond to these items, with UUIDs that we don't know yet.
                // First, fetch all remote items.

                [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseInitialStart];
                op.nextState = CKKSStateBeginFetch;

            } else {
                // Likely a restart of securityd!

                CKKSFixup minimumFixup = CKKSCurrentFixupNumber;
                for(CKKSZoneStateEntry* ckse in ckses) {
                    if(ckse.lastFixup < minimumFixup) {
                        minimumFixup = ckse.lastFixup;
                    }
                }

                // Are there any fixups to run first?
                CKKSState* fixup = [CKKSFixups fixupOperation:minimumFixup];
                if(fixup != nil) {
                    ckksnotice("ckksfixup", self, "We have a fixup to perform: %@", self.lastFixupOperation);
                    op.nextState = fixup;
                    return CKKSDatabaseTransactionCommit;
                }

                // First off, are there any in-flight queue entries? If so, put them back into New.
                // If they're truly in-flight, we'll "conflict" with ourselves, but that should be fine.
                NSError* error = nil;
                [self _onqueueResetAllInflightOQE:&error];
                if(error) {
                    ckkserror("ckks", self, "Couldn't reset in-flight OQEs, bad behavior ahead: %@", error);
                }

                self.operationDependencies.ckoperationGroup = [CKOperationGroup CKKSGroupWithName:@"restart-setup"];
                self.operationDependencies.currentOutgoingQueueOperationGroup = [CKOperationGroup CKKSGroupWithName:@"restart-setup"];

                // If we have a known key hierachy, don't do anything. Otherwise, refetch.

                bool needRefetch = false;

                for(CKKSKeychainViewState* viewState in self.operationDependencies.activeManagedViews) {
                    CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];

                    if(keyset.error && !([keyset.error.domain isEqual: @"securityd"] && keyset.error.code == errSecItemNotFound)) {
                        ckkserror("ckkskey", self, "Error examining existing key hierarchy: %@", keyset.error);
                    }
                    if(!(keyset.tlk && keyset.classA && keyset.classC && !keyset.error)) {
                        // We don't have the TLK, or one doesn't exist.
                        ckksnotice("ckkskey", viewState.zoneID, "No existing key hierarchy for %@. Check if there's one in CloudKit...", viewState.zoneID);
                        needRefetch = true;
                    }
                }

                if(needRefetch || moreComing) {
                    if(moreComing) {
                        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseMoreComing];
                    }
                    if(needRefetch) {
                        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecausePeriodicRefetch];
                    }
                    op.nextState = CKKSStateBeginFetch;

                } else {
                    // This is likely a restart of securityd, and we think we're ready. Double check.
                    op.nextState = CKKSStateProcessReceivedKeys;
                }
            }

            return CKKSDatabaseTransactionCommit;
        }];
    }];
}

- (CKKSResultOperation*)rpcResetLocal:(NSSet<NSString*>* _Nullable)viewNames reply:(void(^)(NSError* _Nullable result))reply
{
    WEAKIFY(self);

    ckksnotice("ckksreset", self, "Requesting local data reset");

    OctagonStateTransitionOperation* setZonesOp = [OctagonStateTransitionOperation named:@"set-zones"
                                                                               intending:CKKSStateResettingLocalData
                                                                              errorState:CKKSStateError
                                                                     withBlockTakingSelf:^(OctagonStateTransitionOperation* op) {
        STRONGIFY(self);

        if(viewNames) {
            ckksnotice("ckksreset", self, "Restricting local data reset to a view subset %@", viewNames);
            NSSet<CKKSKeychainViewState*>* viewStates = [self.operationDependencies viewStatesByNames:viewNames];
            [self.operationDependencies operateOnSelectViews:viewStates];
        };

        ckksnotice("ckksreset", self, "Beginning local data reset for %@", self.operationDependencies.views);

        op.nextState = op.intendedState;
    }];

    return [self.stateMachine doWatchedStateMachineRPC:@"local-reset"
                                          sourceStates:[NSSet setWithArray:@[
                                              // TODO: possibly every state?
                                              CKKSStateReady,
                                              CKKSStateInitialized,
                                              CKKSStateFetchComplete,
                                              CKKSStateProcessReceivedKeys,
                                              CKKSStateWaitForTrust,
                                              CKKSStateLoggedOut,
                                              CKKSStateError,
                                          ]]
                                                  path:[OctagonStateTransitionPath pathFromDictionary:@{
                                                      CKKSStateResettingLocalData: @{
                                                              CKKSStateInitializing: @{
                                                                      CKKSStateInitialized: [OctagonStateTransitionPathStep success],
                                                                      CKKSStateLoggedOut: [OctagonStateTransitionPathStep success],
                                                              }
                                                      }
                                                  }]
                                          transitionOp:setZonesOp
                                                 reply:reply];
}

- (CKKSResultOperation*)rpcResetCloudKit:(NSSet<NSString*>* _Nullable)viewNames reply:(void(^)(NSError* _Nullable result))reply
{
    NSError* error = nil;
    if(![self waitUntilReadyForRPCForOperation:@"reset-cloudkit"
                                          fast:NO
                      errorOnNoCloudKitAccount:YES
                          errorOnPolicyMissing:YES
                                         error:&error]) {
        reply(error);
        CKKSResultOperation* failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            op.error = error;
        }];
        [self.operationQueue addOperation:failOp];
        return failOp;
    }

    WEAKIFY(self);

    ckksnotice("ckksreset", self, "Requesting reset of CK zone (logged in)");

    OctagonStateTransitionOperation* setZonesOp = [OctagonStateTransitionOperation named:@"set-zones"
                                                                               intending:CKKSStateResettingZone
                                                                              errorState:CKKSStateError
                                                                     withBlockTakingSelf:^(OctagonStateTransitionOperation* op) {
        STRONGIFY(self);

        if(viewNames) {
            ckksnotice("ckksreset", self, "Restricting cloudkit zone reset to a view subset %@", viewNames);
            NSSet<CKKSKeychainViewState*>* viewStates = [self.operationDependencies viewStatesByNames:viewNames];
            [self.operationDependencies operateOnSelectViews:viewStates];
        } else {
            NSSet<CKKSKeychainViewState*>* viewStates = self.operationDependencies.activeManagedViews;
            ckksnotice("ckksreset", self, "Restricting cloudkit zone reset to active CKKS-managed subset %@", viewStates);
            [self.operationDependencies operateOnSelectViews:viewStates];
        }

        ckksnotice("ckksreset", self, "Beginning cloudkit zone reset for %@", self.operationDependencies.views);

        op.nextState = op.intendedState;
    }];

    NSDictionary* localResetPath = @{
        CKKSStateInitializing: @{
            CKKSStateInitialized: [OctagonStateTransitionPathStep success],
            CKKSStateLoggedOut: [OctagonStateTransitionPathStep success],
        },
    };

    // If the zone delete doesn't work, try it up to two more times

    return [self.stateMachine doWatchedStateMachineRPC:@"ckks-cloud-reset"
                                          sourceStates:[NSSet setWithArray:@[
                                              // TODO: possibly every state?
                                              CKKSStateReady,
                                              CKKSStateInitialized,
                                              CKKSStateFetchComplete,
                                              CKKSStateProcessReceivedKeys,
                                              CKKSStateWaitForTrust,
                                              CKKSStateLoggedOut,
                                              CKKSStateError,
                                          ]]
                                                  path:[OctagonStateTransitionPath pathFromDictionary:@{
                                                      CKKSStateResettingZone: @{
                                                              CKKSStateResettingLocalData: localResetPath,
                                                              CKKSStateResettingZone: @{
                                                                      CKKSStateResettingLocalData: localResetPath,
                                                                      CKKSStateResettingZone: @{
                                                                              CKKSStateResettingLocalData: localResetPath,
                                                                      }
                                                              }
                                                      }
                                                  }]
                                          transitionOp:setZonesOp
                                                 reply:reply];
}

- (void)keyStateMachineRequestProcess {
    [self.stateMachine handleFlag:CKKSFlagKeyStateProcessRequested];
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

        [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateLoggedOut];
        return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                       intendedState:CKKSStateLoggedOut
                                                          errorState:CKKSStateError];
    }

    if([flags _onqueueContains:CKKSFlagCloudKitZoneMissing]) {
        [flags _onqueueRemoveFlag:CKKSFlagCloudKitZoneMissing];

        // The zone state is 'initializing' because we're going to throw away all knowledge of what's in it
        [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateInitializing];

        // The zone is gone! Let's reset our local state, which will feed into recreating the zone
        return [OctagonStateTransitionOperation named:@"ck-zone-missing"
                                             entering:CKKSStateResettingLocalData];
    }

    if([flags _onqueueContains:CKKSFlagChangeTokenExpired]) {
        [flags _onqueueRemoveFlag:CKKSFlagChangeTokenExpired];

        // The zone state is 'initializing' because we're going to throw away all knowledge of what's in t
        [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateInitializing];

        // Our change token is invalid! We'll have to refetch the world, so let's delete everything locally.
        return [OctagonStateTransitionOperation named:@"ck-token-expired"
                                             entering:CKKSStateResettingLocalData];
    }

    if([currentState isEqualToString:CKKSStateLoggedOut]) {
        if([flags _onqueueContains:CKKSFlagCloudKitLoggedIn] || self.accountStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedIn];

            ckksnotice("ckkskey", self, "CloudKit account now present");
            return [OctagonStateTransitionOperation named:@"ck-sign-in"
                                                 entering:CKKSStateInitializing];
        }

        [self.operationDependencies setStateForAllViews:SecCKKSZoneKeyStateLoggedOut];

        if([flags _onqueueContains:CKKSFlag24hrNotification]) {
            [flags _onqueueRemoveFlag:CKKSFlag24hrNotification];
        }
        return nil;
    }

    if([currentState isEqualToString: CKKSStateWaitForCloudKitAccountStatus]) {
        if([flags _onqueueContains:CKKSFlagCloudKitLoggedIn] || self.accountStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedIn];

            ckksnotice("ckkskey", self, "CloudKit account now present");
            return [OctagonStateTransitionOperation named:@"ck-sign-in"
                                                 entering:CKKSStateInitializing];
        }

        if([flags _onqueueContains:CKKSFlagCloudKitLoggedOut]) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedOut];
            ckksnotice("ckkskey", self, "No account available");

            return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:CKKSStateLoggedOut
                                                              errorState:CKKSStateError];
        }
        return nil;
    }

    if([currentState isEqual:CKKSStateInitializing]) {
        if(self.accountStatus == CKKSAccountStatusNoAccount) {
            ckksnotice("ckkskey", self, "CloudKit account is missing. Departing!");
            return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:CKKSStateLoggedOut
                                                              errorState:CKKSStateError];
        }

        // Initializing means resetting to original states.
        [self.operationDependencies operateOnAllViews];
        [self.operationDependencies setStateForAllViews:SecCKKSZoneKeyStateInitializing];

        // Begin zone creation, but rate-limit it
        CKKSCreateCKZoneOperation* pendingInitializeOp = [[CKKSCreateCKZoneOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                   intendedState:CKKSStateInitialized
                                                                                                      errorState:CKKSStateZoneCreationFailed];
        [pendingInitializeOp addNullableDependency:self.operationDependencies.zoneModifier.cloudkitRetryAfter.operationDependency];
        [self.operationDependencies.zoneModifier.cloudkitRetryAfter trigger];

        return pendingInitializeOp;
    }

    if([currentState isEqualToString:CKKSStateInitialized]) {
        // We're initialized and all CloudKit zones are created.
        [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateInitialized];

        // Now that we've created (hopefully) all views, limit our consideration to only priority views
        // But _only_ if we're already trusted. Otherwise, proceed as normal: we'll focus on the priority views once trust arrives
        if([flags _onqueueContains:CKKSFlagNewPriorityViews] && self.trustStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagNewPriorityViews];
            [self _onqueuePrioritizePriorityViews];
        }

        return [self performInitializedOperation];
    }

    // In error? You probably aren't getting out.
    if([currentState isEqualToString:CKKSStateError]) {
        if([flags _onqueueContains:CKKSFlagCloudKitLoggedIn]) {
            [flags _onqueueRemoveFlag:CKKSFlagCloudKitLoggedIn];

            // Worth one last shot. Reset everything locally, and try again.
            return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:CKKSStateInitializing
                                                              errorState:CKKSStateError];
        }

        ckkserror("ckkskey", self, "Staying in error state %@", currentState);
        return nil;
    }

    if([currentState isEqualToString:CKKSStateFixupRefetchCurrentItemPointers]) {
        CKKSResultOperation<OctagonStateTransitionOperationProtocol>* op = [[CKKSFixupRefetchAllCurrentItemPointers alloc] initWithOperationDependencies:self.operationDependencies
                                                                                                                                        ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"fixup"]];
        self.lastFixupOperation = op;
        return op;
    }

    if([currentState isEqualToString:CKKSStateFixupFetchTLKShares]) {
        CKKSResultOperation<OctagonStateTransitionOperationProtocol>* op = [[CKKSFixupFetchAllTLKShares alloc] initWithOperationDependencies:self.operationDependencies
                                                                                                                            ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"fixup"]];
        self.lastFixupOperation = op;
        return op;
    }
    if([currentState isEqualToString:CKKSStateFixupLocalReload]) {
        CKKSResultOperation<OctagonStateTransitionOperationProtocol>* op = [[CKKSFixupLocalReloadOperation alloc] initWithOperationDependencies:self.operationDependencies
                                                                                                                                    fixupNumber:CKKSFixupLocalReload
                                                                                                                               ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"fixup"]
                                                                                                                                       entering:CKKSStateFixupResaveDeviceStateEntries];
        self.lastFixupOperation = op;
        return op;
    }
    if([currentState isEqualToString:CKKSStateFixupResaveDeviceStateEntries]) {
        CKKSResultOperation<OctagonStateTransitionOperationProtocol>* op = [[CKKSFixupResaveDeviceStateEntriesOperation alloc] initWithOperationDependencies:self.operationDependencies];
        self.lastFixupOperation = op;
        return op;
    }
    if([currentState isEqualToString:CKKSStateFixupDeleteAllCKKSTombstones]) {
        CKKSResultOperation<OctagonStateTransitionOperationProtocol>* op = [[CKKSFixupLocalReloadOperation alloc] initWithOperationDependencies:self.operationDependencies
                                                                                                                                    fixupNumber:CKKSFixupDeleteAllCKKSTombstones
                                                                                                                               ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"fixup"]
                                                                                                                                       entering:CKKSStateInitialized];
        self.lastFixupOperation = op;
        return op;
    }

    if([currentState isEqualToString:CKKSStateResettingZone]) {
        ckksnotice("ckkskey", self, "Deleting the CloudKit Zones for %@", self.operationDependencies.views);

        [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateResettingZone];
        return [[CKKSDeleteCKZoneOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:CKKSStateResettingLocalData
                                                            errorState:CKKSStateResettingZone];
    }

    if([currentState isEqualToString:CKKSStateResettingLocalData]) {
        ckksnotice("ckkskey", self, "Resetting local data for %@", self.operationDependencies.views);

        // Probably a layering violation: if we're resetting the local data, also drop any pending keysets we created
        self.lastNewTLKOperation = nil;

        [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateInitializing];
        return [[CKKSLocalResetOperation alloc] initWithDependencies:self.operationDependencies
                                                       intendedState:CKKSStateInitializing
                                                          errorState:CKKSStateError];
    }

    if([currentState isEqualToString:CKKSStateZoneCreationFailed]) {
        //Prepare to go back into initializing, as soon as the cloudkitRetryAfter is happy
        OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"recover-from-cloudkit-failure" entering:CKKSStateInitializing];

        [op addNullableDependency:self.operationDependencies.zoneModifier.cloudkitRetryAfter.operationDependency];
        [self.operationDependencies.zoneModifier.cloudkitRetryAfter trigger];

        return op;
    }

    if([currentState isEqualToString:CKKSStateLoseTrust]) {
        if([flags _onqueueContains:CKKSFlagBeginTrustedOperation]) {
            [flags _onqueueRemoveFlag:CKKSFlagBeginTrustedOperation];
            // This was likely a race between some operation and the beginTrustedOperation call! Skip changing state and try again.
            return [OctagonStateTransitionOperation named:@"begin-trusted-operation" entering:CKKSStateInitialized];
        }

        return [self loseTrustOperation:CKKSStateWaitForTrust];
    }

    if([currentState isEqualToString:CKKSStateWaitForTrust]) {
        if(self.trustStatus == CKKSAccountStatusAvailable) {
            ckksnotice("ckkskey", self, "Beginning trusted state machine operation");
            return [OctagonStateTransitionOperation named:@"begin-trusted-operation" entering:CKKSStateInitialized];
        }

        if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchRequested];
            return [OctagonStateTransitionOperation named:@"fetch-requested" entering:CKKSStateBeginFetch];
        }

        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            return [OctagonStateTransitionOperation named:@"begin-trusted-operation" entering:CKKSStateProcessReceivedKeys];
        }

        if([flags _onqueueContains:CKKSFlagKeySetRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
            return [OctagonStateTransitionOperation named:@"provide-key-set" entering:CKKSStateProvideKeyHierarchyUntrusted];
        }

        if([flags _onqueueContains:CKKSFlag24hrNotification]) {
            [flags _onqueueRemoveFlag:CKKSFlag24hrNotification];
        }

        return nil;
    }

    if([currentState isEqualToString:CKKSStateProvideKeyHierarchy]) {
        CKKSNewTLKOperation* op = [[CKKSNewTLKOperation alloc] initWithDependencies:self.operationDependencies
                                                                   rollTLKIfPresent:NO
                                                          preexistingPendingKeySets:self.lastNewTLKOperation.keysets
                                                                      intendedState:CKKSStateBecomeReady
                                                                         errorState:CKKSStateError];
        self.lastNewTLKOperation = op;
        return op;
    }

    if([currentState isEqualToString:CKKSStateProvideKeyHierarchyUntrusted]) {
        CKKSNewTLKOperation* op = [[CKKSNewTLKOperation alloc] initWithDependencies:self.operationDependencies
                                                                   rollTLKIfPresent:NO
                                                          preexistingPendingKeySets:self.lastNewTLKOperation.keysets
                                                                      intendedState:CKKSStateWaitForTrust
                                                                         errorState:CKKSStateError];
        self.lastNewTLKOperation = op;
        return op;
    }

    if([currentState isEqualToString:CKKSStateExpandToHandleAllViews]) {
        WEAKIFY(self);
        return [OctagonStateTransitionOperation named:@"handle-all-views"
                                            intending:CKKSStateInitializing
                                           errorState:CKKSStateInitializing
                                  withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
            STRONGIFY(self);
            [self.operationDependencies operateOnAllViews];
            ckksnotice_global("ckksview", "Now operating on these views: %@", self.operationDependencies.views);
        }];
    }

    if([currentState isEqualToString:CKKSStateBecomeReady]) {
        return [self becomeReadyOperation:CKKSStateReady];
    }

    if([currentState isEqualToString:CKKSStateReady]) {
        // If we're ready, we can ignore the trust-related flags
        [flags _onqueueRemoveFlag:CKKSFlagBeginTrustedOperation];

        if(SecCKKSTestsEnabled()) {
            NSAssert([self.operationDependencies.allViews isSubsetOfSet:self.operationDependencies.views], @"When entering ready, CKKS should be operating on all of its zones");
        }

        // This flag indicates that something failed to occur due to lock state. Now that the device is unlocked, re-initialize!
        if([flags _onqueueContains:CKKSFlagDeviceUnlocked]) {
            [flags _onqueueRemoveFlag:CKKSFlagDeviceUnlocked];
            return [OctagonStateTransitionOperation named:@"key-state-after-unlock" entering:SecCKKSZoneKeyStateInitialized];
        }

        if(self.keyStateFullRefetchRequested) {
            // In ready, but something has requested a full refetch.
            ckksnotice("ckkskey", self, "Kicking off a full key refetch based on request:%d", self.keyStateFullRefetchRequested);
            [self.operationDependencies setStateForActiveZones:SecCKKSZoneKeyStateFetch];
            return [OctagonStateTransitionOperation named:@"full-refetch" entering:CKKSStateNeedFullRefetch];
        }

        if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchRequested];
            ckksnotice("ckkskey", self, "Kicking off a key refetch based on request");
            return [OctagonStateTransitionOperation named:@"fetch-requested" entering:CKKSStateBeginFetch];
        }

        if([flags _onqueueContains:CKKSFlagKeyStateProcessRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];
            ckksnotice("ckkskey", self, "Kicking off a key reprocess based on request");
            [self.operationDependencies setStateForActiveCKKSManagedViews:SecCKKSZoneKeyStateProcess];
            return [OctagonStateTransitionOperation named:@"key-process" entering:CKKSStateProcessReceivedKeys];
        }

        if([flags _onqueueContains:CKKSFlagKeySetRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
            return [OctagonStateTransitionOperation named:@"provide-key-set" entering:CKKSStateProvideKeyHierarchy];
        }

        if(self.trustStatus != CKKSAccountStatusAvailable) {
            ckksnotice("ckkskey", self, "In ready, but there's no trust; going into waitfortrust");
            // Allow the losetrust operation to handle setting keystates; it's tricky
            return [OctagonStateTransitionOperation named:@"trust-gone" entering:CKKSStateLoseTrust];
        }

        if([flags _onqueueContains:CKKSFlagTrustedPeersSetChanged]) {
            [flags _onqueueRemoveFlag:CKKSFlagTrustedPeersSetChanged];
            ckksnotice("ckkskey", self, "Received a nudge that the trusted peers set might have changed! Reprocessing.");
            [self.operationDependencies setStateForActiveCKKSManagedViews:SecCKKSZoneKeyStateProcess];
            return [OctagonStateTransitionOperation named:@"trusted-peers-changed" entering:CKKSStateProcessReceivedKeys];
        }

        if([flags _onqueueContains:CKKSFlagCheckQueues]) {
            [flags _onqueueRemoveFlag:CKKSFlagCheckQueues];
            return [OctagonStateTransitionOperation named:@"check-queues" entering:CKKSStateBecomeReady];
        }

        if([flags _onqueueContains:CKKSFlag24hrNotification]) {
            [flags _onqueueRemoveFlag:CKKSFlag24hrNotification];

            // We'd like to trigger our 24-hr backup fetch and scan.
            // That's currently part of the Initialized state, so head that way
            return [OctagonStateTransitionOperation named:@"24-hr-check" entering:CKKSStateInitialized];
        }

        if([flags _onqueueContains:CKKSFlagItemReencryptionNeeded]) {
            [flags _onqueueRemoveFlag:CKKSFlagItemReencryptionNeeded];

            return [OctagonStateTransitionOperation named:@"reencrypt" entering:CKKSStateReencryptOutgoingItems];
        }

        if([flags _onqueueContains:CKKSFlagProcessIncomingQueue]) {
            [flags _onqueueRemoveFlag:CKKSFlagProcessIncomingQueue];
            return [OctagonStateTransitionOperation named:@"process-incoming" entering:CKKSStateProcessIncomingQueue];
        }

        if([flags _onqueueContains:CKKSFlagScanLocalItems]) {
            [flags _onqueueRemoveFlag:CKKSFlagScanLocalItems];
            ckksnotice("ckkskey", self, "Launching a scan operation to find dropped items");

            return [OctagonStateTransitionOperation named:@"scan"
                                                 entering:CKKSStateScanLocalItems];
        }

        if([flags _onqueueContains:CKKSFlagProcessOutgoingQueue]) {
            // We only want to launch the OQO once the scheduler has given us a token.
            if([flags _onqueueContains:CKKSFlagOutgoingQueueOperationRateToken]) {
                [flags _onqueueRemoveFlag:CKKSFlagProcessOutgoingQueue];
                [flags _onqueueRemoveFlag:CKKSFlagOutgoingQueueOperationRateToken];

                return [OctagonStateTransitionOperation named:@"oqo" entering:CKKSStateProcessOutgoingQueue];

            } else {
                [self.outgoingQueueOperationScheduler trigger];
            }
        }

        // We're in ready. Any priority views have been handled!
        if([flags _onqueueContains:CKKSFlagNewPriorityViews]) {
            [flags _onqueueRemoveFlag:CKKSFlagNewPriorityViews];
        }

        // TODO: kick off a key roll if one has been requested

        // If we reach this point, we're in ready, and will stay there.
        // Launch any view states that would like to!
        for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
            [viewState launchComplete];
        }

        return nil;
    }

    if([currentState isEqualToString:CKKSStateBeginFetch]) {
        [flags _onqueueRemoveFlag:CKKSFlagFetchComplete];

        // Now that we've created (hopefully) all views, limit our consideration to only priority views
        // But _only_ if we're already trusted. Otherwise, proceed as normal: we'll focus on the priority views once trust arrives
        if([flags _onqueueContains:CKKSFlagNewPriorityViews] && self.trustStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagNewPriorityViews];
            [self _onqueuePrioritizePriorityViews];
        }

        WEAKIFY(self);

        NSSet<CKKSFetchBecause*>* fetchReasons = [self.operationDependencies.currentFetchReasons copy];
        [self.operationDependencies.currentFetchReasons removeAllObjects];

        CKKSResultOperation* fetchOp = [self.zoneChangeFetcher requestSuccessfulFetchForManyReasons:fetchReasons];
        CKKSResultOperation* flagOp = [CKKSResultOperation named:@"post-fetch"
                                                       withBlock:^{
            STRONGIFY(self);
            [self.stateMachine handleFlag:CKKSFlagFetchComplete];
        }];
        [flagOp addDependency:fetchOp];
        [self scheduleOperation:flagOp];

        // If this is a key hierarchy fetch, or if the zones aren't in ready, set them to fetch!
        // This should leave views that are in 'ready' in ready.
        BOOL keyHierarchyFetch = [self.operationDependencies.currentFetchReasons containsObject:CKKSFetchBecauseKeyHierarchy];
        for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
            ckksnotice("fetch", viewState, "Current state is %@, khf: %d", viewState, (int)keyHierarchyFetch);
            if(keyHierarchyFetch || ![viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady]) {
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateFetch;
            }
        }

        return [OctagonStateTransitionOperation named:@"waiting-for-fetch" entering:CKKSStateFetch];
    }

    if([currentState isEqualToString:CKKSStateFetch]) {
        if([flags _onqueueContains:CKKSFlagFetchComplete]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchComplete];
            return [OctagonStateTransitionOperation named:@"fetch-complete" entering:CKKSStateFetchComplete];
        }

        // Now that we've created (hopefully) all views, limit our consideration to only priority views
        // But _only_ if we're already trusted. Otherwise, proceed as normal: we'll focus on the priority views once trust arrives
        if([flags _onqueueContains:CKKSFlagNewPriorityViews] && self.trustStatus == CKKSAccountStatusAvailable) {
            [flags _onqueueRemoveFlag:CKKSFlagNewPriorityViews];
            [self _onqueuePrioritizePriorityViews];
        }

        // The flags CKKSFlagCloudKitZoneMissing and CKKSFlagChangeTokenExpired are both handled at the top of this function
        // So, we don't need to handle them here.

        return nil;
    }

    if([currentState isEqualToString:CKKSStateFetchComplete]) {
        return [OctagonStateTransitionOperation named:@"post-fetch-process" entering:CKKSStateProcessReceivedKeys];
    }

    if([currentState isEqualToString:CKKSStateProcessReceivedKeys]) {
        // If we've received a fetch request, make that happen first
        if([flags _onqueueContains:CKKSFlagFetchRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagFetchRequested];
            ckksnotice_global("ckkskey", "Kicking off a fetch based on request");
            return [OctagonStateTransitionOperation named:@"fetch-requested" entering:CKKSStateBeginFetch];
        }

        [flags _onqueueRemoveFlag:CKKSFlagKeyStateProcessRequested];

        [self.operationDependencies setStateForActiveCKKSManagedViews:SecCKKSZoneKeyStateProcess];
        [self.operationDependencies setStateForActiveExternallyManagedViews:SecCKKSZoneKeyStateReady];

        return [[CKKSProcessReceivedKeysOperation alloc] initWithDependencies:self.operationDependencies
                                                       allowFullRefetchResult:!self.keyStateMachineRefetched
                                                                intendedState:CKKSStateCheckZoneHierarchies
                                                                   errorState:CKKSStateError];
    }

    if([currentState isEqualToString:CKKSStateCheckZoneHierarchies]) {
        // This is an odd state: our next step will depend on the states of each zone we're currently operating on, and not the current state.
        // The Objective-C type system isn't flexible enough to integrate N states into the state machine.

        if([self anyViewsInState:SecCKKSZoneKeyStateUnhealthy]) {
            return [OctagonStateTransitionOperation named:@"unhealthy"
                                                 entering:CKKSStateUnhealthy];
        }

        if([self anyViewsInState:SecCKKSZoneKeyStateTLKMissing]) {
            return [OctagonStateTransitionOperation named:@"tlk-missing"
                                                 entering:CKKSStateTLKMissing];
        }

        if([self anyViewsInState:SecCKKSZoneKeyStateNewTLKsFailed]) {
            [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseResolvingConflict];
            return [OctagonStateTransitionOperation named:@"newtlks-failed"
                                                 entering:CKKSStateBeginFetch];
        }

        if([self anyViewsInState:SecCKKSZoneKeyStateWaitForTrust]) {
            // If we're noticing a trust failure while processing just priority views, then we'll want to go back to fetch everything, before ending back here.
            if(![self.operationDependencies.allViews isEqualToSet:self.operationDependencies.views]) {
                WEAKIFY(self);
                return [OctagonStateTransitionOperation named:@"handle-all-views-trust-loss"
                                                    intending:CKKSStateInitializing
                                                   errorState:CKKSStateInitializing
                                          withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                    STRONGIFY(self);
                    [self.operationDependencies operateOnAllViews];
                    ckksnotice_global("ckksview", "After trust failure, operating on these views: %@", self.operationDependencies.views);
                    op.nextState = op.intendedState;
                }];
            } else {
                return [OctagonStateTransitionOperation named:@"no-trust"
                                                     entering:CKKSStateLoseTrust];
            }
        }

        if([self anyViewsInState:SecCKKSZoneKeyStateNeedFullRefetch]) {
            NSSet<CKKSKeychainViewState*>* viewsToReset = [self viewsInState:SecCKKSZoneKeyStateNeedFullRefetch];
            [self.operationDependencies operateOnSelectViews:viewsToReset];

            return [OctagonStateTransitionOperation named:@"reset-views"
                                                 entering:CKKSStateNeedFullRefetch];
        }

        // Now that we're here, there's nothing more to be done for the zones.
        if([self anyViewsInState:SecCKKSZoneKeyStateWaitForUnlock]) {
            OctagonPendingFlag* unlocked = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagDeviceUnlocked
                                                                         conditions:OctagonPendingConditionsDeviceUnlocked];
            [pendingFlagHandler _onqueueHandlePendingFlagLater:unlocked];
        }

        if([self anyViewsInState:SecCKKSZoneKeyStateWaitForTLKCreation]) {
            ckksnotice_global("ckkskey", "Requesting TLK upload");
            [self.suggestTLKUpload trigger];
        }

        NSMutableSet<CKKSZoneKeyState*>* keyStates = [NSMutableSet set];
        for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
            [keyStates addObject:viewState.viewKeyHierarchyState];
        }

        if(![keyStates isSubsetOfSet:CKKSKeyStateNonTransientStates()]) {
            ckksnotice_global("ckks", "Misbehaving key states: %@", keyStates);
        }

        if(SecCKKSTestsEnabled()) {
            NSAssert([keyStates isSubsetOfSet:CKKSKeyStateNonTransientStates()], @"All view states should be nontransient before proceeding");
        }

        // If someone has requested the key hierarchies, return those, then return to processing the key sets
        if([flags _onqueueContains:CKKSFlagKeySetRequested]) {
            [flags _onqueueRemoveFlag:CKKSFlagKeySetRequested];
            CKKSNewTLKOperation* op = [[CKKSNewTLKOperation alloc] initWithDependencies:self.operationDependencies
                                                                       rollTLKIfPresent:NO
                                                              preexistingPendingKeySets:self.lastNewTLKOperation.keysets
                                                                          intendedState:CKKSStateCheckZoneHierarchies
                                                                             errorState:CKKSStateError];
            self.lastNewTLKOperation = op;
            return op;
        }

        return [OctagonStateTransitionOperation named:@"heal-tlk-shares"
                                             entering:CKKSStateHealTLKShares];
    }

    if([currentState isEqualToString:CKKSStateTLKMissing]) {
        return [self tlkMissingOperation:CKKSStateCheckZoneHierarchies];
    }

    if([currentState isEqualToString:CKKSStateHealTLKShares]) {
        return [[CKKSHealTLKSharesOperation alloc] initWithDependencies:self.operationDependencies
                                                          intendedState:CKKSStateProcessIncomingQueue
                                                             errorState:CKKSStateHealTLKSharesFailed];
    }

    if([currentState isEqualToString:CKKSStateNeedFullRefetch]) {
        ckksnotice("ckkskey", self, "Starting a key hierarchy full refetch");

        //TODO use states here instead of flags
        self.keyStateMachineRefetched = true;
        self.keyStateFullRefetchRequested = false;

        return [OctagonStateTransitionOperation named:@"fetch-complete" entering:CKKSStateResettingLocalData];
    }

    if([currentState isEqualToString:CKKSStateHealTLKSharesFailed]) {
        ckksnotice("ckkskey", self, "Creating new TLK shares didn't work. Attempting to refetch!");
        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseResolvingConflict];
        return [OctagonStateTransitionOperation named:@"heal-tlks-failed" entering:CKKSStateBeginFetch];
    }

    if([currentState isEqualToString:CKKSStateUnhealthy]) {
        if(self.trustStatus != CKKSAccountStatusAvailable) {
            ckksnotice("ckkskey", self, "Looks like the key hierarchy is unhealthy, but we're untrusted.");
            return [OctagonStateTransitionOperation named:@"unhealthy-lacking-trust" entering:CKKSStateLoseTrust];

        } else {
            ckksnotice("ckkskey", self, "Looks like the key hierarchy is unhealthy. Launching fix.");
            return [[CKKSHealKeyHierarchyOperation alloc] initWithDependencies:self.operationDependencies
                                                        allowFullRefetchResult:!self.keyStateMachineRefetched
                                                                     intending:CKKSStateCheckZoneHierarchies
                                                                    errorState:CKKSStateError];
        }
    }

    if([currentState isEqualToString:CKKSStateProcessIncomingQueue]) {
        [flags _onqueueRemoveFlag:CKKSFlagProcessIncomingQueue];

        BOOL mismatchedViewItems = [flags _onqueueContains:CKKSFlagProcessIncomingQueueWithFreshPolicy];
        [flags _onqueueRemoveFlag:CKKSFlagProcessIncomingQueueWithFreshPolicy];

        if(mismatchedViewItems) {
            ckksnotice_global("ckksincoming", "Going to process the incoming queue with a fresh policy");
        }

        CKKSIncomingQueueOperation* op = [[CKKSIncomingQueueOperation alloc] initWithDependencies:self.operationDependencies
                                                                                        intending:CKKSStateBecomeReady
                                                                 pendingClassAItemsRemainingState:CKKSStateRemainingClassAIncomingItems
                                                                                       errorState:CKKSStateBecomeReady
                                                                        handleMismatchedViewItems:mismatchedViewItems];

        if(self.resultsOfNextIncomingQueueOperationOperation) {
            [self.resultsOfNextIncomingQueueOperationOperation addSuccessDependency:op];
            [self scheduleOperation:self.resultsOfNextIncomingQueueOperationOperation];
            self.resultsOfNextIncomingQueueOperationOperation = nil;
        }

        self.lastIncomingQueueOperation = op;
        [op addNullableDependency:self.holdIncomingQueueOperation];
        return op;
    }

    if([currentState isEqualToString:CKKSStateRemainingClassAIncomingItems]) {
        return [OctagonStateTransitionOperation named:@"iqo-errored"
                                             entering:SecCKKSZoneKeyStateBecomeReady];
    }

    if([currentState isEqualToString:CKKSStateScanLocalItems]) {
        [flags _onqueueRemoveFlag:CKKSFlagScanLocalItems];

        CKKSScanLocalItemsOperation* op = [[CKKSScanLocalItemsOperation alloc] initWithDependencies:self.operationDependencies
                                                                                          intending:CKKSStateBecomeReady
                                                                                         errorState:CKKSStateError
                                                                                   ckoperationGroup:nil];
        self.initiatedLocalScan = true;
        return op;
    }

    if([currentState isEqualToString:CKKSStateReencryptOutgoingItems]) {
        CKKSReencryptOutgoingItemsOperation* op = [[CKKSReencryptOutgoingItemsOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                      intendedState:CKKSStateBecomeReady
                                                                                                         errorState:CKKSStateError
                                                                                                      holdOperation:nil];
        self.lastReencryptOutgoingItemsOperation = op;
        return op;
    }

    if([currentState isEqualToString:CKKSStateProcessOutgoingQueue]) {
        [flags _onqueueRemoveFlag:CKKSFlagProcessOutgoingQueue];

        CKKSOutgoingQueueOperation* op = [[CKKSOutgoingQueueOperation alloc] initWithDependencies:self.operationDependencies
                                                                                        intending:CKKSStateBecomeReady
                                                                                     ckErrorState:CKKSStateOutgoingQueueOperationFailed
                                                                                       errorState:CKKSStateInitialized];
        [op addNullableDependency:self.holdOutgoingQueueOperation];
        [op linearDependencies:self.outgoingQueueOperations];
        self.lastOutgoingQueueOperation = op;
        return op;
    }

    if([currentState isEqualToString:CKKSStateOutgoingQueueOperationFailed]) {
        return [OctagonStateTransitionOperation named:@"oqo-failure" entering:CKKSStateBecomeReady];
    }

    return nil;
}

- (OctagonStateTransitionOperation*)becomeReadyOperation:(CKKSState*)newState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"become-ready"
                                        intending:newState
                                       errorState:CKKSStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);

        [self dispatchSyncWithReadOnlySQLTransaction:^{
            NSSet<CKKSKeychainViewState*>* viewsOfInterest = [self.operationDependencies readyAndSyncingViews];

            for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneName];
                if(!ckse.ckzonecreated || !ckse.ckzonesubscribed) {
                    ckksnotice("ckkszone", viewState.zoneID, "Zone does not yet exist: %@ %@", viewState, ckse);
                    op.nextState = CKKSStateInitializing;
                    return;
                }
            }

            for(CKKSKeychainViewState* viewState in viewsOfInterest) {
                NSError* iqeCountError = nil;
                NSDictionary<NSString*, NSNumber*>* iqeCounts = [CKKSIncomingQueueEntry countNewEntriesByKeyInZone:viewState.zoneID error:&iqeCountError];

                if(iqeCounts.count > 0) {
                    ckksnotice("ckksincoming", viewState.zoneID, "Incoming Queue item counts: %@", iqeCounts);
                }

                for(NSString* keyUUID in iqeCounts) {
                    NSError* keyError = nil;
                    CKKSKey* key = [CKKSKey fromDatabase:keyUUID zoneID:viewState.zoneID error:&keyError];

                    if(!key || keyError) {
                        ckkserror("ckksincoming", viewState.zoneID, "Unable to load key for %@: %@", keyUUID, keyError);
                        continue;
                    }

                    if([key.keyclass isEqualToString:SecCKKSKeyClassA] && self.lockStateTracker.isLocked) {
                        ckksnotice("ckksincoming", viewState.zoneID, "Have pending classA items for view, but device is locked");
                        OctagonPendingFlag* pending = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagCheckQueues
                                                                                    conditions:OctagonPendingConditionsDeviceUnlocked];

                        [self.stateMachine _onqueueHandlePendingFlagLater:pending];
                    } else {
                        op.nextState = CKKSStateProcessIncomingQueue;
                        return;
                    }
                }

                NSError* cipCountError = nil;
                NSInteger cipCount = [CKKSCurrentItemPointer countByState:SecCKKSStateNew zone:viewState.zoneID error:&cipCountError];
                if(cipCount > 0) {
                    ckksnotice("ckksincoming", viewState.zoneID, "Incoming Queue CIP count: %d", (int)cipCount);
                    op.nextState = CKKSStateProcessIncomingQueue;
                    return;
                }

                if(cipCountError != nil) {
                    ckkserror("ckksincoming", viewState.zoneID, "Unable to count CIPs: %@", cipCountError);
                }
            }

            // Now that we've processed incoming items, check to see if we've limited which views we're handling
            // We might be in this situation if we just processed some priority views
            // If we have been operating only on a subset of views, now operate on all of them
            NSMutableSet<CKKSKeychainViewState*>* filteredViews = [self.operationDependencies.allViews mutableCopy];
            [filteredViews minusSet:self.operationDependencies.views];
            if(filteredViews.count > 0) {
                ckksnotice_global("ckkszone", "Beginning again to include these views: %@", filteredViews);
                op.nextState = CKKSStateExpandToHandleAllViews;
                return;
            }

            for(CKKSKeychainViewState* viewState in viewsOfInterest) {
                // Are there any entries waiting for reencryption? If so, set the flag.
                NSError* reencryptOQEError = nil;
                NSInteger reencryptOQEcount = [CKKSOutgoingQueueEntry countByState:SecCKKSStateReencrypt
                                                                              zone:viewState.zoneID
                                                                             error:&reencryptOQEError];
                if(reencryptOQEError) {
                    ckkserror("ckks", viewState.zoneID, "Couldn't count reencrypt OQEs, bad behavior ahead: %@", reencryptOQEError);
                }

                if(![viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady]) {
                    ckksnotice("ckksincoming", viewState.zoneID, "Zone not ready (%@): skipping reencryption", viewState);
                    continue;
                }

                if(![self.operationDependencies.syncingPolicy isSyncingEnabledForView:viewState.zoneID.zoneName]) {
                    ckksnotice("ckksincoming", viewState.zoneID, "Syncing disabled for (%@): skipping incoming queue processing", viewState);
                    continue;
                }

                if(reencryptOQEcount > 0) {
                    op.nextState = CKKSStateReencryptOutgoingItems;
                    return;
                }
            }

            NSDate* now = [NSDate date];
            NSDateComponents* offset = [[NSDateComponents alloc] init];
            [offset setHour:-24];
            NSDate* oneDayDeadline = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:now options:0];

            NSDate* earliestFetchTime = nil;
            CKKSKeychainViewState* earliestFetchedView = nil;

            for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneName];

                if(ckse.lastFetchTime == nil ||
                   [ckse.lastFetchTime compare:oneDayDeadline] == NSOrderedAscending ||
                   ckse.moreRecordsInCloudKit) {

                    if(ckse.lastFetchTime == nil) {
                        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseInitialStart];
                    } else if(ckse.moreRecordsInCloudKit) {
                        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseMoreComing];
                    } else {
                        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecausePeriodicRefetch];
                    }

                    ckksnotice("ckksfetch", viewState.zoneID, "Fetch last occurred at %@ (%@); beginning a new one", ckse.lastFetchTime, ckse.moreRecordsInCloudKit ? @"more coming" : @"complete");

                    op.nextState = CKKSStateBeginFetch;
                    return;
                }

                if(earliestFetchTime == nil || [earliestFetchTime compare:ckse.lastFetchTime] == NSOrderedDescending) {
                    earliestFetchTime = ckse.lastFetchTime;
                    earliestFetchedView = viewState;
                }
            }

            ckksnotice_global("ckksfetch", "Fetch last occurred at %@ (for %@)", earliestFetchTime, earliestFetchedView);

            NSDate* earliestScanTime = nil;
            CKKSKeychainViewState* earliestScannedView = nil;

            for(CKKSKeychainViewState* viewState in viewsOfInterest) {
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneName];

                if(ckse.lastLocalKeychainScanTime == nil || [ckse.lastLocalKeychainScanTime compare:oneDayDeadline] == NSOrderedAscending) {
                    ckksnotice("ckksscan", viewState.zoneID, "CKKS scan last occurred at %@; beginning a new one", ckse.lastLocalKeychainScanTime);

                    if(!SecCKKSTestSkipScan()) {
                        op.nextState = CKKSStateScanLocalItems;
                        return;
                    }
                }

                if(earliestScanTime == nil || [earliestScanTime compare:ckse.lastLocalKeychainScanTime] == NSOrderedDescending) {
                    earliestScanTime = ckse.lastLocalKeychainScanTime;
                    earliestScannedView = viewState;
                }
            }

            ckksnotice_global("ckksscan", "CKKS scan last occurred at %@ (for %@)", earliestScanTime, earliestScannedView);

            for(CKKSKeychainViewState* viewState in viewsOfInterest) {
                NSError* oqeCountError = nil;
                NSInteger oqeCount = [CKKSOutgoingQueueEntry countByState:SecCKKSStateNew zone:viewState.zoneID error:&oqeCountError];

                if(oqeCount > 0) {
                    ckksnotice("ckksoutgoing", viewState.zoneID, "Have %d outgoing items; scheduling upload", (int)oqeCount);
                    // Allow the state machine to control when this occurs, to handle rate-limiting
                    OctagonPendingFlag* pending = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagProcessOutgoingQueue
                                                                                conditions:OctagonPendingConditionsNetworkReachable];
                    [self.stateMachine _onqueueHandlePendingFlagLater:pending];
                    [self.outgoingQueueOperationScheduler trigger];

                } else if(oqeCountError != nil) {
                    ckksnotice("ckksoutgoing", viewState.zoneID, "Error checking outgoing queue: %@", oqeCountError);
                }
            }

            for(CKKSKeychainViewState* viewState in self.operationDependencies.allCKKSManagedViews) {
                if(!viewState.launch.launched) {
                    NSError* error = nil;
                    NSNumber *zoneSize = [CKKSMirrorEntry counts:viewState.zoneID error:&error];

                    if(error) {
                        ckkserror("launch", viewState.zoneID, "Unable to count mirror entries: %@", error);
                    }
                    if (zoneSize) {
                        zoneSize = @(SecBucket1Significant([zoneSize longValue]));
                        [viewState.launch addAttribute:@"zonesize" value:zoneSize];
                    }
                }
            }

            op.nextState = newState;
        }];
    }];
}

- (OctagonStateTransitionOperation*)loseTrustOperation:(CKKSState*)intendedState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"lose-trust"
                                        intending:intendedState
                                       errorState:CKKSStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);

        // If our current state is "trusted", fall out
        if(self.trustStatus == CKKSAccountStatusAvailable) {
            self.trustStatus = CKKSAccountStatusUnknown;
            self.trustStatusKnown = [[CKKSCondition alloc] init];
        }

        for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
            if([viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady]) {
                // If we had a healthy key hierarchy, it's no longer one.
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForTrust;
            }
        }

        // We're now untrusted. If anyone is waiting, tell them things won't happen
        [self.priorityViewsProcessed completeWithErrorIfPending:[NSError errorWithDomain:CKKSErrorDomain code:CKKSLackingTrust description:@"Trust not present"]];

        op.nextState = intendedState;
    }];
}

- (BOOL)anyViewsInState:(CKKSZoneKeyState*)state
{
    for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
        if([viewState.viewKeyHierarchyState isEqualToString:state]) {
            return YES;
        }
    }

    return NO;
}

- (NSSet<CKKSKeychainViewState*>*)viewsInState:(CKKSZoneKeyState*)state
{
    return [self.operationDependencies viewsInState:state];
}

- (NSSet<NSString*>*)viewList
{
    NSMutableSet<NSString*>* set = [NSMutableSet set];

    for(CKKSKeychainViewState* viewState in self.operationDependencies.activeManagedViews) {
        [set addObject:viewState.zoneID.zoneName];
    }

    return set;
}

- (NSDate*)earliestFetchTime
{
    NSDate* earliestFetchTime = nil;

    for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
        CKKSZoneStateEntry* zse = [CKKSZoneStateEntry state:viewState.zoneName];

        if(zse.lastFetchTime == nil) {
            earliestFetchTime = [NSDate distantPast];
            break;
        }

        if(earliestFetchTime == nil || [earliestFetchTime compare:zse.lastFetchTime] == NSOrderedDescending) {
            earliestFetchTime = zse.lastFetchTime;
        }
    }

    return earliestFetchTime ?: [NSDate distantPast];
}

- (OctagonStateTransitionOperation*)tlkMissingOperation:(CKKSState*)newState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"tlk-missing"
                                        intending:newState
                                       errorState:CKKSStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);

        NSArray<CKKSPeerProviderState*>* trustStates = self.operationDependencies.currentTrustStates;
        NSMutableSet<CKKSKeychainViewState*>* viewsToReset = [NSMutableSet set];

        [self.operationDependencies.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
            for(CKKSKeychainViewState* viewState in self.operationDependencies.activeManagedViews) {
                // Ignore all views which aren't in the right state
                if(![viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateTLKMissing]) {
                    continue;
                }

                CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];

                if(keyset.error) {
                    ckkserror("ckkskey", viewState.zoneID, "Unable to load keyset: %@", keyset.error);
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                    continue;
                }

                if(!keyset.currentTLKPointer.currentKeyUUID) {
                    // In this case, there's no current TLK at all. Go into "wait for tlkcreation";
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForTLKCreation;
                    continue;
                }

                if(self.trustStatus != CKKSAccountStatusAvailable) {
                    ckksnotice("ckkskey", viewState.zoneID, "TLK is missing, but no trust is present.");
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForTrust;
                    continue;
                }

                bool otherDevicesPresent = [self _onqueueOtherDevicesReportHavingTLKs:keyset
                                                                          trustStates:trustStates];
                if(otherDevicesPresent) {
                    // We expect this keyset to continue to exist. Send it to our listeners.
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForTLK;
                } else {
                    ckksnotice("ckkskey", viewState.zoneID, "No other devices claim to have the TLK. Resetting zone...");
                    [viewsToReset addObject:viewState];

                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateResettingZone;
                }
            }
            return;
        }];

        if(viewsToReset.count > 0) {
            ckksnotice_global("ckkskey", "Resetting zones due to missing TLKs: %@", viewsToReset);
            [self.operationDependencies operateOnSelectViews:viewsToReset];

            op.nextState = CKKSStateResettingZone;
        } else {
            op.nextState = newState;
        }
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
           (sosPeerID != nil && [trustedPeerIDs containsObject:sosPeerID]) ||
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

    SecDbItemRef modified = added ? added : deleted;

    NSString* keyViewName = [CKKSKey isItemKeyForKeychainView:modified];

    if(keyViewName) {
        if(!SecCKKSTestDisableKeyNotifications()) {
            ckksnotice("ckks", self, "Potential new key material from %@ (source %lu)",
                       keyViewName, (unsigned long)txionSource);

            [self.stateMachine handleFlag:CKKSFlagKeyStateProcessRequested];
        } else {
            ckksnotice("ckks", self, "Ignoring potential new key material from %@ (source %lu)",
                       keyViewName, (unsigned long)txionSource);
        }
        return;
    }

    bool addedSync   = added   && SecDbItemIsSyncable(added);
    bool deletedSync = deleted && SecDbItemIsSyncable(deleted);

    if(!addedSync && !deletedSync) {
        // Local-only change. Skip with prejudice.
        ckksinfo_global("ckks", "skipping sync of non-sync item (%d, %d)", addedSync, deletedSync);
        return;
    }

    if(!SecDbItemIsPrimaryUserItem(modified)) {
        ckksnotice_global("ckks", "Ignoring syncable keychain item for non-primary account");
        return;
    }

    __block bool havePolicy = false;
    dispatch_sync(self.queue, ^{
        if(!self.operationDependencies.syncingPolicy) {
            ckkserror_global("ckks", "No policy configured. Skipping item: %@", modified);
            self.itemModificationsBeforePolicyLoaded = YES;

            return;
        } else {
            havePolicy = true;
        }
    });

    if(!havePolicy) {
        return;
    }

    NSString* viewName = [self.operationDependencies viewNameForItem:modified];

    if(!viewName) {
        ckksnotice_global("ckks", "No intended CKKS view for item; skipping: %@", modified);
        return;
    }

    CKKSKeychainViewState* viewState = nil;

    // Explicitly use allCKKSManagedViews here. If the zone isn't ready, we can mark it as needing a scan later.
    for(CKKSKeychainViewState* vs in self.operationDependencies.allCKKSManagedViews) {
        if([vs.zoneID.zoneName isEqualToString:viewName]) {
            viewState = vs;
            break;
        }
    }

    if(!viewState) {
        ckksnotice_global("ckks", "No CKKS view for %@, skipping: %@", viewName, modified);

        NSString* uuid = (__bridge NSString*)SecDbItemGetValue(modified, &v10itemuuid, NULL);
        SecBoolNSErrorCallback syncCallback = [[CKKSViewManager manager] claimCallbackForUUID:uuid];

        if(syncCallback) {
            syncCallback(false, [NSError errorWithDomain:CKKSErrorDomain
                                                    code:CKKSNoSuchView
                                                userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for '%@'", viewName]}]);
        }
        return;
    }

    ckksnotice("ckks", viewState.zoneID, "Routing item to zone %@: %@", viewName, modified);

    __block NSError* error = nil;

    // Tombstones come in as item modifications or item adds. Handle modifications here.
    bool addedTombstone   = added   && SecDbItemIsTombstone(added);
    bool deletedTombstone = deleted && SecDbItemIsTombstone(deleted);

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
        ckksnotice("ckks", viewState.zoneID, "skipping sync of non-sync item (%d, %d)", addedSync, deletedSync);
        return;
    }

    if(isTombstoneModification) {
        ckksnotice("ckks", viewState.zoneID, "skipping syncing update of tombstone item (%d, %d)", addedTombstone, deletedTombstone);
        return;
    }

    // It's possible to ask for an item to be deleted without adding a corresponding tombstone.
    // This is arguably a bug, as it generates an out-of-sync state, but it is in the API contract.
    // CKKS should ignore these, but log very upset messages.
    if(isDelete && !addedTombstone) {
        ckksnotice("ckks", viewState.zoneID, "Client has asked for an item deletion to not sync. Keychain is now out of sync with account");
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
        ckksnotice("ckks", viewState.zoneID, "Received an incoming %@ from SOS (%@)",
                   isAdd ? @"addition" : (isModify ? @"modification" : @"deletion"),
                   addedUUID);
    }

    // Our caller gave us a database connection. We must get on the local queue to ensure atomicity
    // Note that we're at the mercy of the surrounding db transaction, so don't try to rollback here
    [self.databaseProvider dispatchSyncWithConnection:dbconn
                                       readWriteTxion:YES
                                                block:^CKKSDatabaseTransactionResult
     {
#if TARGET_OS_IOS
        NSUUID* preexistingMusrUUID = nil;
        if(SecCKKSTestsEnabled()) {
            // CKKS will use the SecItem APIs to do its work. But, in real operation, the musr set by the xpc client won't affect securityd's musr.
            NSData* musrUUIDData = [((__bridge NSData*)SecSecurityClientGet()->musr) copy];
            preexistingMusrUUID = musrUUIDData ? [[NSUUID alloc] initWithUUIDBytes:musrUUIDData.bytes] : nil;
            SecSecuritySetPersonaMusr(NULL);
        }
#endif

        // Schedule a "view changed" notification
        [viewState.notifyViewChangedScheduler trigger];

        if(self.accountStatus == CKKSAccountStatusNoAccount) {
            // No account; CKKS shouldn't attempt anything.
            [self.stateMachine _onqueueHandleFlag:CKKSFlagScanLocalItems];
            ckksnotice("ckks", viewState.zoneID, "Dropping sync item modification due to CK account state; will scan to find changes later");

            // We're positively not logged into CloudKit, and therefore don't expect this item to be synced anytime particularly soon.
            NSString* uuid = (__bridge NSString*)SecDbItemGetValue(added ? added : deleted, &v10itemuuid, NULL);

            SecBoolNSErrorCallback syncCallback = [[CKKSViewManager manager] claimCallbackForUUID:uuid];
            if(syncCallback) {
                [CKKSViewManager callSyncCallbackWithErrorNoAccount: syncCallback];
            }

#if TARGET_OS_IOS
            if(SecCKKSTestsEnabled() && preexistingMusrUUID != nil) {
                SecSecuritySetPersonaMusr((__bridge CFStringRef)preexistingMusrUUID.UUIDString);
            }
#endif
            return CKKSDatabaseTransactionCommit;
        }

        CKKSMemoryKeyCache* keyCache = [[CKKSMemoryKeyCache alloc] init];

        CKKSOutgoingQueueEntry* oqe = nil;
        if       (isAdd) {
            oqe = [CKKSOutgoingQueueEntry withItem: added   action: SecCKKSActionAdd    zoneID:viewState.zoneID keyCache:keyCache error: &error];
        } else if(isDelete) {
            oqe = [CKKSOutgoingQueueEntry withItem: deleted action: SecCKKSActionDelete zoneID:viewState.zoneID keyCache:keyCache error: &error];
        } else if(isModify) {
            oqe = [CKKSOutgoingQueueEntry withItem: added   action: SecCKKSActionModify zoneID:viewState.zoneID keyCache:keyCache error: &error];
        } else {
            ckkserror("ckks", viewState.zoneID, "processKeychainEventItemAdded given garbage: %@ %@", added, deleted);

#if TARGET_OS_IOS
            if(SecCKKSTestsEnabled() && preexistingMusrUUID != nil) {
                SecSecuritySetPersonaMusr((__bridge CFStringRef)preexistingMusrUUID.UUIDString);
            }
#endif
            return CKKSDatabaseTransactionCommit;
        }

        if(![self.operationDependencies.syncingPolicy isSyncingEnabledForView:viewState.zoneID.zoneName]) {
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
            ckkserror("ckks", viewState.zoneID, "Couldn't create outgoing queue entry: %@", error);
            [self.stateMachine _onqueueHandleFlag:CKKSFlagScanLocalItems];

            // If the problem is 'couldn't load key', tell the key hierarchy state machine to fix it
            if([error.domain isEqualToString:CKKSErrorDomain] && error.code == errSecItemNotFound) {
                [self.stateMachine _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];
            }

#if TARGET_OS_IOS
            if(SecCKKSTestsEnabled() && preexistingMusrUUID != nil) {
                SecSecuritySetPersonaMusr((__bridge CFStringRef)preexistingMusrUUID.UUIDString);
            }
#endif

            return CKKSDatabaseTransactionCommit;
        } else if(!oqe) {
            ckkserror("ckks", viewState.zoneID, "Decided that no operation needs to occur for %@", error);

#if TARGET_OS_IOS
            if(SecCKKSTestsEnabled() && preexistingMusrUUID != nil) {
                SecSecuritySetPersonaMusr((__bridge CFStringRef)preexistingMusrUUID.UUIDString);
            }
#endif
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
            ckkserror("ckks", viewState.zoneID, "Couldn't save outgoing queue entry to database: %@", error);

#if TARGET_OS_IOS
            if(SecCKKSTestsEnabled() && preexistingMusrUUID != nil) {
                SecSecuritySetPersonaMusr((__bridge CFStringRef)preexistingMusrUUID.UUIDString);
            }
#endif
            return CKKSDatabaseTransactionCommit;
        } else {
            ckksnotice("ckks", viewState.zoneID, "Saved %@ to outgoing queue", oqe);
        }

        // This update supercedes all other local modifications to this item (_except_ those in-flight).
        // Delete all items in reencrypt or error.
        NSArray<CKKSOutgoingQueueEntry*>* siblings = [CKKSOutgoingQueueEntry allWithUUID:oqe.uuid
                                                                                  states:@[SecCKKSStateReencrypt, SecCKKSStateError]
                                                                                  zoneID:viewState.zoneID
                                                                                   error:&error];
        if(error) {
            ckkserror("ckks", viewState.zoneID, "Couldn't load OQE siblings for %@: %@", oqe, error);
        }

        for(CKKSOutgoingQueueEntry* oqeSibling in siblings) {
            NSError* deletionError = nil;
            [oqeSibling deleteFromDatabase:&deletionError];
            if(deletionError) {
                ckkserror("ckks", viewState.zoneID, "Couldn't delete OQE sibling(%@) for %@: %@", oqeSibling, oqe.uuid, deletionError);
            }
        }

        // This update also supercedes any remote changes that are pending.
        NSError* iqeError = nil;
        CKKSIncomingQueueEntry* iqe = [CKKSIncomingQueueEntry tryFromDatabase:oqe.uuid zoneID:viewState.zoneID error:&iqeError];
        if(iqeError) {
            ckkserror("ckks", viewState.zoneID, "Couldn't find IQE matching %@: %@", oqe.uuid, error);
        } else if(iqe) {
            [iqe deleteFromDatabase:&iqeError];
            if(iqeError) {
                ckkserror("ckks", viewState.zoneID, "Couldn't delete IQE matching %@: %@", oqe.uuid, error);
            } else {
                ckksnotice("ckks", viewState.zoneID, "Deleted IQE matching changed item %@", oqe.uuid);
            }
        }

        [self _onqueueProcessOutgoingQueue:operationGroup];

#if TARGET_OS_IOS
            if(SecCKKSTestsEnabled() && preexistingMusrUUID != nil) {
                SecSecuritySetPersonaMusr((__bridge CFStringRef)preexistingMusrUUID.UUIDString);
            }
#endif
        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)setCurrentItemForAccessGroup:(NSData* _Nonnull)newItemPersistentRef
                                hash:(NSData*)newItemSHA1
                         accessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                            viewHint:(NSString*)viewHint
                           replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                                hash:(NSData*)oldItemSHA1
                            complete:(void (^) (NSError* operror)) complete
{
    NSError* viewError = nil;
    CKKSKeychainViewState* viewState = [self policyDependentViewStateForName:viewHint
                                                                       error:&viewError];
    if(!viewState) {
        complete(viewError);
        return;
    }

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

    CKKSAccountStatus accountStatus = self.accountStatus;
    if(accountStatus != CKKSAccountStatusAvailable) {
        NSError* localError = nil;
        if(accountStatus == CKKSAccountStatusUnknown) {
            localError = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSErrorAccountStatusUnknown
                                      description:@"iCloud account status unknown."];
        } else {
            localError = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNotLoggedIn
                                      description:@"User is not signed into iCloud."];
        }

        ckksnotice("ckkscurrent", self, "Rejecting current item pointer set since we don't have an iCloud account: %@", localError);
        complete(localError);
        return;
    }

    ckksnotice("ckkscurrent", self, "Starting change current pointer operation for %@-%@", accessGroup, identifier);
    CKKSUpdateCurrentItemPointerOperation* ucipo = [[CKKSUpdateCurrentItemPointerOperation alloc] initWithCKKSOperationDependencies:self.operationDependencies
                                                                                                                          viewState:viewState
                                                                                                                            newItem:newItemPersistentRef
                                                                                                                               hash:newItemSHA1
                                                                                                                        accessGroup:accessGroup
                                                                                                                         identifier:identifier
                                                                                                                          replacing:oldCurrentItemPersistentRef
                                                                                                                               hash:oldItemSHA1
                                                                                                                   ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"currentitem-api"]];


    CKKSResultOperation* returnCallback = [CKKSResultOperation operationWithBlock:^{
        if(ucipo.error) {
            ckkserror("ckkscurrent", viewState.zoneID, "Failed setting a current item pointer for %@ with %@", ucipo.currentPointerIdentifier, ucipo.error);
        } else {
            ckksnotice("ckkscurrent", viewState.zoneID, "Finished setting a current item pointer for %@", ucipo.currentPointerIdentifier);
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

- (void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                            viewHint:(NSString*)viewHint
                     fetchCloudValue:(bool)fetchCloudValue
                            complete:(void (^) (NSString* uuid, NSError* operror)) complete
{
    NSError* viewError = nil;
    CKKSKeychainViewState* viewState = [self policyDependentViewStateForName:viewHint
                                                                       error:&viewError];
    if(!viewState) {
        complete(nil, viewError);
        return;
    }

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

    CKKSAccountStatus accountStatus = self.accountStatus;
    if(accountStatus != CKKSAccountStatusAvailable) {
        NSError* localError = nil;
        if(accountStatus == CKKSAccountStatusUnknown) {
            localError = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSErrorAccountStatusUnknown
                                      description:@"iCloud account status unknown."];
        } else {
            localError = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNotLoggedIn
                                      description:@"User is not signed into iCloud."];
        }

        ckksnotice("ckkscurrent", self, "Rejecting current item pointer get since we don't have an iCloud account: %@", localError);
        complete(NULL, localError);
        return;
    }

    CKKSResultOperation* fetchAndProcess = nil;
    if(fetchCloudValue) {
        fetchAndProcess = [self rpcFetchAndProcessIncomingQueue:[NSSet setWithObject:viewHint]
                                                        because:CKKSFetchBecauseCurrentItemFetchRequest
                                           errorOnClassAFailure:false];
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
                                                                        zoneID:viewState.zoneID
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

- (CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*)findKeySets:(BOOL)refetchBeforeReturningKeySet
{
    if(refetchBeforeReturningKeySet) {
        [self.operationDependencies.currentFetchReasons addObject:CKKSFetchBecauseKeySetFetchRequest];
        [self.stateMachine handleFlag:CKKSFlagFetchRequested];
    }

    __block CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp = nil;

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        keysetOp = (CKKSProvideKeySetOperation*)[self findFirstPendingOperation:self.operationDependencies.keysetProviderOperations];
        if(!keysetOp) {
            NSMutableSet<CKRecordZoneID*>* allZoneIDs = [NSMutableSet set];
            for(CKKSKeychainViewState* viewState in self.operationDependencies.allCKKSManagedViews) {
                [allZoneIDs addObject:viewState.zoneID];
            }

            keysetOp = [[CKKSProvideKeySetOperation alloc] initWithIntendedZoneIDs:allZoneIDs];
            [self.operationDependencies.keysetProviderOperations addObject:keysetOp];

            // This is an abuse of operations: they should generally run when added to a queue, not wait, but this allows recipients to set timeouts
            [self scheduleOperationWithoutDependencies:keysetOp];
        }

        NSMutableDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>* currentKeysets = [NSMutableDictionary dictionary];
        BOOL needStateMachineHelpForView = NO;

        for(CKKSKeychainViewState* viewState in self.operationDependencies.allCKKSManagedViews) {
            CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];
            if(keyset.currentTLKPointer.currentKeyUUID &&
               (keyset.tlk.uuid ||
                [viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTrust] ||
                [viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLK])) {
                ckksnotice("ckks", viewState.zoneID, "Already have keyset %@", keyset);

                currentKeysets[viewState.zoneID] = keyset;
            } else {
                ckksnotice("ckks", viewState.zoneID, "No current keyset for %@ (%@)", viewState.zoneID, keyset);
                needStateMachineHelpForView = true;
                break;
            }
        }

        if(needStateMachineHelpForView) {
            [self.stateMachine _onqueueHandleFlag:CKKSFlagKeySetRequested];
        } else {
            [keysetOp provideKeySets:currentKeysets];
        }
    }];

    return keysetOp;
}

- (void)receiveTLKUploadRecords:(NSArray<CKRecord*>*)records
{
    ckksnotice("ckkskey", self, "Received a set of %lu TLK upload records", (unsigned long)records.count);

    if(!records || records.count == 0) {
        return;
    }

    [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        for(CKRecord* record in records) {
            [self.operationDependencies intransactionCKRecordChanged:record resync:false];
        }

        [self.stateMachine _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];

        return CKKSDatabaseTransactionCommit;
    }];
}

- (NSSet<CKKSKeychainViewState*>*)viewsRequiringTLKUpload
{
    __block NSSet<CKKSKeychainViewState*>* requiresUpload = nil;

    dispatch_sync(self.queue, ^{
        requiresUpload = [self.operationDependencies viewsInState:SecCKKSZoneKeyStateWaitForTLKCreation];
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

- (CKKSResultOperation*)rpcProcessOutgoingQueue:(NSSet<NSString*>* _Nullable)viewNames
                                 operationGroup:(CKOperationGroup* _Nullable)ckoperationGroup
{
    __block CKKSResultOperation* failOp = nil;

    dispatch_sync(self.queue, ^{
        if([self.stateMachine.currentState isEqualToString:CKKSStateLoggedOut]) {
            failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
                op.error = [NSError errorWithDomain:CKKSErrorDomain
                                               code:CKKSNotLoggedIn
                                        description:@"No CloudKit account; push will not succeed."];
            }];
        }

        NSSet<NSString*>* viewNamesToIterate = viewNames ?: self.viewList;

        for(NSString* viewName in viewNamesToIterate) {
            CKKSKeychainViewState* viewState = [self viewStateForName:viewName];

            if([viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLK]) {
                failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
                    op.error = [NSError errorWithDomain:CKKSErrorDomain
                                                   code:CKKSKeysMissing
                                            description:@"No TLKs for this view; push will not succeed."];
                }];
                break;
            }

            if([viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTrust]) {
                failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
                    op.error = [NSError errorWithDomain:CKKSErrorDomain
                                                   code:CKKSLackingTrust
                                            description:@"No trust; push will not succeed."];
                }];
                break;
            }

            if(!viewState.ckksManagedView) {
                failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
                    op.error = [NSError errorWithDomain:CKKSErrorDomain
                                                   code:CKKSErrorViewIsExternallyManaged
                                            description:[NSString stringWithFormat:@"Cannot push view %@; is externally managed", viewState.zoneName]];
                }];
                break;
            }
        }
    });

    if(failOp) {
        [self scheduleOperation:failOp];
        return failOp;
    }

    NSDictionary<OctagonState*, id>* pathDict =  @{
        CKKSStateProcessOutgoingQueue: @{
            CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
        },
    };

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:pathDict];
    OctagonStateTransitionWatcher* watcher = [[OctagonStateTransitionWatcher alloc] initNamed:@"push"
                                                                                  serialQueue:self.queue
                                                                                         path:path
                                                                               initialRequest:nil];
    [watcher timeout:300*NSEC_PER_SEC];
    [self.stateMachine registerStateTransitionWatcher:watcher];

    dispatch_sync(self.queue, ^{
        [self _onqueueProcessOutgoingQueue:ckoperationGroup];
    });

    return watcher.result;
}

- (void)_onqueueProcessOutgoingQueue:(CKOperationGroup* _Nullable)ckoperationGroup
{
    dispatch_assert_queue(self.queue);

    if(ckoperationGroup) {
        if(self.operationDependencies.currentOutgoingQueueOperationGroup) {
            ckkserror("ckks", self, "Throwing away CKOperationGroup(%@) in favor of (%@)", ckoperationGroup.name, self.operationDependencies.ckoperationGroup.name);
        } else {
            self.operationDependencies.currentOutgoingQueueOperationGroup = ckoperationGroup;
        }
    }

    [self.stateMachine _onqueueHandleFlag:CKKSFlagProcessOutgoingQueue];
    [self.outgoingQueueOperationScheduler trigger];
}

- (CKKSResultOperation*)resultsOfNextProcessIncomingQueueOperation {
    if(self.resultsOfNextIncomingQueueOperationOperation && [self.resultsOfNextIncomingQueueOperationOperation isPending]) {
        return self.resultsOfNextIncomingQueueOperationOperation;
    }

    // Else, make a new one.
    self.resultsOfNextIncomingQueueOperationOperation = [CKKSResultOperation named:[NSString stringWithFormat:@"wait-for-next-incoming-queue-operation"] withBlock:^{}];
    return self.resultsOfNextIncomingQueueOperationOperation;
}

- (CKKSResultOperation*)rpcFetchBecause:(CKKSFetchBecause*)why
{
    NSError* policyError = nil;
    if(![self waitForPolicy:5*NSEC_PER_SEC error:&policyError]) {
        CKKSResultOperation* failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            op.error = policyError;
        }];
        [self.operationQueue addOperation:failOp];
        return failOp;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        CKKSStateBeginFetch: @{
            CKKSStateFetch: @{
                CKKSStateFetchComplete: [OctagonStateTransitionPathStep success],
                CKKSStateResettingLocalData: @{
                    CKKSStateInitializing: @{
                        CKKSStateInitialized: @{
                            CKKSStateBeginFetch: @{
                                CKKSStateFetch: @{
                                    CKKSStateFetchComplete: [OctagonStateTransitionPathStep success],
                                },
                            },
                        },
                    },
                },
            },
        },
    }];
    OctagonStateTransitionWatcher* watcher = [[OctagonStateTransitionWatcher alloc] initNamed:@"rpc-fetch"
                                                                                  serialQueue:self.queue
                                                                                         path:path
                                                                               initialRequest:nil];
    [watcher timeout:300*NSEC_PER_SEC];
    [self.stateMachine registerStateTransitionWatcher:watcher];

    [self.operationDependencies.currentFetchReasons addObject:why];
    [self.stateMachine handleFlag:CKKSFlagFetchRequested];

    CKKSResultOperation* resultOp = [CKKSResultOperation named:@"check-keys" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        NSError* resultError = watcher.result.error;
        if(resultError) {
            ckksnotice_global("ckksrpc", "rpcFetch failed: %@", resultError);
            op.error = resultError;
            return;
        }

        ckksnotice_global("ckksrpc", "rpcFetch succeeded");
    }];

    [resultOp addDependency:watcher.result];
    [self.operationQueue addOperation:resultOp];

    return resultOp;
}

- (CKKSResultOperation*)rpcFetchAndProcessIncomingQueue:(NSSet<NSString*>* _Nullable)viewNames
                                                because:(CKKSFetchBecause*)why
                                   errorOnClassAFailure:(bool)failOnClassA
{
    NSError* policyError = nil;
    if(![self waitForPolicy:5*NSEC_PER_SEC error:&policyError]) {
        CKKSResultOperation* failOp = [CKKSResultOperation named:@"fail" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
            op.error = policyError;
        }];
        [self.operationQueue addOperation:failOp];
        return failOp;
    }

    // Are we okay with leaving class A items around?

    NSDictionary<OctagonState*, id>* incomingQueuePaths = nil;

    if(failOnClassA) {
        incomingQueuePaths = @{
            CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
        };
    } else {
        incomingQueuePaths = @{
            CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
            CKKSStateRemainingClassAIncomingItems: @{
                CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
            },
        };
    }

    NSDictionary<OctagonState*, id>* pathDict =  @{
        CKKSStateBeginFetch: @{
            CKKSStateFetch: @{
                CKKSStateFetchComplete: @{
                    CKKSStateProcessReceivedKeys: @{
                        CKKSStateCheckZoneHierarchies: @{
                            CKKSStateHealTLKShares: @{
                                CKKSStateProcessIncomingQueue: incomingQueuePaths,
                            },
                            CKKSStateTLKMissing: @{
                                CKKSStateCheckZoneHierarchies: @{
                                    CKKSStateHealTLKShares: @{
                                       CKKSStateProcessIncomingQueue: incomingQueuePaths,
                                    },
                                },
                            },
                        },
                    },
                },
            },
        },
    };

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:pathDict];
    OctagonStateTransitionWatcher* watcher = [[OctagonStateTransitionWatcher alloc] initNamed:@"fetch-and-process"
                                                                                  serialQueue:self.queue
                                                                                         path:path
                                                                               initialRequest:nil];
    [watcher timeout:300*NSEC_PER_SEC];
    [self.stateMachine registerStateTransitionWatcher:watcher];

    [self.operationDependencies.currentFetchReasons addObject:why];
    [self.stateMachine handleFlag:CKKSFlagFetchRequested];

    // But if, after all of that, the views of interest are not in a good state, we should error
    WEAKIFY(self);

    CKKSResultOperation* resultOp = [CKKSResultOperation named:@"check-keys" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);

        NSError* resultError = watcher.result.error;
        if(resultError) {
            ckksnotice_global("ckksrpc", "rpcFetchAndProcess failed: %@", resultError);
            op.error = resultError;
            return;
        }

        [self dispatchSyncWithReadOnlySQLTransaction:^{
            NSSet<NSString*>* requestedViewNames = viewNames ?: self.viewList;

            for(NSString* viewName in requestedViewNames) {
                CKKSKeychainViewState* viewState = [self viewStateForName:viewName];

                CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];
                if(!keyset.tlk) {
                    ckksnotice("ckks", self, "No local TLKs for %@; failing a fetch rpc", viewState);
                    op.error = [NSError errorWithDomain:CKKSErrorDomain
                                                   code:CKKSKeysMissing
                                            description:[NSString stringWithFormat:@"No local keys for %@; processing queue will fail", viewState]];
                    break;
                }
            }
        }];
    }];

    [resultOp addDependency:watcher.result];
    [self.operationQueue addOperation:resultOp];

    return resultOp;
}

- (CKKSResultOperation*)rpcProcessIncomingQueue:(NSSet<NSString*>*)viewNames
                           errorOnClassAFailure:(bool)failOnClassA
{
    // Are we okay with leaving class A items around?
    // TODO: viewNames?

    NSDictionary<OctagonState*, id>* incomingQueuePaths = nil;

    if(failOnClassA) {
        incomingQueuePaths = @{
            CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
        };
    } else {
        incomingQueuePaths = @{
            CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
            CKKSStateRemainingClassAIncomingItems: @{
                CKKSStateBecomeReady: [OctagonStateTransitionPathStep success],
            },
        };
    }

    NSDictionary<OctagonState*, id>* pathDict =  @{
        CKKSStateProcessIncomingQueue: incomingQueuePaths,
    };

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:pathDict];
    OctagonStateTransitionWatcher* watcher = [[OctagonStateTransitionWatcher alloc] initNamed:@"process-incoming-queue"
                                                                                  serialQueue:self.queue
                                                                                         path:path
                                                                               initialRequest:nil];
    [watcher timeout:300*NSEC_PER_SEC];
    [self.stateMachine registerStateTransitionWatcher:watcher];

    [self.stateMachine handleFlag:CKKSFlagProcessIncomingQueue];

    return watcher.result;
}

- (CKKSResultOperation*)rpcWaitForPriorityViewProcessing
{
    __block CKKSResultOperation* returnOp = nil;

    dispatch_sync(self.queue, ^{
        NSError* failError = nil;

        if(self.accountStatus != CKKSAccountStatusAvailable) {
            if(self.accountStatus == CKKSAccountStatusUnknown) {
                failError = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSErrorAccountStatusUnknown
                                          description:@"iCloud account status unknown."];
            } else {
                failError = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSNotLoggedIn
                                          description:@"User is not signed into iCloud."];
            }
        } else {
            if(self.operationDependencies.syncingPolicy == nil) {
                failError = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSErrorPolicyNotLoaded
                                          description:@"Syncing policy not yet loaded"];
            } else {
                if(self.trustStatus != CKKSAccountStatusAvailable) {
                    failError = [NSError errorWithDomain:CKKSErrorDomain
                                                     code:CKKSLackingTrust
                                              description:@"No iCloud Keychain Trust"];
                }
            }
        }

        if(failError != nil) {
            returnOp = [CKKSResultOperation named:@"rpcWaitForPriorityViewProcessing-fail"
                              withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
                ckksnotice_global("ckksrpc", "Returning failure for waitForPriorityViews: %@", failError);
                op.error = failError;
            }];
            [self.operationQueue addOperation:returnOp];
            return;
        }

        returnOp = self.priorityViewsProcessed.result;
        if(returnOp == nil) {
            ckksnotice_global("ckksrpc", "Returning success for waitForPriorityViews");
            returnOp = [CKKSResultOperation named:@"waitForPriority-succeed" withBlock:^{}];
            [self.operationQueue addOperation:returnOp];
            return;
        } else {
            ckksnotice_global("ckksrpc", "waitForPriorityViews pending on %@", self.priorityViewsProcessed);
        }
    });

    return returnOp;
}

- (void)scanLocalItems
{
    [self.stateMachine handleFlag:CKKSFlagScanLocalItems];
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

    CKKSUpdateDeviceStateOperation* op = [[CKKSUpdateDeviceStateOperation alloc] initWithOperationDependencies:self.operationDependencies
                                                                                                     rateLimit:rateLimit
                                                                                              ckoperationGroup:ckoperationGroup];
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

- (void)toggleHavoc:(void (^)(BOOL havoc, NSError* _Nullable error))reply
{
    __block BOOL ret = NO;
    dispatch_sync(self.queue, ^{
        self.havoc = !self.havoc;
        ret = self.havoc;
        ckksnotice_global("havoc", "Havoc is now %@", self.havoc ? @"ON" : @"OFF");
    });
    reply(ret, nil);
}

- (void)pcsMirrorKeysForServices:(NSDictionary<NSNumber*,NSArray<NSData*>*>*)services reply:(void (^)(NSDictionary<NSNumber*,NSArray<NSData*>*>* _Nullable result, NSError* _Nullable error))reply
{
    WEAKIFY(self);
    CKKSResultOperation* pcsMirrorKeysOp = [CKKSResultOperation named:@"pcs-mirror-keys" withBlock:^{
        STRONGIFY(self);
        [self dispatchSyncWithReadOnlySQLTransaction:^{
            NSMutableDictionary<NSNumber*,NSArray<NSData*>*>* result = [[NSMutableDictionary alloc] init];
            NSError* error = nil;

            for (NSNumber* serviceNum in services) {
                NSArray<NSData*>* mirrorKeys = [CKKSMirrorEntry pcsMirrorKeysForService:serviceNum matchingKeys:services[serviceNum] error:&error];
                if (mirrorKeys) {
                    result[serviceNum] = mirrorKeys;
                } else {
                    // Errors should not be typical, so bail immediately if we encounter one. */
                    ckksnotice_global("ckks", "Error getting PCS key hash for service %@: %@", serviceNum, error);
                    result = nil;
                    break;
                }
            }

            reply(result, error);
        }];
    }];

    [self scheduleOperation:pcsMirrorKeysOp];
}

- (void)xpc24HrNotification
{
    // Called roughly once every 24hrs
    [self.stateMachine handleFlag:CKKSFlag24hrNotification];
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

- (bool)_onqueueResetAllInflightOQE:(NSError**)error {
    dispatch_assert_queue(self.queue);

    for(CKKSKeychainViewState* viewState in self.operationDependencies.allCKKSManagedViews) {
        NSError* localError = nil;

        while(true) {
            NSArray<CKKSOutgoingQueueEntry*> * inflightQueueEntries = [CKKSOutgoingQueueEntry fetch:SecCKKSOutgoingQueueItemsAtOnce
                                                                                              state:SecCKKSStateInFlight
                                                                                             zoneID:viewState.zoneID
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
                [oqe intransactionMoveToState:SecCKKSStateNew viewState:viewState error:&localError];

                if(localError) {
                    ckkserror("ckks", self, "Error fixing up inflight OQE(%@): %@", oqe, localError);
                    if(error) {
                        *error = localError;
                    }
                    return false;
                }
            }
        }
    }

    return true;
}

- (void)dispatchSyncWithSQLTransaction:(CKKSDatabaseTransactionResult (^)(void))block
{
    [self.operationDependencies.databaseProvider dispatchSyncWithSQLTransaction:block];
}

- (void)dispatchSyncWithReadOnlySQLTransaction:(void (^)(void))block
{
    [self.operationDependencies.databaseProvider dispatchSyncWithReadOnlySQLTransaction:block];
}

- (BOOL)insideSQLTransaction
{
    return [self.operationDependencies.databaseProvider insideSQLTransaction];
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
    ckksnotice("ckkszone", self, "Received notification of CloudKit account status change, moving from %@ to %@",
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
            ckksnotice("ckkszone", self, "Account status has become undetermined. Pausing!");

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

    dispatch_sync(self.queue, ^{
        ckksnotice("ckkstrust", self, "Beginning trusted operation");
        self.operationDependencies.peerProviders = peerProviders;
        self.operationDependencies.requestPolicyCheck = requestPolicyCheck;

        CKKSAccountStatus oldTrustStatus = self.trustStatus;

        self.suggestTLKUpload = suggestTLKUpload;

        self.trustStatus = CKKSAccountStatusAvailable;
        [self.trustStatusKnown fulfill];
        [self.stateMachine _onqueueHandleFlag:CKKSFlagBeginTrustedOperation];

        // Re-process the key hierarchy, just in case the answer is now different
        [self.stateMachine _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];

        if(oldTrustStatus == CKKSAccountStatusNoAccount) {
            ckksnotice("ckkstrust", self, "Moving from an untrusted status; we need to process incoming queue and scan for any new items");

            [self.stateMachine _onqueueHandleFlag:CKKSFlagProcessIncomingQueue];
            [self.stateMachine _onqueueHandleFlag:CKKSFlagScanLocalItems];

            // Since we just gained trust, we'll try to process the priority views first. Set this up again,
            // since the last one likely failed due to trust issues
            [self onqueueCreatePriorityViewsProcessedWatcher];
        }
    });
}

- (void)endTrustedOperation
{
    dispatch_sync(self.queue, ^{
        ckksnotice("ckkstrust", self, "Ending trusted operation");

        self.operationDependencies.peerProviders = @[];

        self.suggestTLKUpload = nil;

        self.trustStatus = CKKSAccountStatusNoAccount;
        [self.trustStatusKnown fulfill];
        [self.stateMachine _onqueueHandleFlag:CKKSFlagEndTrustedOperation];
    });
}

- (TPSyncingPolicy* _Nullable)syncingPolicy
{
    return self.operationDependencies.syncingPolicy;
}

- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy
{
    return [self setCurrentSyncingPolicy:syncingPolicy policyIsFresh:NO];
}

- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy*)syncingPolicy policyIsFresh:(BOOL)policyIsFresh
{
    if(syncingPolicy == nil) {
        ckksnotice_global("ckks-policy", "Nil syncing policy presented; ignoring");
        return NO;
    }

    NSSet<NSString*>* viewNames = syncingPolicy.viewList;
    ckksnotice_global("ckks-policy", "New syncing policy: %@ (%@) views: %@", syncingPolicy,
                      policyIsFresh ? @"fresh" : @"cached",
                      viewNames);

    // The externally managed views are not set by policy, but are limited by the current OS
    NSSet<NSString*>* externallyManagedViewNames = [NSSet setWithArray:@[CKKSSEViewPTA, CKKSSEViewPTC]];

    if(self.viewAllowList) {
        ckksnotice_global("ckks-policy", "Intersecting view list with allow list: %@", self.viewAllowList);
        NSMutableSet<NSString*>* set = [viewNames mutableCopy];
        [set intersectSet:self.viewAllowList];

        viewNames = set;
        ckksnotice_global("ckks-policy", "Final list: %@", viewNames);

        set = [externallyManagedViewNames mutableCopy];
        [set intersectSet:self.viewAllowList];
        externallyManagedViewNames = set;

        ckksnotice_global("ckks-policy", "Final list of externally-managed view names: %@", externallyManagedViewNames);
    }

    __block BOOL newViews = NO;
    __block BOOL disappearedViews = NO;

    [self.operationDependencies.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        TPSyncingPolicy* oldPolicy = self.operationDependencies.syncingPolicy;

        NSMutableSet<CKKSKeychainViewState*>* viewStates = [self.operationDependencies.allViews mutableCopy];
        NSMutableSet<CKKSKeychainViewState*>* viewStatesToRemove = [NSMutableSet set];

        BOOL newPriorityViews = NO;

        for(CKKSKeychainViewState* viewState in viewStates) {
            if([viewNames containsObject:viewState.zoneID.zoneName]) {

                BOOL oldEnabled = [oldPolicy isSyncingEnabledForView:viewState.zoneID.zoneName];
                BOOL enabled = [syncingPolicy isSyncingEnabledForView:viewState.zoneID.zoneName];

                ckksnotice("ckks", viewState.zoneID, "Syncing for %@ is now %@ (used to be %@) (policy: %@)",
                           viewState.zoneID.zoneName,
                           enabled ? @"enabled" : @"paused",
                           oldEnabled ? @"enabled" : @"paused",
                           syncingPolicy);
            } else if([externallyManagedViewNames containsObject:viewState.zoneID.zoneName]) {
                // Ignore externally-managed views.
            } else {
                ckksnotice_global("ckks-policy", "Stopping old view %@", viewState.zoneID.zoneName);
                [viewStatesToRemove addObject:viewState];
                disappearedViews = YES;
            }
        }

        [viewStates minusSet:viewStatesToRemove];

        NSSet<NSString*>* allViewNames = [viewNames setByAddingObjectsFromSet:externallyManagedViewNames];

        for(NSString* viewName in allViewNames) {
            CKKSKeychainViewState* viewState = nil;

            for(CKKSKeychainViewState* vs in viewStates) {
                if([vs.zoneID.zoneName isEqualToString:viewName]) {
                    viewState = vs;
                    break;
                }
            }

            if(viewState != nil) {
                continue;
            }

            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewName];

            bool ckksManaged = [viewNames containsObject:viewName];
            BOOL zoneIsNew = ckse.changeToken == nil;

            viewState = [self createViewState:viewName zoneIsNew:zoneIsNew ckksManagedView:ckksManaged];
            [viewStates addObject:viewState];

            BOOL priorityView = [syncingPolicy.priorityViews containsObject:viewName];

            ckksnotice("ckks", viewState.zoneID, "Created %@ %@ view %@",
                       priorityView ? @"priority" : @"normal",
                       ckksManaged ? @"CKKS" : @"externally-managed",
                       viewState);

            if(priorityView && zoneIsNew) {
                ckksnotice("ckks", viewState.zoneID, "Initializing a priority view for the first time");
                newPriorityViews = YES;
            }
            if(priorityView && ckse.moreRecordsInCloudKit) {
                ckksnotice("ckks", viewState.zoneID, "A priority view has more records in CloudKit; treating as new");
                newPriorityViews = YES;
            }

            newViews = YES;
        }

        [self.operationDependencies applyNewSyncingPolicy:syncingPolicy
                                               viewStates:viewStates];
        [self.stateMachine _onqueueHandleFlag:CKKSFlagCheckQueues];

        if(newPriorityViews) {
            [self onqueueCreatePriorityViewsProcessedWatcher];

            [self.stateMachine _onqueueHandleFlag:CKKSFlagNewPriorityViews];
        }

        for(CKKSKeychainViewState* viewState in viewStates) {
            [self.zoneChangeFetcher registerClient:self zoneID:viewState.zoneID];
        }
    }];

    if(policyIsFresh) {
        [self.stateMachine handleFlag:CKKSFlagProcessIncomingQueueWithFreshPolicy];
        [self.stateMachine handleFlag:CKKSFlagProcessIncomingQueue];
    }

    if(self.itemModificationsBeforePolicyLoaded) {
        ckksnotice_global("ckks-policy", "Issuing scan suggestions to handle missed items");
        self.itemModificationsBeforePolicyLoaded = NO;
        [self .stateMachine handleFlag:CKKSFlagScanLocalItems];
    }

    // Retrigger the analytics setup, so that our views will report status
    [[CKKSViewManager manager] setupAnalytics];

    // The policy is considered loaded once the views have been created
    [self.policyLoaded fulfill];
    return newViews || disappearedViews;
}

- (void)onqueueCreatePriorityViewsProcessedWatcher {
    // Note: once CKKS makes it into ready or restarts with non-priority views, then the initial download for priority views is complete.
    // Be careful about waiting on this if CKKS is not trusted.

    // Note that we fail on CKKSStateLoseTrust, not CKKSStateWaitForTrust. This allows us to handle the racy situation where
    // the state machine is in CKKSStateWaitForTrust, and -beginTrust is called, and creates a new one of these. If we failed on
    // CKKSStateWaitForTrust, this watcher would fail immediately.
    //
    //
    self.priorityViewsProcessed = [[OctagonStateMultiStateArrivalWatcher alloc] initNamed:@"wait-for-priority-view-processing"
                                                                              serialQueue:self.queue
                                                                                   states:[NSSet setWithArray:@[
                                                                                    CKKSStateReady,
                                                                                    CKKSStateExpandToHandleAllViews,
                                                                                   ]]
                                                                               failStates:@{
        CKKSStateLoggedOut: [NSError errorWithDomain:CKKSErrorDomain code:CKKSNotLoggedIn description:@"CloudKit account not present"],
        CKKSStateError: [NSError errorWithDomain:CKKSErrorDomain code:CKKSErrorNotSupported description:@"CKKS currently in error state"],
    }];
    [self.stateMachine _onqueueRegisterMultiStateArrivalWatcher:self.priorityViewsProcessed];
}

- (CKKSKeychainViewState*)createViewState:(NSString*)zoneName
                                zoneIsNew:(BOOL)zoneIsNew
                          ckksManagedView:(BOOL)ckksManagedView
{
    WEAKIFY(self);

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
                                                                                      continuingDelay:SecCKKSTestsEnabled() ? 3 * NSEC_PER_SEC : 120*NSEC_PER_SEC
                                                                                     keepProcessAlive:true
                                                                            dependencyDescriptionCode:CKKSResultDescriptionPendingViewChangedScheduling
                                                                                                block:^{
        STRONGIFY(self);
        NSDistributedNotificationCenter *center = [self.cloudKitClassDependencies.nsdistributednotificationCenterClass defaultCenter];

        [center postNotificationName:@"com.apple.security.view-become-ready"
                              object:nil
                            userInfo:@{ @"view" : zoneName ?: @"unknown" }
                             options:0];

        [self.cloudKitClassDependencies.notifierClass post:[NSString stringWithFormat:@"com.apple.security.view-ready.%@", zoneName]];
    }];

    CKKSKeychainViewState* state = [[CKKSKeychainViewState alloc] initWithZoneID:[[CKRecordZoneID alloc] initWithZoneName:zoneName ownerName:CKCurrentUserDefaultName]
                                                                 ckksManagedView:ckksManagedView
                                                      notifyViewChangedScheduler:notifyViewChangedScheduler
                                                        notifyViewReadyScheduler:notifyViewReadyScheduler];
    if(zoneIsNew) {
        state.launch.firstLaunch = true;
    }
    return state;
}

- (void)setSyncingViewsAllowList:(NSSet<NSString*>*)viewNames
{
    self.viewAllowList = viewNames;
}

- (CKKSKeychainViewState* _Nullable)viewStateForName:(NSString*)viewName
{
    return [self.operationDependencies viewStateForName:viewName];
}

- (void)_onqueuePrioritizePriorityViews
{
    // Let's limit operation to just our priority views (if there are any)
    NSSet<CKKSKeychainViewState*>* priorityViews = self.operationDependencies.allPriorityViews;
    if(priorityViews.count > 0) {
        [self.operationDependencies limitOperationToPriorityViews];
        ckksnotice("ckksviews", self, "Restricting operation to priority views: %@", self.operationDependencies.views);
    }
}

#pragma mark - CKKSChangeFetcherClient

- (BOOL)zoneIsReadyForFetching:(CKRecordZoneID*)zoneID
{
    __block BOOL ready = NO;

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        ready = (bool)[self _onQueueZoneIsReadyForFetching:zoneID];
    }];

    return ready;
}

- (BOOL)_onQueueZoneIsReadyForFetching:(CKRecordZoneID*)zoneID
{
    dispatch_assert_queue(self.queue);
    if(self.accountStatus != CKKSAccountStatusAvailable) {
        ckksnotice("ckksfetch", self, "Not participating in fetch: not logged in");
        return NO;
    }

    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:zoneID.zoneName];

    if(!ckse.ckzonecreated) {
        ckksnotice("ckksfetch", zoneID, "Not participating in fetch: zone not created yet");
        return NO;
    }

    CKKSKeychainViewState* activeViewState = nil;
    for(CKKSKeychainViewState* viewState in self.operationDependencies.views) {
        if([viewState.zoneName isEqualToString:zoneID.zoneName]) {
            activeViewState = viewState;
            break;
        }
    }

    if(activeViewState == nil) {
        // View is not active. Do not fetch.
        ckksnotice("ckksfetch", zoneID, "Not participating in fetch: zone is not active");
        return NO;
    }

    return YES;
}

- (CKKSCloudKitFetchRequest*)participateInFetch:(CKRecordZoneID*)zoneID
{
    __block CKKSCloudKitFetchRequest* request = [[CKKSCloudKitFetchRequest alloc] init];

    [self dispatchSyncWithReadOnlySQLTransaction:^{
        if (![self _onQueueZoneIsReadyForFetching:zoneID]) {
            ckksnotice("ckksfetch", self, "skipping fetch for %@; zone is not ready", zoneID);
            return;
        }

        request.participateInFetch = true;

        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:zoneID.zoneName];
        if(!ckse) {
            ckkserror("ckksfetch", self, "couldn't fetch zone change token for %@", zoneID.zoneName);
            return;
        }
        request.changeToken = ckse.changeToken;
    }];

    return request;
}

- (void)changesFetched:(NSArray<CKRecord*>*)changedRecords
      deletedRecordIDs:(NSArray<CKKSCloudKitDeletion*>*)deletedRecords
                zoneID:(CKRecordZoneID*)zoneID
        newChangeToken:(CKServerChangeToken*)newChangeToken
            moreComing:(BOOL)moreComing
                resync:(BOOL)resync
{
    [self dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        if(self.halted) {
            ckksnotice("ckksfetch", zoneID, "Dropping fetch due to halted operation");
            return CKKSDatabaseTransactionRollback;
        }

        for (CKRecord* record in changedRecords) {
            [self.operationDependencies intransactionCKRecordChanged:record resync:resync];
        }

        for (CKKSCloudKitDeletion* deletion in deletedRecords) {
            [self.operationDependencies intransactionCKRecordDeleted:deletion.recordID recordType:deletion.recordType resync:resync];
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
                ckksnotice("ckksresync", zoneID, "In a resync, but there's More Coming. Waiting to scan for extra items.");

            } else {
                // Scan through all CKMirrorEntries and determine if any exist that CloudKit didn't tell us about
                ckksnotice("ckksresync", zoneID, "Comparing local UUIDs against the CloudKit list");
                NSMutableArray<NSString*>* uuids = [[CKKSMirrorEntry allUUIDs:zoneID error:&error] mutableCopy];

                for(NSString* uuid in uuids) {
                    if([self.resyncRecordsSeen containsObject:uuid]) {
                        ckksnotice("ckksresync", zoneID, "UUID %@ is still in CloudKit; carry on.", uuid);
                    } else {
                        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid zoneID:zoneID error:&error];
                        if(error != nil) {
                            ckkserror("ckksresync", zoneID, "Couldn't read an item from the database, but it used to be there: %@ %@", uuid, error);
                            continue;
                        }
                        if(!ckme) {
                            ckkserror("ckksresync", zoneID, "Couldn't read ckme(%@) from database; continuing", uuid);
                            continue;
                        }

                        ckkserror("ckksresync", zoneID, "BUG: Local item %@ not found in CloudKit, deleting", uuid);
                        [self.operationDependencies intransactionCKRecordDeleted:ckme.item.storedCKRecord.recordID recordType:ckme.item.storedCKRecord.recordType resync:resync];
                    }
                }

                // Now that we've inspected resyncRecordsSeen, reset it for the next time through
                self.resyncRecordsSeen = nil;
            }
        }

        CKKSZoneStateEntry* state = [CKKSZoneStateEntry state:zoneID.zoneName];
        state.lastFetchTime = [NSDate date]; // The last fetch happened right now!
        state.changeToken = newChangeToken;
        state.moreRecordsInCloudKit = moreComing;
        [state saveToDatabase:&error];
        if(error) {
            ckkserror("ckksfetch", zoneID, "Couldn't save new server change token: %@", error);
        }

        if(changedRecords.count == 0 && deletedRecords.count == 0 && !moreComing && !resync) {
            ckksinfo("ckksfetch", zoneID, "No record changes in this fetch");
        } else {
            if(!moreComing) {
                ckksnotice("ckksfetch", self, "Beginning incoming processing for %@", zoneID);
                [self.stateMachine _onqueueHandleFlag:CKKSFlagProcessIncomingQueue];
                // TODO: likely can be removed, once fetching no longer is a complicated upcall dance
            }
        }

        ckksnotice("ckksfetch", zoneID, "Finished processing changes for %@", zoneID);

        return CKKSDatabaseTransactionCommit;
    }];

    // Now that we've committed the transaction, should we send any notifications?
    CKKSKeychainViewState* viewState = [self.operationDependencies viewStateForName:zoneID.zoneName];

    if(deletedRecords.count > 0) {
        // Not strictly true, but will properly send notifications for deleted CIPs
        [viewState.notifyViewChangedScheduler trigger];
    }

    if(!viewState.ckksManagedView) {
        if(changedRecords.count > 0 || deletedRecords.count > 0) {
            [viewState.notifyViewChangedScheduler trigger];
        }

        if(!moreComing) {
            // We've downloaded all there is for this view. Let the client know about it.
            [viewState launchComplete];
        }
    }
}

- (bool)ckErrorOrPartialError:(NSError *)error
                      isError:(CKErrorCode)errorCode
                       zoneID:(CKRecordZoneID*)zoneID
{
    if((error.code == errorCode) && [error.domain isEqualToString:CKErrorDomain]) {
        return true;
    } else if((error.code == CKErrorPartialFailure) && [error.domain isEqualToString:CKErrorDomain]) {
        NSDictionary* partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];

        NSError* partialError = partialErrors[zoneID];
        if ((partialError.code == errorCode) && [partialError.domain isEqualToString:CKErrorDomain]) {
            return true;
        }
    }
    return false;
}

- (bool)shouldRetryAfterFetchError:(NSError*)error
                            zoneID:(CKRecordZoneID*)zoneID
{
    bool isChangeTokenExpiredError = [self ckErrorOrPartialError:error
                                                         isError:CKErrorChangeTokenExpired
                                                          zoneID:zoneID];
    if(isChangeTokenExpiredError) {
        ckkserror("ckks", zoneID, "Received notice that our change token is out of date (for %@). Resetting local data...", zoneID);

        [self.stateMachine handleFlag:CKKSFlagChangeTokenExpired];
        return true;
    }

    bool isDeletedZoneError = [self ckErrorOrPartialError:error
                                                  isError:CKErrorZoneNotFound
                                                   zoneID:zoneID];
    if(isDeletedZoneError) {
        ckkserror("ckks", zoneID, "Received notice that our zone(%@) does not exist. Resetting local data.", zoneID);

        [self.stateMachine handleFlag:CKKSFlagCloudKitZoneMissing];
        return false;
    }

    if([error.domain isEqualToString:CKErrorDomain] && (error.code == CKErrorBadContainer)) {
        ckkserror("ckks", zoneID, "Received notice that our container does not exist. Nothing to do.");
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

- (BOOL)waitForFetchAndIncomingQueueProcessing
{
    CKKSCondition* fetchComplete = self.stateMachine.stateConditions[CKKSStateFetchComplete];

    NSOperation* op = [self.zoneChangeFetcher inflightFetch];
    if(op) {
        [op waitUntilFinished];
        [fetchComplete wait:5*NSEC_PER_SEC];
    }

    // If that fetch did anything to the state machine, wait for it to shake out. 109 chosen as a long time that isn't used elsewhere.
    BOOL ret = 0 == [self.stateMachine.paused wait:109 * NSEC_PER_SEC];
    return ret;
}

- (BOOL)waitForKeyHierarchyReadiness {
    BOOL allSuccess = YES;
    for(CKKSKeychainViewState* viewState in self.operationDependencies.allCKKSManagedViews) {
        if(0 != [viewState.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:300 * NSEC_PER_SEC]) {
            allSuccess = NO;
        }
    }

    return allSuccess;
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

- (BOOL)waitUntilAllOperationsAreFinished
{
    return 0 == [self.stateMachine.paused wait:189 * NSEC_PER_SEC];
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
}

- (void)cancelAllOperations {
    // We don't own the zoneChangeFetcher, so don't cancel it

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
    for(CKKSKeychainViewState* viewState in self.operationDependencies.allViews) {
        [viewState.notifyViewChangedScheduler cancel];
        [viewState.notifyViewReadyScheduler cancel];
    }
}

#pragma mark - RPCs and RPC helpers

- (BOOL)waitForPolicy:(uint64_t)timeout error:(NSError**)error
{
    if([self.policyLoaded wait:timeout] != 0) {
        ckkserror_global("ckks", "Haven't yet received a syncing policy");

        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSErrorPolicyNotLoaded
                                     userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"CKKS syncing policy not yet loaded"]}];

        }
        return NO;
    }

    return YES;
}

- (CKKSKeychainViewState* _Nullable)policyDependentViewStateForName:(NSString*)name
                                                              error:(NSError**)error
{
    if(![self waitForPolicy:5*NSEC_PER_SEC error:error]) {
        return nil;
    }

    __block CKKSKeychainViewState* viewState = nil;

    dispatch_sync(self.queue, ^{
        for(CKKSKeychainViewState* vs in self.operationDependencies.allCKKSManagedViews) {
            if([vs.zoneID.zoneName isEqualToString:name]) {
                viewState = vs;
                break;
            }
        }
    });

    if(!viewState) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoSuchView
                                     userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for '%@'", name]}];
        }
        return nil;
    }

    if(!viewState.ckksManagedView) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSErrorViewIsExternallyManaged
                                  description:[NSString stringWithFormat:@"Cannot get view %@; is externally managed", viewState.zoneName]];
        }
        return nil;
    }

    return viewState;
}

- (BOOL)waitUntilReadyForRPCForOperation:(NSString*)opName
                                    fast:(BOOL)fast
                errorOnNoCloudKitAccount:(BOOL)errorOnNoCloudKitAccount
                    errorOnPolicyMissing:(BOOL)errorOnPolicyMissing
                                   error:(NSError**)error
{
    // Ensure we've actually set up, but don't wait too long. Clients get impatient.
    if([[CKKSViewManager manager].completedSecCKKSInitialize wait:5*NSEC_PER_SEC]) {
        ckkserror_global("ckks", "Haven't yet initialized SecDb; expect failure");
    }

    // Not being in a CloudKit account is an automatic failure.
    // But, wait a good long while for the CloudKit account state to be known (in the case of daemon startup)
    [self.accountStateKnown wait:((SecCKKSTestsEnabled() || fast) ? 500 * NSEC_PER_MSEC : 2*NSEC_PER_SEC)];

    CKKSAccountStatus accountStatus = self.accountStatus;
    if(errorOnNoCloudKitAccount && accountStatus != CKKSAccountStatusAvailable) {
        NSError* localError = nil;

        if(accountStatus == CKKSAccountStatusUnknown) {
            localError = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSErrorAccountStatusUnknown
                                      description:@"iCloud account status unknown."];
        } else {
            localError = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNotLoggedIn
                                      description:@"User is not signed into iCloud."];
        }

        ckksnotice("ckkscurrent", self, "Rejecting %@ RPC since we don't have an iCloud account: %@", opName, localError);
        if(error) {
            *error = localError;
        }
        return NO;
    }

    // If the caller doesn't mind if the policy is missing, wait some, but not the full 5s
    if(errorOnPolicyMissing) {
        if(![self waitForPolicy:5*NSEC_PER_SEC error:error]) {
            ckkserror_global("ckks", "Haven't yet received a policy; failing %@", opName);
            return NO;
        }
    } else {
        if(![self waitForPolicy:0.5*NSEC_PER_SEC error:nil]) {
            ckkserror_global("ckks", "Haven't yet received a policy; expect failure later doing %@", opName);
        }
    }

    return YES;
}

- (NSArray<NSString*>*)viewsForPeerID:(NSString*)peerID error:(NSError**)error
{
    __block NSMutableArray<NSString*> *viewsForPeer = [NSMutableArray array];
    __block NSError *localError = nil;
    [self dispatchSyncWithReadOnlySQLTransaction:^{
        for(CKKSKeychainViewState* viewState in self.operationDependencies.allViews) {
            CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];
            if (keyset.error) {
                ckkserror("ckks", viewState.zoneID, "error loading keyset: %@", keyset.error);
                localError = keyset.error;
            } else {
                if (keyset.currentTLKPointer.currentKeyUUID) {
                    NSArray<CKKSTLKShareRecord*>* tlkShares = [CKKSTLKShareRecord allFor:peerID keyUUID:keyset.currentTLKPointer.currentKeyUUID zoneID:viewState.zoneID error:&localError];
                    if (tlkShares && localError == nil) {
                        [viewsForPeer addObject:viewState.zoneName];
                    }
                }
            }
        }
    }];

    if (error) {
        *error = localError;
    }
    return viewsForPeer;
}

- (void)rpcStatus:(NSString* _Nullable)viewName
        fast:(BOOL)fast
        waitForNonTransientState:(dispatch_time_t)nonTransientStateTimeout
        reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error))reply
{
    // Now, query the views about their status. Don't wait for the policy to be loaded
    NSError* error = nil;

    BOOL readyForRPC = [self waitUntilReadyForRPCForOperation:@"status"
                                                         fast:fast
                                     errorOnNoCloudKitAccount:NO
                                         errorOnPolicyMissing:NO
                                                        error:&error];

    if(!readyForRPC) {
        reply(nil, error);
        return;
    }

    OctagonStateMultiStateArrivalWatcher* waitForTransient = [[OctagonStateMultiStateArrivalWatcher alloc] initNamed:@"rpc-watcher"
                                                                                                         serialQueue:self.queue
                                                                                                              states:[NSSet setWithArray:@[
                                                                                                                  CKKSStateLoggedOut,
                                                                                                                  CKKSStateReady,
                                                                                                                  CKKSStateError,
                                                                                                                  CKKSStateWaitForTrust,
                                                                                                                  OctagonStateMachineHalted,
                                                                                                              ]]];
    [waitForTransient timeout:nonTransientStateTimeout];
    [self.stateMachine registerMultiStateArrivalWatcher:waitForTransient];

    WEAKIFY(self);
    CKKSResultOperation* statusOp = [CKKSResultOperation named:@"status-rpc" withBlock:^{
        STRONGIFY(self);
        NSMutableArray* result = [NSMutableArray array];

        if (fast == false) {
            // Get account state, even wait for it a little
            [self.accountTracker.ckdeviceIDInitialized wait:1*NSEC_PER_SEC];

            NSString *deviceID = self.accountTracker.ckdeviceID;
            NSError *deviceIDError = self.accountTracker.ckdeviceIDError;
            NSDate *lastCKKSPush = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastCKKSPush];

#define stringify(obj) CKKSNilToNSNull([obj description])
            NSDictionary* global = @{
                @"view":                @"global",
                @"reachability":        self.reachabilityTracker.currentReachability ? @"network" : @"no-network",
                @"ckdeviceID":          CKKSNilToNSNull(deviceID),
                @"ckdeviceIDError":     CKKSNilToNSNull(deviceIDError),
                @"lockstatetracker":    stringify(self.lockStateTracker),
                @"cloudkitRetryAfter":  stringify(self.operationDependencies.zoneModifier.cloudkitRetryAfter),
                @"lastCKKSPush":        CKKSNilToNSNull(lastCKKSPush),
                @"policy":              stringify(self.syncingPolicy),
                @"viewsFromPolicy":     @"yes",
                @"ckaccountstatus":     self.accountStatus == CKAccountStatusCouldNotDetermine ? @"could not determine" :
                    self.accountStatus == CKAccountStatusAvailable         ? @"logged in" :
                    self.accountStatus == CKAccountStatusRestricted        ? @"restricted" :
                    self.accountStatus == CKAccountStatusNoAccount         ? @"logged out" : @"unknown",

                @"accounttracker":      stringify(self.accountTracker),
                @"fetcher":             stringify(self.zoneChangeFetcher),
                @"ckksstate":           CKKSNilToNSNull(self.stateMachine.currentState),

                @"lastIncomingQueueOperation":         stringify(self.lastIncomingQueueOperation),
                @"lastNewTLKOperation":                stringify(self.lastNewTLKOperation),
                @"lastOutgoingQueueOperation":         stringify(self.lastOutgoingQueueOperation),
                @"lastProcessReceivedKeysOperation":   stringify(self.lastProcessReceivedKeysOperation),
                @"lastReencryptOutgoingItemsOperation":stringify(self.lastReencryptOutgoingItemsOperation),
            };
            [result addObject:global];
        }

        [self dispatchSyncWithReadOnlySQLTransaction:^{
            NSMutableArray<CKKSKeychainViewState*>* viewsToStatus = [NSMutableArray array];
            for(CKKSKeychainViewState* viewState in self.operationDependencies.allViews) {
                if(viewName == nil || [viewName isEqualToString:viewState.zoneName]) {
                    [viewsToStatus addObject:viewState];
                }
            }

            NSArray<CKKSKeychainViewState*>* sortedViewsToStatus = [viewsToStatus sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"zoneName" ascending:YES]]];

            for(CKKSKeychainViewState* viewState in sortedViewsToStatus) {
                ckksnotice("ckks", viewState.zoneID, "Building status for %@", viewState);

                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneName];
                NSDictionary* status = [self fastStatus:viewState zoneStateEntry:ckse];

                if(!fast) {
                    NSDictionary* slowStatus = [self intransactionSlowStatus:viewState];

                    NSMutableDictionary* merged = [status mutableCopy];
                    [merged addEntriesFromDictionary:slowStatus];
                    status = merged;
                }
                ckksinfo("ckks", viewState, "Status is %@", status);
                if(status) {
                    [result addObject:status];
                }
            }
        }];

        reply(result, nil);
    }];

    [statusOp addDependency:waitForTransient.result];
    [self.operationQueue addOperation:statusOp];
    return;

}

- (NSDictionary*)intransactionSlowStatus:(CKKSKeychainViewState*)viewState
{
#define stringify(obj) CKKSNilToNSNull([obj description])
#define boolstr(obj) (!!(obj) ? @"yes" : @"no")
    NSError* error = nil;

    CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];
    if(keyset.error) {
        ckkserror("ckks", viewState.zoneID, "error loading keyset: %@", keyset.error);
    }

    // Map deviceStates to strings to avoid NSXPC issues. Obj-c, why is this so hard?
    NSMutableArray<NSString*>* mutDeviceStates = [[NSMutableArray alloc] init];
    NSMutableArray<NSString*>* mutTLKShares = [[NSMutableArray alloc] init];

    @autoreleasepool {
        NSArray* deviceStates = [CKKSDeviceStateEntry allInZone:viewState.zoneID error:&error];
        [deviceStates enumerateObjectsUsingBlock:^(id _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [mutDeviceStates addObject:[obj description]];
        }];

        NSArray* tlkShares = [CKKSTLKShareRecord allForUUID:keyset.currentTLKPointer.currentKeyUUID zoneID:viewState.zoneID error:&error];
        [tlkShares enumerateObjectsUsingBlock:^(id _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
            [mutTLKShares addObject:[obj description]];
        }];
    }

    if(viewState.ckksManagedView) {
        return @{
            @"statusError":         stringify(error),
            @"oqe":                 CKKSNilToNSNull([CKKSOutgoingQueueEntry countsByStateInZone:viewState.zoneID error:&error]),
            @"iqe":                 CKKSNilToNSNull([CKKSIncomingQueueEntry countsByStateInZone:viewState.zoneID error:&error]),
            @"ckmirror":            CKKSNilToNSNull([CKKSMirrorEntry        countsByParentKey:viewState.zoneID error:&error]),
            @"devicestates":        CKKSNilToNSNull(mutDeviceStates),
            @"tlkshares":           CKKSNilToNSNull(mutTLKShares),
            @"keys":                CKKSNilToNSNull([CKKSKey countsByClass:viewState.zoneID error:&error]),
            @"currentTLK":          CKKSNilToNSNull(keyset.tlk.uuid),
            @"currentClassA":       CKKSNilToNSNull(keyset.classA.uuid),
            @"currentClassC":       CKKSNilToNSNull(keyset.classC.uuid),
            @"currentTLKPtr":       CKKSNilToNSNull(keyset.currentTLKPointer.currentKeyUUID),
            @"currentClassAPtr":    CKKSNilToNSNull(keyset.currentClassAPointer.currentKeyUUID),
            @"currentClassCPtr":    CKKSNilToNSNull(keyset.currentClassCPointer.currentKeyUUID),
            @"itemsyncing":         [self.operationDependencies.syncingPolicy isSyncingEnabledForView:viewState.zoneID.zoneName] ? @"enabled" : @"paused",
        };
    } else {
        return @{
            @"statusError":         stringify(error),
            @"tlkshares":           CKKSNilToNSNull(mutTLKShares),
            @"currentTLK":          CKKSNilToNSNull(keyset.tlk.uuid),
            @"currentTLKPtr":       CKKSNilToNSNull(keyset.currentTLKPointer.currentKeyUUID),
        };
    }
}

- (NSDictionary*)fastStatus:(CKKSKeychainViewState*)viewState zoneStateEntry:(CKKSZoneStateEntry*)ckse
{
    return @{
            @"view":                CKKSNilToNSNull(viewState.zoneName),
            @"zoneCreated":         boolstr(ckse.ckzonecreated),
            @"zoneSubscribed":      boolstr(ckse.ckzonesubscribed),
            @"keystate":            CKKSNilToNSNull(viewState.viewKeyHierarchyState),
            @"ckksManaged":         boolstr(viewState.ckksManagedView),
            @"statusError":         [NSNull null],
            @"launchSequence":      CKKSNilToNSNull([viewState.launch eventsByTime]),
        };
}

#endif /* OCTAGON */
@end
