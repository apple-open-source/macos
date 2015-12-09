/*
 * Copyright (c) 2007-2015 Apple Inc. All Rights Reserved.
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
#include <utilities/debugging.h>
#include <Security/SecInternal.h>
#include <CoreFoundation/CFDictionary.h>
#include <CoreFoundation/CFNumber.h>
#include <CoreFoundation/CFRuntime.h>
#include <CoreFoundation/CFString.h>
#include <CoreFoundation/CFTimeZone.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecCertificatePriv.h>
#include <libDER/oidsPriv.h>
#include <utilities/SecCFError.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/array_size.h>
#include <ipc/securityd_client.h>
#if TARGET_OS_EMBEDDED
#include <MobileGestalt.h>
#else
#include <sys/utsname.h>
#endif

#include <utilities/SecInternalReleasePriv.h>

/********************************************************
 **************** SecPolicy Constants *******************
 ********************************************************/
// MARK: -
// MARK: SecPolicy Constants

#define SEC_CONST_DECL(k,v) const CFStringRef k = CFSTR(v);

/********************************************************
 ************** Unverified Leaf Checks ******************
 ********************************************************/
SEC_CONST_DECL (kSecPolicyCheckSSLHostname, "SSLHostname");
SEC_CONST_DECL (kSecPolicyCheckEmail, "email");

/* Checks that the issuer of the leaf has exactly one Common Name and that it
   matches the specified string. */
SEC_CONST_DECL (kSecPolicyCheckIssuerCommonName, "IssuerCommonName");

/* Checks that the leaf has exactly one Common Name and that it
   matches the specified string. */
SEC_CONST_DECL (kSecPolicyCheckSubjectCommonName, "SubjectCommonName");

/* Checks that the leaf has exactly one Common Name and that it has the
   specified string as a prefix. */
SEC_CONST_DECL (kSecPolicyCheckSubjectCommonNamePrefix, "SubjectCommonNamePrefix");

/* Checks that the leaf has exactly one Common Name and that it
   matches the specified "<string>" or "TEST <string> TEST". */
SEC_CONST_DECL (kSecPolicyCheckSubjectCommonNameTEST, "SubjectCommonNameTEST");

/* Checks that the leaf has exactly one Organization and that it
   matches the specified string. */
SEC_CONST_DECL (kSecPolicyCheckSubjectOrganization, "SubjectOrganization");

/* Checks that the leaf has exactly one Organizational Unit and that it
   matches the specified string. */
SEC_CONST_DECL (kSecPolicyCheckSubjectOrganizationalUnit, "SubjectOrganizationalUnit");

/* Check that the leaf is not valid before the specified date (or verifyDate
   if none is provided?). */
SEC_CONST_DECL (kSecPolicyCheckNotValidBefore, "NotValidBefore");

SEC_CONST_DECL (kSecPolicyCheckEAPTrustedServerNames, "EAPTrustedServerNames");

SEC_CONST_DECL (kSecPolicyCheckCertificatePolicy, "CertificatePolicy");

#if 0
/* Check for basic constraints on leaf to be valid.  (rfc5280 check) */
SEC_CONST_DECL (kSecPolicyCheckLeafBasicConstraints, "LeafBasicContraints");
#endif

/********************************************************
 *********** Unverified Intermediate Checks *************
 ********************************************************/
SEC_CONST_DECL (kSecPolicyCheckKeyUsage, "KeyUsage"); /* (rfc5280 check) */
SEC_CONST_DECL (kSecPolicyCheckExtendedKeyUsage, "ExtendedKeyUsage"); /* (rfc5280 check) */
SEC_CONST_DECL (kSecPolicyCheckBasicContraints, "BasicContraints"); /* (rfc5280 check) */
SEC_CONST_DECL (kSecPolicyCheckQualifiedCertStatements, "QualifiedCertStatements"); /* (rfc5280 check) */
SEC_CONST_DECL (kSecPolicyCheckIntermediateSPKISHA256, "IntermediateSPKISHA256")

/********************************************************
 ************** Unverified Anchor Checks ****************
 ********************************************************/
SEC_CONST_DECL (kSecPolicyCheckAnchorSHA1, "AnchorSHA1");

/* Fake key for isAnchored check. */
SEC_CONST_DECL (kSecPolicyCheckAnchorTrusted, "AnchorTrusted");

/* Anchor is one of the apple trust anchors */
SEC_CONST_DECL (kSecPolicyCheckAnchorApple, "AnchorApple");

/* options for kSecPolicyCheckAnchorApple */
SEC_CONST_DECL (kSecPolicyAppleAnchorIncludeTestRoots, "AnchorAppleTestRoots");

/********************************************************
 *********** Unverified Certificate Checks **************
 ********************************************************/
/* Unverified Certificate Checks (any of the above) */
SEC_CONST_DECL (kSecPolicyCheckNonEmptySubject, "NonEmptySubject");
SEC_CONST_DECL (kSecPolicyCheckIdLinkage, "IdLinkage") /* (rfc5280 check) */
#if 0
SEC_CONST_DECL (kSecPolicyCheckValidityStarted, "ValidStarted");
SEC_CONST_DECL (kSecPolicyCheckValidityExpired, "ValidExpired");
#else
SEC_CONST_DECL (kSecPolicyCheckValidIntermediates, "ValidIntermediates");
SEC_CONST_DECL (kSecPolicyCheckValidLeaf, "ValidLeaf");
SEC_CONST_DECL (kSecPolicyCheckValidRoot, "ValidRoot");
#endif


/********************************************************
 **************** Verified Path Checks ******************
 ********************************************************/
/* (rfc5280 check) Ideally we should dynamically track all the extensions
   we processed for each certificate and fail this test if any critical
   extensions remain. */
SEC_CONST_DECL (kSecPolicyCheckCriticalExtensions, "CriticalExtensions");

/* Check that the certificate chain length matches the specificed CFNumberRef
   length. */
SEC_CONST_DECL (kSecPolicyCheckChainLength, "ChainLength");

/* (rfc5280 check) */
SEC_CONST_DECL (kSecPolicyCheckBasicCertificateProcessing, "BasicCertificateProcessing");

/********************************************************
 ******************* Feature toggles ********************
 ********************************************************/

/* Check revocation if specified. */
SEC_CONST_DECL (kSecPolicyCheckExtendedValidation, "ExtendedValidation");
SEC_CONST_DECL (kSecPolicyCheckRevocation, "Revocation");
SEC_CONST_DECL (kSecPolicyCheckRevocationResponseRequired, "RevocationResponseRequired");

/* Check Certificate Transparency if specified. */
SEC_CONST_DECL (kSecPolicyCheckCertificateTransparency, "CertificateTransparency");

/* If present and true, we never go out to the network for anything
   (OCSP, CRL or CA Issuer checking) but just used cached data instead. */
SEC_CONST_DECL (kSecPolicyCheckNoNetworkAccess, "NoNetworkAccess");

/* Hack to quickly blacklist certain certs. */
SEC_CONST_DECL (kSecPolicyCheckBlackListedLeaf, "BlackListedLeaf");
SEC_CONST_DECL (kSecPolicyCheckGrayListedLeaf, "GrayListedLeaf");
SEC_CONST_DECL (kSecPolicyCheckGrayListedKey, "GrayListedKey");
SEC_CONST_DECL (kSecPolicyCheckBlackListedKey, "BlackListedKey");

SEC_CONST_DECL (kSecPolicyCheckLeafMarkerOid, "CheckLeafMarkerOid");
SEC_CONST_DECL (kSecPolicyCheckIntermediateMarkerOid, "CheckIntermediateMarkerOid");

/* Public policy names. */
SEC_CONST_DECL (kSecPolicyAppleX509Basic, "1.2.840.113635.100.1.2");
SEC_CONST_DECL (kSecPolicyAppleSSL, "1.2.840.113635.100.1.3");
SEC_CONST_DECL (kSecPolicyAppleSMIME, "1.2.840.113635.100.1.8");
SEC_CONST_DECL (kSecPolicyAppleEAP, "1.2.840.113635.100.1.9");
SEC_CONST_DECL (kSecPolicyAppleSWUpdateSigning, "1.2.840.113635.100.1.10");
SEC_CONST_DECL (kSecPolicyAppleIPsec, "1.2.840.113635.100.1.11");
SEC_CONST_DECL (kSecPolicyApplePKINITClient, "1.2.840.113635.100.1.14");
SEC_CONST_DECL (kSecPolicyApplePKINITServer, "1.2.840.113635.100.1.15");
SEC_CONST_DECL (kSecPolicyAppleCodeSigning, "1.2.840.113635.100.1.16");
SEC_CONST_DECL (kSecPolicyApplePackageSigning, "1.2.840.113635.100.1.17");
SEC_CONST_DECL (kSecPolicyAppleIDValidation, "1.2.840.113635.100.1.18");
SEC_CONST_DECL (kSecPolicyMacAppStoreReceipt, "1.2.840.113635.100.1.19");
SEC_CONST_DECL (kSecPolicyAppleTimeStamping, "1.2.840.113635.100.1.20");
SEC_CONST_DECL (kSecPolicyAppleRevocation, "1.2.840.113635.100.1.21");
SEC_CONST_DECL (kSecPolicyApplePassbookSigning, "1.2.840.113635.100.1.22");
SEC_CONST_DECL (kSecPolicyAppleMobileStore, "1.2.840.113635.100.1.23");
SEC_CONST_DECL (kSecPolicyAppleEscrowService, "1.2.840.113635.100.1.24");
SEC_CONST_DECL (kSecPolicyAppleProfileSigner, "1.2.840.113635.100.1.25");
SEC_CONST_DECL (kSecPolicyAppleQAProfileSigner, "1.2.840.113635.100.1.26");
SEC_CONST_DECL (kSecPolicyAppleTestMobileStore, "1.2.840.113635.100.1.27");
SEC_CONST_DECL (kSecPolicyAppleOTAPKISigner, "1.2.840.113635.100.1.28");
SEC_CONST_DECL (kSecPolicyAppleTestOTAPKISigner, "1.2.840.113635.100.1.29");
/* FIXME: this policy name should be deprecated and replaced with "kSecPolicyAppleIDValidationRecordSigning" */
SEC_CONST_DECL (kSecPolicyAppleIDValidationRecordSigningPolicy, "1.2.840.113625.100.1.30");
SEC_CONST_DECL (kSecPolicyAppleSMPEncryption, "1.2.840.113625.100.1.31");
SEC_CONST_DECL (kSecPolicyAppleTestSMPEncryption, "1.2.840.113625.100.1.32");
SEC_CONST_DECL (kSecPolicyAppleServerAuthentication, "1.2.840.113635.100.1.33");
SEC_CONST_DECL (kSecPolicyApplePCSEscrowService, "1.2.840.113635.100.1.34");
SEC_CONST_DECL (kSecPolicyApplePPQSigning, "1.2.840.113625.100.1.35");
SEC_CONST_DECL (kSecPolicyAppleTestPPQSigning, "1.2.840.113625.100.1.36");
SEC_CONST_DECL (kSecPolicyAppleATVAppSigning, "1.2.840.113625.100.1.37");
SEC_CONST_DECL (kSecPolicyAppleTestATVAppSigning, "1.2.840.113625.100.1.38");
SEC_CONST_DECL (kSecPolicyApplePayIssuerEncryption, "1.2.840.113625.100.1.39");
SEC_CONST_DECL (kSecPolicyAppleOSXProvisioningProfileSigning, "1.2.840.113625.100.1.40");
SEC_CONST_DECL (kSecPolicyAppleATVVPNProfileSigning, "1.2.840.113625.100.1.41");
// TODO need confirmation that OID for kSecPolicyAppleATVVPNProfileSigning is reserved

