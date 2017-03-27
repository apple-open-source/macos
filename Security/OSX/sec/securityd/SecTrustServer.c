/*
 * Copyright (c) 2006-2010,2012-2016 Apple Inc. All Rights Reserved.
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
 *
 */

#include <securityd/SecTrustServer.h>
#include <securityd/SecPolicyServer.h>
#include <securityd/SecTrustLoggingServer.h>
#include <securityd/SecCertificateSource.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecAppleAnchorPriv.h>

#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePath.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecTask.h>
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
#include <limits.h>
#include <sys/codesign.h>
#include <Security/SecBase.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecInternal.h>
#include <ipc/securityd_client.h>
#include <CommonCrypto/CommonDigest.h>
#include "OTATrustUtilities.h"
#include "personalization.h"
#include <utilities/SecInternalReleasePriv.h>

#if TARGET_OS_OSX
#include <Security/SecTaskPriv.h>
#endif

#define MAX_CHAIN_LENGTH  15
#define ACCEPT_PATH_SCORE 10000000

/* Forward declaration for use in SecCertificateSource. */
static void SecPathBuilderExtendPaths(void *context, CFArrayRef parents);

// MARK: -
// MARK: SecPathBuilder
/********************************************************
 *************** SecPathBuilder object ******************
 ********************************************************/
struct SecPathBuilder {
    dispatch_queue_t        queue;
    CFDataRef               clientAuditToken;
    SecCertificateSourceRef certificateSource;
    SecCertificateSourceRef itemCertificateSource;
    SecCertificateSourceRef anchorSource;
    SecCertificateSourceRef appleAnchorSource;
    CFMutableArrayRef       anchorSources;
    CFIndex                 nextParentSource;
    CFMutableArrayRef       parentSources;
    CFArrayRef              ocspResponses;               // Stapled OCSP responses
    CFArrayRef              signedCertificateTimestamps; // Stapled SCTs
    CFArrayRef              trustedLogs;                 // Trusted CT logs

    /* Hashed set of all paths we've constructed so far, used to prevent
       re-considering a path that was already constructed once before.
       Note that this is the only container in which certificatePath
       objects are retained.
       Every certificatePath being considered is always in allPaths and in at
       most one of partialPaths, rejectedPaths, candidatePath or extendedPaths
       all of which don't retain their values.  */
    CFMutableSetRef         allPaths;

    /* No trusted anchor, satisfies the linking to intermediates for all
       policies (unless considerRejected is true). */
    CFMutableArrayRef       partialPaths;
    /* No trusted anchor, does not satisfy linking to intermediates for all
       policies. */
    CFMutableArrayRef       rejectedPaths;
    /* Trusted anchor, satisfies the policies so far. */
    CFMutableArrayRef       candidatePaths;

    CFIndex                 partialIX;

    CFArrayRef              leafDetails;

    CFIndex                 bestPathScore;

    bool                    considerRejected;
    bool                    considerPartials;
    bool                    canAccessNetwork;

    struct OpaqueSecPVC     path;
    SecCertificatePathRef   bestPath;
    bool                    bestPathIsEV;
    bool                    bestPathIsSHA2;
    bool                    denyBestPath;

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
	SecCertificateRef certificate, SecCertificateSourceRef *foundInSource);

