/*
 * Copyright (c) 2007-2008,2010,2012-2014 Apple Inc. All Rights Reserved.
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
 *  SecTrustSettings.c - Implement trust settings for certificates
 */

#include "SecTrustSettings.h" 
#include "SecTrustSettingsPriv.h"
#include <Security/SecCertificatePriv.h>
#include <AssertMacros.h>
#include <pthread.h>
#include <utilities/debugging.h>
#include "SecBasePriv.h"
#include <Security/SecInternal.h>
#include <CoreFoundation/CFRuntime.h>
#include <utilities/SecCFWrappers.h>

#define trustSettingsDbg(args...)		secdebug("trustSettings", ## args)
#define trustSettingsEvalDbg(args...)	secdebug("trustSettingsEval", ## args)

// MARK: -
// MARK: Static Functions

/* Return a CFDataRef representation of a (hex)string. */
static CFDataRef SecCopyDataFromHexString(CFStringRef string) {
    CFMutableDataRef data;
    CFIndex ix, length;
	UInt8 *bytes;

	length = string ? CFStringGetLength(string) : 0;
	if (length & 1) {
		secwarning("Odd length string: %@ returning NULL", string);
		return NULL;
	}

	data = CFDataCreateMutable(kCFAllocatorDefault, length / 2);
	CFDataSetLength(data, length / 2);
	bytes = CFDataGetMutableBytePtr(data);

	CFStringInlineBuffer buf;
	CFRange range = { 0, length };
	CFStringInitInlineBuffer(string, &buf, range);
	UInt8 lastv = 0;
	for (ix = 0; ix < length; ++ix) {
		UniChar c = CFStringGetCharacterFromInlineBuffer(&buf, ix);
		UInt8 v;
		if ('0' <= c && c <= '9')
			v = c - '0';
		else if ('A' <= c && c <= 'F')
			v = c = 'A' + 10;
		else if ('a' <= c && c <= 'a')
			v = c = 'a' + 10;
		else {
			secwarning("Non hex string: %@ returning NULL", string);
			CFRelease(data);
			return NULL;
		}
		if (ix & 1) {
			*bytes++ = (lastv << 4) + v;
		} else {
			lastv = v;
		}
	}

    return data;
}

#if 0
/* 
 * Obtain a string representing a cert's SHA1 digest. This string is
 * the key used to look up per-cert trust settings in a TrustSettings record. 
 */
static CFStringRef SecTrustSettingsCertHashStrFromCert(
	SecCertificateRef certRef)
{
	if (certRef == NULL) {
		return NULL;
	}
	
	if(certRef == kSecTrustSettingsDefaultRootCertSetting) {
		/* use this string instead of the cert hash as the dictionary key */
		secdebug("trustsettings","DefaultSetting");
		return kSecTrustRecordDefaultRootCert;
	}

	CFDataRef digest = SecCertificateGetSHA1Digest(certRef);
	return CFDataCopyHexString(digest);
}
#endif

// MARK: -
// MARK: SecTrustSettings
/********************************************************
 ************** SecTrustSettings object *****************
 ********************************************************/

struct __SecTrustSettings {
    CFRuntimeBase		_base;

	/* the overall parsed TrustSettings - may be NULL if this is trimmed */
	CFDictionaryRef					_propList;
	
	/* and the main thing we work with, the dictionary of per-cert trust settings */
	CFMutableDictionaryRef			_trustDict;
	
	/* version number of mPropDict */
	SInt32							_dictVersion;

	SecTrustSettingsDomain			_domain;
	bool							_dirty;		/* we've changed _trustDict since creation */
};

static CFStringRef SecTrustSettingsCopyDescription(CFTypeRef cf) {
    SecTrustSettingsRef ts = (SecTrustSettingsRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                    CFSTR("<SecTrustSettings: %p>"), ts);
}

static void SecTrustSettingsDestroy(CFTypeRef cf) {
	SecTrustSettingsRef ts = (SecTrustSettingsRef) cf;
	CFReleaseSafe(ts->_propList);
	CFReleaseSafe(ts->_trustDict);
}

/* SecTrustSettings API functions. */
CFGiblisFor(SecTrustSettings)

/* Make sure a presumed CFNumber can be converted to a 32-bit number */
static bool tsIsGoodCfNum(CFNumberRef cfn, SInt32 *num)
{
	/* by convention */
	if (cfn == NULL) {
		*num = 0;
		return true;
	}
	require(CFGetTypeID(cfn) == CFNumberGetTypeID(), errOut);
	return CFNumberGetValue(cfn, kCFNumberSInt32Type, num);
errOut:
	return false;
}

