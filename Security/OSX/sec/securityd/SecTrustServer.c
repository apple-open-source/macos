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
#include <securityd/SecTrustStoreServer.h>
#include <securityd/SecCAIssuerRequest.h>
#include <securityd/SecItemServer.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecDispatchRelease.h>
#include <utilities/SecAppleAnchorPriv.h>

#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePath.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrustSettingsPriv.h>
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
#include "personalization.h"
#include <utilities/SecInternalReleasePriv.h>


/********************************************************
 ***************** OTA Trust support ********************
 ********************************************************/


//#ifndef SECITEM_SHIM_OSX

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
//#endif // SECITEM_SHIM_OSX

/********************************************************
 *************** END OTA Trust support ******************
 ********************************************************/

#define MAX_CHAIN_LENGTH  15
#define ACCEPT_PATH_SCORE 10000000

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
typedef CFArrayRef(*CopyConstraints)(SecCertificateSourceRef source,
	SecCertificateRef certificate);
typedef bool(*Contains)(SecCertificateSourceRef source,
	SecCertificateRef certificate);

struct SecCertificateSource {
	CopyParents		copyParents;
	CopyConstraints	copyUsageConstraints;
	Contains		contains;
};

static bool SecCertificateSourceCopyParents(SecCertificateSourceRef source,
    SecCertificateRef certificate,
    void *context, SecCertificateSourceParents callback) {
    return source->copyParents(source, certificate, context, callback);
}

static CFArrayRef SecCertificateSourceCopyUsageConstraints(
	SecCertificateSourceRef source, SecCertificateRef certificate) {
    if (source->copyUsageConstraints) {
        return source->copyUsageConstraints(source, certificate);
    } else {
        return NULL;
    }
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
    SecurityClient client = {
        .task = NULL,
        .accessGroups = msource->accessGroups,
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
    };

    /* We can make this async or run this on a queue now easily. */
    CFErrorRef localError = NULL;
    if (!_SecItemCopyMatching(query, &client, &results, &localError)) {
        if (localError && (CFErrorGetCode(localError) != errSecItemNotFound)) {
            secdebug("trust", "_SecItemCopyMatching: %@", localError);
        }
        CFReleaseSafe(localError);
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
    /* Look up a certificate by issuer and serial number. */
    CFDataRef normalizedIssuer = SecCertificateGetNormalizedIssuerContent(certificate);
    CFRetainSafe(normalizedIssuer);
    CFDataRef serialNumber =
    #if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
        SecCertificateCopySerialNumber(certificate, NULL);
    #else
        SecCertificateCopySerialNumber(certificate);
    #endif
    const void *keys[] = {
        kSecClass,
        kSecMatchLimit,
        kSecAttrIssuer,
        kSecAttrSerialNumber
    },
    *values[] = {
        kSecClassCertificate,
        kSecMatchLimitOne,
        normalizedIssuer,
        serialNumber
    };
    SecurityClient client = {
        .task = NULL,
        .accessGroups = msource->accessGroups,
        .allowSystemKeychain = true,
        .allowSyncBubbleKeychain = false,
        .isNetworkExtension = false,
    };
    CFDictionaryRef query = CFDictionaryCreate(NULL, keys, values, 4, NULL, NULL);
    CFErrorRef localError = NULL;
    CFTypeRef results = NULL;
    bool ok = _SecItemCopyMatching(query, &client, &results, &localError);
    CFReleaseSafe(query);
    CFReleaseSafe(serialNumber);
    CFReleaseSafe(normalizedIssuer);
    CFReleaseSafe(results);
    if (!ok) {
        if (CFErrorGetCode(localError) != errSecItemNotFound) {
            secdebug("trust", "_SecItemCopyMatching: %@", localError);
        }
        CFReleaseSafe(localError);
        return false;
    }
    return true;
}

static SecCertificateSourceRef SecItemCertificateSourceCreate(CFArrayRef accessGroups) {
	SecItemCertificateSourceRef result = (SecItemCertificateSourceRef)malloc(sizeof(*result));
	result->base.copyParents = SecItemCertificateSourceCopyParents;
	result->base.copyUsageConstraints = NULL;
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
//#ifndef SECITEM_SHIM_OSX
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
//#endif // SECITEM_SHIM_OSX
    return true;
}

/* Quick thought: we can eliminate this method if we search anchor sources
   before all others and we remember if we got a cert from an anchorsource. */
static bool SecSystemAnchorSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
	bool result = false;
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
    return result;
}