/* IDEA: policies could be made capable of replacing incoming anchors and
   anchorsOnly argument values.  For example, some policies require the
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
    CFDataRef clientAuditToken, CFArrayRef certificates,
    CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed,
    CFArrayRef policies, CFArrayRef ocspResponses,
    CFArrayRef signedCertificateTimestamps, CFArrayRef trustedLogs,
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups,
    SecPathBuilderCompleted completed, const void *context) {
    secdebug("alloc", "%p", builder);
    CFAllocatorRef allocator = kCFAllocatorDefault;

    builder->clientAuditToken = (CFDataRef)
        ((clientAuditToken) ? CFRetain(clientAuditToken) : NULL);
    builder->queue = dispatch_queue_create("builder", DISPATCH_QUEUE_SERIAL);

    builder->nextParentSource = 1;
#if !TARGET_OS_WATCH
    builder->canAccessNetwork = true;
#endif

    builder->anchorSources = CFArrayCreateMutable(allocator, 0, NULL);
    builder->parentSources = CFArrayCreateMutable(allocator, 0, NULL);
    builder->allPaths = CFSetCreateMutable(allocator, 0,
		&kCFTypeSetCallBacks);

    builder->partialPaths = CFArrayCreateMutable(allocator, 0, NULL);
    builder->rejectedPaths = CFArrayCreateMutable(allocator, 0, NULL);
    builder->candidatePaths = CFArrayCreateMutable(allocator, 0, NULL);

    /* Init the policy verification context. */
    SecPVCInit(&builder->path, builder, policies, verifyTime);

	/* Let's create all the certificate sources we might want to use. */
	builder->certificateSource =
		SecMemoryCertificateSourceCreate(certificates);
    if (anchors) {
		builder->anchorSource = SecMemoryCertificateSourceCreate(anchors);
    }

    bool allowNonProduction = false;
    builder->appleAnchorSource = SecMemoryCertificateSourceCreate(SecGetAppleTrustAnchors(allowNonProduction));


    /** Parent Sources
     ** The order here avoids the most expensive methods if the cheaper methods
     ** produce an acceptable chain: client-provided, keychains, network-fetched.
     **/
#if !TARGET_OS_BRIDGE
    CFArrayAppendValue(builder->parentSources, builder->certificateSource);
    builder->itemCertificateSource = SecItemCertificateSourceCreate(accessGroups);
    if (keychainsAllowed) {
        CFArrayAppendValue(builder->parentSources, builder->itemCertificateSource);
 #if TARGET_OS_OSX
        /* On OS X, need additional parent source to search legacy keychain files. */
        if (kSecLegacyCertificateSource->contains && kSecLegacyCertificateSource->copyParents) {
            CFArrayAppendValue(builder->parentSources, kSecLegacyCertificateSource);
        }
 #endif
    }
    if (anchorsOnly) {
        /* Add the Apple, system, and user anchor certificate db to the search list
         if we don't explicitly trust them. */
        CFArrayAppendValue(builder->parentSources, builder->appleAnchorSource);
        CFArrayAppendValue(builder->parentSources, kSecSystemAnchorSource);
 #if TARGET_OS_IPHONE
        CFArrayAppendValue(builder->parentSources, kSecUserAnchorSource);
 #endif
    }
    if (keychainsAllowed && builder->canAccessNetwork) {
        CFArrayAppendValue(builder->parentSources, kSecCAIssuerSource);
    }
#else /* TARGET_OS_BRIDGE */
    /* Bridge can only access memory sources. */
    CFArrayAppendValue(builder->parentSources, builder->certificateSource);
    if (anchorsOnly) {
        /* Add the Apple, system, and user anchor certificate db to the search list
         if we don't explicitly trust them. */
        CFArrayAppendValue(builder->parentSources, builder->appleAnchorSource);
    }
#endif /* !TARGET_OS_BRIDGE */

    /** Anchor Sources
     ** The order here allows a client-provided anchor to overrule
     ** a user or admin trust setting which can overrule the system anchors.
     ** Apple's anchors cannot be overriden by a trust setting.
     **/
