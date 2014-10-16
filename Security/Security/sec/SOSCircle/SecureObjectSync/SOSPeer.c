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


/*
 * SOSPeer.c -  Implementation of a secure object syncing peer
 */
#include <SecureObjectSync/SOSPeer.h>

#include <SecureObjectSync/SOSCoder.h>
#include <SecureObjectSync/SOSDigestVector.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSTransport.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecIOFormat.h>
#include <utilities/array_size.h>
#include <utilities/debugging.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>

#include <securityd/SOSCloudCircleServer.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>

#include <AssertMacros.h>

//
// MARK: - SOSPeerPersistence code
//
static CFStringRef kSOSPeerSequenceNumberKey = CFSTR("sequence-number");
static CFStringRef kSOSPeerGetObjectsKey = CFSTR("get-objects");
static CFStringRef kSOSPeerReceivedUnknownConfirmedDigestKey = CFSTR("received-unknown");
static CFStringRef kSOSPeerJoinRequestedKey = CFSTR("join-requested");
static CFStringRef kSOSPeerSkipHelloKey = CFSTR("skip-hello");

CFStringRef kSOSPeerDataLabel = CFSTR("iCloud Peer Data Meta-data");

//
// MARK: SOSPeerState (dictionary keys)
//

// PeerState dictionary keys
static CFStringRef kSOSPeerSendObjectsKey = CFSTR("send-objects"); // bool
static CFStringRef kSOSPeerMustSendMessageKey = CFSTR("must-send"); // bool
static CFStringRef kSOSPeerPendingObjectsKey = CFSTR("pending-objects"); // digest
static CFStringRef kSOSPeerPendingDeletesKey = CFSTR("pending-deletes"); // digest
static CFStringRef kSOSPeerConfirmedManifestKey = CFSTR("confirmed-manifest");  //digest
static CFStringRef kSOSPeerProposedManifestKey = CFSTR("pending-manifest"); // array of digests
static CFStringRef kSOSPeerLocalManifestKey = CFSTR("local-manifest"); // array of digests
static CFStringRef kSOSPeerVersionKey = CFSTR("version"); // int

enum {
    kSOSPeerMaxManifestWindowDepth = 4
};

//
// MARK: - SOSPeer
//

struct __OpaqueSOSPeer {
    CFRuntimeBase _base;
    SOSEngineRef engine;
    
    SOSCoderRef coder;
    CFStringRef peer_id;
    CFIndex version;
    uint64_t sequenceNumber;
    bool mustSendMessage;
};

CFGiblisWithCompareFor(SOSPeer)

static CFStringRef SOSManifestCreateOptionalDescriptionWithLabel(SOSManifestRef manifest, CFStringRef label) {
    if (!manifest) return CFSTR(" -  ");
    //return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@[%zu]"), label, SOSManifestGetCount(manifest));
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(" %@%@"), label, manifest);
}

static CFMutableDictionaryRef SOSPeerGetState(SOSPeerRef peer) {
    return SOSEngineGetPeerState(peer->engine, peer->peer_id);
}

static CFStringRef SOSPeerCreateManifestArrayDescriptionWithKey(SOSPeerRef peer, CFStringRef key, CFStringRef label) {
    CFMutableArrayRef digests = (CFMutableArrayRef)CFDictionaryGetValue(SOSPeerGetState(peer), key);
    CFIndex count = digests ? CFArrayGetCount(digests) : 0;
    if (count == 0) return CFSTR(" -  ");
    CFDataRef digest = CFArrayGetValueAtIndex(digests, 0);
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(" %@[%" PRIdCFIndex "]%@"), label, count, SOSEngineGetManifestForDigest(peer->engine, digest));
}