static bool validateUsageConstraint(const void *value) {
	CFDictionaryRef ucDict = (CFDictionaryRef)value;
	require(CFGetTypeID(ucDict) == CFDictionaryGetTypeID(), errOut);

	CFDataRef certPolicy = (CFDataRef)CFDictionaryGetValue(ucDict,
		kSecTrustSettingsPolicy);
	require(certPolicy && CFGetTypeID(certPolicy) == CFDataGetTypeID(), errOut);

	CFStringRef certApp = (CFStringRef)CFDictionaryGetValue(ucDict,
		kSecTrustSettingsApplication);
	require(certApp && CFGetTypeID(certApp) == CFStringGetTypeID(), errOut);

	CFStringRef policyStr = (CFStringRef)CFDictionaryGetValue(ucDict,
		kSecTrustSettingsPolicyString);
	require(policyStr && CFGetTypeID(policyStr) == CFStringGetTypeID(), errOut);

	SInt32 dummy;
	CFNumberRef allowedError = (CFNumberRef)CFDictionaryGetValue(ucDict,
		kSecTrustSettingsAllowedError);
	require(tsIsGoodCfNum(allowedError, &dummy), errOut);

	CFNumberRef trustSettingsResult = (CFNumberRef)CFDictionaryGetValue(ucDict,
		kSecTrustSettingsResult);
	require(tsIsGoodCfNum(trustSettingsResult, &dummy), errOut);

	CFNumberRef keyUsage = (CFNumberRef)CFDictionaryGetValue(ucDict,
		kSecTrustSettingsKeyUsage);
	require(tsIsGoodCfNum(keyUsage, &dummy), errOut);

	return true;
errOut:
	return false;
}

static bool validateTrustSettingsArray(CFArrayRef usageConstraints) {
	CFIndex ix, numConstraints = CFArrayGetCount(usageConstraints);
	bool result = true;
	for (ix = 0; ix < numConstraints; ++ix) {
		if (!validateUsageConstraint(CFArrayGetValueAtIndex(usageConstraints, ix)))
			result = false;
	}
	return result;
}

struct trustListContext {
	CFMutableDictionaryRef dict;
	SInt32 version;
	bool trim;
	OSStatus status;
};

static void trustListApplierFunction(const void *key, const void *value, void *context) {
	CFStringRef digest = (CFStringRef)key;
	CFDictionaryRef certDict = (CFDictionaryRef)value;
	struct trustListContext *tlc = (struct trustListContext *)context;
	CFDataRef newKey = NULL;
	CFMutableDictionaryRef newDict = NULL;

	/* Get the key. */
	require(digest && CFGetTypeID(digest) == CFStringGetTypeID(), errOut);
	/* Convert it to a CFDataRef. */
	require(newKey = SecCopyDataFromHexString(digest), errOut);

	/* get per-cert dictionary */
	require(certDict && CFGetTypeID(certDict) == CFDictionaryGetTypeID(), errOut);

	/* Create the to be inserted dictionary. */
	require(newDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
		tlc->trim ? 1 : 4, &kCFTypeDictionaryKeyCallBacks,
		&kCFTypeDictionaryValueCallBacks), errOut);
 
	/* The certDict dictionary should have exactly four entries.
	   If we're trimming, all we need is the actual trust settings. */

	/* issuer */
	CFDataRef issuer = (CFDataRef)CFDictionaryGetValue(certDict,
		kTrustRecordIssuer);
	require(issuer && CFGetTypeID(issuer) == CFDataGetTypeID(), errOut);

	/* serial number */
	CFDataRef serial = (CFDataRef)CFDictionaryGetValue(certDict,
		kTrustRecordSerialNumber);
	require(serial && CFGetTypeID(serial) == CFDataGetTypeID(), errOut);

	/* modification date */
	CFDateRef modDate = (CFDateRef)CFDictionaryGetValue(certDict,
		kTrustRecordModDate);
	require(modDate && CFGetTypeID(modDate) == CFDateGetTypeID(), errOut);

	/* If we are not trimming we copy these extra values as well. */
	if (!tlc->trim) {
		CFDictionaryAddValue(newDict, kTrustRecordIssuer, issuer);
		CFDictionaryAddValue(newDict, kTrustRecordSerialNumber, serial);
		CFDictionaryAddValue(newDict, kTrustRecordModDate, modDate);
	}
	
	/* The actual trust settings */
	CFArrayRef trustSettings = (CFArrayRef)CFDictionaryGetValue(certDict,
			kTrustRecordTrustSettings);
	/* optional; this cert's entry is good */
	if(trustSettings) {
		require(CFGetTypeID(trustSettings) == CFArrayGetTypeID(), errOut);

		/* Now validate the usageConstraint array contents */
		require(validateTrustSettingsArray(trustSettings), errOut);

		/* Add the trustSettings to the dict. */
		CFDictionaryAddValue(newDict, kTrustRecordTrustSettings, trustSettings);
	}

	CFDictionaryAddValue(tlc->dict, newKey, newDict);
	CFRelease(newKey);
	CFRelease(newDict);

	return;
