/*
 * Copyright (c) 2006-2018 Apple Inc. All Rights Reserved.
 */

#include <AssertMacros.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <utilities/array_size.h>
#include <utilities/SecCFRelease.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>

#include "shared_regressions.h"

#include "si-23-sectrust-ocsp.h"

static void tests(void)
{
    SecTrustRef trust;
    SecCertificateRef cert0, cert1;
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _ocsp_c0, sizeof(_ocsp_c0)),
	    NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _ocsp_c1, sizeof(_ocsp_c1)),
	    NULL, "create cert1");
    CFMutableArrayRef certs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    CFArrayAppendValue(certs, cert0);
    CFArrayAppendValue(certs, cert1);

    SecPolicyRef sslPolicy = SecPolicyCreateSSL(true, CFSTR("www.apple.com"));
    SecPolicyRef ocspPolicy = SecPolicyCreateRevocation(kSecRevocationOCSPMethod);
    const void *v_policies[] = { sslPolicy, ocspPolicy };
    CFArrayRef policies = CFArrayCreate(NULL, v_policies,
	array_size(v_policies), &kCFTypeArrayCallBacks);
    CFRelease(sslPolicy);
    CFRelease(ocspPolicy);
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust),
        "create trust");
    /* April 14, 2019 at 10:46:40 PM PDT */
    CFDateRef date = CFDateCreate(NULL, 577000000.0);
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    is(SecTrustGetVerifyTime(trust), 577000000.0, "get date");

    SecTrustResultType trustResult;
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

    /* Certificates are only EV if they are also CT. */
    CFDictionaryRef info = SecTrustCopyInfo(trust);
    CFBooleanRef ev = (CFBooleanRef)CFDictionaryGetValue(info,
        kSecTrustInfoExtendedValidationKey);
    ok(ev, "extended validation succeeded");

    CFReleaseSafe(info);
    CFReleaseSafe(trust);
    CFReleaseSafe(policies);
    CFReleaseSafe(certs);
    CFReleaseSafe(cert0);
    CFReleaseSafe(cert1);
    CFReleaseSafe(date);
}

static void test_ocsp_responder_policy() {
    SecCertificateRef leaf = NULL, subCA = NULL, responderCert = NULL;
    CFMutableArrayRef certs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                   &kCFTypeArrayCallBacks);
    SecTrustRef trust = NULL;
    SecPolicyRef ocspSignerPolicy = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    /* August 14, 2018 at 9:26:40 PM PDT */
    CFDateRef date = CFDateCreate(NULL, 556000000.0);

    isnt(leaf = SecCertificateCreateWithBytes(NULL, valid_ist_certificate,
                                                       sizeof(valid_ist_certificate)), NULL, "create ist leaf");
    isnt(subCA = SecCertificateCreateWithBytes(NULL, ist_intermediate_certificate,
                                                       sizeof(ist_intermediate_certificate)), NULL, "create ist subCA");
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, subCA);

    ok(ocspSignerPolicy = SecPolicyCreateOCSPSigner(),
       "create ocspSigner policy");

    ok_status(SecTrustCreateWithCertificates(certs, ocspSignerPolicy, &trust),
              "create trust for c0 -> c1");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
              "trust is kSecTrustResultRecoverableTrustFailure");

    isnt(responderCert = SecCertificateCreateWithBytes(NULL, _responderCert,
                                                       sizeof(_responderCert)), NULL, "create responderCert");
    CFArraySetValueAtIndex(certs, 0, responderCert);
    ok_status(SecTrustCreateWithCertificates(certs, ocspSignerPolicy, &trust),
              "create trust for ocspResponder -> c1");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
              "trust is kSecTrustResultUnspecified");

    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(responderCert);
    CFReleaseNull(certs);
    CFReleaseNull(trust);
    CFReleaseSafe(ocspSignerPolicy);
    CFReleaseNull(date);
}

