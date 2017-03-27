/*
 * Copyright (c) 2012-2015 Apple Inc. All Rights Reserved.
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

#include <Security/SecureObjectSync/SOSChangeTracker.h>
#include <Security/SecureObjectSync/SOSEnginePriv.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSPeer.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSBackupEvent.h>
#include <Security/SecureObjectSync/SOSPersist.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <corecrypto/ccder.h>
#include <stdlib.h>
#include <stdbool.h>
#include <utilities/array_size.h>
#include <utilities/SecCFCCWrappers.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>
#include <utilities/debugging.h>
#include <utilities/iCloudKeychainTrace.h>
#include <utilities/SecCoreCrypto.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecADWrapper.h>
#include <utilities/SecTrace.h>

#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>

#include <securityd/SecItemServer.h>    // TODO: We can't leave this here.
#include <securityd/SOSCloudCircleServer.h> // TODO: We can't leave this here.
#include <Security/SecItem.h>           // TODO: We can't leave this here.
#include <Security/SecItemPriv.h>       // TODO: We can't leave this here.
#include <securityd/SecItemSchema.h>
#include <securityd/iCloudTrace.h>

#include <CoreFoundation/CFURL.h>

//
// MARK: SOSEngine The Keychain database with syncable keychain support.
//

//----------------------------------------------------------------------------------------
// MARK: Engine state v0
//----------------------------------------------------------------------------------------

// Key in dataSource for general engine state file.
// This file only has digest entries in it, no manifests.
static const CFStringRef kSOSEngineState = CFSTR("engine-state");

// Keys in state dictionary
static CFStringRef kSOSEngineManifestCacheKey = CFSTR("manifestCache");
static CFStringRef kSOSEnginePeerStateKey = CFSTR("peerState");
static CFStringRef kSOSEnginePeerIDsKey = CFSTR("peerIDs");
static CFStringRef kSOSEngineIDKey = CFSTR("id");
static CFStringRef kSOSEngineTraceDateKey = CFSTR("traceDate");

//----------------------------------------------------------------------------------------
// MARK: Engine state v2
//----------------------------------------------------------------------------------------

#if !TARGET_IPHONE_SIMULATOR
static const CFIndex kCurrentEngineVersion = 2;
#endif
// Keychain/datasource items
// Used for the kSecAttrAccount when saving in the datasource with dsSetStateWithKey
// Class D [kSecAttrAccessibleAlwaysPrivate/kSecAttrAccessibleAlwaysThisDeviceOnly]
CFStringRef kSOSEngineStatev2 = CFSTR("engine-state-v2");
CFStringRef kSOSEnginePeerStates = CFSTR("engine-peer-states");
CFStringRef kSOSEngineManifestCache = CFSTR("engine-manifest-cache");
CFStringRef kSOSEngineCoders = CFSTR("engine-coders");
#define kSOSEngineProtectionDomainClassA kSecAttrAccessibleWhenUnlockedThisDeviceOnly

// Keys for individual dictionaries
//  engine-state-v2
CFStringRef kSOSEngineStateVersionKey = CFSTR("engine-stateVersion");

// Current save/load routines
// SOSEngineCreate/SOSEngineLoad/SOSEngineSetState
// SOSEngineSave/SOSEngineDoSave/SOSEngineCopyState
// no save/load functions external to this file

/*
    Divide engine state into five pieces:

 - General engine state
    - My peer ID
    - List of other (trusted) peer IDs

 - Coder data (formerly in peer state)
 - Backup Keybags (backup peers only)
 - Peer state (including manifest hashes -- just keys into ManifestCache)
    [__OpaqueSOSPeer/SOSPeerRef]
    must-send
    send-objects
    sequence-number
    Peer object states:
        pending-objects
        unwanted-manifest
        confirmed-manifest
        local-manifest
        pending-manifest
    Views

 - Manifest Cache
    - local manifest hashes (copy of local keychain)
    - peer manifest hashes

 These divisions are based on size, frequency of update, and protection domain

    The Manifest Cache is a dictionary where each key is a hash over its entry,
    which is a concatenation of 20 byte hashes of the keychain items. The local
    keychain is present as one entry. The other entries are subsets of that, one
    for each confirmed/pending/missing/unwanted shared with a peer. The local
    keychain entry can be re-created by iterating over the databse, whereas the
    others are built up through communicating with other peers.

 83:d=2  hl=2 l=  13 prim:   UTF8STRING        :manifestCache
 98:d=2  hl=4 l= 912 cons:   SET
 102:d=3  hl=2 l=  24 cons:    SEQUENCE
 104:d=4  hl=2 l=  20 prim:     OCTET STRING      [HEX DUMP]:DA39A3EE5E6B4B0D3255BFEF95601890AFD80709
 126:d=4  hl=2 l=   0 prim:     OCTET STRING
 128:d=3  hl=2 l= 124 cons:    SEQUENCE
 130:d=4  hl=2 l=  20 prim:     OCTET STRING      [HEX DUMP]:F9B59370A4733F0D174E8D220C5BE3AF062C775B
 152:d=4  hl=2 l= 100 prim:     OCTET STRING      [HEX DUMP]:5A574BB4EC90C3BBCC69EE73CBFE039133AE807265D6A58003B8D205997EAB96390AAB207E63A2E270A476CAB5B2D9D2F7B0E55512AA957B58D5658E7EF907B069B83AA6BA941790A3C3C4A68292D59DABA3CA342966EFF82E1ACAEB691FD6E20772E17E
 254:d=3  hl=4 l= 366 cons:    SEQUENCE
 258:d=4  hl=2 l=  20 prim:     OCTET STRING      [HEX DUMP]:2E69C2F7F3E014075B30004CE0EC6C1AD419EBF5
 280:d=4  hl=4 l= 340 prim:     OCTET STRING      [HEX DUMP]:07571E9678FD7D68812E409CC96C1F54834A099A0C3A2D12CCE2EA95F4505EA52F2C982B2ADEE3DA14D4712C000309BF63D54A98B61AA1D963C40E0E2531C83B28CA5BE6DA0D26400C3C77A618F711DD3CC0BF86CCBAF8AA3332973268B30EEBF21CD8184D9C8427CA13DECCC7BB83C80009A2EF45CCC07F586315C80CEEEEF5D5352FD000AAE6D9CBB4294D5959FD00198225AF9ABD09B341A2FDC278E9FD1465D6A58003B8D205997EAB96390AAB207E63A2E270A476CAB5B2D9D2F7B0E55512AA957B58D5658E7EF907B069B83AA6BA941790A3C3C4A68292D59D95C9D4D8A8BCA2E8242AB0D409F671F298B6DCAE9BC4238C09E07548CEFB300098606F9E4F230C99ABA3CA342966EFF82E1ACAEB691FD6E20772E17EB4FEFB84F8CF75C0C69C59532C354D175A59F961BA4D4DFA017FD8192288F14278AE76712E127D65FE616C7E4FD0713644F7C9A7ABA1CE065694A968
 624:d=3  hl=4 l= 386 cons:    SEQUENCE
 628:d=4  hl=2 l=  20 prim:     OCTET STRING      [HEX DUMP]:CCF179FF718C10F151E7409EDF1A06F0DF10DCAD
 650:d=4  hl=4 l= 360 prim:     OCTET STRING      [HEX DUMP]:07571E9678FD7D68812E409CC96C1F54834A099A0C3A2D12CCE2EA95F4505EA52F2C982B2ADEE3DA14D4712C000309BF63D54A98B61AA1D963C40E0E2531C83B28CA5BE6DA0D26400C3C77A618F711DD3CC0BF86CCBAF8AA3332973268B30EEBF21CD8184D9C8427CA13DECCC7BB83C80009A2EF45CCC07F586315C80CEEEEF5D5352FD000AAE6D9CBB4294D5959FD00198225AF9ABD09B341A2FDC278E9FD145A574BB4EC90C3BBCC69EE73CBFE039133AE807265D6A58003B8D205997EAB96390AAB207E63A2E270A476CAB5B2D9D2F7B0E55512AA957B58D5658E7EF907B069B83AA6BA941790A3C3C4A68292D59D95C9D4D8A8BCA2E8242AB0D409F671F298B6DCAE9BC4238C09E07548CEFB300098606F9E4F230C99ABA3CA342966EFF82E1ACAEB691FD6E20772E17EB4FEFB84F8CF75C0C69C59532C354D175A59F961BA4D4DFA017FD8192288F14278AE76712E127D65FE616C7E4FD0713644F7C9A7ABA1CE065694A968

 */



static bool SOSEngineLoad(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error);
static bool SOSEngineSetPeers_locked(SOSEngineRef engine, SOSPeerMetaRef myPeerMeta, CFArrayRef trustedPeerMetas, CFArrayRef untrustedPeerMetas);
static void SOSEngineApplyPeerState(SOSEngineRef engine, CFDictionaryRef peerStateMap);
static void SOSEngineSynthesizePeerMetas(SOSEngineRef engine, CFMutableArrayRef trustedPeersMetas, CFMutableArrayRef untrustedPeers);
static bool SOSEngineLoadCoders(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error);
#if !TARGET_IPHONE_SIMULATOR
static bool SOSEngineDeleteV0State(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error);
#endif
static CFStringRef SOSPeerIDArrayCreateString(CFArrayRef peerIDs) {
    return peerIDs ? CFStringCreateByCombiningStrings(kCFAllocatorDefault, peerIDs, CFSTR(" ")) : CFSTR("");
}
 
static CFStringRef SOSEngineCopyFormattingDesc(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSEngineRef engine = (SOSEngineRef)cf;
    CFStringRef tpDesc = SOSPeerIDArrayCreateString(engine->peerIDs);
    CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, formatOptions, CFSTR("<Engine %@ peers %@ MC[%d] PS[%d]>"), engine->myID, tpDesc, engine->manifestCache ? (int)CFDictionaryGetCount(engine->manifestCache) : 0, engine->peerMap ? (int)CFDictionaryGetCount(engine->peerMap) : 0);
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
// or remembering boolean flags for a peers sake.
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
    if(!engine) return NULL;
    return engine->peerIDs;
}

void SOSEngineClearCache(SOSEngineRef engine){
    CFReleaseNull(engine->manifestCache);
    CFReleaseNull(engine->localMinusUnreadableDigest);
    if (engine->save_timer)
        dispatch_source_cancel(engine->save_timer);
    dispatch_release(engine->queue);
    engine->queue = NULL;
}

static SOSPeerRef SOSEngineCopyPeerWithMapEntry_locked(SOSEngineRef engine, CFStringRef peerID, CFTypeRef mapEntry, CFErrorRef *error) {
    SOSPeerRef peer = NULL;
    if (mapEntry && CFGetTypeID(mapEntry) == SOSPeerGetTypeID()) {
        // The mapEntry is an SOSPeer, so we're done.
        peer = (SOSPeerRef)CFRetain(mapEntry);
    } else {
        // The mapEntry is a peerState, attempt to initialize a new
        // peer iff peerID is in the set of trusted peerIDs
        if (engine->peerIDs && CFArrayContainsValue(engine->peerIDs, CFRangeMake(0, CFArrayGetCount(engine->peerIDs)), peerID)) {
            CFErrorRef localError = NULL;
            peer = SOSPeerCreateWithState(engine, peerID, mapEntry, &localError);
            if (!peer) {
                secerror("error inflating peer: %@: %@ from state: %@", peerID, localError, mapEntry);
                CFReleaseNull(localError);
                peer = SOSPeerCreateWithState(engine, peerID, NULL, error);
            }
            if (peer) {
                // Replace the map entry with the inflated peer.
                CFDictionarySetValue(engine->peerMap, peerID, peer);
            }
        } else {
            SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("peer: %@ is untrusted inflating not allowed"), peerID);
        }
    }
    return peer;
}

static SOSPeerRef SOSEngineCopyPeerWithID_locked(SOSEngineRef engine, CFStringRef peerID, CFErrorRef *error) {
    CFTypeRef mapEntry = CFDictionaryGetValue(engine->peerMap, peerID);
    SOSPeerRef peer = NULL;
    if (mapEntry) {
        peer = SOSEngineCopyPeerWithMapEntry_locked(engine, peerID, mapEntry, error);
    } else {
        peer = NULL;
        secerror("peer: %@ not found, peerMap: %@, engine: %@", peerID, engine->peerMap, engine);
        SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("peer: %@ not found"), peerID);
    }
    return peer;
}

struct SOSEngineWithPeerContext {
    SOSEngineRef engine;
    void (^with)(SOSPeerRef peer);
};

static void SOSEngineWithPeerMapEntry_locked(const void *peerID, const void *mapEntry, void *context) {
    struct SOSEngineWithPeerContext *ewp = context;
    SOSPeerRef peer = SOSEngineCopyPeerWithMapEntry_locked(ewp->engine, peerID, mapEntry, NULL);
    if (peer) {
        ewp->with(peer);
        CFRelease(peer);
    }
}

static void SOSEngineForEachPeer_locked(SOSEngineRef engine, void (^with)(SOSPeerRef peer)) {
    struct SOSEngineWithPeerContext ewp = { .engine = engine, .with = with };
    CFDictionaryRef peerMapCopy = CFDictionaryCreateCopy(NULL, engine->peerMap);
    CFDictionaryApplyFunction(peerMapCopy, SOSEngineWithPeerMapEntry_locked, &ewp);
    CFRelease(peerMapCopy);
}

static void SOSEngineWithBackupPeerMapEntry_locked(const void *peerID, const void *mapEntry, void *context) {
    struct SOSEngineWithPeerContext *ewp = context;
    // v0 backup peer is always in map but we only consider it a backup peer if it has a keybag.
    if (SOSPeerMapEntryIsBackup(mapEntry)) {
        SOSPeerRef peer = SOSEngineCopyPeerWithMapEntry_locked(ewp->engine, peerID, mapEntry, NULL);
        if (peer) {
            ewp->with(peer);
            CFRelease(peer);
        }
    }
}

static void SOSEngineForEachBackupPeer_locked(SOSEngineRef engine, void (^with)(SOSPeerRef peer)) {
    struct SOSEngineWithPeerContext ewp = { .engine = engine, .with = with };
    CFDictionaryRef peerMapCopy = CFDictionaryCreateCopy(NULL, engine->peerMap);
    CFDictionaryApplyFunction(peerMapCopy, SOSEngineWithBackupPeerMapEntry_locked, &ewp);
    CFRelease(peerMapCopy);
}

//
// Manifest cache
//
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

SOSManifestRef SOSEngineCopyPersistedManifest(SOSEngineRef engine, CFDictionaryRef persisted, CFStringRef key) {
    return CFRetainSafe(SOSEngineGetManifestForDigest(engine, asData(CFDictionaryGetValue(persisted, key), NULL)));
}

CFMutableArrayRef SOSEngineCopyPersistedManifestArray(SOSEngineRef engine, CFDictionaryRef persisted, CFStringRef key, CFErrorRef *error) {
    CFMutableArrayRef manifests = NULL;
    CFArrayRef digests = NULL;
    CFDataRef digest;
    if (asArrayOptional(CFDictionaryGetValue(persisted, key), &digests, error))
        manifests = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    if (digests) CFArrayForEachC(digests, digest) {
        SOSManifestRef manifest = SOSEngineGetManifestForDigest(engine, digest);
        if (manifest)
            CFArrayAppendValue(manifests, manifest);
    }
    return manifests;
}

#if !TARGET_IPHONE_SIMULATOR
static CFDictionaryRef SOSEngineCopyEncodedManifestCache_locked(SOSEngineRef engine, CFErrorRef *error) {
    CFMutableDictionaryRef mfc = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSEngineForEachPeer_locked(engine, ^(SOSPeerRef peer) {
        SOSPeerAddManifestsInUse(peer, mfc);
    });
    return mfc;
}
#endif

//
// End of Manifest cache
//

//----------------------------------------------------------------------------------------
// MARK: Coders
//----------------------------------------------------------------------------------------

/*
 Each peer has an associated coder, whcih the engine keeps track of in a
 CFDictionary indexed by peerID. The coders are read from disk when first needed,
 then kept in memory as SOSCoders.

 N.B. Don't rollback coder in memory if a transaction is rolled back, since this
 might lead to reuse of an IV.
*/

static bool SOSEngineCopyCoderData(SOSEngineRef engine, CFStringRef peerID, CFDataRef *coderData, CFErrorRef *error) {
    bool ok = true;
    SOSCoderRef coder = (SOSCoderRef)CFDictionaryGetValue(engine->coders, peerID);
    if (coder && (CFGetTypeID(coder) == SOSCoderGetTypeID())) {
        CFErrorRef localError = NULL;
        ok = *coderData = SOSCoderCopyDER(coder, &localError);
        if (!ok) {
            secerror("failed to der encode coder for peer %@, dropping it: %@", peerID, localError);
            CFDictionaryRemoveValue(engine->coders, peerID);
            CFErrorPropagate(localError, error);
        }
    } else {
        *coderData = NULL;
    }
    return ok;
}

