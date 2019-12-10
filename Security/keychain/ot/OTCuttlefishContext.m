/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#include <sys/sysctl.h>

#import <os/feature_private.h>

#import <Security/Security.h>

#include <utilities/SecFileLocations.h>
#include <Security/SecRandomP.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "keychain/TrustedPeersHelper/TrustedPeersHelperProtocol.h"
#import <TrustedPeers/TrustedPeers.h>
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSResultOperation.h"

#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/analytics/CKKSLaunchSequence.h"
#import "keychain/ot/OTDeviceInformationAdapter.h"


#import "keychain/ot/OctagonStateMachine.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OctagonCKKSPeerAdapter.h"
#import "keychain/ot/OctagonCheckTrustStateOperation.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/OTFollowup.h"
#import "keychain/ot/OTAuthKitAdapter.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTOperationDependencies.h"
#import "keychain/ot/OTClique.h"
#import "keychain/ot/OTCuttlefishContext.h"
#import "keychain/ot/OTPrepareOperation.h"
#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTSOSUpgradeOperation.h"
#import "keychain/ot/OTUpdateTPHOperation.h"
#import "keychain/ot/OTEpochOperation.h"
#import "keychain/ot/OTClientVoucherOperation.h"
#import "keychain/ot/OTLeaveCliqueOperation.h"
#import "keychain/ot/OTRemovePeersOperation.h"
#import "keychain/ot/OTJoinWithVoucherOperation.h"
#import "keychain/ot/OTVouchWithBottleOperation.h"
#import "keychain/ot/OTVouchWithRecoveryKeyOperation.h"
#import "keychain/ot/OTEstablishOperation.h"
#import "keychain/ot/OTLocalCKKSResetOperation.h"
#import "keychain/ot/OTUpdateTrustedDeviceListOperation.h"
#import "keychain/ot/OTSOSUpdatePreapprovalsOperation.h"
#import "keychain/ot/OTResetOperation.h"
#import "keychain/ot/OTLocalCuttlefishReset.h"
#import "keychain/ot/OTSetRecoveryKeyOperation.h"
#import "keychain/ot/OTResetCKKSZonesLackingTLKsOperation.h"
#import "keychain/ot/OTUploadNewCKKSTLKsOperation.h"
#import "keychain/ot/OTCuttlefishAccountStateHolder.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/proto/generated_source/OTAccountMetadataClassC.h"
#import "keychain/ot/OTTriggerEscrowUpdateOperation.h"
#import "keychain/ot/OTCheckHealthOperation.h"
#import "keychain/ot/OTEnsureOctagonKeyConsistency.h"
#import "keychain/ot/OTDetermineHSA2AccountStatusOperation.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/escrowrequest/EscrowRequestServer.h"

#if TARGET_OS_WATCH
#import "keychain/otpaird/OTPairingClient.h"
#endif /* TARGET_OS_WATCH */

#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"

#import "utilities/SecTapToRadar.h"

#import "keychain/categories/NSError+UsefulConstructors.h"
#import <CoreCDP/CDPAccount.h>
#import <notify.h>

NSString* OTCuttlefishContextErrorDomain = @"otcuttlefish";
static dispatch_time_t OctagonStateTransitionDefaultTimeout = 10*NSEC_PER_SEC;

@class CKKSLockStateTracker;

@interface OTCuttlefishContext () <OTCuttlefishAccountStateHolderNotifier>
{
    NSData* _vouchData;
    NSData* _vouchSig;
    NSString* _bottleID;
    NSString* _bottleSalt;
    NSData* _entropy;
    NSArray<NSData*>* _preapprovedKeys;
    NSString* _recoveryKey;
    CuttlefishResetReason _resetReason;
    BOOL _skipRateLimitingCheck;
}

@property CKKSLaunchSequence* launchSequence;
@property NSOperationQueue* operationQueue;
@property (nonatomic, strong) OTCuttlefishAccountStateHolder *accountMetadataStore;
@property OTFollowup *followupHandler;

@property (readonly) id<CKKSCloudKitAccountStateTrackingProvider, CKKSOctagonStatusMemoizer> accountStateTracker;
@property CKAccountInfo* cloudKitAccountInfo;
@property CKKSCondition *cloudKitAccountStateKnown;

@property BOOL getViewsSuccess;

@property CKKSNearFutureScheduler* suggestTLKUploadNotifier;

// Dependencies (for injection)
@property id<OTSOSAdapter> sosAdapter;
@property id<CKKSPeerProvider> octagonAdapter;
@property id<OTDeviceInformationAdapter> deviceAdapter;
@property (readonly) Class<OctagonAPSConnection> apsConnectionClass;
@property (readonly) Class<SecEscrowRequestable> escrowRequestClass;

@property (nonatomic) BOOL postedRepairCFU;
@property (nonatomic) BOOL postedEscrowRepairCFU;
@property (nonatomic) BOOL postedRecoveryKeyCFU;

@property (nonatomic) BOOL initialBecomeUntrustedPosted;

@end

@implementation OTCuttlefishContext

- (instancetype)initWithContainerName:(NSString*)containerName
                            contextID:(NSString*)contextID
                           cuttlefish:(id<NSXPCProxyCreating>)cuttlefish
                           sosAdapter:(id<OTSOSAdapter>)sosAdapter
                       authKitAdapter:(id<OTAuthKitAdapter>)authKitAdapter
                      ckksViewManager:(CKKSViewManager* _Nullable)viewManager
                     lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
                  accountStateTracker:(id<CKKSCloudKitAccountStateTrackingProvider, CKKSOctagonStatusMemoizer>)accountStateTracker
             deviceInformationAdapter:(id<OTDeviceInformationAdapter>)deviceInformationAdapter
                   apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                   escrowRequestClass:(Class<SecEscrowRequestable>)escrowRequestClass
                                 cdpd:(id<OctagonFollowUpControllerProtocol>)cdpd
{
    if ((self = [super init])) {
        WEAKIFY(self);

        _containerName = containerName;
        _contextID = contextID;

        _viewManager = viewManager;
        _postedRepairCFU = NO;
        _postedRecoveryKeyCFU = NO;
        _postedEscrowRepairCFU = NO;

        _initialBecomeUntrustedPosted = NO;

        _apsConnectionClass = apsConnectionClass;
        _launchSequence = [[CKKSLaunchSequence alloc] initWithRocketName:@"com.apple.octagon.launch"];

        _queue = dispatch_queue_create("com.apple.security.otcuttlefishcontext", DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];
        _cloudKitAccountStateKnown = [[CKKSCondition alloc] init];

        _accountMetadataStore = [[OTCuttlefishAccountStateHolder alloc] initWithQueue:_queue
                                                                            container:_containerName
                                                                              context:_contextID];
        [_accountMetadataStore registerNotification:self];

        _stateMachine = [[OctagonStateMachine alloc] initWithName:@"octagon"
                                                           states:[NSSet setWithArray:[OctagonStateMap() allKeys]]
                                                            flags:AllOctagonFlags()
                                                     initialState:OctagonStateInitializing
                                                            queue:_queue
                                                      stateEngine:self
                                                 lockStateTracker:lockStateTracker];

        _sosAdapter = sosAdapter;
        [_sosAdapter registerForPeerChangeUpdates:self];
        _authKitAdapter = authKitAdapter;
        _deviceAdapter = deviceInformationAdapter;
        [_deviceAdapter registerForDeviceNameUpdates:self];

        _cuttlefishXPCWrapper = [[CuttlefishXPCWrapper alloc] initWithCuttlefishXPCConnection:cuttlefish];
        _lockStateTracker = lockStateTracker;
        _accountStateTracker = accountStateTracker;

        _followupHandler = [[OTFollowup alloc] initWithFollowupController:cdpd];

        [accountStateTracker registerForNotificationsOfCloudKitAccountStatusChange:self];
        [_authKitAdapter registerNotification:self];

        _escrowRequestClass = escrowRequestClass;

        _suggestTLKUploadNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"octagon-tlk-request"
                                                                            delay:500*NSEC_PER_MSEC
                                                                 keepProcessAlive:false
                                                        dependencyDescriptionCode:0
                                                                            block:^{
                                                                                STRONGIFY(self);
                                                                                secnotice("octagon-ckks", "Adding flag for CKKS TLK upload");
                                                                                [self.stateMachine handleFlag:OctagonFlagCKKSRequestsTLKUpload];
                                                                            }];
    }
    return self;
}

- (void)dealloc
{
    // TODO: how to invalidate this?
    //[self.cuttlefishXPCWrapper invalidate];
}

- (void)notifyTrustChanged:(OTAccountMetadataClassC_TrustState)trustState {

    secnotice("octagon", "Changing trust status to: %@",
              (trustState == OTAccountMetadataClassC_TrustState_TRUSTED) ? @"Trusted" : @"Untrusted");

    /*
     * We are posting the legacy SOS notification if we don't use SOS
     * need to rework clients to use a new signal instead of SOS.
     */
    if (!OctagonPlatformSupportsSOS()) {
        notify_post(kSOSCCCircleChangedNotification);
    }

    notify_post(OTTrustStatusChangeNotification);
}

- (void)accountStateUpdated:(OTAccountMetadataClassC*)newState from:(OTAccountMetadataClassC *)oldState
{
    if (newState.icloudAccountState == OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE && oldState.icloudAccountState != OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE) {
        [self.launchSequence addEvent:@"iCloudAccount"];
    }

    if (newState.trustState == OTAccountMetadataClassC_TrustState_TRUSTED && oldState.trustState != OTAccountMetadataClassC_TrustState_TRUSTED) {
        [self.launchSequence addEvent:@"Trusted"];
    }
    if (newState.trustState != OTAccountMetadataClassC_TrustState_TRUSTED && oldState.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
        [self.launchSequence addEvent:@"Untrusted"];
        [self notifyTrustChanged:newState.trustState];
    }
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<OTCuttlefishContext: %@, %@>", self.containerName, self.contextID];
}

- (void)machinesAdded:(NSArray<NSString*>*)machineIDs altDSID:(NSString*)altDSID
{
    WEAKIFY(self);
    NSError* metadataError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];

    if(!accountMetadata || metadataError) {
        // TODO: collect a sysdiagnose here if the error is not "device is in class D"
        secerror("octagon-authkit: Unable to load account metadata: %@", metadataError);
        [self requestTrustedDeviceListRefresh];
        return;
    }

    if(!altDSID || ![accountMetadata.altDSID isEqualToString:altDSID]) {
        secnotice("octagon-authkit", "Machines-added push is for wrong altDSID (%@); current altDSID (%@)", altDSID, accountMetadata.altDSID);
        return;
    }

    secnotice("octagon-authkit", "adding machines for altDSID(%@): %@", altDSID, machineIDs);

    [self.cuttlefishXPCWrapper addAllowedMachineIDsWithContainer:self.containerName
                                                         context:self.contextID
                                                      machineIDs:machineIDs
                                                           reply:^(NSError* error) {
            STRONGIFY(self);
            if (error) {
                secerror("octagon-authkit: addAllow errored: %@", error);
                [self requestTrustedDeviceListRefresh];
            } else {
                secnotice("octagon-authkit", "addAllow succeeded");

                OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagCuttlefishNotification
                                                                                conditions:OctagonPendingConditionsDeviceUnlocked];
                [self.stateMachine handlePendingFlag:pendingFlag];
            }
        }];
}

- (void)machinesRemoved:(NSArray<NSString*>*)machineIDs altDSID:(NSString*)altDSID
{
    WEAKIFY(self);

    NSError* metadataError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&metadataError];

    if(!accountMetadata || metadataError) {
        // TODO: collect a sysdiagnose here if the error is not "device is in class D"
        secerror("octagon-authkit: Unable to load account metadata: %@", metadataError);
        [self requestTrustedDeviceListRefresh];
        return;
    }

    if(!altDSID || ![accountMetadata.altDSID isEqualToString:altDSID]) {
        secnotice("octagon-authkit", "Machines-removed push is for wrong altDSID (%@); current altDSID (%@)", altDSID, accountMetadata.altDSID);
        return;
    }

    secnotice("octagon-authkit", "removing machines for altDSID(%@): %@", altDSID, machineIDs);

    [self.cuttlefishXPCWrapper removeAllowedMachineIDsWithContainer:self.containerName
                                                            context:self.contextID
                                                         machineIDs:machineIDs
                                                              reply:^(NSError* _Nullable error) {
            STRONGIFY(self);
            if (error) {
                secerror("octagon-authkit: removeAllow errored: %@", error);
            } else {
                secnotice("octagon-authkit", "removeAllow succeeded");
            }

            // We don't necessarily trust remove pushes; they could be delayed past when an add has occurred.
            // Request that the full list be rechecked.
            [self requestTrustedDeviceListRefresh];
        }];
}

- (void)incompleteNotificationOfMachineIDListChange
{
    secnotice("octagon", "incomplete machine ID list notification -- refreshing device list");
    [self requestTrustedDeviceListRefresh];
}