static void test_revocation() {
    SecTrustRef trust;
    SecCertificateRef rcert0, rcert1;
    isnt(rcert0 = SecCertificateCreateWithBytes(NULL,
         revoked_ist_certificate, sizeof(revoked_ist_certificate)),
         NULL, "create rcert0");
    isnt(rcert1 = SecCertificateCreateWithBytes(NULL,
         ist_intermediate_certificate, sizeof(ist_intermediate_certificate)),
         NULL, "create rcert1");
    CFMutableArrayRef rcerts = CFArrayCreateMutable(kCFAllocatorDefault, 0,
                                                   &kCFTypeArrayCallBacks);
    CFArrayAppendValue(rcerts, rcert0);
    CFArrayAppendValue(rcerts, rcert1);

    SecPolicyRef sslPolicy = SecPolicyCreateSSL(true, CFSTR("revoked.geotrust-global-ca.test-pages.certificatemanager.apple.com"));
    SecPolicyRef ocspPolicy = SecPolicyCreateRevocation(kSecRevocationOCSPMethod);
    const void *v_policies[] = { sslPolicy, ocspPolicy };
    CFArrayRef policies = CFArrayCreate(NULL, v_policies,
                                        array_size(v_policies), &kCFTypeArrayCallBacks);
    CFRelease(sslPolicy);
    CFRelease(ocspPolicy);
    ok_status(SecTrustCreateWithCertificates(rcerts, policies, &trust),
              "create trust");
    /* Feb 5th 2015. */
    CFDateRef date = CFDateCreate(NULL, 444900000);
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    CFReleaseSafe(date);

    is(SecTrustGetVerifyTime(trust), 444900000, "get date");

    SecTrustResultType trustResult;
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is((trustResult > kSecTrustResultUnspecified), true,
       "trust is %d, expected value greater than 4", (int)trustResult);
    CFDictionaryRef results = SecTrustCopyResult(trust);
    CFTypeRef revoked = NULL;
    if (results) {
        CFArrayRef perCertResults = CFDictionaryGetValue(results, CFSTR("TrustResultDetails"));
        if (perCertResults) {
            CFDictionaryRef leafResults = CFArrayGetValueAtIndex(perCertResults, 0);
            if (leafResults) {
                revoked = CFDictionaryGetValue(leafResults, CFSTR("Revocation"));
            }
        }
    }
    is(revoked != NULL, true, "revoked result is %@", revoked);
    CFReleaseSafe(results);


    /* Now verify the cert at a date in the past relative to the previous
       date, but still within the cert's validity period. Although the
       cached response from our prior attempt will appear to have been
       produced in the future, it should still be honored since it's
       validly signed.
     */
    /* Dec 11th 2014. */
    date = CFDateCreate(NULL, 440000000);
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    CFReleaseSafe(date);

    is(SecTrustGetVerifyTime(trust), 440000000, "get date");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is((trustResult > kSecTrustResultUnspecified), true,
              "trust is %d, expected value greater than 4", (int)trustResult);
    results = SecTrustCopyResult(trust);
    revoked = NULL;
    if (results) {
        CFArrayRef perCertResults = CFDictionaryGetValue(results, CFSTR("TrustResultDetails"));
        if (perCertResults) {
            CFDictionaryRef leafResults = CFArrayGetValueAtIndex(perCertResults, 0);
            if (leafResults) {
                revoked = CFDictionaryGetValue(leafResults, CFSTR("Revocation"));
            }
        }
    }
    is(revoked != NULL, true, "revoked result is %@", revoked);
    CFReleaseSafe(results);

    CFReleaseSafe(trust);
    CFReleaseSafe(policies);
    CFReleaseSafe(rcerts);
    CFReleaseSafe(rcert0);
    CFReleaseSafe(rcert1);
}

