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
#include <Security/SecureObjectSync/SOSPeer.h>

#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSTransport.h>
#include <Security/SecureObjectSync/SOSViews.h>
#include <Security/SecureObjectSync/SOSChangeTracker.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecDb.h>
#include <utilities/SecFileLocations.h>
#include <utilities/SecIOFormat.h>
#include <utilities/array_size.h>
#include <utilities/debugging.h>
#include <Security/SecureObjectSync/SOSBackupEvent.h>
#include <Security/SecItemBackup.h>

#include <securityd/SOSCloudCircleServer.h>

#include <CoreFoundation/CoreFoundation.h>

#include <stdlib.h>

#include <AssertMacros.h>

// Backup Peer Support
#include <securityd/SecKeybagSupport.h>
#include <notify.h>


//
// MARK: - SOSPeerPersistence code
//
static CFStringRef kSOSPeerSequenceNumberKey = CFSTR("sequence-number");

CFStringRef kSOSPeerDataLabel = CFSTR("iCloud Peer Data Meta-data");

//
// MARK: SOSPeerState (dictionary keys)
//

// PeerState dictionary keys
static CFStringRef kSOSPeerSendObjectsKey = CFSTR("send-objects"); // bool
static CFStringRef kSOSPeerMustSendMessageKey = CFSTR("must-send"); // bool
static CFStringRef kSOSPeerHasBeenInSyncKey = CFSTR("has-been-in-sync"); // bool
static CFStringRef kSOSPeerPendingObjectsKey = CFSTR("pending-objects"); // digest
static CFStringRef kSOSPeerUnwantedManifestKey = CFSTR("unwanted-manifest"); // digest
static CFStringRef kSOSPeerConfirmedManifestKey = CFSTR("confirmed-manifest");  //digest
static CFStringRef kSOSPeerProposedManifestKey = CFSTR("pending-manifest"); // array of digests
static CFStringRef kSOSPeerLocalManifestKey = CFSTR("local-manifest"); // array of digests
static CFStringRef kSOSPeerVersionKey = CFSTR("vers"); // int

//
// SOSPeerMeta keys that can also be used in peerstate...
//
static CFStringRef kSOSPeerPeerIDKey = CFSTR("peer-id"); // string
static CFStringRef kSOSPeerViewsKey = CFSTR("views"); // set (or array) of string
static CFStringRef kSOSPeerKeyBagKey = CFSTR("keybag"); // data

/*
 Theory of syncing for both incoming and outgoing messages

 A peerstate consists of:
 (T, U, C, P, L, D->M, M->D, H->O, O->{(Op,Or)|Oa,(Op,Or)|(Om,Oa),(Op,Or)|Om,Oi|Oi|Ob|Oa,Ob|(Om,Oa),Ob|(Ol,Ou)|Oa,(Ol,Ou)|(Om,Oa)(Ol,Ou)})
    T: to be sent (pendingObjects) manifest
    U: unwanted objects manifest
    C: confirmed manifest
    P: proposed manifest
    D->M? digest or manifest to optional manifest function
    M->D manifest to digest of manifest function
    H->O? hash (manifest entry) to optional object function (the datasource)
    O->{                    Mapping from incoming O objects to one of:
        (Op,Or):            Op = Peers object, Or is replaced local object.
        Oa,(Op,Or):         Oa = appeared local object, Or = Oa, see above for (Op, Or)
        (Om,Oa),(Op,Or):    Om missing local object, was apparently Oa instead see above for (Oa, Op, Or)
        Om,Oi:              Om missing local object, inserted Oi (nothing replaced), but Om still disapeared from manifest
        Oi                  Oi inserted object from peer (nothing replaced)
        Ob:                 Ob both remote and local object are identical, nothing changed
        Oa,Ob:              Oa = appeared local object equal to Oa = Ob.  Equivalent to single Oi
        (Om,Oa),Ob:         Om missing local object, must be O->H->Ob, Oa found in place, Ob != Oa, Oa magically appeared unrelated to O->H->Ob
        (Ol,Ou):            Ol local object wins from peers Ou older object => append Ou to U
        Oa,(Ol,Ou):         Oa appeared as a local object and Oa = Ol above
        (Om,Oa),(Ol,Ou):    Om removed and Oa replaced Om and Oa = Ol as above
    }
 A message consists of
 (B,M,E,O)
    B: base digest
    M: missing manifest
    E: extra manifest
    O: objects

 To send a message we simply compute:
    B: D->M->C
    M: C \ L
    E: L \ C          # subsetted if not all of O is sent
    O: H->O?->O       # as size permits
    and change P to (C \ M:) union E: union O->H->O:

 To receive a message we compute
    Op,Oi,Ob,Oo and Om,Oa,Ol
    C: (D->M->B \ M) union E union O->H->O
        A = E \ (O->H->O)
    T: T \ (Om union Or union A union Oa,Ob.last union (Om,Oa),Ob.last ) union (M intersect L)
 */


//
// MARK: SOSPeerMeta
//

static SOSPeerMetaRef SOSPeerMetaCreate(CFStringRef peerID) {
    return CFRetain(peerID);
}

