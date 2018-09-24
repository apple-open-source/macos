/*
 * Copyright (c) 2002-2017 Apple Inc. All Rights Reserved.
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

#include <libDER/oids.h>
#include <Security/oidscert.h>

#include "SecTrust.h"
#include "SecTrustPriv.h"
#include "Trust.h"
#include "SecBase.h"
#include "SecBridge.h"
#include "SecInternal.h"
#include "SecTrustSettings.h"
#include "SecTrustSettingsPriv.h"
#include "SecTrustStatusCodes.h"
#include "SecCertificatePriv.h"
#include "SecPolicyPriv.h"
#include <security_utilities/cfutilities.h>
#include <security_utilities/cfmunge.h>
#include <CoreFoundation/CoreFoundation.h>
#include <syslog.h>

// forward declarations
CFArrayRef SecTrustCopyInputCertificates(SecTrustRef trust);
CFArrayRef SecTrustCopyInputAnchors(SecTrustRef trust);
CFArrayRef SecTrustCopyConstructedChain(SecTrustRef trust);
static CSSM_TP_APPLE_EVIDENCE_INFO * SecTrustGetEvidenceInfo(SecTrustRef trust);

typedef struct SecTrustCheckExceptionContext {
	CFDictionaryRef exception;
	bool exceptionNotFound;
} SecTrustCheckExceptionContext;

//
// Sec* API bridge functions
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustSetParameters(
    SecTrustRef trustRef,
    CSSM_TP_ACTION action,
    CFDataRef actionData)
{
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
}

/* OS X only: __OSX_AVAILABLE_STARTING(__MAC_10_3, __IPHONE_NA) */
OSStatus SecTrustSetKeychains(SecTrustRef trust, CFTypeRef keychainOrArray)
{
	/* this function is currently unsupported in unified SecTrust */
    // TODO: pull all certs out of the specified keychains for the evaluation?
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetKeychains does nothing in 10.11. Use SecTrustSetAnchorCertificates{Only} to provide anchors.");
#endif
	return errSecSuccess;
}

//
// Construct the "official" result evidence and return it
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetResult(
    SecTrustRef trustRef,
    SecTrustResultType *result,
	CFArrayRef *certChain, CSSM_TP_APPLE_EVIDENCE_INFO **statusChain)
{
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
}

//
// Retrieve extended validation trust results
//
/* OS X only: __OSX_AVAILABLE_STARTING(__MAC_10_5, __IPHONE_NA) */
OSStatus SecTrustCopyExtendedResult(SecTrustRef trust, CFDictionaryRef *result)
{
	/* bridge to support old functionality */
#if SECTRUST_DEPRECATION_WARNINGS
    syslog(LOG_ERR, "WARNING: SecTrustCopyExtendedResult will be deprecated in an upcoming release. Please use SecTrustCopyResult instead.");
#endif
	CFDictionaryRef resultDict = SecTrustCopyResult(trust);
	if (result == nil) {
        CFReleaseNull(resultDict);
		return errSecParam;
	}
	*result = resultDict;
	return errSecSuccess;
}

