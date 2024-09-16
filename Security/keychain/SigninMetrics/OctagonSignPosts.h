/*
* Copyright (c) 2019 Apple Inc. All Rights Reserved.
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

#ifndef OctagonSignPosts_h
#define OctagonSignPosts_h

#import <os/activity.h>
#import <os/base.h>
#import <os/log.h>
#import <os/signpost.h>
#import <os/signpost_private.h>

OS_ASSUME_NONNULL_BEGIN

typedef struct octagon_signpost_s {
    const os_signpost_id_t identifier;
    const uint64_t timestamp;
} OctagonSignpost;

#define OctagonSignpostNamePerformEscrowRecovery                            "PerformEscrowRecovery"
#define OctagonSignpostNamePerformSilentEscrowRecovery                      "PerformSilentEscrowRecovery"
#define OctagonSignpostNamePerformRecoveryFromSBD                           "PerformRecoveryFromSBD"

#define OctagonSignpostNameRecoverWithCDPContext                            "RecoverWithCDPContext"
#define OctagonSignpostNameRecoverSilentWithCDPContext                      "RecoverSilentWithCDPContext"
#define OctagonSignpostNamePerformOctagonJoinForSilent                      "PerformOctagonJoinForSilent"
#define OctagonSignpostNamePerformOctagonJoinForNonSilent                   "PerformOctagonJoinForNonSilent"


#define OctagonSignpostNamePerformOctagonJoin                               "PerformOctagonJoin"
#define OctagonSignpostNamePerformResetAndEstablishAfterFailedBottle        "PerformResetAndEstablishAfterFailedBottle"
#define OctagonSignpostNameFetchEgoPeer                                     "FetchEgoPeer"
#define OctagonSignpostNameEstablish                                        "Establish"
#define OctagonSignpostNameResetAndEstablish                                "ResetAndEstablish"
#define OctagonSignpostNameMakeNewFriends                                   "MakeNewFriends"
#define OctagonSignpostNameFetchCliqueStatus                                "FetchCliqueStatus"
#define OctagonSignpostNameRemoveFriendsInClique                            "RemoveFriendsInClique"
#define OctagonSignpostNameLeaveClique                                      "LeaveClique"
#define OctagonSignpostNamePeerDeviceNamesByPeerID                          "PeerDeviceNamesByPeerID"
#define OctagonSignpostNameJoinAfterRestore                                 "JoinAfterRestore"
#define OctagonSignpostNameSafariPasswordSyncingEnabled                     "SafariPasswordSyncingEnabled"
#define OctagonSignpostNameWaitForInitialSync                               "WaitForInitialSync"
#define OctagonSignpostNameCopyViewUnawarePeerInfo                          "CopyViewUnawarePeerInfo"
#define OctagonSignpostNameViewSet                                          "ViewSet"
#define OctagonSignpostNameSetUserCredentialsAndDSID                        "SetUserCredentialsAndDSID"
#define OctagonSignpostNameTryUserCredentialsAndDSID                        "TryUserCredentialsAndDSID"
#define OctagonSignpostNameCopyPeerPeerInfo                                 "CopyPeerPeerInfo"
#define OctagonSignpostNamePeersHaveViewsEnabled                            "PeersHaveViewsEnabled"
#define OctagonSignpostNameRequestToJoinCircle                              "RequestToJoinCircle"
#define OctagonSignpostNameAccountUserKeyAvailable                          "AccountUserKeyAvailable"
#define OctagonSignpostNameFindOptimalBottleIDsWithContextData              "FindOptimalBottleIDsWithContextData"
#define OctagonSignpostNameFetchEscrowRecords                               "FetchEscrowRecords"
#define OctagonSignpostNameFetchEscrowContents                              "FetchEscrowContents"
#define OctagonSignpostNameSetNewRecoveryKeyWithData                        "SetNewRecoveryKeyWithData"
#define OctagonSignpostNameRecoverOctagonUsingCustodianRecoveryKey          "RecoverOctagonUsingCustodianRecoveryKey"
#define OctagonSignpostNamePreflightRecoverOctagonUsingCustodianRecoveryKey "PreflightRecoverOctagonUsingCustodianRecoveryKey"
#define OctagonSignpostNameRecoverOctagonUsingInheritanceKey                "RecoverOctagonUsingInheritanceKey"
#define OctagonSignpostNamePreflightRecoverOctagonUsingInheritanceKey       "PreflightRecoverOctagonUsingInheritanceKey"
#define OctagonSignpostNameCreateCustodianRecoveryKey                       "CreateCustodianRecoveryKey"
#define OctagonSignpostNameCreateInheritanceKey                             "CreateInheritanceKey"
#define OctagonSignpostNameGenerateInheritanceKey                           "GenerateInheritanceKey"
#define OctagonSignpostNameStoreInheritanceKey                              "StoreInheritanceKey"
#define OctagonSignpostNameRemoveCustodianRecoveryKey                       "RemoveCustodianRecoveryKey"
#define OctagonSignpostNameCheckCustodianRecoveryKey                        "CheckCustodianRecoveryKey"
#define OctagonSignpostNameRemoveInheritanceKey                             "RemoveInheritanceKey"
#define OctagonSignpostNameCheckInheritanceKey                              "CheckInheritanceKey"
#define OctagonSignpostNameRecreateInheritanceKey                           "RecreateInheritanceKey"
#define OctagonSignpostNameCreateInheritanceKeyWithClaimTokenAndWrappingKey "CreateInheritanceKeyWithClaimTokenAndWrappingKey"
#define OctagonSignpostNamePerformedCDPStateMachineRun                      "PerformedCDPStateMachineRun"
#define OctagonSignpostNameWaitForOctagonUpgrade                            "WaitForOctagonUpgrade"

// Pairing Channel SignPosts
#define OctagonSignpostNamePairingChannelAcceptorMessage1                   "PairingChannelAcceptorMessage1"
#define OctagonSignpostNamePairingChannelAcceptorMessage2                   "PairingChannelAcceptorMessage2"
#define OctagonSignpostNamePairingChannelAcceptorMessage3                   "PairingChannelAcceptorMessage3"
#define OctagonSignpostNamePairingChannelAcceptorEpoch                      "PairingChannelAcceptorEpoch"
#define OctagonSignpostNamePairingChannelAcceptorVoucher                    "PairingChannelAcceptorVoucher"
#define OctagonSignpostNamePairingChannelAcceptorCircleJoiningBlob          "PairingChannelAcceptorCircleJoiningBlob"
#define OctagonSignpostNamePairingChannelAcceptorFetchStashCredential       "PairingChannelAcceptorFetchStashCredential"

#define OctagonSignpostNamePairingChannelInitiatorMessage1                  "PairingChannelInitiatorMessage1"
#define OctagonSignpostNamePairingChannelInitiatorMessage2                  "PairingChannelInitiatorMessage2"
#define OctagonSignpostNamePairingChannelInitiatorMessage3                  "PairingChannelInitiatorMessage3"
#define OctagonSignpostNamePairingChannelInitiatorMessage4                  "PairingChannelInitiatorMessage4"
#define OctagonSignpostNamePairingChannelInitiatorPrepare                   "PairingChannelInitiatorPrepare"
#define OctagonSignpostNamePairingChannelInitiatorJoinOctagon               "PairingChannelInitiatorJoinOctagon"
#define OctagonSignpostNamePairingChannelInitiatorStashAccountCredential    "PairingChannelInitiatorStashAccountCredential"
#define OctagonSignpostNamePairingChannelInitiatorMakeSOSPeer               "PairingChannelInitiatorMakeSOSPeer"
#define OctagonSignpostNamePairingChannelInitiatorJoinSOS                   "PairingChannelInitiatorJoinSOS"

#define SOSSignpostNameAssertUserCredentialsAndOptionalDSID     "AssertUserCredentialsAndOptionalDSID"
#define SOSSignpostNameSOSCCTryUserCredentials                  "SOSCCTryUserCredentials"
#define SOSSignpostNameSOSCCCanAuthenticate                     "SOSCCCanAuthenticate"
#define SOSSignpostNameSOSCCRequestToJoinCircle                 "SOSCCRequestToJoinCircle"
#define SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore     "SOSCCRequestToJoinCircleAfterRestore"
#define SOSSignpostNameSOSCCResetToOffering                     "SOSCCResetToOffering"
#define SOSSignpostNameSOSCCResetToEmpty                        "SOSCCResetToEmpty"
#define SOSSignpostNameSOSCCRemoveThisDeviceFromCircle          "SOSCCRemoveThisDeviceFromCircle"
#define SOSSignpostNameSOSCCRemovePeersFromCircle               "SOSCCRemovePeersFromCircle"
#define SOSSignpostNameSOSCCLoggedOutOfAccount                  "SOSCCLoggedOutOfAccount"
#define SOSSignpostNameSOSCCCopyApplicantPeerInfo               "SOSCCCopyApplicantPeerInfo"
#define SOSSignpostNameFlush                                    "Flush"
#define SOSSignpostNameSyncKVSAndWait                           "SyncKVSAndWait"
#define SOSSignpostNameSyncTheLastDataToKVS                     "SyncTheLastDataToKVS"
#define SOSSignpostNameSOSCCViewSet                             "SOSCCViewSet"
#define SOSSignpostNameSOSCCCopyValidPeerPeerInfo               "SOSCCCopyValidPeerPeerInfo"
#define SOSSignpostNameSOSCCValidateUserPublic                  "SOSCCValidateUserPublic"
#define SOSSignpostNameSOSCCCopyViewUnawarePeerInfo             "SOSCCCopyViewUnawarePeerInfo"
#define SOSSignpostNameSOSCCWaitForInitialSync                  "SOSCCWaitForInitialSync"
#define SOSSignpostNameSOSCCAcceptApplicants                    "SOSCCAcceptApplicants"
#define SOSSignpostNameSOSCCRejectApplicants                    "SOSCCRejectApplicants"
#define SOSSignpostNameSOSCCCopyConcurringPeerPeerInfo          "SOSCCCopyConcurringPeerPeerInfo"
#define SOSSignpostNameSOSCCCopyMyPeerInfo                      "SOSCCCopyMyPeerInfo"
#define SOSSignpostNameSOSCCSetNewPublicBackupKey               "SOSCCSetNewPublicBackupKey"
#define SOSSignpostNameSOSCCRegisterSingleRecoverySecret        "SOSCCRegisterSingleRecoverySecret"
#define SOSSignpostNameSOSCCProcessEnsurePeerRegistration       "SOSCCProcessEnsurePeerRegistration"
#define SOSSignpostNameSOSCCProcessSyncWithPeers                "SOSCCProcessSyncWithPeers"
#define SOSSignpostNameSOSCCProcessSyncWithAllPeers             "SOSCCProcessSyncWithAllPeers"
#define SOSSignpostNameSOSCCRequestSyncWithPeersList            "SOSCCRequestSyncWithPeersList"
#define SOSSignpostNameSOSCCRequestSyncWithBackupPeerList       "SOSCCRequestSyncWithBackupPeerList"
#define SOSSignpostNameSOSCCEnsurePeerRegistration              "SOSCCEnsurePeerRegistration"
#define SOSSignpostNameSOSCCHandleUpdateMessage                 "SOSCCHandleUpdateMessage"
#define SOSSignpostNameSOSCCCopyApplication                     "SOSCCCopyApplication"
#define SOSSignpostNameSOSCCCopyCircleJoiningBlob               "SOSCCCopyCircleJoiningBlob"
#define SOSSignpostNameSOSCCCopyInitialSyncData                 "SOSCCCopyInitialSyncData"
#define SOSSignpostNameSOSCCJoinWithCircleJoiningBlob           "SOSCCJoinWithCircleJoiningBlob"
#define SOSSignpostNameSOSCCPeersHaveViewsEnabled               "SOSCCPeersHaveViewsEnabled"
#define SOSSignpostNameSOSCCRegisterRecoveryPublicKey           "SOSCCRegisterRecoveryPublicKey"
#define SOSSignpostNameSOSCCCopyRecoveryPublicKey               "SOSCCCopyRecoveryPublicKey"
#define SOSSignpostNameSOSCCMessageFromPeerIsPending            "SOSCCMessageFromPeerIsPending"
#define SOSSignpostNameSOSCCSendToPeerIsPending                 "SOSCCSendToPeerIsPending"
#define SOSSignpostNameSOSCCSetCompatibilityMode                "SOSCCSetCompatibilityMode"
#define SOSSignpostNameSOSCCFetchCompatibilityMode              "SOSCCFetchCompatibilityMode"

#define OctagonSignpostString1(label) " "#label"=%{public,signpost.telemetry:string1,name="#label"}@ "
#define OctagonSignpostString2(label) " "#label"=%{public,signpost.telemetry:string2,name="#label"}@ "
#define OctagonSignpostNumber1(label) " "#label"=%{public,signpost.telemetry:number1,name="#label"}d "
#define OctagonSignpostNumber2(label) " "#label"=%{public,signpost.telemetry:number2,name="#label"}d "

// Used to begin tracking a timed event.
#define OctagonSignpostBegin(name) _OctagonSignpostBegin(_OctagonSignpostLogSystem(), name, OS_SIGNPOST_ENABLE_TELEMETRY)

// For marking a significant event.
#define OctagonSignpostEvent(signpost, name, ...) _OctagonSignpostEvent(_OctagonSignpostLogSystem(), signpost, name, __VA_ARGS__)

// For completing a timed event associated with the given signpost.
#define OctagonSignpostEnd(signpost, name, ...) _OctagonSignpostEnd(_OctagonSignpostLogSystem(), signpost, name, __VA_ARGS__)

extern os_log_t _OctagonSignpostLogSystem(void);

extern OctagonSignpost _OctagonSignpostCreate(os_log_t subsystem);
extern uint64_t _OctagonSignpostGetNanoseconds(OctagonSignpost signpost);

#define _OctagonSignpostBegin(subsystem, name, ...) __extension__({ \
    OctagonSignpost internalSignpost = _OctagonSignpostCreate(subsystem); \
    os_signpost_interval_begin(subsystem, internalSignpost.identifier, name, __VA_ARGS__); \
    os_log(subsystem, "BEGIN [%lld]: " name " " _OctagonSwizzle1(internalSignpost.identifier, __VA_ARGS__)); \
    internalSignpost; \
})

#define _OctagonSignpostEvent(subsystem, signpost, name, ...) __extension__({ \
    double interval = ((double)_OctagonSignpostGetNanoseconds(_signpost) / NSEC_PER_SEC); \
    os_signpost_event_emit(subsystem, signpost.identifier, name, __VA_ARGS__); \
    os_log(subsystem, "EVENT [%lld] %fs: " name " " _OctagonSwizzle2(signpost.identifier, interval, __VA_ARGS__)); \
})

#define _OctagonSignpostEnd(subsystem, signpost, name, ...) __extension__({ \
    double interval = ((double)_OctagonSignpostGetNanoseconds(signpost) / NSEC_PER_SEC); \
    os_signpost_interval_end(subsystem, signpost.identifier, name, __VA_ARGS__); \
    os_log(subsystem, "END [%lld] %fs: " name " " _OctagonSwizzle2(signpost.identifier, interval, __VA_ARGS__)); \
})

#define _OctagonSwizzle1(x, a, ...) a, x, ##__VA_ARGS__
#define _OctagonSwizzle2(x, y, a, ...) a, x, y, ##__VA_ARGS__

OS_ASSUME_NONNULL_END

#endif /* OctagonSignPosts_h */