static SOSPeerMetaRef SOSPeerMetaCreateWithViews(CFStringRef peerID, CFSetRef views) {
    const void *keys[] = { kSOSPeerPeerIDKey, kSOSPeerViewsKey };
    const void *values[] = { peerID, views };
    return CFDictionaryCreate(kCFAllocatorDefault, keys, values, array_size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static SOSPeerMetaRef SOSPeerMetaCreateWithViewsAndKeyBag(CFStringRef peerID, CFSetRef views, CFDataRef keybag) {
    const void *keys[] = { kSOSPeerPeerIDKey, kSOSPeerViewsKey, kSOSPeerKeyBagKey };
    const void *values[] = { peerID, views, keybag };
    return CFDictionaryCreate(kCFAllocatorDefault, keys, values, array_size(keys), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

SOSPeerMetaRef SOSPeerMetaCreateWithComponents(CFStringRef peerID, CFSetRef views, CFDataRef keybag) {
    if (!isString(peerID))
        return NULL;
    if (views) {
        if (keybag)
            return SOSPeerMetaCreateWithViewsAndKeyBag(peerID, views, keybag);
        else
            return SOSPeerMetaCreateWithViews(peerID, views);
    } else
        return SOSPeerMetaCreate(peerID);
}

SOSPeerMetaRef SOSPeerMetaCreateWithState(CFStringRef peerID, CFDictionaryRef state) {
    return SOSPeerMetaCreateWithComponents(peerID, CFDictionaryGetValue(state, kSOSPeerViewsKey), CFDictionaryGetValue(state, kSOSPeerKeyBagKey));
}

CFStringRef SOSPeerMetaGetComponents(SOSPeerMetaRef peerMeta, CFSetRef *views, CFDataRef *keybag, CFErrorRef *error) {
    if (isDictionary(peerMeta)) {
        CFDictionaryRef meta = (CFDictionaryRef)peerMeta;
        CFStringRef peerID = asString(CFDictionaryGetValue(meta, kSOSPeerPeerIDKey), error);
        CFSetRef vns = asSet(CFDictionaryGetValue(meta, kSOSPeerViewsKey), error);
        if (vns && asDataOptional(CFDictionaryGetValue(meta, kSOSPeerKeyBagKey), keybag, error)) {
            if (views)
                *views = vns;
            return peerID;
        }
        return NULL;
    } else {
        if (views) {
            // Hack so tests can pass simple peerIDs
            *views = SOSViewsGetV0ViewSet();
        }
        return asString(peerMeta, error);
    }
}

CFTypeRef SOSPeerOrStateSetViewsKeyBagAndCreateCopy(CFTypeRef peerOrState, CFSetRef views, CFDataRef keyBag) {
    assert(views);
    if (peerOrState && CFGetTypeID(peerOrState) == SOSPeerGetTypeID()) {
        // Inflated peer, update its views and move on
        SOSPeerRef peer = (SOSPeerRef)peerOrState;
        SOSPeerSetViewNameSet(peer, views);
        SOSPeerSetKeyBag(peer, keyBag);
        return CFRetainSafe(peer);
    } else if (peerOrState && CFGetTypeID(peerOrState) == CFDictionaryGetTypeID()) {
        CFMutableDictionaryRef state = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, peerOrState);
        // Deserialized peer, just updated the serialized state with the new views
        CFDictionarySetValue(state, kSOSPeerViewsKey, views);
        if (keyBag)
            CFDictionarySetValue(state, kSOSPeerKeyBagKey, keyBag);
        else
            CFDictionaryRemoveValue(state, kSOSPeerKeyBagKey);
        return state;
    } else {
        // New peer, just create a state object.
        if (keyBag)
            return CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kSOSPeerViewsKey, views, kSOSPeerKeyBagKey, keyBag, NULL);
        else
            return CFDictionaryCreateForCFTypes(kCFAllocatorDefault, kSOSPeerViewsKey, views, NULL);
    }
}

CFTypeRef SOSPeerOrStateSetViewsAndCopyState(CFTypeRef peerOrState, CFSetRef views) {
    assert(views);

    if (peerOrState && CFGetTypeID(peerOrState) == SOSPeerGetTypeID()) {
        // Inflated peer, update its views and deflate it
        SOSPeerRef peer = (SOSPeerRef)peerOrState;
        SOSPeerSetViewNameSet(peer, views);
        return SOSPeerCopyState(peer, NULL);
    } else if (peerOrState && CFGetTypeID(peerOrState) == CFDictionaryGetTypeID()) {
        // We have a deflated peer.  Update its views and keep it deflated
        CFSetRef oldViews = (CFSetRef) CFDictionaryGetValue(peerOrState, kSOSPeerViewsKey);
        CFMutableDictionaryRef state = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, peerOrState);
        CFDictionarySetValue(state, kSOSPeerViewsKey, views);
        if (oldViews && !CFSetIsSubset(views, oldViews)) {
            CFDictionarySetValue(state, kSOSPeerHasBeenInSyncKey, kCFBooleanFalse);
        }
        return state;
    } else {
        return NULL;
    }
}

bool SOSPeerMapEntryIsBackup(const void *mapEntry) {
    if (!mapEntry) return false;
    if (CFGetTypeID(mapEntry) == SOSPeerGetTypeID()) {
        return SOSPeerGetKeyBag((SOSPeerRef)mapEntry);
    } else {
        return CFDictionaryContainsKey(mapEntry, kSOSPeerKeyBagKey);
    }
}

//
// MARK: - SOSManifest
//

enum {
    kSOSPeerMaxManifestWindowDepth = 4
};

static CFStringRef SOSManifestCreateOptionalDescriptionWithLabel(SOSManifestRef manifest, CFStringRef label) {
    if (!manifest) return CFSTR(" -  ");
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(" %@%@"), label, manifest);
}

static CFStringRef SOSManifestArrayCreateOptionalDescriptionWithLabel(CFArrayRef manifests, CFStringRef label) {
    CFIndex count = manifests ? CFArrayGetCount(manifests) : 0;
    if (count == 0) return CFSTR(" -  ");
    SOSManifestRef manifest = (SOSManifestRef)CFArrayGetValueAtIndex(manifests, 0);
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR(" %@[%" PRIdCFIndex "]%@"), label, count, manifest);
}

static void SOSManifestArraySetManifest(CFMutableArrayRef *manifests, SOSManifestRef manifest) {
    if (manifest) {
        if (*manifests)
            CFArrayRemoveAllValues(*manifests);
        else
            *manifests = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        CFArrayAppendValue(*manifests, manifest);
    } else {
        CFReleaseNull(*manifests);
    }
}

