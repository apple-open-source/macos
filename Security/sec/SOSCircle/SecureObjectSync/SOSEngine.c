/*
 * Created by Michael Brouwer on 7/17/12.
 * Copyright 2012 Apple Inc. All Rights Reserved.
 */

/*
 * SOSEngine.c -  Implementation of a secure object syncing engine
 */

#include <SecureObjectSync/SOSEngine.h>
#include <SecureObjectSync/SOSPeer.h>
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
#include <SecItemServer.h>
#include <SecItemPriv.h>

/* DataSource helper macros and functions. */

// TODO: Change to create with DER.
#define SOSObjectCreateWithPropertyList(dataSource, plist, error) (dataSource->createWithPropertyList(dataSource, plist, error))

#define SOSObjectCopyPropertyList(dataSource, object, error) (dataSource->copyPropertyList(object, error))
#define SOSObjectCopyDigest(dataSource, object, error) (dataSource->copyDigest(object, error))
#define SOSObjectCopyPrimaryKey(dataSource, object, error) (dataSource->copyPrimaryKey(object, error))
#define SOSObjectCopyMergedObject(dataSource, object1, object2, error) (dataSource->copyMergedObject(object1, object2, error))

#define kSOSMaxObjectPerMessage (500)

static CFArrayRef SOSDataSourceCopyObjectArray(SOSDataSourceRef data_source, SOSManifestRef manifest, CFErrorRef *error) {
    CFMutableArrayRef objects = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);

    // Delta sync by only sending a max of kSOSMaxObjectPerMessage objects at a time.
    SOSManifestRef toSend = NULL;
    if (SOSManifestGetCount(manifest) > kSOSMaxObjectPerMessage) {
        toSend = SOSManifestCreateWithBytes(SOSManifestGetBytePtr(manifest), kSOSMaxObjectPerMessage * SOSDigestSize, error);
    } else {
        toSend = manifest;
        CFRetain(toSend);
    }

    if (!data_source->foreach_object(data_source, toSend, error, ^bool (SOSObjectRef object, CFErrorRef *localError) {
        CFDictionaryRef plist = SOSObjectCopyPropertyList(data_source, object, localError);
        if (plist) {
            CFArrayAppendValue(objects, plist);
            CFRelease(plist);
        }
        return plist;
    })) {
        CFReleaseNull(objects);
    }
    CFRetainSafe(toSend);
    return objects;
}

static CFDataRef SOSDataSourceCopyManifestDigest(SOSDataSourceRef ds, CFErrorRef *error) {
    CFMutableDataRef manifestDigest = CFDataCreateMutable(0, SOSDigestSize);
    CFDataSetLength(manifestDigest, SOSDigestSize);
    if (!ds->get_manifest_digest(ds, CFDataGetMutableBytePtr(manifestDigest), error))
        CFReleaseNull(manifestDigest);

    return manifestDigest;
}

static SOSManifestRef SOSDataSourceCopyManifest(SOSDataSourceRef ds, CFErrorRef *error) {
    return ds->copy_manifest(ds, error);
}

static void SOSDataSourceRelease(SOSDataSourceRef ds) {
    ds->release(ds);
}


/* SOSEngine implementation. */

static CFStringRef sErrorDomain = CFSTR("com.apple.security.sos.engine.error");

static bool SOSEngineCreateError(CFIndex errorCode, CFStringRef descriptionString, CFErrorRef previousError, CFErrorRef *newError) {
    SecCFCreateError(errorCode, descriptionString, sErrorDomain, previousError, newError);
    return true;
}

struct __OpaqueSOSEngine {
    SOSDataSourceRef dataSource;
};

SOSEngineRef SOSEngineCreate(SOSDataSourceRef dataSource, CFErrorRef *error) {
    SOSEngineRef engine = calloc(1, sizeof(struct __OpaqueSOSEngine));
    engine->dataSource = dataSource;

    return engine;
}

void SOSEngineDispose(SOSEngineRef engine) {
    SOSDataSourceRelease(engine->dataSource);
    free(engine);
}

/* SOSEngine. */
enum SOSMessageType {
    SOSManifestInvalidMessageType = 0,
    SOSManifestDigestMessageType = 1,
    SOSManifestMessageType = 2,
    SOSManifestDeltaAndObjectsMessageType = 3,
};

