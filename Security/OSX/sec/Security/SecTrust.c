/*
 * Copyright (c) 2006-2015 Apple Inc. All Rights Reserved.
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
 * SecTrust.c - CoreFoundation based certificate trust evaluator
 *
 */

#include <Security/SecTrustPriv.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePath.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyCerts.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecuritydXPC.h>
#include <Security/SecInternal.h>
#include <Security/SecBasePriv.h>
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
#include <syslog.h>
#include <pthread.h>
#include <os/activity.h>

#include <utilities/SecIOFormat.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecCertificateTrace.h>
#include <utilities/debugging.h>

#include "SecRSAKey.h"
#include <libDER/oids.h>

#include <ipc/securityd_client.h>

#include <securityd/SecTrustServer.h>

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

SEC_CONST_DECL (kSecTrustInfoExtendedValidationKey, "ExtendedValidation");
SEC_CONST_DECL (kSecTrustInfoCompanyNameKey, "CompanyName");
SEC_CONST_DECL (kSecTrustInfoRevocationKey, "Revocation");
SEC_CONST_DECL (kSecTrustInfoRevocationValidUntilKey, "RevocationValidUntil");
SEC_CONST_DECL (kSecTrustInfoCertificateTransparencyKey, "CertificateTransparency");

/* Public trust result constants */
SEC_CONST_DECL (kSecTrustEvaluationDate, "TrustEvaluationDate");
SEC_CONST_DECL (kSecTrustExtendedValidation, "TrustExtendedValidation");
SEC_CONST_DECL (kSecTrustOrganizationName, "Organization");
SEC_CONST_DECL (kSecTrustResultValue, "TrustResultValue");
SEC_CONST_DECL (kSecTrustRevocationChecked, "TrustRevocationChecked");
SEC_CONST_DECL (kSecTrustRevocationReason, "TrustRevocationReason");
SEC_CONST_DECL (kSecTrustRevocationValidUntilDate, "TrustExpirationDate");
SEC_CONST_DECL (kSecTrustResultDetails, "TrustResultDetails");
SEC_CONST_DECL (kSecTrustCertificateTransparency, "TrustCertificateTransparency");

#pragma mark -
#pragma mark SecTrust

/********************************************************
 ****************** SecTrust object *********************
 ********************************************************/
struct __SecTrust {
	CFRuntimeBase			_base;
	CFArrayRef				_certificates;
	CFArrayRef				_anchors;
	CFTypeRef				_policies;
	CFArrayRef				_responses;
	CFArrayRef				_SCTs;
    CFArrayRef              _trustedLogs;
	CFDateRef				_verifyDate;
	SecCertificatePathRef	_chain;
	SecKeyRef				_publicKey;
	CFArrayRef              _details;
	CFDictionaryRef         _info;
	CFArrayRef              _exceptions;

    /* Note that a value of kSecTrustResultInvalid (0)
     * indicates the trust must be (re)evaluated; any
     * functions which modify trust parameters in a way
     * that would invalidate the current result must set
     * this value back to kSecTrustResultInvalid.
     */
    SecTrustResultType      _trustResult;

    /* If true we don't trust any anchors other than the ones in _anchors. */
    bool                    _anchorsOnly;

	/* Master switch to permit or disable network use in policy evaluation */
	SecNetworkPolicy		_networkPolicy;
};

/* Forward declarations of static functions. */
static OSStatus SecTrustEvaluateIfNecessary(SecTrustRef trust);

/* Static functions. */
static CFStringRef SecTrustCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
    SecTrustRef trust = (SecTrustRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<SecTrustRef: %p>"), trust);
}

static void SecTrustDestroy(CFTypeRef cf) {
    SecTrustRef trust = (SecTrustRef)cf;
	CFReleaseSafe(trust->_certificates);
	CFReleaseSafe(trust->_policies);
	CFReleaseSafe(trust->_responses);
    CFReleaseSafe(trust->_SCTs);
    CFReleaseSafe(trust->_trustedLogs);
	CFReleaseSafe(trust->_verifyDate);
	CFReleaseSafe(trust->_anchors);
	CFReleaseSafe(trust->_chain);
	CFReleaseSafe(trust->_publicKey);
	CFReleaseSafe(trust->_details);
	CFReleaseSafe(trust->_info);
    CFReleaseSafe(trust->_exceptions);
}

/* Public API functions. */
CFGiblisFor(SecTrust)

OSStatus SecTrustCreateWithCertificates(CFTypeRef certificates,
    CFTypeRef policies, SecTrustRef *trust) {
    OSStatus status = errSecParam;
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFArrayRef l_certs = NULL, l_policies = NULL;
    SecTrustRef result = NULL;

	check(certificates);
	check(trust);
    CFTypeID certType = CFGetTypeID(certificates);
    if (certType == CFArrayGetTypeID()) {
        /* We need at least 1 certificate. */
        require_quiet(CFArrayGetCount(certificates) > 0, errOut);
        l_certs = CFArrayCreateCopy(allocator, certificates);
    } else if (certType == SecCertificateGetTypeID()) {
        l_certs = CFArrayCreate(allocator, &certificates, 1,
            &kCFTypeArrayCallBacks);
    } else {
        goto errOut;
    }
    if (!l_certs) {
        status = errSecAllocate;
        goto errOut;
    }

	if (!policies) {
		CFTypeRef policy = SecPolicyCreateBasicX509();
		l_policies = CFArrayCreate(allocator, &policy, 1,
			&kCFTypeArrayCallBacks);
		CFRelease(policy);
	}
	else if (CFGetTypeID(policies) == CFArrayGetTypeID()) {
#if (SECTRUST_OSX && TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE || TARGET_IPHONE_SIMULATOR))
#warning STU: <rdar://21330613>
		// On OS X, passing an array consisting of the ssl and
		// revocation policies is causing us not to return all the
		// expected EV keys from SecTrustCopyResult, whereas if the
		// revocation policy is omitted, they are present.
		// Must debug this.
		CFMutableArrayRef t_policies = CFArrayCreateMutable(allocator, 0,
				&kCFTypeArrayCallBacks);
		CFIndex ix, count = CFArrayGetCount(policies);
		SecPolicyRef revocationPolicy = NULL, sslServerPolicy = NULL;
		for (ix=0; ix<count; ix++) {
			SecPolicyRef t_policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, ix);
			CFStringRef oidstr = SecPolicyGetOidString(t_policy);
			if (oidstr && CFEqual(oidstr, CFSTR("sslServer"))) {
				sslServerPolicy = t_policy;
			}
			if (oidstr && CFEqual(oidstr, CFSTR("revocation"))) {
				revocationPolicy = t_policy;
			} else if (t_policy) {
				CFArrayAppendValue(t_policies, t_policy);
			}
		}
		if (revocationPolicy && !(sslServerPolicy && count==2)) {
			CFArrayAppendValue(t_policies, revocationPolicy);
		}
		l_policies = CFArrayCreateCopy(allocator, t_policies);
		CFReleaseSafe(t_policies);
#else
		l_policies = CFArrayCreateCopy(allocator, policies);
#endif
	}
	else if (CFGetTypeID(policies) == SecPolicyGetTypeID()) {
		l_policies = CFArrayCreate(allocator, &policies, 1,
			&kCFTypeArrayCallBacks);
    } else {
        goto errOut;
    }
    if (!l_policies) {
        status = errSecAllocate;
        goto errOut;
    }

    CFIndex size = sizeof(struct __SecTrust);
    require_quiet(result = (SecTrustRef)_CFRuntimeCreateInstance(allocator,
        SecTrustGetTypeID(), size - sizeof(CFRuntimeBase), 0), errOut);
    memset((char*)result + sizeof(result->_base), 0,
        sizeof(*result) - sizeof(result->_base));
    status = errSecSuccess;

errOut:
    if (status) {
        CFReleaseSafe(result);
        CFReleaseSafe(l_certs);
        CFReleaseSafe(l_policies);
    } else {
        result->_certificates = l_certs;
        result->_policies = l_policies;
        if (trust)
            *trust = result;
        else
            CFReleaseSafe(result);
    }
    return status;
}

static void SetTrustSetNeedsEvaluation(SecTrustRef trust) {
    check(trust);
    if (trust) {
        trust->_trustResult = kSecTrustResultInvalid;
    }
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust,
    Boolean anchorCertificatesOnly) {
    if (!trust) {
        return errSecParam;
    }
    SetTrustSetNeedsEvaluation(trust);
    trust->_anchorsOnly = anchorCertificatesOnly;

    return errSecSuccess;
}

OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust,
    CFArrayRef anchorCertificates) {
    if (!trust) {
        return errSecParam;
    }
    SetTrustSetNeedsEvaluation(trust);
    if (anchorCertificates)
        CFRetain(anchorCertificates);
    if (trust->_anchors)
        CFRelease(trust->_anchors);
    trust->_anchors = anchorCertificates;
    trust->_anchorsOnly = (anchorCertificates != NULL);

    return errSecSuccess;
}

OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust,
    CFArrayRef *anchors) {
	if (!trust|| !anchors) {
		return errSecParam;
	}
	CFArrayRef anchorsArray = NULL;
	if (trust->_anchors) {
		anchorsArray = CFArrayCreateCopy(kCFAllocatorDefault, trust->_anchors);
		if (!anchorsArray) {
			return errSecAllocate;
		}
	}
	*anchors = anchorsArray;
	return errSecSuccess;
}

OSStatus SecTrustSetOCSPResponse(SecTrustRef trust, CFTypeRef responseData) {
    if (!trust) {
        return errSecParam;
    }
	SetTrustSetNeedsEvaluation(trust);
	CFArrayRef responseArray = NULL;
	if (responseData) {
		if (CFGetTypeID(responseData) == CFArrayGetTypeID()) {
            responseArray = CFArrayCreateCopy(kCFAllocatorDefault, responseData);
        } else if (CFGetTypeID(responseData) == CFDataGetTypeID()) {
            responseArray = CFArrayCreate(kCFAllocatorDefault, &responseData, 1,
                                          &kCFTypeArrayCallBacks);
        } else {
            return errSecParam;
        }
    }
	if (trust->_responses)
		CFRelease(trust->_responses);
	trust->_responses = responseArray;

	return errSecSuccess;
}

OSStatus SecTrustSetSignedCertificateTimestamps(SecTrustRef trust, CFArrayRef sctArray) {
    if (!trust) {
        return errSecParam;
    }
    SetTrustSetNeedsEvaluation(trust);
    CFRetainAssign(trust->_SCTs, sctArray);

    return errSecSuccess;
}

OSStatus SecTrustSetTrustedLogs(SecTrustRef trust, CFArrayRef trustedLogs) {
    if (!trust) {
        return errSecParam;
    }
    SetTrustSetNeedsEvaluation(trust);
    CFRetainAssign(trust->_trustedLogs, trustedLogs);

    return errSecSuccess;
}

OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate) {
    if (!trust) {
        return errSecParam;
    }
    SetTrustSetNeedsEvaluation(trust);
    check(verifyDate);
    CFRetainAssign(trust->_verifyDate, verifyDate);

    return errSecSuccess;
}

OSStatus SecTrustSetPolicies(SecTrustRef trust, CFTypeRef newPolicies) {
    if (!trust || !newPolicies) {
        return errSecParam;
    }
    SetTrustSetNeedsEvaluation(trust);
    check(newPolicies);

    CFArrayRef policyArray = NULL;
    if (CFGetTypeID(newPolicies) == CFArrayGetTypeID()) {
		policyArray = CFArrayCreateCopy(kCFAllocatorDefault, newPolicies);
	} else if (CFGetTypeID(newPolicies) == SecPolicyGetTypeID()) {
		policyArray = CFArrayCreate(kCFAllocatorDefault, &newPolicies, 1,
			&kCFTypeArrayCallBacks);
    } else {
        return errSecParam;
    }

    if (trust->_policies)
        CFRelease(trust->_policies);
    trust->_policies = policyArray;

    return errSecSuccess;
}

OSStatus SecTrustCopyPolicies(SecTrustRef trust, CFArrayRef *policies) {
	if (!trust|| !policies) {
		return errSecParam;
	}
	if (!trust->_policies) {
		return errSecInternal;
	}
	CFArrayRef policyArray = CFArrayCreateCopy(kCFAllocatorDefault, trust->_policies);
	if (!policyArray) {
		return errSecAllocate;
	}
	*policies = policyArray;
	return errSecSuccess;
}

OSStatus SecTrustSetNetworkFetchAllowed(SecTrustRef trust, Boolean allowFetch) {
	if (!trust) {
		return errSecParam;
	}
	trust->_networkPolicy = (allowFetch) ? useNetworkEnabled : useNetworkDisabled;
	return errSecSuccess;
}

OSStatus SecTrustGetNetworkFetchAllowed(SecTrustRef trust, Boolean *allowFetch) {
	if (!trust || !allowFetch) {
		return errSecParam;
	}
	Boolean allowed = false;
	SecNetworkPolicy netPolicy = trust->_networkPolicy;
	if (netPolicy == useNetworkDefault) {
		// network fetch is enabled by default for SSL only
		CFIndex idx, count = (trust->_policies) ? CFArrayGetCount(trust->_policies) : 0;
		for (idx=0; idx<count; idx++) {
			SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(trust->_policies, idx);
			if (policy) {
				CFDictionaryRef props = SecPolicyCopyProperties(policy);
				if (props) {
					CFTypeRef value = (CFTypeRef)CFDictionaryGetValue(props, kSecPolicyOid);
					if (value) {
						if (CFEqual(value, kSecPolicyAppleSSL)) {
							allowed = true;
						}
					}
					CFRelease(props);
				}
			}
		}
	} else {
		// caller has explicitly set the network policy
		allowed = (netPolicy == useNetworkEnabled);
	}
	*allowFetch = allowed;
	return errSecSuccess;
}

CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust) {
    CFAbsoluteTime verifyTime;
    if (trust && trust->_verifyDate) {
        verifyTime = CFDateGetAbsoluteTime(trust->_verifyDate);
    } else {
        verifyTime = CFAbsoluteTimeGetCurrent();
		/* Record the verifyDate we ended up using. */
        if (trust) {
            trust->_verifyDate = CFDateCreate(CFGetAllocator(trust), verifyTime);
        }
    }
    return verifyTime;
}

CFArrayRef SecTrustGetDetails(SecTrustRef trust) {
	if (!trust) {
		return NULL;
	}
    SecTrustEvaluateIfNecessary(trust);
    return trust->_details;
}

OSStatus SecTrustGetTrustResult(SecTrustRef trust,
    SecTrustResultType *result) {
	if (!trust || !result) {
		return errSecParam;
	}
	*result = trust->_trustResult;
	return errSecSuccess;
}

static CFStringRef kSecCertificateDetailSHA1Digest = CFSTR("SHA1Digest");

static CFDictionaryRef SecTrustGetExceptionForCertificateAtIndex(SecTrustRef trust, CFIndex ix) {
    if (!trust->_exceptions || ix >= CFArrayGetCount(trust->_exceptions))
        return NULL;
    CFDictionaryRef exception = (CFDictionaryRef)CFArrayGetValueAtIndex(trust->_exceptions, ix);
    if (CFGetTypeID(exception) != CFDictionaryGetTypeID())
        return NULL;

	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    if (!certificate)
        return NULL;

    /* If the exception contains the current certificates sha1Digest in the
       kSecCertificateDetailSHA1Digest key then we use it otherwise we ignore it. */
    CFDataRef sha1Digest = SecCertificateGetSHA1Digest(certificate);
    CFTypeRef digestValue = CFDictionaryGetValue(exception, kSecCertificateDetailSHA1Digest);
    if (!digestValue || !CFEqual(sha1Digest, digestValue))
        exception = NULL;

    return exception;
}

struct SecTrustCheckExceptionContext {
    CFDictionaryRef exception;
    bool exceptionNotFound;

};

static void SecTrustCheckException(const void *key, const void *value, void *context) {
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

#if TARGET_OS_IPHONE
static CFArrayRef SecTrustCreatePolicyAnchorsArray(const UInt8* certData, CFIndex certLength)
{
    CFArrayRef array = NULL;
    CFAllocatorRef allocator = kCFAllocatorDefault;
    SecCertificateRef cert = SecCertificateCreateWithBytes(allocator, certData, certLength);
    if (cert) {
        array = CFArrayCreate(allocator, (const void **)&cert, 1, &kCFTypeArrayCallBacks);
        CFReleaseSafe(cert);
    }
    return array;
}
#endif

static void SecTrustAddPolicyAnchors(SecTrustRef trust)
{
    /* Provide anchor certificates specifically required by certain policies.
       This is used to evaluate test policies where the anchor is not provided
       in the root store and may not be able to be supplied by the caller.
     */
    CFArrayRef policies = (trust) ? trust->_policies : NULL;
    if (!policies) {
        return;
    }
    CFIndex ix, count = CFArrayGetCount(policies);
    for (ix = 0; ix < count; ++ix) {
        SecPolicyRef policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, ix);
        if (policy) {
			#if TARGET_OS_IPHONE
            if (CFEqual(policy->_oid, kSecPolicyAppleTestSMPEncryption)) {
                CFReleaseSafe(trust->_anchors);
                trust->_anchors = SecTrustCreatePolicyAnchorsArray(_SEC_TestAppleRootCAECC, sizeof(_SEC_TestAppleRootCAECC));
                trust->_anchorsOnly = true;
                break;
            }
			#endif
        }
    }
}


