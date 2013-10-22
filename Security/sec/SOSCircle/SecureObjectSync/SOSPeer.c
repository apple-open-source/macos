/*
 * Created by Michael Brouwer on 6/22/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*
 * SOSPeer.c -  Implementation of a secure object syncing peer
 */
#include <SecureObjectSync/SOSPeer.h>
#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSFullPeerInfo.h>
#include <SecureObjectSync/SOSPeerInfo.h>
#include <SecureObjectSync/SOSCoder.h>
#include <SecureObjectSync/SOSInternal.h>
#include <utilities/SecCFRelease.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/debugging.h>
#include <utilities/SecFileLocations.h>
#include <utilities/der_plist.h>
#include <utilities/der_plist_internal.h>

#include <utilities/SecDb.h>

#include <securityd/SOSCloudCircleServer.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>

#include <AssertMacros.h>

//
//
//
static CFStringRef sErrorDomain = CFSTR("com.apple.security.sos.peer.error");

static CFMutableDictionaryRef sPersistenceCache = NULL;
static CFStringRef peerFile = CFSTR("PeerManifestCache.plist");

static CFMutableDictionaryRef SOSPeerGetPersistenceCache(CFStringRef my_id)
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        CFErrorRef localError = NULL;
        CFMutableDictionaryRef peerDict = NULL;
        CFDataRef dictAsData = SOSItemGet(kSOSPeerDataLabel, &localError);

        if (dictAsData) {
            der_decode_dictionary(kCFAllocatorDefault, kCFPropertyListMutableContainers, (CFDictionaryRef*)&peerDict, &localError,
                                  CFDataGetBytePtr(dictAsData),
                                  CFDataGetBytePtr(dictAsData) + CFDataGetLength(dictAsData));
        }
        
        if (!isDictionary(peerDict)) {
            CFReleaseNull(peerDict);
            secnotice("peer", "Error finding persisted peer data %@, using empty", localError);
            peerDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
            CFReleaseNull(localError);
        }
        
        if (CFDictionaryGetValue(peerDict, my_id) != NULL) {
            CFMutableDictionaryRef mySubDictionary = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);

            CFDictionaryForEach(peerDict, ^(const void *key, const void *value) {
                if (!isDictionary(value)) {
                    CFDictionaryAddValue(mySubDictionary, key, value);
                };
            });
            
            CFDictionaryForEach(mySubDictionary, ^(const void *key, const void *value) {
                CFDictionaryRemoveValue(peerDict, key);
            });
            
            CFDictionaryAddValue(peerDict, my_id, mySubDictionary);
        }
        sPersistenceCache = peerDict;
    });

    return sPersistenceCache;
}

static void SOSPeerFlushPersistenceCache()
{
    if (!sPersistenceCache)
        return;

    CFErrorRef localError = NULL;
    CFIndex size = der_sizeof_dictionary(sPersistenceCache, &localError);
    CFMutableDataRef dataToStore = CFDataCreateMutableWithScratch(kCFAllocatorDefault, size);

    if (size == 0) {
        secerror("Error calculating size of persistence cache: %@", localError);
        goto fail;
    }

    uint8_t *der = NULL;
    if (CFDataGetBytePtr(dataToStore) != (der = der_encode_dictionary(sPersistenceCache, &localError,
					                                                  CFDataGetBytePtr(dataToStore),
                                                                      CFDataGetMutableBytePtr(dataToStore) + CFDataGetLength(dataToStore)))) {
        secerror("Error flattening peer cache: %@", localError);
        secerror("ERROR flattening peer cache (%@): size=%zd %@ (%p %p)", sPersistenceCache, size, dataToStore, CFDataGetBytePtr(dataToStore), der);
        goto fail;
}

    if (!SOSItemUpdateOrAdd(kSOSPeerDataLabel, kSecAttrAccessibleWhenUnlockedThisDeviceOnly, dataToStore, &localError)) {
        secerror("Peer cache item save failed: %@", localError);
        goto fail;
    }

fail:
    CFReleaseNull(localError);
    CFReleaseNull(dataToStore);
}

