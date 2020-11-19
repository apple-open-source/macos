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

#import <Foundation/Foundation.h>
#import <os/activity.h>
#import <os/log.h>
#import <os/signpost.h>
#import <os/signpost_private.h>

NS_ASSUME_NONNULL_BEGIN

typedef struct octagon_signpost_s {
    const os_signpost_id_t identifier;
    const uint64_t timestamp;
} OctagonSignpost;

#define OctagonSignpostNamePerformEscrowRecovery                        "OctagonSignpostNamePerformEscrowRecovery"
#define OctagonSignpostNamePerformSilentEscrowRecovery                  "OctagonSignpostNamePerformSilentEscrowRecovery"
#define OctagonSignpostNamePerformRecoveryFromSBD                       "OctagonSignpostNamePerformRecoveryFromSBD"

#define OctagonSignpostNameRecoverWithCDPContext                        "OctagonSignpostNameRecoverWithCDPContext"
#define OctagonSignpostNameRecoverSilentWithCDPContext                  "OctagonSignpostNameRecoverSilentWithCDPContext"
#define OctagonSignpostNamePerformOctagonJoinForSilent                  "OctagonSignpostNamePerformOctagonJoinForSilent"
#define OctagonSignpostNamePerformOctagonJoinForNonSilent               "OctagonSignpostNamePerformOctagonJoinForNonSilent"


#define OctagonSignpostNamePerformOctagonJoin                           "OctagonSignpostNamePerformOctagonJoin"
#define OctagonSignpostNamePerformResetAndEstablishAfterFailedBottle    "OctagonSignpostNamePerformResetAndEstablishAfterFailedBottle"
#define OctagonSignpostNameFetchEgoPeer                                 "OctagonSignpostNameFetchEgoPeer"
#define OctagonSignpostNameEstablish                                    "OctagonSignpostNameEstablish"
#define OctagonSignpostNameResetAndEstablish                            "OctagonSignpostNameResetAndEstablish"
#define OctagonSignpostNameMakeNewFriends                               "OctagonSignpostNameMakeNewFriends"
#define OctagonSignpostNameFetchCliqueStatus                            "OctagonSignpostNameFetchCliqueStatus"
#define OctagonSignpostNameRemoveFriendsInClique                        "OctagonSignpostNameRemoveFriendsInClique"
#define OctagonSignpostNameLeaveClique                                  "OctagonSignpostNameLeaveClique"
#define OctagonSignpostNamePeerDeviceNamesByPeerID                      "OctagonSignpostNamePeerDeviceNamesByPeerID"
#define OctagonSignpostNameJoinAfterRestore                             "OctagonSignpostNameJoinAfterRestore"
#define OctagonSignpostNameSafariPasswordSyncingEnabled                 "OctagonSignpostNameSafariPasswordSyncingEnabled"
#define OctagonSignpostNameWaitForInitialSync                           "OctagonSignpostNameWaitForInitialSync"
#define OctagonSignpostNameCopyViewUnawarePeerInfo                      "OctagonSignpostNameCopyViewUnawarePeerInfo"
#define OctagonSignpostNameViewSet                                      "OctagonSignpostNameViewSet"
#define OctagonSignpostNameSetUserCredentialsAndDSID                    "OctagonSignpostNameSetUserCredentialsAndDSID"
#define OctagonSignpostNameTryUserCredentialsAndDSID                    "OctagonSignpostNameTryUserCredentialsAndDSID"
#define OctagonSignpostNameCopyPeerPeerInfo                             "OctagonSignpostNameCopyPeerPeerInfo"
#define OctagonSignpostNamePeersHaveViewsEnabled                        "OctagonSignpostNamePeersHaveViewsEnabled"
#define OctagonSignpostNameRequestToJoinCircle                          "OctagonSignpostNameRequestToJoinCircle"
#define OctagonSignpostNameAccountUserKeyAvailable                      "OctagonSignpostNameAccountUserKeyAvailable"
#define OctagonSignpostNameFindOptimalBottleIDsWithContextData          "OctagonSignpostNameFindOptimalBottleIDsWithContextData"
#define OctagonSignpostNameFetchEscrowRecords                           "OctagonSignpostNameFetchEscrowRecords"
#define OctagonSignpostNameFetchEscrowContents                          "OctagonSignpostNameFetchEscrowContents"
#define OctagonSignpostNameSetNewRecoveryKeyWithData                    "OctagonSignpostNameSetNewRecoveryKeyWithData"
#define OctagonSignpostNameRecoverOctagonUsingData                      "OctagonSignpostNameRecoverOctagonUsingData"
#define OctagonSignpostNamePerformedCDPStateMachineRun                  "OctagonSignpostNamePerformedCDPStateMachineRun"
#define OctagonSignpostNameWaitForOctagonUpgrade                        "OctagonSignpostNameWaitForOctagonUpgrade"
#define OctagonSignpostNameGetAccountInfo                               "OctagonSignpostNameGetAccountInfo"

