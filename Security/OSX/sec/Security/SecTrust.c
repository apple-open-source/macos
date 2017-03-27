/*
 * Copyright (c) 2006-2016 Apple Inc. All Rights Reserved.
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
#include <utilities/der_plist.h>
#include <utilities/SecDispatchRelease.h>

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
SEC_CONST_DECL (kSecTrustInfoCertificateTransparencyWhiteListKey, "CertificateTransparencyWhiteList");

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
SEC_CONST_DECL (kSecTrustCertificateTransparencyWhiteList, "TrustCertificateTransparencyWhiteList");

#pragma mark -
#pragma mark SecTrust

/********************************************************
 ****************** SecTrust object *********************
 ********************************************************/
struct __SecTrust {
    CFRuntimeBase           _base;
    CFArrayRef              _certificates;
    CFArrayRef              _anchors;
    CFTypeRef               _policies;
    CFArrayRef              _responses;
    CFArrayRef              _SCTs;
    CFArrayRef              _trustedLogs;
    CFDateRef               _verifyDate;
    SecCertificatePathRef   _chain;
    SecKeyRef               _publicKey;
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
    /* If false we shouldn't search keychains for parents or anchors. */
    bool                    _keychainsAllowed;

    /* Data blobs for legacy CSSM_TP_APPLE_EVIDENCE_INFO structure,
     * to support callers of SecTrustGetResult on OS X. Since fields of
     * one structure contain pointers into the other, these cannot be
     * serialized; if a SecTrust is being serialized or copied, these values
     * should just be initialized to NULL in the copy and built when needed. */
    void*                   _legacy_info_array;
    void*                   _legacy_status_array;

    /* The trust result as determined by the trust server,
     * before the caller's exceptions are applied.
     */
    SecTrustResultType      _trustResultBeforeExceptions;

    /* Dispatch queue for thread-safety */
    dispatch_queue_t        _trustQueue;

    /* === IMPORTANT! ===
     * Any change to this structure definition
     * must also be made in the TSecTrust structure,
     * located in SecTrust.cpp. To avoid problems,
     * new fields should always be appended at the
     * end of the structure.
     */
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

    dispatch_release_null(trust->_trustQueue);
    CFReleaseNull(trust->_certificates);
    CFReleaseNull(trust->_policies);
    CFReleaseNull(trust->_responses);
    CFReleaseNull(trust->_SCTs);
    CFReleaseNull(trust->_trustedLogs);
    CFReleaseNull(trust->_verifyDate);
    CFReleaseNull(trust->_anchors);
    CFReleaseNull(trust->_chain);
    CFReleaseNull(trust->_publicKey);
    CFReleaseNull(trust->_details);
    CFReleaseNull(trust->_info);
    CFReleaseNull(trust->_exceptions);

    if (trust->_legacy_info_array) {
        free(trust->_legacy_info_array);
    }
    if (trust->_legacy_status_array) {
        free(trust->_legacy_status_array);
    }
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
		l_policies = CFArrayCreateCopy(allocator, policies);
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
        result->_keychainsAllowed = true;
        result->_trustQueue = dispatch_queue_create("trust", DISPATCH_QUEUE_SERIAL);
        if (trust)
            *trust = result;
        else
            CFReleaseSafe(result);
    }
    return status;
}

OSStatus SecTrustCopyInputCertificates(SecTrustRef trust, CFArrayRef *certificates) {
    if (!trust || !certificates) {
        return errSecParam;
    }
    __block CFArrayRef certArray = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        certArray = CFArrayCreateCopy(NULL, trust->_certificates);
    });
    if (!certArray) {
        return errSecAllocate;
    }
    *certificates = certArray;
    return errSecSuccess;
}

OSStatus SecTrustAddToInputCertificates(SecTrustRef trust, CFTypeRef certificates) {
    if (!trust || !certificates) {
        return errSecParam;
    }
    __block CFMutableArrayRef newCertificates = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        newCertificates = CFArrayCreateMutableCopy(NULL, 0, trust->_certificates);
    });

    if (isArray(certificates)) {
        CFArrayAppendArray(newCertificates, certificates,
                            CFRangeMake(0, CFArrayGetCount(certificates)));
    } else if (CFGetTypeID(certificates) == SecCertificateGetTypeID()) {
        CFArrayAppendValue(newCertificates, certificates);
    } else {
        CFReleaseNull(newCertificates);
        return errSecParam;
    }

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseNull(trust->_certificates);
        trust->_certificates = (CFArrayRef)newCertificates;
    });

    return errSecSuccess;
}

static void SecTrustSetNeedsEvaluation(SecTrustRef trust) {
    check(trust);
    if (trust) {
        dispatch_sync(trust->_trustQueue, ^{
            trust->_trustResult = kSecTrustResultInvalid;
            trust->_trustResultBeforeExceptions = kSecTrustResultInvalid;
        });
    }
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust,
    Boolean anchorCertificatesOnly) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    trust->_anchorsOnly = anchorCertificatesOnly;

    return errSecSuccess;
}

OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust,
    CFArrayRef anchorCertificates) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    if (anchorCertificates)
        CFRetain(anchorCertificates);
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_anchors)
            CFRelease(trust->_anchors);
        trust->_anchors = anchorCertificates;
    });
    trust->_anchorsOnly = (anchorCertificates != NULL);

    return errSecSuccess;
}

