/*
 * Copyright (c) 2007-2010 Apple Inc. All Rights Reserved.
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
 * SecPolicy.c - Implementation of various X.509 certificate trust policies
 */

#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <AssertMacros.h>
#include <pthread.h>
#include <security_utilities/debugging.h>
#include <Security/SecInternal.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFTimeZone.h>
#include <Security/SecCertificateInternal.h>
#include <libDER/oids.h>

/********************************************************
 **************** SecPolicy Constants *******************
 ********************************************************/
#pragma mark -
#pragma mark SecPolicy Constants

/********************************************************
 ************** Unverified Leaf Checks ******************
 ********************************************************/
CFStringRef kSecPolicyCheckSSLHostname = CFSTR("SSLHostname");

CFStringRef kSecPolicyCheckEmail = CFSTR("email");

/* Checks that the issuer of the leaf has exactly one Common Name and that it
   matches the specified string. */
CFStringRef kSecPolicyCheckIssuerCommonName = CFSTR("IssuerCommonName");

/* Checks that the leaf has exactly one Common Name and that it
   matches the specified string. */
CFStringRef kSecPolicyCheckSubjectCommonName = CFSTR("SubjectCommonName");

/* Checks that the leaf has exactly one Common Name and that it has the
   specified string as a prefix. */
CFStringRef kSecPolicyCheckSubjectCommonNamePrefix = CFSTR("SubjectCommonNamePrefix");

/* Checks that the leaf has exactly one Common Name and that it
   matches the specified "<string>" or "TEST <string> TEST". */
CFStringRef kSecPolicyCheckSubjectCommonNameTEST = CFSTR("SubjectCommonNameTEST");

/* Checks that the leaf has exactly one Organzation and that it
   matches the specified string. */
CFStringRef kSecPolicyCheckSubjectOrganization = CFSTR("SubjectOrganization");

/* Check that the leaf is not valid before the specified date (or verifyDate
   if none is provided?). */
CFStringRef kSecPolicyCheckNotValidBefore = CFSTR("NotValidBefore");

CFStringRef kSecPolicyCheckEAPTrustedServerNames = CFSTR("EAPTrustedServerNames");

#if 0
/* Check for basic constraints on leaf to be valid.  (rfc5280 check) */
CFStringRef kSecPolicyCheckLeafBasicConstraints = CFSTR("LeafBasicContraints");
#endif

/********************************************************
 *********** Unverified Intermediate Checks *************
 ********************************************************/
CFStringRef kSecPolicyCheckKeyUsage = CFSTR("KeyUsage"); /* (rfc5280 check) */
CFStringRef kSecPolicyCheckExtendedKeyUsage = CFSTR("ExtendedKeyUsage"); /* (rfc5280 check) */
CFStringRef kSecPolicyCheckBasicContraints = CFSTR("BasicContraints"); /* (rfc5280 check) */
CFStringRef kSecPolicyCheckQualifiedCertStatements =
    CFSTR("QualifiedCertStatements"); /* (rfc5280 check) */

/********************************************************
 ************** Unverified Anchor Checks ****************
 ********************************************************/
CFStringRef kSecPolicyCheckAnchorSHA1 = CFSTR("AnchorSHA1");

/* Fake key for isAnchored check. */
CFStringRef kSecPolicyCheckAnchorTrusted = CFSTR("AnchorTrusted");

/********************************************************
 *********** Unverified Certificate Checks **************
 ********************************************************/
/* Unverified Certificate Checks (any of the above) */
CFStringRef kSecPolicyCheckNonEmptySubject = CFSTR("NonEmptySubject");
CFStringRef kSecPolicyCheckIdLinkage = CFSTR("IdLinkage"); /* (rfc5280 check) */
#if 0
CFStringRef kSecPolicyCheckValidityStarted = CFSTR("ValidStarted");
CFStringRef kSecPolicyCheckValidityExpired = CFSTR("ValidExpired");
#else
CFStringRef kSecPolicyCheckValidIntermediates = CFSTR("ValidIntermediates");
CFStringRef kSecPolicyCheckValidLeaf = CFSTR("ValidLeaf");
CFStringRef kSecPolicyCheckValidRoot = CFSTR("ValidRoot");
#endif


/********************************************************
 **************** Verified Path Checks ******************
 ********************************************************/
/* (rfc5280 check) Ideally we should dynamically track all the extensions
   we processed for each certificate and fail this test if any critical
   extensions remain. */
CFStringRef kSecPolicyCheckCriticalExtensions = CFSTR("CriticalExtensions");

/* Check that the certificate chain length matches the specificed CFNumberRef
   length. */
CFStringRef kSecPolicyCheckChainLength = CFSTR("ChainLength");

/* (rfc5280 check) */
CFStringRef kSecPolicyCheckBasicCertificateProcessing =
    CFSTR("BasicCertificateProcessing");

