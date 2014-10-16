/*
 * Copyright (c) 2013-2014 Apple Inc. All Rights Reserved.
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
 * SOSManifest.c -  Manifest object and some manipulation around it
 */

#include <SecureObjectSync/SOSManifest.h>
#include <SecureObjectSync/SOSDigestVector.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

#include <securityd/SecDbKeychainItem.h> // for kc_copy_sha1, which needs to move to utilities

CFStringRef kSOSManifestErrorDomain = CFSTR("com.apple.security.sos.manifest.error");

/* SOSManifest implementation. */
struct __OpaqueSOSManifest {
    CFRuntimeBase _base;
    CFDataRef digest;
    CFDataRef digestVector;
    struct SOSDigestVector dv;
};

CFGiblisWithCompareFor(SOSManifest)

static void SOSManifestDestroy(CFTypeRef cf) {
    SOSManifestRef mf = (SOSManifestRef)cf;
    CFReleaseSafe(mf->digest);
    CFReleaseSafe(mf->digestVector);
}

static Boolean SOSManifestCompare(CFTypeRef cf1, CFTypeRef cf2) {
    SOSManifestRef mf1 = (SOSManifestRef)cf1;
    SOSManifestRef mf2 = (SOSManifestRef)cf2;
    return CFEqualSafe(SOSManifestGetDigest(mf1, NULL), SOSManifestGetDigest(mf2, NULL));
}

SOSManifestRef SOSManifestCreateWithData(CFDataRef data, CFErrorRef *error)
{
    SOSManifestRef manifest = CFTypeAllocate(SOSManifest, struct __OpaqueSOSManifest, kCFAllocatorDefault);
    if (!manifest)
        SecCFCreateErrorWithFormat(kSOSManifestCreateError, kSOSManifestErrorDomain, NULL, error, NULL, CFSTR("Failed to create manifest"));
    else if (data)
        manifest->digestVector = CFDataCreateCopy(kCFAllocatorDefault, data);
    else
        manifest->digestVector = CFDataCreate(kCFAllocatorDefault, NULL, 0);

    assert(!manifest || manifest->digestVector != NULL);
    return manifest;
}

SOSManifestRef SOSManifestCreateWithBytes(const uint8_t *bytes, size_t len,
                                          CFErrorRef *error) {
    CFDataRef data = CFDataCreate(kCFAllocatorDefault, bytes, (CFIndex)len);
    SOSManifestRef manifest = SOSManifestCreateWithData(data, error);
    CFReleaseSafe(data);
    return manifest;
}

size_t SOSManifestGetSize(SOSManifestRef m) {
    return m ? (size_t)CFDataGetLength(m->digestVector) : 0;
}

size_t SOSManifestGetCount(SOSManifestRef m) {
    return m ? SOSManifestGetSize(m) / SOSDigestSize : 0;
}

const uint8_t *SOSManifestGetBytePtr(SOSManifestRef m) {
    return m ? CFDataGetBytePtr(m->digestVector) : NULL;
}

CFDataRef SOSManifestGetData(SOSManifestRef m) {
    return m ? m->digestVector : NULL;
}

const struct SOSDigestVector *SOSManifestGetDigestVector(SOSManifestRef manifest) {
    if (!manifest) {
        static struct SOSDigestVector nulldv = SOSDigestVectorInit;
        return &nulldv;
    }
    manifest->dv.capacity = manifest->dv.count = SOSManifestGetCount(manifest);
    manifest->dv.digest = (void *)SOSManifestGetBytePtr(manifest);
    manifest->dv.unsorted = false;
    return &manifest->dv;
}

bool SOSManifestDiff(SOSManifestRef a, SOSManifestRef b,
                     SOSManifestRef *a_minus_b, SOSManifestRef *b_minus_a,
                     CFErrorRef *error) {
    bool result = true;
    struct SOSDigestVector dvab = SOSDigestVectorInit, dvba = SOSDigestVectorInit;
    SOSDigestVectorDiffSorted(SOSManifestGetDigestVector(a), SOSManifestGetDigestVector(b), &dvab, &dvba);
    if (a_minus_b) {
        *a_minus_b = SOSManifestCreateWithDigestVector(&dvab, error);
        if (!*a_minus_b)
            result = false;
    }
    if (b_minus_a) {
        *b_minus_a = SOSManifestCreateWithDigestVector(&dvba, error);
        if (!*b_minus_a)
            result = false;
    }
    SOSDigestVectorFree(&dvab);
    SOSDigestVectorFree(&dvba);
    return result;
}