static void SOSManifestMutableArrayAppendManifest(CFMutableArrayRef manifests, SOSManifestRef manifest) {
    if (manifest) {
        CFIndex count = CFArrayGetCount(manifests);
        CFIndex ixOfManifest = CFArrayGetFirstIndexOfValue(manifests, CFRangeMake(0, count), manifest);
        if (ixOfManifest != 0) {
            // If the manifest isn't at the front of the array move it there.
            // If it's not in the array, remove enough entires from the end to
            // make room to put it in the front.
            if (ixOfManifest != kCFNotFound) {
                CFArrayRemoveValueAtIndex(manifests, ixOfManifest);
            } else {
                while (count >= kSOSPeerMaxManifestWindowDepth)
                    CFArrayRemoveValueAtIndex(manifests, --count);
            }

            CFArrayInsertValueAtIndex(manifests, 0, manifest);
        }
    } else {
        // pending == NULL => nothing clear history
        CFArrayRemoveAllValues(manifests);
    }
}

static void SOSManifestArrayAppendManifest(CFMutableArrayRef *manifests, SOSManifestRef manifest) {
    if (*manifests)
        SOSManifestMutableArrayAppendManifest(*manifests, manifest);
    else
        SOSManifestArraySetManifest(manifests, manifest);
}

//
// MARK: - SOSPeer
//

struct __OpaqueSOSPeer {
    CFRuntimeBase _base;

    CFStringRef peer_id;
    CFSetRef views;
    CFIndex version;
    uint64_t sequenceNumber;
    bool mustSendMessage;
    bool sendObjects;

    bool hasBeenInSync;

    SOSManifestRef pendingObjects;
    SOSManifestRef unwantedManifest;
    SOSManifestRef confirmedManifest;
    CFMutableArrayRef proposedManifests;
    CFMutableArrayRef localManifests;

    // Only backup peers have these:
    CFDataRef _keyBag;
    FILE *journalFile;
};

CFGiblisWithCompareFor(SOSPeer)

static CFStringRef SOSPeerCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSPeerRef peer = (SOSPeerRef)cf;
    if(peer){
        CFStringRef po = SOSManifestCreateOptionalDescriptionWithLabel(peer->pendingObjects, CFSTR("O"));
        CFStringRef uo = SOSManifestCreateOptionalDescriptionWithLabel(peer->unwantedManifest, CFSTR("U"));
        CFStringRef co = SOSManifestCreateOptionalDescriptionWithLabel(peer->confirmedManifest, CFSTR("C"));
        CFStringRef pe = SOSManifestArrayCreateOptionalDescriptionWithLabel(peer->proposedManifests, CFSTR("P"));
        CFStringRef lo = SOSManifestArrayCreateOptionalDescriptionWithLabel(peer->localManifests, CFSTR("L"));
        CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("<%@ %s%s%s%@%@%@%@%@>"),
                                                    SOSPeerGetID(peer),
                                                    SOSPeerMustSendMessage(peer) ? "F" : "f",
                                                    SOSPeerSendObjects(peer) ? "S" : "s",
                                                    SOSPeerHasBeenInSync(peer) ? "K" : "k",
                                                    po, uo, co, pe, lo);
        CFReleaseSafe(lo);
        CFReleaseSafe(pe);
        CFReleaseSafe(co);
        CFReleaseSafe(uo);
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


static bool SOSPeerGetPersistedBoolean(CFDictionaryRef persisted, CFStringRef key) {
    CFBooleanRef boolean = CFDictionaryGetValue(persisted, key);
    return boolean && CFBooleanGetValue(boolean);
}

static CFDataRef SOSPeerGetPersistedData(CFDictionaryRef persisted, CFStringRef key) {
    return asData(CFDictionaryGetValue(persisted, key), NULL);
}

static int64_t SOSPeerGetPersistedInt64(CFDictionaryRef persisted, CFStringRef key) {
    int64_t integer = 0;
    CFNumberRef number = CFDictionaryGetValue(persisted, key);
    if (number) {
        CFNumberGetValue(number, kCFNumberSInt64Type, &integer);
    }
    return integer;
}

static void SOSPeerGetOptionalPersistedCFIndex(CFDictionaryRef persisted, CFStringRef key, CFIndex *value) {
    CFNumberRef number = CFDictionaryGetValue(persisted, key);
    if (number) {
        CFNumberGetValue(number, kCFNumberCFIndexType, value);
    }
}

static CFSetRef SOSPeerGetPersistedViewNameSet(SOSPeerRef peer, CFDictionaryRef persisted, CFStringRef key) {
    CFSetRef vns = CFDictionaryGetValue(persisted, key);
    if (!vns) {
        // Engine state in db contained a v0 peer, thus it must be in the V0ViewSet.
        vns = SOSViewsGetV0ViewSet();
        secnotice("peer", "%@ had no views, inferring: %@", peer->peer_id, vns);
    }
    return vns;
}

//
// MARK: Backup Peers
//

void SOSBackupPeerPostNotification(const char *reason) {
    // Let sbd know when a notable event occurs
    // - Disk full
    // - Backup bag change
    secnotice("backup", "posting notification to CloudServices: %s", reason?reason:"");
    notify_post(kSecItemBackupNotification);
}

static bool SOSPeerDoWithJournalPath(SOSPeerRef peer, CFErrorRef *error, void(^with)(const char *journalPath)) {
    // TODO: Probably switch to using CFURL to construct the path.
    bool ok = true;
    char strBuffer[PATH_MAX + 1];
    size_t userTempLen = confstr(_CS_DARWIN_USER_TEMP_DIR, strBuffer, sizeof(strBuffer));
    if (userTempLen == 0) {
        ok = SecCheckErrno(-1, error, CFSTR("confstr on _CS_DARWIN_USER_TEMP_DIR returned an error."));
    } else {
        CFStringRef journalName = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%s/SOSBackup-%@"), strBuffer, SOSPeerGetID(peer));
        CFStringPerformWithCString(journalName, with);
        CFReleaseSafe(journalName);
    }
    return ok;
}

