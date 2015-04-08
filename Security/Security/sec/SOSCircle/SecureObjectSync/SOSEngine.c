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
 * SOSEngine.c -  Implementation of a secure object syncing engine
 */

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSDigestVector.h>
#include <SecureObjectSync/SOSInternal.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <corecrypto/ccder.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <utilities/debugging.h>
#include <utilities/iCloudKeychainTrace.h>
#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <securityd/SecItemDataSource.h> // TODO: We can't leave this here.
#include <securityd/SecDbItem.h> // TODO: We can't leave this here.
#include <securityd/SecItemServer.h>// TODO: We can't leave this here.
#include <Security/SecItemPriv.h>// TODO: We can't leave this here.
#include <securityd/SOSCloudCircleServer.h>

//
// MARK: SOSEngine The Keychain database with syncable keychain support.
//

// Key in dataSource for general engine state file.
// This file only has digest entries in it, no manifests.
static const CFStringRef kSOSEngineState = CFSTR("engine-state");

// Keys in state dictionary
static CFStringRef kSOSPeerCoderKey = CFSTR("coder");
static CFStringRef kSOSEngineManifestCacheKey = CFSTR("manifestCache");
static CFStringRef kSOSEnginePeerStateKey = CFSTR("peerState");
static CFStringRef kSOSEnginePeerIDsKey = CFSTR("peerIDs");
static CFStringRef kSOSEngineIDKey = CFSTR("id");

/* SOSEngine implementation. */
struct __OpaqueSOSEngine {
    CFRuntimeBase _base;
    SOSDataSourceRef dataSource;
    CFStringRef myID;                       // My peerID in the circle
    SOSManifestRef manifest;                // Explicitly not in cache since it's not persisted?
    // We need to address the issues of corrupt keychain items
    SOSManifestRef unreadble;               // Possibly by having a set of unreadble items, to which we
    // add any corrupted items in the db that have yet to be deleted.
    // This happens if we notce corruption during a (read only) query.
    // We would also perma-subtract unreadable from manifest whenever
    // anyone asked for manifest.  This result would be cached in
    // The manifestCache below, so we just need a key into the cache
    CFDataRef localMinusUnreadableDigest;   // or a digest (CFDataRef of the right size).
    
    CFMutableDictionaryRef manifestCache;   // digest -> ( refcount, manifest )
    CFMutableDictionaryRef peerState;       // peerId -> mutable array of digests
    CFArrayRef peerIDs;

    dispatch_queue_t queue;
};

static bool SOSEngineLoad(SOSEngineRef engine, CFErrorRef *error);
 
 
static CFStringRef SOSPeerIDArrayCreateString(CFArrayRef peerIDs) {
    return peerIDs ? CFStringCreateByCombiningStrings(kCFAllocatorDefault, peerIDs, CFSTR(" ")) : CFSTR("");
 }
 
static CFStringRef SOSEngineCopyFormattingDesc(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSEngineRef engine = (SOSEngineRef)cf;
    CFStringRef tpDesc = SOSPeerIDArrayCreateString(engine->peerIDs);
    CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, formatOptions, CFSTR("<Engine %@ peers %@ MC[%d] PS[%d]>"), engine->myID, tpDesc, engine->manifestCache ? (int)CFDictionaryGetCount(engine->manifestCache) : 0, engine->peerState ? (int)CFDictionaryGetCount(engine->peerState) : 0);
    CFReleaseSafe(tpDesc);
    return desc;
 }
 
static CFStringRef SOSEngineCopyDebugDesc(CFTypeRef cf) {
    return SOSEngineCopyFormattingDesc(cf, NULL);
 }
 
static dispatch_queue_t sEngineQueue;
static CFDictionaryRef sEngineMap;
 
CFGiblisWithFunctions(SOSEngine, NULL, NULL, NULL, NULL, NULL, SOSEngineCopyFormattingDesc, SOSEngineCopyDebugDesc, NULL, NULL, ^{
    sEngineQueue = dispatch_queue_create("SOSEngine queue", DISPATCH_QUEUE_SERIAL);
    sEngineMap = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
});

#define _LOG_RAW_MESSAGES 0
void logRawMessage(CFDataRef message, bool sending, uint64_t seqno)
{
#if _LOG_RAW_MESSAGES
    CFStringRef hexMessage = NULL;
    if (message) {
        hexMessage = CFDataCopyHexString(message);
        if (sending)
            secnoticeq("engine", "%s RAW%1d %@", sending ? "send" : "recv", seqno?2:0, hexMessage);
        else
            secnoticeq("engine", "%s RAWx %@", sending ? "send" : "recv", hexMessage);  // we don't know vers of received msg here
    }
    CFReleaseSafe(hexMessage);
#endif
}
//
// Peer state layout.  WRONG! It's an array now
// The peer state is an array.
// The first element of the array is a dictionary with any number of keys and
// values in it (for future expansion) such as changing the digest size or type
// or remebering boolean flags for a peers sake.
// The next three are special in that they are manifest digests with special
// meaning and rules as to how they are treated (These are dynamically updated
// based on database activity so they have a fully history of all changes made
// to the local db. The first is the manifest representing the pendingObjects
// to send to the other peer.  This is normally only ever appending to, and in
// particular with transactions originating from the Keychain API that affect
// syncable items will need to add the new objects digests to the pendingObjects list
// while adding the digests of any tombstones encountered to the extra list.

CFStringRef SOSEngineGetMyID(SOSEngineRef engine) {
    // TODO: this should not be needed
    return engine->myID;
}

// TEMPORARY: Get the list of IDs for cleanup, this shouldn't be used instead it should iterate KVS.
CFArrayRef SOSEngineGetPeerIDs(SOSEngineRef engine) {
    return engine->peerIDs;
}

SOSManifestRef SOSEngineGetManifestForDigest(SOSEngineRef engine, CFDataRef digest) {
    if (!engine->manifestCache || !digest) return NULL;
    SOSManifestRef manifest = (SOSManifestRef)CFDictionaryGetValue(engine->manifestCache, digest);
    if (!manifest) return NULL;
    if (CFGetTypeID(manifest) != SOSManifestGetTypeID()) {
        secerror("dropping corrupt manifest for %@ from cache", digest);
        CFDictionaryRemoveValue(engine->manifestCache, digest);
        return NULL;
    }
    
    return manifest;
}