/********************************************************
 ******************* Feature toggles ********************
 ********************************************************/

/* Check revocation if specified. */
CFStringRef kSecPolicyCheckExtendedValidation = CFSTR("ExtendedValidation");
CFStringRef kSecPolicyCheckRevocation = CFSTR("Revocation");

/* If present and true, we never go out to the network for anything
   (OCSP, CRL or CA Issuer checking) but just used cached data instead. */
CFStringRef kSecPolicyCheckNoNetworkAccess = CFSTR("NoNetworkAccess");

/* Hack to quickly blacklist certain certs. */
CFStringRef kSecPolicyCheckBlackListedLeaf = CFSTR("BlackListedLeaf");
CFStringRef kSecPolicyCheckBlackListedKey = CFSTR("BlackListedKey");

CFStringRef kSecPolicyCheckLeafMarkerOid = CFSTR("CheckLeafMarkerOid");
CFStringRef kSecPolicyCheckIntermediateMarkerOid = CFSTR("CheckIntermediateMarkerOid");

/* Policy names. */
static CFStringRef kSecPolicyOIDBasicX509 = CFSTR("basicX509");
static CFStringRef kSecPolicyOIDSSLServer = CFSTR("sslServer");
static CFStringRef kSecPolicyOIDSSLClient = CFSTR("sslClient");
static CFStringRef kSecPolicyOIDiPhoneActivation = CFSTR("iPhoneActivation");
static CFStringRef kSecPolicyOIDiPhoneDeviceCertificate =
    CFSTR("iPhoneDeviceCertificate");
static CFStringRef kSecPolicyOIDFactoryDeviceCertificate =
    CFSTR("FactoryDeviceCertificate");
static CFStringRef kSecPolicyOIDiAP = CFSTR("iAP");
static CFStringRef kSecPolicyOIDiTunesStoreURLBag = CFSTR("iTunesStoreURLBag");
static CFStringRef kSecPolicyEAPServer = CFSTR("eapServer");
static CFStringRef kSecPolicyEAPClient = CFSTR("eapClient");
static CFStringRef kSecPolicyOIDIPSecServer = CFSTR("ipsecServer");
static CFStringRef kSecPolicyOIDIPSecClient = CFSTR("ipsecClient");
static CFStringRef kSecPolicyOIDiPhoneApplicationSigning =
    CFSTR("iPhoneApplicationSigning");
static CFStringRef kSecPolicyOIDiPhoneProfileApplicationSigning =
    CFSTR("iPhoneProfileApplicationSigning");
static CFStringRef kSecPolicyOIDiPhoneProvisioningProfileSigning =
    CFSTR("iPhoneProvisioningProfileSigning");
static CFStringRef kSecPolicyOIDRevocation = CFSTR("revocation");
static CFStringRef kSecPolicyOIDOCSPSigner = CFSTR("OCSPSigner");
static CFStringRef kSecPolicyOIDSMIME = CFSTR("SMIME");
static CFStringRef kSecPolicyOIDCodeSigning = CFSTR("CodeSigning");
static CFStringRef kSecPolicyOIDLockdownPairing = CFSTR("LockdownPairing");
static CFStringRef kSecPolicyOIDURLBag = CFSTR("URLBag");
static CFStringRef kSecPolicyOIDOTATasking = CFSTR("OTATasking");
static CFStringRef kSecPolicyOIDMobileAsset = CFSTR("MobileAsset");
static CFStringRef kSecPolicyOIDAppleIDAuthority = CFSTR("AppleIDAuthority");