static FILE *fopen_journal(const char *journalPath, const char *mode, CFErrorRef *error) {
    FILE *file = fopen(journalPath, mode);
    SecCheckErrno(!file, error, CFSTR("fopen %s,%s"), journalPath, mode);
    return file;
}

#include <sys/stat.h>

#if !defined(NDEBUG)
static off_t getFileSize(int fd) {
    struct stat sb;
    fstat(fd, &sb);
    return sb.st_size;
}
#endif

int SOSPeerHandoffFD(SOSPeerRef peer, CFErrorRef *error) {
    __block int fd = -1;
    SOSPeerDoWithJournalPath(peer, error, ^(const char *journalName) {
        fd = open(journalName, O_RDONLY | O_CLOEXEC);
        if (SecCheckErrno(fd < 0, error, CFSTR("open %s"), journalName)) {
            if (!SecCheckErrno(unlink(journalName), error, CFSTR("unlink %s"), journalName)) {
                close(fd);
                fd = -1;
            } else {
                secdebug("backup", "Handing off file %s with fd %d of size %llu", journalName, fd, getFileSize(fd));
            }
        } else {
            secdebug("backup", "Handing off file %s failed, %@", journalName, error?*error:NULL);
        }
    });
    return fd;
}

static CFDataRef SOSPeerCopyAKSKeyBag(SOSPeerRef peer, CFErrorRef *error) {
    if (CFEqual(peer->peer_id, kSOSViewKeychainV0_tomb)) {
        return CFRetainSafe(peer->_keyBag);
    } else {
        CFDataRef aksKeybag = NULL;
        SOSBackupSliceKeyBagRef backupSliceKeyBag = SOSBackupSliceKeyBagCreateFromData(kCFAllocatorDefault, peer->_keyBag, error);
        if (backupSliceKeyBag) {
            aksKeybag = SOSBSKBCopyAKSBag(backupSliceKeyBag, error);
            CFRelease(backupSliceKeyBag);
        }
        return aksKeybag;
    }
}

bool SOSPeerAppendToJournal(SOSPeerRef peer, CFErrorRef *error, void(^with)(FILE *journalFile, keybag_handle_t kbhandle)) {
    __block bool ok = true;
    // We only need a keybag if we are writing ADDs. Since we don't know at this layer
    // what operations we may be doing, open keybag if we have one, otherwise don't
    ok &= SOSPeerDoWithJournalPath(peer, error, ^(const char *fname) {
        FILE *file = fopen_journal(fname, "a", error);
        if (file) {
            keybag_handle_t kbhandle = bad_keybag_handle;
            CFDataRef keybag = SOSPeerCopyAKSKeyBag(peer, error);
            ok = keybag;
            if (ok && (ok = ks_open_keybag(keybag, NULL, &kbhandle, error))) {
                with(file, kbhandle);
                if (kbhandle != bad_keybag_handle)
                    ok &= ks_close_keybag(kbhandle, error);
            }
            CFReleaseSafe(keybag);
            fclose(file);
        }
    });
    return ok;
}

static bool SOSPeerTruncateJournal(SOSPeerRef peer, CFErrorRef *error, void(^with)(FILE *journalFile)) {
    __block bool ok = true;
    ok &= SOSPeerDoWithJournalPath(peer, error, ^(const char *fname) {
        FILE *file = fopen_journal(fname, "w", error);
        if (file) {
            with(file);
            fclose(file);
        }
    });
    return ok;
}

bool SOSPeerSetState(SOSPeerRef p, SOSEngineRef engine, CFDictionaryRef state, CFErrorRef *error) {
    bool ok = true;
    if (state) {
        SOSPeerGetOptionalPersistedCFIndex(state, kSOSPeerVersionKey, &p->version);

        p->sequenceNumber = SOSPeerGetPersistedInt64(state, kSOSPeerSequenceNumberKey);
        p->mustSendMessage = SOSPeerGetPersistedBoolean(state, kSOSPeerMustSendMessageKey);
        p->sendObjects = SOSPeerGetPersistedBoolean(state, kSOSPeerSendObjectsKey);
        p->hasBeenInSync = SOSPeerGetPersistedBoolean(state, kSOSPeerHasBeenInSyncKey);
        CFRetainAssign(p->views, SOSPeerGetPersistedViewNameSet(p, state, kSOSPeerViewsKey));
        SOSPeerSetKeyBag(p, SOSPeerGetPersistedData(state, kSOSPeerKeyBagKey));
        CFAssignRetained(p->pendingObjects, SOSEngineCopyPersistedManifest(engine, state, kSOSPeerPendingObjectsKey));
        CFAssignRetained(p->unwantedManifest, SOSEngineCopyPersistedManifest(engine, state, kSOSPeerUnwantedManifestKey));
        CFAssignRetained(p->confirmedManifest, SOSEngineCopyPersistedManifest(engine, state, kSOSPeerConfirmedManifestKey));
        CFAssignRetained(p->proposedManifests, SOSEngineCopyPersistedManifestArray(engine, state, kSOSPeerProposedManifestKey, error));
        ok &= p->proposedManifests != NULL;
        CFAssignRetained(p->localManifests, SOSEngineCopyPersistedManifestArray(engine, state, kSOSPeerLocalManifestKey, error));
        ok &= p->localManifests != NULL;
    }
    return ok;
}

static SOSPeerRef SOSPeerCreate_Internal(SOSEngineRef engine, CFDictionaryRef state, CFStringRef theirPeerID, CFIndex version, CFErrorRef *error) {
    SOSPeerRef p = CFTypeAllocate(SOSPeer, struct __OpaqueSOSPeer, kCFAllocatorDefault);
    p->peer_id = CFRetainSafe(theirPeerID);
    p->version = version;
    CFDictionaryRef empty = NULL;
    if (!state) {
        empty = CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        state = empty;
    }
    if (!SOSPeerSetState(p, engine, state, error)) {
        CFReleaseNull(p);
    }
    CFReleaseNull(empty);
    return p;
}

