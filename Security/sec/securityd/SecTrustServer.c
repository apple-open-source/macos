/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
 *
 * SecTrustServer.c - certificate trust evaluation engine
 *
 *  Created by Michael Brouwer on 12/12/08.
 *
 */

#include <securityd/SecTrustServer.h>
#include <securityd/SecPolicyServer.h>
#include <securityd/SecTrustStoreServer.h>
#include <securityd/SecCAIssuerRequest.h>

#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePath.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyInternal.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFSet.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFArray.h>
#include <CoreFoundation/CFPropertyList.h>
#include <AssertMacros.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <MacErrors.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <security_utilities/debugging.h>
#include <Security/SecInternal.h>
#include "securityd_client.h"
#include "securityd_server.h"

const struct digest_to_ix_t *
digest_to_anchor_ix (register const char *str, register unsigned int len);
const struct subject_to_ix_t *
subject_to_anchor_ix (register const char *str, register unsigned int len);
const struct ev_oids *
ev_oid (register const char *str, register unsigned int len);

#ifndef SECITEM_SHIM_OSX
#include "evroots.h"
#endif

#define MAX_CHAIN_LENGTH  15

/* Forward declaration for use in SecCertificateSource. */
static void SecPathBuilderExtendPaths(void *context, CFArrayRef parents);


#pragma mark -
#pragma mark SecCertificateSource
/********************************************************
 ************ SecCertificateSource object ***************
 ********************************************************/

typedef struct SecCertificateSource *SecCertificateSourceRef;
typedef void(*SecCertificateSourceParents)(void *, CFArrayRef);
typedef bool(*CopyParents)(SecCertificateSourceRef source,
	SecCertificateRef certificate, void *context, SecCertificateSourceParents);
typedef bool(*Contains)(SecCertificateSourceRef source,
	SecCertificateRef certificate);

struct SecCertificateSource {
	CopyParents		copyParents;
	Contains		contains;
};

static bool SecCertificateSourceCopyParents(SecCertificateSourceRef source,
    SecCertificateRef certificate,
    void *context, SecCertificateSourceParents callback) {
    return source->copyParents(source, certificate, context, callback);
}

static bool SecCertificateSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
	return source->contains(source, certificate);
}

#pragma mark -
#pragma mark SecItemCertificateSource
/********************************************************
 *********** SecItemCertificateSource object ************
 ********************************************************/
static bool SecItemCertificateSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    /* FIXME: Search for things other than just subject of our issuer if we
       have a subjectID or authorityKeyIdentifier. */
    CFDataRef normalizedIssuer =
        SecCertificateGetNormalizedIssuerContent(certificate);
    const void *keys[] = {
        kSecClass,
        kSecReturnRef,
        kSecMatchLimit,
        kSecAttrSubject
    },
    *values[] = {
        kSecClassCertificate,
        kCFBooleanTrue,
        kSecMatchLimitAll,
        normalizedIssuer
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4,
		NULL, NULL);
    CFTypeRef results = NULL;
    /* We can make this async or run this on a queue now easily. */
    OSStatus status = SecItemCopyMatching(query, &results);
    CFRelease(query);
    if (status) {
		secdebug("trust", "SecItemCopyMatching status: %lu", status);
    }
    callback(context, results);
    CFReleaseSafe(results);
    return true;
}

static bool SecItemCertificateSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
    /* Lookup a certificate by issuer and serial number. */
    CFDataRef normalizedSubject =
        SecCertificateGetNormalizedSubjectContent(certificate);
    CFDataRef serialNumber =
        SecCertificateCopySerialNumber(certificate);
    const void *keys[] = {
        kSecClass,
        kSecReturnRef,
        kSecMatchLimit,
        kSecAttrIssuer,
		kSecAttrSerialNumber
    },
    *values[] = {
        kSecClassCertificate,
        kCFBooleanTrue,
        kSecMatchLimitOne,
        normalizedSubject,
		serialNumber
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 5,
        NULL, NULL);
    OSStatus status = SecItemCopyMatching(query, NULL);
    CFRelease(query);
    CFRelease(serialNumber);

    if (status) {
		if (status != errSecItemNotFound) {
			secdebug("trust", "SecItemCopyMatching returned %d", status);
		}
		return false;
    }
    return true;
}

struct SecCertificateSource kSecItemCertificateSource = {
	SecItemCertificateSourceCopyParents,
	SecItemCertificateSourceContains
};

#if 0
#pragma mark -
#pragma mark SecSystemAnchorSource
/********************************************************
 *********** SecSystemAnchorSource object ************
 ********************************************************/
struct SecSystemAnchorSource {
	struct SecCertificateSource base;
	CFSetRef digests;
};
typedef struct SecSystemAnchorSource *SecSystemAnchorSourceRef;

/* One time init data. */
static pthread_once_t kSecSystemAnchorSourceInit = PTHREAD_ONCE_INIT;
static SecCertificateSourceRef kSecSystemAnchorSource = NULL;

static bool SecSystemAnchorSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    callback(context, NULL);
    return true;
}

static bool SecSystemAnchorSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
	SecSystemAnchorSourceRef sasource = (SecSystemAnchorSourceRef)source;
	CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
	return CFSetContainsValue(sasource->digests, digest);
}

static void SecSystemAnchorSourceInit(void) {
	SecSystemAnchorSourceRef result = (SecSystemAnchorSourceRef)
		malloc(sizeof(*result));
	result->base.copyParents = SecSystemAnchorSourceCopyParents;
	result->base.contains = SecSystemAnchorSourceContains;

	CFDataRef xmlData = SecFrameworkCopyResourceContents(
		CFSTR("SystemAnchors"), CFSTR("plist"), NULL);
	CFPropertyListRef plist = CFPropertyListCreateFromXMLData(
		kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL);
	if (plist) {
		if (CFGetTypeID(plist) == CFDictionaryGetTypeID()) {
			result->digests = (CFSetRef)plist;
		} else {
			secwarning("SystemAnchors plist is wrong type.");
			CFRelease(plist);
		}
	}

	if (!result->digests) {
		result->digests = CFSetCreate(kCFAllocatorDefault, NULL, 0,
			&kCFTypeSetCallBacks);
	}

	kSecSystemAnchorSource = (SecCertificateSourceRef)result;
}