/* Policies will now change to multiple categories of checks.

    IDEA Store partial valid policy tree in each chain?  Result tree pruning might make this not feasible unless you can pretend to prune the tree without actually deleting nodes and somehow still have shable nodes with parent chains (this assumes that chains will be built as cached things from the root down), and we can build something equivalent to rfc5280 in a tree of certs.  So we need to maintain a cache of leaf->chain with this certificate as any_ca_cert->tree.  Revocation status caching can be done in this cache as well, so maybe the cache should be in sqlite3, or at least written there before exit and querying of the cache could be done first on the in core (possibly CF or custom tree like structure) and secondarly on the sqlite3 backed store.  We should choose the lowest memory footprint solution in my mind, while choosing a sqlite3 cache size that gives us a resonable io usage pattern.
    NOTE no certificate can be an intermediate unless it is X.509V3 and it has a basicConstraints extension with isCA set to true.  This can be used to classify certs for caching purposes.

    kSecPolicySLCheck   Static Subscriber Certificate Checks
    kSecPolicySICheck   Static Subsidiary CA Checks
    kSecPolicySACheck   Static Anchor Checks

    kSecPolicyDLCheck   Dynamic Subscriber Certificate Checks
    kSecPolicyDICheck   Dynamic Subsidiary CA Checks
    kSecPolicyDACheck   Dynamic Anchor Checks ? not yet needed other than to
    possibly exclude in a exception template (but those should still be per
    certificate --- i.o.w. exceptions (or a database backed multiple role/user
    trust store of some sort) and policies are 2 different things and this
    text is about policies.

   All static checks are only allowed to consider the certificate in isolation,
   just given the position in the chain or the cert (leaf, intermidate, root).
   dynamic checks can make determinations about the chain as a whole.

   Static Subscriber Certificate Checks will be done up front before the
   chainbuilder is even instantiated.  If they fail and details aren't required
   by the client (if no exceptions were present for this certificate) we could
   short circuit fail the evaluation.
   IDEA: These checks can dynamically add new checks...[needs work]
   ALTERNATIVE: A policy can have one or more sub-policies.  Each sub-policy will be evaluated only after the parent policy succeeds.  Subpolicies can be either required (making the parent policy fail) or optional making the parent policy succeed, but allowing the chainbuilder to continue building chains after an optional subpolicy failure in search of a chain for which the subpolicy also succeeded.  Subpolicies can be dynamically added to the policy evaluation context tree (a tree with a node for every node in the certificate path. This tree however is from the leaf up stored in the SecCertificatePathRef objects themselves possibly - requiring a separate shared subtree of nodes for the underlying certificate state tree.) by a parent policy at any stage, since the subpolicy evaluation only starts after 
   will have a key in the info (or even details and make info client side generated from info to indicate the success or failure of optional subpolicies) tree the value of which is an
   equivalent subtree from that level down.  So SSL has EV as a subpolicy, but
   EV dynamically enables the ocsp or crl or dcrl or any combination thereof subpolicies.

   Static Subsidiary CA Checks will be used by the chain-builder to choose the
   best parents to evaluate first. This feature is currently already implemented
   but with a hardcoded is_valid(verifyTime) check. Instead we will evaluate all
   Static Subsidiary CA Checks.  The results of these checks for purposes of
   generating details could be cached in the SecCertificatePathRefs themselves, or we can short circuit fail and recalc details on demand later.

   Static Anchor Checks can do things like populate the chainbuilder level context value of the initial_valid_policy_tree with a particular anchors list of ev policies it represents or modify inputs to the policy itself. 

   Dynamic Subscriber Certificate Checks These can do things like check for EV policy conformance based on the valid_policy_tree at the end of the certificate evaluation, or based on things like the pathlen, etc. in the chain validation context.

   Dynamic Subsidiary CA Checks might not be needed to have custom
   implementations, since they are all done as part of the rfc5280 checks now.
   This assumes that checks like issuer common name includes 'foo' are
   implmented as Static Subscriber Certificate Checks instead.

   Dynamic Anchor Checks might include EV type checks or chain validation context seeding as well, allthough we might be able to do them as static checks to seed the chain validation context instead.


   Questions/Notes: Do we need to dynamically add new policies?  If policy static checks fail and policy is optional we don't even run policy dynamic checks nor do we compute subpolicy values.  So if the static check of the leaf for EV fails we skip the rest of the EV style checks and instead don't run the revocation subpolicy of the ev policy.

   If an optional subpolicy s_p has a required subpolicy r_s_p.  Then success of s_p will cause the entire chain evaluation to fail if r_s_p fails.

   All policies static revocation checks are run at the appropriate phase in the evaluation.  static leaf checks are done before chainbuilding even starts.  static intermediate checks are done in the chainbuilder for each cadidate parent certificate.  If all policies pass we check the signatures. We reject the whole chain if that step fails. Otherwise we add the path to builder->candidatePaths. If the top level policy or a required subpolicy or a required subpolicy of a successful subpolicy fails we stick the chain at the end of the expiredPaths, if one of the optional subpolicies fail, we stick the chain at the start of expiredPaths so it's considered first after all real candidatePaths have been processed.
   
   Static revocation policy checks could check the passed in ocspresponses or even the local cache, though the latter is probably best left for the dynamic phase.

   The same rules that apply above to the adding paths to candidatePaths v/s expiredPaths apply to dynamicpolicy checks, except that we don't remember failures anymore, we reject them.
   
   We need to remember the best successful chain we find, where best is defined by: satisfies as many optional policies as possible.
   
   Chain building ends when either we find a chain that matches all optional and required policies, or we run out of chains to build.  Another case is if we run out of candiate paths but we already have a chain that matches at least the top level and required subpolicies.   In that case we don't even consider any expiredPaths.  Example: we find a valid SSL chain (top level policy), but no partial chain we constructed satisfied the static checks of the ev subpolicy, or the required revocation sub-subpolicy of the ev policy.

   In order for this to work well with exceptions on subpolicies, we'd need to move the validation of exceptions to the server, something we'd do anyway if we had full on truststore.  In this case exceptions would be live in the failure callback for a trust check.

Example sectrust operation in psuedocode:
 */
