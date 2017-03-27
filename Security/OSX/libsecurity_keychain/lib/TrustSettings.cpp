/*
 * Copyright (c) 2005,2011-2015 Apple Inc. All Rights Reserved.
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
 * TrustSettings.h - class to manage cert trust settings.
 *
 */

#include "TrustSettings.h"
#include "TrustSettingsSchema.h"
#include "SecTrustSettings.h"
#include "TrustSettingsUtils.h"
#include "TrustKeychains.h"
#include "Certificate.h"
#include "cssmdatetime.h"
#include <Security/SecBase.h>
#include "SecTrustedApplicationPriv.h"
#include <security_utilities/errors.h>
#include <security_utilities/debugging.h>
#include <security_utilities/logging.h>
#include <security_utilities/cfutilities.h>
#include <security_utilities/alloc.h>
#include <security_utilities/casts.h>
#include <utilities/SecCFRelease.h>
#include <Security/Authorization.h>
#include <Security/cssmapplePriv.h>
#include <Security/oidscert.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <security_keychain/KCCursor.h>
#include <security_ocspd/ocspdClient.h>
#include <CoreFoundation/CoreFoundation.h>
#include <assert.h>
#include <dispatch/dispatch.h>
#include <sys/stat.h>
#include <syslog.h>

#if 0
#define trustSettingsDbg(args...)		syslog(LOG_ERR, ## args)
#define trustSettingsEvalDbg(args...)	syslog(LOG_ERR, ## args)
#else
#define trustSettingsDbg(args...)		secinfo("trustSettings", ## args)
#define trustSettingsEvalDbg(args...)	secinfo("trustSettingsEval", ## args)
#endif

/*
 * Common error return for "malformed TrustSettings record"
 */
#define errSecInvalidTrustedRootRecord	errSecInvalidTrustSettings

using namespace KeychainCore;

#pragma mark --- Static functions ---

/*
 * Comparator atoms to determine if an app's specified usage
 * matches an individual trust setting. Each returns true on a match, false
 * if the trust setting does not match the app's spec.
 *
 * A match fails iff:
 *
 * -- the app has specified a field, and the cert has a spec for that
 *    field, and the two specs do not match;
 *
 * OR
 *
 * -- the cert has a spec for the field and the app hasn't specified the field
 */
static bool tsCheckPolicy(
	const CSSM_OID *appPolicy,
	CFDataRef certPolicy)
{
	if(certPolicy != NULL) {
		if(appPolicy == NULL) {
			trustSettingsEvalDbg("tsCheckPolicy: certPolicy, !appPolicy");
			return false;
		}
		unsigned cLen = (unsigned)CFDataGetLength(certPolicy);
		const UInt8 *cData = CFDataGetBytePtr(certPolicy);
		if((cLen != appPolicy->Length) || memcmp(appPolicy->Data, cData, cLen)) {
			trustSettingsEvalDbg("tsCheckPolicy: policy mismatch");
			return false;
		}
	}
	return true;
}

/*
 * This one's slightly different: the match is for *this* app, not one
 * specified by the app.
 */
static bool tsCheckApp(
	CFDataRef certApp)
{
	if(certApp != NULL) {
		SecTrustedApplicationRef appRef;
		OSStatus ortn;
		ortn = SecTrustedApplicationCreateWithExternalRepresentation(certApp, &appRef);
		if(ortn) {
			trustSettingsDbg("tsCheckApp: bad trustedApp data");
			return false;
		}
		ortn = SecTrustedApplicationValidateWithPath(appRef, NULL);
		if(ortn) {
			/* Not this app */
			return false;
		}
	}

	return true;
}

static bool tsCheckKeyUse(
	SecTrustSettingsKeyUsage appKeyUse,
	CFNumberRef certKeyUse)
{
	if(certKeyUse != NULL) {
		SInt32 certUse;
		CFNumberGetValue(certKeyUse, kCFNumberSInt32Type, &certUse);
		SecTrustSettingsKeyUsage cku = (SecTrustSettingsKeyUsage)certUse;
		if(cku == kSecTrustSettingsKeyUseAny) {
			/* explicitly allows anything */
			return true;
		}
		/* cert specification must be a superset of app's intended use */
		if(appKeyUse == 0) {
			trustSettingsEvalDbg("tsCheckKeyUse: certKeyUsage, !appKeyUsage");
			return false;
		}

		if((cku & appKeyUse) != appKeyUse) {
			trustSettingsEvalDbg("tsCheckKeyUse: keyUse mismatch");
			return false;
		}
	}
	return true;
}

static bool tsCheckPolicyStr(
	const char *appPolicyStr,
	CFStringRef certPolicyStr)
{
	if(certPolicyStr != NULL) {
		if(appPolicyStr == NULL) {
			trustSettingsEvalDbg("tsCheckPolicyStr: certPolicyStr, !appPolicyStr");
			return false;
		}
		/* Let CF do the string compare */
		CFStringRef cfPolicyStr = CFStringCreateWithCString(NULL, appPolicyStr,
			kCFStringEncodingUTF8);
		if(cfPolicyStr == NULL) {
			/* I really don't see how this can happen */
			trustSettingsEvalDbg("tsCheckPolicyStr: policyStr string conversion error");
			return false;
		}

		// Some trust setting strings were created with a NULL character at the
		// end, which was included in the length. Strip those off before compare

		CFMutableStringRef certPolicyStrNoNULL = CFStringCreateMutableCopy(NULL, 0, certPolicyStr);
		if (certPolicyStrNoNULL == NULL) {
			/* I really don't see how this can happen either */
			trustSettingsEvalDbg("tsCheckPolicyStr: policyStr string conversion error 2");
            CFReleaseNull(cfPolicyStr);
			return false;
		}

		CFStringFindAndReplace(certPolicyStrNoNULL, CFSTR("\00"),
			CFSTR(""), CFRangeMake(0, CFStringGetLength(certPolicyStrNoNULL)), kCFCompareBackwards);

		CFComparisonResult res = CFStringCompare(cfPolicyStr, certPolicyStrNoNULL, 0);
		CFRelease(cfPolicyStr);
		CFRelease(certPolicyStrNoNULL);
		if(res != kCFCompareEqualTo) {
			trustSettingsEvalDbg("tsCheckPolicyStr: policyStr mismatch");
			return false;
		}
	}
	return true;
}

/*
 * Determine if a cert's trust settings dictionary satisfies the specified
 * usage constraints. Returns true if so.
 * Only certs with a SecTrustSettingsResult of kSecTrustSettingsResultTrustRoot
 * or kSecTrustSettingsResultTrustAsRoot will match.
 */