static SecCertificateSourceRef SecSystemAnchorSourceGetDefault(void) {
    pthread_once(&kSecSystemAnchorSourceInit, SecSystemAnchorSourceInit);
    return kSecSystemAnchorSource;
}
#else
#pragma mark -
#pragma mark SecSystemAnchorSource
/********************************************************
 *********** SecSystemAnchorSource object ************
 ********************************************************/
static bool SecSystemAnchorSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
#ifndef SECITEM_SHIM_OSX
    CFMutableArrayRef parents = NULL;
    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
       It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */
    const struct subject_to_ix_t *i2x =
        subject_to_anchor_ix((const char *)CFDataGetBytePtr(nic),
            (unsigned int)CFDataGetLength(nic));
    require_quiet(i2x, errOut);
    int anchor_ix = i2x->anchor_ix;
    CFIndex capacity = 0;
    do {
        ++capacity;
    } while ((anchor_ix = anchorslist[anchor_ix].next_same_subject));

    parents = CFArrayCreateMutable(kCFAllocatorDefault, capacity,
        &kCFTypeArrayCallBacks);
    anchor_ix = i2x->anchor_ix;
    do {
        const void *anchor = NULL;
        CFDataRef anchor_data = NULL;

        require_quiet(anchor_data = CFDataCreateWithBytesNoCopy(
            kCFAllocatorDefault, (const UInt8 *)anchorslist[anchor_ix].data,
            anchorslist[anchor_ix].length, kCFAllocatorNull), errOut);
        anchor = SecCertificateCreateWithData(kCFAllocatorDefault,
            anchor_data);
        CFRelease(anchor_data);
        if (anchor) {
            CFArrayAppendValue(parents, anchor);
            CFRelease(anchor);
        }
    } while ((anchor_ix = anchorslist[anchor_ix].next_same_subject));

errOut:
    callback(context, parents);
    CFReleaseSafe(parents);
#endif
    return true;
}

/* Quick thought: we can eliminate this method if we search anchor sources
   before all others and we remember if we got a cert from an anchorsource. */
static bool SecSystemAnchorSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
#ifndef SECITEM_SHIM_OSX
    CFDataRef nic = SecCertificateGetNormalizedSubjectContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
     It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */
    const struct subject_to_ix_t *i2x =
        subject_to_anchor_ix((const char *)CFDataGetBytePtr(nic),
            (unsigned int)CFDataGetLength(nic));
    require_quiet(i2x, errOut);
    CFIndex cert_length = SecCertificateGetLength(certificate);
    const UInt8 *cert_data = SecCertificateGetBytePtr(certificate);
    int anchor_ix = i2x->anchor_ix;
    do {
        if (cert_length == anchorslist[anchor_ix].length &&
            !memcmp(anchorslist[anchor_ix].data, cert_data, cert_length))
            return true;
    } while ((anchor_ix = anchorslist[anchor_ix].next_same_subject));

errOut:
#endif
    return false;
}

struct SecCertificateSource kSecSystemAnchorSource = {
	SecSystemAnchorSourceCopyParents,
	SecSystemAnchorSourceContains
};

#pragma mark -
#pragma mark SecUserAnchorSource
/********************************************************
 *********** SecUserAnchorSource object ************
 ********************************************************/
static bool SecUserAnchorSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = SecTrustStoreCopyParents(
        SecTrustStoreForDomain(kSecTrustStoreDomainUser), certificate);
    callback(context, parents);
    CFReleaseSafe(parents);
    return true;
}

static bool SecUserAnchorSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
    return SecTrustStoreContains(
        SecTrustStoreForDomain(kSecTrustStoreDomainUser), certificate);
}

struct SecCertificateSource kSecUserAnchorSource = {
	SecUserAnchorSourceCopyParents,
	SecUserAnchorSourceContains
};
#endif

#pragma mark -
#pragma mark SecMemoryCertificateSource
/********************************************************
 *********** SecMemoryCertificateSource object ************
 ********************************************************/
struct SecMemoryCertificateSource {
	struct SecCertificateSource base;
	CFMutableSetRef certificates;
	CFMutableDictionaryRef subjects;
};
typedef struct SecMemoryCertificateSource *SecMemoryCertificateSourceRef;

static bool SecMemoryCertificateSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
	SecMemoryCertificateSourceRef msource =
		(SecMemoryCertificateSourceRef)source;
	CFDataRef normalizedIssuer =
        SecCertificateGetNormalizedIssuerContent(certificate);
	CFArrayRef parents = CFDictionaryGetValue(msource->subjects,
		normalizedIssuer);
    /* FIXME filter parents by subjectID if certificate has an
       authorityKeyIdentifier. */
    secdebug("trust", "%@ parents -> %@", certificate, parents);
    callback(context, parents);
    return true;
}

static bool SecMemoryCertificateSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
	SecMemoryCertificateSourceRef msource =
		(SecMemoryCertificateSourceRef)source;
	return CFSetContainsValue(msource->certificates, certificate);
}

static void dictAddValueToArrayForKey(CFMutableDictionaryRef dict,
	const void *key, const void *value) {
	if (!key)
		return;

	CFMutableArrayRef values =
		(CFMutableArrayRef)CFDictionaryGetValue(dict, key);
	if (!values) {
		values = CFArrayCreateMutable(kCFAllocatorDefault, 0,
			&kCFTypeArrayCallBacks);
		CFDictionaryAddValue(dict, key, values);
		CFRelease(values);
	}

	if (values)
		CFArrayAppendValue(values, value);
}

