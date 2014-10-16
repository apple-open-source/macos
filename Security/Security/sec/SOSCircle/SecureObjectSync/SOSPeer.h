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
 @header SOSPeer
 The functions provided in SOSPeer provide an interface to a
 secure object syncing peer in a circle
 */

#ifndef _SOSPEER_H_
#define _SOSPEER_H_

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSCoder.h>

__BEGIN_DECLS

/* Constructor called by Engine */
SOSPeerRef SOSPeerCreateWithEngine(SOSEngineRef engine, CFStringRef peer_id);
void SOSPeerMarkDigestsInUse(SOSPeerRef peer, struct SOSDigestVector *mdInUse);
bool SOSPeerDidReceiveRemovalsAndAdditions(SOSPeerRef peer, SOSManifestRef absentFromRemote, SOSManifestRef additionsFromRemote,
                                           SOSManifestRef local, CFErrorRef *error);
bool SOSPeerDidReceiveConfirmedManifest(SOSPeerRef peer, SOSManifestRef confirmed, SOSManifestRef local, CFErrorRef *error);
bool SOSPeerDataSourceWillCommit(SOSPeerRef peer, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error);

/* Constructor called by Account */
SOSPeerRef SOSPeerCreate(SOSEngineRef engine, SOSPeerInfoRef peerInfo, CFErrorRef *error);

/* For testing, doesn't OTR encode and uses static ID for self */
SOSPeerRef SOSPeerCreateSimple(SOSEngineRef engine, CFStringRef peer_id, CFIndex version, CFErrorRef *error);

//
//
//

CFIndex SOSPeerGetVersion(SOSPeerRef peer);
CFStringRef SOSPeerGetID(SOSPeerRef peer);
bool SOSPeersEqual(SOSPeerRef peerA, SOSPeerRef peerB);
SOSCoderRef SOSPeerGetCoder(SOSPeerRef peer);
void SOSPeerSetCoder(SOSPeerRef peer, SOSCoderRef coder);

uint64_t SOSPeerNextSequenceNumber(SOSPeerRef peer);
uint64_t SOSPeerGetMessageVersion(SOSPeerRef peer);

//
// MARK: State tracking helpers
//

#if 0
bool SOSPeerJoinRequest(SOSPeerRef peer);
void SOSPeerSetJoinRequest(SOSPeerRef peer, bool joinRequest);

bool SOSPeerShouldRequestObjects(SOSPeerRef peer);
void SOSPeerSetShouldRequestObjects(SOSPeerRef peer, bool requestObjects);

bool SOSPeerSkipHello(SOSPeerRef peer);
void SOSPeerSetSkipHello(SOSPeerRef peer, bool skipHello);

// The last manifestDigest peer received couldn't be matched to a known manifest
bool SOSPeerReceivedUnknownManifest(SOSPeerRef peer);
void SOSPeerSetReceivedUnknownManifest(SOSPeerRef peer, bool unknownManifest);
#endif


// Return true if the peer is ready to transmit data.
void SOSPeerDidConnect(SOSPeerRef peer);
bool SOSPeerMustSendMessage(SOSPeerRef peer);
void SOSPeerSetMustSendMessage(SOSPeerRef peer, bool must);

bool SOSPeerSendObjects(SOSPeerRef peer);
void SOSPeerSetSendObjects(SOSPeerRef peer, bool sendObjects);
SOSEngineRef SOSPeerGetEngine(SOSPeerRef peer);

SOSManifestRef SOSPeerGetProposedManifest(SOSPeerRef peer);
SOSManifestRef SOSPeerGetConfirmedManifest(SOSPeerRef peer);
void SOSPeerSetConfirmedManifest(SOSPeerRef peer, SOSManifestRef confirmed);
void SOSPeerAddProposedManifest(SOSPeerRef peer, SOSManifestRef pending);
void SOSPeerSetProposedManifest(SOSPeerRef peer, SOSManifestRef pending);
void SOSPeerAddLocalManifest(SOSPeerRef peer, SOSManifestRef local);
SOSManifestRef SOSPeerGetPendingObjects(SOSPeerRef peer);
void SOSPeerSetPendingObjects(SOSPeerRef peer, SOSManifestRef pendingObjects);
SOSManifestRef SOSPeerGetPendingDeletes(SOSPeerRef peer);
void SOSPeerSetPendingDeletes(SOSPeerRef peer, SOSManifestRef pendingDeletes);

SOSManifestRef SOSPeerGetPendingObjects(SOSPeerRef peer);

__END_DECLS

#endif /* !_SOSPEER_H_ */
