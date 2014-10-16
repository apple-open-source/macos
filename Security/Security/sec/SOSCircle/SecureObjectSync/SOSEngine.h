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
 @header SOSEngine.h - Manifest managent engine and decision making for
 object syncing protocol.
 */

#ifndef _SEC_SOSENGINE_H_
#define _SEC_SOSENGINE_H_

#include <SecureObjectSync/SOSDataSource.h>
#include <SecureObjectSync/SOSMessage.h>
#include <dispatch/dispatch.h>

__BEGIN_DECLS

enum {
    kSOSEngineInvalidMessageError = 1,
    kSOSEngineInternalError = 2,
};

typedef struct __OpaqueSOSPeer *SOSPeerRef;

typedef void (^SOSEnginePeerMessageSentBlock)(bool success);

//Remove me!
void SOSEngineCircleChanged_locked(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers, CFArrayRef untrustedPeers);


// Return a new engine instance for a given data source.
SOSEngineRef SOSEngineCreate(SOSDataSourceRef dataSource, CFErrorRef *error);

// Return a snapshot of the current manifest of the engines data source.
SOSManifestRef SOSEngineCopyManifest(SOSEngineRef engine, CFErrorRef *error);

// Remove removals and add additions to the (cached) local manifest, and update all peers accordingly
bool SOSEngineUpdateLocalManifest(SOSEngineRef engine, SOSDataSourceTransactionSource source, struct SOSDigestVector *removals, struct SOSDigestVector *additions, CFErrorRef *error);

// Store manifest indexed by it's own digest.  Can be retrieved with SOSEngineGetManifestForDigest()
void SOSEngineAddManifest(SOSEngineRef engine, SOSManifestRef manifest);

// Retrive a digest stored with SOSEngineAddManifest()
SOSManifestRef SOSEngineGetManifestForDigest(SOSEngineRef engine, CFDataRef digest);

// Return the digest for a patched manifest (which is stored in the cache already).
CFDataRef SOSEnginePatchRecordAndCopyDigest(SOSEngineRef engine, SOSManifestRef base, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error);

//Set/Get coders
bool SOSEngineSetCoderData(SOSEngineRef engine, CFStringRef peer_id, CFDataRef data, CFErrorRef *error);
CFDataRef SOSEngineGetCoderData(SOSEngineRef engine, CFStringRef peer_id);

//Get peer state
CFMutableDictionaryRef SOSEngineGetPeerState(SOSEngineRef engine, CFStringRef peerID);

// Dispose of an engine when it's no longer needed.
void SOSEngineDispose(SOSEngineRef engine);

// Handle incoming message from a remote peer.
bool SOSEngineHandleMessage(SOSEngineRef engine, CFStringRef peerID,
                            CFDataRef message, CFErrorRef *error);

void SOSEngineCircleChanged(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers, CFArrayRef untrustedPeers);

// Return a message to be sent for the current state.  Returns NULL on errors,
// return a zero length CFDataRef if there is nothing to send.
// If *ProposedManifest is set the caller is responsible for updating their
// proposed manifest upon successful transmission of the message.
CFDataRef SOSEngineCreateMessageToSyncToPeer(SOSEngineRef engine, CFStringRef peerID, SOSEnginePeerMessageSentBlock *sentBlock, CFErrorRef *error);

CFStringRef SOSEngineGetMyID(SOSEngineRef engine);
bool SOSEnginePeerDidConnect(SOSEngineRef engine, CFStringRef peerID, CFErrorRef *error);

void logRawMessage(CFDataRef message, bool sending, uint64_t seqno);

// TODO: TEMPORARY: Get the list of IDs for cleanup, this shouldn't be used instead transport should iterate KVS.
CFArrayRef SOSEngineGetPeerIDs(SOSEngineRef engine);

__END_DECLS

#endif /* !_SEC_SOSENGINE_H_ */