static void SecMemoryCertificateSourceApplierFunction(const void *value,
	void *context) {
	SecMemoryCertificateSourceRef msource =
		(SecMemoryCertificateSourceRef)context;
	SecCertificateRef certificate = (SecCertificateRef)value;

	/* CFSet's API has no way to combine these 2 operations into 1 sadly. */
	if (CFSetContainsValue(msource->certificates, certificate))
		return;
	CFSetAddValue(msource->certificates, certificate);

	CFDataRef key = SecCertificateGetNormalizedSubjectContent(certificate);
	dictAddValueToArrayForKey(msource->subjects, key, value);
}

static SecCertificateSourceRef SecMemoryCertificateSourceCreate(
	CFArrayRef certificates) {
	SecMemoryCertificateSourceRef result = (SecMemoryCertificateSourceRef)
		malloc(sizeof(*result));
	result->base.copyParents = SecMemoryCertificateSourceCopyParents;
	result->base.contains = SecMemoryCertificateSourceContains;
	CFIndex count = CFArrayGetCount(certificates);
	result->certificates = CFSetCreateMutable(kCFAllocatorDefault, count,
		&kCFTypeSetCallBacks);
	result->subjects = CFDictionaryCreateMutable(kCFAllocatorDefault,
		count, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks);
	CFRange range = { 0, count };
	CFArrayApplyFunction(certificates, range,
		SecMemoryCertificateSourceApplierFunction, result);

	return (SecCertificateSourceRef)result;
}

static void SecMemoryCertificateSourceDestroy(
	SecCertificateSourceRef source) {
	SecMemoryCertificateSourceRef msource =
		(SecMemoryCertificateSourceRef)source;
	CFRelease(msource->certificates);
	CFRelease(msource->subjects);
	free(msource);
}

#pragma mark -
#pragma mark SecCAIssuerCertificateSource
/********************************************************
 ********* SecCAIssuerCertificateSource object **********
 ********************************************************/
static bool SecCAIssuerCertificateSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    return SecCAIssuerCopyParents(certificate, context, callback);
}

static bool SecCAIssuerCertificateSourceContains(
    SecCertificateSourceRef source, SecCertificateRef certificate) {
	return false;
}

struct SecCertificateSource kSecCAIssuerSource = {
	SecCAIssuerCertificateSourceCopyParents,
	SecCAIssuerCertificateSourceContains
};

#pragma mark -
#pragma mark SecPathBuilder
/********************************************************
 *************** SecPathBuilder object ******************
 ********************************************************/
struct SecPathBuilder {
	SecCertificateSourceRef	certificateSource;
	SecCertificateSourceRef	anchorSource;
	CFMutableArrayRef		anchorSources;
	CFIndex					nextParentSource;
	CFMutableArrayRef		parentSources;

    /* Hashed set of all paths we've constructed so far, used to prevent
       re-considering a path that was already constructed once before.
       Note that this is the only container in which certificatePath
       objects are retained.
       Every certificatePath being considered is always in allPaths and in at
       most one of partialPaths, rejectedPaths, candidatePath or extendedPaths
       all of which don't retain their values.  */
	CFMutableSetRef			allPaths;

    /* No trusted anchor, satisfies the linking to intermediates for all
       policies (unless considerRejected is true). */
	CFMutableArrayRef		partialPaths;
    /* No trusted anchor, does not satisfy linking to intermediates for all
       policies. */
	CFMutableArrayRef		rejectedPaths;
    /* Trusted anchor, satisfies the policies so far. */
	CFMutableArrayRef		candidatePaths;

	CFIndex					partialIX;

	CFArrayRef              leafDetails;

	CFIndex					rejectScore;

	bool                    considerRejected;
	bool                    considerPartials;
	bool                    canAccessNetwork;

    struct OpaqueSecPVC     path;
	SecCertificatePathRef   bestPath;
    bool                    bestPathIsEV;

    CFIndex                 activations;
    bool (*state)(SecPathBuilderRef);
    SecPathBuilderCompleted completed;
    const void *context;
};

/* State functions.  Return false if a async job was scheduled, return
   true to execute the next state. */
static bool SecPathBuilderGetNext(SecPathBuilderRef builder);
static bool SecPathBuilderValidatePath(SecPathBuilderRef builder);
static bool SecPathBuilderDidValidatePath(SecPathBuilderRef builder);
static bool SecPathBuilderComputeDetails(SecPathBuilderRef builder);
static bool SecPathBuilderReportResult(SecPathBuilderRef builder);

/* Forward declarations. */
static bool SecPathBuilderIsAnchor(SecPathBuilderRef builder,
	SecCertificateRef certificate);

/* IDEA: policies could be made cabable of replacing incoming anchors and
   anchorsOnly argument values.  For example some policies require the
   Apple Inc. CA and not any other anchor.  This can be done in
   SecPathBuilderLeafCertificateChecks since this only runs once. */
static void SecPathBuilderLeafCertificateChecks(SecPathBuilderRef builder,
    SecCertificatePathRef path) {
    CFMutableDictionaryRef certDetail = CFDictionaryCreateMutable(
        kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
        &kCFTypeDictionaryValueCallBacks);
    builder->leafDetails = CFArrayCreate(kCFAllocatorDefault,
        (const void **)&certDetail, 1, &kCFTypeArrayCallBacks);
    CFRelease(certDetail);
    SecPVCRef pvc = &builder->path;
    SecPVCSetPath(pvc, path, builder->leafDetails);
    builder->considerRejected = !SecPVCLeafChecks(pvc);
}