static SOSCoderRef SOSEngineGetCoderInTx_locked(SOSEngineRef engine, SOSTransactionRef txn, CFStringRef peerID, CFErrorRef *error) {
    if (!engine->haveLoadedCoders) {
        engine->haveLoadedCoders = SOSEngineLoadCoders(engine, txn, error);

        if (!engine->haveLoadedCoders) {
            return NULL;
        }
    }

    SOSCoderRef coder = (SOSCoderRef)CFDictionaryGetValue(engine->coders, peerID);
    if (!coder || (CFGetTypeID(coder) != SOSCoderGetTypeID())) {
        SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("No coder for peer: %@"), peerID);
    }
    return coder;
}

static bool SOSEngineEnsureCoder_locked(SOSEngineRef engine, SOSTransactionRef txn, CFStringRef peerID, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, SOSCoderRef ourCoder, CFErrorRef *error) {
    //have to have caused coder loading, transactions do this.
    if (!ourCoder || !SOSCoderIsFor(ourCoder, peerInfo, myPeerInfo)) {
        secinfo("coder", "New coder for id %@.", peerID);
        CFErrorRef localError = NULL;
        SOSCoderRef coder = SOSCoderCreate(peerInfo, myPeerInfo, kCFBooleanFalse, &localError);
        if (!coder) {
            secerror("Failed to create coder for %@: %@", peerID, localError);
            CFErrorPropagate(localError, error);
            return false;
        }
        CFDictionarySetValue(engine->coders, peerID, coder);
        secdebug("coder", "setting coder for peerid: %@, coder: %@", peerID, coder);
        CFReleaseNull(coder);
        engine->codersNeedSaving = true;
    }
    return true;
}

bool SOSEngineInitializePeerCoder(SOSEngineRef engine, SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo, CFErrorRef *error) {
    __block bool ok = true;
    CFStringRef peerID = SOSPeerInfoGetPeerID(peerInfo);

    ok &= SOSEngineWithPeerID(engine, peerID, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState) {
        ok = SOSEngineEnsureCoder_locked(engine, txn, peerID, myPeerInfo, peerInfo, coder, error);
        *forceSaveState = ok;
    });

    return ok;
}

static bool SOSEngineGCPeerState_locked(SOSEngineRef engine, CFErrorRef *error) {
    bool ok = true;
    
    //require_quiet(ok = SOSEngineGCManifests_locked(engine, error), exit);

//exit:
    return ok;
}
#if !TARGET_IPHONE_SIMULATOR
static CFMutableDictionaryRef SOSEngineCopyPeerState_locked(SOSEngineRef engine, CFErrorRef *error) {
    CFMutableDictionaryRef peerState = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(engine->peerMap, ^(const void *key, const void *value) {
        CFDictionaryRef state = NULL;
        if (value && CFGetTypeID(value) == SOSPeerGetTypeID()) {
            CFErrorRef localError = NULL;
            // Inflated peer
            state = SOSPeerCopyState((SOSPeerRef)value, &localError);
            if (!state)
                secnotice("engine", "%@ failed to encode peer: %@", key, localError);
            CFReleaseNull(localError);
            // TODO: Potentially replace inflated peer with deflated peer in peerMap
        } else if (value) {
            // We have a deflated peer.
            state = CFRetainSafe(value);
        }

        if (state) {
            CFDictionarySetValue(peerState, key, state);
            CFReleaseSafe(state);
        }
    });
    return peerState;
}
#endif
static CFMutableDictionaryRef SOSEngineCopyPeerCoders_locked(SOSEngineRef engine, CFErrorRef *error) {
    CFMutableDictionaryRef coders = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(engine->peerMap, ^(const void *key, const void *value) {
        CFDataRef coderData = NULL;
        CFErrorRef localError = NULL;
        bool ok = SOSEngineCopyCoderData(engine, (CFStringRef)key, &coderData, &localError);

        if (!ok) {
            secnotice("engine", "%@ no coder for peer: %@", key, localError);
        }
        if (ok && coderData) {
            CFDictionarySetValue(coders, key, coderData);
        }
        CFReleaseNull(coderData);
        CFReleaseNull(localError);
    });
    return coders;
}

//----------------------------------------------------------------------------------------
// MARK: Engine state v2 Save
//----------------------------------------------------------------------------------------

// Coders and keybags

static CFDataRef SOSEngineCopyCoders(SOSEngineRef engine, CFErrorRef *error) {
    // Copy the CFDataRef version of the coders into a dictionary, which is then DER-encoded for saving
    CFDictionaryRef coders = SOSEngineCopyPeerCoders_locked(engine, error);
    secdebug("coders", "copying coders! %@", coders);
    CFDataRef der = CFPropertyListCreateDERData(kCFAllocatorDefault, coders, error);
    CFReleaseSafe(coders);
    return der;
}

static bool SOSEngineSaveCoders(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    // MUST hold engine lock
    // Device must be unlocked for this to succeed

    if(!engine->haveLoadedCoders){
        secdebug("coders", "attempting to save coders before we have loaded them!");
    }
    
    bool ok = true;
    if (engine->codersNeedSaving) {
        CFDataRef derCoders = SOSEngineCopyCoders(engine, error);
        bool ok = derCoders && SOSDataSourceSetStateWithKey(engine->dataSource, txn, kSOSEngineCoders,
                                                            kSOSEngineProtectionDomainClassA, derCoders, error);
        if (ok) {
            engine->codersNeedSaving = false;
            secnotice("coder", "saved coders: %@", engine->coders);
        }
        CFReleaseSafe(derCoders);
    }
    return ok;
}

bool SOSTestEngineSaveCoders(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error){
    return SOSEngineSaveCoders(engine, txn, error);
}
#if !TARGET_IPHONE_SIMULATOR

static CFDictionaryRef SOSEngineCopyBasicState(SOSEngineRef engine, CFErrorRef *error) {
    // Create a version of the in-memory engine state for saving to disk
    CFMutableDictionaryRef state = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    if (engine->myID)
        CFDictionarySetValue(state, kSOSEngineIDKey, engine->myID);
    if (engine->peerIDs)
        CFDictionarySetValue(state, kSOSEnginePeerIDsKey, engine->peerIDs);
    if (engine->lastTraceDate)
        CFDictionarySetValue(state, kSOSEngineTraceDateKey, engine->lastTraceDate);

    SOSPersistCFIndex(state, kSOSEngineStateVersionKey, kCurrentEngineVersion);
    return state;
}

static bool SOSEngineDoSaveOneState(SOSEngineRef engine, SOSTransactionRef txn, CFStringRef key, CFStringRef pdmn,
                                    CFDictionaryRef state, CFErrorRef *error) {
    CFDataRef derState = CFPropertyListCreateDERData(kCFAllocatorDefault, state, error);
    bool ok = derState && SOSDataSourceSetStateWithKey(engine->dataSource, txn, key, pdmn, derState, error);
    CFReleaseSafe(derState);
    return ok;
}

static bool SOSEngineDoSave(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    bool ok = true;

    CFDictionaryRef state = SOSEngineCopyBasicState(engine, error);
    ok &= state && SOSEngineDoSaveOneState(engine, txn, kSOSEngineStatev2, kSOSEngineProtectionDomainClassD, state, error);
    CFReleaseNull(state);

    state = SOSEngineCopyPeerState_locked(engine, error);
    ok &= state && SOSEngineDoSaveOneState(engine, txn, kSOSEnginePeerStates, kSOSEngineProtectionDomainClassD, state, error);
    CFReleaseNull(state);

    state = SOSEngineCopyEncodedManifestCache_locked(engine, error);
    ok &= state && SOSEngineDoSaveOneState(engine, txn, kSOSEngineManifestCache, kSOSEngineProtectionDomainClassD, state, error);
    CFReleaseNull(state);

    ok &= SOSEngineSaveCoders(engine, txn, error);

    SOSEngineDeleteV0State(engine, txn, NULL);

    return ok;
}
#endif

static bool SOSEngineSave(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    // Don't save engine state from tests
    if (!engine->dataSource)
        return true;
#if !TARGET_IPHONE_SIMULATOR
    return SOSEngineDoSave(engine, txn, error);
#endif
    return true;
}

//----------------------------------------------------------------------------------------
// MARK: Engine state v2 Load/Restore
//----------------------------------------------------------------------------------------

// Restore the in-memory state of engine from saved state loaded from the db
static bool SOSEngineSetManifestCacheWithDictionary(SOSEngineRef engine, CFDictionaryRef manifestCache, CFErrorRef *error) {
    __block bool ok = true;
    CFReleaseNull(engine->manifestCache);
    if (manifestCache) {
        engine->manifestCache = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryForEach(manifestCache, ^(const void *key, const void *value) {
            CFDataRef data = (CFDataRef)value;
            if (isData(data)) {
                SOSManifestRef mf = SOSManifestCreateWithData(data, NULL);
                if (mf)
                    CFDictionarySetValue(engine->manifestCache, key, mf);
                CFReleaseSafe(mf);
            }
        });
    }

    return ok;
}

static bool SOSEngineUpdateStateWithDictionary(SOSEngineRef engine, CFDictionaryRef stateDict, CFErrorRef *error) {
    bool ok = true;
#if 0
    if (stateDict) {
        // If kCurrentEngineVersion > 2, uncomment and fill in code below
        CFIndex engineVersion = 0 ;
        bool versionPresent = SOSPeerGetOptionalPersistedCFIndex(stateDict, kSOSEngineStateVersionKey, &engineVersion);
        if (versionPresent && (engineVersion != kCurrentEngineVersion)) {
            // need migration
        }
    }
#endif
    return ok;
}

static bool SOSEngineSetStateWithDictionary(SOSEngineRef engine, CFDictionaryRef stateDict, CFErrorRef *error) {
    bool ok = true;
    if (stateDict) {
        SOSEngineUpdateStateWithDictionary(engine, stateDict, error);
        CFRetainAssign(engine->myID, asString(CFDictionaryGetValue(stateDict, kSOSEngineIDKey), NULL));
        CFRetainAssign(engine->peerIDs, asArray(CFDictionaryGetValue(stateDict, kSOSEnginePeerIDsKey), NULL));
        CFRetainAssign(engine->lastTraceDate, asDate(CFDictionaryGetValue(stateDict, kSOSEngineTraceDateKey), NULL));

    }
    secnotice("engine", "%@", engine);
    return ok;
}

static bool SOSEngineSetPeerStateWithDictionary(SOSEngineRef engine, CFDictionaryRef peerStateDict, CFErrorRef *error) {
    // Set the in-memory peer state using the dictionary version of the DER-encoded version from disk
    CFMutableArrayRef untrustedPeers = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef trustedPeersMetas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSEngineApplyPeerState(engine, asDictionary(peerStateDict, NULL));
    SOSEngineSynthesizePeerMetas(engine, trustedPeersMetas, untrustedPeers);
    SOSEngineSetPeers_locked(engine, engine->myID, trustedPeersMetas, untrustedPeers);
    CFReleaseNull(trustedPeersMetas);
    CFReleaseNull(untrustedPeers);
    return true;
}

CFMutableDictionaryRef derStateToDictionaryCopy(CFDataRef state, CFErrorRef *error) {
    bool ok = true;
    CFMutableDictionaryRef stateDict = NULL;
    if (state) {
        const uint8_t *der = CFDataGetBytePtr(state);
        const uint8_t *der_end = der + CFDataGetLength(state);
        ok = der = der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef *)&stateDict, error, der, der_end);
        if (der && der != der_end) {
            ok = SOSErrorCreate(kSOSErrorDecodeFailure, error, NULL, CFSTR("trailing %td bytes at end of state"), der_end - der);
        }
        if (!ok) {
            CFReleaseNull(stateDict);
        }
    }
    return stateDict;
}
bool TestSOSEngineLoadCoders(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error)
{
    return SOSEngineLoadCoders(engine, txn, error);
}

static bool SOSEngineLoadCoders(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    // Read the serialized engine state from the datasource (aka keychain) and populate the in-memory engine
    bool ok = true;
    CFDataRef derCoders = NULL;
    CFMutableDictionaryRef codersDict = NULL;
    derCoders = SOSDataSourceCopyStateWithKey(engine->dataSource, kSOSEngineCoders, kSOSEngineProtectionDomainClassA, txn, error);
    require_quiet(derCoders, xit);
    codersDict = derStateToDictionaryCopy(derCoders, error);
    require_quiet(codersDict, xit);
    CFDictionaryForEach(engine->peerMap, ^(const void *peerID, const void *peerState) {
        if (peerID) {
            CFTypeRef coderRef = CFDictionaryGetValue(codersDict, peerID);
            if (coderRef) {
                CFDataRef coderData = asData(coderRef, NULL);
                if (coderData) {
                    CFErrorRef createError = NULL;
                    SOSCoderRef coder = SOSCoderCreateFromData(coderData, &createError);
                    if (coder) {
                        CFDictionaryAddValue(engine->coders, peerID, coder);
                        secnotice("coder", "adding coder: %@ for peerid: %@", coder, peerID);
                    } else {
                        secnotice("coder", "Coder for '%@' failed to create: %@", peerID, createError);
                    }
                    CFReleaseNull(createError);
                    CFReleaseNull(coder);
                } else {
                    // Needed a coder, didn't find one, notify the account to help us out.
                    // Next attempt to sync will fix this
                    secnotice("coder", "coder for %@ was not cf data: %@", peerID, coderData);
                    SOSCCEnsurePeerRegistration();
                }
            }
            else{
                secnotice("coder", "didn't find coder for peer: %@ engine dictionary: %@", peerID, codersDict);
                SOSCCEnsurePeerRegistration();
            }

        }
    });

    engine->haveLoadedCoders = true;

xit:
    CFReleaseNull(derCoders);
    CFReleaseNull(codersDict);
    return ok;
}
#if !TARGET_IPHONE_SIMULATOR
static bool SOSEngineDeleteV0State(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
//    SOSDataSourceDeleteStateWithKey(engine->dataSource, kSOSEngineState, kSOSEngineProtectionDomainClassD, txn, error);

    // Create effectively empty state until delete is working
    CFMutableDictionaryRef state = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    if (engine->myID)
        CFDictionarySetValue(state, kSOSEngineIDKey, engine->myID);
    CFDataRef derState = CFPropertyListCreateDERData(kCFAllocatorDefault, state, error);
    CFReleaseNull(state);

    bool ok = derState && SOSDataSourceSetStateWithKey(engine->dataSource, txn, kSOSEngineState, kSOSEngineProtectionDomainClassD, derState, error);
    CFReleaseSafe(derState);
    return ok;
}
#endif
static bool SOSEngineLoad(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    // Read the serialized engine state from the datasource (aka keychain) and populate the in-memory engine
    bool ok = true;
    CFDataRef basicEngineState = NULL;
    CFMutableDictionaryRef engineState = NULL;
    CFDictionaryRef manifestCache = NULL;
    CFDictionaryRef peerStateDict = NULL;
    CFMutableDictionaryRef codersDict = NULL;
    // Look for the v2 engine state first
    basicEngineState = SOSDataSourceCopyStateWithKey(engine->dataSource, kSOSEngineStatev2, kSOSEngineProtectionDomainClassD, txn, error);
    if (basicEngineState) {
        CFDataRef data = NULL;
        engineState = derStateToDictionaryCopy(basicEngineState, error);

        data = SOSDataSourceCopyStateWithKey(engine->dataSource, kSOSEngineManifestCache, kSOSEngineProtectionDomainClassD, txn, error);
        manifestCache = derStateToDictionaryCopy(data, error);
        CFReleaseNull(data);

        data = SOSDataSourceCopyStateWithKey(engine->dataSource, kSOSEnginePeerStates, kSOSEngineProtectionDomainClassD, txn, error);
        peerStateDict = derStateToDictionaryCopy(data, error);
        CFReleaseNull(data);
    } else {
        // Look for original V0 engine state next
        CFDataRef v0EngineStateData = SOSDataSourceCopyStateWithKey(engine->dataSource, kSOSEngineState, kSOSEngineProtectionDomainClassD, txn, error);
        if (v0EngineStateData) {
            engineState = derStateToDictionaryCopy(v0EngineStateData, error);
            if (engineState) {
                manifestCache = CFRetainSafe(asDictionary(CFDictionaryGetValue(engineState, kSOSEngineManifestCacheKey), NULL));
                peerStateDict = CFRetainSafe(asDictionary(CFDictionaryGetValue(engineState, kSOSEnginePeerStateKey), NULL));
            }
            CFReleaseNull(v0EngineStateData);
        }
        secnotice("coder", "Migrating from v0 engine state; dropping coders and forcing re-negotiation");
        SOSCCEnsurePeerRegistration();

        if (engine->peerIDs) {
            SOSCCRequestSyncWithPeersList(engine->peerIDs);
        }
    }

    ok = engineState && SOSEngineSetStateWithDictionary(engine, engineState, error);

    ok &= SOSEngineSetManifestCacheWithDictionary(engine, manifestCache, error);

    ok &= peerStateDict && SOSEngineSetPeerStateWithDictionary(engine, peerStateDict, error);

    CFReleaseSafe(basicEngineState);
    CFReleaseSafe(engineState);
    CFReleaseSafe(manifestCache);
    CFReleaseSafe(peerStateDict);
    CFReleaseSafe(codersDict);
    return ok;
}