SEC_CONST_DECL (kSecPolicyOid, "SecPolicyOid");
SEC_CONST_DECL (kSecPolicyName, "SecPolicyName");
SEC_CONST_DECL (kSecPolicyClient, "SecPolicyClient");
SEC_CONST_DECL (kSecPolicyRevocationFlags, "SecPolicyRevocationFlags");
SEC_CONST_DECL (kSecPolicyTeamIdentifier, "SecPolicyTeamIdentifier");

/* Private policy names */
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
static CFStringRef kSecPolicyOIDEAPServer = CFSTR("eapServer");
static CFStringRef kSecPolicyOIDEAPClient = CFSTR("eapClient");
static CFStringRef kSecPolicyOIDIPSecServer = CFSTR("ipsecServer");
static CFStringRef kSecPolicyOIDIPSecClient = CFSTR("ipsecClient");
static CFStringRef kSecPolicyOIDiPhoneApplicationSigning =
    CFSTR("iPhoneApplicationSigning");
static CFStringRef kSecPolicyOIDiPhoneProfileApplicationSigning =
    CFSTR("iPhoneProfileApplicationSigning");
static CFStringRef kSecPolicyOIDiPhoneProvisioningProfileSigning =
    CFSTR("iPhoneProvisioningProfileSigning");
static CFStringRef kSecPolicyOIDAppleSWUpdateSigning = CFSTR("AppleSWUpdateSigning");
static CFStringRef kSecPolicyOIDAppleTVOSApplicationSigning =
    CFSTR("AppleTVApplicationSigning");
static CFStringRef kSecPolicyOIDRevocation = CFSTR("revocation");
static CFStringRef kSecPolicyOIDOCSPSigner = CFSTR("OCSPSigner");
static CFStringRef kSecPolicyOIDSMIME = CFSTR("SMIME");
static CFStringRef kSecPolicyOIDCodeSigning = CFSTR("CodeSigning");
static CFStringRef kSecPolicyOIDPackageSigning = CFSTR("PackageSigning");
static CFStringRef kSecPolicyOIDLockdownPairing = CFSTR("LockdownPairing");
static CFStringRef kSecPolicyOIDURLBag = CFSTR("URLBag");
static CFStringRef kSecPolicyOIDOTATasking = CFSTR("OTATasking");
static CFStringRef kSecPolicyOIDMobileAsset = CFSTR("MobileAsset");
static CFStringRef kSecPolicyOIDAppleIDAuthority = CFSTR("AppleIDAuthority");
static CFStringRef kSecPolicyOIDMacAppStoreReceipt = CFSTR("MacAppStoreReceipt");
static CFStringRef kSecPolicyOIDAppleTimeStamping = CFSTR("AppleTimeStamping");
static CFStringRef kSecPolicyOIDApplePassbook = CFSTR("ApplePassbook");
static CFStringRef kSecPolicyOIDAppleMobileStore = CFSTR("AppleMobileStore");
static CFStringRef kSecPolicyOIDAppleTestMobileStore = CFSTR("AppleTestMobileStore");
static CFStringRef kSecPolicyOIDAppleEscrowService = CFSTR("AppleEscrowService");
static CFStringRef kSecPolicyOIDApplePCSEscrowService = CFSTR("ApplePCSEscrowService");
static CFStringRef kSecPolicyOIDAppleProfileSigner = CFSTR("AppleProfileSigner");
static CFStringRef kSecPolicyOIDAppleQAProfileSigner = CFSTR("AppleQAProfileSigner");
static CFStringRef kSecPolicyOIDAppleOTAPKIAssetSigner = CFSTR("AppleOTAPKIAssetSigner");
static CFStringRef kSecPolicyOIDAppleTestOTAPKIAssetSigner = CFSTR("AppleTestOTAPKIAssetSigner");
static CFStringRef kSecPolicyOIDAppleIDValidationRecordSigningPolicy = CFSTR("AppleIDValidationRecordSigningPolicy");
static CFStringRef kSecPolicyOIDAppleATVAppSigning = CFSTR("AppleATVAppSigning");
static CFStringRef kSecPolicyOIDAppleTestATVAppSigning = CFSTR("AppleTestATVAppSigning");
static CFStringRef kSecPolicyOIDApplePayIssuerEncryption = CFSTR("ApplePayIssuerEncryption");
static CFStringRef kSecPolicyOIDAppleOSXProvisioningProfileSigning = CFSTR("AppleOSXProvisioningProfileSigning");
static CFStringRef kSecPolicyOIDAppleATVVPNProfileSigning = CFSTR("AppleATVVPNProfileSigning");

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
/*
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
            // Modify SecPathBuilderIsPartial() to
            // run builder_check(buildier, policies, kSecPolicySICheck) instead
            // of SecCertificateIsValid.  Also rename considerExpired to
            //   considerSIFailures.
            foreach (policy in p_d) {
                check_policy(builder, chain, policy, kSecPolicySICheck, depth);
            }
            /// Recalculate since the static checks might have added new dynamic
            //   policies.
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

*/



#define kSecPolicySHA1Size 20
#define kSecPolicySHA256Size 32
const UInt8 kAppleCASHA1[kSecPolicySHA1Size] = {
    0x61, 0x1E, 0x5B, 0x66, 0x2C, 0x59, 0x3A, 0x08, 0xFF, 0x58,
    0xD1, 0x4A, 0xE2, 0x24, 0x52, 0xD1, 0x98, 0xDF, 0x6C, 0x60
};

__unused static const UInt8 kAppleTESTCASHA1[kSecPolicySHA1Size] = {
    0xbc, 0x30, 0x55, 0xc8, 0xc8, 0xd3, 0x48, 0x3f, 0xf4, 0x8d,
    0xfe, 0x3d, 0x51, 0x75, 0x31, 0xc9, 0xf4, 0xd7, 0x4a, 0xf7
};

static const UInt8 kITMSCASHA1[kSecPolicySHA1Size] = {
    0x1D, 0x33, 0x42, 0x46, 0x8B, 0x10, 0xBD, 0xE6, 0x45, 0xCE,
    0x44, 0x6E, 0xBB, 0xE8, 0xF5, 0x03, 0x5D, 0xF8, 0x32, 0x22
};

static const UInt8 kFactoryDeviceCASHA1[kSecPolicySHA1Size] = {
  	0xef, 0x68, 0x73, 0x17, 0xa4, 0xf8, 0xf9, 0x4b, 0x7b, 0x21,
  	0xe2, 0x2f, 0x09, 0x8f, 0xfd, 0x6a, 0xae, 0xc0, 0x0d, 0x63
};

static const UInt8 kApplePKISettingsAuthority[kSecPolicySHA1Size] = {
	0x1D, 0x0C, 0xBA, 0xAD, 0x17, 0xFD, 0x7E, 0x9E, 0x9F, 0xF1,
	0xC9, 0xA2, 0x66, 0x79, 0x60, 0x00, 0x8B, 0xAE, 0x70, 0xB8
};

static const UInt8 kAppleTestPKISettingsAuthority[kSecPolicySHA1Size] = {
	0xDB, 0xBA, 0x25, 0x0B, 0xD8, 0x62, 0x71, 0x87, 0x54, 0x7E,
	0xD7, 0xEF, 0x11, 0x94, 0x7E, 0x82, 0xE6, 0xD8, 0x1C, 0x9A
};

static const UInt8 kTestAppleRootCA_ECC_SHA1[kSecPolicySHA1Size] = {
	0x62, 0x0A, 0xED, 0x83, 0xD2, 0x97, 0x4A, 0x77, 0x56, 0x33,
	0x83, 0xBE, 0xDB, 0xF9, 0xA1, 0xBD, 0x5F, 0xFE, 0x55, 0x7B
};

static const UInt8 kAppleRootCA_ECC_SHA1[kSecPolicySHA1Size] = {
    0xB5, 0x2C, 0xB0, 0x2F, 0xD5, 0x67, 0xE0, 0x35, 0x9F, 0xE8,
    0xFA, 0x4D, 0x4C, 0x41, 0x03, 0x79, 0x70, 0xFE, 0x01, 0xB0
};

// MARK: -
// MARK: SecPolicy
/********************************************************
 ****************** SecPolicy object ********************
 ********************************************************/

static void SecPolicyDestroy(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;
	CFRelease(policy->_oid);
	CFRelease(policy->_options);
}

static Boolean SecPolicyCompare(CFTypeRef cf1, CFTypeRef cf2) {
	SecPolicyRef policy1 = (SecPolicyRef) cf1;
	SecPolicyRef policy2 = (SecPolicyRef) cf2;
	return CFEqual(policy1->_oid, policy2->_oid) &&
		CFEqual(policy1->_options, policy2->_options);
}

static CFHashCode SecPolicyHash(CFTypeRef cf) {
	SecPolicyRef policy = (SecPolicyRef) cf;

	return CFHash(policy->_oid) + CFHash(policy->_options);
}

