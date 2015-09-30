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
 * tpCertAllowList.c - List of allowed certificates without a trusted root
 */


#include <stdlib.h>
#include <strings.h>

#include "certGroupUtils.h"
#include "TPCertInfo.h"
#include "TPCrlInfo.h"
#include "tpPolicies.h"
#include "tpdebugging.h"
#include "tpCrlVerify.h"

#include <Security/oidsalg.h>
#include <Security/cssmapple.h>
#include "tpCertAllowList.h"
#include <Security/SecBase.h>
#include <CommonCrypto/CommonDigest.h>
#include <CommonCrypto/CommonDigestSPI.h>
#include <CoreFoundation/CFError.h>
#include <CoreFoundation/CFDictionary.h>


static CFStringRef kSecSystemTrustStoreBundlePath = CFSTR("/System/Library/Security/Certificates.bundle");

static CFURLRef SecSystemTrustStoreCopyResourceURL(CFStringRef resourceName,
						   CFStringRef resourceType, CFStringRef subDirName)
{
	CFURLRef rsrcUrl = NULL;
	CFBundleRef bundle = NULL;
	
	CFURLRef bundleUrl = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, kSecSystemTrustStoreBundlePath, kCFURLPOSIXPathStyle, true);
	if (bundleUrl) {
		bundle = CFBundleCreate(kCFAllocatorDefault, bundleUrl);
		CFRelease(bundleUrl);
	}
	
	if (bundle) {
		rsrcUrl = CFBundleCopyResourceURL(bundle, resourceName, resourceType, subDirName);
		if (!rsrcUrl) {
			tpDebug("resource: not found");
		}
		CFRelease(bundle);
	}
	
	return rsrcUrl;
}

static CFDataRef SecSystemTrustStoreCopyResourceContents(CFStringRef resourceName,
							 CFStringRef resourceType, CFStringRef subDirName)
{
	CFURLRef url = SecSystemTrustStoreCopyResourceURL(resourceName, resourceType, subDirName);
	CFDataRef data = NULL;
	if (url) {
		SInt32 error;
		if (!CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault,
							      url, &data, NULL, NULL, &error)) {
			tpDebug("Allow list read: %ld", (long) error);
		}
		CFRelease(url);
	}
	return data;
}

static CFDictionaryRef InitializeAllowList()
{
	CFDataRef xmlData = NULL;
	// Use the file in the system trust store bundle
	xmlData = SecSystemTrustStoreCopyResourceContents(CFSTR("Allowed"), CFSTR("plist"), NULL);
	
	CFPropertyListRef allowList = NULL;
	if (xmlData) {
		allowList = CFPropertyListCreateWithData(kCFAllocatorDefault, xmlData, kCFPropertyListImmutable, NULL, NULL);
		CFRelease(xmlData);
	}
	
	if (allowList && (CFGetTypeID(allowList) == CFDictionaryGetTypeID())) {
		return (CFDictionaryRef) allowList;
	} else {
		if (allowList)
			CFRelease(allowList);
		return NULL;
	}
}

/* helper functions borrowed from SecCFWrappers */
static inline CFComparisonResult CFDataCompare(CFDataRef left, CFDataRef right)
{
	const size_t left_size = CFDataGetLength(left);
	const size_t right_size = CFDataGetLength(right);
	const size_t shortest = (left_size <= right_size) ? left_size : right_size;
	
	int comparison = memcmp(CFDataGetBytePtr(left), CFDataGetBytePtr(right), shortest);
	
	if (comparison > 0 || (comparison == 0 && left_size > right_size))
		return kCFCompareGreaterThan;
	else if (comparison < 0 || (comparison == 0 && left_size < right_size))
		return kCFCompareLessThan;
	else
		return kCFCompareEqualTo;
}

static inline void CFStringAppendHexData(CFMutableStringRef s, CFDataRef data) {
	const uint8_t *bytes = CFDataGetBytePtr(data);
	CFIndex len = CFDataGetLength(data);
	for (CFIndex ix = 0; ix < len; ++ix) {
		CFStringAppendFormat(s, 0, CFSTR("%02X"), bytes[ix]);
	}
}

static inline CF_RETURNS_RETAINED CFStringRef CFDataCopyHexString(CFDataRef data) {
	CFMutableStringRef hexString = CFStringCreateMutable(kCFAllocatorDefault, 2 * CFDataGetLength(data));
	CFStringAppendHexData(hexString, data);
	return hexString;
}

CSSM_RETURN tpCheckCertificateAllowList(TPCertGroup &certGroup) {
	CSSM_RETURN result = CSSMERR_TP_NOT_TRUSTED;
	unsigned numCerts = certGroup.numCerts();
	int i;
	CFRange range;
	CFArrayRef allowedCerts = NULL;
	
	TPCertInfo *last = certGroup.lastCert();
	if (!last) {
		return result;
	}
	
	/* parse authority key ID from certificate that would have been signed by a distrusted root */
	const CSSM_DATA *authKeyID = last->authorityKeyID();
	if (!authKeyID || !authKeyID->Data) {
		return result;
	}
	
	CSSM_X509_EXTENSION *ake = (CSSM_X509_EXTENSION *)authKeyID->Data;
	if (!ake || ake->format != CSSM_X509_DATAFORMAT_PARSED) {
		return result;
	}
	
	const CE_AuthorityKeyID *akid = (CE_AuthorityKeyID *)ake->value.parsedValue;
	if (!akid || !akid->keyIdentifierPresent) {
		return result;
	}

	CFDataRef akData = CFDataCreate(kCFAllocatorDefault, akid->keyIdentifier.Data, akid->keyIdentifier.Length);
	CFStringRef akString = CFDataCopyHexString(akData);
	
	/* search allow list for allowed certs for this distrusted root */
	CFDictionaryRef allowList = InitializeAllowList();
	if (NULL == allowList) {
		goto errout;
	}

	allowedCerts = (CFArrayRef)CFDictionaryGetValue(allowList, akString);
	if (!allowedCerts || !CFArrayGetCount(allowedCerts)) {
		goto errout;
	}
	
	/* found some allowed certificates: check whether a certificate in this chain is present */
	range = CFRangeMake(0, CFArrayGetCount(allowedCerts));
	for (i = 0; i < numCerts; i++) {
		TPCertInfo *cert = certGroup.certAtIndex(i);
		UInt8 hashBytes[CC_SHA256_DIGEST_LENGTH] = {0};
		
		const CSSM_DATA *certData = cert->itemData();
		if (!certData || !certData->Data || (certData->Length <= 0)) {
			goto errout;
		}
		
		int err = CCDigest(kCCDigestSHA256, certData->Data, certData->Length, hashBytes);
		if (err) {
			goto errout;
		}
		
		CFDataRef hashData = CFDataCreate(kCFAllocatorDefault, hashBytes, sizeof(hashBytes));
		
		CFIndex position = CFArrayBSearchValues(allowedCerts, range, hashData, (CFComparatorFunction)CFDataCompare, NULL);
		if (position < CFArrayGetCount(allowedCerts)) {
			CFDataRef possibleMatch = (CFDataRef) CFArrayGetValueAtIndex(allowedCerts, position);
			if (!CFDataCompare(hashData, possibleMatch)) {
				//this cert is in the allowlist
				result = CSSM_OK;
			}
		}
		CFRelease(hashData);
	}
	
errout:
	if (akString)
		CFRelease(akString);
	if (akData)
		CFRelease(akData);
	if (allowList)
		CFRelease(allowList);
	return result;
}