// uncomment for verbose debug logging (debug builds only)
//#define CERT_TRUST_DUMP 1

#if CERT_TRUST_DUMP
static void sectrustlog(int priority, const char *format, ...)
{
#ifndef NDEBUG
	// log everything
#else
	if (priority < LOG_NOTICE) // log warnings and errors
#endif
	{
		va_list list;
		va_start(list, format);
		vsyslog(priority, format, list);
		va_end(list);
	}
}

static void sectrustshow(CFTypeRef obj, const char *context)
{
#ifndef NDEBUG
	CFStringRef desc = CFCopyDescription(obj);
	if (!desc) return;

	CFIndex length = CFStringGetMaximumSizeForEncoding(CFStringGetLength(desc), kCFStringEncodingUTF8) + 1;
	char* buffer = (char*) malloc(length);
	if (buffer) {
		Boolean converted = CFStringGetCString(desc, buffer, length, kCFStringEncodingUTF8);
		if (converted) {
			const char *prefix = (context) ? context : "";
			const char *separator = (context) ? " " : "";
			sectrustlog(LOG_NOTICE, "%s%s%s", prefix, separator, buffer);
		}
		free(buffer);
	}
	CFRelease(desc);
#endif
}

static void cert_trust_dump(SecTrustRef trust) {
	SecCertificateRef leaf = (SecCertificateRef) CFArrayGetValueAtIndex(trust->_certificates, 0);
	CFStringRef name = (leaf) ? SecCertificateCopySubjectSummary(leaf) : NULL;
	secerror("leaf \"%@\"", name);
	secerror(": result = %d", (int) trust->_trustResult);
	if (trust->_chain) {
		CFIndex ix, count = SecCertificatePathGetCount(trust->_chain);
		CFMutableArrayRef chain = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
		for (ix = 0; ix < count; ix++) {
			SecCertificateRef cert = SecCertificatePathGetCertificateAtIndex(trust->_chain, ix);
			if (cert) {
				CFArrayAppendValue(chain, cert);
			}
		}
		sectrustshow(chain, "chain:");
		CFReleaseSafe(chain);
	}
	secerror(": %ld certificates, %ld anchors, %ld policies,  %ld details",
		(trust->_certificates) ? (long)CFArrayGetCount(trust->_certificates) : 0,
		(trust->_anchors) ? (long)CFArrayGetCount(trust->_anchors) : 0,
		(trust->_policies) ? (long)CFArrayGetCount(trust->_policies) : 0,
		(trust->_details) ? (long)CFArrayGetCount(trust->_details) : 0);

	sectrustshow(trust->_verifyDate, "verify date:");
	sectrustshow(trust->_certificates, "certificates:");
	sectrustshow(trust->_anchors, "anchors:");
	sectrustshow(trust->_policies, "policies:");
	sectrustshow(trust->_details, "details:");
	sectrustshow(trust->_info, "info:");

	CFReleaseSafe(name);
}
#else
static void cert_trust_dump(SecTrustRef trust) {}
#endif


OSStatus SecTrustEvaluate(SecTrustRef trust, SecTrustResultType *result) {
    if (!trust) {
        return errSecParam;
    }
    OSStatus status = SecTrustEvaluateIfNecessary(trust);
    if (status) {
        return status;
    }
    /* post-process trust result based on exceptions */
    SecTrustResultType trustResult = trust->_trustResult;
    if (trustResult == kSecTrustResultUnspecified) {
        /* If leaf is in exceptions -> proceed, otherwise unspecified. */
        if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
            trustResult = kSecTrustResultProceed;
    } else if (trustResult == kSecTrustResultRecoverableTrustFailure) {
        /* If we have exceptions get details and match to exceptions. */
        CFIndex pathLength = CFArrayGetCount(trust->_details);
        struct SecTrustCheckExceptionContext context = {};
        CFIndex ix;
        for (ix = 0; ix < pathLength; ++ix) {
            CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(trust->_details, ix);

			if ((ix == 0) && CFDictionaryContainsKey(detail, kSecPolicyCheckBlackListedLeaf))
		   	{
	   			trustResult = kSecTrustResultFatalTrustFailure;
	   			goto DoneCheckingTrust;
	   		}

	   		if (CFDictionaryContainsKey(detail, kSecPolicyCheckBlackListedKey))
	   		{
	   			trustResult = kSecTrustResultFatalTrustFailure;
	   			goto DoneCheckingTrust;
	   		}

            context.exception = SecTrustGetExceptionForCertificateAtIndex(trust, ix);
            CFDictionaryApplyFunction(detail, SecTrustCheckException, &context);
            if (context.exceptionNotFound) {
                break;
            }
        }
        if (!context.exceptionNotFound)
            trustResult = kSecTrustResultProceed;
    }
DoneCheckingTrust:
#if SECTRUST_OSX
#warning STU: <rdar://21014749>
	// may be fixed with rdar://21014749
	if (trustResult == kSecTrustResultProceed)
		trustResult = kSecTrustResultUnspecified;
#endif
	trust->_trustResult = trustResult;

    /* log to syslog when there is a trust failure */
    if (trustResult != kSecTrustResultProceed &&
        trustResult != kSecTrustResultConfirm &&
        trustResult != kSecTrustResultUnspecified) {
        CFStringRef failureDesc = SecTrustCopyFailureDescription(trust);
        secerror("%@", failureDesc);
        CFRelease(failureDesc);
    }


    if (result) {
        *result = trustResult;
    }

    return status;
}

OSStatus SecTrustEvaluateAsync(SecTrustRef trust,
	dispatch_queue_t queue, SecTrustCallback result)
{
	dispatch_async(queue, ^{
		SecTrustResultType trustResult;
		if (errSecSuccess != SecTrustEvaluate(trust, &trustResult)) {
			trustResult = kSecTrustResultInvalid;
		}
		result(trust, trustResult);
	});
	return errSecSuccess;
}

static bool append_certificate_to_xpc_array(SecCertificateRef certificate, xpc_object_t xpc_certificates);
static xpc_object_t copy_xpc_certificates_array(CFArrayRef certificates);
xpc_object_t copy_xpc_policies_array(CFArrayRef policies);
OSStatus validate_array_of_items(CFArrayRef array, CFStringRef arrayItemType, CFTypeID itemTypeID, bool required);

static bool append_certificate_to_xpc_array(SecCertificateRef certificate, xpc_object_t xpc_certificates) {
    if (!certificate) {
        return true; // NOOP
	}
    size_t length = SecCertificateGetLength(certificate);
    const uint8_t *bytes = SecCertificateGetBytePtr(certificate);
    if (!length || !bytes) {
		return false;
	}
    xpc_array_set_data(xpc_certificates, XPC_ARRAY_APPEND, bytes, length);
    return true;
}

static xpc_object_t copy_xpc_certificates_array(CFArrayRef certificates) {
    xpc_object_t xpc_certificates = xpc_array_create(NULL, 0);
	if (!xpc_certificates) {
		return NULL;
	}
    CFIndex ix, count = CFArrayGetCount(certificates);
    for (ix = 0; ix < count; ++ix) {
		SecCertificateRef certificate = (SecCertificateRef) CFArrayGetValueAtIndex(certificates, ix);
    #if SECTRUST_VERBOSE_DEBUG
		size_t length = SecCertificateGetLength(certificate);
		const uint8_t *bytes = SecCertificateGetBytePtr(certificate);
		secerror("idx=%d of %d; cert=0x%lX length=%ld bytes=0x%lX", (int)ix, (int)count, (uintptr_t)certificate, (size_t)length, (uintptr_t)bytes);
    #endif
        if (!append_certificate_to_xpc_array(certificate, xpc_certificates)) {
            xpc_release(xpc_certificates);
            xpc_certificates = NULL;
			break;
        }
    }
	return xpc_certificates;
}

static bool SecXPCDictionarySetCertificates(xpc_object_t message, const char *key, CFArrayRef certificates, CFErrorRef *error) {
	xpc_object_t xpc_certificates = copy_xpc_certificates_array(certificates);
    if (!xpc_certificates) {
		SecError(errSecAllocate, error, CFSTR("failed to create xpc_array of certificates"));
        return false;
	}
    xpc_dictionary_set_value(message, key, xpc_certificates);
    xpc_release(xpc_certificates);
    return true;
}

static bool SecXPCDictionarySetPolicies(xpc_object_t message, const char *key, CFArrayRef policies, CFErrorRef *error) {
    xpc_object_t xpc_policies = copy_xpc_policies_array(policies);
    if (!xpc_policies) {
		SecError(errSecAllocate, error, CFSTR("failed to create xpc_array of policies"));
        return false;
	}
    xpc_dictionary_set_value(message, key, xpc_policies);
    xpc_release(xpc_policies);
    return true;
}


