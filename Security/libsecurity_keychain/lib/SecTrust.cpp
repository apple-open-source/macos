/*
 * Copyright (c) 2002-2010,2012-2013 Apple Inc. All Rights Reserved.
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
#include "SecBridge.h"
#include "SecInternal.h"
#include "SecInternalP.h"
#include "SecTrustSettings.h"
#include "SecCertificatePriv.h"
#include "SecCertificateP.h"
#include "SecCertificatePrivP.h"
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>
#include <CoreFoundation/CoreFoundation.h>

// forward declarations
CFArrayRef SecTrustCopyDetails(SecTrustRef trust);
static CFDictionaryRef SecTrustGetExceptionForCertificateAtIndex(SecTrustRef trust, CFIndex ix);
static void SecTrustCheckException(const void *key, const void *value, void *context);

typedef struct SecTrustCheckExceptionContext {
	CFDictionaryRef exception;
	bool exceptionNotFound;
} SecTrustCheckExceptionContext;

// public trust result constants
CFTypeRef kSecTrustEvaluationDate           = CFSTR("TrustEvaluationDate");
CFTypeRef kSecTrustExtendedValidation       = CFSTR("TrustExtendedValidation");
CFTypeRef kSecTrustOrganizationName         = CFSTR("Organization");
CFTypeRef kSecTrustResultValue              = CFSTR("TrustResultValue");
CFTypeRef kSecTrustRevocationChecked        = CFSTR("TrustRevocationChecked");
CFTypeRef kSecTrustRevocationValidUntilDate = CFSTR("TrustExpirationDate");
CFTypeRef kSecTrustResultDetails            = CFSTR("TrustResultDetails");

//
// CF boilerplate
//
CFTypeID SecTrustGetTypeID(void)
{
	BEGIN_SECAPI

	return gTypes().Trust.typeID;

	END_SECAPI1(_kCFRuntimeNotATypeID)
}


//
// Sec* API bridge functions
//
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

OSStatus
SecTrustSetPolicies(SecTrustRef trustRef, CFTypeRef policies)
{
	BEGIN_SECAPI
	Trust::required(trustRef)->policies(policies);
	END_SECAPI
}

OSStatus
SecTrustSetOptions(SecTrustRef trustRef, SecTrustOptionFlags options)
{
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
}

OSStatus SecTrustSetParameters(
    SecTrustRef trustRef,
    CSSM_TP_ACTION action,
    CFDataRef actionData)
{
    BEGIN_SECAPI
    Trust *trust = Trust::required(trustRef);
    trust->action(action);
    trust->actionData(actionData);
    END_SECAPI
}


OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust, CFArrayRef anchorCertificates)
{
    BEGIN_SECAPI
    Trust::required(trust)->anchors(anchorCertificates);
    END_SECAPI
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust, Boolean anchorCertificatesOnly)
{
    BEGIN_SECAPI
    Trust::AnchorPolicy policy = (anchorCertificatesOnly) ? Trust::useAnchorsOnly : Trust::useAnchorsAndBuiltIns;
    Trust::required(trust)->anchorPolicy(policy);
    END_SECAPI
}

OSStatus SecTrustSetKeychains(SecTrustRef trust, CFTypeRef keychainOrArray)
{
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
}


OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate)
{
    BEGIN_SECAPI
    Trust::required(trust)->time(verifyDate);
    END_SECAPI
}


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

//
// Construct the "official" result evidence and return it
//
OSStatus SecTrustGetResult(
    SecTrustRef trustRef,
    SecTrustResultType *result,
	CFArrayRef *certChain, CSSM_TP_APPLE_EVIDENCE_INFO **statusChain)
{
    BEGIN_SECAPI
    Trust *trust = Trust::required(trustRef);
    if (result)
        *result = trust->result();
    if (certChain && statusChain)
        trust->buildEvidence(*certChain, TPEvidenceInfo::overlayVar(*statusChain));
    END_SECAPI
}

//
// Retrieve result of trust evaluation only
//
OSStatus SecTrustGetTrustResult(SecTrustRef trustRef,
	SecTrustResultType *result)
{
    BEGIN_SECAPI
    Trust *trust = Trust::required(trustRef);
    if (result) *result = trust->result();
    END_SECAPI
}

//
// Retrieve extended validation trust results
//
OSStatus SecTrustCopyExtendedResult(SecTrustRef trust, CFDictionaryRef *result)
{
    BEGIN_SECAPI
	Trust *trustObj = Trust::required(trust);
	if (result == nil)
		return errSecParam;
	trustObj->extendedResult(*result);
    END_SECAPI
}

//
// Retrieve CSSM-level information for those who want to dig down
//
OSStatus SecTrustGetCssmResult(SecTrustRef trust, CSSM_TP_VERIFY_CONTEXT_RESULT_PTR *result)
{
    BEGIN_SECAPI
    Required(result) = Trust::required(trust)->cssmResult();
    END_SECAPI
}

//
// Retrieve CSSM_LEVEL TP return code
//
OSStatus SecTrustGetCssmResultCode(SecTrustRef trustRef, OSStatus *result)
{
	BEGIN_SECAPI
		Trust *trust = Trust::required(trustRef);
	if (trust->result() == kSecTrustResultInvalid)
		return errSecParam;
	else
		Required(result) = trust->cssmResultCode();
	END_SECAPI
}

OSStatus SecTrustGetTPHandle(SecTrustRef trust, CSSM_TP_HANDLE *handle)
{
	BEGIN_SECAPI
		Required(handle) = Trust::required(trust)->getTPHandle();
	END_SECAPI
}

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

OSStatus SecTrustSetNetworkFetchAllowed(SecTrustRef trust, Boolean allowFetch)
{
	BEGIN_SECAPI
	Trust *trustObj = Trust::required(trust);
	Trust::NetworkPolicy netPolicy = (allowFetch) ?
		Trust::useNetworkEnabled : Trust::useNetworkDisabled;
	trustObj->networkPolicy(netPolicy);
	END_SECAPI
}

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

OSStatus SecTrustSetOCSPResponse(SecTrustRef trust, CFTypeRef responseData)
{
	BEGIN_SECAPI
	Trust::required(trust)->responses(responseData);
	END_SECAPI
}

OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust, CFArrayRef *anchorCertificates)
{
	BEGIN_SECAPI
	CFArrayRef customAnchors = Trust::required(trust)->anchors();
	Required(anchorCertificates) = (customAnchors) ?
		(const CFArrayRef)CFRetain(customAnchors) : (const CFArrayRef)NULL;
	END_SECAPI
}

//
// Get the user's default anchor certificate set
//
OSStatus SecTrustCopyAnchorCertificates(CFArrayRef *anchorCertificates)
{
	BEGIN_SECAPI

	return SecTrustSettingsCopyUnrestrictedRoots(
			true, true, true,		/* all domains */
			anchorCertificates);

	END_SECAPI
}

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


