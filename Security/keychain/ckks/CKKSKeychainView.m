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
#import "keychain/ckks/CKKSDeviceStateEntry.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSUpdateCurrentItemPointerOperation.h"
#import "keychain/ckks/CKKSUpdateDeviceStateOperation.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CloudKitCategories.h"

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

#if OCTAGON
@interface CKKSKeychainView()
@property bool setupSuccessful;
@property bool keyStateFetchRequested;
@property bool keyStateFullRefetchRequested;
@property bool keyStateProcessRequested;
@property (atomic) NSString *activeTLK;

@property (readonly) Class<CKKSNotifier> notifierClass;

@property CKKSNearFutureScheduler* initializeScheduler;

@property CKKSResultOperation* processIncomingQueueAfterNextUnlockOperation;

@property NSMutableDictionary<NSString*, SecBoolNSErrorCallback>* pendingSyncCallbacks;
@end
#endif

@implementation CKKSKeychainView
#if OCTAGON

- (instancetype)initWithContainer:     (CKContainer*) container
                             zoneName: (NSString*) zoneName
                       accountTracker:(CKKSCKAccountStateTracker*) accountTracker
                     lockStateTracker:(CKKSLockStateTracker*) lockStateTracker
                     savedTLKNotifier:(CKKSNearFutureScheduler*) savedTLKNotifier
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
                        notifierClass: (Class<CKKSNotifier>) notifierClass
{

    if(self = [super initWithContainer:container
                              zoneName:zoneName
                        accountTracker:accountTracker
  fetchRecordZoneChangesOperationClass:fetchRecordZoneChangesOperationClass
     modifySubscriptionsOperationClass:modifySubscriptionsOperationClass
       modifyRecordZonesOperationClass:modifyRecordZonesOperationClass
                    apsConnectionClass:apsConnectionClass]) {
        __weak __typeof(self) weakSelf = self;

        _incomingQueueOperations = [NSHashTable weakObjectsHashTable];
        _outgoingQueueOperations = [NSHashTable weakObjectsHashTable];
        _zoneChangeFetcher = [[CKKSZoneChangeFetcher alloc] initWithCKKSKeychainView: self];

        _notifierClass = notifierClass;
        _notifyViewChangedScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-notify-scheduler", self.zoneName]
                                                            initialDelay:250*NSEC_PER_MSEC
                                                         continuingDelay:1*NSEC_PER_SEC
                                                        keepProcessAlive:true
                                                                   block:^{
                                                                       __strong __typeof(self) strongSelf = weakSelf;
                                                                       ckksnotice("ckks", strongSelf, "");
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

        _setupSuccessful = false;

        _keyHierarchyConditions = [[NSMutableDictionary alloc] init];
        [CKKSZoneKeyStateMap() enumerateKeysAndObjectsUsingBlock:^(CKKSZoneKeyState * _Nonnull key, NSNumber * _Nonnull obj, BOOL * _Nonnull stop) {
            [self.keyHierarchyConditions setObject: [[CKKSCondition alloc] init] forKey:key];
        }];

        self.keyHierarchyState = SecCKKSZoneKeyStateInitializing;
        _keyHierarchyError = nil;
        _keyHierarchyOperationGroup = nil;
        _keyStateMachineOperation = nil;
        _keyStateFetchRequested = false;
        _keyStateProcessRequested = false;

        _keyStateReadyDependency = [CKKSResultOperation operationWithBlock:^{
            ckksnotice("ckkskey", weakSelf, "Key state has become ready for the first time.");
        }];
        self.keyStateReadyDependency.name = [NSString stringWithFormat: @"%@-key-state-ready", self.zoneName];

        dispatch_time_t initializeDelay = SecCKKSTestsEnabled() ? NSEC_PER_MSEC * 500 : NSEC_PER_SEC * 30;
        _initializeScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat: @"%@-zone-initializer", self.zoneName]
                                                                initialDelay:0
                                                             continuingDelay:initializeDelay
                                                            keepProcessAlive:false
                                                                       block:^{
                                                                           __strong __typeof(self) strongSelf = weakSelf;
                                                                           ckksnotice("ckks", strongSelf, "initialize-scheduler restarting setup");
                                                                           [strongSelf maybeRestartSetup];
                                                                       }];

    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<%@: %@>", NSStringFromClass([self class]), self.zoneName];
}

- (NSString*)debugDescription {
    return [NSString stringWithFormat:@"<%@: %@ %p>", NSStringFromClass([self class]), self.zoneName, self];
}

- (CKKSZoneKeyState*)keyHierarchyState {
    return _keyHierarchyState;
}

- (void)setKeyHierarchyState:(CKKSZoneKeyState *)keyHierarchyState {
    if((keyHierarchyState == nil && _keyHierarchyState == nil) || ([keyHierarchyState isEqualToString:_keyHierarchyState])) {
        // No change, do nothing.
    } else {
        // Fixup the condition variables
        if(_keyHierarchyState) {
            self.keyHierarchyConditions[_keyHierarchyState] = [[CKKSCondition alloc] init];
        }
        if(keyHierarchyState) {
            [self.keyHierarchyConditions[keyHierarchyState] fulfill];
        }
    }

    _keyHierarchyState = keyHierarchyState;
}

- (NSString *)lastActiveTLKUUID
{
    return self.activeTLK;
}

- (void) initializeZone {
    // Unfortunate, but makes retriggering easy.
    [self.initializeScheduler trigger];
}

- (void)maybeRestartSetup {
    [self dispatchSync: ^bool{
        if(self.setupStarted && !self.setupComplete) {
            ckksdebug("ckks", self, "setup has restarted. Ignoring timer fire");
            return false;
        }

        if(self.setupSuccessful) {
            ckksdebug("ckks", self, "setup has completed successfully. Ignoring timer fire");
            return false;
        }

        [self resetSetup];
        [self _onqueueInitializeZone];
        return true;
    }];
}

