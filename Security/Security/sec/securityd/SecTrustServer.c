/*
 * Copyright (c) 2006-2010,2012-2014 Apple Inc. All Rights Reserved.
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
#include <securityd/SecTrustStoreServer.h>
#include <securityd/SecCAIssuerRequest.h>
#include <securityd/SecItemServer.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecDispatchRelease.h>

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
#include <limits.h>
#include <Security/SecBase.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <utilities/debugging.h>
#include <utilities/SecCFWrappers.h>
#include <Security/SecInternal.h>
#include <ipc/securityd_client.h>
#include <CommonCrypto/CommonDigest.h>
#include "OTATrustUtilities.h"


/********************************************************
 ***************** OTA Trust support ********************
 ********************************************************/


#ifndef SECITEM_SHIM_OSX

static CFArrayRef subject_to_anchors(CFDataRef nic);
static CFArrayRef CopyCertsFromIndices(CFArrayRef offsets);

static CFArrayRef subject_to_anchors(CFDataRef nic)
{
    CFArrayRef result = NULL;

    if (NULL == nic)
    {
        return result;
    }

	SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL == otapkiref)
	{
		return result;
	}
	
	CFDictionaryRef lookupTable = SecOTAPKICopyAnchorLookupTable(otapkiref);
	CFRelease(otapkiref);

	if (NULL == lookupTable)
	{
		return result;
	}

    unsigned char subject_digest[CC_SHA1_DIGEST_LENGTH];
    memset(subject_digest, 0, CC_SHA1_DIGEST_LENGTH);

    (void)CC_SHA1(CFDataGetBytePtr(nic), (CC_LONG)CFDataGetLength(nic), subject_digest);
    CFDataRef sha1Digest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, subject_digest, CC_SHA1_DIGEST_LENGTH, kCFAllocatorNull);


    result = (CFArrayRef)CFDictionaryGetValue(lookupTable, sha1Digest);
	CFReleaseSafe(lookupTable);
    CFReleaseSafe(sha1Digest);

    return result;
}

static CFArrayRef CopyCertDataFromIndices(CFArrayRef offsets)
{
    CFMutableArrayRef result = NULL;

	SecOTAPKIRef otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
	if (NULL == otapkiref)
	{
		return result;
	}
	
	const char* anchorTable = SecOTAPKIGetAnchorTable(otapkiref);	
	if (NULL == anchorTable)
	{
		CFReleaseSafe(otapkiref);
		return result;
	}

	CFIndex num_offsets = CFArrayGetCount(offsets);
	
	result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
	
	for (CFIndex idx = 0; idx < num_offsets; idx++)
    {
		CFNumberRef offset = (CFNumberRef)CFArrayGetValueAtIndex(offsets, idx);
		uint32_t offset_value = 0;
		if (CFNumberGetValue(offset, kCFNumberSInt32Type, &offset_value))
		{
			char* pDataPtr = (char *)(anchorTable + offset_value);
			//int32_t record_length = *((int32_t * )pDataPtr);
			//record_length = record_length;
			pDataPtr += sizeof(uint32_t);

			int32_t cert_data_length = *((int32_t * )pDataPtr);
			pDataPtr += sizeof(uint32_t);

			CFDataRef cert_data = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, (const UInt8 *)pDataPtr,
                                                              cert_data_length, kCFAllocatorNull);
			if (NULL != cert_data)
			{
                CFArrayAppendValue(result, cert_data);
                CFReleaseSafe(cert_data);
            }
		}
	}
	CFReleaseSafe(otapkiref);
	return result;
}

static CFArrayRef CopyCertsFromIndices(CFArrayRef offsets)
{
	CFMutableArrayRef result = NULL;

    CFArrayRef cert_data_array = CopyCertDataFromIndices(offsets);

    if (NULL != cert_data_array)
    {
        result = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        CFIndex num_cert_datas = CFArrayGetCount(cert_data_array);
        for (CFIndex idx = 0; idx < num_cert_datas; idx++)
        {
            CFDataRef cert_data = (CFDataRef)CFArrayGetValueAtIndex(cert_data_array, idx);
            if (NULL != cert_data)
            {
                SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, cert_data);
                if (NULL != cert)
                {
                    CFArrayAppendValue(result, cert);
                    CFRelease(cert);
                }
            }
        }
        CFRelease(cert_data_array);
    }
    return result;

}
#endif