/* H(): SHA1 hash function.
 M: Manifest of peer p
 MSG: H(M).
 
 
 SOSPeerMessage := SEQUENCE {
 messageType INTEGER (manifestDigest, manifest, manifestDeltaAndObjects)
 version INTEGER OPTIONAL default v0
 content ANY defined by messageType
 }
 
 ManifestDigest := OCTECT STRING (length 20)
 Manifest := OCTECT STRING (length 20 * number of entries)
 
 
 Value := CHOICE {
 bool Boolean
 number INTEGER
 string UTF8String
 data OCTECT STRING
 date GENERAL TIME
 dictionary Object
 array Array
 }
 
 KVPair := SEQUENCE {
 key UTF8String
 value Value
 }
 
 Array := SEQUENCE of Value
 Dictionary := SET of KVPair
 
 Object := SEQUENCE {
 [0] conflict OCTECT STRING OPTIONAL
 [1] change OCTECT STRING OPTIONAL
 object Dictionary
 
 
 ManifestDeltaAndObjects := SEQUENCE {
 manfestDigest ManifestDigest
 removals Manifest
 additions Manifest
 addedObjects SEQUENCE of Object
 }
 
 manifestDigest content = OCTECT STRING
 manifest content := OCTECT STRING
 manifestDeltaAndObjects := SEQUENCE {
 manfestDigest ManifestDigest
 }
 
 */


/* ManifestDigest message */
static size_t der_sizeof_manifest_digest_message(void) {
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        (ccder_sizeof_uint64(SOSManifestDigestMessageType) +
                         ccder_sizeof_raw_octet_string(SOSDigestSize)));
}

static uint8_t *der_encode_manifest_digest_message(const uint8_t digest[SOSDigestSize], const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           ccder_encode_uint64(SOSManifestDigestMessageType, der,
           ccder_encode_raw_octet_string(SOSDigestSize, digest, der, der_end)));
}

/* This message is sent to each peer that joins a circle and can also be sent
 as a form of ACK to confirm that the local peer is in sync with the peer
 this is beig sent to. */
CFDataRef SOSEngineCreateManifestDigestMessage(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
    /* TODO: avoid copying the digest here by inlining der_encode_manifest_digest_message(). */

    uint8_t digest[SOSDigestSize];
    if (!engine->dataSource->get_manifest_digest(engine->dataSource, &digest[0], error)) {
        return NULL;
    }
    
    size_t der_size = der_sizeof_manifest_digest_message();
    CFMutableDataRef message = CFDataCreateMutable(NULL, der_size);
    if (message == NULL) {
	return NULL;
    }
    CFDataSetLength(message, der_size);
    uint8_t *der_end = CFDataGetMutableBytePtr(message);
    const uint8_t *der = der_end;
    der_end += der_size;

    der_end = der_encode_manifest_digest_message(digest, der, der_end);
    assert(der == der_end);

    return message;
}


/* Manifest message */
static size_t der_sizeof_manifest_message(SOSManifestRef manifest) {
    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        (ccder_sizeof_uint64(SOSManifestMessageType) +
                         ccder_sizeof_raw_octet_string(SOSManifestGetSize(manifest))));
}

static uint8_t *der_encode_manifest_message(SOSManifestRef manifest, const uint8_t *der, uint8_t *der_end) {
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           ccder_encode_uint64(SOSManifestMessageType, der,
           ccder_encode_raw_octet_string(SOSManifestGetSize(manifest),
                                         SOSManifestGetBytePtr(manifest), der, der_end)));
}

/* This message is sent in response to a manifestDigest if our manifestDigest
 differs from that of the received manifestDigest, or in response to a
 manifestAndObjects message if the manifestDigest in the received message
 doesn't match our own manifestDigest. */
CFDataRef SOSEngineCreateManifestMessage(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
    SOSManifestRef manifest = SOSDataSourceCopyManifest(engine->dataSource, error);
    if (!manifest)
        return NULL;

    size_t der_size = der_sizeof_manifest_message(manifest);
    CFMutableDataRef message = CFDataCreateMutable(NULL, der_size);
    CFDataSetLength(message, der_size);
    uint8_t *der_end = CFDataGetMutableBytePtr(message);
    const uint8_t *der = der_end;
    der_end += der_size;

    der_end = der_encode_manifest_message(manifest, der, der_end);
    assert(der == der_end);

    return message;
}