- (void)resetSetup {
    [super resetSetup];
    self.setupSuccessful = false;

    // Key hierarchy state machine resets, too
    self.keyHierarchyState = SecCKKSZoneKeyStateInitializing;
    _keyHierarchyError = nil;
}

 - (void)_onqueueInitializeZone {
    if(!SecCKKSIsEnabled()) {
        ckksnotice("ckks", self, "Skipping CloudKit initialization due to disabled CKKS");
        return;
    }

    dispatch_assert_queue(self.queue);

    __weak __typeof(self) weakSelf = self;

    NSBlockOperation* afterZoneSetup = [NSBlockOperation blockOperationWithBlock: ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return;
        }

        __block bool quit = false;

        [strongSelf dispatchSync: ^bool {
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

            if(!strongSelf.zoneCreated || !strongSelf.zoneSubscribed || strongSelf.accountStatus != CKAccountStatusAvailable) {
                // Something has gone very wrong. Error out and maybe retry.
                quit = true;

                // Note that CKKSZone has probably called [handleLogout]; which means we have a key hierarchy reset queued up. Error here anyway.
                NSError* realReason = strongSelf.zoneCreatedError ? strongSelf.zoneCreatedError : strongSelf.zoneSubscribedError;
                [strongSelf _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: realReason];

                // We're supposed to be up, but something has gone wrong. Blindly retry until it works.
                if(strongSelf.accountStatus == CKKSAccountStatusAvailable) {
                    [strongSelf.initializeScheduler trigger];
                    ckksnotice("ckks", strongSelf, "We're logged in, but setup didn't work. Scheduling retry for %@", strongSelf.initializeScheduler.nextFireTime);
                }
                return true;
            } else {
                strongSelf.setupSuccessful = true;
            }

            return true;
        }];

        if(quit) {
            ckkserror("ckks", strongSelf, "Quitting setup.");
            return;
        }

        // We can't enter the account queue until an account exists. Before this point, we don't know if one does.
        [strongSelf dispatchSyncWithAccountQueue: ^bool{
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: strongSelf.zoneName];

            // Check if we believe we've synced this zone before.
            if(ckse.changeToken == nil) {
                strongSelf.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"initial-setup"];

                ckksnotice("ckks", strongSelf, "No existing change token; going to try to match local items with CloudKit ones.");

                // Onboard this keychain: there's likely items in it that we haven't synced yet.
                // But, there might be items in The Cloud that correspond to these items, with UUIDs that we don't know yet.
                // First, fetch all remote items.
                CKKSResultOperation* fetch = [strongSelf.zoneChangeFetcher requestSuccessfulFetch:CKKSFetchBecauseInitialStart];
                fetch.name = @"initial-fetch";

                // Next, try to process them (replacing local entries)
                CKKSIncomingQueueOperation* initialProcess = [strongSelf processIncomingQueue: true after: fetch ];
                initialProcess.name = @"initial-process-incoming-queue";

                // If all that succeeds, iterate through all keychain items and find the ones which need to be uploaded
                strongSelf.initialScanOperation = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView:strongSelf ckoperationGroup:strongSelf.keyHierarchyOperationGroup];
                strongSelf.initialScanOperation.name = @"initial-scan-operation";
                [strongSelf.initialScanOperation addNullableDependency:strongSelf.lockStateTracker.unlockDependency];
                [strongSelf.initialScanOperation addDependency: initialProcess];
                [strongSelf scheduleOperation: strongSelf.initialScanOperation];

            } else {
                // Likely a restart of securityd!

                strongSelf.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"restart-setup"];

                if ([CKKSManifest shouldSyncManifests]) {
                    strongSelf.egoManifest = [CKKSEgoManifest tryCurrentEgoManifestForZone:strongSelf.zoneName];
                }

                // If it's been more than 24 hours since the last fetch, fetch and process everything.
                // Otherwise, just kick off the local queue processing.

                NSDate* now = [NSDate date];
                NSDateComponents* offset = [[NSDateComponents alloc] init];
                [offset setHour:-24];
                NSDate* deadline = [[NSCalendar currentCalendar] dateByAddingComponents:offset toDate:now options:0];

                NSOperation* initialProcess = nil;
                if(ckse.lastFetchTime == nil || [ckse.lastFetchTime compare: deadline] == NSOrderedAscending) {
                    initialProcess = [strongSelf fetchAndProcessCKChanges:CKKSFetchBecauseSecuritydRestart];
                } else {
                    initialProcess = [strongSelf processIncomingQueue:false];
                }

                if(!strongSelf.egoManifest) {
                    ckksnotice("ckksmanifest", strongSelf, "No ego manifest on restart; rescanning");
                    strongSelf.initialScanOperation = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView:strongSelf ckoperationGroup:strongSelf.keyHierarchyOperationGroup];
                    strongSelf.initialScanOperation.name = @"initial-scan-operation";
                    [strongSelf.initialScanOperation addNullableDependency:strongSelf.lockStateTracker.unlockDependency];
                    [strongSelf.initialScanOperation addDependency: initialProcess];
                    [strongSelf scheduleOperation: strongSelf.initialScanOperation];
                }

                [strongSelf processOutgoingQueue:strongSelf.keyHierarchyOperationGroup];
            }

            // Tell the key state machine to fire off.
            [strongSelf _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateInitialized withError: nil];
            return true;
        }];
    }];
    afterZoneSetup.name = @"view-setup";

    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: self.zoneName];
    NSOperation* zoneSetupOperation = [self createSetupOperation: ckse.ckzonecreated zoneSubscribed: ckse.ckzonesubscribed];

    self.viewSetupOperation = [[CKKSGroupOperation alloc] init];
    self.viewSetupOperation.name = @"view-setup-group";
    if(!zoneSetupOperation.isFinished) {
        [self.viewSetupOperation runBeforeGroupFinished: zoneSetupOperation];
    }

    [afterZoneSetup addDependency: zoneSetupOperation];
    [self.viewSetupOperation runBeforeGroupFinished: afterZoneSetup];

    [self scheduleAccountStatusOperation: self.viewSetupOperation];
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

    [CKKSCurrentKeyPointer deleteAll:self.zoneID error: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "couldn't delete all CKKSCurrentKeyPointer: %@", localerror);
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

    [self _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateInitializing withError:nil];

    return (localerror == nil && !setError);
}

- (CKKSResultOperation*)resetLocalData {
    __weak __typeof(self) weakSelf = self;

    CKKSGroupOperation* resetFollowUp = [[CKKSGroupOperation alloc] init];
    resetFollowUp.name = @"local-reset-follow-up-group";
    __weak __typeof(resetFollowUp) weakResetFollowUp = resetFollowUp;

    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    op.name = @"local-reset";

    __weak __typeof(op) weakOp = op;
    [op addExecutionBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;
        __strong __typeof(op) strongOp = weakOp;
        __strong __typeof(resetFollowUp) strongResetFollowUp = weakResetFollowUp;
        if(!strongSelf || !strongOp || !strongResetFollowUp) {
            return;
        }

        __block NSError* error = nil;

        [strongSelf dispatchSync: ^bool{
            [self _onqueueResetLocalData: &error];
            return true;
        }];

        [strongSelf resetSetup];

        if(error) {
            ckksnotice("ckksreset", strongSelf, "Local reset finished with error %@", error);
            strongOp.error = error;
        } else {
            if(strongSelf.accountStatus == CKKSAccountStatusAvailable) {
                // Since we're logged in, we expect a reset to fix up the key hierarchy
                ckksnotice("ckksreset", strongSelf, "logged in; re-initializing zone");
                [strongSelf initializeZone];

                ckksnotice("ckksreset", strongSelf, "waiting for key hierarchy to become ready");
                CKKSResultOperation* waitOp = [CKKSResultOperation named:@"waiting-for-key-hierarchy" withBlock:^{}];
                [waitOp timeout: 60*NSEC_PER_SEC];
                [waitOp addNullableDependency:strongSelf.keyStateReadyDependency];

                [strongResetFollowUp runBeforeGroupFinished:waitOp];
            } else {
                ckksnotice("ckksreset", strongSelf, "logged out; not initializing zone");
            }
        }
    }];

    [resetFollowUp runBeforeGroupFinished:op];
    [self scheduleOperationWithoutDependencies:resetFollowUp];
    return resetFollowUp;
}

