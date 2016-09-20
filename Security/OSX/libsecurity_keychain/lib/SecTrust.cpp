/*
 * Copyright (c) 2002-2016 Apple Inc. All Rights Reserved.
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
#include "SecBase.h"
#include "SecBridge.h"
#include "SecInternal.h"
#include "SecInternalP.h"
#include "SecTrustSettings.h"
#include "SecTrustSettingsPriv.h"
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
#else
CFArrayRef SecTrustCopyInputCertificates(SecTrustRef trust);
CFArrayRef SecTrustCopyInputAnchors(SecTrustRef trust);
CFArrayRef SecTrustCopyConstructedChain(SecTrustRef trust);
static CSSM_TP_APPLE_EVIDENCE_INFO * SecTrustGetEvidenceInfo(SecTrustRef trust);
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

// Policy check string to CSSM_RETURN mapping

struct resultmap_entry_s {
	const CFStringRef checkstr;
	const CSSM_RETURN resultcode;
};
typedef struct resultmap_entry_s resultmap_entry_t;

#if SECTRUST_OSX
const resultmap_entry_t cssmresultmap[] = {
    { CFSTR("SSLHostname"), CSSMERR_APPLETP_HOSTNAME_MISMATCH },
    { CFSTR("email"), CSSMERR_APPLETP_SMIME_EMAIL_ADDRS_NOT_FOUND },
    { CFSTR("IssuerCommonName"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("SubjectCommonName"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("SubjectCommonNamePrefix"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("SubjectCommonNameTEST"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("SubjectOrganization"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("SubjectOrganizationalUnit"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("EAPTrustedServerNames"), CSSMERR_APPLETP_HOSTNAME_MISMATCH },
    { CFSTR("CertificatePolicy"), CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION },
    { CFSTR("KeyUsage"), CSSMERR_APPLETP_INVALID_KEY_USAGE },
    { CFSTR("ExtendedKeyUsage"), CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE },
    { CFSTR("BasicConstraints"), CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS },
    { CFSTR("QualifiedCertStatements"), CSSMERR_APPLETP_UNKNOWN_QUAL_CERT_STATEMENT },
    { CFSTR("IntermediateSPKISHA256"), CSSMERR_APPLETP_IDENTIFIER_MISSING },
    { CFSTR("IntermediateEKU"), CSSMERR_APPLETP_INVALID_EXTENDED_KEY_USAGE },
    { CFSTR("AnchorSHA1"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("AnchorSHA256"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("AnchorTrusted"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("AnchorApple"), CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH },
    { CFSTR("NonEmptySubject"), CSSMERR_APPLETP_INVALID_EMPTY_SUBJECT },
    { CFSTR("IdLinkage"), CSSMERR_APPLETP_INVALID_AUTHORITY_ID },
    { CFSTR("WeakIntermediates"), CSSMERR_TP_INVALID_CERTIFICATE },
    { CFSTR("WeakLeaf"), CSSMERR_TP_INVALID_CERTIFICATE },
    { CFSTR("WeakRoot"), CSSMERR_TP_INVALID_CERTIFICATE },
    { CFSTR("KeySize"), CSSMERR_CSP_UNSUPPORTED_KEY_SIZE },
    { CFSTR("SignatureHashAlgorithms"), CSSMERR_CSP_ALGID_MISMATCH },
    { CFSTR("CriticalExtensions"), CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN },
    { CFSTR("ChainLength"), CSSMERR_APPLETP_PATH_LEN_CONSTRAINT },
    { CFSTR("BasicCertificateProcessing"), CSSMERR_TP_INVALID_CERTIFICATE },
    { CFSTR("ExtendedValidation"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("Revocation"), CSSMERR_TP_CERT_REVOKED },
    { CFSTR("RevocationResponseRequired"), CSSMERR_TP_VERIFY_ACTION_FAILED },
    { CFSTR("CertificateTransparency"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("BlackListedLeaf"), CSSMERR_TP_CERT_REVOKED },
    { CFSTR("GrayListedLeaf"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("GrayListedKey"), CSSMERR_TP_NOT_TRUSTED },
    { CFSTR("BlackListedKey"), CSSMERR_TP_CERT_REVOKED },
    { CFSTR("CheckLeafMarkerOid"), CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION },
    { CFSTR("CheckLeafMarkerOidNoValueCheck"), CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION },
    { CFSTR("CheckIntermediateMarkerOid"), CSSMERR_APPLETP_MISSING_REQUIRED_EXTENSION },
    { CFSTR("UsageConstraints"), CSSMERR_APPLETP_TRUST_SETTING_DENY },
    { CFSTR("NotValidBefore"), CSSMERR_TP_CERT_NOT_VALID_YET },
    { CFSTR("ValidIntermediates"), CSSMERR_TP_CERT_EXPIRED },
    { CFSTR("ValidLeaf"), CSSMERR_TP_CERT_EXPIRED },
    { CFSTR("ValidRoot"), CSSMERR_TP_CERT_EXPIRED },
//  { CFSTR("AnchorAppleTestRoots"),  },
//  { CFSTR("AnchorAppleTestRootsOnProduction"),  },
//  { CFSTR("NoNetworkAccess"),  },
};
#endif

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

#if SECTRUST_OSX
typedef struct {
	SecTrustOptionFlags flags;
	CFIndex certIX;
	SecTrustRef trustRef;
	CFMutableDictionaryRef filteredException;
	CFDictionaryRef oldException;
} SecExceptionFilterContext;

#if 0
//%%%FIXME SecCFWrappers produces some conflicting definitions on OSX
#include <utilities/SecCFWrappers.h>
#else
// inline function from SecCFWrappers.h
static inline char *CFStringToCString(CFStringRef inStr)
{
    if (!inStr)
        return (char *)strdup("");
    CFRetain(inStr);        // compensate for release on exit

    // need to extract into buffer
    CFIndex length = CFStringGetLength(inStr);  // in 16-bit character units
    size_t len = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingUTF8);
    char *buffer = (char *)malloc(len);                 // pessimistic
    if (!CFStringGetCString(inStr, buffer, len, kCFStringEncodingUTF8))
        buffer[0] = 0;

    CFRelease(inStr);
    return buffer;
}
#endif

static void
filter_exception(const void *key, const void *value, void *context)
{
	SecExceptionFilterContext *ctx = (SecExceptionFilterContext *)context;
	if (!ctx) { return; }

	SecTrustOptionFlags options = ctx->flags;
	CFMutableDictionaryRef filteredException = ctx->filteredException;
	CFStringRef keystr = (CFStringRef)key;

	if (ctx->oldException && CFDictionaryContainsKey(ctx->oldException, key)) {
		// Keep existing exception in filtered dictionary, regardless of options
		CFDictionaryAddValue(filteredException, key, CFDictionaryGetValue(ctx->oldException, key));
		return;
	}

	bool allowed = false;

	if (CFEqual(keystr, CFSTR("SHA1Digest"))) {
		allowed = true; // this key is informational and always permitted
	}
	else if (CFEqual(keystr, CFSTR("NotValidBefore"))) {
		allowed = ((options & kSecTrustOptionAllowExpired) != 0);
	}
	else if (CFEqual(keystr, CFSTR("ValidLeaf"))) {
		allowed = ((options & kSecTrustOptionAllowExpired) != 0);
	}
	else if (CFEqual(keystr, CFSTR("ValidIntermediates"))) {
		allowed = ((options & kSecTrustOptionAllowExpired) != 0);
	}
	else if (CFEqual(keystr, CFSTR("ValidRoot"))) {
        if (((options & kSecTrustOptionAllowExpired) != 0) ||
            ((options & kSecTrustOptionAllowExpiredRoot) != 0)) {
            allowed = true;
        }
	}
	else if (CFEqual(keystr, CFSTR("AnchorTrusted"))) {
		bool implicitAnchors = ((options & kSecTrustOptionImplicitAnchors) != 0);
		// Implicit anchors option only filters exceptions for self-signed certs
		if (implicitAnchors && ctx->trustRef &&
		    (ctx->certIX < SecTrustGetCertificateCount(ctx->trustRef))) {
			Boolean isSelfSigned = false;
			SecCertificateRef cert = SecTrustGetCertificateAtIndex(ctx->trustRef, ctx->certIX);
			if (cert && (errSecSuccess == SecCertificateIsSelfSigned(cert, &isSelfSigned)) &&
			    isSelfSigned) {
				allowed = true;
			}
		}
	}
	else if (CFEqual(keystr, CFSTR("KeyUsage")) ||
	         CFEqual(keystr, CFSTR("ExtendedKeyUsage")) ||
	         CFEqual(keystr, CFSTR("BasicConstraints")) ||
	         CFEqual(keystr, CFSTR("NonEmptySubject")) ||
	         CFEqual(keystr, CFSTR("IdLinkage"))) {
		// Cannot override these exceptions
		allowed = false;
	}
	else {
		// Unhandled exceptions should not be overridden,
		// but we want to know which ones we're missing
		char *cstr = CFStringToCString(keystr);
		syslog(LOG_ERR, "Unfiltered exception: %s", (cstr) ? cstr : "<NULL>");
		if (cstr) { free(cstr); }
		allowed = false;
	}

	if (allowed) {
		CFDictionaryAddValue(filteredException, key, value);
	}
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
	CFDataRef encodedExceptions = SecTrustCopyExceptions(trustRef);
	CFArrayRef exceptions = NULL,
            oldExceptions = SecTrustGetTrustExceptionsArray(trustRef);

	if (encodedExceptions) {
		exceptions = (CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
			encodedExceptions, kCFPropertyListImmutable, NULL, NULL);
		CFRelease(encodedExceptions);
		encodedExceptions = NULL;
	}

	if (exceptions && CFGetTypeID(exceptions) != CFArrayGetTypeID()) {
		CFRelease(exceptions);
		exceptions = NULL;
	}

	if (oldExceptions && exceptions &&
		CFArrayGetCount(oldExceptions) > CFArrayGetCount(exceptions)) {
		oldExceptions = NULL;
	}

	/* verify both exceptions are for the same leaf */
	if (oldExceptions && exceptions && CFArrayGetCount(oldExceptions) > 0) {
		CFDictionaryRef oldLeafExceptions = (CFDictionaryRef)CFArrayGetValueAtIndex(oldExceptions, 0);
		CFDictionaryRef leafExceptions = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, 0);
		CFDataRef oldDigest = (CFDataRef)CFDictionaryGetValue(oldLeafExceptions, CFSTR("SHA1Digest"));
		CFDataRef digest = (CFDataRef)CFDictionaryGetValue(leafExceptions, CFSTR("SHA1Digest"));
		if (!oldDigest || !digest || !CFEqual(oldDigest, digest)) {
			oldExceptions = NULL;
		}
	}

	/* add only those exceptions which are allowed by the supplied options */
	if (exceptions) {
		CFMutableArrayRef filteredExceptions = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
		CFIndex i, exceptionCount = (filteredExceptions) ? CFArrayGetCount(exceptions) : 0;

		for (i = 0; i < exceptionCount; ++i) {
			CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, i);
			CFDictionaryRef oldException = NULL;
			if (oldExceptions && i < CFArrayGetCount(oldExceptions)) {
				oldException = (CFDictionaryRef)CFArrayGetValueAtIndex(oldExceptions, i);
			}
			CFMutableDictionaryRef filteredException = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
																				 &kCFTypeDictionaryValueCallBacks);
			if (exception && filteredException) {
				SecExceptionFilterContext filterContext = { options, i, trustRef, filteredException, oldException };
				CFDictionaryApplyFunction(exception, filter_exception, &filterContext);
				CFArrayAppendValue(filteredExceptions, filteredException);
				CFRelease(filteredException);
			}
		}

		if (filteredExceptions) {
			CFIndex filteredCount = CFArrayGetCount(filteredExceptions);
			/* remove empty trailing entries to match iOS behavior */
			for (i = filteredCount; i-- > 1;) {
				CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(filteredExceptions, i);
				if (CFDictionaryGetCount(exception) == 0) {
					CFArrayRemoveValueAtIndex(filteredExceptions, i);
				} else {
					break;
				}
			}
			encodedExceptions = CFPropertyListCreateData(kCFAllocatorDefault,
				filteredExceptions, kCFPropertyListBinaryFormat_v1_0, 0, NULL);
			CFRelease(filteredExceptions);

			SecTrustSetExceptions(trustRef, encodedExceptions);
			CFRelease(encodedExceptions);
		}
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


	secnotice("SecTrustEvaluate", "SecTrustEvaluate trust result = %d", (int)trustResult);
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
	syslog(LOG_ERR, "WARNING: SecTrustGetResult has been deprecated since 10.7. Please use SecTrustGetTrustResult instead.");