OSStatus SecTrustCopyCustomAnchorCertificates(SecTrustRef trust,
    CFArrayRef *anchors) {
	if (!trust|| !anchors) {
		return errSecParam;
	}
	__block CFArrayRef anchorsArray = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_anchors) {
            anchorsArray = CFArrayCreateCopy(kCFAllocatorDefault, trust->_anchors);
        }
    });

	*anchors = anchorsArray;
	return errSecSuccess;
}

OSStatus SecTrustSetOCSPResponse(SecTrustRef trust, CFTypeRef responseData) {
    if (!trust) {
        return errSecParam;
    }
	SecTrustSetNeedsEvaluation(trust);
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
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_responses)
            CFRelease(trust->_responses);
        trust->_responses = responseArray;
    });
	return errSecSuccess;
}

OSStatus SecTrustSetSignedCertificateTimestamps(SecTrustRef trust, CFArrayRef sctArray) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        CFRetainAssign(trust->_SCTs, sctArray);
    });
    return errSecSuccess;
}

OSStatus SecTrustSetTrustedLogs(SecTrustRef trust, CFArrayRef trustedLogs) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    dispatch_sync(trust->_trustQueue, ^{
        CFRetainAssign(trust->_trustedLogs, trustedLogs);
    });
    return errSecSuccess;
}

OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    check(verifyDate);
    dispatch_sync(trust->_trustQueue, ^{
        CFRetainAssign(trust->_verifyDate, verifyDate);
    });
    return errSecSuccess;
}

OSStatus SecTrustSetPolicies(SecTrustRef trust, CFTypeRef newPolicies) {
    if (!trust || !newPolicies) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    check(newPolicies);

    __block CFArrayRef policyArray = NULL;
    if (CFGetTypeID(newPolicies) == CFArrayGetTypeID()) {
		policyArray = CFArrayCreateCopy(kCFAllocatorDefault, newPolicies);
	} else if (CFGetTypeID(newPolicies) == SecPolicyGetTypeID()) {
		policyArray = CFArrayCreate(kCFAllocatorDefault, &newPolicies, 1,
			&kCFTypeArrayCallBacks);
    } else {
        return errSecParam;
    }

    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseSafe(trust->_policies);
        trust->_policies = policyArray;
    });

    return errSecSuccess;
}

OSStatus SecTrustSetKeychainsAllowed(SecTrustRef trust, Boolean allowed) {
    if (!trust) {
        return errSecParam;
    }
    SecTrustSetNeedsEvaluation(trust);
    trust->_keychainsAllowed = allowed;

    return errSecSuccess;
}

OSStatus SecTrustGetKeychainsAllowed(SecTrustRef trust, Boolean *allowed) {
    if (!trust || !allowed) {
        return errSecParam;
    }
    *allowed = trust->_keychainsAllowed;

    return errSecSuccess;
}

OSStatus SecTrustCopyPolicies(SecTrustRef trust, CFArrayRef *policies) {
	if (!trust|| !policies) {
		return errSecParam;
	}
    __block CFArrayRef policyArray = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        policyArray = CFArrayCreateCopy(kCFAllocatorDefault, trust->_policies);
    });
	if (!policyArray) {
		return errSecAllocate;
	}
	*policies = policyArray;
	return errSecSuccess;
}

static OSStatus SecTrustSetOptionInPolicies(CFArrayRef policies, CFStringRef key, CFTypeRef value) {
    OSStatus status = errSecSuccess;
    require_action(policies && CFGetTypeID(policies) == CFArrayGetTypeID(), out, status = errSecInternal);
    for (int i=0; i < CFArrayGetCount(policies); i++) {
        SecPolicyRef policy = NULL;
        require_action_quiet(policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, i), out, status = errSecInternal);
        CFMutableDictionaryRef options = NULL;
        require_action_quiet(options = CFDictionaryCreateMutableCopy(NULL, 0, policy->_options), out, status = errSecAllocate);
        CFDictionaryAddValue(options, key, value);
        CFReleaseNull(policy->_options);
        policy->_options = options;
    }
out:
    return status;
}

static OSStatus SecTrustRemoveOptionInPolicies(CFArrayRef policies, CFStringRef key) {
    OSStatus status = errSecSuccess;
    require_action(policies && CFGetTypeID(policies) == CFArrayGetTypeID(), out, status = errSecInternal);
    for (int i=0; i < CFArrayGetCount(policies); i++) {
        SecPolicyRef policy = NULL;
        require_action_quiet(policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, i), out, status = errSecInternal);
        if (CFDictionaryGetValue(policy->_options, key)) {
            CFMutableDictionaryRef options = NULL;
            require_action_quiet(options = CFDictionaryCreateMutableCopy(NULL, 0, policy->_options), out, status = errSecAllocate);
            CFDictionaryRemoveValue(options, key);
            CFReleaseNull(policy->_options);
            policy->_options = options;
        }
    }
out:
    return status;
}

static CF_RETURNS_RETAINED CFArrayRef SecTrustCopyOptionsFromPolicies(CFArrayRef policies, CFStringRef key) {
    CFMutableArrayRef foundValues = NULL;
    foundValues = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    for (int i=0; i < CFArrayGetCount(policies); i++) {
        SecPolicyRef policy = (SecPolicyRef)CFArrayGetValueAtIndex(policies, i);
        CFTypeRef value = CFDictionaryGetValue(policy->_options, key);
        if (value) {
            CFArrayAppendValue(foundValues, value);
        }
    }
    if (!CFArrayGetCount(foundValues)) {
        CFReleaseNull(foundValues);
        return NULL;
    }
    else {
        return foundValues;
    }
}

