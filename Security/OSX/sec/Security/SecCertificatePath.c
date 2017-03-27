/*
 * Copyright (c) 2007-2010,2012-2016 Apple Inc. All Rights Reserved.
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
#include <libDER/oidsPriv.h>
#include <utilities/debugging.h>
#include <Security/SecInternal.h>
#include <AssertMacros.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>

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

	/* Usage constraints derived from trust settings. */
	CFMutableArrayRef	usageConstraints;

	SecCertificateRef	certificates[];
};

CFGiblisWithHashFor(SecCertificatePath)

static void SecCertificatePathDestroy(CFTypeRef cf) {
	SecCertificatePathRef certificatePath = (SecCertificatePathRef) cf;
	CFIndex ix;
	for (ix = 0; ix < certificatePath->count; ++ix) {
		CFRelease(certificatePath->certificates[ix]);
    }
    CFRelease(certificatePath->usageConstraints);
}

static Boolean SecCertificatePathCompare(CFTypeRef cf1, CFTypeRef cf2) {
	SecCertificatePathRef cp1 = (SecCertificatePathRef) cf1;
	SecCertificatePathRef cp2 = (SecCertificatePathRef) cf2;
	if (cp1->count != cp2->count)
		return false;
	CFIndex ix;
	for (ix = 0; ix < cp1->count; ++ix) {
		if (!CFEqual(cp1->certificates[ix], cp2->certificates[ix]))
			return false;
	}
	if (!CFEqual(cp1->usageConstraints, cp2->usageConstraints))
		return false;

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
	hashCode += CFHash(certificatePath->usageConstraints);
	return hashCode;
}

static CFStringRef SecCertificatePathCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
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

/* Create a new certificate path from an old one. */
SecCertificatePathRef SecCertificatePathCreate(SecCertificatePathRef path,
	SecCertificateRef certificate, CFArrayRef usageConstraints) {
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

	CFArrayRef emptyArray = NULL;
	if (!usageConstraints) {
		require_action_quiet(emptyArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks), exit, CFReleaseNull(result));
		usageConstraints = emptyArray;
	}
	CFMutableArrayRef constraints;
	if (path) {
		require_action_quiet(constraints = CFArrayCreateMutableCopy(kCFAllocatorDefault, count, path->usageConstraints), exit, CFReleaseNull(result));
	} else {
		require_action_quiet(constraints = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit, CFReleaseNull(result));
	}
	CFArrayAppendValue(constraints, usageConstraints);
	result->usageConstraints = constraints;

exit:
    CFReleaseSafe(emptyArray);
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
    CFMutableArrayRef constraints;
    require_action_quiet(constraints = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit, SecError(errSecAllocate, error, CFSTR("failed to create constraints")); CFReleaseNull(result));

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = count;
	result->selfIssued = -1;
	result->isSelfSigned = false;
	result->isAnchored = false;
	result->usageConstraints = constraints;

	size_t ix;
	for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = SecCertificateCreateWithXPCArrayAtIndex(xpc_path, ix, error);
        if (certificate) {
            result->certificates[ix] = certificate;
            CFArrayRef emptyArray;
            require_action_quiet(emptyArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks), exit, SecError(errSecAllocate, error, CFSTR("failed to create emptyArray")); CFReleaseNull(result));
            CFArrayAppendValue(result->usageConstraints, emptyArray);
            CFRelease(emptyArray);
        } else {
            result->count = ix; // total allocated
            CFReleaseNull(result);
            break;
        }
	}

exit:
    return result;
}