static bool CFDataAppendToXPCArray(CFDataRef data, xpc_object_t xpc_data_array, CFErrorRef *error) {
    if (!data)
        return true; // NOOP

    size_t length = CFDataGetLength(data);
    const uint8_t *bytes = CFDataGetBytePtr(data);
    if (!length || !bytes)
        return SecError(errSecParam, error, CFSTR("invalid CFDataRef"));

    xpc_array_set_data(xpc_data_array, XPC_ARRAY_APPEND, bytes, length);
    return true;
}


static xpc_object_t CFDataArrayCopyXPCArray(CFArrayRef data_array, CFErrorRef *error) {
    xpc_object_t xpc_data_array;
    require_action_quiet(xpc_data_array = xpc_array_create(NULL, 0), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array")));
    CFIndex ix, count = CFArrayGetCount(data_array);
    for (ix = 0; ix < count; ++ix) {
        if (!CFDataAppendToXPCArray((CFDataRef)CFArrayGetValueAtIndex(data_array, ix), xpc_data_array, error)) {
            xpc_release(xpc_data_array);
            return NULL;
        }
    }

exit:
    return xpc_data_array;
}

static bool SecXPCDictionarySetDataArray(xpc_object_t message, const char *key, CFArrayRef data_array, CFErrorRef *error) {
    xpc_object_t xpc_data_array = CFDataArrayCopyXPCArray(data_array, error);
    if (!xpc_data_array)
        return false;
    xpc_dictionary_set_value(message, key, xpc_data_array);
    xpc_release(xpc_data_array);
    return true;
}

static bool SecXPCDictionaryCopyChainOptional(xpc_object_t message, const char *key, SecCertificatePathRef *path, CFErrorRef *error) {
    xpc_object_t xpc_path = xpc_dictionary_get_value(message, key);
    if (!xpc_path) {
        *path = NULL;
        return true;
    }
    *path = SecCertificatePathCreateWithXPCArray(xpc_path, error);
    return *path;
}

static int SecXPCDictionaryGetNonZeroInteger(xpc_object_t message, const char *key, CFErrorRef *error) {
    int64_t value = xpc_dictionary_get_int64(message, key);
    if (!value) {
        SecError(errSecInternal, error, CFSTR("object for key %s is 0"), key);
    }
    return (int)value;
}

static SecTrustResultType certs_anchors_bool_policies_responses_scts_logs_date_ag_to_details_info_chain_int_error_request(enum SecXPCOperation op, CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef *details, CFDictionaryRef *info, SecCertificatePathRef *chain, CFErrorRef *error)
{
    __block SecTrustResultType tr = kSecTrustResultInvalid;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        if (!SecXPCDictionarySetCertificates(message, kSecTrustCertificatesKey, certificates, error))
            return false;
        if (anchors && !SecXPCDictionarySetCertificates(message, kSecTrustAnchorsKey, anchors, error))
            return false;
        if (anchorsOnly)
            xpc_dictionary_set_bool(message, kSecTrustAnchorsOnlyKey, anchorsOnly);
        if (!SecXPCDictionarySetPolicies(message, kSecTrustPoliciesKey, policies, error))
            return false;
        if (responses && !SecXPCDictionarySetDataArray(message, kSecTrustResponsesKey, responses, error))
            return false;
        if (SCTs && !SecXPCDictionarySetDataArray(message, kSecTrustSCTsKey, SCTs, error))
            return false;
        if (trustedLogs && !SecXPCDictionarySetPList(message, kSecTrustTrustedLogsKey, trustedLogs, error))
            return false;
        xpc_dictionary_set_double(message, kSecTrustVerifyDateKey, verifyTime);
        return true;
    }, ^bool(xpc_object_t response, CFErrorRef *error) {
        secdebug("trust", "response: %@", response);
        return SecXPCDictionaryCopyArrayOptional(response, kSecTrustDetailsKey, details, error) &&
        SecXPCDictionaryCopyDictionaryOptional(response, kSecTrustInfoKey, info, error) &&
        SecXPCDictionaryCopyChainOptional(response, kSecTrustChainKey, chain, error) &&
        ((tr = SecXPCDictionaryGetNonZeroInteger(response, kSecTrustResultKey, error)) != kSecTrustResultInvalid);
    });
    return tr;
}

OSStatus validate_array_of_items(CFArrayRef array, CFStringRef arrayItemType, CFTypeID itemTypeID, bool required) {
	OSStatus result = errSecSuccess;
	CFIndex index, count;
	count = (array) ? CFArrayGetCount(array) : 0;
	if (!count && required) {
		secerror("no %@ in array!", arrayItemType);
		result = errSecParam;
	}
	for (index = 0; index < count; index++) {
		CFTypeRef item = (CFTypeRef) CFArrayGetValueAtIndex(array, index);
		if (!item) {
			secerror("%@ %@ (index %d)", arrayItemType, CFSTR("reference is nil"), (int)index);
			result = errSecParam;
			continue;
		}
		if (CFGetTypeID(item) != itemTypeID) {
			secerror("%@ %@ (index %d)", arrayItemType, CFSTR("is not the expected CF type"), (int)index);
			result = errSecParam;
		}
		// certificates
		if (CFGetTypeID(item) == SecCertificateGetTypeID()) {
			SecCertificateRef certificate = (SecCertificateRef) item;
			CFIndex length = SecCertificateGetLength(certificate);
			const UInt8 *bytes = SecCertificateGetBytePtr(certificate);
			if (!length) {
				secerror("%@ %@ (index %d)", arrayItemType, CFSTR("has zero length"), (int)index);
				result = errSecParam;
			}
			if (!bytes) {
				secerror("%@ %@ (index %d)", arrayItemType, CFSTR("has nil bytes"), (int)index);
				result = errSecParam;
			}
		#if SECTRUST_VERBOSE_DEBUG
			secerror("%@[%d] of %d = %ld bytes @ 0x%lX", arrayItemType, (int)index, (int)count, (size_t)length, (uintptr_t)bytes);
		#endif
		}
		// policies
		if (CFGetTypeID(item) == SecPolicyGetTypeID()) {
			SecPolicyRef policy = (SecPolicyRef) item;
			CFStringRef oidStr = policy->_oid;
			if (!oidStr || (CFGetTypeID(oidStr) != CFStringGetTypeID())) {
				oidStr = CFSTR("has invalid OID string!");
				secerror("%@ %@ (index %d)", arrayItemType, oidStr, (int)index);
			}
		#if SECTRUST_VERBOSE_DEBUG
			secerror("%@[%d] of %d = \"%@\" 0x%lX", arrayItemType, (int)index, (int)count, oidStr, (uintptr_t)policy);
		#endif
		}
	}
	return result;
}

static OSStatus SecTrustValidateInput(SecTrustRef trust) {
	OSStatus status, result = errSecSuccess;

	// certificates (required)
	status = validate_array_of_items(trust->_certificates, CFSTR("certificate"), SecCertificateGetTypeID(), true);
	if (status) result = status;
	// anchors (optional)
	status = validate_array_of_items(trust->_anchors, CFSTR("input anchor"), SecCertificateGetTypeID(), false);
	if (status) result = status;
	// policies (required??)
	status = validate_array_of_items(trust->_policies, CFSTR("policy"), SecPolicyGetTypeID(), true);
	if (status) result = status;
	// _responses, _SCTs, _trustedLogs, ...
	// verify time: SecTrustGetVerifyTime(trust)
	// access groups: SecAccessGroupsGetCurrent()

	return result;
}


static OSStatus SecTrustEvaluateIfNecessary(SecTrustRef trust) {
    __block OSStatus result;
    check(trust);
    if (!trust)
        return errSecParam;

    if (trust->_trustResult != kSecTrustResultInvalid)
        return errSecSuccess;

    trust->_trustResult = kSecTrustResultOtherError; /* to avoid potential recursion */

    CFReleaseNull(trust->_chain);
    CFReleaseNull(trust->_details);
    CFReleaseNull(trust->_info);

    os_activity_initiate("SecTrustEvaluateIfNecessary", OS_ACTIVITY_FLAG_DEFAULT, ^{
        SecTrustAddPolicyAnchors(trust);
		SecTrustValidateInput(trust);

        /* @@@ Consider an optimization where we keep a side dictionary with the SHA1 hash of ever SecCertificateRef we send, so we only send potential duplicates once, and have the server respond with either just the SHA1 hash of a certificate, or the complete certificate in the response depending on whether the client already sent it, so we don't send back certificates to the client it already has. */
        result = SecOSStatusWith(^bool (CFErrorRef *error) {
            trust->_trustResult = SECURITYD_XPC(sec_trust_evaluate,
                                                certs_anchors_bool_policies_responses_scts_logs_date_ag_to_details_info_chain_int_error_request,
                                                trust->_certificates, trust->_anchors, trust->_anchorsOnly,
                                                trust->_policies, trust->_responses, trust->_SCTs, trust->_trustedLogs,
                                                SecTrustGetVerifyTime(trust), SecAccessGroupsGetCurrent(),
                                                &trust->_details, &trust->_info, &trust->_chain, error);
            if (trust->_trustResult == kSecTrustResultInvalid /* TODO check domain */ &&
                SecErrorGetOSStatus(*error) == errSecNotAvailable &&
                CFArrayGetCount(trust->_certificates)) {
            /* We failed to talk to securityd.  The only time this should
               happen is when we are running prior to launchd enabling
               registration of services.  This currently happens when we
               are running from the ramdisk.   To make ASR happy we initialize
               _chain and return success with a failure as the trustResult, to
               make it seem like we did a cert evaluation, so ASR can extract
               the public key from the leaf. */
                trust->_chain = SecCertificatePathCreate(NULL, (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0));
                if (error)
                    CFReleaseNull(*error);
                return true;
            }
            return trust->_trustResult != kSecTrustResultInvalid;
        });
    });

    return result;
}