/* The only effective way to disable network fetch is within the policy options:
 * presence of the kSecPolicyCheckNoNetworkAccess key in any of the policies
 * will prevent network access for fetching.
 * The current SecTrustServer implementation doesn't distinguish between network
 * access for revocation and network access for fetching.
 */
OSStatus SecTrustSetNetworkFetchAllowed(SecTrustRef trust, Boolean allowFetch) {
	if (!trust) {
		return errSecParam;
	}
    __block OSStatus status = errSecSuccess;
    dispatch_sync(trust->_trustQueue, ^{
        if (!allowFetch) {
            status = SecTrustSetOptionInPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);
        } else {
            status = SecTrustRemoveOptionInPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess);
        }
    });
	return status;
}

OSStatus SecTrustGetNetworkFetchAllowed(SecTrustRef trust, Boolean *allowFetch) {
	if (!trust || !allowFetch) {
		return errSecParam;
	}
    __block CFArrayRef foundValues = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        foundValues = SecTrustCopyOptionsFromPolicies(trust->_policies, kSecPolicyCheckNoNetworkAccess);
    });
    if (foundValues) {
        *allowFetch = false;
    } else {
        *allowFetch = true;
    }
    CFReleaseNull(foundValues);
	return errSecSuccess;
}

CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust) {
    __block CFAbsoluteTime verifyTime = CFAbsoluteTimeGetCurrent();
    if (!trust) {
        return verifyTime;
    }
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_verifyDate) {
            verifyTime = CFDateGetAbsoluteTime(trust->_verifyDate);
        } else {
            trust->_verifyDate = CFDateCreate(CFGetAllocator(trust), verifyTime);
        }
    });

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
    dispatch_sync(trust->_trustQueue, ^{
        *result = trust->_trustResult;
    });
	return errSecSuccess;
}

static CFStringRef kSecCertificateDetailSHA1Digest = CFSTR("SHA1Digest");

static CFDictionaryRef SecTrustGetExceptionForCertificateAtIndex(SecTrustRef trust, CFIndex ix) {
    CFDictionaryRef exception = NULL;
    __block CFArrayRef exceptions = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        exceptions = CFRetainSafe(trust->_exceptions);
    });
    if (!exceptions || ix >= CFArrayGetCount(exceptions)) {
        goto out;
    }

    SecCertificateRef certificate = SecTrustGetCertificateAtIndex(trust, ix);
    if (!certificate) {
        goto out;
    }

    exception = (CFDictionaryRef)CFArrayGetValueAtIndex(exceptions, ix);
    if (CFGetTypeID(exception) != CFDictionaryGetTypeID()) {
        exception = NULL;
        goto out;
    }

    /* If the exception contains the current certificates sha1Digest in the
       kSecCertificateDetailSHA1Digest key then we use it otherwise we ignore it. */
    CFDataRef sha1Digest = SecCertificateGetSHA1Digest(certificate);
    CFTypeRef digestValue = CFDictionaryGetValue(exception, kSecCertificateDetailSHA1Digest);
    if (!digestValue || !CFEqual(sha1Digest, digestValue))
        exception = NULL;

out:
    CFReleaseSafe(exceptions);
    return exception;
}

struct SecTrustFilteredDetailContext {
    CFDictionaryRef exception;
    CFMutableDictionaryRef filteredDetail;
};

static void SecTrustFilterDetail(const void *key, const void *value, void *context) {
    struct SecTrustFilteredDetailContext *ctx = (struct SecTrustFilteredDetailContext *)context;
    if (!key || !value || !ctx->exception || !ctx->filteredDetail) {
        return;
    }
    if (CFEqual(kSecCertificateDetailSHA1Digest, key)) {
        return; /* ignore SHA1 hash entry */
    }
    CFTypeRef exceptionValue = CFDictionaryGetValue(ctx->exception, key);
    if (exceptionValue && CFEqual(exceptionValue, value)) {
        /* both key and value match the exception */
        CFDictionaryRemoveValue(ctx->filteredDetail, key);
    }
}

