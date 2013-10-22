/*
 * Copyright (c) 2007-2010 Apple Inc. All Rights Reserved.
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
 * SecCertificatePath.c - CoreFoundation based certificate path object
 */

#include "SecCertificatePath.h"

#include <Security/SecTrust.h>
#include <Security/SecTrustStore.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <utilities/SecIOFormat.h>
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
#include <Security/SecBase.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <utilities/debugging.h>
#include <Security/SecInternal.h>
#include <AssertMacros.h>
#include <utilities/SecCFError.h>

// MARK: -
// MARK: SecCertificatePath
/********************************************************
 ************* SecCertificatePath object ****************
 ********************************************************/
struct SecCertificatePath {
    CFRuntimeBase		_base;
	CFIndex				count;

	/* Index of next parent source to search for parents. */
	CFIndex				nextParentSource;

	/* Index of last certificate in chain who's signature has been verified.
	   0 means nothing has been checked.  1 means the leaf has been verified
	   against it's issuer, etc. */
	CFIndex				lastVerifiedSigner;

	/* Index of first self issued certificate in the chain.  -1 mean there is
	   none.  0 means the leaf is self signed.  */
	CFIndex				selfIssued;

	/* True iff cert at index selfIssued does in fact self verify. */
	bool				isSelfSigned;

	/* True if the root of this path is a trusted anchor.
	   FIXME get rid of this since it's a property of the evaluation, not a
	   static feature of a certificate path? */
	bool				isAnchored;
	SecCertificateRef	certificates[];
};

/* CFRuntime regsitration data. */
static pthread_once_t kSecCertificatePathRegisterClass = PTHREAD_ONCE_INIT;
static CFTypeID kSecCertificatePathTypeID = _kCFRuntimeNotATypeID;

static void SecCertificatePathDestroy(CFTypeRef cf) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
	CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
		CFRelease(certificatePath->certificates[ix]);
    }
}

static Boolean SecCertificatePathEqual(CFTypeRef cf1, CFTypeRef cf2) {
	SecCertificatePathRef cp1 = (SecCertificatePathRef) cf1;
	SecCertificatePathRef cp2 = (SecCertificatePathRef) cf2;
	if (cp1->count != cp2->count)
		return false;
	CFIndex ix;
	for (ix = 0; ix < cp1->count; ++ix) {
		if (!CFEqual(cp1->certificates[ix], cp2->certificates[ix]))
			return false;
	}

	return true;
}

static CFHashCode SecCertificatePathHash(CFTypeRef cf) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
	CFHashCode hashCode = 0;
	// hashCode = 31 * SecCertificatePathGetTypeID();
	CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
		hashCode += CFHash(certificatePath->certificates[ix]);
	}
	return hashCode;
}

static CFStringRef SecCertificateCopyPathDescription(CFTypeRef cf) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef typeStr = CFCopyTypeIDDescription(CFGetTypeID(cf));
    CFStringAppendFormat(desc, NULL,
        CFSTR("<%@ lvs: %" PRIdCFIndex " certs: "), typeStr,
        certificatePath->lastVerifiedSigner);
    CFRelease(typeStr);
    CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
        if (ix > 0) {
            CFStringAppend(desc, CFSTR(", "));
        }
        CFStringRef str = CFCopyDescription(certificatePath->certificates[ix]);
        CFStringAppend(desc, str);
        CFRelease(str);
	}
    CFStringAppend(desc, CFSTR(" >"));

    return desc;
}

static void SecCertificatePathRegisterClass(void) {
	static const CFRuntimeClass kSecCertificatePathClass = {
		0,												/* version */
        "SecCertificatePath",							/* class name */
		NULL,											/* init */
		NULL,											/* copy */
		SecCertificatePathDestroy,						/* dealloc */
		SecCertificatePathEqual,						/* equal */
		SecCertificatePathHash,							/* hash */
		NULL,											/* copyFormattingDesc */
		SecCertificateCopyPathDescription				/* copyDebugDesc */
	};

    kSecCertificatePathTypeID =
		_CFRuntimeRegisterClass(&kSecCertificatePathClass);
}

/* SecCertificatePath API functions. */
CFTypeID SecCertificatePathGetTypeID(void) {
    pthread_once(&kSecCertificatePathRegisterClass,
		SecCertificatePathRegisterClass);
    return kSecCertificatePathTypeID;
}