#if !TARGET_OS_BRIDGE
	if (builder->anchorSource) {
		CFArrayAppendValue(builder->anchorSources, builder->anchorSource);
	}
    if (!anchorsOnly) {
        /* Only add the system and user anchor certificate db to the
         anchorSources if we are supposed to trust them. */
        CFArrayAppendValue(builder->anchorSources, builder->appleAnchorSource);
 #if TARGET_OS_IPHONE
        CFArrayAppendValue(builder->anchorSources, kSecUserAnchorSource);
 #else /* TARGET_OS_OSX */
        if (keychainsAllowed && kSecLegacyAnchorSource->contains && kSecLegacyAnchorSource->copyParents) {
            CFArrayAppendValue(builder->anchorSources, kSecLegacyAnchorSource);
        }
 #endif
        CFArrayAppendValue(builder->anchorSources, kSecSystemAnchorSource);
    }
#else /* TARGET_OS_BRIDGE */
    /* Bridge can only access memory sources. */
    if (builder->anchorSource) {
        CFArrayAppendValue(builder->anchorSources, builder->anchorSource);
    }
    if (!anchorsOnly) {
        CFArrayAppendValue(builder->anchorSources, builder->appleAnchorSource);
    }
#endif /* !TARGET_OS_BRIDGE */

	/* Now let's get the leaf cert and turn it into a path. */
	SecCertificateRef leaf =
		(SecCertificateRef)CFArrayGetValueAtIndex(certificates, 0);
	SecCertificateSourceRef source = NULL;
	bool isAnchor = false;
	CFArrayRef constraints = NULL;
    if (SecPathBuilderIsAnchor(builder, leaf, &source)) {
        isAnchor = true;
    }
    if (source) {
        constraints = SecCertificateSourceCopyUsageConstraints(source, leaf);
    }
    SecCertificatePathRef path = SecCertificatePathCreate(NULL, leaf, constraints);
    CFReleaseSafe(constraints);
    CFSetAddValue(builder->allPaths, path);
	CFArrayAppendValue(builder->partialPaths, path);
    if (isAnchor) {
        SecCertificatePathSetIsAnchored(path);
        CFArrayAppendValue(builder->candidatePaths, path);
    }
    SecPathBuilderLeafCertificateChecks(builder, path);
	CFRelease(path);

    builder->ocspResponses = CFRetainSafe(ocspResponses);
    builder->signedCertificateTimestamps = CFRetainSafe(signedCertificateTimestamps);

    if(trustedLogs) {
        builder->trustedLogs = CFRetainSafe(trustedLogs);
    } else {
        SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
        builder->trustedLogs = SecOTAPKICopyTrustedCTLogs(otapkiref);
        CFReleaseSafe(otapkiref);
    }

    builder->state = SecPathBuilderGetNext;
    builder->completed = completed;
    builder->context = context;
}

SecPathBuilderRef SecPathBuilderCreate(CFDataRef clientAuditToken,
    CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly,
    bool keychainsAllowed, CFArrayRef policies, CFArrayRef ocspResponses,
    CFArrayRef signedCertificateTimestamps, CFArrayRef trustedLogs,
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups,
    SecPathBuilderCompleted completed, const void *context) {
    SecPathBuilderRef builder = malloc(sizeof(*builder));
    memset(builder, 0, sizeof(*builder));
    SecPathBuilderInit(builder, clientAuditToken, certificates,
        anchors, anchorsOnly, keychainsAllowed, policies, ocspResponses,
        signedCertificateTimestamps, trustedLogs, verifyTime,
        accessGroups, completed, context);
    return builder;
}

static void SecPathBuilderDestroy(SecPathBuilderRef builder) {
    secdebug("alloc", "%p", builder);
    dispatch_release_null(builder->queue);
    if (builder->anchorSource) {
        SecMemoryCertificateSourceDestroy(builder->anchorSource); }
    if (builder->certificateSource) {
        SecMemoryCertificateSourceDestroy(builder->certificateSource); }
    if (builder->itemCertificateSource) {
        SecItemCertificateSourceDestroy(builder->itemCertificateSource); }
    if (builder->appleAnchorSource) {
        SecMemoryCertificateSourceDestroy(builder->appleAnchorSource); }
	CFReleaseSafe(builder->clientAuditToken);
	CFReleaseSafe(builder->anchorSources);
	CFReleaseSafe(builder->parentSources);
	CFReleaseSafe(builder->allPaths);
	CFReleaseSafe(builder->partialPaths);
	CFReleaseSafe(builder->rejectedPaths);
	CFReleaseSafe(builder->candidatePaths);
	CFReleaseSafe(builder->leafDetails);
    CFReleaseSafe(builder->ocspResponses);
    CFReleaseSafe(builder->signedCertificateTimestamps);
    CFReleaseSafe(builder->trustedLogs);

    SecPVCDelete(&builder->path);
}

