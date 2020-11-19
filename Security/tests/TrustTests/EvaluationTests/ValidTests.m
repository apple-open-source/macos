/*
 *  Copyright (c) 2017-2019 Apple Inc. All Rights Reserved.
 *
 */

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>

#include <Security/Security.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>
#include "trust/trustd/OTATrustUtilities.h"

#import "../TestMacroConversions.h"
#import "TrustEvaluationTestCase.h"

enum {
    kBasicPolicy = 0,
    kSSLServerPolicy = 1,
};

@interface ValidTests : TrustEvaluationTestCase
@end

@implementation ValidTests

#if !TARGET_OS_BRIDGE
- (void) run_valid_trust_test:(SecCertificateRef)leaf
                           ca:(SecCertificateRef)ca
                        subca:(SecCertificateRef)subca
                      anchors:(CFArrayRef)anchors
                         date:(CFDateRef)date
                     policyID:(CFIndex)policyID
                     expected:(SecTrustResultType)expected
                    test_name:(const char *)test_name
{
    CFArrayRef policies=NULL;
    SecPolicyRef policy=NULL;
    SecTrustRef trust=NULL;
    SecTrustResultType trustResult;
    CFMutableArrayRef certs=NULL;

    printf("Starting %s\n", test_name);
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    if (certs) {
        if (leaf) {
            CFArrayAppendValue(certs, leaf);
        }
        if (ca) {
            CFArrayAppendValue(certs, ca);
        }
        if (subca) {
            CFArrayAppendValue(certs, subca);
        }
    }

    if (policyID == kSSLServerPolicy) {
        isnt(policy = SecPolicyCreateSSL(true, NULL), NULL, "create ssl policy");
    } else {
        isnt(policy = SecPolicyCreateBasicX509(), NULL, "create basic policy");
    }
    isnt(policies = CFArrayCreate(kCFAllocatorDefault, (const void **)&policy, 1, &kCFTypeArrayCallBacks), NULL, "create policies");
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust), "create trust");

    assert(trust); // silence analyzer
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    ok(trustResult == expected, "trustResult %d expected (got %d) for %s",
       (int)expected, (int)trustResult, test_name);

    CFReleaseSafe(certs);
    CFReleaseSafe(policy);
    CFReleaseSafe(policies);
    CFReleaseSafe(trust);
}

- (SecCertificateRef) CF_RETURNS_RETAINED createCertFromResource:(NSString *)name
{
    id cert = [self SecCertificateCreateFromPEMResource:name subdirectory:@"si-88-sectrust-valid-data"];
    return (__bridge SecCertificateRef)cert;
}

