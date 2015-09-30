/*
 * Copyright (c) 2002-2015 Apple Inc. All Rights Reserved.
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

#include "SecTrust.h"
#include "SecTrustPriv.h"
#include "Trust.h"
#include <security_keychain/SecTrustSettingsPriv.h>
#include "SecBase.h"
#include "SecBridge.h"
#include "SecInternal.h"
#include "SecInternalP.h"
#include "SecTrustSettings.h"
#include "SecCertificatePriv.h"
#include "SecCertificateP.h"
#include "SecCertificatePrivP.h"
#include "SecPolicyPriv.h"
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>
#include <CoreFoundation/CoreFoundation.h>
#include <syslog.h>

// forward declarations
#if !SECTRUST_OSX
CFArrayRef SecTrustCopyDetails(SecTrustRef trust);
static CFDictionaryRef SecTrustGetExceptionForCertificateAtIndex(SecTrustRef trust, CFIndex ix);
static void SecTrustCheckException(const void *key, const void *value, void *context);
#endif

typedef struct SecTrustCheckExceptionContext {
	CFDictionaryRef exception;
	bool exceptionNotFound;
} SecTrustCheckExceptionContext;

// public trust result constants
const CFStringRef kSecTrustEvaluationDate           = CFSTR("TrustEvaluationDate");
const CFStringRef kSecTrustExtendedValidation       = CFSTR("TrustExtendedValidation");
const CFStringRef kSecTrustOrganizationName         = CFSTR("Organization");
const CFStringRef kSecTrustResultValue              = CFSTR("TrustResultValue");
const CFStringRef kSecTrustRevocationChecked        = CFSTR("TrustRevocationChecked");
const CFStringRef kSecTrustRevocationReason         = CFSTR("TrustRevocationReason");
const CFStringRef kSecTrustRevocationValidUntilDate = CFSTR("TrustExpirationDate");
const CFStringRef kSecTrustResultDetails            = CFSTR("TrustResultDetails");

//
// CF boilerplate
//
#if !SECTRUST_OSX
CFTypeID SecTrustGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().Trust.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}
#endif

//
// Sec* API bridge functions
//
#if !SECTRUST_OSX
OSStatus SecTrustCreateWithCertificates(
	CFTypeRef certificates,
	CFTypeRef policies,
	SecTrustRef *trustRef)
{
	BEGIN_SECAPI
	Required(trustRef);
	*trustRef = (new Trust(certificates, policies))->handle();
	END_SECAPI
}
#endif

#if !SECTRUST_OSX
OSStatus
SecTrustSetPolicies(SecTrustRef trustRef, CFTypeRef policies)
{
	BEGIN_SECAPI
	Trust::required(trustRef)->policies(policies);
	END_SECAPI
}
#endif

/* OS X only: __OSX_AVAILABLE_STARTING(__MAC_10_7, __IPHONE_NA) */
OSStatus
SecTrustSetOptions(SecTrustRef trustRef, SecTrustOptionFlags options)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	CSSM_APPLE_TP_ACTION_DATA actionData = {
		CSSM_APPLE_TP_ACTION_VERSION,
		(CSSM_APPLE_TP_ACTION_FLAGS)options
	};
	Trust *trust = Trust::required(trustRef);
	CFDataRef actionDataRef = CFDataCreate(NULL,
		(const UInt8 *)&actionData,
		(CFIndex)sizeof(CSSM_APPLE_TP_ACTION_DATA));
	trust->action(CSSM_TP_ACTION_DEFAULT);
	trust->actionData(actionDataRef);
	if (actionDataRef) CFRelease(actionDataRef);
	END_SECAPI
#else
	/* bridge to support API functionality for legacy callers */
	OSStatus status = errSecSuccess;
#if 1
#warning STU: <rdar://21328005>
//%%% need to ensure that the exception covers only the requested options
#else
	CFArrayRef details = SecTrustGetDetails(trustRef); // NOTE: performs the evaluation if not done already
	CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
	CFIndex ix;
	for (ix = 0; ix < pathLength; ++ix) {
		CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
		CFIndex detailCount = CFDictionaryGetCount(detail);
		if (detailCount > 0) {
			// see if we can ignore this error
			syslog(LOG_ERR, "SecTrustSetOptions: examining detail dictionary items at ix %ld", (long)ix);
			CFShow(detail);
		}
	}
	syslog(LOG_ERR, "SecTrustSetOptions: creating trust exception");
#endif
	CFDataRef exceptions = SecTrustCopyExceptions(trustRef);
	if (exceptions) {
		SecTrustSetExceptions(trustRef, exceptions);
		CFRelease(exceptions);
	}