bool SOSTestEngineSaveWithDER(SOSEngineRef engine, CFDataRef derState, CFErrorRef *error) {
    assert(true);
    return true;
}

bool SOSTestEngineSave(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    bool bx = SOSEngineSave(engine, txn, error);
    secnotice("test", "saved engine: %@", engine);
    return bx;
}

bool SOSTestEngineLoad(SOSEngineRef engine, SOSTransactionRef txn, CFErrorRef *error) {
    bool bx = SOSEngineLoad(engine, txn, error);
    secnotice("test", "loaded engine: %@", engine);
    return bx;
}

//----------------------------------------------------------------------------------------
// MARK: Change Trackers and Peer Manifests
//----------------------------------------------------------------------------------------

static SOSManifestRef SOSEngineCreateManifestWithViewNameSet_locked(SOSEngineRef engine, CFSetRef viewNameSet, CFErrorRef *error) {
    // TODO: Potentially tell all changeTrackers to track manifests (    //forall ct do SOSChangeTrackerSetConcrete(ct, true);
    // and read the entire dataSource and pass all objects though the filter here, instead of
    // forcing the datasource to be able to do "smart" queries
    return SOSDataSourceCopyManifestWithViewNameSet(engine->dataSource, viewNameSet, error);
}

static SOSChangeTrackerRef SOSEngineCopyChangeTrackerWithViewNameSet_locked(SOSEngineRef engine, CFSetRef viewNameSet, CFErrorRef *error) {
    SOSChangeTrackerRef ct = (SOSChangeTrackerRef)CFDictionaryGetValue(engine->viewNameSet2ChangeTracker, viewNameSet);
    if (!ct)
        SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("no change tracker for view set %@"), viewNameSet);
    return CFRetainSafe(ct);
}

static SOSManifestRef SOSEngineCopyManifestWithViewNameSet_locked(SOSEngineRef engine, CFSetRef viewNameSet, CFErrorRef *error) {
    SOSChangeTrackerRef ct = SOSEngineCopyChangeTrackerWithViewNameSet_locked(engine, viewNameSet, error);
    if (!ct)
        return NULL;

    SOSManifestRef manifest = SOSChangeTrackerCopyManifest(ct, NULL);
    if (!manifest) {
        manifest = SOSEngineCreateManifestWithViewNameSet_locked(engine, viewNameSet, error); // Do the SQL query
        SOSChangeTrackerSetManifest(ct, manifest);
    }
    CFReleaseSafe(ct);
    return manifest;
}

SOSManifestRef SOSEngineCopyLocalPeerManifest_locked(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
    return SOSEngineCopyManifestWithViewNameSet_locked(engine, SOSPeerGetViewNameSet(peer), error);
}

#define withViewAndBackup(VIEW)  do { with(VIEW); if (!isTomb) with(VIEW ## _tomb); } while(0)


// Invoke with once for each view an object is in.
// TODO: Move this function into the DataSource
static void SOSEngineObjectWithView(SOSEngineRef engine, SOSObjectRef object, void (^with)(CFStringRef view)) {
    // Filter items into v0 only view here
    SecDbItemRef item = (SecDbItemRef)object; // TODO: Layer violation, breaks tests
    if (isDictionary(object)) {
        CFTypeRef isTombValue = CFDictionaryGetValue((CFDictionaryRef)object, kSecAttrTombstone);
        bool isTomb = isTombValue && CFBooleanGetValue(isTombValue);
        // We are in the test just assume v0 and v2 views.
        withViewAndBackup(kSOSViewKeychainV0);
    } else if (SecDbItemIsSyncableOrCorrupted(item)) {
        const SecDbClass *iclass = SecDbItemGetClass(item);
        CFTypeRef pdmn = SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);
        if ((iclass == &genp_class || iclass == &inet_class || iclass == &keys_class || iclass == &cert_class)
            && isString(pdmn)
            && (CFEqual(pdmn, kSecAttrAccessibleWhenUnlocked)
                || CFEqual(pdmn, kSecAttrAccessibleAfterFirstUnlock)
                || CFEqual(pdmn, kSecAttrAccessibleAlwaysPrivate)
                || CFEqual(pdmn, kSecAttrAccessibleWhenUnlockedThisDeviceOnly)
                || CFEqual(pdmn, kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly)
                || CFEqual(pdmn, kSecAttrAccessibleAlwaysThisDeviceOnlyPrivate)))
        {
            CFTypeRef tomb = SecDbItemGetCachedValueWithName(item, kSecAttrTombstone);
            char cvalue = 0;
            bool isTomb = (isNumber(tomb) && CFNumberGetValue(tomb, kCFNumberCharType, &cvalue) && cvalue == 1);
            CFTypeRef viewHint = SecDbItemGetCachedValueWithName(item, kSecAttrSyncViewHint);
            if (viewHint == NULL) {
                if (iclass == &cert_class) {
                    withViewAndBackup(kSOSViewOtherSyncable);
                } else {
                    if (!SecDbItemGetCachedValueWithName(item, kSecAttrTokenID)) {
                        withViewAndBackup(kSOSViewKeychainV0);
                    }
                    CFTypeRef agrp = SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup);
                    if (iclass == &keys_class && CFEqualSafe(agrp, CFSTR("com.apple.security.sos"))) {
                        withViewAndBackup(kSOSViewiCloudIdentity);
                    } else if (CFEqualSafe(agrp, CFSTR("com.apple.cfnetwork"))) {
                        withViewAndBackup(kSOSViewAutofillPasswords);
                    } else if (CFEqualSafe(agrp, CFSTR("com.apple.safari.credit-cards"))) {
                        withViewAndBackup(kSOSViewSafariCreditCards);
                    } else if (iclass == &genp_class) {
                        if (CFEqualSafe(agrp, CFSTR("apple")) &&
                            CFEqualSafe(SecDbItemGetCachedValueWithName(item, kSecAttrService), CFSTR("AirPort"))) {
                            withViewAndBackup(kSOSViewWiFi);
                        } else if (CFEqualSafe(agrp, CFSTR("com.apple.sbd"))) {
                            withViewAndBackup(kSOSViewBackupBagV0);
                        } else {
                            withViewAndBackup(kSOSViewOtherSyncable); // (genp)
                        }
                    } else {
                        withViewAndBackup(kSOSViewOtherSyncable); // (inet || keys)
                    }
                }
            } else {
                with(viewHint);
                if (!isTomb) {
                    CFStringRef viewHintTomb = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%@-tomb"), viewHint);
                    if (viewHintTomb) {
                        with(viewHintTomb);
                        CFRelease(viewHintTomb);
                    }
                }
            }
        }
    } else {
        // TODO: general queries
#if 0
        SOSViewRef view;
        CFArrayForEachC(engine->views, view) {
            bool inView = SOSViewQueryMatchItem(view, item);
            if (inView) {
                CFStringRef viewName = SOSViewCopyName(view);
                with(viewName);
                CFReleaseSafe(viewName);
            }
        }
#endif
    }
}

//
// Deliver delayed notifiations of changes in keychain
//

static void
SOSSendViewNotification(CFSetRef viewNotifications)
{
    CFNotificationCenterRef center = CFNotificationCenterGetDarwinNotifyCenter();

    CFSetForEach(viewNotifications, ^(const void *value) {
        secinfo("view", "Sending view notification for view %@", value);

        CFStringRef str = CFStringCreateWithFormat(NULL, NULL, CFSTR("com.apple.security.view-change.%@"), value);
        if (str == NULL)
            return;

        CFNotificationCenterPostNotificationWithOptions(center, str, NULL, NULL, 0);
        CFRelease(str);

    });
}

static void
SOSArmViewNotificationEvents(CFSetRef viewNotifications)
{
    static CFMutableSetRef pendingViewNotifications;
    static dispatch_once_t onceToken;
    static dispatch_queue_t queue;

    dispatch_once(&onceToken, ^{
        queue = dispatch_queue_create("ViewNotificationQueue", NULL);
    });
    if (queue == NULL || CFSetGetCount(viewNotifications) == 0)
        return;

    /*
     * PendingViewNotifications is only modified on queue.
     * PendingViewNotifications is used as a signal if a timer is running.
     *
     * If a timer is running, new events are just added to the existing
     * pendingViewNotifications.
     */

#define DELAY_OF_NOTIFICATION_IN_NS    (NSEC_PER_SEC)

    CFRetain(viewNotifications);

    dispatch_async(queue, ^{
        if (pendingViewNotifications == NULL) {
            pendingViewNotifications = CFSetCreateMutableCopy(NULL, 0, viewNotifications);

            dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)DELAY_OF_NOTIFICATION_IN_NS), queue, ^{
                SOSSendViewNotification(pendingViewNotifications);

                // when timer hits, clear out set of modified views
                CFRelease(pendingViewNotifications);
                pendingViewNotifications = NULL;
            });
        } else {
            CFSetUnion(pendingViewNotifications, viewNotifications);
        }
        CFRelease(viewNotifications);
    });
}


//
// SOSChangeMapper - Helper for SOSEngineUpdateChanges_locked
//
struct SOSChangeMapper {
    SOSEngineRef engine;
    SOSTransactionRef txn;
    SOSDataSourceTransactionPhase phase;
    SOSDataSourceTransactionSource source;
    CFMutableDictionaryRef ct2changes;
    CFMutableSetRef viewNotifications;
};

static void SOSChangeMapperInit(struct SOSChangeMapper *cm, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source) {
    cm->engine = engine;
    cm->txn = txn;
    cm->phase = phase;
    cm->source = source;
    cm->ct2changes = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    cm->viewNotifications = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
}

static void SOSChangeMapperSendNotifications(struct SOSChangeMapper *cm)
{
    SOSArmViewNotificationEvents(cm->viewNotifications);
}

static void SOSChangeMapperFree(struct SOSChangeMapper *cm) {
    CFReleaseSafe(cm->ct2changes);
    CFReleaseSafe(cm->viewNotifications);
}

static void SOSChangeMapperAddViewNotification(struct SOSChangeMapper *cm, CFStringRef view)
{
    assert(isString(view));

    // aggregate the PCS view into one notification
    if (CFStringHasPrefix(view, CFSTR("PCS-"))) {
        view = CFSTR("PCS");
    }
    CFSetSetValue(cm->viewNotifications, view);
}

static void SOSChangeMapperAppendObject(struct SOSChangeMapper *cm, SOSChangeTrackerRef ct, bool isAdd, CFTypeRef object) {
    CFMutableArrayRef changes = (CFMutableArrayRef)CFDictionaryGetValue(cm->ct2changes, ct);
    if (!changes) {
        changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionarySetValue(cm->ct2changes, ct, changes);
        CFReleaseSafe(changes);
    }
    isAdd ? SOSChangesAppendAdd(changes, object) : SOSChangesAppendDelete(changes, object);
}

static bool SOSChangeMapperIngestChange(struct SOSChangeMapper *cm, bool isAdd, CFTypeRef change) {
    bool someoneCares = false;
    if (isData(change)) {
        // TODO: Reenable assertion once the tests have been updated
        //assert(!isAdd);
        // We got a digest for a deleted object. Our dataSource probably couldn't find
        // an object with this digest, probably because it went missing, or it was
        // discovered to be corrupted.
        // Tell all our changeTrackers about this digest since we don't know who might need it.
        CFDictionaryForEach(cm->engine->viewNameSet2ChangeTracker, ^(const void *viewNameSet, const void *ct) {
            SOSChangeMapperAppendObject(cm, (SOSChangeTrackerRef)ct, isAdd, change);
        });
        someoneCares = CFDictionaryGetCount(cm->engine->viewNameSet2ChangeTracker);
    } else {
        // We got an object let's figure out which views it's in and schedule it for
        // delivery to all changeTrackers interested in any of those views.
        SOSObjectRef object = (SOSObjectRef)change;
        CFMutableSetRef changeTrackerSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
        // First gather all the changeTrackers interested in this object (eliminating dupes by collecting them in a set)
        SOSEngineObjectWithView(cm->engine, object, ^(CFStringRef viewName) {
            const void *ctorset = CFDictionaryGetValue(cm->engine->viewName2ChangeTracker, viewName);
            if (isSet(ctorset)) {
                CFSetForEach((CFSetRef)ctorset, ^(const void *ct) { CFSetAddValue(changeTrackerSet, ct); });
            } else if (ctorset) {
                CFSetAddValue(changeTrackerSet, ctorset);
            }


            SOSChangeMapperAddViewNotification(cm, viewName);
        });
        // Then append the object to the changes array in the ct2changes dictionary keyed by viewSet
        CFSetForEach(changeTrackerSet, ^(const void *ct) {
            SOSChangeMapperAppendObject(cm, (SOSChangeTrackerRef)ct, isAdd, object);
        });
        someoneCares = CFSetGetCount(changeTrackerSet);
        CFReleaseSafe(changeTrackerSet);
    }
    return someoneCares;
}

static bool SOSChangeMapperSend(struct SOSChangeMapper *cm, CFErrorRef *error) {
    __block bool ok = true;
    CFDictionaryForEach(cm->ct2changes, ^(const void *ct, const void *changes) {
        ok &= SOSChangeTrackerTrackChanges((SOSChangeTrackerRef)ct, cm->engine, cm->txn, cm->source, cm->phase, (CFArrayRef)changes, error);
    });
    return ok;
}

static bool SOSEngineUpdateChanges_locked(SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, CFArrayRef changes, CFErrorRef *error)
{
    secnoticeq("engine", "%@: %s %s %ld changes, txn=%@, %p", engine->myID, phase == kSOSDataSourceTransactionWillCommit ? "will-commit" : phase == kSOSDataSourceTransactionDidCommit ? "did-commit" : "did-rollback", source == kSOSDataSourceSOSTransaction ? "sos" : "api", CFArrayGetCount(changes), txn, txn);
    bool ok = true;
    switch (phase) {
        case kSOSDataSourceTransactionDidRollback:
            ok &= SOSEngineLoad(engine, txn, error);
            break;
        case kSOSDataSourceTransactionDidCommit: // Corruption causes us to process items at DidCommit
        case kSOSDataSourceTransactionWillCommit:
        {
            bool mappedItemChanged = false;

            struct SOSChangeMapper cm;
            SOSChangeMapperInit(&cm, engine, txn, phase, source);
            SecDbEventRef event;
            CFArrayForEachC(changes, event) {
                CFTypeRef deleted = NULL;
                CFTypeRef inserted = NULL;
                SecDbEventGetComponents(event, &deleted, &inserted, error);
                if (deleted) {
                    bool someoneCares = SOSChangeMapperIngestChange(&cm, false, deleted);
                    if (someoneCares) {
#if TARGET_OS_EMBEDDED
                        SecADAddValueForScalarKey(CFSTR("com.apple.security.sos.delete"), 1);
#endif
                        mappedItemChanged = true;
                    }
                }
                if (inserted) {
                    bool someoneCares = SOSChangeMapperIngestChange(&cm, true, inserted);
                    if (someoneCares) {
#if TARGET_OS_EMBEDDED
                        if (deleted == NULL) {
                            SecADAddValueForScalarKey(CFSTR("com.apple.security.sos.add"), 1);
                        } else {
                            SecADAddValueForScalarKey(CFSTR("com.apple.security.sos.update"), 1);
                        }
#endif
                        mappedItemChanged = true;
                    }
                    if (!someoneCares && !isData(inserted) && SecDbItemIsTombstone((SecDbItemRef)inserted) && !CFEqualSafe(SecDbItemGetValue((SecDbItemRef)inserted, &v7utomb, NULL), kCFBooleanTrue)) {
                        CFErrorRef localError = NULL;
                        // A tombstone was inserted but there is no changetracker that
                        // cares about it.
                        if (!SecDbItemDoDeleteSilently((SecDbItemRef)inserted, (SecDbConnectionRef)txn, &localError)) {
                            secerror("failed to delete tombstone %@ that no one cares about: %@", inserted, localError);
                            CFReleaseNull(localError);
                        }
                    }
                }
            }

            ok &= SOSChangeMapperSend(&cm, error);
            SOSChangeMapperSendNotifications(&cm); // Trigger notifications for view that changes changed
            SOSChangeMapperFree(&cm);

            if (ok && phase == kSOSDataSourceTransactionWillCommit) {
                // Only consider writing if we're in the WillCommit phase.
                // DidCommit phases happen outside the database lock and
                // writing to the DBConn will cause deadlocks.
                if (mappedItemChanged || source == kSOSDataSourceSOSTransaction) {
                    // Write SOSEngine and SOSPeer state to disk
                    secnotice("engine", "saving engine state");
                    ok &= SOSEngineSave(engine, txn, error);

                    if (kSOSDataSourceAPITransaction == source)
                        SOSCCRequestSyncWithPeersList(engine->peerIDs);
                } else {
                    secnotice("engine", "Not saving engine state, nothing changed.");
                }
            }

            break;
        }
    }
    return ok;
}

