/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/ot/OTStates.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

OctagonState* const OctagonStateNoAccount = (OctagonState*)@"NoAccount";

OctagonState* const OctagonStateWaitForHSA2 = (OctagonState*)@"WaitForHSA2";
OctagonState* const OctagonStateWaitForCDP = (OctagonState*)@"WaitForCDP";

OctagonState* const OctagonStateUntrusted = (OctagonState*)@"Untrusted";
OctagonState* const OctagonStateBecomeUntrusted = (OctagonState*)@"BecomeUntrusted";

OctagonState* const OctagonStateReady = (OctagonState*)@"Ready";
OctagonState* const OctagonStateBecomeReady = (OctagonState*)@"BecomeReady";
OctagonState* const OctagonStateInherited = (OctagonState*)@"Inherited";
OctagonState* const OctagonStateBecomeInherited = (OctagonState*)@"BecomeInherited";

OctagonState* const OctagonStateEnsureConsistency = (OctagonState*)@"EnsureConsistency";
OctagonState* const OctagonStateEnsureOctagonKeysAreConsistent = (OctagonState*)@"EnsureOctagonKeysAreConsistent";
OctagonState* const OctagonStateEnsureUpdatePreapprovals = (OctagonState*)@"EnsureUpdatePreapprovals";

OctagonState* const OctagonStateInitializing = (OctagonState*)@"Initializing";
OctagonState* const OctagonStateWaitingForCloudKitAccount = (OctagonState*)@"WaitingForCloudKitAccount";
OctagonState* const OctagonStateCloudKitNewlyAvailable = (OctagonState*)@"CloudKitNewlyAvailable";
OctagonState* const OctagonStateRefetchCKKSPolicy = (OctagonState*)@"RefetchCKKSPolicy";
OctagonState* const OctagonStateDetermineCDPState = (OctagonState*)@"DetermineCDPState";
OctagonState* const OctagonStateCheckForAccountFixups = (OctagonState*)@"CheckForAccountFixups";
OctagonState* const OctagonStateCheckTrustState = (OctagonState*)@"CheckTrustState";

OctagonState* const OctagonStatePerformAccountFixups = (OctagonState*)@"PerformAccountFixups";

OctagonState* const OctagonStateEnableUserControllableViews = (OctagonState*)@"EnableUserControllableViews";
OctagonState* const OctagonStateDisableUserControllableViews = (OctagonState*)@"DisableUserControllableViews";
OctagonState* const OctagonStateSetUserControllableViewsToPeerConsensus = (OctagonState*)@"SetUserControllableViewsToPeerConsensus";

OctagonState* const OctagonStateUpdateSOSPreapprovals = (OctagonState*)@"UpdateSOSPreapprovals";

/*Piggybacking and ProximitySetup as Initiator Octagon only*/
OctagonState* const OctagonStateInitiatorSetCDPBit = (OctagonState*)@"InitiatorSetCDPBit";
OctagonState* const OctagonStateInitiatorUpdateDeviceList = (OctagonState*)@"InitiatorUpdateDeviceList";
OctagonState* const OctagonStateInitiatorAwaitingVoucher = (OctagonState*)@"InitiatorAwaitingVoucher";
OctagonState* const OctagonStateInitiatorJoin = (OctagonState*)@"InitiatorJoin";
OctagonState* const OctagonStateInitiatorJoinCKKSReset = (OctagonState*)@"InitiatorJoinCKKSReset";
OctagonState* const OctagonStateInitiatorJoinAfterCKKSReset = (OctagonState*)@"InitiatorJoinAfterCKKSReset";

/* used in restore (join with bottle)*/
OctagonState* const OctagonStateBottleJoinCreateIdentity = (OctagonState*)@"BottleJoinCreateIdentity";
OctagonState* const OctagonStateBottleJoinVouchWithBottle = (OctagonState*)@"BottleJoinVouchWithBottle";
OctagonState* const OctagonStateCreateIdentityForRecoveryKey = (OctagonState*)@"CreateIdentityForRecoveryKey";
OctagonState* const OctagonStateCreateIdentityForCustodianRecoveryKey = (OctagonState*)@"CreateIdentityForCustodianRecoveryKey";