#if SECTRUST_DEPRECATION_WARNINGS
	bool displayModifyMsg = false;
	bool displayNetworkMsg = false;
	bool displayPolicyMsg = false;
	const char *baseMsg = "WARNING: SecTrustSetOptions called with";
	const char *modifyMsg = "Use SecTrustSetExceptions and SecTrustCopyExceptions to modify default trust results.";
	const char *networkMsg = "Use SecTrustSetNetworkFetchAllowed to specify whether missing certificates can be fetched from the network.";
	const char *policyMsg = "Use SecPolicyCreateRevocation to specify revocation policy requirements.";

	if (options & kSecTrustOptionAllowExpired) {
		syslog(LOG_ERR, "%s %s.", baseMsg, "kSecTrustOptionAllowExpired");
		displayModifyMsg = true;
	}
	if (options & kSecTrustOptionAllowExpiredRoot) {
		syslog(LOG_ERR, "%s %s.", baseMsg, "kSecTrustOptionAllowExpiredRoot");
		displayModifyMsg = true;
	}
	if (options & kSecTrustOptionFetchIssuerFromNet) {
		syslog(LOG_ERR, "%s %s.", baseMsg, "kSecTrustOptionFetchIssuerFromNet");
		displayNetworkMsg = true;
	}
	if (options & kSecTrustOptionRequireRevPerCert) {
		syslog(LOG_ERR, "%s %s.", baseMsg, "kSecTrustOptionRequireRevPerCert");
		displayPolicyMsg = true;
	}
	if (displayModifyMsg || displayNetworkMsg || displayPolicyMsg) {
		syslog(LOG_ERR, "%s %s %s",
			(displayModifyMsg) ? modifyMsg : "",
			(displayNetworkMsg) ? networkMsg : "",
			(displayPolicyMsg) ? policyMsg : "");
	}
#endif

	return status;

#endif
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustSetParameters(
    SecTrustRef trustRef,
    CSSM_TP_ACTION action,
    CFDataRef actionData)
{
#if !SECTRUST_OSX
    BEGIN_SECAPI
    Trust *trust = Trust::required(trustRef);
    trust->action(action);
    trust->actionData(actionData);
    END_SECAPI
#else
	/* bridge to support API functionality for legacy callers */
	OSStatus status;
	CSSM_APPLE_TP_ACTION_FLAGS actionFlags = 0;
	if (actionData) {
		CSSM_APPLE_TP_ACTION_DATA *actionDataPtr = (CSSM_APPLE_TP_ACTION_DATA *) CFDataGetBytePtr(actionData);
		if (actionDataPtr) {
			actionFlags = actionDataPtr->ActionFlags;
		}
	}
	// note that SecTrustOptionFlags == CSSM_APPLE_TP_ACTION_FLAGS;
	// both are sizeof(uint32) and the flag values have identical meanings
    status = SecTrustSetOptions(trustRef, (SecTrustOptionFlags)actionFlags);

#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetParameters was deprecated in 10.7. Use SecTrustSetOptions instead.");
#endif

	return status;

#endif
}

#if !SECTRUST_OSX
OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust, CFArrayRef anchorCertificates)
{
    BEGIN_SECAPI
    Trust::required(trust)->anchors(anchorCertificates);
    END_SECAPI
}
#endif

#if !SECTRUST_OSX
OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust, Boolean anchorCertificatesOnly)
{
    BEGIN_SECAPI
    Trust::AnchorPolicy policy = (anchorCertificatesOnly) ? Trust::useAnchorsOnly : Trust::useAnchorsAndBuiltIns;
    Trust::required(trust)->anchorPolicy(policy);
    END_SECAPI
}
#endif

/* OS X only: __OSX_AVAILABLE_STARTING(__MAC_10_3, __IPHONE_NA) */
OSStatus SecTrustSetKeychains(SecTrustRef trust, CFTypeRef keychainOrArray)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
		StorageManager::KeychainList keychains;
	// avoid unnecessary global initializations if an empty array is passed in
	if (!( (keychainOrArray != NULL) &&
				(CFGetTypeID(keychainOrArray) == CFArrayGetTypeID()) &&
				(CFArrayGetCount((CFArrayRef)keychainOrArray) == 0) )) {
		globals().storageManager.optionalSearchList(keychainOrArray, keychains);
	}
	Trust::required(trust)->searchLibs(keychains);
	END_SECAPI
#else
	/* this function is currently unsupported in unified SecTrust */
    // TODO: pull all certs out of the specified keychains for the evaluation?
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetKeychains does nothing in 10.11. Use SecTrustSetAnchorCertificates{Only} to provide anchors.");
#endif
	return errSecSuccess;
#endif
}

#if !SECTRUST_OSX
OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate)
{
    BEGIN_SECAPI
    Trust::required(trust)->time(verifyDate);
    END_SECAPI
}
#endif

#if !SECTRUST_OSX
CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust)
{
	CFAbsoluteTime verifyTime = 0;
	OSStatus __secapiresult = errSecSuccess;
	try {
		CFRef<CFDateRef> verifyDate = Trust::required(trust)->time();
		verifyTime = CFDateGetAbsoluteTime(verifyDate);
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }
	return verifyTime;
}
#endif