void SOSPeerPurge(SOSPeerRef peer) {
    // TODO: Do we use this or some other end-around for PurgeAll?
}

void SOSPeerPurgeAllFor(CFStringRef my_id)
{
    if (!my_id)
        return;
    
    CFMutableDictionaryRef persistenceCache = SOSPeerGetPersistenceCache(my_id);
    
    CFMutableDictionaryRef myPeerIDs = (CFMutableDictionaryRef) CFDictionaryGetValue(persistenceCache, my_id);
    if (myPeerIDs)
    {
        CFRetainSafe(myPeerIDs);

        CFDictionaryRemoveValue(myPeerIDs, my_id);

        if (isDictionary(myPeerIDs)) {
            CFDictionaryForEach(myPeerIDs, ^(const void *key, const void *value) {
                // TODO: Inflate each and purge its keys.
            });
        }

        CFReleaseNull(myPeerIDs);
    }
}

static bool SOSPeerFindDataFor(CFTypeRef *peerData, CFStringRef my_id, CFStringRef peer_id, CFErrorRef *error)
{
    CFDictionaryRef table = (CFDictionaryRef) CFDictionaryGetValue(SOSPeerGetPersistenceCache(my_id), my_id);

    *peerData = isDictionary(table) ? CFDictionaryGetValue(table, peer_id) : NULL;

    return true;
}

static bool SOSPeerCopyPersistedManifest(SOSManifestRef* manifest, CFStringRef my_id, CFStringRef peer_id, CFErrorRef *error)
{
    CFTypeRef persistedObject = NULL;
    
    require(SOSPeerFindDataFor(&persistedObject, my_id, peer_id, error), fail);
    
    CFDataRef persistedData = NULL;
    
    if (isData(persistedObject))
        persistedData = (CFDataRef)persistedObject;
    else if (isArray(persistedObject) && (CFArrayGetCount((CFArrayRef) persistedObject) == 2))
        persistedData = CFArrayGetValueAtIndex((CFArrayRef) persistedObject, 1);
    
    if (isData(persistedData)) {
        SOSManifestRef createdManifest = SOSManifestCreateWithData(persistedData, error);

        require(createdManifest, fail);

        *manifest = createdManifest;
}

    return true;

fail:
    return false;
}


static bool SOSPeerCopyCoderData(CFDataRef *data, CFStringRef my_id, CFStringRef peer_id, CFErrorRef *error)
{    
    CFTypeRef persistedObject = NULL;
    
    require(SOSPeerFindDataFor(&persistedObject, my_id, peer_id, error), fail);
    
    CFDataRef persistedData = NULL;
    
    if (isArray(persistedObject))
        persistedData = CFArrayGetValueAtIndex((CFArrayRef) persistedObject, 0);
    
    if (isData(persistedData)) {
        CFRetainSafe(persistedData);
        *data = persistedData;
    }

    return true;
    
fail:
    return false;
}


static void SOSPeerPersistData(CFStringRef my_id, CFStringRef peer_id, SOSManifestRef manifest, CFDataRef coderData)
{
    CFMutableArrayRef data_array = CFArrayCreateMutableForCFTypes(0);
	if (coderData) {
    CFArrayAppendValue(data_array, coderData);
    } else {
        CFDataRef nullData = CFDataCreate(kCFAllocatorDefault, NULL, 0);
        CFArrayAppendValue(data_array, nullData);
        CFReleaseNull(nullData);
	}

    if (manifest) {
        CFArrayAppendValue(data_array, SOSManifestGetData(manifest));
    }

    CFMutableDictionaryRef mySubDict = (CFMutableDictionaryRef) CFDictionaryGetValue(SOSPeerGetPersistenceCache(my_id), my_id);

    if (mySubDict == NULL) {
        mySubDict = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
        CFDictionaryAddValue(SOSPeerGetPersistenceCache(my_id), my_id, mySubDict);
    }

    CFDictionarySetValue(mySubDict, peer_id, data_array);
    
    CFReleaseNull(data_array);

    SOSPeerFlushPersistenceCache();
}

