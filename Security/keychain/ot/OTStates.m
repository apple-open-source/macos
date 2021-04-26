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

OctagonState* const OctagonStateNoAccount = (OctagonState*) @"no_account";

OctagonState* const OctagonStateWaitForHSA2 = (OctagonState*) @"wait_for_hsa2";
OctagonState* const OctagonStateWaitForCDP = (OctagonState*) @"wait_for_cdp_enable";

OctagonState* const OctagonStateUntrusted = (OctagonState*) @"untrusted";
OctagonState* const OctagonStateBecomeUntrusted = (OctagonState*) @"become_untrusted";

OctagonState* const OctagonStateReady = (OctagonState*) @"ready";
OctagonState* const OctagonStateBecomeReady = (OctagonState*) @"become_ready";

OctagonState* const OctagonStateEnsureConsistency = (OctagonState*) @"consistency_check";
OctagonState* const OctagonStateEnsureOctagonKeysAreConsistent = (OctagonState*)@"key_consistency_check";
OctagonState* const OctagonStateEnsureUpdatePreapprovals = (OctagonState*)@"ensure_preapprovals_updated";

OctagonState* const OctagonStateInitializing = (OctagonState*) @"initializing";
OctagonState* const OctagonStateWaitingForCloudKitAccount = (OctagonState*) @"waiting_for_cloudkit_account";
OctagonState* const OctagonStateCloudKitNewlyAvailable = (OctagonState*) @"account_newly_available";
OctagonState* const OctagonStateRefetchCKKSPolicy = (OctagonState*) @"ckks_fetch_policy";
OctagonState* const OctagonStateDetermineCDPState = (OctagonState*) @"check_cdp_state";
OctagonState* const OctagonStateCheckTrustState = (OctagonState*) @"check_trust_state";

OctagonState* const OctagonStateEnableUserControllableViews = (OctagonState*) @"ckks_set_user_controllable_views_on";
OctagonState* const OctagonStateDisableUserControllableViews = (OctagonState*) @"ckks_set_user_controlable_views_off";
OctagonState* const OctagonStateSetUserControllableViewsToPeerConsensus = (OctagonState*) @"ckks_set_user_controlable_views_peer_consensus";

OctagonState* const OctagonStateUpdateSOSPreapprovals = (OctagonState*) @"update_sos_preapprovals";

/*Piggybacking and ProximitySetup as Initiator Octagon only*/
OctagonState* const OctagonStateInitiatorSetCDPBit = (OctagonState*) @"initiator_set_cdp";
OctagonState* const OctagonStateInitiatorUpdateDeviceList = (OctagonState*) @"initiator_device_list_update";
OctagonState* const OctagonStateInitiatorAwaitingVoucher = (OctagonState*)@"await_voucher";
OctagonState* const OctagonStateInitiatorJoin = (OctagonState*)@"join";
OctagonState* const OctagonStateInitiatorJoinCKKSReset = (OctagonState*)@"join_ckks_reset";
OctagonState* const OctagonStateInitiatorJoinAfterCKKSReset = (OctagonState*)@"join_after_ckks_reset";

/* used in restore (join with bottle)*/
OctagonState* const OctagonStateBottleJoinCreateIdentity = (OctagonState*)@"bottle_join_create_identity";
OctagonState* const OctagonStateBottleJoinVouchWithBottle = (OctagonState*)@"bottle_join_vouch_with_bottle";
OctagonState* const OctagonStateCreateIdentityForRecoveryKey = (OctagonState*)@"vouchWithRecovery";
OctagonState* const OctagonStateBottlePreloadOctagonKeysInSOS = (OctagonState*)@"bottle_preload_octagon_keys_in_sos";

/* used in resotre (join with recovery key)*/
OctagonState* const OctagonStateVouchWithRecoveryKey = (OctagonState*)@"vouchWithRecoveryKey";

OctagonState* const OctagonStateStartCompanionPairing = (OctagonState*)@"start_companion_pairing";

OctagonState* const OctagonStateWaitForCDPUpdated = (OctagonState*)@"wait_for_cdp_update";