#if !SECTRUST_OSX
OSStatus SecTrustEvaluate(SecTrustRef trust, SecTrustResultType *resultP)
{
	SecTrustResultType trustResult = kSecTrustResultInvalid;
	CFArrayRef exceptions = NULL;
	OSStatus __secapiresult = errSecSuccess;
	try {
		Trust *trustObj = Trust::required(trust);
		trustObj->evaluate();
		trustResult = trustObj->result();
		exceptions = trustObj->exceptions();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (__secapiresult) {
		return __secapiresult;
	}

	/* post-process trust result based on exceptions */
	if (trustResult == kSecTrustResultUnspecified) {
		/* If leaf is in exceptions -> proceed, otherwise unspecified. */
		if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
			trustResult = kSecTrustResultProceed;
	}
	else if (trustResult == kSecTrustResultRecoverableTrustFailure && exceptions) {
		/* If we have exceptions get details and match to exceptions. */
		CFArrayRef details = SecTrustCopyDetails(trust);
		if (details) {
			CFIndex pathLength = CFArrayGetCount(details);
			struct SecTrustCheckExceptionContext context = {};
			CFIndex ix;
			for (ix = 0; ix < pathLength; ++ix) {
				CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
			//	if ((ix == 0) && CFDictionaryContainsKey(detail, kSecPolicyCheckBlackListedLeaf))
			//		trustResult = kSecTrustResultFatalTrustFailure;
				context.exception = SecTrustGetExceptionForCertificateAtIndex(trust, ix);
				CFDictionaryApplyFunction(detail, SecTrustCheckException, &context);
				if (context.exceptionNotFound) {
					break;
				}
			}
			if (!context.exceptionNotFound)
				trustResult = kSecTrustResultProceed;
		}
	}


	secdebug("SecTrustEvaluate", "SecTrustEvaluate trust result = %d", (int)trustResult);
	if (resultP) {
		*resultP = trustResult;
	}
	return __secapiresult;
}
#endif

#if !SECTRUST_OSX
OSStatus SecTrustEvaluateAsync(SecTrustRef trust,
	dispatch_queue_t queue, SecTrustCallback result)
{
	BEGIN_SECAPI
	dispatch_async(queue, ^{
		try {
			Trust *trustObj = Trust::required(trust);
			trustObj->evaluate();
			SecTrustResultType trustResult = trustObj->result();
			result(trust, trustResult);
		}
		catch (...) {
			result(trust, kSecTrustResultInvalid);
		};
	});
	END_SECAPI
}
#endif

//
// Construct the "official" result evidence and return it
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetResult(
    SecTrustRef trustRef,
    SecTrustResultType *result,
	CFArrayRef *certChain, CSSM_TP_APPLE_EVIDENCE_INFO **statusChain)
{
#if !SECTRUST_OSX
    BEGIN_SECAPI
    Trust *trust = Trust::required(trustRef);
    if (result)
        *result = trust->result();
    if (certChain && statusChain)
        trust->buildEvidence(*certChain, TPEvidenceInfo::overlayVar(*statusChain));
    END_SECAPI
#else
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetResult has been deprecated since 10.7, and may not return a statusChain in 10.11. Please use SecTrustGetTrustResult instead.");
#endif
	SecTrustResultType trustResult;
	OSStatus status = SecTrustGetTrustResult(trustRef, &trustResult);
	if (result) {
		*result = trustResult;
	}
	if (certChain && !statusChain) {
		/* This is the easy case; caller only wants cert chain and not status chain. */
		CFMutableArrayRef certArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFIndex idx, count = SecTrustGetCertificateCount(trustRef);
		for (idx=0; idx < count; idx++) {
			SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trustRef, idx);
			if (certificate) {
				CFArrayAppendValue(certArray, certificate);
			}
		}
		*certChain = certArray;
	}
	else if (certChain && statusChain) {
		/*
		 * Here is where backward compatibility gets ugly. CSSM_TP_APPLE_EVIDENCE_INFO* is tied to a
		 * Trust object and does not exist in the new unified world. Unfortunately, some clients are
		 * still calling this legacy API and grubbing through the info for StatusBits and StatusCodes.
		 * If they want this info, then we have to do a legacy evaluation to get it. The info struct
		 * goes away once the old-style object does, so we must keep the old-style object alive after
		 * returning from the function.
		 *
		 * TODO: keep a dictionary and figure out how to expire entries when no longer needed.,
		 * or build the evidence info ourselves: rdar://21005914
		 */
		static CFMutableArrayRef sTrustArray = NULL;

		// make array of Certificate instances from unified SecCertificateRefs
		CFMutableArrayRef certArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFIndex idx, count = SecTrustGetCertificateCount(trustRef);
		for (idx=0; idx < count; idx++) {
			SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trustRef, idx);
			if (certificate) {
				SecCertificateRef itemImplRef = SecCertificateCreateItemImplInstance(certificate);
				if (itemImplRef) {
					CFArrayAppendValue(certArray, itemImplRef);
					CFRelease(itemImplRef);
				}
			}
		}
		// make array of Policy instances from unified SecPolicyRefs
		CFMutableArrayRef policyArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFArrayRef policies = NULL;
		status = SecTrustCopyPolicies(trustRef, &policies);
		count = (!status && policies) ? CFArrayGetCount(policies) : 0;
		for (idx=0; idx < count; idx++) {
			SecPolicyRef policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, idx);
			if (policy) {
				SecPolicyRef itemImplRef = SecPolicyCreateItemImplInstance(policy);
				if (itemImplRef) {
					CFArrayAppendValue(policyArray, itemImplRef);
					CFRelease(itemImplRef);
				}
			}
		}
		// now make a Trust instance and evaluate it
		try {
			Trust *trustObj = new Trust(certArray, policyArray);
			SecTrustRef trust = trustObj->handle();
			if (!trust) {
				MacOSError::throwMe(errSecTrustNotAvailable);
			}
			if (!sTrustArray) {
				sTrustArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
				if (!sTrustArray) {
					MacOSError::throwMe(errSecAllocate);
				}
			}
			// fetch the built cert chain and status chain
			CFArrayRef itemImplCertArray = NULL;
			trustObj->evaluate();
			trustObj->buildEvidence(itemImplCertArray, TPEvidenceInfo::overlayVar(*statusChain));

			// convert each Certificate in the built chain to a unified SecCertificateRef
			CFMutableArrayRef outCertChain = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
			CFIndex idx, count = (itemImplCertArray) ? CFArrayGetCount(itemImplCertArray) : 0;
			for (idx=0; idx < count; idx++) {
				SecCertificateRef inCert = (SecCertificateRef) CFArrayGetValueAtIndex(itemImplCertArray, idx);
				SecCertificateRef outCert = SecCertificateCreateFromItemImplInstance(inCert);
				if (outCert) {
					CFArrayAppendValue(outCertChain, outCert);
					CFRelease(outCert);
				}
			}
			*certChain = outCertChain;
			if (itemImplCertArray) {
				CFRelease(itemImplCertArray);
			}
			CFArrayAppendValue(sTrustArray, trust);
			status = errSecSuccess;
		}
		catch (const MacOSError &err) { status=err.osStatus(); }
		catch (const CommonError &err) { status=SecKeychainErrFromOSStatus(err.osStatus()); }
		catch (const std::bad_alloc &) { status=errSecAllocate; }
		catch (...) { status=errSecInternalComponent; }

		if (policyArray)
			CFRelease(policyArray);
		if (certArray)
			CFRelease(certArray);
	}
	return status;
