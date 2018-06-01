/*
 *  si-88-sectrust-valid.m
 *  Security
 *
 *  Copyright (c) 2017-2018 Apple Inc. All Rights Reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicy.h>
#include <stdlib.h>
#include <unistd.h>
#include <utilities/SecCFWrappers.h>

#include "shared_regressions.h"

static void test_valid_trust(SecCertificateRef leaf, SecCertificateRef ca, CFArrayRef anchors,
                             CFDateRef date, CFIndex policyID, SecTrustResultType expected, const char *test_name)
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
    }

    if (policyID == 1) {
        isnt(policy = SecPolicyCreateSSL(true, NULL), NULL, "create ssl policy");
    } else {
        isnt(policy = SecPolicyCreateBasicX509(), NULL, "create basic policy");
    }
    isnt(policies = CFArrayCreate(kCFAllocatorDefault, (const void **)&policy, 1, &kCFTypeArrayCallBacks), NULL, "create policies");
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust), "create trust");

    assert(trust); // silence analyzer
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    ok(trustResult == expected, "trustResult %d expected (got %d)",
       (int)expected, (int)trustResult);

    CFReleaseSafe(certs);
    CFReleaseSafe(policy);
    CFReleaseSafe(policies);
    CFReleaseSafe(trust);
}

#import <Foundation/Foundation.h>
SecCertificateRef SecCertificateCreateWithPEM(CFAllocatorRef allocator, CFDataRef pem_certificate);

static SecCertificateRef SecCertificateCreateFromResource(NSString *name)
{
    NSString *resources = @"si-88-sectrust-valid-data";
    NSString *extension = @"pem";

    NSURL *url = [[NSBundle mainBundle] URLForResource:name withExtension:extension subdirectory:resources];
    if (!url) {
        printf("No URL for resource \"%s.pem\"\n", [name UTF8String]);
        return NULL;
    }

    NSData *certData = [NSData dataWithContentsOfURL:url];
    if (!certData) {
        printf("No cert data for resource \"%s.pem\"\n", [name UTF8String]);
        return NULL;
    }

    return SecCertificateCreateWithPEM(kCFAllocatorDefault, (__bridge CFDataRef)certData);
}

static void tests()
{
    SecCertificateRef ca_na=NULL, ca_nb=NULL, root=NULL;
    SecCertificateRef leaf_na_ok1=NULL, leaf_na_ok2=NULL;
    SecCertificateRef leaf_nb_ok1=NULL, leaf_nb_ok2=NULL, leaf_nb_revoked1=NULL;

    isnt(ca_na = SecCertificateCreateFromResource(@"ca-na"), NULL, "create ca-na cert");
    isnt(ca_nb = SecCertificateCreateFromResource(@"ca-nb"), NULL, "create ca-nb cert");
    isnt(root = SecCertificateCreateFromResource(@"root"), NULL, "create root cert");
    isnt(leaf_na_ok1 = SecCertificateCreateFromResource(@"leaf-na-ok1"), NULL, "create leaf-na-ok1 cert");
    isnt(leaf_na_ok2 = SecCertificateCreateFromResource(@"leaf-na-ok2"), NULL, "create leaf-na-ok2 cert");
    isnt(leaf_nb_ok1 = SecCertificateCreateFromResource(@"leaf-nb-ok1"), NULL, "create leaf-nb-ok1 cert");
    isnt(leaf_nb_ok2 = SecCertificateCreateFromResource(@"leaf-nb-ok2"), NULL, "create leaf-nb-ok2 cert");
    isnt(leaf_nb_revoked1 = SecCertificateCreateFromResource(@"leaf-nb-revoked1"), NULL, "create leaf-nb-revoked1 cert");

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
    test_valid_trust(leaf_na_ok1, ca_na, anchors, date_20180102, 0, kSecTrustResultUnspecified, "leaf_na_ok1 basic test");

    /* Case 1: leaf_na_ok1 (not revoked) */
    /* -- BAD: since a not-after date now requires CT (for SSL) and the test cert has no SCT, this is fatal. */
    test_valid_trust(leaf_na_ok1, ca_na, anchors, date_20180102, 1, kSecTrustResultFatalTrustFailure, "leaf_na_ok1 ssl test");

    /* Case 2: leaf_na_ok2 (revoked) */
    /* -- BAD: cert issued 2017-10-26, after the CA not-after date of 2017-10-21 */
    test_valid_trust(leaf_na_ok2, ca_na, anchors, date_20180102, 0, kSecTrustResultFatalTrustFailure, "leaf_na_ok2 basic test");

    /* Case 3: leaf_nb_ok1 (revoked) */
    /* -- BAD: cert issued 2017-10-20, before the CA not-before date of 2017-10-22 */
    test_valid_trust(leaf_nb_ok1, ca_nb, anchors, date_20180102, 0, kSecTrustResultFatalTrustFailure, "leaf_nb_ok1 basic test");

    /* Case 4: leaf_nb_ok2 (not revoked) */
    /* -- OK: cert issued 2017-10-26, after the CA not-before date of 2017-10-22 */
    test_valid_trust(leaf_nb_ok2, ca_nb, anchors, date_20180102, 0, kSecTrustResultUnspecified, "leaf_nb_ok2 basic test");

    /* Case 5: leaf_nb_revoked1 (revoked) */
    /* -- BAD: cert issued 2017-10-20, before the CA not-before date of 2017-10-22 */
    test_valid_trust(leaf_nb_revoked1, ca_nb, anchors, date_20180102, 0, kSecTrustResultFatalTrustFailure, "leaf_nb_revoked1 basic test");

    CFReleaseSafe(ca_na);
    CFReleaseSafe(ca_nb);
    CFReleaseSafe(root);
    CFReleaseSafe(leaf_na_ok1);
    CFReleaseSafe(leaf_na_ok2);
    CFReleaseSafe(leaf_nb_ok1);
    CFReleaseSafe(leaf_nb_ok2);
    CFReleaseSafe(leaf_nb_revoked1);
    CFReleaseSafe(anchors);
    CFReleaseSafe(cal);
    CFReleaseSafe(date_20180102);
}


int si_88_sectrust_valid(int argc, char *const *argv)
{
    plan_tests(12+(6*8));

    tests();

    return 0;
}