/* ManifestDeltaAndObjects message */
static size_t der_sizeof_manifest_and_objects_message(SOSManifestRef removals, SOSManifestRef additions, CFArrayRef objects, CFErrorRef *error) {
    size_t objects_size = der_sizeof_plist(objects, error);
    if (objects_size == 0)
        return objects_size;

    return ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                        (ccder_sizeof_uint64(SOSManifestDeltaAndObjectsMessageType) +
                         ccder_sizeof(CCDER_CONSTRUCTED_SEQUENCE,
                         (ccder_sizeof_raw_octet_string(SOSDigestSize) +
                          ccder_sizeof_raw_octet_string(SOSManifestGetSize(removals)) +
                          ccder_sizeof_raw_octet_string(SOSManifestGetSize(additions)) +
                          objects_size))));
}

static uint8_t *der_encode_manifest_and_objects_message(CFDataRef digest, SOSManifestRef removals, SOSManifestRef additions, CFArrayRef objects, CFErrorRef *error, const uint8_t *der, uint8_t *der_end) {
    assert(CFDataGetLength(digest) == SOSDigestSize);
    return ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           ccder_encode_uint64(SOSManifestDeltaAndObjectsMessageType, der,
           ccder_encode_constructed_tl(CCDER_CONSTRUCTED_SEQUENCE, der_end, der,
           ccder_encode_raw_octet_string(SOSDigestSize, CFDataGetBytePtr(digest), der,
           ccder_encode_raw_octet_string(SOSManifestGetSize(removals), SOSManifestGetBytePtr(removals), der,
           ccder_encode_raw_octet_string(SOSManifestGetSize(additions), SOSManifestGetBytePtr(additions), der,
           der_encode_plist(objects, error, der, der_end)))))));
}

/* This message is sent in response to a local change that needs to be
 propagated to our peers or in response to a manifest or manifestDigest
 message from a peer that is not in sync with us yet. */
CFDataRef SOSEngineCreateManifestAndObjectsMessage(SOSEngineRef engine, SOSPeerRef peer, CFErrorRef *error) {
    /* Assumptions:
       peer has a manifest that corresponds to peers real manifest.
       we send everything in our datasource that's not in peers manifest already to peer.
     */
    CFMutableDataRef message = NULL;
    SOSManifestRef manifest, peerManifest, additions, removals;

retry:
    manifest = SOSDataSourceCopyManifest(engine->dataSource, error);
    if (!manifest)
        goto errOut4;
    
    peerManifest = SOSPeerCopyManifest(peer, error);
    if (!peerManifest)
        goto errOut3;

    if (!SOSManifestDiff(manifest, peerManifest, &additions, &removals, error))
        goto errOut2;

    CFErrorRef localError = NULL;
    CFArrayRef objects = SOSDataSourceCopyObjectArray(engine->dataSource, additions, &localError);
    if (!objects) {
        if(SecErrorGetOSStatus(localError)==errSecDecode) {
            secnotice("engine", "Corrupted item found: %@", localError);
            CFReleaseNull(manifest);
            CFReleaseNull(additions);
            CFReleaseNull(removals);
            CFReleaseNull(peerManifest);
            CFReleaseNull(localError);
            goto retry;
        }
        if(error && *error==NULL)
            *error=localError;
        else
            CFReleaseNull(localError);
        goto errOut1;
    }

    size_t der_size = der_sizeof_manifest_and_objects_message(removals, additions, objects, error);
    if (der_size == 0)
        goto errOut0;

    /* TODO: avoid copying the digest here by inlining der_encode_manifest_and_objects_message(). */
    CFDataRef peerDigest = SOSPeerCopyManifestDigest(peer, error);
    if (!peerDigest)
        goto errOut0;

    message = CFDataCreateMutable(NULL, der_size);
    CFDataSetLength(message, der_size);
    uint8_t *der_end = CFDataGetMutableBytePtr(message);
    const uint8_t *der = der_end;
    der_end += der_size;

    der_end = der_encode_manifest_and_objects_message(peerDigest, removals, additions, objects, error, der, der_end);
    assert(der == der_end);
    if (der_end == NULL) {
        CFReleaseNull(message);
        goto errOut_;
    }

    /* Record the peers new manifest assuming that peer will accept all the
       changes we are about to send them. */
    SOSPeerSetManifest(peer, manifest, error);

errOut_:
    CFRelease(peerDigest);
errOut0:
    CFRelease(objects);
errOut1:
    SOSManifestDispose(removals);
    SOSManifestDispose(additions);
errOut2:
    SOSManifestDispose(peerManifest);
errOut3:
    SOSManifestDispose(manifest);
errOut4:

    return message;
}