SOSManifestRef SOSManifestCreateWithDigestVector(struct SOSDigestVector *dv, CFErrorRef *error) {
    if (!dv) return NULL;
    if (dv->unsorted) SOSDigestVectorSort(dv);
    return SOSManifestCreateWithBytes((const uint8_t *)dv->digest, dv->count * SOSDigestSize, error);
}

SOSManifestRef SOSManifestCreateWithPatch(SOSManifestRef base,
                                          SOSManifestRef removals,
                                          SOSManifestRef additions,
                                          CFErrorRef *error) {
    struct SOSDigestVector dvresult = SOSDigestVectorInit;
    SOSManifestRef result;
    if (SOSDigestVectorPatchSorted(SOSManifestGetDigestVector(base), SOSManifestGetDigestVector(removals),
                             SOSManifestGetDigestVector(additions), &dvresult, error)) {
        result = SOSManifestCreateWithDigestVector(&dvresult, error);
    } else {
        result = NULL;
    }
    SOSDigestVectorFree(&dvresult);
    return result;
}

// This is the set of elements in m2, but not in m1.
SOSManifestRef SOSManifestCreateComplement(SOSManifestRef m1,
                                           SOSManifestRef m2,
                                           CFErrorRef *error) {
    // m2 \ emptySet => m2
    if (SOSManifestGetCount(m1) == 0)
        return CFRetainSafe(m2);

    struct SOSDigestVector dvresult = SOSDigestVectorInit;
    SOSManifestRef result;
    SOSDigestVectorComplementSorted(SOSManifestGetDigestVector(m1), SOSManifestGetDigestVector(m2), &dvresult);
    result = SOSManifestCreateWithDigestVector(&dvresult, error);
    SOSDigestVectorFree(&dvresult);
    return result;
}

SOSManifestRef SOSManifestCreateIntersection(SOSManifestRef m1,
                                             SOSManifestRef m2,
                                             CFErrorRef *error) {
    struct SOSDigestVector dvresult = SOSDigestVectorInit;
    SOSManifestRef result;
    SOSDigestVectorIntersectSorted(SOSManifestGetDigestVector(m1), SOSManifestGetDigestVector(m2), &dvresult);
    result = SOSManifestCreateWithDigestVector(&dvresult, error);
    SOSDigestVectorFree(&dvresult);
    return result;
}

SOSManifestRef SOSManifestCreateUnion(SOSManifestRef m1,
                                      SOSManifestRef m2,
                                      CFErrorRef *error) {
    struct SOSDigestVector dvresult = SOSDigestVectorInit;
    SOSManifestRef result;
    SOSDigestVectorUnionSorted(SOSManifestGetDigestVector(m1), SOSManifestGetDigestVector(m2), &dvresult);
    result = SOSManifestCreateWithDigestVector(&dvresult, error);
    SOSDigestVectorFree(&dvresult);
    return result;
}

void SOSManifestForEach(SOSManifestRef m, void(^block)(CFDataRef e, bool *stop)) {
    CFDataRef e;
    const uint8_t *p, *q;
    bool stop = false;
    for (p = SOSManifestGetBytePtr(m), q = p + SOSManifestGetSize(m);
         !stop && p + SOSDigestSize <= q; p += SOSDigestSize) {
        e = CFDataCreateWithBytesNoCopy(0, p, SOSDigestSize, kCFAllocatorNull);
        if (e) {
            block(e, &stop);
            CFRelease(e);
        }
    }
}

CFDataRef SOSManifestGetDigest(SOSManifestRef m, CFErrorRef *error) {
    if (!m) return NULL;
    if (!m->digest)
        m->digest = kc_copy_sha1(SOSManifestGetSize(m), SOSManifestGetBytePtr(m), error);
    return m->digest;
}

static CFStringRef SOSManifestCopyDescription(CFTypeRef cf) {
    SOSManifestRef m = (SOSManifestRef)cf;
    CFMutableStringRef desc = CFStringCreateMutable(0, 0);
    CFStringAppendFormat(desc, NULL, CFSTR("<[%zu]"), SOSManifestGetCount(m));
    __block size_t maxEntries = 8;
    SOSManifestForEach(m, ^(CFDataRef e, bool *stop) {
        const uint8_t *d = CFDataGetBytePtr(e);
        CFStringAppendFormat(desc, NULL, CFSTR(" %02X%02X%02X%02X"), d[0], d[1], d[2], d[3]);
        if (!--maxEntries) {
            CFStringAppend(desc, CFSTR("..."));
            *stop = true;
        }
    });
    CFStringAppend(desc, CFSTR(">"));

    return desc;
}