static void SOSEngineSetNotifyPhaseBlock(SOSEngineRef engine) {
    SOSDataSourceAddNotifyPhaseBlock(engine->dataSource, ^(SOSDataSourceRef ds, SOSTransactionRef txn, SOSDataSourceTransactionPhase phase, SOSDataSourceTransactionSource source, CFArrayRef changes) {
        dispatch_sync(engine->queue, ^{
            CFErrorRef localError = NULL;
            if (!SOSEngineUpdateChanges_locked(engine, txn, phase, source, changes, &localError)) {
                secerror("updateChanged failed: %@", localError);
            }
            CFReleaseSafe(localError);
        });
    });
}

#if 0 // TODO: update these checks
static void SOSEngineCircleChanged_sanitycheck(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers, CFArrayRef untrustedPeers) {
    // Logging code
    CFMutableArrayRef addedPeers = CFArrayCreateDifference(kCFAllocatorDefault, trustedPeers, engine->peerIDs);
    CFMutableArrayRef deletedPeers = CFArrayCreateDifference(kCFAllocatorDefault, engine->peerIDs, trustedPeers);
    CFMutableArrayRef addedUntrustedPeers = CFArrayCreateDifference(kCFAllocatorDefault, untrustedPeers, engine->peerIDs);
    CFMutableArrayRef deletedUntrustedPeers = CFArrayCreateDifference(kCFAllocatorDefault, engine->peerIDs, untrustedPeers);

    CFStringRef tpDesc = SOSPeerIDArrayCreateString(trustedPeers);
    CFStringRef apDesc = SOSPeerIDArrayCreateString(addedPeers);
    CFStringRef dpDesc = SOSPeerIDArrayCreateString(deletedPeers);
    CFStringRef aupDesc = SOSPeerIDArrayCreateString(addedUntrustedPeers);
    CFStringRef dupDesc = SOSPeerIDArrayCreateString(deletedUntrustedPeers);
    secnotice("engine", "trusted %@ added %@ removed %@ add ut: %@ rem ut: %@", tpDesc, apDesc, dpDesc, aupDesc, dupDesc);
    CFReleaseSafe(dupDesc);
    CFReleaseSafe(aupDesc);
    CFReleaseSafe(dpDesc);
    CFReleaseSafe(apDesc);
    CFReleaseSafe(tpDesc);

    // Assertions:
    // Ensure SOSAccount isn't giving us the runaround.
    // Assert that trustedPeers, untrustedPeers and myPeerId are disjoint sets
    if (trustedPeers) {
        CFMutableArrayRef allTrustedPeers = CFArrayCreateDifference(kCFAllocatorDefault, trustedPeers, untrustedPeers);
        assert(CFEqual(trustedPeers, allTrustedPeers));
        CFReleaseSafe(allTrustedPeers);
        assert(!CFArrayContainsValue(trustedPeers, CFRangeMake(0, CFArrayGetCount(trustedPeers)), myPeerID));
    }
    if (untrustedPeers) {
        CFMutableArrayRef allUntrustedPeers = CFArrayCreateDifference(kCFAllocatorDefault, untrustedPeers, trustedPeers);
        assert(CFEqual(untrustedPeers, allUntrustedPeers));
        CFReleaseSafe(allUntrustedPeers);
        assert(!CFArrayContainsValue(untrustedPeers, CFRangeMake(0, CFArrayGetCount(trustedPeers)), myPeerID));
    }

    CFReleaseNull(deletedUntrustedPeers);
    CFReleaseNull(addedUntrustedPeers);
    CFReleaseNull(deletedPeers);
    CFReleaseNull(addedPeers);

    // End of logging and asertions, actual code here.
}
#endif

static SOSChangeTrackerRef SOSReferenceAndGetChangeTracker(CFDictionaryRef lookup, CFMutableDictionaryRef referenced, CFSetRef viewNameSet) {
    SOSChangeTrackerRef ct = (SOSChangeTrackerRef)CFDictionaryGetValue(referenced, viewNameSet);
    if (!ct) {
        ct = (SOSChangeTrackerRef)CFDictionaryGetValue(lookup, viewNameSet);
        if (ct) {
            SOSChangeTrackerResetRegistration(ct);
            CFDictionarySetValue(referenced, viewNameSet, ct);
        } else {
            ct = SOSChangeTrackerCreate(kCFAllocatorDefault, false, NULL, NULL);
            CFDictionarySetValue(referenced, viewNameSet, ct);
            CFReleaseSafe(ct);
        }
    }
    return ct;
}

static void CFStringAppendPeerIDAndViews(CFMutableStringRef desc, CFStringRef peerID, CFSetRef vns) {
    CFStringSetPerformWithDescription(vns, ^(CFStringRef description) {
        CFStringAppendFormat(desc, NULL, CFSTR(" %@ (%@)"), peerID, description);
    });
}

// Must be called after updating viewNameSet2ChangeTracker
static void SOSEngineUpdateViewName2ChangeTracker(SOSEngineRef engine) {
    // Create the mapping from viewName -> ChangeTracker used for lookup during change notification
    CFMutableDictionaryRef newViewName2ChangeTracker = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(engine->viewNameSet2ChangeTracker, ^(const void *viewNameSet, const void *ct) {
        CFSetForEach(viewNameSet, ^(const void *viewName) {
            const void *ctorset = NULL;
            if (CFDictionaryGetValueIfPresent(newViewName2ChangeTracker, viewName, &ctorset)) {
                if (isSet(ctorset)) {
                    CFSetAddValue((CFMutableSetRef)ctorset, ct);
                } else if (!CFEqual(ct, ctorset)) {
                    CFMutableSetRef set = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);
                    CFSetAddValue(set, ctorset);
                    CFSetAddValue(set, ct);
                    CFDictionaryReplaceValue(newViewName2ChangeTracker, viewName, set);
                    CFRelease(set);
                }
            } else {
                CFDictionarySetValue(newViewName2ChangeTracker, viewName, ct);
            }
        });
    });
    CFAssignRetained(engine->viewName2ChangeTracker, newViewName2ChangeTracker);
}

static void SOSEngineSetBackupBag(SOSEngineRef engine, SOSObjectRef bagItem);

// This is called only if we are in a circle and we should listen for keybag changes
static void SOSEngineRegisterBackupBagV0Tracker(SOSEngineRef engine, CFMutableDictionaryRef newViewNameSet2ChangeTracker, CFMutableStringRef desc) {
    SOSChangeTrackerRef bbct = SOSReferenceAndGetChangeTracker(engine->viewNameSet2ChangeTracker, newViewNameSet2ChangeTracker, SOSViewsGetV0BackupBagViewSet());
    SOSChangeTrackerRegisterChangeUpdate(bbct, ^bool(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, CFArrayRef changes, CFErrorRef *error) {
        SOSChangeRef change;
        CFArrayForEachC(changes, change) {
            CFTypeRef object = NULL;
            bool isAdd = SOSChangeGetObject(change, &object);
            SecDbItemRef dbi = (SecDbItemRef)object;
            if (!isData(object) &&
                CFEqualSafe(SecDbItemGetCachedValueWithName(dbi, kSecAttrService), CFSTR("SecureBackupService")) &&
                CFEqualSafe(SecDbItemGetCachedValueWithName(dbi, kSecAttrAccessible), kSecAttrAccessibleWhenUnlocked) &&
                CFEqualSafe(SecDbItemGetCachedValueWithName(dbi, kSecAttrAccount), CFSTR("SecureBackupPublicKeybag"))) {
                SOSEngineSetBackupBag(engine, isAdd ? (SOSObjectRef)object : NULL);
            }
        }
        return true;
    });
}

static void SOSEngineReferenceBackupPeer(SOSEngineRef engine, CFStringRef peerID, CFSetRef viewNameSet, CFDataRef keyBag, CFMutableDictionaryRef newViewNameSet2ChangeTracker, CFMutableDictionaryRef newPeerMap) {
    CFTypeRef oldEntry = CFDictionaryGetValue(engine->peerMap, peerID);
    CFTypeRef newEntry = SOSPeerOrStateSetViewsKeyBagAndCreateCopy(oldEntry, viewNameSet, keyBag);
    if (newEntry) {
        if (isDictionary(newEntry)) {
            // Backup peers, are always inflated
            CFAssignRetained(newEntry, SOSPeerCreateWithState(engine, peerID, newEntry, NULL));
            // If !oldEntry this is an edge (first creation of a peer).
            if (!oldEntry) {
                SOSPeerKeyBagDidChange((SOSPeerRef)newEntry);
            }
        }
        CFDictionarySetValue(newPeerMap, peerID, newEntry);
        CFRelease(newEntry);

        if (keyBag) {
            SOSChangeTrackerRef ct = SOSReferenceAndGetChangeTracker(engine->viewNameSet2ChangeTracker, newViewNameSet2ChangeTracker, viewNameSet);
            
            SOSChangeTrackerUpdatesChanges child = Block_copy(^bool(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, CFArrayRef changes, CFErrorRef *error) {
                return SOSPeerDataSourceWillChange((SOSPeerRef)newEntry, SOSEngineGetDataSource(engine), source, changes, error);
            });
           
            SOSChangeTrackerRegisterChangeUpdate(ct, child);
            Block_release(child);
        }
    }
}

static void SOSEngineReferenceSyncPeer(SOSEngineRef engine, CFStringRef peerID, CFSetRef viewNameSet, CFMutableDictionaryRef newViewNameSet2ChangeTracker, CFMutableDictionaryRef newPeerMap) {
    CFTypeRef newEntry = SOSPeerOrStateSetViewsKeyBagAndCreateCopy(CFDictionaryGetValue(engine->peerMap, peerID), viewNameSet, NULL);
    if (newEntry) {
        SOSChangeTrackerRef ct = SOSReferenceAndGetChangeTracker(engine->viewNameSet2ChangeTracker, newViewNameSet2ChangeTracker, viewNameSet);
        // Standard peer, inflated on demand
        SOSChangeTrackerUpdatesManifests trackManifest;
        if (isDictionary(newEntry)) {
            // Uninflated peer, inflate on first notification.
            trackManifest = ^bool(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
                CFErrorRef localError = NULL;
                SOSPeerRef peer = SOSEngineCopyPeerWithID_locked(engine, peerID, &localError);
                bool ok;
                if (!peer) {
                    secerror("%@: peer failed to inflate: %@", peerID, localError);
                    CFReleaseSafe(localError);
                    ok = false;
                } else {
                    ok = SOSPeerDataSourceWillCommit(peer, source, removals, additions, error);
                }
                CFReleaseSafe(peer);
                return ok;
            };
        } else {
            // Inflated peer, just forward the changes to the peer
            trackManifest = ^bool(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
                return SOSPeerDataSourceWillCommit((SOSPeerRef)newEntry, source, removals, additions, error);
            };
        }
        SOSChangeTrackerUpdatesManifests trackManifestCopy = Block_copy(trackManifest);
        SOSChangeTrackerRegisterManifestUpdate(ct, trackManifestCopy);
        Block_release(trackManifestCopy);

        CFDictionarySetValue(newPeerMap, peerID, newEntry);
        CFRelease(newEntry);
    }
}


static void SOSEngineReferenceTrustedPeer(SOSEngineRef engine, SOSPeerMetaRef peerMeta, CFMutableDictionaryRef newViewNameSet2ChangeTracker, CFMutableDictionaryRef newPeerMap, CFMutableArrayRef peerIDs, CFMutableStringRef desc) {
    CFSetRef viewNameSet = NULL;
    CFDataRef keyBag = NULL;
    CFStringRef peerID = SOSPeerMetaGetComponents(peerMeta, &viewNameSet, &keyBag, NULL);
    // We trust peerID so append it to peerIDs
    CFArrayAppendValue(peerIDs, peerID);
    if (desc) CFStringAppendPeerIDAndViews(desc, peerID, viewNameSet);
    // Update the viewNameSet for this peer, to appease tests, default to a viewset of the V0 view.
    if (!viewNameSet)
        viewNameSet = SOSViewsGetV0ViewSet();

    // Always inflate backup peers, since they need to register with their changeTrackers right away.
    if (keyBag) {
        SOSEngineReferenceBackupPeer(engine, peerID, viewNameSet, keyBag, newViewNameSet2ChangeTracker, newPeerMap);
    } else {
        SOSEngineReferenceSyncPeer(engine, peerID, viewNameSet, newViewNameSet2ChangeTracker, newPeerMap);
    }
}

static CFDataRef SOSEngineLoadV0KeyBag(SOSEngineRef engine, CFErrorRef *error) {
    // Return the keybag for the given peerID.
    /*
     Values for V0 are:
     kSecAttrAccessGroup ==> CFSTR("com.apple.sbd")
     kSecAttrAccessible  ==> kSecAttrAccessibleWhenUnlocked
     kSecAttrAccount     ==> CFSTR("SecureBackupPublicKeybag")
     kSecAttrService     ==> CFSTR("SecureBackupService")
     */

    CFMutableDictionaryRef keys = CFDictionaryCreateMutableForCFTypesWith(kCFAllocatorDefault,
                                            kSecAttrAccessGroup, CFSTR("com.apple.sbd"),
                                            kSecAttrAccount, CFSTR("SecureBackupPublicKeybag"),
                                            kSecAttrService, CFSTR("SecureBackupService"),
                                            kSecAttrAccessible, kSecAttrAccessibleWhenUnlocked,
                                            kSecAttrSynchronizable, kCFBooleanTrue,
                                            NULL);

    CFDataRef keybag = engine->dataSource->dsCopyItemDataWithKeys(engine->dataSource, keys, error);
    CFReleaseSafe(keys);

    return keybag;
}

static void SOSEngineReferenceBackupV0Peer(SOSEngineRef engine, CFMutableDictionaryRef newViewNameSet2ChangeTracker, CFMutableDictionaryRef newPeerMap, CFMutableArrayRef newPeerIDs, CFMutableStringRef desc) {
    SOSPeerRef backupPeer = (SOSPeerRef)CFDictionaryGetValue(engine->peerMap, kSOSViewKeychainV0_tomb);
    CFDataRef bag = NULL;
    if (backupPeer && CFGetTypeID(backupPeer) == SOSPeerGetTypeID()) {
        bag = SOSPeerGetKeyBag(backupPeer);
    } else {
        CFErrorRef localError = NULL;
        if (!(bag = SOSEngineLoadV0KeyBag(engine, &localError))) {
            secnotice("engine", "No keybag found for v0 backup peer: %@", localError);
            CFReleaseSafe(localError);
        }
    }
    SOSEngineReferenceBackupPeer(engine, kSOSViewKeychainV0_tomb, SOSViewsGetV0BackupViewSet(), bag, newViewNameSet2ChangeTracker, newPeerMap);
}

static void SOSEngineReferenceTrustedPeers(SOSEngineRef engine, CFMutableDictionaryRef newViewNameSet2ChangeTracker, CFMutableDictionaryRef newPeerMap, CFMutableArrayRef newPeerIDs, CFArrayRef trustedPeerMetas, CFMutableStringRef desc) {
    // Then update the views for all trusted peers and add them to newPeerMap.
    if (trustedPeerMetas != NULL && CFArrayGetCount(trustedPeerMetas) != 0) {
        if (desc) CFStringAppend(desc, CFSTR(" trusted"));
        // Remake engine->peerIDs
        SOSPeerMetaRef peerMeta;
        CFArrayForEachC(trustedPeerMetas, peerMeta) {
            SOSEngineReferenceTrustedPeer(engine, peerMeta, newViewNameSet2ChangeTracker, newPeerMap, newPeerIDs, desc);
        }
    }
}

static void SOSEngineReferenceUntrustedPeers(SOSEngineRef engine, CFMutableDictionaryRef newPeerMap, CFArrayRef untrustedPeerMetas, CFMutableStringRef description) {
    // Copy any untrustedPeers to newPeerMap as well if we have a state
    // for them, if not no big deal.  We also serialize all the untrustedPeers
    // since they don't need to be deserializable
    if (untrustedPeerMetas != NULL && CFArrayGetCount(untrustedPeerMetas) != 0) {
        if (description) CFStringAppend(description, CFSTR(" untrusted"));
        SOSPeerMetaRef peerMeta;
        CFArrayForEachC(untrustedPeerMetas, peerMeta) {
            CFSetRef views = NULL;
            CFStringRef peerID = SOSPeerMetaGetComponents(peerMeta, &views, NULL, NULL);
            if (description) CFStringAppendPeerIDAndViews(description, peerID, views);
            CFSetRef nviews = NULL;
            if (!views)
                views = nviews = CFSetCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeSetCallBacks);
            CFTypeRef newEntry = SOSPeerOrStateSetViewsAndCopyState(CFDictionaryGetValue(engine->peerMap, peerID), views);
            CFReleaseSafe(nviews);
            if (newEntry) {
                CFDictionarySetValue(newPeerMap, peerID, newEntry);
                CFReleaseSafe(newEntry);
            }
        }
    }
}