static const uint8_t *der_decode_msg_type(enum SOSMessageType *msg_type,
                                          const uint8_t *der,
                                          const uint8_t *der_end,
                                          CFErrorRef *error) {
    const uint8_t *body_end;
    der = ccder_decode_sequence_tl(&body_end, der, der_end);
    if (!der)
        return NULL;

    if (body_end != der_end) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Trailing garbage at end of message"), NULL, error);
        return NULL;
    }

    uint64_t msgType;
    der = ccder_decode_uint64(&msgType, der, der_end);
    if (msgType < 1 || msgType > SOSManifestDeltaAndObjectsMessageType) {
        SecCFCreateErrorWithFormat(kSOSEngineInvalidMessageError, sErrorDomain,
                                   NULL, error, NULL,
                                   CFSTR("Bad message type: %llu"), msgType);
        return NULL;
    }
    *msg_type = (enum SOSMessageType)msgType;
    return der;
}

static const uint8_t *
der_decode_manifest_digest(CFDataRef *digest, CFErrorRef *error,
                           const uint8_t *der, const uint8_t *der_end) {
    require_quiet(der, errOut);
    size_t len;
    der = ccder_decode_tl(CCDER_OCTET_STRING, &len, der, der_end);
    require_action_quiet(der, errOut, SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Failed to find string"), NULL, error));
    require_action_quiet(len == SOSDigestSize, errOut, SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Invalid digest size"), NULL, error));

    *digest = CFDataCreate(0, der, len);
    require_action_quiet(*digest, errOut, SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Failed to create digest"), NULL, error));

    der += len;
    require_action_quiet(der, errOut, CFReleaseNull(*digest); SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Failed to find string"), NULL, error));

    return der;

errOut:
    return NULL;
}

static const uint8_t *
der_decode_manifest(SOSManifestRef *manifest, CFErrorRef *error,
                    const uint8_t *der, const uint8_t *der_end) {
    if (!der)
        goto errOut;
    size_t len;
    der = ccder_decode_tl(CCDER_OCTET_STRING, &len, der, der_end);
    if (!der) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Failed to decode manifest"), NULL, error);
        goto errOut;
    }
    if (len % SOSDigestSize != 0) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("manifest not a multiple of digest size"), NULL, error);
        goto errOut;
    }
    *manifest = SOSManifestCreateWithBytes(der, len, error);
    if (!*manifest)
        goto errOut;

    return der += len;

errOut:
    return NULL;
}

static const uint8_t *
der_decode_manifest_digest_message(CFDataRef *digest, CFErrorRef *error,
                                   const uint8_t *der, const uint8_t *der_end) {
    der = der_decode_manifest_digest(digest, error, der, der_end);
    if (der && der != der_end) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Trailing garbage after digest"), NULL, error);
        CFReleaseNull(*digest);
        der = NULL;
    }
    return der;
}

static const uint8_t *
der_decode_manifest_message(SOSManifestRef *manifest, CFErrorRef *error,
                            const uint8_t *der, const uint8_t *der_end) {
    der = der_decode_manifest(manifest, error, der, der_end);
    if (der && der != der_end) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Trailing garbage after manifest"), NULL, error);
        SOSManifestDispose(*manifest);
        *manifest = NULL;
        der = NULL;
    }
    return der;
}

static const uint8_t *
der_decode_manifest_and_objects_message(CFDataRef *peerManifestDigest,
                                        SOSManifestRef *removals,
                                        SOSManifestRef *additions,
                                        CFArrayRef *objects,
                                        CFErrorRef *error, const uint8_t *der,
                                        const uint8_t *der_end) {
    const uint8_t *body_end;
    der = ccder_decode_sequence_tl(&body_end, der, der_end);
    if (!der) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Failed to decode top level sequence"), NULL, error);
        goto errOut;
    }

    if (body_end != der_end) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Trailing garbage at end of message"), NULL, error);
        goto errOut;
    }

    der = der_decode_manifest_digest(peerManifestDigest, error, der, der_end);
    if (!der)
        goto errOut;
    der = der_decode_manifest(removals, error, der, der_end);
    if (!der)
        goto errOut1;
    der = der_decode_manifest(additions, error, der, der_end);
    if (!der)
        goto errOut2;

    CFPropertyListRef pl;
    der = der_decode_plist(0, 0, &pl, error, der, der_end);
    if (!der)
        goto errOut3;

    if (der != der_end) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("Trailing garbage at end of message body"), NULL, error);
        goto errOut4;
    }

    // TODO Check that objects is in fact an array. */
    if (CFArrayGetTypeID() != CFGetTypeID(pl)) {
        SOSEngineCreateError(kSOSEngineInvalidMessageError, CFSTR("objects is not an array"), NULL, error);
        goto errOut4;
    }
    *objects = pl;

    return der;

