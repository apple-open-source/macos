/*
 * Created by Michael Brouwer on 6/22/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*!
 @header SOSPeer
 The functions provided in SOSPeer provide an interface to a
 secure object syncing peer in a circle
 */

#ifndef _SOSPEER_H_
#define _SOSPEER_H_

#include <SecureObjectSync/SOSEngine.h>
// #include <SecureObjectSync/SOSCoder.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>

__BEGIN_DECLS

enum {
    kSOSPeerHasNoManifest = 1,
    kSOSPeerDigestFailure = 2,
};

enum {
    kSOSPeerCoderDataReturned = 0,
    kSOSPeerCoderNegotiating = 1,
    kSOSPeerCoderNegotiationCompleted = 2,
    kSOSPeerCoderFailure = 3,
    kSOSPeerCoderStaleEvent = 4,
};
typedef uint32_t SOSPeerCoderStatus;

typedef bool (^SOSPeerSendBlock)(CFDataRef message, CFErrorRef *error);


/* Constructor */
SOSPeerRef SOSPeerCreate(SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error,
                         SOSPeerSendBlock sendBlock);

// Permanently forgetting stored information (e.g. keys on keychain)
void SOSPeerPurge(SOSPeerRef);
void SOSPeerPurgeAllFor(CFStringRef my_id);

// Dispose of a peer when it's no longer needed.
void SOSPeerDispose(SOSPeerRef peer);

SOSPeerCoderStatus SOSPeerStartSync(SOSPeerRef peer, SOSEngineRef engine, CFErrorRef *error);

// Handle an incoming message and pass it to the engine.
SOSPeerCoderStatus SOSPeerHandleMessage(SOSPeerRef peer, SOSEngineRef engine, CFDataRef message, CFErrorRef *error);

// Called by engine to send message to our tranport.
bool SOSPeerSendMessage(SOSPeerRef peer, CFDataRef message, CFErrorRef *error);

// Return true if the peer is ready to transmit data.
bool SOSPeerCanSendMessage(SOSPeerRef peer);

CFIndex SOSPeerGetVersion(SOSPeerRef peer);

CFStringRef SOSPeerGetID(SOSPeerRef peer);
bool SOSPeersEqual(SOSPeerRef peerA, SOSPeerRef peerB);

bool SOSPeerSetManifest(SOSPeerRef peer, SOSManifestRef manifest, CFErrorRef *error);

SOSManifestRef SOSPeerCopyManifest(SOSPeerRef peer, CFErrorRef *error);
CFDataRef SOSPeerCopyManifestDigest(SOSPeerRef peer, CFErrorRef *error);

/* For testing, doesn't OTR encode and uses static ID for self */
SOSPeerRef SOSPeerCreateSimple(CFStringRef peer_id, CFIndex version, CFErrorRef *error,
                               SOSPeerSendBlock sendBlock);

__END_DECLS

#endif /* !_SOSPEER_H_ */