static CFStringRef SecPolicyCopyFormatDescription(CFTypeRef cf, CFDictionaryRef formatOptions) {
	SecPolicyRef policy = (SecPolicyRef) cf;
    CFMutableStringRef desc = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringRef typeStr = CFCopyTypeIDDescription(CFGetTypeID(cf));
    CFStringAppendFormat(desc, NULL,
        CFSTR("<%@: oid: %@ options %@"), typeStr,
        policy->_oid, policy->_options);
    CFRelease(typeStr);
    CFStringAppend(desc, CFSTR(" >"));

    return desc;
}

/* SecPolicy API functions. */
CFGiblisWithHashFor(SecPolicy);

/* AUDIT[securityd](done):
   oid (ok) is a caller provided string, only its cf type has been checked.
   options is a caller provided dictionary, only its cf type has been checked.
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

SecPolicyRef SecPolicyCreateWithProperties(CFTypeRef policyIdentifier,
	CFDictionaryRef properties) {
	// Creates a policy reference for a given policy object identifier.
	// If policy-specific parameters can be supplied (e.g. hostname),
	// attempt to obtain from input properties dictionary.
	// Returns NULL if the given identifier is unsupported.

	SecPolicyRef policy = NULL;
	CFStringRef name = NULL;
	CFStringRef teamID = NULL;
	Boolean client = false;
	require(policyIdentifier && (CFStringGetTypeID() == CFGetTypeID(policyIdentifier)), errOut);

	if (properties) {
		name = CFDictionaryGetValue(properties, kSecPolicyName);
		teamID = CFDictionaryGetValue(properties, kSecPolicyTeamIdentifier);

		CFBooleanRef dictionaryClientValue;
		client = (CFDictionaryGetValueIfPresent(properties, kSecPolicyClient, (const void **)&dictionaryClientValue) &&
				(dictionaryClientValue != NULL) && CFEqual(kCFBooleanTrue, dictionaryClientValue));
	}

	if (CFEqual(policyIdentifier, kSecPolicyAppleX509Basic)) {
		policy = SecPolicyCreateBasicX509();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleSSL)) {
		policy = SecPolicyCreateSSL(!client, name);
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleEAP)) {
		CFArrayRef array = NULL;
		if (name) {
			array = CFArrayCreate(kCFAllocatorDefault, (const void **)&name, 1, &kCFTypeArrayCallBacks);
		}
		policy = SecPolicyCreateEAP(!client, array);
		CFReleaseSafe(array);
	}
	else if (CFEqual(policyIdentifier, kSecPolicyApplePackageSigning)) {
		policy = SecPolicyCreateApplePackageSigning();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleSWUpdateSigning)) {
		policy = SecPolicyCreateAppleSWUpdateSigning();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleIPsec)) {
		policy = SecPolicyCreateIPSec(!client, name);
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleRevocation)) {
		policy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleSMIME)) {
		policy = SecPolicyCreateSMIME(kSecSignSMIMEUsage | kSecAnyEncryptSMIME, name);
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleCodeSigning)) {
		policy = SecPolicyCreateCodeSigning();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleTimeStamping)) {
		policy = SecPolicyCreateAppleTimeStamping();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyMacAppStoreReceipt)) {
		policy = SecPolicyCreateMacAppStoreReceipt();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleIDValidation)) {
		policy = SecPolicyCreateAppleIDAuthorityPolicy();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyApplePassbookSigning)) {
		policy = SecPolicyCreatePassbookCardSigner(name, teamID);
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleMobileStore)) {
		policy = SecPolicyCreateMobileStoreSigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleTestMobileStore)) {
		policy = SecPolicyCreateTestMobileStoreSigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleEscrowService)) {
		policy = SecPolicyCreateEscrowServiceSigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyApplePCSEscrowService)) {
		policy = SecPolicyCreatePCSEscrowServiceSigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleProfileSigner)) {
		policy = SecPolicyCreateConfigurationProfileSigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleQAProfileSigner)) {
		policy = SecPolicyCreateQAConfigurationProfileSigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleServerAuthentication)) {
		policy = SecPolicyCreateAppleSSLService(name);
	}
#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
	else if (CFEqual(policyIdentifier, kSecPolicyAppleOTAPKISigner)) {
		policy = SecPolicyCreateOTAPKISigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleTestOTAPKISigner)) {
		policy = SecPolicyCreateTestOTAPKISigner();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleIDValidationRecordSigningPolicy)) {
		policy = SecPolicyCreateAppleIDValidationRecordSigningPolicy();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleSMPEncryption)) {
		policy = SecPolicyCreateAppleSMPEncryption();
	}
	else if (CFEqual(policyIdentifier, kSecPolicyAppleTestSMPEncryption)) {
		policy = SecPolicyCreateTestAppleSMPEncryption();
	}
    else if (CFEqual(policyIdentifier, kSecPolicyAppleATVAppSigning)) {
        policy = SecPolicyCreateAppleATVAppSigning();
    }
    else if (CFEqual(policyIdentifier, kSecPolicyAppleTestATVAppSigning)) {
        policy = SecPolicyCreateTestAppleATVAppSigning();
    }
#endif
    else if (CFEqual(policyIdentifier, kSecPolicyApplePPQSigning)) {
        policy = SecPolicyCreateApplePPQSigning();
    }
    else if (CFEqual(policyIdentifier, kSecPolicyAppleTestPPQSigning)) {
        policy = SecPolicyCreateTestApplePPQSigning();
    }
    else if (CFEqual(policyIdentifier, kSecPolicyApplePayIssuerEncryption)) {
        policy = SecPolicyCreateApplePayIssuerEncryption();
    }
    else if (CFEqual(policyIdentifier, kSecPolicyAppleATVVPNProfileSigning)) {
        policy = SecPolicyCreateAppleATVVPNProfileSigning();
    }
	else {
		secerror("ERROR: policy \"%@\" is unsupported", policyIdentifier);
	}

errOut:
	return policy;
}

CFDictionaryRef SecPolicyCopyProperties(SecPolicyRef policyRef) {
	// Builds and returns a dictionary which the caller must release.

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // After introducing nullability annotations, policyRef is supposed to be nonnull, suppress the warning
	if (!policyRef) return NULL;
#pragma clang diagnostic pop
	CFMutableDictionaryRef properties = CFDictionaryCreateMutable(NULL, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // 'properties' is nonnull in reality suppress the warning
	if (!properties) return NULL;
#pragma clang diagnostic pop
	CFStringRef oid = (CFStringRef) CFRetain(policyRef->_oid);
	CFTypeRef nameKey = NULL;

	// Convert private to public OID if we have one
	CFStringRef outOid = oid;
	if (CFEqual(oid, kSecPolicyOIDBasicX509)) {
		outOid = kSecPolicyAppleX509Basic;
	}
	else if (CFEqual(oid, kSecPolicyOIDSSLServer) ||
			 CFEqual(oid, kSecPolicyOIDSSLClient)) {
		outOid = kSecPolicyAppleSSL;
		nameKey = kSecPolicyCheckSSLHostname;
	}
	else if (CFEqual(oid, kSecPolicyOIDEAPServer) ||
			 CFEqual(oid, kSecPolicyOIDEAPClient)) {
		outOid = kSecPolicyAppleEAP;
		nameKey = kSecPolicyCheckEAPTrustedServerNames;
	}
	else if (CFEqual(oid, kSecPolicyOIDIPSecServer) ||
			 CFEqual(oid, kSecPolicyOIDIPSecClient)) {
		outOid = kSecPolicyAppleIPsec;
		nameKey = kSecPolicyCheckSSLHostname;
	}
	else if (CFEqual(oid, kSecPolicyOIDRevocation)) {
		outOid = kSecPolicyAppleRevocation;
	}
	else if (CFEqual(oid, kSecPolicyOIDSMIME)) {
		outOid = kSecPolicyAppleSMIME;
		nameKey = kSecPolicyCheckEmail;
	}
	else if (CFEqual(oid, kSecPolicyOIDCodeSigning)) {
		outOid = kSecPolicyAppleCodeSigning;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleIDAuthority)) {
		outOid = kSecPolicyAppleIDValidation;
	}
	else if (CFEqual(oid, kSecPolicyOIDApplePassbook)) {
		outOid = kSecPolicyApplePassbookSigning;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleMobileStore)) {
		outOid = kSecPolicyAppleMobileStore;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleTestMobileStore)) {
		outOid = kSecPolicyAppleTestMobileStore;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleEscrowService)) {
		outOid = kSecPolicyAppleEscrowService;
	}
	else if (CFEqual(oid, kSecPolicyOIDApplePCSEscrowService)) {
		outOid = kSecPolicyApplePCSEscrowService;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleProfileSigner)) {
		outOid = kSecPolicyAppleProfileSigner;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleQAProfileSigner)) {
		outOid = kSecPolicyAppleQAProfileSigner;
	}
#if TARGET_OS_IPHONE && !TARGET_IPHONE_SIMULATOR
	else if (CFEqual(oid, kSecPolicyOIDAppleOTAPKIAssetSigner)) {
		outOid = kSecPolicyAppleOTAPKISigner;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleTestOTAPKIAssetSigner)) {
		outOid = kSecPolicyAppleTestOTAPKISigner;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleIDValidationRecordSigningPolicy)) {
		outOid = kSecPolicyAppleIDValidationRecordSigningPolicy;
	}
    else if (CFEqual(oid, kSecPolicyOIDAppleATVAppSigning)) {
        outOid = kSecPolicyAppleATVAppSigning;
    }
    else if (CFEqual(oid, kSecPolicyOIDAppleTestATVAppSigning)) {
        outOid = kSecPolicyAppleTestATVAppSigning;
    }
#endif
	else if (CFEqual(oid, kSecPolicyOIDApplePayIssuerEncryption)) {
		outOid = kSecPolicyApplePayIssuerEncryption;
	}
	else if (CFEqual(oid, kSecPolicyOIDAppleOSXProvisioningProfileSigning)) {
		outOid = kSecPolicyAppleOSXProvisioningProfileSigning;
	}
    else if (CFEqual(oid, kSecPolicyOIDAppleATVVPNProfileSigning)) {
        outOid = kSecPolicyAppleATVVPNProfileSigning;
    }

	// Set kSecPolicyOid
	CFDictionarySetValue(properties, (const void *)kSecPolicyOid,
		(const void *)outOid);

	// Set kSecPolicyName if we have one
	if (nameKey && policyRef->_options) {
		CFTypeRef name = (CFTypeRef) CFDictionaryGetValue(policyRef->_options,
			nameKey);
		if (name) {
			CFDictionarySetValue(properties, (const void *)kSecPolicyName,
				(const void *)name);
		}
	}

	// Set kSecPolicyClient
	if (CFEqual(oid, kSecPolicyOIDSSLClient) ||
		CFEqual(oid, kSecPolicyOIDIPSecClient) ||
		CFEqual(oid, kSecPolicyOIDEAPClient)) {
		CFDictionarySetValue(properties, (const void *)kSecPolicyClient,
			(const void *)kCFBooleanTrue);
	}

	CFRelease(oid);
	return properties;
}

static void SecPolicySetOid(SecPolicyRef policy, CFStringRef oid) {
	if (!policy || !oid) return;
	CFStringRef temp = policy->_oid;
	CFRetain(oid);
	policy->_oid = oid;
	CFReleaseSafe(temp);
}

CFStringRef SecPolicyGetOidString(SecPolicyRef policy) {
	return policy->_oid;
}

CFDictionaryRef SecPolicyGetOptions(SecPolicyRef policy) {
	return policy->_options;
}

void SecPolicySetOptionsValue(SecPolicyRef policy, CFStringRef key, CFTypeRef value) {
	if (!policy || !key) return;
	CFMutableDictionaryRef options = (CFMutableDictionaryRef) policy->_options;
	if (!options) {
		options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
		if (!options) return;
		policy->_options = options;
	}
	CFDictionarySetValue(options, key, value);
}

#if !SECTRUST_OSX
// this is declared as NA for iPhone in SecPolicy.h, so declare here
OSStatus SecPolicySetProperties(SecPolicyRef policyRef, CFDictionaryRef properties);
#endif

OSStatus SecPolicySetProperties(SecPolicyRef policyRef, CFDictionaryRef properties) {
	// Set policy options based on the provided dictionary keys.

	if (!(policyRef && properties && (CFDictionaryGetTypeID() == CFGetTypeID(properties)))) {
		return errSecParam;
	}
	CFStringRef oid = (CFStringRef) CFRetain(policyRef->_oid);
	OSStatus result = errSecSuccess;

	// kSecPolicyName
	CFTypeRef name = NULL;
	if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyName,
		(const void **)&name) && name) {
		CFTypeID typeID = CFGetTypeID(name);
		if (CFEqual(oid, kSecPolicyOIDSSLServer) ||
			CFEqual(oid, kSecPolicyOIDSSLClient) ||
			CFEqual(oid, kSecPolicyOIDIPSecServer) ||
			CFEqual(oid, kSecPolicyOIDIPSecClient)) {
			if (CFStringGetTypeID() == typeID) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyCheckSSLHostname, name);
			}
			else result = errSecParam;
		}
		else if (CFEqual(oid, kSecPolicyOIDEAPServer) ||
				 CFEqual(oid, kSecPolicyOIDEAPClient)) {
			if ((CFStringGetTypeID() == typeID) ||
				(CFArrayGetTypeID() == typeID)) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyCheckEAPTrustedServerNames, name);
			}
			else result = errSecParam;
		}
		else if (CFEqual(oid, kSecPolicyOIDSMIME)) {
			if (CFStringGetTypeID() == typeID) {
				SecPolicySetOptionsValue(policyRef, kSecPolicyCheckEmail, name);
			}
			else result = errSecParam;
		}
	}

	// kSecPolicyClient
	CFTypeRef client = NULL;
	if (CFDictionaryGetValueIfPresent(properties, (const void *)kSecPolicyClient,
		(const void **)&client) && client) {
		if (!(CFBooleanGetTypeID() == CFGetTypeID(client))) {
			result = errSecParam;
		}
		else if (CFEqual(client, kCFBooleanTrue)) {
			if (CFEqual(oid, kSecPolicyOIDSSLServer)) {
				SecPolicySetOid(policyRef, kSecPolicyOIDSSLClient);
			}
			else if (CFEqual(oid, kSecPolicyOIDIPSecServer)) {
				SecPolicySetOid(policyRef, kSecPolicyOIDIPSecClient);
			}
			else if (CFEqual(oid, kSecPolicyOIDEAPServer)) {
				SecPolicySetOid(policyRef, kSecPolicyOIDEAPClient);
			}
		}
		else {
			if (CFEqual(oid, kSecPolicyOIDSSLClient)) {
				SecPolicySetOid(policyRef, kSecPolicyOIDSSLServer);
			}
			else if (CFEqual(oid, kSecPolicyOIDIPSecClient)) {
				SecPolicySetOid(policyRef, kSecPolicyOIDIPSecServer);
			}
			else if (CFEqual(oid, kSecPolicyOIDEAPClient)) {
				SecPolicySetOid(policyRef, kSecPolicyOIDEAPServer);
			}
		}
	}

	CFRelease(oid);
	return result;
}

static xpc_object_t copy_xpc_policy_object(SecPolicyRef policy);
static bool append_policy_to_xpc_array(SecPolicyRef policy, xpc_object_t xpc_policies);
extern xpc_object_t copy_xpc_policies_array(CFArrayRef policies);
extern OSStatus validate_array_of_items(CFArrayRef array, CFStringRef arrayItemType, CFTypeID itemTypeID, bool required);

static xpc_object_t copy_xpc_policy_object(SecPolicyRef policy) {
    xpc_object_t xpc_policy = NULL;
    xpc_object_t data[2] = { NULL, NULL };
	if (policy->_oid && (CFGetTypeID(policy->_oid) == CFStringGetTypeID())) {
		data[0] = _CFXPCCreateXPCObjectFromCFObject(policy->_oid);
	} else {
		secerror("policy 0x%lX has no _oid", (uintptr_t)policy);
	}
	if (policy->_options && (CFGetTypeID(policy->_options) == CFDictionaryGetTypeID())) {
		data[1] = _CFXPCCreateXPCObjectFromCFObject(policy->_options);
	} else {
		secerror("policy 0x%lX has no _options", (uintptr_t)policy);
	}
	xpc_policy = xpc_array_create(data, array_size(data));
    if (data[0]) xpc_release(data[0]);
    if (data[1]) xpc_release(data[1]);
    return xpc_policy;
}

static bool append_policy_to_xpc_array(SecPolicyRef policy, xpc_object_t xpc_policies) {
    if (!policy) {
        return true; // NOOP
	}
    xpc_object_t xpc_policy = copy_xpc_policy_object(policy);
    if (!xpc_policy) {
        return false;
	}
    xpc_array_append_value(xpc_policies, xpc_policy);
    xpc_release(xpc_policy);
    return true;
}

xpc_object_t copy_xpc_policies_array(CFArrayRef policies) {
	xpc_object_t xpc_policies = xpc_array_create(NULL, 0);
	if (!xpc_policies) {
		return NULL;
	}
	validate_array_of_items(policies, CFSTR("policy"), SecPolicyGetTypeID(), true);
	CFIndex ix, count = CFArrayGetCount(policies);
	for (ix = 0; ix < count; ++ix) {
		SecPolicyRef policy = (SecPolicyRef) CFArrayGetValueAtIndex(policies, ix);
    #if SECTRUST_VERBOSE_DEBUG
		CFDictionaryRef props = SecPolicyCopyProperties(policy);
		secerror("idx=%d of %d; policy=0x%lX properties=%@", (int)ix, (int)count, (uintptr_t)policy, props);
		CFReleaseSafe(props);
    #endif
		if (!append_policy_to_xpc_array(policy, xpc_policies)) {
			xpc_release(xpc_policies);
			xpc_policies = NULL;
			break;
		}
	}
	return xpc_policies;
}

static xpc_object_t SecPolicyCopyXPCObject(SecPolicyRef policy, CFErrorRef *error) {
    xpc_object_t xpc_policy = NULL;
    xpc_object_t data[2] = {};
    require_action_quiet(data[0] = _CFXPCCreateXPCObjectFromCFObject(policy->_oid), exit,
                         SecError(errSecParam, error, CFSTR("failed to create xpc_object from policy oid")));
    require_action_quiet(data[1] = _CFXPCCreateXPCObjectFromCFObject(policy->_options), exit,
                         SecError(errSecParam, error, CFSTR("failed to create xpc_object from policy options")));
    require_action_quiet(xpc_policy = xpc_array_create(data, array_size(data)), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array for policy")));

exit:
    if (data[0]) xpc_release(data[0]);
    if (data[1]) xpc_release(data[1]);
    return xpc_policy;
}

static bool SecPolicyAppendToXPCArray(SecPolicyRef policy, xpc_object_t policies, CFErrorRef *error) {
    if (!policy)
        return true; // NOOP

    xpc_object_t xpc_policy = SecPolicyCopyXPCObject(policy, error);
    if (!xpc_policy)
        return false;

    xpc_array_append_value(policies, xpc_policy);
    xpc_release(xpc_policy);
    return true;
}

xpc_object_t SecPolicyArrayCopyXPCArray(CFArrayRef policies, CFErrorRef *error) {
    xpc_object_t xpc_policies;
    require_action_quiet(xpc_policies = xpc_array_create(NULL, 0), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create xpc_array")));
    CFIndex ix, count = CFArrayGetCount(policies);
    for (ix = 0; ix < count; ++ix) {
        if (!SecPolicyAppendToXPCArray((SecPolicyRef)CFArrayGetValueAtIndex(policies, ix), xpc_policies, error)) {
            xpc_release(xpc_policies);
            return NULL;
        }
    }
exit:
    return xpc_policies;
}

static SecPolicyRef SecPolicyCreateWithXPCObject(xpc_object_t xpc_policy, CFErrorRef *error) {
    SecPolicyRef policy = NULL;
    CFTypeRef oid = NULL;
    CFTypeRef options = NULL;

    require_action_quiet(xpc_policy, exit, SecError(errSecParam, error, CFSTR("policy xpc value is NULL")));
    require_action_quiet(xpc_get_type(xpc_policy) == XPC_TYPE_ARRAY, exit, SecError(errSecDecode, error, CFSTR("policy xpc value is not an array")));
    require_action_quiet(xpc_array_get_count(xpc_policy) == 2, exit, SecError(errSecDecode, error, CFSTR("policy xpc array count != 2")));
    oid = _CFXPCCreateCFObjectFromXPCObject(xpc_array_get_value(xpc_policy, 0));
    require_action_quiet(isString(oid), exit,
                         SecError(errSecParam, error, CFSTR("failed to convert xpc policy[0]=%@ to CFString"), oid));
    options = _CFXPCCreateCFObjectFromXPCObject(xpc_array_get_value(xpc_policy, 1));
    require_action_quiet(isDictionary(options), exit,
                         SecError(errSecParam, error, CFSTR("failed to convert xpc policy[1]=%@ to CFDictionary"), options));
    require_action_quiet(policy = SecPolicyCreate(oid, options), exit, SecError(errSecDecode, error, CFSTR("Failed to create policy")));

exit:
    CFReleaseSafe(oid);
    CFReleaseSafe(options);
    return policy;
}

CFArrayRef SecPolicyXPCArrayCopyArray(xpc_object_t xpc_policies, CFErrorRef *error) {
    CFMutableArrayRef policies = NULL;
    require_action_quiet(xpc_get_type(xpc_policies) == XPC_TYPE_ARRAY, exit,
                         SecError(errSecParam, error, CFSTR("policies xpc value is not an array")));
    size_t count = xpc_array_get_count(xpc_policies);
    require_action_quiet(policies = CFArrayCreateMutable(kCFAllocatorDefault, count, &kCFTypeArrayCallBacks), exit,
                         SecError(errSecAllocate, error, CFSTR("failed to create CFArray of capacity %zu"), count));

    size_t ix;
    for (ix = 0; ix < count; ++ix) {
        SecPolicyRef policy = SecPolicyCreateWithXPCObject(xpc_array_get_value(xpc_policies, ix), error);
        if (!policy) {
            CFRelease(policies);
            return NULL;
        }
        CFArraySetValueAtIndex(policies, ix, policy);
        CFRelease(policy);
    }

exit:
    return policies;

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

static void add_leaf_marker_value(CFMutableDictionaryRef options, const DERItem *markerOid, CFStringRef string_value) {

    CFTypeRef policyData = NULL;

    if (NULL == string_value) {
        policyData = CFDataCreate(kCFAllocatorDefault,
                                markerOid ? markerOid->data : NULL,
                                markerOid ? markerOid->length : 0);
    } else {
        CFStringRef oid_as_string = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, markerOid);

        const void *key[1]   = { oid_as_string };
        const void *value[1] = { string_value };
        policyData = CFDictionaryCreate(kCFAllocatorDefault,
                                        key, value, 1,
                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFReleaseNull(oid_as_string);
    }

    add_element(options, kSecPolicyCheckLeafMarkerOid, policyData);

    CFReleaseNull(policyData);

}

static void add_leaf_marker(CFMutableDictionaryRef options, const DERItem *markerOid) {
    add_leaf_marker_value(options, markerOid, NULL);
}


static void add_certificate_policy_oid(CFMutableDictionaryRef options, const DERItem *certificatePolicyOid, CFStringRef string_value) {
	 CFTypeRef certificatePolicyData = NULL;

	if (NULL == string_value) {
        certificatePolicyData = CFDataCreate(kCFAllocatorDefault,
                                certificatePolicyOid ? certificatePolicyOid->data : NULL,
                                certificatePolicyOid ? certificatePolicyOid->length : 0);
    } else {
        CFStringRef oid_as_string = SecDERItemCopyOIDDecimalRepresentation(kCFAllocatorDefault, certificatePolicyOid);

        const void *key[1]   = { oid_as_string };
        const void *value[1] = { string_value };
        certificatePolicyData = CFDictionaryCreate(kCFAllocatorDefault,
                                        key, value, 1,
                                        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        CFReleaseNull(oid_as_string);
    }

	add_element(options, kSecPolicyCheckCertificatePolicy, certificatePolicyData);

	CFReleaseNull(certificatePolicyData);
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

	// Make sure that black and gray leaf checks are performed for basic X509 chain building
    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);
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
    add_element(options, kSecPolicyCheckAnchorSHA1, anchorData);

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
	CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess,
                         kCFBooleanTrue);

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

	if (hostname) {
		CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);
	}

	CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
	CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

	/* If server and EKU ext present then EKU ext should contain one of
	   ServerAuth or ExtendedKeyUsageAny or NetscapeSGC or MicrosoftSGC.
	   else if !server and EKU ext present then EKU ext should contain one of
	   ClientAuth or ExtendedKeyUsageAny. */

	/* We always allow certificates that specify oidAnyExtendedKeyUsage. */
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

    date = CFDateCreateForGregorianZuluDay(NULL, 2006, 5, 31);
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

	/* Since EAP is used to setup the network we don't want evaluation
	   using this policy to access the network. */
	CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess,
			kCFBooleanTrue);

	if (trustedServerNames) {
		CFDictionaryAddValue(options, kSecPolicyCheckEAPTrustedServerNames, trustedServerNames);

		/* Specifying trusted server names implies EAP-TLS,
		   so we need to check for EKU per rdar://22206018 */

		/* If server and EKU ext present then EKU ext should contain one of
		   ServerAuth or ExtendedKeyUsageAny or NetscapeSGC or MicrosoftSGC.
		   else if !server and EKU ext present then EKU ext should contain one of
		   ClientAuth or ExtendedKeyUsageAny. */

		/* We always allow certificates that specify oidAnyExtendedKeyUsage. */
		add_eku(options, NULL); /* eku extension is optional */
		add_eku(options, &oidAnyExtendedKeyUsage);
		if (server) {
			add_eku(options, &oidExtendedKeyUsageServerAuth);
			add_eku(options, &oidExtendedKeyUsageMicrosoftSGC);
			add_eku(options, &oidExtendedKeyUsageNetscapeSGC);
		} else {
			add_eku(options, &oidExtendedKeyUsageClientAuth);
		}
	}

	require(result = SecPolicyCreate(
				server ? kSecPolicyOIDEAPServer : kSecPolicyOIDEAPClient,
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
    if (SecIsInternalRelease() && !SecIsProductionFused()) {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
                             CFSTR("Apple iPhone OS Application Signing"));
    }
    else {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                             CFSTR("Apple iPhone OS Application Signing"));
    }

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
    if (SecIsInternalRelease() && !SecIsProductionFused()) {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonNameTEST,
                             CFSTR("Apple iPhone OS Provisioning Profile Signing"));
    }
    else {
        CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                             CFSTR("Apple iPhone OS Provisioning Profile Signing"));
    }

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDiPhoneProvisioningProfileSigning, options),
        errOut);

    /* 1.2.840.113635.100.6.2.2.1, non-critical: DER:05:00 - provisioning profile */

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateAppleTVOSApplicationSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;
	CFDataRef atvProdOid = NULL;
	CFDataRef atvTestOid = NULL;
	CFArrayRef oids = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFMutableDictionaryRef appleAnchorOptions = CFDictionaryCreateMutableForCFTypes(NULL);
    require(appleAnchorOptions, errOut);
    add_element(options, kSecPolicyCheckAnchorApple, appleAnchorOptions);

    /* Check for intermediate: Apple Worldwide Developer Relations */
    /* 1.2.840.113635.100.6.2.1 */
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleWWDR);

    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);

    /* Check for prod or test AppleTV Application Signing OIDs */
    /* Prod: 1.2.840.113635.100.6.1.24 */
    /* Test: 1.2.840.113635.100.6.1.24.1 */
    atvProdOid = CFDataCreate(kCFAllocatorDefault, oidAppleTVOSApplicationSigningProd.data, oidAppleTVOSApplicationSigningProd.length);
    require(atvProdOid, errOut);
    atvTestOid = CFDataCreate(kCFAllocatorDefault, oidAppleTVOSApplicationSigningTest.data, oidAppleTVOSApplicationSigningTest.length);
    require(atvTestOid, errOut);

    oids = CFArrayCreateForCFTypes(kCFAllocatorDefault, atvProdOid, atvTestOid, NULL);
    require(oids, errOut);

    add_element(options, kSecPolicyCheckLeafMarkerOid, oids);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleTVOSApplicationSigning, options),
			errOut);