static bool qualifyUsageWithCertDict(
	CFDictionaryRef			certDict,
	const CSSM_OID			*policyOID,		/* optional */
	const char				*policyStr,		/* optional */
	SecTrustSettingsKeyUsage keyUsage,	/* optional; default = any (actually "all" here) */
	bool					onlyRoots)
{
	/* this array is optional */
	CFArrayRef trustSettings = (CFArrayRef)CFDictionaryGetValue(certDict,
		kTrustRecordTrustSettings);
	CFIndex numSpecs = 0;
	if(trustSettings != NULL) {
		numSpecs = CFArrayGetCount(trustSettings);
	}
	if(numSpecs == 0) {
		/*
		 * Trivial case: cert has no trust settings, indicating that
		 * it's used for everything.
		 */
		trustSettingsEvalDbg("qualifyUsageWithCertDict: no trust settings");
		return true;
	}
	for(CFIndex addDex=0; addDex<numSpecs; addDex++) {
		CFDictionaryRef tsDict = (CFDictionaryRef)CFArrayGetValueAtIndex(trustSettings,
			addDex);

		/* per-cert specs: all optional */
		CFDataRef   certPolicy     = (CFDataRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsPolicy);
		CFDataRef   certApp        = (CFDataRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsApplication);
		CFStringRef certPolicyStr  = (CFStringRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsPolicyString);
		CFNumberRef certKeyUsage   = (CFNumberRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsKeyUsage);
		CFNumberRef certResultType = (CFNumberRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsResult);

		if(!tsCheckPolicy(policyOID, certPolicy)) {
			continue;
		}
		if(!tsCheckApp(certApp)) {
			continue;
		}
		if(!tsCheckKeyUse(keyUsage, certKeyUsage)) {
			continue;
		}
		if(!tsCheckPolicyStr(policyStr, certPolicyStr)) {
			continue;
		}

		/*
		 * This is a match, take whatever SecTrustSettingsResult is here,
		 * including the default if not specified.
		 */
		SecTrustSettingsResult resultType = kSecTrustSettingsResultTrustRoot;
		if(certResultType) {
			SInt32 s;
			CFNumberGetValue(certResultType, kCFNumberSInt32Type, &s);
			resultType = (SecTrustSettingsResult)s;
		}
		switch(resultType) {
			case kSecTrustSettingsResultTrustRoot:
				trustSettingsEvalDbg("qualifyUsageWithCertDict: TrustRoot MATCH");
				return true;
			case kSecTrustSettingsResultTrustAsRoot:
				if(onlyRoots) {
					trustSettingsEvalDbg("qualifyUsageWithCertDict: TrustAsRoot but not root");
					return false;
				}
				trustSettingsEvalDbg("qualifyUsageWithCertDict: TrustAsRoot MATCH");
				return true;
			default:
				trustSettingsEvalDbg("qualifyUsageWithCertDict: bad resultType "
					"(%lu)", (unsigned long)resultType);
				return false;
		}
	}
	trustSettingsEvalDbg("qualifyUsageWithCertDict: NO MATCH");
	return false;
}

/*
 * Create initial top-level dictionary when constructing a new TrustSettings.
 */
static CFMutableDictionaryRef tsInitialDict()
{
	CFMutableDictionaryRef dict = CFDictionaryCreateMutable(NULL,
		kSecTrustRecordNumTopDictKeys,
		&kCFTypeDictionaryKeyCallBacks,	&kCFTypeDictionaryValueCallBacks);

	/* the dictionary of per-cert entries */
	CFMutableDictionaryRef trustDict = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks,	&kCFTypeDictionaryValueCallBacks);
	CFDictionaryAddValue(dict, kTrustRecordTrustList, trustDict);
	CFRelease(trustDict);

	SInt32 vers = kSecTrustRecordVersionCurrent;
	CFNumberRef cfVers = CFNumberCreate(NULL, kCFNumberSInt32Type, &vers);
	CFDictionaryAddValue(dict, kTrustRecordVersion, cfVers);
	CFRelease(cfVers);
	return dict;
}

/*
 * Set the modification date of a per-cert dictionary to current time.
 */
static void tsSetModDate(
	CFMutableDictionaryRef dict)
{
	CFDateRef modDate;

	modDate = CFDateCreate(NULL, CFAbsoluteTimeGetCurrent());
	CFDictionarySetValue(dict, kTrustRecordModDate, modDate);
	CFRelease(modDate);
}

/* make sure a presumed CFNumber can be converted to a 32-bit number */
static
bool tsIsGoodCfNum(CFNumberRef cfn, SInt32 *num = NULL)
{
	if(cfn == NULL) {
		/* by convention */
		if(num) {
			*num = 0;
		}
		return true;
	}
	if(CFGetTypeID(cfn) != CFNumberGetTypeID()) {
		return false;
	}

	SInt32 s;
	if(!CFNumberGetValue(cfn, kCFNumberSInt32Type, &s)) {
		return false;
	}
	else {
		if(num) {
			*num = s;
		}
		return true;
	}
}

TrustSettings::TrustSettings(SecTrustSettingsDomain domain)
		: mPropList(NULL),
		  mTrustDict(NULL),
		  mDictVersion(0),
		  mDomain(domain),
		  mDirty(false)
{
}



#pragma mark --- Public methods ---

/*
 * Normal constructor, from disk.
 * If create is true, the absence of an on-disk TrustSettings file
 * results in the creation of a new empty TrustSettings. If create is
 * false and no on-disk TrustSettings exists, errSecNoTrustSettings is
 * thrown.
 * If trim is true, the components of the on-disk TrustSettings not
 * needed for cert evaluation are discarded. This is for TrustSettings
 * that will be cached in memory long-term.
 */
OSStatus TrustSettings::CreateTrustSettings(
	SecTrustSettingsDomain	domain,
	bool					create,
	bool					trim,
	TrustSettings*&			ts)
{
	TrustSettings* t = new TrustSettings(domain);

	Allocator &alloc = Allocator::standard();
	CSSM_DATA fileData = {0, NULL};
	OSStatus ortn = errSecSuccess;
	struct stat sb;
	const char *path;

	/* get trust settings from file, one way or another */
	switch(domain) {
		case kSecTrustSettingsDomainAdmin:
			/*
 			 * Quickie optimization: if it's not there, don't try to
			 * get it from ocspd. This is possible because the name of the
			 * admin file is hard coded, but the per-user files aren't.
			 */
			path = TRUST_SETTINGS_PATH "/" ADMIN_TRUST_SETTINGS;
			if(stat(path, &sb)) {
				trustSettingsDbg("TrustSettings: no admin record; skipping");
				ortn = errSecNoTrustSettings;
				break;
			}
			/* else drop thru, get it from ocspd */
		case kSecTrustSettingsDomainUser:
			/* get settings from ocspd */
			ortn = ocspdTrustSettingsRead(alloc, domain, fileData);
			break;
		case kSecTrustSettingsDomainSystem:
			/* immutable; it's safe for us to read this directly */
			if(tsReadFile(SYSTEM_TRUST_SETTINGS_PATH, alloc, fileData)) {
				ortn = errSecNoTrustSettings;
			}
			break;
		default:
			delete t;
			return errSecParam;
	}
	if(ortn) {
		if(create) {
			trustSettingsDbg("TrustSettings: creating new record for domain %d",
				(int)domain);
			t->mPropList = tsInitialDict();
			t->mDirty = true;
		}
		else {
			trustSettingsDbg("TrustSettings: record not found for domain %d",
				(int)domain);
			delete t;
			return ortn;
		}
	}
	else {
		CFRef<CFDataRef> propList(CFDataCreate(NULL, fileData.Data, fileData.Length));
		t->initFromData(propList);
		alloc.free(fileData.Data);
	}
	t->validatePropList(trim);

	ts = t;
	return errSecSuccess;
}

/*
 * Create from external data, obtained by createExternal().
 * If externalData is NULL, we'll create an empty mTrustDict.
 */
OSStatus TrustSettings::CreateTrustSettings(
	SecTrustSettingsDomain				domain,
	CFDataRef							externalData,
	TrustSettings*&						ts)
{
	switch(domain) {
		case kSecTrustSettingsDomainUser:
		case kSecTrustSettingsDomainAdmin:
		case kSecTrustSettingsDomainMemory:
			break;
		case kSecTrustSettingsDomainSystem:		/* no can do, that implies writing to it */
		default:
			return errSecParam;
	}

	TrustSettings* t = new TrustSettings(domain);

	if(externalData != NULL) {
		t->initFromData(externalData);
	}
	else {
		t->mPropList = tsInitialDict();
	}
	t->validatePropList(TRIM_NO);		/* never trim this */
	t->mDirty = true;

	ts = t;
	return errSecSuccess;
}