static void test_forced_revocation()
{
    /*
     *  Test verification requiring a positive response from the revocation server
     */

    OSStatus status;
    SecCertificateRef smime_leaf_cert;
    SecCertificateRef smime_CA_cert;
    SecCertificateRef smime_root_cert;

    // Import certificates from byte array above
    isnt(smime_leaf_cert = SecCertificateCreateWithBytes(NULL, ocsp_smime_leaf_certificate, sizeof(ocsp_smime_leaf_certificate)),
         NULL, "SMIME Leaf Cert");
    isnt(smime_CA_cert   = SecCertificateCreateWithBytes(NULL, ocsp_smime_CA_certificate, sizeof(ocsp_smime_CA_certificate)),
         NULL, "SMIME CA Cert");
    isnt(smime_root_cert = SecCertificateCreateWithBytes(NULL, ocsp_smime_root_certificate, sizeof(ocsp_smime_root_certificate)),
         NULL, "SMIME Root Cert");

    SecPolicyRef smimePolicy = SecPolicyCreateWithProperties(kSecPolicyAppleSMIME, NULL);
    SecPolicyRef revocPolicy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod | kSecRevocationRequirePositiveResponse);
    isnt(smimePolicy, NULL, "SMIME Policy");
    isnt(revocPolicy, NULL, "SMIME Revocation Policy");

    // Default Policies
    CFMutableArrayRef SMIMEDefaultPolicy = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(SMIMEDefaultPolicy, smimePolicy);

    // Default Policies + explicit revocation
    CFMutableArrayRef SMIMEDefaultPolicyWithRevocation = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(SMIMEDefaultPolicyWithRevocation, smimePolicy);
    CFArrayAppendValue(SMIMEDefaultPolicyWithRevocation, revocPolicy);

    // Valid chain of Cert (leaf + CA)
    CFMutableArrayRef SMIMECertChain = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(SMIMECertChain, smime_leaf_cert);
    CFArrayAppendValue(SMIMECertChain, smime_CA_cert);

    // Valid anchor certs
    CFMutableArrayRef SMIMEAnchors = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(SMIMEAnchors, smime_root_cert);

    // Free Resources contained in arrays
    CFReleaseSafe(smime_leaf_cert);
    CFReleaseSafe(smime_CA_cert);
    CFReleaseSafe(smime_root_cert);
    CFReleaseSafe(smimePolicy);
    CFReleaseSafe(revocPolicy);

    CFDateRef VerifyDate;
    isnt(VerifyDate = CFDateCreate(NULL, 332900000.0), NULL, "Create verify date");
    if (!VerifyDate) { goto errOut; }

    // Standard evaluation for the given verify date
    {
        SecTrustRef trust = NULL;
        SecTrustResultType trust_result;

        ok_status(status = SecTrustCreateWithCertificates(SMIMECertChain, SMIMEDefaultPolicy, &trust),
                  "SecTrustCreateWithCertificates");
        ok_status(SecTrustSetVerifyDate(trust, VerifyDate), "Set date");
        ok_status(SecTrustSetAnchorCertificates(trust, SMIMEAnchors), "Set anchors");

        ok_status(status = SecTrustEvaluate(trust, &trust_result), "SecTrustEvaluate");

        // Check results
        // NOTE: We now expect a fatal error, since the "TC TrustCenter Class 1 L1 CA IX" CA
        // is revoked. That CA is no longer present in Valid since the TC root was removed
        // from the trust store, and as of May 2018, its OCSP server no longer resolves in DNS.
        // However, the OCSP URI for the CA's issuer is still active and reports the CA as revoked.
        //
        is_status(trust_result, kSecTrustResultFatalTrustFailure, "trust is kSecTrustResultFatalTrustFailure");

        CFReleaseNull(trust);
    }

    // Revocation-required evaluation should fail, since this CA's servers no longer exist
    // and no valid responses are available
    {
        SecTrustRef trust = NULL;
        SecTrustResultType trust_result;

        ok_status(status = SecTrustCreateWithCertificates(SMIMECertChain, SMIMEDefaultPolicyWithRevocation, &trust),
                  "SecTrustCreateWithCertificates");
        ok_status(SecTrustSetVerifyDate(trust, VerifyDate), "Set date");
        ok_status(SecTrustSetAnchorCertificates(trust, SMIMEAnchors), "Set anchors");

        ok_status(status = SecTrustEvaluate(trust, &trust_result), "SecTrustEvaluate");

        // Check results
        // NOTE: We now expect a fatal error, since the "TC TrustCenter Class 1 L1 CA IX" CA
        // is revoked. That CA is no longer present in Valid since the TC root was removed
        // from the trust store, and as of May 2018, its OCSP server no longer resolves in DNS.
        // However, the OCSP URI for the CA's issuer is still active and reports the CA as revoked.
        //
        is_status(trust_result, kSecTrustResultFatalTrustFailure, "trust is kSecTrustResultFatalTrustFailure");

        CFReleaseNull(trust);
    }

    // Free remaining resources