void SOSEngineAddManifest(SOSEngineRef engine, SOSManifestRef manifest) {
    CFDataRef digest = SOSManifestGetDigest(manifest, NULL);
    if (digest) {
        if (!engine->manifestCache)
            engine->manifestCache = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryAddValue(engine->manifestCache, digest, manifest);
    }
}

CFDataRef SOSEnginePatchRecordAndCopyDigest(SOSEngineRef engine, SOSManifestRef base, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
    CFDataRef digest = NULL;
    SOSManifestRef manifest = SOSManifestCreateWithPatch(base, removals, additions, error);
    if (manifest) {
        SOSEngineAddManifest(engine, manifest);
        digest = CFRetainSafe(SOSManifestGetDigest(manifest, NULL));
    }
    CFReleaseSafe(manifest);
    return digest;
}

static bool SOSEngineHandleManifestUpdates(SOSEngineRef engine, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
    __block struct SOSDigestVector mdInCache = SOSDigestVectorInit;
    struct SOSDigestVector mdInUse = SOSDigestVectorInit;
    struct SOSDigestVector mdUnused = SOSDigestVectorInit;
    struct SOSDigestVector mdMissing = SOSDigestVectorInit;
    CFStringRef peerID = NULL;
    bool ok = true;
    
    require_quiet(engine->peerState, exit); // Not a failure no work to do

    if(engine->peerIDs){
        CFArrayForEachC(engine->peerIDs, peerID) {
            SOSPeerRef peer = SOSPeerCreateWithEngine(engine, peerID);
            if (removals || additions)
                ok &= SOSPeerDataSourceWillCommit(peer, source, removals, additions, error);
            SOSPeerMarkDigestsInUse(peer, &mdInUse);
            CFReleaseSafe(peer);
        }
    }
    if(engine->manifestCache){
        CFDictionaryForEach(engine->manifestCache, ^(const void *key, const void *value) {
            CFDataRef digest = (CFDataRef)key;
            if (isData(digest))
                SOSDigestVectorAppend(&mdInCache, CFDataGetBytePtr(digest));
        });
        
        // Delete unused manifests.
        SOSDigestVectorDiff(&mdInCache, &mdInUse, &mdUnused, &mdMissing);
        SOSManifestRef unused = SOSManifestCreateWithDigestVector(&mdUnused, NULL);
        SOSManifestForEach(unused, ^(CFDataRef digest, bool *stop) {
            if (digest)
                CFDictionaryRemoveValue(engine->manifestCache, digest);
        });
        CFReleaseSafe(unused);
    }
    // Delete unused peerState
    if (engine->peerState && engine->peerIDs) {
        CFMutableDictionaryRef newPeerState = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFArrayForEachC(engine->peerIDs, peerID) {
            CFTypeRef value = CFDictionaryGetValue(engine->peerState, peerID);
            if (value)
                CFDictionarySetValue(newPeerState, peerID, value);
        }
        CFDictionaryForEach(engine->peerState, ^(const void *key, const void *value) {
            if(isDictionary(value) && !CFDictionaryContainsKey(newPeerState, key)){
                CFMutableDictionaryRef untrustedStuff = (CFMutableDictionaryRef)value;
                CFDataRef untrustedCoder = (CFDataRef)CFDictionaryGetValue(untrustedStuff, kSOSPeerCoderKey);
                if(untrustedCoder){
                    CFMutableDictionaryRef untrustedDict = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault, kSOSPeerCoderKey, untrustedCoder, NULL);
                    CFDictionarySetValue(newPeerState, key, untrustedDict);
                    CFReleaseNull(untrustedDict);
                }
            }
        });
        CFReleaseSafe(engine->peerState);
        engine->peerState = newPeerState;
    }

exit:
    SOSDigestVectorFree(&mdInCache);
    SOSDigestVectorFree(&mdInUse);
    SOSDigestVectorFree(&mdUnused);
    SOSDigestVectorFree(&mdMissing);
    return ok;
}

static CFDataRef SOSEngineCopyState(SOSEngineRef engine, CFErrorRef *error) {
    CFDataRef der = NULL;
    CFMutableDictionaryRef state = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    if (engine->myID) CFDictionarySetValue(state, kSOSEngineIDKey, engine->myID);
    if (engine->peerIDs) CFDictionarySetValue(state, kSOSEnginePeerIDsKey, engine->peerIDs);
    if (engine->peerState) CFDictionarySetValue(state, kSOSEnginePeerStateKey, engine->peerState);
    if (engine->manifestCache) {
        CFMutableDictionaryRef mfc = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(state, kSOSEngineManifestCacheKey, mfc);
        CFDictionaryForEach(engine->manifestCache, ^(const void *key, const void *value) {
            SOSManifestRef mf = (SOSManifestRef)value;
            if (mf && (CFGetTypeID(mf) == SOSManifestGetTypeID()))
                CFDictionarySetValue(mfc, key, SOSManifestGetData(mf));
        });
        CFReleaseSafe(mfc);
    }
    der = kc_plist_copy_der(state, error);
    CFReleaseSafe(state);
    secnotice("engine", "%@", engine);
    return der;
}

static bool SOSEngineSave(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    CFDataRef derState = SOSEngineCopyState(engine, error);
    bool ok = derState && SOSDataSourceSetStateWithKey(engine->dataSource, txn, kSOSEngineState, kSecAttrAccessibleAlways, derState, error);
    CFReleaseSafe(derState);
    return ok;
}

static bool SOSEngineUpdateLocalManifest_locked(SOSEngineRef engine, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
    bool ok = true;
    if (engine->manifest) {
        SOSManifestRef updatedManifest = SOSManifestCreateWithPatch(engine->manifest, removals, additions, error);
        if (updatedManifest)
            CFAssignRetained(engine->manifest, updatedManifest);

        // Update Peer Manifests. -- Shouldn't this be deferred until we apply our toAdd and toDel to the local manifest?
        ok &= SOSEngineHandleManifestUpdates(engine, source, removals, additions, error);
    }
    return ok;
}