#endif
}

//
// Retrieve result of trust evaluation only
//
#if !SECTRUST_OSX
OSStatus SecTrustGetTrustResult(SecTrustRef trustRef,
	SecTrustResultType *result)
{
    BEGIN_SECAPI
    Trust *trust = Trust::required(trustRef);
    if (result) *result = trust->result();
    END_SECAPI
}
#endif

//
// Retrieve extended validation trust results
//
/* OS X only: __OSX_AVAILABLE_STARTING(__MAC_10_5, __IPHONE_NA) */
OSStatus SecTrustCopyExtendedResult(SecTrustRef trust, CFDictionaryRef *result)
{
#if !SECTRUST_OSX
    BEGIN_SECAPI
	Trust *trustObj = Trust::required(trust);
	if (result == nil)
		return errSecParam;
	trustObj->extendedResult(*result);
    END_SECAPI
#else
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
    syslog(LOG_ERR, "WARNING: SecTrustCopyExtendedResult will be deprecated in an upcoming release. Please use SecTrustCopyResult instead.");
#endif
	CFDictionaryRef resultDict = SecTrustCopyResult(trust);
	if (result == nil) {
		return errSecParam;
	}
	*result = resultDict;
	return errSecSuccess;
#endif
}

//
// Retrieve CSSM-level information for those who want to dig down
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetCssmResult(SecTrustRef trust, CSSM_TP_VERIFY_CONTEXT_RESULT_PTR *result)
{
#if !SECTRUST_OSX
    BEGIN_SECAPI
    Required(result) = Trust::required(trust)->cssmResult();
    END_SECAPI
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetCssmResult has been deprecated since 10.7, and has no functional equivalent in 10.11. Please use SecTrustCopyResult instead.");
#endif
	if (result) {
		*result = NULL;
	}
	return errSecServiceNotAvailable;
#endif
}

#if SECTRUST_OSX
static void applyPropertyToCssmResultCode(const void *_key, const void *_value, void *context) {
	CFStringRef key = (CFStringRef)_key;
	CFStringRef value = (CFStringRef)_value;
	OSStatus *result = (OSStatus *)context;
	if (CFGetTypeID(_value) != CFStringGetTypeID()) {
		return;
	}
	if (!CFEqual(CFSTR("value"), key)) {
		return;
	}
	if (CFEqual(CFSTR("Invalid certificate chain linkage."), value)) {
		*result = CSSMERR_APPLETP_INVALID_ID_LINKAGE;
	} else if (CFEqual(CFSTR("One or more unsupported critical extensions found."), value)) {
		*result = CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN;
	} else if (CFEqual(CFSTR("Root certificate is not trusted."), value)) {
		*result = CSSMERR_TP_NOT_TRUSTED;
	} else if (CFEqual(CFSTR("Hostname mismatch."), value)) {
		*result = CSSMERR_APPLETP_HOSTNAME_MISMATCH;
	} else if (CFEqual(CFSTR("One or more certificates have expired or are not valid yet."), value)) {
		*result = CSSMERR_TP_CERT_EXPIRED;
	} else if (CFEqual(CFSTR("Policy requirements not met."), value)) {
		*result = CSSMERR_TP_VERIFY_ACTION_FAILED;
	}
}
#endif

//
// Retrieve CSSM_LEVEL TP return code
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetCssmResultCode(SecTrustRef trustRef, OSStatus *result)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
		Trust *trust = Trust::required(trustRef);
	if (trust->result() == kSecTrustResultInvalid)
		return errSecParam;
	else
		Required(result) = trust->cssmResultCode();
	END_SECAPI
#else
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
    syslog(LOG_ERR, "WARNING: SecTrustGetCssmResultCode has been deprecated since 10.7, and will be removed in a future release. Please use SecTrustCopyProperties instead.");