/* Helper for the qsort below. */
static int compare_strings(const void *a1, const void *a2) {
    CFStringRef s1 = *(CFStringRef *)a1;
    CFStringRef s2 = *(CFStringRef *)a2;
    return (int) CFStringCompare(s1, s2, kCFCompareForcedOrdering);
}

CFStringRef SecTrustCopyFailureDescription(SecTrustRef trust) {
    CFMutableStringRef reason = CFStringCreateMutable(NULL, 0);
    CFArrayRef details = SecTrustGetDetails(trust);
    CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
    for (CFIndex ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        CFIndex dCount = CFDictionaryGetCount(detail);
        if (dCount) {
            if (ix == 0)
                CFStringAppend(reason, CFSTR(" [leaf"));
            else if (ix == pathLength - 1)
                CFStringAppend(reason, CFSTR(" [root"));
            else
                CFStringAppendFormat(reason, NULL, CFSTR(" [ca%" PRIdCFIndex ), ix);

            const void *keys[dCount];
            CFDictionaryGetKeysAndValues(detail, &keys[0], NULL);
            qsort(&keys[0], dCount, sizeof(keys[0]), compare_strings);
            for (CFIndex kix = 0; kix < dCount; ++kix) {
                CFStringRef key = keys[kix];
                const void *value = CFDictionaryGetValue(detail, key);
                CFStringAppendFormat(reason, NULL, CFSTR(" %@%@"), key,
                                     (CFGetTypeID(value) == CFBooleanGetTypeID()
                                      ? CFSTR("") : value));
            }
            CFStringAppend(reason, CFSTR("]"));
        }
    }
    return reason;
}

#if SECTRUST_OSX
/* On OS X we need SecTrustCopyPublicKey to give us a CDSA-based SecKeyRef,
   so we will refer to this one internally as SecTrustCopyPublicKey_ios,
   and call it from SecTrustCopyPublicKey.
 */
SecKeyRef SecTrustCopyPublicKey_ios(SecTrustRef trust)
#else
SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust)
#endif
{
    if (!trust) {
        return NULL;
    }
	if (!trust->_publicKey) {
        if (!trust->_chain) {
            /* Trust hasn't been evaluated yet, first attempt to retrieve public key from leaf cert as is. */
        #if SECTRUST_OSX
            trust->_publicKey = SecCertificateCopyPublicKey_ios(SecTrustGetCertificateAtIndex(trust, 0));
        #else
            trust->_publicKey = SecCertificateCopyPublicKey(SecTrustGetCertificateAtIndex(trust, 0));
        #endif
#if 0
            if (!trust->_publicKey) {
                /* If this fails, use the passed-in certs in order as if they are a valid cert path,
                   and attempt to extract the key. */
                SecCertificatePathRef path;
                // SecCertificatePathCreateWithArray would have crashed if this code was ever called,
                // since it expected an array of CFDataRefs, not an array of certificates.
                path = SecCertificatePathCreateWithArray(trust->_certificates);
                trust->_publicKey = SecCertificatePathCopyPublicKeyAtIndex(path, 0);
                CFRelease(path);
            }
#endif
            if (!trust->_publicKey) {
                /* Last resort, we evaluate the trust to get a _chain. */
                SecTrustEvaluateIfNecessary(trust);
            }
        }
        if (trust->_chain) {
            trust->_publicKey = SecCertificatePathCopyPublicKeyAtIndex(trust->_chain, 0);
        }
	}

	if (trust->_publicKey)
		CFRetain(trust->_publicKey);

	return trust->_publicKey;
}

CFIndex SecTrustGetCertificateCount(SecTrustRef trust) {
    if (!trust) {
        return 0;
    }
    SecTrustEvaluateIfNecessary(trust);
	return (trust->_chain) ? SecCertificatePathGetCount(trust->_chain) : 1;
}