static bool SOSEngineUpdateChanges(SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error)
{
    secnotice("engine", "%s %s dels:%@ adds:%@", phase == kSOSDataSourceTransactionWillCommit ? "will-commit" : phase == kSOSDataSourceTransactionDidCommit ? "did-commit" : "did-rollback", source == kSOSDataSourceSOSTransaction ? "sos" : "api", removals, additions);
    bool ok = true;
    switch (phase) {
        case kSOSDataSourceTransactionDidRollback:
            ok &= SOSEngineLoad(engine, error);
            break;
        case kSOSDataSourceTransactionWillCommit: {
            ok &= SOSEngineUpdateLocalManifest_locked(engine, source, removals, additions, error);
            // Write SOSEngine and SOSPeer state to disk if dirty
            ok &= SOSEngineSave(engine, txn, error);
            break;
        }
        case kSOSDataSourceTransactionDidCommit:
            break;
    }
    return ok;
}

static void SOSEngineSetTrustedPeers(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers) {
    const bool wasInCircle = engine->myID;
    const bool isInCircle = myPeerID;
    const bool inCircleChanged = wasInCircle != isInCircle;

    CFStringRef peerID = NULL;
    CFRetainAssign(engine->myID, myPeerID);

    if(trustedPeers != NULL && CFArrayGetCount(trustedPeers) != 0){
        CFReleaseNull(engine->peerIDs);
        engine->peerIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFArrayForEachC(trustedPeers, peerID){
            CFArrayAppendValue((CFMutableArrayRef)engine->peerIDs, peerID);
        };
    }
    else{
        engine->peerIDs = NULL;
    }
    // If we entered a circle of more than 2 or our last peer left we need to do stuff
    if (inCircleChanged) {
        if (isInCircle) {
            CFErrorRef dsError = NULL;
            if (!(engine->manifest = SOSDataSourceCopyManifest(engine->dataSource, &dsError))) {
                secerror("failed to load manifest from datasource: %@", dsError);
                CFReleaseNull(dsError);
            }
            SOSDataSourceSetNotifyPhaseBlock(engine->dataSource, ^(SOSDataSourceRef ds, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, struct SOSDigestVector *removals, struct SOSDigestVector *additions) {
                SOSManifestRef mfdel = SOSManifestCreateWithDigestVector(removals, NULL);
                SOSManifestRef mfadd = SOSManifestCreateWithDigestVector(additions, NULL);
                dispatch_block_t processUpdates = ^{
                    CFErrorRef localError = NULL;
                    if (!SOSEngineUpdateChanges(engine, txn, phase, source, mfdel, mfadd, &localError)) {
                        secerror("updateChanged failed: %@", localError);
                    }
                    CFReleaseSafe(localError);
                    CFReleaseSafe(mfdel);
                    CFReleaseSafe(mfadd);
                };

                // WARNING: This will deadlock the engine if you call a
                // SecItem API function while holding the engine lock!
                // However making this async right now isn't safe yet either
                // Due to some code in the enginer using Get v/s copy to
                // access some of the values that would be modified
                // asynchronously here since the engine is coded as if
                // running on a serial queue.
                dispatch_sync(engine->queue, processUpdates);
            });
        } else {
            SOSDataSourceSetNotifyPhaseBlock(engine->dataSource, ^(SOSDataSourceRef ds, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, struct SOSDigestVector *removals, struct SOSDigestVector *additions) {
                secnoticeq("engine", "No peers to notify");     // TODO: DEBUG - remove this
            });
            CFReleaseNull(engine->manifest);
        }
    }
}

static bool SOSEngineSetState(SOSEngineRef engine, CFDataRef state, CFErrorRef *error) {
    bool ok = true;
    if (state) {
        CFMutableDictionaryRef dict = NULL;
        const uint8_t *der = CFDataGetBytePtr(state);
        const uint8_t *der_end = der + CFDataGetLength(state);
        der = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *)&dict, error, der, der_end);
        if (der && der != der_end) {
            ok = SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("trailing %td bytes at end of state"), der_end - der);
        }
        if (ok) {
            SOSEngineSetTrustedPeers(engine, (CFStringRef)CFDictionaryGetValue(dict, kSOSEngineIDKey),
                                     (CFArrayRef)CFDictionaryGetValue(dict, kSOSEnginePeerIDsKey));
            CFRetainAssign(engine->peerState, (CFMutableDictionaryRef)CFDictionaryGetValue(dict, kSOSEnginePeerStateKey));
            
            CFReleaseNull(engine->manifestCache);
            CFMutableDictionaryRef mfc = (CFMutableDictionaryRef)CFDictionaryGetValue(dict, kSOSEngineManifestCacheKey);
            if (mfc) {
                engine->manifestCache = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
                CFDictionaryForEach(mfc, ^(const void *key, const void *value) {
                    CFDataRef data = (CFDataRef)value;
                    if (isData(data)) {
                        SOSManifestRef mf = SOSManifestCreateWithData(data, NULL);
                        if (mf)
                            CFDictionarySetValue(engine->manifestCache, key, mf);
                        CFReleaseSafe(mf);
                    }
                });
            }
        }
        CFReleaseNull(dict);
    }
    secnotice("engine", "%@", engine);
    return ok;
}

static bool SOSEngineLoad(SOSEngineRef engine, CFErrorRef *error) {
    CFDataRef state = SOSDataSourceCopyStateWithKey(engine->dataSource, kSOSEngineState, kSecAttrAccessibleAlways, error);
    bool ok = state && SOSEngineSetState(engine, state, error);
    CFReleaseSafe(state);
    return ok;
}

static void CFArraySubtract(CFMutableArrayRef from, CFArrayRef remove) {
    if (remove) {
        CFArrayForEach(remove, ^(const void *value) {
            CFArrayRemoveAllValue(from, value);
        });
    }
}

static CFMutableArrayRef CFArrayCreateDifference(CFAllocatorRef alloc, CFArrayRef set, CFArrayRef remove) {
    CFMutableArrayRef result;
    if (!set) {
        result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    } else {
        result = CFArrayCreateMutableCopy(alloc, 0, set);

        if (remove)
            CFArraySubtract(result, remove);
    }

    return result;
}