OctagonState* const OctagonStateBottlePreloadOctagonKeysInSOS = (OctagonState*)@"BottlePreloadOctagonKeysInSOS";

/* used in restore (join with recovery key)*/
OctagonState* const OctagonStateVouchWithRecoveryKey = (OctagonState*)@"VouchWithRecoveryKey";
OctagonState* const OctagonStateVouchWithCustodianRecoveryKey = (OctagonState*)@"VouchWithCustodianRecoveryKey";
OctagonState* const OctagonStateJoinSOSAfterCKKSFetch = (OctagonState*)@"JoinSOSAfterCKKSFetch";
OctagonState* const OctagonStatePrepareAndRecoverTLKSharesForInheritancePeer = (OctagonState*)@"PrepareAndRecoverTLKSharesForInheritancePeer";

OctagonState* const OctagonStateStartCompanionPairing = (OctagonState*)@"StartCompanionPairing";

OctagonState* const OctagonStateWaitForCDPUpdated = (OctagonState*)@"WaitForCDPUpdated";

// Untrusted cuttlefish notification.
OctagonState* const OctagonStateUntrustedUpdated = (OctagonState*)@"UntrustedUpdated";

// Cuttlefish notifiation while ready.
OctagonState* const OctagonStateReadyUpdated = (OctagonState*)@"ReadyUpdated";

OctagonState* const OctagonStateError = (OctagonState*)@"Error";
OctagonState* const OctagonStateDisabled = (OctagonState*)@"Disabled";

OctagonState* const OctagonStateDetermineiCloudAccountState = (OctagonState*)@"DetermineiCloudAccountState";

OctagonState* const OctagonStateAttemptSOSUpgradeDetermineCDPState = (OctagonState*)@"AttemptSOSUpgradeDetermineCDPState";
OctagonState* const OctagonStateAttemptSOSUpgrade = (OctagonState*)@"AttemptSOSUpgrade";
OctagonState* const OctagonStateSOSUpgradeCKKSReset = (OctagonState*)@"SOSUpgradeCKKSReset";
OctagonState* const OctagonStateSOSUpgradeAfterCKKSReset = (OctagonState*)@"SOSUpgradeAfterCKKSReset";
OctagonState* const OctagonStateUnimplemented = (OctagonState*)@"Unimplemented";

/* Reset and establish */
OctagonState* const OctagonStateResetBecomeUntrusted = (OctagonState*)@"ResetBecomeUntrusted";
OctagonState* const OctagonStateResetAndEstablish = (OctagonState*)@"ResetAndEstablish";
OctagonState* const OctagonStateResetAnyMissingTLKCKKSViews = (OctagonState*)@"ResetAnyMissingTLKCKKSViews";
OctagonState* const OctagonStateEstablishEnableCDPBit = (OctagonState*)@"EstablishEnableCDPBit";
OctagonState* const OctagonStateReEnactDeviceList = (OctagonState*)@"ReEnactDeviceList";
OctagonState* const OctagonStateReEnactPrepare = (OctagonState*)@"ReEnactPrepare";
OctagonState* const OctagonStateReEnactReadyToEstablish = (OctagonState*)@"ReEnactReadyToEstablish";
OctagonState* const OctagonStateEstablishCKKSReset = (OctagonState*)@"EstablishCKKSReset";
OctagonState* const OctagonStateEstablishAfterCKKSReset = (OctagonState*)@"EstablishAfterCKKSReset";
OctagonState* const OctagonStateResetAndEstablishClearLocalContextState = (OctagonState*)@"ResetAndEstablishClearLocalContextState";

/* local reset */
OctagonState* const OctagonStateLocalReset = (OctagonState*)@"LocalReset";
OctagonState* const OctagonStateLocalResetClearLocalContextState = (OctagonState*)@"LocalResetClearLocalContextState";

/* used for trust health checks */
OctagonState* const OctagonStateHSA2HealthCheck = (OctagonState*)@"HSA2HealthCheck";
OctagonState* const OctagonStateCDPHealthCheck = (OctagonState*)@"CDPHealthCheck";
OctagonState* const OctagonStateTPHTrustCheck = (OctagonState*)@"TPHTrustCheck";
OctagonState* const OctagonStateCuttlefishTrustCheck = (OctagonState*)@"CuttlefishTrustCheck";
OctagonState* const OctagonStatePostRepairCFU = (OctagonState*)@"PostRepairCFU";
OctagonState* const OctagonStateSecurityTrustCheck = (OctagonState*)@"SecurityTrustCheck";
OctagonState* const OctagonStateHealthCheckReset = (OctagonState*)@"HealthCheckReset";