#if 0
{
    new builder(verifyTime, certificates, anchors, anchorsOnly, policies);
    chain = builder.subscriber_only_chain;
    foreach (policy in policies{kSecPolicySLCheck}) {
        foreach(check in policy)
            SecPolicyRunCheck(builder, chain, check, details);
        foreach (subpolicy in policy) {
            check_policy(builder, chain, subpolicy, details{subpolicy.name})
        }
        propagate_subpolicy_results(builder, chain, details);
    }
    while (chain = builder.next) {
        for (depth = 0; p_d = policies.at_depth(depth),
            d_p_d = dynamic_policies.at_depth(depth), p_d || d_p_d; ++depth)
        {
            /* Modify SecPathBuilderIsPartial() to
               run builder_check(buildier, policies, kSecPolicySICheck) instead
               of SecCertificateIsValid.  Also rename considerExpired to
               considerSIFailures.
               */
            foreach (policy in p_d) {
                check_policy(builder, chain, policy, kSecPolicySICheck, depth);
            }
            /* Recalculate since the static checks might have added new dynamic
               policies. */
            d_p_d = dynamic_policies.at_depth(depth);
            foreach (policy in d_p_d) {
                check_policy(builder, chain, policy, kSecPolicySICheck, depth);
            }
            if (chain.is_anchored) {
                foreach (policy in p_d) {
                    check_policy(builder, chain, policy, kSecPolicySACheck, depth);
                }
                foreach (policy in d_p_d) {
                    check_policy(builder, chain, policy, kSecPolicySACheck, depth);
                }
                foreach (policy in p_d) {
                    check_policy(builder, chain, policy, kSecPolicyDACheck, depth);
                }
                foreach (policy in d_p_d) {
                    check_policy(builder, chain, policy, kSecPolicyDACheck, depth);
                }
            }
            foreach (policy in policies) {
                check_policy(builder, chain, policy, kSecPolicySACheck, depth);
                check_policy(builder, chain, policy, kSecPolicyDACheck, depth);
            }
            foreach (policy in policies{kSecPolicySDCheck}) {
        }
    }
}

check_policy(builder, chain, policy, check_class, details, depth) {
    if (depth == 0) {
        foreach(check in policy{check_class}) {
            SecPolicyRunCheck(builder, chain, check, details);
        }
    } else {
        depth--;
        foreach (subpolicy in policy) {
            if (!check_policy(builder, chain, subpolicy, check_class,
            details{subpolicy.name}) && subpolicy.is_required, depth)
                secpvcsetresult()
        }
    }
    propagate_subpolicy_results(builder, chain, details);
}

#endif



#define kSecPolicySHA1Size 20
static const UInt8 kAppleCASHA1[kSecPolicySHA1Size] = {
    0x61, 0x1E, 0x5B, 0x66, 0x2C, 0x59, 0x3A, 0x08, 0xFF, 0x58,
    0xD1, 0x4A, 0xE2, 0x24, 0x52, 0xD1, 0x98, 0xDF, 0x6C, 0x60
};

static const UInt8 kITMSCASHA1[kSecPolicySHA1Size] = {
    0x1D, 0x33, 0x42, 0x46, 0x8B, 0x10, 0xBD, 0xE6, 0x45, 0xCE, 
    0x44, 0x6E, 0xBB, 0xE8, 0xF5, 0x03, 0x5D, 0xF8, 0x32, 0x22
};

static const UInt8 kFactoryDeviceCASHA1[kSecPolicySHA1Size] = {
  0xef, 0x68, 0x73, 0x17, 0xa4, 0xf8, 0xf9, 0x4b, 0x7b, 0x21, 
  0xe2, 0x2f, 0x09, 0x8f, 0xfd, 0x6a, 0xae, 0xc0, 0x0d, 0x63
};

#pragma mark -
#pragma mark SecPolicy
/********************************************************
 ****************** SecPolicy object ********************
 ********************************************************/

/* CFRuntime regsitration data. */
static pthread_once_t kSecPolicyRegisterClass = PTHREAD_ONCE_INIT;
static CFTypeID kSecPolicyTypeID = _kCFRuntimeNotATypeID;

static void SecPolicyDestroy(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;
	CFRelease(policy->_oid);
	CFRelease(policy->_options);
}

static Boolean SecPolicyEqual(CFTypeRef cf1, CFTypeRef cf2) {
	SecPolicyRef policy1 = (SecPolicyRef) cf1;
	SecPolicyRef policy2 = (SecPolicyRef) cf2;
	return CFEqual(policy1->_oid, policy2->_oid) &&
		CFEqual(policy1->_options, policy2->_options);
}

static CFHashCode SecPolicyHash(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;

	return CFHash(policy->_oid) + CFHash(policy->_options);
}

static CFStringRef SecPolicyDescribe(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef typeStr = CFCopyTypeIDDescription(CFGetTypeID(cf));
    CFStringAppendFormat(desc, NULL,
        CFSTR("<%@ %@: oid: %@ options %@"), typeStr,
        policy->_oid, policy->_options);
    CFRelease(typeStr);
    CFStringAppend(desc, CFSTR(" >"));

    return desc;
}

static void SecPolicyRegisterClass(void) {
	static const CFRuntimeClass kSecPolicyClass = {
		0,										/* version */
        "SecPolicy",							/* class name */
		NULL,									/* init */
		NULL,									/* copy */
		SecPolicyDestroy,						/* dealloc */
		SecPolicyEqual,							/* equal */
		SecPolicyHash,							/* hash */
		NULL,									/* copyFormattingDesc */
		SecPolicyDescribe						/* copyDebugDesc */
	};

    kSecPolicyTypeID = _CFRuntimeRegisterClass(&kSecPolicyClass);
}

/* SecPolicy API functions. */
CFTypeID SecPolicyGetTypeID(void) {
    pthread_once(&kSecPolicyRegisterClass, SecPolicyRegisterClass);
    return kSecPolicyTypeID;
}

/* AUDIT[securityd](done):
   oid (ok) is a caller providied string, only it's cf type has been checked.
   options is a caller provided dictionary, only its cf type has
   been checked.
 */
SecPolicyRef SecPolicyCreate(CFStringRef oid, CFDictionaryRef options) {
	SecPolicyRef result = NULL;

	require(oid, errOut);
	require(options, errOut);
    require(result =
		(SecPolicyRef)_CFRuntimeCreateInstance(kCFAllocatorDefault,
		SecPolicyGetTypeID(),
		sizeof(struct __SecPolicy) - sizeof(CFRuntimeBase), 0), errOut);

	CFRetain(oid);
	result->_oid = oid;
	CFRetain(options);
	result->_options = options;

errOut:
    return result;
}

static CFArrayRef SecPolicyCopyArray(SecPolicyRef policy) {
    const void *values[] = { policy->_oid, policy->_options };
    return CFArrayCreate(kCFAllocatorDefault, values, 2, &kCFTypeArrayCallBacks);
}

static void serializePolicy(const void *value, void *context) {
    CFTypeRef serializedPolicy = SecPolicyCopyArray((SecPolicyRef)value);
    CFArrayAppendValue((CFMutableArrayRef)context, serializedPolicy);
    CFRelease(serializedPolicy);
}

CFArrayRef SecPolicyArraySerialize(CFArrayRef policies) {
    CFMutableArrayRef result = NULL;
    require_quiet(policies && CFGetTypeID(policies) == CFArrayGetTypeID(), errOut);
    CFIndex count = CFArrayGetCount(policies);
    result = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks);
    CFRange all_policies = { 0, count };
    CFArrayApplyFunction(policies, all_policies, serializePolicy, result);
errOut:
    return result;
}