static void SecPathBuilderInit(SecPathBuilderRef builder,
	CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly,
    CFArrayRef policies, CFAbsoluteTime verifyTime,
    SecPathBuilderCompleted completed, const void *context) {
    secdebug("alloc", "%p", builder);
	CFAllocatorRef allocator = kCFAllocatorDefault;

	builder->nextParentSource = 1;
	builder->considerPartials = false;
    builder->canAccessNetwork = true;

    builder->anchorSources = CFArrayCreateMutable(allocator, 0, NULL);
    builder->parentSources = CFArrayCreateMutable(allocator, 0, NULL);
    builder->allPaths = CFSetCreateMutable(allocator, 0,
		&kCFTypeSetCallBacks);

    builder->partialPaths = CFArrayCreateMutable(allocator, 0, NULL);
    builder->rejectedPaths = CFArrayCreateMutable(allocator, 0, NULL);
    builder->candidatePaths = CFArrayCreateMutable(allocator, 0, NULL);
    builder->partialIX = 0;

    /* Init the policy verification context. */
    SecPVCInit(&builder->path, builder, policies, verifyTime);
	builder->bestPath = NULL;
	builder->bestPathIsEV = false;
	builder->rejectScore = 0;

	/* Let's create all the certificate sources we might want to use. */
	builder->certificateSource =
		SecMemoryCertificateSourceCreate(certificates);
	if (anchors)
		builder->anchorSource = SecMemoryCertificateSourceCreate(anchors);
	else
		builder->anchorSource = NULL;

	/* We always search certificateSource for parents since it includes the
	   leaf itself and it might be self signed. */
	CFArrayAppendValue(builder->parentSources, builder->certificateSource);
	if (builder->anchorSource) {
		CFArrayAppendValue(builder->anchorSources, builder->anchorSource);
	}
	CFArrayAppendValue(builder->parentSources, &kSecItemCertificateSource);
    if (anchorsOnly) {
        /* Add the system and user anchor certificate db to the search list
           if we don't explicitly trust them. */
        CFArrayAppendValue(builder->parentSources, &kSecSystemAnchorSource);
        CFArrayAppendValue(builder->parentSources, &kSecUserAnchorSource);
    } else {
        /* Only add the system and user anchor certificate db to the
           anchorSources if we are supposed to trust them. */
        CFArrayAppendValue(builder->anchorSources, &kSecSystemAnchorSource);
        CFArrayAppendValue(builder->anchorSources, &kSecUserAnchorSource);
    }
    CFArrayAppendValue(builder->parentSources, &kSecCAIssuerSource);

	/* Now let's get the leaf cert and turn it into a path. */
	SecCertificateRef leaf =
		(SecCertificateRef)CFArrayGetValueAtIndex(certificates, 0);
	SecCertificatePathRef path = SecCertificatePathCreate(NULL, leaf);
	CFSetAddValue(builder->allPaths, path);
	CFArrayAppendValue(builder->partialPaths, path);
    if (SecPathBuilderIsAnchor(builder, leaf)) {
        SecCertificatePathSetIsAnchored(path);
        CFArrayAppendValue(builder->candidatePaths, path);
    }
    SecPathBuilderLeafCertificateChecks(builder, path);
	CFRelease(path);

    builder->activations = 0;
    builder->state = SecPathBuilderGetNext;
    builder->completed = completed;
    builder->context = context;
}

SecPathBuilderRef SecPathBuilderCreate(CFArrayRef certificates,
    CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies,
    CFAbsoluteTime verifyTime,
    SecPathBuilderCompleted completed, const void *context) {
    SecPathBuilderRef builder = malloc(sizeof(*builder));
    SecPathBuilderInit(builder, certificates, anchors, anchorsOnly,
        policies, verifyTime, completed, context);
    return builder;
}

static void SecPathBuilderDestroy(SecPathBuilderRef builder) {
    secdebug("alloc", "%p", builder);
	if (builder->anchorSource)
		SecMemoryCertificateSourceDestroy(builder->anchorSource);
	if (builder->certificateSource)
		SecMemoryCertificateSourceDestroy(builder->certificateSource);

	CFReleaseSafe(builder->anchorSources);
	CFReleaseSafe(builder->parentSources);
	CFReleaseSafe(builder->allPaths);
	CFReleaseSafe(builder->partialPaths);
	CFReleaseSafe(builder->rejectedPaths);
	CFReleaseSafe(builder->candidatePaths);
	CFReleaseSafe(builder->leafDetails);

    SecPVCDelete(&builder->path);
}

bool SecPathBuilderCanAccessNetwork(SecPathBuilderRef builder) {
    return builder->canAccessNetwork;
}

void SecPathBuilderSetCanAccessNetwork(SecPathBuilderRef builder, bool allow) {
    if (builder->canAccessNetwork != allow) {
        builder->canAccessNetwork = allow;
        if (allow) {
            secdebug("http", "network access re-enabled by policy");
            /* re-enabling network_access re-adds kSecCAIssuerSource as
               a parent source. */
            CFArrayAppendValue(builder->parentSources, &kSecCAIssuerSource);
        } else {
            secdebug("http", "network access disabled by policy");
            /* disabling network_access removes kSecCAIssuerSource from
               the list of parent sources. */
            CFIndex ix = CFArrayGetFirstIndexOfValue(builder->parentSources,
                CFRangeMake(0, CFArrayGetCount(builder->parentSources)),
                &kSecCAIssuerSource);
            if (ix >= 0)
                CFArrayRemoveValueAtIndex(builder->parentSources, ix);
        }
    }
}

static bool SecPathBuilderIsAnchor(SecPathBuilderRef builder,
	SecCertificateRef certificate) {
	/* We always look through all anchor sources. */
	CFIndex count = CFArrayGetCount(builder->anchorSources);
	CFIndex ix;
	for (ix = 0; ix < count; ++ix) {
		SecCertificateSourceRef source = (SecCertificateSourceRef)
			CFArrayGetValueAtIndex(builder->anchorSources, ix);
		if (SecCertificateSourceContains(source, certificate)) {
			return true;
		}
	}
	return false;
}

