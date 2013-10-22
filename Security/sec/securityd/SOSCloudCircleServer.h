//
//  SOSCloudCircleServer.h
//  sec
//
//  Created by Mitch Adler on 11/15/12.
//
//

#ifndef _SECURITY_SOSCLOUDCIRCLESERVER_H_
#define _SECURITY_SOSCLOUDCIRCLESERVER_H_

#include <SecureObjectSync/SOSCloudCircle.h>
#include <SecureObjectSync/SOSAccount.h>

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
bool SOSCCRemoveThisDeviceFromCircle_Server(CFErrorRef* error);
bool SOSCCBailFromCircle_Server(uint64_t limit_in_seconds, CFErrorRef* error);

CFArrayRef SOSCCCopyApplicantPeerInfo_Server(CFErrorRef* error);
bool SOSCCRejectApplicants_Server(CFArrayRef applicants, CFErrorRef* error);
bool SOSCCAcceptApplicants_Server(CFArrayRef applicants, CFErrorRef* error);

CFArrayRef SOSCCCopyPeerPeerInfo_Server(CFErrorRef* error);
CFArrayRef SOSCCCopyConcurringPeerPeerInfo_Server(CFErrorRef* error);

bool SOSCCResetToOffering_Server(CFErrorRef* error);
bool SOSCCResetToEmpty_Server(CFErrorRef* error);

CFStringRef SOSCCCopyIncompatibilityInfo_Server(CFErrorRef* error);
enum DepartureReason SOSCCGetLastDepartureReason_Server(CFErrorRef* error);

SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers_Server(CFErrorRef* error);

//
// MARK: Internal kicks.
//

void SOSCCHandleUpdate(CFDictionaryRef updates);

// Expected to be called when the data source changes.
void SOSCCSyncWithAllPeers(void);

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

CFDataRef SOSItemGet(CFStringRef label, CFErrorRef* error);
bool SOSItemUpdateOrAdd(CFStringRef label, CFStringRef accessibility, CFDataRef data, CFErrorRef *error);

bool SOSCCCircleIsOn_Artifact(void);

#endif
