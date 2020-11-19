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
#include <utilities/SecCFWrappers.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecPolicyInternal.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "../TrustEvaluationTestHelpers.h"

const NSString *kSecTrustTestPinningPolicyResources = @"si-20-sectrust-policies-data";
const NSString *kSecTrustTestPinnningTest = @"PinningPolicyTrustTest";

@interface PolicyTests : TrustEvaluationTestCase
@end

@implementation PolicyTests

- (void)testPolicies {
    NSURL *testPlist = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"debugging" withExtension:@"plist"
                                                          subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    if (!testPlist) {
        testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:(NSString *)kSecTrustTestPinnningTest withExtension:@"plist"
                                                              subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
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

- (void)testSecPolicyReconcilePinningRequiredIfInfoSpecified
{
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("www.example.org"));
    CFMutableArrayRef emptySPKISHA256 = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);

    CFMutableArrayRef nonemtpySPKISHA256 = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    uint8_t _random_data256[256/sizeof(uint8_t)];
    (void)SecRandomCopyBytes(NULL, sizeof(_random_data256), _random_data256);
    CFDataRef random_data256 = CFDataCreate(kCFAllocatorDefault, _random_data256, sizeof(_random_data256));
    CFArrayAppendValue(nonemtpySPKISHA256, random_data256);
    CFReleaseNull(random_data256);

    // kSecPolicyCheckPinningRequired should be unset after reconciliation.
    // Empty values for both SPKI policies signal a pinning exception.
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckLeafSPKISHA256, emptySPKISHA256);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckCAspkiSHA256, emptySPKISHA256);
    CFDictionaryRef policyOptions = SecPolicyGetOptions(policy);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);
    SecPolicyReconcilePinningRequiredIfInfoSpecified((CFMutableDictionaryRef)policyOptions);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), false);

    // kSecPolicyCheckPinningRequired overrules the other two policies.
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckLeafSPKISHA256, nonemtpySPKISHA256);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckCAspkiSHA256, emptySPKISHA256);
    policyOptions = SecPolicyGetOptions(policy);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);
    SecPolicyReconcilePinningRequiredIfInfoSpecified((CFMutableDictionaryRef)policyOptions);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), false);

    // kSecPolicyCheckPinningRequired overrules the other two policies.
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckLeafSPKISHA256, emptySPKISHA256);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckCAspkiSHA256, nonemtpySPKISHA256);
    policyOptions = SecPolicyGetOptions(policy);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);
    SecPolicyReconcilePinningRequiredIfInfoSpecified((CFMutableDictionaryRef)policyOptions);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), false);

    // In the absence of kSecPolicyCheckPinningRequired there is nothing to reconcile.
    CFReleaseNull(policy);
    policy = SecPolicyCreateSSL(true, CFSTR("www.example.org"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckLeafSPKISHA256, emptySPKISHA256);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckCAspkiSHA256, emptySPKISHA256);
    policyOptions = SecPolicyGetOptions(policy);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);
    SecPolicyReconcilePinningRequiredIfInfoSpecified((CFMutableDictionaryRef)policyOptions);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);

    CFReleaseNull(policy);
    CFReleaseNull(emptySPKISHA256);
    CFReleaseNull(nonemtpySPKISHA256);
}