TrustSettings::~TrustSettings()
{
	trustSettingsDbg("TrustSettings(domain %d) destructor", (int)mDomain);
	CFRELEASE(mPropList);		/* may be null if trimmed */
	CFRELEASE(mTrustDict);		/* normally always non-NULL */

}

/* common code to init mPropList from raw data */
void TrustSettings::initFromData(
	CFDataRef	trustSettingsData)
{
	CFStringRef errStr = NULL;

	mPropList = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(
		NULL,
		trustSettingsData,
		kCFPropertyListMutableContainersAndLeaves,
		&errStr);
	if(mPropList == NULL) {
		trustSettingsDbg("TrustSettings::initFromData decode err (%s)",
			errStr ? CFStringGetCStringPtr(errStr, kCFStringEncodingUTF8) : "<no err>");
		if(errStr != NULL) {
			CFRelease(errStr);
		}
		MacOSError::throwMe(errSecInvalidTrustSettings);
	}
}

/*
 * Flush property list data out to disk if dirty.
 */
void TrustSettings::flushToDisk()
{
	if(!mDirty) {
		trustSettingsDbg("flushToDisk, domain %d, !dirty!", (int)mDomain);
		return;
	}
	if(mPropList == NULL) {
		trustSettingsDbg("flushToDisk, domain %d, trimmed!", (int)mDomain);
		assert(0);
		MacOSError::throwMe(errSecInternalComponent);
	}
	switch(mDomain) {
		case kSecTrustSettingsDomainSystem:
		case kSecTrustSettingsDomainMemory:
		/* caller shouldn't even try this */
		default:
			trustSettingsDbg("flushToDisk, bad domain (%d)", (int)mDomain);
			MacOSError::throwMe(errSecInternalComponent);
		case kSecTrustSettingsDomainUser:
		case kSecTrustSettingsDomainAdmin:
			break;
	}

	/*
	 * Optimization: if there are no certs in the mTrustDict dictionary,
	 * we tell ocspd to *remove* the settings for the specified domain.
	 * Having *no* settings uses less memory and is faster than having
	 * an empty settings file, especially for the admin domain, where we
	 * can avoid
	 * an RPC if the settings file is simply not there.
	 */
	CFRef<CFDataRef> xmlData;
	CSSM_DATA cssmXmlData = {0, NULL};
	CFIndex numCerts = CFDictionaryGetCount(mTrustDict);
	if(numCerts) {
		xmlData.take(CFPropertyListCreateXMLData(NULL, mPropList));
		if(!xmlData) {
			/* we've been very careful; this should never happen */
			trustSettingsDbg("flushToDisk, domain %d: error converting to XML", (int)mDomain);
			MacOSError::throwMe(errSecInternalComponent);
		}
		cssmXmlData.Data = (uint8 *)CFDataGetBytePtr(xmlData);
		cssmXmlData.Length = CFDataGetLength(xmlData);
	}
	else {
		trustSettingsDbg("flushToDisk, domain %d: DELETING trust settings", (int)mDomain);
	}

	/* cook up auth stuff so ocspd can act on our behalf */
	AuthorizationRef authRef;
	OSStatus ortn;
	ortn = AuthorizationCreate(NULL, kAuthorizationEmptyEnvironment,
			0, &authRef);
	if(ortn) {
		trustSettingsDbg("flushToDisk, domain %d: AuthorizationCreate returned %ld",
			(int)mDomain, (long)ortn);
		MacOSError::throwMe(errSecInternalComponent);
	}
	AuthorizationExternalForm authExt;
	CSSM_DATA authBlob = {sizeof(authExt), (uint8 *)&authExt};
	ortn = AuthorizationMakeExternalForm(authRef, &authExt);
	if(ortn) {
		trustSettingsDbg("flushToDisk, domain %d: AuthorizationMakeExternalForm returned %ld",
			(int)mDomain, (long)ortn);
		ortn = errSecInternalComponent;
		goto errOut;
	}

	ortn = ocspdTrustSettingsWrite(mDomain, authBlob, cssmXmlData);
	if(ortn) {
		trustSettingsDbg("flushToDisk, domain %d: ocspdTrustSettingsWrite returned %ld",
			(int)mDomain, (long)ortn);
		goto errOut;
	}
	trustSettingsDbg("flushToDisk, domain %d: wrote to disk", (int)mDomain);
	mDirty = false;
errOut:
	AuthorizationFree(authRef, 0);
	if(ortn) {
		MacOSError::throwMe(ortn);
	}
}

/*
 * Obtain external representation of TrustSettings data.
 */
CFDataRef TrustSettings::createExternal()
{
	assert(mPropList);
	CFDataRef xmlData = CFPropertyListCreateXMLData(NULL, mPropList);
	if(xmlData == NULL) {
		trustSettingsDbg("createExternal, domain %d: error converting to XML",
			(int)mDomain);
		MacOSError::throwMe(errSecInternalComponent);
	}
	return xmlData;
}

/*
 * Evaluate specified cert. Returns true if we found a record for the cert
 * matching specified constraints.
 * Note that a true return with a value of kSecTrustSettingsResultUnspecified for
 * the resultType means that a cert isn't to be trusted or untrusted
 * per se; it just means that we only found allowedErrors entries.
 *
 * Found "allows errors" values are added to the incoming allowedErrors
 * array which is reallocd as needed (and which may be NULL or non-NULL on
 * entry).
 */