/* signout */
OctagonState* const OctagonStateNoAccountDoReset = (OctagonState*)@"NoAccountDoReset";

OctagonState* const OctagonStateLostAccountAuth = (OctagonState*)@"LostAccountAuth";
OctagonState* const OctagonStatePeerMissingFromServer = (OctagonState*)@"PeerMissingFromServer";

OctagonState* const OctagonStateWaitForUnlock = (OctagonState*)@"WaitForUnlock";
OctagonState* const OctagonStateWaitForClassCUnlock = (OctagonState*)@"WaitForClassCUnlock";

OctagonState* const OctagonStateAssistCKKSTLKUpload = (OctagonState*)@"AssistCKKSTLKUpload";
OctagonState* const OctagonStateAssistCKKSTLKUploadCKKSReset = (OctagonState*)@"AssistCKKSTLKUploadCKKSReset";
OctagonState* const OctagonStateAssistCKKSTLKUploadAfterCKKSReset = (OctagonState*)@"AssistCKKSTLKUploadAfterCKKSReset";

OctagonState* const OctagonStateHealthCheckLeaveClique = (OctagonState*)@"HealthCheckLeaveClique";


/* escrow */
OctagonState* const OctagonStateEscrowTriggerUpdate = (OctagonState*)@"EscrowTriggerUpdate";

// Flags
OctagonFlag* const OctagonFlagIDMSLevelChanged = (OctagonFlag*) @"idms_level";
OctagonFlag* const OctagonFlagEgoPeerPreapproved = (OctagonFlag*) @"preapproved";
OctagonFlag* const OctagonFlagCKKSRequestsTLKUpload = (OctagonFlag*) @"tlk_upload_needed";
OctagonFlag* const OctagonFlagCKKSRequestsPolicyCheck = (OctagonFlag*) @"policy_check_needed";
OctagonFlag* const OctagonFlagCKKSViewSetChanged = (OctagonFlag*) @"ckks_views_changed";
OctagonFlag* const OctagonFlagCuttlefishNotification = (OctagonFlag*) @"recd_push";
OctagonFlag* const OctagonFlagAccountIsAvailable = (OctagonFlag*)@"account_available";
OctagonFlag* const OctagonFlagCDPEnabled = (OctagonFlag*) @"cdp_enabled";
OctagonFlag* const OctagonFlagAttemptSOSUpgrade = (OctagonFlag*)@"attempt_sos_upgrade";
OctagonFlag* const OctagonFlagFetchAuthKitMachineIDList = (OctagonFlag*)@"attempt_machine_id_list";
OctagonFlag* const OctagonFlagUnlocked = (OctagonFlag*)@"unlocked";
OctagonFlag* const OctagonFlagAttemptSOSUpdatePreapprovals = (OctagonFlag*)@"attempt_sos_update_preapprovals";
OctagonFlag* const OctagonFlagAttemptSOSConsistency = (OctagonFlag*)@"attempt_sos_consistency";
OctagonFlag* const OctagonFlagEscrowRequestInformCloudServicesOperation = (OctagonFlag*)@"escrowrequest_inform_cloudservices";
OctagonFlag* const OctagonFlagAttemptBottleTLKExtraction = (OctagonFlag*)@"retry_bottle_tlk_extraction";
OctagonFlag* const OctagonFlagAttemptRecoveryKeyTLKExtraction = (OctagonFlag*)@"retry_rk_tlk_extraction";
OctagonFlag* const OctagonFlagSecureElementIdentityChanged  = (OctagonFlag*)@"se_id_changed";
OctagonFlag* const OctagonFlagAttemptUserControllableViewStatusUpgrade = (OctagonFlag*)@"attempt_ucv_upgrade";

@implementation OTStates