bool SecPathBuilderCanAccessNetwork(SecPathBuilderRef builder) {
    return builder->canAccessNetwork;
}

void SecPathBuilderSetCanAccessNetwork(SecPathBuilderRef builder, bool allow) {
    if (builder->canAccessNetwork != allow) {
        builder->canAccessNetwork = allow;
        if (allow) {
#if !TARGET_OS_WATCH
            secinfo("http", "network access re-enabled by policy");
            /* re-enabling network_access re-adds kSecCAIssuerSource as
               a parent source. */
            CFArrayAppendValue(builder->parentSources, kSecCAIssuerSource);
#else
            secnotice("http", "network access not allowed on WatchOS");
            builder->canAccessNetwork = false;
#endif
        } else {
            secinfo("http", "network access disabled by policy");
            /* disabling network_access removes kSecCAIssuerSource from
               the list of parent sources. */
            CFIndex ix = CFArrayGetFirstIndexOfValue(builder->parentSources,
                CFRangeMake(0, CFArrayGetCount(builder->parentSources)),
                kSecCAIssuerSource);
            if (ix >= 0)
                CFArrayRemoveValueAtIndex(builder->parentSources, ix);
        }
    }
}

CFArrayRef SecPathBuilderCopyOCSPResponses(SecPathBuilderRef builder)
{
    return CFRetainSafe(builder->ocspResponses);
}

CFArrayRef SecPathBuilderCopySignedCertificateTimestamps(SecPathBuilderRef builder)
{
    return CFRetainSafe(builder->signedCertificateTimestamps);
}

CFArrayRef SecPathBuilderCopyTrustedLogs(SecPathBuilderRef builder)
{
    return CFRetainSafe(builder->trustedLogs);
}

SecCertificatePathRef SecPathBuilderGetBestPath(SecPathBuilderRef builder)
{
    return builder->bestPath;
}

/* This function assumes that the input source is an anchor source */
static bool SecPathBuilderIsAnchorPerConstraints(SecPathBuilderRef builder, SecCertificateSourceRef source,
    SecCertificateRef certificate) {
    bool result = false;
    CFArrayRef constraints = NULL;
    constraints = SecCertificateSourceCopyUsageConstraints(source, certificate);

    /* Unrestricted certificates:
     *      -those that come from anchor sources with no constraints
     *      -self-signed certificates with empty contraints arrays
     */
    Boolean selfSigned = false;
    require(errSecSuccess == SecCertificateIsSelfSigned(certificate, &selfSigned), out);
    if ((NULL == source->copyUsageConstraints) ||
        (constraints && (CFArrayGetCount(constraints) == 0) && selfSigned)) {
        secinfo("trust", "unrestricted anchor%s",
                (NULL == source->copyUsageConstraints) ? " source" : "");
        result = true;
        goto out;
    }

    /* Get the trust settings result for the PVC */
    require(constraints, out);
    SecTrustSettingsResult settingsResult = kSecTrustSettingsResultInvalid;
    settingsResult = SecPVCGetTrustSettingsResult(&builder->path,
                                                  certificate,
                                                  constraints);
    if ((selfSigned && settingsResult == kSecTrustSettingsResultTrustRoot) ||
        (!selfSigned && settingsResult == kSecTrustSettingsResultTrustAsRoot)) {
        // For our purposes, this is an anchor.
        secinfo("trust", "complex trust settings anchor");
        result = true;
    }

    if (settingsResult == kSecTrustSettingsResultDeny) {
        /* We consider denied certs "anchors" because the trust decision
           is set regardless of building the chain further. The policy
           validation will handle rejecting this chain. */
        secinfo("trust", "complex trust settings denied anchor");
        result = true;
    }

out:
    CFReleaseNull(constraints);
    return result;
}