struct SecCertificateSource kSecSystemAnchorSource = {
	SecSystemAnchorSourceCopyParents,
	NULL,
	SecSystemAnchorSourceContains
};

#if TARGET_OS_IPHONE
// MARK: -
// MARK: SecUserAnchorSource
/********************************************************
 ************* SecUserAnchorSource object ***************
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

static CFArrayRef SecUserAnchorSourceCopyUsageConstraints(
	SecCertificateSourceRef source, SecCertificateRef certificate) {
    CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
    if (!digest)
        return NULL;
    CFArrayRef usageConstraints = NULL;
    bool ok = _SecTrustStoreCopyUsageConstraints(
        SecTrustStoreForDomain(kSecTrustStoreDomainUser), digest, &usageConstraints, NULL);
    return (ok) ? usageConstraints : NULL;
}

static bool SecUserAnchorSourceContains(SecCertificateSourceRef source,
	SecCertificateRef certificate) {
    return SecTrustStoreContains(
        SecTrustStoreForDomain(kSecTrustStoreDomainUser), certificate);
}

struct SecCertificateSource kSecUserAnchorSource = {
	SecUserAnchorSourceCopyParents,
	SecUserAnchorSourceCopyUsageConstraints,
	SecUserAnchorSourceContains
};
#endif

// MARK: -
// MARK: SecMemoryCertificateSource
/********************************************************
 ********** SecMemoryCertificateSource object ***********
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
	CFArrayRef parents = (normalizedIssuer) ? CFDictionaryGetValue(msource->subjects,
		normalizedIssuer) : NULL;
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
	result->base.copyUsageConstraints = NULL;
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
	NULL,
	SecCAIssuerCertificateSourceContains
};

#if (SECTRUST_OSX && TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#include <Security/SecItemPriv.h>
// MARK: -
// MARK: SecLegacyCertificateSource
/********************************************************
 ********** SecLegacyCertificateSource object ***********
 ********************************************************/

static bool SecLegacyCertificateSourceCopyParents(
    SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    CFArrayRef parents = SecItemCopyParentCertificates(certificate, NULL);
    callback(context, parents);
    CFReleaseSafe(parents);
    return true;
}

static bool SecLegacyCertificateSourceContains(
    SecCertificateSourceRef source, SecCertificateRef certificate) {
    SecCertificateRef cert = SecItemCopyStoredCertificate(certificate, NULL);
    bool result = (cert) ? true : false;
    CFReleaseSafe(cert);
    return result;
}

struct SecCertificateSource kSecLegacyCertificateSource = {
    SecLegacyCertificateSourceCopyParents,
    NULL,
    SecLegacyCertificateSourceContains
};
#endif /* SecLegacyCertificateSource */

#if (SECTRUST_OSX && TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
// MARK: -
// MARK: SecLegacyAnchorSource
/********************************************************
 ************ SecLegacyAnchorSource object **************
 ********************************************************/

static bool SecLegacyAnchorSourceCopyParents(
    SecCertificateSourceRef source, SecCertificateRef certificate,
        void *context, SecCertificateSourceParents callback) {
    CFMutableArrayRef anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFArrayRef parents = SecItemCopyParentCertificates(certificate, NULL);
    CFArrayRef trusted = NULL;
    if (parents == NULL) {
        goto finish;
    }
    /* Get the custom anchors which have been trusted in the user and admin domains.
     * We don't need system domain roots here, since SecSystemAnchorSource provides those.
     */
    OSStatus status = SecTrustSettingsCopyCertificatesForUserAdminDomains(&trusted);
    if (status == errSecSuccess && trusted) {
        CFIndex index, count = CFArrayGetCount(parents);
        for (index = 0; index < count; index++) {
            SecCertificateRef parent = (SecCertificateRef)CFArrayGetValueAtIndex(parents, index);
            if (parent && CFArrayContainsValue(trusted, CFRangeMake(0, CFArrayGetCount(trusted)), parent)) {
                CFArrayAppendValue(anchors, parent);
            }
        }
    }

finish:
    callback(context, anchors);
    CFReleaseSafe(anchors);
    CFReleaseSafe(parents);
    CFReleaseSafe(trusted);
    return true;
}

