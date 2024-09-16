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

#import <Foundation/Foundation.h>
#import <AppleFeatures/AppleFeatures.h>
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

NS_ASSUME_NONNULL_BEGIN

extern NSString* const OctagonStateTransitionErrorDomain;

// Two 'bad states':
//   No iCloud Account (the state machine won't help at all)
//   Untrusted (user interaction is required to resolve)
//   WaitForCDPCapableSecurityLevel (there's some primary icloud account, but it's not HSA2/Managed (yet))
//   WaitForCDP (there's some HSA2/Managed primary icloud account, but it's not CDP-enabled (yet)
extern OctagonState* const OctagonStateNoAccount;
extern OctagonState* const OctagonStateUntrusted;
extern OctagonState* const OctagonStateWaitForCDPCapableSecurityLevel;
extern OctagonState* const OctagonStateWaitForCDP;

// Entering this state will mark down that the device is untrusted, then go to OctagonStateUntrusted
extern OctagonState* const OctagonStateBecomeUntrusted;

// WaitForUnlock indicates that Octagon is waiting for the device to unlock before attempting the pended operation
extern OctagonState* const OctagonStateWaitForUnlock;

// Similar to the above, but we can't even be sure there's an account until the device unlocks for the first time.
extern OctagonState* const OctagonStateWaitForClassCUnlock;

// 'ready' indicates that this machine believes it is trusted by its peers
// and has no pending things to do.
extern OctagonState* const OctagonStateReady;

// 'inherited' indicates that this machine believes it has finished the inheritance flow
// and has no pending things to do.
extern OctagonState* const OctagonStateBecomeInherited;
extern OctagonState* const OctagonStateInherited;

// This state runs any final preparation to enter the Ready state
extern OctagonState* const OctagonStateBecomeReady;

// BecomeReady might go here, if it's not actually ready
extern OctagonState* const OctagonStateRefetchCKKSPolicy;

// Used in RPCs to set CKKS sync status
extern OctagonState* const OctagonStateEnableUserControllableViews;
extern OctagonState* const OctagonStateDisableUserControllableViews;
extern OctagonState* const OctagonStateSetUserControllableViewsToPeerConsensus;

// Enter this state if you'd like the state machine to double-check everything
extern OctagonState* const OctagonStateEnsureConsistency;
extern OctagonState* const OctagonStateEnsureUpdatePreapprovals;

// The boot-up sequence looks as follows:
extern OctagonState* const OctagonStateInitializing;
extern OctagonState* const OctagonStateWaitingForCloudKitAccount;
extern OctagonState* const OctagonStateCloudKitNewlyAvailable;
extern OctagonState* const OctagonStateDetermineCDPState;
extern OctagonState* const OctagonStateCheckForAccountFixups;
extern OctagonState* const OctagonStateCheckTrustState;

extern OctagonState* const OctagonStatePerformAccountFixups;

/*Piggybacking and ProximitySetup as Initiator, Octagon only*/
extern OctagonState* const OctagonStateInitiatorAwaitingVoucher;

extern OctagonState* const OctagonStateInitiatorSetCDPBit;
extern OctagonState* const OctagonStateInitiatorUpdateDeviceList;
extern OctagonState* const OctagonStateInitiatorJoin;
extern OctagonState* const OctagonStateInitiatorJoinCKKSReset;
extern OctagonState* const OctagonStateInitiatorJoinAfterCKKSReset;

extern OctagonState* const OctagonStateBottleJoinVouchWithBottle;
extern OctagonState* const OctagonStateIdentityPrepared;
// OctagonStateIdentityPrepared leads directly to
extern OctagonState* const OctagonStateDeviceListUpdated;

/* used for join with bottle */
extern OctagonState* const OctagonStateBottleJoinCreateIdentity;
extern OctagonState* const OctagonStateBottlePreloadOctagonKeysInSOS;

/* used as part of joining with recovery key */
extern OctagonState* const OctagonStateStashAccountSettingsForRecoveryKey;

/* used for join with recovery key */
extern OctagonState* const OctagonStateCreateIdentityForRecoveryKey;

/* used for join with recovery key*/
extern OctagonState* const OctagonStateVouchWithRecoveryKey;

/* used for join with custodian recovery key */
extern OctagonState* const OctagonStateCreateIdentityForCustodianRecoveryKey;

/* used for join with custodian recovery key */
extern OctagonState* const OctagonStateCreateIdentityForInheritanceKey;

/* used for join with custodian recovery key*/
extern OctagonState* const OctagonStateVouchWithCustodianRecoveryKey;
extern OctagonState* const OctagonStateJoinSOSAfterCKKSFetch;

/* used inheritance key flow*/
extern OctagonState* const OctagonStatePrepareAndRecoverTLKSharesForInheritancePeer;

// State flow when performing a full account reset
extern OctagonState* const OctagonStateResetBecomeUntrusted;
extern OctagonState* const OctagonStateResetAndEstablish;
extern OctagonState* const OctagonStateResetAnyMissingTLKCKKSViews;
extern OctagonState* const OctagonStateEstablishEnableCDPBit;
extern OctagonState* const OctagonStateReEnactDeviceList;
extern OctagonState* const OctagonStateReEnactPrepare;
extern OctagonState* const OctagonStateReEnactReadyToEstablish;
extern OctagonState* const OctagonStateResetAndEstablishClearLocalContextState;

// local reset
extern OctagonState* const OctagonStateLocalReset;
extern OctagonState* const OctagonStateLocalResetClearLocalContextState;