SecCertificateRef SecTrustGetCertificateAtIndex(SecTrustRef trust,
    CFIndex ix) {
    if (!trust) {
        return NULL;
    }
    if (ix == 0) {
        return (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
    }
    SecTrustEvaluateIfNecessary(trust);
    return (trust->_chain) ? SecCertificatePathGetCertificateAtIndex(trust->_chain, ix) : NULL;
}

CFDictionaryRef SecTrustCopyInfo(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    CFDictionaryRef info = trust->_info;
    if (info)
        CFRetain(info);
    return info;
}

CFDataRef SecTrustCopyExceptions(SecTrustRef trust) {
    CFArrayRef details = SecTrustGetDetails(trust);
    CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
    CFMutableArrayRef exceptions = CFArrayCreateMutable(kCFAllocatorDefault, pathLength, &kCFTypeArrayCallBacks);
    CFIndex ix;
    for (ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        CFIndex detailCount = CFDictionaryGetCount(detail);
        CFMutableDictionaryRef exception;
        if (ix == 0 || detailCount > 0) {
            exception = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, detailCount + 1, detail);
            SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
            CFDataRef digest = SecCertificateGetSHA1Digest(certificate);
            CFDictionaryAddValue(exception, kSecCertificateDetailSHA1Digest, digest);
        } else {
            /* Add empty exception dictionaries for non leaf certs which have no exceptions to save space. */
            exception = (CFMutableDictionaryRef)CFDictionaryCreate(kCFAllocatorDefault, NULL, NULL, 0,
                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        }
        CFArrayAppendValue(exceptions, exception);
        CFRelease(exception);
    }

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

bool SecTrustSetExceptions(SecTrustRef trust, CFDataRef encodedExceptions) {
	if (!trust) {
		return false;
	}
	CFArrayRef exceptions = NULL;

	if (NULL != encodedExceptions) {
		exceptions = CFPropertyListCreateWithData(kCFAllocatorDefault,
			encodedExceptions, kCFPropertyListImmutable, NULL, NULL);
	}

    if (exceptions && CFGetTypeID(exceptions) != CFArrayGetTypeID()) {
        CFRelease(exceptions);
        exceptions = NULL;
    }
    CFReleaseSafe(trust->_exceptions);
    trust->_exceptions = exceptions;

    /* If there is a valid exception entry for our current leaf we're golden. */
    if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
        return true;

    /* The passed in exceptions didn't match our current leaf, so we discard it. */
    CFReleaseNull(trust->_exceptions);
    return false;
}

CFArrayRef SecTrustCopySummaryPropertiesAtIndex(SecTrustRef trust, CFIndex ix) {
    CFMutableArrayRef summary;
	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    summary = SecCertificateCopySummaryProperties(certificate,
        SecTrustGetVerifyTime(trust));
    /* FIXME Add more details in the failure case. */

    return summary;
}

CFArrayRef SecTrustCopyDetailedPropertiesAtIndex(SecTrustRef trust, CFIndex ix) {
    CFArrayRef summary;
	SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    summary = SecCertificateCopyProperties(certificate);

    return summary;
}

#if 0



/* Valid chain.
   Can be on any non root cert in the chain.
   Priority: Top down
   Short circuit: Yes (No other errors matter after this one)
   Non recoverable error
   Trust UI: Invalid certificate chain linkage
   Cert UI: Invalid linkage to parent certificate
*/
CFStringRef kSecPolicyCheckIdLinkage = CFSTR("IdLinkage");

/* X.509 required checks.
   Can be on any cert in the chain
   Priority: Top down
   Short circuit: Yes (No other errors matter after this one)
   Non recoverable error
   Trust UI: (One or more) unsupported critical extensions found.
*/
/* If we have no names for the extention oids use:
   Cert UI: One or more unsupported critical extensions found (Non recoverable error).
   Cert UI: Unsupported 'foo', 'bar', baz' critical extensions found.
*/
CFStringRef kSecPolicyCheckCriticalExtensions = CFSTR("CriticalExtensions");
/* Cert UI: Unsupported critical Qualified Certificate Statements extension found (Non recoverable error). */
CFStringRef kSecPolicyCheckQualifiedCertStatements = CFSTR("QualifiedCertStatements");
/* Cert UI: Certificate has an empty subject (and no critial subjectAltname). */

/* Trusted root.
   Only apply to the anchor.
   Priority: N/A
   Short circuit: No (Under discussion)
   Recoverable
   Trust UI: Root certificate is not trusted (for this policy/app/host/whatever?)
   Cert UI: Not a valid anchor
*/
CFStringRef kSecPolicyCheckAnchorTrusted = CFSTR("AnchorTrusted");
CFStringRef kSecPolicyCheckAnchorSHA1 = CFSTR("AnchorSHA1");

CFStringRef kSecPolicyCheckAnchorApple = CFSTR("AnchorApple");
CFStringRef kSecPolicyAppleAnchorIncludeTestRoots = CFSTR("AnchorAppleTestRoots");

/* Binding.
   Only applies to leaf
   Priority: N/A
   Short Circuit: No
   Recoverable
   Trust UI: (Hostname|email address) mismatch
*/
CFStringRef kSecPolicyCheckSSLHostname = CFSTR("SSLHostname");

/* Policy specific checks.
   Can be on any cert in the chain
   Priority: Top down
   Short Circuit: No
   Recoverable
   Trust UI: Certificate chain is not valid for the current policy.
   OR: (One or more) certificates in the chain are not valid for the current policy/application
*/
CFStringRef kSecPolicyCheckNonEmptySubject = CFSTR("NonEmptySubject");
/* Cert UI: Non CA certificate used as CA.
   Cert UI: CA certificate used as leaf.
   Cert UI: Cert chain length exceeded.
   Cert UI: Basic constraints extension not critical (non fatal).
   Cert UI: Leaf certificate has basic constraints extension (non fatal).
 */
CFStringRef kSecPolicyCheckBasicContraints = CFSTR("BasicContraints");
CFStringRef kSecPolicyCheckKeyUsage = CFSTR("KeyUsage");
CFStringRef kSecPolicyCheckExtendedKeyUsage = CFSTR("ExtendedKeyUsage");
/* Checks that the issuer of the leaf has exactly one Common Name and that it
   matches the specified string. */
CFStringRef kSecPolicyCheckIssuerCommonName = CFSTR("IssuerCommonName");
/* Checks that the leaf has exactly one Common Name and that it has the
   specified string as a prefix. */
CFStringRef kSecPolicyCheckSubjectCommonNamePrefix = CFSTR("SubjectCommonNamePrefix");
/* Check that the certificate chain length matches the specificed CFNumberRef
   length. */
CFStringRef kSecPolicyCheckChainLength = CFSTR("ChainLength");
CFStringRef kSecPolicyCheckNotValidBefore = CFSTR("NotValidBefore");

/* Expiration.
   Can be on any cert in the chain
   Priority: Top down
   Short Circuit: No
   Recoverable
   Trust UI: One or more certificates have expired or are not valid yet.
   OS: The (root|intermediate|leaf) certificate (expired on 'date'|is not valid until 'date')
   Cert UI: Certificate (expired on 'date'|is not valid until 'date')
*/
CFStringRef kSecPolicyCheckValidIntermediates = CFSTR("ValidIntermediates");
CFStringRef kSecPolicyCheckValidLeaf = CFSTR("ValidLeaf");
CFStringRef kSecPolicyCheckValidRoot = CFSTR("ValidRoot");

#endif

struct TrustFailures {
    bool badLinkage;
    bool unknownCritExtn;
    bool untrustedAnchor;
    bool hostnameMismatch;
    bool policyFail;
    bool invalidCert;
};

static void applyDetailProperty(const void *_key, const void *_value,
    void *context) {
    CFStringRef key = (CFStringRef)_key;
    struct TrustFailures *tf = (struct TrustFailures *)context;
    if (CFGetTypeID(_value) != CFBooleanGetTypeID()) {
        /* Value isn't a CFBooleanRef, oh no! */
        return;
    }
    CFBooleanRef value = (CFBooleanRef)_value;
    if (CFBooleanGetValue(value)) {
        /* Not an actual failure so we don't report it. */
        return;
    }

    /* @@@ FIXME: Report a different return value when something is in the
       details but masked out by an exception and use that below for display
       purposes. */
    if (CFEqual(key, kSecPolicyCheckIdLinkage)) {
        tf->badLinkage = true;
    } else if (CFEqual(key, kSecPolicyCheckCriticalExtensions)
        || CFEqual(key, kSecPolicyCheckQualifiedCertStatements)) {
        tf->unknownCritExtn = true;
    } else if (CFEqual(key, kSecPolicyCheckAnchorTrusted)
        || CFEqual(key, kSecPolicyCheckAnchorSHA1)
        || CFEqual(key, kSecPolicyCheckAnchorApple)) {
        tf->untrustedAnchor = true;
    } else if (CFEqual(key, kSecPolicyCheckSSLHostname)) {
        tf->hostnameMismatch = true;
    } else if (CFEqual(key, kSecPolicyCheckValidIntermediates)
        || CFEqual(key, kSecPolicyCheckValidLeaf)
        || CFEqual(key, kSecPolicyCheckValidLeaf)) {
        tf->invalidCert = true;
    } else
    /* Anything else is a policy failure. */
#if 0
    if (CFEqual(key, kSecPolicyCheckNonEmptySubject)
        || CFEqual(key, kSecPolicyCheckBasicContraints)
        || CFEqual(key, kSecPolicyCheckKeyUsage)
        || CFEqual(key, kSecPolicyCheckExtendedKeyUsage)
        || CFEqual(key, kSecPolicyCheckIssuerCommonName)
        || CFEqual(key, kSecPolicyCheckSubjectCommonNamePrefix)
        || CFEqual(key, kSecPolicyCheckChainLength)
        || CFEqual(key, kSecPolicyCheckNotValidBefore))
#endif
    {
        tf->policyFail = true;
    }
}

static void appendError(CFMutableArrayRef properties, CFStringRef error) {
    CFStringRef localizedError = SecFrameworkCopyLocalizedString(error,
        CFSTR("SecCertificate"));
	if (!localizedError) {
		//secerror("WARNING: localized error string was not found in Security.framework");
		localizedError = CFRetain(error);
	}
    appendProperty(properties, kSecPropertyTypeError, NULL, NULL,
                   localizedError);
    CFReleaseNull(localizedError);
}

CFArrayRef SecTrustCopyProperties(SecTrustRef trust) {
    CFArrayRef details = SecTrustGetDetails(trust);
    if (!details)
        return NULL;

    struct TrustFailures tf = {};

    CFIndex ix, count = CFArrayGetCount(details);
    for (ix = 0; ix < count; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)
            CFArrayGetValueAtIndex(details, ix);
        /* We now have a detail dictionary for certificate at index ix, with
           a key value pair for each failed policy check.  Let's convert it
           from Ro-Man form into something a Hu-Man can understand. */
        CFDictionaryApplyFunction(detail, applyDetailProperty, &tf);
    }

    CFMutableArrayRef properties = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    /* The badLinkage and unknownCritExtn failures are short circuited, since
       you can't recover from those errors. */
    if (tf.badLinkage) {
        appendError(properties, CFSTR("Invalid certificate chain linkage."));
    } else if (tf.unknownCritExtn) {
        appendError(properties, CFSTR("One or more unsupported critical extensions found."));
    } else {
        if (tf.untrustedAnchor) {
            appendError(properties, CFSTR("Root certificate is not trusted."));
        }
        if (tf.hostnameMismatch) {
            appendError(properties, CFSTR("Hostname mismatch."));
        }
        if (tf.policyFail) {
            appendError(properties, CFSTR("Policy requirements not met."));
        }
        if (tf.invalidCert) {
            appendError(properties, CFSTR("One or more certificates have expired or are not valid yet."));
        }
    }

    if (CFArrayGetCount(properties) == 0) {
        /* The certificate chain is trusted, return an empty plist */
#if (TARGET_OS_MAC && !(TARGET_OS_EMBEDDED || TARGET_OS_IPHONE))
        // return empty (non-null) plist
#else
        // return NULL plist
        CFReleaseNull(properties);
#endif
    }

    return properties;
}

CFDictionaryRef SecTrustCopyResult(SecTrustRef trust) {
	// Builds and returns a dictionary of evaluation results.
	if (!trust) {
		return NULL;
	}
	CFMutableDictionaryRef results = CFDictionaryCreateMutable(NULL, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

	// kSecTrustResultDetails (per-cert results)
	CFArrayRef details = SecTrustGetDetails(trust);
	if (details) {
		CFDictionarySetValue(results, (const void *)kSecTrustResultDetails, (const void *)details);
	}

	// kSecTrustResultValue (overall trust result)
	CFNumberRef numValue = CFNumberCreate(NULL, kCFNumberSInt32Type, &trust->_trustResult);
	if (numValue) {
		CFDictionarySetValue(results, (const void *)kSecTrustResultValue, (const void *)numValue);
		CFRelease(numValue);
	}
	CFDictionaryRef info = trust->_info;
	if (trust->_trustResult == kSecTrustResultInvalid || !info) {
		return results; // we have nothing more to add
	}

	// kSecTrustEvaluationDate
	CFDateRef evaluationDate = trust->_verifyDate;
	if (evaluationDate) {
		CFDictionarySetValue(results, (const void *)kSecTrustEvaluationDate, (const void *)evaluationDate);
	}

	// kSecTrustCertificateTransparency
	CFBooleanRef ctValue;
	if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoCertificateTransparencyKey, (const void **)&ctValue)) {
		CFDictionarySetValue(results, (const void *)kSecTrustCertificateTransparency, (const void *)ctValue);
	}

	// kSecTrustExtendedValidation
	CFBooleanRef evValue;
	if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoExtendedValidationKey, (const void **)&evValue)) {
		CFDictionarySetValue(results, (const void *)kSecTrustExtendedValidation, (const void *)evValue);
	}

	// kSecTrustOrganizationName
	CFStringRef organizationName;
	if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoCompanyNameKey, (const void **)&organizationName)) {
		CFDictionarySetValue(results, (const void *)kSecTrustOrganizationName, (const void *)organizationName);
	}

	// kSecTrustRevocationChecked
	CFBooleanRef revocationChecked;
	if (CFDictionaryGetValueIfPresent(info, kSecTrustRevocationChecked, (const void **)&revocationChecked)) {
		CFDictionarySetValue(results, (const void *)kSecTrustRevocationChecked, (const void *)revocationChecked);
	}

	// kSecTrustRevocationReason
	CFNumberRef revocationReason;
	if (CFDictionaryGetValueIfPresent(info, kSecTrustRevocationReason, (const void **)&revocationReason)) {
		CFDictionarySetValue(results, (const void *)kSecTrustRevocationReason, (const void *)revocationReason);
	}

	// kSecTrustRevocationValidUntilDate
	CFDateRef validUntilDate;
	if (CFDictionaryGetValueIfPresent(info, kSecTrustRevocationValidUntilDate, (const void **)&validUntilDate)) {
		CFDictionarySetValue(results, (const void *)kSecTrustRevocationValidUntilDate, (const void *)validUntilDate);
	}

	return results;
}