errOut:
    CFReleaseSafe(VerifyDate);
    CFReleaseSafe(SMIMEDefaultPolicy);
    CFReleaseSafe(SMIMEDefaultPolicyWithRevocation);
    CFReleaseSafe(SMIMECertChain);
    CFReleaseSafe(SMIMEAnchors);
}

#if 0
static void hexdump(const uint8_t *bytes, size_t len) {
	size_t ix;
	printf("#anchor-sha1: ");
	for (ix = 0; ix < len; ++ix) {
		printf("%02X", bytes[ix]);
	}
	printf("\n");
}

static void datadump(const uint8_t *bytes, size_t len) {
	size_t ix;
	printf("#anchor-sha1: ");
	for (ix = 0; ix < len; ++ix) {
		printf("%c", bytes[ix]);
	}
	printf("\n");
}

static void display_anchor_digest(SecTrustRef trust) {
    CFIndex count = SecTrustGetCertificateCount(trust);
    SecCertificateRef anchor = SecTrustGetCertificateAtIndex(trust, count - 1);
    CFDataRef digest = SecCertificateGetSHA1Digest(anchor);
    CFDataRef xml = CFPropertyListCreateXMLData(NULL, digest);
    datadump(CFDataGetBytePtr(xml), CFDataGetLength(xml));
}
#endif

static void test_aia(void) {
    SecCertificateRef ovh = NULL, comodo_ev = NULL, comodo_aia = NULL;
    CFMutableArrayRef certs = NULL, policies = NULL;
    SecPolicyRef sslPolicy = NULL, revPolicy = NULL;
    CFDateRef verifyDate = NULL;
    CFDictionaryRef info = NULL;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    CFBooleanRef ev = NULL;

    /* Initialize common variables */
    isnt(ovh = SecCertificateCreateWithBytes(NULL, ovh_certificate,
        sizeof(ovh_certificate)), NULL, "create ovh cert");
    isnt(comodo_ev = SecCertificateCreateWithBytes(NULL, comodo_ev_certificate,
        sizeof(comodo_ev_certificate)), NULL, "create comodo_ev cert");
    isnt(comodo_aia = SecCertificateCreateWithBytes(NULL,
        comodo_aia_certificate, sizeof(comodo_aia_certificate)), NULL,
        "create comodo_aia cert");
    certs = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    policies = CFArrayCreateMutable(kCFAllocatorDefault, 0,
        &kCFTypeArrayCallBacks);
    sslPolicy = SecPolicyCreateSSL(false, NULL); // For now, use SSL client policy to avoid SHA-1 deprecation
    revPolicy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
    CFArrayAppendValue(policies, sslPolicy);
    CFArrayAppendValue(policies, revPolicy);
    /* May 9th 2018. */
    verifyDate = CFDateCreate(NULL, 547600500);

    /* First run with no intermediate and disallow network fetching.
     * Evaluation should fail because it couldn't get the intermediate. */
    CFArrayAppendValue(certs, ovh);
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust),
              "create trust");
    ok_status(SecTrustSetVerifyDate(trust, verifyDate), "set date");
    ok_status(SecTrustSetNetworkFetchAllowed(trust, false), "set no network");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
       "trust is kSecTrustResultRecoverableTrustFailure");

    /* Now allow networking. Evaluation should succeed after fetching
     * the intermediate. */
    ok_status(SecTrustSetNetworkFetchAllowed(trust, true), "set allow network");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
       "trust is kSecTrustResultUnspecified");
    CFReleaseNull(trust);

    /* Now run with the intermediate returned by the ssl server. */
    CFArrayAppendValue(certs, comodo_ev);
    ok_status(SecTrustCreateWithCertificates(certs, sslPolicy, &trust),
        "create trust");
    ok_status(SecTrustSetVerifyDate(trust, verifyDate), "set date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");
    info = SecTrustCopyInfo(trust);
    ev = (CFBooleanRef)CFDictionaryGetValue(info,
        kSecTrustInfoExtendedValidationKey);
    ok(ev, "extended validation succeeded due to caissuers fetch");
    //display_anchor_digest(trust);
    CFReleaseSafe(info);
    CFReleaseSafe(trust);

    /* Now run with the intermediate returned by following the url in the
       Certificate Access Information Authority (AIA) extension of the ovh
       leaf certificate. */
    CFArrayAppendValue(certs, comodo_aia);
    ok_status(SecTrustCreateWithCertificates(certs, sslPolicy, &trust),
        "re-create trust with aia intermediate");
    ok_status(SecTrustSetVerifyDate(trust, verifyDate), "set date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");
    info = SecTrustCopyInfo(trust);
    ev = (CFBooleanRef)CFDictionaryGetValue(info,
        kSecTrustInfoExtendedValidationKey);
    ok(ev, "extended validation succeeded");
    //display_anchor_digest(trust);
    CFReleaseSafe(info);
    CFReleaseSafe(trust);

    /* Now run with the intermediate returned by following the url in the
       Certificate Access Information Authority (AIA) extension of the ovh
       leaf certificate. */
    CFArrayRemoveValueAtIndex(certs, 1);
    ok_status(SecTrustCreateWithCertificates(certs, sslPolicy, &trust),
        "re-create trust with aia intermediate");
    ok_status(SecTrustSetVerifyDate(trust, verifyDate), "set date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");
    info = SecTrustCopyInfo(trust);
    ev = (CFBooleanRef)CFDictionaryGetValue(info,
        kSecTrustInfoExtendedValidationKey);
    ok(ev, "extended validation succeeded");
    //display_anchor_digest(trust);
    CFReleaseSafe(info);
    CFReleaseSafe(trust);

    /* Common variable cleanup. */
    CFReleaseSafe(sslPolicy);
    CFReleaseSafe(revPolicy);
    CFReleaseSafe(certs);
    CFReleaseSafe(policies);
    CFReleaseSafe(comodo_aia);
    CFReleaseSafe(comodo_ev);
    CFReleaseSafe(ovh);
    CFReleaseSafe(verifyDate);
}