+ (NSArray<NSArray*>*) stateInit {
    NSArray<NSArray*>* stateInit = @[
                                     @[OctagonStateReady,                                   @0U,],
                                     @[OctagonStateError,                                    @1U,],
                                     @[OctagonStateInitializing,                             @2U,],
                                     @[OctagonStateMachineNotStarted,                        @3U,],
                                     @[OctagonStateDisabled,                                 @4U,],
                                     @[OctagonStateUntrusted,                                @5U,],

                                       //Removed: OctagonStateIdentityPrepared: @6U,],
                                       //Removed: OctagonStateDeviceListUpdated: @7U,],

                                       //8,11,12 used by multiple states

                                       //Removed: OctagonStateInitiatorAwaitingAcceptorEpoch: @9U,],
                                       //Removed: OctagonStateInitiatorReadyToSendIdentity: @10U,],


                                     @[OctagonStateReEnactDeviceList,                        @13U,],
                                     @[OctagonStateReEnactPrepare,                           @14U,],
                                     @[OctagonStateReEnactReadyToEstablish,                  @15U,],
                                     @[OctagonStateNoAccountDoReset,                         @16U,],
                                     @[OctagonStateBottleJoinVouchWithBottle,                @17U,],
                                     @[OctagonStateBottleJoinCreateIdentity,                 @18U,],
                                     @[OctagonStateCloudKitNewlyAvailable,                   @19U,],
                                     @[OctagonStateCheckTrustState,                          @20U,],
                                     @[OctagonStateBecomeUntrusted,                          @21U,],
                                     @[OctagonStateWaitForUnlock,                            @22U,],
                                     @[OctagonStateWaitingForCloudKitAccount,                @23U,],
                                     @[OctagonStateBecomeReady,                              @24U,],
                                     @[OctagonStateVouchWithRecoveryKey,                     @25U,],
                                     @[OctagonStateCreateIdentityForRecoveryKey,             @26U,],
                                     @[OctagonStateUpdateSOSPreapprovals,                    @27U,],
                                     @[OctagonStateWaitForHSA2,                              @28U,],
                                     @[OctagonStateAssistCKKSTLKUpload,                      @29U,],
                                     @[OctagonStateStartCompanionPairing,                    @30U,],
                                     @[OctagonStateEscrowTriggerUpdate,                      @31U,],
                                     @[OctagonStateEnsureConsistency,                        @32U,],
                                     @[OctagonStateResetBecomeUntrusted,                     @33U,],
                                     @[OctagonStateUntrustedUpdated,                         @34U,],
                                     @[OctagonStateReadyUpdated,                             @35U,],
                                     @[OctagonStateTPHTrustCheck,                            @36U,],
                                     @[OctagonStateCuttlefishTrustCheck,                     @37U,],
                                     @[OctagonStatePostRepairCFU,                            @38U,],
                                     @[OctagonStateSecurityTrustCheck,                       @39U,],
                                     @[OctagonStateEnsureOctagonKeysAreConsistent,           @40U,],
                                     @[OctagonStateEnsureUpdatePreapprovals,                 @41U,],
                                     @[OctagonStateResetAnyMissingTLKCKKSViews,              @42U,],
                                     @[OctagonStateEstablishCKKSReset,                       @43U,],
                                     @[OctagonStateEstablishAfterCKKSReset,                  @44U,],
                                     @[OctagonStateSOSUpgradeCKKSReset,                      @45U,],
                                     @[OctagonStateSOSUpgradeAfterCKKSReset,                 @46U,],
                                     @[OctagonStateInitiatorJoinCKKSReset,                   @47U,],
                                     @[OctagonStateInitiatorJoinAfterCKKSReset,              @48U,],
                                     @[OctagonStateHSA2HealthCheck,                          @49U,],
                                     @[OctagonStateHealthCheckReset,                         @50U,],
                                     @[OctagonStateAssistCKKSTLKUploadCKKSReset,             @51U,],
                                     @[OctagonStateAssistCKKSTLKUploadAfterCKKSReset,        @52U,],
                                     @[OctagonStateWaitForCDP,                               @53U,],
                                     @[OctagonStateDetermineCDPState,                        @54U,],
                                     @[OctagonStateWaitForCDPUpdated,                        @55U,],
                                     @[OctagonStateEstablishEnableCDPBit,                    @56U,],
                                     @[OctagonStateInitiatorSetCDPBit,                       @57U,],
                                     @[OctagonStateCDPHealthCheck,                           @58U,],
                                     @[OctagonStateHealthCheckLeaveClique,                   @59U,],
                                     @[OctagonStateRefetchCKKSPolicy,                        @60U,],
                                     @[OctagonStateEnableUserControllableViews,              @61U,],
                                     @[OctagonStateDisableUserControllableViews,             @62U,],
                                     @[OctagonStateSetUserControllableViewsToPeerConsensus,  @63U,],
                                     @[OctagonStateWaitForClassCUnlock,                      @64U,],
                                     @[OctagonStateBottlePreloadOctagonKeysInSOS,            @65U,],
                                     @[OctagonStateAttemptSOSUpgradeDetermineCDPState,       @66U,],
                                     @[OctagonStateLostAccountAuth,                          @67U,],
                                     @[OctagonStateCreateIdentityForCustodianRecoveryKey,    @69U,],
                                     @[OctagonStateVouchWithCustodianRecoveryKey,            @70U,],
                                     //Removed: @[OctagonStateFixupRefetchCuttlefishForCustodian,       @71U,],
                                     //Removed: @[OctagonStateFixupRefetchCuttlefishForCustodianFailed, @72U,],
                                     @[OctagonStateCheckForAccountFixups,                    @73U,],
                                     @[OctagonStatePerformAccountFixups,                     @74U,],
                                     @[OctagonStateJoinSOSAfterCKKSFetch,                    @75U,],
                                     //Removed: OctagonStateVouchWithInheritanceKey:         @76U,],
                                     //Removed: OctagonStateCreateIdentityForInheritanceKey: @77U,],
                                     @[OctagonStateAttemptSOSUpgrade,                        @78U,],
                                     @[OctagonStateInitiatorUpdateDeviceList,                @79U,],
                                     @[OctagonStateInitiatorAwaitingVoucher,                 @80U,],
                                     @[OctagonStateInitiatorJoin,                            @81U,],
                                     @[OctagonStateNoAccount,                                @82U,],
                                     @[OctagonStateResetAndEstablish,                        @83U,],
                                     @[OctagonStateUnimplemented,                            @84U,],
                                     @[OctagonStateDetermineiCloudAccountState,              @85U,],
                                     @[OctagonStateDetermineiCloudAccountState,              @85U,],
                                     @[OctagonStatePrepareAndRecoverTLKSharesForInheritancePeer,              @86U,],
                                     @[OctagonStateBecomeInherited,                          @87U,],
                                     @[OctagonStateInherited,                                @88U,],
                                     @[OctagonStatePeerMissingFromServer,                    @89U,],
                                     @[OctagonStateResetAndEstablishClearLocalContextState,  @90U,],
                                     @[OctagonStateLocalReset,                               @91U,],
                                     @[OctagonStateLocalResetClearLocalContextState,         @92U,],
                                     ];
    return stateInit;
}