static CFStringRef SOSPeerCopyDescription(CFTypeRef cf) {
    SOSPeerRef peer = (SOSPeerRef)cf;
    if(peer){
        CFStringRef po = SOSManifestCreateOptionalDescriptionWithLabel(SOSPeerGetPendingObjects(peer), CFSTR("O"));
        CFStringRef de = SOSManifestCreateOptionalDescriptionWithLabel(SOSPeerGetPendingDeletes(peer), CFSTR("D"));
        CFStringRef co = SOSManifestCreateOptionalDescriptionWithLabel(SOSPeerGetConfirmedManifest(peer), CFSTR("C"));
        CFStringRef pe = SOSPeerCreateManifestArrayDescriptionWithKey(peer, kSOSPeerProposedManifestKey, CFSTR("P"));
        CFStringRef lo = SOSPeerCreateManifestArrayDescriptionWithKey(peer, kSOSPeerLocalManifestKey, CFSTR("L"));
        CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<%@ %s%s%@%@%@%@%@>"),
                                                    SOSPeerGetID(peer),
                                                    SOSPeerMustSendMessage(peer) ? "F" : "f",
                                                    SOSPeerSendObjects(peer) ? "S" : "s",
                                                    po, de, co, pe, lo);
        CFReleaseSafe(lo);
        CFReleaseSafe(pe);
        CFReleaseSafe(co);
        CFReleaseSafe(de);
        CFReleaseSafe(po);
    
        return desc;
    }
    else
        return CFSTR("NULL");
}

static Boolean SOSPeerCompare(CFTypeRef cfA, CFTypeRef cfB)
{
    SOSPeerRef peerA = (SOSPeerRef)cfA, peerB = (SOSPeerRef)cfB;
    // Use mainly to see if peerB is actually this device (peerA)
    return CFStringCompare(SOSPeerGetID(peerA), SOSPeerGetID(peerB), 0) == kCFCompareEqualTo;
}

static CFMutableArrayRef SOSPeerGetDigestsWithKey(SOSPeerRef peer, CFStringRef key) {
    CFMutableDictionaryRef peerState = SOSPeerGetState(peer);
    CFMutableArrayRef digests = (CFMutableArrayRef)CFDictionaryGetValue(peerState, key);
    if (!digests) {
        digests = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(peerState, key, digests);
        CFReleaseSafe(digests);
    }
    return digests;
}

static void SOSPeerStateSetDigestForKey(CFMutableDictionaryRef peerState, CFStringRef key, CFDataRef digest) {
    if (digest)
        CFDictionarySetValue(peerState, key, digest);
    else
        CFDictionaryRemoveValue(peerState, key);
}

static void SOSPeerAddManifestWithKey(SOSPeerRef peer, CFStringRef key, SOSManifestRef manifest) {
    CFMutableArrayRef digests = SOSPeerGetDigestsWithKey(peer, key);
    CFDataRef digest = SOSManifestGetDigest(manifest, NULL);
    if (digest) {
        CFIndex count = CFArrayGetCount(digests);
        SOSEngineAddManifest(peer->engine, manifest);
        CFIndex ixOfDigest = CFArrayGetFirstIndexOfValue(digests, CFRangeMake(0, count), digest);
        if (ixOfDigest != 0) {
            if (ixOfDigest != kCFNotFound) {
                CFArrayRemoveValueAtIndex(digests, ixOfDigest);
            } else {
                while (count >= kSOSPeerMaxManifestWindowDepth)
                    CFArrayRemoveValueAtIndex(digests, --count);
            }

            CFArrayInsertValueAtIndex(digests, 0, digest);
        }
    } else {
        // pending == NULL => nothing clear history
        CFArrayRemoveAllValues(digests);
    }
}

static SOSPeerRef SOSPeerCreate_Internal(SOSEngineRef engine, CFDictionaryRef persisted, CFStringRef theirPeerID, CFIndex version, CFErrorRef *error) {
    SOSPeerRef p = CFTypeAllocate(SOSPeer, struct __OpaqueSOSPeer, kCFAllocatorDefault);
    p->engine = engine;
    p->peer_id = CFRetainSafe(theirPeerID);
    p->version = version;

    if (persisted) {
        CFDictionaryRef peer_dict = (CFDictionaryRef) persisted;

        int64_t sequenceNumber;
        CFNumberRef seqNo = CFDictionaryGetValue(peer_dict, kSOSPeerSequenceNumberKey);
        if (seqNo) {
            CFNumberGetValue(seqNo, kCFNumberSInt64Type, &sequenceNumber);
            p->sequenceNumber = sequenceNumber;
        }
        CFNumberRef version = CFDictionaryGetValue(peer_dict, kSOSPeerVersionKey);
        if (version)
            CFNumberGetValue(version, kCFNumberCFIndexType, &p->version);
    }

    return p;
}