static CFArrayRef SecLegacyAnchorSourceCopyUsageConstraints(
	SecCertificateSourceRef source, SecCertificateRef certificate) {

    CFArrayRef result = NULL;
    CFArrayRef userTrustSettings = NULL, adminTrustSettings = NULL;

    OSStatus status = SecTrustSettingsCopyTrustSettings(certificate,
                                                        kSecTrustSettingsDomainUser,
                                                        &userTrustSettings);
    if ((status == errSecSuccess) && (userTrustSettings != NULL)) {
       result = CFRetain(userTrustSettings);
    }

    status = SecTrustSettingsCopyTrustSettings(certificate,
                                               kSecTrustSettingsDomainAdmin,
                                               &adminTrustSettings);
    /* user trust settings overrule admin trust settings */
    if ((status == errSecSuccess) && (adminTrustSettings != NULL) && (result == NULL)) {
        result = CFRetain(adminTrustSettings);
    }

    CFReleaseNull(userTrustSettings);
    CFReleaseNull(adminTrustSettings);
	return result;
}

static bool SecLegacyAnchorSourceContains(
    SecCertificateSourceRef source, SecCertificateRef certificate) {
    if (certificate == NULL) {
        return false;
    }
    CFArrayRef trusted = NULL;
    bool result = false;
    OSStatus status = SecTrustSettingsCopyCertificatesForUserAdminDomains(&trusted);
    if ((status == errSecSuccess) && (trusted != NULL)) {
        CFIndex index, count = CFArrayGetCount(trusted);
        for (index = 0; index < count; index++) {
            SecCertificateRef anchor = (SecCertificateRef)CFArrayGetValueAtIndex(trusted, index);
            if (anchor && CFEqual(anchor, certificate)) {
                result = true;
                break;
            }
        }
    }
    CFReleaseSafe(trusted);
    return result;
}

struct SecCertificateSource kSecLegacyAnchorSource = {
    SecLegacyAnchorSourceCopyParents,
    SecLegacyAnchorSourceCopyUsageConstraints,
    SecLegacyAnchorSourceContains
};
#endif /* SecLegacyAnchorSource */

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
        if (kSecLegacyCertificateSource.contains && kSecLegacyCertificateSource.copyParents) {
            CFArrayAppendValue(builder->parentSources, &kSecLegacyCertificateSource);
        }
 #endif
    }
    if (anchorsOnly) {
        /* Add the Apple, system, and user anchor certificate db to the search list
         if we don't explicitly trust them. */
        CFArrayAppendValue(builder->parentSources, builder->appleAnchorSource);
        CFArrayAppendValue(builder->parentSources, &kSecSystemAnchorSource);
 #if TARGET_OS_IPHONE
        CFArrayAppendValue(builder->parentSources, &kSecUserAnchorSource);
 #endif
    }
    if (keychainsAllowed && builder->canAccessNetwork) {
        CFArrayAppendValue(builder->parentSources, &kSecCAIssuerSource);
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
        CFArrayAppendValue(builder->anchorSources, &kSecUserAnchorSource);
 #else /* TARGET_OS_OSX */
        if (keychainsAllowed && kSecLegacyAnchorSource.contains && kSecLegacyAnchorSource.copyParents) {
            CFArrayAppendValue(builder->anchorSources, &kSecLegacyAnchorSource);
        }
 #endif
        CFArrayAppendValue(builder->anchorSources, &kSecSystemAnchorSource);
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
            CFArrayAppendValue(builder->parentSources, &kSecCAIssuerSource);
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
                &kSecCAIssuerSource);
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
    check(builder);
    SecPVCRef pvc = &builder->path;
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
       and is temporally valid. */
    if (completed && pvc->is_allowlisted &&
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
    if (builder->bestPathScore > ACCEPT_PATH_SCORE) {
        result = kSecTrustResultUnspecified;
    } else if (builder->denyBestPath) {
        result = kSecTrustResultDeny;
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