errOut:
	CFReleaseSafe(newKey);
	CFReleaseSafe(newDict);
	tlc->status = errSecInvalidTrustSettings;
}

static OSStatus SecTrustSettingsValidate(SecTrustSettingsRef ts, bool trim) {
	/* top level dictionary */
	require(ts->_propList, errOut);
	require(CFGetTypeID(ts->_propList) == CFDictionaryGetTypeID(), errOut);

	/* That dictionary has two entries */
	CFNumberRef cfVers = (CFNumberRef)CFDictionaryGetValue(ts->_propList, kTrustRecordVersion);
	require(cfVers != NULL && CFGetTypeID(cfVers) == CFNumberGetTypeID(), errOut);
	require(CFNumberGetValue(cfVers, kCFNumberSInt32Type, &ts->_dictVersion), errOut);
	require((ts->_dictVersion <= kSecTrustRecordVersionCurrent) &&
		(ts->_dictVersion != kSecTrustRecordVersionInvalid), errOut);

	/* other backwards-compatibility handling done later, if needed, per _dictVersion */
	
	require(ts->_trustDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
	
	CFDictionaryRef trustList = (CFDictionaryRef)CFDictionaryGetValue(
		ts->_propList, kTrustRecordTrustList);
	require(trustList != NULL &&
		CFGetTypeID(trustList) == CFDictionaryGetTypeID(), errOut);

	/* Convert the per-cert entries from on disk to in memory format. */
	struct trustListContext context = {
		ts->_trustDict, ts->_dictVersion, trim, errSecSuccess
	};
	CFDictionaryApplyFunction(trustList, trustListApplierFunction, &context);

	if (trim) {
		/* we don't need the top-level dictionary any more */
		CFRelease(ts->_propList);
		ts->_propList = NULL;
	}

	return context.status;

errOut:
	return errSecInvalidTrustSettings;
}

OSStatus SecTrustSettingsCreateFromExternal(SecTrustSettingsDomain domain,
    CFDataRef external, SecTrustSettingsRef *ts) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFIndex size = sizeof(struct __SecTrustSettings);
    SecTrustSettingsRef result;

	require(result = (SecTrustSettingsRef)_CFRuntimeCreateInstance(allocator,
		SecTrustSettingsGetTypeID(), size - sizeof(CFRuntimeBase), 0), errOut);

	CFErrorRef error = NULL;
	CFPropertyListRef plist = CFPropertyListCreateWithData(kCFAllocatorDefault,
        external, kCFPropertyListImmutable, NULL, &error);
	if (!plist) {
		secwarning("SecTrustSettingsCreateFromExternal: %@", error);
		CFReleaseSafe(error);
        CFReleaseSafe(result);
		goto errOut;
	}

	result->_propList = plist;
	result->_trustDict = NULL;
	SecTrustSettingsValidate(result, false);

	*ts = result;

	return errSecSuccess;

errOut:
    return errSecInvalidTrustSettings;
}

SecTrustSettingsRef SecTrustSettingsCreate(SecTrustSettingsDomain domain,
    bool create, bool trim) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFIndex size = sizeof(struct __SecTrustSettings);
    SecTrustSettingsRef result =
		(SecTrustSettingsRef)_CFRuntimeCreateInstance(allocator,
		SecTrustSettingsGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (!result)
        return NULL;

	//result->_data = NULL;

    return result;
}

CFDataRef SecTrustSettingsCopyExternal(SecTrustSettingsRef ts) {
	/* Transform the internal represantation of the trustSettings to an
	   external form. */
	// @@@ SecTrustSettingsUpdatePropertyList(SecTrustSettingsRef ts);
	CFDataRef xmlData;
	verify(xmlData = CFPropertyListCreateXMLData(kCFAllocatorDefault,
		ts->_propList));
	return xmlData;
}

void SecTrustSettingsSet(SecCertificateRef certRef,
    CFTypeRef trustSettingsDictOrArray) {
}



// MARK: -
// MARK: SPI functions


/*
 * Fundamental routine used by TP to ascertain status of one cert.
 *
 * Returns true in *foundMatchingEntry if a trust setting matching
 * specific constraints was found for the cert. Returns true in 
 * *foundAnyEntry if any entry was found for the cert, even if it
 * did not match the specified constraints. The TP uses this to 
 * optimize for the case where a cert is being evaluated for
 * one type of usage, and then later for another type. If
 * foundAnyEntry is false, the second evaluation need not occur. 
 *
 * Returns the domain in which a setting was found in *foundDomain. 
 *
 * Allowed errors applying to the specified cert evaluation 
 * are returned in a mallocd array in *allowedErrors and must
 * be freed by caller. 
 *
 * The design of the entire TrustSettings module is centered around
 * optimizing the performance of this routine (security concerns 
 * aside, that is). It's why the per-cert dictionaries are stored 
 * as a dictionary, keyed off of the cert hash. It's why TrustSettings 
 * are cached in memory by tsGetGlobalTrustSettings(), and why those 
 * cached TrustSettings objects are 'trimmed' of dictionary fields 
 * which are not needed to verify a cert. 
 *
 * The API functions which are used to manipulate Trust Settings
 * are called infrequently and need not be particularly fast since 
 * they result in user interaction for authentication. Thus they do 
 * not use cached TrustSettings as this function does. 
 */