/********************************************************
 *************** END OTA Trust support ******************
 ********************************************************/

#define MAX_CHAIN_LENGTH  15

/* Forward declaration for use in SecCertificateSource. */
static void SecPathBuilderExtendPaths(void *context, CFArrayRef parents);


// MARK: -
// MARK: SecCertificateSource
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

// MARK: -
// MARK: SecItemCertificateSource
/********************************************************
 *********** SecItemCertificateSource object ************
 ********************************************************/
struct SecItemCertificateSource {
	struct SecCertificateSource base;
	CFArrayRef accessGroups;
};
typedef struct SecItemCertificateSource *SecItemCertificateSourceRef;

static CFTypeRef SecItemCertificateSourceResultsPost(CFTypeRef raw_results) {
    if (isArray(raw_results)) {
        CFMutableArrayRef result = CFArrayCreateMutable(kCFAllocatorDefault, CFArrayGetCount(raw_results), &kCFTypeArrayCallBacks);
        CFArrayForEach(raw_results, ^(const void *value) {
            SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, value);
            if (cert) {
                CFArrayAppendValue(result, cert);
				CFRelease(cert);
			}
        });
        return result;
    } else if (isData(raw_results)) {
        return SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)raw_results);
    }
    return NULL;
}

static bool SecItemCertificateSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
	SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    /* FIXME: Search for things other than just subject of our issuer if we
       have a subjectID or authorityKeyIdentifier. */
    CFDataRef normalizedIssuer =
        SecCertificateGetNormalizedIssuerContent(certificate);
    const void *keys[] = {
        kSecClass,
        kSecReturnData,
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
    CFErrorRef localError = NULL;
    if (!_SecItemCopyMatching(query, msource->accessGroups, &results, &localError)) {
        if (CFErrorGetCode(localError) != errSecItemNotFound) {
            secdebug("trust", "_SecItemCopyMatching: %@", localError);
        }
        CFRelease(localError);
    }
    CFRelease(query);
    CFTypeRef certs = SecItemCertificateSourceResultsPost(results);
    CFReleaseSafe(results);
    callback(context, certs);
    CFReleaseSafe(certs);
    return true;
}

static bool SecItemCertificateSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
	SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
    /* Lookup a certificate by issuer and serial number. */
    CFDataRef normalizedSubject =
        SecCertificateGetNormalizedSubjectContent(certificate);
    CFDataRef serialNumber =
        SecCertificateCopySerialNumber(certificate);
    const void *keys[] = {
        kSecClass,
        kSecMatchLimit,
        kSecAttrIssuer,
		kSecAttrSerialNumber
    },
    *values[] = {
        kSecClassCertificate,
        kSecMatchLimitOne,
        normalizedSubject,
		serialNumber
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 5,
        NULL, NULL);
    CFErrorRef localError = NULL;
    CFTypeRef results = NULL;
    bool ok = _SecItemCopyMatching(query, msource->accessGroups, &results, &localError);
    CFRelease(query);
    CFRelease(serialNumber);
    CFReleaseSafe(results);
    if (!ok) {
        if (CFErrorGetCode(localError) != errSecItemNotFound) {
            secdebug("trust", "_SecItemCopyMatching: %@", localError);
        }
        CFRelease(localError);
		return false;
    }
    return true;
}

static SecCertificateSourceRef SecItemCertificateSourceCreate(CFArrayRef accessGroups) {
	SecItemCertificateSourceRef result = (SecItemCertificateSourceRef)malloc(sizeof(*result));
	result->base.copyParents = SecItemCertificateSourceCopyParents;
	result->base.contains = SecItemCertificateSourceContains;
	result->accessGroups = accessGroups;
    CFRetainSafe(accessGroups);
	return (SecCertificateSourceRef)result;
}

static void SecItemCertificateSourceDestroy(SecCertificateSourceRef source) {
	SecItemCertificateSourceRef msource = (SecItemCertificateSourceRef)source;
	CFReleaseSafe(msource->accessGroups);
	free(msource);
}