static void SOSPeerPersistBool(CFMutableDictionaryRef persist, CFStringRef key, bool value) {
    CFDictionarySetValue(persist, key, value ? kCFBooleanTrue : kCFBooleanFalse);
}

static void SOSPeerPersistInt64(CFMutableDictionaryRef persist, CFStringRef key, int64_t value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &value);
    CFDictionarySetValue(persist, key, number);
    CFReleaseSafe(number);
}

static void SOSPeerPersistCFIndex(CFMutableDictionaryRef persist, CFStringRef key, CFIndex value) {
    CFNumberRef number = CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &value);
    CFDictionarySetValue(persist, key, number);
    CFReleaseSafe(number);
}

static bool SOSPeerPersistOptionalManifest(CFMutableDictionaryRef persist, CFStringRef key, SOSManifestRef manifest, CFErrorRef *error) {
    if (!manifest)
        return true;
    CFDataRef digest = SOSManifestGetDigest(manifest, error);
    bool ok = digest;
    if (ok)
        CFDictionarySetValue(persist, key, digest);
    return ok;
}

static bool SSOSPeerPersistManifestArray(CFMutableDictionaryRef persist, CFStringRef key, CFArrayRef manifests, CFErrorRef *error) {
    CFMutableArrayRef digests = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSManifestRef manifest;
    if (manifests) CFArrayForEachC(manifests, manifest) {
        CFDataRef digest = SOSManifestGetDigest(manifest, error);
        if (!digest)
            CFReleaseNull(digests);
        if (digests) {
            CFArrayAppendValue(digests, digest);
        }
    }
    if (digests) {
        CFDictionarySetValue(persist, key, digests);
        CFRelease(digests);
    }
    return digests;
}

static void SOSPeerPersistOptionalValue(CFMutableDictionaryRef persist, CFStringRef key, CFTypeRef value) {
    if (value)
        CFDictionarySetValue(persist, key, value);
}

CFDictionaryRef SOSPeerCopyState(SOSPeerRef peer, CFErrorRef *error) {
    CFMutableDictionaryRef state = CFDictionaryCreateMutableForCFTypes(kCFAllocatorDefault);
    SOSPeerPersistInt64(state, kSOSPeerSequenceNumberKey, peer->sequenceNumber);
    if (peer->version) {
        SOSPeerPersistCFIndex(state, kSOSPeerVersionKey, peer->version);
    }

    SOSPeerPersistBool(state, kSOSPeerMustSendMessageKey, peer->mustSendMessage);
    SOSPeerPersistBool(state, kSOSPeerSendObjectsKey, peer->sendObjects);
    SOSPeerPersistBool(state, kSOSPeerHasBeenInSyncKey, peer->hasBeenInSync);
    SOSPeerPersistOptionalValue(state, kSOSPeerViewsKey, peer->views);

    CFDataRef keybag = SOSPeerGetKeyBag(peer);
    if (keybag && !CFEqual(peer->peer_id, kSOSViewKeychainV0_tomb))
        SOSPeerPersistOptionalValue(state, kSOSPeerKeyBagKey, keybag);

    if (!SOSPeerPersistOptionalManifest(state, kSOSPeerPendingObjectsKey, peer->pendingObjects, error)
        || !SOSPeerPersistOptionalManifest(state, kSOSPeerUnwantedManifestKey, peer->unwantedManifest, error)
        || !SOSPeerPersistOptionalManifest(state, kSOSPeerConfirmedManifestKey, peer->confirmedManifest, error)
        || !SSOSPeerPersistManifestArray(state, kSOSPeerProposedManifestKey, peer->proposedManifests, error)
        || !SSOSPeerPersistManifestArray(state, kSOSPeerLocalManifestKey, peer->localManifests, error)) {
        CFReleaseNull(state);
    }
    return state;
}

SOSPeerRef SOSPeerCreateWithState(SOSEngineRef engine, CFStringRef peer_id, CFDictionaryRef state, CFErrorRef *error) {
    return SOSPeerCreate_Internal(engine, state, peer_id, 0, error);
}

static void SOSPeerDestroy(CFTypeRef cf) {
    SOSPeerRef peer = (SOSPeerRef)cf;
    CFReleaseNull(peer->peer_id);
    CFReleaseNull(peer->views);
    CFReleaseNull(peer->pendingObjects);
    CFReleaseNull(peer->unwantedManifest);
    CFReleaseNull(peer->confirmedManifest);
    CFReleaseNull(peer->proposedManifests);
    CFReleaseNull(peer->localManifests);
}

bool SOSPeerDidConnect(SOSPeerRef peer) {
    SOSPeerSetMustSendMessage(peer, true);
    SOSPeerSetProposedManifest(peer, SOSPeerGetConfirmedManifest(peer));
    // TODO: Return false if nothing changed.
    return true;
}

// MARK: accessors

CFIndex SOSPeerGetVersion(SOSPeerRef peer) {
    return peer->version;
}

CFStringRef SOSPeerGetID(SOSPeerRef peer) {
    return peer->peer_id;
}

CFSetRef SOSPeerGetViewNameSet(SOSPeerRef peer) {
    return peer->views;
}

void SOSPeerSetViewNameSet(SOSPeerRef peer, CFSetRef views) {
    if (peer->views && !CFSetIsSubset(views, peer->views)) {
        SOSPeerSetHasBeenInSync(peer, false);
    }

    CFRetainAssign(peer->views, views);
}

CFDataRef SOSPeerGetKeyBag(SOSPeerRef peer) {
    return peer->_keyBag;
}

static bool SOSPeerUnlinkBackupJournal(SOSPeerRef peer, CFErrorRef *error) {
    __block bool ok = true;
    ok &= SOSPeerDoWithJournalPath(peer, error, ^(const char *journalName) {
        secnotice("backup", "%@ unlinking journal file %s", peer, journalName);
        ok &= SecCheckErrno(unlink(journalName), error, CFSTR("unlink %s"), journalName);
    });
    return ok;
}