bool TrustSettings::evaluateCert(
	CFStringRef				certHashStr,
	const CSSM_OID			*policyOID,			/* optional */
	const char				*policyStr,			/* optional */
	SecTrustSettingsKeyUsage keyUsage,			/* optional */
	bool					isRootCert,			/* for checking default setting */
	CSSM_RETURN				**allowedErrors,	/* IN/OUT; reallocd as needed */
	uint32					*numAllowedErrors,	/* IN/OUT */
	SecTrustSettingsResult	*resultType,		/* RETURNED */
	bool					*foundAnyEntry)		/* RETURNED */
{
	assert(mTrustDict != NULL);

	/* get trust settings dictionary for this cert */
	CFDictionaryRef certDict = findDictionaryForCertHash(certHashStr);
#if CERT_HASH_DEBUG
	/* @@@ debug only @@@ */
	/* print certificate hash and found dictionary reference */
	const size_t maxHashStrLen = 512;
	char *buf = (char*)malloc(maxHashStrLen);
	if (buf) {
		if (!CFStringGetCString(certHashStr, buf, (CFIndex)maxHashStrLen, kCFStringEncodingUTF8)) {
			buf[0]='\0';
		}
		trustSettingsEvalDbg("evaluateCert for \"%s\", found dict %p", buf, certDict);
		free(buf);
	}
#endif

	if(certDict == NULL) {
		*foundAnyEntry = false;
		return false;
	}
	*foundAnyEntry = true;

	/* to-be-returned array of allowed errors */
	CSSM_RETURN *allowedErrs = *allowedErrors;
	uint32 numAllowedErrs = *numAllowedErrors;

	/* this means "we found something other than allowedErrors" if true */
	bool foundSettings = false;

	/* to be returned in *resultType if it ends up something other than Invalid */
	SecTrustSettingsResult returnedResult = kSecTrustSettingsResultInvalid;

	/*
	 * Note since we validated the entire mPropList in our constructor, and we're careful
	 * about what we put into it, we don't bother typechecking its contents here.
	 * Also note that the kTrustRecordTrustSettings entry is optional.
	 */
	CFArrayRef trustSettings = (CFArrayRef)CFDictionaryGetValue(certDict,
			kTrustRecordTrustSettings);
	CFIndex numSpecs = 0;
	if(trustSettings != NULL) {
		numSpecs = CFArrayGetCount(trustSettings);
	}
	if(numSpecs == 0) {
		/*
		 * Trivial case: cert has no trust settings, indicating that
		 * it's used for everything.
		 */
		trustSettingsEvalDbg("evaluateCert: no trust settings");
		/* the default... */
		*resultType = kSecTrustSettingsResultTrustRoot;
		return true;
	}

	/*
	 * The decidedly nontrivial part: grind thru all of the cert's trust
	 * settings, see if the cert matches the caller's specified usage.
	 */
	for(CFIndex addDex=0; addDex<numSpecs; addDex++) {
		CFDictionaryRef tsDict = (CFDictionaryRef)CFArrayGetValueAtIndex(trustSettings,
			addDex);

		/* per-cert specs: all optional */
		CFDataRef   certPolicy     = (CFDataRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsPolicy);
		CFDataRef   certApp        = (CFDataRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsApplication);
		CFStringRef certPolicyStr  = (CFStringRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsPolicyString);
		CFNumberRef certKeyUsage   = (CFNumberRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsKeyUsage);
		CFNumberRef certResultType = (CFNumberRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsResult);
		CFNumberRef certAllowedErr = (CFNumberRef)CFDictionaryGetValue(tsDict,
										kSecTrustSettingsAllowedError);

		/* now, skip if we find a constraint that doesn't match intended use */
		if(!tsCheckPolicy(policyOID, certPolicy)) {
			continue;
		}
		if(!tsCheckApp(certApp)) {
			continue;
		}
		if(!tsCheckKeyUse(keyUsage, certKeyUsage)) {
			continue;
		}
		if(!tsCheckPolicyStr(policyStr, certPolicyStr)) {
			continue;
		}

		trustSettingsEvalDbg("evaluateCert: MATCH");
		foundSettings = true;

		if(certAllowedErr) {
			/* note we already validated this value */
			SInt32 s;
			CFNumberGetValue(certAllowedErr, kCFNumberSInt32Type, &s);
			allowedErrs = (CSSM_RETURN *)::realloc(allowedErrs,
				++numAllowedErrs * sizeof(CSSM_RETURN));
			allowedErrs[numAllowedErrs-1] = (CSSM_RETURN) s;
		}

		/*
		 * We found a match, but we only return the current result type
		 * to caller if we haven't already returned something other than
		 * kSecTrustSettingsResultUnspecified. Once we find a valid result type,
		 * we keep on searching, but only for additional allowed errors.
		 */
		switch(returnedResult) {
			/* found match but no valid resultType yet */
			case kSecTrustSettingsResultUnspecified:
			/* haven't been thru here */
			case kSecTrustSettingsResultInvalid:
				if(certResultType) {
					/* note we already validated this */
					SInt32 s;
					CFNumberGetValue(certResultType, kCFNumberSInt32Type, &s);
					returnedResult = (SecTrustSettingsResult)s;
				}
				else {
					/* default is "copacetic" */
					returnedResult = kSecTrustSettingsResultTrustRoot;
				}
				break;
			default:
				/* we already have a definitive resultType, don't change it */
				break;
		}
	}	/* for each dictionary in trustSettings */

	*allowedErrors = allowedErrs;
	*numAllowedErrors = numAllowedErrs;
	if(returnedResult != kSecTrustSettingsResultInvalid) {
		*resultType = returnedResult;
	}
	return foundSettings;
}


/*
 * Find all certs in specified keychain list which have entries in this trust record.
 * Certs already in the array are not added.
 */
void TrustSettings::findCerts(
	StorageManager::KeychainList	&keychains,
	CFMutableArrayRef				certArray)
{
	findQualifiedCerts(keychains,
		true,		/* findAll */
		false,		/* onlyRoots */
		NULL, NULL, kSecTrustSettingsKeyUseAny,
		certArray);
}

void TrustSettings::findQualifiedCerts(
	StorageManager::KeychainList	&keychains,
	/*
	 * If findAll is true, all certs are returned and the subsequent
	 * qualifiers are ignored
	 */
	bool							findAll,
	/* if true, only return root (self-signed) certs */
	bool							onlyRoots,
	const CSSM_OID					*policyOID,			/* optional */
	const char						*policyString,		/* optional */
	SecTrustSettingsKeyUsage		keyUsage,			/* optional */
	CFMutableArrayRef				certArray)			/* certs appended here */
{
	StLock<Mutex> _(SecTrustKeychainsGetMutex());

	/*
	 * a set, hopefully with a good hash function for CFData, to keep track of what's
	 * been added to the outgoing array.
	 */
	CFRef<CFMutableSetRef> certSet(CFSetCreateMutable(NULL, 0, &kCFTypeSetCallBacks));

	/* search: all certs, no attributes */
	KCCursor cursor(keychains, (SecItemClass) CSSM_DL_DB_RECORD_X509_CERTIFICATE, NULL);
	Item certItem;
	bool found;
	unsigned int total=0, entries=0, qualified=0;
	do {
		found = cursor->next(certItem);
		if(!found) {
			break;
		}
		++total;

		/* must convert to unified SecCertificateRef */
		SecPointer<Certificate> certificate(static_cast<Certificate *>(&*certItem));
        CssmData certCssmData;
        try {
            certCssmData = certificate->data();
        }
        catch (...) {}
		if (!(certCssmData.Data && certCssmData.Length)) {
			continue;
		}
		CFRef<CFDataRef> cfDataRef(CFDataCreate(NULL, certCssmData.Data, certCssmData.Length));
		CFRef<SecCertificateRef> certRef(SecCertificateCreateWithData(NULL, cfDataRef));

		/* do we have an entry for this cert? */
		CFDictionaryRef certDict = findDictionaryForCert(certRef);
		if(certDict == NULL) {
			continue;
		}
		++entries;

		if(!findAll) {
			/* qualify */
			if(!qualifyUsageWithCertDict(certDict, policyOID,
					policyString, keyUsage, onlyRoots)) {
				continue;
			}
		}
		++qualified;

		/* see if we already have this one - get in CFData form */
		CSSM_DATA certData;
		OSStatus ortn = SecCertificateGetData(certRef, &certData);
		if(ortn) {
			trustSettingsEvalDbg("findQualifiedCerts: SecCertificateGetData error");
			continue;
		}
		CFRef<CFDataRef> cfData(CFDataCreate(NULL, certData.Data, certData.Length));
		CFDataRef cfd = cfData.get();
		if(CFSetContainsValue(certSet, cfd)) {
			trustSettingsEvalDbg("findQualifiedCerts: dup cert");
			continue;
		}
		else {
			/* add to the tracking set, which owns the CFData now */
			CFSetAddValue(certSet, cfd);
			/* and add the SecCert to caller's array, which owns that now */
			CFArrayAppendValue(certArray, certRef);
		}
	} while(found);

	trustSettingsEvalDbg("findQualifiedCerts: examined %d certs, qualified %d of %d",
		total, qualified, entries);
}

/*
 * Obtain trust settings for the specified cert. Returned settings array
 * is in the public API form; caller must release. Returns NULL
 * (does not throw) if the cert is not present in this TrustRecord.
 */