SecCertificatePathRef SecCertificatePathCreateDeserialized(CFArrayRef certificates, CFErrorRef *error) {
    SecCertificatePathRef result = NULL;
    require_action_quiet(isArray(certificates), exit,
                         SecError(errSecParam, error, CFSTR("certificates is not an array")));
    size_t count = 0;
    require_action_quiet(count = CFArrayGetCount(certificates), exit,
                         SecError(errSecDecode, error, CFSTR("certificates array count == 0")));
    size_t size = sizeof(struct SecCertificatePath) + count * sizeof(SecCertificateRef);
    require_action_quiet(result = (SecCertificatePathRef)_CFRuntimeCreateInstance(kCFAllocatorDefault, SecCertificatePathGetTypeID(), size - sizeof(CFRuntimeBase), 0), exit,
                         SecError(errSecDecode, error, CFSTR("_CFRuntimeCreateInstance returned NULL")));
    CFMutableArrayRef constraints;
    require_action_quiet(constraints = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create constraints")); CFReleaseNull(result));

    result->count = count;
    result->nextParentSource = 0;
    result->lastVerifiedSigner = count;
    result->selfIssued = -1;
    result->isSelfSigned = false;
    result->isAnchored = false;
    result->usageConstraints = constraints;

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef certificate = SecCertificateCreateWithData(NULL, CFArrayGetValueAtIndex(certificates, ix));
        if (certificate) {
            result->certificates[ix] = certificate;
            CFArrayRef emptyArray;
            require_action_quiet(emptyArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks), exit,
                                 SecError(errSecAllocate, error, CFSTR("failed to create emptyArray")); CFReleaseNull(result));
            CFArrayAppendValue(result->usageConstraints, emptyArray);
            CFRelease(emptyArray);
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

	CFMutableArrayRef constraints;
	require_action_quiet(constraints = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit, CFReleaseNull(result));

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = lastVerifiedSigner;
	result->selfIssued = selfIssued;
	result->isSelfSigned = isSelfSigned;
	result->isAnchored = path->isAnchored;
	result->usageConstraints = constraints;
	CFIndex ix;
	for (ix = 0; ix < count; ++ix) {
		CFIndex pathIX = ix + skipCount;
		result->certificates[ix] = path->certificates[pathIX];
		CFRetain(result->certificates[ix]);
		CFArrayAppendValue(result->usageConstraints, CFArrayGetValueAtIndex(path->usageConstraints, pathIX));
	}

exit:
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

	CFMutableArrayRef constraints;
	require_action_quiet(constraints = CFArrayCreateMutableCopy(kCFAllocatorDefault, count, path->usageConstraints), exit, CFReleaseNull(result));

	result->count = count;
	result->nextParentSource = 0;
	result->lastVerifiedSigner = lastVerifiedSigner;
	result->selfIssued = selfIssued;
	result->isSelfSigned = isSelfSigned;
	result->isAnchored = path->isAnchored;
	result->usageConstraints = constraints;
	CFIndex ix;
	for (ix = 1; ix < count; ++ix) {
		result->certificates[ix] = path->certificates[ix - 1];
		CFRetain(result->certificates[ix]);
	}
	result->certificates[0] = leaf;
	CFRetain(leaf);

	CFArrayRef emptyArray;
	require_action_quiet(emptyArray = CFArrayCreate(kCFAllocatorDefault, NULL, 0, &kCFTypeArrayCallBacks), exit, CFReleaseNull(result));
	CFArrayInsertValueAtIndex(result->usageConstraints, 0, emptyArray);
	CFRelease(emptyArray);

exit:
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

/* Create an array of SecCertificateRefs from a certificate path. */
CFArrayRef SecCertificatePathCopyCertificates(SecCertificatePathRef path, CFErrorRef *error) {
    CFMutableArrayRef outCerts = NULL;
    size_t ix, count = path->count;
    require_action_quiet(outCerts = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecParam, error, CFSTR("CFArray failed to create")));
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(path, ix);
        if (cert) {
            CFArrayAppendValue(outCerts, cert);
        }
   }
exit:
    return outCerts;
}

CFArrayRef SecCertificatePathCreateSerialized(SecCertificatePathRef path, CFErrorRef *error) {
    CFMutableArrayRef serializedCerts = NULL;
    require_quiet(path, exit);
    size_t ix, count = path->count;
    require_action_quiet(serializedCerts = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecParam, error, CFSTR("CFArray failed to create")));
    for (ix = 0; ix < count; ++ix) {
        SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(path, ix);
        CFDataRef certData = SecCertificateCopyData(cert);
        if (certData) {
            CFArrayAppendValue(serializedCerts, certData);
            CFRelease(certData);
        }
    }
exit:
    return serializedCerts;
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

    /* now check that the selfIssued cert was actually self-signed */
    if (certificatePath->selfIssued >= 0 && !certificatePath->isSelfSigned) {
        SecCertificateRef cert = certificatePath->certificates[certificatePath->selfIssued];
        Boolean isSelfSigned = false;
        OSStatus status = SecCertificateIsSelfSigned(cert, &isSelfSigned);
        if ((status == errSecSuccess) && isSelfSigned) {
            certificatePath->isSelfSigned = true;
        } else {
            certificatePath->selfIssued = -1;
        }
    }
}