errOut4:
    CFRelease(pl);
errOut3:
    CFRelease(additions);
errOut2:
    CFRelease(removals);
errOut1:
    CFRelease(peerManifestDigest);
errOut:
    return NULL;
}


#if 0
enum SOSMessageType SOSMessageGetType(CFDataRef message) {
    const uint8_t *der = CFDataGetBytePtr(message);
    const uint8_t *der_end = der + CFDataGetLength(message);
    enum SOSMessageType msg_type;
    der_decode_msg_type(&msg_type, der, der_end, NULL);
    if (!der) {
        return SOSManifestInvalidMessageType;
    }

    return msg_type;
}
#endif

/* H(): SHA1 hash function.
 M: Manifest of peer p
 MSG: H(M) */
static CFDataRef SOSEngineCopyManifestDigestReply(SOSEngineRef engine,
                                                  SOSPeerRef peer,
                                                  CFDataRef digest,
                                                  CFErrorRef *error) {
    CFDataRef reply = NULL;
    CFDataRef peerDigest = SOSPeerCopyManifestDigest(peer, NULL);
    CFDataRef manifestDigest = SOSDataSourceCopyManifestDigest(engine->dataSource, error);
    if (manifestDigest) {
        if (CFEqual(manifestDigest, digest)) {
            /* Our dataSources manifest and that of the peer are equal, we are in sync. */
            if (peerDigest && CFEqual(peerDigest, digest)) {
                /* The last known digest we had for peer already matched the digest peer
                   sent us, so this message is redundant, consider it an ack of our last
                   message to peer. */
                reply = CFDataCreate(kCFAllocatorDefault, NULL, 0);
            } else {
                /* Our peer just sent us a manifest digest that matches our own, but the digest
                   we have for the peer (if any) doesn't match that.   Peer must have the same
                   manifest we do, so record that. */
                SOSManifestRef manifest = SOSDataSourceCopyManifest(engine->dataSource, error);
                if (manifest) {
                    bool ok = SOSPeerSetManifest(peer, manifest, error);
                    SOSManifestDispose(manifest);
                    if (ok) {
                        /* Since we got lucky and happen to have the same digest as our peer, we
                         send back an ack to ensure our peer ends up knowning our manifest as well. */
                        reply = SOSEngineCreateManifestDigestMessage(engine, peer, error);
                    }
                }
            }
        } else if (peerDigest && CFEqual(peerDigest, digest)) {
            /* We know peer's current manifest is correct (the computed digest
               matches the passed in one) but peer and our dataSource
               are not in sync.  Send the deltas to peer. */
            reply = SOSEngineCreateManifestAndObjectsMessage(engine, peer, error);
        } else {
            /* Our peer has no digest yet, or the manifestDigest peer just sent
               us doesn't match the digest of the manifest we think peer has.
               We need to get peer to tell us their manifest, to do so we sent
               it ours and hope it responds with deltas. */
            reply = SOSEngineCreateManifestMessage(engine, peer, error);
        }
        CFRelease(manifestDigest);
    }
    CFReleaseSafe(peerDigest);
    return reply;
}

/* M: Manifest of peer p
 MSG: M */
static CFDataRef SOSEngineCopyManifestReply(SOSEngineRef engine, SOSPeerRef peer,
                                            SOSManifestRef manifest,
                                            CFErrorRef *error) {
    CFDataRef reply = NULL;
    // Peer just told us what his manifest was.  Let's roll with it.
    SOSPeerSetManifest(peer, manifest, error);
    CFDataRef peerManifestDigest = SOSPeerCopyManifestDigest(peer, error);
    if (peerManifestDigest) {
        CFDataRef manifestDigest = SOSDataSourceCopyManifestDigest(engine->dataSource, error);
        if (manifestDigest) {
            if (CFEqual(peerManifestDigest, manifestDigest)) {
                /* We're in sync, optionally send peer an ack. */
                reply = SOSEngineCreateManifestDigestMessage(engine, peer, error);
            } else {
                /* Send peer the objects it is missing from our manifest. */
                reply = SOSEngineCreateManifestAndObjectsMessage(engine, peer, error);
            }
            CFRelease(manifestDigest);
        }
        CFRelease(peerManifestDigest);
    }
    return reply;
}