CFArrayRef SecTrustCopyFilteredDetails(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
    CFIndex ix, pathLength = details ? CFArrayGetCount(details) : 0;
    CFMutableArrayRef filteredDetails = CFArrayCreateMutable(kCFAllocatorDefault, pathLength, &kCFTypeArrayCallBacks);
    if (!filteredDetails) {
        CFReleaseNull(details);
        return NULL;
    }
    for (ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
        CFIndex count = (detail) ? CFDictionaryGetCount(detail) : 0;
        CFMutableDictionaryRef filteredDetail = CFDictionaryCreateMutableCopy(kCFAllocatorDefault, count, detail);
        CFDictionaryRef exception = SecTrustGetExceptionForCertificateAtIndex(trust, ix);
        if (exception) {
            /* for each entry in the detail dictionary, remove from filteredDetail
               if it also appears in the corresponding exception dictionary. */
            struct SecTrustFilteredDetailContext context = { exception, filteredDetail };
            CFDictionaryApplyFunction(detail, SecTrustFilterDetail, &context);
        }
        if (filteredDetail) {
            CFArrayAppendValue(filteredDetails, filteredDetail);
            CFReleaseSafe(filteredDetail);
        }
    }
    CFReleaseNull(details);
    return filteredDetails;
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
    if (!trust) { return; }
    __block CFArrayRef policies = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        policies = CFRetain(trust->_policies);
    });
    CFIndex ix, count = CFArrayGetCount(policies);
    for (ix = 0; ix < count; ++ix) {
        SecPolicyRef policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, ix);
        if (policy) {
			#if TARGET_OS_IPHONE
            if (CFEqual(policy->_oid, kSecPolicyAppleTestSMPEncryption)) {
                __block CFArrayRef policyAnchors = SecTrustCreatePolicyAnchorsArray(_SEC_TestAppleRootCAECC, sizeof(_SEC_TestAppleRootCAECC));
                dispatch_sync(trust->_trustQueue, ^{
                    CFReleaseSafe(trust->_anchors);
                    trust->_anchors = policyAnchors;
                });
                trust->_anchorsOnly = true;
                break;
            }
			#endif
        }
    }
    CFReleaseSafe(policies);
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
	SecCertificateRef leaf = SecTrustGetCertificateAtIndex(trust, 0);
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
    __block SecTrustResultType trustResult = kSecTrustResultInvalid;
    dispatch_sync(trust->_trustQueue, ^{
        trustResult = trust->_trustResult;
    });
    if (trustResult == kSecTrustResultUnspecified) {
        /* If leaf is in exceptions -> proceed, otherwise unspecified. */
        if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
            trustResult = kSecTrustResultProceed;
    } else if (trustResult == kSecTrustResultRecoverableTrustFailure) {
        /* If we have exceptions get details and match to exceptions. */
        __block CFArrayRef details = NULL;
        dispatch_sync(trust->_trustQueue, ^{
            details = CFRetainSafe(trust->_details);
        });
        CFIndex pathLength = details ? CFArrayGetCount(details) : 0;
        struct SecTrustCheckExceptionContext context = {};
        CFIndex ix;
        for (ix = 0; ix < pathLength; ++ix) {
            CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
            context.exception = SecTrustGetExceptionForCertificateAtIndex(trust, ix);
            CFDictionaryApplyFunction(detail, SecTrustCheckException, &context);
            if (context.exceptionNotFound) {
                break;
            }
        }
        CFReleaseSafe(details);
        __block bool done = false;
        dispatch_sync(trust->_trustQueue, ^{
            if (!trust->_exceptions || !CFArrayGetCount(trust->_exceptions)) {
                done = true;
            }
        });
        if (done) {
            goto DoneCheckingTrust;
        }
        if (!context.exceptionNotFound)
            trustResult = kSecTrustResultProceed;
    }