void SOSEngineCircleChanged_locked(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers, CFArrayRef untrustedPeers) {
    CFMutableArrayRef addedPeers = CFArrayCreateDifference(kCFAllocatorDefault, trustedPeers, engine->peerIDs);
    CFMutableArrayRef deletedPeers = CFArrayCreateDifference(kCFAllocatorDefault, engine->peerIDs, trustedPeers);

    CFStringRef tpDesc = SOSPeerIDArrayCreateString(trustedPeers);
    CFStringRef apDesc = SOSPeerIDArrayCreateString(addedPeers);
    CFStringRef dpDesc = SOSPeerIDArrayCreateString(deletedPeers);
    secnotice("engine", "trusted %@ added %@ removed %@", tpDesc, apDesc, dpDesc);
    CFReleaseSafe(dpDesc);
    CFReleaseSafe(apDesc);
    CFReleaseSafe(tpDesc);

    SOSEngineSetTrustedPeers(engine, myPeerID, trustedPeers);

    // Remove any cached state for peers we no longer use but keep coders alive
    if (deletedPeers && CFArrayGetCount(deletedPeers) && engine->peerState) {
        CFStringRef peerID = NULL;
        CFArrayForEachC(deletedPeers, peerID) {
            CFMutableDictionaryRef peer_data = (CFMutableDictionaryRef) CFDictionaryGetValue(engine->peerState, peerID);
            CFDataRef coder_data = isDictionary(peer_data) ? (CFDataRef) CFDictionaryGetValue(peer_data, kSOSPeerCoderKey) : NULL;

            if(isData(coder_data) &&
               untrustedPeers && CFArrayContainsValue(untrustedPeers, CFRangeMake(0, CFArrayGetCount(untrustedPeers)), peerID)) {
                CFRetainSafe(coder_data);
                CFDictionaryRemoveAllValues(peer_data);
                CFDictionaryAddValue(peer_data, kSOSPeerCoderKey, coder_data);
                CFReleaseSafe(coder_data);
            } else {
                CFDictionaryRemoveValue(engine->peerState, peerID);
            }
        }
        // Run though all peers and only cache manifests for peers we still have
        // TODO: Factor out gc from SOSEngineHandleManifestUpdates and just call that
        SOSEngineHandleManifestUpdates(engine, kSOSDataSourceSOSTransaction, NULL, NULL, NULL);
    }
    
    CFReleaseNull(addedPeers);
    CFReleaseNull(deletedPeers);
    
}

#if 0
static SOSManifestRef SOSEngineCopyCleanManifest(SOSEngineRef engine, CFErrorRef *error) {
    SOSManifestRef localMinusUnreadable;
    }
#endif

// Initialize the engine if a load fails.  Basically this is our first time setup
static bool SOSEngineInit(SOSEngineRef engine, CFErrorRef *error) {
    bool ok = true;
    secnotice("engine", "new engine for datasource named %@", SOSDataSourceGetName(engine->dataSource));
    return ok;
}

// Called by our DataSource in its constructor
SOSEngineRef SOSEngineCreate(SOSDataSourceRef dataSource, CFErrorRef *error) {
    SOSEngineRef engine = NULL;
    engine = CFTypeAllocate(SOSEngine, struct __OpaqueSOSEngine, kCFAllocatorDefault);
    engine->dataSource = dataSource;
    engine->queue = dispatch_queue_create("engine", DISPATCH_QUEUE_SERIAL);
    CFErrorRef engineError = NULL;
    if (!SOSEngineLoad(engine, &engineError)) {
        secwarning("engine failed load state starting with nothing %@", engineError);
        CFReleaseNull(engineError);
        if (!SOSEngineInit(engine, error)) {
            secerror("engine failed to initialze %@ giving up", engineError);
        }
    }
    return engine;
}


//
// MARK: SOSEngine API
//

void SOSEngineDispose(SOSEngineRef engine) {
    // NOOP Engines stick around forever to monitor dataSource changes.
}

static SOSManifestRef SOSEngineCopyManifest_locked(SOSEngineRef engine, CFErrorRef *error) {
    return CFRetainSafe(engine->manifest);
}