// account wipe for SOS deferral testing
extern OctagonState* const OctagonStateCuttlefishReset;
extern OctagonState* const OctagonStateCKKSResetAfterOctagonReset;

// this last state might loop through:
extern OctagonState* const OctagonStateEstablishCKKSReset;
extern OctagonState* const OctagonStateEstablishAfterCKKSReset;
// End of account reset state flow

/* used for trust health checks */
extern OctagonState* const OctagonStateCDPHealthCheck;
extern OctagonState* const OctagonStateSecurityTrustCheck;
extern OctagonState* const OctagonStateTPHTrustCheck;
extern OctagonState* const OctagonStateCuttlefishTrustCheck;
extern OctagonState* const OctagonStatePostRepairCFU;
extern OctagonState* const OctagonStateHealthCheckReset;

extern OctagonState* const OctagonStateSetAccountSettings;

//Leave Clique
extern OctagonState* const OctagonStateHealthCheckLeaveClique;

// Part of the signout flow
extern OctagonState* const OctagonStateNoAccountDoReset;
//

// Used if Cuttlefish tells us that our peer is gone
extern OctagonState* const OctagonStatePeerMissingFromServer;

// escrow
extern OctagonState* const OctagonStateEscrowTriggerUpdate;

// Enter this state to perform an SOS peer update, and return to ready.
extern OctagonState* const OctagonStateUpdateSOSPreapprovals;

extern OctagonState* const OctagonStateError;

extern OctagonState* const OctagonStateAttemptSOSUpgradeDetermineCDPState;
extern OctagonState* const OctagonStateAttemptSOSUpgrade;
extern OctagonState* const OctagonStateSOSUpgradeCKKSReset;
extern OctagonState* const OctagonStateSOSUpgradeAfterCKKSReset;

extern OctagonState* const OctagonStateDetermineiCloudAccountState;

// CKKS sometimes needs an assist. These states are supposed to handle those cases
extern OctagonState* const OctagonStateAssistCKKSTLKUpload;
extern OctagonState* const OctagonStateAssistCKKSTLKUploadCKKSReset;
extern OctagonState* const OctagonStateAssistCKKSTLKUploadAfterCKKSReset;

// Cuttlefish notification while waiting for CDP
extern OctagonState* const OctagonStateWaitForCDPUpdated;

// Untrusted cuttlefish notification.
extern OctagonState* const OctagonStateUntrustedUpdated;

// Cuttlefish notifiation while ready.
extern OctagonState* const OctagonStateReadyUpdated;

extern OctagonState* const OctagonStateStashAccountSettingsForReroll;
extern OctagonState* const OctagonStateCreateIdentityForReroll;
extern OctagonState* const OctagonStateVouchWithReroll;

extern OctagonState* const OctagonStateUnimplemented;

@interface OTStates: NSObject
+ (NSDictionary<OctagonState*, NSNumber*>*) OctagonStateMap;
+ (NSDictionary<NSNumber*, OctagonState*>*) OctagonStateInverseMap;

// Unfortunately, this set contains the 'wait for cdp capable security level' state, which means that many
// of our state machine RPCs will work in the SA case.
// <rdar://problem/54094162> Octagon: ensure Octagon operations can't occur on SA accounts
+ (NSSet<OctagonState*>*) OctagonInAccountStates;
+ (NSSet<OctagonState*>*) OctagonHealthSourceStates;
+ (NSSet<OctagonState*>*) OctagonNotInCliqueStates;
+ (NSSet<OctagonState*>*) OctagonReadyStates;
+ (NSSet<OctagonState*>*) OctagonAllStates;
+ (NSSet<OctagonFlag*>*)  AllOctagonFlags;

@end

////// State machine flags
extern OctagonFlag* const OctagonFlagIDMSLevelChanged;

extern OctagonFlag* const OctagonFlagEgoPeerPreapproved;

extern OctagonFlag* const OctagonFlagCKKSRequestsTLKUpload;
extern OctagonFlag* const OctagonFlagCKKSRequestsPolicyCheck;

// Set by Octagon when the CKKS view set has changed. Indicates a need to re-tell CKKS if it's trusted or not.
extern OctagonFlag* const OctagonFlagCKKSViewSetChanged;

// We've received a change notification from cuttlefish; we should probably see what's new
extern OctagonFlag* const OctagonFlagCuttlefishNotification NS_SWIFT_NAME(OctagonFlagCuttlefishNotification);

extern OctagonFlag* const OctagonFlagAccountIsAvailable;
extern OctagonFlag* const OctagonFlagCDPEnabled;

extern OctagonFlag* const OctagonFlagAttemptSOSUpgrade;

extern OctagonFlag* const OctagonFlagFetchAuthKitMachineIDList;

extern OctagonFlag* const OctagonFlagUnlocked;

extern OctagonFlag* const OctagonFlagAttemptSOSUpdatePreapprovals;
extern OctagonFlag* const OctagonFlagAttemptSOSConsistency;

extern OctagonFlag* const OctagonFlagSecureElementIdentityChanged;

extern OctagonFlag* const OctagonFlagAttemptUserControllableViewStatusUpgrade;

extern OctagonFlag* const OctagonFlagCheckOnRTCMetrics;

extern OctagonFlag* const OctagonFlagPendingNetworkAvailablity;

extern OctagonFlag* const OctagonFlagCheckTrustState;

extern OctagonFlag* const OctagonFlagAppleAccountSignedOut;

NS_ASSUME_NONNULL_END

#endif // OCTAGON