errOut:
	CFReleaseSafe(options);
	CFReleaseSafe(oids);
	CFReleaseSafe(atvProdOid);
	CFReleaseSafe(atvTestOid);
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

SecPolicyRef SecPolicyCreateRevocation(CFOptionFlags revocationFlags) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

    /* false = ocsp, true = crl, string/url value = crl distribution point,
       array = list of multiple values for example false, true, url1, url2
       check ocsp, crl, and url1 and url2 for certs which have no extensions.
     */
	if (revocationFlags & kSecRevocationOCSPMethod) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);
	}
	else if (revocationFlags & kSecRevocationCRLMethod) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanTrue);
	}

	if (revocationFlags & kSecRevocationRequirePositiveResponse) {
		CFDictionaryAddValue(options, kSecPolicyCheckRevocationResponseRequired, kCFBooleanTrue);
	}

	if (revocationFlags & kSecRevocationNetworkAccessDisabled) {
		CFDictionaryAddValue(options, kSecPolicyCheckNoNetworkAccess, kCFBooleanTrue);
	}

	/* Only flag bits 0-4 are currently defined */
	require(((revocationFlags >> 5) == 0), errOut);

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

    /* RFC 3850 paragraph 4.4.4

       If the extended key usage extension is present in the certificate
       then interpersonal message S/MIME receiving agents MUST check that it
       contains either the emailProtection or the anyExtendedKeyUsage OID as
       defined in [KEYM].  S/MIME uses other than interpersonal messaging
       MAY require the explicit presence of the extended key usage extension
       or other OIDs to be present in the extension or both.
     */
    add_eku(options, NULL); /* eku extension is optional */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageEmailProtection);

	require(result = SecPolicyCreate(kSecPolicyOIDSMIME, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreateApplePackageSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	// TBD: review OS X policy to see what options are needed for this policy
	SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDPackageSigning, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;

}

SecPolicyRef SecPolicyCreateAppleSWUpdateSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;
/*
 * OS X rules for this policy:
 * -- Must have one intermediate cert
 * -- intermediate must have basic constraints with path length 0
 * -- intermediate has CSSMOID_APPLE_EKU_CODE_SIGNING EKU
 * -- leaf cert has either CODE_SIGNING or CODE_SIGN_DEVELOPMENT EKU (the latter of
 *    which triggers a CSSMERR_APPLETP_CODE_SIGN_DEVELOPMENT error)
 */
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

	// TBD: review OS X policy to see what options are needed for this policy
	SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

    add_eku(options, &oidAppleExtendedKeyUsageCodeSigning);
    add_eku(options, &oidAppleExtendedKeyUsageCodeSigningDev);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleSWUpdateSigning, options),
        errOut);