/* Handle incoming message from peer p.  Return false if there was an error, true otherwise. */
static bool SOSEngineHandleMessage_locked(SOSEngineRef engine, CFStringRef peerID, SOSMessageRef message,
                                          SOSTransactionRef txn, bool *commit, bool *somethingChanged, CFErrorRef *error) {
    SOSPeerRef peer = SOSPeerCreateWithEngine(engine, peerID);
    CFStringRef peerDesc = NULL;
    SOSManifestRef localManifest = NULL;
    SOSManifestRef allAdditions = NULL;
    SOSManifestRef confirmed = NULL;
    SOSManifestRef base = NULL;
    SOSManifestRef confirmedRemovals = NULL, confirmedAdditions = NULL;
    __block struct SOSDigestVector receivedObjects = SOSDigestVectorInit;

    // Check for unknown criticial extensions in the message, and handle
    // any other extensions we support
    __block bool ok = true;
    __block struct SOSDigestVector dvadd = SOSDigestVectorInit;

    require_action_quiet(peer, exit, ok = SOSErrorCreate(errSecParam, error, NULL, CFSTR("Couldn't create peer with Engine for %@"), peerID));
    peerDesc = CFCopyDescription(peer);

    SOSMessageWithExtensions(message, true, ^(CFDataRef oid, bool isCritical, CFDataRef extension, bool *stop) {
        // OMFG a Critical extension what shall I do!
        ok = SOSErrorCreate(kSOSErrorNotReady, error, NULL, CFSTR("Unknown criticial extension in peer message"));
        *stop = true;
    });
    require_quiet(ok, exit);

    // Merge Objects from the message into our DataSource.
    // Should we move the transaction to the SOSAccount level?
    require_quiet(ok &= SOSMessageWithSOSObjects(message, engine->dataSource, error, ^(SOSObjectRef peersObject, bool *stop) {
        CFDataRef digest = SOSObjectCopyDigest(engine->dataSource, peersObject, error);
        if (!digest) {
            *stop = true;
            *commit = false;
            secerror("%@ peer sent bad object: %@, rolling back changes", SOSPeerGetID(peer), error ? *error : NULL);
            return;
        }
        SOSDigestVectorAppend(&receivedObjects, CFDataGetBytePtr(digest));
        SOSMergeResult mr = SOSDataSourceMergeObject(engine->dataSource, txn, peersObject, NULL, error);
        // TODO: If the mr is kSOSMergeLocalObject most of the time (or all of the time),
        // consider asking the peer to stop sending us objects, and send it objects instead.
        ok &= (mr != kSOSMergeFailure);
        if (!ok) {
            *stop = true;
            *commit = false;
            // TODO: Might want to change to warning since the race of us locking after ckd sends us a message could cause db locked errors here.
            secerror("%@ SOSDataSourceMergeObject failed %@ rolling back changes", SOSPeerGetID(peer), error ? *error : NULL);
        } else if (mr==kSOSMergePeersObject || mr==kSOSMergeCreatedObject) {
            *somethingChanged = true;
        } else {
            // mr == kSOSMergeLocalObject
            // Ensure localObject is in local manifest (possible corruption) by posting an update when we are done.
            SOSDigestVectorAppend(&dvadd, CFDataGetBytePtr(digest));
        }
        CFReleaseSafe(digest);
    }), exit);
    struct SOSDigestVector dvunion = SOSDigestVectorInit;
    SOSDigestVectorSort(&receivedObjects);
    SOSDigestVectorUnionSorted(SOSManifestGetDigestVector(SOSMessageGetAdditions(message)), &receivedObjects, &dvunion);
    allAdditions = SOSManifestCreateWithDigestVector(&dvunion, error);
    SOSDigestVectorFree(&receivedObjects);
    SOSDigestVectorFree(&dvunion);

    if (dvadd.count) {
        // Ensure any objects that we received and have localally already are actually in our local manifest
        SOSManifestRef mfadd = SOSManifestCreateWithDigestVector(&dvadd, error);
        SOSDigestVectorFree(&dvadd);
        SOSEngineUpdateLocalManifest_locked(engine, kSOSDataSourceSOSTransaction, NULL, mfadd, error);
        CFReleaseSafe(mfadd);
    }

    // ---- Don't use local or peer manifests from above this line, since commiting the SOSDataSourceWith transaction might change them ---

    // Take a snapshot of our dataSource's local manifest.
    require_quiet(ok = localManifest = SOSEngineCopyManifest_locked(engine, error), exit);

    CFDataRef baseDigest = SOSMessageGetBaseDigest(message);
    CFDataRef proposedDigest = SOSMessageGetProposedDigest(message);
    
#if 0
    // I believe this is no longer needed now that we have eliminated extra,
    // Since this is handeled below once we get a confirmed manifest from our
    // peer.
    
    // If we just received a L00 reset pendingObjects to localManifest
    if (!baseDigest && !proposedDigest) {
        SOSPeerSetPendingObjects(peer, localManifest);
        secnotice("engine", "SOSPeerSetPendingObjects: %@", localManifest);
    }
#endif

    base = CFRetainSafe(SOSEngineGetManifestForDigest(engine, baseDigest));
    confirmed = CFRetainSafe(SOSEngineGetManifestForDigest(engine, SOSMessageGetSenderDigest(message)));
    if (!confirmed) {
        if (SOSManifestGetCount(SOSMessageGetRemovals(message)) || SOSManifestGetCount(allAdditions)) {
            if (base || !baseDigest) {
                confirmed = SOSManifestCreateWithPatch(base, SOSMessageGetRemovals(message), allAdditions, error);
            }
            if (!confirmed) {
                confirmedRemovals = CFRetainSafe(SOSMessageGetRemovals(message));
                confirmedAdditions = CFRetainSafe(allAdditions);
            }
        } else if (baseDigest) {
            confirmed = CFRetainSafe(base);
            secerror("Protocol error send L00 - figure out later base: %@", base);
        }
    }
    secnotice("engine", "Confirmed: %@ base: %@", confirmed, base);
    if (confirmed)
        ok &= SOSManifestDiff(SOSPeerGetConfirmedManifest(peer), confirmed, &confirmedRemovals, &confirmedAdditions, error);
    if (confirmedRemovals || confirmedAdditions)
        ok &= SOSPeerDidReceiveRemovalsAndAdditions(peer, confirmedRemovals, confirmedAdditions, localManifest, error);
    SOSPeerSetConfirmedManifest(peer, confirmed);

    // ---- SendObjects and extra->pendingObjects promotion dance ----

    // The first block of code below sets peer.sendObjects to true when we receive a L00 and the second block
    // moves extra to pendingObjects once we receive a confirmed manifest in or after the L00.
    if (!baseDigest && !proposedDigest) {
        SOSPeerSetSendObjects(peer, true);
    }

    // TODO: should this not depend on SOSPeerSendObjects?:
    if (confirmed /* && SOSPeerSendObjects(peer)*/) {
        SOSManifestRef allExtra = NULL;
        ok &= SOSManifestDiff(confirmed, localManifest, NULL, &allExtra, error);
        secnotice("engine", "%@ confirmed %@ setting O:%@", SOSPeerGetID(peer), confirmed, allExtra);
        SOSPeerSetPendingObjects(peer, allExtra);
        CFReleaseSafe(allExtra);
    }

exit:
    secnoticeq("engine", "recv %@ %@", SOSPeerGetID(peer), message);
    secnoticeq("peer", "recv %@ -> %@", peerDesc, peer);

    CFReleaseNull(base);
    CFReleaseSafe(confirmed);
    CFReleaseSafe(localManifest);
    CFReleaseSafe(peerDesc);
    CFReleaseSafe(allAdditions);
    CFReleaseSafe(confirmedRemovals);
    CFReleaseSafe(confirmedAdditions);
    CFReleaseSafe(peer);
    return ok;
}

static CFDataRef SOSEngineCopyObjectDER(SOSEngineRef engine, SOSObjectRef object, CFErrorRef *error) {
    CFDataRef der = NULL;
    CFDictionaryRef plist = SOSObjectCopyPropertyList(engine->dataSource, object, error);
    if (plist) {
        der = kc_plist_copy_der(plist, error);
        CFRelease(plist);
                            }
    return der;
                        }