/* Return false if path is not a partial, if path was a valid candidate it
   will have been added to builder->candidatePaths, if path was rejected
   by the parent certificate checks (because it's expired or some other
   static chaining check failed) it will have been added to rejectedPaths.
   Return true path if path is a partial. */
static bool SecPathBuilderIsPartial(SecPathBuilderRef builder,
	SecCertificatePathRef path) {
    SecPVCRef pvc = &builder->path;
    SecPVCSetPath(pvc, path, NULL);

    if (!builder->considerRejected && !SecPVCParentCertificateChecks(pvc,
        SecPVCGetCertificateCount(pvc) - 1)) {
        secdebug("trust", "Found rejected path %@", path);
		CFArrayAppendValue(builder->rejectedPaths, path);
		return false;
	}

	SecPathVerifyStatus vstatus = SecCertificatePathVerify(path);
	/* Candidate paths with failed signatures are discarded. */
	if (vstatus == kSecPathVerifyFailed) {
        secdebug("trust", "Verify failed for path %@", path);
		return false;
	}

	if (vstatus == kSecPathVerifySuccess) {
		/* The signature chain verified sucessfully, now let's find
		   out if we have an anchor for path.  */
		if (SecCertificatePathIsAnchored(path)) {
            secdebug("trust", "Adding candidate %@", path);
			CFArrayAppendValue(builder->candidatePaths, path);
			return false;
		}
	}

	return true;
}

/* Given the builder, a partial chain partial and the parents array, construct
   a SecCertificatePath for each parent.  After discarding previously
   considered paths and paths with cycles, sort out which array each path
   should go in, if any. */
static void SecPathBuilderProccessParents(SecPathBuilderRef builder,
    SecCertificatePathRef partial, CFArrayRef parents) {
    CFIndex rootIX = SecCertificatePathGetCount(partial) - 1;
    CFIndex num_parents = parents ? CFArrayGetCount(parents) : 0;
    CFIndex parentIX;
    bool is_anchor = SecCertificatePathGetNextSourceIndex(partial) <=
        CFArrayGetCount(builder->anchorSources);
    secdebug("trust", "found %d candidate %s", num_parents,
             (is_anchor ? "anchors" : "parents"));
    for (parentIX = 0; parentIX < num_parents; ++parentIX) {
        SecCertificateRef parent = (SecCertificateRef)
            CFArrayGetValueAtIndex(parents, parentIX);
        CFIndex ixOfParent = SecCertificatePathGetIndexOfCertificate(partial,
            parent);
        if (ixOfParent != kCFNotFound) {
            /* partial already contains parent.  Let's not add the same
               certificate again. */
            if (ixOfParent == rootIX) {
                /* parent is equal to the root of the partial, so partial
                   looks to be self issued. */
                SecCertificatePathSetSelfIssued(partial);
            }
            continue;
        }

        /* FIXME Add more sanity checks to see that parent really can be
           a parent of partial_root.  subjectKeyID == authorityKeyID,
           signature algorithm matches public key algorithm, etc. */
        SecCertificatePathRef path = SecCertificatePathCreate(partial, parent);
        if (!path)
            continue;
        if (!CFSetContainsValue(builder->allPaths, path)) {
            CFSetAddValue(builder->allPaths, path);
            if (is_anchor)
                SecCertificatePathSetIsAnchored(path);
            if (SecPathBuilderIsPartial(builder, path)) {
                /* Insert path right at the current position since it's a new
                   candiate partial. */
                CFArrayInsertValueAtIndex(builder->partialPaths,
                    ++builder->partialIX, path);
                secdebug("trust", "Adding partial for parent %d/%d %@",
                    parentIX + 1, num_parents, path);
            }
            secdebug("trust", "found new path %@", path);
        }
        CFRelease(path);
    }
}

/* Callback for the SecPathBuilderGetNext() functions call to
   SecCertificateSourceCopyParents(). */
static void SecPathBuilderExtendPaths(void *context, CFArrayRef parents) {
    SecPathBuilderRef builder = (SecPathBuilderRef)context;
    SecCertificatePathRef partial = (SecCertificatePathRef)
        CFArrayGetValueAtIndex(builder->partialPaths, builder->partialIX);
    secdebug("async", "%@ parents %@", partial, parents);
    SecPathBuilderProccessParents(builder, partial, parents);

    builder->state = SecPathBuilderGetNext;
    SecPathBuilderStep(builder);
}