errOut:
	CFReleaseSafe(options);
	return result;

}

SecPolicyRef SecPolicyCreateCodeSigning(void) {
	CFMutableDictionaryRef options = NULL;
	SecPolicyRef result = NULL;

	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
		&kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks), errOut);

#if SECTRUST_OSX
#warning STU: <rdar://21328880>
//%%% figure out why this policy is not passing for OS X.
// I suspect it has to do with iOS not supporting anchor and keychain options yet.
	SecPolicyAddBasicCertOptions(options);
#else
    SecPolicyAddBasicX509Options(options);

    /* If the key usage extension is present we accept it having either of
       these values. */
    add_ku(options, kSecKeyUsageDigitalSignature);
    add_ku(options, kSecKeyUsageNonRepudiation);

    /* We require a extended key usage extension and we accept any or
       codesigning ekus. */
    /* TODO: Do we want to accept the apple codesigning oid as well or is
       that a separate policy? */
    /* ANSWER: it's a separate policy, SecPolicyCreateAppleSWUpdateSigning */
    add_eku(options, &oidAnyExtendedKeyUsage);
    add_eku(options, &oidExtendedKeyUsageCodeSigning);
#endif

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
    add_leaf_marker(options, &oidAppleExtendedKeyUsageAppleID);

    // and validate that intermediate has extension with CSSMOID_APPLE_EXTENSION_AAI_INTERMEDIATE  oid (1.2.840.113635.100.6.2.3) and goes back to the Apple Root CA.
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID);
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID2);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleIDAuthority, options), out);

out:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMacAppStoreReceipt(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), out);

    SecPolicyAddBasicX509Options(options);

    // Apple CA anchored
    require(SecPolicyAddAppleAnchorOptions(options), out);

	// - leaf needs certificatePolicies with CSSMOID_MACAPPSTORE_RECEIPT_CERT_POLICY
	// - chain length must be 3

	require(result = SecPolicyCreate(kSecPolicyOIDMacAppStoreReceipt, options), out);

out:
    CFReleaseSafe(options);
    return result;

}

static SecPolicyRef _SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer, CFStringRef teamIdentifier, bool requireTeamID)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
				&kCFTypeDictionaryKeyCallBacks,
				&kCFTypeDictionaryValueCallBacks), out);

	SecPolicyAddBasicX509Options(options);
	SecPolicyAddAppleAnchorOptions(options);

	if (teamIdentifier) {
		// If supplied, teamIdentifier must match subject OU field
		CFDictionaryAddValue(options, kSecPolicyCheckSubjectOrganizationalUnit, teamIdentifier);
	}
	else {
		// If not supplied, and it was required, fail
		require(!requireTeamID, out);
	}

    // Must be both push and 3rd party package signing
    add_leaf_marker_value(options, &oidAppleInstallerPackagingSigningExternal, cardIssuer);

	// We should check that it also has push marker, but we don't support requiring both, only either.
	// add_independent_oid(options, kSecPolicyCheckLeafMarkerOid, &oidApplePushServiceClient);

	// And Passbook signing eku
	add_eku(options, &oidAppleExtendedKeyUsagePassbook);

	require(result = SecPolicyCreate(kSecPolicyOIDApplePassbook, options), out);

out:
	CFReleaseSafe(options);
	return result;
}

SecPolicyRef SecPolicyCreatePassbookCardSigner(CFStringRef cardIssuer, CFStringRef teamIdentifier)
{
    return _SecPolicyCreatePassbookCardSigner(cardIssuer, teamIdentifier, true);
}


static SecPolicyRef CreateMobileStoreSigner(Boolean forTest)
{

    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicX509Options(options);
    SecPolicyAddAppleAnchorOptions(options);

    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple System Integration 2 Certification Authority"));

    add_ku(options, kSecKeyUsageDigitalSignature);

    const DERItem* pOID = (forTest) ? &oidApplePolicyTestMobileStore : &oidApplePolicyMobileStore;

    add_certificate_policy_oid(options, pOID, NULL);

    require(result = SecPolicyCreate(kSecPolicyOIDAppleMobileStore, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

SecPolicyRef SecPolicyCreateMobileStoreSigner(void)
{

	return CreateMobileStoreSigner(false);
}

SecPolicyRef SecPolicyCreateTestMobileStoreSigner(void)
{

	return CreateMobileStoreSigner(true);
}


CF_RETURNS_RETAINED SecPolicyRef SecPolicyCreateEscrowServiceSigner(void)
{
	SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    CFArrayRef anArray = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

	// X509, ignoring date validity
	SecPolicyAddBasicCertOptions(options);


	add_ku(options, kSecKeyUsageKeyEncipherment);

	//add_leaf_marker(options, &oidApplePolicyEscrowService);
	require(SecPolicyAddChainLengthOptions(options, 2), errOut);


	Boolean anchorAdded = false;
	// Get the roots by calling the SecCertificateCopyEscrowRoots
	anArray = SecCertificateCopyEscrowRoots(kSecCertificateProductionEscrowRoot);
    CFIndex numRoots = 0;
	if (NULL == anArray || 0 == (numRoots = CFArrayGetCount(anArray)))
	{
		goto errOut;
	}

	for (CFIndex iCnt = 0; iCnt < numRoots; iCnt++)
	{
		SecCertificateRef aCert = (SecCertificateRef)CFArrayGetValueAtIndex(anArray, iCnt);

		if (NULL != aCert)
		{
			CFDataRef sha_data = SecCertificateGetSHA1Digest(aCert);
			if (NULL != sha_data)
			{
				const UInt8* pSHAData = CFDataGetBytePtr(sha_data);
				if (NULL != pSHAData)
				{
					SecPolicyAddAnchorSHA1Options(options, pSHAData);
					anchorAdded = true;
				}
			}
		}
	}
	CFReleaseNull(anArray);

	if (!anchorAdded)
	{
		goto errOut;
	}


    require(result = SecPolicyCreate(kSecPolicyOIDAppleEscrowService, options), errOut);

errOut:
    CFReleaseSafe(anArray);
	CFReleaseSafe(options);
	return result;
}

CF_RETURNS_RETAINED SecPolicyRef SecPolicyCreatePCSEscrowServiceSigner(void)
{
	SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    CFArrayRef anArray = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);


	add_ku(options, kSecKeyUsageKeyEncipherment);

	//add_leaf_marker(options, &oidApplePolicyEscrowService);
	require(SecPolicyAddChainLengthOptions(options, 2), errOut);


	Boolean anchorAdded = false;
	anArray = SecCertificateCopyEscrowRoots(kSecCertificateProductionPCSEscrowRoot);
    CFIndex numRoots = 0;
	if (NULL == anArray || 0 == (numRoots = CFArrayGetCount(anArray)))
	{
		goto errOut;
	}

	for (CFIndex iCnt = 0; iCnt < numRoots; iCnt++)
	{
		SecCertificateRef aCert = (SecCertificateRef)CFArrayGetValueAtIndex(anArray, iCnt);

		if (NULL != aCert)
		{
			CFDataRef sha_data = SecCertificateGetSHA1Digest(aCert);
			if (NULL != sha_data)
			{
				const UInt8* pSHAData = CFDataGetBytePtr(sha_data);
				if (NULL != pSHAData)
				{
					SecPolicyAddAnchorSHA1Options(options, pSHAData);
					anchorAdded = true;
				}
			}
		}
	}
	CFReleaseNull(anArray);

	if (!anchorAdded)
	{
		goto errOut;
	}


    require(result = SecPolicyCreate(kSecPolicyOIDApplePCSEscrowService, options), errOut);

errOut:
    CFReleaseSafe(anArray);
	CFReleaseSafe(options);
	return result;
}
SecCertificateRef SecPolicyCopyEscrowRootCertificate(void)
{
	SecCertificateRef result = NULL;

	return result;
}

SecPolicyRef SecPolicyCreateConfigurationProfileSigner(void)
{
  SecPolicyRef result = NULL;
  CFMutableDictionaryRef options = NULL;
  require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

  SecPolicyAddBasicX509Options(options);
  SecPolicyAddAppleAnchorOptions(options);

  // Require the profile signing EKU
  add_eku(options, &oidAppleExtendedKeyUsageProfileSigning);

  require(result = SecPolicyCreate(kSecPolicyOIDAppleProfileSigner, options), errOut);

errOut:
  CFReleaseSafe(options);
  return result;
}


SecPolicyRef SecPolicyCreateQAConfigurationProfileSigner(void)
{
  SecPolicyRef result = NULL;
  CFMutableDictionaryRef options = NULL;
  require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

  SecPolicyAddBasicX509Options(options);
  SecPolicyAddAppleAnchorOptions(options);

  // Require the QA profile signing EKU
  add_eku(options, &oidAppleExtendedKeyUsageQAProfileSigning);

  require(result = SecPolicyCreate(kSecPolicyOIDAppleQAProfileSigner, options), errOut);

errOut:
  CFReleaseSafe(options);
  return result;
}

SecPolicyRef SecPolicyCreateOSXProvisioningProfileSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    // Require valid chain from the Apple root
    SecPolicyAddBasicX509Options(options);
    SecPolicyAddAppleAnchorOptions(options);

    // Require provisioning profile leaf marker OID (1.2.840.113635.100.4.11)
    add_leaf_marker(options, &oidAppleCertExtOSXProvisioningProfileSigning);

    // Require intermediate marker OID (1.2.840.113635.100.6.2.1)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleWWDR);

    // Require key usage that allows signing
    add_ku(options, kSecKeyUsageDigitalSignature);

    // Ensure that revocation is checked (OCSP)
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

    require(result = SecPolicyCreate(kSecPolicyOIDAppleOSXProvisioningProfileSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}


SecPolicyRef SecPolicyCreateOTAPKISigner(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	                                              &kCFTypeDictionaryKeyCallBacks,
	                                              &kCFTypeDictionaryValueCallBacks), errOut);
	SecPolicyAddBasicX509Options(options);

	SecPolicyAddAnchorSHA1Options(options, kApplePKISettingsAuthority);
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleOTAPKIAssetSigner, options), errOut);