OSStatus SecTrustSettingsEvaluateCertificate(
	SecCertificateRef certificate,
	SecPolicyRef policy,
	SecTrustSettingsKeyUsage	keyUsage,			/* optional */
	bool						isSelfSignedCert,	/* for checking default setting */
	/* RETURNED values */
	SecTrustSettingsDomain		*foundDomain,
	CFArrayRef					*allowedErrors,		/* RETURNED */
	SecTrustSettingsResult		*resultType,		/* RETURNED */
	bool						*foundMatchingEntry,/* RETURNED */
	bool						*foundAnyEntry)		/* RETURNED */
{
#if 0
	BEGIN_RCSAPI

	StLock<Mutex>	_(sutCacheLock());

	TS_REQUIRED(certHashStr)
	TS_REQUIRED(foundDomain)
	TS_REQUIRED(allowedErrors)
	TS_REQUIRED(numAllowedErrors)
	TS_REQUIRED(resultType)
	TS_REQUIRED(foundMatchingEntry)
	TS_REQUIRED(foundMatchingEntry)
	
	/* ensure a NULL_terminated string */
	auto_array<char> polStr;
	if(policyString != NULL) {
		polStr.allocate(policyStringLen + 1);
		memmove(polStr.get(), policyString, policyStringLen);
		if(policyString[policyStringLen - 1] != '\0') {
			(polStr.get())[policyStringLen] = '\0';
		}
	}
	
	/* initial condition - this can grow if we inspect multiple TrustSettings */
	*allowedErrors = NULL;
	*numAllowedErrors = 0;
	
	/*
	 * This loop relies on the ordering of the SecTrustSettingsDomain enum:
	 * search user first, then admin, then system.
	 */
	assert(kSecTrustSettingsDomainAdmin == (kSecTrustSettingsDomainUser + 1));
	assert(kSecTrustSettingsDomainSystem == (kSecTrustSettingsDomainAdmin + 1));
	bool foundAny = false;
	for(unsigned domain=kSecTrustSettingsDomainUser; 
			     domain<=kSecTrustSettingsDomainSystem; 
				 domain++) {
		TrustSettings *ts = tsGetGlobalTrustSettings(domain);
		if(ts == NULL) {
			continue;
		}

		/* validate cert returns true if matching entry was found */
		bool foundAnyHere = false;
		bool found = ts->evaluateCert(certHashStr, policyOID,
			polStr.get(), keyUsage, isRootCert, 
			allowedErrors, numAllowedErrors, resultType, &foundAnyHere);

		if(found) {
			/* 
			 * Note this, even though we may overwrite it later if this
			 * is an Unspecified entry and we find a definitive entry 
			 * later
			 */
			*foundDomain = domain;
		}
		if(found && (*resultType != kSecTrustSettingsResultUnspecified)) {
			trustSettingsDbg("SecTrustSettingsEvaluateCert: found in domain %d", domain);
			*foundAnyEntry = true;
			*foundMatchingEntry = true;
			return errSecSuccess;
		}
		foundAny |= foundAnyHere;
	}
	trustSettingsDbg("SecTrustSettingsEvaluateCert: NOT FOUND");
	*foundAnyEntry = foundAny;
	*foundMatchingEntry = false;
	return errSecSuccess;
	END_RCSAPI
#endif
	return errSecSuccess;
}

/*
 * Add a cert's TrustSettings to a non-persistent TrustSettings record.
 * No locking or cache flushing here; it's all local to the TrustSettings
 * we construct here.
 */
OSStatus SecTrustSettingsSetTrustSettingsExternal(
	CFDataRef			settingsIn,					/* optional */
	SecCertificateRef	certRef,					/* optional */
	CFTypeRef			trustSettingsDictOrArray,	/* optional */
	CFDataRef			*settingsOut)				/* RETURNED */
{
    SecTrustSettingsRef ts = NULL;
    OSStatus status;

    require_noerr(status = SecTrustSettingsCreateFromExternal(
        kSecTrustSettingsDomainMemory, settingsIn, &ts), errOut);
    SecTrustSettingsSet(certRef, trustSettingsDictOrArray);
    *settingsOut = SecTrustSettingsCopyExternal(ts);

errOut:
    CFReleaseSafe(ts);
	return status;
}
