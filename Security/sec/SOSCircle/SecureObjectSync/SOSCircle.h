/*
 * Created by Michael Brouwer on 6/22/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*!
 @header SOSCircle.h
 The functions provided in SOSCircle.h provide an interface to a
 secure object syncing circle for a single class
 */

#ifndef _SOSCIRCLE_H_
#define _SOSCIRCLE_H_

#include <Security/Security.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>

__BEGIN_DECLS

typedef struct __OpaqueSOSCircle *SOSCircleRef;

/* SOSDataSourceFactory protocol (non opaque). */
typedef struct SOSDataSourceFactory *SOSDataSourceFactoryRef;

struct SOSDataSourceFactory {
    CFArrayRef       (*copy_names)(SOSDataSourceFactoryRef factory);
    SOSDataSourceRef (*create_datasource)(SOSDataSourceFactoryRef factory, CFStringRef dataSourceName, bool readOnly, CFErrorRef *error);
    void             (*release)(SOSDataSourceFactoryRef factory);
};


CFTypeID SOSCircleGetTypeID();

SOSCircleRef SOSCircleCreate(CFAllocatorRef allocator, CFStringRef circleName, CFErrorRef *error);
SOSCircleRef SOSCircleCreateFromDER(CFAllocatorRef allocator, CFErrorRef* error,
                                    const uint8_t** der_p, const uint8_t *der_end);
SOSCircleRef SOSCircleCreateFromData(CFAllocatorRef allocator, CFDataRef circleData, CFErrorRef *error);
SOSCircleRef SOSCircleCopyCircle(CFAllocatorRef allocator, SOSCircleRef otherCircle, CFErrorRef *error);

bool SOSCircleSign(SOSCircleRef circle, SecKeyRef privkey, CFErrorRef *error);
bool SOSCircleVerifySignatureExists(SOSCircleRef circle, SecKeyRef pubKey, CFErrorRef *error);
bool SOSCircleVerify(SOSCircleRef circle, SecKeyRef pubkey, CFErrorRef *error);

bool SOSCircleVerifyPeerSigned(SOSCircleRef circle, SOSPeerInfoRef peer, CFErrorRef *error);

bool SOSCircleGenerationSign(SOSCircleRef circle, SecKeyRef user_approver, SOSFullPeerInfoRef peerinfo, CFErrorRef *error);

size_t SOSCircleGetDEREncodedSize(SOSCircleRef cir, CFErrorRef *error);
uint8_t* SOSCircleEncodeToDER(SOSCircleRef cir, CFErrorRef* error, const uint8_t* der, uint8_t* der_end);
CFDataRef SOSCircleCopyEncodedData(SOSCircleRef circle, CFAllocatorRef allocator, CFErrorRef *error);

int SOSCircleCountApplicants(SOSCircleRef circle);
bool SOSCircleHasApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);
CFMutableArrayRef SOSCircleCopyApplicants(SOSCircleRef c, CFAllocatorRef allocator);
void SOSCircleForEachApplicant(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer));

int SOSCircleCountRejectedApplicants(SOSCircleRef circle);
bool SOSCircleHasRejectedApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);
SOSPeerInfoRef SOSCircleCopyRejectedApplicant(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);
CFMutableArrayRef SOSCircleCopyRejectedApplicants(SOSCircleRef c, CFAllocatorRef allocator);

CFStringRef SOSCircleGetName(SOSCircleRef circle);
const char *SOSCircleGetNameC(SOSCircleRef circle);

CFNumberRef SOSCircleGetGeneration(SOSCircleRef circle);
int64_t SOSCircleGetGenerationSint(SOSCircleRef circle);
void SOSCircleGenerationIncrement(SOSCircleRef circle);

CFMutableArrayRef SOSCircleCopyPeers(SOSCircleRef circle, CFAllocatorRef allocator);
CFMutableArrayRef SOSCircleCopyConcurringPeers(SOSCircleRef circle, CFErrorRef* error);

int SOSCircleCountPeers(SOSCircleRef circle);
int SOSCircleCountActivePeers(SOSCircleRef circle);
int SOSCircleCountActiveValidPeers(SOSCircleRef circle, SecKeyRef pubkey);
int SOSCircleCountRetiredPeers(SOSCircleRef circle);

void SOSCircleForEachPeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer));
void SOSCircleForEachRetiredPeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer));
void SOSCircleForEachActivePeer(SOSCircleRef circle, void (^action)(SOSPeerInfoRef peer));
void SOSCircleForEachActiveValidPeer(SOSCircleRef circle, SecKeyRef user_public_key, void (^action)(SOSPeerInfoRef peer));

bool SOSCircleHasPeerWithID(SOSCircleRef circle, CFStringRef peerid, CFErrorRef *error);
bool SOSCircleHasPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);
bool SOSCircleHasActivePeerWithID(SOSCircleRef circle, CFStringRef peerid, CFErrorRef *error);
bool SOSCircleHasActivePeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);

bool SOSCircleResetToOffering(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSCircleResetToEmpty(SOSCircleRef circle, CFErrorRef *error);
bool SOSCircleRequestAdmission(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);
bool SOSCircleRequestReadmission(SOSCircleRef circle, SecKeyRef user_pubkey, SOSFullPeerInfoRef requestor, CFErrorRef *error);

bool SOSCircleAcceptRequest(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error);
bool SOSCircleRejectRequest(SOSCircleRef circle, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error);
bool SOSCircleWithdrawRequest(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);
bool SOSCircleRemoveRejectedPeer(SOSCircleRef circle, SOSPeerInfoRef peerInfo, CFErrorRef *error);
bool SOSCirclePeerSigUpdate(SOSCircleRef circle, SecKeyRef userPrivKey, SOSFullPeerInfoRef fpi,
                            CFErrorRef *error);
//
// Update a peer's meta information.
// No resigning of the circle is done, only updates to their own self signed description.
//
bool SOSCircleUpdatePeerInfo(SOSCircleRef circle, SOSPeerInfoRef replacement_peer_info);

bool SOSCircleRemovePeer(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, SOSPeerInfoRef peerInfo, CFErrorRef *error);

CFIndex SOSCircleRemoveRetired(SOSCircleRef circle, CFErrorRef *error);

bool SOSCircleAcceptRequests(SOSCircleRef circle, SecKeyRef user_privkey, SOSFullPeerInfoRef device_approver, CFErrorRef *error);

SOSPeerInfoRef SOSCircleCopyPeerInfo(SOSCircleRef circle, CFStringRef peer_id, CFErrorRef *error);

// Stuff above this line is really SOSCircleInfo below the line is the active SOSCircle functionality

bool SOSCircleSyncWithPeer(SOSFullPeerInfoRef myRef, SOSCircleRef circle,
                           SOSDataSourceFactoryRef factory,
                           SOSPeerSendBlock sendBlock, CFStringRef peer_id,
                           CFErrorRef *error);

bool SOSCircleHandlePeerMessage(SOSCircleRef circle, SOSFullPeerInfoRef myRef, SOSDataSourceFactoryRef factory,
                                SOSPeerSendBlock sendBlock, CFStringRef peer_id,
                                CFDataRef message, CFErrorRef *error);

SOSFullPeerInfoRef SOSCircleGetiCloudFullPeerInfoRef(SOSCircleRef circle);

bool SOSCircleConcordanceSign(SOSCircleRef circle, SOSFullPeerInfoRef peerinfo, CFErrorRef *error);

enum {
    kSOSConcordanceTrusted = 0,
    kSOSConcordanceGenOld = 1,     // kSOSErrorReplay
    kSOSConcordanceNoUserSig = 2,  // kSOSErrorBadSignature
    kSOSConcordanceNoUserKey = 3,  // kSOSErrorNoKey
    kSOSConcordanceNoPeer = 4,     // kSOSErrorPeerNotFound
    kSOSConcordanceBadUserSig = 5, // kSOSErrorBadSignature
    kSOSConcordanceBadPeerSig = 6, // kSOSErrorBadSignature
    kSOSConcordanceNoPeerSig = 7,
    kSOSConcordanceWeSigned = 8,
};
typedef uint32_t SOSConcordanceStatus;

bool SOSCircleSharedTrustedPeers(SOSCircleRef current, SOSCircleRef proposed, SOSPeerInfoRef me);

SOSConcordanceStatus SOSCircleConcordanceTrust(SOSCircleRef known_circle, SOSCircleRef proposed_circle,
                                               SecKeyRef known_pubkey, SecKeyRef user_pubkey,
                                               SOSPeerInfoRef exclude, CFErrorRef *error);
//
// Testing routines:
//

CFDataRef SOSCircleCreateIncompatibleCircleDER(CFErrorRef* error);

__END_DECLS

#endif /* !_SOSCIRCLE_H_ */