#endif
    SecTrustResultType trustResult;
    OSStatus status = SecTrustGetTrustResult(trustRef, &trustResult);
    if (status != errSecSuccess) {
        return status;
    }
	if (result) {
		*result = trustResult;
	}
	if (certChain) {
        *certChain = SecTrustCopyConstructedChain(trustRef);
	}
	if (statusChain) {
        *statusChain = SecTrustGetEvidenceInfo(trustRef);
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
//
// Returns a malloced array of CSSM_RETURN values, with the length in numStatusCodes,
// for the certificate specified by chain index in the given SecTrustRef.
//
// To match legacy behavior, the array actually allocates one element more than the
// value of numStatusCodes; if the certificate is revoked, the additional element
// at the end contains the CrlReason value.
//
// Caller must free the returned pointer.
//
static CSSM_RETURN *copyCssmStatusCodes(SecTrustRef trust,
	unsigned int index, unsigned int *numStatusCodes)
{
	if (!trust || !numStatusCodes) {
		return NULL;
	}
	*numStatusCodes = 0;
	CFArrayRef details = SecTrustGetDetails(trust);
	CFIndex chainLength = (details) ? CFArrayGetCount(details) : 0;
	if (!(index < chainLength)) {
		return NULL;
	}
	CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, index);
	CFIndex ix, detailCount = CFDictionaryGetCount(detail);
	*numStatusCodes = (unsigned int)detailCount;

	// Allocate one more entry than we need; this is used to store a CrlReason
	// at the end of the array.
	CSSM_RETURN *statusCodes = (CSSM_RETURN*)malloc((detailCount+1) * sizeof(CSSM_RETURN));
	statusCodes[*numStatusCodes] = 0;

	const unsigned int resultmaplen = sizeof(cssmresultmap) / sizeof(resultmap_entry_t);
	const void *keys[detailCount];
	CFDictionaryGetKeysAndValues(detail, &keys[0], NULL);
	for (ix = 0; ix < detailCount; ix++) {
		CFStringRef key = (CFStringRef)keys[ix];
		CSSM_RETURN statusCode = CSSM_OK;
		for (unsigned int mapix = 0; mapix < resultmaplen; mapix++) {
			CFStringRef str = (CFStringRef) cssmresultmap[mapix].checkstr;
			if (CFStringCompare(str, key, 0) == kCFCompareEqualTo) {
				statusCode = (CSSM_RETURN) cssmresultmap[mapix].resultcode;
				break;
			}
		}
		if (statusCode == CSSMERR_TP_CERT_REVOKED) {
			SInt32 reason;
			CFNumberRef number = (CFNumberRef)CFDictionaryGetValue(detail, key);
			if (number && CFNumberGetValue(number, kCFNumberSInt32Type, &reason)) {
				statusCodes[*numStatusCodes] = (CSSM_RETURN)reason;
			}
		}
		statusCodes[ix] = statusCode;
	}

	return statusCodes;
}

