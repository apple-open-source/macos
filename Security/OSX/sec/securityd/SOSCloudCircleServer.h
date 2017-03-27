/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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


#ifndef _SECURITY_SOSCLOUDCIRCLESERVER_H_
#define _SECURITY_SOSCLOUDCIRCLESERVER_H_

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSAccount.h>


__BEGIN_DECLS

//
// MARK: Server versions of our SPI
//
bool SOSCCTryUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);
bool SOSCCSetUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);
bool SOSCCSetUserCredentialsAndDSID_Server(CFStringRef user_label, CFDataRef user_password, CFStringRef dsid, CFErrorRef *error);

bool SOSCCCanAuthenticate_Server(CFErrorRef *error);
bool SOSCCPurgeUserCredentials_Server(CFErrorRef *error);

SOSCCStatus SOSCCThisDeviceIsInCircle_Server(CFErrorRef *error);
bool SOSCCRequestToJoinCircle_Server(CFErrorRef* error);
bool SOSCCRequestToJoinCircleAfterRestore_Server(CFErrorRef* error);
CFStringRef SOSCCCopyDeviceID_Server(CFErrorRef *error);
bool SOSCCSetDeviceID_Server(CFStringRef IDS, CFErrorRef *error);
HandleIDSMessageReason SOSCCHandleIDSMessage_Server(CFDictionaryRef messageDict, CFErrorRef* error);
bool SOSCCRequestSyncWithPeerOverKVS_Server(CFStringRef peerID, CFDataRef message, CFErrorRef *error);
bool SOSCCClearPeerMessageKeyInKVS_Server(CFStringRef peerID, CFErrorRef *error);

bool SOSCCIDSServiceRegistrationTest_Server(CFStringRef message, CFErrorRef *error);
bool SOSCCIDSPingTest_Server(CFStringRef message, CFErrorRef *error);
bool SOSCCIDSDeviceIDIsAvailableTest_Server(CFErrorRef *error);

bool SOSCCRemoveThisDeviceFromCircle_Server(CFErrorRef* error);
bool SOSCCRemovePeersFromCircle_Server(CFArrayRef peers, CFErrorRef* error);
bool SOSCCLoggedOutOfAccount_Server(CFErrorRef *error);
bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error);
bool SOSCCRequestEnsureFreshParameters_Server(CFErrorRef* error);


bool SOSCCApplyToARing_Server(CFStringRef ringName, CFErrorRef *error);
bool SOSCCWithdrawlFromARing_Server(CFStringRef ringName, CFErrorRef *error);
SOSRingStatus SOSCCRingStatus_Server(CFStringRef ringName, CFErrorRef *error);
CFStringRef SOSCCGetAllTheRings_Server(CFErrorRef *error);
bool SOSCCEnableRing_Server(CFStringRef ringName, CFErrorRef *error);


CFArrayRef SOSCCCopyGenerationPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyApplicantPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyValidPeerPeerInfo_Server(CFErrorRef* error);
bool SOSCCValidateUserPublic_Server(CFErrorRef* error);

CFArrayRef SOSCCCopyNotValidPeerPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyRetirementPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyViewUnawarePeerInfo_Server(CFErrorRef* error);
bool SOSCCRejectApplicants_Server(CFArrayRef applicants, CFErrorRef* error);
bool SOSCCAcceptApplicants_Server(CFArrayRef applicants, CFErrorRef* error);

SOSPeerInfoRef SOSCCCopyMyPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyEngineState_Server(CFErrorRef* error);

CFArrayRef SOSCCCopyPeerPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyConcurringPeerPeerInfo_Server(CFErrorRef* error);
bool SOSCCCheckPeerAvailability_Server(CFErrorRef *error);
bool SOSCCkSecXPCOpIsThisDeviceLastBackup_Server(CFErrorRef *error);
bool SOSCCkSecXPCOpIsThisDeviceLastBackup_Server(CFErrorRef *error);
bool SOSCCAccountSetToNew_Server(CFErrorRef *error);
bool SOSCCResetToOffering_Server(CFErrorRef* error);
bool SOSCCResetToEmpty_Server(CFErrorRef* error);

CFBooleanRef SOSCCPeersHaveViewsEnabled_Server(CFArrayRef viewNames, CFErrorRef *error);

SOSViewResultCode SOSCCView_Server(CFStringRef view, SOSViewActionCode action, CFErrorRef *error);
bool SOSCCViewSet_Server(CFSetRef enabledView, CFSetRef disabledViews);

SOSSecurityPropertyResultCode SOSCCSecurityProperty_Server(CFStringRef property, SOSSecurityPropertyActionCode action, CFErrorRef *error);

CFStringRef SOSCCCopyIncompatibilityInfo_Server(CFErrorRef* error);
enum DepartureReason SOSCCGetLastDepartureReason_Server(CFErrorRef* error);
bool SOSCCSetLastDepartureReason_Server(enum DepartureReason reason, CFErrorRef *error);
bool SOSCCSetHSA2AutoAcceptInfo_Server(CFDataRef pubKey, CFErrorRef *error);