static void add_element(CFMutableDictionaryRef options, CFStringRef key,
    CFTypeRef value) {
    CFTypeRef old_value = CFDictionaryGetValue(options, key);
    if (old_value) {
        CFMutableArrayRef array;
        if (CFGetTypeID(old_value) == CFArrayGetTypeID()) {
            array = (CFMutableArrayRef)old_value;
        } else {
            array = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                       &kCFTypeArrayCallBacks);
            CFArrayAppendValue(array, old_value);
            CFDictionarySetValue(options, key, array);
            CFRelease(array);
        }
        CFArrayAppendValue(array, value);
    } else {
        CFDictionaryAddValue(options, key, value);
    }
}

static void add_eku(CFMutableDictionaryRef options, const DERItem *ekuOid) {
    CFDataRef eku = CFDataCreate(kCFAllocatorDefault,
                                 ekuOid ? ekuOid->data : NULL,
                                 ekuOid ? ekuOid->length : 0);
    if (eku) {
        add_element(options, kSecPolicyCheckExtendedKeyUsage, eku);
        CFRelease(eku);
    }
}

static void add_ku(CFMutableDictionaryRef options, SecKeyUsage keyUsage) {
    SInt32 dku = keyUsage;
    CFNumberRef ku = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type,
        &dku);
    if (ku) {
        add_element(options, kSecPolicyCheckKeyUsage, ku);
        CFRelease(ku);
    }
}

static void add_oid(CFMutableDictionaryRef options, CFStringRef policy_key, const DERItem *oid) {
    CFDataRef oid_data = CFDataCreate(kCFAllocatorDefault,
                                 oid ? oid->data : NULL,
                                 oid ? oid->length : 0);
    if (oid_data) {
        add_element(options, policy_key, oid_data);
        CFRelease(oid_data);
    }
}

//
// Routines for adding dictionary entries for policies.
//