static uint8_t convertCssmResultToPriority(CSSM_RETURN resultCode) {
    switch (resultCode) {
        /* explicitly not trusted */
        case CSSMERR_TP_CERT_REVOKED:
        case CSSMERR_APPLETP_TRUST_SETTING_DENY:
            return 1;
        /* failure to comply with X.509 */
        case CSSMERR_APPLETP_NO_BASIC_CONSTRAINTS:
        case CSSMERR_APPLETP_UNKNOWN_QUAL_CERT_STATEMENT:
        case CSSMERR_APPLETP_INVALID_EMPTY_SUBJECT:
        case CSSMERR_APPLETP_INVALID_AUTHORITY_ID:
        case CSSMERR_TP_INVALID_CERTIFICATE:
        case CSSMERR_APPLETP_UNKNOWN_CRITICAL_EXTEN:
            return 2;
        case CSSMERR_TP_CERT_EXPIRED:
            return 3;
        /* doesn't chain to trusted root */
        case CSSMERR_TP_NOT_TRUSTED:
        case CSSMERR_APPLETP_CS_BAD_CERT_CHAIN_LENGTH:
            return 4;
        /* all others are policy-specific failures */
        default:
            return 5;
    }
}

#include <libDER/oidsPriv.h>
#include <Security/oidscert.h>
static bool isSoftwareUpdateDevelopment(SecTrustRef trust) {
    bool isPolicy = false, isEKU = false;
    CFArrayRef policies = NULL;

    /* Policy used to evaluate was SWUpdateSigning */
    SecTrustCopyPolicies(trust, &policies);
    if (policies) {
        SecPolicyRef swUpdatePolicy = SecPolicyCreateAppleSWUpdateSigning();
        if (swUpdatePolicy && CFArrayContainsValue(policies, CFRangeMake(0, CFArrayGetCount(policies)),
                                                   swUpdatePolicy)) {
            isPolicy = true;
        }
        if (swUpdatePolicy) { CFRelease(swUpdatePolicy); }
        CFRelease(policies);
    }
    if (!isPolicy) {
        return false;
    }

    /* Only error was EKU on the leaf */
    CFArrayRef details = SecTrustGetDetails(trust);
    CFIndex ix, count = CFArrayGetCount(details);
    for (ix = 0; ix < count; ix++) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        if (ix == 0) { // Leaf
            if (CFDictionaryGetCount(detail) != 1 || // One error
                CFDictionaryGetValue(detail, CFSTR("ExtendedKeyUsage")) != kCFBooleanFalse) // kSecPolicyCheckExtendedKeyUsage
                return false;
        } else {
            if (CFDictionaryGetCount(detail) > 0) { // No errors on other certs
                return false;
            }
        }
    }

    /* EKU on the leaf is the Apple Development Code Signing OID */
    SecCertificateRef leaf = SecTrustGetCertificateAtIndex(trust, 0);
    CSSM_DATA *fieldValue = NULL;
    if (errSecSuccess != SecCertificateCopyFirstFieldValue(leaf, &CSSMOID_ExtendedKeyUsage, &fieldValue)) {
        return false;
    }
    if (fieldValue && fieldValue->Data && fieldValue->Length == sizeof(CSSM_X509_EXTENSION)) {
        const CSSM_X509_EXTENSION *ext = (const CSSM_X509_EXTENSION *)fieldValue->Data;
        if (ext->format == CSSM_X509_DATAFORMAT_PARSED) {
            const CE_ExtendedKeyUsage *ekus = (const CE_ExtendedKeyUsage *)ext->value.parsedValue;
            if (ekus && (ekus->numPurposes == 1) && ekus->purposes[0].Data &&
                (ekus->purposes[0].Length == CSSMOID_APPLE_EKU_CODE_SIGNING_DEV.Length) &&
                (memcmp(ekus->purposes[0].Data, CSSMOID_APPLE_EKU_CODE_SIGNING_DEV.Data,
                        ekus->purposes[0].Length) == 0)) {
                isEKU = true;
            }
        }
    }
    SecCertificateReleaseFirstFieldValue(leaf, &CSSMOID_ExtendedKeyUsage, fieldValue);
    return isEKU;
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

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    (void) SecTrustGetTrustResult(trustRef, &trustResult);
    if (trustResult == kSecTrustResultProceed || trustResult == kSecTrustResultUnspecified) {
        if (result) { *result = 0; }
        return errSecSuccess;
    }

    /* Development Software Update certs return a special error code when evaluated
     * against the AppleSWUpdateSigning policy. See <rdar://27362805>. */
    if (isSoftwareUpdateDevelopment(trustRef)) {
        if (result) {
            *result = CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT;
        }
        return errSecSuccess;
    }

    OSStatus cssmResultCode = errSecSuccess;
    uint8_t resultCodePriority = 0xFF;
    CFIndex ix, count = SecTrustGetCertificateCount(trustRef);
    for (ix = 0; ix < count; ix++) {
        unsigned int numStatusCodes;
        CSSM_RETURN *statusCodes = NULL;
        statusCodes = copyCssmStatusCodes(trustRef, (uint32_t)ix, &numStatusCodes);
        if (statusCodes && numStatusCodes > 0) {
            unsigned int statusIX;
            for (statusIX = 0; statusIX < numStatusCodes; statusIX++) {
                CSSM_RETURN currStatus = statusCodes[statusIX];
                uint8_t currPriotiy = convertCssmResultToPriority(currStatus);
                if (resultCodePriority > currPriotiy) {
                    cssmResultCode = currStatus;
                    resultCodePriority = currPriotiy;
                }
            }
        }
        if (statusCodes) { free(statusCodes); }
        if (resultCodePriority == 1) { break; }
    }

	if (result) {
		*result = cssmResultCode;
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

// cannot link against the new iOS SecTrust from this implementation,
// so there are no possible accessors for the fields of this struct
typedef struct __TSecTrust {
    CFRuntimeBase           _base;
    CFArrayRef              _certificates;
    CFArrayRef              _anchors;
    CFTypeRef               _policies;
    CFArrayRef              _responses;
    CFArrayRef              _SCTs;
    CFArrayRef              _trustedLogs;
    CFDateRef               _verifyDate;
    CFTypeRef               _chain;
    SecKeyRef               _publicKey;
    CFArrayRef              _details;
    CFDictionaryRef         _info;
    CFArrayRef              _exceptions;
    SecTrustResultType      _trustResult;
    bool                    _anchorsOnly;
    bool                    _keychainsAllowed;
    void*                   _legacy_info_array;
    void*                   _legacy_status_array;
    SecTrustResultType      _trustResultBeforeExceptions;
} TSecTrust;

#if SECTRUST_OSX
CFArrayRef SecTrustCopyInputCertificates(SecTrustRef trust)
{
	if (!trust) { return NULL; };
	TSecTrust *secTrust = (TSecTrust *)trust;
	if (secTrust->_certificates) {
		CFRetain(secTrust->_certificates);
	}
	return secTrust->_certificates;
}
#endif

#if SECTRUST_OSX
CFArrayRef SecTrustCopyInputAnchors(SecTrustRef trust)
{
	if (!trust) { return NULL; };
	TSecTrust *secTrust = (TSecTrust *)trust;
	if (secTrust->_anchors) {
		CFRetain(secTrust->_anchors);
	}
	return secTrust->_anchors;
}
#endif

#if SECTRUST_OSX
// Return the constructed certificate chain for this trust reference,
// making output certificates pointer-equivalent to any provided input
// certificates (where possible) for legacy behavioral compatibility.
// Caller must release this array.
//
CFArrayRef SecTrustCopyConstructedChain(SecTrustRef trust)
{
	CFMutableArrayRef certChain = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	CFIndex idx, count = SecTrustGetCertificateCount(trust);
	for (idx=0; idx < count; idx++) {
		SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, idx);
		if (certificate) {
			CFArrayAppendValue(certChain, certificate);
		}
	}
	// <rdar://24393060>
	// Some callers make the assumption that the certificates in
	// this chain are pointer-equivalent to ones they passed to the
	// SecTrustCreateWithCertificates function. We'll maintain that
	// behavior here for compatibility.
	//
	CFArrayRef inputCertArray = SecTrustCopyInputCertificates(trust);
	CFArrayRef inputAnchorArray = SecTrustCopyInputAnchors(trust);
	CFIndex inputCertIdx, inputCertCount = (inputCertArray) ? CFArrayGetCount(inputCertArray) : 0;
	CFIndex inputAnchorIdx, inputAnchorCount = (inputAnchorArray) ? CFArrayGetCount(inputAnchorArray) : 0;
	for (idx=0; idx < count; idx++) {
		SecCertificateRef tmpCert = (SecCertificateRef) CFArrayGetValueAtIndex(certChain, idx);
		if (tmpCert) {
			SecCertificateRef matchCert = NULL;
			for (inputCertIdx=0; inputCertIdx < inputCertCount && !matchCert; inputCertIdx++) {
				SecCertificateRef inputCert = (SecCertificateRef) CFArrayGetValueAtIndex(inputCertArray, inputCertIdx);
				if (inputCert && CFEqual(inputCert, tmpCert)) {
					matchCert = inputCert;
				}
			}
			for (inputAnchorIdx=0; inputAnchorIdx < inputAnchorCount && !matchCert; inputAnchorIdx++) {
				SecCertificateRef inputAnchor = (SecCertificateRef) CFArrayGetValueAtIndex(inputAnchorArray, inputAnchorIdx);
				if (inputAnchor && CFEqual(inputAnchor, tmpCert)) {
					matchCert = inputAnchor;
				}
			}
			if (matchCert) {
				CFArraySetValueAtIndex(certChain, idx, matchCert);
			}
		}
	}
	if (inputCertArray) {
		CFRelease(inputCertArray);
	}
	if (inputAnchorArray) {
		CFRelease(inputAnchorArray);
	}
	return certChain;
}
#endif

