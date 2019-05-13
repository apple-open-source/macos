/*
 * Copyright (c) 2017-2018 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <utilities/SecCFRelease.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>

#include "shared_regressions.h"

#include "si-32-sectrust-pinning-required.h"


static NSArray *certs = nil;
static NSArray *root = nil;
static NSDate *verifyDate = nil;

static void setup_globals(void) {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _ids_test, sizeof(_ids_test));
    SecCertificateRef intermediate = SecCertificateCreateWithBytes(NULL, _TestAppleServerAuth, sizeof(_TestAppleServerAuth));
    SecCertificateRef rootcert = SecCertificateCreateWithBytes(NULL, _TestAppleRootCA, sizeof(_TestAppleRootCA));

    certs = @[(__bridge id)leaf,(__bridge id)intermediate];
    root = @[(__bridge id)rootcert];
    verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:560000000.0]; //September 30, 2018 at 4:33:20 AM PDT

    CFReleaseNull(leaf);
    CFReleaseNull(intermediate);
    CFReleaseNull(rootcert);
}

static SecTrustResultType test_with_policy_exception(SecPolicyRef CF_CONSUMED policy, bool set_exception)
{
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    require_noerr_quiet(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), cleanup);
    require_noerr_quiet(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)root), cleanup);
    require_noerr_quiet(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), cleanup);
    if (set_exception) {
        SecTrustSetPinningException(trust);
    }
    require_noerr_quiet(SecTrustEvaluate(trust, &trustResult), cleanup);

cleanup:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    return trustResult;
}

static SecTrustResultType test_with_policy(SecPolicyRef CF_CONSUMED policy) {
    return test_with_policy_exception(policy, false);
}


/* Technically, this feature works by reading the info plist of the caller. We'll fake it here by
 * setting the policy option for requiring pinning. */
static void tests(void)
{
    SecPolicyRef policy = NULL;

    policy = SecPolicyCreateSSL(true, CFSTR("openmarket.ess.apple.com"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultRecoverableTrustFailure, "Unpinned connection succeeeded when pinning required");

    policy = SecPolicyCreateAppleIDSServiceContext(CFSTR("openmarket.ess.apple.com"), NULL);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Policy pinned connection failed when pinning required");

    policy = SecPolicyCreateSSL(true, CFSTR("profile.ess.apple.com"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Systemwide hostname pinned connection failed when pinning required");

    NSDictionary *policy_properties = @{
                                        (__bridge NSString *)kSecPolicyName : @"openmarket.ess.apple.com",
                                        (__bridge NSString *)kSecPolicyPolicyName : @"IDS",
                                        };
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)policy_properties);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Systemwide policy name pinned connection failed when pinning required");

    policy = SecPolicyCreateSSL(true, CFSTR("openmarket.ess.apple.com"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy_exception(policy, true), kSecTrustResultUnspecified, "Unpinned connection failed when pinning exception set");

    /* can I write an effective test for charles?? */
}

int si_32_sectrust_pinning_required(int argc, char *const *argv)
{
    plan_tests(5);
    setup_globals();
    tests();
    return 0;
}