+ (NSDictionary<OctagonState*, NSNumber*>*) OctagonStateMap {
    static NSDictionary<OctagonState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
            NSMutableDictionary *tmp = [[NSMutableDictionary alloc] init];
            NSArray<NSArray*>* aa = [self stateInit];
            for(NSArray *a in aa) {
                NSString *stateName = a[0];
                NSNumber *stateNum = a[1];

                NSAssert([stateName isKindOfClass:[NSString class]], @"stateName should be string");
                NSAssert([stateNum isKindOfClass:[NSNumber class]], @"stateNum should be number");
                tmp[stateName] = stateNum;
            }
            map = tmp;
    });
    return map;
}

+ (NSDictionary<NSNumber*, OctagonState*>*) OctagonStateInverseMap {
    static NSDictionary<NSNumber*, OctagonState*>* backwardMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
            NSMutableDictionary *tmp = [[NSMutableDictionary alloc] init];
            NSArray<NSArray*>* aa = [self stateInit];
            for(NSArray *a in aa) {
                NSString *stateName = a[0];
                NSNumber *stateNum = a[1];

                NSAssert([stateName isKindOfClass:[NSString class]], @"stateName should be string");
                NSAssert([stateNum isKindOfClass:[NSNumber class]], @"stateNum should be number");
                tmp[stateNum] = stateName;
            }
            backwardMap = tmp;
    });
    return backwardMap;
}