DoneCheckingTrust:
    dispatch_sync(trust->_trustQueue, ^{
        trust->_trustResult = trustResult;
    });

    /* log to syslog when there is a trust failure */
    if (trustResult != kSecTrustResultProceed &&
        trustResult != kSecTrustResultConfirm &&
        trustResult != kSecTrustResultUnspecified) {
        CFStringRef failureDesc = SecTrustCopyFailureDescription(trust);
        secerror("%{public}@", failureDesc);
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
    CFRetainSafe(trust);
	dispatch_async(queue, ^{
		SecTrustResultType trustResult;
		if (errSecSuccess != SecTrustEvaluate(trust, &trustResult)) {
			trustResult = kSecTrustResultInvalid;
		}
		result(trust, trustResult);
        CFReleaseSafe(trust);
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

static SecTrustResultType certs_anchors_bool_bool_policies_responses_scts_logs_date_ag_to_details_info_chain_int_error_request(enum SecXPCOperation op, CFArrayRef certificates, CFArrayRef anchors, bool anchorsOnly, bool keychainsAllowed, CFArrayRef policies, CFArrayRef responses, CFArrayRef SCTs, CFArrayRef trustedLogs, CFAbsoluteTime verifyTime, __unused CFArrayRef accessGroups, CFArrayRef *details, CFDictionaryRef *info, SecCertificatePathRef *chain, CFErrorRef *error)
{
    __block SecTrustResultType tr = kSecTrustResultInvalid;
    securityd_send_sync_and_do(op, error, ^bool(xpc_object_t message, CFErrorRef *error) {
        if (!SecXPCDictionarySetCertificates(message, kSecTrustCertificatesKey, certificates, error))
            return false;
        if (anchors && !SecXPCDictionarySetCertificates(message, kSecTrustAnchorsKey, anchors, error))
            return false;
        if (anchorsOnly)
            xpc_dictionary_set_bool(message, kSecTrustAnchorsOnlyKey, anchorsOnly);
        xpc_dictionary_set_bool(message, kSecTrustKeychainsAllowedKey, keychainsAllowed);
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


static void SecTrustPostEvaluate(SecTrustRef trust) {
    if (!trust) { return; }

    CFIndex pathLength = (trust->_details) ? CFArrayGetCount(trust->_details) : 0;
    CFIndex ix;
    for (ix = 0; ix < pathLength; ++ix) {
        CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(trust->_details, ix);
        if ((ix == 0) && CFDictionaryContainsKey(detail, kSecPolicyCheckBlackListedLeaf)) {
            trust->_trustResult = kSecTrustResultFatalTrustFailure;
            return;
        }
        if (CFDictionaryContainsKey(detail, kSecPolicyCheckBlackListedKey)) {
            trust->_trustResult = kSecTrustResultFatalTrustFailure;
            return;
        }
    }
}

static OSStatus SecTrustEvaluateIfNecessary(SecTrustRef trust) {
    __block OSStatus result;
    check(trust);
    if (!trust)
        return errSecParam;

    __block CFAbsoluteTime verifyTime = SecTrustGetVerifyTime(trust);
    SecTrustAddPolicyAnchors(trust);
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_trustResult != kSecTrustResultInvalid) {
            result = errSecSuccess;
            return;
        }

        trust->_trustResult = kSecTrustResultOtherError; /* to avoid potential recursion */

        CFReleaseNull(trust->_chain);
        CFReleaseNull(trust->_details);
        CFReleaseNull(trust->_info);
        if (trust->_legacy_info_array) {
            free(trust->_legacy_info_array);
            trust->_legacy_info_array = NULL;
        }
        if (trust->_legacy_status_array) {
            free(trust->_legacy_status_array);
            trust->_legacy_status_array = NULL;
        }

        os_activity_initiate("SecTrustEvaluateIfNecessary", OS_ACTIVITY_FLAG_DEFAULT, ^{
            SecTrustValidateInput(trust);

            /* @@@ Consider an optimization where we keep a side dictionary with the SHA1 hash of ever SecCertificateRef we send, so we only send potential duplicates once, and have the server respond with either just the SHA1 hash of a certificate, or the complete certificate in the response depending on whether the client already sent it, so we don't send back certificates to the client it already has. */
            result = SecOSStatusWith(^bool (CFErrorRef *error) {
                trust->_trustResult = SECURITYD_XPC(sec_trust_evaluate,
                                                    certs_anchors_bool_bool_policies_responses_scts_logs_date_ag_to_details_info_chain_int_error_request,
                                                    trust->_certificates, trust->_anchors, trust->_anchorsOnly, trust->_keychainsAllowed,
                                                    trust->_policies, trust->_responses, trust->_SCTs, trust->_trustedLogs,
                                                    verifyTime, SecAccessGroupsGetCurrent(),
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
                    trust->_chain = SecCertificatePathCreate(NULL, (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0), NULL);
                    if (error)
                        CFReleaseNull(*error);
                    return true;
                }
                SecTrustPostEvaluate(trust);
                trust->_trustResultBeforeExceptions = trust->_trustResult;
                return trust->_trustResult != kSecTrustResultInvalid;
            });
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
    if (!trust) {
        return NULL;
    }
    CFMutableStringRef reason = CFStringCreateMutable(NULL, 0);
    SecTrustEvaluateIfNecessary(trust);
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
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
    CFReleaseSafe(details);
    return reason;
}

#if TARGET_OS_OSX
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
    __block SecKeyRef publicKey = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_publicKey) {
            publicKey = CFRetainSafe(trust->_publicKey);
            return;
        }
        SecCertificateRef leaf = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
#if TARGET_OS_OSX
        trust->_publicKey = SecCertificateCopyPublicKey_ios(leaf);
#else
        trust->_publicKey = SecCertificateCopyPublicKey(leaf);
#endif
        if (trust->_publicKey) {
            publicKey = CFRetainSafe(trust->_publicKey);
        }
    });
    /* If we couldn't get a public key from the leaf cert alone. */
    if (!publicKey) {
        SecTrustEvaluateIfNecessary(trust);
        dispatch_sync(trust->_trustQueue, ^{
            if (trust->_chain) {
                trust->_publicKey = SecCertificatePathCopyPublicKeyAtIndex(trust->_chain, 0);
                publicKey = CFRetainSafe(trust->_publicKey);
            }
        });
    }
	return publicKey;
}

CFIndex SecTrustGetCertificateCount(SecTrustRef trust) {
    if (!trust) {
        return 0;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFIndex certCount = 1;
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_chain) {
            certCount = SecCertificatePathGetCount(trust->_chain);
        }
    });
	return certCount;
}

SecCertificateRef SecTrustGetCertificateAtIndex(SecTrustRef trust,
    CFIndex ix) {
    if (!trust) {
        return NULL;
    }
    __block SecCertificateRef cert = NULL;
    if (ix == 0) {
        dispatch_sync(trust->_trustQueue, ^{
            cert = (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
        });
        return cert;
    }
    SecTrustEvaluateIfNecessary(trust);
    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_chain) {
            cert = SecCertificatePathGetCertificateAtIndex(trust->_chain, ix);
        }
    });
    return cert;
}

CFDictionaryRef SecTrustCopyInfo(SecTrustRef trust) {
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFDictionaryRef info = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        info = CFRetainSafe(trust->_info);
    });
    return info;
}

CFArrayRef SecTrustGetTrustExceptionsArray(SecTrustRef trust) {
    return trust->_exceptions;
}

CFDataRef SecTrustCopyExceptions(SecTrustRef trust) {
    __block CFArrayRef details = NULL;
    SecTrustEvaluateIfNecessary(trust);
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
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
    CFReleaseSafe(details);
    return encodedExceptions;
}