/* Create a new certificate path from an old one. */
SecCertificatePathRef SecCertificatePathCreate(SecCertificatePathRef path,
	SecCertificateRef certificate) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
	check(certificate);
	CFIndex count;
	CFIndex selfIssued, lastVerifiedSigner;
	bool isSelfSigned;
	if (path) {
		count = path->count + 1;
		lastVerifiedSigner = path->lastVerifiedSigner;
		selfIssued = path->selfIssued;
		isSelfSigned = path->isSelfSigned;
	} else {
		count = 1;
		lastVerifiedSigner = 0;
		selfIssued = -1;
		isSelfSigned = false;
	}

    CFIndex size = sizeof(struct SecCertificatePath) +
		count * sizeof(SecCertificateRef);
    SecCertificatePathRef result =
		(SecCertificatePathRef)_CFRuntimeCreateInstance(allocator,
		SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (!result)
        return NULL;

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = lastVerifiedSigner;
	result->selfIssued = selfIssued;
	result->isSelfSigned = isSelfSigned;
	result->isAnchored = false;
	CFIndex ix;
	for (ix = 0; ix < count - 1; ++ix) {
		result->certificates[ix] = path->certificates[ix];
		CFRetain(result->certificates[ix]);
	}
	result->certificates[count - 1] = certificate;
	CFRetainSafe(certificate);

    return result;
}

/* Create a new certificate path from an xpc_array of data. */
SecCertificatePathRef SecCertificatePathCreateWithXPCArray(xpc_object_t xpc_path, CFErrorRef *error) {
    SecCertificatePathRef result = NULL;
    require_action_quiet(xpc_path, exit, SecError(errSecParam, error, CFSTR("xpc_path is NULL")));
    require_action_quiet(xpc_get_type(xpc_path) == XPC_TYPE_ARRAY, exit, SecError(errSecDecode, error, CFSTR("xpc_path value is not an array")));
    size_t count;
    require_action_quiet(count = xpc_array_get_count(xpc_path), exit, SecError(errSecDecode, error, CFSTR("xpc_path array count == 0")));
    size_t size = sizeof(struct SecCertificatePath) + count * sizeof(SecCertificateRef);
    require_action_quiet(result = (SecCertificatePathRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0), exit, SecError(errSecDecode, error, CFSTR("_CFRuntimeCreateInstance returned NULL")));

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = count;
	result->selfIssued = -1;
	result->isSelfSigned = false;
	result->isAnchored = false;
	size_t ix;
	for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = SecCertificateCreateWithXPCArrayAtIndex(xpc_path, ix, error);
        if (certificate) {
            result->certificates[ix] = certificate;
        } else {
            result->count = ix; // total allocated
            CFReleaseNull(result);
            break;
        }
	}

exit:
    return result;
}

SecCertificatePathRef SecCertificatePathCopyFromParent(
    SecCertificatePathRef path, CFIndex skipCount) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
	CFIndex count;
	CFIndex selfIssued, lastVerifiedSigner;
	bool isSelfSigned;

    /* Ensure we are at least returning a path of length 1. */
    if (skipCount < 0 || path->count < 1 + skipCount)
        return NULL;

    count = path->count - skipCount;
    lastVerifiedSigner = path->lastVerifiedSigner > skipCount
        ? path->lastVerifiedSigner - skipCount : 0;
    selfIssued = path->selfIssued >= skipCount
        ? path->selfIssued - skipCount : -1;
    isSelfSigned = path->selfIssued >= 0 ? path->isSelfSigned : false;

    CFIndex size = sizeof(struct SecCertificatePath) +
		count * sizeof(SecCertificateRef);
    SecCertificatePathRef result =
		(SecCertificatePathRef)_CFRuntimeCreateInstance(allocator,
		SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (!result)
        return NULL;

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = lastVerifiedSigner;
	result->selfIssued = selfIssued;
	result->isSelfSigned = isSelfSigned;
	result->isAnchored = path->isAnchored;
	CFIndex ix;
	for (ix = 0; ix < count; ++ix) {
		result->certificates[ix] = path->certificates[ix + skipCount];
		CFRetain(result->certificates[ix]);
	}

    return result;
}