errOut:
  CFReleaseSafe(options);
  return result;

}


SecPolicyRef SecPolicyCreateTestOTAPKISigner(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	                                              &kCFTypeDictionaryKeyCallBacks,
	                                              &kCFTypeDictionaryValueCallBacks), errOut);
	SecPolicyAddBasicX509Options(options);

	SecPolicyAddAnchorSHA1Options(options, kAppleTestPKISettingsAuthority);
    require(SecPolicyAddChainLengthOptions(options, 2), errOut);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleTestOTAPKIAssetSigner, options), errOut);

errOut:
  CFReleaseSafe(options);
  return result;
}

/*!
 @function SecPolicyCreateAppleSMPEncryption
 @abstract Check for intermediate certificate 'Apple System Integration CA - G3' by name,
    and root certificate 'Apple Root CA - G3' by hash.
    Leaf cert must have Key Encipherment usage.
    Leaf cert must have Apple SMP Encryption marker OID (1.2.840.113635.100.6.30).
    Intermediate must have marker OID (1.2.840.113635.100.6.2.13).
 */
SecPolicyRef SecPolicyCreateAppleSMPEncryption(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	                                              &kCFTypeDictionaryKeyCallBacks,
	                                              &kCFTypeDictionaryValueCallBacks), errOut);
	SecPolicyAddBasicCertOptions(options);

	SecPolicyAddAnchorSHA1Options(options, kAppleRootCA_ECC_SHA1);
	require(SecPolicyAddChainLengthOptions(options, 3), errOut);

	CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
					CFSTR("Apple System Integration CA - G3"));

	// Check that leaf has extension with "Apple SMP Encryption" oid (1.2.840.113635.100.6.30)
	add_leaf_marker(options, &oidAppleCertExtAppleSMPEncryption);

	// Check that intermediate has extension (1.2.840.113635.100.6.2.13)
	add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntgG3);

	add_ku(options, kSecKeyUsageKeyEncipherment);

	// Ensure that revocation is checked (OCSP)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

	require(result = SecPolicyCreate(kSecPolicyAppleSMPEncryption, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/*!
 @function SecPolicyCreateTestAppleSMPEncryption
 @abstract Check for intermediate certificate 'Test Apple System Integration CA - ECC' by name,
    and root certificate 'Test Apple Root CA - ECC' by hash.
    Leaf cert must have Key Encipherment usage. Other checks TBD.
 */
SecPolicyRef SecPolicyCreateTestAppleSMPEncryption(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
	                                              &kCFTypeDictionaryKeyCallBacks,
	                                              &kCFTypeDictionaryValueCallBacks), errOut);
	SecPolicyAddBasicCertOptions(options);

	SecPolicyAddAnchorSHA1Options(options, kTestAppleRootCA_ECC_SHA1);
	require(SecPolicyAddChainLengthOptions(options, 3), errOut);

	CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
			CFSTR("Test Apple System Integration CA - ECC"));

	add_ku(options, kSecKeyUsageKeyEncipherment);

	// Ensure that revocation is checked (OCSP)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

	require(result = SecPolicyCreate(kSecPolicyAppleTestSMPEncryption, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}


SecPolicyRef SecPolicyCreateAppleIDValidationRecordSigningPolicy(void)
{
	SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

    //Leaf appears to be a SSL only cert, so policy should expand on that policy
    SecPolicyAddBasicX509Options(options);

    // Apple CA anchored
    require(SecPolicyAddAppleAnchorOptions(options), errOut);

    // Check for an extension with " Apple ID Validation Record Signing" oid (1.2.840.113635.100.6.25)
    add_leaf_marker(options, &oidAppleCertExtensionAppleIDRecordValidationSigning);

    // and validate that intermediate has extension
	// Application Integration Intermediate Certificate (1.2.840.113635.100.6.2.3)
	// and also validate that intermediate has extension
	// System Integration 2 Intermediate Certificate (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleID);
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);

	// Ensure that revocation is checked (OCSP)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleIDValidationRecordSigningPolicy, options), errOut);

errOut:
  CFReleaseSafe(options);
  return result;
}

static bool
allowUATRoot(CFStringRef service, CFDictionaryRef context)
{
    bool UATAllowed = false;
    if (SecIsInternalRelease()) {
        CFStringRef setting = CFStringCreateWithFormat(NULL, NULL, CFSTR("AppleServerAuthenticationAllowUAT%@"), service);
        CFTypeRef value = NULL;
        require(setting, fail);

        if (context &&
            CFDictionaryGetValueIfPresent(context, setting, &value) &&
            isBoolean(value) &&
            CFBooleanGetValue(value))
        {
            UATAllowed = true;
        }

        if (CFPreferencesGetAppBooleanValue(setting, CFSTR("com.apple.Security"), NULL)) {
            UATAllowed = true;
        }
        CFRelease(setting);
    }
fail:
    return UATAllowed;
}

static bool
requirePinning(CFStringRef service)
{
    bool pinningRequired = true;

    if (SecIsInternalRelease()) {
        CFStringRef setting = CFStringCreateWithFormat(NULL, NULL, CFSTR("AppleServerAuthenticationNoPinning%@"), service);
        require(setting, fail);
        if (CFPreferencesGetAppBooleanValue(setting, CFSTR("com.apple.Security"), NULL))
            pinningRequired = false;
        CFRelease(setting);
    }
fail:
    return pinningRequired;
}

/*!
 @function SecPolicyCreateAppleServerAuthCommon
 @abstract Generic policy for server authentication Sub CAs

 Allows control for both if pinning is required at all and if UAT environments should be added
 to the trust policy.

 No pinning is for developer/QA that needs to use proxy to debug the protocol, while UAT
 environment is for QA/internal developer that have no need allow fake servers.

 Both the noPinning and allowUAT are gated on that you run on internal hardware.

 */

static SecPolicyRef
SecPolicyCreateAppleServerAuthCommon(CFStringRef hostname,
                                     CFDictionaryRef __unused context,
                                     CFStringRef service,
                                     const DERItem *leafMarkerOID,
                                     const DERItem *UATLeafMarkerOID)
{
    CFMutableDictionaryRef appleAnchorOptions = NULL;
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFDataRef oid = NULL, uatoid = NULL;

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(options, errOut);

    SecPolicyAddBasicX509Options(options);

    CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);

    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

    add_eku(options, &oidExtendedKeyUsageServerAuth);

    if (requirePinning(service)) {
        bool allowUAT = allowUATRoot(service, context);

	/*
	 * Require pinning to the Apple CA's (and if UAT environment,
	 * include the Apple Test CA's as anchors).
	 */

        appleAnchorOptions = CFDictionaryCreateMutableForCFTypes(NULL);
        require(appleAnchorOptions, errOut);

        if (allowUAT) {
            CFDictionarySetValue(appleAnchorOptions,
                                 kSecPolicyAppleAnchorIncludeTestRoots, kCFBooleanTrue);
        }

        add_element(options, kSecPolicyCheckAnchorApple, appleAnchorOptions);

	/*
	 * Check if we also should allow the UAT variant of the leafs
	 * as some variants of the UAT environment uses that instead
	 * of the test Apple CA's.
	 */

        if (allowUAT) {
            oid = CFDataCreate(kCFAllocatorDefault, leafMarkerOID->data, leafMarkerOID->length);
            require(oid, errOut);

            uatoid = CFDataCreate(kCFAllocatorDefault, UATLeafMarkerOID->data, UATLeafMarkerOID->length);
            require(oid, errOut);

            CFArrayRef array = CFArrayCreateForCFTypes(NULL, oid, uatoid, NULL);
            require(array, errOut);

            add_element(options, kSecPolicyCheckLeafMarkerOid, array);
            CFReleaseSafe(array);
        } else {
            add_leaf_marker(options, leafMarkerOID);

        }
        add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleServerAuthentication);
    }


    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

    result = SecPolicyCreate(kSecPolicyOIDSSLServer, options);
    require(result, errOut);

errOut:
    CFReleaseSafe(appleAnchorOptions);
    CFReleaseSafe(options);
    CFReleaseSafe(oid);
    CFReleaseSafe(uatoid);
    return result;
}



/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSService(CFStringRef hostname)
{
#if 1
    return SecPolicyCreateSSL(true, hostname);
#else
    return SecPolicyCreateAppleServerAuthCommon(hostname, NULL, CFSTR("IDS"),
                                                &oidAppleCertExtAppleServerAuthenticationIDSProd,
                                                &oidAppleCertExtAppleServerAuthenticationIDSTest);
#endif
}

/*!
 @function SecPolicyCreateAppleIDSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleIDSServiceContext(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, CFSTR("IDS"),
                                                &oidAppleCertExtAppleServerAuthenticationIDSProd,
                                                &oidAppleCertExtAppleServerAuthenticationIDSTest);
}

/*!
 @function SecPolicyCreateAppleGSService
 @abstract Ensure we're appropriately pinned to the GS service
 */
SecPolicyRef SecPolicyCreateAppleGSService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, CFSTR("GS"),
                                                &oidAppleCertExtAppleServerAuthenticationGS,
                                                NULL);
}

/*!
 @function SecPolicyCreateApplePushService
 @abstract Ensure we're appropriately pinned to the Push service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePushService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, CFSTR("APN"),
                                                &oidAppleCertExtAppleServerAuthenticationAPNProd,
                                                &oidAppleCertExtAppleServerAuthenticationAPNTest);
}

/*!
 @function SecPolicyCreateApplePPQService
 @abstract Ensure we're appropriately pinned to the PPQ service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateApplePPQService(CFStringRef hostname, CFDictionaryRef context)
{
    return SecPolicyCreateAppleServerAuthCommon(hostname, context, CFSTR("PPQ"),
                                                &oidAppleCertExtAppleServerAuthenticationPPQProd ,
                                                &oidAppleCertExtAppleServerAuthenticationPPQTest);
}

/* should use verbatim copy, but since this is the deprecated way, don't care right now */
static const UInt8 entrustSPKIL1C[kSecPolicySHA256Size] = {
    0x54, 0x5b, 0xf9, 0x35, 0xe9, 0xad, 0xa1, 0xda,
    0x11, 0x7e, 0xdc, 0x3c, 0x2a, 0xcb, 0xc5, 0x6f,
    0xc0, 0x28, 0x09, 0x6c, 0x0e, 0x24, 0xbe, 0x9b,
    0x38, 0x94, 0xbe, 0x52, 0x2d, 0x1b, 0x43, 0xde
};