- (void)cloudkitAccountStateChange:(CKAccountInfo* _Nullable)oldAccountInfo
                                to:(CKAccountInfo*)currentAccountInfo
{
    dispatch_sync(self.queue, ^{
        // We don't persist the CK account state; rather, we fetch it anew on every daemon launch.
        // But, we also have to integrate it into our asynchronous state machine.
        // So, record the current CK account value, and trigger state machine reprocessing.

        secnotice("octagon", "Told of a new CK account status: %@", currentAccountInfo);
        self.cloudKitAccountInfo = currentAccountInfo;
        [self.stateMachine _onqueuePokeStateMachine];

        // But, having the state machine perform the signout is confusing: it would need to make decisions based
        // on things other than the current state. So, use the RPC mechanism to give it input.
        // If we receive a sign-in before the sign-out rpc runs, the state machine will be sufficient to get back into
        // the in-account state.

        // Also let other clients now that we have CK account status
        [self.cloudKitAccountStateKnown fulfill];
    });

    if(!(currentAccountInfo.accountStatus == CKAccountStatusAvailable)) {
        secnotice("octagon", "Informed that the CK account is now unavailable: %@", currentAccountInfo);

        // Add a state machine request to return to OctagonStateWaitingForCloudKitAccount
        [self.stateMachine doSimpleStateMachineRPC:@"cloudkit-account-gone"
                                                op:[OctagonStateTransitionOperation named:@"cloudkit-account-gone"
                                                                                 entering:OctagonStateWaitingForCloudKitAccount]
                                      sourceStates:OctagonInAccountStates()
                                             reply:^(NSError* error) {}];
    }
}

- (BOOL)accountAvailable:(NSString*)altDSID error:(NSError**)error
{
    secnotice("octagon", "Account available with altDSID: %@ %@", altDSID, self);

    self.launchSequence.firstLaunch = true;

    NSError* localError = nil;
    [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
        // Do not set the account available bit here, since we need to check if it's HSA2. The initializing state should do that for us...
        metadata.altDSID = altDSID;

        return metadata;
    } error:&localError];

    if(localError) {
        secerror("octagon: unable to persist new account availability: %@", localError);
    }

    [self.stateMachine handleFlag:OctagonFlagAccountIsAvailable];
    
    if(localError) {
        if(error) {
            *error = localError;
        }
        return NO;
    }
    return YES;
}
- (void) moveToCheckTrustedState
{
    CKKSResultOperation<OctagonStateTransitionOperationProtocol>* checkTrust
    = [OctagonStateTransitionOperation named:[NSString stringWithFormat:@"%@", @"check-trust-state"]
                                    entering:OctagonStateCheckTrustState];

    NSSet* sourceStates = [NSSet setWithArray: @[OctagonStateUntrusted]];

    OctagonStateTransitionRequest<OctagonStateTransitionOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"check-trust-state"
                                                                                                              sourceStates:sourceStates
                                                                                                               serialQueue:self.queue
                                                                                                                   timeout:OctagonStateTransitionDefaultTimeout
                                                                                                              transitionOp:checkTrust];
    [self.stateMachine handleExternalRequest:request];
}


- (BOOL)idmsTrustLevelChanged:(NSError**)error
{
    [self.stateMachine handleFlag:OctagonFlagIDMSLevelChanged];
    return YES;
}

- (BOOL)accountNoLongerAvailable:(NSError**)error
{
    OctagonStateTransitionOperation* attemptOp = [OctagonStateTransitionOperation named:@"octagon-account-gone"
                                                                              intending:OctagonStateNoAccountDoReset
                                                                             errorState:OctagonStateNoAccountDoReset
                                                                    withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                                                        __block NSError* localError = nil;

                                                                        secnotice("octagon", "Account now unavailable: %@", self);
                                                                        [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
                                                                            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
                                                                            metadata.altDSID = nil;
                                                                            metadata.trustState = OTAccountMetadataClassC_TrustState_UNKNOWN;

                                                                            return metadata;
                                                                        } error:&localError];

                                                                        if(localError) {
                                                                            secerror("octagon: unable to persist new account availability: %@", localError);
                                                                        }

                                                                        [self.accountStateTracker setHSA2iCloudAccountStatus:CKKSAccountStatusNoAccount];

                                                                        // Bring CKKS down, too
                                                                        for (id key in self.viewManager.views) {
                                                                            CKKSKeychainView* view = self.viewManager.views[key];
                                                                            secnotice("octagon-ckks", "Informing %@ of new untrusted status (due to account disappearance)", view);
                                                                            [view endTrustedOperation];
                                                                        }

                                                                        op.error = localError;
                                                                    }];

    // Signout works from literally any state. Goodbye, account!
    NSSet* sourceStates = [NSSet setWithArray: OctagonStateMap().allKeys];
    OctagonStateTransitionRequest<OctagonStateTransitionOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"account-not-available"
                                                                                                              sourceStates:sourceStates
                                                                                                               serialQueue:self.queue
                                                                                                                   timeout:OctagonStateTransitionDefaultTimeout
                                                                                                              transitionOp:attemptOp];
    [self.stateMachine handleExternalRequest:request];

    return YES;
}

- (void)resetOctagonStateMachine
{
    OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"resetting-state-machine"
                                                                        entering:OctagonStateInitializing];
    NSMutableSet* sourceStates = [NSMutableSet setWithArray: OctagonStateMap().allKeys];

    OctagonStateTransitionRequest<OctagonStateTransitionOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"resetting-state-machine"
                                                                                                              sourceStates:sourceStates
                                                                                                               serialQueue:self.queue
                                                                                                                   timeout:OctagonStateTransitionDefaultTimeout
                                                                                                              transitionOp:op];

    [self.stateMachine handleExternalRequest:request];

}

- (void)localReset:(nonnull void (^)(NSError * _Nullable))reply
{
    OTLocalResetOperation* pendingOp = [[OTLocalResetOperation alloc] init:self.containerName
                                                       contextID:self.contextID
                                                   intendedState:OctagonStateBecomeUntrusted
                                                      errorState:OctagonStateError
                                                   cuttlefishXPCWrapper:self.cuttlefishXPCWrapper];

    NSMutableSet* sourceStates = [NSMutableSet setWithArray: OctagonStateMap().allKeys];
    [self.stateMachine doSimpleStateMachineRPC:@"local-reset" op:pendingOp sourceStates:sourceStates reply:reply];
}

- (NSDictionary*)establishStatePathDictionary
{
    return @{
        OctagonStateReEnactDeviceList: @{
            OctagonStateReEnactPrepare: @{
                OctagonStateReEnactReadyToEstablish: @{
                    OctagonStateEscrowTriggerUpdate: @{
                        OctagonStateBecomeReady: @{
                            OctagonStateReady: [OctagonStateTransitionPathStep success],
                        },
                    },

                    // Error handling extra states:
                    OctagonStateEstablishCKKSReset: @{
                        OctagonStateEstablishAfterCKKSReset: @{
                            OctagonStateEscrowTriggerUpdate: @{
                                OctagonStateBecomeReady: @{
                                    OctagonStateReady: [OctagonStateTransitionPathStep success],
                                },
                            },
                        },
                    },
                },
            },
        },
    };
}

- (void)rpcEstablish:(nonnull NSString *)altDSID
               reply:(nonnull void (^)(NSError * _Nullable))reply
{
    // The reset flow can split into an error-handling path halfway through; this is okay
    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:[self establishStatePathDictionary]];

    [self.stateMachine doWatchedStateMachineRPC:@"establish"
                                   sourceStates:OctagonInAccountStates()
                                           path:path
                                          reply:reply];
}

- (void)rpcResetAndEstablish:(CuttlefishResetReason)resetReason reply:(nonnull void (^)(NSError * _Nullable))reply
{
    _resetReason = resetReason;
    
    // The reset flow can split into an error-handling path halfway through; this is okay
    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary: @{
        OctagonStateResetBecomeUntrusted: @{
            OctagonStateResetAndEstablish: @{
                OctagonStateResetAnyMissingTLKCKKSViews: [self establishStatePathDictionary]
            },
        },
    }];

    // Now, take the state machine from any in-account state to the beginning of the reset flow.
    [self.stateMachine doWatchedStateMachineRPC:@"rpc-reset-and-establish"
                                   sourceStates:OctagonInAccountStates()
                                           path:path
                                          reply:reply];
}

- (void)rpcLeaveClique:(nonnull void (^)(NSError * _Nullable))reply
{
    OTLeaveCliqueOperation* op = [[OTLeaveCliqueOperation alloc] initWithDependencies:self.operationDependencies
                                                                        intendedState:OctagonStateBecomeUntrusted
                                                                           errorState:OctagonStateError];

    NSSet* sourceStates = [NSSet setWithObject: OctagonStateReady];
    [self.stateMachine doSimpleStateMachineRPC:@"leave-clique" op:op sourceStates:sourceStates reply:reply];
}

- (void)rpcRemoveFriendsInClique:(NSArray<NSString*>*)peerIDs
                           reply:(void (^)(NSError * _Nullable))reply
{
    OTRemovePeersOperation* op = [[OTRemovePeersOperation alloc] initWithDependencies:self.operationDependencies
                                                                            intendedState:OctagonStateBecomeReady
                                                                               errorState:OctagonStateBecomeReady
                                                                              peerIDs:peerIDs];

    NSSet* sourceStates = [NSSet setWithObject: OctagonStateReady];
    [self.stateMachine doSimpleStateMachineRPC:@"remove-friends" op:op sourceStates:sourceStates reply:reply];
}

- (OTDeviceInformation*)prepareInformation
{
    NSError* error = nil;
    NSString* machineID = [self.authKitAdapter machineID:&error];

    if(!machineID || error) {
        secerror("octagon: Unable to fetch machine ID; expect signin to fail: %@", error);
    }

    return [[OTDeviceInformation alloc] initForContainerName:self.containerName
                                                   contextID:self.contextID
                                                       epoch:0
                                                   machineID:machineID
                                                     modelID:self.deviceAdapter.modelID
                                                  deviceName:self.deviceAdapter.deviceName
                                                serialNumber:self.deviceAdapter.serialNumber
                                                   osVersion:self.deviceAdapter.osVersion];
}

- (OTOperationDependencies*)operationDependencies
{
    return [[OTOperationDependencies alloc] initForContainer:self.containerName
                                                   contextID:self.contextID
                                                 stateHolder:self.accountMetadataStore
                                                 flagHandler:self.stateMachine
                                                  sosAdapter:self.sosAdapter
                                              octagonAdapter:self.octagonAdapter
                                              authKitAdapter:self.authKitAdapter
                                           deviceInfoAdapter:self.deviceAdapter
                                                 viewManager:self.viewManager
                                            lockStateTracker:self.lockStateTracker
                                        cuttlefishXPCWrapper:self.cuttlefishXPCWrapper
                                          escrowRequestClass:self.escrowRequestClass];
}

- (void)startOctagonStateMachine
{
    [self.stateMachine startOperation];
}

- (void)handlePairingRestart:(OTJoiningConfiguration*)config
{
    if(self.pairingUUID == nil){
        secnotice("octagon-pairing", "received new pairing UUID (%@)", config.pairingUUID);
        self.pairingUUID = config.pairingUUID;
    }

    if(![self.pairingUUID isEqualToString:config.pairingUUID]){
        secnotice("octagon-pairing", "current pairing UUID (%@) does not match config UUID (%@)", self.pairingUUID, config.pairingUUID);

        dispatch_semaphore_t sema = dispatch_semaphore_create(0);
        [self localReset:^(NSError * _Nullable localResetError) {
            if(localResetError) {
                secerror("localReset returned an error: %@", localResetError);
            }else{
                secnotice("octagon", "localReset succeeded");
                self.pairingUUID = config.pairingUUID;
            }
            dispatch_semaphore_signal(sema);
        }];
        if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 10))) {
            secerror("octagon: Timed out waiting for local reset to complete");
        }
    }
}