SOSPeerRef SOSPeerCreateWithEngine(SOSEngineRef engine, CFStringRef peer_id) {
    CFMutableDictionaryRef state = SOSEngineGetPeerState(engine, peer_id);
    return SOSPeerCreate_Internal(engine, state, peer_id, 0, NULL);
}

static bool SOSPeerPersistData(SOSPeerRef peer, CFErrorRef *error)
{
    CFMutableDictionaryRef data_dict = SOSPeerGetState(peer);

    int64_t sequenceNumber = peer->sequenceNumber;
    CFNumberRef seqNo = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &sequenceNumber);
    CFDictionarySetValue(data_dict, kSOSPeerSequenceNumberKey, seqNo);
    CFReleaseNull(seqNo);
    CFDictionarySetValue(data_dict, kSOSPeerMustSendMessageKey, peer->mustSendMessage ? kCFBooleanTrue : kCFBooleanFalse);
    if (peer->version) {
        CFNumberRef version = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &peer->version);
        CFDictionarySetValue(data_dict, kSOSPeerVersionKey, version);
        CFReleaseSafe(version);
    }
    return true;
}

SOSPeerRef SOSPeerCreate(SOSEngineRef engine, SOSPeerInfoRef peerInfo,
                         CFErrorRef *error) {
    if (peerInfo == NULL) {
        SOSCreateError(kSOSErrorUnsupported, CFSTR("Can't create peer without their peer info!"), NULL, error);
        return NULL;
    }

    CFStringRef peer_id = SOSPeerInfoGetPeerID(peerInfo);
    CFDictionaryRef persisted = SOSEngineGetPeerState(engine, peer_id);
    SOSPeerRef peer = SOSPeerCreate_Internal(engine,
                                             persisted,
                                             peer_id,
                                             SOSPeerInfoGetVersion(peerInfo),
                                             error);
    if (peer)
        SOSPeerPersistData(peer, error);
    return peer;
}

SOSPeerRef SOSPeerCreateSimple(SOSEngineRef engine, CFStringRef peer_id, CFIndex version, CFErrorRef *error) {
    
    CFDictionaryRef persisted = SOSEngineGetPeerState(engine, peer_id);
    SOSPeerRef peer = SOSPeerCreate_Internal(engine, persisted, peer_id, version, error);
    if (peer)
        SOSPeerPersistData(peer, error);
    return peer;
}

static void SOSPeerDestroy(CFTypeRef cf) {
    SOSPeerRef peer = (SOSPeerRef)cf;
    SOSPeerPersistData(peer, NULL);
    CFReleaseSafe(peer->peer_id);
}

void SOSPeerDidConnect(SOSPeerRef peer) {
    SOSPeerSetMustSendMessage(peer, true);
    SOSPeerSetProposedManifest(peer, SOSPeerGetConfirmedManifest(peer));
}

CFIndex SOSPeerGetVersion(SOSPeerRef peer) {
    return peer->version;
}

CFStringRef SOSPeerGetID(SOSPeerRef peer) {
    return peer->peer_id;
}

SOSEngineRef SOSPeerGetEngine(SOSPeerRef peer){
    return peer->engine;
}
SOSCoderRef SOSPeerGetCoder(SOSPeerRef peer){
        return peer->coder;
}
void SOSPeerSetCoder(SOSPeerRef peer, SOSCoderRef coder){
        peer->coder = coder;
}

uint64_t SOSPeerNextSequenceNumber(SOSPeerRef peer) {
    return ++peer->sequenceNumber;
}

uint64_t SOSPeerGetMessageVersion(SOSPeerRef peer) {
    return SOSPeerGetVersion(peer);

}

bool SOSPeerMustSendMessage(SOSPeerRef peer) {
    CFBooleanRef must = CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerMustSendMessageKey);
    return must && CFBooleanGetValue(must);
}