void SecCertificatePathSetIsAnchored(
	SecCertificatePathRef certificatePath) {
    secdebug("trust", "%@ is anchored", certificatePath);
	certificatePath->isAnchored = true;

    /* Now check if that anchor (last cert) was actually self-signed.
     * In the non-anchor case, this is handled by SecCertificatePathSetSelfIssued.
     * Because anchored chains immediately go into the candidate bucket in the trust
     * server, we need to ensure that the self-signed/self-issued members are set
     * for the purposes of scoring. */
    if (!certificatePath->isSelfSigned && certificatePath->count > 0) {
        SecCertificateRef cert = certificatePath->certificates[certificatePath->count - 1];
        Boolean isSelfSigned = false;
        OSStatus status = SecCertificateIsSelfSigned(cert, &isSelfSigned);
        if ((status == errSecSuccess) && isSelfSigned) {
            certificatePath->isSelfSigned = true;
            if (certificatePath->selfIssued == -1) {
                certificatePath->selfIssued = certificatePath->count - 1;
            }
        }
    }
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
	check(certificatePath && ix >= 0 && ix < certificatePath->count);
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
#if TARGET_OS_OSX
    return SecCertificateCopyPublicKey_ios(certificate);
#else
    return SecCertificateCopyPublicKey(certificate);
#endif
}

CFArrayRef SecCertificatePathGetUsageConstraintsAtIndex(
    SecCertificatePathRef certificatePath, CFIndex ix) {
    return (CFArrayRef)CFArrayGetValueAtIndex(certificatePath->usageConstraints, ix);
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

	return kSecPathVerifySuccess;
}

bool SecCertificatePathIsValid(SecCertificatePathRef certificatePath, CFAbsoluteTime verifyTime) {
    CFIndex ix;
    for (ix = 0; ix < certificatePath->count; ++ix) {
        if (!SecCertificateIsValid(certificatePath->certificates[ix],
                                   verifyTime))
            return false;
    }
    return true;
}

bool SecCertificatePathHasWeakHash(SecCertificatePathRef certificatePath) {
    CFIndex ix, count = certificatePath->count;

    if (SecCertificatePathIsAnchored(certificatePath)) {
        /* For anchored paths, don't check the hash algorithm of the anchored cert,
         * since we already decided to trust it. */
        count--;
    }
    for (ix = 0; ix < count; ++ix) {
        if (SecCertificateIsWeakHash(certificatePath->certificates[ix])) {
            return true;
        }
    }
    return false;
}

bool SecCertificatePathHasWeakKeySize(SecCertificatePathRef certificatePath) {
    CFDictionaryRef keySizes = NULL;
    CFNumberRef rsaSize = NULL, ecSize = NULL;
    bool result = true;

    /* RSA key sizes are 2048-bit or larger. EC key sizes are P-224 or larger. */
    require(rsaSize = CFNumberCreateWithCFIndex(NULL, 2048), errOut);
    require(ecSize = CFNumberCreateWithCFIndex(NULL, 224), errOut);
    const void *keys[] = { kSecAttrKeyTypeRSA, kSecAttrKeyTypeEC };
    const void *values[] = { rsaSize, ecSize };
    require(keySizes = CFDictionaryCreate(NULL, keys, values, 2,
                                          &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    CFIndex ix;
    for (ix = 0; ix < certificatePath->count; ++ix) {
        if (!SecCertificateIsAtLeastMinKeySize(certificatePath->certificates[ix],
                                               keySizes)) {
            result = true;
            goto errOut;
        }
    }
    result = false;

errOut:
    CFReleaseSafe(keySizes);
    CFReleaseSafe(rsaSize);
    CFReleaseSafe(ecSize);
    return result;
}

/* Return a score for this certificate chain. */
CFIndex SecCertificatePathScore(
	SecCertificatePathRef certificatePath, CFAbsoluteTime verifyTime) {
	CFIndex score = 0;

    /* Paths that don't verify score terribly.c */
    if (certificatePath->lastVerifiedSigner != certificatePath->count - 1) {
        secdebug("trust", "lvs: %" PRIdCFIndex " count: %" PRIdCFIndex,
                 certificatePath->lastVerifiedSigner, certificatePath->count);
        score -= 100000;
    }

	if (certificatePath->isAnchored) {
		/* Anchored paths for the win! */
		score += 10000;
	}

    if (certificatePath->isSelfSigned && (certificatePath->selfIssued == certificatePath->count - 1)) {
        /* Chains that terminate in a self-signed certificate are preferred,
         even if they don't end in an anchor. */
        score += 1000;
        /* Shorter chains ending in a self-signed cert are preferred. */
        score -= 1 * certificatePath->count;
    } else {
        /* Longer chains are preferred when the chain doesn't end in a self-signed cert. */
        score += 1 * certificatePath->count;
    }

    if (SecCertificatePathIsValid(certificatePath, verifyTime)) {
        score += 100;
    }

    if (!SecCertificatePathHasWeakHash(certificatePath)) {
        score += 10;
    }

    if (!SecCertificatePathHasWeakKeySize(certificatePath)) {
        score += 10;
    }

	return score;
}