#pragma mark --- State Machine Transitions

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)_onqueueNextStateMachineTransition:(OctagonState*)currentState
                                                                                                        flags:(nonnull OctagonFlags *)flags
                                                                                                 pendingFlags:(nonnull id<OctagonStateOnqueuePendingFlagHandler>)pendingFlagHandler
{
    dispatch_assert_queue(self.queue);

    WEAKIFY(self);

    [self.launchSequence addEvent:currentState];

    // If We're initializing, or there was some recent update to the account state,
    // attempt to see what state we should enter.
    if([currentState isEqualToString: OctagonStateInitializing]) {
        return [self initializingOperation];
    }

    if([currentState isEqualToString:OctagonStateWaitForHSA2]) {
        if([flags _onqueueContains:OctagonFlagIDMSLevelChanged]) {
            [flags _onqueueRemoveFlag:OctagonFlagIDMSLevelChanged];
            return [OctagonStateTransitionOperation named:@"hsa2-check"
                                                 entering:OctagonStateDetermineiCloudAccountState];
        }

        secnotice("octagon", "Waiting for an HSA2 account");
        return nil;
    }

    if([currentState isEqualToString:OctagonStateWaitingForCloudKitAccount]) {
        // Here, integrate the memoized CK account state into our state machine
        if(self.cloudKitAccountInfo && self.cloudKitAccountInfo.accountStatus == CKAccountStatusAvailable) {
            secnotice("octagon", "CloudKit reports an account is available!");
            return [OctagonStateTransitionOperation named:@"ck-available"
                                                 entering:OctagonStateCloudKitNewlyAvailable];
        } else {
            secnotice("octagon", "Waiting for a CloudKit account; current state is %@", self.cloudKitAccountInfo ?: @"uninitialized");
            return nil;
        }
    }

    if([currentState isEqualToString:OctagonStateCloudKitNewlyAvailable]) {
        return [self cloudKitAccountNewlyAvailableOperation];
    }

    if([currentState isEqualToString:OctagonStateCheckTrustState]) {
        return [[OctagonCheckTrustStateOperation alloc] initWithDependencies:self.operationDependencies
                                                               intendedState:OctagonStateBecomeUntrusted
                                                                  errorState:OctagonStateBecomeUntrusted];
    }
#pragma mark --- Octagon Health Check States
    if([currentState isEqualToString:OctagonStateHSA2HealthCheck]) {
        return [[OTDetermineHSA2AccountStatusOperation alloc] initWithDependencies:self.operationDependencies
                                                                       stateIfHSA2:OctagonStateSecurityTrustCheck
                                                                    stateIfNotHSA2:OctagonStateWaitForHSA2
                                                                  stateIfNoAccount:OctagonStateNoAccount
                                                                        errorState:OctagonStateError];
    }

    if([currentState isEqualToString:OctagonStateSecurityTrustCheck]) {
        return [self evaluateSecdOctagonTrust];
    }

    if([currentState isEqualToString:OctagonStateTPHTrustCheck]) {
        return [self evaluateTPHOctagonTrust];
    }

    if([currentState isEqualToString:OctagonStateCuttlefishTrustCheck]) {
        return [self cuttlefishTrustEvaluation];
    }
    
    if ([currentState isEqualToString:OctagonStatePostRepairCFU]) {
        return [self postRepairCFUAndBecomeUntrusted];
    }

    if ([currentState isEqualToString:OctagonStateHealthCheckReset]) {
        // A small violation of state machines...
        _resetReason = CuttlefishResetReasonHealthCheck;
        return [OctagonStateTransitionOperation named:@"begin-reset"
                                             entering:OctagonStateResetBecomeUntrusted];
    }
    
#pragma mark --- Watch Pairing States

#if TARGET_OS_WATCH
    if([currentState isEqualToString:OctagonStateStartCompanionPairing]) {
        return [self startCompanionPairingOperation];
    }
#endif /* TARGET_OS_WATCH */

    if([currentState isEqualToString:OctagonStateBecomeUntrusted]) {
        return [self becomeUntrustedOperation:OctagonStateUntrusted];
    }

    if([currentState isEqualToString:OctagonStateBecomeReady]) {
        return [self becomeReadyOperation];
    }
    
    if([currentState isEqualToString:OctagonStateNoAccount]) {
        // We only want to move out of untrusted if something useful has happened!
        if([flags _onqueueContains:OctagonFlagAccountIsAvailable]) {
            [flags _onqueueRemoveFlag:OctagonFlagAccountIsAvailable];
            secnotice("octagon", "Account is available!  Attempting initializing op!");
            return [OctagonStateTransitionOperation named:@"account-probably-present"
                                                 entering:OctagonStateInitializing];
        }
    }

    if([currentState isEqualToString:OctagonStateUntrusted]) {
        // We only want to move out of untrusted if something useful has happened!
        if([flags _onqueueContains:OctagonFlagEgoPeerPreapproved]) {
            [flags _onqueueRemoveFlag:OctagonFlagEgoPeerPreapproved];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Preapproved flag is high. Attempt SOS upgrade again!");
                return [OctagonStateTransitionOperation named:@"ck-available"
                                                     entering:OctagonStateAttemptSOSUpgrade];

            } else {
                secnotice("octagon", "We are untrusted, but it seems someone preapproves us now. Unfortunately, this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSUpgrade]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSUpgrade];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempt SOS upgrade again!");
                return [OctagonStateTransitionOperation named:@"attempt-sos-upgrade"
                                                     entering:OctagonStateAttemptSOSUpgrade];

            } else {
                secnotice("octagon", "We are untrusted, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagCuttlefishNotification]) {
            [flags _onqueueRemoveFlag:OctagonFlagCuttlefishNotification];
            secnotice("octagon", "Updating TPH (while untrusted) due to push");
            return [OctagonStateTransitionOperation named:@"untrusted-update"
                                                 entering:OctagonStateUntrustedUpdated];
        }

        // We're untrusted; no need for the IDMS level flag anymore
        if([flags _onqueueContains:OctagonFlagIDMSLevelChanged]) {
            [flags _onqueueRemoveFlag:OctagonFlagIDMSLevelChanged];
        }
    }

    if([currentState isEqualToString:OctagonStateUntrustedUpdated]) {
            return [[OTUpdateTPHOperation alloc] initWithDependencies:self.operationDependencies
                                                        intendedState:OctagonStateUntrusted
                                                           errorState:OctagonStateError
                                                            retryFlag:OctagonFlagCuttlefishNotification];
    }

    if([currentState isEqualToString:OctagonStateDetermineiCloudAccountState]) {
        secnotice("octagon", "Determine iCloud account status");

        // TODO replace with OTDetermineHSA2AccountStatusOperation in <rdar://problem/54094162> Octagon: ensure Octagon operations can't occur on SA accounts
        return [OctagonStateTransitionOperation named:@"octagon-determine-icloud-state"
                                            intending:OctagonStateNoAccount
                                           errorState:OctagonStateError
                                  withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
            STRONGIFY(self);

            NSError *authKitError = nil;
            NSString* primaryAccountAltDSID = [self.authKitAdapter primaryiCloudAccountAltDSID:&authKitError];

            dispatch_sync(self.queue, ^{
                NSError* error = nil;

                if(primaryAccountAltDSID != nil) {
                    secnotice("octagon", "iCloud account is present; checking HSA2 status");

                    bool hsa2 = [self.authKitAdapter accountIsHSA2ByAltDSID:primaryAccountAltDSID];
                    secnotice("octagon", "HSA2 is %@", hsa2 ? @"enabled" : @"disabled");

                    [self.accountMetadataStore _onqueuePersistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
                        if(hsa2) {
                            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE;
                        } else {
                            metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
                        }
                        metadata.altDSID = primaryAccountAltDSID;
                        return metadata;
                    } error:&error];

                    // If there's an HSA2 account, return to 'initializing' here, as we want to centralize decisions on what to do next
                    if(hsa2) {
                        op.nextState = OctagonStateInitializing;
                    } else {
                        [self.accountStateTracker setHSA2iCloudAccountStatus:CKKSAccountStatusNoAccount];
                        op.nextState = OctagonStateWaitForHSA2;
                    }

                } else {
                    secnotice("octagon", "iCloud account is not present: %@", authKitError);

                    [self.accountMetadataStore _onqueuePersistAccountChanges:^OTAccountMetadataClassC *(OTAccountMetadataClassC * metadata) {
                        metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_NO_ACCOUNT;
                        metadata.altDSID = nil;
                        return metadata;
                    } error:&error];

                    op.nextState = OctagonStateNoAccount;
                }

                if(error) {
                    secerror("octagon: unable to save new account state: %@", error);
                }
            });
        }];
    }

    if([currentState isEqualToString:OctagonStateNoAccountDoReset]) {
        secnotice("octagon", "Attempting local-reset as part of signout");
        return [[OTLocalResetOperation alloc] init:self.containerName
                                         contextID:self.contextID
                                     intendedState:OctagonStateNoAccount
                                        errorState:OctagonStateNoAccount
                              cuttlefishXPCWrapper:self.cuttlefishXPCWrapper];
    }

    if([currentState isEqualToString:OctagonStateEnsureConsistency]) {
        secnotice("octagon", "Ensuring consistency of things that might've changed");
        if(self.sosAdapter.sosEnabled) {
            return [[OTEnsureOctagonKeyConsistency alloc] initWithDependencies:self.operationDependencies
                                                                 intendedState:OctagonStateEnsureUpdatePreapprovals
                                                                    errorState:OctagonStateBecomeReady];
        }

        // Add further consistency checks here.
        return [OctagonStateTransitionOperation named:@"no-consistency-checks"
                                             entering:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateEnsureUpdatePreapprovals]) {
        secnotice("octagon", "SOS is enabled; ensuring preapprovals are correct");
        return [[OTSOSUpdatePreapprovalsOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:OctagonStateBecomeReady
                                                           sosNotPresentState:OctagonStateBecomeReady
                                                                   errorState:OctagonStateBecomeReady];
    }

    if([currentState isEqualToString:OctagonStateAttemptSOSUpgrade] && OctagonPerformSOSUpgrade()) {
        secnotice("octagon", "Investigating SOS status");
        return [[OTSOSUpgradeOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateBecomeReady
                                                 ckksConflictState:OctagonStateSOSUpgradeCKKSReset
                                                        errorState:OctagonStateBecomeUntrusted
                                                        deviceInfo:self.prepareInformation];

    } else if([currentState isEqualToString:OctagonStateSOSUpgradeCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateSOSUpgradeAfterCKKSReset
                                                            errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateSOSUpgradeAfterCKKSReset]) {
        return [[OTSOSUpgradeOperation alloc] initWithDependencies:self.operationDependencies
                                                     intendedState:OctagonStateBecomeReady
                                                 ckksConflictState:OctagonStateBecomeUntrusted
                                                        errorState:OctagonStateBecomeUntrusted
                                                        deviceInfo:self.prepareInformation];


    } else if([currentState isEqualToString:OctagonStateCreateIdentityForRecoveryKey]) {
        OTPrepareOperation* op = [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                                    intendedState:OctagonStateVouchWithRecoveryKey
                                                                       errorState:OctagonStateBecomeUntrusted
                                                                       deviceInfo:[self prepareInformation]
                                                                            epoch:1];
        return op;

    } else if([currentState isEqualToString:OctagonStateInitiatorCreateIdentity]) {
        OTPrepareOperation* op = [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                                           intendedState:OctagonStateInitiatorVouchWithBottle
                                                                              errorState:OctagonStateBecomeUntrusted
                                                                              deviceInfo:[self prepareInformation]
                                                                                   epoch:1];
        return op;

    } else if([currentState isEqualToString:OctagonStateInitiatorVouchWithBottle]) {
        OTVouchWithBottleOperation* pendingOp  = [[OTVouchWithBottleOperation alloc] initWithDependencies:self.operationDependencies
                                                                                            intendedState:OctagonStateInitiatorUpdateDeviceList
                                                                                               errorState:OctagonStateBecomeUntrusted
                                                                                                 bottleID:_bottleID
                                                                                                  entropy:_entropy
                                                                                               bottleSalt:_bottleSalt];

        CKKSResultOperation* callback = [CKKSResultOperation named:@"vouchWithBottle-callback"
                                                         withBlock:^{
                                                             secnotice("otrpc", "Returning a vouch with bottle call: %@, %@  %@", pendingOp.voucher, pendingOp.voucherSig, pendingOp.error);
                                                             self->_vouchSig = pendingOp.voucherSig;
                                                             self->_vouchData = pendingOp.voucher;
                                                         }];
        [callback addDependency:pendingOp];
        [self.operationQueue addOperation: callback];

        return pendingOp;

    } else if([currentState isEqualToString:OctagonStateVouchWithRecoveryKey]) {
        OTVouchWithRecoveryKeyOperation* pendingOp  = [[OTVouchWithRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                                                            intendedState:OctagonStateInitiatorUpdateDeviceList
                                                                                               errorState:OctagonStateBecomeUntrusted
                                                                                                 recoveryKey:_recoveryKey];

        CKKSResultOperation* callback = [CKKSResultOperation named:@"vouchWithRecoveryKey-callback"
                                                         withBlock:^{
                                                             secnotice("otrpc", "Returning a vouch with recovery key call: %@, %@  %@", pendingOp.voucher, pendingOp.voucherSig, pendingOp.error);
                                                             self->_vouchSig = pendingOp.voucherSig;
                                                             self->_vouchData = pendingOp.voucher;
                                                         }];
        [callback addDependency:pendingOp];
        [self.operationQueue addOperation: callback];

        return pendingOp;

    } else if([currentState isEqualToString:OctagonStateInitiatorUpdateDeviceList]) {
        // As part of the 'initiate' flow, we need to update the trusted device list-you're probably on it already
        OTUpdateTrustedDeviceListOperation* op = [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                                                    intendedState:OctagonStateInitiatorJoin
                                                                                                 listUpdatesState:OctagonStateInitiatorJoin
                                                                                                       errorState:OctagonStateBecomeUntrusted
                                                                                                        retryFlag:nil];
        return op;

    } else if ([currentState isEqualToString:OctagonStateInitiatorJoin]){
        OTJoinWithVoucherOperation* op  = [[OTJoinWithVoucherOperation alloc] initWithDependencies:self.operationDependencies
                                                                                     intendedState:OctagonStateBecomeReady
                                                                                 ckksConflictState:OctagonStateInitiatorJoinCKKSReset
                                                                                        errorState:OctagonStateBecomeUntrusted
                                                                                       voucherData:_vouchData
                                                                                        voucherSig:_vouchSig
                                                                                   preapprovedKeys:_preapprovedKeys];
        return op;

    } else if([currentState isEqualToString:OctagonStateInitiatorJoinCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateInitiatorJoinAfterCKKSReset
                                                            errorState:OctagonStateBecomeUntrusted];

    } else if ([currentState isEqualToString:OctagonStateInitiatorJoinAfterCKKSReset]){
        return [[OTJoinWithVoucherOperation alloc] initWithDependencies:self.operationDependencies
                                                          intendedState:OctagonStateBecomeReady
                                                      ckksConflictState:OctagonStateBecomeUntrusted
                                                             errorState:OctagonStateBecomeUntrusted
                                                            voucherData:_vouchData
                                                             voucherSig:_vouchSig
                                                        preapprovedKeys:_preapprovedKeys];

    } else if([currentState isEqualToString:OctagonStateResetBecomeUntrusted]) {
        return [self becomeUntrustedOperation:OctagonStateResetAndEstablish];

    } else if([currentState isEqualToString:OctagonStateResetAndEstablish]) {
        return [[OTResetOperation alloc] init:self.containerName
                                    contextID:self.contextID
                                       reason:_resetReason
                                intendedState:OctagonStateResetAnyMissingTLKCKKSViews
                                   errorState:OctagonStateError
                         cuttlefishXPCWrapper:self.cuttlefishXPCWrapper];

    } else if([currentState isEqualToString:OctagonStateResetAnyMissingTLKCKKSViews]) {
        return [[OTResetCKKSZonesLackingTLKsOperation alloc] initWithDependencies:self.operationDependencies
                                                           intendedState:OctagonStateReEnactDeviceList
                                                              errorState:OctagonStateError];

    } else if([currentState isEqualToString:OctagonStateReEnactDeviceList]) {
        return [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                  intendedState:OctagonStateReEnactPrepare
                                                               listUpdatesState:OctagonStateReEnactPrepare
                                                                     errorState:OctagonStateError
                                                                      retryFlag:nil];

    } else if([currentState isEqualToString:OctagonStateReEnactPrepare]) {
        // Note: Resetting the account returns epoch to 0.
        return [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                  intendedState:OctagonStateReEnactReadyToEstablish
                                                     errorState:OctagonStateError
                                                     deviceInfo:[self prepareInformation]
                                                          epoch:0];

    } else if([currentState isEqualToString:OctagonStateReEnactReadyToEstablish]) {
        return [[OTEstablishOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateEscrowTriggerUpdate
                                                ckksConflictState:OctagonStateEstablishCKKSReset
                                                       errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateEstablishCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateEstablishAfterCKKSReset
                                                            errorState:OctagonStateBecomeUntrusted];

    } else if([currentState isEqualToString:OctagonStateEstablishAfterCKKSReset]) {
        // If CKKS fails again, just go to "become untrusted"
        return [[OTEstablishOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateEscrowTriggerUpdate
                                                   ckksConflictState:OctagonStateBecomeUntrusted
                                                       errorState:OctagonStateBecomeUntrusted];

    } else if ([currentState isEqualToString:OctagonStateEscrowTriggerUpdate]){

        return [[OTTriggerEscrowUpdateOperation alloc] initWithDependencies:self.operationDependencies
                                                              intendedState:OctagonStateBecomeReady
                                                                 errorState:OctagonStateError];

    } else if([currentState isEqualToString: OctagonStateWaitForUnlock]) {
        if([flags _onqueueContains:OctagonFlagUnlocked]) {
            [flags _onqueueRemoveFlag:OctagonFlagUnlocked];
            return [OctagonStateTransitionOperation named:[NSString stringWithFormat:@"%@", @"initializing-after-unlock"]
                                                 entering:OctagonStateInitializing];
        }

        secnotice("octagon", "Requested to enter wait for unlock");
        [pendingFlagHandler _onqueueHandlePendingFlag:[[OctagonPendingFlag alloc] initWithFlag:OctagonFlagUnlocked
                                                                                    conditions:OctagonPendingConditionsDeviceUnlocked]];
        return nil;

    } else if([currentState isEqualToString: OctagonStateUpdateSOSPreapprovals]) {
        secnotice("octagon", "Updating SOS preapprovals");

        // TODO: if this update fails, we need to redo it later.
        return [[OTSOSUpdatePreapprovalsOperation alloc] initWithDependencies:self.operationDependencies
                                                                intendedState:OctagonStateReady
                                                           sosNotPresentState:OctagonStateReady
                                                                   errorState:OctagonStateReady];

    } else if([currentState isEqualToString:OctagonStateAssistCKKSTLKUpload]) {
        return [[OTUploadNewCKKSTLKsOperation alloc] initWithDependencies:self.operationDependencies
                                                            intendedState:OctagonStateReady
                                                        ckksConflictState:OctagonStateAssistCKKSTLKUploadCKKSReset
                                                               errorState:OctagonStateReady];

    } else if([currentState isEqualToString:OctagonStateAssistCKKSTLKUploadCKKSReset]) {
        return [[OTLocalCKKSResetOperation alloc] initWithDependencies:self.operationDependencies
                                                         intendedState:OctagonStateAssistCKKSTLKUploadAfterCKKSReset
                                                            errorState:OctagonStateBecomeReady];

    } else if([currentState isEqualToString:OctagonStateAssistCKKSTLKUploadAfterCKKSReset]) {
        // If CKKS fails again, just go to 'ready'
        return [[OTUploadNewCKKSTLKsOperation alloc] initWithDependencies:self.operationDependencies
                                                            intendedState:OctagonStateReady
                                                        ckksConflictState:OctagonStateReady
                                                               errorState:OctagonStateReady];

    } else if([currentState isEqualToString:OctagonStateReady]) {
        if([flags _onqueueContains:OctagonFlagCKKSRequestsTLKUpload]) {
            [flags _onqueueRemoveFlag:OctagonFlagCKKSRequestsTLKUpload];
            return [OctagonStateTransitionOperation named:@"ckks-assist"
                                                 entering:OctagonStateAssistCKKSTLKUpload];
        }

        if([flags _onqueueContains:OctagonFlagCuttlefishNotification]) {
            [flags _onqueueRemoveFlag:OctagonFlagCuttlefishNotification];
            secnotice("octagon", "Updating TPH (while ready) due to push");
            return [OctagonStateTransitionOperation named:@"octagon-update"
                                                 entering:OctagonStateReadyUpdated];
        }

        if([flags _onqueueContains:OctagonFlagFetchAuthKitMachineIDList]) {
            [flags _onqueueRemoveFlag:OctagonFlagFetchAuthKitMachineIDList];

            secnotice("octagon", "Received an suggestion to update the machine ID list (while ready); updating trusted device list");

            // If the cached list changes due to this fetch, go into 'updated'. Otherwise, back into ready with you!
            return [[OTUpdateTrustedDeviceListOperation alloc] initWithDependencies:self.operationDependencies
                                                                      intendedState:OctagonStateReady
                                                                   listUpdatesState:OctagonStateReadyUpdated
                                                                         errorState:OctagonStateReady
                                                                          retryFlag:OctagonFlagFetchAuthKitMachineIDList];
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSUpdatePreapprovals]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSUpdatePreapprovals];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempt SOS Update preapprovals again!");
                return [OctagonStateTransitionOperation named:@"attempt-sos-update-preapproval"
                                                     entering:OctagonStateUpdateSOSPreapprovals];
            } else {
                secnotice("octagon", "We are untrusted, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAttemptSOSConsistency]) {
            [flags _onqueueRemoveFlag:OctagonFlagAttemptSOSConsistency];
            if(self.sosAdapter.sosEnabled) {
                secnotice("octagon", "Attempting SOS consistency checks");
                return [OctagonStateTransitionOperation named:@"attempt-sos-update-preapproval"
                                                     entering:OctagonStateEnsureConsistency];
            } else {
                secnotice("octagon", "Someone would like us to check SOS consistency, but this platform doesn't support SOS.");
            }
        }

        if([flags _onqueueContains:OctagonFlagAccountIsAvailable]) {
            // We're in ready--we already know the account is available
            secnotice("octagon", "Removing 'account is available' flag");
            [flags _onqueueRemoveFlag:OctagonFlagAccountIsAvailable];
        }

        // We're ready; no need for the IDMS level flag anymore
        if([flags _onqueueContains:OctagonFlagIDMSLevelChanged]) {
            secnotice("octagon", "Removing 'IDMS level changed' flag");
            [flags _onqueueRemoveFlag:OctagonFlagIDMSLevelChanged];
        }

        secnotice("octagon", "Entering state ready");
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:OctagonAnalyticsLastKeystateReady];
        [self.launchSequence launch];
        return nil;
    } else if([currentState isEqualToString:OctagonStateReadyUpdated]) {
        return [[OTUpdateTPHOperation alloc] initWithDependencies:self.operationDependencies
                                                    intendedState:OctagonStateReady
                                                       errorState:OctagonStateError
                                                        retryFlag:OctagonFlagCuttlefishNotification];

    } else if ([currentState isEqualToString:OctagonStateError]) {
    }

    return nil;
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)initializingOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-initializing"
                                        intending:OctagonStateNoAccount
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);
                                  NSError* localError = nil;
                                  OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
                                  if(localError && [self.lockStateTracker isLockedError:localError]){
                                      secnotice("octagon", "Device is locked! pending initialization on unlock");
                                      op.nextState = OctagonStateWaitForUnlock;
                                      return;
                                  }

                                  if(localError || !account) {
                                      secnotice("octagon", "Error loading account data: %@", localError);
                                      op.nextState = OctagonStateNoAccount;

                                  } else if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE) {
                                      secnotice("octagon", "An HSA2 iCloud account exists; waiting for CloudKit to confirm");

                                      // Inform the account state tracker of our HSA2 account
                                      [self.accountStateTracker setHSA2iCloudAccountStatus:CKKSAccountStatusAvailable];

                                      // This seems an odd place to do this, but CKKS currently also tracks the CloudKit account state.
                                      // Since we think we have an HSA2 account, let CKKS figure out its own CloudKit state
                                      secnotice("octagon-ckks", "Initializing CKKS views");
                                      [self.viewManager createViews];
                                      [self.viewManager beginCloudKitOperationOfAllViews];

                                      op.nextState = OctagonStateWaitingForCloudKitAccount;

                                  } else if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_NO_ACCOUNT && account.altDSID != nil) {
                                      secnotice("octagon", "An iCloud account exists, but doesn't appear to be HSA2. Let's check!");
                                      op.nextState = OctagonStateDetermineiCloudAccountState;

                                  } else if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_NO_ACCOUNT) {
                                      [self.accountStateTracker setHSA2iCloudAccountStatus:CKKSAccountStatusNoAccount];

                                      secnotice("octagon", "No iCloud account available.");
                                      op.nextState = OctagonStateNoAccount;

                                  } else {
                                      secnotice("octagon", "Unknown account state (%@). Determining...", [account icloudAccountStateAsString:account.icloudAccountState]);
                                      op.nextState = OctagonStateDetermineiCloudAccountState;
                                  }
                              }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)evaluateSecdOctagonTrust
{
    return [OctagonStateTransitionOperation named:@"octagon-health-securityd-trust-check"
                                 intending:OctagonStateTPHTrustCheck
                                errorState:OctagonStatePostRepairCFU
                       withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                           NSError* localError = nil;
                           OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
                           if(account.peerID && account.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
                               secnotice("octagon-health", "peer is trusted: %@", account.peerID);
                               op.nextState = OctagonStateTPHTrustCheck;

                           } else {
                               secnotice("octagon-health", "trust state (%@). checking in with TPH", [account trustStateAsString:account.trustState]);
                               op.nextState = [self repairAccountIfTrustedByTPHWithIntededState:OctagonStateTPHTrustCheck errorState:OctagonStatePostRepairCFU];
                           }
                       }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)evaluateTPHOctagonTrust
{
    return [OctagonStateTransitionOperation named:@"octagon-health-tph-trust-check"
                                        intending:OctagonStateCuttlefishTrustCheck
                                       errorState:OctagonStatePostRepairCFU
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  [self checkTrustStatusAndPostRepairCFUIfNecessary:^(CliqueStatus status, BOOL posted, BOOL hasIdentity, NSError *trustFromTPHError) {

                                      [[CKKSAnalytics logger] logResultForEvent:OctagonEventTPHHealthCheckStatus hardFailure:false result:trustFromTPHError];
                                      if(trustFromTPHError) {
                                          secerror("octagon-health: hit an error asking TPH for trust status: %@", trustFromTPHError);
                                          op.error = trustFromTPHError;
                                          op.nextState = OctagonStateError;
                                      } else {
                                          if(hasIdentity == NO) {
                                              op.nextState = OctagonStateUntrusted;
                                          } else if(hasIdentity == YES && status == CliqueStatusIn){
                                              secnotice("octagon-health", "TPH says we're trusted and in");
                                              op.nextState = OctagonStateCuttlefishTrustCheck;
                                          } else if (hasIdentity == YES && status != CliqueStatusIn){
                                              secnotice("octagon-health", "TPH says we have an identity but we are not in Octagon, posted CFU: %d", !!posted);
                                              op.nextState = OctagonStatePostRepairCFU;
                                          } else {
                                              secnotice("octagon-health", "weird shouldn't hit this catch all.. assuming untrusted");
                                              op.nextState = OctagonStateUntrusted;
                                          }
                                      }
                                  }];
                              }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)cuttlefishTrustEvaluation
{

    OTCheckHealthOperation* op = [[OTCheckHealthOperation alloc] initWithDependencies:self.operationDependencies
                                                                        intendedState:OctagonStateBecomeReady
                                                                           errorState:OctagonStateBecomeReady
                                                                           deviceInfo:self.prepareInformation
                                                                 skipRateLimitedCheck:_skipRateLimitingCheck];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"rpcHealthCheck"
                                                     withBlock:^{
                                                         secnotice("octagon-health", "Returning from cuttlefish trust check call: postRepairCFU(%d), postEscrowCFU(%d), resetOctagon(%d)",
                                                                   op.postRepairCFU, op.postEscrowCFU, op.resetOctagon);
                                                         if(op.postRepairCFU) {
                                                             secnotice("octagon-health", "Posting Repair CFU");
                                                             NSError* postRepairCFUError = nil;
                                                             [self postRepairCFU:&postRepairCFUError];
                                                             if(postRepairCFUError) {
                                                                 op.error = postRepairCFUError;
                                                             }
                                                         }
                                                         if(op.postEscrowCFU) {
                                                             //hold up, perhaps we already are pending an upload.
                                                             NSError* shouldPostError = nil;
                                                             BOOL shouldPost = [self shouldPostConfirmPasscodeCFU:&shouldPostError];
                                                             if(shouldPostError) {
                                                                 secerror("octagon-health, hit an error evaluating prerecord status: %@", shouldPostError);
                                                                 op.error = shouldPostError;
                                                             }
                                                             if(shouldPost) {
                                                                 secnotice("octagon-health", "Posting Escrow CFU");
                                                                 NSError* postEscrowCFUError = nil;
                                                                 [self postConfirmPasscodeCFU:&postEscrowCFUError];
                                                                 if(postEscrowCFUError) {
                                                                     op.error = postEscrowCFUError;
                                                                 }
                                                             } else {
                                                                 secnotice("octagon-health", "Not posting confirm passcode CFU, already pending a prerecord upload");
                                                             }
                                                         }
                                                     }];
    [callback addDependency:op];
    [self.operationQueue addOperation: callback];
    return op;
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)postRepairCFUAndBecomeUntrusted
{
    return [OctagonStateTransitionOperation named:@"octagon-health-post-repair-cfu"
                                        intending:OctagonStateUntrusted
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        [self checkTrustStatusAndPostRepairCFUIfNecessary:^(CliqueStatus status,
                                                            BOOL posted,
                                                            BOOL hasIdentity,
                                                            NSError * _Nullable postError) {
            if(postError) {
                secerror("ocagon-health: failed to post repair cfu via state machine: %@", postError);
            } else {
                secnotice("octagon-health", "posted repair cfu via state machine");
            }
        }];
        op.nextState = OctagonStateUntrusted;
      }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)cloudKitAccountNewlyAvailableOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-icloud-account-available"
                                        intending:OctagonStateCheckTrustState
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);
                                  // Register with APS, but don't bother to wait until it's complete.
                                  secnotice("octagon", "iCloud sign in occurred. Attemping to register with APS...");

                                  CKContainer* ckContainer = [CKContainer containerWithIdentifier:self.containerName];
                                  [ckContainer serverPreferredPushEnvironmentWithCompletionHandler: ^(NSString *apsPushEnvString, NSError *error) {
                                      STRONGIFY(self);

                                      if(!self) {
                                          secerror("octagonpush: received callback for released object");
                                          return;
                                      }

                                      if(error || (apsPushEnvString == nil)) {
                                          secerror("octagonpush: Received error fetching preferred push environment (%@): %@", apsPushEnvString, error);
                                      } else {
                                          secnotice("octagonpush", "Registering for environment '%@'", apsPushEnvString);

                                          OctagonAPSReceiver* aps = [OctagonAPSReceiver receiverForEnvironment:apsPushEnvString
                                                                                             namedDelegatePort:SecCKKSAPSNamedPort
                                                                                            apsConnectionClass:self.apsConnectionClass];
                                          [aps registerCuttlefishReceiver:self forContainerName:self.containerName];
                                      }
                                  }];

                                  op.nextState = op.intendedState;
                              }];
}

