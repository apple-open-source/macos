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

#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSAccount.h>


__BEGIN_DECLS

//
// MARK: Server versions of our SPI
//
bool SOSCCTryUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);
bool SOSCCSetUserCredentials_Server(CFStringRef user_label, CFDataRef user_password, CFErrorRef *error);
bool SOSCCCanAuthenticate_Server(CFErrorRef *error);
bool SOSCCPurgeUserCredentials_Server(CFErrorRef *error);

SOSCCStatus SOSCCThisDeviceIsInCircle_Server(CFErrorRef *error);
bool SOSCCRequestToJoinCircle_Server(CFErrorRef* error);
bool SOSCCRequestToJoinCircleAfterRestore_Server(CFErrorRef* error);
CFStringRef SOSCCRequestDeviceID_Server(CFErrorRef *error);
bool SOSCCSetDeviceID_Server(CFStringRef IDS, CFErrorRef *error);
bool SOSCCRemoveThisDeviceFromCircle_Server(CFErrorRef* error);
bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error);
bool SOSCCRequestEnsureFreshParameters_Server(CFErrorRef* error);

CFArrayRef SOSCCCopyGenerationPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyApplicantPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyValidPeerPeerInfo_Server(CFErrorRef* error);
bool SOSCCValidateUserPublic_Server(CFErrorRef* error);

CFArrayRef SOSCCCopyNotValidPeerPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyRetirementPeerInfo_Server(CFErrorRef* error);
bool SOSCCRejectApplicants_Server(CFArrayRef applicants, CFErrorRef* error);
bool SOSCCAcceptApplicants_Server(CFArrayRef applicants, CFErrorRef* error);

CFArrayRef SOSCCCopyPeerPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyConcurringPeerPeerInfo_Server(CFErrorRef* error);

bool SOSCCResetToOffering_Server(CFErrorRef* error);
bool SOSCCResetToEmpty_Server(CFErrorRef* error);

CFStringRef SOSCCCopyIncompatibilityInfo_Server(CFErrorRef* error);
enum DepartureReason SOSCCGetLastDepartureReason_Server(CFErrorRef* error);

bool SOSCCProcessEnsurePeerRegistration_Server(CFErrorRef* error);
SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers_Server(CFErrorRef* error);

//
// MARK: Internal kicks.
//
CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateKeyParameter(CFDictionaryRef updates);
CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateCircle(CFDictionaryRef updates);
CF_RETURNS_RETAINED CFArrayRef SOSCCHandleUpdateMessage(CFDictionaryRef updates);


// Expected to be called when the data source changes.
void SOSCCSyncWithAllPeers(void);
void SOSCCAddSyncablePeerBlock(CFStringRef ds_name, SOSAccountSyncablePeersBlock changeBlock);
dispatch_queue_t SOSCCGetAccountQueue(void);


// Internal careful questioning.
bool SOSCCThisDeviceDefinitelyNotActiveInCircle(void);
void SOSCCSetThisDeviceDefinitelyNotActiveInCircle(SOSCCStatus currentStatus);

//
// MARK: Internal access to local account for tests.
//
typedef SOSDataSourceFactoryRef (^SOSCCAccountDataSourceFactoryBlock)();

SOSAccountRef SOSKeychainAccountGetSharedAccount(void);
bool SOSKeychainAccountSetFactoryForAccount(SOSCCAccountDataSourceFactoryBlock factory);

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

bool SOSCCCircleIsOn_Artifact(void);

__END_DECLS

#endif