- (void)test_date_constraints
{
    SecCertificateRef ca_na=NULL, ca_nb=NULL, root=NULL;
    SecCertificateRef leaf_na_ok1=NULL, leaf_na_ok2=NULL;
    SecCertificateRef leaf_nb_ok1=NULL, leaf_nb_ok2=NULL, leaf_nb_revoked1=NULL;

    isnt(ca_na = [self createCertFromResource:@"ca-na"], NULL, "create ca-na cert");
    isnt(ca_nb = [self createCertFromResource:@"ca-nb"], NULL, "create ca-nb cert");
    isnt(root = [self createCertFromResource:@"root"], NULL, "create root cert");
    isnt(leaf_na_ok1 = [self createCertFromResource:@"leaf-na-ok1"], NULL, "create leaf-na-ok1 cert");
    isnt(leaf_na_ok2 = [self createCertFromResource:@"leaf-na-ok2"], NULL, "create leaf-na-ok2 cert");
    isnt(leaf_nb_ok1 = [self createCertFromResource:@"leaf-nb-ok1"], NULL, "create leaf-nb-ok1 cert");
    isnt(leaf_nb_ok2 = [self createCertFromResource:@"leaf-nb-ok2"], NULL, "create leaf-nb-ok2 cert");
    isnt(leaf_nb_revoked1 = [self createCertFromResource:@"leaf-nb-revoked1"], NULL, "create leaf-nb-revoked1 cert");

    CFMutableArrayRef anchors=NULL;
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create anchors array");
    if (anchors && root) {
        CFArrayAppendValue(anchors, root);
    }
    CFCalendarRef cal = NULL;
    CFAbsoluteTime at;
    CFDateRef date_20180102 = NULL; // a date when our test certs would all be valid, in the absence of Valid db info

    isnt(cal = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar), NULL, "create calendar");
    ok(CFCalendarComposeAbsoluteTime(cal, &at, "yMd", 2018, 1, 2), "create verify absolute time 20180102");
    isnt(date_20180102 = CFDateCreate(kCFAllocatorDefault, at), NULL, "create verify date 20180102");

    /* Case 0: leaf_na_ok1 (not revoked) */
    /* -- OK: cert issued 2017-10-20, before the CA not-after date of 2017-10-21 */
    /*        test cert has no SCT, but is expected to be OK since we now only apply the CT restriction for SSL. */
    [self run_valid_trust_test:leaf_na_ok1 ca:ca_na subca:NULL anchors:anchors date:date_20180102
                      policyID:kBasicPolicy expected:kSecTrustResultUnspecified
                     test_name:"leaf_na_ok1 basic"];

    /* Case 1: leaf_na_ok1 (not revoked) */
    /* -- BAD: since a not-after date now requires CT (for SSL) and the test cert has no SCT, this is fatal. */
    /* Mock a successful mobile asset check-in so that we enforce CT */
    XCTAssertTrue(UpdateOTACheckInDate(), "failed to set check-in date as now");
    [self run_valid_trust_test:leaf_na_ok1 ca:ca_na subca:NULL anchors:anchors date:date_20180102
                      policyID:kSSLServerPolicy expected:kSecTrustResultFatalTrustFailure
                     test_name:"leaf_na_ok1 ssl"];

    /* Case 2: leaf_na_ok2 (revoked) */
    /* -- BAD: cert issued 2017-10-26, after the CA not-after date of 2017-10-21 */
    [self run_valid_trust_test:leaf_na_ok2 ca:ca_na subca:NULL anchors:anchors date:date_20180102
                      policyID:kBasicPolicy expected:kSecTrustResultFatalTrustFailure
                     test_name:"leaf_na_ok2 basic"];

    /* Case 3: leaf_nb_ok1 (revoked) */
    /* -- BAD: cert issued 2017-10-20, before the CA not-before date of 2017-10-22 */
    [self run_valid_trust_test:leaf_nb_ok1 ca:ca_nb subca:NULL anchors:anchors date:date_20180102
                      policyID:kBasicPolicy expected:kSecTrustResultFatalTrustFailure
                     test_name:"leaf_nb_ok1 basic"];

    /* Case 4: leaf_nb_ok2 (not revoked) */
    /* -- OK: cert issued 2017-10-26, after the CA not-before date of 2017-10-22 */
    [self run_valid_trust_test:leaf_nb_ok2 ca:ca_nb subca:NULL anchors:anchors date:date_20180102
                      policyID:kBasicPolicy expected:kSecTrustResultUnspecified
                     test_name:"leaf_nb_ok2 basic"];

    /* Case 5: leaf_nb_revoked1 (revoked) */
    /* -- BAD: cert issued 2017-10-20, before the CA not-before date of 2017-10-22 */
    [self run_valid_trust_test:leaf_nb_revoked1 ca:ca_nb subca:NULL anchors:anchors date:date_20180102
                      policyID:kBasicPolicy expected:kSecTrustResultFatalTrustFailure
                     test_name:"leaf_nb_revoked1 basic"];

    CFReleaseSafe(ca_na);
    CFReleaseSafe(ca_nb);
    CFReleaseSafe(leaf_na_ok1);
    CFReleaseSafe(leaf_na_ok2);
    CFReleaseSafe(leaf_nb_ok1);
    CFReleaseSafe(leaf_nb_ok2);
    CFReleaseSafe(leaf_nb_revoked1);
    CFReleaseSafe(root);
    CFReleaseSafe(anchors);
    CFReleaseSafe(cal);
    CFReleaseSafe(date_20180102);
}