- (OctagonState*) repairAccountIfTrustedByTPHWithIntededState:(OctagonState*)intendedState errorState:(OctagonState*)errorState
{
    __block OctagonState* nextState = intendedState;

    //let's check in with TPH real quick to make sure it agrees with our local assessment
    secnotice("octagon-health", "repairAccountIfTrustedByTPHWithIntededState: calling into TPH for trust status");
   
    OTOperationConfiguration *config = [[OTOperationConfiguration alloc]init];
   
    [self rpcTrustStatus:config reply:^(CliqueStatus status,
                                        NSString* egoPeerID,
                                        NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                        BOOL isExcluded,
                                        NSError * _Nullable error) {
        BOOL hasIdentity = egoPeerID != nil;
        secnotice("octagon-health", "repairAccountIfTrustedByTPHWithIntededState status: %ld, peerID: %@, isExcluded: %d error: %@", (long)status, egoPeerID, isExcluded, error);

        if (error) {
            secnotice("octagon-health", "got an error from tph, returning to become_ready state: %@", error);
            nextState = OctagonStateBecomeReady;
            return;
        }

        if(OctagonAuthoritativeTrustIsEnabled() && hasIdentity && status == CliqueStatusIn) {
            dispatch_semaphore_t sema = dispatch_semaphore_create(0);
            [self rpcStatus:^(NSDictionary *dump, NSError *dumpError) {
                if(dumpError) {
                    secerror("octagon-health: error fetching ego peer id!: %@", dumpError);
                    nextState = errorState;
                } else {
                    NSDictionary* egoInformation = dump[@"self"];
                    NSString* peerID = egoInformation[@"peerID"];
                    NSError* persistError = nil;
                    BOOL persisted = [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                        metadata.trustState = OTAccountMetadataClassC_TrustState_TRUSTED;
                        metadata.icloudAccountState = OTAccountMetadataClassC_AccountState_ACCOUNT_AVAILABLE;
                        metadata.peerID = peerID;
                        return metadata;
                    } error:&persistError];
                    if(!persisted || persistError) {
                        secerror("octagon-health: couldn't persist results: %@", persistError);
                        nextState = errorState;
                    } else {
                        secnotice("octagon-health", "added trusted identity to account metadata");
                        nextState = intendedState;
                    }
                }
                dispatch_semaphore_signal(sema);
            }];
            if (0 != dispatch_semaphore_wait(sema, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * 10))) {
                secerror("octagon: Timed out checking trust status");
            }
        } else if (OctagonAuthoritativeTrustIsEnabled() && (self.postedRepairCFU == NO) && hasIdentity && status != CliqueStatusIn){
            nextState = errorState;
        }
    }];

    return nextState;
}

