/*
 * Copyright (c) 2006-2010 Apple Inc. All Rights Reserved.
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
 * Created by Michael Brouwer on 10/17/06.
 */

#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePath.h>
#include <Security/SecFramework.h>
#include <Security/SecPolicyInternal.h>
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
#include <pthread.h>
#include <MacErrors.h>
#include "SecRSAKey.h"
#include <libDER/oids.h>
#include <security_utilities/debugging.h>
#include <Security/SecInternal.h>
#include <ipc/securityd_client.h>
#include "securityd_server.h"

CFStringRef kSecTrustInfoExtendedValidationKey = CFSTR("ExtendedValidation");
CFStringRef kSecTrustInfoCompanyNameKey = CFSTR("CompanyName");
CFStringRef kSecTrustInfoRevocationKey = CFSTR("Revocation");
CFStringRef kSecTrustInfoRevocationValidUntilKey =
    CFSTR("RevocationValidUntil");

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
	CFDateRef				_verifyDate;
	SecCertificatePathRef	_chain;
	SecKeyRef				_publicKey;
	CFArrayRef              _details;
	CFDictionaryRef         _info;
	CFArrayRef              _exceptions;

    /* If true we don't trust any anchors other than the ones in _anchors. */
    bool                    _anchorsOnly;
};

/* CFRuntime regsitration data. */
static pthread_once_t kSecTrustRegisterClass = PTHREAD_ONCE_INIT;
static CFTypeID kSecTrustTypeID = _kCFRuntimeNotATypeID;

/* Forward declartions of static functions. */
static CFStringRef SecTrustDescribe(CFTypeRef cf);
static void SecTrustDestroy(CFTypeRef cf);

/* Static functions. */
static CFStringRef SecTrustDescribe(CFTypeRef cf) {
    SecTrustRef certificate = (SecTrustRef)cf;
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR("<SecTrustRef: %p>"), certificate);
}

static void SecTrustDestroy(CFTypeRef cf) {
    SecTrustRef trust = (SecTrustRef)cf;
	CFReleaseSafe(trust->_certificates);
	CFReleaseSafe(trust->_policies);
	CFReleaseSafe(trust->_verifyDate);
	CFReleaseSafe(trust->_anchors);
	CFReleaseSafe(trust->_chain);
	CFReleaseSafe(trust->_publicKey);
	CFReleaseSafe(trust->_details);
	CFReleaseSafe(trust->_info);
    CFReleaseSafe(trust->_exceptions);
}

static void SecTrustRegisterClass(void) {
	static const CFRuntimeClass kSecTrustClass = {
		0,												/* version */
        "SecTrust",                                     /* class name */
		NULL,											/* init */
		NULL,											/* copy */
		SecTrustDestroy,                                /* dealloc */
		NULL,                                           /* equal */
		NULL,											/* hash */
		NULL,											/* copyFormattingDesc */
		SecTrustDescribe                                /* copyDebugDesc */
	};

    kSecTrustTypeID = _CFRuntimeRegisterClass(&kSecTrustClass);
}

/* Public API functions. */
CFTypeID SecTrustGetTypeID(void) {
    pthread_once(&kSecTrustRegisterClass, SecTrustRegisterClass);
    return kSecTrustTypeID;
}

OSStatus SecTrustCreateWithCertificates(CFTypeRef certificates,
    CFTypeRef policies, SecTrustRef *trustRef) {
    OSStatus status = errSecParam;
    CFAllocatorRef allocator = kCFAllocatorDefault;
    CFArrayRef l_certs = NULL, l_policies = NULL;
    SecTrustRef result = NULL;

	check(certificates);
	check(trustRef);
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
	} else if (CFGetTypeID(policies) == CFArrayGetTypeID()) {
		l_policies = CFArrayCreateCopy(allocator, policies);
	} else if (CFGetTypeID(policies) == SecPolicyGetTypeID()) {
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
    status = noErr;

errOut:
    if (status) {
        CFReleaseSafe(result);
        CFReleaseSafe(l_certs);
        CFReleaseSafe(l_policies);
    } else {
        result->_certificates = l_certs;
        result->_policies = l_policies;
        *trustRef = result;
    }
    return status;
}

OSStatus SecTrustSetAnchorCertificatesOnly(SecTrustRef trust,
    Boolean anchorCertificatesOnly) {
    check(trust);
    trust->_anchorsOnly = anchorCertificatesOnly;

	/* FIXME changing this options should (potentially) invalidate the chain. */
    return noErr;
}

OSStatus SecTrustSetAnchorCertificates(SecTrustRef trust,
    CFArrayRef anchorCertificates) {
    check(trust);
    check(anchorCertificates);
	CFRetain(anchorCertificates);
    if (trust->_anchors)
        CFRelease(trust->_anchors);
    trust->_anchors = anchorCertificates;
    trust->_anchorsOnly = true;

	/* FIXME changing the anchor set should invalidate the chain. */
    return noErr;
}