static bool SOSEngineProccesObjects(SOSEngineRef engine,
                                    SOSPeerRef peer,
                                    CFDataRef digest,
                                    SOSManifestRef removals,
                                    SOSManifestRef additions,
                                    CFArrayRef objects,
                                    CFErrorRef *error) {
    __block bool result = true;
    CFArrayForEach(objects, ^(const void *value) {
        SOSObjectRef ob = SOSObjectCreateWithPropertyList(engine->dataSource, value, error);
        if (ob) {
            SOSMergeResult mr = engine->dataSource->add(engine->dataSource, ob, error);
            if (!mr) {
                result = false;
                // assertion failure, duplicate object added during transaction, that wasn't explicitly listed in removal list.
                // treat as conflict?
                // oa =  ds->lookup(pkb);
                // ds->choose_between(oa, ob)
                // TODO: This is needed is we want to allow conflicts with other circles.
                SetCloudKeychainTraceValueForKey(kCloudKeychainNumberOfTimesSyncFailed, 1);
                secerror("assertion failure, add failed: %@",
                         error ? *error : (CFErrorRef)CFSTR("error is null"));
            }
            CFRelease(ob);
        }
    });
    return result;
}

/* H(): SHA1 hash function.
 L: Manifest of local peer.
 M: Manifest of peer p.
 M-L: Manifest of entries in M but not in L
 L-M: Manifest of entries in L but not in M
 O(M): Objects in manifest M
 MSG: H(L) || L-M || M-L || O(M-L)  */
static CFDataRef SOSEngineCopyManifestAndObjectsReply(SOSEngineRef engine,
                                                      SOSPeerRef peer,
                                                      CFDataRef digest,
                                                      SOSManifestRef removals,
                                                      SOSManifestRef additions,
                                                      CFArrayRef objects,
                                                      CFErrorRef *error) {
    CFDataRef reply = NULL;
    CFMutableDataRef manifestDigest = (CFMutableDataRef)SOSDataSourceCopyManifestDigest(engine->dataSource, error);
    if (manifestDigest) {
        SOSManifestRef manifest = SOSDataSourceCopyManifest(engine->dataSource, error);

        /* Always proccess the objects after we snapshot our manifest. */
        if (!SOSEngineProccesObjects(engine, peer, digest, removals, additions, objects, error)) {
            secerror("peer: %@ SOSEngineProccesObjects(): %@", SOSPeerGetID(peer), *error);
        }

        if (CFEqual(manifestDigest, digest)) {
            SOSManifestRef peerManifest = NULL;
            if (manifest) {
                peerManifest = SOSManifestCreateWithPatch(manifest, removals, additions, error);
            }
            if (peerManifest) {
                if (SOSPeerSetManifest(peer, peerManifest, error)) {
                    /* Now proccess the objects. */
                    if (!SOSEngineProccesObjects(engine, peer, digest, removals, additions, objects, error)) {
                        secerror("peer: %@ SOSEngineProccesObjects(): %@", SOSPeerGetID(peer), *error);
                    }

                    CFDataRef peerDigest = SOSPeerCopyManifestDigest(peer, error);
                    if (peerDigest) {
                        /* Depending on whether after proccess objects we still have objects that need to be sent back to peer we respond with our digestManifest or with a manifestAndObjectsMessage. */
                        if (engine->dataSource->get_manifest_digest(engine->dataSource, CFDataGetMutableBytePtr(manifestDigest), error)) {
                            if (CFEqual(manifestDigest, peerDigest)) {
                                reply = SOSEngineCreateManifestDigestMessage(engine, peer, error);
                            } else {
                                reply = SOSEngineCreateManifestAndObjectsMessage(engine, peer, error);
                            }
                        }
                        CFRelease(peerDigest);
                    }
                }
                CFRelease(peerManifest);
            } else {
                secerror("Received peer: %@ sent bad message: %@", SOSPeerGetID(peer), *error);
                /* We failed to compute peer's digest, let's tell him ours again and hope for a retransmission. */
                /* TODO: Perhaps this should be sent by the top level whenever an error occurs during parsing. */
                reply = SOSEngineCreateManifestDigestMessage(engine, peer, error);
            }
        } else {
            /* ds->manifestDigest != msg->manigestDigest => We received deltas
               against a manifest we don't have respond with our current
               manifest to get back in sync. */
            reply = SOSEngineCreateManifestMessage(engine, peer, error);
        }
        CFReleaseSafe(manifest);
        CFRelease(manifestDigest);
    }
    return reply;
}