#define SOSSignpostNameAssertUserCredentialsAndOptionalDSID     "SOSSignpostNameAssertUserCredentialsAndOptionalDSID"
#define SOSSignpostNameSOSCCTryUserCredentials                  "SOSSignpostNameSOSCCTryUserCredentials"
#define SOSSignpostNameSOSCCCanAuthenticate                     "SOSSignpostNameSOSCCCanAuthenticate"
#define SOSSignpostNameSOSCCRequestToJoinCircle                 "SOSSignpostNameSOSCCRequestToJoinCircle"
#define SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore     "SOSSignpostNameSOSCCRequestToJoinCircleAfterRestore"
#define SOSSignpostNameSOSCCResetToOffering                     "SOSSignpostNameSOSCCResetToOffering"
#define SOSSignpostNameSOSCCResetToEmpty                        "SOSSignpostNameSOSCCResetToEmpty"
#define SOSSignpostNameSOSCCRemoveThisDeviceFromCircle          "SOSSignpostNameSOSCCRemoveThisDeviceFromCircle"
#define SOSSignpostNameSOSCCRemovePeersFromCircle               "SOSSignpostNameSOSCCRemovePeersFromCircle"
#define SOSSignpostNameSOSCCLoggedOutOfAccount                  "SOSSignpostNameSOSCCLoggedOutOfAccount"
#define SOSSignpostNameSOSCCCopyApplicantPeerInfo               "SOSSignpostNameSOSCCCopyApplicantPeerInfo"
#define SOSSignpostNameFlush                                    "SOSSignpostNameFlush"
#define SOSSignpostNameSyncKVSAndWait                           "SOSSignpostNameSyncKVSAndWait"
#define SOSSignpostNameSyncTheLastDataToKVS                     "SOSSignpostNameSyncTheLastDataToKVS"
#define SOSSignpostNameSOSCCViewSet                             "SOSSignpostNameSOSCCViewSet"
#define SOSSignpostNameSOSCCCopyValidPeerPeerInfo               "SOSSignpostNameSOSCCCopyValidPeerPeerInfo"
#define SOSSignpostNameSOSCCValidateUserPublic                  "SOSSignpostNameSOSCCValidateUserPublic"
#define SOSSignpostNameSOSCCCopyViewUnawarePeerInfo             "SOSSignpostNameSOSCCCopyViewUnawarePeerInfo"
#define SOSSignpostNameSOSCCWaitForInitialSync                  "SOSSignpostNameSOSCCWaitForInitialSync"
#define SOSSignpostNameSOSCCAcceptApplicants                    "SOSSignpostNameSOSCCAcceptApplicants"
#define SOSSignpostNameSOSCCRejectApplicants                    "SOSSignpostNameSOSCCRejectApplicants"
#define SOSSignpostNameSOSCCCopyConcurringPeerPeerInfo          "SOSSignpostNameSOSCCCopyConcurringPeerPeerInfo"
#define SOSSignpostNameSOSCCCopyMyPeerInfo                      "SOSSignpostNameSOSCCCopyMyPeerInfo"
#define SOSSignpostNameSOSCCSetNewPublicBackupKey               "SOSSignpostNameSOSCCSetNewPublicBackupKey"
#define SOSSignpostNameSOSCCRegisterSingleRecoverySecret        "SOSSignpostNameSOSCCRegisterSingleRecoverySecret"
#define SOSSignpostNameSOSCCProcessEnsurePeerRegistration       "SOSSignpostNameSOSCCProcessEnsurePeerRegistration"
#define SOSSignpostNameSOSCCProcessSyncWithPeers                "SOSSignpostNameSOSCCProcessSyncWithPeers"
#define SOSSignpostNameSOSCCProcessSyncWithAllPeers             "SOSSignpostNameSOSCCProcessSyncWithAllPeers"
#define SOSSignpostNameSOSCCRequestSyncWithPeersList            "SOSSignpostNameSOSCCRequestSyncWithPeersList"
#define SOSSignpostNameSOSCCRequestSyncWithBackupPeerList       "SOSSignpostNameSOSCCRequestSyncWithBackupPeerList"
#define SOSSignpostNameSOSCCEnsurePeerRegistration              "SOSSignpostNameSOSCCEnsurePeerRegistration"
#define SOSSignpostNameSOSCCHandleUpdateMessage                 "SOSSignpostNameSOSCCHandleUpdateMessage"
#define SOSSignpostNameSOSCCCopyApplication                     "SOSSignpostNameSOSCCCopyApplication"
#define SOSSignpostNameSOSCCCopyCircleJoiningBlob               "SOSSignpostNameSOSCCCopyCircleJoiningBlob"
#define SOSSignpostNameSOSCCCopyInitialSyncData                 "SOSSignpostNameSOSCCCopyInitialSyncData"
#define SOSSignpostNameSOSCCJoinWithCircleJoiningBlob           "SOSSignpostNameSOSCCJoinWithCircleJoiningBlob"
#define SOSSignpostNameSOSCCPeersHaveViewsEnabled               "SOSSignpostNameSOSCCPeersHaveViewsEnabled"
#define SOSSignpostNameSOSCCRegisterRecoveryPublicKey           "SOSSignpostNameSOSCCRegisterRecoveryPublicKey"
#define SOSSignpostNameSOSCCCopyRecoveryPublicKey               "SOSSignpostNameSOSCCCopyRecoveryPublicKey"
#define SOSSignpostNameSOSCCMessageFromPeerIsPending            "SOSSignpostNameSOSCCMessageFromPeerIsPending"
#define SOSSignpostNameSOSCCSendToPeerIsPending                 "SOSSignpostNameSOSCCSendToPeerIsPending"

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

NS_ASSUME_NONNULL_END

#endif /* OctagonSignPosts_h */
