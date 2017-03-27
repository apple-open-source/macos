/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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
 * SOSChangeTracker.c -  Implementation of a manifest caching change tracker that forwards changes to children
 */

#include <Security/SecureObjectSync/SOSChangeTracker.h>
#include <Security/SecureObjectSync/SOSDigestVector.h>
#include <Security/SecureObjectSync/SOSEnginePriv.h>
#include <Security/SecureObjectSync/SOSManifest.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

CFStringRef SOSChangeCopyDescription(SOSChangeRef change) {
    CFTypeRef object = NULL;
    bool isAdd = SOSChangeGetObject(change, &object);
    // TODO: Print objects or digests
    return (isData(object)
            ? isAdd ? CFSTR("a") : CFSTR("d")
            : isAdd ? CFSTR("A") : CFSTR("D"));
}

CFDataRef SOSChangeCopyDigest(SOSDataSourceRef dataSource, SOSChangeRef change, bool *isDel, SOSObjectRef *object, CFErrorRef *error) {
    CFDataRef digest = NULL;
    if (isArray(change)) {
        if (CFArrayGetCount(change) != 1) {
            SecError(errSecDecode, error, CFSTR("change array count: %ld"), CFArrayGetCount(change));
            return NULL;
        }
        change = CFArrayGetValueAtIndex(change, 0);
        *isDel = true;
    } else {
        *isDel = false;
    }

    // If the change is a CFData, this is the signal that it is a delete
    if (isData(change)) {
        digest = (CFDataRef)CFRetain(change);
    } else {
        digest = SOSObjectCopyDigest(dataSource, (SOSObjectRef)change, error);
        *object = (SOSObjectRef)change;
    }
    assert(digest && CFDataGetLength(digest) == CCSHA1_OUTPUT_SIZE);
    return digest;
}

CFStringRef SOSChangesCopyDescription(CFArrayRef changes) {
    CFMutableStringRef desc = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, CFSTR("("));
    CFTypeRef change;
    if (changes) CFArrayForEachC(changes, change) {
        CFStringRef changeDesc = SOSChangeCopyDescription(change);
        CFStringAppend(desc, changeDesc);
        CFReleaseNull(changeDesc);
    }
    CFStringAppend(desc, CFSTR(")"));
    return desc;
}


/* SOSChangeTracker implementation. */
struct __OpaqueSOSChangeTracker {
    CFRuntimeBase _base;
    SOSManifestRef manifest;                // Optional: Only concrete cts have a manifest
    CFMutableArrayRef changeChildren;       // Optional: cts can have children
    CFMutableArrayRef manifestChildren;     // Optional: cts can have children
};

static CFStringRef SOSChangeTrackerCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SOSChangeTrackerRef ct = (SOSChangeTrackerRef)cf;
    CFStringRef desc = CFStringCreateWithFormat(kCFAllocatorDefault, formatOptions, CFSTR("<ChangeTracker %@ children %ld/%ld>"),
                                                ct->manifest ? ct->manifest : (SOSManifestRef)CFSTR("NonConcrete"),
                                                CFArrayGetCount(ct->changeChildren), CFArrayGetCount(ct->manifestChildren));
    return desc;
}

static void SOSChangeTrackerDestroy(CFTypeRef cf) {
    SOSChangeTrackerRef ct = (SOSChangeTrackerRef)cf;
    CFReleaseSafe(ct->manifest);
    CFReleaseSafe(ct->changeChildren);
    CFReleaseSafe(ct->manifestChildren);
}

// Even though SOSChangeTracker instances are used as keys in dictionaries, they are treated as pointers when used as such
// which is fine since the engine ensures instances are singletons.
CFGiblisFor(SOSChangeTracker);

SOSChangeTrackerRef SOSChangeTrackerCreate(CFAllocatorRef allocator, bool isConcrete, CFArrayRef changeChildren, CFErrorRef *error) {
    SOSChangeTrackerRef ct = NULL;
    ct = CFTypeAllocate(SOSChangeTracker, struct __OpaqueSOSChangeTracker, allocator);
    if (ct && isConcrete) {
        ct->manifest = SOSManifestCreateWithData(NULL, error);
        if (!ct->manifest)
            CFReleaseNull(ct);
    }
    if (ct) {
        if (changeChildren)
            ct->changeChildren = CFArrayCreateMutableCopy(kCFAllocatorDefault, 0, changeChildren);
        else
            ct->changeChildren = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
        ct->manifestChildren = CFArrayCreateMutableForCFTypes(kCFAllocatorDefault);
    }

    return ct;
}

// Change the concreteness of the current ct (a non concrete ct does not support SOSChangeTrackerCopyManifest().
void SOSChangeTrackerSetConcrete(SOSChangeTrackerRef ct, bool isConcrete) {
    if (!isConcrete)
        CFReleaseNull(ct->manifest);
    else if (!ct->manifest) {
        ct->manifest = SOSManifestCreateWithData(NULL, NULL);
    }
}