/* Handle incoming message from peer p.  Return false if there was an error, true otherwise. */
bool SOSEngineHandleMessage(SOSEngineRef engine, SOSPeerRef peer,
                            CFDataRef message, CFErrorRef *error) {
    CFDataRef reply = NULL;
    SOSManifestRef oldPeerManifest = SOSPeerCopyManifest(peer, NULL);
    const uint8_t *der = CFDataGetBytePtr(message);
    const uint8_t *der_end = der + CFDataGetLength(message);
    enum SOSMessageType msgType;

    der = der_decode_msg_type(&msgType, der, der_end, error);
    if (der) switch (msgType) {
        case SOSManifestDigestMessageType:
        {
            CFDataRef digest = NULL; // Make the static analyzer happy by NULL and Release safe
            der = der_decode_manifest_digest_message(&digest, error, der, der_end);
            if (der) {
                reply = SOSEngineCopyManifestDigestReply(engine, peer, digest, error);
            }
            CFReleaseSafe(digest);
            break;
        }
        case SOSManifestMessageType:
        {
            SOSManifestRef manifest;
            der = der_decode_manifest_message(&manifest, error, der, der_end);
            if (der) {
                reply = SOSEngineCopyManifestReply(engine, peer, manifest, error);
                SOSManifestDispose(manifest);
            }
            break;
        }
        case SOSManifestDeltaAndObjectsMessageType:
        {
            CFDataRef peerManifestDigest;
            SOSManifestRef removals;
            SOSManifestRef additions;
            CFArrayRef objects;
            der = der_decode_manifest_and_objects_message(&peerManifestDigest, &removals, &additions, &objects, error, der, der_end);
            if (der) {
                reply = SOSEngineCopyManifestAndObjectsReply(engine, peer, peerManifestDigest, removals, additions, objects, error);
                CFRelease(peerManifestDigest);
                SOSManifestDispose(removals);
                SOSManifestDispose(additions);
                CFRelease(objects);
            }
            break;
        }
        default:
            SecCFCreateErrorWithFormat(kSOSEngineInvalidMessageError, sErrorDomain,
                                       NULL, error, NULL, CFSTR("Invalid message type %d"), msgType);
            break;
    }

    bool ok = reply;
    if (reply && CFDataGetLength(reply)) {
        ok = SOSPeerSendMessage(peer, reply, error);
        if (!ok)
            SOSPeerSetManifest(peer, oldPeerManifest, NULL);
    }
    secnotice("engine", "%@", SOSPeerGetID(peer));
    CFReleaseSafe(oldPeerManifest);
    CFReleaseSafe(reply);
    return ok;
}

bool SOSEngineSyncWithPeer(SOSEngineRef engine, SOSPeerRef peer, bool force,
                           CFErrorRef *error) {
    CFDataRef reply = NULL;
    SOSManifestRef oldPeerManifest = SOSPeerCopyManifest(peer, NULL);
    bool ok = true;
    require_quiet(SOSPeerCanSendMessage(peer), exit);
    CFDataRef peerDigest = SOSPeerCopyManifestDigest(peer, NULL);
    CFMutableDataRef manifestDigest = CFDataCreateMutable(0, SOSDigestSize);
    CFDataSetLength(manifestDigest, SOSDigestSize);
    if (engine->dataSource->get_manifest_digest(engine->dataSource, CFDataGetMutableBytePtr(manifestDigest), error)) {
        if (peerDigest) {
            if (CFEqual(peerDigest, manifestDigest)) {
                /* We are in sync with peer already. */
                if (force) {
                    /* If we are at the end of the OTR handshake, we have to send
                       something to our peer no matter what to break the symmmetry.  */
                    reply = SOSEngineCreateManifestDigestMessage(engine, peer, error);
                } else {
                    reply = CFDataCreate(kCFAllocatorDefault, NULL, 0);
                }
            } else {
                /* We have have a digest for peer's manifest and it doesn't
                   match our current digest, so send deltas to peer. */
                reply = SOSEngineCreateManifestAndObjectsMessage(engine, peer, error);
            }
        } else {
            /* We have no digest for peer yet, send our manifest digest to peer,
               it should respond with it's manifest so we can sync. */
            reply = SOSEngineCreateManifestDigestMessage(engine, peer, error);
        }
    }
    CFRelease(manifestDigest);
    CFReleaseSafe(peerDigest);

    ok = ok && reply;
    if (ok && CFDataGetLength(reply)) {
        ok = SOSPeerSendMessage(peer, reply, error);
        if (!ok)
            SOSPeerSetManifest(peer, oldPeerManifest, NULL);
    }

exit:
    secnotice("engine", "%@", SOSPeerGetID(peer));
    CFReleaseSafe(oldPeerManifest);
    CFReleaseSafe(reply);
    return ok;
}