static void SOSEngineReferenceChangeTrackers(SOSEngineRef engine, CFArrayRef trustedPeerMetas, CFArrayRef untrustedPeerMetas, CFMutableStringRef desc) {
    CFMutableArrayRef newPeerIDs = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef newPeerMap = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableDictionaryRef newViewNameSet2ChangeTracker = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    if (engine->myID) {
        // We have an engineID => in a circle (with 0 or more peers)
        // Ensure we have a v0 backup peer and it's listening for backup bag changes
        SOSEngineReferenceBackupV0Peer(engine, newViewNameSet2ChangeTracker, newPeerMap, newPeerIDs, desc);
        SOSEngineRegisterBackupBagV0Tracker(engine, newViewNameSet2ChangeTracker, desc);
    }
    SOSEngineReferenceTrustedPeers(engine, newViewNameSet2ChangeTracker, newPeerMap, newPeerIDs, trustedPeerMetas, desc);
    SOSEngineReferenceUntrustedPeers(engine, newPeerMap, untrustedPeerMetas, desc);

    CFAssignRetained(engine->peerIDs, newPeerIDs);
    CFAssignRetained(engine->peerMap, newPeerMap);
    CFAssignRetained(engine->viewNameSet2ChangeTracker, newViewNameSet2ChangeTracker);
    SOSEngineUpdateViewName2ChangeTracker(engine);
}

// Return true iff peers or views changed
static bool SOSEngineSetPeers_locked(SOSEngineRef engine, SOSPeerMetaRef myPeerMeta, CFArrayRef trustedPeerMetas, CFArrayRef untrustedPeerMetas) {
    CFErrorRef error = NULL;
    CFSetRef myViews = NULL;
    CFDataRef myKeyBag = NULL;
    CFMutableStringRef desc = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("me"));
    CFStringRef myPeerID = myPeerMeta ? SOSPeerMetaGetComponents(myPeerMeta, &myViews, &myKeyBag, &error) : NULL;
    if (desc) CFStringAppendPeerIDAndViews(desc, myPeerID, myViews);

    // Start with no coders
    CFMutableDictionaryRef codersToKeep = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

    if(engine->haveLoadedCoders){
        // If we're the same peerID we keep known peers (both trusted and untrusted)
        if (CFEqualSafe(myPeerID, engine->myID)) {
            void (^copyPeerMetasCoder)(const void *value) = ^(const void*element) {
                SOSPeerMetaRef peerMeta = (SOSPeerMetaRef) element;

                CFStringRef currentID = SOSPeerMetaGetComponents(peerMeta, NULL, NULL, NULL);
                if (currentID) {
                    SOSCoderRef coder = (SOSCoderRef) CFDictionaryGetValue(engine->coders, currentID);
                    if (coder) {
                        CFDictionarySetValue(codersToKeep, currentID, coder);
                    }
                }
            };

            if (trustedPeerMetas) {
                CFArrayForEach(trustedPeerMetas, copyPeerMetasCoder);
            }
            if (untrustedPeerMetas) {
                CFArrayForEach(untrustedPeerMetas, copyPeerMetasCoder);
            }
        }

        engine->codersNeedSaving = true;
    }
    CFRetainAssign(engine->myID, myPeerID);
    CFTransferRetained(engine->coders, codersToKeep);

    // Remake engine->peerMap from both trusted and untrusted peers
    SOSEngineReferenceChangeTrackers(engine, trustedPeerMetas, untrustedPeerMetas, desc);

    secnotice("engine", "%@", desc);
    CFReleaseSafe(desc);
    return true;
}

static void SOSEngineApplyPeerState(SOSEngineRef engine, CFDictionaryRef peerStateMap) {
    if (peerStateMap) CFDictionaryForEach(peerStateMap, ^(const void *peerID, const void *peerState) {
        CFTypeRef mapEntry = CFDictionaryGetValue(engine->peerMap, peerID);
        if (mapEntry && CFGetTypeID(mapEntry) == SOSPeerGetTypeID()) {
            // Update the state of any already inflated peers
            SOSPeerRef peer = (SOSPeerRef)mapEntry;
            CFErrorRef localError = NULL;
            if (!SOSPeerSetState(peer, engine, peerState, &localError)) {
                CFStringRef stateHex = NULL;
                stateHex = CFDataCopyHexString(peerState);
                secerror("peer: %@: bad state: %@ in engine state: %@", peerID, localError, stateHex);
                CFReleaseSafe(stateHex);
                CFReleaseNull(localError);
                // Possibly ask for an ensurePeerRegistration so we have a good list of peers again.
            }
        } else {
            // Just record the state for non inflated peers for now.
            CFDictionarySetValue(engine->peerMap, peerID, peerState);
        }
    });
}

static void SOSEngineSynthesizePeerMetas(SOSEngineRef engine, CFMutableArrayRef trustedPeersMetas, CFMutableArrayRef untrustedPeers) {
    CFSetRef trustedPeerSet = engine->peerIDs ? CFSetCreateCopyOfArrayForCFTypes(engine->peerIDs) : NULL;
    CFDictionaryForEach(engine->peerMap, ^(const void *peerID, const void *peerState) {
        SOSPeerMetaRef meta = NULL;
        if (peerState && CFGetTypeID(peerState) == SOSPeerGetTypeID()) {
            SOSPeerRef peer = (SOSPeerRef)peerState;
            meta = SOSPeerMetaCreateWithComponents(peerID, SOSPeerGetViewNameSet(peer), SOSPeerGetKeyBag(peer));
        } else {
            // We don't need to add the meta for the backup case, since
            //   SOSEngineReferenceBackupV0Peer will do the right thing
            if (!CFEqualSafe(peerID, kSOSViewKeychainV0_tomb)) {
                meta = SOSPeerMetaCreateWithState(peerID, peerState);
            }
        }
        // Any peer in peerStateMap that is not in trustedPeers is an untrustedPeer unless it's the v0 backup peer
        if ((trustedPeerSet && CFSetContainsValue(trustedPeerSet, peerID)) || CFEqualSafe(peerID, kSOSViewKeychainV0_tomb)) {
            if (meta) {
                CFArrayAppendValue(trustedPeersMetas, meta);
            }
        } else {
            CFArrayAppendValue(untrustedPeers, peerID);
        }
        CFReleaseNull(meta);
    });
    CFReleaseNull(trustedPeerSet);
}

static void SOSEngineSetBackupBag(SOSEngineRef engine, SOSObjectRef bagItem) {
    CFMutableStringRef desc = NULL;
    SOSPeerRef backupPeer = SOSEngineCopyPeerWithID_locked(engine, kSOSViewKeychainV0_tomb, NULL);
    CFDataRef keybag = NULL;
    if (bagItem) {
        keybag = SecDbItemGetValue((SecDbItemRef)bagItem, &v6v_Data, NULL);
    }

    // Since SOSPeerSetKeyBag() doesn't notify on the edge from NULL->initial keybag, since
    // that is the right behaviour for non v0 backup peers, we need to do it here for the v0 peer.
    bool hadBag = SOSPeerGetKeyBag(backupPeer);
    SOSPeerSetKeyBag(backupPeer, keybag);
    if (!hadBag)
        SOSPeerKeyBagDidChange(backupPeer);

    CFReleaseSafe(backupPeer);

    CFMutableArrayRef untrustedPeerMetas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFMutableArrayRef trustedPeersMetas = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSEngineSynthesizePeerMetas(engine, trustedPeersMetas, untrustedPeerMetas);
    SOSEngineReferenceChangeTrackers(engine, trustedPeersMetas, untrustedPeerMetas, desc);
    CFReleaseSafe(trustedPeersMetas);
    CFReleaseSafe(untrustedPeerMetas);
}

#define SECONDS_PER_DAY  (86400.0)

#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
#define TRACE_INTERVAL (7 * SECONDS_PER_DAY)
#elif (TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR)
#define TRACE_INTERVAL (1 * SECONDS_PER_DAY)
#endif

#ifdef TRACE_INTERVAL
static void SOSEngineCloudKeychainTrace(SOSEngineRef engine, CFAbsoluteTime now) {
    CFAssignRetained(engine->lastTraceDate, CFDateCreate(kCFAllocatorDefault, now));
    CFIndex num_peers = engine->peerIDs ? 1 + CFArrayGetCount(engine->peerIDs) : 1;
    SOSManifestRef manifest = SOSEngineCopyManifestWithViewNameSet_locked(engine, SOSViewsGetV0ViewSet(), NULL);
    if (!manifest)
        manifest = SOSDataSourceCopyManifestWithViewNameSet(engine->dataSource, SOSViewsGetV0ViewSet(), NULL);
    size_t num_items = SOSManifestGetCount(manifest);
    CFReleaseSafe(manifest);

    struct _SecServerKeyStats genpStats = { };
    struct _SecServerKeyStats inetStats = { };
    struct _SecServerKeyStats keysStats = { };

    _SecServerGetKeyStats(&genp_class, &genpStats);
    _SecServerGetKeyStats(&inet_class, &inetStats);
    _SecServerGetKeyStats(&keys_class, &keysStats);

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        CloudKeychainTrace(num_peers, num_items, &genpStats, &inetStats, &keysStats);
    });
}
#endif

static void SOSEngineCloudKeychainTraceIfNeeded(SOSEngineRef engine) {
#ifdef TRACE_INTERVAL
    if (!engine->myID)
        return;
    CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
    if (engine->lastTraceDate) {
        CFAbsoluteTime lastTraceTime = CFDateGetAbsoluteTime(engine->lastTraceDate);
        if ((now - lastTraceTime) >= TRACE_INTERVAL) {
            SOSEngineCloudKeychainTrace(engine, now);
        }
    } else {
        SOSEngineCloudKeychainTrace(engine, now);
    }
#endif
}


static bool SOSEngineCircleChanged_locked(SOSEngineRef engine, SOSPeerMetaRef myPeerMeta, CFArrayRef trustedPeers, CFArrayRef untrustedPeers) {
    // Sanity check params
//    SOSEngineCircleChanged_sanitycheck(engine, myPeerID, trustedPeers, untrustedPeers);

    // Transform from SOSPeerInfoRefs to CFDictionaries with the info we want per peer.
    // Or, Tell the real SOSPeerRef what the SOSPeerInfoRef is and have it copy out the data it needs.
    bool peersOrViewsChanged = SOSEngineSetPeers_locked(engine, myPeerMeta, trustedPeers, untrustedPeers);

    // Run though all peers and only cache manifests for peers we still have
    CFErrorRef localError = NULL;
    if (!SOSEngineGCPeerState_locked(engine, &localError)) {
        secerror("SOSEngineGCPeerState_locked failed: %@", localError);
        CFReleaseNull(localError);
    }
    return peersOrViewsChanged;
}

// Initialize the engine if a load fails.  Basically this is our first time setup
static bool SOSEngineInit(SOSEngineRef engine, CFErrorRef *error) {
    bool ok = true;
    secnotice("engine", "new engine for datasource named %@", SOSDataSourceGetName(engine->dataSource));
    CFAssignRetained(engine->peerMap, CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault));
    CFAssignRetained(engine->viewNameSet2ChangeTracker, CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault));
    CFAssignRetained(engine->viewName2ChangeTracker, CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault));
    CFReleaseNull(engine->manifestCache);
    CFReleaseNull(engine->peerIDs);
    // TODO: We shouldn't need to load the backup bag if there was no engine
    // state (load failed), since that means there was no circle nor were we an applicant.

    // Set up change trackers so we know when a backup peer needs to be created?
    // no, since myID is not set, we are not in a circle, so no need to back up
    SOSEngineSetPeers_locked(engine, NULL, NULL, NULL);
    return ok;
}

// Called by our DataSource in its constructor
SOSEngineRef SOSEngineCreate(SOSDataSourceRef dataSource, CFErrorRef *error) {
    SOSEngineRef engine = NULL;
    engine = CFTypeAllocate(SOSEngine, struct __OpaqueSOSEngine, kCFAllocatorDefault);
    engine->dataSource = dataSource;
    engine->queue = dispatch_queue_create("engine", DISPATCH_QUEUE_SERIAL);

    engine->peerMap = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    engine->viewNameSet2ChangeTracker = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    engine->viewName2ChangeTracker = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    //engine->syncCompleteQueue = NULL;
    engine->syncCompleteListener = NULL;
    engine->coders = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    engine->haveLoadedCoders = false;
    engine->codersNeedSaving = false;
    
    CFErrorRef engineError = NULL;
    if (!SOSEngineLoad(engine, NULL, &engineError)) {
        secwarning("engine failed load state starting with nothing %@", engineError);
        CFReleaseNull(engineError);
        if (!SOSEngineInit(engine, error)) {
            secerror("engine failed to initialze %@ giving up", error ? *error : NULL);
        }
    } else {
        // Successfully loaded engine state, let's trace if we haven't in a while
        SOSEngineCloudKeychainTraceIfNeeded(engine);
    }
    SOSEngineSetNotifyPhaseBlock(engine);
    return engine;
}

// --- Called from off the queue, need to move to on the queue

static void SOSEngineDoOnQueue(SOSEngineRef engine, dispatch_block_t action)
{
    dispatch_sync(engine->queue, action);
}

static bool SOSEngineDoTxnOnQueue(SOSEngineRef engine, CFErrorRef *error, void(^transaction)(SOSTransactionRef txn, bool *commit))
{
    return SOSDataSourceWithCommitQueue(engine->dataSource, error, ^(SOSTransactionRef txn, bool *commit) {
        SOSEngineDoOnQueue(engine, ^{ transaction(txn, commit); });
    });
}

//
// MARK: SOSEngine API
//

void SOSEngineDispose(SOSEngineRef engine) {
    // NOOP Engines stick around forever to monitor dataSource changes.
    engine->dataSource = NULL;
    CFReleaseNull(engine->coders);
}

void SOSEngineForEachPeer(SOSEngineRef engine, void (^with)(SOSPeerRef peer)) {
    SOSEngineDoOnQueue(engine, ^{
        SOSEngineForEachPeer_locked(engine, with);
    });
}

static void SOSEngineForEachBackupPeer(SOSEngineRef engine, void (^with)(SOSPeerRef peer)) {
    SOSEngineDoOnQueue(engine, ^{
        SOSEngineForEachBackupPeer_locked(engine, with);
    });
}

#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
static const CFStringRef kSecADSecurityNewItemSyncTimeKey = CFSTR("com.apple.security.secureobjectsync.itemtime.new");
static const CFStringRef kSecADSecurityKnownItemSyncTimeKey = CFSTR("com.apple.security.secureobjectsync.itemtime.known");
#else
#endif


static void ReportItemSyncTime(SOSDataSourceRef ds, bool known, SOSObjectRef object)
{
    CFDateRef itemModDate = SOSObjectCopyModificationDate(ds, object, NULL);
    if (itemModDate) {
        CFAbsoluteTime syncTime = 0;
        CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();

        CFAbsoluteTime peerModificationAbsoluteTime = CFDateGetAbsoluteTime(itemModDate);
        if (peerModificationAbsoluteTime > now) {
            syncTime = now - peerModificationAbsoluteTime;
        }

#if TARGET_OS_EMBEDDED && !TARGET_IPHONE_SIMULATOR
        SecADClientPushValueForDistributionKey(known ? kSecADSecurityKnownItemSyncTimeKey : kSecADSecurityNewItemSyncTimeKey,
                                               SecBucket2Significant(syncTime));
#else
#endif
    }
}