- (BOOL) didDeviceAttemptToJoinOctagon:(NSError**)error
{
    NSError* fetchAttemptError = nil;
    OTAccountMetadataClassC_AttemptedAJoinState attemptedAJoin = [self.accountMetadataStore fetchPersistedJoinAttempt:&fetchAttemptError];
    if(fetchAttemptError) {
        secerror("octagon: failed to fetch data indicating device attempted to join octagon, assuming it did: %@", fetchAttemptError);
        if(error){
            *error = fetchAttemptError;
        }
        return YES;
    }
    BOOL attempted = YES;
    switch (attemptedAJoin) {
        case OTAccountMetadataClassC_AttemptedAJoinState_NOTATTEMPTED:
            attempted = NO;
            break;
        case OTAccountMetadataClassC_AttemptedAJoinState_ATTEMPTED:
        case OTAccountMetadataClassC_AttemptedAJoinState_UNKNOWN:
        default:
            break;
    }
    return attempted;
}

- (void)checkTrustStatusAndPostRepairCFUIfNecessary:(void (^ _Nullable)(CliqueStatus status, BOOL posted, BOOL hasIdentity, NSError * _Nullable error))reply
{
    WEAKIFY(self);
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];
    [self rpcTrustStatus:configuration reply:^(CliqueStatus status,
                                               NSString* _Nullable egoPeerID,
                                               NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                               BOOL isExcluded,
                                               NSError * _Nullable error) {
        STRONGIFY(self);

        secnotice("octagon", "clique status: %@, egoPeerID: %@, peerCountByModelID: %@, isExcluded: %d error: %@", OTCliqueStatusToString(status), egoPeerID, peerCountByModelID, isExcluded, error);

        BOOL hasIdentity = egoPeerID != nil;
        if (error && error.code != errSecInteractionNotAllowed) {
            reply(status, NO, hasIdentity, error);
            return;
        }

#if TARGET_OS_TV
        // Are there any iphones or iPads? about? Only iOS devices can repair apple TVs.
        bool phonePeerPresent = false;
        for(NSString* modelID in peerCountByModelID.allKeys) {
            bool iPhone = [modelID hasPrefix:@"iPhone"];
            bool iPad = [modelID hasPrefix:@"iPad"];
            if(!iPhone && !iPad) {
                continue;
            }

            int count = [peerCountByModelID[modelID] intValue];
            if(count > 0) {
                secnotice("octagon", "Have %d peers with model %@", count, modelID);
                phonePeerPresent = true;
                break;
            }
        }
        if(!phonePeerPresent) {
            secnotice("octagon", "No iOS peers in account; not posting CFU");
            reply(status, NO, hasIdentity, nil);
            return;
        }
#endif

        // On platforms with SOS, we only want to post a CFU if we've attempted to join at least once.
        // This prevents us from posting a CFU, then performing an SOS upgrade and succeeding.
        if(self.sosAdapter.sosEnabled) {
            NSError* fetchAttemptError = nil;
            BOOL attemptedToJoin = [self didDeviceAttemptToJoinOctagon:&fetchAttemptError];
            if(fetchAttemptError){
                secerror("octagon: failed to retrieve joining attempt information: %@", fetchAttemptError);
                attemptedToJoin = YES;
            }

            if(!attemptedToJoin) {
                secnotice("octagon", "SOS is enabled and we haven't attempted to join; not posting CFU");
                reply(status, NO, hasIdentity, nil);
                return;
            }
        }

        if(OctagonAuthoritativeTrustIsEnabled() && (status == CliqueStatusNotIn || status == CliqueStatusAbsent || isExcluded)) {
            NSError* localError = nil;
            BOOL posted = [self postRepairCFU:&localError];
            reply(status, posted, hasIdentity, localError);
            return;
        }
        reply(status, NO, hasIdentity, nil);
        return;
    }];
}

#if TARGET_OS_WATCH
- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)startCompanionPairingOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"start-companion-pairing"
                                        intending:OctagonStateBecomeUntrusted
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
        STRONGIFY(self);
        OTPairingInitiateWithCompletion(self.queue, ^(bool success, NSError *error) {
            if (success) {
                secnotice("octagon", "companion pairing succeeded");
            } else {
                if (error == nil) {
                    error = [NSError errorWithDomain:(__bridge NSString *)kSecErrorDomain code:errSecInternalError userInfo:nil];
                }
                secnotice("octagon", "companion pairing failed: %@", error);
            }
            [[CKKSAnalytics logger] logResultForEvent:OctagonEventCompanionPairing hardFailure:false result:error];
        });
        op.nextState = op.intendedState;
    }];
}
#endif /* TARGET_OS_WATCH */

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)becomeUntrustedOperation:(OctagonState*)intendedState
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-become-untrusted"
                                        intending:intendedState
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);
                                  NSError* localError = nil;

                                  [self.accountStateTracker triggerOctagonStatusFetch];

                                  [self checkTrustStatusAndPostRepairCFUIfNecessary:^(CliqueStatus status, BOOL posted, BOOL hasIdentity, NSError * _Nullable postError) {

                                      [[CKKSAnalytics logger] logResultForEvent:OctagonEventCheckTrustForCFU hardFailure:false result:postError];
                                      if(postError){
                                          secerror("octagon: cfu failed to post");
                                      } else {
                                          secnotice("octagon", "clique status: %@, posted cfu: %d", OTCliqueStatusToString(status), !!posted);
                                      }
                                  }];

                                  [self.accountMetadataStore persistAccountChanges:^OTAccountMetadataClassC * _Nonnull(OTAccountMetadataClassC * _Nonnull metadata) {
                                      metadata.trustState = OTAccountMetadataClassC_TrustState_UNTRUSTED;
                                      return metadata;
                                  } error:&localError];

                                  if(localError) {
                                      secnotice("octagon", "Unable to set trust state: %@", localError);
                                      op.nextState = OctagonStateError;
                                  } else {
                                      op.nextState = op.intendedState;
                                  }

                                  for (id key in self.viewManager.views) {
                                      CKKSKeychainView* view = self.viewManager.views[key];
                                      secnotice("octagon-ckks", "Informing %@ of new untrusted status", view);
                                      [view endTrustedOperation];
                                  }

                                  /*
                                   * Initial notification that we let the world know that trust is up and doing something
                                   */
                                  if (!self.initialBecomeUntrustedPosted) {
                                      [self notifyTrustChanged:OTAccountMetadataClassC_TrustState_UNTRUSTED];
                                      self.initialBecomeUntrustedPosted = YES;
                                  }

                                  self.octagonAdapter = nil;
                              }];
}

- (CKKSResultOperation<OctagonStateTransitionOperationProtocol>* _Nullable)becomeReadyOperation
{
    WEAKIFY(self);
    return [OctagonStateTransitionOperation named:@"octagon-ready"
                                        intending:OctagonStateReady
                                       errorState:OctagonStateError
                              withBlockTakingSelf:^(OctagonStateTransitionOperation * _Nonnull op) {
                                  STRONGIFY(self);

                                  // Note: we don't modify the account metadata store here; that will have been done
                                  // by a join or upgrade operation, possibly long ago

                                  [self.accountStateTracker triggerOctagonStatusFetch];

                                  NSError* localError = nil;
                                  NSString* peerID = [self.accountMetadataStore getEgoPeerID:&localError];
                                  if(!peerID || localError) {
                                      secerror("octagon-ckks: No peer ID to pass to CKKS. Syncing will be disabled.");
                                  } else {
                                      OctagonCKKSPeerAdapter* octagonAdapter = [[OctagonCKKSPeerAdapter alloc] initWithPeerID:peerID operationDependencies:[self operationDependencies]];

                                      // This octagon adapter must be able to load the self peer keys, or we're in trouble.
                                      NSError* egoPeerKeysError = nil;
                                      CKKSSelves* selves = [octagonAdapter fetchSelfPeers:&egoPeerKeysError];
                                      if(!selves || egoPeerKeysError) {
                                          secerror("octagon-ckks: Unable to fetch self peers for %@: %@", octagonAdapter, egoPeerKeysError);

                                          if([self.lockStateTracker isLockedError:egoPeerKeysError]) {
                                              secnotice("octagon-ckks", "Waiting for device unlock to proceed");
                                              op.nextState = OctagonStateWaitForUnlock;
                                          } else {
                                              secnotice("octagon-ckks", "Error is scary; becoming untrusted");
                                              op.nextState = OctagonStateBecomeUntrusted;
                                          }
                                          return;
                                      }

                                      // stash a reference to the adapter so we can provided updates later
                                      self.octagonAdapter = octagonAdapter;

                                      // Start all our CKKS views!
                                      for (id key in self.viewManager.views) {
                                          CKKSKeychainView* view = self.viewManager.views[key];
                                          secnotice("octagon-ckks", "Informing CKKS view '%@' of trusted operation with self peer %@", view.zoneName, peerID);

                                          NSArray<id<CKKSPeerProvider>>* peerProviders = nil;

                                          if(self.sosAdapter.sosEnabled) {
                                              peerProviders = @[self.octagonAdapter, self.sosAdapter];

                                          } else {
                                              peerProviders = @[self.octagonAdapter];
                                          }

                                          [view beginTrustedOperation:peerProviders
                                                     suggestTLKUpload:self.suggestTLKUploadNotifier];
                                      }
                                  }
                                  [self notifyTrustChanged:OTAccountMetadataClassC_TrustState_TRUSTED];

                                  op.nextState = op.intendedState;
                              }];
}

