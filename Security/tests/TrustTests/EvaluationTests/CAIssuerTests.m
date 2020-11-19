/*
* Copyright (c) 2006-2019 Apple Inc. All Rights Reserved.
*/

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <utilities/array_size.h>
#include <utilities/SecCFRelease.h>

#import "../TestMacroConversions.h"
#import "../TrustEvaluationTestHelpers.h"
#import "TrustEvaluationTestCase.h"

#import "CAIssuerTests_data.h"

@interface CAIssuerTests: TrustEvaluationTestCase
@end

@implementation CAIssuerTests

#if !TARGET_OS_WATCH && !TARGET_OS_BRIDGE
- (void) test_aia
{
    if (!ping_host("crt.comodoca.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    SecCertificateRef ovh = NULL, comodo_ev = NULL, comodo_aia = NULL;
    CFMutableArrayRef certs = NULL, policies = NULL;
    SecPolicyRef sslPolicy = NULL, revPolicy = NULL;
    CFDateRef verifyDate = NULL;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

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
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
       "trust is kSecTrustResultRecoverableTrustFailure");

    /* Now allow networking. Evaluation should succeed after fetching
     * the intermediate. */
    ok_status(SecTrustSetNetworkFetchAllowed(trust, true), "set allow network");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
       "trust is kSecTrustResultUnspecified");
    CFReleaseNull(trust);

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

- (void) test_aia_https {
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

    /* Evaluate trust. This cert does not chain to anything trusted and we can't fetch an
     * intermediate because the URI is https. */
    is(SecTrustEvaluateWithError(trust, &error), false, "leaf with missing intermediate and https CAIssuer URI succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCreateChainFailed, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCreateChainFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(certs);
    CFReleaseNull(verifyDate);
    CFReleaseNull(error);
}
#else /* TARGET_OS_WATCH || TARGET_OS_BRIDGE */
- (void) testNoNetworking
{
    SecCertificateRef ovh = NULL, comodo_ev = NULL, comodo_aia = NULL;
    CFMutableArrayRef certs = NULL, policies = NULL;
    SecPolicyRef sslPolicy = NULL, revPolicy = NULL;
    CFDateRef verifyDate = NULL;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

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

    /* Evaluation should fail because it couldn't get the intermediate. */
    CFArrayAppendValue(certs, ovh);
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust),
              "create trust");
    ok_status(SecTrustSetVerifyDate(trust, verifyDate), "set date");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
       "trust is kSecTrustResultRecoverableTrustFailure");

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
#endif

@end