SecCertificatePathRef SecCertificatePathCopyAddingLeaf(SecCertificatePathRef path,
    SecCertificateRef leaf) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
	CFIndex count;
	CFIndex selfIssued, lastVerifiedSigner;
	bool isSelfSigned;

    /* First make sure the new leaf is signed by path's current leaf. */
    SecKeyRef issuerKey = SecCertificatePathCopyPublicKeyAtIndex(path, 0);
    if (!issuerKey)
        return NULL;
    OSStatus status = SecCertificateIsSignedBy(leaf, issuerKey);
    CFRelease(issuerKey);
    if (status)
        return NULL;

    count = path->count + 1;
    lastVerifiedSigner = path->lastVerifiedSigner + 1;
    selfIssued = path->selfIssued;
    isSelfSigned = path->isSelfSigned;

    CFIndex size = sizeof(struct SecCertificatePath) +
		count * sizeof(SecCertificateRef);
    SecCertificatePathRef result =
		(SecCertificatePathRef)_CFRuntimeCreateInstance(allocator,
		SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (!result)
        return NULL;

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = lastVerifiedSigner;
	result->selfIssued = selfIssued;
	result->isSelfSigned = isSelfSigned;
	result->isAnchored = path->isAnchored;
	CFIndex ix;
	for (ix = 1; ix < count; ++ix) {
		result->certificates[ix] = path->certificates[ix - 1];
		CFRetain(result->certificates[ix]);
	}
	result->certificates[0] = leaf;
	CFRetain(leaf);

    return result;
}

/* Create an array of CFDataRefs from a certificate path. */
xpc_object_t SecCertificatePathCopyXPCArray(SecCertificatePathRef path, CFErrorRef *error) {
    xpc_object_t xpc_chain = NULL;
    size_t ix, count = path->count;
    require_action_quiet(xpc_chain = xpc_array_create(NULL, 0), exit, SecError(errSecParam, error, CFSTR("xpc_array_create failed")));
	for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(path, ix);
        if (!SecCertificateAppendToXPCArray(cert, xpc_chain, error)) {
            xpc_release(xpc_chain);
            return NULL;
        }
    }

exit:
    return xpc_chain;
}

/* Record the fact that we found our own root cert as our parent
   certificate. */
void SecCertificatePathSetSelfIssued(
	SecCertificatePathRef certificatePath) {
	if (certificatePath->selfIssued >= 0) {
		secdebug("trust", "%@ is already issued at %" PRIdCFIndex, certificatePath,
			certificatePath->selfIssued);
		return;
	}
    secdebug("trust", "%@ is self issued", certificatePath);
	certificatePath->selfIssued = certificatePath->count - 1;
}

void SecCertificatePathSetIsAnchored(
	SecCertificatePathRef certificatePath) {
    secdebug("trust", "%@ is anchored", certificatePath);
	certificatePath->isAnchored = true;
}

/* Return the index of the first non anchor certificate in the chain that is
   self signed counting from the leaf up.  Return -1 if there is none. */
CFIndex SecCertificatePathSelfSignedIndex(
	SecCertificatePathRef certificatePath) {
	if (certificatePath->isSelfSigned)
		return certificatePath->selfIssued;
	return -1;
}

Boolean SecCertificatePathIsAnchored(
	SecCertificatePathRef certificatePath) {
	return certificatePath->isAnchored;
}

void SecCertificatePathSetNextSourceIndex(
	SecCertificatePathRef certificatePath, CFIndex sourceIndex) {
	certificatePath->nextParentSource = sourceIndex;
}

CFIndex SecCertificatePathGetNextSourceIndex(
	SecCertificatePathRef certificatePath) {
	return certificatePath->nextParentSource;
}

CFIndex SecCertificatePathGetCount(
	SecCertificatePathRef certificatePath) {
	check(certificatePath);
	return certificatePath ? certificatePath->count : 0;
}

SecCertificateRef SecCertificatePathGetCertificateAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix) {
	check(certificatePath);
	check(ix >= 0 && ix < certificatePath->count);
	return certificatePath->certificates[ix];
}

CFIndex SecCertificatePathGetIndexOfCertificate(SecCertificatePathRef path,
    SecCertificateRef certificate) {
    CFIndex ix, count = path->count;
	for (ix = 0; ix < count; ++ix) {
        if (CFEqual(path->certificates[ix], certificate))
            return ix;
	}
    return kCFNotFound;
}

#if 0
/* Return the leaf certificate for certificatePath. */
SecCertificateRef SecCertificatePathGetLeaf(
	SecCertificatePathRef certificatePath) {
	return SecCertificatePathGetCertificateAtIndex(certificatePath, 0);
}
#endif

/* Return the root certificate for certificatePath.  Note that root is just
   the top of the path as far as it is constructed.  It may or may not be
   trusted or self signed.  */
SecCertificateRef SecCertificatePathGetRoot(
	SecCertificatePathRef certificatePath) {
	return SecCertificatePathGetCertificateAtIndex(certificatePath,
		SecCertificatePathGetCount(certificatePath) - 1);
}