#pragma mark --- Utilities to run at times

- (NSString * _Nullable)extractStringKey:(NSString * _Nonnull)key fromDictionary:(NSDictionary * _Nonnull)d
{
    NSString *value = d[key];
    if ([value isKindOfClass:[NSString class]]) {
        return value;
    }
    return NULL;
}

- (void)handleHealthRequest
{
    NSString *trustState = OTAccountMetadataClassC_TrustStateAsString(self.currentMemoizedTrustState);
    OctagonState* currentState = [self.stateMachine waitForState:OctagonStateReady wait:3*NSEC_PER_SEC];

    [self.cuttlefishXPCWrapper reportHealthWithContainer:self.containerName context:self.contextID stateMachineState:currentState trustState:trustState reply:^(NSError * _Nullable error) {
        if (error) {
            secerror("octagon: health report is lost: %@", error);
        }
    }];
}

- (void)handleTTRRequest:(NSDictionary *)cfDictionary
{
    NSString *serialNumber = [self extractStringKey:@"s" fromDictionary:cfDictionary];
    NSString *ckDeviceId = [self extractStringKey:@"D" fromDictionary:cfDictionary];
    NSString *alert = [self extractStringKey:@"a" fromDictionary:cfDictionary];
    NSString *description = [self extractStringKey:@"d" fromDictionary:cfDictionary];
    NSString *radar = [self extractStringKey:@"R" fromDictionary:cfDictionary];
    NSString *componentName = [self extractStringKey:@"n" fromDictionary:cfDictionary];
    NSString *componentVersion = [self extractStringKey:@"v" fromDictionary:cfDictionary];
    NSString *componentID = [self extractStringKey:@"I" fromDictionary:cfDictionary];

    if (serialNumber) {
        if (![self.deviceAdapter.serialNumber isEqualToString:serialNumber]) {
            secnotice("octagon", "TTR request not for me (sn)");
            return;
        }
    }
    if (ckDeviceId) {
        NSString *selfDeviceID = self.viewManager.accountTracker.ckdeviceID;
        if (![selfDeviceID isEqualToString:serialNumber]) {
            secnotice("octagon", "TTR request not for me (deviceId)");
            return;
        }
    }

    if (alert == NULL || description == NULL || radar == NULL) {
        secerror("octagon: invalid type of TTR requeat: %@", cfDictionary);
        return;
    }

    SecTapToRadar *ttr = [[SecTapToRadar alloc] initTapToRadar:alert
                                                   description:description
                                                         radar:radar];
    if (componentName && componentVersion && componentID) {
        ttr.componentName = componentName;
        ttr.componentVersion = componentVersion;
        ttr.componentID = componentID;
    }
    [ttr trigger];
}

// We can't make a APSIncomingMessage in the tests (no public constructor),
// but we don't really care about anything in it but the userInfo dictionary anyway
- (void)notifyContainerChange:(APSIncomingMessage* _Nullable)notification
{
    [self notifyContainerChangeWithUserInfo:notification.userInfo];
}

- (void)notifyContainerChangeWithUserInfo:(NSDictionary*)userInfo
{
    secerror("OTCuttlefishContext: received a cuttlefish push notification (%@): %@",
             self.containerName, userInfo);

    NSDictionary *cfDictionary = userInfo[@"cf"];
    if ([cfDictionary isKindOfClass:[NSDictionary class]]) {
        NSString *command = [self extractStringKey:@"k" fromDictionary:cfDictionary];
        if(command) {
            if ([command isEqualToString:@"h"]) {
                [self handleHealthRequest];
            } else if ([command isEqualToString:@"r"]) {
                [self handleTTRRequest:cfDictionary];
            } else {
                secerror("octagon: unknown command: %@", command);
            }
            return;
        }
    }

    if (self.apsRateLimiter == nil) {
        secnotice("octagon", "creating aps rate limiter");
        // If we're testing, for the initial delay, use 0.2 second. Otherwise, 2s.
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 200 * NSEC_PER_MSEC : 2 * NSEC_PER_SEC);

        // If we're testing, for the initial delay, use 2 second. Otherwise, 30s.
        dispatch_time_t continuingDelay = (SecCKKSReduceRateLimiting() ? 2 * NSEC_PER_SEC : 30 * NSEC_PER_SEC);

        WEAKIFY(self);
        self.apsRateLimiter = [[CKKSNearFutureScheduler alloc] initWithName:@"aps-push-ratelimiter"
                                                               initialDelay:initialDelay
                                                            continuingDelay:continuingDelay
                                                           keepProcessAlive:YES
                                                  dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                      block:^{
                                                                          STRONGIFY(self);
                                                                          if (self == nil) {
                                                                              return;
                                                                          }
                                                                          secnotice("octagon-push-ratelimited", "notifying container of change for context: %@", self.contextID);
                                                                        OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagCuttlefishNotification
                                                                       conditions:OctagonPendingConditionsDeviceUnlocked];

                                                                          [self.stateMachine handlePendingFlag:pendingFlag];
                                                                      }];
    }
    
    [self.apsRateLimiter trigger];
}

- (BOOL)waitForReady:(int64_t)timeOffset
{
    OctagonState* currentState = [self.stateMachine waitForState:OctagonStateReady wait:timeOffset];
    return [currentState isEqualToString:OctagonStateReady];

}

- (OTAccountMetadataClassC_TrustState)currentMemoizedTrustState
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(!accountMetadata) {
        secnotice("octagon", "Unable to fetch account metadata: %@", localError);
        return OTAccountMetadataClassC_TrustState_UNKNOWN;
    }

    return accountMetadata.trustState;
}

- (OTAccountMetadataClassC_AccountState)currentMemoizedAccountState
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(!accountMetadata) {
        secnotice("octagon", "Unable to fetch account metadata: %@", localError);
        return OTAccountMetadataClassC_AccountState_UNKNOWN;
    }

    return accountMetadata.icloudAccountState;
}

- (NSDate* _Nullable) currentMemoizedLastHealthCheck
{
    NSError* localError = nil;
    OTAccountMetadataClassC* accountMetadata = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];

    if(!accountMetadata) {
        secnotice("octagon", "Unable to fetch account metadata: %@", localError);
        return nil;
    }
    if(accountMetadata.lastHealthCheckup == 0) {
        return nil;
    }
    return [[NSDate alloc] initWithTimeIntervalSince1970: accountMetadata.lastHealthCheckup];
}

- (void)requestTrustedDeviceListRefresh
{
    [self.stateMachine handleFlag:OctagonFlagFetchAuthKitMachineIDList];
}

#pragma mark --- Device Info update handling

- (void)deviceNameUpdated {
    secnotice("octagon-devicename", "device name updated: %@", self.contextID);
    OctagonPendingFlag *pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:OctagonFlagCuttlefishNotification
                                                                    conditions:OctagonPendingConditionsDeviceUnlocked];
    [self.stateMachine handlePendingFlag:pendingFlag];
}

#pragma mark --- SOS update handling


- (void)selfPeerChanged:(id<CKKSPeerProvider>)provider
{
    // Currently, we register for peer changes with just our SOS peer adapter, so the only reason this is called is to receive SOS updates
    // Ignore SOS self peer updates for now.
}

- (void)trustedPeerSetChanged:(id<CKKSPeerProvider>)provider
{
    // Currently, we register for peer changes with just our SOS peer adapter, so the only reason this is called is to receive SOS updates
    secnotice("octagon-sos", "Received an update of an SOS trust set change");

    if(!self.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "This platform doesn't support SOS. This is probably a bug?");
    }

    if (self.sosConsistencyRateLimiter == nil) {
        secnotice("octagon", "creating SOS consistency rate limiter");
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 200 * NSEC_PER_MSEC : 2 * NSEC_PER_SEC);
        dispatch_time_t maximumDelay = (SecCKKSReduceRateLimiting() ? 10 * NSEC_PER_SEC : 30 * NSEC_PER_SEC);

        WEAKIFY(self);

        void (^block)(void) = ^{
            STRONGIFY(self);
            [self.stateMachine handleFlag:OctagonFlagAttemptSOSConsistency];
        };

        self.sosConsistencyRateLimiter = [[CKKSNearFutureScheduler alloc] initWithName:@"sos-consistency-ratelimiter"
                                                                          initialDelay:initialDelay
                                                                      expontialBackoff:2
                                                                          maximumDelay:maximumDelay
                                                                      keepProcessAlive:false
                                                             dependencyDescriptionCode:CKKSResultDescriptionPendingZoneChangeFetchScheduling
                                                                                 block:block];
    }

    [self.sosConsistencyRateLimiter trigger];
}

#pragma mark --- External Interfaces

//Check for account
- (CKKSAccountStatus)checkForCKAccount:(OTOperationConfiguration * _Nullable)configuration {

#if TARGET_OS_WATCH
    // Watches can be very, very slow getting the CK account state
    uint64_t timeout = (90 * NSEC_PER_SEC);
#else
    uint64_t timeout = (10 * NSEC_PER_SEC);
#endif
    if (configuration.timeoutWaitForCKAccount != 0) {
        timeout = configuration.timeoutWaitForCKAccount;
    }
    if (timeout) {
        /* wait if account is not present yet */
        if([self.cloudKitAccountStateKnown wait:timeout] != 0) {
            secnotice("octagon-ck", "Unable to determine CloudKit account state?");
            return CKKSAccountStatusUnknown;
        }
    }

    __block bool haveAccount = true;
    dispatch_sync(self.queue, ^{
        if (self.cloudKitAccountInfo == NULL || self.cloudKitAccountInfo.accountStatus != CKKSAccountStatusAvailable) {
            haveAccount = false;
        }
    });
    return haveAccount ? CKKSAccountStatusAvailable : CKKSAccountStatusNoAccount;
}

- (NSError *)errorNoiCloudAccount
{
    return [NSError errorWithDomain:OctagonErrorDomain
                               code:OTErrorNotSignedIn
                        description:@"User is not signed into iCloud."];
}

//Initiator interfaces

- (void)rpcPrepareIdentityAsApplicantWithConfiguration:(OTJoiningConfiguration*)config
                                              epoch:(uint64_t)epoch
                                              reply:(void (^)(NSString * _Nullable peerID,
                                                              NSData * _Nullable permanentInfo,
                                                              NSData * _Nullable permanentInfoSig,
                                                              NSData * _Nullable stableInfo,
                                                              NSData * _Nullable stableInfoSig,
                                                              NSError * _Nullable error))reply
{
    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply(NULL, NULL, NULL, NULL, NULL, [self errorNoiCloudAccount]);
        return;
    }

    secnotice("otrpc", "Preparing identity as applicant");
    OTPrepareOperation* pendingOp = [[OTPrepareOperation alloc] initWithDependencies:self.operationDependencies
                                                                       intendedState:OctagonStateInitiatorAwaitingVoucher
                                                                          errorState:OctagonStateBecomeUntrusted
                                                                          deviceInfo:[self prepareInformation]
                                                                               epoch:epoch];


    dispatch_time_t timeOut = 0;
    if(config.timeout != 0) {
        timeOut = config.timeout;
    } else if(!OctagonPlatformSupportsSOS()){
        // Non-iphone non-mac platforms can be slow; heuristically slow them down
        timeOut = 60*NSEC_PER_SEC;
    } else {
        timeOut = 2*NSEC_PER_SEC;
    }

    OctagonStateTransitionRequest<OTPrepareOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"prepareForApplicant"
                                                                                                   sourceStates:[NSSet setWithArray:@[OctagonStateUntrusted, OctagonStateNoAccount, OctagonStateMachineNotStarted]]
                                                                                                    serialQueue:self.queue
                                                                                                        timeout:timeOut
                                                                                                   transitionOp:pendingOp];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"rpcPrepare-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a prepare call: %@  %@", pendingOp.peerID, pendingOp.error);
                                                         reply(pendingOp.peerID,
                                                               pendingOp.permanentInfo,
                                                               pendingOp.permanentInfoSig,
                                                               pendingOp.stableInfo,
                                                               pendingOp.stableInfoSig,
                                                               pendingOp.error);
                                                     }];
    [callback addDependency:pendingOp];
    [self.operationQueue addOperation: callback];

    [self.stateMachine handleExternalRequest:request];

    return;
}