// X.509, but missing validity requirements.
static void SecPolicyAddBasicCertOptions(CFMutableDictionaryRef options)
{
    //CFDictionaryAddValue(options, kSecPolicyCheckBasicCertificateProcessing, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckCriticalExtensions, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckIdLinkage, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckBasicContraints, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckNonEmptySubject, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckQualifiedCertStatements, kCFBooleanTrue);
}

static void SecPolicyAddBasicX509Options(CFMutableDictionaryRef options)
{
    SecPolicyAddBasicCertOptions(options);
    CFDictionaryAddValue(options, kSecPolicyCheckValidIntermediates, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckValidLeaf, kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckValidRoot, kCFBooleanTrue);
}

static bool SecPolicyAddChainLengthOptions(CFMutableDictionaryRef options, CFIndex length)
{
    bool result = false;
    CFNumberRef lengthAsCF = NULL;

    require(lengthAsCF = CFNumberCreate(kCFAllocatorDefault,
                                         kCFNumberCFIndexType, &length), errOut);
    CFDictionaryAddValue(options, kSecPolicyCheckChainLength, lengthAsCF);

    result = true;

errOut:
	CFReleaseSafe(lengthAsCF);
    return result;
}

static bool SecPolicyAddAnchorSHA1Options(CFMutableDictionaryRef options,
                                          const UInt8 anchorSha1[kSecPolicySHA1Size])
{
    bool success = false;
    CFDataRef anchorData = NULL;

    require(anchorData = CFDataCreate(kCFAllocatorDefault, anchorSha1, kSecPolicySHA1Size), errOut);
    CFDictionaryAddValue(options, kSecPolicyCheckAnchorSHA1, anchorData);

    success = true;

errOut:
    CFReleaseSafe(anchorData);
    return success;
}

static bool SecPolicyAddAppleAnchorOptions(CFMutableDictionaryRef options)
{
    return SecPolicyAddAnchorSHA1Options(options, kAppleCASHA1);
}