SecKeyRef SecCertificatePathCopyPublicKeyAtIndex(
	SecCertificatePathRef certificatePath, CFIndex ix) {
	SecCertificateRef certificate =
        SecCertificatePathGetCertificateAtIndex(certificatePath, ix);
	const DERAlgorithmId *algId =
		SecCertificateGetPublicKeyAlgorithm(certificate);
    const DERItem *params = NULL;
    if (algId->params.length != 0) {
        params = &algId->params;
    } else {
        CFIndex count = certificatePath->count;
        for (++ix; ix < count; ++ix) {
            certificate = certificatePath->certificates[ix];
            const DERAlgorithmId *chain_algId =
                SecCertificateGetPublicKeyAlgorithm(certificate);
            if (!DEROidCompare(&algId->oid, &chain_algId->oid)) {
                /* Algorithm oids differ, params stay NULL. */
                break;
            }
            if (chain_algId->params.length != 0) {
                params = &chain_algId->params;
                break;
            }
        }
    }
	const DERItem *keyData = SecCertificateGetPublicKeyData(certificate);
    SecAsn1Oid oid1 = { .Data = algId->oid.data, .Length = algId->oid.length };
    SecAsn1Item params1 = {
        .Data = params ? params->data : NULL,
        .Length = params ? params->length : 0
    };
    SecAsn1Item keyData1 = {
        .Data = keyData ? keyData->data : NULL,
        .Length = keyData ? keyData->length : 0
    };
    return SecKeyCreatePublicFromDER(kCFAllocatorDefault, &oid1, &params1,
        &keyData1);
}

SecPathVerifyStatus SecCertificatePathVerify(
	SecCertificatePathRef certificatePath) {
	check(certificatePath);
    if (!certificatePath)
        return kSecPathVerifyFailed;
	for (;
		certificatePath->lastVerifiedSigner < certificatePath->count - 1;
		++certificatePath->lastVerifiedSigner) {
		SecKeyRef issuerKey =
			SecCertificatePathCopyPublicKeyAtIndex(certificatePath,
				certificatePath->lastVerifiedSigner + 1);
		if (!issuerKey)
			return kSecPathVerifiesUnknown;
		OSStatus status = SecCertificateIsSignedBy(
			certificatePath->certificates[certificatePath->lastVerifiedSigner],
			issuerKey);
		CFRelease(issuerKey);
		if (status) {
			return kSecPathVerifyFailed;
		}
	}

	if (certificatePath->selfIssued >= 0 && !certificatePath->isSelfSigned) {
		SecKeyRef issuerKey =
			SecCertificatePathCopyPublicKeyAtIndex(certificatePath,
				certificatePath->selfIssued);
		if (!issuerKey) {
			certificatePath->selfIssued = -1;
		} else {
			OSStatus status = SecCertificateIsSignedBy(
				certificatePath->certificates[certificatePath->selfIssued],
				issuerKey);
			CFRelease(issuerKey);
			if (!status) {
				certificatePath->isSelfSigned = true;
			} else {
				certificatePath->selfIssued = -1;
			}
		}
	}

	return kSecPathVerifySuccess;
}

/* Return a score for this certificate chain. */
CFIndex SecCertificatePathScore(
	SecCertificatePathRef certificatePath, CFAbsoluteTime verifyTime) {
	CFIndex score = 0;
	if (certificatePath->isAnchored) {
		/* Anchored paths for the win! */
		score += 10000;
	}

	/* Score points for each certificate in the chain. */
	score += 10 * certificatePath->count;

	if (certificatePath->isSelfSigned) {
		/* If there is a self signed certificate at the end ofthe chain we
		   count it as an extra certificate.  If there is one in the middle
		   of the chain we count it for half. */
		if (certificatePath->selfIssued == certificatePath->count - 1)
			score += 10;
		else
			score += 5;
	}

	/* Paths that don't verify score terribly. */
	if (certificatePath->lastVerifiedSigner != certificatePath->count - 1) {
		secdebug("trust", "lvs: %" PRIdCFIndex " count: %" PRIdCFIndex,
			certificatePath->lastVerifiedSigner, certificatePath->count);
		score -= 100000;
	}

	/* Subtract 1 point for each not valid certificate, make sure we
       subtract less than the amount we add per certificate, since
       regardless of temporal validity we still prefer longer chains
       to shorter ones.  This distinction is just to ensure that when
       everything else is equal we prefer the chain with the most
       certificates that are valid at the given verifyTime. */
	CFIndex ix;
	for (ix = 0; ix < certificatePath->count - 1; ++ix) {
		if (!SecCertificateIsValid(certificatePath->certificates[ix],
			verifyTime))
			score -= 1;
	}

	return score;
}