// MARK: -
// MARK: SecSystemAnchorSource
/********************************************************
 *********** SecSystemAnchorSource object ************
 ********************************************************/

static bool SecSystemAnchorSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
#ifndef SECITEM_SHIM_OSX
    CFArrayRef parents = NULL;
	CFArrayRef anchors = NULL;
	SecOTAPKIRef otapkiref = NULL;
	
    CFDataRef nic = SecCertificateGetNormalizedIssuerContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
       It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */

	otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
	require_quiet(otapkiref, errOut);
	anchors = subject_to_anchors(nic);
	require_quiet(anchors, errOut);
	parents = CopyCertsFromIndices(anchors);

errOut:
    callback(context, parents);
    CFReleaseSafe(parents);
	CFReleaseSafe(otapkiref);
#endif
    return true;
}

/* Quick thought: we can eliminate this method if we search anchor sources
   before all others and we remember if we got a cert from an anchorsource. */
static bool SecSystemAnchorSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
	bool result = false;
#ifndef SECITEM_SHIM_OSX
	CFArrayRef anchors = NULL;
	SecOTAPKIRef otapkiref = NULL;
	CFArrayRef cert_datas = NULL;
	
    CFDataRef nic = SecCertificateGetNormalizedSubjectContent(certificate);
    /* 64 bits cast: the worst that can happen here is we truncate the length and match an actual anchor.
     It does not matter since we would be returning the wrong anchors */
    assert((unsigned long)CFDataGetLength(nic)<UINT_MAX); /* Debug check. correct as long as CFIndex is signed long */
    
	otapkiref = SecOTAPKICopyCurrentOTAPKIRef();
	require_quiet(otapkiref, errOut);
    anchors = subject_to_anchors(nic);
	require_quiet(anchors, errOut);
    cert_datas = CopyCertDataFromIndices(anchors);
    require_quiet(cert_datas, errOut);
  
    CFIndex cert_length = SecCertificateGetLength(certificate);
    const UInt8 *cert_data_ptr = SecCertificateGetBytePtr(certificate);
    
    CFIndex num_cert_datas = CFArrayGetCount(cert_datas);
    for (CFIndex idx = 0; idx < num_cert_datas; idx++)
    {
        CFDataRef cert_data = (CFDataRef)CFArrayGetValueAtIndex(cert_datas, idx);
               
		if (NULL != cert_data)
		{
            if (CFGetTypeID(cert_data) == CFDataGetTypeID())
            {
                CFIndex  aCert_Length = CFDataGetLength(cert_data);
                const UInt8*  aCert_Data_Ptr = CFDataGetBytePtr(cert_data);
                
                if (aCert_Length == cert_length)
                {
                    if (!memcmp(cert_data_ptr, aCert_Data_Ptr, cert_length))
                    {
						result = true;
						break;
                    }
                }
            }
		}
    }

errOut:
	CFReleaseSafe(cert_datas);
	CFReleaseSafe(otapkiref);
#endif
    return result;
}



struct SecCertificateSource kSecSystemAnchorSource = {
	SecSystemAnchorSourceCopyParents,
	SecSystemAnchorSourceContains
};

// MARK: -
// MARK: SecUserAnchorSource
/********************************************************
 *********** SecUserAnchorSource object ************
 ********************************************************/
static bool SecUserAnchorSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = SecTrustStoreCopyParents(
        SecTrustStoreForDomain(kSecTrustStoreDomainUser), certificate, NULL);
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

// MARK: -
// MARK: SecMemoryCertificateSource
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

// MARK: -
// MARK: SecCAIssuerCertificateSource
/********************************************************
 ********* SecCAIssuerCertificateSource object **********
 ********************************************************/
static bool SecCAIssuerCertificateSourceCopyParents(
	SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    return SecCAIssuerCopyParents(certificate, SecPathBuilderGetQueue((SecPathBuilderRef)context), context, callback);
}

static bool SecCAIssuerCertificateSourceContains(
    SecCertificateSourceRef source, SecCertificateRef certificate) {
	return false;
}

struct SecCertificateSource kSecCAIssuerSource = {
	SecCAIssuerCertificateSourceCopyParents,
	SecCAIssuerCertificateSourceContains
};

