/*
 * Copyright (c) 2016-2018 Apple Inc. All Rights Reserved.
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


/* INSTRUCTIONS FOR ADDING NEW SUBTESTS:
 *   1. Add the certificates, as DER-encoded files with the 'cer' extension, to OSX/shared_regressions/si-20-sectrust-policies-data/
 *        NOTE: If your cert needs to be named with "(i[Pp]hone|i[Pp]ad|i[Pp]od)", you need to make two copies -- one named properly
 *        and another named such that it doesn't match that regex. Use the regex trick below for TARGET_OS_TV to make sure your test
 *        works.
 *   2. Add a new dictionary to the test plist (OSX/shared_regressions/si-20-sectrust-policies-data/PinningPolicyTrustTest.plist).
 *      This dictionary must include: (see constants below)
 *          MajorTestName
 *          MinorTestName
 *          Policies
 *          Leaf
 *          Intermediates
 *          ExpectedResult
 *      It is strongly recommended that all test dictionaries include the Anchors and VerifyDate keys.
 *      Addtional optional keys are defined below.
 */

/* INSTRUCTIONS FOR DEBUGGING SUBTESTS:
 *   Add a debugging.plist to OSX/shared_regressions/si-20-sectrust-policies-data/ containing only those subtest dictionaries
 *   you want to debug.
 */

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>

#include <utilities/SecInternalReleasePriv.h>
#include <utilities/SecCFRelease.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "../TrustEvaluationTestHelpers.h"

const NSString *kSecTrustTestPinningPolicyResources = @"si-20-sectrust-policies-data";

@interface PolicyTests : TrustEvaluationTestCase
@end

@implementation PolicyTests

- (void)testPolicies {
    NSURL *testPlist = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"debugging" withExtension:@"plist"
                                                          subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    if (!testPlist) {
        testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:nil withExtension:@"plist"
                                                              subdirectory:(NSString *)kSecTrustTestPinningPolicyResources ];
    }
    if (!testPlist) {
        fail("Failed to get tests plist from %@", kSecTrustTestPinningPolicyResources);
        return;
    }

    testsArray = [NSArray arrayWithContentsOfURL: testPlist];
    if (!testsArray) {
        fail("Failed to create array from plist");
        return;
    }

    [testsArray enumerateObjectsUsingBlock:^(NSDictionary *testDict, NSUInteger idx, BOOL * _Nonnull stop) {
        TestTrustEvaluation *testObj = [[TestTrustEvaluation alloc] initWithTrustDictionary:testDict];
        XCTAssertNotNil(testObj, "failed to create test object for %lu", (unsigned long)idx);

        NSError *testError = nil;
        XCTAssert([testObj evaluateForExpectedResults:&testError], "Test %@ failed: %@", testObj.fullTestName, testError);
    }];
}

- (void)testPinningDisable
{
    SecCertificateRef baltimoreRoot = NULL, appleISTCA2 = NULL, pinnedNonCT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("caldav.icloud.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:580000000.0]; // May 19, 2019 at 4:06:40 PM PDT
    NSArray *certs = nil, *enforcement_anchors = nil;
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];

    require_action(baltimoreRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"BaltimoreCyberTrustRoot"
                                                                                         subdirectory:(NSString *)kSecTrustTestPinningPolicyResources],
                   errOut, fail("failed to create geotrust root"));
    require_action(appleISTCA2 = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"AppleISTCA2G1-Baltimore"
                                                                                       subdirectory:(NSString *)kSecTrustTestPinningPolicyResources],
                   errOut, fail("failed to create apple IST CA"));
    require_action(pinnedNonCT = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"caldav"
                                                                                       subdirectory:(NSString *)kSecTrustTestPinningPolicyResources],
                   errOut, fail("failed to create deprecated SSL Server cert"));

    certs = @[ (__bridge id)pinnedNonCT, (__bridge id)appleISTCA2 ];
    enforcement_anchors = @[ (__bridge id)baltimoreRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforcement_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetPinningPolicyName(trust, kSecPolicyNameAppleAMPService), errOut, fail("failed to set policy name"));
#if !TARGET_OS_BRIDGE
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "pinning against wrong profile succeeded");
#endif

    // Test with pinning disabled
    [defaults setBool:YES forKey:@"AppleServerAuthenticationNoPinning"];
    [defaults synchronize];
    SecTrustSetNeedsEvaluation(trust);
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "failed to disable pinning");
    [defaults setBool:NO forKey:@"AppleServerAuthenticationNoPinning"];
    [defaults synchronize];

errOut:
    CFReleaseNull(baltimoreRoot);
    CFReleaseNull(appleISTCA2);
    CFReleaseNull(pinnedNonCT);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

@end
