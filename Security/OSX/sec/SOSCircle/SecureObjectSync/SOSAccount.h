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


/*!
 @header SOSAccount.h
 The functions provided in SOSCircle.h provide an interface to a
 secure object syncing circle for a single class
 */

#ifndef _SOSACCOUNT_H_
#define _SOSACCOUNT_H_

/* Forward declarations of SOS types. */
typedef struct __OpaqueSOSAccount *SOSAccountRef;
typedef struct __OpaqueSOSRecoveryKeyBag *SOSRecoveryKeyBagRef;

#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecureObjectSync/SOSAccountTransaction.h>
#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSPeerInfoSecurityProperties.h>
#include <Security/SecureObjectSync/SOSRecoveryKeyBag.h>

#include <dispatch/dispatch.h>

__BEGIN_DECLS

#define RETIREMENT_FINALIZATION_SECONDS (24*60*60)


typedef void (^SOSAccountCircleMembershipChangeBlock)(SOSCircleRef new_circle,
                                                      CFSetRef added_peers, CFSetRef removed_peers,
                                                      CFSetRef added_applicants, CFSetRef removed_applicants);
typedef void (^SOSAccountSyncablePeersBlock)(CFArrayRef trustedPeers, CFArrayRef addedPeers, CFArrayRef removedPeers);
typedef bool (^SOSAccountWaitForInitialSyncBlock)(SOSAccountRef account);
typedef void (^SOSAccountSaveBlock)(CFDataRef flattenedAccount, CFErrorRef flattenFailError);

SOSAccountRef SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory);
SOSAccountRef SOSAccountCreateBasic(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory);


CFTypeID SOSAccountGetTypeID(void);

//
// MARK: Persistent Encode decode
//

SOSAccountRef SOSAccountCreateFromDER(CFAllocatorRef allocator, SOSDataSourceFactoryRef factory,
                                      CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end);
SOSAccountRef SOSAccountCreateFromData(CFAllocatorRef allocator, CFDataRef circleData,
                                       SOSDataSourceFactoryRef factory,
                                       CFErrorRef* error);

size_t SOSAccountGetDEREncodedSize(SOSAccountRef cir, CFErrorRef *error);
uint8_t* SOSAccountEncodeToDER(SOSAccountRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);
size_t SOSAccountGetDEREncodedSize_V3(SOSAccountRef cir, CFErrorRef *error);
uint8_t* SOSAccountEncodeToDER_V3(SOSAccountRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);
CFDataRef SOSAccountCopyEncodedData(SOSAccountRef circle, CFAllocatorRef allocator, CFErrorRef *error);
//
//MARK: IDS Device ID
CFStringRef SOSAccountCopyDeviceID(SOSAccountRef account, CFErrorRef *error);
bool SOSAccountSetMyDSID(SOSAccountTransactionRef txn, CFStringRef IDS, CFErrorRef* errror);
bool SOSAccountSendIDSTestMessage(SOSAccountRef account, CFStringRef message, CFErrorRef *error);
bool SOSAccountStartPingTest(SOSAccountRef account, CFStringRef message, CFErrorRef *error);
bool SOSAccountRetrieveDeviceIDFromKeychainSyncingOverIDSProxy(SOSAccountRef account, CFErrorRef *error);

//
// MARK: Credential management
//

SecKeyRef SOSAccountGetTrustedPublicCredential(SOSAccountRef account, CFErrorRef* error);

SecKeyRef SOSAccountGetPrivateCredential(SOSAccountRef account, CFErrorRef* error);
CFDataRef SOSAccountGetCachedPassword(SOSAccountRef account, CFErrorRef* error);

void SOSAccountSetParameters(SOSAccountRef account, CFDataRef parameters);

void SOSAccountPurgePrivateCredential(SOSAccountRef account);

bool SOSAccountTryUserCredentials(SOSAccountRef account,
                                  CFStringRef user_account, CFDataRef user_password,
                                  CFErrorRef *error);

bool SOSAccountAssertUserCredentials(SOSAccountRef account,
                                     CFStringRef user_account, CFDataRef user_password,
                                     CFErrorRef *error);