// MARK: -
// MARK: SecPathBuilder
/********************************************************
 *************** SecPathBuilder object ******************
 ********************************************************/
struct SecPathBuilder {
    dispatch_queue_t queue;
	SecCertificateSourceRef	certificateSource;
	SecCertificateSourceRef	itemCertificateSource;
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
    CFArrayRef policies, CFAbsoluteTime verifyTime, CFArrayRef accessGroups,
    SecPathBuilderCompleted completed, const void *context) {
    secdebug("alloc", "%p", builder);
	CFAllocatorRef allocator = kCFAllocatorDefault;

    builder->queue = dispatch_queue_create("builder", DISPATCH_QUEUE_SERIAL);

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
    builder->itemCertificateSource = SecItemCertificateSourceCreate(accessGroups);
	CFArrayAppendValue(builder->parentSources, builder->itemCertificateSource);
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
    CFAbsoluteTime verifyTime, CFArrayRef accessGroups,
    SecPathBuilderCompleted completed, const void *context) {
    SecPathBuilderRef builder = malloc(sizeof(*builder));
    SecPathBuilderInit(builder, certificates, anchors, anchorsOnly,
        policies, verifyTime, accessGroups, completed, context);
    return builder;
}

static void SecPathBuilderDestroy(SecPathBuilderRef builder) {
    secdebug("alloc", "%p", builder);
    dispatch_release_null(builder->queue);
	if (builder->anchorSource)
		SecMemoryCertificateSourceDestroy(builder->anchorSource);
	if (builder->certificateSource)
		SecMemoryCertificateSourceDestroy(builder->certificateSource);
    if (builder->itemCertificateSource)
        SecItemCertificateSourceDestroy(builder->itemCertificateSource);
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
    secdebug("trust", "found %" PRIdCFIndex " candidate %s", num_parents,
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
                "replacing %sev %s score: %ld with %sev reject score: %" PRIdCFIndex " %@",
                (builder->bestPathIsEV ? "" : "non "),
                (builder->rejectScore == INTPTR_MAX ? "accept" : "reject"),
                builder->rejectScore,
                (pvc->is_ev ? "" : "non "), (long)score, builder->path.path);
        } else {
            secdebug("reject", "%sev reject score: %" PRIdCFIndex " %@",
                (pvc->is_ev ? "" : "non "), score, builder->path.path);
        }

		builder->rejectScore = score;
        builder->bestPath = pvc->path;
        builder->bestPathIsEV = pvc->is_ev;
	} else {
        secdebug("reject", "%sev reject score: %" PRIdCFIndex " lower than %" PRIdCFIndex " %@",
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
    // foobar
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
    if (builder->rejectScore == INTPTR_MAX && !pvc->result) {
        builder->rejectScore = 0;
    }

    CFReleaseSafe(details);

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

dispatch_queue_t SecPathBuilderGetQueue(SecPathBuilderRef builder) {
    return builder->queue;
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
SecTrustServerEvaluateBlock(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, void (^evaluated)(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef error)) {
    SecTrustServerEvaluationCompleted userData = Block_copy(evaluated);
    /* Call the actual evaluator function. */
    SecPathBuilderRef builder = SecPathBuilderCreate(certificates, anchors,
                                                     anchorsOnly, policies,
                                                     verifyTime, accessGroups,
                                                     SecTrustServerEvaluateCompleted, userData);
    dispatch_async(builder->queue, ^{ SecPathBuilderStep(builder); });
}


// NO_SERVER Shim code only, xpc interface should call SecTrustServerEvaluateBlock() directly
SecTrustResultType SecTrustServerEvaluate(CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef *pdetails, CFDictionaryRef *pinfo, SecCertificatePathRef *pchain, CFErrorRef *perror) {
    dispatch_semaphore_t done = dispatch_semaphore_create(0);
    __block SecTrustResultType result = kSecTrustResultInvalid;
    SecTrustServerEvaluateBlock(certificates, anchors, anchorsOnly, policies, verifyTime, accessGroups, ^(SecTrustResultType tr, CFArrayRef details, CFDictionaryRef info, SecCertificatePathRef chain, CFErrorRef error) {
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