- (CFDictionaryRef)getNSPinnedDomainsFromDictionaryInfoFile:(NSString *)fileName
{
    NSURL *infoPlist = [[NSBundle bundleForClass:[self class]] URLForResource:fileName withExtension:@"plist"
                                                                 subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    if (!infoPlist) {
        fail("Failed to access plist file \"%@\"", fileName);
        return NULL;
    }

    NSDictionary *infoDictionary = [NSDictionary dictionaryWithContentsOfURL:infoPlist];
    if (!infoDictionary) {
        fail("Failed to create dictionary from plist file \"%@\"", fileName);
        return NULL;
    }

    CFTypeRef nsAppTransportSecurityDict = CFDictionaryGetValue((__bridge CFDictionaryRef)infoDictionary, CFSTR("NSAppTransportSecurity"));
    if (!isDictionary(nsAppTransportSecurityDict)) {
        fail("NSAppTransportSecurity dictionary entry is missing from plist file \"%@\"", fileName);
        return NULL;
    }

    CFDictionaryRef nsPinnedDomainsDict = CFDictionaryGetValue(nsAppTransportSecurityDict, CFSTR("NSPinnedDomains"));
    if (!isDictionary(nsPinnedDomainsDict)) {
        fail("NSPinnedDomains dictionary entry is missing from plist file \"%@\"", fileName);
        return NULL;
    }

    return nsPinnedDomainsDict;
}

- (void)testParseNSPinnedDomains
{
    NSURL *testPlist = nil;
    NSArray *testsArray = nil;


    testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"NSPinnedDomainsParsingTest_debugging" withExtension:@"plist"
                                                          subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    if (!testPlist) {
        testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"NSPinnedDomainsParsingTest" withExtension:@"plist"
                                                              subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    }
    if (!testPlist) {
        fail("Failed to get tests plist from \"%@\"", kSecTrustTestPinningPolicyResources);
        return;
    }

    testsArray = [NSArray arrayWithContentsOfURL: testPlist];
    if (!testsArray) {
        fail("Failed to create array from plist");
        return;
    }

    [testsArray enumerateObjectsUsingBlock:^(NSDictionary *testDict, NSUInteger idx, BOOL * _Nonnull stop) {
        NSString *plistFileName = testDict[@"PlistFileName"];
        if (!plistFileName) {
            fail("Failed to read plist file name from \"%@\":%lu", plistFileName, (unsigned long)idx);
            return;
        }

        NSDictionary *expectedProperties = testDict[@"ExpectedProperties"];
        if (!expectedProperties) {
            fail("Missing the expected results in \"%@\"", plistFileName);
            return;
        }
        bool hasNSPinnedLeafIdentities = [expectedProperties[@"NSPinnedLeafIdentities"] boolValue];
        int NSPinnedLeafIdentitiesCount = [expectedProperties[@"NSPinnedLeafIdentitiesCount"] intValue];
        bool hasNSPinnedCAIdentities = [expectedProperties[@"NSPinnedCAIdentities"] boolValue];
        int NSPinnedCAIdentitiesCount = [expectedProperties[@"NSPinnedCAIdentitiesCount"] intValue];
        bool hasNSIncludesSubdomains = [expectedProperties[@"NSIncludesSubdomains"] boolValue];

        CFDictionaryRef nsPinnedDomainsDict = [self getNSPinnedDomainsFromDictionaryInfoFile:plistFileName];
        if (!isDictionary(nsPinnedDomainsDict)) {
            fail("Unable to read %@", plistFileName);
            return;
        }
        CFArrayRef leafSPKISHA256 = parseNSPinnedDomains(nsPinnedDomainsDict, CFSTR("example.org"), CFSTR("NSPinnedLeafIdentities"));
        is(leafSPKISHA256 != NULL, hasNSPinnedLeafIdentities, "leafSPKISHA256 test failed in \"%@\"", plistFileName);
        is(leafSPKISHA256 != NULL && CFArrayGetCount(leafSPKISHA256) != 0, NSPinnedLeafIdentitiesCount != 0, "leafSPKISHA256 count test failed in \"%@\"", plistFileName);

        CFArrayRef caSPKISHA256 = parseNSPinnedDomains(nsPinnedDomainsDict, CFSTR("example.org"), CFSTR("NSPinnedCAIdentities"));
        is(caSPKISHA256 != NULL, hasNSPinnedCAIdentities, "caSPKISHA256 test failed in \"%@\"", plistFileName);
        is(caSPKISHA256 != NULL && CFArrayGetCount(caSPKISHA256) != 0, NSPinnedCAIdentitiesCount != 0, "caSPKISHA256 count test failed in \"%@\"", plistFileName);

        caSPKISHA256 = parseNSPinnedDomains(nsPinnedDomainsDict, CFSTR("subdomain.example.org."), CFSTR("NSPinnedCAIdentities"));
        is(caSPKISHA256 != NULL, hasNSIncludesSubdomains, "caSPKISHA256 subdomain test failed in \"%@\"", plistFileName);
    }];
}

@end