#endif
	if (!trustRef || !result) {
		return errSecParam;
	}
	CFArrayRef properties = SecTrustCopyProperties(trustRef);
	if (!properties) {
		*result = 0;
		return errSecSuccess;
	}
	OSStatus cssmResultCode = 0;
	CFIndex ix, count = CFArrayGetCount(properties);
	for (ix = 0; ix < count; ++ix) {
		CFDictionaryRef property = (CFDictionaryRef)
			CFArrayGetValueAtIndex(properties, ix);
		CFDictionaryApplyFunction(property, applyPropertyToCssmResultCode, &cssmResultCode);
	}
	if (result) {
		*result = cssmResultCode;
	}
	if (properties) {
		CFRelease(properties);
	}
	return errSecSuccess;
#endif
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetTPHandle(SecTrustRef trust, CSSM_TP_HANDLE *handle)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
		Required(handle) = Trust::required(trust)->getTPHandle();
	END_SECAPI
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetTPHandle has been deprecated since 10.7, and cannot return CSSM objects in 10.11. Please stop using it.");
#endif
	if (handle) {
		*handle = NULL;
	}
	return errSecServiceNotAvailable;
#endif
}

#if !SECTRUST_OSX
OSStatus SecTrustCopyPolicies(SecTrustRef trust, CFArrayRef *policies)
{
	BEGIN_SECAPI
		CFArrayRef currentPolicies = Trust::required(trust)->policies();
	if (currentPolicies != NULL)
	{
		CFRetain(currentPolicies);
	}

	Required(policies) = currentPolicies;
	END_SECAPI
}
#endif

#if !SECTRUST_OSX
OSStatus SecTrustSetNetworkFetchAllowed(SecTrustRef trust, Boolean allowFetch)
{
	BEGIN_SECAPI
	Trust *trustObj = Trust::required(trust);
	Trust::NetworkPolicy netPolicy = (allowFetch) ?
		Trust::useNetworkEnabled : Trust::useNetworkDisabled;
	trustObj->networkPolicy(netPolicy);
	END_SECAPI
}
#endif

#if !SECTRUST_OSX
OSStatus SecTrustGetNetworkFetchAllowed(SecTrustRef trust, Boolean *allowFetch)
{
	BEGIN_SECAPI
	Boolean allowed = false;
	Trust *trustObj = Trust::required(trust);
	Trust::NetworkPolicy netPolicy = trustObj->networkPolicy();
	if (netPolicy == Trust::useNetworkDefault) {
		// network fetch is enabled by default for SSL only
		allowed = trustObj->policySpecified(trustObj->policies(), CSSMOID_APPLE_TP_SSL);
	} else {
		// caller has explicitly set the network policy
		allowed = (netPolicy == Trust::useNetworkEnabled);
	}
	Required(allowFetch) = allowed;
	END_SECAPI
}
#endif

#if !SECTRUST_OSX
OSStatus SecTrustSetOCSPResponse(SecTrustRef trust, CFTypeRef responseData)
{
	BEGIN_SECAPI
	Trust::required(trust)->responses(responseData);
	END_SECAPI
}
#endif

#if !SECTRUST_OSX
OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust, CFArrayRef *anchorCertificates)
{
	BEGIN_SECAPI
	CFArrayRef customAnchors = Trust::required(trust)->anchors();
	Required(anchorCertificates) = (customAnchors) ?
		(const CFArrayRef)CFRetain(customAnchors) : (const CFArrayRef)NULL;
	END_SECAPI
}
#endif

//
// Get the user's default anchor certificate set
//
/* OS X only */
OSStatus SecTrustCopyAnchorCertificates(CFArrayRef *anchorCertificates)
{
	BEGIN_SECAPI

	return SecTrustSettingsCopyUnrestrictedRoots(
			true, true, true,		/* all domains */
			anchorCertificates);

	END_SECAPI
}

#if SECTRUST_OSX
/* We have an iOS-style SecTrustRef, but we need to return a CDSA-based SecKeyRef.
 */
SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust)
{
	SecKeyRef pubKey = NULL;
	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, 0);
	(void) SecCertificateCopyPublicKey(certificate, &pubKey);
	return pubKey;
}
#else
/* new in 10.6 */
SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust)
{
	SecKeyRef pubKey = NULL;
	CFArrayRef certChain = NULL;
	CFArrayRef evidenceChain = NULL;
	CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;
	OSStatus __secapiresult = errSecSuccess;
	try {
		Trust *trustObj = Trust::required(trust);
		if (trustObj->result() == kSecTrustResultInvalid) {
			// Trust hasn't been evaluated; attempt to retrieve public key from leaf.
			SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, 0);
			__secapiresult = SecCertificateCopyPublicKey(cert, &pubKey);
			if (pubKey) {
				return pubKey;
			}
			// Otherwise, we must evaluate first.
			trustObj->evaluate();
			if (trustObj->result() == kSecTrustResultInvalid) {
				MacOSError::throwMe(errSecTrustNotAvailable);
			}
		}
		if (trustObj->evidence() == nil) {
			trustObj->buildEvidence(certChain, TPEvidenceInfo::overlayVar(statusChain));
		}
		evidenceChain = trustObj->evidence();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (certChain)
		CFRelease(certChain);

	if (evidenceChain) {
		if (CFArrayGetCount(evidenceChain) > 0) {
			SecCertificateRef cert = (SecCertificateRef) CFArrayGetValueAtIndex(evidenceChain, 0);
			__secapiresult = SecCertificateCopyPublicKey(cert, &pubKey);
		}
		// do not release evidenceChain, as it is owned by the trust object.
	}
	return pubKey;
}
#endif

