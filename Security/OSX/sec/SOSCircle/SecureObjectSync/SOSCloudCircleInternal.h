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


#ifndef _SECURITY_SOSCLOUDCIRCLEINTERNAL_H_
#define _SECURITY_SOSCLOUDCIRCLEINTERNAL_H_

#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSPeerInfo.h>

#include <xpc/xpc.h>
#include <Security/SecKey.h>
//#include <Security/SecureObjectSync/SOSRing.h>    // only included for SOSRingStatus, see rdar://problem/20831215 before uncommenting

__BEGIN_DECLS

// Use the kSecAttrViewHint* constants in SecItemPriv.h instead

extern const CFStringRef kSOSViewHintPCSMasterKey DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSiCloudDrive DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSPhotos DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSCloudKit DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSEscrow DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSFDE DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSMailDrop DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSiCloudBackup DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSNotes DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintPCSiMessage DEPRECATED_ATTRIBUTE;

extern const CFStringRef kSOSViewHintAppleTV DEPRECATED_ATTRIBUTE;
extern const CFStringRef kSOSViewHintHomeKit DEPRECATED_ATTRIBUTE;

CFArrayRef SOSCCCopyConcurringPeerPeerInfo(CFErrorRef* error);

bool SOSCCPurgeUserCredentials(CFErrorRef* error);

CFStringRef SOSCCGetStatusDescription(SOSCCStatus status);
bool SOSCCAccountHasPublicKey(CFErrorRef *error);
bool SOSCCAccountIsNew(CFErrorRef *error);

/*!
 @function SOSCCHandleIDSMessage
 @abstract Handles an IDS message from KeychainSyncingOverIDSProxy
 @param IDS The incoming IDS message
 @param error What went wrong if we returned false
 */
HandleIDSMessageReason SOSCCHandleIDSMessage(CFDictionaryRef IDS, CFErrorRef* error);

/*!
 @function SOSCCProcessSyncWithPeers
 @abstract Returns the peers for whom we handled syncing from the list send to us.
 @param peers Set of peerIDs to sync with
 @param backupPeers Set of backup peerIDs to sync with
 */
CFSetRef /* CFString */ SOSCCProcessSyncWithPeers(CFSetRef peers, CFSetRef backupPeers, CFErrorRef* error);

/*!
 @function SOSCCProcessSyncWithAllPeers
 @abstract Returns the information (string, hopefully URL) that will lead to an explanation of why you have an incompatible circle.
 @param error What went wrong if we returned NULL.
 */
SyncWithAllPeersReason SOSCCProcessSyncWithAllPeers(CFErrorRef* error);

bool SOSCCProcessEnsurePeerRegistration(CFErrorRef* error);

//Rings
CFStringRef SOSCCGetAllTheRings(CFErrorRef *error);

bool SOSCCApplyToARing(CFStringRef ringName, CFErrorRef* error);
bool SOSCCWithdrawlFromARing(CFStringRef ringName, CFErrorRef* error);
int SOSCCRingStatus(CFStringRef ringName, CFErrorRef* error);   // TODO: this returns SOSRingStatus
bool SOSCCEnableRing(CFStringRef ringName, CFErrorRef* error);



/*!
 @function SOSCCCopyMyPeerInfo
 @abstract Returns a copy of my peer info
 @param error What went wrong if we returned NULL
 */
SOSPeerInfoRef SOSCCCopyMyPeerInfo(CFErrorRef *error);

/*!
 @function SOSCCIDSServiceRegistrationTest
 @abstract Attempts to send a message over IDS to test IDS service set up
 @param message The message to send over IDS
 @param error What went wrong if we returned false
 */
bool SOSCCIDSServiceRegistrationTest(CFStringRef message, CFErrorRef *error);

/*!
 @function SOSCCIDSPingTest
 @abstract Attempts to ping devices within an IDS Account, check device availability
 @param message The message to send over IDS
 @param error What went wrong if we returned false
 */
bool SOSCCIDSPingTest(CFStringRef message, CFErrorRef *error);

/*!
 @function SOSCCIDSDeviceIDIsAvailableTest
 @abstract Attempts to communicate with KeychainSyncingOverIDSProxy to retrieve the device ID using IDS framework
 @param error What went wrong if we returned false
 */
bool SOSCCIDSDeviceIDIsAvailableTest(CFErrorRef *error);

/*!
 @function SOSWrapToBackupSliceKeyBagForView
 @abstract Encrypts the given plaintext, and wraps the encryption key to the backup slice keybag for this view
 @param viewName The view to wrap to
 @param input The plaintext to encrypt
 @param output The ciphertext
 @param bskbEncoded The encoded backup slice keybag used to wrap the data
 @param error What went wrong if we returned false
 */
bool SOSWrapToBackupSliceKeyBagForView(CFStringRef viewName, CFDataRef input, CFDataRef* output, CFDataRef* bskbEncoded, CFErrorRef* error);

//
// Security Tool calls
//
CFDataRef SOSCCCopyAccountState(CFErrorRef* error);
bool SOSCCDeleteAccountState(CFErrorRef *error);
CFDataRef SOSCCCopyEngineData(CFErrorRef* error);
bool SOSCCDeleteEngineState(CFErrorRef *error);
bool SOSCCRequestSyncWithPeerOverKVS( CFStringRef peerID, CFDataRef message, CFErrorRef *error);
bool SOSCCClearPeerMessageKeyInKVS(CFStringRef peerID, CFErrorRef *error);
CFDataRef SOSCCCopyRecoveryPublicKey(CFErrorRef *error);
CFDictionaryRef SOSCCCopyBackupInformation(CFErrorRef *error);
bool SOSCCRequestSyncWithPeerOverKVSUsingIDOnly(CFStringRef peerID, CFErrorRef *error);

char *SOSCCSysdiagnose(const char *directoryname);

void SOSCCForEachEngineStateAsStringFromArray(CFArrayRef states, void (^block)(CFStringRef oneStateString));

__END_DECLS

#endif
