/*
 * Created by Michael Brouwer on 6/22/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*!
 @header SOSAccount.h
 The functions provided in SOSCircle.h provide an interface to a
 secure object syncing circle for a single class
 */

#ifndef _SOSACCOUNT_H_
#define _SOSACCOUNT_H_

#include <CoreFoundation/CoreFoundation.h>

#include <SecureObjectSync/SOSCircle.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSCloudCircle.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

#define RETIREMENT_FINALIZATION_SECONDS (24*60*60)


/* Forward declarations of SOS types. */
typedef struct __OpaqueSOSAccount *SOSAccountRef;

typedef void (^SOSAccountKeyInterestBlock)(bool getNewKeysOnly, CFArrayRef alwaysKeys, CFArrayRef afterFirstUnlockKeys, CFArrayRef unlockedKeys);
typedef bool (^SOSAccountDataUpdateBlock)(CFDictionaryRef keys, CFErrorRef *error);
typedef void (^SOSAccountCircleMembershipChangeBlock)(SOSCircleRef new_circle,
                                                      CFArrayRef added_peers, CFArrayRef removed_peers,
                                                      CFArrayRef added_applicants, CFArrayRef removed_applicants);

SOSAccountRef SOSAccountGetShared(void);
SOSAccountRef SOSAccountCreate(CFAllocatorRef allocator,
                               CFDictionaryRef gestalt,
                               SOSDataSourceFactoryRef factory,
                               SOSAccountKeyInterestBlock interest_block,
                               SOSAccountDataUpdateBlock update_block);

//
// MARK: Persistent Encode decode
//

SOSAccountRef SOSAccountCreateFromDER(CFAllocatorRef allocator, SOSDataSourceFactoryRef factory,
                                      SOSAccountKeyInterestBlock interest_block, SOSAccountDataUpdateBlock update_block,
                                      CFErrorRef* error,
                                      const uint8_t** der_p, const uint8_t *der_end);

SOSAccountRef SOSAccountCreateFromDER_V3(CFAllocatorRef allocator,
                                         SOSDataSourceFactoryRef factory,
                                         SOSAccountKeyInterestBlock interest_block,
                                         SOSAccountDataUpdateBlock update_block,
                                         CFErrorRef* error,
                                         const uint8_t** der_p, const uint8_t *der_end);

SOSAccountRef SOSAccountCreateFromData(CFAllocatorRef allocator, CFDataRef circleData,
                                       SOSDataSourceFactoryRef factory,
                                       SOSAccountKeyInterestBlock interest_block, SOSAccountDataUpdateBlock update_block,
                                       CFErrorRef* error);

size_t SOSAccountGetDEREncodedSize(SOSAccountRef cir, CFErrorRef *error);
uint8_t* SOSAccountEncodeToDER(SOSAccountRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);
size_t SOSAccountGetDEREncodedSize_V3(SOSAccountRef cir, CFErrorRef *error);
uint8_t* SOSAccountEncodeToDER_V3(SOSAccountRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);
CFDataRef SOSAccountCopyEncodedData(SOSAccountRef circle, CFAllocatorRef allocator, CFErrorRef *error);


//
// MARK: Local Peer finding
//
SOSPeerInfoRef SOSAccountGetMyPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);
SOSPeerInfoRef SOSAccountGetMyPeerInCircleNamed(SOSAccountRef account, CFStringRef circle, CFErrorRef* error);

SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef* error);
SOSFullPeerInfoRef SOSAccountGetMyFullPeerInCircleNamed(SOSAccountRef account, CFStringRef name, CFErrorRef *error);

//
// MARK: Credential management
//

SecKeyRef SOSAccountGetPrivateCredential(SOSAccountRef account, CFErrorRef* error);
void SOSAccountPurgePrivateCredential(SOSAccountRef account);

bool SOSAccountTryUserCredentials(SOSAccountRef account,
                                  CFStringRef user_account, CFDataRef user_password,
                                  CFErrorRef *error);

bool SOSAccountAssertUserCredentials(SOSAccountRef account,
                                     CFStringRef user_account, CFDataRef user_password,
                                     CFErrorRef *error);


//
// MARK: Circle management
//
int SOSAccountCountCircles(SOSAccountRef a);

void SOSAccountForEachCircle(SOSAccountRef account, void (^process)(SOSCircleRef circle));

SOSCircleRef SOSAccountFindCompatibleCircle(SOSAccountRef a, CFStringRef name);
SOSCircleRef SOSAccountFindCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error);
SOSCircleRef SOSAccountEnsureCircle(SOSAccountRef a, CFStringRef name, CFErrorRef *error);
bool SOSAccountUpdateCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error);

bool SOSAccountModifyCircle(SOSAccountRef account,
                            CFStringRef circleName,
                            CFErrorRef *error,
                            void (^action)(SOSCircleRef circle));