CFArrayRef TrustSettings::copyTrustSettings(
	SecCertificateRef	certRef)
{
	CFDictionaryRef certDict = NULL;

	/* find the on-disk usage constraints for this cert */
	certDict = findDictionaryForCert(certRef);
	if(certDict == NULL) {
		trustSettingsDbg("copyTrustSettings: dictionary not found");
		return NULL;
	}
	CFArrayRef diskTrustSettings = (CFArrayRef)CFDictionaryGetValue(certDict,
		kTrustRecordTrustSettings);
	CFIndex numSpecs = 0;
	if(diskTrustSettings != NULL) {
		/* this field is optional */
		numSpecs = CFArrayGetCount(diskTrustSettings);
	}

	/*
	 * Convert to API-style array of dictionaries.
	 * We give the caller an array even if it's empty.
	 */
	CFRef<CFMutableArrayRef> outArray(CFArrayCreateMutable(NULL, numSpecs,
		&kCFTypeArrayCallBacks));
	for(CFIndex dex=0; dex<numSpecs; dex++) {
		CFDictionaryRef diskTsDict =
			(CFDictionaryRef)CFArrayGetValueAtIndex(diskTrustSettings, dex);
		/* already validated... */
		assert(CFGetTypeID(diskTsDict) == CFDictionaryGetTypeID());

		CFTypeRef   certPolicy = (CFTypeRef)  CFDictionaryGetValue(diskTsDict, kSecTrustSettingsPolicy);
		CFStringRef	policyName = (CFStringRef)CFDictionaryGetValue(diskTsDict, kSecTrustSettingsPolicyName);
		CFDataRef   certApp    = (CFDataRef)  CFDictionaryGetValue(diskTsDict, kSecTrustSettingsApplication);
		CFStringRef policyStr  = (CFStringRef)CFDictionaryGetValue(diskTsDict, kSecTrustSettingsPolicyString);
		CFNumberRef allowedErr = (CFNumberRef)CFDictionaryGetValue(diskTsDict, kSecTrustSettingsAllowedError);
		CFNumberRef resultType = (CFNumberRef)CFDictionaryGetValue(diskTsDict, kSecTrustSettingsResult);
		CFNumberRef keyUsage   = (CFNumberRef)CFDictionaryGetValue(diskTsDict, kSecTrustSettingsKeyUsage);

		if((certPolicy == NULL) &&
		   (certApp == NULL) &&
		   (policyStr == NULL) &&
		   (allowedErr == NULL) &&
		   (resultType == NULL) &&
		   (keyUsage == NULL)) {
			/* weird but legal */
			continue;
		}
		CFRef<CFMutableDictionaryRef> outTsDict(CFDictionaryCreateMutable(NULL,
			0,			// capacity
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks));

		if(certPolicy != NULL) {
			SecPolicyRef policyRef = NULL;
			if (CFDataGetTypeID() == CFGetTypeID(certPolicy)) {
				/* convert OID as CFDataRef to SecPolicyRef */
				CSSM_OID policyOid = { int_cast<CFIndex, CSSM_SIZE>(CFDataGetLength((CFDataRef)certPolicy)),
					(uint8 *)CFDataGetBytePtr((CFDataRef)certPolicy) };
				OSStatus ortn = SecPolicyCopy(CSSM_CERT_X_509v3, &policyOid, &policyRef);
				if(ortn) {
					trustSettingsDbg("copyTrustSettings: OID conversion error");
					abort("Bad Policy OID in trusted root list", errSecInvalidTrustedRootRecord);
				}
			} else if (CFStringGetTypeID() == CFGetTypeID(certPolicy)) {
				policyRef = SecPolicyCreateWithProperties(certPolicy, NULL);
			}
			if (policyRef) {
				CFDictionaryAddValue(outTsDict, kSecTrustSettingsPolicy, policyRef);
				CFRelease(policyRef);			// owned by dictionary
			}
		}

		if (policyName != NULL) {
			/*
			 * copy, since policyName is in our mutable dictionary and could change out from
			 * under the caller
			 */
			CFStringRef str = CFStringCreateCopy(NULL, policyName);
			CFDictionaryAddValue(outTsDict, kSecTrustSettingsPolicyName, str);
			CFRelease(str);			// owned by dictionary
		}

		if(certApp != NULL) {
			/* convert app as CFDataRef to SecTrustedApplicationRef */
			SecTrustedApplicationRef appRef;
			OSStatus ortn = SecTrustedApplicationCreateWithExternalRepresentation(certApp, &appRef);
			if(ortn) {
				trustSettingsDbg("copyTrustSettings: App conversion error");
				abort("Bad application data in trusted root list", errSecInvalidTrustedRootRecord);
			}
			CFDictionaryAddValue(outTsDict, kSecTrustSettingsApplication, appRef);
			CFRelease(appRef);			// owned by dictionary
		}

		/* remaining 4 are trivial */
		if(policyStr != NULL) {
			/*
			 * copy, since policyStr is in our mutable dictionary and could change out from
			 * under the caller
			 */
			CFStringRef str = CFStringCreateCopy(NULL, policyStr);
			CFDictionaryAddValue(outTsDict, kSecTrustSettingsPolicyString, str);
			CFRelease(str);			// owned by dictionary
		}
		if(allowedErr != NULL) {
			/* there is no mutable CFNumber, so.... */
			CFDictionaryAddValue(outTsDict, kSecTrustSettingsAllowedError, allowedErr);
		}
		if(resultType != NULL) {
			CFDictionaryAddValue(outTsDict, kSecTrustSettingsResult, resultType);
		}
		if(keyUsage != NULL) {
			CFDictionaryAddValue(outTsDict, kSecTrustSettingsKeyUsage, keyUsage);
		}
		CFArrayAppendValue(outArray, outTsDict);
		/* outTsDict autoreleases; owned by outArray now */
	}
	CFRetain(outArray);		// now that it's good to go....
	return outArray;
}

CFDateRef TrustSettings::copyModDate(
	SecCertificateRef	certRef)
{
	CFDictionaryRef certDict = NULL;

	/* find the on-disk usage constraints dictionary for this cert */
	certDict = findDictionaryForCert(certRef);
	if(certDict == NULL) {
		trustSettingsDbg("copyModDate: dictionary not found");
		return NULL;
	}
	CFDateRef modDate = (CFDateRef)CFDictionaryGetValue(certDict, kTrustRecordModDate);
	if(modDate == NULL) {
		return NULL;
	}

	/* this only works becuase there is no mutable CFDateRef */
	CFRetain(modDate);
	return modDate;
}

/*
 * Modify cert's trust settings, or add a new cert to the record.
 */