/* Source returned in foundInSource has the same lifetime as the builder. */
static bool SecPathBuilderIsAnchor(SecPathBuilderRef builder,
    SecCertificateRef certificate, SecCertificateSourceRef *foundInSource) {
    /* We look through the anchor sources in order. They are ordered in
       SecPathBuilderInit so that process anchors override user anchors which
       override system anchors. */
    CFIndex count = CFArrayGetCount(builder->anchorSources);
    CFIndex ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateSourceRef source = (SecCertificateSourceRef)
        CFArrayGetValueAtIndex(builder->anchorSources, ix);
        if (SecCertificateSourceContains(source, certificate)) {
            if (foundInSource)
                *foundInSource = source;
            if (SecPathBuilderIsAnchorPerConstraints(builder, source, certificate)) {
                return true;
            }
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
		}
        /* The path is not partial if the last cert is self-signed. */
        if ((SecCertificatePathSelfSignedIndex(path) >= 0) &&
            (SecCertificatePathSelfSignedIndex(path) == SecCertificatePathGetCount(path)-1)) {
            return false;
        }
	}

	return true;
}

/* Given the builder, a partial chain partial and the parents array, construct
   a SecCertificatePath for each parent.  After discarding previously
   considered paths and paths with cycles, sort out which array each path
   should go in, if any. */