/* Handle incoming message from peer p.  Return false if there was an error, true otherwise. */
bool SOSEngineHandleMessage_locked(SOSEngineRef engine, CFStringRef peerID, SOSMessageRef message,
                                   SOSTransactionRef txn, bool *commit, bool *somethingChanged, CFErrorRef *error) {
    SOSPeerRef peer = SOSEngineCopyPeerWithID_locked(engine, peerID, error);
    if (!peer) return false;

    CFStringRef peerDesc = NULL;
    SOSManifestRef localManifest = NULL;
    SOSManifestRef allAdditions = NULL;
    SOSManifestRef unwanted = NULL;
    SOSManifestRef confirmed = NULL;
    SOSManifestRef base = NULL;
    SOSManifestRef confirmedRemovals = NULL, confirmedAdditions = NULL;
    __block struct SOSDigestVector receivedObjects = SOSDigestVectorInit;
    __block struct SOSDigestVector unwantedObjects = SOSDigestVectorInit;

    // Check for unknown criticial extensions in the message, and handle
    // any other extensions we support
    __block bool ok = true;
    CFMutableArrayRef changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);

    require_action_quiet(peer, exit, ok = SOSErrorCreate(errSecParam, error, NULL, CFSTR("Couldn't create peer with Engine for %@"), peerID));
    peerDesc = CFCopyDescription(peer);

    bool hadBeenInSyncAtStart = SOSPeerHasBeenInSync(peer);

    SOSMessageWithExtensions(message, true, ^(CFDataRef oid, bool isCritical, CFDataRef extension, bool *stop) {
        // OMFG a Critical extension what shall I do!
        ok = SOSErrorCreate(kSOSErrorNotReady, error, NULL, CFSTR("Unknown criticial extension in peer message"));
        *stop = true;
    });
    require_quiet(ok, exit);

    // Merge Objects from the message into our DataSource.
    // Should we move the transaction to the SOSAccount level?
    // TODO: Filter incoming objects
    //if (!SOSDataSourceForEachObjectInViewSet(engine->dataSource, pendingObjects, SOSPeerGetViewNameSet(peer), error, ^void(CFDataRef key, SOSObjectRef object, bool *stop) {
    require_quiet(ok &= SOSMessageWithSOSObjects(message, engine->dataSource, error, ^(SOSObjectRef peersObject, bool *stop) {
        CFDataRef digest = SOSObjectCopyDigest(engine->dataSource, peersObject, error);
        if (!digest) {
            *stop = true;
            *commit = false;
            secerror("%@ peer sent bad object: %@, rolling back changes", SOSPeerGetID(peer), error ? *error : NULL);
            return;
        }
        SOSDigestVectorAppend(&receivedObjects, CFDataGetBytePtr(digest));
        SOSObjectRef mergedObject = NULL;
        SOSMergeResult mr = SOSDataSourceMergeObject(engine->dataSource, txn, peersObject, &mergedObject, error);
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
            if (!CFEqual(mergedObject, peersObject)) {
                // Record this object as something we don't want peer to ever send us again.  By adding it to
                // unwantedObjects we'll falsely claim to peer we have it until they tell us they don't have it anymore.
                SOSDigestVectorAppend(&unwantedObjects, CFDataGetBytePtr(digest));
            }
            // Ensure localObject is in local manifest (possible corruption) by posting an update when we are done.
            SOSChangesAppendAdd(changes, mergedObject);
        }

        if (ok && hadBeenInSyncAtStart) {
            ReportItemSyncTime(engine->dataSource,
                               mr == kSOSMergeLocalObject,
                               peersObject);
        }

        CFReleaseSafe(mergedObject);
        CFReleaseSafe(digest);
    }), exit);
    struct SOSDigestVector dvunion = SOSDigestVectorInit;
    SOSDigestVectorSort(&receivedObjects);
    SOSDigestVectorUnionSorted(SOSManifestGetDigestVector(SOSMessageGetAdditions(message)), &receivedObjects, &dvunion);
    allAdditions = SOSManifestCreateWithDigestVector(&dvunion, error);
    SOSDigestVectorFree(&receivedObjects);
    SOSDigestVectorFree(&dvunion);

    unwanted = SOSManifestCreateWithDigestVector(&unwantedObjects, error);
    SOSDigestVectorFree(&unwantedObjects);

    if (CFArrayGetCount(changes)) {
        // NOTE: This is always notifiying of all additions that end up choosing local, which should be rare, since we shouldn't
        // be receiving objects we already have.   When we do we tell ourselves to add them all again so our views will properly
        // reflect that we actually have these objects if we didn't already.

        // Ensure any objects that we received and have locally already are actually in our local manifest
        SOSEngineUpdateChanges_locked(engine, txn, kSOSDataSourceTransactionDidCommit, kSOSDataSourceSOSTransaction, changes, error);
    }
    CFReleaseSafe(changes);

    // ---- Don't use local or peer manifests from above this line,
    // ---- since commiting the SOSDataSourceWith transaction might change them ---

    // Take a snapshot of our dataSource's local manifest.
    require_quiet(ok = localManifest = SOSEngineCopyLocalPeerManifest_locked(engine, peer, error), exit);

    CFDataRef baseDigest = SOSMessageGetBaseDigest(message);
    CFDataRef proposedDigest = SOSMessageGetProposedDigest(message);
    
#if 0
    // I believe this is no longer needed now that we have eliminated extra,
    // since this is handled below once we get a confirmed manifest from our
    // peer.
    
    // If we just received a L00 reset pendingObjects to localManifest
    if (!baseDigest && !proposedDigest) {
        // TODO: This is definitely busted for v0 peers since v0 peers always send a
        // L00 (ManifestDigestMessage as an ack) whereas in v2 this is a protocol restart
        // However if we can still find a confirmed manifest below we probably
        // don't want to do this even for v2.
        // Also I don't think we will ever send a ManifestMessage right now in
        // response to a ManifestDigest
        SOSPeerSetPendingObjects(peer, localManifest);
        secnoticeq("engine", "%@:%@ SOSPeerSetPendingObjects: %@", engine->myID, peerID, localManifest);
    }
#endif

    base = SOSPeerCopyManifestForDigest(peer, baseDigest);
    confirmed = SOSPeerCopyManifestForDigest(peer, SOSMessageGetSenderDigest(message));
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
            secerror("%@:%@ Protocol error send L00 - figure out later base: %@", engine->myID, peerID, base);
        }
    }
    secnoticeq("engine", "%@:%@ confirmed: %@ base: %@", engine->myID, peerID, confirmed, base);
    if (confirmed) {
        ok &= SOSManifestDiff(SOSPeerGetConfirmedManifest(peer), confirmed, &confirmedRemovals, &confirmedAdditions, error);
        if (SOSManifestGetCount(SOSMessageGetRemovals(message)))
            CFAssignRetained(confirmedRemovals, SOSManifestCreateUnion(confirmedRemovals, SOSMessageGetRemovals(message), error));
    }
    if (SOSManifestGetCount(confirmedRemovals) || SOSManifestGetCount(confirmedAdditions) || SOSManifestGetCount(unwanted))
        ok &= SOSPeerDidReceiveRemovalsAndAdditions(peer, confirmedRemovals, confirmedAdditions, unwanted, localManifest, error);
    // TODO: We should probably remove the if below and always call SOSPeerSetConfirmedManifest,
    // since having a NULL confirmed will force us to send a manifest message to get in sync again.
    if (confirmed)
        SOSPeerSetConfirmedManifest(peer, confirmed);
    else if (SOSPeerGetConfirmedManifest(peer)) {
        secnoticeq("engine", "%@:%@ unable to find confirmed in %@, sync protocol reset", engine->myID, peer, message);

        SOSPeerSetConfirmedManifest(peer, NULL);
        //SOSPeerSetSendObjects(peer, true);
    }

    // ---- SendObjects and extra->pendingObjects promotion dance ----

    // The first block of code below sets peer.sendObjects to true when we receive a L00 and the second block
    // moves extra to pendingObjects once we receive a confirmed manifest in or after the L00.
    if (!baseDigest && !proposedDigest) {
        SOSPeerSetSendObjects(peer, true);
    }

    if (0 /* confirmed && SOSPeerSendObjects(peer) */) {
        SOSManifestRef allExtra = NULL;
        ok &= SOSManifestDiff(confirmed, localManifest, NULL, &allExtra, error);
        secnoticeq("engine", "%@:%@ confirmed %@ (re)setting O:%@", engine->myID, SOSPeerGetID(peer), confirmed, allExtra);
        SOSPeerSetPendingObjects(peer, allExtra);
        CFReleaseSafe(allExtra);
    }

exit:
    secnotice("engine", "recv %@:%@ %@", engine->myID, SOSPeerGetID(peer), message);
    secnotice("peer", "recv %@ -> %@", peerDesc, peer);

    CFReleaseNull(base);
    CFReleaseSafe(confirmed);
    CFReleaseSafe(localManifest);
    CFReleaseSafe(peerDesc);
    CFReleaseSafe(allAdditions);
    CFReleaseSafe(unwanted);
    CFReleaseSafe(confirmedRemovals);
    CFReleaseSafe(confirmedAdditions);
    CFReleaseSafe(peer);
    return ok;
}

static CFDataRef SOSEngineCopyObjectDER(SOSEngineRef engine, SOSObjectRef object, CFErrorRef *error) {
    CFDataRef der = NULL;
    CFDictionaryRef plist = SOSObjectCopyPropertyList(engine->dataSource, object, error);
    if (plist) {
        der = CFPropertyListCreateDERData(kCFAllocatorDefault, plist, error);
        CFRelease(plist);
    }
    return der;
}


/*

            +-----------------------------+_
            |            |                | \
            |      A      |    T          |  \
            |              |              |   \
           _+=============================+    } L
          / |                             |   /
         /  |              S              |  /
        /   |                             |_/
       /    +==============================
      /     |                             |
   C {      |                             |
      \     |        M       +------------|
       \    |               |             |
        \   |              |      U       |
         \  |             |               |
          \_+-------------+---------------+

A assumed
T to be sent
S shared
M missing
U unwanted
L local
C confirmed

*/
#if 0
static bool SOSAppendRemoveToPatch(CFTypeRef remove, CFMutableDictionaryRef patch, CFErrorRef *error) {
}

static bool SOSAppendAddToPatch(CFTypeRef add, CFMutableDictionaryRef patch, CFErrorRef *error) {
}

static bool SOSAppendDiffToPatch(CFTypeRef left, CFTypeRef right, CFMutableDictionaryRef patch, CFErrorRef *error) {
    bool ok = true;
    if (!left && right) {
        SOSAppendAddToPatch(right, patch, error);
    } else if (left && !right) {
        SOSAppendRemoveToPatch(left, patch, error);
    } else if (left && right) {
        CFTypeID ltype = CFGetTypeID(left);
        CFTypeID rtype = CFGetTypeID(right);
        if (ltype == rtype) {
            if (CFArrayGetTypeID() == ltype) {
                ok = SecError(errSecParam, error, CFSTR("unsupported type array"), ltype);
            } else if (CFBooleanGetTypeID == ltype) {
                ok = SecError(errSecParam, error, CFSTR("unsupported type boolean"), ltype);
            } else if (CFDataGetTypeID == ltype) {
                ok = SecError(errSecParam, error, CFSTR("unsupported type data"), ltype);
            } else if (CFDictionaryGetTypeID == ltype) {
                __block CFMutableDictionaryRef leftnotright = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
                __block CFMutableDictionaryRef rightnotleft = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, right);

                CFDictionaryForEach(left, ^(const void *key, const void *lvalue) {
                    const void *rvalue = NULL;
                    if (CFDictionaryGetValueIfPresent(right, key, &rvalue)) {
                        CFDictionaryRemoveValue(rightnotleft, key);

                        CFMutableDictionaryRef subpatch = CFDictionaryCreateForCFTypes(kCFAllocatorDefault);
                        CFDictionaryAddValue(patch, key, subpatch);
                        SOSAppendDiffToPatch(lvalue, rvalue, subpatch, error);
                        CFReleaseSafe(subpatch);
                    } else {
                        CFDictionaryAddValue(leftnotright, key, lvalue);
                    }
                });
                // Proccess leftnotright and rightnotleft
                CFReleaseSafe(leftnotright);
                CFReleaseSafe(rightnotleft);
            } else if (SOSManifestGetTypeID == ltype) {
                SOSManifestRef removed = NULL, added = NULL;
                ok &= SOSManifestDiff(left, right, &removed, &added, error);
                if (SOSManifestGetCount(removed) || SOSManifestGetCount(added)) {
                    SOSAppendDiffToPatch(lvalue, rvalue, subpatch, error);
                    CFStringAppend(, <#CFStringRef appendedString#>)
                }
                CFReleaseSafe(removed);
                CFReleaseSafe(added);
            } else if (CFNumberGetTypeID == ltype) {
                ok = SecError(errSecParam, error, CFSTR("unsupported type number"), ltype);
            } else if (CFSetGetTypeID == ltype) {
                ok = SecError(errSecParam, error, CFSTR("unsupported type set"), ltype);
            } else if (CFStringGetTypeID == ltype) {
                ok = SecError(errSecParam, error, CFSTR("unsupported type string"), ltype);
            } else {
                ok = SecError(errSecParam, error, CFSTR("unknown type %lu"), ltype);
            }
        }
    } else if (!left && !right) {
        // NOOP
    }
}
#endif

static __unused bool SOSEngineCheckPeerIntegrity(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
#if 0
    //static CFMutableDictionaryRef p2amtu;
    if (!engine->p2amtu)
        engine->p2amtu = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryRef amtu = CFDictionaryGetValue(engine->p2amtu, SOSPeerGetID(peer));
#endif

    // Inputs
    SOSManifestRef L = SOSEngineCopyLocalPeerManifest_locked(engine, peer, error);
    SOSManifestRef T = SOSPeerGetPendingObjects(peer);
    SOSManifestRef C = SOSPeerGetConfirmedManifest(peer);
    SOSManifestRef U = SOSPeerGetUnwantedManifest(peer);

    // Computed
    SOSManifestRef CunionU = SOSManifestCreateUnion(C, U, error);
    SOSManifestRef S = SOSManifestCreateIntersection(L, CunionU, error);

    SOSManifestRef AunionT = NULL, MunionU = NULL;
    SOSManifestDiff(L, C, &AunionT, &MunionU, error);

    SOSManifestRef A = SOSManifestCreateComplement(T, AunionT, error);
    SOSManifestRef M = SOSManifestCreateComplement(U, MunionU, error);

    SOSManifestRef SunionAunionT = SOSManifestCreateUnion(S, AunionT, error);
    SOSManifestRef SunionMunionU = SOSManifestCreateUnion(S, MunionU, error);

    SOSManifestRef AintersectM = SOSManifestCreateIntersection(A, M, error);
    SOSManifestRef AintersectS = SOSManifestCreateIntersection(A, S, error);
    SOSManifestRef AintersectT = SOSManifestCreateIntersection(A, T, error);
    SOSManifestRef AintersectU = SOSManifestCreateIntersection(A, U, error);
    SOSManifestRef MintersectS = SOSManifestCreateIntersection(M, S, error);
    SOSManifestRef MintersectT = SOSManifestCreateIntersection(M, T, error);
    SOSManifestRef MintersectU = SOSManifestCreateIntersection(M, U, error);
    SOSManifestRef SintersectT = SOSManifestCreateIntersection(S, T, error);
    SOSManifestRef SintersectU = SOSManifestCreateIntersection(S, U, error);
    SOSManifestRef TintersectU = SOSManifestCreateIntersection(T, U, error);

#if 0
    CFDictionaryRef newAmtu = CFDictionaryCreateForCFTypes(kCFAllocatorDefault, CFSTR("A"), A, CFSTR("M"), M, CFSTR("T"), T, CFSTR("U") U, NULL);
    CFDictionarySetValue(engine->p2amtu, SOSPeerGetID(peer), newAmtu);
    CFMutableStringRef amtuChanges = CFStringCreateMutable(kCFAllocatorDefault, 0);
    SOSAppendDiffToString(amtu, newAmtu, amtuChanges);
    secnotice("engine", "%@: %@", SOSPeerGetID(peer), amtuChanges);
#endif

#define SOSASSERT(e)      (__builtin_expect(!(e), 0) ? secnotice("engine", "state-assertion %s", #e), assert(e) : (void)0)

    SOSASSERT(L ? CFEqual(L, SunionAunionT) : SOSManifestGetCount(SunionAunionT) == 0);
    SOSASSERT(C ? CFEqual(C, SunionMunionU) : SOSManifestGetCount(SunionMunionU) == 0);

    SOSASSERT(SOSManifestGetCount(AintersectM) == 0);
    SOSASSERT(SOSManifestGetCount(AintersectS) == 0);
    SOSASSERT(SOSManifestGetCount(AintersectT) == 0);
    SOSASSERT(SOSManifestGetCount(AintersectU) == 0);
    SOSASSERT(SOSManifestGetCount(MintersectS) == 0);
    SOSASSERT(SOSManifestGetCount(MintersectT) == 0);
    SOSASSERT(SOSManifestGetCount(MintersectU) == 0);
    SOSASSERT(SOSManifestGetCount(SintersectT) == 0);
    SOSASSERT(SOSManifestGetCount(SintersectU) == 0);
    SOSASSERT(SOSManifestGetCount(TintersectU) == 0);

    CFReleaseSafe(AintersectM);
    CFReleaseSafe(AintersectS);
    CFReleaseSafe(AintersectT);
    CFReleaseSafe(AintersectU);
    CFReleaseSafe(MintersectS);
    CFReleaseSafe(MintersectT);
    CFReleaseSafe(MintersectU);
    CFReleaseSafe(SintersectT);
    CFReleaseSafe(SintersectU);
    CFReleaseSafe(TintersectU);

    CFReleaseSafe(AunionT);
    CFReleaseSafe(MunionU);


    CFReleaseSafe(A);
    CFReleaseSafe(M);
    CFReleaseSafe(S);
    //CFReleaseSafe(T); // Get
    //CFReleaseSafe(U); // Get
    //CFReleaseSafe(C); // Get
    CFReleaseSafe(L);
    return true;
}