static CFDataRef SOSEngineCreateMessage_locked(SOSEngineRef engine, SOSPeerRef peer,
                                               CFErrorRef *error, SOSEnginePeerMessageSentBlock *sent) {
    SOSManifestRef local = SOSEngineCopyManifest_locked(engine, error);
    __block SOSMessageRef message = SOSMessageCreate(kCFAllocatorDefault, SOSPeerGetMessageVersion(peer), error);
    SOSManifestRef confirmed = SOSPeerGetConfirmedManifest(peer);
    SOSManifestRef pendingObjects = SOSPeerGetPendingObjects(peer);
    SOSManifestRef objectsSent = NULL;
    SOSManifestRef proposed = NULL;
    SOSManifestRef allMissing = NULL;
    SOSManifestRef allExtra = NULL;
    SOSManifestRef extra = NULL;
    SOSManifestRef excessPending = NULL;
    SOSManifestRef missing = NULL;
    SOSManifestRef deleted = SOSPeerGetPendingDeletes(peer);
    SOSManifestRef excessDeleted = NULL;
    CFDataRef result = NULL;
    bool ok;
    
    ok = SOSManifestDiff(confirmed, local, &allMissing, &allExtra, error);
    ok = ok && SOSManifestDiff(allExtra, pendingObjects, &extra, &excessPending, error);
    if (SOSManifestGetCount(excessPending)) {
        secerror("%@ ASSERTION FAILURE excess pendingObjects: %@", peer, excessPending);
        // Remove excessPending from pendingObjects since they are either
        // already in confirmed or not in local, either way there is no point
        // keeping them in pendingObjects.

        pendingObjects = SOSManifestCreateComplement(excessPending, pendingObjects, error);
        SOSPeerSetPendingObjects(peer, pendingObjects);
        CFReleaseSafe(pendingObjects);
        ok = false;
    }
    ok = ok && SOSManifestDiff(allMissing, deleted, &missing, &excessDeleted, error);
    if (SOSManifestGetCount(excessDeleted)) {
        secerror("%@ ASSERTION FAILURE excess deleted: %@", peer, excessDeleted);
        ok = false;
    }
    (void)ok;   // Dead store
    CFReleaseNull(allExtra);
    CFReleaseNull(excessPending);
    CFReleaseNull(allMissing);
    CFReleaseNull(excessDeleted);
    
    // Send state for peer 7T0M+TD+A7HZ0frC5oHZnmdR0G: [LCP][os] P: 0, E: 0, M: 0
    secnoticeq("engine", "Send state for peer %@: [%s%s%s][%s%s] P: %zu, E: %zu, M: %zu", SOSPeerGetID(peer),
               local ? "L":"l",
               confirmed ? "C":"0",
               pendingObjects ? "P":"0",
               SOSPeerSendObjects(peer) ? "O":"o",
               SOSPeerMustSendMessage(peer) ? "S":"s",
               SOSManifestGetCount(pendingObjects),
               SOSManifestGetCount(extra),
               SOSManifestGetCount(missing)
               );

    if (confirmed) {
        // TODO: Because of not letting things terminate while we have extra left
        // we might send objects when we didn't need to, but there is always an
        // extra roundtrip required for objects that we assume the other peer
        // should have already.
        // TODO: If there are extra objects left, calling this function is not
        // idempotent we should check if pending is what we are about to send and not send anything in this case.
        if (SOSManifestGetCount(pendingObjects) == 0 && SOSManifestGetCount(extra) == 0)
            SOSPeerSetSendObjects(peer, false);

        if (CFEqualSafe(local, SOSPeerGetProposedManifest(peer)) && !SOSPeerMustSendMessage(peer)) {
            bool send = false;
            if (CFEqual(confirmed, local)) {
                secnoticeq("engine", "synced <No MSG> %@",  peer);
            } else if (SOSManifestGetCount(pendingObjects) == 0 /* TODO: No entries moved from extra to pendingObjects. */
                && SOSManifestGetCount(missing) == 0) {
                secnoticeq("engine", "waiting <MSG not resent> %@", peer);
            } else {
                send = true;
            }
            if (!send) {
                CFReleaseSafe(local);
                CFReleaseSafe(message);
                CFReleaseNull(extra);
                CFReleaseNull(missing);
                return CFDataCreate(kCFAllocatorDefault, NULL, 0);
            }
        }

        if (SOSManifestGetCount(pendingObjects)) {
            // If we have additions and we need to send objects send them.
            __block size_t objectsSize = 0;
            __block struct SOSDigestVector dv = SOSDigestVectorInit;
            __block struct SOSDigestVector dvdel = SOSDigestVectorInit;
            if (!SOSDataSourceForEachObject(engine->dataSource, pendingObjects, error, ^void(CFDataRef key, SOSObjectRef object, bool *stop) {
                CFErrorRef localError = NULL;
                CFDataRef digest = NULL;
                CFDataRef der = NULL;
                if (!object) {
                    const uint8_t *d = CFDataGetBytePtr(key);
                    secerrorq("%@ object %02X%02X%02X%02X dropping from manifest: not found in datasource",
                               SOSPeerGetID(peer), d[0], d[1], d[2], d[3]);
                    SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(key));
                } else if (!(der = SOSEngineCopyObjectDER(engine, object, &localError))
                           || !(digest = SOSObjectCopyDigest(engine->dataSource, object, &localError))) {
                    if (SecErrorGetOSStatus(localError) == errSecDecode) {
                        // Decode error, we need to drop these objects from our manifests
                        const uint8_t *d = CFDataGetBytePtr(key);
                        secerrorq("%@ object %02X%02X%02X%02X dropping from manifest: %@",
                            SOSPeerGetID(peer), d[0], d[1], d[2], d[3], localError);
                        SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(key));
                        CFRelease(localError);
                    } else {
                        // Stop iterating and propagate out all other errors.
                        const uint8_t *d = CFDataGetBytePtr(key);
                        secwarning("%@ object %02X%02X%02X%02X in SOSDataSourceForEachObject: %@",
                            SOSPeerGetID(peer), d[0], d[1], d[2], d[3], localError);
                        *stop = true;
                        CFErrorPropagate(localError, error);
                        CFReleaseNull(message);
                    }
                } else {
                    if (!CFEqual(key, digest)) {
                        const uint8_t *d = CFDataGetBytePtr(key);
                        const uint8_t *e = CFDataGetBytePtr(digest);
                        secwarning("@ object %02X%02X%02X%02X is really %02X%02X%02X%02X dropping from local manifest", d[0], d[1], d[2], d[3], e[0], e[1], e[2], e[3]);
                        SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(key));
                    }

                    size_t objectLen = (size_t)CFDataGetLength(der);
                    if (SOSMessageAppendObject(message, der, &localError)) {
                        SOSDigestVectorAppend(&dv, CFDataGetBytePtr(digest));
                    } else {
                        const uint8_t *d = CFDataGetBytePtr(digest);
                        CFStringRef hexder = CFDataCopyHexString(der);
                        secerrorq("%@ object %02X%02X%02X%02X der: %@ dropping from manifest: %@",
                                  SOSPeerGetID(peer), d[0], d[1], d[2], d[3], hexder, localError);
                        CFReleaseNull(hexder);
                        CFReleaseNull(message);
                        // Since we can't send these objects let's assume they are bad too?
                        SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(digest));
                    }
                    objectsSize += objectLen;
                    if (objectsSize > kSOSMessageMaxObjectsSize)
                        *stop = true;
                }
                CFReleaseSafe(der);
                CFReleaseSafe(digest);
            })) {
                CFReleaseNull(message);
            }
            if (dv.count)
                objectsSent = SOSManifestCreateWithDigestVector(&dv, error);
            if (dvdel.count) {
                CFErrorRef localError = NULL;
                SOSManifestRef mfdel = SOSManifestCreateWithDigestVector(&dvdel, error);
                SOSDigestVectorFree(&dvdel);
                if (!SOSEngineUpdateLocalManifest_locked(engine, kSOSDataSourceSOSTransaction, mfdel, NULL, &localError))
                    secerror("SOSEngineUpdateLocalManifest deleting: %@ failed: %@", mfdel, localError);
                CFReleaseSafe(localError);
                CFReleaseSafe(mfdel);
                CFAssignRetained(local, SOSEngineCopyManifest_locked(engine, error));
            }
            SOSDigestVectorFree(&dv);
        }
    } else {
        // If we have no confirmed manifest, we want all pendedObjects going out as a manifest
        objectsSent = CFRetainSafe(pendingObjects);
    }

    if (confirmed || SOSManifestGetCount(missing) || SOSManifestGetCount(extra) || objectsSent) {
        SOSManifestRef allExtra = SOSManifestCreateUnion(extra, objectsSent, error);
        proposed = SOSManifestCreateWithPatch(confirmed, missing, allExtra, error);
        CFReleaseNull(allExtra);
    }

    if (!SOSMessageSetManifests(message, local, confirmed, proposed, proposed, confirmed ? objectsSent : NULL, error))
        CFReleaseNull(message);

    CFReleaseNull(objectsSent);

    if (message) {
        result = SOSMessageCreateData(message, SOSPeerNextSequenceNumber(peer), error);
    }
    
    if (result) {
        // Capture the peer in our block (SOSEnginePeerMessageSentBlock)
        CFRetainSafe(peer);
        *sent = Block_copy(^(bool success) {
            dispatch_async(engine->queue, ^{
            if (success) {
                if (!confirmed && !proposed) {
                    SOSPeerSetSendObjects(peer, true);
                    secnotice("engine", "SOSPeerSetSendObjects(true) L:%@", local);
                }
                SOSPeerAddLocalManifest(peer, local);
                SOSPeerAddProposedManifest(peer, proposed);
                secnoticeq("engine", "send %@ %@", SOSPeerGetID(peer), message);
            } else {
                secerror("%@ failed to send %@", SOSPeerGetID(peer), message);
            }
            CFReleaseSafe(peer);
            CFReleaseSafe(local);
            CFReleaseSafe(proposed);
            CFReleaseSafe(message);
            });
        });
    } else {
        CFReleaseSafe(local);
        CFReleaseSafe(proposed);
        CFReleaseSafe(message);
    }
    CFReleaseNull(extra);
    CFReleaseNull(missing);
    if (error && *error)
        secerror("%@ error in send: %@", SOSPeerGetID(peer), *error);

    return result;
}