bool SOSCCProcessEnsurePeerRegistration_Server(CFErrorRef* error);

CF_RETURNS_RETAINED CFSetRef SOSCCProcessSyncWithPeers_Server(CFSetRef peers, CFSetRef backupPeers, CFErrorRef *error);
SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers_Server(CFErrorRef* error);
bool SOSCCRequestSyncWithPeerOverKVSUsingIDOnly_Server(CFStringRef deviceID, CFErrorRef *error);

SOSPeerInfoRef SOSCCSetNewPublicBackupKey_Server(CFDataRef newPublicBackup, CFErrorRef *error);
bool SOSCCRegisterSingleRecoverySecret_Server(CFDataRef backupSlice, bool setupV0Only, CFErrorRef *error);

bool SOSCCWaitForInitialSync_Server(CFErrorRef*);
CFArrayRef SOSCCCopyYetToSyncViewsList_Server(CFErrorRef*);

bool SOSWrapToBackupSliceKeyBagForView_Server(CFStringRef viewName, CFDataRef input, CFDataRef* output, CFDataRef* bskbEncoded, CFErrorRef* error);

SOSBackupSliceKeyBagRef SOSBackupSliceKeyBagForView(CFStringRef viewName, CFErrorRef* error);
CF_RETURNS_RETAINED CFDataRef SOSWrapToBackupSliceKeyBag(SOSBackupSliceKeyBagRef bskb, CFDataRef input, CFErrorRef* error);

//
// MARK: Internal kicks.
//
CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateMessage(CFDictionaryRef updates);
void sync_the_last_data_to_kvs(SOSAccountRef account, bool waitForeverForSynchronization);


// Expected to be called when the data source changes.
void SOSCCRequestSyncWithPeer(CFStringRef peerID);
void SOSCCRequestSyncWithPeers(CFSetRef /*SOSPeerInfoRef/CFStringRef*/ peerIDs);
void SOSCCRequestSyncWithPeersList(CFArrayRef /*CFStringRef*/ peerIDs);
void SOSCCRequestSyncWithBackupPeer(CFStringRef backupPeerId);

bool SOSCCIsSyncPendingFor(CFStringRef peerID, CFErrorRef *error);

void SOSCCEnsurePeerRegistration(void);
void SOSCCAddSyncablePeerBlock(CFStringRef ds_name, SOSAccountSyncablePeersBlock changeBlock);
dispatch_queue_t SOSCCGetAccountQueue(void);

//
// MARK: Internal access to local account for tests.
//
typedef SOSDataSourceFactoryRef (^SOSCCAccountDataSourceFactoryBlock)();

SOSAccountRef SOSKeychainAccountGetSharedAccount(void);
bool SOSKeychainAccountSetFactoryForAccount(SOSCCAccountDataSourceFactoryBlock factory);

//
// MARK: Internal SPIs for testing
//
CFStringRef CopyOSVersion(void);
CFDataRef SOSCCCopyAccountState_Server(CFErrorRef* error);
CFDataRef SOSCCCopyEngineData_Server(CFErrorRef* error);
bool SOSCCDeleteEngineState_Server(CFErrorRef* error);
bool SOSCCDeleteAccountState_Server(CFErrorRef* error);


//
// MARK: Testing operations, dangerous to call in normal operation.
//
bool SOSKeychainSaveAccountDataAndPurge(CFErrorRef *error);

//
// MARK: Constants for where we store persistent information in the keychain
//

extern CFStringRef kSOSInternalAccessGroup;

extern CFStringRef kSOSAccountLabel;
extern CFStringRef kSOSPeerDataLabel;

CFDataRef SOSItemCopy(CFStringRef label, CFErrorRef* error);
bool SOSItemUpdateOrAdd(CFStringRef label, CFStringRef accessibility, CFDataRef data, CFErrorRef *error);

bool SOSCCSetEscrowRecord_Server(CFStringRef escrow_label, uint64_t tries, CFErrorRef *error);
CFDictionaryRef SOSCCCopyEscrowRecord_Server(CFErrorRef *error);
bool SOSCCRegisterRecoveryPublicKey_Server(CFDataRef recovery_key, CFErrorRef *error);
CFDataRef SOSCCCopyRecoveryPublicKey_Server(CFErrorRef *error);

CFDictionaryRef SOSCCCopyBackupInformation_Server(CFErrorRef *error);

SOSPeerInfoRef SOSCCCopyApplication_Server(CFErrorRef *error);
CFDataRef SOSCCCopyCircleJoiningBlob_Server(SOSPeerInfoRef applicant, CFErrorRef *error);
bool SOSCCJoinWithCircleJoiningBlob_Server(CFDataRef joiningBlob, CFErrorRef *error);

bool SOSCCAccountHasPublicKey_Server(CFErrorRef *error);
bool SOSCCAccountIsNew_Server(CFErrorRef *error);

bool SOSCCMessageFromPeerIsPending_Server(SOSPeerInfoRef peer, CFErrorRef *error);
bool SOSCCSendToPeerIsPending_Server(SOSPeerInfoRef peer, CFErrorRef *error);

__END_DECLS

#endif