#if !SECTRUST_OSX
/* new in 10.6 */
CFIndex SecTrustGetCertificateCount(SecTrustRef trust)
{
	CFIndex chainLen = 0;
	CFArrayRef certChain = NULL;
	CFArrayRef evidenceChain = NULL;
	CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;
    OSStatus __secapiresult = errSecSuccess;
	try {
		Trust *trustObj = Trust::required(trust);
		if (trustObj->result() == kSecTrustResultInvalid) {
			trustObj->evaluate();
			if (trustObj->result() == kSecTrustResultInvalid)
				MacOSError::throwMe(errSecTrustNotAvailable);
		}
		if (trustObj->evidence() == nil)
			trustObj->buildEvidence(certChain, TPEvidenceInfo::overlayVar(statusChain));
		evidenceChain = trustObj->evidence();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (certChain)
		CFRelease(certChain);

	if (evidenceChain)
		chainLen = CFArrayGetCount(evidenceChain); // don't release, trust object owns it.

    return chainLen;
}
#endif

#if !SECTRUST_OSX
/* new in 10.6 */
SecCertificateRef SecTrustGetCertificateAtIndex(SecTrustRef trust, CFIndex ix)
{
	SecCertificateRef certificate = NULL;
	CFArrayRef certChain = NULL;
	CFArrayRef evidenceChain = NULL;
	CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;
    OSStatus __secapiresult = errSecSuccess;
	try {
		Trust *trustObj = Trust::required(trust);
		if (trustObj->result() == kSecTrustResultInvalid) {
			// If caller is asking for the leaf, we can return it without
			// having to evaluate the entire chain. Note that we don't retain
			// the cert as it's owned by the trust and this is a 'Get' API.
			if (ix == 0) {
				CFArrayRef certs = trustObj->certificates();
				if (certs && (CFArrayGetCount(certs) > 0)) {
					certificate = (SecCertificateRef) CFArrayGetValueAtIndex(certs, 0);
					if (certificate) {
						return certificate;
					}
				}
			}
			// Otherwise, we must evaluate first.
			trustObj->evaluate();
			if (trustObj->result() == kSecTrustResultInvalid) {
				MacOSError::throwMe(errSecTrustNotAvailable);
			}
		}
		if (trustObj->evidence() == nil) {
			trustObj->buildEvidence(certChain, TPEvidenceInfo::overlayVar(statusChain));
		}
		evidenceChain = trustObj->evidence();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (certChain)
		CFRelease(certChain);

	if (evidenceChain) {
		if (ix < CFArrayGetCount(evidenceChain)) {
			certificate = (SecCertificateRef) CFArrayGetValueAtIndex(evidenceChain, ix);
			// note: we do not retain this certificate. The assumption here is
			// that the certificate is retained by the trust object, so it is
			// valid unil the trust is released (or until re-evaluated.)
			// also note: we do not release the evidenceChain, as it is owned
			// by the trust object.
		}
	}
	return certificate;
}
#endif


#if !SECTRUST_OSX
static CFStringRef kSecCertificateDetailSHA1Digest = CFSTR("SHA1Digest");
static CFStringRef kSecCertificateDetailStatusCodes = CFSTR("StatusCodes");

static void
_AppendStatusCode(CFMutableArrayRef array, OSStatus statusCode)
{
	if (!array)
		return;
	SInt32 num = statusCode;
	CFNumberRef numRef = CFNumberCreate(NULL, kCFNumberSInt32Type, &num);
	if (!numRef)
		return;
	CFArrayAppendValue(array, numRef);
	CFRelease(numRef);
}
#endif

#if !SECTRUST_OSX
CFArrayRef SecTrustCopyDetails(SecTrustRef trust)
{
	// This function returns an array of dictionaries, one per certificate,
	// holding status info for each certificate in the evaluated chain.
	//
	CFIndex count, chainLen = 0;
	CFArrayRef certChain = NULL;
	CFMutableArrayRef details = NULL;
	CSSM_TP_APPLE_EVIDENCE_INFO *statusChain = NULL;
    OSStatus __secapiresult = errSecSuccess;
	try {
		Trust *trustObj = Trust::required(trust);
		if (trustObj->result() == kSecTrustResultInvalid) {
			trustObj->evaluate();
			if (trustObj->result() == kSecTrustResultInvalid)
				MacOSError::throwMe(errSecTrustNotAvailable);
		}
		trustObj->buildEvidence(certChain, TPEvidenceInfo::overlayVar(statusChain));
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (certChain) {
		chainLen = CFArrayGetCount(certChain);
		CFRelease(certChain);
	}
	if (statusChain) {
		details = CFArrayCreateMutable(NULL, chainLen, &kCFTypeArrayCallBacks);
		for (count = 0; count < chainLen; count++) {
			CFMutableDictionaryRef certDict = CFDictionaryCreateMutable(kCFAllocatorDefault,
				0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
			CFMutableArrayRef statusCodes = CFArrayCreateMutable(kCFAllocatorDefault,
				0, &kCFTypeArrayCallBacks);
			CSSM_TP_APPLE_EVIDENCE_INFO *evInfo = &statusChain[count];
			CSSM_TP_APPLE_CERT_STATUS statBits = evInfo->StatusBits;

			// translate status bits
			if (statBits & CSSM_CERT_STATUS_EXPIRED)
				_AppendStatusCode(statusCodes, errSecCertificateExpired);
			if (statBits & CSSM_CERT_STATUS_NOT_VALID_YET)
				_AppendStatusCode(statusCodes, errSecCertificateNotValidYet);
			if (statBits & CSSM_CERT_STATUS_TRUST_SETTINGS_DENY)
				_AppendStatusCode(statusCodes, errSecTrustSettingDeny);

			// translate status codes
			unsigned int i;
			for (i = 0; i < evInfo->NumStatusCodes; i++) {
				CSSM_RETURN scode = evInfo->StatusCodes[i];
				_AppendStatusCode(statusCodes, (OSStatus)scode);
			}

			CFDictionarySetValue(certDict, kSecCertificateDetailStatusCodes, statusCodes);
			CFRelease(statusCodes);
			CFArrayAppendValue(details, certDict);
			CFRelease(certDict);
		}
	}
	return details;
}
#endif

#if !SECTRUST_OSX
static CFDictionaryRef SecTrustGetExceptionForCertificateAtIndex(SecTrustRef trust, CFIndex ix)
{
	CFArrayRef exceptions = NULL;
    OSStatus __secapiresult = errSecSuccess;
	try {
		exceptions = Trust::required(trust)->exceptions();
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	if (!exceptions || ix >= CFArrayGetCount(exceptions))
		return NULL;
	CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, ix);
	if (CFGetTypeID(exception) != CFDictionaryGetTypeID())
		return NULL;

	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
	if (!certificate)
		return NULL;

	/* If the exception contains the current certificate's sha1Digest in the
	   kSecCertificateDetailSHA1Digest key then we use it otherwise we ignore it. */
	CFDataRef sha1Digest = SecCertificateGetSHA1Digest(certificate);
	CFTypeRef digestValue = CFDictionaryGetValue(exception, kSecCertificateDetailSHA1Digest);
	if (!digestValue || !CFEqual(sha1Digest, digestValue))
		exception = NULL;

	return exception;
}
#endif

#if !SECTRUST_OSX
static void SecTrustCheckException(const void *key, const void *value, void *context)
{
	struct SecTrustCheckExceptionContext *cec = (struct SecTrustCheckExceptionContext *)context;
	if (cec->exception) {
		CFTypeRef exceptionValue = CFDictionaryGetValue(cec->exception, key);
		if (!exceptionValue || !CFEqual(value, exceptionValue)) {
			cec->exceptionNotFound = true;
		}
	} else {
		cec->exceptionNotFound = true;
	}
}
#endif

#if !SECTRUST_OSX
/* new in 10.9 */
CFDataRef SecTrustCopyExceptions(SecTrustRef trust)
{
	CFArrayRef details = SecTrustCopyDetails(trust);
	CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
	CFMutableArrayRef exceptions = CFArrayCreateMutable(kCFAllocatorDefault,
			pathLength, &kCFTypeArrayCallBacks);
	CFIndex ix;
	for (ix = 0; ix < pathLength; ++ix) {
		CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
		CFIndex detailCount = CFDictionaryGetCount(detail);
		CFMutableDictionaryRef exception;
		if (ix == 0 || detailCount > 0) {
			exception = CFDictionaryCreateMutableCopy(kCFAllocatorDefault,
				detailCount + 1, detail);
			SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
			CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
			if (digest) {
				CFDictionaryAddValue(exception, kSecCertificateDetailSHA1Digest, digest);
			}
		} else {
			/* Add empty exception dictionaries for non leaf certs which have no exceptions
			 * to save space.
			 */
			exception = (CFMutableDictionaryRef)CFDictionaryCreate(kCFAllocatorDefault,
				NULL, NULL, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks);
		}
		CFArrayAppendValue(exceptions, exception);
		CFReleaseNull(exception);
	}
	CFReleaseSafe(details);

	/* Remove any trailing empty dictionaries to save even more space (we skip the leaf
	   since it will never be empty). */
	for (ix = pathLength; ix-- > 1;) {
		CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, ix);
		if (CFDictionaryGetCount(exception) == 0) {
			CFArrayRemoveValueAtIndex(exceptions, ix);
		} else {
			break;
		}
	}

	CFDataRef encodedExceptions = CFPropertyListCreateData(kCFAllocatorDefault,
			exceptions, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
	CFRelease(exceptions);

	return encodedExceptions;
}
#endif

#if !SECTRUST_OSX
/* new in 10.9 */
bool SecTrustSetExceptions(SecTrustRef trust, CFDataRef encodedExceptions)
{
	CFArrayRef exceptions;
	exceptions = (CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
		encodedExceptions, kCFPropertyListImmutable, NULL, NULL);
	if (exceptions && CFGetTypeID(exceptions) != CFArrayGetTypeID()) {
		CFRelease(exceptions);
		exceptions = NULL;
	}

	OSStatus __secapiresult = errSecSuccess;
	try {
		Trust::required(trust)->exceptions(exceptions);
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	/* If there is a valid exception entry for our current leaf we're golden. */
	if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
		return true;

	/* The passed in exceptions didn't match our current leaf, so we discard it. */
	try {
		Trust::required(trust)->exceptions(NULL);
		__secapiresult = errSecSuccess;
	}
	catch (const MacOSError &err) { __secapiresult=err.osStatus(); }
	catch (const CommonError &err) { __secapiresult=SecKeychainErrFromOSStatus(err.osStatus()); }
	catch (const std::bad_alloc &) { __secapiresult=errSecAllocate; }
	catch (...) { __secapiresult=errSecInternalComponent; }

	return false;
}
#endif

#if !SECTRUST_OSX
/* new in 10.9 */
CFDictionaryRef
SecTrustCopyResult(SecTrustRef trust)
{
	CFDictionaryRef result = NULL;
	try {
		result = Trust::required(trust)->results();
		// merge details into result
		CFArrayRef details = SecTrustCopyDetails(trust);
		if (details) {
			CFDictionarySetValue((CFMutableDictionaryRef)result,
				kSecTrustResultDetails, details);
			CFRelease(details);
		}
	}
	catch (...) {
		if (result) {
			CFRelease(result);
			result = NULL;
		}
	}
	return result;
}
#endif

#if !SECTRUST_OSX
/* new in 10.7 */
CFArrayRef
SecTrustCopyProperties(SecTrustRef trust)
{
	/* can't use SECAPI macros, since this function does not return OSStatus */
	CFArrayRef result = NULL;
	try {
		result = Trust::required(trust)->properties();
	}
	catch (...) {
		if (result) {
			CFRelease(result);
			result = NULL;
		}
	}
	return result;
}
#endif

/* deprecated in 10.5 */
OSStatus SecTrustGetCSSMAnchorCertificates(const CSSM_DATA **cssmAnchors,
	uint32 *cssmAnchorCount)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	CertGroup certs;
	Trust::gStore().getCssmRootCertificates(certs);
	Required(cssmAnchors) = certs.blobCerts();
	Required(cssmAnchorCount) = certs.count();
	END_SECAPI
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetCSSMAnchorCertificates has been deprecated since 10.5, and cannot return CSSM objects in 10.11. Please stop using it.");
#endif
	if (cssmAnchors) {
		*cssmAnchors = NULL;
	}
	if (cssmAnchorCount) {
		*cssmAnchorCount = 0;
	}
	return errSecServiceNotAvailable;
#endif
}


//
// Get and set user trust settings. Deprecated in 10.5.
// User Trust getter, deprecated, works as it always has.
//
OSStatus SecTrustGetUserTrust(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting *trustSetting)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	StorageManager::KeychainList searchList;
	globals().storageManager.getSearchList(searchList);
	Required(trustSetting) = Trust::gStore().find(
		Certificate::required(certificate),
		Policy::required(policy),
		searchList);
	END_SECAPI
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetUserTrust has been deprecated since 10.5, and does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
#endif
}

//
// The public setter, also deprecated; it maps to the appropriate
// Trust Settings call if possible, else throws errSecUnimplemented.
//
OSStatus SecTrustSetUserTrust(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting trustSetting)
{
#if !SECTRUST_OSX
	SecTrustSettingsResult tsResult = kSecTrustSettingsResultInvalid;
	OSStatus ortn;
	Boolean isRoot;

	Policy::required(policy);
	switch(trustSetting) {
		case kSecTrustResultProceed:
			/* different SecTrustSettingsResult depending in root-ness */
			ortn = SecCertificateIsSelfSigned(certificate, &isRoot);
			if(ortn) {
				return ortn;
			}
			if(isRoot) {
				tsResult = kSecTrustSettingsResultTrustRoot;
			}
			else {
				tsResult = kSecTrustSettingsResultTrustAsRoot;
			}
			break;
		case kSecTrustResultDeny:
			tsResult = kSecTrustSettingsResultDeny;
			break;
		default:
			return errSecUnimplemented;
	}

	/* make a usage constraints dictionary */
	CFRef<CFMutableDictionaryRef> usageDict(CFDictionaryCreateMutable(NULL,
		0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
	CFDictionaryAddValue(usageDict, kSecTrustSettingsPolicy, policy);
	if(tsResult != kSecTrustSettingsResultTrustRoot) {
		/* skip if we're specifying the default */
		SInt32 result = tsResult;
		CFNumberRef cfNum = CFNumberCreate(NULL, kCFNumberSInt32Type, &result);
		CFDictionarySetValue(usageDict, kSecTrustSettingsResult, cfNum);
		CFRelease(cfNum);
	}
	return SecTrustSettingsSetTrustSettings(certificate, kSecTrustSettingsDomainUser,
		usageDict);
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetUserTrust has been deprecated since 10.5, and does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
#endif
}

//
// This one is the now-private version of what SecTrustSetUserTrust() used to
// be. The public API can no longer manipulate User Trust settings, only
// view them.
//
OSStatus SecTrustSetUserTrustLegacy(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting trustSetting)
{
#if !SECTRUST_OSX
	BEGIN_SECAPI
	switch (trustSetting) {
    case kSecTrustResultProceed:
    case kSecTrustResultConfirm:
    case kSecTrustResultDeny:
    case kSecTrustResultUnspecified:
		break;
	default:
		MacOSError::throwMe(errSecInvalidTrustSetting);
	}
	Trust::gStore().assign(
		Certificate::required(certificate),
		Policy::required(policy),
		trustSetting);
	END_SECAPI
#else
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetUserTrustLegacy does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
#endif
}