struct __OpaqueSOSPeer {
    SOSPeerSendBlock send_block;
    CFStringRef my_id;
    CFStringRef peer_id;
    CFIndex version;
    SOSManifestRef manifest;
    CFDataRef manifest_digest;
    SOSCoderRef coder; // Currently will be used for OTR stuff.
};

static SOSPeerRef SOSPeerCreate_Internal(CFStringRef myPeerID, CFStringRef theirPeerID, CFIndex version, CFErrorRef *error,
                                         SOSPeerSendBlock sendBlock) {
    SOSPeerRef p = calloc(1, sizeof(struct __OpaqueSOSPeer));
    p->send_block = sendBlock;
    p->peer_id = theirPeerID;
    CFRetainSafe(p->peer_id);

    p->version = version;
    
    p->my_id = myPeerID;
    CFRetainSafe(myPeerID);
    
    require(SOSPeerCopyPersistedManifest(&p->manifest, p->my_id, p->peer_id, error), fail);

    return p;

fail:
    CFReleaseSafe(p->peer_id);
    CFReleaseSafe(p->my_id);
    free(p);
    return NULL;
}


SOSPeerRef SOSPeerCreate(SOSFullPeerInfoRef myPeerInfo, SOSPeerInfoRef peerInfo,
                         CFErrorRef *error, SOSPeerSendBlock sendBlock) {
    
    if (myPeerInfo == NULL) {
        SOSCreateError(kSOSErrorUnsupported, CFSTR("Can't create peer without my peer info!"), NULL, error);
        return NULL;
    }
    if (peerInfo == NULL) {
        SOSCreateError(kSOSErrorUnsupported, CFSTR("Can't create peer without their peer info!"), NULL, error);
        return NULL;
    }

    SOSPeerRef result = NULL;
    SOSPeerRef p = SOSPeerCreate_Internal(SOSPeerInfoGetPeerID(SOSFullPeerInfoGetPeerInfo(myPeerInfo)),
                                          SOSPeerInfoGetPeerID(peerInfo),
                                          SOSPeerInfoGetVersion(peerInfo),
                                          error, sendBlock);
    
    require(p, fail);

    CFDataRef coderData = NULL;
    CFErrorRef coderError = NULL;

    if (SOSPeerCopyCoderData(&coderData, p->my_id, p->peer_id, &coderError)
        && coderData && CFDataGetLength(coderData) != 0) {
        p->coder = SOSCoderCreateFromData(coderData, &coderError);
    }

    if (p->coder) {
        secnotice("peer", "Old coder for me: %@ to peer: %@", p->my_id, p->peer_id);
    } else {
        secnotice("peer", "New coder for me: %@ to peer: %@ [Got error: %@]", p->my_id, p->peer_id, coderError);

        p->coder = SOSCoderCreate(peerInfo, myPeerInfo, error);
        
        if (!p->coder) {
            SOSPeerDispose(p);
            p = NULL;
        }
    }

    CFReleaseNull(coderData);
    CFReleaseNull(coderError);

    result = p;
    p = NULL;
    
fail:
    CFReleaseNull(p);
    return result;
}

SOSPeerRef SOSPeerCreateSimple(CFStringRef peer_id, CFIndex version, CFErrorRef *error,
                               SOSPeerSendBlock sendBlock) {
    return SOSPeerCreate_Internal(CFSTR("FakeTestID"), peer_id, version, error, sendBlock);
}

