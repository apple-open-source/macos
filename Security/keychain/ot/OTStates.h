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
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"

NS_ASSUME_NONNULL_BEGIN

// Two 'bad states':
//   No iCloud Account (the state machine won't help at all)
//   Untrusted (user interaction is required to resolve)
//   WaitForHSA2 (there's some primary icloud account, but it's not HSA2 (yet))
extern OctagonState* const OctagonStateNoAccount;
extern OctagonState* const OctagonStateUntrusted;
extern OctagonState* const OctagonStateWaitForHSA2;

// Entering this state will mark down that the device is untrusted, then go to OctagonStateUntrusted
extern OctagonState* const OctagonStateBecomeUntrusted;

// WaitForUnlock indicates that Octagon is waiting for the device to unlock before attempting the pended operation
extern OctagonState* const OctagonStateWaitForUnlock;

// 'ready' indicates that this machine believes it is trusted by its peers
// and has no pending things to do.
extern OctagonState* const OctagonStateReady;

// This state runs any final preparation to enter the Ready state
extern OctagonState* const OctagonStateBecomeReady;

// Enter this state if you'd like the state machine to double-check everything
extern OctagonState* const OctagonStateEnsureConsistency;
extern OctagonState* const OctagonStateEnsureOctagonKeysAreConsistent;
extern OctagonState* const OctagonStateEnsureUpdatePreapprovals;

// The boot-up sequence looks as follows:
extern OctagonState* const OctagonStateInitializing;
extern OctagonState* const OctagonStateWaitingForCloudKitAccount;
extern OctagonState* const OctagonStateCloudKitNewlyAvailable;
extern OctagonState* const OctagonStateCheckTrustState;

/*Piggybacking and ProximitySetup as Initiator, Octagon only*/
extern OctagonState* const OctagonStateInitiatorAwaitingVoucher;

extern OctagonState* const OctagonStateInitiatorUpdateDeviceList;
extern OctagonState* const OctagonStateInitiatorJoin;
extern OctagonState* const OctagonStateInitiatorJoinCKKSReset;
extern OctagonState* const OctagonStateInitiatorJoinAfterCKKSReset;

extern OctagonState* const OctagonStateInitiatorVouchWithBottle;
extern OctagonState* const OctagonStateIdentityPrepared;
// OctagonStateIdentityPrepared leads directly to
extern OctagonState* const OctagonStateDeviceListUpdated;

/* used for join with bottle */
extern OctagonState* const OctagonStateInitiatorCreateIdentity;

/* used for join with recovery key */
extern OctagonState* const OctagonStateCreateIdentityForRecoveryKey;

/* used for join with recovery key*/
extern OctagonState* const OctagonStateVouchWithRecoveryKey;

// State flow when performing a full account reset
extern OctagonState* const OctagonStateResetBecomeUntrusted;
extern OctagonState* const OctagonStateResetAndEstablish;
extern OctagonState* const OctagonStateResetAnyMissingTLKCKKSViews;
extern OctagonState* const OctagonStateReEnactDeviceList;
extern OctagonState* const OctagonStateReEnactPrepare;
extern OctagonState* const OctagonStateReEnactReadyToEstablish;
// this last state might loop through:
extern OctagonState* const OctagonStateEstablishCKKSReset;
extern OctagonState* const OctagonStateEstablishAfterCKKSReset;

/* used for trust health checks */
extern OctagonState* const OctagonStateHSA2HealthCheck;
extern OctagonState* const OctagonStateSecurityTrustCheck;
extern OctagonState* const OctagonStateTPHTrustCheck;
extern OctagonState* const OctagonStateCuttlefishTrustCheck;
extern OctagonState* const OctagonStatePostRepairCFU;
extern OctagonState* const OctagonStateHealthCheckReset;

// End of account reset state flow

// Part of the signout flow
extern OctagonState* const OctagonStateNoAccountDoReset;
//

// escrow
extern OctagonState* const OctagonStateEscrowTriggerUpdate;

// Enter this state to perform an SOS peer update, and return to ready.
extern OctagonState* const OctagonStateUpdateSOSPreapprovals;

extern OctagonState* const OctagonStateError;
extern OctagonState* const OctagonStateDisabled;

extern OctagonState* const OctagonStateAttemptSOSUpgrade;
extern OctagonState* const OctagonStateSOSUpgradeCKKSReset;
extern OctagonState* const OctagonStateSOSUpgradeAfterCKKSReset;

extern OctagonState* const OctagonStateDetermineiCloudAccountState;

// CKKS sometimes needs an assist. These states are supposed to handle those cases
extern OctagonState* const OctagonStateAssistCKKSTLKUpload;
extern OctagonState* const OctagonStateAssistCKKSTLKUploadCKKSReset;
extern OctagonState* const OctagonStateAssistCKKSTLKUploadAfterCKKSReset;

// Call out to otpaird (KCPairing via IDS), then proceed to BecomeUntrusted
extern OctagonState* const OctagonStateStartCompanionPairing;

// Untrusted cuttlefish notification.
extern OctagonState* const OctagonStateUntrustedUpdated;

// Cuttlefish notifiation while ready.
extern OctagonState* const OctagonStateReadyUpdated;

extern OctagonState* const OctagonStateUnimplemented;

NSDictionary<OctagonState*, NSNumber*>* OctagonStateMap(void);
NSDictionary<NSNumber*, OctagonState*>* OctagonStateInverseMap(void);

// Unfortunately, this set contains the 'wait for hsa2' state, which means that many
// of our state machine RPCs will work in the SA case.
// <rdar://problem/54094162> Octagon: ensure Octagon operations can't occur on SA accounts
NSSet<OctagonState*>* OctagonInAccountStates(void);
NSSet<OctagonState *>* OctagonHealthSourceStates(void);
NSSet<OctagonFlag *>* AllOctagonFlags(void);

////// State machine flags
extern OctagonFlag* const OctagonFlagIDMSLevelChanged;

extern OctagonFlag* const OctagonFlagEgoPeerPreapproved;

extern OctagonFlag* const OctagonFlagCKKSRequestsTLKUpload;

// We've received a change notification from cuttlefish; we should probably see what's new
extern OctagonFlag* const OctagonFlagCuttlefishNotification NS_SWIFT_NAME(OctagonFlagCuttlefishNotification);


extern OctagonFlag* const OctagonFlagFetchAuthKitMachineIDList;

extern OctagonFlag* const OctagonFlagAccountIsAvailable;

extern OctagonFlag* const OctagonFlagAttemptSOSUpgrade;
extern OctagonFlag* const OctagonFlagUnlocked;

extern OctagonFlag* const OctagonFlagAttemptSOSUpdatePreapprovals;
extern OctagonFlag* const OctagonFlagAttemptSOSConsistency;

extern OctagonFlag* const OctagonFlagEscrowRequestInformCloudServicesOperation;


NS_ASSUME_NONNULL_END

#endif // OCTAGON