/*!
 @function SecPolicyCreateApplePushServiceLegacy
 @abstract Ensure we're appropriately pinned to the Push service (via Entrust)
 */
SecPolicyRef SecPolicyCreateApplePushServiceLegacy(CFStringRef hostname)
{
    CFMutableDictionaryRef options = NULL;
    SecPolicyRef result = NULL;
    CFDataRef digest = NULL;

    digest = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, entrustSPKIL1C, sizeof(entrustSPKIL1C), kCFAllocatorNull);
    require(digest, errOut);

    options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    require(options, errOut);

    SecPolicyAddBasicX509Options(options);

    CFDictionaryAddValue(options, kSecPolicyCheckSSLHostname, hostname);

    CFDictionaryAddValue(options, kSecPolicyCheckBlackListedLeaf,  kCFBooleanTrue);
    CFDictionaryAddValue(options, kSecPolicyCheckGrayListedLeaf,   kCFBooleanTrue);

    CFDictionaryAddValue(options, kSecPolicyCheckIntermediateSPKISHA256, digest);

    add_eku(options, &oidExtendedKeyUsageServerAuth);

    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

    result = SecPolicyCreate(kSecPolicyOIDSSLServer, options);
    require(result, errOut);

errOut:
    CFReleaseSafe(digest);
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateAppleMMCSService
 @abstract Ensure we're appropriately pinned to the IDS service (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleMMCSService(CFStringRef hostname, CFDictionaryRef __unused context)
{
    return SecPolicyCreateSSL(true, hostname);
}

/*!
 @function SecPolicyCreateAppleSSLService
 @abstract Ensure we're appropriately pinned to an Apple server (SSL + Apple restrictions)
 */
SecPolicyRef SecPolicyCreateAppleSSLService(CFStringRef hostname)
{
	// SSL server, pinned to an Apple intermediate
	SecPolicyRef policy = SecPolicyCreateSSL(true, hostname);
	CFMutableDictionaryRef options = NULL;
	require(policy, errOut);

	// change options for SSL policy evaluation
	require((options=(CFMutableDictionaryRef)policy->_options) != NULL, errOut);

	// Apple CA anchored
	require(SecPolicyAddAppleAnchorOptions(options), errOut);

	// Check leaf for Apple Server Authentication marker oid (1.2.840.113635.100.6.27.1)
	add_leaf_marker(options, &oidAppleCertExtAppleServerAuthentication);

	// Check intermediate for Apple Server Authentication intermediate marker (1.2.840.113635.100.6.2.12)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleServerAuthentication);

	// Ensure that revocation is checked (OCSP only)
	CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);

	return policy;

errOut:
	CFReleaseSafe(options);
	CFReleaseSafe(policy);
	return NULL;
}

/*!
 @function SecPolicyCreateApplePPQSigning
 @abstract Check for intermediate certificate 'Apple System Integration 2 Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple PPQ Signing marker OID (1.2.840.113635.100.6.38.2).
 Intermediate must have marker OID (1.2.840.113635.100.6.2.10).
 */
SecPolicyRef SecPolicyCreateApplePPQSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    SecPolicyAddAppleAnchorOptions(options);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple System Integration 2 Certification Authority"));

    // Check that leaf has extension with "Apple PPQ Signing" prod oid (1.2.840.113635.100.6.38.2)
    add_leaf_marker(options, &oidAppleCertExtApplePPQSigningProd);

    // Check that intermediate has extension (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);

    add_ku(options, kSecKeyUsageDigitalSignature);

    require(result = SecPolicyCreate(kSecPolicyApplePPQSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateTestApplePPQSigning
 @abstract Check for intermediate certificate 'Apple System Integration 2 Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple PPQ Signing Test marker OID (1.2.840.113635.100.6.38.1).
 Intermediate must have marker OID (1.2.840.113635.100.6.2.10).
 */
SecPolicyRef SecPolicyCreateTestApplePPQSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    SecPolicyAddAppleAnchorOptions(options);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple System Integration 2 Certification Authority"));

    // Check that leaf has extension with "Apple PPQ Signing" test oid (1.2.840.113635.100.6.38.1)
    add_leaf_marker(options, &oidAppleCertExtApplePPQSigningTest);

    // Check that intermediate has extension (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);

    add_ku(options, kSecKeyUsageDigitalSignature);

    require(result = SecPolicyCreate(kSecPolicyAppleTestPPQSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}
/*!
 @function SecPolicyCreateAppleTimeStamping
 @abstract Check for RFC3161 timestamping EKU.
 */
SecPolicyRef SecPolicyCreateAppleTimeStamping(void)
{
	SecPolicyRef result = NULL;
	CFMutableDictionaryRef options = NULL;
	require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);

	SecPolicyAddBasicX509Options(options);

	/* Require id-kp-timeStamping extendedKeyUsage to be present. */
	add_eku(options, &oidExtendedKeyUsageTimeStamping);

	require(result = SecPolicyCreate(kSecPolicyOIDAppleTimeStamping, options), errOut);

errOut:
	CFReleaseSafe(options);
	return result;
}

/*!
 @function SecPolicyCreateAppleATVAppSigning
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple ATV App Signing marker OID (1.2.840.113635.100.6.1.24).
 Leaf cert must have 'Apple TVOS Application Signing' common name.
 */
SecPolicyRef SecPolicyCreateAppleATVAppSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddAppleAnchorOptions(options), errOut);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple Worldwide Developer Relations Certification Authority"));
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                         CFSTR("Apple TVOS Application Signing"));

    // Check that leaf has extension with "Apple ATV App Signing" prod oid (1.2.840.113635.100.6.1.24)
    add_leaf_marker(options, &oidAppleCertExtATVAppSigningProd);

    add_ku(options, kSecKeyUsageDigitalSignature);

    require(result = SecPolicyCreate(kSecPolicyAppleATVAppSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateTestAppleATVAppSigning
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations Certification Authority' by name,
 and apple anchor.
 Leaf cert must have Digital Signature usage.
 Leaf cert must have Apple ATV App Signing Test marker OID (1.2.840.113635.100.6.1.24.1).
 Leaf cert must have 'TEST Apple TVOS Application Signing TEST' common name.
 */
SecPolicyRef SecPolicyCreateTestAppleATVAppSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddAppleAnchorOptions(options), errOut);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple Worldwide Developer Relations Certification Authority"));
    CFDictionaryAddValue(options, kSecPolicyCheckSubjectCommonName,
                         CFSTR("TEST Apple TVOS Application Signing TEST"));

    // Check that leaf has extension with "Apple ATV App Signing" test oid (1.2.840.113635.100.6.1.24.1)
    add_leaf_marker(options, &oidAppleCertExtATVAppSigningTest);

    add_ku(options, kSecKeyUsageDigitalSignature);

    require(result = SecPolicyCreate(kSecPolicyAppleTestATVAppSigning, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateApplePayIssuerEncryption
 @abstract Check for intermediate certificate 'Apple Worldwide Developer Relations CA - G2' by name,
 and ECC apple anchor.
 Leaf cert must have Key Encipherment and Key Agreement usage.
 Leaf cert must have Apple Pay Issuer Encryption marker OID (1.2.840.113635.100.6.39).
 */
SecPolicyRef SecPolicyCreateApplePayIssuerEncryption(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    SecPolicyAddBasicCertOptions(options);

    require(SecPolicyAddAnchorSHA1Options(options, kAppleRootCA_ECC_SHA1), errOut);
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);

    CFDictionaryAddValue(options, kSecPolicyCheckIssuerCommonName,
                         CFSTR("Apple Worldwide Developer Relations CA - G2"));

    // Check that leaf has extension with "Apple Pay Issuer Encryption" oid (1.2.840.113635.100.6.39)
    add_leaf_marker(options, &oidAppleCertExtCryptoServicesExtEncryption);

    add_ku(options, kSecKeyUsageKeyEncipherment);

    require(result = SecPolicyCreate(kSecPolicyApplePayIssuerEncryption, options), errOut);

errOut:
    CFReleaseSafe(options);
    return result;
}

/*!
 @function SecPolicyCreateAppleATVVPNProfileSigning
 @abstract Check for leaf marker OID 1.2.840.113635.100.6.43,
 intermediate marker OID 1.2.840.113635.100.6.2.10,
 chains to Apple Root CA, path length 3
 */
SecPolicyRef SecPolicyCreateAppleATVVPNProfileSigning(void)
{
    SecPolicyRef result = NULL;
    CFMutableDictionaryRef options = NULL;
    CFMutableDictionaryRef appleAnchorOptions = NULL;
    require(options = CFDictionaryCreateMutable(kCFAllocatorDefault, 0,
                                                &kCFTypeDictionaryKeyCallBacks,
                                                &kCFTypeDictionaryValueCallBacks), errOut);
    
    SecPolicyAddBasicCertOptions(options);
    
    // Require pinning to the Apple CAs (including test CA for internal releases)
    appleAnchorOptions = CFDictionaryCreateMutableForCFTypes(NULL);
    require(appleAnchorOptions, errOut);
    
    if (SecIsInternalRelease()) {
        CFDictionarySetValue(appleAnchorOptions,
                             kSecPolicyAppleAnchorIncludeTestRoots, kCFBooleanTrue);
    }
    
    add_element(options, kSecPolicyCheckAnchorApple, appleAnchorOptions);
    
    // Cert chain length 3
    require(SecPolicyAddChainLengthOptions(options, 3), errOut);
    
    // Check leaf for Apple ATV VPN Profile Signing OID (1.2.840.113635.100.6.43)
    add_leaf_marker(options, &oidAppleCertExtATVVPNProfileSigning);
    
    // Check intermediate for Apple System Integration 2 CA intermediate marker (1.2.840.113635.100.6.2.10)
    add_oid(options, kSecPolicyCheckIntermediateMarkerOid, &oidAppleIntmMarkerAppleSystemIntg2);
    
    // Ensure that revocation is checked (OCSP only)
    CFDictionaryAddValue(options, kSecPolicyCheckRevocation, kCFBooleanFalse);
    
    require(result = SecPolicyCreate(kSecPolicyAppleATVVPNProfileSigning, options), errOut);
    
errOut:
    CFReleaseSafe(options);
    CFReleaseSafe(appleAnchorOptions);
    return result;
}