void SOSPeerDispose(SOSPeerRef peer) {
        CFErrorRef error = NULL;
    CFDataRef coderData = NULL;
    if (peer->coder) {
        coderData = SOSCoderCopyDER(peer->coder, &error);
        if (coderData == NULL) {
			secerror("Coder data failed to export (%@), zapping data for me: %@ to peer: %@", error, peer->my_id, peer->peer_id);
		}
		CFReleaseNull(error);
    }
        
        if (!coderData) {
            coderData = CFDataCreate(NULL, NULL, 0);
        }
        
        SOSPeerPersistData(peer->my_id, peer->peer_id, peer->manifest, coderData);
        
        CFReleaseNull(coderData);
    CFReleaseSafe(peer->peer_id);
    CFReleaseSafe(peer->my_id);
    if (peer->manifest)
        SOSManifestDispose(peer->manifest);
    CFReleaseSafe(peer->manifest_digest);
    if (peer->coder)
        SOSCoderDispose(peer->coder);

    free(peer);
}

SOSPeerCoderStatus SOSPeerHandleMessage(SOSPeerRef peer, SOSEngineRef engine, CFDataRef codedMessage, CFErrorRef *error) {
    CFMutableDataRef message = NULL;
    SOSPeerCoderStatus coderStatus = kSOSPeerCoderDataReturned;

    if (peer->coder) {
        coderStatus = SOSCoderUnwrap(peer->coder, peer->send_block, codedMessage, &message, peer->peer_id, error);
    } else {
        message = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, codedMessage);
    }

    switch(coderStatus) {
        case kSOSPeerCoderDataReturned: {
            CFStringRef description = SOSMessageCopyDescription(message);
            secnotice("peer", "Got message from %@: %@", peer->peer_id, description);
            CFReleaseSafe(description);
            coderStatus = (SOSEngineHandleMessage(engine, peer, message, error)) ? coderStatus: kSOSPeerCoderFailure;
            break;
        }
        case kSOSPeerCoderNegotiating:  // Sent message already in Unwrap.
            secnotice("peer", "Negotiating with %@: Got: %@", peer->peer_id, codedMessage);
            break;
        case kSOSPeerCoderNegotiationCompleted:
            if (SOSEngineSyncWithPeer(engine, peer, true, error)) {
                secnotice("peer", "Negotiating with %@ completed: %@" , peer->peer_id, codedMessage);
            } else {
                secerror("Negotiating with %@ completed syncWithPeer: %@ calling syncWithAllPeers" , peer->peer_id, error ? *error : NULL);
                // Clearing the manifest forces SOSEngineSyncWithPeer(engine, peer, false, error) to send a message no matter what.
                // This is needed because that's what gets called by SOSPeerStartSync, which is what SOSCCSyncWithAllPeers triggers.
                SOSPeerSetManifest(peer, NULL, NULL);
                SOSCCSyncWithAllPeers();
                coderStatus = kSOSPeerCoderFailure;
            }
            break;
        case kSOSPeerCoderFailure:      // Probably restart coder
            secnotice("peer", "Failed handling message from %@: Got: %@", peer->peer_id, codedMessage);
            SOSCoderReset(peer->coder);
            coderStatus = SOSCoderStart(peer->coder, peer->send_block, peer->peer_id, error);
            break;
        case kSOSPeerCoderStaleEvent:   // We received an event we have already processed in the past.
            secnotice("peer", "StaleEvent from %@: Got: %@", peer->peer_id, codedMessage);
            break;
        default:
            assert(false);
            break;
    }

    CFReleaseNull(message);

    return coderStatus;
}

SOSPeerCoderStatus SOSPeerStartSync(SOSPeerRef peer, SOSEngineRef engine, CFErrorRef *error) {
    SOSPeerCoderStatus coderStatus = kSOSPeerCoderDataReturned;

    if (peer->coder) {
        coderStatus = SOSCoderStart(peer->coder, peer->send_block, peer->peer_id, error);
    }

    switch(coderStatus) {
        case kSOSPeerCoderDataReturned:         // fallthrough
        case kSOSPeerCoderNegotiationCompleted: // fallthrough
            coderStatus = (SOSEngineSyncWithPeer(engine, peer, false, error)) ? coderStatus: kSOSPeerCoderFailure;
            break;
        case kSOSPeerCoderNegotiating: // Sent message already in Unwrap.
            secnotice("peer", "Started sync with %@", peer->peer_id);
            break;
        case kSOSPeerCoderFailure: // Probably restart coder
            break;
        default:
            assert(false);
            break;
    }
    return coderStatus;
}