static bool SOSPeerWriteReset(SOSPeerRef peer, CFErrorRef *error) {
    __block bool ok = true;
    __block CFErrorRef localError = NULL;
    ok &= SOSPeerTruncateJournal(peer, &localError, ^(FILE *journalFile) {
        ok = SOSBackupEventWriteReset(journalFile, peer->_keyBag, &localError);
        if (ok && !peer->_keyBag)
            ok = SOSBackupEventWriteCompleteMarker(journalFile, 999, &localError);
    });

    if (!ok) {
        secwarning("%@ failed to write reset to backup journal: %@", peer->peer_id, localError);
        CFErrorPropagate(localError, error);
    } else {
        secnotice("backup-peer", "%@ Wrote reset.", peer->peer_id);
    }

    // Forget we ever wrote anything to the journal.
    SOSPeerSetConfirmedManifest(peer, NULL);
    SOSPeerSetProposedManifest(peer, NULL);

    SOSPeerSetMustSendMessage(peer, !ok);
    return ok;
}

void SOSPeerKeyBagDidChange(SOSPeerRef peer) {
    // If !keyBag unlink the file, instead of writing a reset.
    // CloudServices does not want to hear about empty keybags
    SOSPeerSetSendObjects(peer, false);
    if (!peer->_keyBag) {
        SOSPeerUnlinkBackupJournal(peer, NULL);
    } else {
        // Attempt to write a reset (ignoring failures since it will
        // be pended stickily if it fails).
        SOSPeerWriteReset(peer, NULL);
        SOSCCRequestSyncWithBackupPeer(SOSPeerGetID(peer));
    }
}

void SOSPeerSetKeyBag(SOSPeerRef peer, CFDataRef keyBag) {
    if (CFEqualSafe(keyBag, peer->_keyBag)) return;
    bool hadKeybag = peer->_keyBag;
    if (!keyBag) {
        secwarning("%@ keybag for backup unset", SOSPeerGetID(peer));
    } else {
        secnotice("backup", "%@ backup bag: %@", SOSPeerGetID(peer), keyBag);
    }
    CFRetainAssign(peer->_keyBag, keyBag);
    // Don't call SOSPeerKeybagDidChange for the inital edge from NULL -> having a keybag.
    if (hadKeybag) {
        SOSPeerKeyBagDidChange(peer);
    }
}

bool SOSPeerWritePendingReset(SOSPeerRef peer, CFErrorRef *error) {
    return !SOSPeerMustSendMessage(peer) || SOSPeerWriteReset(peer, error);
}

uint64_t SOSPeerNextSequenceNumber(SOSPeerRef peer) {
    return ++peer->sequenceNumber;
}

uint64_t SOSPeerGetMessageVersion(SOSPeerRef peer) {
    return SOSPeerGetVersion(peer);
}

bool SOSPeerMustSendMessage(SOSPeerRef peer) {
    return peer->mustSendMessage;
}

void SOSPeerSetMustSendMessage(SOSPeerRef peer, bool sendMessage) {
    peer->mustSendMessage = sendMessage;
}

bool SOSPeerSendObjects(SOSPeerRef peer) {
    return peer->sendObjects;
}

void SOSPeerSetSendObjects(SOSPeerRef peer, bool sendObjects) {
    peer->sendObjects = sendObjects;
}

bool SOSPeerHasBeenInSync(SOSPeerRef peer) {
    return peer->hasBeenInSync;
}

void SOSPeerSetHasBeenInSync(SOSPeerRef peer, bool hasBeenInSync) {
    peer->hasBeenInSync = hasBeenInSync;
}

// MARK: Manifests

SOSManifestRef SOSPeerGetProposedManifest(SOSPeerRef peer) {
    if (peer->proposedManifests && CFArrayGetCount(peer->proposedManifests))
        return (SOSManifestRef)CFArrayGetValueAtIndex(peer->proposedManifests, 0);
    return NULL;
}

SOSManifestRef SOSPeerGetConfirmedManifest(SOSPeerRef peer) {
    return peer->confirmedManifest;
}

void SOSPeerSetConfirmedManifest(SOSPeerRef peer, SOSManifestRef confirmed) {
    CFRetainAssign(peer->confirmedManifest, confirmed);

    // TODO: Clear only expired pending and local manifests from the array - this clears them all
    // To do so we'd have to track the messageIds we sent to our peer and when we proposed a particular manifest.
    // Then we simply remove the entries from messages older than the one we are confirming now
    //CFArrayRemoveAllValues(SOSPeerGetDigestsWithKey(peer, kSOSPeerProposedManifestKey));
    //CFArrayRemoveAllValues(SOSPeerGetDigestsWithKey(peer, kSOSPeerLocalManifestKey));
}

void SOSPeerAddProposedManifest(SOSPeerRef peer, SOSManifestRef proposed) {
    SOSManifestArrayAppendManifest(&peer->proposedManifests, proposed);
}

void SOSPeerSetProposedManifest(SOSPeerRef peer, SOSManifestRef proposed) {
    SOSManifestArraySetManifest(&peer->proposedManifests, proposed);
}

void SOSPeerAddLocalManifest(SOSPeerRef peer, SOSManifestRef local) {
    SOSManifestArrayAppendManifest(&peer->localManifests, local);
}

SOSManifestRef SOSPeerGetPendingObjects(SOSPeerRef peer) {
    return peer->pendingObjects;
}

void SOSPeerSetPendingObjects(SOSPeerRef peer, SOSManifestRef pendingObjects) {
    CFRetainAssign(peer->pendingObjects, pendingObjects);
}

SOSManifestRef SOSPeerGetUnwantedManifest(SOSPeerRef peer) {
    return peer->unwantedManifest;
}