bool SOSAccountRetryUserCredentials(SOSAccountRef account);
void SOSAccountSetUnTrustedUserPublicKey(SOSAccountRef account, SecKeyRef publicKey);

bool SOSAccountGenerationSignatureUpdate(SOSAccountRef account, CFErrorRef *error);

//
// MARK: Circle management
//

bool SOSAccountUpdateCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error);
void SOSTransportEachMessage(SOSAccountRef account, CFDictionaryRef updates, CFErrorRef *error);


SOSCCStatus SOSAccountGetCircleStatus(SOSAccountRef account, CFErrorRef* error);
CFStringRef SOSAccountGetSOSCCStatusString(SOSCCStatus status);
bool SOSAccountIsInCircle(SOSAccountRef account, CFErrorRef *error);
bool SOSAccountJoinCircles(SOSAccountTransactionRef aTxn, CFErrorRef* error);
bool SOSAccountJoinCirclesAfterRestore(SOSAccountTransactionRef aTxn, CFErrorRef* error);
bool SOSAccountLeaveCircle(SOSAccountRef account,CFErrorRef* error);
bool SOSAccountRemovePeersFromCircle(SOSAccountRef account, CFArrayRef peers, CFErrorRef* error);
bool SOSAccountBail(SOSAccountRef account, uint64_t limit_in_seconds, CFErrorRef* error);
bool SOSAccountAcceptApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error);
bool SOSAccountRejectApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error);

bool SOSAccountResetToOffering(SOSAccountTransactionRef aTxn, CFErrorRef* error);
bool SOSAccountResetToEmpty(SOSAccountRef account, CFErrorRef* error);
bool SOSValidateUserPublic(SOSAccountRef account, CFErrorRef* error);

void SOSAccountForEachCirclePeerExceptMe(SOSAccountRef account, void (^action)(SOSPeerInfoRef peer));