- (void)test_known_intermediate
{
    SecCertificateRef ca_ki=NULL, root=NULL;
    SecCertificateRef leaf_ki_ok1=NULL, leaf_ki_revoked1=NULL;
    SecCertificateRef leaf_unknown=NULL, ca_unknown=NULL;

    isnt(ca_ki = [self createCertFromResource:@"ca-ki"], NULL, "create ca-ki cert");
    isnt(root = [self createCertFromResource:@"root"], NULL, "create root cert");
    isnt(leaf_ki_ok1 = [self createCertFromResource:@"leaf-ki-ok1"], NULL, "create leaf-ki-ok1 cert");
    isnt(leaf_ki_revoked1 = [self createCertFromResource:@"leaf-ki-revoked1"], NULL, "create leaf-ki-revoked1 cert");
    isnt(ca_unknown = [self createCertFromResource:@"ca-unknown"], NULL, "create ca-unknown cert");
    isnt(leaf_unknown = [self createCertFromResource:@"leaf-unknown"], NULL, "create leaf-unknown cert");

    CFMutableArrayRef anchors=NULL;
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create anchors array");
    if (anchors && root) {
        CFArrayAppendValue(anchors, root);
    }
    CFCalendarRef cal = NULL;
    CFAbsoluteTime at;
    CFDateRef date_20180310 = NULL; // a date when our test certs would all be valid, in the absence of Valid db info

    isnt(cal = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar), NULL, "create calendar");
    ok(CFCalendarComposeAbsoluteTime(cal, &at, "yMd", 2018, 3, 10), "create verify absolute time 20180310");
    isnt(date_20180310 = CFDateCreate(kCFAllocatorDefault, at), NULL, "create verify date 20180310");

    /* Case 1: leaf_ki_ok1 */
    /* -- OK: cert issued by a known intermediate */
    [self run_valid_trust_test:leaf_ki_ok1 ca:ca_ki subca:NULL anchors:anchors date:date_20180310 policyID:kBasicPolicy expected:kSecTrustResultUnspecified test_name:"leaf_ki_ok1"];

    /* Case 2: leaf_ki_revoked1 */
    /* -- BAD: CA specifies known-only+complete serial blocklist; this cert is on the blocklist. */
    [self run_valid_trust_test:leaf_ki_revoked1 ca:ca_ki subca:NULL anchors:anchors date:date_20180310 policyID:kBasicPolicy expected:kSecTrustResultFatalTrustFailure test_name:"leaf_ki_revoked"];

    /* Case 3: leaf_unknown */
    /* -- BAD: ca_unknown issued from ca_ki, but is not a known intermediate.
     * ca_ki has a path len of 0 which would normally result in kSecTrustResultRecoverableTrustFailure;
     * however, since known-intermediates is asserted for ca_ki (non-overridable), we expect a fatal failure. */
    [self run_valid_trust_test:leaf_unknown ca:ca_unknown subca:ca_ki anchors:anchors date:date_20180310 policyID:kBasicPolicy expected:kSecTrustResultFatalTrustFailure test_name:"leaf_unknown test"];

    CFReleaseSafe(ca_ki);
    CFReleaseSafe(leaf_ki_ok1);
    CFReleaseSafe(leaf_ki_revoked1);
    CFReleaseSafe(ca_unknown);
    CFReleaseSafe(leaf_unknown);
    CFReleaseSafe(root);
    CFReleaseSafe(anchors);
    CFReleaseSafe(cal);
    CFReleaseSafe(date_20180310);
}
#else /* TARGET_OS_BRIDGE */
/* Valid is not supported on bridgeOS */
- (void)testSkipTests
{
    XCTAssert(true);
}
#endif

@end