void SOSPeerSetUnwantedManifest(SOSPeerRef peer, SOSManifestRef unwantedManifest) {
    CFRetainAssign(peer->unwantedManifest, unwantedManifest);
}

SOSManifestRef SOSPeerCopyManifestForDigest(SOSPeerRef peer, CFDataRef digest) {
    if (!digest) return NULL;
    SOSManifestRef manifest;
    if (peer->proposedManifests) CFArrayForEachC(peer->proposedManifests, manifest) {
        if (CFEqual(digest, SOSManifestGetDigest(manifest, NULL)))
            return CFRetainSafe(manifest);
    }
    if (peer->localManifests) CFArrayForEachC(peer->localManifests, manifest) {
        if (CFEqual(digest, SOSManifestGetDigest(manifest, NULL)))
            return CFRetainSafe(manifest);
    }
    if (peer->confirmedManifest && CFEqual(digest, SOSManifestGetDigest(peer->confirmedManifest, NULL)))
        return CFRetainSafe(peer->confirmedManifest);

    return NULL;
}

static void SOSMarkManifestInUse(struct SOSDigestVector *mdInUse, SOSManifestRef manifest) {
    CFDataRef digest = SOSManifestGetDigest(manifest, NULL);
    if (digest)
        SOSDigestVectorAppend(mdInUse, CFDataGetBytePtr(digest));
}

static void SOSMarkManifestsInUse(struct SOSDigestVector *mdInUse, CFArrayRef manifests) {
    if (!isArray(manifests)) return;
    SOSManifestRef manifest = NULL;
    CFArrayForEachC(manifests, manifest) {
        SOSMarkManifestInUse(mdInUse, manifest);
    }
}

// Add all digests we are using to mdInUse
void SOSPeerMarkDigestsInUse(SOSPeerRef peer, struct SOSDigestVector *mdInUse) {
    SOSMarkManifestInUse(mdInUse, peer->pendingObjects);
    SOSMarkManifestInUse(mdInUse, peer->unwantedManifest);
    SOSMarkManifestInUse(mdInUse, peer->confirmedManifest);
    SOSMarkManifestsInUse(mdInUse, peer->localManifests);
    SOSMarkManifestsInUse(mdInUse, peer->proposedManifests);
}

static void SOSAddManifestInUse(CFMutableDictionaryRef mfc, SOSManifestRef manifest) {
    CFDataRef digest = SOSManifestGetDigest(manifest, NULL);
    CFDataRef data = SOSManifestGetData(manifest);
    if (digest && data)
        CFDictionarySetValue(mfc, digest, data);
}

static void SOSAddManifestsInUse(CFMutableDictionaryRef mfc, CFArrayRef manifests) {
    if (!isArray(manifests)) return;
    SOSManifestRef manifest = NULL;
    CFArrayForEachC(manifests, manifest) {
        SOSAddManifestInUse(mfc, manifest);
    }
}

void SOSPeerAddManifestsInUse(SOSPeerRef peer, CFMutableDictionaryRef mfc) {
    SOSAddManifestInUse(mfc, peer->pendingObjects);
    SOSAddManifestInUse(mfc, peer->unwantedManifest);
    SOSAddManifestInUse(mfc, peer->confirmedManifest);
    SOSAddManifestsInUse(mfc, peer->localManifests);
    SOSAddManifestsInUse(mfc, peer->proposedManifests);

}

// absentFromRemote
// AbsentLocally
// additionsFromRemote
// original intent was that digests only got added to pendingObjects. We only know for sure if it is something added locally via api call


bool SOSPeerDidReceiveRemovalsAndAdditions(SOSPeerRef peer, SOSManifestRef absentFromRemote, SOSManifestRef additionsFromRemote, SOSManifestRef unwantedFromRemote,
                                           SOSManifestRef local, CFErrorRef *error) {
    // We assume that incoming manifests are all sorted, and absentFromRemote is disjoint from additionsFromRemote
    bool ok = true;
    SOSManifestRef remoteMissing = NULL, sharedRemovals = NULL, sharedAdditions = NULL;

    // TODO: Simplify -- a lot.
    ok = ok && (remoteMissing = SOSManifestCreateIntersection(absentFromRemote, local, error));           // remoteMissing = absentFromRemote <Intersected> local
    ok = ok && (sharedRemovals = SOSManifestCreateComplement(remoteMissing, absentFromRemote, error));    // sharedRemovals = absentFromRemote - remoteMissing
    ok = ok && (sharedAdditions = SOSManifestCreateIntersection(additionsFromRemote, local, error));         // sharedAdditions = additionsFromRemote <Intersected> local
    //ok = ok && (remoteAdditions = SOSManifestCreateComplement(sharedAdditions, additionsFromRemote, error)); // remoteAdditions = additionsFromRemote - sharedAdditions

    // remoteMissing are things we have that remote has asked for => add to pendingObjects
    // sharedRemovals are things we don't have that remote has asked for => remove from pendingDeletes
    // sharedAdditions are things we have that remote has too => remove from pendingObjects
    // remoteAdditions are things that remote said they have that we don't and we should probably ask for => add to pendingDeletes?
    // unwantedFromRemote are things we received from remote for which we already have a newer object according to the conflict resolver.
    secnotice("peer", "%@ RM:%@ SR:%@ SA:%@ UR:%@", peer, remoteMissing, sharedRemovals, sharedAdditions, unwantedFromRemote);

    SOSManifestRef pendingObjectsManifest = SOSManifestCreateWithPatch(peer->pendingObjects, sharedAdditions, remoteMissing, error);
    SOSManifestRef unwantedManifest = SOSManifestCreateWithPatch(peer->unwantedManifest, sharedRemovals,  unwantedFromRemote, error);
    CFAssignRetained(peer->pendingObjects, pendingObjectsManifest); // PO = PO - sharedAdditions + remoteMissing
    CFAssignRetained(peer->unwantedManifest, unwantedManifest); // U = U - sharedRemovals + unwantedFromRemote

    CFReleaseSafe(remoteMissing);
    CFReleaseSafe(sharedRemovals);
    CFReleaseSafe(sharedAdditions);

    secnotice("peer", "%@ C:%@ U:%@ O:%@", peer, SOSPeerGetConfirmedManifest(peer), SOSPeerGetUnwantedManifest(peer), SOSPeerGetPendingObjects(peer));

    return ok;
}