CFArrayRef SOSAccountCopyApplicants(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyGeneration(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyValidPeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyPeersToListenTo(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyNotValidPeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyRetired(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyViewUnaware(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyPeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyActivePeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyActiveValidPeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyConcurringPeers(SOSAccountRef account, CFErrorRef *error);

SOSFullPeerInfoRef SOSAccountCopyAccountIdentityPeerInfo(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef* error);
bool SOSAccountIsAccountIdentity(SOSAccountRef account, SOSPeerInfoRef peer_info, CFErrorRef *error);

enum DepartureReason SOSAccountGetLastDepartureReason(SOSAccountRef account, CFErrorRef* error);

//
// MARK: iCloud Identity
//
bool SOSAccountAddiCloudIdentity(SOSAccountRef account, SOSCircleRef circle, SecKeyRef user_key, CFErrorRef *error);
bool SOSAccountRemoveIncompleteiCloudIdentities(SOSAccountRef account, SOSCircleRef circle, SecKeyRef privKey, CFErrorRef *error);

//
// MARK: Save Block
//

void SOSAccountSetSaveBlock(SOSAccountRef account, SOSAccountSaveBlock saveBlock);
void SOSAccountFlattenToSaveBlock(SOSAccountRef account);

//
// MARK: Change blocks
//
void SOSAccountAddChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock);
void SOSAccountRemoveChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock);

void SOSAccountAddSyncablePeerBlock(SOSAccountRef a,
                                    CFStringRef ds_name,
                                    SOSAccountSyncablePeersBlock changeBlock);

//
// MARK: Local device gestalt change.
//
bool SOSAccountUpdateGestalt(SOSAccountRef account, CFDictionaryRef new_gestalt);

CFDictionaryRef SOSAccountCopyGestalt(SOSAccountRef account);

CFDictionaryRef SOSAccountCopyV2Dictionary(SOSAccountRef account);

bool SOSAccountUpdateV2Dictionary(SOSAccountRef account, CFDictionaryRef newV2Dict);

bool SOSAccountUpdateFullPeerInfo(SOSAccountRef account, CFSetRef minimumViews, CFSetRef excludedViews);

SOSViewResultCode SOSAccountUpdateView(SOSAccountRef account, CFStringRef viewname, SOSViewActionCode actionCode, CFErrorRef *error);

SOSViewResultCode SOSAccountViewStatus(SOSAccountRef account, CFStringRef viewname, CFErrorRef *error);

bool SOSAccountUpdateViewSets(SOSAccountRef account, CFSetRef enabledViews, CFSetRef disabledViews);

void SOSAccountPendEnableViewSet(SOSAccountRef account, CFSetRef enabledViews);
void SOSAccountPendDisableViewSet(SOSAccountRef account, CFSetRef disabledViews);


SOSSecurityPropertyResultCode SOSAccountUpdateSecurityProperty(SOSAccountRef account, CFStringRef property, SOSSecurityPropertyActionCode actionCode, CFErrorRef *error);

SOSSecurityPropertyResultCode SOSAccountSecurityPropertyStatus(SOSAccountRef account, CFStringRef property, CFErrorRef *error);


bool SOSAccountHandleParametersChange(SOSAccountRef account, CFDataRef updates, CFErrorRef *error);

//
// MARK: Requests for syncing later
//
bool SOSAccountRequestSyncWithAllPeers(SOSAccountTransactionRef txn, CFErrorRef *error);

//
// MARK: Outgoing/Sync functions
//      

bool SOSAccountSyncWithKVSPeerWithMessage(SOSAccountTransactionRef txn, CFStringRef peerid, CFDataRef message, CFErrorRef *error);
bool SOSAccountClearPeerMessageKey(SOSAccountTransactionRef txn, CFStringRef peerID, CFErrorRef *error);

CF_RETURNS_RETAINED CFSetRef SOSAccountProcessSyncWithPeers(SOSAccountTransactionRef txn, CFSetRef /* CFStringRef */ peers, CFSetRef /* CFStringRef */ backupPeers, CFErrorRef *error);

bool SOSAccountSendIKSPSyncList(SOSAccountRef account, CFErrorRef *error);
bool SOSAccountSyncWithKVSUsingIDSID(SOSAccountRef account, CFStringRef deviceID, CFErrorRef *error);


//
// MARK: Cleanup functions
//
bool SOSAccountCleanupAfterPeer(SOSAccountRef account, size_t seconds, SOSCircleRef circle,
                                SOSPeerInfoRef cleanupPeer, CFErrorRef* error);

bool SOSAccountCleanupRetirementTickets(SOSAccountRef account, size_t seconds, CFErrorRef* error);

bool SOSAccountScanForRetired(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error);

SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccountRef account, SOSCircleRef starting_circle, CFErrorRef *error);

bool SOSAccountPostDebugScope(SOSAccountRef account, CFTypeRef scope, CFErrorRef *error);

//
// MARK: Version incompatibility Functions
//
CFStringRef SOSAccountCopyIncompatibilityInfo(SOSAccountRef account, CFErrorRef* error);

//
// MARK: Backup functions
//

bool SOSAccountIsBackupRingEmpty(SOSAccountRef account, CFStringRef viewName);
bool SOSAccountNewBKSBForView(SOSAccountRef account, CFStringRef viewName, CFErrorRef *error);

bool SOSAccountSetBackupPublicKey(SOSAccountTransactionRef aTxn, CFDataRef backupKey, CFErrorRef *error);
bool SOSAccountRemoveBackupPublickey(SOSAccountTransactionRef aTxn, CFErrorRef *error);
bool SOSAccountSetBSKBagForAllSlices(SOSAccountRef account, CFDataRef backupSlice, bool setupV0Only, CFErrorRef *error);

SOSBackupSliceKeyBagRef SOSAccountBackupSliceKeyBagForView(SOSAccountRef account, CFStringRef viewName, CFErrorRef* error);

bool SOSAccountIsLastBackupPeer(SOSAccountRef account, CFErrorRef *error);


//
// MARK: Recovery Public Key Functions
//
bool SOSAccountRegisterRecoveryPublicKey(SOSAccountTransactionRef txn, CFDataRef recovery_key, CFErrorRef *error);
CFDataRef SOSAccountCopyRecoveryPublicKey(SOSAccountTransactionRef txn, CFErrorRef *error);
bool SOSAccountClearRecoveryPublicKey(SOSAccountTransactionRef txn, CFDataRef recovery_key, CFErrorRef *error);
bool SOSAccountSetRecoveryKey(SOSAccountRef account, CFDataRef pubData, CFErrorRef *error);
bool SOSAccountRemoveRecoveryKey(SOSAccountRef account, CFErrorRef *error);
SOSRecoveryKeyBagRef SOSAccountCopyRecoveryKeyBag(CFAllocatorRef allocator, SOSAccountRef account, CFErrorRef *error);
CFDataRef SOSAccountCopyRecoveryPublic(CFAllocatorRef allocator, SOSAccountRef account, CFErrorRef *error);
bool SOSAccountRecoveryKeyIsInBackupAndCurrentInView(SOSAccountRef account, CFStringRef viewname);
bool SOSAccountSetRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccountRef account, SOSRecoveryKeyBagRef rkbg, CFErrorRef *error);
SOSRecoveryKeyBagRef SOSAccountCopyRecoveryKeyBagEntry(CFAllocatorRef allocator, SOSAccountRef account, CFErrorRef *error);
void SOSAccountEnsureRecoveryRing(SOSAccountRef account);

//
// MARK: Private functions
//

dispatch_queue_t SOSAccountGetQueue(SOSAccountRef account);

typedef bool (^SOSAccountSendBlock)(CFStringRef key, CFDataRef message, CFErrorRef *error);

//
// MARK: Utility functions
//

CFStringRef SOSAccountCreateCompactDescription(SOSAccountRef a);
CFStringRef SOSInterestListCopyDescription(CFArrayRef interests);

//
// MARK: View Funcitons
//

// Use these to tell the engine what views are common to myPeer and other circle peers
CFArrayRef SOSCreateActiveViewIntersectionArrayForPeerID(SOSAccountRef account, CFStringRef peerID);
CFDictionaryRef SOSViewsCreateActiveViewMatrixDictionary(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error);

const uint8_t* der_decode_cloud_parameters(CFAllocatorRef allocator,
                                           CFIndex algorithmID, SecKeyRef* publicKey,
                                           CFDataRef *parameters,
                                           CFErrorRef* error,
                                           const uint8_t* der, const uint8_t* der_end);

/* CFSet <-> XPC functions */
CFSetRef CreateCFSetRefFromXPCObject(xpc_object_t xpcSetDER, CFErrorRef* error);
xpc_object_t CreateXPCObjectWithCFSetRef(CFSetRef setref, CFErrorRef *error);


//
// MARK: HSA2 Piggyback Support Functions
//

SOSPeerInfoRef SOSAccountCopyApplication(SOSAccountRef account, CFErrorRef*);
CFDataRef SOSAccountCopyCircleJoiningBlob(SOSAccountRef account, SOSPeerInfoRef applicant, CFErrorRef *error);
bool SOSAccountJoinWithCircleJoiningBlob(SOSAccountRef account, CFDataRef joiningBlob, CFErrorRef *error);

//
// MARK: Initial-Sync
//
bool SOSAccountHasCompletedInitialSync(SOSAccountRef account);
CFMutableSetRef SOSAccountCopyUnsyncedInitialViews(SOSAccountRef account);
bool SOSAccountHasCompletedRequiredBackupSync(SOSAccountRef account);

//
// MARK: State Logging
//
void SOSAccountLogState(SOSAccountRef account);
void SOSAccountLogViewState(SOSAccountRef account);

//
// MARK: Checking other peer views
//

CFBooleanRef SOSAccountPeersHaveViewsEnabled(SOSAccountRef account, CFArrayRef viewNames, CFErrorRef *error);

void SOSAccountSetTestSerialNumber(SOSAccountRef account, CFStringRef serial);


//
// MARK: Syncing status functions
//
bool SOSAccountMessageFromPeerIsPending(SOSAccountTransactionRef txn, SOSPeerInfoRef peer, CFErrorRef *error);
bool SOSAccountSendToPeerIsPending(SOSAccountTransactionRef txn, SOSPeerInfoRef peer, CFErrorRef *error);

__END_DECLS

#endif /* !_SOSACCOUNT_H_ */