OSStatus SecTrustSetVerifyDate(SecTrustRef trust, CFDateRef verifyDate) {
    check(trust);
    check(verifyDate);
    CFRetain(verifyDate);
    if (trust->_verifyDate)
        CFRelease(trust->_verifyDate);
    trust->_verifyDate = verifyDate;

	/* FIXME changing the verifydate should invalidate the chain. */
    return noErr;
}

CFAbsoluteTime SecTrustGetVerifyTime(SecTrustRef trust) {
    CFAbsoluteTime verifyTime;
    if (trust->_verifyDate) {
        verifyTime = CFDateGetAbsoluteTime(trust->_verifyDate);
    } else {
        verifyTime = CFAbsoluteTimeGetCurrent();
		/* Record the verifyDate we ended up using. */
        trust->_verifyDate = CFDateCreate(CFGetAllocator(trust), verifyTime);
    }

    return verifyTime;
}

CFArrayRef SecTrustGetDetails(SecTrustRef trust) {
    return trust->_details;
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

OSStatus SecTrustEvaluate(SecTrustRef trust, SecTrustResultType *result) {
    CFMutableDictionaryRef args_in = NULL;
    CFTypeRef args_out = NULL;
    OSStatus status;

    check(trust);
    check(result);
    CFReleaseNull(trust->_chain);
    CFReleaseNull(trust->_details);
    CFReleaseNull(trust->_info);

    args_in = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    /* Translate certificates to CFDataRefs. */
    CFArrayRef certificates = SecCertificateArrayCopyDataArray(trust->_certificates);
    CFDictionaryAddValue(args_in, kSecTrustCertificatesKey, certificates);
    CFRelease(certificates);

    if (trust->_anchors) {
        /* Translate anchors to CFDataRefs. */
        CFArrayRef anchors = SecCertificateArrayCopyDataArray(trust->_anchors);
        CFDictionaryAddValue(args_in, kSecTrustAnchorsKey, anchors);
        CFRelease(anchors);
    }
    if (trust->_anchorsOnly)
        CFDictionaryAddValue(args_in, kSecTrustAnchorsOnlyKey, kCFBooleanTrue);

    /* Translate policies to plist capable CFTypeRefs. */
    CFArrayRef serializedPolicies = SecPolicyArraySerialize(trust->_policies);
    CFDictionaryAddValue(args_in, kSecTrustPoliciesKey, serializedPolicies);
    CFRelease(serializedPolicies);

    /* Ensure trust->_verifyDate is initialized. */
    SecTrustGetVerifyTime(trust);
    CFDictionaryAddValue(args_in, kSecTrustVerifyDateKey, trust->_verifyDate);

    /* @@@ Consider an optimization where we keep a side dictionary with the SHA1 hash of ever SecCertificateRef we send, so we only send potential duplicates once, and have the server respond with either just the SHA1 hash of a certificate, or the complete certificate in the response depending on whether the client already sent it, so we don't send back certificates to the client it already has. */
    //status = SECURITYD(sec_trust_evaluate, args_in, &args_out);
    if (gSecurityd) {
        status = gSecurityd->sec_trust_evaluate(args_in, &args_out);
    } else {
        status = ServerCommandSendReceive(sec_trust_evaluate_id, 
            args_in, &args_out);
    }
    if (status == errSecNotAvailable && CFArrayGetCount(trust->_certificates)) {
        /* We failed to talk to securityd.  The only time this should
           happen is when we are running prior to launchd enabling
           registration of services.  This currently happens when we
           are running from the ramdisk.   To make ASR happy we initialize
           _chain and return success with a failure as the trustResult, to
           make it seem like we did a cert evaluation, so ASR can extract
           the public key from the leaf. */
        trust->_chain = SecCertificatePathCreate(NULL,
            (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0));
        if (result)
            *result = kSecTrustResultOtherError;
        status = noErr;
        goto errOut;
    }

	require_quiet(args_out, errOut);

	CFArrayRef details = (CFArrayRef)CFDictionaryGetValue(args_out, kSecTrustDetailsKey);
    if (details) {
        require(CFGetTypeID(details) == CFArrayGetTypeID(), errOut);
        CFRetain(details);
        trust->_details = details;
    }

	CFDictionaryRef info = (CFDictionaryRef)CFDictionaryGetValue(args_out, kSecTrustInfoKey);
    if (info) {
        require(CFGetTypeID(info) == CFDictionaryGetTypeID(), errOut);
        CFRetain(info);
        trust->_info = info;
    }

	CFArrayRef chainArray = (CFArrayRef)CFDictionaryGetValue(args_out, kSecTrustChainKey);
    require(chainArray && CFGetTypeID(chainArray) == CFArrayGetTypeID(), errOut);
    trust->_chain = SecCertificatePathCreateWithArray(chainArray);
    require(trust->_chain, errOut);

    CFNumberRef cfResult = (CFNumberRef)CFDictionaryGetValue(args_out, kSecTrustResultKey);
    require(cfResult && CFGetTypeID(cfResult) == CFNumberGetTypeID(), errOut);
    if (result) {
        SInt32 trustResult;
        CFNumberGetValue(cfResult, kCFNumberSInt32Type, &trustResult);
        if (trustResult == kSecTrustResultUnspecified) {
            /* If leaf is in exceptions -> proceed, otherwise unspecified. */
            if (SecTrustGetExceptionForCertificateAtIndex(trust, 0))
                trustResult = kSecTrustResultProceed;
        } else if (trustResult == kSecTrustResultRecoverableTrustFailure) {
            /* If we have exceptions get details and match to exceptions. */
            CFIndex pathLength = CFArrayGetCount(details);
            struct SecTrustCheckExceptionContext context = {};
            CFIndex ix;
            for (ix = 0; ix < pathLength; ++ix) {
                CFDictionaryRef detail = (CFDictionaryRef)CFArrayGetValueAtIndex(details, ix);
                if ((ix == 0) && CFDictionaryContainsKey(detail, kSecPolicyCheckBlackListedLeaf))
                    trustResult = kSecTrustResultFatalTrustFailure;
                context.exception = SecTrustGetExceptionForCertificateAtIndex(trust, ix);
                CFDictionaryApplyFunction(detail, SecTrustCheckException, &context);
                if (context.exceptionNotFound) {
                    break;
                }
            }

            if (!context.exceptionNotFound)
                trustResult = kSecTrustResultProceed;
        }

        *result = trustResult;
	}

errOut:
    CFReleaseSafe(args_out);
    CFReleaseSafe(args_in);
	return status;
}


SecKeyRef SecTrustCopyPublicKey(SecTrustRef trust) {
	if (!trust->_publicKey && trust->_chain) {
		trust->_publicKey = SecCertificatePathCopyPublicKeyAtIndex(
		trust->_chain, 0);
	}
	if (trust->_publicKey)
		CFRetain(trust->_publicKey);

	return trust->_publicKey;
}

CFIndex SecTrustGetCertificateCount(SecTrustRef trust) {
	return trust->_chain ? SecCertificatePathGetCount(trust->_chain) : 1;
}

SecCertificateRef SecTrustGetCertificateAtIndex(SecTrustRef trust,
    CFIndex ix) {
    if (trust->_chain)
        return SecCertificatePathGetCertificateAtIndex(trust->_chain, ix);
    else if (ix == 0)
        return (SecCertificateRef)CFArrayGetValueAtIndex(trust->_certificates, 0);
    else
        return NULL;
}

CFDictionaryRef SecTrustCopyInfo(SecTrustRef trust) {
    CFDictionaryRef info = trust->_info;
    if (info)
        CFRetain(info);
    return info;
}

CFDataRef SecTrustCopyExceptions(SecTrustRef trust) {

    CFArrayRef details = SecTrustGetDetails(trust);
    CFIndex pathLength = CFArrayGetCount(details);
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
    for (ix = pathLength - 1; ix > 0; --ix) {
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
    CFArrayRef exceptions;
    exceptions = CFPropertyListCreateFromXMLData(kCFAllocatorDefault, encodedExceptions, kCFPropertyListImmutable, NULL);
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
   OS: The (root|intermidate|leaf) certificate (expired on 'date'|is not valid until 'date')
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
        || CFEqual(key, kSecPolicyCheckAnchorSHA1)) {
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
    appendProperty(properties, kSecPropertyTypeError, NULL, NULL,
                   localizedError);
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
        CFReleaseNull(properties);
    }

    return properties;
}


#if 0
#pragma mark -
#pragma mark SecTrustNode
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

/* CFRuntime regsitration data. */
static pthread_once_t kSecTrustNodeRegisterClass = PTHREAD_ONCE_INIT;
static CFTypeID kSecTrustNodeTypeID = _kCFRuntimeNotATypeID;

/* Forward declartions of static functions. */
static CFStringRef SecTrustNodeDescribe(CFTypeRef cf);
static void SecTrustNodeDestroy(CFTypeRef cf);

/* Static functions. */
static CFStringRef SecTrustNodeDescribe(CFTypeRef cf) {
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

static void SecTrustNodeRegisterClass(void) {
	static const CFRuntimeClass kSecTrustNodeClass = {
		0,												/* version */
        "SecTrustNode",                                 /* class name */
		NULL,											/* init */
		NULL,											/* copy */
		SecTrustNodeDestroy,                            /* dealloc */
		NULL,                                           /* equal */
		NULL,											/* hash */
		NULL,											/* copyFormattingDesc */
		SecTrustNodeDescribe                            /* copyDebugDesc */
	};

    kSecTrustNodeTypeID = _CFRuntimeRegisterClass(&kSecTrustNodeClass);
}

/* SecTrustNode API functions. */
CFTypeID SecTrustNodeGetTypeID(void) {
    pthread_once(&kSecTrustNodeRegisterClass, SecTrustNodeRegisterClass);
    return kSecTrustNodeTypeID;
}

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

    return noErr;
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