// Called for a normal syncing peer.  Only updates pendingObjects currently.
bool SOSPeerDataSourceWillCommit(SOSPeerRef peer, SOSDataSourceTransactionSource source, SOSManifestRef removals, SOSManifestRef additions, CFErrorRef *error) {
    bool isAPITransaction = source == kSOSDataSourceAPITransaction;
    SOSManifestRef unconfirmedAdditions = NULL;
    if (isAPITransaction && SOSManifestGetCount(additions)) {
        // Remove confirmed from additions, leaving us with additions to the local db that the remote peer doesn't have yet
        unconfirmedAdditions = SOSManifestCreateComplement(SOSPeerGetConfirmedManifest(peer), additions, error);
    }

    if (SOSManifestGetCount(removals) || SOSManifestGetCount(unconfirmedAdditions)) {
        SOSManifestRef pendingObjectsManifest = SOSManifestCreateWithPatch(peer->pendingObjects, removals, unconfirmedAdditions, error);

//#if DEBUG
        // TODO: Only do this if debugScope "peer", notice is enabled.
        // if (!SecIsScopeActive(kSecLevelNotice, "peer"))
        {
            // pended == UA unless the db is renotifying of an addition for something we already have
            SOSManifestRef unpended = NULL, pended = NULL;
            SOSManifestDiff(peer->pendingObjects, pendingObjectsManifest, &unpended, &pended, error);
            secinfo("peer", "%@: willCommit R:%@ A:%@ UA:%@ %s O%s%@%s%@",
                       SOSPeerGetID(peer), removals, additions, unconfirmedAdditions,
                       (isAPITransaction ? "api": "sos"),
                       (SOSManifestGetCount(unpended) ? "-" : ""),
                       (SOSManifestGetCount(unpended) ? (CFStringRef)unpended : CFSTR("")),
                       (SOSManifestGetCount(pended) ? "+" : SOSManifestGetCount(unpended) ? "" : "="),
                       (SOSManifestGetCount(pended) ? (CFStringRef)pended : CFSTR("")));
            CFReleaseSafe(unpended);
            CFReleaseSafe(pended);
        }
//#endif /* DEBUG */
        CFAssignRetained(peer->pendingObjects, pendingObjectsManifest);
    }
    CFReleaseSafe(unconfirmedAdditions);

    return true;
}

bool SOSPeerWriteAddEvent(FILE *journalFile, keybag_handle_t kbhandle, SOSDataSourceRef dataSource, SOSObjectRef object, CFErrorRef *error) {
    CFDictionaryRef backup_item = NULL;
    bool ok = ((backup_item = SOSObjectCopyBackup(dataSource, object, kbhandle, error))
               && SOSBackupEventWriteAdd(journalFile, backup_item, error));
    CFReleaseSafe(backup_item);
    return ok;
}

// Called for a backup peer, should stream objects in changes right away
bool SOSPeerDataSourceWillChange(SOSPeerRef peer, SOSDataSourceRef dataSource, SOSDataSourceTransactionSource source, CFArrayRef changes, CFErrorRef *error) {
    __block bool ok = true;
    ok &= SOSPeerWritePendingReset(peer, error) && SOSPeerAppendToJournal(peer, error, ^(FILE *journalFile, keybag_handle_t kbhandle) {
        struct SOSDigestVector dvdel = SOSDigestVectorInit;
        struct SOSDigestVector dvadd = SOSDigestVectorInit;
        SOSChangeRef change;
        CFArrayForEachC(changes, change) {
            bool isDelete = false;
            CFErrorRef localError = NULL;
            CFDataRef digest = NULL;
            SOSObjectRef object = NULL;
            bool ok = digest = SOSChangeCopyDigest(dataSource, change, &isDelete, &object, &localError);
            if (isDelete) {
                ok &= SOSBackupEventWriteDelete(journalFile, digest, &localError);
                SOSDigestVectorAppend(&dvdel, CFDataGetBytePtr(digest));
            } else {
                ok &= SOSPeerWriteAddEvent(journalFile, kbhandle, dataSource, object, &localError);
                SOSDigestVectorAppend(&dvadd, CFDataGetBytePtr(digest));
            }
            if (!ok) {
                secerror("bad change %@: %@", change, localError);
            }
            CFReleaseSafe(digest);
            CFReleaseSafe(localError);
        };

        if (ok) {
            // Update our proposed manifest since we just wrote stuff
            struct SOSDigestVector dvresult = SOSDigestVectorInit;
            SOSDigestVectorSort(&dvdel);
            SOSDigestVectorSort(&dvadd);
            if ((ok = SOSDigestVectorPatchSorted(SOSManifestGetDigestVector(SOSPeerGetProposedManifest(peer)), &dvdel,
                                                 &dvadd, &dvresult, error))) {
                SOSManifestRef proposed;
                ok = proposed = SOSManifestCreateWithDigestVector(&dvresult, error);
                SOSPeerSetProposedManifest(peer, proposed);
                CFReleaseSafe(proposed);
            }
            SOSDigestVectorFree(&dvresult);
        }
        SOSDigestVectorFree(&dvdel);
        SOSDigestVectorFree(&dvadd);

        // Only Write marker if we are actually in sync now (local == propopsed).
        if (SOSPeerSendObjects(peer))
            SOSBackupEventWriteCompleteMarker(journalFile, 799, error);
    });

    if (!ok) {
        // We were unable to stream everything out neatly
        SOSCCRequestSyncWithBackupPeer(SOSPeerGetID(peer));
    }
    return ok;
}