/* deprecated in 10.5 */
OSStatus SecTrustGetCSSMAnchorCertificates(const CSSM_DATA **cssmAnchors,
	uint32 *cssmAnchorCount)
{
	BEGIN_SECAPI
	CertGroup certs;
	Trust::gStore().getCssmRootCertificates(certs);
	Required(cssmAnchors) = certs.blobCerts();
	Required(cssmAnchorCount) = certs.count();
	END_SECAPI
}


//
// Get and set user trust settings. Deprecated in 10.5.
// User Trust getter, deprecated, works as it always has.
//
OSStatus SecTrustGetUserTrust(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting *trustSetting)
{
	BEGIN_SECAPI
	StorageManager::KeychainList searchList;
	globals().storageManager.getSearchList(searchList);
	Required(trustSetting) = Trust::gStore().find(
		Certificate::required(certificate),
		Policy::required(policy),
		searchList);
	END_SECAPI
}

//
// The public setter, also deprecated; it maps to the appropriate
// Trust Settings call if possible, else throws errSecUnimplemented.
//
OSStatus SecTrustSetUserTrust(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting trustSetting)
{
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
}

//
// This one is the now-private version of what SecTrustSetUserTrust() used to
// be. The public API can no longer manipulate User Trust settings, only
// view them.
//
OSStatus SecTrustSetUserTrustLegacy(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting trustSetting)
{
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
}

/*   SecGetAppleTPHandle - @@@NOT EXPORTED YET; copied from SecurityInterface,
                           but could be useful in the future.
*/
/*
CSSM_TP_HANDLE
SecGetAppleTPHandle()
{
	BEGIN_SECAPI
	return TP(gGuidAppleX509TP)->handle();
	END_SECAPI1(NULL);
}
*/