bool SecTrustSetExceptions(SecTrustRef trust, CFDataRef encodedExceptions) {
	if (!trust) {
		return false;
	}
	CFArrayRef exceptions = NULL;

	if (NULL != encodedExceptions) {
		exceptions = (CFArrayRef)CFPropertyListCreateWithData(kCFAllocatorDefault,
			encodedExceptions, kCFPropertyListImmutable, NULL, NULL);
	}

	if (exceptions && CFGetTypeID(exceptions) != CFArrayGetTypeID()) {
		CFRelease(exceptions);
		exceptions = NULL;
	}

    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_exceptions && !exceptions) {
            /* Exceptions are currently set and now we are clearing them. */
            trust->_trustResult = trust->_trustResultBeforeExceptions;
        }

        CFReleaseSafe(trust->_exceptions);
        trust->_exceptions = exceptions;
    });

	/* If there is a valid exception entry for our current leaf we're golden. */
	if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
		return true;

	/* The passed in exceptions didn't match our current leaf, so we discard it. */
    dispatch_sync(trust->_trustQueue, ^{
        CFReleaseNull(trust->_exceptions);
    });
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
CFStringRef kSecPolicyCheckBasicConstraints = CFSTR("BasicConstraints");
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
    bool weakKey;
    bool revocation;
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
        || CFEqual(key, kSecPolicyCheckAnchorSHA256)
        || CFEqual(key, kSecPolicyCheckAnchorApple)) {
        tf->untrustedAnchor = true;
    } else if (CFEqual(key, kSecPolicyCheckSSLHostname)) {
        tf->hostnameMismatch = true;
    } else if (CFEqual(key, kSecPolicyCheckValidIntermediates)
        || CFEqual(key, kSecPolicyCheckValidLeaf)
        || CFEqual(key, kSecPolicyCheckValidRoot)) {
        tf->invalidCert = true;
    } else if (CFEqual(key, kSecPolicyCheckWeakIntermediates)
               || CFEqual(key, kSecPolicyCheckWeakLeaf)
               || CFEqual(key, kSecPolicyCheckWeakRoot)) {
        tf->weakKey = true;
    } else if (CFEqual(key, kSecPolicyCheckRevocation)) {
        tf->revocation = true;
    } else
    /* Anything else is a policy failure. */
#if 0
    if (CFEqual(key, kSecPolicyCheckNonEmptySubject)
        || CFEqual(key, kSecPolicyCheckBasicConstraints)
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

#if TARGET_OS_OSX
/* OS X properties array has a different structure and is implemented SecTrust.cpp. */
CFArrayRef SecTrustCopyProperties_ios(SecTrustRef trust)
#else
CFArrayRef SecTrustCopyProperties(SecTrustRef trust)
#endif
{
    if (!trust) {
        return NULL;
    }
    SecTrustEvaluateIfNecessary(trust);
    __block CFArrayRef details = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        details = CFRetainSafe(trust->_details);
    });
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
        if (tf.weakKey) {
            appendError(properties, CFSTR("One or more certificates is using a weak key size."));
        }
        if (tf.revocation) {
            appendError(properties, CFSTR("One or more certificates have been revoked."));
        }
    }

    if (CFArrayGetCount(properties) == 0) {
        /* The certificate chain is trusted, return an empty plist */
        CFReleaseNull(properties);
    }

    CFReleaseNull(details);
    return properties;
}

CFDictionaryRef SecTrustCopyResult(SecTrustRef trust) {
	// Builds and returns a dictionary of evaluation results.
	if (!trust) {
		return NULL;
	}
	__block CFMutableDictionaryRef results = CFDictionaryCreateMutable(NULL, 0,
			&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    SecTrustEvaluateIfNecessary(trust);
    dispatch_sync(trust->_trustQueue, ^{
        // kSecTrustResultDetails (per-cert results)
        CFArrayRef details = trust->_details;
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
            return; // we have nothing more to add
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

        // kSecTrustCertificateTransparencyWhiteList
        CFBooleanRef ctWhiteListValue;
        if (CFDictionaryGetValueIfPresent(info, kSecTrustInfoCertificateTransparencyWhiteListKey, (const void **)&ctWhiteListValue)) {
            CFDictionarySetValue(results, (const void *)kSecTrustCertificateTransparencyWhiteList, (const void *)ctWhiteListValue);
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
    });

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

/*
 * This function performs an evaluation of the leaf certificate only, and
 * does so in the process that called it. Its primary use is in SecItemCopyMatching
 * when kSecMatchPolicy is in the dictionary.
 */
OSStatus SecTrustEvaluateLeafOnly(SecTrustRef trust, SecTrustResultType *result) {
    if (!trust) {
        return errSecParam;
    }
    OSStatus status = errSecSuccess;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    if((status = SecTrustValidateInput(trust))) {
        return status;
    }

    struct OpaqueSecLeafPVC pvc;
    SecCertificateRef leaf = SecTrustGetCertificateAtIndex(trust, 0);
    __block CFArrayRef policies = NULL;
    dispatch_sync(trust->_trustQueue, ^{
        policies = CFRetainSafe(trust->_policies);
    });
    SecLeafPVCInit(&pvc, leaf, policies, SecTrustGetVerifyTime(trust));

    if(!SecLeafPVCLeafChecks(&pvc)) {
        trustResult = kSecTrustResultRecoverableTrustFailure;
    } else {
        trustResult = kSecTrustResultUnspecified;
    }

    /* Set other result context information */
    dispatch_sync(trust->_trustQueue, ^{
        trust->_trustResult = trustResult;
        trust->_details = CFRetainSafe(pvc.details);
        trust->_info = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
        trust->_chain = SecCertificatePathCreate(NULL, (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0), NULL);
    });

    SecLeafPVCDelete(&pvc);

    /* log to syslog when there is a trust failure */
    if (trustResult != kSecTrustResultUnspecified) {
        CFStringRef failureDesc = SecTrustCopyFailureDescription(trust);
        secerror("%@", failureDesc);
        CFRelease(failureDesc);
    }

    if (result) {
        *result = trustResult;
    }

    CFReleaseSafe(policies);
    return status;
}

static void deserializeCert(const void *value, void *context) {
    CFDataRef certData = (CFDataRef)value;
    if (isData(certData)) {
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, certData);
        if (cert) {
            CFArrayAppendValue((CFMutableArrayRef)context, cert);
            CFRelease(cert);
        }
    }
}

static CF_RETURNS_RETAINED CFArrayRef SecCertificateArrayDeserialize(CFArrayRef serializedCertificates) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(serializedCertificates), errOut);
    CFIndex count = CFArrayGetCount(serializedCertificates);
    result = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    CFRange all_certs = { 0, count };
    CFArrayApplyFunction(serializedCertificates, all_certs, deserializeCert, result);
errOut:
    return result;
}