SOSCCStatus SOSAccountIsInCircles(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountJoinCircles(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountJoinCirclesAfterRestore(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountLeaveCircles(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountBail(SOSAccountRef account, uint64_t limit_in_seconds, CFErrorRef* error);
bool SOSAccountAcceptApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error);
bool SOSAccountRejectApplicants(SOSAccountRef account, CFArrayRef applicants, CFErrorRef* error);

bool SOSAccountResetToOffering(SOSAccountRef account, CFErrorRef* error);
bool SOSAccountResetToEmpty(SOSAccountRef account, CFErrorRef* error);

CFArrayRef SOSAccountCopyApplicants(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyPeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyActivePeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyActiveValidPeers(SOSAccountRef account, CFErrorRef *error);
CFArrayRef SOSAccountCopyConcurringPeers(SOSAccountRef account, CFErrorRef *error);

CFArrayRef SOSAccountCopyAccountIdentityPeerInfos(SOSAccountRef account, CFAllocatorRef allocator, CFErrorRef* error);
bool SOSAccountIsAccountIdentity(SOSAccountRef account, SOSPeerInfoRef peer_info, CFErrorRef *error);

enum DepartureReason SOSAccountGetLastDepartureReason(SOSAccountRef account, CFErrorRef* error);

//
// MARK: Change blocks
//
void SOSAccountAddChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock);
void SOSAccountRemoveChangeBlock(SOSAccountRef a, SOSAccountCircleMembershipChangeBlock changeBlock);

//
// MARK: Local device gestalt change.
//
bool SOSAccountUpdateGestalt(SOSAccountRef account, CFDictionaryRef new_gestalt);

// TODO: ds should be a SOSDataSourceFactoryRef
bool SOSAccountHandleUpdates(SOSAccountRef account,
                             CFDictionaryRef updates,
                             CFErrorRef *error);

bool SOSAccountSyncWithPeer(SOSAccountRef account, SOSCircleRef circle, SOSPeerInfoRef thisPeer, bool* didSendData, CFErrorRef* error);
bool SOSAccountSyncWithAllPeers(SOSAccountRef account, CFErrorRef *error);
bool SOSAccountSyncWithAllPeersInCircle(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error);

bool SOSAccountCleanupAfterPeer(SOSAccountRef account, size_t seconds, SOSCircleRef circle,
                                SOSPeerInfoRef cleanupPeer, CFErrorRef* error);

bool SOSAccountCleanupRetirementTickets(SOSAccountRef account, size_t seconds, CFErrorRef* error);

bool SOSAccountScanForRetired(SOSAccountRef account, SOSCircleRef circle, CFErrorRef *error);

SOSCircleRef SOSAccountCloneCircleWithRetirement(SOSAccountRef account, SOSCircleRef starting_circle, CFErrorRef *error);

//
// MARK: Version incompatibility Functions
//
CFStringRef SOSAccountCopyIncompatibilityInfo(SOSAccountRef account, CFErrorRef* error);

//
// MARK: Private functions
//

dispatch_queue_t SOSAccountGetQueue(SOSAccountRef account);


//
// MARK: Private functions for testing
//


typedef enum {
    kCircleKey,
    kMessageKey,
    kParametersKey,
    kInitialSyncKey,
    kRetirementKey,
    kAccountChangedKey,
    kUnknownKey,
} SOSKVSKeyType;

extern const CFStringRef kSOSKVSKeyParametersKey;
extern const CFStringRef kSOSKVSInitialSyncKey;
extern const CFStringRef kSOSKVSAccountChangedKey;

SOSKVSKeyType SOSKVSKeyGetKeyType(CFStringRef key);
SOSKVSKeyType SOSKVSKeyGetKeyTypeAndParse(CFStringRef key, CFStringRef *circle, CFStringRef *from, CFStringRef *to);

CFStringRef SOSCircleKeyCreateWithCircle(SOSCircleRef circle, CFErrorRef *error);
CFStringRef SOSCircleKeyCreateWithName(CFStringRef name, CFErrorRef *error);
CFStringRef SOSCircleKeyCopyCircleName(CFStringRef key, CFErrorRef *error);

CFStringRef SOSMessageKeyCopyCircleName(CFStringRef key, CFErrorRef *error);
CFStringRef SOSMessageKeyCopyFromPeerName(CFStringRef messageKey, CFErrorRef *error);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerNames(SOSCircleRef circle, CFStringRef from_peer_name, CFStringRef to_peer_name);
CFStringRef SOSMessageKeyCreateWithCircleAndPeerInfos(SOSCircleRef circle, SOSPeerInfoRef from_peer, SOSPeerInfoRef to_peer);
CFStringRef SOSMessageKeyCreateWithAccountAndPeer(SOSAccountRef account, SOSCircleRef circle, CFStringRef peer_name);

CFStringRef SOSRetirementKeyCreateWithCircleAndPeer(SOSCircleRef circle, CFStringRef retirement_peer_name);

typedef void (^SOSAccountMessageProcessedBlock)(SOSCircleRef circle, CFDataRef messageIn, CFDataRef messageOut);
typedef bool (^SOSAccountSendBlock)(SOSCircleRef circle, CFStringRef key, CFDataRef message, CFErrorRef *error);

void SOSAccountSetMessageProcessedBlock(SOSAccountRef account, SOSAccountMessageProcessedBlock processedBlock);

//
// MARK: Utility functions
//

CFStringRef SOSInterestListCopyDescription(CFArrayRef interests);

__END_DECLS

#endif /* !_SOSACCOUNT_H_ */