+ (NSSet<OctagonState*>*) OctagonInAccountStates
{
    static NSSet<OctagonState*>* s = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet* sourceStates = [NSMutableSet setWithArray: [self OctagonStateMap].allKeys];

        // NoAccount is obviously not in-account, but we also include the startup states that determine
        // apple account and icloud account status:
        [sourceStates removeObject:OctagonStateNoAccount];
        [sourceStates removeObject:OctagonStateNoAccountDoReset];
        [sourceStates removeObject:OctagonStateInitializing];
        [sourceStates removeObject:OctagonStateDetermineiCloudAccountState];
        [sourceStates removeObject:OctagonStateWaitingForCloudKitAccount];
        [sourceStates removeObject:OctagonStateCloudKitNewlyAvailable];
        [sourceStates removeObject:OctagonStateWaitForHSA2];
        [sourceStates removeObject:OctagonStateLocalReset];
        [sourceStates removeObject:OctagonStateLocalResetClearLocalContextState];

        // If the device hasn't unlocked yet, we don't know what we wrote down for iCloud account status
        [sourceStates removeObject:OctagonStateWaitForClassCUnlock];

        s = sourceStates;
    });
    return s;
}

+ (NSSet<OctagonState*>*) OctagonNotInCliqueStates
{
    static NSSet<OctagonState*>* s = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet* sourceStates = [NSMutableSet set];

        [sourceStates addObject:OctagonStateNoAccount];
        [sourceStates addObject:OctagonStateNoAccountDoReset];
        [sourceStates addObject:OctagonStateInitializing];
        [sourceStates addObject:OctagonStateDetermineiCloudAccountState];
        [sourceStates addObject:OctagonStateWaitingForCloudKitAccount];
        [sourceStates addObject:OctagonStateCloudKitNewlyAvailable];
        [sourceStates addObject:OctagonStateWaitForHSA2];
        [sourceStates addObject:OctagonStateUntrusted];

        s = sourceStates;
    });
    return s;
}

+ (NSSet<OctagonState *>*) OctagonHealthSourceStates
{
    static NSSet<OctagonState*>* s = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet* sourceStates = [NSMutableSet set];

        [sourceStates addObject:OctagonStateReady];
        [sourceStates addObject:OctagonStateError];
        [sourceStates addObject:OctagonStateUntrusted];
        [sourceStates addObject:OctagonStateWaitForHSA2];
        [sourceStates addObject:OctagonStateWaitForUnlock];
        [sourceStates addObject:OctagonStateWaitForCDP];

        s = sourceStates;
    });
    return s;
}

+ (NSSet<OctagonFlag *>*) AllOctagonFlags
{
    static NSSet<OctagonFlag*>* f = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet* flags = [NSMutableSet set];

        [flags addObject:OctagonFlagIDMSLevelChanged];
        [flags addObject:OctagonFlagEgoPeerPreapproved];
        [flags addObject:OctagonFlagCKKSRequestsTLKUpload];
        [flags addObject:OctagonFlagCKKSRequestsPolicyCheck];
        [flags addObject:OctagonFlagCKKSViewSetChanged];
        [flags addObject:OctagonFlagCuttlefishNotification];
        [flags addObject:OctagonFlagAccountIsAvailable];
        [flags addObject:OctagonFlagCDPEnabled];
        [flags addObject:OctagonFlagAttemptSOSUpgrade];
        [flags addObject:OctagonFlagFetchAuthKitMachineIDList];
        [flags addObject:OctagonFlagUnlocked];
        [flags addObject:OctagonFlagAttemptSOSUpdatePreapprovals];
        [flags addObject:OctagonFlagAttemptSOSConsistency];
        [flags addObject:OctagonFlagAttemptUserControllableViewStatusUpgrade];
        [flags addObject:OctagonFlagAttemptBottleTLKExtraction];
        [flags addObject:OctagonFlagAttemptRecoveryKeyTLKExtraction];
        [flags addObject:OctagonFlagSecureElementIdentityChanged];

        f = flags;
    });
    return f;
}

@end

#endif // OCTAGON