static void serializeCertificate(const void *value, void *context) {
    SecCertificateRef cert = (SecCertificateRef)value;
    if (cert && SecCertificateGetTypeID() == CFGetTypeID(cert)) {
        CFDataRef certData = SecCertificateCopyData(cert);
        if (certData) {
            CFArrayAppendValue((CFMutableArrayRef)context, certData);
            CFRelease(certData);
        }
    }
}

static CFArrayRef SecCertificateArraySerialize(CFArrayRef certificates) {
    CFMutableArrayRef result = NULL;
    require_quiet(isArray(certificates), errOut);
    CFIndex count = CFArrayGetCount(certificates);
    result = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
    CFRange all_certificates = { 0, count};
    CFArrayApplyFunction(certificates, all_certificates, serializeCertificate, result);
errOut:
    return result;
}

static CFPropertyListRef SecTrustCopyPlist(SecTrustRef trust) {
    __block CFMutableDictionaryRef output = NULL;
    output = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks,
                                       &kCFTypeDictionaryValueCallBacks);

    dispatch_sync(trust->_trustQueue, ^{
        if (trust->_certificates) {
            CFArrayRef serializedCerts = SecCertificateArraySerialize(trust->_certificates);
            if (serializedCerts) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustCertificatesKey), serializedCerts);
                CFRelease(serializedCerts);
            }
        }
        if (trust->_anchors) {
            CFArrayRef serializedAnchors = SecCertificateArraySerialize(trust->_anchors);
            if (serializedAnchors) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustAnchorsKey), serializedAnchors);
                CFRelease(serializedAnchors);
            }
        }
        if (trust->_policies) {
            CFArrayRef serializedPolicies = SecPolicyArrayCreateSerialized(trust->_policies);
            if (serializedPolicies) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustPoliciesKey), serializedPolicies);
                CFRelease(serializedPolicies);
            }
        }
        if (trust->_responses) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustResponsesKey), trust->_responses);
        }
        if (trust->_SCTs) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustSCTsKey), trust->_SCTs);
        }
        if (trust->_trustedLogs) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustTrustedLogsKey), trust->_trustedLogs);
        }
        if (trust->_verifyDate) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustVerifyDateKey), trust->_verifyDate);
        }
        if (trust->_chain) {
            CFArrayRef serializedChain = SecCertificatePathCreateSerialized(trust->_chain, NULL);
            if (serializedChain) {
                CFDictionaryAddValue(output, CFSTR(kSecTrustChainKey), serializedChain);
                CFRelease(serializedChain);
            }
        }
        if (trust->_details) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustDetailsKey), trust->_details);
        }
        if (trust->_info) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustInfoKey), trust->_info);
        }
        if (trust->_exceptions) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustExceptionsKey), trust->_exceptions);
        }
        CFNumberRef trustResult = CFNumberCreate(NULL, kCFNumberSInt32Type, &trust->_trustResult);
        if (trustResult) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustResultKey), trustResult);
        }
        CFReleaseNull(trustResult);
        if (trust->_anchorsOnly) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustAnchorsOnlyKey), kCFBooleanTrue);
        } else {
            CFDictionaryAddValue(output, CFSTR(kSecTrustAnchorsOnlyKey), kCFBooleanFalse);
        }
        if (trust->_keychainsAllowed) {
            CFDictionaryAddValue(output, CFSTR(kSecTrustKeychainsAllowedKey), kCFBooleanTrue);
        } else {
            CFDictionaryAddValue(output, CFSTR(kSecTrustKeychainsAllowedKey), kCFBooleanFalse);
        }
    });

    return output;
}

CFDataRef SecTrustSerialize(SecTrustRef trust, CFErrorRef *error) {
    CFPropertyListRef plist = NULL;
    CFDataRef derTrust = NULL;
    require_action_quiet(trust, out,
                         SecError(errSecParam, error, CFSTR("null trust input")));
    require_action_quiet(plist = SecTrustCopyPlist(trust), out,
                         SecError(errSecDecode, error, CFSTR("unable to create trust plist")));
    require_quiet(derTrust = CFPropertyListCreateDERData(NULL, plist, error), out);

out:
    CFReleaseNull(plist);
    return derTrust;
}