//
// Retrieve CSSM-level information for those who want to dig down
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetCssmResult(SecTrustRef trust, CSSM_TP_VERIFY_CONTEXT_RESULT_PTR *result)
{
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetCssmResult has been deprecated since 10.7, and has no functional equivalent in 10.11. Please use SecTrustCopyResult instead.");
#endif
	if (result) {
		*result = NULL;
	}
	return errSecServiceNotAvailable;
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
    CFArrayRef details = SecTrustCopyFilteredDetails(trust);
    CFIndex ix, count = CFArrayGetCount(details);
    bool hasDisqualifyingError = false;
    for (ix = 0; ix < count; ix++) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        if (ix == 0) { // Leaf
            if (CFDictionaryGetCount(detail) != 1 || // One error
                CFDictionaryGetValue(detail, CFSTR("ExtendedKeyUsage")) != kCFBooleanFalse) { // kSecPolicyCheckExtendedKeyUsage
                hasDisqualifyingError = true;
                break;
            }
        } else {
            if (CFDictionaryGetCount(detail) > 0) { // No errors on other certs
                hasDisqualifyingError = true;
                break;
            }
        }
    }
    CFReleaseSafe(details);
    if (hasDisqualifyingError) {
        return false;
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

//
// Retrieve CSSM_LEVEL TP return code
//
/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetCssmResultCode(SecTrustRef trustRef, OSStatus *result)
{
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
        CFIndex numStatusCodes;
        CSSM_RETURN *statusCodes = NULL;
        statusCodes = (CSSM_RETURN*)SecTrustCopyStatusCodes(trustRef, ix, &numStatusCodes);
        if (statusCodes && numStatusCodes > 0) {
            unsigned int statusIX;
            for (statusIX = 0; statusIX < numStatusCodes; statusIX++) {
                CSSM_RETURN currStatus = statusCodes[statusIX];
                uint8_t currPriority = convertCssmResultToPriority(currStatus);
                if (resultCodePriority > currPriority) {
                    cssmResultCode = currStatus;
                    resultCodePriority = currPriority;
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
}

/* OS X only: __OSX_AVAILABLE_BUT_DEPRECATED(__MAC_10_2, __MAC_10_7, __IPHONE_NA, __IPHONE_NA) */
OSStatus SecTrustGetTPHandle(SecTrustRef trust, CSSM_TP_HANDLE *handle)
{
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetTPHandle has been deprecated since 10.7, and cannot return CSSM objects in 10.11. Please stop using it.");
#endif
	if (handle) {
		*handle = NULL;
	}
	return errSecServiceNotAvailable;
}

//
// Get the user's default anchor certificate set
//
/* OS X only */
OSStatus SecTrustCopyAnchorCertificates(CFArrayRef *anchorCertificates)
{
	BEGIN_SECAPI
    CFArrayRef outArray;
	OSStatus status = SecTrustSettingsCopyUnrestrictedRoots(
			true, true, true,		/* all domains */
			&outArray);
    if (status != errSecSuccess) {
        return status;
    }
    CFIndex count = outArray ? CFArrayGetCount(outArray) : 0;
    if(count == 0) {
        return errSecNoTrustSettings;
    }
    
    /* Go through outArray and do a SecTrustEvaluate */
    CFIndex i;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    CFMutableArrayRef trustedCertArray = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (i = 0; i < count ; i++) {
        SecTrustResultType result;
        SecCertificateRef certificate = (SecCertificateRef) CFArrayGetValueAtIndex(outArray, i);
        status = SecTrustCreateWithCertificates(certificate, policy, &trust);
        if (status != errSecSuccess) {
            CFReleaseSafe(trustedCertArray);
            goto out;
        }
        status = SecTrustEvaluate(trust, &result);
        if (status != errSecSuccess) {
			CFReleaseSafe(trustedCertArray);
            goto out;
        }
        if (result != kSecTrustResultFatalTrustFailure) {
            CFArrayAppendValue(trustedCertArray, certificate);
        }
        CFReleaseNull(trust);
    }
    if (CFArrayGetCount(trustedCertArray) == 0) {
    	status = errSecNoTrustSettings;
        CFReleaseSafe(trustedCertArray);
        goto out;
    }
    *anchorCertificates = trustedCertArray;
out:
	CFReleaseSafe(outArray);
    CFReleaseSafe(policy);
    CFReleaseSafe(trust);
    return status;
	END_SECAPI
}

/* We have an iOS-style SecTrustRef, but we need to return a CDSA-based SecKeyRef.
 */
SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust)
{
	SecKeyRef pubKey = NULL;
	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, 0);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	(void) SecCertificateCopyPublicKey(certificate, &pubKey);
#pragma clang diagnostic pop
	return pubKey;
}

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
    dispatch_queue_t        _trustQueue;
} TSecTrust;

CFArrayRef SecTrustCopyInputCertificates(SecTrustRef trust)
{
	if (!trust) { return NULL; };
	TSecTrust *secTrust = (TSecTrust *)trust;
	if (secTrust->_certificates) {
		CFRetain(secTrust->_certificates);
	}
	return secTrust->_certificates;
}

CFArrayRef SecTrustCopyInputAnchors(SecTrustRef trust)
{
	if (!trust) { return NULL; };
	TSecTrust *secTrust = (TSecTrust *)trust;
	if (secTrust->_anchors) {
		CFRetain(secTrust->_anchors);
	}
	return secTrust->_anchors;
}

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
	if (secTrust->_trustResult != kSecTrustResultInvalid &&
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
	CFIndex numStatusCodes = 0;

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
		SecTrustSettingsDomain foundDomain = kSecTrustSettingsDomainUser;
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

		CFIndex numCodes=0;
		CSSM_RETURN *statusCodes = (CSSM_RETURN*)SecTrustCopyStatusCodes(trust, idx, &numCodes);
		if (statusCodes) {
			// Realloc space for these status codes at end of our status codes block.
			// Two important things to note:
			// 1. the actual length is numCodes+1 because SecTrustCopyStatusCodes
			// allocates one more element at the end for the CrlReason value.
			// 2. realloc may cause the pointer to move, which means we will
			// need to fix up the StatusCodes fields after we're done with this loop.
			CFIndex totalStatusCodes = numStatusCodes + numCodes + 1;
			statusArray = (CSSM_RETURN *)realloc(statusArray, totalStatusCodes * sizeof(CSSM_RETURN));
			evInfo->StatusCodes = &statusArray[numStatusCodes];
			evInfo->NumStatusCodes = (uint32)numCodes;
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
        CFIndex numStatusCodes;
        CSSM_RETURN *statusCodes = NULL;
        statusCodes = (CSSM_RETURN*)SecTrustCopyStatusCodes(trust, ix, &numStatusCodes);
        if (statusCodes) {
            SInt32 reason = statusCodes[numStatusCodes];  // stored at end of status codes array
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
        CFRelease(certDict);
    }

    return properties;
}

/* deprecated in 10.5 */
OSStatus SecTrustGetCSSMAnchorCertificates(const CSSM_DATA **cssmAnchors,
	uint32 *cssmAnchorCount)
{
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
}


//
// Get and set user trust settings. Deprecated in 10.5.
// User Trust getter, deprecated, works as it always has.
//
OSStatus SecTrustGetUserTrust(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting *trustSetting)
{
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustGetUserTrust has been deprecated since 10.5, and does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
}

//
// The public setter, also deprecated; it maps to the appropriate
// Trust Settings call if possible, else throws errSecUnimplemented.
//
OSStatus SecTrustSetUserTrust(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting trustSetting)
{
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetUserTrust has been deprecated since 10.5, and does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
}

//
// This one is the now-private version of what SecTrustSetUserTrust() used to
// be. The public API can no longer manipulate User Trust settings, only
// view them.
//
OSStatus SecTrustSetUserTrustLegacy(SecCertificateRef certificate,
    SecPolicyRef policy, SecTrustUserSetting trustSetting)
{
	/* this function is unsupported in unified SecTrust */
#if SECTRUST_DEPRECATION_WARNINGS
	syslog(LOG_ERR, "WARNING: SecTrustSetUserTrustLegacy does nothing in 10.11. Please stop using it.");
#endif
	return errSecServiceNotAvailable;
}