bool SOSPeerSendMessage(SOSPeerRef peer, CFDataRef message, CFErrorRef *error) {
    CFMutableDataRef codedMessage = NULL;
    CFStringRef description = SOSMessageCopyDescription(message);

    SOSPeerCoderStatus coderStatus = kSOSPeerCoderDataReturned;

    if (peer->coder) {
        coderStatus = SOSCoderWrap(peer->coder, message, &codedMessage, peer->peer_id, error);
    } else {
        codedMessage = CFDataCreateMutableCopy(kCFAllocatorDefault, 0, message);
    }
    bool ok = true;
    switch(coderStatus) {
        case kSOSPeerCoderDataReturned:
            secnotice("peer", "%@ message: %@", peer->peer_id, description);
            peer->send_block(codedMessage, error);
            break;
        case kSOSPeerCoderNegotiating:
            secnotice("peer", "%@ coder Negotiating - message not sent", peer->peer_id);
            ok = SOSCreateErrorWithFormat(kSOSCCError, NULL, error, NULL, CFSTR("%@ failed to send message peer still negotiating"), peer->peer_id);
            break;
        default: // includes kSOSPeerCoderFailure
            secerror("%@ coder failure - message not sent %@", peer->peer_id, error ? *error : NULL);
            ok = false;
            break;
    }
    CFReleaseSafe(description);
    return ok;
}

bool SOSPeerCanSendMessage(SOSPeerRef peer) {
    return (!peer->coder || SOSCoderCanWrap(peer->coder));
}

CFIndex SOSPeerGetVersion(SOSPeerRef peer) {
    return peer->version;
}

CFStringRef SOSPeerGetID(SOSPeerRef peer) {
    return peer->peer_id;
}

bool SOSPeersEqual(SOSPeerRef peerA, SOSPeerRef peerB)
{
    // Use mainly to see if peerB is actually this device (peerA)
    return CFStringCompare(SOSPeerGetID(peerA), SOSPeerGetID(peerB), 0) == kCFCompareEqualTo;
}

bool SOSPeerSetManifest(SOSPeerRef peer, SOSManifestRef manifest, CFErrorRef *error __unused) {
    CFRetainSafe(manifest);
    CFReleaseSafe(peer->manifest);
    peer->manifest = manifest;

    CFReleaseNull(peer->manifest_digest);
    return true;
}

SOSManifestRef SOSPeerCopyManifest(SOSPeerRef peer, CFErrorRef *error __unused) {
    if (!peer->manifest) {
        SecCFCreateError(kSOSPeerHasNoManifest, sErrorDomain, CFSTR("failed to find peer manifest - not yet implemented"), NULL, error);
        return NULL;
    }

    CFRetain(peer->manifest);
    return peer->manifest;
}

CFDataRef SOSPeerCopyManifestDigest(SOSPeerRef peer, CFErrorRef *error) {
    if (peer->manifest_digest) {
        CFRetain(peer->manifest_digest);
    } else {
        if (peer->manifest) {
            CFMutableDataRef data = CFDataCreateMutable(NULL, CC_SHA1_DIGEST_LENGTH);
            if (data) {
                CFDataSetLength(data, CC_SHA1_DIGEST_LENGTH);
                CCDigest(kCCDigestSHA1, SOSManifestGetBytePtr(peer->manifest), (CC_LONG)SOSManifestGetSize(peer->manifest), CFDataGetMutableBytePtr(data));
                peer->manifest_digest = data;
                CFRetain(peer->manifest_digest);
            } else {
                SecCFCreateError(kSOSPeerDigestFailure, sErrorDomain, CFSTR("failed to create digest"), NULL, error);
            }
        } else {
            SecCFCreateError(kSOSPeerHasNoManifest, sErrorDomain, CFSTR("peer has no manifest, can't create digest"), NULL, error);
        }
    }

    return peer->manifest_digest;
}