void TrustSettings::setTrustSettings(
	SecCertificateRef	certRef,
	CFTypeRef			trustSettingsDictOrArray)
{
	/* to validate, we need to know if the cert is self-signed */
	OSStatus ortn;
	Boolean isSelfSigned = false;

	if(certRef == kSecTrustSettingsDefaultRootCertSetting) {
		/*
 		 * Validate settings as if this were root, specifically,
		 * kSecTrustSettingsResultTrustRoot (explicitly or by
		 * default) is OK.
		 */
		isSelfSigned = true;
	}
	else {
		ortn = SecCertificateIsSelfSigned(certRef, &isSelfSigned);
		if(ortn) {
			MacOSError::throwMe(ortn);
		}
	}

	/* caller's app/policy spec OK? */
	CFRef<CFArrayRef> trustSettings(validateApiTrustSettings(
		trustSettingsDictOrArray, isSelfSigned));

	/* caller is responsible for ensuring these */
	assert(mPropList != NULL);
	assert(mDomain != kSecTrustSettingsDomainSystem);

	/* extract issuer and serial number from the cert, if it's a cert */
	CFRef<CFDataRef> issuer;
	CFRef<CFDataRef> serial;
	if(certRef != kSecTrustSettingsDefaultRootCertSetting) {
		copyIssuerAndSerial(certRef, issuer.take(), serial.take());
	}
	else {
		UInt8 dummy;
		issuer = CFDataCreate(NULL, &dummy, 0);
		serial = CFDataCreate(NULL, &dummy, 0);
	}

	/* SHA1 digest as string */
	CFRef<CFStringRef> certHashStr(SecTrustSettingsCertHashStrFromCert(certRef));
	if(!certHashStr) {
		trustSettingsDbg("TrustSettings::setTrustSettings: CertHashStrFromCert error");
		MacOSError::throwMe(errSecItemNotFound);
	}

	/*
	 * Find entry for this cert, if present.
	 */
	CFMutableDictionaryRef certDict =
		(CFMutableDictionaryRef)findDictionaryForCertHash(certHashStr);
	if(certDict == NULL) {
		/* create new dictionary */
		certDict = CFDictionaryCreateMutable(NULL, kSecTrustRecordNumCertDictKeys,
			&kCFCopyStringDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if(certDict == NULL) {
			MacOSError::throwMe(errSecAllocate);
		}
		CFDictionaryAddValue(certDict, kTrustRecordIssuer, issuer);
		CFDictionaryAddValue(certDict, kTrustRecordSerialNumber, serial);
		if(CFArrayGetCount(trustSettings) != 0) {
			/* skip this if the settings array is empty */
			CFDictionaryAddValue(certDict, kTrustRecordTrustSettings, trustSettings);
		}
		tsSetModDate(certDict);

		/* add this new cert dictionary to top-level mTrustDict */
		CFDictionaryAddValue(mTrustDict, static_cast<CFStringRef>(certHashStr), certDict);

		/* mTrustDict owns the dictionary now */
		CFRelease(certDict);
	}
	else {
		/* update */
		tsSetModDate(certDict);
		if(CFArrayGetCount(trustSettings) != 0) {
			CFDictionarySetValue(certDict, kTrustRecordTrustSettings, trustSettings);
		}
		else {
			/* empty settings array: remove from dictionary */
			CFDictionaryRemoveValue(certDict, kTrustRecordTrustSettings);
		}
	}
	mDirty = true;
}

/*
 * Delete a certificate's trust settings.
 */
void TrustSettings::deleteTrustSettings(
	SecCertificateRef	certRef)
{
	CFDictionaryRef certDict = NULL;

	/* caller is responsible for ensuring these */
	assert(mPropList != NULL);
	assert(mDomain != kSecTrustSettingsDomainSystem);

	/* SHA1 digest as string */
	CFRef<CFStringRef> certHashStr(SecTrustSettingsCertHashStrFromCert(certRef));
	if(!certHashStr) {
		MacOSError::throwMe(errSecItemNotFound);
	}

	/* present in top-level mTrustDict? */
	certDict = findDictionaryForCertHash(certHashStr);
	if(certDict != NULL) {
		CFDictionaryRemoveValue(mTrustDict, static_cast<CFStringRef>(certHashStr));
		mDirty = true;
	}
	else {
		/*
		 * Throwing this error is the only reason we don't blindly do
		 * a CFDictionaryRemoveValue() without first doing
		 * findDictionaryForCertHash().
		 */
		trustSettingsDbg("TrustSettings::deleteRoot: cert dictionary not found");
		MacOSError::throwMe(errSecItemNotFound);
	}
}

#pragma mark --- Private methods ---

/*
 * Find a given cert's entry in the top-level mTrustDict. Return the
 * entry as a dictionary. Returned dictionary is not refcounted.
 * The mutability of the returned dictionary is the same as the mutability
 * of the underlying StickRecord::mPropList, which the caller is just
 * going to have to know (and cast accordingly if a mutable dictionary
 * is needed).
 */
CFDictionaryRef TrustSettings::findDictionaryForCert(
	SecCertificateRef	certRef)
{
	CFRef<CFStringRef> certHashStr(SecTrustSettingsCertHashStrFromCert(certRef));
	if (certHashStr.get() == NULL)
	{
		return NULL;
	}

	return findDictionaryForCertHash(static_cast<CFStringRef>(certHashStr.get()));
}

/*
 * Find entry in mTrustDict given cert hash string.
 */
CFDictionaryRef TrustSettings::findDictionaryForCertHash(
	CFStringRef		certHashStr)
{
	assert(mTrustDict != NULL);
	return (CFDictionaryRef)CFDictionaryGetValue(mTrustDict, certHashStr);
}

/*
 * Validate incoming trust settings, which may be NULL, a dictionary, or
 * an array of dictionaries. Convert from the API-style dictionaries
 * to the internal style suitable for writing to disk as part of
 * mPropList.
 *
 * We return a refcounted CFArray in any case if the incoming parameter is good.
 */
CFArrayRef TrustSettings::validateApiTrustSettings(
	CFTypeRef trustSettingsDictOrArray,
	Boolean isSelfSigned)
{
	CFArrayRef tmpInArray = NULL;

	if(trustSettingsDictOrArray == NULL) {
		/* trivial case, only valid for roots */
		if(!isSelfSigned) {
			trustSettingsDbg("validateApiUsageConstraints: !isSelfSigned, no settings");
			MacOSError::throwMe(errSecParam);
		}
		return CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
	}
	else if(CFGetTypeID(trustSettingsDictOrArray) == CFDictionaryGetTypeID()) {
		/* array-ize it */
		tmpInArray = CFArrayCreate(NULL, &trustSettingsDictOrArray, 1,
			&kCFTypeArrayCallBacks);
	}
	else if(CFGetTypeID(trustSettingsDictOrArray) == CFArrayGetTypeID()) {
		/* as is, refcount - we'll release later */
		tmpInArray = (CFArrayRef)trustSettingsDictOrArray;
		CFRetain(tmpInArray);
	}
	else {
		trustSettingsDbg("validateApiUsageConstraints: bad trustSettingsDictOrArray");
		MacOSError::throwMe(errSecParam);
	}

	CFIndex numSpecs = CFArrayGetCount(tmpInArray);
	CFMutableArrayRef outArray = CFArrayCreateMutable(NULL, numSpecs, &kCFTypeArrayCallBacks);
	CSSM_OID oid;
	OSStatus ortn = errSecSuccess;
	SecPolicyRef certPolicy;
	SecTrustedApplicationRef certApp;

	/* convert */
	for(CFIndex dex=0; dex<numSpecs; dex++) {
		CFTypeRef   oidData = NULL;
		CFStringRef policyName = NULL;
		CFDataRef   appData = NULL;
		CFStringRef policyStr = NULL;
		CFNumberRef allowedErr = NULL;
		CFNumberRef resultType = NULL;
		CFNumberRef keyUsage = NULL;
		SInt32 resultNum;
		SecTrustSettingsResult result;

		/* each element is a dictionary */
		CFDictionaryRef ucDict = (CFDictionaryRef)CFArrayGetValueAtIndex(tmpInArray, dex);
		if(CFGetTypeID(ucDict) != CFDictionaryGetTypeID()) {
			trustSettingsDbg("validateAppPolicyArray: malformed usageConstraint dictionary");
			ortn = errSecParam;
			break;
		}

		/* policy - optional */
		certPolicy = (SecPolicyRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicy);
		if(certPolicy != NULL) {
			if(CFGetTypeID(certPolicy) != SecPolicyGetTypeID()) {
				trustSettingsDbg("validateAppPolicyArray: malformed certPolicy");
				ortn = errSecParam;
				break;
			}
			ortn = SecPolicyGetOID(certPolicy, &oid);
			if (ortn) {
				/* newer policies don't have CSSM OIDs but they do have string OIDs */
				oidData = CFRetain(SecPolicyGetOidString(certPolicy));
			} else {
				oidData = CFDataCreate(NULL, oid.Data, oid.Length);
			}

			if (!oidData) {
				trustSettingsDbg("validateAppPolicyArray: SecPolicyGetOID error");
				break;
			}
			policyName = SecPolicyGetName(certPolicy);
		}

		/* application - optional */
		certApp = (SecTrustedApplicationRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsApplication);
		if(certApp != NULL) {
			if(CFGetTypeID(certApp) != SecTrustedApplicationGetTypeID()) {
				trustSettingsDbg("validateAppPolicyArray: malformed certApp");
				ortn = errSecParam;
				break;
			}
			ortn = SecTrustedApplicationCopyExternalRepresentation(certApp, &appData);
			if(ortn) {
				trustSettingsDbg("validateAppPolicyArray: "
					"SecTrustedApplicationCopyExternalRepresentation error");
				break;
			}
		}

		policyStr  = (CFStringRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicyString);
		if(policyStr != NULL) {
			if(CFGetTypeID(policyStr) != CFStringGetTypeID()) {
				trustSettingsDbg("validateAppPolicyArray: malformed policyStr");
				ortn = errSecParam;
				break;
			}
		}
		allowedErr = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsAllowedError);
		if(!tsIsGoodCfNum(allowedErr)) {
			trustSettingsDbg("validateAppPolicyArray: malformed allowedErr");
			ortn = errSecParam;
			break;
		}
		resultType = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsResult);
		if(!tsIsGoodCfNum(resultType, &resultNum)) {
			trustSettingsDbg("validateAppPolicyArray: malformed resultType");
			ortn = errSecParam;
			break;
		}
		result = (SecTrustSettingsResult) resultNum;
		/* validate result later */

		keyUsage   = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsKeyUsage);
		if(!tsIsGoodCfNum(keyUsage)) {
			trustSettingsDbg("validateAppPolicyArray: malformed keyUsage");
			ortn = errSecParam;
			break;
		}

		if(!oidData && !appData && !policyStr &&
		   !allowedErr && !resultType && !keyUsage) {
			/* nothing here - weird, but legal - skip it */
			continue;
		}

		/* create dictionary for this usageConstraint */
		CFMutableDictionaryRef outDict = CFDictionaryCreateMutable(NULL,
			2,
			&kCFTypeDictionaryKeyCallBacks,
			&kCFTypeDictionaryValueCallBacks);
		if(oidData) {
			CFDictionaryAddValue(outDict, kSecTrustSettingsPolicy, oidData);
			CFRelease(oidData);			// owned by dictionary
		}
		if(policyName) {
			CFDictionaryAddValue(outDict, kSecTrustSettingsPolicyName, policyName);
			/* still owned by ucDict */
		}
		if(appData) {
			CFDictionaryAddValue(outDict, kSecTrustSettingsApplication, appData);
			CFRelease(appData);			// owned by dictionary
		}
		if(policyStr) {
			CFDictionaryAddValue(outDict, kSecTrustSettingsPolicyString, policyStr);
			/* still owned by ucDict */
		}
		if(allowedErr) {
			CFDictionaryAddValue(outDict, kSecTrustSettingsAllowedError, allowedErr);
		}

		ortn = errSecSuccess;

		if(resultType) {
			/* let's be really picky on this one */
			switch(result) {
				case kSecTrustSettingsResultInvalid:
					ortn = errSecParam;
					break;
				case kSecTrustSettingsResultTrustRoot:
					if(!isSelfSigned) {
						trustSettingsDbg("validateAppPolicyArray: TrustRoot, !isSelfSigned");
						ortn = errSecParam;
					}
					break;
				case kSecTrustSettingsResultTrustAsRoot:
					if(isSelfSigned) {
						trustSettingsDbg("validateAppPolicyArray: TrustAsRoot, isSelfSigned");
						ortn = errSecParam;
					}
					break;
				case kSecTrustSettingsResultDeny:
				case kSecTrustSettingsResultUnspecified:
					break;
				default:
					trustSettingsDbg("validateAppPolicyArray: bogus resultType");
					ortn = errSecParam;
					break;
			}
			if(ortn) {
				break;
			}
			CFDictionaryAddValue(outDict, kSecTrustSettingsResult, resultType);
		}
		else {
			/* no resultType; default of TrustRoot only valid for root */
			if(!isSelfSigned) {
				trustSettingsDbg("validateAppPolicyArray: default result, !isSelfSigned");
				ortn = errSecParam;
				break;
			}
		}

		if(keyUsage) {
			CFDictionaryAddValue(outDict, kSecTrustSettingsKeyUsage, keyUsage);
		}

		/* append dictionary to output */
		CFArrayAppendValue(outArray, outDict);
		/* array owns the dictionary now */
		CFRelease(outDict);

	}	/* for each usage constraint dictionary */

	CFRelease(tmpInArray);
	if(ortn) {
		CFRelease(outArray);
		MacOSError::throwMe(ortn);
	}
	return outArray;
}