- (CKKSResultOperation*)resetCloudKitZone {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping CloudKit reset due to disabled CKKS");
        return nil;
    }

    CKKSResultOperation* reset = [super beginResetCloudKitZoneOperation];

    __weak __typeof(self) weakSelf = self;
    CKKSGroupOperation* resetFollowUp = [[CKKSGroupOperation alloc] init];
    resetFollowUp.name = @"cloudkit-reset-follow-up-group";

    __weak __typeof(resetFollowUp) weakResetFollowUp = resetFollowUp;
    [resetFollowUp runBeforeGroupFinished: [CKKSResultOperation named:@"cloudkit-reset-follow-up" withBlock: ^{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return;
        }
        __strong __typeof(resetFollowUp) strongResetFollowUp = weakResetFollowUp;

        if(!reset.error) {
            ckksnotice("ckks", strongSelf, "Successfully deleted zone %@", strongSelf.zoneName);
            __block NSError* error = nil;

            [strongSelf dispatchSync: ^bool{
                [strongSelf _onqueueResetLocalData: &error];
                strongSelf.setupSuccessful = false;
                return true;
            }];

            if(strongSelf.accountStatus == CKKSAccountStatusAvailable) {
                // Since we're logged in, we expect a reset to fix up the key hierarchy
                ckksnotice("ckksreset", strongSelf, "re-initializing zone");
                [strongSelf initializeZone];

                ckksnotice("ckksreset", strongSelf, "waiting for key hierarchy to become ready");
                CKKSResultOperation* waitOp = [CKKSResultOperation named:@"waiting-for-reset" withBlock:^{}];
                [waitOp timeout: 60*NSEC_PER_SEC];
                [waitOp addNullableDependency:strongSelf.keyStateReadyDependency];

                [strongResetFollowUp runBeforeGroupFinished:waitOp];
            } else {
                ckksnotice("ckksreset", strongSelf, "logged out; not initializing zone");
            }
        } else {
            // Shouldn't ever happen, since reset is a successDependency
            ckkserror("ckks", strongSelf, "Couldn't reset zone %@: %@", strongSelf.zoneName, reset.error);
        }
    }]];

    [resetFollowUp addSuccessDependency:reset];
    [self scheduleOperationWithoutDependencies:resetFollowUp];
    return resetFollowUp;
}

- (void)advanceKeyStateMachine {
    __weak __typeof(self) weakSelf = self;

    [self dispatchAsync: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            false;
        }

        [strongSelf _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
        return true;
    }];
};

- (void)_onqueueKeyStateMachineRequestFetch {
    dispatch_assert_queue(self.queue);

    // We're going to set this flag, then nudge the key state machine.
    // If it was idle, then it should launch a fetch. If there was an active process, this flag will stay high
    // and the fetch will be launched later.

    self.keyStateFetchRequested = true;
    [self _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
}

- (void)_onqueueKeyStateMachineRequestFullRefetch {
    dispatch_assert_queue(self.queue);

    self.keyStateFullRefetchRequested = true;
    [self _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
}

- (void)keyStateMachineRequestProcess {
    __weak __typeof(self) weakSelf = self;
    [self dispatchAsync: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return false;
        }

        [strongSelf _onqueueKeyStateMachineRequestProcess];
        return true;
    }];
}

- (void)_onqueueKeyStateMachineRequestProcess {
    dispatch_assert_queue(self.queue);

    // Set the request flag, then nudge the key state machine.
    // If it was idle, then it should launch a fetch. If there was an active process, this flag will stay high
    // and the fetch will be launched later.

    self.keyStateProcessRequested = true;
    [self _onqueueAdvanceKeyStateMachineToState: nil withError: nil];
}

// The operations suggested by this state machine should call _onqueueAdvanceKeyStateMachineToState once they are complete.
// At no other time should keyHierarchyState be modified.