// Untrusted cuttlefish notification.
OctagonState* const OctagonStateUntrustedUpdated = (OctagonState*)@"untrusted_update";

// Cuttlefish notifiation while ready.
OctagonState* const OctagonStateReadyUpdated = (OctagonState*)@"ready_update";

OctagonState* const OctagonStateError = (OctagonState*) @"error";
OctagonState* const OctagonStateDisabled = (OctagonState*) @"disabled";

OctagonState* const OctagonStateDetermineiCloudAccountState = (OctagonState*) @"determine_icloud_account";

OctagonState* const OctagonStateAttemptSOSUpgradeDetermineCDPState = (OctagonState*) @"sosupgrade_cdp_check";
OctagonState* const OctagonStateAttemptSOSUpgrade = (OctagonState*) @"sosupgrade";
OctagonState* const OctagonStateSOSUpgradeCKKSReset = (OctagonState*) @"sosupgrade_ckks_reset";
OctagonState* const OctagonStateSOSUpgradeAfterCKKSReset = (OctagonState*) @"sosupgrade_after_ckks_reset";
OctagonState* const OctagonStateUnimplemented = (OctagonState*) @"unimplemented";

/* Reset and establish */
OctagonState* const OctagonStateResetBecomeUntrusted = (OctagonState*) @"reset_become_untrusted";
OctagonState* const OctagonStateResetAndEstablish = (OctagonState*) @"reset_and_establish";
OctagonState* const OctagonStateResetAnyMissingTLKCKKSViews = (OctagonState*) @"reset_ckks_missing_views";
OctagonState* const OctagonStateEstablishEnableCDPBit = (OctagonState*) @"reenact_cdp_bit";
OctagonState* const OctagonStateReEnactDeviceList = (OctagonState*) @"reenact_device_list";
OctagonState* const OctagonStateReEnactPrepare = (OctagonState*) @"reenact_prepare";
OctagonState* const OctagonStateReEnactReadyToEstablish = (OctagonState*) @"reenact_ready_to_establish";
OctagonState* const OctagonStateEstablishCKKSReset = (OctagonState*) @"reenact_ckks_reset";
OctagonState* const OctagonStateEstablishAfterCKKSReset = (OctagonState*) @"reenact_establish_after_ckks_reset";

/* used for trust health checks */
OctagonState* const OctagonStateHSA2HealthCheck = (OctagonState*) @"health_hsa2_check";
OctagonState* const OctagonStateCDPHealthCheck = (OctagonState*) @"health_cdp_check";
OctagonState* const OctagonStateTPHTrustCheck = (OctagonState*) @"tph_trust_check";
OctagonState* const OctagonStateCuttlefishTrustCheck = (OctagonState*) @"cuttlefish_trust_check";
OctagonState* const OctagonStatePostRepairCFU = (OctagonState*) @"post_repair_cfu";
OctagonState* const OctagonStateSecurityTrustCheck = (OctagonState*) @"security_trust_check";
OctagonState* const OctagonStateHealthCheckReset = (OctagonState*) @"health_check_reset";
/* signout */
OctagonState* const OctagonStateNoAccountDoReset = (OctagonState*) @"no_account_do_reset";

OctagonState* const OctagonStateLostAccountAuth = (OctagonState*) @"authkit_auth_lost";

OctagonState* const OctagonStateWaitForUnlock = (OctagonState*) @"wait_for_unlock";
OctagonState* const OctagonStateWaitForClassCUnlock = (OctagonState*) @"wait_for_class_c_unlock";

OctagonState* const OctagonStateAssistCKKSTLKUpload = (OctagonState*) @"assist_ckks_tlk_upload";
OctagonState* const OctagonStateAssistCKKSTLKUploadCKKSReset = (OctagonState*) @"assist_ckks_tlk_upload_ckks_reset";
OctagonState* const OctagonStateAssistCKKSTLKUploadAfterCKKSReset = (OctagonState*) @"assist_ckks_tlk_upload_after_ckks_reset";

OctagonState* const OctagonStateHealthCheckLeaveClique = (OctagonState*) @"leave_clique";