/*
 * Validate an trust settings array obtained from disk.
 * Returns true if OK, else returns false.
 */
bool TrustSettings::validateTrustSettingsArray(
	CFArrayRef trustSettings)
{
	CFIndex numSpecs = CFArrayGetCount(trustSettings);
	for(CFIndex dex=0; dex<numSpecs; dex++) {
		CFDictionaryRef ucDict = (CFDictionaryRef)CFArrayGetValueAtIndex(trustSettings,
			dex);
		if(CFGetTypeID(ucDict) != CFDictionaryGetTypeID()) {
			trustSettingsDbg("validateAppPolicyArray: malformed app/policy dictionary");
			return false;
		}
		CFDataRef certPolicy = (CFDataRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicy);
		if((certPolicy != NULL) && (CFGetTypeID(certPolicy) != CFDataGetTypeID())) {
			trustSettingsDbg("validateAppPolicyArray: malformed certPolicy");
			return false;
		}
		CFDataRef certApp = (CFDataRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsApplication);
		if((certApp != NULL) && (CFGetTypeID(certApp) != CFDataGetTypeID())) {
			trustSettingsDbg("validateAppPolicyArray: malformed certApp");
			return false;
		}
		CFStringRef policyStr = (CFStringRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsPolicyString);
		if((policyStr != NULL) && (CFGetTypeID(policyStr) != CFStringGetTypeID())) {
			trustSettingsDbg("validateAppPolicyArray: malformed policyStr");
			return false;
		}
		CFNumberRef cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsAllowedError);
		if(!tsIsGoodCfNum(cfNum)) {
			trustSettingsDbg("validateAppPolicyArray: malformed allowedErr");
			return false;
		}
		cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsResult);
		if(!tsIsGoodCfNum(cfNum)) {
			trustSettingsDbg("validateAppPolicyArray: malformed resultType");
			return false;
		}
		cfNum = (CFNumberRef)CFDictionaryGetValue(ucDict, kSecTrustSettingsKeyUsage);
		if(!tsIsGoodCfNum(cfNum)) {
			trustSettingsDbg("validateAppPolicyArray: malformed keyUsage");
			return false;
		}
	}	/* for each usageConstraint dictionary */
	return true;
}