#if 0
static void appendObject(CFMutableStringRef desc, CFDictionaryRef object) {
    __block bool needComma = false;
    CFDictionaryForEach(object, ^(const void *key, const void *value) {
        if (needComma)
            CFStringAppend(desc, CFSTR(","));
        else
            needComma = true;

        CFStringAppend(desc, key);
        CFStringAppend(desc, CFSTR("="));
        if (CFEqual(CFSTR("data"), key)) {
            CFStringAppend(desc, CFSTR("<?>"));
        } else if (isData(value)) {
            CFStringAppendHexData(desc, value);
        } else {
            CFStringAppendFormat(desc, 0, CFSTR("%@"), value);
        }
    });
}
#endif

static void appendObjects(CFMutableStringRef desc, CFArrayRef objects) {
    __block bool needComma = false;
    CFArrayForEach(objects, ^(const void *value) {
        if (needComma)
            CFStringAppend(desc, CFSTR(","));
        else
            needComma = true;

        SecItemServerAppendItemDescription(desc, value);
    });
}

CFStringRef SOSMessageCopyDescription(CFDataRef message) {
    if (!message)
        return CFSTR("<NULL>");

    CFMutableStringRef desc = CFStringCreateMutable(0, 0);
    const uint8_t *der = CFDataGetBytePtr(message);
    const uint8_t *der_end = der + CFDataGetLength(message);
    enum SOSMessageType msgType;

    CFStringAppend(desc, CFSTR("<Msg"));
    der = der_decode_msg_type(&msgType, der, der_end, 0);
    if (der) switch (msgType) {
        case SOSManifestDigestMessageType:
        {
            CFStringAppend(desc, CFSTR("ManifestDigest digest: "));
            CFDataRef digest = NULL;
            der = der_decode_manifest_digest_message(&digest, 0, der, der_end);
            if (der) {
                CFStringAppendHexData(desc, digest);
            }
            CFReleaseNull(digest);

            break;
        }
        case SOSManifestMessageType:
        {
            CFStringAppend(desc, CFSTR("Manifest"));

            SOSManifestRef manifest;
            der = der_decode_manifest_message(&manifest, 0, der, der_end);
            if (der) {
                CFStringRef mfdesc = SOSManifestCopyDescription(manifest);
                if (mfdesc) {
                    CFStringAppendFormat(desc, 0, CFSTR(" manifest: %@"), mfdesc);
                    CFRelease(mfdesc);
                }
                SOSManifestDispose(manifest);
            }
            break;
        }
        case SOSManifestDeltaAndObjectsMessageType:
        {
            CFStringAppend(desc, CFSTR("ManifestDeltaAndObjects digest:"));

            CFDataRef peerManifestDigest;
            SOSManifestRef removals;
            SOSManifestRef additions;
            CFArrayRef objects;
            der = der_decode_manifest_and_objects_message(&peerManifestDigest, &removals, &additions, &objects, 0, der, der_end);
            if (der) {
                CFStringAppendHexData(desc, peerManifestDigest);
                CFStringRef remdesc = SOSManifestCopyDescription(removals);
                if (remdesc) {
                    CFStringAppendFormat(desc, 0, CFSTR(" removals: %@"), remdesc);
                    CFRelease(remdesc);
                }
                CFStringRef adddesc = SOSManifestCopyDescription(additions);
                if (adddesc) {
                    CFStringAppendFormat(desc, 0, CFSTR(" additions: %@"), adddesc);
                    CFRelease(adddesc);
                }
                CFStringAppendFormat(desc, 0, CFSTR(" objects: "));
                appendObjects(desc, objects);

                CFRelease(peerManifestDigest);
                SOSManifestDispose(removals);
                SOSManifestDispose(additions);
                CFRelease(objects);
            }
            break;
        }
        default:
            CFStringAppendFormat(desc, 0, CFSTR("InvalidType: %d"), msgType);
            break;
    }

    CFStringAppend(desc, CFSTR(">"));

    return desc;
}