static CFDataRef SOSEngineCreateMessageToSyncToPeer_locked(SOSEngineRef engine, CFStringRef peerID, SOSEnginePeerMessageSentBlock *sentBlock, CFErrorRef *error)
{
    SOSPeerRef peer = SOSPeerCreateWithEngine(engine, peerID);
    CFDataRef message = SOSEngineCreateMessage_locked(engine, peer, error, sentBlock);
    CFReleaseSafe(peer);
    
    return message;
}

bool SOSEngineHandleMessage(SOSEngineRef engine, CFStringRef peerID,
                            CFDataRef raw_message, CFErrorRef *error)
{
    __block bool result = false;
    __block bool somethingChanged = false;
    SOSMessageRef message = SOSMessageCreateWithData(kCFAllocatorDefault, raw_message, error);
    result = message && SOSDataSourceWith(engine->dataSource, error, ^(SOSTransactionRef txn, bool *commit) {
        dispatch_sync(engine->queue, ^{
            result = SOSEngineHandleMessage_locked(engine, peerID, message, txn, commit, &somethingChanged, error);
        });
    });
    CFReleaseSafe(message);
    if (somethingChanged)
        SecKeychainChanged(false);
    return result;
}

// --- Called from off the queue, need to move to on the queue

static void SOSEngineDoOnQueue(SOSEngineRef engine, dispatch_block_t action)
{
    dispatch_sync(engine->queue, action);
}

void SOSEngineCircleChanged(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers, CFArrayRef untrustedPeers) {
    SOSEngineDoOnQueue(engine, ^{
        SOSEngineCircleChanged_locked(engine, myPeerID, trustedPeers, untrustedPeers);
    });
    
    __block CFErrorRef localError = NULL;
    SOSDataSourceWith(engine->dataSource, &localError, ^(SOSTransactionRef txn, bool *commit) {
        SOSEngineDoOnQueue(engine, ^{
            *commit = SOSEngineSave(engine, txn, &localError);
        });
    });
    if (localError)
        secerror("failed to save engine state: %@", localError);
    CFReleaseSafe(localError);
    
}

