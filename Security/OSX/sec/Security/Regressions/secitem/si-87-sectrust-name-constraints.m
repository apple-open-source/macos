/*
 * Copyright (c) 2015-2017 Apple Inc. All Rights Reserved.
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <utilities/SecCFWrappers.h>

#include "shared_regressions.h"

#include "si-87-sectrust-name-constraints.h"


static void tests(void) {
    SecCertificateRef root = NULL, subca = NULL, leaf1 = NULL, leaf2 = NULL;
    NSArray *certs1 = nil, *certs2, *anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:517282600.0]; // 23 May 2017

    require_action(root = SecCertificateCreateWithBytes(NULL, _test_root, sizeof(_test_root)), errOut,
                   fail("Failed to create root cert"));
    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf1 = SecCertificateCreateWithBytes(NULL, _test_leaf1, sizeof(_test_leaf1)), errOut,
                   fail("Failed to create leaf cert 1"));
    require_action(leaf2 = SecCertificateCreateWithBytes(NULL, _test_leaf2, sizeof(_test_leaf2)), errOut,
                   fail("Failed to create leaf cert 2"));

    certs1 = @[(__bridge id)leaf1, (__bridge id)subca];
    certs2 = @[(__bridge id)leaf2, (__bridge id)subca];
    anchors = @[(__bridge id)root];

    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut,
                         fail("Failed to set anchors"));
    require_noerr_action(SecTrustEvaluate(trust, &trustResult), errOut,
                         fail("Failed to evaluate trust"));
    is(trustResult, kSecTrustResultUnspecified, "Got wrong trust result for leaf 1");

    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs2,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut,
                         fail("Failed to set anchors"));
    require_noerr_action(SecTrustEvaluate(trust, &trustResult), errOut,
                         fail("Failed to evaluate trust"));
    is(trustResult, kSecTrustResultUnspecified, "Got wrong trust result for leaf 1");

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(leaf1);
    CFReleaseNull(leaf2);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

int si_87_sectrust_name_constraints(int argc, char *const *argv)
{
	plan_tests(2);
	tests();
	return 0;
}