/* escrow */
OctagonState* const OctagonStateEscrowTriggerUpdate = (OctagonState*) @"escrow-trigger-update";

NSDictionary<OctagonState*, NSNumber*>* OctagonStateMap(void) {
    static NSDictionary<OctagonState*, NSNumber*>* map = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        map = @{
                OctagonStateReady:                              @0U,
                OctagonStateError:                              @1U,
                OctagonStateInitializing:                       @2U,
                OctagonStateMachineNotStarted:                  @3U,
                OctagonStateDisabled:                           @4U,
                OctagonStateUntrusted:                          @5U,

                //Removed: OctagonStateInitiatorAwaitingAcceptorEpoch:     @9U,
                //Removed: OctagonStateInitiatorReadyToSendIdentity:       @10U,

                OctagonStateInitiatorUpdateDeviceList:          @8U,
                OctagonStateInitiatorAwaitingVoucher:           @11U,
                OctagonStateInitiatorJoin:                      @12U,

                //Removed: OctagonStateIdentityPrepared:                   @6U,
                //Removed: OctagonStateDeviceListUpdated:                  @7U,

                OctagonStateAttemptSOSUpgrade:                  @8U,

                OctagonStateUnimplemented:                      @9U,
                OctagonStateDetermineiCloudAccountState:        @10U,
                OctagonStateNoAccount:                          @11U,

                OctagonStateResetAndEstablish:                  @12U,
                OctagonStateReEnactDeviceList:                  @13U,
                OctagonStateReEnactPrepare:                     @14U,
                OctagonStateReEnactReadyToEstablish:            @15U,
                OctagonStateNoAccountDoReset:                   @16U,
                OctagonStateBottleJoinVouchWithBottle:          @17U,
                OctagonStateBottleJoinCreateIdentity:           @18U,
                OctagonStateCloudKitNewlyAvailable:             @19U,
                OctagonStateCheckTrustState:                    @20U,
                OctagonStateBecomeUntrusted:                    @21U,
                OctagonStateWaitForUnlock:                      @22U,
                OctagonStateWaitingForCloudKitAccount:          @23U,
                OctagonStateBecomeReady:                        @24U,
                OctagonStateVouchWithRecoveryKey:               @25U,
                OctagonStateCreateIdentityForRecoveryKey:       @26U,
                OctagonStateUpdateSOSPreapprovals:              @27U,
                OctagonStateWaitForHSA2:                        @28U,
                OctagonStateAssistCKKSTLKUpload:                @29U,
                OctagonStateStartCompanionPairing:              @30U,
                OctagonStateEscrowTriggerUpdate:                @31U,
                OctagonStateEnsureConsistency:                  @32U,
                OctagonStateResetBecomeUntrusted:               @33U,
                OctagonStateUntrustedUpdated:                   @34U,
                OctagonStateReadyUpdated:                       @35U,
                OctagonStateTPHTrustCheck:                      @36U,
                OctagonStateCuttlefishTrustCheck:               @37U,
                OctagonStatePostRepairCFU:                      @38U,
                OctagonStateSecurityTrustCheck:                 @39U,
                OctagonStateEnsureOctagonKeysAreConsistent:     @40U,
                OctagonStateEnsureUpdatePreapprovals:           @41U,
                OctagonStateResetAnyMissingTLKCKKSViews:        @42U,
                OctagonStateEstablishCKKSReset:                 @43U,
                OctagonStateEstablishAfterCKKSReset:            @44U,
                OctagonStateSOSUpgradeCKKSReset:                @45U,
                OctagonStateSOSUpgradeAfterCKKSReset:           @46U,
                OctagonStateInitiatorJoinCKKSReset:             @47U,
                OctagonStateInitiatorJoinAfterCKKSReset:        @48U,
                OctagonStateHSA2HealthCheck:                    @49U,
                OctagonStateHealthCheckReset:                   @50U,
                OctagonStateAssistCKKSTLKUploadCKKSReset:       @51U,
                OctagonStateAssistCKKSTLKUploadAfterCKKSReset:  @52U,
                OctagonStateWaitForCDP:                         @53U,
                OctagonStateDetermineCDPState:                  @54U,
                OctagonStateWaitForCDPUpdated:                  @55U,
                OctagonStateEstablishEnableCDPBit:              @56U,
                OctagonStateInitiatorSetCDPBit:                 @57U,
                OctagonStateCDPHealthCheck:                     @58U,
                OctagonStateHealthCheckLeaveClique:             @59U,
                OctagonStateRefetchCKKSPolicy:                  @60U,
                OctagonStateEnableUserControllableViews:        @61U,
                OctagonStateDisableUserControllableViews:       @62U,
                OctagonStateSetUserControllableViewsToPeerConsensus: @63U,
                OctagonStateWaitForClassCUnlock:                @64U,
                OctagonStateBottlePreloadOctagonKeysInSOS:      @65U,
                OctagonStateAttemptSOSUpgradeDetermineCDPState: @66U,
                OctagonStateLostAccountAuth:                    @67U,
            };
    });
    return map;
}