//
// Policy Creation Functions
//
SecPolicyRef SecPolicyCreateBasicX509(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

	require(result = SecPolicyCreate(kSecPolicyOIDBasicX509, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateSSL(Boolean server, CFStringRef hostname) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
#endif

	if (hostname) {
		CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);
	}
    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,
        kCFBooleanTrue);

    /* If server and EKU ext present then EKU ext should contain one of
       CSSMOID_ServerAuth or CSSMOID_ExtendedKeyUsageAny or
       CSSMOID_NetscapeSGC or CSSMOID_MicrosoftSGC.
       else if !server and EKU ext present then EKU ext should contain one of
       CSSMOID_ClientAuth or CSSMOID_ExtendedKeyUsageAny. */

    /* We always allow certification that specify oidAnyExtendedKeyUsage. */
    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    if (server) {
        add_eku(options, &oidExtendedKeyUsageServerAuth);
        add_eku(options, &oidExtendedKeyUsageMicrosoftSGC);
        add_eku(options, &oidExtendedKeyUsageNetscapeSGC);
    } else {
        add_eku(options, &oidExtendedKeyUsageClientAuth);
    }

	require(result = SecPolicyCreate(
		server ? kSecPolicyOIDSSLServer : kSecPolicyOIDSSLClient,
		options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneActivation(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
		kCFBooleanTrue);
#endif

    /* Basic X.509 policy with the additional requirements that the chain
       length is 3, it's anchored at the AppleCA and the leaf certificate 
       has issuer "Apple iPhone Certification Authority" and
       subject "Apple iPhone Activation" for the common name. */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Certification Authority"));
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
        CFSTR("Apple iPhone Activation"));

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDiPhoneActivation, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneDeviceCertificate(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
		kCFBooleanTrue);
#endif

    /* Basic X.509 policy with the additional requirements that the chain
       length is 4, it's anchored at the AppleCA and the first intermediate 
       has the subject "Apple iPhone Device CA". */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Device CA"));

    require(SecPolicyAddChainLengthOptions(options, 4), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDiPhoneDeviceCertificate, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateFactoryDeviceCertificate(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
		kCFBooleanTrue);
#endif

    /* Basic X.509 policy with the additional requirements that the chain
       is anchored at the factory device certificate issuer. */
    require(SecPolicyAddAnchorSHA1Options(options, kFactoryDeviceCASHA1), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDFactoryDeviceCertificate, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiAP(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;
	CFTimeZoneRef tz = NULL;
	CFDateRef date = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNamePrefix,
        CFSTR("IPA_"));

	CFGregorianDate gd = {
		2006,
		5,
		31,
		0,
		0,
		0.0
	};
	require(tz = CFTimeZoneCreateWithTimeIntervalFromGMT(NULL, 0), errOut);
	CFAbsoluteTime at = CFGregorianDateGetAbsoluteTime(gd, tz);
	require(date = CFDateCreate(kCFAllocatorDefault, at), errOut);
    CFDictionaryAddValue(options, kSecPolicyCheckNotValidBefore, date);

	require(result = SecPolicyCreate(kSecPolicyOIDiAP, options),
        errOut);

errOut:
	CFReleaseSafe(date);
	CFReleaseSafe(tz);
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiTunesStoreURLBag(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;


	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

	CFDictionaryAddValue(options, kSecPolicyCheckSubjectOrganization,
		CFSTR("Apple Inc."));
	CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
		CFSTR("iTunes Store URL Bag"));

    require(SecPolicyAddChainLengthOptions(options, 2), errOut);
    require(SecPolicyAddAnchorSHA1Options(options, kITMSCASHA1), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDiTunesStoreURLBag, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateEAP(Boolean server, CFArrayRef trustedServerNames) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

#if 0
	CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
		kCFBooleanTrue);
#endif

    /* Since EAP is used to setup the network we don't want evaluation
       using this policy to access the network. */
	CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess,
		kCFBooleanTrue);
	if (trustedServerNames) {
		CFDictionaryAddValue(options, kSecPolicyCheckEAPTrustedServerNames, trustedServerNames);
	}

	require(result = SecPolicyCreate(
		server ? kSecPolicyEAPServer : kSecPolicyEAPClient,
		options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateIPSec(Boolean server, CFStringRef hostname) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

	if (hostname) {
		CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);
	}

    /* Require oidExtendedKeyUsageIPSec if Extended Keyusage Extention is
       present. */
    /* Per <rdar://problem/6843827> Cisco VPN Certificate compatibility issue.
       We don't check the EKU for IPSec certs for now.  If we do add eku
       checking back in the future, we should probably also accept the
       following EKUs:
           ipsecEndSystem   1.3.6.1.5.5.7.3.5
       and possibly even
           ipsecTunnel      1.3.6.1.5.5.7.3.6
           ipsecUser        1.3.6.1.5.5.7.3.7
     */
    //add_eku(options, NULL); /* eku extension is optional */
    //add_eku(options, &oidAnyExtendedKeyUsage);
    //add_eku(options, &oidExtendedKeyUsageIPSec);

	require(result = SecPolicyCreate(
		server ? kSecPolicyOIDIPSecServer : kSecPolicyOIDIPSecClient,
		options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneApplicationSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Basic X.509 policy with the additional requirements that the chain
       length is 3, it's anchored at the AppleCA and the leaf certificate 
       has issuer "Apple iPhone Certification Authority" and
       subject "Apple iPhone OS Application Signing" for the common name. */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Certification Authority"));
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
        CFSTR("Apple iPhone OS Application Signing"));
    
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);

	require(result = SecPolicyCreate(kSecPolicyOIDiPhoneApplicationSigning, options),
        errOut);

    /* 1.2.840.113635.100.6.1.3, non-critical: DER:05:00 - application signing */

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneProfileApplicationSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);
	CFDictionaryAddValue(options, kSecPolicyCheckValidLeaf, kCFBooleanFalse);

	require(result = SecPolicyCreate(kSecPolicyOIDiPhoneProfileApplicationSigning,
        options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateiPhoneProvisioningProfileSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    /* Basic X.509 policy with the additional requirements that the chain
       length is 3, it's anchored at the AppleCA and the leaf certificate 
       has issuer "Apple iPhone Certification Authority" and
       subject "Apple iPhone OS Provisioning Profile Signing" for the common name. */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
        CFSTR("Apple iPhone Certification Authority"));
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
        CFSTR("Apple iPhone OS Provisioning Profile Signing"));
        
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDiPhoneProvisioningProfileSigning, options),
        errOut);
        
    /* 1.2.840.113635.100.6.2.2.1, non-critical: DER:05:00 - provisioning profile */

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateOCSPSigner(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* Require id-kp-OCSPSigning extendedKeyUsage to be present, not optional. */
    add_eku(options, &oidExtendedKeyUsageOCSPSigning);

	require(result = SecPolicyCreate(kSecPolicyOIDOCSPSigner, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateRevocation(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
    /* false = ocsp, true = crl, string/url value = crl distribution point,
       array = list of multiple values for example false, true, url1, url2
       check ocsp, crl, and url1 and url2 for certs which have no extensions.
     */
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

	require(result = SecPolicyCreate(kSecPolicyOIDRevocation, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateSMIME(CFIndex smimeUsage, CFStringRef email) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* We call add_ku for each combination of bits we are willing to allow. */
    if (smimeUsage & kSecSignSMIMEUsage) {
        add_ku(options, kSecKeyUsageUnspecified);
        add_ku(options, kSecKeyUsageDigitalSignature);
        add_ku(options, kSecKeyUsageNonRepudiation);
    }
    if (smimeUsage & kSecKeyEncryptSMIMEUsage) {
        add_ku(options, kSecKeyUsageKeyEncipherment);
    }
    if (smimeUsage & kSecDataEncryptSMIMEUsage) {
        add_ku(options, kSecKeyUsageDataEncipherment);
    }
    if (smimeUsage & kSecKeyExchangeDecryptSMIMEUsage) {
        add_ku(options, kSecKeyUsageKeyAgreement | kSecKeyUsageDecipherOnly);
    }
    if (smimeUsage & kSecKeyExchangeEncryptSMIMEUsage) {
        add_ku(options, kSecKeyUsageKeyAgreement | kSecKeyUsageEncipherOnly);
    }
    if (smimeUsage & kSecKeyExchangeBothSMIMEUsage) {
        add_ku(options, kSecKeyUsageKeyAgreement | kSecKeyUsageEncipherOnly | kSecKeyUsageDecipherOnly);
    }

	if (email) {
		CFDictionaryAddValue(options, kSecPolicyCheckEmail, email);
	}

    /* To be a valid SMIME certifcate we have to have an eku extension.
     * We only accept emailProtection (and not any) to make this policy
     * effective for selection in Mail. */
    add_eku(options, &oidExtendedKeyUsageEmailProtection);

	require(result = SecPolicyCreate(kSecPolicyOIDSMIME, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateCodeSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicX509Options(options);

    /* If the keuusage extension is present we accept it having either of
       these values. */
    add_ku(options, kSecKeyUsageDigitalSignature);
    add_ku(options, kSecKeyUsageNonRepudiation);

    /* We require a extended key usage extension and we accept any or
       codesigning ekus. */
    /* TODO: Do we want to accept the apple codesigning oid as well or is
       that a separate policy? */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);

	require(result = SecPolicyCreate(kSecPolicyOIDCodeSigning, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/* Explicitly leave out empty subject/subjectaltname check */
SecPolicyRef SecPolicyCreateLockdownPairing(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);
	//CFDictionaryAddValue(options, kSecPolicyCheckBasicCertificateProcessing,
    //    kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckCriticalExtensions,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckIdLinkage,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckBasicContraints,
		kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckQualifiedCertStatements,
		kCFBooleanTrue);

	require(result = SecPolicyCreate(kSecPolicyOIDLockdownPairing, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateURLBag(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    add_eku(options, &oidExtendedKeyUsageCodeSigning);

	require(result = SecPolicyCreate(kSecPolicyOIDURLBag, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

static bool SecPolicyAddAppleCertificationAuthorityOptions(CFMutableDictionaryRef options, bool honorValidity)
{
    bool success = false;

    if (honorValidity)
        SecPolicyAddBasicX509Options(options);
    else
        SecPolicyAddBasicCertOptions(options);

#if 0
    CFDictionaryAddValue(options, kSecPolicyCheckKeyUsage,
                         kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckExtendedKeyUsage,
                         kCFBooleanTrue);
#endif

    /* Basic X.509 policy with the additional requirements that the chain
     length is 3, it's anchored at the AppleCA and the leaf certificate
     has issuer "Apple iPhone Certification Authority". */
    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple iPhone Certification Authority"));

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

    success = true;

errOut:
    return success;
}

static SecPolicyRef SecPolicyCreateAppleCertificationAuthorityPolicy(CFStringRef policyOID, CFStringRef leafName, bool honorValidity)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;

    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    require(SecPolicyAddAppleCertificationAuthorityOptions(options, honorValidity), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName, leafName);

    require(result = SecPolicyCreate(policyOID, options),
            errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}


SecPolicyRef SecPolicyCreateOTATasking(void)
{
    return SecPolicyCreateAppleCertificationAuthorityPolicy(kSecPolicyOIDOTATasking, CFSTR("OTA Task Signing"), true);
}

SecPolicyRef SecPolicyCreateMobileAsset(void)
{
    return SecPolicyCreateAppleCertificationAuthorityPolicy(kSecPolicyOIDMobileAsset, CFSTR("Asset Manifest Signing"), false);
}

SecPolicyRef SecPolicyCreateAppleIDAuthorityPolicy(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), out);

    //Leaf appears to be a SSL only cert, so policy should expand on that policy
    SecPolicyAddBasicX509Options(options);

    // Apple CA anchored
    require(SecPolicyAddAppleAnchorOptions(options), out);

    // with the addition of the existence check of an extension with "Apple ID Sharing Certificate" oid (1.2.840.113635.100.4.7)
    // NOTE: this obviously intended to have gone into Extended Key Usage, but evidence of existing certs proves the contrary.
    add_oid(options, kSecPolicyCheckLeafMarkerOid, &oidAppleExtendedKeyUsageAppleID);

    // and validate that intermediate has extension with CSSMOID_APPLE_EXTENSION_AAI_INTERMEDIATE  oid (1.2.840.113635.100.6.2.3) and goes back to the Apple Root CA.
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID);
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID2);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleIDAuthority, options), out);

out:
    CFReleaseSafe(options);
    return result;
}