static void test_aia_https(void) {
    SecCertificateRef leaf = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _caissuer_https, sizeof(_caissuer_https));
    const void *v_certs[] = { leaf };

    certs = CFArrayCreate(NULL, v_certs, 1, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    verifyDate = CFDateCreate(NULL, 546700000.0); // April 29, 2018 at 6:06:40 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    /* Evaluate trust. This cert does not chain to anything trusted and we can't fetch an
     * intermediate because the URI is https. */
    is(SecTrustEvaluateWithError(trust, &error), false, "leaf with missing intermediate and https CAIssuer URI succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCreateChainFailed, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCreateChainFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
#pragma clang diagnostic pop

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(certs);
    CFReleaseNull(verifyDate);
    CFReleaseNull(error);
}

static void test_results_dictionary_revocation_reason(void) {
    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;
    CFDataRef ocspResponse = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyRevokedLeaf, sizeof(_probablyRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _digiCertSha2SubCA, sizeof(_digiCertSha2SubCA));
    root = SecCertificateCreateWithBytes(NULL, _digiCertGlobalRoot, sizeof(_digiCertGlobalRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(true, CFSTR("revoked.badssl.com"));
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Set the stapled response */
    ocspResponse = CFDataCreate(NULL, _digicertOCSPResponse, sizeof(_digicertOCSPResponse));
    ok_status(SecTrustSetOCSPResponse(trust, ocspResponse), "failed to set OCSP response");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    /* Evaluate trust. This cert is revoked, but is only listed as "probably revoked" by valid.apple.com.
     * This cert should come back as revoked. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);

        /* Verify that the results dictionary contains all the right keys for a revoked cert */
        CFDictionaryRef result = SecTrustCopyResult(trust);
        isnt(result, NULL, "failed to copy result dictionary");
        if (result) {
            int64_t reason = -1;
            CFNumberRef cfreason = CFNumberCreate(NULL, kCFNumberSInt64Type, &reason);
            is(CFNumberCompare(cfreason, CFDictionaryGetValue(result, kSecTrustRevocationReason), NULL), kCFCompareEqualTo, "expected revocation reason -1");
            CFReleaseNull(cfreason);
        }
        CFReleaseNull(result);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
#pragma clang diagnostic pop

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(root);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(verifyDate);
    CFReleaseNull(error);
    CFReleaseNull(ocspResponse);
}

static void test_results_dictionary_revocation_checked(void) {
    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef sslPolicy = NULL, ocspPolicy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL, policies = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _ocsp_c0, sizeof(_ocsp_c0));
    subCA = SecCertificateCreateWithBytes(NULL, _ocsp_c1, sizeof(_ocsp_c1));
    root = SecCertificateCreateWithBytes(NULL, _ocsp_c2, sizeof(_ocsp_c2));

    sslPolicy = SecPolicyCreateSSL(true, CFSTR("www.apple.com"));
    ocspPolicy = SecPolicyCreateRevocation(kSecRevocationOCSPMethod);

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };
    const void *v_policies[] = { sslPolicy, ocspPolicy };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policies = CFArrayCreate(NULL, v_policies, 2, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustCreateWithCertificates(certs, policies, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 577000000.0); // April 14, 2019 at 10:46:40 PM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    is(SecTrustEvaluateWithError(trust, &error), true, "valid cert failed");

    /* Verify that the results dictionary contains all the right keys for a valid cert where revocation checked */
    CFDictionaryRef result = SecTrustCopyResult(trust);
    isnt(result, NULL, "failed to copy result dictionary");
    if (result) {
        is(CFDictionaryGetValue(result, kSecTrustRevocationChecked), kCFBooleanTrue, "expected revocation checked flag");
        CFDateRef validUntil = CFDictionaryGetValue(result, kSecTrustRevocationValidUntilDate);
        isnt(validUntil, NULL, "expected revocation valid until date");
        if (validUntil) {
            ok(CFDateGetAbsoluteTime(validUntil) > CFAbsoluteTimeGetCurrent(), "expected valid until date in the future");
        } else {
            fail("did not get valid until date");
        }
    }
    CFReleaseNull(result);
#pragma clang diagnostic pop

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(root);
    CFReleaseNull(ocspPolicy);
    CFReleaseNull(sslPolicy);
    CFReleaseNull(trust);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(policies);
    CFReleaseNull(verifyDate);
    CFReleaseNull(error);
}

static int ping_host(char *host_name){

    struct sockaddr_in pin;
    struct hostent *nlp_host;
    int sd;
    int port;
    int retries = 5;

    port=80;

    //tries 5 times then give up
    while ((nlp_host=gethostbyname(host_name))==0 && retries--){
        printf("Resolve Error! (%s) %d\n", host_name, h_errno);
        sleep(1);
    }

    if(nlp_host==0)
        return 0;

    bzero(&pin,sizeof(pin));
    pin.sin_family=AF_INET;
    pin.sin_addr.s_addr=htonl(INADDR_ANY);
    pin.sin_addr.s_addr=((struct in_addr *)(nlp_host->h_addr))->s_addr;
    pin.sin_port=htons(port);

    sd=socket(AF_INET,SOCK_STREAM,0);

    if (connect(sd,(struct sockaddr*)&pin,sizeof(pin))==-1){
        printf("connect error! (%s) %d\n", host_name, errno);
        close(sd);
        return 0;
    }
    else{
        close(sd);
        return 1;
    }
}

int si_23_sectrust_ocsp(int argc, char *const *argv)
{
    char *hosts[] = {
        "EVSecure-ocsp.verisign.com",
        "EVIntl-ocsp.verisign.com",
        "EVIntl-aia.verisign.com",
        "ocsp.comodoca.com",
        "crt.comodoca.com",
        "ocsp.entrust.net",
        "ocsp.digicert.com",
    };

    unsigned host_cnt = 0;

    plan_tests(105);

    for (host_cnt = 0; host_cnt < sizeof(hosts)/sizeof(hosts[0]); host_cnt ++) {
        if(!ping_host(hosts[host_cnt])) {
            printf("Accessing specific server (%s) failed, check the network!\n", hosts[host_cnt]);
            return 0;
        }
    }

    tests();
    test_ocsp_responder_policy();
    test_aia();
    test_aia_https();
    test_revocation();
    test_forced_revocation();
    test_results_dictionary_revocation_reason();
    test_results_dictionary_revocation_checked();

    return 0;
}