#if SECTRUST_OSX
//
// Here is where backward compatibility gets ugly. CSSM_TP_APPLE_EVIDENCE_INFO does not exist
// in the unified SecTrust world. Unfortunately, some clients are still calling legacy APIs
// (e.g. SecTrustGetResult) and grubbing through the info for StatusBits and StatusCodes.
// SecTrustGetEvidenceInfo builds the legacy evidence info structure as needed, and returns
// a pointer to it. The evidence data is allocated here and set in the _legacy_* fields
// of the TSecTrust; the trust object subsequently owns it. The returned pointer is expected
// to be valid for the lifetime of the SecTrustRef, or until the trust parameters are changed,
// which would force re-evaluation.
//
static CSSM_TP_APPLE_EVIDENCE_INFO *
SecTrustGetEvidenceInfo(SecTrustRef trust)
{
	TSecTrust *secTrust = (TSecTrust *)trust;
	if (!secTrust) {
		return NULL;
	}
	if (secTrust->_trustResult != kSecTrustSettingsResultInvalid &&
		secTrust->_legacy_info_array) {
		// we've already got valid evidence info, return it now.
		return (CSSM_TP_APPLE_EVIDENCE_INFO *)secTrust->_legacy_info_array;
	}

	// Getting the count implicitly evaluates the chain if necessary.
	CFIndex idx, count = SecTrustGetCertificateCount(trust);
	CFArrayRef inputCertArray = SecTrustCopyInputCertificates(trust);
	CFArrayRef inputAnchorArray = SecTrustCopyInputAnchors(trust);
	CFIndex inputCertIdx, inputCertCount = (inputCertArray) ? CFArrayGetCount(inputCertArray) : 0;
	CFIndex inputAnchorIdx, inputAnchorCount = (inputAnchorArray) ? CFArrayGetCount(inputAnchorArray) : 0;

	CSSM_TP_APPLE_EVIDENCE_INFO *infoArray = (CSSM_TP_APPLE_EVIDENCE_INFO *)calloc(count, sizeof(CSSM_TP_APPLE_EVIDENCE_INFO));
	CSSM_RETURN *statusArray = NULL;
	unsigned int numStatusCodes = 0;

	// Set status codes for each certificate in the constructed chain
	for (idx=0; idx < count; idx++) {
		SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, idx);
		if (!cert) {
			continue;
		}
		CSSM_TP_APPLE_EVIDENCE_INFO *evInfo = &infoArray[idx];

		/* first the booleans (StatusBits flags) */
		CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
		if (secTrust->_verifyDate) {
			now = CFDateGetAbsoluteTime(secTrust->_verifyDate);
		}
		CFAbsoluteTime na = SecCertificateNotValidAfter(cert);
		if (na < now) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_EXPIRED;
		}
		CFAbsoluteTime nb = SecCertificateNotValidBefore(cert);
		if (nb > now) {
			evInfo->StatusBits |= CSSM_CERT_STATUS_NOT_VALID_YET;
		}
		for (inputAnchorIdx=0; inputAnchorIdx < inputAnchorCount; inputAnchorIdx++) {
			SecCertificateRef inputAnchor = (SecCertificateRef) CFArrayGetValueAtIndex(inputAnchorArray, inputAnchorIdx);
			if (inputAnchor && CFEqual(inputAnchor, cert)) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_IN_ANCHORS;
				break;
			}
		}
		for (inputCertIdx=0; inputCertIdx < inputCertCount; inputCertIdx++) {
			SecCertificateRef inputCert = (SecCertificateRef) CFArrayGetValueAtIndex(inputCertArray, inputCertIdx);
			if (inputCert && CFEqual(inputCert, cert)) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_IN_INPUT_CERTS;
				break;
			}
		}

		/* See if there are trust settings for this certificate. */
		CFStringRef hashStr = SecTrustSettingsCertHashStrFromCert(cert);
		bool foundMatch = false;
		bool foundAny = false;
		CSSM_RETURN *errors = NULL;
		uint32 errorCount = 0;
		OSStatus status = 0;
		SecTrustSettingsDomain foundDomain = 0;
		SecTrustSettingsResult foundResult = kSecTrustSettingsResultInvalid;
		bool isSelfSigned = false;
		if ((count - 1) == idx) {
			// Only the last cert in the chain needs to be considered
			Boolean selfSigned;
			status = SecCertificateIsSelfSigned(cert, &selfSigned);
			isSelfSigned = (status) ? false : ((selfSigned) ? true : false);
			if (isSelfSigned) {
				evInfo->StatusBits |= CSSM_CERT_STATUS_IS_ROOT;
			}
		}
		// STU: rdar://25554967
		// %%% need to get policyOID, policyString, and keyUsage here!

		status = SecTrustSettingsEvaluateCert(
				hashStr,		/* certHashStr */
				NULL,			/* policyOID (optional) */
				NULL,			/* policyString (optional) */
				0,				/* policyStringLen */
				0,				/* keyUsage */
				isSelfSigned,	/* isRootCert */
				&foundDomain,	/* foundDomain */
				&errors,		/* allowedErrors -- MUST FREE */
				&errorCount,	/* numAllowedErrors */
				&foundResult,	/* resultType */
				&foundMatch,	/* foundMatchingEntry */
				&foundAny);		/* foundAnyEntry */

		if (status == errSecSuccess) {
			if (foundMatch) {
				switch (foundResult) {
					case kSecTrustSettingsResultTrustRoot:
					case kSecTrustSettingsResultTrustAsRoot:
						/* these two can be disambiguated by IS_ROOT */
						evInfo->StatusBits |= CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST;
						break;
					case kSecTrustSettingsResultDeny:
						evInfo->StatusBits |= CSSM_CERT_STATUS_TRUST_SETTINGS_DENY;
						break;
					case kSecTrustSettingsResultUnspecified:
					case kSecTrustSettingsResultInvalid:
					default:
						break;
				}
			}
		}
		if (errors) {
			free(errors);
		}
		if (hashStr) {
			CFRelease(hashStr);
		}

		unsigned int numCodes=0;
		CSSM_RETURN *statusCodes = copyCssmStatusCodes(trust, (unsigned int)idx, &numCodes);
		if (statusCodes) {
			// Realloc space for these status codes at end of our status codes block.
			// Two important things to note:
			// 1. the actual length is numCodes+1 because copyCssmStatusCodes
			// allocates one more element at the end for the CrlReason value.
			// 2. realloc may cause the pointer to move, which means we will
			// need to fix up the StatusCodes fields after we're done with this loop.
			unsigned int totalStatusCodes = numStatusCodes + numCodes + 1;
			statusArray = (CSSM_RETURN *)realloc(statusArray, totalStatusCodes * sizeof(CSSM_RETURN));
			evInfo->StatusCodes = &statusArray[numStatusCodes];
			evInfo->NumStatusCodes = numCodes;
			// Copy the new codes (plus one) into place
			for (unsigned int cpix = 0; cpix <= numCodes; cpix++) {
				evInfo->StatusCodes[cpix] = statusCodes[cpix];
			}
			numStatusCodes = totalStatusCodes;
			free(statusCodes);
		}

		if(evInfo->StatusBits & (CSSM_CERT_STATUS_TRUST_SETTINGS_TRUST |
								 CSSM_CERT_STATUS_TRUST_SETTINGS_DENY |
								 CSSM_CERT_STATUS_TRUST_SETTINGS_IGNORED_ERROR)) {
			/* Something noteworthy happened involving TrustSettings */
			uint32 whichDomain = 0;
			switch(foundDomain) {
				case kSecTrustSettingsDomainUser:
					whichDomain = CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_USER;
					break;
				case kSecTrustSettingsDomainAdmin:
					whichDomain = CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_ADMIN;
					break;
				case kSecTrustSettingsDomainSystem:
					whichDomain = CSSM_CERT_STATUS_TRUST_SETTINGS_FOUND_SYSTEM;
					break;
			}
			evInfo->StatusBits |= whichDomain;
		}

		/* index into raw cert group or AnchorCerts depending on IS_IN_ANCHORS */
		//evInfo->Index = certInfo->index();
		/* nonzero if cert came from a DLDB */
		//evInfo->DlDbHandle = certInfo->dlDbHandle();
		//evInfo->UniqueRecord = certInfo->uniqueRecord();
	}

	// Now that all the status codes have been allocated in a contiguous block,
	// refresh the StatusCodes pointer in each array element.
	numStatusCodes = 0;
	for (idx=0; idx < count; idx++) {
		CSSM_TP_APPLE_EVIDENCE_INFO *evInfo = &infoArray[idx];
		evInfo->StatusCodes = &statusArray[numStatusCodes];
		numStatusCodes += evInfo->NumStatusCodes + 1;
	}

	secTrust->_legacy_info_array = infoArray;
	secTrust->_legacy_status_array = statusArray;

	if (inputCertArray) {
		CFRelease(inputCertArray);
	}
	if (inputAnchorArray) {
		CFRelease(inputAnchorArray);
	}

	return (CSSM_TP_APPLE_EVIDENCE_INFO *)secTrust->_legacy_info_array;
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
	CFArrayRef exceptions = NULL;

	if (NULL != encodedExceptions) {
		exceptions = (CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
				encodedExceptions, kCFPropertyListImmutable, NULL, NULL);
	}

	if (exceptions && CFGetTypeID(exceptions) != CFArrayGetTypeID()) {
		CFRelease(exceptions);
		exceptions = NULL;
	}

	OSStatus __secapiresult = errSecSuccess;
	try {
		/* Exceptions are being set or cleared, we'll need to re-evaluate trust either way. */
		Trust::required(trust)->setResult(kSecTrustResultInvalid);
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
#else
CFArrayRef SecTrustCopyProperties(SecTrustRef trust) {
    /* OS X creates a completely different structure with one dictionary for each certificate */
    CFIndex ix, count = SecTrustGetCertificateCount(trust);

    CFMutableArrayRef properties = CFArrayCreateMutable(kCFAllocatorDefault, count,
                                                        &kCFTypeArrayCallBacks);

    for (ix = 0; ix < count; ix++) {
        CFMutableDictionaryRef certDict = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                                                    &kCFTypeDictionaryValueCallBacks);
        /* Populate the certificate title */
        SecCertificateRef cert = SecTrustGetCertificateAtIndex(trust, ix);
        if (cert) {
            CFStringRef subjectSummary = SecCertificateCopySubjectSummary(cert);
            if (subjectSummary) {
                CFDictionaryAddValue(certDict, kSecPropertyTypeTitle, subjectSummary);
                CFRelease(subjectSummary);
            }
        }

        /* Populate a revocation reason if the cert was revoked */
        unsigned int numStatusCodes;
        CSSM_RETURN *statusCodes = NULL;
        statusCodes = copyCssmStatusCodes(trust, (uint32_t)ix, &numStatusCodes);
        if (statusCodes) {
            int32_t reason = statusCodes[numStatusCodes];  // stored at end of status codes array
            if (reason > 0) {
                CFNumberRef cfreason = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &reason);
                if (cfreason) {
                    CFDictionarySetValue(certDict, kSecTrustRevocationReason, cfreason);
                    CFRelease(cfreason);
                }
            }
            free(statusCodes);
        }

        /* Populate the error in the leaf dictionary */
        if (ix == 0) {
            OSStatus error = errSecSuccess;
            (void)SecTrustGetCssmResultCode(trust, &error);
            CFStringRef errorStr = SecCopyErrorMessageString(error, NULL);
            if (errorStr) {
                CFDictionarySetValue(certDict, kSecPropertyTypeError, errorStr);
                CFRelease(errorStr);
            }
        }

        CFArrayAppendValue(properties, certDict);
    }

    return properties;
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