void SOSEngineSetSyncCompleteListener(SOSEngineRef engine, SOSEnginePeerInSyncBlock notify_block) {
    SOSEngineDoOnQueue(engine, ^{
        CFAssignRetained(engine->syncCompleteListener, Block_copy(notify_block));
    });
}

void SOSEngineSetSyncCompleteListenerQueue(SOSEngineRef engine, dispatch_queue_t notify_queue) {
    SOSEngineDoOnQueue(engine, ^{
        CFRetainAssign(engine->syncCompleteQueue, notify_queue);
    });
}

static void SOSEngineCompletedSyncWithPeer(SOSEngineRef engine, SOSPeerRef peer) {
    SOSEnginePeerInSyncBlock block_to_call = engine->syncCompleteListener;

    if (block_to_call && engine->syncCompleteQueue) {
        CFStringRef ID = CFRetainSafe(SOSPeerGetID(peer));
        CFSetRef views = CFRetainSafe(SOSPeerGetViewNameSet(peer));
        CFRetainSafe(block_to_call);

        dispatch_async(engine->syncCompleteQueue, ^{
            block_to_call(ID, views);
            CFReleaseSafe(ID);
            CFReleaseSafe(views);
            CFReleaseSafe(block_to_call);
        });
    }

    SOSPeerSetHasBeenInSync(peer, true);
}