// Note that this function cannot rely on doing any database work; it might get rolled back, especially in an error state
- (void)_onqueueAdvanceKeyStateMachineToState: (CKKSZoneKeyState*) state withError: (NSError*) error {
    dispatch_assert_queue(self.queue);
    __weak __typeof(self) weakSelf = self;

    // Resetting back to 'initializing' takes all precedence.
    if([state isEqual: SecCKKSZoneKeyStateInitializing]) {
        ckksnotice("ckkskey", self, "Resetting the key hierarchy state machine back to 'initializing'");

        [self.keyStateMachineOperation cancel];
        self.keyStateMachineOperation = nil;

        self.keyHierarchyState = SecCKKSZoneKeyStateInitializing;
        self.keyHierarchyError = nil;
        self.keyStateFetchRequested = false;
        self.keyStateProcessRequested = false;

        self.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"key-state-reset"];
        self.keyStateReadyDependency = [CKKSResultOperation operationWithBlock:^{
            ckksnotice("ckkskey", weakSelf, "Key state has become ready for the first time (after reset).");
        }];
        self.keyStateReadyDependency.name = [NSString stringWithFormat: @"%@-key-state-ready", self.zoneName];
        return;
    }

    // Cancels and error states take precedence
    if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateError] ||
       [self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateCancelled] ||
       self.keyHierarchyError != nil) {
        // Error state: nowhere to go. Early-exit.
        ckkserror("ckkskey", self, "Asked to advance state machine from non-exit state %@: %@", self.keyHierarchyState, self.keyHierarchyError);
        return;
    }

    if(error != nil || [state isEqual: SecCKKSZoneKeyStateError]) {
        // But wait! Is this a "we're locked" error?
        if([self.lockStateTracker isLockedError:error]) {
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
            [self.keyStateMachineOperation addNullableDependency:self.lockStateTracker.unlockDependency];
            [self scheduleOperation:self.keyStateMachineOperation];

        } else {
            // Error state: record the error and exit early
            ckkserror("ckkskey", self, "advised of error: coming from state (%@): %@", self.keyHierarchyState, error);
            self.keyHierarchyState = SecCKKSZoneKeyStateError;
            self.keyHierarchyError = error;
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
        self.keyStateMachineOperation = nil;

        ckksnotice("ckkskey", self, "Advancing key hierarchy state machine from %@ to %@", self.keyHierarchyState, state);
        self.keyHierarchyState = state;
    }

    // Many of our decisions below will be based on what keys exist. Help them out.
    CKKSCurrentKeySet* keyset = [[CKKSCurrentKeySet alloc] initForZone:self.zoneID];
    NSError* localerror = nil;
    NSArray<CKKSKey*>* localKeys = [CKKSKey localKeys:self.zoneID error:&localerror];
    NSArray<CKKSKey*>* remoteKeys = [CKKSKey remoteKeys:self.zoneID error: &localerror];

    // We also are checking for OutgoingQueueEntries in the reencrypt state; this is a sign that our key hierarchy is out of date.
    NSArray<CKKSOutgoingQueueEntry*>* outdatedOQEs = [CKKSOutgoingQueueEntry allInState: SecCKKSStateReencrypt zoneID:self.zoneID error: &localerror];

    SecADSetValueForScalarKey((__bridge CFStringRef) SecCKKSAggdViewKeyCount, [localKeys count]);

    if(localerror) {
        ckkserror("ckkskey", self, "couldn't fetch keys and OQEs from local database, entering error state: %@", localerror);
        self.keyHierarchyState = SecCKKSZoneKeyStateError;
        self.keyHierarchyError = localerror;
        return;
    }

#if !defined(NDEBUG)
    NSArray<CKKSKey*>* allKeys = [CKKSKey allKeys:self.zoneID error:&localerror];
    ckksdebug("ckkskey", self, "All keys: %@", allKeys);
#endif

    NSError* hierarchyError = nil;

    if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateInitializing]) {
        if(state != nil) {
            // Wait for CKKSZone to finish initialization.
            ckkserror("ckkskey", self, "Asked to advance state machine (to %@) while CK zone still initializing.", state);
        }
        return;

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateReady]) {
        if(self.keyStateProcessRequested || [remoteKeys count] > 0) {
            // We've either received some remote keys from the last fetch, or someone has requested a reprocess.
            ckksnotice("ckkskey", self, "Kicking off a key reprocess based on request:%d and remote key count %lu", self.keyStateProcessRequested, (unsigned long)[remoteKeys count]);
            [self _onqueueKeyHierarchyProcess];

        } else if(self.keyStateFullRefetchRequested) {
            // In ready, but someone has requested a full fetch. Kick it off.
            ckksnotice("ckkskey", self, "Kicking off a key refetch based on request:%d", self.keyStateFetchRequested);
            [self _onqueueKeyHierarchyRefetch];

        } else if(self.keyStateFetchRequested) {
            // In ready, but someone has requested a fetch. Kick it off.
            ckksnotice("ckkskey", self, "Kicking off a key refetch based on request:%d", self.keyStateFetchRequested);
            [self _onqueueKeyHierarchyFetch];
        }
        // TODO: kick off a key roll if one has been requested

        if(!self.keyStateMachineOperation) {
            // We think we're ready. Double check.
            bool ready = [self _onqueueEnsureKeyHierarchyHealth:&hierarchyError];
            if(!ready || hierarchyError) {
                // Things is bad. Kick off a heal to fix things up.
                ckksnotice("ckkskey", self, "Thought we were ready, but the key hierarchy is unhealthy: %@", hierarchyError);
                self.keyHierarchyState = SecCKKSZoneKeyStateUnhealthy;

            } else {
                // In ready, nothing to do. Notify waiters and quit.
                self.keyHierarchyOperationGroup = nil;
                if(self.keyStateReadyDependency) {
                    [self scheduleOperation: self.keyStateReadyDependency];
                    self.keyStateReadyDependency = nil;
                }

                // If there are any OQEs waiting to be encrypted, launch an op to fix them
                if([outdatedOQEs count] > 0u) {
                    ckksnotice("ckksreencrypt", self, "Reencrypting outgoing items as the key hierarchy is ready");
                    CKKSReencryptOutgoingItemsOperation* op = [[CKKSReencryptOutgoingItemsOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:self.keyHierarchyOperationGroup];
                    [self scheduleOperation:op];
                }

                return;
            }
        }

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateInitialized]) {
        // We're initialized and CloudKit is ready. See what needs done...

        // Check if we have an existing key hierarchy
        CKKSKey* tlk    = [CKKSKey currentKeyForClass:SecCKKSKeyClassTLK zoneID:self.zoneID error:&error];
        CKKSKey* classA = [CKKSKey currentKeyForClass:SecCKKSKeyClassA   zoneID:self.zoneID error:&error];
        CKKSKey* classC = [CKKSKey currentKeyForClass:SecCKKSKeyClassC   zoneID:self.zoneID error:&error];

        if(error && !([error.domain isEqual: @"securityd"] && error.code == errSecItemNotFound)) {
            ckkserror("ckkskey", self, "Error examining existing key hierarchy: %@", error);
        }

        if(tlk && classA && classC && !error) {
            // This is likely a restart of securityd, and we think we're ready. Double check.
            bool ready = [self _onqueueEnsureKeyHierarchyHealth:&hierarchyError];
            if(ready && !hierarchyError) {
                ckksnotice("ckkskey", self, "Already have existing key hierarchy for %@; using it.", self.zoneID.zoneName);
            } else if(hierarchyError && [self.lockStateTracker isLockedError:hierarchyError]) {
                ckksnotice("ckkskey", self, "Initial scan shows key hierarchy is unavailable since keychain is locked: %@", hierarchyError);
                self.keyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;
            } else {
                ckksnotice("ckkskey", self, "Initial scan shows key hierarchy is unhealthy: %@", hierarchyError);
                self.keyHierarchyState = SecCKKSZoneKeyStateUnhealthy;
            }

        } else {
            // We have no local key hierarchy. One might exist in CloudKit, or it might not.
            ckksnotice("ckkskey", self, "No existing key hierarchy for %@. Check if there's one in CloudKit...", self.zoneID.zoneName);

            [self _onqueueKeyHierarchyFetch];
        }

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateFetchComplete]) {
        // We're initializing this zone, and just completed a fetch of everything. Are there any remote keys?
        if(remoteKeys.count > 0u) {
            // Process the keys we received.
            self.keyStateMachineOperation = [[CKKSProcessReceivedKeysOperation alloc] initWithCKKSKeychainView: self];
        } else if( (keyset.currentTLKPointer || keyset.currentClassAPointer || keyset.currentClassCPointer) &&
                  !(keyset.tlk && keyset.classA && keyset.classC)) {
            // Huh. We appear to have current key pointers, but the keys themselves don't exist. That's weird.
            // Transfer to the "unhealthy" state to request a fix
            ckksnotice("ckkskey", self, "We appear to have current key pointers but no keys to match them. Moving to 'unhealthy'");
            self.keyHierarchyState = SecCKKSZoneKeyStateUnhealthy;

        } else if([remoteKeys count] == 0) {
            // No keys, no pointers? make some new ones!
            self.keyStateMachineOperation = [[CKKSNewTLKOperation alloc] initWithCKKSKeychainView: self ckoperationGroup:self.keyHierarchyOperationGroup];
        }

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateWaitForTLK]) {
        // We're in a hold state: waiting for the TLK bytes to arrive.

        if(self.keyStateProcessRequested) {
            // Someone has requsted a reprocess! Run a ProcessReceivedKeysOperation.
            ckksnotice("ckkskey", self, "Received a nudge that our TLK might be here! Starting operation to check.");
            [self _onqueueKeyHierarchyProcess];
        }

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateWaitForUnlock]) {
        // We're in a hold state: waiting for the keybag to unlock so we can process the keys again.

        [self _onqueueKeyHierarchyProcess];
        [self.keyStateMachineOperation addNullableDependency: self.lockStateTracker.unlockDependency];

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateBadCurrentPointers]) {
        // The current key pointers are broken, but we're not sure why.
        ckksnotice("ckkskey", self, "Our current key pointers are reported broken. Attempting a fix!");
        self.keyStateMachineOperation = [[CKKSHealKeyHierarchyOperation alloc] initWithCKKSKeychainView: self ckoperationGroup:self.keyHierarchyOperationGroup];

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateNewTLKsFailed]) {
        ckksnotice("ckkskey", self, "Creating new TLKs didn't work. Attempting to refetch!");
        [self _onqueueKeyHierarchyFetch];

    } else if([self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateNeedFullRefetch]) {
        ckksnotice("ckkskey", self, "Informed of request for full refetch");
        [self _onqueueKeyHierarchyRefetch];

    } else {
        ckkserror("ckks", self, "asked to advance state machine to unknown state: %@", self.keyHierarchyState);
        return;
    }

    if(self.keyStateMachineOperation) {

        if(self.keyStateReadyDependency == nil || [self.keyStateReadyDependency isFinished]) {
            ckksnotice("ckkskey", self, "reloading keyStateReadyDependency due to operation %@", self.keyStateMachineOperation);

            __weak __typeof(self) weakSelf = self;
            self.keyHierarchyOperationGroup = [CKOperationGroup CKKSGroupWithName:@"key-state-broken"];
            self.keyStateReadyDependency = [CKKSResultOperation operationWithBlock:^{
                ckksnotice("ckkskey", weakSelf, "Key state has become ready again.");
            }];
            self.keyStateReadyDependency.name = [NSString stringWithFormat: @"%@-key-state-ready", self.zoneName];
        }

        [self scheduleOperation: self.keyStateMachineOperation];
    } else if([self.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateWaitForTLK]) {
        ckksnotice("ckkskey", self, "Entering %@", self.keyHierarchyState);

    } else if([self.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateUnhealthy]) {
        ckksnotice("ckkskey", self, "Looks like the key hierarchy is unhealthy. Launching fix.");
        self.keyStateMachineOperation = [[CKKSHealKeyHierarchyOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:self.keyHierarchyOperationGroup];
        [self scheduleOperation: self.keyStateMachineOperation];

    } else {
        // Nothing to do and not in a waiting state? Awesome; we must be in the ready state.
        if(![self.keyHierarchyState isEqual: SecCKKSZoneKeyStateReady]) {
            ckksnotice("ckkskey", self, "No action to take in state %@; we must be ready.", self.keyHierarchyState);
            self.keyHierarchyState = SecCKKSZoneKeyStateReady;

            self.keyHierarchyOperationGroup = nil;
            if(self.keyStateReadyDependency) {
                [self scheduleOperation: self.keyStateReadyDependency];
                self.keyStateReadyDependency = nil;
            }
        }
    }
}

