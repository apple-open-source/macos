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
#include "trust/trustd/SecOCSPCache.h"

#import "../TestMacroConversions.h"
#import "../TrustEvaluationTestHelpers.h"
#import "TrustEvaluationTestCase.h"

#include "RevocationTests_data.h"

@interface RevocationTests : TrustEvaluationTestCase
@end

@implementation RevocationTests

- (void)setUp
{
    // Delete the OCSP cache between each test
    [super setUp];
    SecOCSPCacheDeleteContent(nil);
}

#if !TARGET_OS_WATCH && !TARGET_OS_BRIDGE
/* watchOS and bridgeOS don't support networking in trustd */
- (void)testRevocation
{
    if (!ping_host("ocsp.digicert.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

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
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
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

- (void) test_ocsp_responder_policy
{
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
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
              "trust is kSecTrustResultRecoverableTrustFailure");

    isnt(responderCert = SecCertificateCreateWithBytes(NULL, _responderCert,
                                                       sizeof(_responderCert)), NULL, "create responderCert");
    CFArraySetValueAtIndex(certs, 0, responderCert);
    ok_status(SecTrustCreateWithCertificates(certs, ocspSignerPolicy, &trust),
              "create trust for ocspResponder -> c1");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
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

- (void)test_always_honor_cached_revoked_responses {
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

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
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is(trustResult, kSecTrustResultFatalTrustFailure);
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

    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is(trustResult, kSecTrustResultFatalTrustFailure);
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

- (void) test_require_positive_response
{
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL, revocationPolicy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    revocationPolicy = SecPolicyCreateRevocation(kSecRevocationRequirePositiveResponse | kSecRevocationOCSPMethod);
    NSArray *policies = @[ (__bridge id)policy, (__bridge id)revocationPolicy ];
    require_noerr_action(SecTrustCreateWithCertificates(certs, (__bridge CFArrayRef)policies, &trust), errOut,
                         fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Set no fetch allowed */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. Since we required a response but disabled networking, should fail. */
    is(SecTrustEvaluateWithError(trust, &error), false, "non-definitive revoked cert without network failed");
    if (error) {
        is(CFErrorGetCode(error), errSecIncompleteCertRevocationCheck, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecIncompleteCertRevocationCheck);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
    CFReleaseNull(error);

    /* Set fetch allowed */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, true), errOut, fail("failed to set network fetch allowed"));

    /* Evaluate trust. We should re-do the evaluation and get a revoked failure from the OCSP check. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with network succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

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
}

- (void) test_set_fetch_allowed {
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Set no fetch allowed */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. This cert is revoked, but is only listed as "probably not revoked" by valid.apple.com.
     * Since network fetch is not allowed and we fail open, this cert should come back as trusted. */
    ok(SecTrustEvaluateWithError(trust, &error), "non-definitive revoked cert without network failed");
    CFReleaseNull(error);

    /* Set fetch allowed */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, true), errOut, fail("failed to set network fetch allowed"));

    /* Evaluate trust. SetFetchAllowed should have reset the trust result, so now we should re-do the evaluation and get a revoked failure. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with network succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

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
}

- (void) test_check_if_trusted {
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef codesigningPolicy = NULL, revocationPolicy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL, policies = NULL;
    CFDateRef verifyDate = NULL, badVerifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    codesigningPolicy = SecPolicyCreateAppleExternalDeveloper();
    revocationPolicy = SecPolicyCreateRevocation(kSecRevocationCheckIfTrusted);

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };
    const void *v_policies[] = { codesigningPolicy, revocationPolicy };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policies = CFArrayCreate(NULL, v_policies, 2, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustCreateWithCertificates(certs, policies, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));
    badVerifyDate = CFDateCreate(NULL, 490000000.0);  // July 12, 2016 at 12:06:40 AM PDT (before cert issued)
    require_noerr_action(SecTrustSetVerifyDate(trust, badVerifyDate), errOut, fail("failed to set verify date"));

    /* Set no fetch allowed */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. This cert is revoked, but is only listed as "probably not revoked" by valid.apple.com.
     * Since we are evaluating it at a time before it was issued, it should come back as untrusted
     * due to the temporal validity failure, but not due to revocation since we couldn't check for this
     * untrusted chain. */
    is(SecTrustEvaluateWithError(trust, &error), false, "not yet valid cert succeeded trust evaluation");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateExpired, "got wrong error code for expired cert");
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
    CFReleaseNull(error);

    /* Set verify date within validity period */
    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Evaluate trust. Now that we trust the chain, we should do a revocation check and get a revocation failure. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with network succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(root);
    CFReleaseNull(codesigningPolicy);
    CFReleaseNull(revocationPolicy);
    CFReleaseNull(trust);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(policies);
    CFReleaseNull(verifyDate);
    CFReleaseNull(badVerifyDate);
    CFReleaseNull(error);
}

- (void) test_cache {
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Evaluate trust. This cert is revoked, but is only listed as "probably not revoked" by valid.apple.com.
     * This cert should come back as revoked after a network-based fetch. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with network succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

    /* Set no fetch allowed, so we're relying on the cached response from above */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. Cached response should tell us that it's revoked. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with cached response succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

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
}

- (void)test_revoked_responses_not_flushed_from_cache
{
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Evaluate trust. This cert is revoked, but is only listed as "probably not revoked" by valid.apple.com.
     * This cert should come back as revoked after a network-based fetch. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with network succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

    /* Set no fetch allowed, so we're relying on the cached response from above */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. Cached response should tell us that it's revoked. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with cached response succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

    /* flush the cache and reset the turst, the revoked response should still be present afterwards */
    XCTAssert(SecTrustFlushResponseCache(NULL));
    SecTrustSetNeedsEvaluation(trust);

    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with cached response succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

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
}

- (void) test_results_dictionary_revocation_checked {
    if (!ping_host("ocsp.digicert.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

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

- (void) test_revocation_checked_via_cache {
    if (!ping_host("ocsp.digicert.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

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

    is(SecTrustEvaluateWithError(trust, &error), true, "valid cert failed");

    /* Set no fetch allowed, so we're relying on the cached response from above */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. Cached response should tell us that it's revoked. */
    is(SecTrustEvaluateWithError(trust, &error), true, "valid cert failed");

    /* Verify that the results dictionary contains the kSecTrustRevocationChecked key for a valid cert where revocation checked */
    CFDictionaryRef result = SecTrustCopyResult(trust);
    isnt(result, NULL, "failed to copy result dictionary");
    if (result) {
        is(CFDictionaryGetValue(result, kSecTrustRevocationChecked), kCFBooleanTrue, "expected revocation checked flag");
    }
    CFReleaseNull(result);

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

#else  /* TARGET_OS_WATCH || TARGET_OS_BRIDGE */
- (void)testNoNetworking
{
    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Evaluate trust. Since we aren't allowed to do networking (and this cert is only "Probably Not Revoked" in Valid),
     * we shouldn't see this cert as revoked */
    is(SecTrustEvaluateWithError(trust, &error), true, "revoked cert with no network failed");

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
}
#endif /* !TARGET_OS_WATCH && !TARGET_OS_BRIDGE */

#if !TARGET_OS_BRIDGE
/* bridgeOS doesn't use Valid */
- (NSNumber *)runRevocationCheckNoNetwork:(SecCertificateRef)leaf
                                    subCA:(SecCertificateRef)subCA
{
    CFArrayRef anchors = NULL;
    SecPolicyRef smimePolicy = NULL, revocationPolicy = NULL;
    CFArrayRef certs = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFErrorRef error = NULL;
    NSArray *policies = nil;
    NSDictionary *result = nil;
    NSNumber *revocationChecked = nil;

    const void *v_certs[] = { leaf };
    require_action(certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks), errOut,
                   fail("unable to create certificates array"));
    require_action(anchors = CFArrayCreate(NULL, (const void **)&subCA, 1, &kCFTypeArrayCallBacks), errOut,
                   fail("unable to create anchors array"));

    require_action(smimePolicy = SecPolicyCreateSMIME(kSecSignSMIMEUsage, NULL), errOut, fail("unable to create policy"));
    revocationPolicy = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod | kSecRevocationCheckIfTrusted);
    policies = @[(__bridge id)smimePolicy, (__bridge id)revocationPolicy];
    ok_status(SecTrustCreateWithCertificates(certs, (__bridge CFArrayRef)policies, &trust), "failed to create trust");
    ok_status(SecTrustSetNetworkFetchAllowed(trust, false), "SecTrustSetNetworkFetchAllowed failed");

    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut,
                         fail("unable to set anchors"));
    ok(SecTrustEvaluateWithError(trust, &error), "Not trusted");
    result = CFBridgingRelease(SecTrustCopyResult(trust));
    revocationChecked = result[(__bridge NSString *)kSecTrustRevocationChecked];

errOut:
    CFReleaseNull(anchors);
    CFReleaseNull(smimePolicy);
    CFReleaseNull(revocationPolicy);
    CFReleaseNull(certs);
    CFReleaseNull(trust);
    CFReleaseNull(date);
    CFReleaseNull(error);

    return revocationChecked;
}

- (void) test_revocation_checked_via_valid {
    SecCertificateRef leaf = NULL, subCA = NULL;
    NSNumber *revocationChecked = NULL;

    require_action(leaf = SecCertificateCreateWithBytes(NULL, _leaf_sha256_valid_cav2_complete_ok1, sizeof(_leaf_sha256_valid_cav2_complete_ok1)), errOut,
                   fail("unable to create cert"));
    require_action(subCA = SecCertificateCreateWithBytes(NULL, _ca_sha256_valid_cav2_complete, sizeof(_ca_sha256_valid_cav2_complete)), errOut, fail("unable to create cert"));

    revocationChecked = [self runRevocationCheckNoNetwork:leaf
                                                    subCA:subCA];
    XCTAssert(revocationChecked != NULL, "kSecTrustRevocationChecked is not in the result dictionary");

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
}

- (void) test_revocation_not_checked_no_network {
    /* The intermediate does not have the noCAv2 flag and is "probably not revoked,", so
       kSecTrustRevocationChecked should not be in the results dictionary */
    SecCertificateRef leaf = NULL, subCA = NULL;
    NSNumber *revocationChecked = NULL;

    require_action(leaf = SecCertificateCreateWithBytes(NULL, _leaf_serial_invalid_incomplete_ok1, sizeof(_leaf_serial_invalid_incomplete_ok1)), errOut,
                   fail("unable to create cert"));
    require_action(subCA = SecCertificateCreateWithBytes(NULL, _ca_serial_invalid_incomplete, sizeof(_ca_serial_invalid_incomplete)), errOut, fail("unable to create cert"));

    revocationChecked = [self runRevocationCheckNoNetwork:leaf
                                                    subCA:subCA];
    XCTAssert(revocationChecked == NULL, "kSecTrustRevocationChecked is in the result dictionary");

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
}
#endif /* !TARGET_OS_BRIDGE */

/* bridgeOS and watchOS do not support networked OCSP but do support stapling */
- (void) test_stapled_revoked_response {
    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;
    CFDataRef ocspResponse = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Set the stapled response */
    ocspResponse = CFDataCreate(NULL, _devID_OCSPResponse, sizeof(_devID_OCSPResponse));
    ok_status(SecTrustSetOCSPResponse(trust, ocspResponse), "failed to set OCSP response");

    /* Set no fetch allowed, so we're relying on the stapled response from above */
    require_noerr_action(SecTrustSetNetworkFetchAllowed(trust, false), errOut, fail("failed to set network fetch disallowed"));

    /* Evaluate trust. This cert is revoked, but is only listed as "probably not revoked" by valid.apple.com.
     * This cert should come back as revoked because of the stapled revoked response. */
    is(SecTrustEvaluateWithError(trust, &error), false, "revoked cert with stapled response succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecCertificateRevoked, "got wrong error code for revoked cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), errSecCertificateRevoked);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

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

- (void) test_results_dictionary_revocation_reason {
    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;
    CFDataRef ocspResponse = NULL;

    leaf = SecCertificateCreateWithBytes(NULL, _probablyNotRevokedLeaf, sizeof(_probablyNotRevokedLeaf));
    subCA = SecCertificateCreateWithBytes(NULL, _devIDCA, sizeof(_devIDCA));
    root = SecCertificateCreateWithBytes(NULL, _appleRoot, sizeof(_appleRoot));

    const void *v_certs[] = { leaf, subCA };
    const void *v_anchors[] = { root };

    certs = CFArrayCreate(NULL, v_certs, 2, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleExternalDeveloper();
    require_noerr_action(SecTrustCreateWithCertificates(certs, policy, &trust), errOut, fail("failed to create trust object"));

    anchors = CFArrayCreate(NULL, v_anchors, 1, &kCFTypeArrayCallBacks);
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut, fail("failed to set anchors"));

    verifyDate = CFDateCreate(NULL, 543000000.0); // March 17, 2018 at 10:20:00 AM PDT
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut, fail("failed to set verify date"));

    /* Set the stapled response */
    ocspResponse = CFDataCreate(NULL, _devID_OCSPResponse, sizeof(_devID_OCSPResponse));
    ok_status(SecTrustSetOCSPResponse(trust, ocspResponse), "failed to set OCSP response");

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
            int64_t reason = 4; // superceded
            CFNumberRef cfreason = CFNumberCreate(NULL, kCFNumberSInt64Type, &reason);
            is(CFNumberCompare(cfreason, CFDictionaryGetValue(result, kSecTrustRevocationReason), NULL), kCFCompareEqualTo, "expected revocation reason 4");
            CFReleaseNull(cfreason);
        }
        CFReleaseNull(result);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

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

@end