static bool SecPathBuilderGetNext(SecPathBuilderRef builder) {
    /* If we have any candidates left to go return those first. */
    if (CFArrayGetCount(builder->candidatePaths)) {
        SecCertificatePathRef path = (SecCertificatePathRef)
            CFArrayGetValueAtIndex(builder->candidatePaths, 0);
        CFArrayRemoveValueAtIndex(builder->candidatePaths, 0);
        secdebug("trust", "SecPathBuilderGetNext returning candidate %@",
            path);
        SecPVCSetPath(&builder->path, path, NULL);
        builder->state = SecPathBuilderValidatePath;
        return true;
    }

    /* If we are considering rejected chains we check each rejected path
       with SecPathBuilderIsPartial() which checks the signature chain and
       either drops the path if it's not properly signed, add it as a
       candidate if it has a trusted anchor, or adds it as a partial
       to be considered once we finish considering all the rejects. */
    if (builder->considerRejected) {
        CFIndex rejectedIX = CFArrayGetCount(builder->rejectedPaths);
        if (rejectedIX) {
            rejectedIX--;
            SecCertificatePathRef path = (SecCertificatePathRef)
                CFArrayGetValueAtIndex(builder->rejectedPaths, rejectedIX);
            if (SecPathBuilderIsPartial(builder, path)) {
                CFArrayInsertValueAtIndex(builder->partialPaths,
                    ++builder->partialIX, path);
            }
            CFArrayRemoveValueAtIndex(builder->rejectedPaths, rejectedIX);

            /* Keep going until we have moved all rejected partials into
               the regular partials or candidates array. */
            return true;
        }
    }

    /* If builder->partialIX is < 0 we have considered all partial chains
       this block must ensure partialIX >= 0 if execution continues past
       it's end. */
    if (builder->partialIX < 0) {
        CFIndex num_sources = CFArrayGetCount(builder->parentSources);
        if (builder->nextParentSource < num_sources) {
            builder->nextParentSource++;
            secdebug("trust", "broading search to %d/%d sources",
                builder->nextParentSource, num_sources);
        } else {
            /* We've run out of new sources to consider so let's look at
               rejected chains and after that even consider partials
               directly.
               FIXME we might not want to consider partial paths that
               are subsets of other partial paths, or not consider them
               at all if we already have an anchored reject. */
            if (!builder->considerRejected) {
                builder->considerRejected = true;
                secdebug("trust", "considering rejected paths");
            } else if (!builder->considerPartials) {
                builder->considerPartials = true;
                secdebug("trust", "considering partials");
            } else {
                /* We're all out of options, so we can't produce any more
                   candidates.  Let's calculate details and return the best
                   path we found. */
                builder->state = SecPathBuilderComputeDetails;
                return true;
            }
        }
        builder->partialIX = CFArrayGetCount(builder->partialPaths) - 1;
        secdebug("trust", "re-checking %d partials", builder->partialIX + 1);
        return true;
    }

    /* We know builder->partialIX >= 0 if we get here.  */
    SecCertificatePathRef partial = (SecCertificatePathRef)
        CFArrayGetValueAtIndex(builder->partialPaths, builder->partialIX);
    /* Don't try to extend partials anymore once we are in the considerPartials
       state, since at this point every partial has been extended with every
       possible parentSource already. */
    if (builder->considerPartials) {
        --builder->partialIX;
        SecPVCSetPath(&builder->path, partial, NULL);
        builder->state = SecPathBuilderValidatePath;
        return true;
    }

    /* Attempt to extend this partial path with another certificate. This
       should give us a list of potential parents to consider. */
    secdebug("trust", "looking for parents of partial %d/%d: %@",
        builder->partialIX + 1, CFArrayGetCount(builder->partialPaths),
        partial);

    /* Attempt to extend partial, leaving all possible extended versions
       of partial in builder->extendedPaths. */
	CFIndex sourceIX = SecCertificatePathGetNextSourceIndex(partial);
    CFIndex num_anchor_sources = CFArrayGetCount(builder->anchorSources);
    if (sourceIX < num_anchor_sources + builder->nextParentSource) {
        SecCertificateSourceRef source;
        if (sourceIX < num_anchor_sources) {
            source = (SecCertificateSourceRef)
                CFArrayGetValueAtIndex(builder->anchorSources, sourceIX);
            secdebug("trust", "searching anchor source %d/%d", sourceIX + 1,
                     num_anchor_sources);
        } else {
            CFIndex parentIX = sourceIX - num_anchor_sources;
            source = (SecCertificateSourceRef)
                CFArrayGetValueAtIndex(builder->parentSources, parentIX);
            secdebug("trust", "searching parent source %d/%d", parentIX + 1,
                     builder->nextParentSource);
        }
        SecCertificatePathSetNextSourceIndex(partial, sourceIX + 1);
        SecCertificateRef root = SecCertificatePathGetRoot(partial);
		return SecCertificateSourceCopyParents(source, root,
            builder, SecPathBuilderExtendPaths);
    } else {
        --builder->partialIX;
    }

    return true;
}

/* One or more of the policies did not accept the candidate path. */
static void SecPathBuilderReject(SecPathBuilderRef builder) {
    check(builder);
    SecPVCRef pvc = &builder->path;

    builder->state = SecPathBuilderGetNext;

    if (builder->bestPathIsEV && !pvc->is_ev) {
        /* We never replace an ev reject with a non ev reject. */
        return;
    }

    CFIndex rejectScore = builder->rejectScore;
	CFIndex score = SecCertificatePathScore(builder->path.path,
        SecPVCGetVerifyTime(&builder->path));

    /* The current chain is valid for EV, but revocation checking failed.  We
       replace any previously accepted or rejected non EV chains with the
       current one. */
    if (pvc->is_ev && !builder->bestPathIsEV) {
        rejectScore = 0;
    }

#if 0
    if (pvc->is_ev) {
        /* Since this means we found a valid ev chain that was revoked,
           we might want to switch directly to the
           SecPathBuilderComputeDetails state here if we think further
           searching for new chains is pointless.  For now we'll keep
           going, since we could accept an alternate EV certification
           path that isn't revoked. */
        builder->state = SecPathBuilderComputeDetails;
    }
#endif

    /* Do this last so that changes to rejectScore above will take affect. */
	if (!builder->bestPath || score > rejectScore) {
        if (builder->bestPath) {
            secdebug("reject",
                "replacing %sev %s score: %ld with %sev reject score: %d %@",
                (builder->bestPathIsEV ? "" : "non "),
                (builder->rejectScore == INTPTR_MAX ? "accept" : "reject"),
                builder->rejectScore,
                (pvc->is_ev ? "" : "non "), score, builder->path.path);
        } else {
            secdebug("reject", "%sev reject score: %d %@",
                (pvc->is_ev ? "" : "non "), score, builder->path.path);
        }

		builder->rejectScore = score;
        builder->bestPath = pvc->path;
        builder->bestPathIsEV = pvc->is_ev;
	} else {
        secdebug("reject", "%sev reject score: %d lower than %d %@",
            (pvc->is_ev ? "" : "non "), score, rejectScore, builder->path.path);
    }
}