// Return 0 upon error.
static int to_int_error_request(enum SecXPCOperation op, CFErrorRef *error) {
    __block int64_t result = 0;
    securityd_send_sync_and_do(op, error, NULL, ^bool(xpc_object_t response, CFErrorRef *error) {
        result = xpc_dictionary_get_int64(response, kSecXPCKeyResult);
        if (!result)
            return SecError(errSecInternal, error, CFSTR("int64 missing in response"));
        return true;
    });
    return (int)result;
}

// version 0 -> error, so we need to start at version 1 or later.
OSStatus SecTrustGetOTAPKIAssetVersionNumber(int* versionNumber)
{
    OSStatus result;
    os_activity_t trace_activity = os_activity_start("SecTrustGetOTAPKIAssetVersionNumber", OS_ACTIVITY_FLAG_DEFAULT);
    result = SecOSStatusWith(^bool(CFErrorRef *error) {
        if (!versionNumber)
            return SecError(errSecParam, error, CFSTR("versionNumber is NULL"));

        return (*versionNumber = SECURITYD_XPC(sec_ota_pki_asset_version, to_int_error_request, error)) != 0;
    });

    os_activity_end(trace_activity);
    return result;
}

#define do_if_registered(sdp, ...) if (gSecurityd && gSecurityd->sdp) { return gSecurityd->sdp(__VA_ARGS__); }

static bool xpc_dictionary_entry_is_type(xpc_object_t dictionary, const char *key, xpc_type_t type)
{
    xpc_object_t value = xpc_dictionary_get_value(dictionary, key);

    return value && (xpc_get_type(value) == type);
}

OSStatus SecTrustOTAPKIGetUpdatedAsset(int* didUpdateAsset)
{
	CFErrorRef error = NULL;
	do_if_registered(sec_ota_pki_get_new_asset, &error);

	int64_t num = 0;
    xpc_object_t message = securityd_create_message(kSecXPCOpOTAPKIGetNewAsset, &error);
    if (message)
	{
        xpc_object_t response = securityd_message_with_reply_sync(message, &error);

        if (response && xpc_dictionary_entry_is_type(response, kSecXPCKeyResult, XPC_TYPE_INT64))
		{
            num = (int64_t) xpc_dictionary_get_int64(response, kSecXPCKeyResult);
			xpc_release(response);
        }

        xpc_release(message);
	}

	if (NULL != didUpdateAsset)
	{
		*didUpdateAsset = (int)num;
	}
	return noErr;
}


#if 0
// MARK: -
// MARK: SecTrustNode
/********************************************************
 **************** SecTrustNode object *******************
 ********************************************************/
typedef uint8_t SecFetchingState;
enum {
    kSecFetchingStatePassedIn = 0,
    kSecFetchingStateLocal,
    kSecFetchingStateFromURL,
    kSecFetchingStateDone,
};

typedef uint8_t SecTrustState;
enum {
    kSecTrustStateUnknown = 0,
    kSecTrustStateNotSigner,
    kSecTrustStateValidSigner,
};

typedef struct __SecTrustNode *SecTrustNodeRef;
struct __SecTrustNode {
	SecTrustNodeRef		_child;
	SecCertificateRef	_certificate;

    /* Cached information about _certificate */
    bool                _isAnchor;
    bool                _isSelfSigned;

    /* Set of all certificates we have ever considered as a parent.  We use
       this to avoid having to recheck certs when we go to the next phase. */
    CFMutableSet        _certificates;

    /* Parents that are still partial chains we haven't yet considered. */
    CFMutableSet        _partials;
    /* Parents that are still partial chains we have rejected.  We reconsider
      these if we get to the final phase and we still haven't found a valid
      candidate. */
    CFMutableSet        _rejected_partials;

    /* Parents that are complete chains we haven't yet considered. */
    CFMutableSet        _candidates;
    /* Parents that are complete chains we have rejected. */
    CFMutableSet        _rejected_candidates;

    /* State of candidate fetching. */
    SecFetchingState    _fetchingState;

    /* Trust state of _candidates[_candidateIndex] */
    SecTrustState       _trustState;
};
typedef struct __SecTrustNode SecTrustNode;

/* Forward declarations of static functions. */
static CFStringRef SecTrustNodeDescribe(CFTypeRef cf);
static void SecTrustNodeDestroy(CFTypeRef cf);