-(void)joinWithBottle:(NSString*)bottleID
              entropy:(NSData *)entropy
           bottleSalt:(NSString *)bottleSalt
                reply:(void (^)(NSError * _Nullable error))reply
{
    _bottleID = bottleID;
    _entropy = entropy;
    _bottleSalt = bottleSalt;
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    if ([self checkForCKAccount:configuration] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply([self errorNoiCloudAccount]);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateInitiatorCreateIdentity: @{
            OctagonStateInitiatorVouchWithBottle: [self joinStatePathDictionary],
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join-with-bottle"
                                   sourceStates:OctagonInAccountStates()
                                           path:path
                                          reply:reply];
}

-(void)joinWithRecoveryKey:(NSString*)recoveryKey
                     reply:(void (^)(NSError * _Nullable error))reply
{
    _recoveryKey = recoveryKey;
    OTOperationConfiguration *configuration = [[OTOperationConfiguration alloc] init];

    if ([self checkForCKAccount:configuration] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply([self errorNoiCloudAccount]);
        return;
    }

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateCreateIdentityForRecoveryKey: @{
            OctagonStateVouchWithRecoveryKey: [self joinStatePathDictionary],
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join-with-recovery-key"
                                   sourceStates:OctagonInAccountStates()
                                           path:path
                                          reply:reply];
}

- (NSDictionary*)joinStatePathDictionary
{
    return @{
        OctagonStateInitiatorUpdateDeviceList: @{
            OctagonStateInitiatorJoin: @{
                OctagonStateBecomeReady: @{
                    OctagonStateReady: [OctagonStateTransitionPathStep success],
                },

                OctagonStateInitiatorJoinCKKSReset: @{
                    OctagonStateInitiatorJoinAfterCKKSReset: @{
                        OctagonStateBecomeReady: @{
                            OctagonStateReady: [OctagonStateTransitionPathStep success]
                        },
                    },
                },
            },
        },
    };
}

- (void)rpcJoin:(NSData*)vouchData
       vouchSig:(NSData*)vouchSig
preapprovedKeys:(NSArray<NSData*>* _Nullable)preapprovedKeys
          reply:(void (^)(NSError * _Nullable error))reply
{

    _vouchData = vouchData;
    _vouchSig = vouchSig;
    _preapprovedKeys = preapprovedKeys;

    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply([self errorNoiCloudAccount]);
        return;
    }

    NSMutableSet* sourceStates = [NSMutableSet setWithObject:OctagonStateInitiatorAwaitingVoucher];

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:[self joinStatePathDictionary]];

    [self.stateMachine doWatchedStateMachineRPC:@"rpc-join"
                                   sourceStates:sourceStates
                                           path:path
                                          reply:reply];
}

- (NSDictionary *)ckksPeerStatus:(id<CKKSPeer>)peer
{
    NSMutableDictionary *peerStatus = [NSMutableDictionary dictionary];

    if (peer.peerID) {
        peerStatus[@"peerID"] = peer.peerID;
    }
    NSData *spki = peer.publicSigningKey.encodeSubjectPublicKeyInfo;
    if (spki) {
        peerStatus[@"signingSPKI"] = [spki base64EncodedStringWithOptions:0];
        peerStatus[@"signingSPKIHash"] = [TPHashBuilder hashWithAlgo:kTPHashAlgoSHA256 ofData:spki];
    }
    return peerStatus;
}

- (NSArray *)sosTrustedPeersStatus
{
    NSError *localError = nil;
    NSSet<id<CKKSRemotePeerProtocol>>* _Nullable peers = [self.sosAdapter fetchTrustedPeers:&localError];
    if (peers == nil || localError) {
        secnotice("octagon", "No SOS peers present: %@, skipping in status", localError);
        return nil;
    }
    NSMutableArray<NSDictionary *>* trustedSOSPeers = [NSMutableArray array];

    for (id<CKKSPeer> peer in peers) {
        NSDictionary *peerStatus = [self ckksPeerStatus:peer];
        if (peerStatus) {
            [trustedSOSPeers addObject:peerStatus];
        }
    }
    return trustedSOSPeers;
}

- (NSDictionary *)sosSelvesStatus
{
    NSError *localError = nil;

    CKKSSelves* selves = [self.sosAdapter fetchSelfPeers:&localError];
    if (selves == nil || localError) {
        secnotice("octagon", "No SOS selves present: %@, skipping in status", localError);
        return nil;
    }
    NSMutableDictionary* selvesSOSPeers = [NSMutableDictionary dictionary];

    selvesSOSPeers[@"currentSelf"] = [self ckksPeerStatus:selves.currentSelf];

    /*
     * If we have past selves, include them too
     */
    NSMutableSet* pastSelves = [selves.allSelves mutableCopy];
    [pastSelves removeObject:selves.currentSelf];
    if (pastSelves.count) {
        NSMutableArray<NSDictionary *>* pastSelvesStatus = [NSMutableArray array];

        for (id<CKKSPeer> peer in pastSelves) {
            NSDictionary *peerStatus = [self ckksPeerStatus:peer];
            if (peerStatus) {
                [pastSelvesStatus addObject:peerStatus];
            }
        }
        selvesSOSPeers[@"pastSelves"] = pastSelvesStatus;
    }
    return selvesSOSPeers;
}

- (void)rpcStatus:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    __block NSMutableDictionary* result = [NSMutableDictionary dictionary];

    result[@"containerName"] = self.containerName;
    result[@"contextID"] = self.contextID;

    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply(NULL, [self errorNoiCloudAccount]);
        return;
    }

    if([self.stateMachine.paused wait:3*NSEC_PER_SEC] != 0) {
        secnotice("octagon", "Returning status of unpaused state machine for container (%@) and context (%@)", self.containerName, self.contextID);
        result[@"stateUnpaused"] = @1;
    }

    // This will try to allow the state machine to pause
    result[@"state"] = self.stateMachine.currentState;
    result[@"statePendingFlags"] = [self.stateMachine dumpPendingFlags];
    result[@"stateFlags"] = [self.stateMachine.flags dumpFlags];

    result[@"memoizedTrustState"] = @(self.currentMemoizedTrustState);
    result[@"memoizedAccountState"] = @(self.currentMemoizedAccountState);
    result[@"octagonLaunchSeqence"] = [self.launchSequence eventsByTime];
    result[@"memoizedlastHealthCheck"] = self.currentMemoizedLastHealthCheck ? self.currentMemoizedLastHealthCheck : @"Never checked";
    if (self.sosAdapter.sosEnabled) {
        result[@"sosTrustedPeersStatus"] = [self sosTrustedPeersStatus];
        result[@"sosSelvesStatus"] = [self sosSelvesStatus];
    }

    {
        NSError *error;
        id<SecEscrowRequestable> request = [self.escrowRequestClass request:&error];
        result[@"escrowRequest"] = [request fetchStatuses:&error];
    }

    result[@"CoreFollowUp"] = [self.followupHandler sysdiagnoseStatus];
    result[@"lastOctagonPush"] = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastOctagonPush];


    [self.cuttlefishXPCWrapper dumpWithContainer:self.containerName
                                         context:self.contextID
                                           reply:^(NSDictionary * _Nullable dump, NSError * _Nullable dumpError) {
            secnotice("octagon", "Finished dump for status RPC");
            if(dumpError) {
                result[@"contextDumpError"] = dumpError;
            } else {
                result[@"contextDump"] = dump;
            }
            reply(result, nil);
        }];
}

- (void)rpcFetchEgoPeerID:(void (^)(NSString* peerID, NSError* error))reply
{
    // We've memoized this peer ID. Use the memorized version...
    NSError* localError = nil;
    NSString* peerID = [self.accountMetadataStore getEgoPeerID:&localError];

    if(peerID) {
        secnotice("octagon", "Returning peer ID: %@", peerID);
    } else {
        secnotice("octagon", "Unable to fetch peer ID: %@", localError);
    }
    reply(peerID, localError);
}

- (void)rpcFetchDeviceNamesByPeerID:(void (^)(NSDictionary<NSString*, NSString*>* _Nullable peers, NSError* _Nullable error))reply
{
    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply(NULL, [self errorNoiCloudAccount]);
        return;
    }

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper dumpWithContainer:self.containerName
                                         context:self.contextID
                                           reply:^(NSDictionary * _Nullable dump, NSError * _Nullable dumpError) {
            // Pull out our peers
            if(dumpError) {
                secnotice("octagon", "Unable to dump info: %@", dumpError);
                reply(nil, dumpError);
                return;
            }

            NSDictionary* selfInfo = dump[@"self"];
            NSArray* peers = dump[@"peers"];
            NSArray* trustedPeerIDs = selfInfo[@"dynamicInfo"][@"included"];

            NSMutableDictionary<NSString*, NSString*>* peerMap = [NSMutableDictionary dictionary];

            for(NSString* peerID in trustedPeerIDs) {
                NSDictionary* peerMatchingID = nil;

                for(NSDictionary* peer in peers) {
                    if([peer[@"peerID"] isEqualToString:peerID]) {
                        peerMatchingID = peer;
                        break;
                    }
                }

                if(!peerMatchingID) {
                    secerror("octagon: have a trusted peer ID without peer information: %@", peerID);
                    continue;
                }

                peerMap[peerID] = peerMatchingID[@"stableInfo"][@"device_name"];
            }

            reply(peerMap, nil);
        }];
}

- (void)rpcSetRecoveryKey:(NSString*)recoveryKey reply:(void (^)(NSError * _Nullable error))reply
{
    OTSetRecoveryKeyOperation *pendingOp = [[OTSetRecoveryKeyOperation alloc] initWithDependencies:self.operationDependencies
                                                                                       recoveryKey:recoveryKey];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"setRecoveryKey-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning a set recovery key call: %@", pendingOp.error);
                                                         reply(pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation:callback];
    [self.operationQueue addOperation:pendingOp];
}

- (void)rpcTrustStatusCachedStatus:(OTAccountMetadataClassC*)account
                             reply:(void (^)(CliqueStatus status,
                                             NSString* egoPeerID,
                                             NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                             BOOL isExcluded,
                                             NSError *error))reply
{
    CliqueStatus status = CliqueStatusAbsent;

    if (account.trustState == OTAccountMetadataClassC_TrustState_TRUSTED) {
        status = CliqueStatusIn;
    } else if (account.trustState == OTAccountMetadataClassC_TrustState_UNTRUSTED) {
        status = CliqueStatusNotIn;
    }

    secnotice("octagon", "returning cached clique status: %@", OTCliqueStatusToString(status));
    reply(status, account.peerID, nil, NO, NULL);
}


- (void)rpcTrustStatus:(OTOperationConfiguration *)configuration
                 reply:(void (^)(CliqueStatus status,
                                 NSString* _Nullable peerID,
                                 NSDictionary<NSString*, NSNumber*>* _Nullable peerCountByModelID,
                                 BOOL isExcluded,
                                 NSError *error))reply
{
    __block NSError* localError = nil;
    
    OTAccountMetadataClassC* account = [self.accountMetadataStore loadOrCreateAccountMetadata:&localError];
    if(localError && [self.lockStateTracker isLockedError:localError]){
        secnotice("octagon", "Device is locked! pending initialization on unlock");
        reply(CliqueStatusError, nil, nil, NO, localError);
        return;
    }

    if(account.icloudAccountState == OTAccountMetadataClassC_AccountState_NO_ACCOUNT) {
        secnotice("octagon", "no account! returning clique status 'no account'");
        reply(CliqueStatusNoCloudKitAccount, nil, nil, NO, NULL);
        return;
    }

    if (configuration.useCachedAccountStatus) {
        [self rpcTrustStatusCachedStatus:account reply:reply];
        return;
    }

    CKKSAccountStatus ckAccountStatus = [self checkForCKAccount:configuration];
    if(ckAccountStatus == CKKSAccountStatusNoAccount) {
        secnotice("octagon", "No cloudkit account present");
        reply(CliqueStatusNoCloudKitAccount, nil, nil, NO, NULL);
        return;
    } else if(ckAccountStatus == CKKSAccountStatusUnknown) {
        secnotice("octagon", "Unknown cloudkit account status, returning cached trust value");
        [self rpcTrustStatusCachedStatus:account reply:reply];
        return;
    }

    __block NSString* peerID = nil;
    __block NSDictionary<NSString*, NSNumber*>* peerModelCounts = nil;
    __block BOOL excluded = NO;
    __block CliqueStatus trustStatus = CliqueStatusError;

    [self.cuttlefishXPCWrapper trustStatusWithContainer:self.containerName
                                                context:self.contextID
                                                  reply:^(TrustedPeersHelperEgoPeerStatus *egoStatus,
                                                          NSError *xpcError) {
        TPPeerStatus status = egoStatus.egoStatus;
        peerID = egoStatus.egoPeerID;
        excluded = egoStatus.isExcluded;
        peerModelCounts = egoStatus.viablePeerCountsByModelID;
        localError = xpcError;

        if(xpcError) {
            secnotice("octagon", "error fetching trust status: %@", xpcError);
        } else {
            secnotice("octagon", "trust status: %@", TPPeerStatusToString(status));
            
            if((status&TPPeerStatusExcluded) == TPPeerStatusExcluded){
                trustStatus = CliqueStatusNotIn;
            }
            else if((status&TPPeerStatusPartiallyReciprocated) == TPPeerStatusPartiallyReciprocated){
                trustStatus = CliqueStatusIn;
            }
            else if((status&TPPeerStatusAncientEpoch) == TPPeerStatusAncientEpoch){
                //FIX ME HANDLE THIS CASE
                trustStatus=  CliqueStatusIn;
            }
            else if((status&TPPeerStatusOutdatedEpoch) == TPPeerStatusOutdatedEpoch){
                //FIX ME HANDLE THIS CASE
                trustStatus = CliqueStatusIn;
            }
            else if((status&TPPeerStatusFullyReciprocated) == TPPeerStatusFullyReciprocated){
                trustStatus = CliqueStatusIn;
            }
            else if((status&TPPeerStatusUnknown) == TPPeerStatusUnknown){
                trustStatus = CliqueStatusAbsent;
            }
            else if ((status&TPPeerStatusSelfTrust) == TPPeerStatusSelfTrust) {
                trustStatus = CliqueStatusIn;
            }
            else {
                secnotice("octagon", "TPPeerStatus is empty");
                trustStatus = CliqueStatusAbsent;
            }
        }
    }];

    if(trustStatus == CliqueStatusIn && self.postedRepairCFU == YES){
        NSError* clearError = nil;
        [self.followupHandler clearFollowUp:OTFollowupContextTypeStateRepair error:&clearError];
        // TODO(caw): should we clear this flag if `clearFollowUpForContext` fails?
        self.postedRepairCFU = NO;
    }
    reply(trustStatus, peerID, peerModelCounts, excluded, localError);
}