- (bool)_onqueueEnsureKeyHierarchyHealth:(NSError* __autoreleasing *)error {
    dispatch_assert_queue(self.queue);

    NSError* localerror = nil;

    // Check if we have an existing key hierarchy
    CKKSKey* tlk    = [CKKSKey currentKeyForClass:SecCKKSKeyClassTLK zoneID:self.zoneID error:&localerror];
    CKKSKey* classA = [CKKSKey currentKeyForClass:SecCKKSKeyClassA   zoneID:self.zoneID error:&localerror];
    CKKSKey* classC = [CKKSKey currentKeyForClass:SecCKKSKeyClassC   zoneID:self.zoneID error:&localerror];

    if(localerror || !tlk || !classA || !classC) {
        ckkserror("ckkskey", self, "Error examining existing key hierarchy: %@", localerror);
        ckkserror("ckkskey", self, "Keys are: %@ %@ %@", tlk, classA, classC);
        if(error) {
            *error = localerror;
        }
        return false;
    }

    // keychain being locked is not a fatal error here
    [tlk loadKeyMaterialFromKeychain:&localerror];
    if(localerror && !([localerror.domain isEqual: @"securityd"] && localerror.code == errSecInteractionNotAllowed)) {
        ckksinfo("ckkskey", self, "Error loading TLK(%@): %@", tlk, localerror);
        if(error) {
            *error = localerror;
        }
        return false;
    } else if(localerror) {
        ckksinfo("ckkskey", self, "Error loading TLK(%@), maybe locked: %@", tlk, localerror);
    }
    localerror = nil;

    // keychain being locked is not a fatal error here
    [classA loadKeyMaterialFromKeychain:&localerror];
    if(localerror && !([localerror.domain isEqual: @"securityd"] && localerror.code == errSecInteractionNotAllowed)) {
        ckksinfo("ckkskey", self, "Error loading classA key(%@): %@", classA, localerror);
        if(error) {
            *error = localerror;
        }
        return false;
    } else if(localerror) {
        ckksinfo("ckkskey", self, "Error loading classA key(%@), maybe locked: %@", classA, localerror);
    }
    localerror = nil;

    // keychain being locked is a fatal error here, since this is class C
    [classA loadKeyMaterialFromKeychain:&localerror];
    if(localerror) {
        ckksinfo("ckkskey", self, "Error loading classC(%@): %@", classC, localerror);
        if(error) {
            *error = localerror;
        }
        return false;
    }

    self.activeTLK = [tlk uuid];

    // Got to the bottom? Cool! All keys are present and accounted for.
    return true;
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

        [strongSelf dispatchSync: ^bool{
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

        [strongSelf dispatchSync: ^bool{
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
        ckksinfo("ckks", self, "Skipping handleKeychainEventDbConnection due to disabled CKKS");
        return;
    }

    __block NSError* error = nil;

    if(self.accountStatus != CKKSAccountStatusAvailable && syncCallback) {
        // We're not logged into CloudKit, and therefore don't expect this item to be synced anytime particularly soon.
        CKKSAccountStatus accountStatus = self.accountStatus;
        dispatch_async(self.queue, ^{
            syncCallback(false, [NSError errorWithDomain:@"securityd"
                                                    code:errSecNotLoggedIn
                                                userInfo:@{NSLocalizedDescriptionKey:
                                                               [NSString stringWithFormat: @"No iCloud account available(%d); item is not expected to sync", (int)accountStatus]}]);
        });

        syncCallback = nil;
    }

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
        ckksinfo("ckks", self, "skipping sync of non-sync item");
        return;
    }

    // Only synchronize items which can transfer between devices
    NSString* protection = (__bridge NSString*)SecDbItemGetCachedValueWithName(added ? added : deleted, kSecAttrAccessible);
    if(! ([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleWhenUnlocked] ||
          [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAfterFirstUnlock] ||
          [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAlways])) {
        ckksinfo("ckks", self, "skipping sync of device-bound item");
        return;
    }

    // Our caller gave us a database connection. We must get on the local queue to ensure atomicity
    // Note that we're at the mercy of the surrounding db transaction, so don't try to rollback here
    [self dispatchSyncWithConnection: dbconn block: ^bool {
        if(![self.keyHierarchyState isEqualToString: SecCKKSZoneKeyStateReady]) {
            ckksnotice("ckks", self, "Key state not ready for new items; skipping");
            return true;
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

            // If the problem is 'no UUID', launch a scan operation to find and fix it
            // We don't want to fix it up here, in the closing moments of a transaction
            if([error.domain isEqualToString:@"securityd"] && error.code == CKKSNoUUIDOnItem) {
                ckksnotice("ckks", self, "Launching scan operation");
                CKKSScanLocalItemsOperation* scanOperation = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView: self ckoperationGroup:operationGroup];
                [self scheduleOperation: scanOperation];
            }

            // If the problem is 'couldn't load key', tell the key hierarchy state machine to fix it
            // Then, launch a scan operation to find this item and upload it
            if([error.domain isEqualToString:@"securityd"] && error.code == errSecItemNotFound) {
                [self _onqueueAdvanceKeyStateMachineToState: nil withError: nil];

                ckksnotice("ckks", self, "Launching scan operation to refind %@", added);
                CKKSScanLocalItemsOperation* scanOperation = [[CKKSScanLocalItemsOperation alloc] initWithCKKSKeychainView: self ckoperationGroup:operationGroup];
                [scanOperation addNullableDependency:self.keyStateReadyDependency];
                [self scheduleOperation: scanOperation];
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

        if(syncCallback) {
            self.pendingSyncCallbacks[oqe.uuid] = syncCallback;
        }

        // Schedule a "view changed" notification
        [self.notifyViewChangedScheduler trigger];

        [self processOutgoingQueue:operationGroup];

        return true;
    }];
}

-(void)setCurrentItemForAccessGroup:(SecDbItemRef)newItem
                               hash:(NSData*)newItemSHA1
                        accessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                          replacing:(SecDbItemRef)oldItem
                               hash:(NSData*)oldItemSHA1
                           complete:(void (^) (NSError* operror)) complete
{
    if(accessGroup == nil || identifier == nil) {
        NSError* error = [NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"No access group or identifier given"}];
        ckkserror("ckkscurrent", self, "Cancelling request: %@", error);
        complete(error);
        return;
    }

    __weak __typeof(self) weakSelf = self;

    [self dispatchSync:^bool {
        NSError* error = nil;
        CFErrorRef cferror = NULL;

        NSString* newItemUUID = nil;
        NSString* oldItemUUID = nil;

        // Now that we're on the db queue, ensure that the given hashes for the items match the hashes as they are now.
        // That is, the items haven't changed since the caller knew about the item.
        NSData* newItemComputedSHA1 = (NSData*) CFBridgingRelease(CFRetainSafe(SecDbItemGetSHA1(newItem, &cferror)));
        if(!newItemComputedSHA1 || cferror ||
           ![newItemComputedSHA1 isEqual:newItemSHA1]) {
            ckksnotice("ckkscurrent", self, "Hash mismatch for new item: %@ vs %@", newItemComputedSHA1, newItemSHA1);
            error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue userInfo:@{NSLocalizedDescriptionKey: @"New item has changed; hashes mismatch. Refetch and try again."}];
            complete(error);
            CFReleaseNull(cferror);
            return false;
        }

        newItemUUID = (NSString*) CFBridgingRelease(CFRetainSafe(SecDbItemGetValue(newItem, &v10itemuuid, &cferror)));
        if(!newItemUUID || cferror) {
            ckkserror("ckkscurrent", self, "Error fetching UUID for new item: %@", cferror);
            complete((__bridge NSError*) cferror);
            CFReleaseNull(cferror);
            return false;
        }

        if(oldItem) {
            NSData* oldItemComputedSHA1 = (NSData*) CFBridgingRelease(CFRetainSafe(SecDbItemGetSHA1(oldItem, &cferror)));
            if(!oldItemComputedSHA1 || cferror ||
               ![oldItemComputedSHA1 isEqual:oldItemSHA1]) {
                ckksnotice("ckkscurrent", self, "Hash mismatch for old item: %@ vs %@", oldItemComputedSHA1, oldItemSHA1);
                error = [NSError errorWithDomain:@"securityd" code:errSecItemInvalidValue userInfo:@{NSLocalizedDescriptionKey: @"Old item has changed; hashes mismatch. Refetch and try again."}];
                complete(error);
                CFReleaseNull(cferror);
                return false;
            }

            oldItemUUID = (NSString*) CFBridgingRelease(CFRetainSafe(SecDbItemGetValue(oldItem, &v10itemuuid, &cferror)));
            if(!oldItemUUID || cferror) {
                ckkserror("ckkscurrent", self, "Error fetching UUID for old item: %@", cferror);
                complete((__bridge NSError*) cferror);
                CFReleaseNull(cferror);
                return false;
            }
        }

        // Not being in a CloudKit account is an automatic failure.
        if(self.accountStatus != CKKSAccountStatusAvailable) {
            ckksnotice("ckkscurrent", self, "Rejecting current item pointer set since we don't have an iCloud account.");
            error = [NSError errorWithDomain:@"securityd" code:errSecNotLoggedIn userInfo:@{NSLocalizedDescriptionKey: @"User is not signed into iCloud."}];
            complete(error);
            return false;
        }

        // At this point, we've completed all the checks we need for the SecDbItems. Try to launch this boat!
        NSString* currentIdentifier = [NSString stringWithFormat:@"%@-%@", accessGroup, identifier];
        ckksnotice("ckkscurrent", self, "Setting current pointer for %@ to %@ (from %@)", currentIdentifier, newItemUUID, oldItemUUID);
        CKKSUpdateCurrentItemPointerOperation* ucipo = [[CKKSUpdateCurrentItemPointerOperation alloc] initWithCKKSKeychainView:self
                                                                                                                currentPointer:(NSString*)currentIdentifier
                                                                                                                   oldItemUUID:(NSString*)oldItemUUID
                                                                                                                   newItemUUID:(NSString*)newItemUUID
                                                                                                              ckoperationGroup:[CKOperationGroup CKKSGroupWithName:@"currentitem-api"]];
        CKKSResultOperation* returnCallback = [CKKSResultOperation operationWithBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;

            if(ucipo.error) {
                ckkserror("ckkscurrent", strongSelf, "Failed setting a current item pointer with %@", ucipo.error);
            } else {
                ckksnotice("ckkscurrent", strongSelf, "Finished setting a current item pointer");
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
        return true;
    }];
    return;
}

-(void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                    fetchCloudValue:(bool)fetchCloudValue
                           complete:(void (^) (NSString* uuid, NSError* operror)) complete
{
    if(accessGroup == nil || identifier == nil) {
        complete(NULL, [NSError errorWithDomain:@"securityd" code:errSecParam userInfo:@{NSLocalizedDescriptionKey: @"No access group or identifier given"}]);
        return;
    }

    // Not being in a CloudKit account is an automatic failure.
    if(self.accountStatus != CKKSAccountStatusAvailable) {
        ckksnotice("ckkscurrent", self, "Rejecting current item pointer get since we don't have an iCloud account.");
        complete(NULL, [NSError errorWithDomain:@"securityd" code:errSecNotLoggedIn userInfo:@{NSLocalizedDescriptionKey: @"User is not signed into iCloud."}]);
        return;
    }

    CKKSResultOperation* fetchAndProcess = nil;
    if(fetchCloudValue) {
        fetchAndProcess = [self fetchAndProcessCKChanges:CKKSFetchBecauseCurrentItemFetchRequest];
    }

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* getCurrentItem = [CKKSResultOperation operationWithBlock:^{
        if(fetchAndProcess.error) {
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
                complete(nil, [NSError errorWithDomain:@"securityd"
                                                  code:errSecInternalError
                                              userInfo:@{NSLocalizedDescriptionKey: @"Current item pointer is empty"}]);
                return false;
            }

            complete(cip.currentItemUUID, NULL);

            return true;
        }];
    }];
    getCurrentItem.name = @"get-current-item-pointer";

    [getCurrentItem addNullableDependency:fetchAndProcess];
    [self scheduleOperation: getCurrentItem];
}

- (CKKSKey*) keyForItem: (SecDbItemRef) item error: (NSError * __autoreleasing *) error {
    CKKSKeyClass* class = nil;

    NSString* protection = (__bridge NSString*)SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);
    if([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleWhenUnlocked]) {
        class = SecCKKSKeyClassA;
    } else if([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAlways] ||
              [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAfterFirstUnlock]) {
        class = SecCKKSKeyClassC;
    } else {
        ckkserror("ckks", self, "can't pick key class for protection %@: %@", protection, item);
        if(error) {
           *error =[NSError errorWithDomain:@"securityd"
                                code:5
                            userInfo:@{NSLocalizedDescriptionKey:
                                           [NSString stringWithFormat:@"can't pick key class for protection %@: %@", protection, item]}];
        }

        return nil;
    }

    CKKSKey* key = [CKKSKey currentKeyForClass: class zoneID:self.zoneID error:error];

    // and make sure it's unwrapped.
    if(![key ensureKeyLoaded:error]) {
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
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping processOutgoingQueue due to disabled CKKS");
        return nil;
    }

    CKKSOutgoingQueueOperation* outgoingop =
            (CKKSOutgoingQueueOperation*) [self findFirstPendingOperation:self.outgoingQueueOperations
                                                                  ofClass:[CKKSOutgoingQueueOperation class]];
    if(outgoingop) {
        ckksinfo("ckks", self, "Skipping processOutgoingQueue due to at least one pending instance");
        if(after) {
            [outgoingop addDependency: after];
        }
        if([outgoingop isPending]) {
            if(!outgoingop.ckoperationGroup && ckoperationGroup) {
                outgoingop.ckoperationGroup = ckoperationGroup;
            } else if(ckoperationGroup) {
                ckkserror("ckks", self, "Throwing away CKOperationGroup(%@) in favor of %@", ckoperationGroup, outgoingop.ckoperationGroup);
            }

            return outgoingop;
        }
    }

    CKKSOutgoingQueueOperation* op = [[CKKSOutgoingQueueOperation alloc] initWithCKKSKeychainView:self ckoperationGroup:ckoperationGroup];
    op.name = @"outgoing-queue-operation";
    [op addNullableDependency:after];

    [op addNullableDependency: self.initialScanOperation];

    [self scheduleOperation: op];
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
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping processIncomingQueue due to disabled CKKS");
        return nil;
    }

    CKKSIncomingQueueOperation* incomingop = (CKKSIncomingQueueOperation*) [self findFirstPendingOperation:self.incomingQueueOperations];
    if(incomingop) {
        ckksinfo("ckks", self, "Skipping processIncomingQueue due to at least one pending instance");
        if(after) {
            [incomingop addDependency: after];
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

- (CKKSUpdateDeviceStateOperation*)updateDeviceState:(bool)rateLimit ckoperationGroup:(CKOperationGroup*)ckoperationGroup {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping updateDeviceState due to disabled CKKS");
        return nil;
    }

    CKKSUpdateDeviceStateOperation* op = [[CKKSUpdateDeviceStateOperation alloc] initWithCKKSKeychainView:self rateLimit:rateLimit ckoperationGroup:ckoperationGroup];
    op.name = @"device-state-operation";

    // op modifies the CloudKit zone, so it should insert itself into the list of OutgoingQueueOperations.
    // Then, we won't have simultaneous zone-modifying operations and confuse ourselves.
    [op linearDependencies:self.outgoingQueueOperations];

    // CKKSUpdateDeviceStateOperations are special: they should fire even if we don't believe we're in an iCloud account.
    [self scheduleAccountStatusOperation:op];
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

    // We only really want the oldcdse for its encodedCKRecord, so make a new cdse here
    CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initForDevice:accountTracker.ckdeviceID
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
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping resyncWithCloud due to disabled CKKS");
        return nil;
    }

    CKKSSynchronizeOperation* op = [[CKKSSynchronizeOperation alloc] initWithCKKSKeychainView: self];
    [self scheduleOperation: op];
    return op;
}

- (CKKSResultOperation*)fetchAndProcessCKChanges:(CKKSFetchBecause*)because {
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Skipping fetchAndProcessCKChanges due to disabled CKKS");
        return nil;
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
            ckksnotice("ckksresync", self, "Already know about this item record, skipping update: %@", record);
            return;
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
        if([ckme matchesCKRecord:record]) {
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

    // For now, drop into the synckeys table as a 'remote' key, then ask for a rekey operation.
    CKKSKey* remotekey = [[CKKSKey alloc] initWithCKRecord: record];

    // We received this from an update. Don't use, yet.
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

- (void)_onqueueCKRecordCurrentKeyPointerChanged:(CKRecord*)record resync:(bool)resync {
    dispatch_assert_queue(self.queue);

    if(resync) {
        NSError* ckperror = nil;
        CKKSCurrentKeyPointer* ckp = [CKKSCurrentKeyPointer tryFromDatabase:((CKKSKeyClass*) record.recordID.recordName) zoneID:self.zoneID error:&ckperror];
        if(ckperror) {
            ckkserror("ckksresync", self, "error loading ckp: %@", ckperror);
        }
        if(!ckp) {
            ckkserror("ckksresync", self, "BUG: No current key pointer matching resynced CloudKit record: %@", record);
        } else if(![ckp matchesCKRecord:record]) {
            ckkserror("ckksresync", self, "BUG: Local current key pointer doesn't match resynced CloudKit record: %@ %@", ckp, record);
        } else {
            ckksnotice("ckksresync", self, "Already know about this current key pointer, skipping update: %@", record);
            return;
        }
    }

    NSError* error = nil;
    CKKSCurrentKeyPointer* currentkey = [[CKKSCurrentKeyPointer alloc] initWithCKRecord: record];

    [currentkey saveToDatabase: &error];
    if(error) {
        ckkserror("ckkskey", self, "Couldn't save current key pointer to database: %@: %@", currentkey, error);
        ckksinfo("ckkskey", self, "CKRecord was %@", record);
    }

    // We've saved a new key in the database; trigger a rekey operation.
    [self _onqueueKeyStateMachineRequestProcess];
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

- (bool)_onqueueChangeOutgoingQueueEntry: (CKKSOutgoingQueueEntry*) oqe toState: (NSString*) state error: (NSError* __autoreleasing*) error {
    dispatch_assert_queue(self.queue);

    NSError* localerror = nil;

    if([state isEqualToString: SecCKKSStateDeleted]) {
        // Hurray, this must be a success
        SecBoolNSErrorCallback callback = self.pendingSyncCallbacks[oqe.uuid];
        if(callback) {
            callback(true, nil);
        }

        [oqe deleteFromDatabase: &localerror];
        if(localerror) {
            ckkserror("ckks", self, "Couldn't delete %@: %@", oqe, localerror);
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
    }
    NSError* localerror = nil;

    oqe.state = SecCKKSStateError;
    [oqe saveToDatabase: &localerror];
    if(localerror) {
        ckkserror("ckks", self, "Couldn't set %@ as error: %@", oqe, localerror);
    }

    if(error && localerror) {
        *error = localerror;
    }
    return localerror == nil;
}

- (bool)_onQueueUpdateLatestManifestWithError:(NSError**)error
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

- (bool)checkTLK: (CKKSKey*) proposedTLK error: (NSError * __autoreleasing *) error {
    // Until we have Octagon Trust, accept this TLK iff we have its actual AES key in the keychain

    if([proposedTLK loadKeyMaterialFromKeychain:error]) {
        // Hurray!
        return true;
    } else {
        return false;
    }
}

- (void) dispatchAsync: (bool (^)(void)) block {
    // We need to call kc_with_dbt, which blocks. Route up through a global queue...
    __weak __typeof(self) weakSelf = self;

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        [weakSelf dispatchSync:block];
    });
}

// Use this if you have a potential database connection already
- (void) dispatchSyncWithConnection: (SecDbConnectionRef) dbconn block: (bool (^)(void)) block {
    if(dbconn) {
        dispatch_sync(self.queue, ^{
            CFErrorRef cferror = NULL;
            kc_transaction_type(dbconn, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, block);

            if(cferror) {
                ckkserror("ckks", self, "error doing database transaction (sync), major problems ahead: %@", cferror);
            }
        });
    } else {
        [self dispatchSync: block];
    }
}

- (void) dispatchSync: (bool (^)(void)) block {
    // important enough to block this thread. Must get a connection first, though!
    __weak __typeof(self) weakSelf = self;

    CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            ckkserror("ckks", strongSelf, "received callback for released object");
            return false;
        }

        __block bool ok = false;
        __block CFErrorRef cferror = NULL;

        dispatch_sync(strongSelf.queue, ^{
            ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, block);
        });
        return ok;
    });
    if(cferror) {
        ckkserror("ckks", self, "error getting database connection (sync), major problems ahead: %@", cferror);
    }
}