/* Static functions. */
static CFStringRef SecTrustNodeCopyDescription(CFTypeRef cf) {
    SecTrustNodeRef node = (SecTrustNodeRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<SecTrustNodeRef: %p>"), node);
}

static void SecTrustNodeDestroy(CFTypeRef cf) {
    SecTrustNodeRef trust = (SecTrustNodeRef)cf;
    if (trust->_child) {
        free(trust->_child);
    }
    if (trust->_certificate) {
        free(trust->_certificate);
    }
	if (trust->_candidates) {
		CFRelease(trust->_candidates);
	}
}

/* SecTrustNode API functions. */
CFGiblisFor(SecTrustNode)

SecTrustNodeRef SecTrustNodeCreate(SecTrustRef trust,
    SecCertificateRef certificate, SecTrustNodeRef child) {
    CFAllocatorRef allocator = kCFAllocatorDefault;
	check(trust);
	check(certificate);

    CFIndex size = sizeof(struct __SecTrustNode);
    SecTrustNodeRef result = (SecTrustNodeRef)_CFRuntimeCreateInstance(
		allocator, SecTrustNodeGetTypeID(), size - sizeof(CFRuntimeBase), 0);
	if (!result)
        return NULL;

    memset((char*)result + sizeof(result->_base), 0,
        sizeof(*result) - sizeof(result->_base));
    if (child) {
        CFRetain(child);
        result->_child = child;
    }
    CFRetain(certificate);
    result->_certificate = certificate;
    result->_isAnchor = SecTrustCertificateIsAnchor(certificate);

    return result;
}

SecCertificateRef SecTrustGetCertificate(SecTrustNodeRef node) {
	check(node);
    return node->_certificate;
}

CFArrayRef SecTrustNodeCopyProperties(SecTrustNodeRef node,
    SecTrustRef trust) {
	check(node);
    check(trust);
    CFMutableArrayRef summary = SecCertificateCopySummaryProperties(
        node->_certificate, SecTrustGetVerifyTime(trust));
    /* FIXME Add more details in the failure case. */
    return summary;
}

/* Attempt to verify this node's signature chain down to the child. */
SecTrustState SecTrustNodeVerifySignatureChain(SecTrustNodeRef node) {
    /* FIXME */
    return kSecTrustStateUnknown;
}


/* See if the next candidate works. */
SecTrustNodeRef SecTrustNodeCopyNextCandidate(SecTrustNodeRef node,
    SecTrustRef trust, SecFetchingState fetchingState) {
    check(node);
    check(trust);

    CFAbsoluteTime verifyTime = SecTrustGetVerifyTime(trust);

    for (;;) {
        /* If we have any unconsidered candidates left check those first. */
        while (node->_candidateIndex < CFArrayGetCount(node->_candidates)) {
            SecCertificateRef candidate = (SecCertificateRef)
                CFArrayGetValueAtIndex(node->_candidates, node->_candidateIndex);
            if (node->_fetchingState != kSecFetchingStateDone) {
                /* If we still have potential sources to fetch other candidates
                   from we ignore expired candidates. */
                if (!SecCertificateIsValidOn(candidate, verifyTime)) {
                    node->_candidateIndex++;
                    continue;
                }
            }

            SecTrustNodeRef parent = SecTrustNodeCreate(candidate, node);
            CFArrayRemoveValueAtIndex(node->_candidates, node->_candidateIndex);
            if (SecTrustNodeVerifySignatureChain(parent) ==
                kSecTrustStateNotSigner) {
                /* This candidate parent is not a valid signer of its
                   child. */
                CFRelease(parent);
                /* If another signature failed further down the chain we need
                   to backtrack down to whatever child is still a valid
                   candidate and has additional candidates to consider.
                   @@@ We really want to make the fetchingState a global of
                   SecTrust itself as well and not have any node go beyond the
                   current state of SecTrust if there are other (read cheap)
                   options to consider. */
                continue;
            }
            return parent;
        }

        /* We've run out of candidates in our current state so let's try to
           find some more. Note we fetch candidates in increasing order of
           cost in the hope we won't ever get to the more expensive fetching
           methods.  */
        SecCertificateRef certificate = node->_certificate;
        switch (node->_fetchingState) {
        case kSecFetchingStatePassedIn:
            /* Get the list of candidates from SecTrust. */
            CFDataRef akid = SecCertificateGetAuthorityKeyID(certificate);
            if (akid) {
                SecTrustAppendCandidatesWithAuthorityKeyID(akid, node->_candidates);
            } else {
                CFDataRef issuer =
                    SecCertificateGetNormalizedIssuerContent(certificate);
                SecTrustAppendCandidatesWithSubject(issuer, node->_candidates);
            }
            node->_fetchingState = kSecFetchingStateLocal;
            break;
        case kSecFetchingStateLocal:
            /* Lookup candidates in the local database. */
            node->_fetchingState = kSecFetchingStateFromURL;
            break;
        case kSecFetchingStateFromURL:
            node->_fetchingState = kSecFetchingStateCheckExpired;
            break;
        case kSecFetchingStateCheckExpired:
            /* Time to start considering expired candidates as well. */
            node->_candidateIndex = 0;
            node->_fetchingState = kSecFetchingStateDone;
            break;
        case kSecFetchingStateDone;
            return NULL;
        }
    }

    CFAllocatorRef allocator = CFGetAllocator(node);

    /* A trust node has a number of states.
       1) Look for issuing certificates by asking SecTrust about known
          parent certificates.
       2) Look for issuing certificates in certificate databases (keychains)
       3) Look for issuing certificates by going out to the web if the nodes
          certificate has a issuer location URL.
       4) Look through expired or not yet valid candidates we have put aside.

       We go though the stages 1 though 3 looking for candidate issuer
       certificates.  If a candidate certificate is not valid at verifyTime
       we put it in a to be examined later queue.  If a candidate certificate
       is valid we verify if it actually signed our certificate (if possible).
       If not we discard it and continue on to the next candidate certificate.
       If it is we return a new SecTrustNodeRef for that certificate.  */

    CFMutableArrayRef issuers = CFArrayCreateMutable(allocator, 0,
        &kCFTypeArrayCallBacks);

    /* Find a node's parent. */
    certificate = node->_certificate;
    CFDataRef akid = SecCertificateGetAuthorityKeyID(certificate);
    CFTypeRef candidates = NULL;
    if (akid) {
        candidates = (CFTypeRef)CFDictionaryGetValueForKey(skidDict, akid);
        if (candidates) {
            addValidIssuersFrom(issuers, certificate, candidates, true);
        }
    }
    if (!candidates) {
        CFDataRef issuer =
            SecCertificateGetNormalizedIssuerContent(certificate);
        candidates = (CFTypeRef)
            CFDictionaryGetValueForKey(subjectDict, issuer);
        addValidIssuersFrom(issuers, certificate, candidates, false);
    }

    if (CFArrayGetCount(issuers) == 0) {
        /* O no! we can't find an issuer for this certificate.  Let's see
           if we can find one in the local database. */
    }

    return errSecSuccess;
}

CFArrayRef SecTrustNodeCopyNextChain(SecTrustNodeRef node,
    SecTrustRef trust) {
    /* Return the next full chain that isn't a reject unless we are in
       a state where we consider returning rejects. */

    switch (node->_fetchingState) {
    case kSecFetchingStatePassedIn:
        /* Get the list of candidates from SecTrust. */
        CFDataRef akid = SecCertificateGetAuthorityKeyID(certificate);
        if (akid) {
            SecTrustAppendCandidatesWithAuthorityKeyID(akid, node->_candidates);
        } else {
            CFDataRef issuer =
                SecCertificateGetNormalizedIssuerContent(certificate);
            SecTrustAppendCandidatesWithSubject(issuer, node->_candidates);
        }
        node->_fetchingState = kSecFetchingStateLocal;
        break;
    case kSecFetchingStateLocal:
        /* Lookup candidates in the local database. */
        node->_fetchingState = kSecFetchingStateFromURL;
        break;
    case kSecFetchingStateFromURL:
        node->_fetchingState = kSecFetchingStateCheckExpired;
        break;
    case kSecFetchingStateCheckExpired:
        /* Time to start considering expired candidates as well. */
        node->_candidateIndex = 0;
        node->_fetchingState = kSecFetchingStateDone;
        break;
    case kSecFetchingStateDone;
        return NULL;
    }
}

class Source {
	Iterator parentIterator(Cert);
};

class NodeCache {
	Set nodes;

	static bool unique(Node node) {
		if (nodes.contains(node))
			return false;
		nodes.add(node);
		return true;
	}

	static bool isAnchor(Cert cert);
};

class Node {
	Cert cert;
	Node child;
	int nextSource;
	Iterator parentIterator; /* For current source of parents. */

	Node(Cert inCert) : child(nil), cert(inCert), nextSource(0) {}
	Node(Node inChild, Cert inCert) : child(inChild), cert(inCert),
		nextSource(0) {}

	CertPath certPath() {
		CertPath path;
		Node node = this;
		while (node) {
			path.add(node.cert);
			node = node.child;
		}
		return path;
	}

	void contains(Cert cert) {
		Node node = this;
		while (node) {
			if (cert == node.cert)
				return true;
			node = node.child;
		}
		return false;
	}

	Node nextParent(Array currentSources) {
		for (;;) {
			if (!nextSource ||
				parentIterator == currentSources[nextSource - 1].end()) {
				if (nextSource == currentSources.count) {
					/* We ran out of parent sources. */
					return nil;
				}
				parentIterator = currentSources[nextSource++].begin();
			}
			Certificate cert = *parentIterator++;
			/* Check for cycles and self signed chains. */
			if (!contains(cert)) {
				Node node = Node(this, parent);
				if (!NodeCache.unique(node))
					return node;
			}
		}
	}
};


class PathBuilder {
	List nodes;
	List rejects;
	Array currentSources;
	Iterator nit;
	Array allSources;
	Iterator sourceIT;
	CertPath chain;

	PathBuilder(Cert cert) {
		nodes.append(Node(cert));
		nit = nodes.begin();
		sourceIT = allSources.begin();
	}

	nextAnchoredPath() {
		if (nit == nodes.end()) {
			/* We should add another source to the list of sources to
			   search. */
			if (sourceIT == allSources.end()) {
				/* No more sources to add. */
			}
			currentSources += *sourceIT++;
			/* Resort nodes by size. */
			Nodes.sortBySize();
			nit = nodes.begin();
			/* Set the source list for all nodes. */

		}
		while (Node node = *nit) {
			Node candidate = node.nextParent(currentSources);
			if (!candidate) {
				/* The current node has no more candidate parents so move
				   along. */
				nit++;
				continue;
			}

			if (candidate.isAnchored) {
				candidates.append(candidate);
			} else
				nodes.insert(candidate, nit);
		}
	}

	findValidPath() {
		while (Node node = nextAnchoredPath()) {
			if (node.isValid()) {
				chain = node.certPath;
				break;
			}
			rejects.append(node);
		}
	}
}


#endif