static void SecPathBuilderProcessParents(SecPathBuilderRef builder,
    SecCertificatePathRef partial, CFArrayRef parents) {
    CFIndex rootIX = SecCertificatePathGetCount(partial) - 1;
    CFIndex num_parents = parents ? CFArrayGetCount(parents) : 0;
    CFIndex parentIX;
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
        SecCertificateSourceRef source = NULL;
        bool is_anchor = SecPathBuilderIsAnchor(builder, parent, &source);
        CFArrayRef constraints = (source) ? SecCertificateSourceCopyUsageConstraints(source, parent) : NULL;
        SecCertificatePathRef path = SecCertificatePathCreate(partial, parent, constraints);
        CFReleaseSafe(constraints);
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
                secdebug("trust", "Adding partial for parent %" PRIdCFIndex "/%" PRIdCFIndex " %@",
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
    SecPathBuilderProcessParents(builder, partial, parents);

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
            secdebug("trust", "broading search to %" PRIdCFIndex "/%" PRIdCFIndex " sources",
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
        secdebug("trust", "re-checking %" PRIdCFIndex " partials", builder->partialIX + 1);
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
    secdebug("trust", "looking for parents of partial %" PRIdCFIndex "/%" PRIdCFIndex ": %@",
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
            secdebug("trust", "searching anchor source %" PRIdCFIndex "/%" PRIdCFIndex, sourceIX + 1,
                     num_anchor_sources);
        } else {
            CFIndex parentIX = sourceIX - num_anchor_sources;
            source = (SecCertificateSourceRef)
                CFArrayGetValueAtIndex(builder->parentSources, parentIX);
            secdebug("trust", "searching parent source %" PRIdCFIndex "/%" PRIdCFIndex, parentIX + 1,
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

    CFIndex bestPathScore = builder->bestPathScore;
    CFIndex score = SecCertificatePathScore(builder->path.path,
        SecPVCGetVerifyTime(&builder->path));

    /* The current chain is valid for EV, but revocation checking failed.  We
       replace any previously accepted or rejected non EV chains with the
       current one. */
    if (pvc->is_ev && !builder->bestPathIsEV) {
        bestPathScore = 0;
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

    /* Do this last so that changes to bestPathScore above will take effect. */
    if (!builder->bestPath || score > bestPathScore) {
        if (builder->bestPath) {
            secinfo("reject",
                "replacing %sev %s score: %ld with %sev score: %" PRIdCFIndex " %@",
                (builder->bestPathIsEV ? "" : "non "),
                (builder->bestPathScore > ACCEPT_PATH_SCORE ? "accept" : "reject"),
                builder->bestPathScore,
                (pvc->is_ev ? "" : "non "), (long)score, builder->path.path);
        } else {
            secinfo("reject", "%sev score: %" PRIdCFIndex " %@",
                (pvc->is_ev ? "" : "non "), score, builder->path.path);
        }

		builder->bestPathScore = score;
        builder->bestPath = pvc->path;
        builder->bestPathIsEV = pvc->is_ev;
        builder->denyBestPath = SecPVCCheckUsageConstraints(pvc);
	} else {
        secinfo("reject", "%sev score: %" PRIdCFIndex " lower than %" PRIdCFIndex " %@",
            (pvc->is_ev ? "" : "non "), score, bestPathScore, builder->path.path);
    }
}

/* All policies accepted the candidate path. */
static void SecPathBuilderAccept(SecPathBuilderRef builder) {
    if (!builder) { return; }
    SecPVCRef pvc = &builder->path;
    if (!pvc) { return; }
    bool isSHA2 = !SecCertificatePathHasWeakHash(pvc->path);
    bool isOptionallySHA2 = !SecCertificateIsWeakHash(SecPVCGetCertificateAtIndex(pvc, 0));
    CFIndex bestScore = builder->bestPathScore;
    /* Score this path. Note that all points awarded or deducted in
     * SecCertificatePathScore are < 100,000 */
    CFIndex currScore = (SecCertificatePathScore(pvc->path, pvc->verifyTime) +
                         ACCEPT_PATH_SCORE  + // 10,000,000 points for accepting
                         ((pvc->is_ev) ? 1000000 : 0)); //1,000,000 points for EV
    if (currScore > bestScore) {
        // current path is better than existing best path
        secinfo("accept", "replacing %sev %s score: %ld with %sev score: %" PRIdCFIndex " %@",
                 (builder->bestPathIsEV ? "" : "non "),
                 (builder->bestPathScore > ACCEPT_PATH_SCORE ? "accept" : "reject"),
                 builder->bestPathScore,
                 (pvc->is_ev ? "" : "non "), (long)currScore, builder->path.path);

        builder->bestPathScore = currScore;
        builder->bestPathIsEV = pvc->is_ev;
        builder->bestPathIsSHA2 = isSHA2;
        builder->bestPath = pvc->path;
        builder->denyBestPath = SecPVCCheckUsageConstraints(pvc); /* should always be false */
    }

    /* If we found the best accept we can, we want to switch directly to the
       SecPathBuilderComputeDetails state here, since we're done. */
    if ((pvc->is_ev || !pvc->optionally_ev) && (isSHA2 || !isOptionallySHA2))
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
    // foobar
    SecPVCRef pvc = &builder->path;
#if 0
    if (!builder->caller_wants_details) {
        SecPVCSetPath(pvc, builder->bestPath, NULL);
        pvc->result = builder->bestPathScore > ACCEPT_PATH_SCORE;
        builder->state = SecPathBuilderReportResult;
        return true;
    }
#endif
    CFIndex ix, pathLength = SecCertificatePathGetCount(builder->bestPath);
    CFMutableArrayRef details = CFArrayCreateMutableCopy(kCFAllocatorDefault,
        pathLength, builder->leafDetails);
    CFRetainSafe(details);
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
        SecPVCGrayListedKeyChecks(pvc, ix);
        SecPVCBlackListedKeyChecks(pvc, ix);
    }
    builder->state = SecPathBuilderReportResult;
    bool completed = SecPVCPathChecks(pvc);

    /* Reject the certificate if it was accepted before but we failed it now. */
    if (builder->bestPathScore > ACCEPT_PATH_SCORE && !pvc->result) {
        builder->bestPathScore = 0;
    }

    /* Accept a partial path if certificate is on the allow list
       and is temporally valid and passed all PVC checks. */
    if (completed && pvc->is_allowlisted && pvc->result &&
        builder->bestPathScore < ACCEPT_PATH_SCORE &&
        SecCertificatePathIsValid(pvc->path, pvc->verifyTime)) {
        builder->bestPathScore += ACCEPT_PATH_SCORE;
    }

    CFReleaseSafe(details);

    return completed;
}

static bool SecPathBuilderReportResult(SecPathBuilderRef builder) {
    SecPVCRef pvc = &builder->path;
    bool haveRevocationResponse = false;
    if (pvc->info && pvc->is_ev && pvc->result) {
        CFDictionarySetValue(pvc->info, kSecTrustInfoExtendedValidationKey,
            kCFBooleanTrue); /* iOS key */
        CFDictionarySetValue(pvc->info, kSecTrustExtendedValidation,
            kCFBooleanTrue); /* unified API key */
        SecCertificateRef leaf = SecPVCGetCertificateAtIndex(pvc, 0);
        CFStringRef leafCompanyName = SecCertificateCopyCompanyName(leaf);
        if (leafCompanyName) {
            CFDictionarySetValue(pvc->info, kSecTrustInfoCompanyNameKey,
                leafCompanyName); /* iOS key */
            CFDictionarySetValue(pvc->info, kSecTrustOrganizationName,
                leafCompanyName); /* unified API key */
            CFRelease(leafCompanyName);
        }
        if (pvc->rvcs) {
            CFAbsoluteTime nextUpdate = SecPVCGetEarliestNextUpdate(pvc);
            if (nextUpdate == 0) {
                /* populate revocation info for failed revocation check */
                CFDictionarySetValue(pvc->info, kSecTrustInfoRevocationKey,
                    kCFBooleanFalse); /* iOS key */
                CFDictionarySetValue(pvc->info, kSecTrustRevocationChecked,
                    kCFBooleanFalse); /* unified API key */
            }
        }
    }

    if (pvc->info && pvc->result && pvc->rvcs) {
        CFAbsoluteTime nextUpdate = SecPVCGetEarliestNextUpdate(pvc);
        if (nextUpdate != 0) {
            /* always populate revocation info for successful revocation check */
            haveRevocationResponse = true;
            CFDateRef validUntil = CFDateCreate(kCFAllocatorDefault, nextUpdate);
            CFDictionarySetValue(pvc->info, kSecTrustInfoRevocationValidUntilKey,
                                 validUntil); /* iOS key */
            CFDictionarySetValue(pvc->info, kSecTrustRevocationValidUntilDate,
                                 validUntil); /* unified API key */
            CFRelease(validUntil);
            CFDictionarySetValue(pvc->info, kSecTrustInfoRevocationKey,
                                 kCFBooleanTrue); /* iOS key */
            CFDictionarySetValue(pvc->info, kSecTrustRevocationChecked,
                                 kCFBooleanTrue); /* unified API key */
        }
    }

    if (pvc->info && pvc->result && pvc->response_required && !haveRevocationResponse) {
        builder->bestPathScore = 0;
        SecPVCSetResultForced(pvc, kSecPolicyCheckRevocationResponseRequired,
            0, kCFBooleanFalse, true);
    }

    if (pvc->info && pvc->is_ct && pvc->result) {
        CFDictionarySetValue(pvc->info, kSecTrustInfoCertificateTransparencyKey,
                             kCFBooleanTrue);
    }

    if (pvc->info && pvc->is_ct_whitelisted && pvc->result) {
        CFDictionarySetValue(pvc->info, kSecTrustInfoCertificateTransparencyWhiteListKey,
                             kCFBooleanTrue);
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

    SecTrustResultType result  = kSecTrustResultInvalid;
    if (builder->denyBestPath) {
        result = kSecTrustResultDeny;
    } else if (builder->bestPathScore > ACCEPT_PATH_SCORE) {
        result = kSecTrustResultUnspecified;
    } else {
        result = kSecTrustResultRecoverableTrustFailure;
    }

    secinfo("trust", "completed: %@ details: %@ result: %d",
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

dispatch_queue_t SecPathBuilderGetQueue(SecPathBuilderRef builder) {
    return (builder) ? builder->queue : NULL;
}

CFDataRef SecPathBuilderCopyClientAuditToken(SecPathBuilderRef builder) {
    return (builder) ? (CFDataRef)CFRetainSafe(builder->clientAuditToken) : NULL;
}

// MARK: -
// MARK: SecTrustServer
/********************************************************
 ****************** SecTrustServer **********************
 ********************************************************/

typedef void (^SecTrustServerEvaluationCompleted)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef error);

static void
SecTrustServerEvaluateCompleted(const void *userData,
                                SecCertificatePathRef chain, CFArrayRef details, CFDictionaryRef info,
                                SecTrustResultType result) {
    SecTrustServerEvaluationCompleted evaluated = (SecTrustServerEvaluationCompleted)userData;
    evaluated(result, details, info, chain, NULL);
    Block_release(evaluated);
}

void
SecTrustServerEvaluateBlock(CFDataRef clientAuditToken, CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, void (^evaluated)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef error)) {
    /* We need an array containing at least one certificate to proceed. */
    if (!isArray(certificates) || !(CFArrayGetCount(certificates) > 0)) {
        CFErrorRef certError = CFErrorCreate(NULL, kCFErrorDomainOSStatus, errSecInvalidCertificate, NULL);
        evaluated(kSecTrustResultInvalid, NULL, NULL, NULL, certError);
        CFReleaseSafe(certError);
        return;
    }
    SecTrustServerEvaluationCompleted userData = Block_copy(evaluated);
    /* Call the actual evaluator function. */
    SecPathBuilderRef builder = SecPathBuilderCreate(clientAuditToken,
                                                     certificates, anchors,
                                                     anchorsOnly, keychainsAllowed, policies,
                                                     responses, SCTs, trustedLogs,
                                                     verifyTime, accessGroups,
                                                     SecTrustServerEvaluateCompleted, userData);
    dispatch_async(builder->queue, ^{ SecPathBuilderStep(builder); });
}


// NO_SERVER Shim code only, xpc interface should call SecTrustServerEvaluateBlock() directly
SecTrustResultType SecTrustServerEvaluate(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef *pdetails, CFDictionaryRef *pinfo, SecCertificatePathRef *pchain, CFErrorRef *perror) {
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block SecTrustResultType result = kSecTrustResultInvalid;
    SecTrustServerEvaluateBlock(NULL, certificates, anchors, anchorsOnly, keychainsAllowed, policies, responses, SCTs, trustedLogs, verifyTime, accessGroups, ^(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef error) {
        result = tr;
        if (tr == kSecTrustResultInvalid) {
            if (perror) {
                *perror = error;
                CFRetainSafe(error);
            }
        } else {
            if (pdetails) {
                *pdetails = details;
                CFRetainSafe(details);
            }
            if (pinfo) {
                *pinfo = info;
                CFRetainSafe(info);
            }
            if (pchain) {
                *pchain = chain;
                CFRetainSafe(chain);
            }
        }
        dispatch_semaphore_signal(done);
    });
    dispatch_semaphore_wait(done, DISPATCH_TIME_FOREVER);

    return result;
}