void SOSPeerSetMustSendMessage(SOSPeerRef peer, bool sendMessage) {
    CFDictionarySetValue(SOSPeerGetState(peer), kSOSPeerMustSendMessageKey, sendMessage ? kCFBooleanTrue : kCFBooleanFalse);
}

bool SOSPeerSendObjects(SOSPeerRef peer) {
    CFBooleanRef send = CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerSendObjectsKey);
    return send && CFBooleanGetValue(send);
}

void SOSPeerSetSendObjects(SOSPeerRef peer, bool sendObjects) {
    CFDictionarySetValue(SOSPeerGetState(peer), kSOSPeerSendObjectsKey, sendObjects ? kCFBooleanTrue : kCFBooleanFalse);
}

SOSManifestRef SOSPeerGetProposedManifest(SOSPeerRef peer) {
    CFDataRef digest = NULL;
    CFMutableArrayRef proposedDigests = (CFMutableArrayRef)CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerProposedManifestKey);
    if (proposedDigests && CFArrayGetCount(proposedDigests) > 0)
        digest = CFArrayGetValueAtIndex(proposedDigests, 0);
    return SOSEngineGetManifestForDigest(peer->engine, digest);
}

#if 0
static SOSManifestRef SOSPeerGetLocalManifest(SOSPeerRef peer) {
    CFDataRef digest = NULL;
    CFMutableArrayRef localDigests = (CFMutableArrayRef)CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerLocalManifestKey);
    if (localDigests && CFArrayGetCount(localDigests) > 0)
        digest = CFArrayGetValueAtIndex(localDigests, 0);
    return SOSEngineGetManifestForDigest(peer->engine, digest);
}
#endif

SOSManifestRef SOSPeerGetConfirmedManifest(SOSPeerRef peer) {
    return SOSEngineGetManifestForDigest(peer->engine, CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerConfirmedManifestKey));
}

void SOSPeerSetConfirmedManifest(SOSPeerRef peer, SOSManifestRef confirmed) {
    SOSEngineAddManifest(peer->engine, confirmed);
    SOSPeerStateSetDigestForKey(SOSPeerGetState(peer), kSOSPeerConfirmedManifestKey, SOSManifestGetDigest(confirmed, NULL));

    // TODO: Clear only expired pending and local manifests from the array - this clears them all
    // To do so we'd have to track the messageIds we sent to our peer and when we proposed a particular manifest.
    // Then we simply remove the entires from messages older that the one we are confirming now
    //CFArrayRemoveAllValues(SOSPeerGetDigestsWithKey(peer, kSOSPeerProposedManifestKey));
    //CFArrayRemoveAllValues(SOSPeerGetDigestsWithKey(peer, kSOSPeerLocalManifestKey));
}

void SOSPeerAddProposedManifest(SOSPeerRef peer, SOSManifestRef pending) {
    SOSPeerAddManifestWithKey(peer, kSOSPeerProposedManifestKey, pending);
}

void SOSPeerSetProposedManifest(SOSPeerRef peer, SOSManifestRef pending) {
    SOSEngineAddManifest(peer->engine, pending);
    CFMutableArrayRef proposedDigests = SOSPeerGetDigestsWithKey(peer, kSOSPeerProposedManifestKey);
    CFArrayRemoveAllValues(proposedDigests);
    if (pending)
        CFArrayAppendValue(proposedDigests, SOSManifestGetDigest(pending, NULL));
}

void SOSPeerAddLocalManifest(SOSPeerRef peer, SOSManifestRef local) {
    SOSPeerAddManifestWithKey(peer, kSOSPeerLocalManifestKey, local);
}

SOSManifestRef SOSPeerGetPendingObjects(SOSPeerRef peer) {
    return SOSEngineGetManifestForDigest(peer->engine, CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerPendingObjectsKey));
}

void SOSPeerSetPendingObjects(SOSPeerRef peer, SOSManifestRef pendingObjects) {
    SOSEngineAddManifest(peer->engine, pendingObjects);
    SOSPeerStateSetDigestForKey(SOSPeerGetState(peer), kSOSPeerPendingObjectsKey, SOSManifestGetDigest(pendingObjects, NULL));
}