/* All policies accepted the candidate path. */
static void SecPathBuilderAccept(SecPathBuilderRef builder) {
    check(builder);
    SecPVCRef pvc = &builder->path;
    if (pvc->is_ev || !builder->bestPathIsEV) {
		secdebug("accept", "replacing %sev accept with %sev %@",
            (builder->bestPathIsEV ? "" : "non "),
            (pvc->is_ev ? "" : "non "), builder->path.path);
        builder->rejectScore = INTPTR_MAX; /* CFIndex is signed long which is INTPTR_T */
		builder->bestPathIsEV = pvc->is_ev;
        builder->bestPath = pvc->path;
    }

    /* If we found the best accept we can we want to switch directly to the
       SecPathBuilderComputeDetails state here, since we're done. */
    if (pvc->is_ev || !pvc->optionally_ev)
        builder->state = SecPathBuilderComputeDetails;
    else
        builder->state = SecPathBuilderGetNext;
}

/* Return true iff a given path satisfies all the specified policies at
   verifyTime. */
static bool SecPathBuilderValidatePath(SecPathBuilderRef builder) {
    SecPVCRef pvc = &builder->path;

    if (builder->considerRejected) {
        SecPathBuilderReject(builder);
        return true;
    }

    builder->state = SecPathBuilderDidValidatePath;
    return SecPVCPathChecks(pvc);
}

static bool SecPathBuilderDidValidatePath(SecPathBuilderRef builder) {
    SecPVCRef pvc = &builder->path;
    if (pvc->result) {
        SecPathBuilderAccept(builder);
    } else {
        SecPathBuilderReject(builder);
    }
    assert(builder->state != SecPathBuilderDidValidatePath);
    return true;
}

static bool SecPathBuilderComputeDetails(SecPathBuilderRef builder) {
    SecPVCRef pvc = &builder->path;
#if 0
    if (!builder->caller_wants_details) {
        SecPVCSetPath(pvc, builder->bestPath, NULL);
        pvc->result = builder->rejectScore == INTPTR_MAX;
        builder->state = SecPathBuilderReportResult;
        return true;
    }
#endif
    CFIndex ix, pathLength = SecCertificatePathGetCount(builder->bestPath);
    CFMutableArrayRef details = CFArrayCreateMutableCopy(kCFAllocatorDefault,
        pathLength, builder->leafDetails);
    SecPVCSetPath(pvc, builder->bestPath, details);
    /* Only report on EV stuff if the bestPath actually was valid for EV. */
    pvc->optionally_ev = builder->bestPathIsEV;
    pvc->info = CFDictionaryCreateMutable(kCFAllocatorDefault,
        0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    for (ix = 1; ix < pathLength; ++ix) {
        CFMutableDictionaryRef certDetail = CFDictionaryCreateMutable(
            kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks);
        CFArrayAppendValue(details, certDetail);
        CFRelease(certDetail);
        SecPVCParentCertificateChecks(pvc, ix);
        SecPVCBlackListedKeyChecks(pvc, ix);
    }
    builder->state = SecPathBuilderReportResult;
    bool completed = SecPVCPathChecks(pvc);

    /* Reject the certificate if it was accepted before but we failed it now. */
    if (builder->rejectScore == INTPTR_MAX && !pvc->result) {
        builder->rejectScore = 0;
    }

    return completed;
}

static bool SecPathBuilderReportResult(SecPathBuilderRef builder) {
    SecPVCRef pvc = &builder->path;
    if (pvc->info && pvc->is_ev && pvc->result) {
        CFDictionarySetValue(pvc->info, kSecTrustInfoExtendedValidationKey,
            kCFBooleanTrue);
        SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
        CFStringRef leafCompanyName = SecCertificateCopyCompanyName(leaf);
        if (leafCompanyName) {
            CFDictionarySetValue(pvc->info, kSecTrustInfoCompanyNameKey,
                leafCompanyName);
            CFRelease(leafCompanyName);
        }
        if (pvc->rvcs) {
            CFAbsoluteTime nextUpdate = SecPVCGetEarliestNextUpdate(pvc);
            if (nextUpdate == 0) {
                CFDictionarySetValue(pvc->info, kSecTrustInfoRevocationKey,
                    kCFBooleanFalse);
            } else {
                CFDateRef validUntil = CFDateCreate(kCFAllocatorDefault, nextUpdate);
                CFDictionarySetValue(pvc->info, kSecTrustInfoRevocationValidUntilKey,
                    validUntil);
                CFRelease(validUntil);
                CFDictionarySetValue(pvc->info, kSecTrustInfoRevocationKey,
                    kCFBooleanTrue);
            }
        }
    }

    /* This will trigger the outer step function to call the completion
       function. */
    builder->state = NULL;
    return false;
}

/* @function SecPathBuilderStep
   @summary This is the core of the async engine.
   @description Return false iff job is complete, true if a network request
   is pending.
   builder->state is a function pointer which is to be invoked.
   If you call this function from within a builder->state invocation it
   immediately returns true.
   Otherwise the following steps are repeated endlessly (unless a step returns)
   builder->state is invoked.  If it returns true and builder->state is still
   non NULL this proccess is repeated.
   If a state returns false, SecPathBuilder will return true
   if builder->state is non NULL.
   If builder->state is NULL then regardless of what the state function returns
   the completion callback will be invoked and the builder will be deallocated.
 */
bool SecPathBuilderStep(SecPathBuilderRef builder) {
    if (builder->activations) {
        secdebug("async", "activations: %lu returning true",
            builder->activations);
        return true;
    }

    secdebug("async", "activations: %lu", builder->activations);
    builder->activations++;
    while (builder->state && builder->state(builder));
    --builder->activations;

    if (builder->state) {
        secdebug("async", "waiting for async reply, exiting");
        /* A state returned false, it's waiting for network traffic.  Let's
           return. */
        return true;
    }

    if (builder->activations) {
        /* There is still at least one other running instance of this builder
           somewhere on the stack, we let that instance take care of sending
           the client a response. */
        return false;
    }

    SecTrustResultType result = (builder->rejectScore == INTPTR_MAX
        ? kSecTrustResultUnspecified : kSecTrustResultRecoverableTrustFailure);
    secdebug("trust", "completed: %@ details: %@ result: %d",
        builder->bestPath, builder->path.details, result);
    if (builder->completed) {
        builder->completed(builder->context, builder->bestPath,
            builder->path.details, builder->path.info, result);
    }

    /* Finally, destroy the builder and free it. */
    SecPathBuilderDestroy(builder);
    free(builder);

    return false;
}


#pragma mark -
#pragma mark SecTrustServer
/********************************************************
 ****************** SecTrustServer **********************
 ********************************************************/

/* AUDIT[securityd](done):
   args_in (ok) is a caller provided dictionary, only its cf type has been checked.
 */
OSStatus
SecTrustServerEvaluateAsync(CFDictionaryRef args_in,
    SecPathBuilderCompleted completed, const void *userData) {
    OSStatus status = paramErr;
    CFArrayRef certificates = NULL, anchors = NULL, policies = NULL;

    /* Proccess incoming arguments. */
    CFArrayRef certificatesData = (CFArrayRef)CFDictionaryGetValue(args_in, kSecTrustCertificatesKey);
    require_quiet(certificatesData && CFGetTypeID(certificatesData) == CFArrayGetTypeID(), errOut);
    certificates = SecCertificateDataArrayCopyArray(certificatesData);
    require_quiet(certificates
        && CFGetTypeID(certificates) == CFArrayGetTypeID()
        && CFArrayGetCount(certificates) > 0, errOut);
    CFArrayRef anchorsData = (CFArrayRef)CFDictionaryGetValue(args_in, kSecTrustAnchorsKey);
    if (anchorsData) {
        require_quiet(CFGetTypeID(anchorsData) == CFArrayGetTypeID(), errOut);
        anchors = SecCertificateDataArrayCopyArray(anchorsData);
    }
    bool anchorsOnly = CFDictionaryContainsKey(args_in, kSecTrustAnchorsOnlyKey);
    CFArrayRef serializedPolicies = (CFArrayRef)CFDictionaryGetValue(args_in, kSecTrustPoliciesKey);
    if (serializedPolicies) {
        require_quiet(CFGetTypeID(serializedPolicies) == CFArrayGetTypeID(), errOut);
        policies = SecPolicyArrayDeserialize(serializedPolicies);
    }
    CFDateRef verifyDate = (CFDateRef)CFDictionaryGetValue(args_in, kSecTrustVerifyDateKey);
    require_quiet(verifyDate && CFGetTypeID(verifyDate) == CFDateGetTypeID(), errOut);
    CFAbsoluteTime verifyTime = CFDateGetAbsoluteTime(verifyDate);

    /* Call the actual evaluator function. */
    SecPathBuilderRef builder = SecPathBuilderCreate(certificates, anchors,
        anchorsOnly, policies, verifyTime, completed, userData);
    status = SecPathBuilderStep(builder) ? errSecWaitForCallback : noErr;

errOut:
    CFReleaseSafe(policies);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certificates);
    return status;
}