SOSManifestRef SOSEngineCopyManifest(SOSEngineRef engine, CFErrorRef *error) {
    __block SOSManifestRef result = NULL;
    SOSEngineDoOnQueue(engine, ^{
        result = SOSEngineCopyManifest_locked(engine, error);
    });
    return result;
}

bool SOSEngineUpdateLocalManifest(SOSEngineRef engine, SOSDataSourceTransactionSource source, struct SOSDigestVector *removals, struct SOSDigestVector *additions, CFErrorRef *error) {
    __block bool result = true;
    SOSManifestRef mfdel = SOSManifestCreateWithDigestVector(removals, error);
    SOSManifestRef mfadd = SOSManifestCreateWithDigestVector(additions, error);
    SOSEngineDoOnQueue(engine, ^{
        // Safe to run async if needed...
        result = SOSEngineUpdateLocalManifest_locked(engine, source, mfdel, mfadd, error);
        CFReleaseSafe(mfdel);
        CFReleaseSafe(mfadd);
    });
    return result;
}

static bool SOSEngineSetCoderData_locked(SOSEngineRef engine, CFStringRef peer_id, CFDataRef data, CFErrorRef *error) {
    CFMutableDictionaryRef state = NULL;
    if (data) {
        if (!engine->peerState) {
            engine->peerState = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
            state = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        else{
            state = (CFMutableDictionaryRef)CFDictionaryGetValue(engine->peerState, peer_id);
            if(!state)
                state = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        }
        CFDictionarySetValue(state, kSOSPeerCoderKey, data);
        CFDictionarySetValue(engine->peerState, peer_id, state);
        
    }else if (engine->peerState) {
        if(CFDictionaryContainsKey(engine->peerState, peer_id)){
            CFMutableDictionaryRef state = (CFMutableDictionaryRef)CFDictionaryGetValue(engine->peerState, peer_id);
            if(CFDictionaryContainsKey(state, kSOSPeerCoderKey))
                CFDictionaryRemoveValue(state, kSOSPeerCoderKey);
        }
        if (CFDictionaryGetCount(engine->peerState) == 0) {
            CFReleaseNull(engine->peerState);
        }
    }
    return true;
}

bool SOSEngineSetCoderData(SOSEngineRef engine, CFStringRef peer_id, CFDataRef data, CFErrorRef *error) {
    __block bool result = false;
    
    SOSDataSourceWith(engine->dataSource, error, ^(SOSTransactionRef txn, bool *commit) {
        dispatch_sync(engine->queue, ^{
            result = SOSEngineSetCoderData_locked(engine, peer_id, data, error);
        });
    });
    
    return true;
}

static CFDataRef SOSEngineGetCoderData_locked(SOSEngineRef engine, CFStringRef peer_id) {
    // TODO: probably remove these secnotices
    CFDataRef result = NULL;
    CFMutableDictionaryRef peerState = NULL;
    
    if (!engine->peerState)
        secdebug("engine", "No engine coderData");
    else
        peerState = (CFMutableDictionaryRef)CFDictionaryGetValue(engine->peerState, peer_id);
    if (!peerState)
        secdebug("engine", "No peerState for peer %@", peer_id);
    else{
        result = CFDictionaryGetValue(peerState, kSOSPeerCoderKey);
        if(!result)
            secdebug("engine", "No coder data for peer %@", peer_id);
    }
    return result;
}


CFDataRef SOSEngineGetCoderData(SOSEngineRef engine, CFStringRef peer_id) {
    __block CFDataRef result = NULL;
    SOSDataSourceWith(engine->dataSource, NULL, ^(SOSTransactionRef txn, bool *commit) {
        dispatch_sync(engine->queue, ^{
            result = SOSEngineGetCoderData_locked(engine, peer_id);
        });
    });
    
    return result;
}

//
// Peer state layout.  WRONG! It's an array now
// The peer state is an array.
// The first element of the array is a dictionary with any number of keys and
// values in it (for future expansion) such as changing the digest size or type
// or remebering boolean flags for a peers sake.
// The next three are special in that they are manifest digests with special
// meaning and rules as to how they are treated (These are dynamically updated
// based on database activity so they have a fully history of all changes made
// to the local db. The first is the manifest representing the pendingObjects
// to send to the other peer.  This is normally only ever appending to, and in
// particular with transactions originating from the Keychain API that affect
// syncable items will need to add the new objects digests to the pendingObjects list
// while adding the digests of any tombstones encountered to the extra list.

CFMutableDictionaryRef SOSEngineGetPeerState(SOSEngineRef engine, CFStringRef peerID) {
    CFMutableDictionaryRef peerState = NULL;
    if (engine->peerState)
        peerState = (CFMutableDictionaryRef)CFDictionaryGetValue(engine->peerState, peerID);
    else
        engine->peerState = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    if (!peerState) {
        peerState = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryAddValue(engine->peerState, peerID, peerState);
        CFReleaseSafe(peerState);
    }
    return peerState;
}

CFDataRef SOSEngineCreateMessageToSyncToPeer(SOSEngineRef engine, CFStringRef peerID, SOSEnginePeerMessageSentBlock *sentBlock, CFErrorRef *error) {
    __block CFDataRef result = NULL;
    SOSEngineDoOnQueue(engine, ^{
        result = SOSEngineCreateMessageToSyncToPeer_locked(engine, peerID, sentBlock, error);
    });
    return result;
}

bool SOSEnginePeerDidConnect(SOSEngineRef engine, CFStringRef peerID, CFErrorRef *error) {
    __block bool result = true;
    result &= SOSDataSourceWith(engine->dataSource, error, ^(SOSTransactionRef txn, bool *commit) {
        dispatch_sync(engine->queue, ^{
            SOSPeerRef peer = SOSPeerCreateWithEngine(engine, peerID);
            if (!peer) {
                result = SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("Engine has no peer for %@"), peerID);
            } else {
                SOSPeerDidConnect(peer);
                result = SOSEngineSave(engine, txn, error);
                CFReleaseSafe(peer);
            }
        });
    });
    
    return result;
}