/*
 * Validate mPropList after it's read from disk or supplied as an external
 * representation. Allows subsequent use of mTrustDict to proceed with
 * relative impunity.
 */
void TrustSettings::validatePropList(bool trim)
{
	/* top level dictionary */
	if(!mPropList) {
		trustSettingsDbg("TrustSettings::validatePropList missing mPropList");
		abort("missing propList", errSecInvalidTrustedRootRecord);
	}

	if(CFGetTypeID(mPropList) != CFDictionaryGetTypeID()) {
		trustSettingsDbg("TrustSettings::validatePropList: malformed mPropList");
		abort("malformed propList", errSecInvalidTrustedRootRecord);
	}

	/* That dictionary has two entries */
	CFNumberRef cfVers = (CFNumberRef)CFDictionaryGetValue(mPropList, kTrustRecordVersion);
	if((cfVers == NULL) || (CFGetTypeID(cfVers) != CFNumberGetTypeID())) {
		trustSettingsDbg("TrustSettings::validatePropList: malformed version");
		abort("malformed version", errSecInvalidTrustedRootRecord);
	}
	if(!CFNumberGetValue(cfVers, kCFNumberSInt32Type, &mDictVersion)) {
		trustSettingsDbg("TrustSettings::validatePropList: malformed version");
		abort("malformed version", errSecInvalidTrustedRootRecord);
	}
	if((mDictVersion > kSecTrustRecordVersionCurrent) ||
	   (mDictVersion == kSecTrustRecordVersionInvalid)) {
		trustSettingsDbg("TrustSettings::validatePropList: incompatible version");
		abort("incompatible version", errSecInvalidTrustedRootRecord);
	}
	/* other backwards-compatibility handling done later, if needed, per mDictVersion */

	mTrustDict = (CFMutableDictionaryRef)CFDictionaryGetValue(mPropList, kTrustRecordTrustList);
	if(mTrustDict != NULL) {
		CFRetain(mTrustDict);
	}
	if((mTrustDict == NULL) || (CFGetTypeID(mTrustDict) != CFDictionaryGetTypeID())) {
		trustSettingsDbg("TrustSettings::validatePropList: malformed mTrustDict");
		abort("malformed TrustArray", errSecInvalidTrustedRootRecord);
	}

	/* grind through the per-cert entries */
	CFIndex numCerts = CFDictionaryGetCount(mTrustDict);
	const void *dictKeys[numCerts];
	const void *dictValues[numCerts];
	CFDictionaryGetKeysAndValues(mTrustDict, dictKeys, dictValues);

	for(CFIndex dex=0; dex<numCerts; dex++) {
		/* get per-cert dictionary */
		CFMutableDictionaryRef certDict = (CFMutableDictionaryRef)dictValues[dex];
		if((certDict == NULL) || (CFGetTypeID(certDict) != CFDictionaryGetTypeID())) {
			trustSettingsDbg("TrustSettings::validatePropList: malformed certDict");
			abort("malformed certDict", errSecInvalidTrustedRootRecord);
		}

		/*
		 * That dictionary has exactly four entries.
		 * If we're trimming, all we need is the actual trust settings.
		 */

		/* issuer */
		CFDataRef cfd = (CFDataRef)CFDictionaryGetValue(certDict, kTrustRecordIssuer);
		if(cfd == NULL) {
			trustSettingsDbg("TrustSettings::validatePropList: missing issuer");
			abort("missing issuer", errSecInvalidTrustedRootRecord);
		}
		if(CFGetTypeID(cfd) != CFDataGetTypeID()) {
			trustSettingsDbg("TrustSettings::validatePropList: malformed issuer");
			abort("malformed issuer", errSecInvalidTrustedRootRecord);
		}
		if(trim) {
			CFDictionaryRemoveValue(certDict, kTrustRecordIssuer);
		}

		/* serial number */
		cfd = (CFDataRef)CFDictionaryGetValue(certDict, kTrustRecordSerialNumber);
		if(cfd == NULL) {
			trustSettingsDbg("TrustSettings::validatePropList: missing serial number");
			abort("missing serial number", errSecInvalidTrustedRootRecord);
		}
		if(CFGetTypeID(cfd) != CFDataGetTypeID()) {
			trustSettingsDbg("TrustSettings::validatePropList: malformed serial number");
			abort("malformed serial number", errSecInvalidTrustedRootRecord);
		}
		if(trim) {
			CFDictionaryRemoveValue(certDict, kTrustRecordSerialNumber);
		}

		/* modification date */
		CFDateRef modDate = (CFDateRef)CFDictionaryGetValue(certDict, kTrustRecordModDate);
		if(modDate == NULL) {
			trustSettingsDbg("TrustSettings::validatePropList: missing modDate");
			abort("missing modDate", errSecInvalidTrustedRootRecord);
		}
		if(CFGetTypeID(modDate) != CFDateGetTypeID()) {
			trustSettingsDbg("TrustSettings::validatePropList: malformed modDate");
			abort("malformed modDate", errSecInvalidTrustedRootRecord);
		}
		if(trim) {
			CFDictionaryRemoveValue(certDict, kTrustRecordModDate);
		}

		/* the actual trust settings */
		CFArrayRef trustSettings = (CFArrayRef)CFDictionaryGetValue(certDict,
				kTrustRecordTrustSettings);
		if(trustSettings == NULL) {
			/* optional; this cert's entry is good */
			continue;
		}
		if(CFGetTypeID(trustSettings) != CFArrayGetTypeID()) {
			trustSettingsDbg("TrustSettings::validatePropList: malformed useConstraint"
				"array");
			abort("malformed useConstraint array", errSecInvalidTrustedRootRecord);
		}

		/* Now validate the usageConstraint array contents */
		if(!validateTrustSettingsArray(trustSettings)) {
			abort("malformed useConstraint array", errSecInvalidTrustedRootRecord);
		}
	} /* for each cert dictionary  in top-level array */

	if(trim) {
		/* we don't need the top-level dictionary any more */
		CFRelease(mPropList);
		mPropList = NULL;
	}
}

/*
 * Obtain non-normalized issuer and serial number for specified cert, both
 * returned as CFDataRefs owned by caller.
 */
void TrustSettings::copyIssuerAndSerial(
	SecCertificateRef	certRef,
	CFDataRef			*issuer,		/* optional, RETURNED */
	CFDataRef			*serial)		/* RETURNED */
{
	CFRef<SecCertificateRef> certificate = SecCertificateCreateItemImplInstance(certRef);

	SecPointer<Certificate> cert = Certificate::required(certificate);
	CSSM_DATA_PTR fieldVal;

	if(issuer != NULL) {
		fieldVal = cert->copyFirstFieldValue(CSSMOID_X509V1IssuerNameStd);
		*issuer = CFDataCreate(NULL, fieldVal->Data, fieldVal->Length);
		cert->releaseFieldValue(CSSMOID_X509V1IssuerNameStd, fieldVal);
	}

	fieldVal = cert->copyFirstFieldValue(CSSMOID_X509V1SerialNumber);
	*serial = CFDataCreate(NULL, fieldVal->Data, fieldVal->Length);
	cert->releaseFieldValue(CSSMOID_X509V1SerialNumber, fieldVal);
}

void TrustSettings::abort(
	const char *why,
	OSStatus err)
{
	Syslog::error("TrustSettings: %s", why);
	MacOSError::throwMe(err);
}