// Add a child to the current ct
void SOSChangeTrackerRegisterChangeUpdate(SOSChangeTrackerRef ct, SOSChangeTrackerUpdatesChanges child) {
    CFArrayAppendValue(ct->changeChildren, child);
}

void SOSChangeTrackerRegisterManifestUpdate(SOSChangeTrackerRef ct, SOSChangeTrackerUpdatesManifests child) {
    CFArrayAppendValue(ct->manifestChildren, child);
}

void SOSChangeTrackerResetRegistration(SOSChangeTrackerRef ct) {
    CFArrayRemoveAllValues(ct->changeChildren);
    CFArrayRemoveAllValues(ct->manifestChildren);
}

void SOSChangeTrackerSetManifest(SOSChangeTrackerRef ct, SOSManifestRef manifest) {
    CFRetainAssign(ct->manifest, manifest);
}

SOSManifestRef SOSChangeTrackerCopyManifest(SOSChangeTrackerRef ct, CFErrorRef *error) {
    if (ct->manifest) {
        return (SOSManifestRef)CFRetain(ct->manifest);
    }
    SOSErrorCreate(kSOSErrorNotConcreteError, error, NULL, CFSTR("ChangeTracker is not concrete"));
    return NULL;
}

static bool SOSChangeTrackerCreateManifestsWithChanges(SOSEngineRef engine, CFArrayRef changes, SOSManifestRef *removals, SOSManifestRef *additions, CFErrorRef *error) {
    bool ok = true;
    struct SOSDigestVector dvdels = SOSDigestVectorInit;
    struct SOSDigestVector dvadds = SOSDigestVectorInit;
    struct SOSDigestVector *dv;
    CFTypeRef change;
    CFArrayForEachC(changes, change) {
        CFDataRef digest, allocatedDigest = NULL;
        if (isArray(change)) {
            assert(CFArrayGetCount(change) == 1);
            change = CFArrayGetValueAtIndex(change, 0);
            dv = &dvdels;
        } else {
            dv = &dvadds;
        }

        if (isData(change)) {
            digest = (CFDataRef)change;
        } else {
            CFErrorRef digestError = NULL;
            digest = allocatedDigest = SOSObjectCopyDigest(SOSEngineGetDataSource(engine), (SOSObjectRef)change, &digestError);
            if (!digest) {
                secerror("change %@ SOSObjectCopyDigest: %@", change, digestError);
                CFReleaseNull(digestError);
                continue;
            }
        }

        if (CFDataGetLength(digest) == 20) {
            SOSDigestVectorAppend(dv, CFDataGetBytePtr(digest));
        } else {
            secerror("change %@ bad length digest: %@", change, digest);
        }
        CFReleaseNull(allocatedDigest);
    }
    if (ok && removals)
        ok = *removals = SOSManifestCreateWithDigestVector(&dvdels, error);
    if (ok && additions)
        ok = *additions = SOSManifestCreateWithDigestVector(&dvadds, error);

    SOSDigestVectorFree(&dvadds);
    SOSDigestVectorFree(&dvdels);

    return ok;
}

bool SOSChangeTrackerTrackChanges(SOSChangeTrackerRef ct, SOSEngineRef engine, SOSTransactionRef txn, SOSDataSourceTransactionSource source, SOSDataSourceTransactionPhase phase, CFArrayRef changes, CFErrorRef *error) {
    bool ok = true;
    if (changes && CFArrayGetCount(changes)) {
        CFStringRef changesDesc = SOSChangesCopyDescription(changes);
        secnotice("tracker", "%@ %s %s changes: %@", ct, phase == kSOSDataSourceTransactionWillCommit ? "will-commit" : phase == kSOSDataSourceTransactionDidCommit ? "did-commit" : "did-rollback", source == kSOSDataSourceSOSTransaction ? "sos" : "api", changesDesc);
        CFReleaseSafe(changesDesc);
        if (ct->manifest || ct->manifestChildren) {
            SOSManifestRef additions = NULL;
            SOSManifestRef removals = NULL;
            ok &= SOSChangeTrackerCreateManifestsWithChanges(engine, changes, &removals, &additions, error);
            if (ok) {
                if (ct->manifest) {
                    SOSManifestRef updatedManifest = SOSManifestCreateWithPatch(ct->manifest, removals, additions, error);
                    if (updatedManifest){
                        CFTransferRetained(ct->manifest, updatedManifest);
                    }
                }
                if (ct->manifestChildren) {
                    SOSChangeTrackerUpdatesManifests child;
                    CFArrayForEachC(ct->manifestChildren, child) {
                        ok = ok && child(ct, engine, txn, source, phase, removals, additions, error);
                    }
                }
            }
            CFReleaseSafe(removals);
            CFReleaseSafe(additions);
            // TODO: Potentially filter changes to eliminate any changes that were already in our manifest
            // Backup Peers and the like would probably enjoy this so they don't have to do it themselves.
        }

        if (ct->changeChildren) {
            SOSChangeTrackerUpdatesChanges child;
            CFArrayForEachC(ct->changeChildren, child) {
                ok = ok && child(ct, engine, txn, source, phase, changes, error);
            }
        }
    }
    
    return ok;
}