struct SecTrustEvaluationContext {
    CFTypeRef args_out;
    bool running;
};

static void
SecTrustServerEvaluateDone(const void *userData,
    SecCertificatePathRef chain, CFArrayRef details, CFDictionaryRef info,
    SecTrustResultType result) {
    struct SecTrustEvaluationContext *tec =
        (struct SecTrustEvaluationContext *)userData;

    /* @@@ This code snippit is also in server.c.  I'd factor it, but a better
       fix would be to chage the interfaces here to not use single in/out args
       and do all the argument munging in server.c and client.c. */
    CFDictionaryRef args_out;
    CFNumberRef resultNumber = NULL;
    CFArrayRef chain_certs = NULL;
    /* Proccess outgoing results. */
    resultNumber = CFNumberCreate(NULL, kCFNumberSInt32Type, &result);
    chain_certs = SecCertificatePathCopyArray(chain);
    const void *out_keys[] = { kSecTrustChainKey, kSecTrustDetailsKey,
        kSecTrustInfoKey, kSecTrustResultKey };
    const void *out_values[] = { chain_certs, details, info, resultNumber };
    args_out = (CFTypeRef)CFDictionaryCreate(kCFAllocatorDefault, out_keys,
        out_values, sizeof(out_keys) / sizeof(*out_keys),
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFReleaseSafe(chain_certs);
    CFReleaseSafe(resultNumber);

    /* Return the final result. */
    tec->args_out = args_out;
    if (tec->running) {
        /* Stop the runloop in SecTrustServerEvaluate if it is running. */
        CFRunLoopStop(CFRunLoopGetCurrent());
    }
}

OSStatus
SecTrustServerEvaluate(CFDictionaryRef args_in, CFTypeRef *args_out) {
    OSStatus status;
    struct SecTrustEvaluationContext tec;
    tec.args_out = NULL;
    tec.running = false;
    status = SecTrustServerEvaluateAsync(args_in, SecTrustServerEvaluateDone,
        &tec);
    if (status == noErr || status == errSecWaitForCallback) {
        if (status == errSecWaitForCallback) {
            /* Since errSecWaitForCallback isn't a real error clear status. */
            status = noErr;
            /* Mark the context as running so the callback will stop the runloop,
               and run the default runloop until the callback stops us. */
            tec.running = true;
            CFRunLoopRun();
        }
        *args_out = tec.args_out;
    }

    return status;
}