static OSStatus SecTrustCreateFromPlist(CFPropertyListRef plist, SecTrustRef CF_RETURNS_RETAINED *trust) {
    OSStatus status = errSecParam;
    SecTrustRef output = NULL;
    CFTypeRef serializedCertificates = NULL, serializedPolicies = NULL, serializedAnchors = NULL,
                serializedChain = NULL;
    CFNumberRef trustResultNum = NULL;
    CFArrayRef certificates = NULL, policies = NULL, anchors = NULL, responses = NULL,
                SCTs = NULL, trustedLogs = NULL, details = NULL, exceptions = NULL;
    CFDateRef verifyDate = NULL;
    CFDictionaryRef info = NULL;
    SecCertificatePathRef chain = NULL;

    require_quiet(CFDictionaryGetTypeID() == CFGetTypeID(plist), out);
    require_quiet(serializedCertificates = CFDictionaryGetValue(plist, CFSTR(kSecTrustCertificatesKey)), out);
    require_quiet(certificates = SecCertificateArrayDeserialize(serializedCertificates), out);
    require_quiet(serializedPolicies = CFDictionaryGetValue(plist, CFSTR(kSecTrustPoliciesKey)), out);
    require_quiet(policies = SecPolicyArrayCreateDeserialized(serializedPolicies), out);
    require_noerr_quiet(status = SecTrustCreateWithCertificates(certificates, policies, &output), out);

    serializedAnchors = CFDictionaryGetValue(plist, CFSTR(kSecTrustAnchorsKey));
    if (isArray(serializedAnchors)) {
        anchors = SecCertificateArrayDeserialize(serializedAnchors);
        output->_anchors = anchors;
    }
    responses = CFDictionaryGetValue(plist, CFSTR(kSecTrustResponsesKey));
    if (isArray(responses)) {
        output->_responses = CFRetainSafe(responses);
    }
    SCTs = CFDictionaryGetValue(plist, CFSTR(kSecTrustSCTsKey));
    if (isArray(responses)) {
        output->_SCTs = CFRetainSafe(SCTs);
    }
    trustedLogs = CFDictionaryGetValue(plist, CFSTR(kSecTrustTrustedLogsKey));
    if (isArray(trustedLogs)) {
        output->_trustedLogs = CFRetainSafe(trustedLogs);
    }
    verifyDate = CFDictionaryGetValue(plist, CFSTR(kSecTrustVerifyDateKey));
    if (isDate(verifyDate)) {
        output->_verifyDate = CFRetainSafe(verifyDate);
    }
    serializedChain = CFDictionaryGetValue(plist, CFSTR(kSecTrustChainKey));
    if (isArray(serializedChain)) {
        chain = SecCertificatePathCreateDeserialized(serializedChain, NULL);
        output->_chain = chain;
    }
    details = CFDictionaryGetValue(plist, CFSTR(kSecTrustDetailsKey));
    if (isArray(details)) {
        output->_details = CFRetainSafe(details);
    }
    info = CFDictionaryGetValue(plist, CFSTR(kSecTrustInfoKey));
    if (isDictionary(info)) {
        output->_info = CFRetainSafe(info);
    }
    exceptions = CFDictionaryGetValue(plist, CFSTR(kSecTrustExceptionsKey));
    if (isArray(exceptions)) {
        output->_exceptions = CFRetainSafe(exceptions);
    }
    int32_t trustResult = -1;
    trustResultNum = CFDictionaryGetValue(plist, CFSTR(kSecTrustResultKey));
    if (isNumber(trustResultNum) && CFNumberGetValue(trustResultNum, kCFNumberSInt32Type, &trustResult) &&
        (trustResult >= 0)) {
        output->_trustResult = trustResult;
    } else {
        status = errSecParam;
    }
    if (CFDictionaryGetValue(plist, CFSTR(kSecTrustAnchorsOnlyKey)) == kCFBooleanTrue) {
        output->_anchorsOnly = true;
    } /* false is set by default */
    if (CFDictionaryGetValue(plist, CFSTR(kSecTrustKeychainsAllowedKey)) == kCFBooleanFalse) {
        output->_keychainsAllowed = false;
    } /* true is set by default */

out:
    if (errSecSuccess == status && trust) {
        *trust = output;
    }
    CFReleaseNull(policies);
    CFReleaseNull(certificates);
    return status;
}

SecTrustRef SecTrustDeserialize(CFDataRef serializedTrust, CFErrorRef *error) {
    SecTrustRef trust = NULL;
    CFPropertyListRef plist = NULL;
    OSStatus status = errSecSuccess;
    require_action_quiet(serializedTrust, out,
                         SecError(errSecParam, error, CFSTR("null serialized trust input")));
    require_quiet(plist = CFPropertyListCreateWithDERData(NULL, serializedTrust,
                                                          kCFPropertyListImmutable, NULL, error), out);
    require_noerr_action_quiet(status = SecTrustCreateFromPlist(plist, &trust), out,
                               SecError(status, error, CFSTR("unable to create trust ref")));

out:
    CFReleaseNull(plist);
    return trust;
}