SOSManifestRef SOSPeerGetPendingDeletes(SOSPeerRef peer) {
    return SOSEngineGetManifestForDigest(peer->engine, CFDictionaryGetValue(SOSPeerGetState(peer), kSOSPeerPendingDeletesKey));
}

void SOSPeerSetPendingDeletes(SOSPeerRef peer, SOSManifestRef pendingDeletes) {
    SOSEngineAddManifest(peer->engine, pendingDeletes);
    SOSPeerStateSetDigestForKey(SOSPeerGetState(peer), kSOSPeerPendingDeletesKey, SOSManifestGetDigest(pendingDeletes, NULL));
}

static void SOSMarkDigestInUse(struct SOSDigestVector *mdInUse, CFDataRef digest) {
    if (!isData(digest)) return;
    SOSDigestVectorAppend(mdInUse, CFDataGetBytePtr(digest));
}

static void SOSMarkDigestsInUse(struct SOSDigestVector *mdInUse, CFArrayRef digests) {
    if (!isArray(digests)) return;
    CFDataRef digest = NULL;
    CFArrayForEachC(digests, digest) {
        SOSMarkDigestInUse(mdInUse, digest);
    }
}

// Add all digests we are using to mdInUse
void SOSPeerMarkDigestsInUse(SOSPeerRef peer, struct SOSDigestVector *mdInUse) {
    CFMutableDictionaryRef peerState = SOSPeerGetState(peer);
    SOSMarkDigestInUse(mdInUse, CFDictionaryGetValue(peerState, kSOSPeerPendingObjectsKey));
    SOSMarkDigestInUse(mdInUse, CFDictionaryGetValue(peerState, kSOSPeerPendingDeletesKey));
    SOSMarkDigestInUse(mdInUse, CFDictionaryGetValue(peerState, kSOSPeerConfirmedManifestKey));
    SOSMarkDigestsInUse(mdInUse, CFDictionaryGetValue(peerState, kSOSPeerLocalManifestKey));
    SOSMarkDigestsInUse(mdInUse, CFDictionaryGetValue(peerState, kSOSPeerProposedManifestKey));
}


// absentFromRemote
// AbsentLocally
// additionsFromRemote
// original intent was that digests only got added to pendingObjects. We only know for sure if it is something added locally via api call


bool SOSPeerDidReceiveRemovalsAndAdditions(SOSPeerRef peer, SOSManifestRef absentFromRemote, SOSManifestRef additionsFromRemote,
                                           SOSManifestRef local, CFErrorRef *error) {
    // We assume that incoming manifests are all sorted, and absentFromRemote is disjoint from additionsFromRemote
    bool ok = true;
    SOSManifestRef remoteRemovals = NULL, sharedRemovals = NULL, sharedAdditions = NULL, remoteAdditions = NULL;
    CFDataRef pendingObjectsDigest, pendingDeletesDigest;
    
    // TODO: Simplyfy -- a lot.
    ok = ok && (remoteRemovals = SOSManifestCreateIntersection(absentFromRemote, local, error));           // remoteRemovals = absentFromRemote <Intersected> local
    ok = ok && (sharedRemovals = SOSManifestCreateComplement(remoteRemovals, absentFromRemote, error));    // sharedRemovals = absentFromRemote - remoteRemovals
    ok = ok && (sharedAdditions = SOSManifestCreateIntersection(additionsFromRemote, local, error));         // sharedAdditions = additionsFromRemote <Intersected> local
    ok = ok && (remoteAdditions = SOSManifestCreateComplement(sharedAdditions, additionsFromRemote, error)); // remoteAdditions = additionsFromRemote - sharedAdditions

    secnotice("peer", "%@ R:%@ A:%@ C:%@ D:%@ O:%@", peer, absentFromRemote, additionsFromRemote, SOSPeerGetConfirmedManifest(peer), SOSPeerGetPendingDeletes(peer), SOSPeerGetPendingObjects(peer));

    // TODO: Does the value of SOSPeerSendObjects() matter here?
    pendingObjectsDigest = SOSEnginePatchRecordAndCopyDigest(peer->engine, SOSPeerGetPendingObjects(peer), sharedAdditions, NULL, error);  // PO = PO - sharedAdditions
    pendingDeletesDigest = SOSEnginePatchRecordAndCopyDigest(peer->engine, SOSPeerGetPendingDeletes(peer), sharedRemovals, NULL, error);  // D = D - sharedRemovals
    
    CFMutableDictionaryRef peerState = SOSPeerGetState(peer);
    SOSPeerStateSetDigestForKey(peerState, kSOSPeerPendingObjectsKey, pendingObjectsDigest);
    SOSPeerStateSetDigestForKey(peerState, kSOSPeerPendingDeletesKey, pendingDeletesDigest);
    
    CFReleaseSafe(pendingDeletesDigest);
    CFReleaseSafe(pendingObjectsDigest);
    CFReleaseSafe(remoteRemovals);
    CFReleaseSafe(sharedRemovals);
    CFReleaseSafe(sharedAdditions);
    CFReleaseSafe(remoteAdditions);

    secnotice("peer", "%@ C:%@ D:%@ O:%@", peer, SOSPeerGetConfirmedManifest(peer), SOSPeerGetPendingDeletes(peer), SOSPeerGetPendingObjects(peer));

    return ok;
}