CFDataRef SOSEngineCreateMessage_locked(SOSEngineRef engine, SOSTransactionRef txn, SOSPeerRef peer,
                                        CFErrorRef *error, SOSEnginePeerMessageSentBlock *sent) {
    SOSManifestRef local = SOSEngineCopyLocalPeerManifest_locked(engine, peer, error);
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
    SOSManifestRef unwanted = SOSPeerGetUnwantedManifest(peer);
    SOSManifestRef excessUnwanted = NULL;
    CFDataRef result = NULL;

    // Given (C, L, T, U) compute (T, U, M, A)
    // (C \ L) \ U => M
    // (L \ C) \ T => A
    // we also compute
    // U \ (C \ L) => EU
    // T \ (L \ C) => ET
    // And assert that both EU and ET are empty and if not remove them from U and T respectively
    SOSManifestDiff(confirmed, local, &allMissing, &allExtra, error);
    SOSManifestDiff(allExtra, pendingObjects, &extra, &excessPending, error);
    if (SOSManifestGetCount(excessPending)) {
        // T \ (L \ C) => excessPending (items both in L and C or in neither that are still pending)
        // Can only happen if a member of T was removed from L without us having a chance to update T
        secerror("%@ ASSERTION FAILURE purging excess pendingObjects: %@", peer, excessPending);
        SOSManifestRef newPendingObjects = SOSManifestCreateComplement(excessPending, pendingObjects, error);
        SOSPeerSetPendingObjects(peer, newPendingObjects);
        CFReleaseSafe(newPendingObjects);
        pendingObjects = SOSPeerGetPendingObjects(peer);
    }
    SOSManifestDiff(allMissing, unwanted, &missing, &excessUnwanted, error);
    if (SOSManifestGetCount(excessUnwanted)) {
        // U \ (C \ L) => excessUnwanted (items both in L and C or in neither that are still unwanted)
        // Can only happen if a member of U was added to L without us having a chance to update U.
        // Since U only contains items the conflict resolver rejected, this implies L somehow got rolled back
        // The other option (and more likely) is a member of U was removed from C and not from U.
        secerror("%@ ASSERTION FAILURE purging excess unwanted: %@", peer, excessUnwanted);
        SOSManifestRef newUnwanted = SOSManifestCreateComplement(excessUnwanted, unwanted, error);
        SOSPeerSetUnwantedManifest(peer, newUnwanted);
        CFReleaseSafe(newUnwanted);
        unwanted = SOSPeerGetUnwantedManifest(peer);
    }

    CFReleaseNull(allExtra);
    CFReleaseNull(excessPending);
    CFReleaseNull(allMissing);
    CFReleaseNull(excessUnwanted);

    secnoticeq("engine", "%@:%@: send state for peer [%s%s%s][%s%s] P:%zu, E:%zu, M:%zu U:%zu", engine->myID, SOSPeerGetID(peer),
               local ? "L":"l",
               confirmed ? "C":"0",
               pendingObjects ? "P":"0",
               SOSPeerSendObjects(peer) ? "O":"o",
               SOSPeerMustSendMessage(peer) ? "S":"s",
               SOSManifestGetCount(pendingObjects),
               SOSManifestGetCount(extra),
               SOSManifestGetCount(missing),
               SOSManifestGetCount(unwanted)
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

        // If we aren't missing anything, we've gotten all their data, so we're sync even if they haven't seen ours.
        if (missing && SOSManifestGetCount(missing) == 0) {
            SOSEngineCompletedSyncWithPeer(engine, peer);
        }

        if (CFEqualSafe(local, SOSPeerGetProposedManifest(peer)) && !SOSPeerMustSendMessage(peer)) {
            bool send = false;
            if (CFEqual(confirmed, local)) {
                secnoticeq("engine", "synced <No MSG> %@:%@", engine->myID,  peer);
            } else if (SOSManifestGetCount(pendingObjects) == 0 /* TODO: No entries moved from extra to pendingObjects. */
                       && SOSManifestGetCount(missing) == 0) {
                secnoticeq("engine", "waiting <MSG not resent> %@:%@ extra: %@", engine->myID, peer, extra);
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
            // If we have additions and we need to send objects, do so.
            __block size_t objectsSize = 0;
            __block struct SOSDigestVector dv = SOSDigestVectorInit;
            CFMutableArrayRef changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            __block CFErrorRef dsfeError = NULL;

            if (!SOSDataSourceForEachObject(engine->dataSource, txn, pendingObjects, &dsfeError, ^void(CFDataRef key, SOSObjectRef object, bool *stop) {
                CFErrorRef localError = NULL;
                CFDataRef digest = NULL;
                CFDataRef der = NULL;
#if !defined(NDEBUG)
                const uint8_t *d = CFDataGetBytePtr(key);
#endif
                secdebug("engine", "%@:%@ object %02X%02X%02X%02X error from SOSDataSourceForEachObject: %@",
                           engine->myID, SOSPeerGetID(peer), d[0], d[1], d[2], d[3], dsfeError);
                if (!object) {
                    const uint8_t *d = CFDataGetBytePtr(key);
                    secerror("%@:%@ object %02X%02X%02X%02X dropping from manifest: not found in datasource: %@",
                               engine->myID, SOSPeerGetID(peer), d[0], d[1], d[2], d[3], dsfeError);
                    SOSChangesAppendDelete(changes, key);
                } else if (!(der = SOSEngineCopyObjectDER(engine, object, &localError))
                           || !(digest = SOSObjectCopyDigest(engine->dataSource, object, &localError))) {
                    if (SecErrorGetOSStatus(localError) == errSecDecode) {
                        // Decode error, we need to drop these objects from our manifests
                        const uint8_t *d = CFDataGetBytePtr(key);
                        secnoticeq("engine", "%@:%@ object %02X%02X%02X%02X dropping from manifest: %@",
                            engine->myID, SOSPeerGetID(peer), d[0], d[1], d[2], d[3], localError);
                        SOSChangesAppendDelete(changes, key);
                        CFRelease(localError);
                    } else {
                        // Stop iterating and propagate out all other errors.
                        const uint8_t *d = CFDataGetBytePtr(key);
                        secnoticeq("engine", "%@:%@ object %02X%02X%02X%02X in SOSDataSourceForEachObject: %@",
                            engine->myID, SOSPeerGetID(peer), d[0], d[1], d[2], d[3], localError);
                        *stop = true;
                        CFErrorPropagate(localError, error);
                        CFReleaseNull(message);
                    }
                } else {
                    if (!CFEqual(key, digest)) {
                        const uint8_t *d = CFDataGetBytePtr(key);
                        const uint8_t *e = CFDataGetBytePtr(digest);
                        secnoticeq("engine", "%@:%@ object %02X%02X%02X%02X is really %02X%02X%02X%02X dropping from local manifest",
                                   engine->myID, SOSPeerGetID(peer), d[0], d[1], d[2], d[3], e[0], e[1], e[2], e[3]);
                        SOSChangesAppendDelete(changes, key);
                        SOSChangesAppendAdd(changes, object); // This is new behaviour but we think it's more correct
                    }

                    size_t objectLen = (size_t)CFDataGetLength(der);
                    if (SOSMessageAppendObject(message, der, &localError)) {
                        SOSDigestVectorAppend(&dv, CFDataGetBytePtr(digest));
                    } else {
                        const uint8_t *d = CFDataGetBytePtr(digest);
                        CFStringRef hexder = CFDataCopyHexString(der);
                        secnoticeq("engine", "%@:%@ object %02X%02X%02X%02X der: %@ dropping from manifest: %@",
                                  engine->myID, SOSPeerGetID(peer), d[0], d[1], d[2], d[3], hexder, localError);
                        CFReleaseNull(hexder);
                        CFReleaseNull(message);
                        // Since we can't send these objects let's assume they are bad too?
                        SOSChangesAppendDelete(changes, digest);
                    }
                    objectsSize += objectLen;
                    if (objectsSize > kSOSMessageMaxObjectsSize)
                        *stop = true;
                }
                CFErrorPropagate(dsfeError, error); // this also releases dsfeError
                dsfeError = NULL;
                CFReleaseSafe(der);
                CFReleaseSafe(digest);
            })) {
                CFReleaseNull(message);
            }
            if (dv.count){
                objectsSent = SOSManifestCreateWithDigestVector(&dv, error);
            }
            if (CFArrayGetCount(changes)) {
                CFErrorRef localError = NULL;
                if (!SOSEngineUpdateChanges_locked(engine, NULL, kSOSDataSourceTransactionDidCommit, kSOSDataSourceSOSTransaction, changes, &localError))
                    secerror("SOSEngineUpdateChanges_locked: %@ failed: %@", changes, localError);
                CFReleaseSafe(localError);
                CFAssignRetained(local, SOSEngineCopyLocalPeerManifest_locked(engine, peer, error));
            }
            CFReleaseSafe(changes);
            SOSDigestVectorFree(&dv);
            CFReleaseNull(dsfeError);
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

    SOSManifestRef sender = local;
    // We actually send the remote peer its own digest.
    // Note that both pendingObjects and unwanted may have been changed, so we get them again
    if (SOSManifestGetCount(SOSPeerGetPendingObjects(peer))==0 && SOSManifestGetCount(extra)==0 &&
        SOSManifestGetCount(missing)==0 && SOSManifestGetCount(SOSPeerGetUnwantedManifest(peer))!=0) {
        secnoticeq("engine", "%@:%@: only have differences in unwanted set; lying to peer to stop sync",engine->myID, SOSPeerGetID(peer));
        sender = confirmed;
    }

    if (!SOSMessageSetManifests(message, sender, confirmed, proposed, proposed, confirmed ? objectsSent : NULL, error)) {
        secnoticeq("engine", "%@:%@: failed to set message manifests",engine->myID, SOSPeerGetID(peer));
        CFReleaseNull(message);
    }

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
                SOSPeerSetMustSendMessage(peer, false);
                if (!confirmed && !proposed) {
                    SOSPeerSetSendObjects(peer, true);
                    secnoticeq("engine", "%@:%@ sendObjects=true L:%@", engine->myID, SOSPeerGetID(peer), local);
                }
                SOSPeerAddLocalManifest(peer, local);
                SOSPeerAddProposedManifest(peer, proposed);
                secnoticeq("engine", "send %@:%@ %@", engine->myID, SOSPeerGetID(peer), message);
                //SOSEngineCheckPeerIntegrity(engine, peer, NULL);
            } else {
                secerror("%@:%@ failed to send %@", engine->myID, SOSPeerGetID(peer), message);
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
        secerror("%@:%@ error in send: %@", engine->myID, SOSPeerGetID(peer), *error);

    return result;
}

static void SOSEngineLogItemError(SOSEngineRef engine, CFStringRef peerID, CFDataRef key, CFDataRef optionalDigest, const char *where, CFErrorRef error) {
    if (!optionalDigest) {
        const uint8_t *d = CFDataGetBytePtr(key);
        secwarning("%@:%@ object %02X%02X%02X%02X %s: %@", engine->myID, peerID, d[0], d[1], d[2], d[3], where, error ? (CFTypeRef)error : CFSTR(""));
    } else {
        const uint8_t *d = CFDataGetBytePtr(key);
        const uint8_t *e = CFDataGetBytePtr(optionalDigest);
        secwarning("%@:%@ object %02X%02X%02X%02X is really %02X%02X%02X%02X dropping from local manifest", engine->myID, peerID, d[0], d[1], d[2], d[3], e[0], e[1], e[2], e[3]);
    }
}

static bool SOSEngineWriteToBackup_locked(SOSEngineRef engine, SOSPeerRef peer, bool rewriteComplete, bool *didWrite, bool *incomplete, CFErrorRef *error) {
    __block bool ok = SOSPeerWritePendingReset(peer, error);
    if (!ok || !SOSPeerGetKeyBag(peer))
        return ok;
    __block SOSManifestRef local = SOSEngineCopyLocalPeerManifest_locked(engine, peer, error);
    __block SOSManifestRef proposed = SOSPeerGetProposedManifest(peer);
    __block bool notify = true;
    SOSManifestRef pendingObjects = NULL;
    SOSManifestRef missing = NULL;
    CFStringRef peerID = SOSPeerGetID(peer);

    ok &= SOSManifestDiff(proposed, local, &missing, &pendingObjects, error);

    secnoticeq("engine", "%@:%@: Send state for peer [%s%s%s] O: %zu, M: %zu", engine->myID, peerID,
               local ? "L":"l",
               proposed ? "P":"0",
               pendingObjects ? "O":"0",
               SOSManifestGetCount(pendingObjects),
               SOSManifestGetCount(missing));

    if (SOSManifestGetCount(missing) == 0 && SOSManifestGetCount(pendingObjects) == 0) {
        // proposed == local (faster test than CFEqualSafe above), since we
        // already did the SOSManifestDiff
        if (rewriteComplete) {
            notify = false;
        } else {
            secnoticeq("engine", "%@:%@ backup still done", engine->myID, peer);
            goto done;
        }
    }
    ok &= SOSPeerAppendToJournal(peer, error, ^(FILE *journalFile, keybag_handle_t kbhandle) {
        SOSManifestRef objectsSent = NULL;
        __block struct SOSDigestVector dvdel = SOSDigestVectorInit;
        __block struct SOSDigestVector dvadd = SOSDigestVectorInit;
        SOSManifestForEach(missing, ^(CFDataRef key, bool *stop) {
            CFErrorRef localError = NULL;
            if (ftello(journalFile) > kSOSBackupMaxFileSize) {
                // Highwatermark hit on file.
                *stop = true;
            } else if (SOSBackupEventWriteDelete(journalFile, key, &localError)) {
                SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(key));
            } else {
                SOSEngineLogItemError(engine, peerID, key, NULL, "in SOSPeerWriteDelete", localError);
                CFErrorPropagate(localError, error);
                // TODO: Update of missing so proposed is updated properly
                *stop = true; // Disk full?
                ok = false;
            }
        });
        if (ok && SOSManifestGetCount(pendingObjects)) {
            CFMutableArrayRef changes = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
            ok &= SOSDataSourceForEachObject(engine->dataSource, NULL, pendingObjects, error, ^void(CFDataRef key, SOSObjectRef object, bool *stop) {
                CFErrorRef localError = NULL;
                CFDataRef digest = NULL;
                CFDictionaryRef backupItem = NULL;
                if (ftello(journalFile) > kSOSBackupMaxFileSize) {
                    // Highwatermark hit on file.
                    *stop = true;
                } else if (!object) {
                    SOSEngineLogItemError(engine, peerID, key, NULL, "dropping from manifest: not found in datasource", localError);
                    SOSChangesAppendDelete(changes, key);
                } else if (!(backupItem = SOSObjectCopyBackup(engine->dataSource, object, kbhandle, &localError))
                           || !(digest = SOSObjectCopyDigest(engine->dataSource, object, &localError))) {
                    if (SecErrorGetOSStatus(localError) == errSecDecode) {
                        // Decode error, we need to drop these objects from our manifests
                        SOSEngineLogItemError(engine, peerID, key, NULL, "dropping from manifest", localError);
                        SOSChangesAppendDelete(changes, key);
                        CFRelease(localError);
                    } else {
                        // Stop iterating and propagate out all other errors.
                        SOSEngineLogItemError(engine, peerID, key, NULL, "in SOSDataSourceForEachObject", localError);
                        *stop = true;
                        CFErrorPropagate(localError, error);
                        ok = false;
                    }
                } else {
                    if (!CFEqual(key, digest)) {
                        SOSEngineLogItemError(engine, peerID, key, digest, "", NULL);
                        SOSChangesAppendDelete(changes, key);
                        SOSChangesAppendAdd(changes, object); // This is new behaviour but we think it's more correct
                    }

                    if (SOSBackupEventWriteAdd(journalFile, backupItem, &localError)) {
                        SOSDigestVectorAppend(&dvadd, CFDataGetBytePtr(digest));
                    } else {
                        SOSEngineLogItemError(engine, peerID, key, NULL, "in SOSPeerWriteAdd", localError);
                        *stop = true; // Disk full?
                        CFErrorPropagate(localError, error);
                        ok = false;
                    }
                }
                CFReleaseSafe(backupItem);
                CFReleaseSafe(digest);
            });
            if (CFArrayGetCount(changes)) {
                CFErrorRef localError = NULL;
                if (!SOSEngineUpdateChanges_locked(engine, NULL, kSOSDataSourceTransactionDidCommit, kSOSDataSourceSOSTransaction, changes, &localError))
                    secerror("SOSEngineUpdateChanges_locked: %@ failed: %@", changes, localError);
                CFReleaseSafe(localError);
                // Since calling SOSEngineUpdateChanges_locked might cause local to change and might cause the backup peer to update proposed, refetch them here.
                CFAssignRetained(local, SOSEngineCopyLocalPeerManifest_locked(engine, peer, error));
                proposed = SOSPeerGetProposedManifest(peer);
            }
            CFReleaseSafe(changes);
        }

        if (dvadd.count || (proposed && dvdel.count)) {
            *didWrite = true;
            SOSManifestRef deleted = SOSManifestCreateWithDigestVector(&dvdel, error);
            SOSManifestRef objectsSent = SOSManifestCreateWithDigestVector(&dvadd, error);
            SOSManifestRef newProposed = SOSManifestCreateWithPatch(proposed, deleted, objectsSent, error);
            CFReleaseSafe(deleted);
            CFReleaseSafe(objectsSent);
            SOSPeerSetProposedManifest(peer, newProposed);
            CFReleaseSafe(newProposed);
            proposed = SOSPeerGetProposedManifest(peer);
        }
        SOSDigestVectorFree(&dvdel);
        SOSDigestVectorFree(&dvadd);

        // TODO: If proposed is NULL, and local is empty we should still consider ourselves done.
        // It so happens this can't happen in practice today since there is at least a backupbag
        // in the backup, but this is a bug waiting to rear its head in the future.
        if (ok && CFEqualSafe(local, proposed)) {
            CFErrorRef localError = NULL;
            if (SOSBackupEventWriteCompleteMarker(journalFile, 899, &localError)) {
                SOSPeerSetSendObjects(peer, true);
                *didWrite = true;
                secnoticeq("backup", "%@:%@ backup done%s", engine->myID, peerID, notify ? " notifying sbd" : "");
                // TODO: Now switch to changes based writing to backup sync.
                // Currently we leave changes enabled but we probably shouldn't
            } else {
                secwarning("%@:%@ in SOSBackupPeerWriteCompleteMarker: %@", engine->myID, peerID, localError);
                ok = false;
                *incomplete = true;
                CFErrorPropagate(localError, error);
            }
        } else {
            secnoticeq("backup", "%@:%@ backup incomplete [%zu/%zu]%s", engine->myID, peerID, SOSManifestGetCount(local), SOSManifestGetCount(proposed), notify ? " notifying sbd" : "");
            *incomplete = true;
        }
        CFReleaseNull(objectsSent);
    });
    if (notify)
        SOSBackupPeerPostNotification("writing changes to backup");

done:
    CFReleaseSafe(local);
    CFReleaseNull(pendingObjects);
    CFReleaseNull(missing);

    return ok;
}

CF_RETURNS_RETAINED CFSetRef SOSEngineSyncWithBackupPeers(SOSEngineRef engine, CFSetRef /* CFStringRef */ peers, CFErrorRef *error)
{
    __block bool incomplete = false;
    CFMutableSetRef handledSet = CFSetCreateMutableForCFTypes(kCFAllocatorDefault);

    bool ok = SOSEngineDoTxnOnQueue(engine, error, ^(SOSTransactionRef txn, bool *commit) {
        __block bool dirty = false;
        CFSetForEach(peers, ^(const void *value) {
            bool report_handled = true;
            CFErrorRef localError = NULL;
            SOSPeerRef peer = NULL;
            CFStringRef peerID = asString(value, &localError);
            require_action_quiet(peerID, done, report_handled = false);

            peer = SOSEngineCopyPeerWithID_locked(engine, peerID, &localError);
            require_quiet(peerID, done);

            if (SOSPeerMapEntryIsBackup(peer)) {
                report_handled = SOSEngineWriteToBackup_locked(engine, peer, false, &dirty, &incomplete, &localError);
            }

        done:
            if (localError) {
                secnotice("engine-sync", "Failed to process sync for %@: %@", peerID, localError);
            }
            if (report_handled) {
                CFSetAddValue(handledSet, peerID);
            }
            CFReleaseNull(localError);
            CFReleaseNull(peer);
        });

        if (dirty) {
            CFErrorRef saveError = NULL;
            if (!SOSEngineSave(engine, txn, &saveError)) {
                secnotice("engine-save", "Failed to save engine: %@", saveError);
            }
        }
    });
    if (incomplete) {
        // Ensure we get called again in a while (after a backup timeout)
        // sbd will do this since we never wrote a complete marker.
        // TODO: This relies on us not writing complete marker for update
        // event while we havn't finished a full backup, which we currently still do.
    }
    if (!ok)
        CFReleaseNull(handledSet);

    return handledSet;
}

bool SOSEngineHandleMessage(SOSEngineRef engine, CFStringRef peerID,
                            CFDataRef raw_message, CFErrorRef *error)
{
    __block bool result = true;
    __block bool somethingChanged = false;
    SOSMessageRef message = SOSMessageCreateWithData(kCFAllocatorDefault, raw_message, error);
    result &= message && SOSEngineDoTxnOnQueue(engine, error, ^(SOSTransactionRef txn, bool *commit) {
        result = SOSEngineHandleMessage_locked(engine, peerID, message, txn, commit, &somethingChanged, error);
    });
    CFReleaseSafe(message);
    if (somethingChanged)
        SecKeychainChanged();
    return result;
}

void SOSEngineCircleChanged(SOSEngineRef engine, CFStringRef myPeerID, CFArrayRef trustedPeers, CFArrayRef untrustedPeers) {
    __block bool peersOrViewsChanged = false;
    SOSEngineDoOnQueue(engine, ^{
        peersOrViewsChanged = SOSEngineCircleChanged_locked(engine, myPeerID, trustedPeers, untrustedPeers);

        // We should probably get a more precise list of peers that actually need talking to
        if (peersOrViewsChanged && engine->myID && CFArrayGetCount(engine->peerIDs) != 0)
            SOSCCRequestSyncWithPeersList(engine->peerIDs);
    });

    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    ok &= SOSEngineDoTxnOnQueue(engine, &localError, ^(SOSTransactionRef txn, bool *commit) {
        ok = *commit = SOSEngineSave(engine, txn, &localError);
    });
    if (!ok) {
        secerror("failed to save engine state: %@", localError);
        CFReleaseSafe(localError);
    }

}

SOSManifestRef SOSEngineCopyManifest(SOSEngineRef engine, CFErrorRef *error) {
    __block SOSManifestRef result = NULL;
    SOSEngineDoOnQueue(engine, ^{
        result = SOSEngineCopyManifestWithViewNameSet_locked(engine, SOSViewsGetV0ViewSet(), error);
    });
    return result;
}

SOSManifestRef SOSEngineCopyLocalPeerManifest(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
    __block SOSManifestRef result = NULL;
    SOSEngineDoOnQueue(engine, ^{
        result = SOSEngineCopyLocalPeerManifest_locked(engine, peer, error);
    });
    return result;
}

bool SOSEngineUpdateChanges(SOSEngineRef engine, SOSDataSourceTransactionSource source, CFArrayRef changes, CFErrorRef *error) {
    __block bool result = true;
    SOSEngineDoOnQueue(engine, ^{
        result = SOSEngineUpdateChanges_locked(engine, NULL, kSOSDataSourceTransactionDidCommit, source, changes, error);
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

SOSPeerRef SOSEngineCopyPeerWithID(SOSEngineRef engine, CFStringRef peer_id, CFErrorRef *error) {
    __block SOSPeerRef peer = NULL;
    SOSEngineDoOnQueue(engine, ^{
        peer = SOSEngineCopyPeerWithID_locked(engine, peer_id, error);
    });
    return peer;
}

bool SOSEngineForPeerID(SOSEngineRef engine, CFStringRef peerID, CFErrorRef *error, void (^forPeer)(SOSTransactionRef txn, SOSPeerRef peer)) {
    __block bool ok = true;
    SOSDataSourceReadWithCommitQueue(engine->dataSource, error, ^(SOSTransactionRef txn) {
        SOSEngineDoOnQueue(engine, ^{
            SOSPeerRef peer = SOSEngineCopyPeerWithID_locked(engine, peerID, error);
            if (peer) {
                forPeer(txn, peer);
                CFRelease(peer);
            } else {
                ok = false;
            }
        });
    });

    return ok;
}

bool SOSEngineWithPeerID(SOSEngineRef engine, CFStringRef peerID, CFErrorRef *error, void (^with)(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *forceSaveState)) {
    __block bool result = true;
    result &= SOSEngineDoTxnOnQueue(engine, error, ^(SOSTransactionRef txn, bool *commit) {
        SOSPeerRef peer = SOSEngineCopyPeerWithID_locked(engine, peerID, error);
        if (!peer) {
            result = SOSErrorCreate(kSOSErrorPeerNotFound, error, NULL, CFSTR("Engine has no peer for %@"), peerID);
        } else {
            bool saveState = false;
            SOSCoderRef coder = SOSEngineGetCoderInTx_locked(engine, txn, peerID, NULL);
            with(peer, coder, engine->dataSource, txn, &saveState);
            CFReleaseSafe(peer);
            if (saveState)
                result = SOSEngineSave(engine, txn, error);
            // TODO: Don't commit if engineSave fails?
        }
    });

    return result;
}

CFDataRef SOSEngineCreateMessageToSyncToPeer(SOSEngineRef engine, CFStringRef peerID, SOSEnginePeerMessageSentBlock *sentBlock, CFErrorRef *error) {
    __block CFDataRef message = NULL;
    SOSEngineForPeerID(engine, peerID, error, ^(SOSTransactionRef txn, SOSPeerRef peer) {
        message = SOSEngineCreateMessage_locked(engine, txn, peer, error, sentBlock);
    });
    return message;
}

bool SOSEnginePeerDidConnect(SOSEngineRef engine, CFStringRef peerID, CFErrorRef *error) {
    return SOSEngineWithPeerID(engine, peerID, error, ^(SOSPeerRef peer, SOSCoderRef coder, SOSDataSourceRef dataSource, SOSTransactionRef txn, bool *saveState) {
        *saveState = SOSPeerDidConnect(peer);
    });
}

bool SOSEngineSetPeerConfirmedManifest(SOSEngineRef engine, CFStringRef backupName,
                                       CFDataRef keybagDigest, CFDataRef manifestData, CFErrorRef *error) {
    __block bool ok = true;

    ok &= SOSEngineForPeerID(engine, backupName, error, ^(SOSTransactionRef txn, SOSPeerRef peer) {
        bool dirty = false;
        bool incomplete = false;
        SOSManifestRef confirmed = NULL;
        CFDataRef keybag = SOSPeerGetKeyBag(peer);
        CFDataRef computedKeybagDigest = keybag ? CFDataCopySHA1Digest(keybag, NULL) : NULL;
        if (CFEqualSafe(keybagDigest, computedKeybagDigest)) {
            ok = confirmed = SOSManifestCreateWithData(manifestData, error);
            if (ok) {
                // Set both confirmed and proposed (confirmed is just
                // for debug status, proposed is actually what's used
                // by the backup peer).
                SOSPeerSetConfirmedManifest(peer, confirmed);
                SOSPeerSetProposedManifest(peer, confirmed);
            }
        } else {
            // sbd missed a reset event, send it again
            // Force SOSEngineWriteToBackup_locked to call SOSPeerWriteReset, which clears
            // confirmed and proposed manifests and writes the keybag to the journal.
            SOSPeerSetMustSendMessage(peer, true);
        }

        // Stop changes from writing complete markers, unless SOSEngineWriteToBackup_locked() detects we are in sync
        SOSPeerSetSendObjects(peer, false);
        // Write data for this peer if we can, technically not needed for non legacy protocol support all the time.
        ok = SOSEngineWriteToBackup_locked(engine, peer, true, &dirty, &incomplete, error);

        CFReleaseSafe(confirmed);
        CFReleaseSafe(computedKeybagDigest);
    });
    return ok;
}

CFArrayRef SOSEngineCopyBackupPeerNames(SOSEngineRef engine, CFErrorRef *error) {
    __block CFMutableArrayRef backupNames = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSEngineForEachBackupPeer(engine, ^(SOSPeerRef peer) {
        CFArrayAppendValue(backupNames, SOSPeerGetID(peer));
    });
    return backupNames;
}

static CFMutableDictionaryRef SOSEngineCreateStateDictionary(CFStringRef peerID, SOSManifestRef manifest, CFSetRef vns, CFStringRef coderString) {
    CFNumberRef manifestCount = CFNumberCreateWithCFIndex(kCFAllocatorDefault, SOSManifestGetCount(manifest));
    CFDataRef manifestHash = SOSManifestGetDigest(manifest, NULL);
    CFMutableDictionaryRef result = CFDictionaryCreateMutableForCFTypesWithSafe(kCFAllocatorDefault,
                                                                                kSOSCCEngineStatePeerIDKey, peerID,
                                                                                kSOSCCEngineStateManifestCountKey, manifestCount,
                                                                                kSOSCCEngineStateManifestHashKey, manifestHash,
                                                                                kSOSCCEngineStateSyncSetKey, asSet(vns, NULL),
                                                                                kSOSCCEngineStateCoderKey, coderString,
                                                                                NULL);
    CFReleaseNull(manifestCount);
    return result;
}

static void SOSEngineAppendStateDictionary(CFMutableArrayRef stateArray, CFStringRef peerID, SOSManifestRef manifest, CFSetRef vns, CFStringRef coderString) {
    CFMutableDictionaryRef newState = SOSEngineCreateStateDictionary(peerID, manifest, vns, coderString);
    CFArrayAppendValue(stateArray, newState);
    CFReleaseNull(newState);
}

static CFArrayRef SOSEngineCopyPeerConfirmedDigests_locked(SOSEngineRef engine, CFErrorRef *error) {
    CFMutableArrayRef result = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    CFDictionaryForEach(engine->viewNameSet2ChangeTracker, ^(const void *vns, const void *ct) {
        SOSManifestRef manifest = SOSEngineCopyManifestWithViewNameSet_locked(engine, vns, error);
        SOSEngineAppendStateDictionary(result, NULL, manifest, vns, NULL);
        CFReleaseNull(manifest);
    });

    // Copy other peers even if we aren't in the circle, since we're observing it.
    SOSEngineForEachPeer_locked(engine, ^(SOSPeerRef peer) {
        CFTypeRef coderObject = engine->coders ? CFDictionaryGetValue(engine->coders, SOSPeerGetID(peer)) : CFSTR("Coders not loaded.");
        CFStringRef coderState = coderObject ? CFCopyDescription(coderObject) : NULL;
        SOSEngineAppendStateDictionary(result, SOSPeerGetID(peer), SOSPeerGetConfirmedManifest(peer), SOSPeerGetViewNameSet(peer), coderState);
        CFReleaseNull(coderState);
    });
    return result;
}

CFArrayRef SOSEngineCopyPeerConfirmedDigests(SOSEngineRef engine, CFErrorRef *error) {
    __block CFArrayRef result = NULL;
    SOSEngineDoOnQueue(engine, ^{
        result = SOSEngineCopyPeerConfirmedDigests_locked(engine, error);
    });
    return result;
}

SOSDataSourceRef SOSEngineGetDataSource(SOSEngineRef engine) {
    return engine->dataSource;
}

#define ENGINELOGSTATE "engineLogState"
void SOSEngineLogState(SOSEngineRef engine) {
    CFErrorRef error = NULL;
    CFArrayRef confirmedDigests = NULL;

    secnotice(ENGINELOGSTATE, "Start");

    require_action_quiet(engine, retOut, secnotice(ENGINELOGSTATE, "No Engine Available"));
    confirmedDigests = SOSEngineCopyPeerConfirmedDigests(engine, &error);
    require_action_quiet(confirmedDigests, retOut, secnotice(ENGINELOGSTATE, "No engine peers: %@\n", error));

    SOSCCForEachEngineStateAsStringFromArray(confirmedDigests, ^(CFStringRef onePeerDescription) {
        secnotice(ENGINELOGSTATE, "%@", onePeerDescription);
    });

retOut:
    CFReleaseNull(error);
    CFReleaseNull(confirmedDigests);
    secnotice(ENGINELOGSTATE, "Finish");

    return;
}
