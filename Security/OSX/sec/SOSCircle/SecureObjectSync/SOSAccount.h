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


#include <CoreFoundation/CoreFoundation.h>

#include <Security/SecureObjectSync/SOSCircle.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSTransportKeyParameter.h>
#include <Security/SecureObjectSync/SOSTransportCircle.h>
#include <Security/SecureObjectSync/SOSTransportMessage.h>
#include <Security/SecureObjectSync/SOSRing.h>
#include <Security/SecureObjectSync/SOSPeerInfoSecurityProperties.h>

#include <dispatch/dispatch.h>

__BEGIN_DECLS

#define RETIREMENT_FINALIZATION_SECONDS (24*60*60)


typedef void (^SOSAccountCircleMembershipChangeBlock)(SOSCircleRef new_circle,
                                                      CFSetRef added_peers, CFSetRef removed_peers,
                                                      CFSetRef added_applicants, CFSetRef removed_applicants);
typedef void (^SOSAccountSyncablePeersBlock)(CFArrayRef trustedPeers, CFArrayRef addedPeers, CFArrayRef removedPeers);

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

SOSAccountRef SOSAccountCreateFromDER_V3(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
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
bool SOSAccountSetMyDSID(SOSAccountRef account, CFStringRef IDS, CFErrorRef* errror);
bool SOSAccountSendIDSTestMessage(SOSAccountRef account, CFStringRef message, CFErrorRef *error);
bool SOSAccountStartPingTest(SOSAccountRef account, CFStringRef message, CFErrorRef *error);
bool SOSAccountRetrieveDeviceIDFromIDSKeychainSyncingProxy(SOSAccountRef account, CFErrorRef *error);

//
// MARK: Credential management
//

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
bool SOSAccountIsInCircle(SOSAccountRef account, CFErrorRef *error);
bool SOSAccountJoinCircles(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountJoinCirclesAfterRestore(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountLeaveCircle(SOSAccountRef account,CFErrorRef* error);
bool SOSAccountRemovePeersFromCircle(SOSAccountRef account, CFArrayRef peers, CFErrorRef* error);
bool SOSAccountBail(SOSAccountRef account, uint64_t limit_in_seconds, CFErrorRef* error);
bool SOSAccountAcceptApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error);
bool SOSAccountRejectApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error);

bool SOSAccountResetToOffering(SOSAccountRef account, CFErrorRef* error);
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

bool SOSAccountUpdateFullPeerInfo(SOSAccountRef account, CFSetRef minimumViews, CFSetRef excludedViews);

SOSViewResultCode SOSAccountUpdateView(SOSAccountRef account, CFStringRef viewname, SOSViewActionCode actionCode, CFErrorRef *error);

SOSViewResultCode SOSAccountViewStatus(SOSAccountRef account, CFStringRef viewname, CFErrorRef *error);

bool SOSAccountUpdateViewSets(SOSAccountRef account, CFSetRef enabledViews, CFSetRef disabledViews);

SOSSecurityPropertyResultCode SOSAccountUpdateSecurityProperty(SOSAccountRef account, CFStringRef property, SOSSecurityPropertyActionCode actionCode, CFErrorRef *error);

SOSSecurityPropertyResultCode SOSAccountSecurityPropertyStatus(SOSAccountRef account, CFStringRef property, CFErrorRef *error);


bool SOSAccountHandleParametersChange(SOSAccountRef account, CFDataRef updates, CFErrorRef *error);

bool SOSAccountSyncWithAllPeers(SOSAccountRef account, CFErrorRef *error);

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
bool SOSAccountStartNewBackup(SOSAccountRef account, CFStringRef viewName, CFErrorRef *error);

bool SOSAccountSetBackupPublicKey(SOSAccountRef account, CFDataRef backupKey, CFErrorRef *error);
bool SOSAccountRemoveBackupPublickey(SOSAccountRef account, CFErrorRef *error);
bool SOSAccountSetBSKBagForAllSlices(SOSAccountRef account, CFDataRef backupSlice, bool setupV0Only, CFErrorRef *error);

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

//
// MARK: Transactional functions
//

void SOSAccountFinishTransaction(SOSAccountRef account);

const uint8_t* der_decode_cloud_parameters(CFAllocatorRef allocator,
                                           CFIndex algorithmID, SecKeyRef* publicKey,
                                           CFDataRef *parameters,
                                           CFErrorRef* error,
                                           const uint8_t* der, const uint8_t* der_end);

/* CFSet <-> XPC functions */
CFSetRef CreateCFSetRefFromXPCObject(xpc_object_t xpcSetDER, CFErrorRef* error);
xpc_object_t CreateXPCObjectWithCFSetRef(CFSetRef setref, CFErrorRef *error);


__END_DECLS

#endif /* !_SOSACCOUNT_H_ */