bool SOSPeerDidReceiveConfirmedManifest(SOSPeerRef peer, SOSManifestRef confirmed, SOSManifestRef local, CFErrorRef *error) {
    bool ok = true;
    if (!confirmed) return ok;
    SOSManifestRef confirmedRemovals = NULL, confirmedAdditions = NULL;
    
    // confirmedAdditions = confirmed - previous_confirmed, confirmedRemovals = previous_confirmed - confirmed
    ok &= SOSManifestDiff(SOSPeerGetConfirmedManifest(peer), confirmed, &confirmedRemovals, &confirmedAdditions, error);
    ok &= SOSPeerDidReceiveRemovalsAndAdditions(peer, confirmedRemovals, confirmedAdditions, local, error);

    CFReleaseSafe(confirmedRemovals);
    CFReleaseSafe(confirmedAdditions);
    return ok;
}

bool SOSPeerDataSourceWillCommit(SOSPeerRef peer, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
    SOSManifestRef unconfirmedAdditions = NULL;
    CFDataRef pendingObjectsDigest, pendingDeletesDigest = NULL;

    secnotice("peer", "%@ R:%@ A:%@ C:%@ D:%@ O:%@", peer, removals, additions, SOSPeerGetConfirmedManifest(peer), SOSPeerGetPendingDeletes(peer), SOSPeerGetPendingObjects(peer));

    // Remove confirmed from additions
    // TODO: Add require and check for error
    unconfirmedAdditions = SOSManifestCreateComplement(SOSPeerGetConfirmedManifest(peer), additions, error);
    secnotice("peer", "%@ UA: %@ source: %s", peer, unconfirmedAdditions, source == kSOSDataSourceSOSTransaction ? "sos" : "api");

    pendingObjectsDigest = SOSEnginePatchRecordAndCopyDigest(peer->engine, SOSPeerGetPendingObjects(peer), removals, source == kSOSDataSourceAPITransaction ? unconfirmedAdditions : NULL, error);
    // TODO: Figure out how to update pendingDeletes...
    //pendingDeletesDigest = SOSEnginePatchRecordAndCopyDigest(peer->engine, SOSPeerGetPendingDeletes(peer), removals, NULL, error);

    CFMutableDictionaryRef peerState = SOSPeerGetState(peer);
    
    SOSPeerStateSetDigestForKey(peerState, kSOSPeerPendingObjectsKey, pendingObjectsDigest);
    SOSPeerStateSetDigestForKey(peerState, kSOSPeerPendingDeletesKey, pendingDeletesDigest);
    
    CFReleaseSafe(pendingDeletesDigest);
    CFReleaseSafe(pendingObjectsDigest);
    CFReleaseSafe(unconfirmedAdditions);

    secnotice("peer", "%@ C:%@ D:%@ P:%@", peer, SOSPeerGetConfirmedManifest(peer), SOSPeerGetPendingDeletes(peer), SOSPeerGetPendingObjects(peer));

    return true;
}