- (void)dispatchSyncWithAccountQueue:(bool (^)(void))block
{
    [SOSAccount performOnAccountQueue:^{
        [CKKSManifest performWithAccountInfo:^{
            [self dispatchSync:^bool{
                __block bool result = false;
                [SOSAccount performWhileHoldingAccountQueue:^{ // so any calls through SOS account will know they can perform their work without dispatching to the account queue, which we already hold
                    result = block();
                }];
                return result;
            }];
        }];
    }];
}

#pragma mark - CKKSZoneUpdateReceiver

- (void)notifyZoneChange: (CKRecordZoneNotification*) notification {
    ckksinfo("ckks", self, "hurray, got a zone change for %@ %@", self, notification);

    [self fetchAndProcessCKChanges:CKKSFetchBecauseAPNS];
}

// Must be on the queue when this is called
- (void)handleCKLogin {
    dispatch_assert_queue(self.queue);

    if(!self.setupStarted) {
        [self _onqueueInitializeZone];
    } else {
        ckksinfo("ckks", self, "ignoring login as setup has already started");
    }
}

- (void)handleCKLogout {
    NSBlockOperation* logout = [NSBlockOperation blockOperationWithBlock: ^{
        [self dispatchSync:^bool {
            ckksnotice("ckks", self, "received a notification of CK logout for %@", self.zoneName);
            NSError* error = nil;

            [self _onqueueResetLocalData: &error];

            if(error) {
                ckkserror("ckks", self, "error while resetting local data: %@", error);
            }
            return true;
        }];
    }];

    logout.name = @"cloudkit-logout";
    [self scheduleAccountStatusOperation: logout];
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
        [self cancelAllOperations];
        CKKSResultOperation* resetOp = [self resetLocalData];
        CKKSResultOperation* resetHandler = [CKKSResultOperation named:@"local-reset-handler" withBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror("ckks", strongSelf, "received callback for released object");
                return;
            }

            if(resetOp.error) {
                ckksnotice("ckks", strongSelf, "CloudKit-inspired local reset of %@ ended with error: %@", strongSelf.zoneID, error);
            } else {
                ckksnotice("ckksreset", strongSelf, "re-initializing zone %@", strongSelf.zoneID);
                [strongSelf initializeZone];
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
        ckkserror("ckks", self, "Received notice that our zone does not exist. Resetting all data.");
        [self cancelAllOperations];
        CKKSResultOperation* resetOp = [self resetCloudKitZone];
        CKKSResultOperation* resetHandler = [CKKSResultOperation named:@"reset-handler" withBlock:^{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                ckkserror("ckks", strongSelf, "received callback for released object");
                return;
            }

            if(resetOp.error) {
                ckksnotice("ckks", strongSelf, "CloudKit-inspired zone reset of %@ ended with error: %@", strongSelf.zoneID, resetOp.error);
            } else {
                ckksnotice("ckksreset", strongSelf, "re-initializing zone %@", strongSelf.zoneID);
                [strongSelf initializeZone];
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
    if(!SecCKKSIsEnabled()) {
        ckksinfo("ckks", self, "Due to disabled CKKS, returning fast from waitForFetchAndIncomingQueueProcessing");
        return nil;
    }

    CKKSResultOperation* op = [self fetchAndProcessCKChanges:CKKSFetchBecauseTesting];
    [op waitUntilFinished];
    return op;
}

- (void)waitForKeyHierarchyReadiness {
    if(self.keyStateReadyDependency) {
        [self.keyStateReadyDependency waitUntilFinished];
    }
}

- (void)cancelAllOperations {
    [self.zoneSetupOperation cancel];
    [self.keyStateMachineOperation cancel];
    [self.keyStateReadyDependency cancel];
    [self.zoneChangeFetcher cancel];

    [super cancelAllOperations];

    [self dispatchSync:^bool{
        [self _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateCancelled withError: nil];
        return true;
    }];
}

- (NSDictionary*)status {
#define stringify(obj) CKKSNilToNSNull([obj description])
#define boolstr(obj) (!!(obj) ? @"yes" : @"no")
    __block NSDictionary* ret = nil;
    __block NSError* error = nil;
    CKKSManifest* manifest = [CKKSManifest latestTrustedManifestForZone:self.zoneName error:&error];
    [self dispatchSync: ^bool {

        NSString* uuidTLK    = [CKKSKey currentKeyForClass:SecCKKSKeyClassTLK zoneID:self.zoneID error:&error].uuid;
        NSString* uuidClassA = [CKKSKey currentKeyForClass:SecCKKSKeyClassA   zoneID:self.zoneID error:&error].uuid;
        NSString* uuidClassC = [CKKSKey currentKeyForClass:SecCKKSKeyClassC   zoneID:self.zoneID error:&error].uuid;
        
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

        ret = @{
                 @"view":                CKKSNilToNSNull(self.zoneName),
                 @"ckaccountstatus":     self.accountStatus == CKAccountStatusCouldNotDetermine ? @"could not determine" :
                                         self.accountStatus == CKAccountStatusAvailable         ? @"logged in" :
                                         self.accountStatus == CKAccountStatusRestricted        ? @"restricted" :
                                         self.accountStatus == CKAccountStatusNoAccount         ? @"logged out" : @"unknown",
                 @"lockstatetracker":    stringify(self.lockStateTracker),
                 @"accounttracker":      stringify(self.accountTracker),
                 @"fetcher":             stringify(self.zoneChangeFetcher),
                 @"setup":               boolstr(self.setupComplete),
                 @"zoneCreated":         boolstr(self.zoneCreated),
                 @"zoneCreatedError":    stringify(self.zoneCreatedError),
                 @"zoneSubscribed":      boolstr(self.zoneSubscribed),
                 @"zoneSubscribedError": stringify(self.zoneSubscribedError),
                 @"zoneInitializeScheduler": stringify(self.initializeScheduler),
                 @"keystate":            CKKSNilToNSNull(self.keyHierarchyState),
                 @"keyStateError":       stringify(self.keyHierarchyError),
                 @"statusError":         stringify(error),
                 @"oqe":                 CKKSNilToNSNull([CKKSOutgoingQueueEntry countsByState:self.zoneID error:&error]),
                 @"iqe":                 CKKSNilToNSNull([CKKSIncomingQueueEntry countsByState:self.zoneID error:&error]),
                 @"ckmirror":            CKKSNilToNSNull([CKKSMirrorEntry        countsByParentKey:self.zoneID error:&error]),
                 @"devicestates":        CKKSNilToNSNull(mutDeviceStates),
                 @"keys":                CKKSNilToNSNull([CKKSKey countsByClass:self.zoneID error:&error]),
                 @"currentTLK":          CKKSNilToNSNull(uuidTLK),
                 @"currentClassA":       CKKSNilToNSNull(uuidClassA),
                 @"currentClassC":       CKKSNilToNSNull(uuidClassC),
                 @"currentManifestGen":   CKKSNilToNSNull(manifestGeneration),

                 @"zoneSetupOperation":                 stringify(self.zoneSetupOperation),
                 @"viewSetupOperation":                 stringify(self.viewSetupOperation),
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