- (void)rpcFetchAllViableBottles:(void (^)(NSArray<NSString*>* _Nullable sortedBottleIDs, NSArray<NSString*>* _Nullable sortedPartialEscrowRecordIDs, NSError* _Nullable error))reply
{
    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply(NULL, NULL, [self errorNoiCloudAccount]);
        return;
    }

    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper fetchViableBottlesWithContainer:self.containerName
                                                       context:self.contextID
                                                         reply:^(NSArray<NSString*>* _Nullable sortedEscrowRecordIDs, NSArray<NSString*>* _Nullable sortedPartialEscrowRecordIDs, NSError * _Nullable error) {
            if(error){
                secerror("octagon: error fetching all viable bottles: %@", error);
                reply(nil, nil, error);
            }else{
                secnotice("octagon", "fetched viable bottles: %@", sortedEscrowRecordIDs);
                secnotice("octagon", "fetched partially viable bottles: %@", sortedPartialEscrowRecordIDs);
                reply(sortedEscrowRecordIDs, sortedPartialEscrowRecordIDs, error);
            }
        }];
}

- (void)fetchEscrowContents:(void (^)(NSData* _Nullable entropy,
                                      NSString* _Nullable bottleID,
                                      NSData* _Nullable signingPublicKey,
                                      NSError* _Nullable error))reply
{
    // As this isn't a state-modifying operation, we don't need to go through the state machine.
    [self.cuttlefishXPCWrapper fetchEscrowContentsWithContainer:self.containerName
                                                        context:self.contextID
                                                          reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
            if(error){
                secerror("octagon: error fetching escrow contents: %@", error);
                reply(nil, nil, nil, error);
            }else{
                secnotice("octagon", "fetched escrow contents for bottle: %@", bottleID);
                reply(entropy, bottleID, signingPublicKey, error);
            }
        }];
}

- (void)rpcValidatePeers:(void (^)(NSDictionary* _Nullable result, NSError* _Nullable error))reply
{
    __block NSMutableDictionary* result = [NSMutableDictionary dictionary];

    result[@"containerName"] = self.containerName;
    result[@"contextID"] = self.contextID;
    result[@"state"] = [self.stateMachine waitForState:OctagonStateReady wait:3*NSEC_PER_SEC];

    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon", "No cloudkit account present");
        reply(NULL, [self errorNoiCloudAccount]);
        return;
    }

    [self.cuttlefishXPCWrapper validatePeersWithContainer:self.containerName
                                                  context:self.contextID
                                                    reply:^(NSDictionary * _Nullable validateData, NSError * _Nullable dumpError) {
            secnotice("octagon", "Finished validatePeers for status RPC");
            if(dumpError) {
                result[@"error"] = dumpError;
            } else {
                result[@"validate"] = validateData;
            }
            reply(result, nil);
        }];
}


#pragma mark --- Testing
- (void) setAccountStateHolder:(OTCuttlefishAccountStateHolder*)accountMetadataStore
{
    self.accountMetadataStore = accountMetadataStore;
}

- (void)setPostedBool:(BOOL)posted
{
    self.postedRepairCFU = posted;
}

#pragma mark --- Health Checker

- (BOOL)postRepairCFU:(NSError**)error
{
    NSError* localError = nil;
    BOOL postSuccess = NO;
    if (self.postedRepairCFU == NO) {
        [self.followupHandler postFollowUp:OTFollowupContextTypeStateRepair error:&localError];
        if(localError){
            secerror("octagon-health: CoreCDP repair failed: %@", localError);
            if(error){
                *error = localError;
            }
        }
        else{
            secnotice("octagon-health", "CoreCDP post repair success");
            self.postedRepairCFU = YES;
            postSuccess = YES;
        }
    } else {
        secnotice("octagon-health", "already posted a repair CFU!");
    }
    return postSuccess;
}

- (BOOL)shouldPostConfirmPasscodeCFU:(NSError**)error
{
    NSError* localError = nil;
    id<SecEscrowRequestable> request = [self.escrowRequestClass request:&localError];
    if(!request || localError) {
        secnotice("octagon-health", "Unable to acquire a EscrowRequest object: %@", localError);
        if(error){
            *error = localError;
        }
        return YES;
    }
    BOOL pendingUpload = [request pendingEscrowUpload:&localError];

    if(localError) {
        secnotice("octagon-health", "Failed to check escrow prerecord status: %@", localError);
        if(error) {
            *error = localError;
        }
        return YES;
    }

    if(pendingUpload == YES) {
        secnotice("octagon-health", "prerecord is pending, NOT posting CFU");
        return NO;
    } else {
        secnotice("octagon-health", "no pending prerecords, posting CFU");
        return YES;
    }
}

- (void)postConfirmPasscodeCFU:(NSError**)error
{
    NSError* localError = nil;
    if (self.postedEscrowRepairCFU == NO) {
        [self.followupHandler postFollowUp:OTFollowupContextTypeOfflinePasscodeChange error:&localError];
        if(localError){
            secerror("octagon-health: CoreCDP offline passcode change failed: %@", localError);
            *error = localError;
        }
        else{
            secnotice("octagon-health", "CoreCDP offline passcode change success");
            self.postedEscrowRepairCFU = YES;
        }
    } else {
        secnotice("octagon-health", "already posted escrow CFU");
    }
}

- (void)postRecoveryKeyCFU:(NSError**)error
{
    NSError* localError = nil;
    if (self.postedRecoveryKeyCFU == NO) {
        [self.followupHandler postFollowUp:OTFollowupContextTypeRecoveryKeyRepair error:&localError];
        if(localError){
            secerror("octagon-health: CoreCDP recovery key cfu failed: %@", localError);
        }
        else{
            secnotice("octagon-health", "CoreCDP recovery key cfu success");
            self.postedRecoveryKeyCFU = YES;
        }
    } else {
        secnotice("octagon-health", "already posted recovery key CFU");
    }
}

- (void)checkOctagonHealth:(BOOL)skipRateLimitingCheck reply:(void (^)(NSError * _Nullable error))reply
{
    secnotice("octagon-health", "Beginning checking overall Octagon Trust");

    _skipRateLimitingCheck = skipRateLimitingCheck;

    // Ending in "waitforunlock" is okay for a health check
    [self.stateMachine doWatchedStateMachineRPC:@"octagon-trust-health-check"
                                   sourceStates:OctagonHealthSourceStates()
                                           path:[OctagonStateTransitionPath pathFromDictionary:@{
                                               OctagonStateHSA2HealthCheck: @{
                                                   OctagonStateSecurityTrustCheck: @{
                                                       OctagonStateTPHTrustCheck: @{
                                                           OctagonStateCuttlefishTrustCheck: @{
                                                               OctagonStateBecomeReady: @{
                                                                   OctagonStateReady: [OctagonStateTransitionPathStep success],
                                                                   OctagonStateWaitForUnlock: [OctagonStateTransitionPathStep success],
                                                               },
                                                               // Cuttlefish can suggest we reset the world. Consider reaching here a success,
                                                               // instead of tracking the whole reset.
                                                               OctagonStateHealthCheckReset: [OctagonStateTransitionPathStep success],
                                                           },
                                                       },
                                                   },
                                                   OctagonStateWaitForHSA2: [OctagonStateTransitionPathStep success],
                                               }
                                           }]
                                          reply:reply];
}

- (void)attemptSOSUpgrade:(void (^)(NSError* _Nullable error))reply
{
    secnotice("octagon-sos", "attempting to perform an sos upgrade");

    if ([self checkForCKAccount:nil] != CKKSAccountStatusAvailable) {
        secnotice("octagon-sos", "No cloudkit account present");
        reply([self errorNoiCloudAccount]);
        return;
    }

    NSSet* sourceStates = [NSSet setWithArray: @[OctagonStateUntrusted]];

    OTSOSUpgradeOperation *pendingOp = [[OTSOSUpgradeOperation alloc] initWithDependencies:self.operationDependencies
                                                                             intendedState:OctagonStateBecomeReady
                                                                         ckksConflictState:OctagonStateBecomeUntrusted
                                                                                errorState:OctagonStateBecomeUntrusted
                                                                                deviceInfo:self.prepareInformation];

    OctagonStateTransitionRequest<OTSOSUpgradeOperation*>* request = [[OctagonStateTransitionRequest alloc] init:@"attempt-sos-upgrade"
                                                                                                    sourceStates:sourceStates
                                                                                                     serialQueue:self.queue
                                                                                                         timeout:OctagonStateTransitionDefaultTimeout
                                                                                                    transitionOp:pendingOp];

    CKKSResultOperation* callback = [CKKSResultOperation named:@"sos-upgrade-callback"
                                                     withBlock:^{
                                                         secnotice("otrpc", "Returning from an sos upgrade attempt: %@", pendingOp.error);
                                                         [[CKKSAnalytics logger] logResultForEvent:OctagonEventUpgradePreapprovedJoinAfterPairing hardFailure:false result:pendingOp.error];
                                                         reply(pendingOp.error);
                                                     }];

    [callback addDependency:pendingOp];
    [self.operationQueue addOperation: callback];
    
    [self.stateMachine handleExternalRequest:request];
}

- (void)waitForOctagonUpgrade:(void (^)(NSError* error))reply
{
    secnotice("octagon-sos", "waitForOctagonUpgrade");

    if (!self.sosAdapter.sosEnabled) {
        secnotice("octagon-sos", "sos not enabled, nothing to do for waitForOctagonUpgrade");
        reply(nil);
        return;
    }

    if ([self.stateMachine isPaused]) {
        if ([[self.stateMachine currentState] isEqualToString:OctagonStateReady]) {
            secnotice("octagon-sos", "waitForOctagonUpgrade: already ready, returning");
            reply(nil);
            return;
        }
    } else {
        if ([[self.stateMachine waitForState:OctagonStateReady wait:10*NSEC_PER_SEC] isEqualToString:OctagonStateReady]) {
            secnotice("octagon-sos", "waitForOctagonUpgrade: in ready (after waiting), returning");
            reply(nil);
            return;
        } else {
            secnotice("octagon-sos", "waitForOctagonUpgrade: fail to get to ready after timeout, attempting upgrade");
        }
    }

    NSSet* sourceStates = [NSSet setWithArray: @[OctagonStateUntrusted]];

    OctagonStateTransitionPath* path = [OctagonStateTransitionPath pathFromDictionary:@{
        OctagonStateAttemptSOSUpgrade: @{
            OctagonStateBecomeReady: @{
                OctagonStateReady: [OctagonStateTransitionPathStep success],
            },
        },
    }];

    [self.stateMachine doWatchedStateMachineRPC:@"sos-upgrade-to-ready"
                                   sourceStates:sourceStates
                                           path:path
                                          reply:reply];
}

- (void)clearPendingCFUFlags
{
    self.postedRecoveryKeyCFU = NO;
    self.postedEscrowRepairCFU = NO;
    self.postedRepairCFU = NO;
}

// Metrics passthroughs

- (BOOL)machineIDOnMemoizedList:(NSString*)machineID error:(NSError**)error
{
    __block BOOL onList = NO;
    __block NSError* reterror = nil;
    [self.cuttlefishXPCWrapper fetchAllowedMachineIDsWithContainer:self.containerName
                                                            context:self.contextID
                                                             reply:^(NSSet<NSString *> * _Nonnull machineIDs, NSError * _Nullable miderror) {
        if(miderror) {
            secnotice("octagon-metrics", "Failed to fetch allowed machineIDs: %@", miderror);
            reterror = miderror;
        } else {
            if([machineIDs containsObject:machineID]) {
                onList = YES;
            }
            secnotice("octagon-metrics", "MID (%@) on list: %@", machineID, onList ? @"yes" : @"no");
        }
    }];

    if(reterror && error) {
        *error = reterror;
    }
    return onList;
}

- (NSNumber* _Nullable)numberOfPeersInModelWithMachineID:(NSString*)machineID error:(NSError**)error
{
    __block NSNumber* ret = nil;
    __block NSError* retError = nil;
    [self.cuttlefishXPCWrapper trustStatusWithContainer:self.containerName
                                                context:self.contextID
                                                  reply:^(TrustedPeersHelperEgoPeerStatus *egoStatus,
                                                          NSError *xpcError) {
        if(xpcError) {
            secnotice("octagon-metrics", "Unable to fetch trust status: %@", xpcError);
            retError = xpcError;
        } else {
            ret = egoStatus.peerCountsByMachineID[machineID] ?: @(0);
            secnotice("octagon-metrics", "Number of peers with machineID (%@): %@", machineID, ret);
        }
    }];

    if(retError && error) {
        *error = retError;
    }

    return ret;
}

@end
#endif