NSDictionary<NSNumber*, OctagonState*>* OctagonStateInverseMap(void) {
    static NSDictionary<NSNumber*, OctagonState*>* backwardMap = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSDictionary<OctagonState*, NSNumber*>* forwardMap = OctagonStateMap();
        backwardMap = [NSDictionary dictionaryWithObjects:[forwardMap allKeys] forKeys:[forwardMap allValues]];
    });
    return backwardMap;
}

NSSet<OctagonState*>* OctagonInAccountStates(void)
{
    static NSSet<OctagonState*>* s = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet* sourceStates = [NSMutableSet setWithArray: OctagonStateMap().allKeys];

        // NoAccount is obviously not in-account, but we also include the startup states that determine
        // apple account and icloud account status:
        [sourceStates removeObject:OctagonStateNoAccount];
        [sourceStates removeObject:OctagonStateNoAccountDoReset];
        [sourceStates removeObject:OctagonStateInitializing];
        [sourceStates removeObject:OctagonStateDetermineiCloudAccountState];
        [sourceStates removeObject:OctagonStateWaitingForCloudKitAccount];
        [sourceStates removeObject:OctagonStateCloudKitNewlyAvailable];
        [sourceStates removeObject:OctagonStateWaitForHSA2];

        // If the device hasn't unlocked yet, we don't know what we wrote down for iCloud account status
        [sourceStates removeObject:OctagonStateWaitForClassCUnlock];

        s = sourceStates;
    });
    return s;
}

NSSet<OctagonState*>* OctagonNotInCliqueStates(void)
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

NSSet<OctagonState *>* OctagonHealthSourceStates(void)
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

// Flags
OctagonFlag* const OctagonFlagIDMSLevelChanged = (OctagonFlag*) @"idms_level";
OctagonFlag* const OctagonFlagEgoPeerPreapproved = (OctagonFlag*) @"preapproved";
OctagonFlag* const OctagonFlagCKKSRequestsTLKUpload = (OctagonFlag*) @"tlk_upload_needed";
OctagonFlag* const OctagonFlagCKKSRequestsPolicyCheck = (OctagonFlag*) @"policy_check_needed";;
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
OctagonFlag* const OctagonFlagWarmEscrowRecordCache = (OctagonFlag*)@"warm_escrow_cache";
OctagonFlag* const OctagonFlagAttemptBottleTLKExtraction = (OctagonFlag*)@"retry_bottle_tlk_extraction";
OctagonFlag* const OctagonFlagAttemptRecoveryKeyTLKExtraction = (OctagonFlag*)@"retry_rk_tlk_extraction";

OctagonFlag* const OctagonFlagAttemptUserControllableViewStatusUpgrade = (OctagonFlag*)@"attempt_ucv_upgrade";

NSSet<OctagonFlag *>* AllOctagonFlags(void)
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
        [flags addObject:OctagonFlagWarmEscrowRecordCache];
        [flags addObject:OctagonFlagAttemptUserControllableViewStatusUpgrade];
        [flags addObject:OctagonFlagAttemptBottleTLKExtraction];
        [flags addObject:OctagonFlagAttemptRecoveryKeyTLKExtraction];

        f = flags;
    });
    return f;
}

#endif // OCTAGON
