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


/* INSTRUCTIONS FOR ADDING NEW SUBTESTS TO testPolicies:
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
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecCMS.h>

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "../TrustEvaluationTestHelpers.h"
#include "PolicyTests_data.h"

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

#if !TARGET_OS_BRIDGE
- (NSData *)random
{
    uint8_t random[32];
    (void)SecRandomCopyBytes(kSecRandomDefault, sizeof(random), random);
    return [[NSData alloc] initWithBytes:random length:sizeof(random)];
}

- (void)testSetTransparentConnectionPins
{
    CFErrorRef error = NULL;
    const CFStringRef TrustTestsAppID = CFSTR("com.apple.trusttests");
    const CFStringRef AnotherAppID = CFSTR("com.apple.security.not-this-one");
    CFArrayRef copiedPins = NULL;

    /* Verify no pins set */
    copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, NULL);
    XCTAssertEqual(copiedPins, NULL);
    if (copiedPins) {
        /* If we're startign out with pins set, a lot of the following will also fail, so just skip them */
        CFReleaseNull(copiedPins);
        return;
    }

    NSArray *pin1 = @[@{
        (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
    }];
    /* Set pin with specified AppID */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(TrustTestsAppID, (__bridge CFArrayRef)pin1, &error),
              "failed to set pins: %@", error);

    /* Copy all pins (with only one set) */
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, &error),
                      "failed to copy all pins: %@", error);
    XCTAssertEqualObjects(pin1, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Copy this app's pins */
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(TrustTestsAppID, &error),
                      "failed to copy this app's pins: %@", error);
    XCTAssertEqualObjects(pin1, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Copy a different app's pins */
    XCTAssertEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(AnotherAppID, &error),
                   "failed to copy another app's pins: %@", error);
    CFReleaseNull(copiedPins);

    /* Set a different pin with implied AppID and ensure pins are replaced */
    NSArray *pin2 = @[@{
        (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
    }];
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)pin2, &error),
              "failed to set pins: %@", error);
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(TrustTestsAppID, &error),
                      "failed to copy this app's pins: %@", error);
    XCTAssertEqualObjects(pin2, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Set pins for a different AppID */
    NSArray *pin3 = @[@{
      (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
      (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
    }, @{
      (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
      (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
    }
    ];
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(AnotherAppID, (__bridge CFArrayRef)pin3, &error),
              "failed to set pins: %@", error);
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(AnotherAppID, &error),
                      "failed to copy another app's pins: %@", error);
    XCTAssertEqualObjects(pin3, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Set empty pins, and ensure no change */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)@[], &error),
              "failed to set empty pins: %@", error);
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(TrustTestsAppID, &error),
                      "failed to copy this app's pins: %@", error);
    XCTAssertEqualObjects(pin2, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Copy all pins */
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, &error),
                      "failed to copy all pins: %@", error);
    NSArray *nsCopiedPins = CFBridgingRelease(copiedPins);
    XCTAssertNotNil(nsCopiedPins);
    XCTAssertEqual([nsCopiedPins count], 3);

    /* Reset other app's pins (this app remains) */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(AnotherAppID, NULL, &error),
              "failed to reset pins: %@", error);
    XCTAssertNotEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, &error),
                      "failed to copy this app's pins: %@", error);
    XCTAssertEqualObjects(pin2, (__bridge NSArray*)copiedPins);
    CFReleaseNull(copiedPins);

    /* Reset remaining pins */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(TrustTestsAppID, NULL, &error),
              "failed to reset pins: %@", error);
    XCTAssertEqual(NULL, copiedPins = SecTrustStoreCopyTransparentConnectionPins(NULL, &error),
                   "failed to copy all pins: %@", error);
    CFReleaseNull(copiedPins);
}

- (void)testSetTransparentConnectionPins_InputValidation
{
    CFErrorRef error = NULL;
#define check_errSecParam \
if (error) { \
is(CFErrorGetCode(error), errSecParam, "bad input produced unxpected error code: %ld", (long)CFErrorGetCode(error)); \
CFReleaseNull(error); \
} else { \
fail("expected failure to set NULL exceptions"); \
}
    NSArray *badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
         @"not a key" : @"not a value"
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
         @"not a key" : [self random]
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
         (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random],
         @"not a key" : @"not a value"
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha3",
         (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random],
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    badPins = @[@{
         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
         (__bridge NSString*)kSecTrustStoreSPKIHashKey : @"sha256",
    }];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    badPins = @[[self random]];
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)badPins, &error));
    check_errSecParam

    NSDictionary *dictionary = @{ @"aKey": @"aValue" };
    XCTAssertFalse(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)dictionary[@"aKey"], &error));
    check_errSecParam

#undef check_errSecParam

    /* Remove pins */
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(NULL, NULL, &error),
              "failed to reset pins: %@", error);
    XCTAssertEqual(NULL, SecTrustStoreCopyTransparentConnectionPins(NULL, &error));
}

- (void)testTransparentConnections {
    SecCertificateRef pallasLeaf = SecCertificateCreateWithBytes(NULL, _pallas_leaf, sizeof(_pallas_leaf));
    SecCertificateRef pallasCA = SecCertificateCreateWithBytes(NULL, _pallas_ca, sizeof(_pallas_ca));
    SecCertificateRef tcLeaf = SecCertificateCreateWithBytes(NULL, _transparent_connection_leaf, sizeof(_transparent_connection_leaf));
    SecCertificateRef tcCA = SecCertificateCreateWithBytes(NULL, _transparent_connection_ca, sizeof(_transparent_connection_ca));
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("gdmf.apple.com"));

    /* Transparent Connection Pins not configured */
    // TC policy accepts apple pins
    NSArray *appleCerts = @[ (__bridge id) pallasLeaf, (__bridge id)pallasCA ];
    TestTrustEvaluation *trustTest = [[TestTrustEvaluation alloc] initWithCertificates:appleCerts policies:@[(__bridge id)policy]];
    [trustTest setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:624000000.0]]; // October 9, 2020 at 10:20:00 PM PDT
    XCTAssert([trustTest evaluate:nil]);

    // TC policy doesn't accept TC pins
    trustTest = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)tcLeaf] policies:@[(__bridge id)policy]];
    [trustTest setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:624000000.0]];
    [trustTest addAnchor:tcCA];
    XCTAssertFalse([trustTest evaluate:nil]);

    /* Transparent Connection pins configured */
    NSArray *pin = @[@{
                         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
                         (__bridge NSString*)kSecTrustStoreSPKIHashKey : CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(tcCA))
                    },
                     @{
                         (__bridge NSString*)kSecTrustStoreHashAlgorithmKey : @"sha256",
                         (__bridge NSString*)kSecTrustStoreSPKIHashKey : [self random]
                     }
    ];
    XCTAssert(SecTrustStoreSetTransparentConnectionPins(NULL, (__bridge CFArrayRef)pin, NULL));

    // TC policy accepts TC pins if set
    [trustTest setNeedsEvaluation];
    XCTAssert([trustTest evaluate:nil]);

    // non TC policy doesn't accept TC pins if set
    [trustTest setNeedsEvaluation];
    SecTrustSetPinningPolicyName([trustTest trust], kSecPolicyNameAppleMMCSService);
    XCTAssertFalse([trustTest evaluate:nil]);

    // TC policy always accepts apple pin
    CFReleaseNull(policy); // reset policy so to remove policy name
    policy = SecPolicyCreateSSL(true, CFSTR("gdmf.apple.com"));
    trustTest = [[TestTrustEvaluation alloc] initWithCertificates:appleCerts policies:@[(__bridge id)policy]];
    [trustTest setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:624000000.0]]; // October 9, 2020 at 10:20:00 PM PDT
    XCTAssert([trustTest evaluate:nil]);

    XCTAssert(SecTrustStoreSetTransparentConnectionPins(NULL, NULL, NULL));
    CFReleaseNull(pallasLeaf);
    CFReleaseNull(pallasCA);
    CFReleaseNull(tcLeaf);
    CFReleaseNull(tcCA);
    CFReleaseNull(policy);
}
#endif // !TARGET_OS_BRIDGE

static SecTrustResultType test_with_policy_exception(SecPolicyRef CF_CONSUMED policy, bool set_exception)
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _ids_test, sizeof(_ids_test));
    SecCertificateRef intermediate = SecCertificateCreateWithBytes(NULL, _TestAppleServerAuth, sizeof(_TestAppleServerAuth));
    SecCertificateRef rootcert = SecCertificateCreateWithBytes(NULL, _TestAppleRootCA, sizeof(_TestAppleRootCA));

    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    NSArray *certs = @[(__bridge id)leaf,(__bridge id)intermediate];
    NSArray *root = @[(__bridge id)rootcert];
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:622000000.0]; //September 16, 2020 at 6:46:40 PM PDT

    require_noerr_quiet(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), cleanup);
    require_noerr_quiet(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)root), cleanup);
    require_noerr_quiet(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), cleanup);
    if (set_exception) {
        SecTrustSetPinningException(trust);
    }
    require_noerr_quiet(SecTrustGetTrustResult(trust, &trustResult), cleanup);

cleanup:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(leaf);
    CFReleaseNull(intermediate);
    CFReleaseNull(rootcert);
    return trustResult;
}

static SecTrustResultType test_with_policy(SecPolicyRef CF_CONSUMED policy) {
    return test_with_policy_exception(policy, false);
}

/* Technically, this feature works by reading the info plist of the caller. We'll fake it here by
 * setting the policy option for requiring pinning. */
- (void)testPinningRequired {
    SecPolicyRef policy = NULL;

    // init domains are excluded from IDS pinning rules
    policy = SecPolicyCreateSSL(true, CFSTR("init.ess.apple.com"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultRecoverableTrustFailure, "Unpinned connection succeeeded when pinning required");

    policy = SecPolicyCreateAppleIDSServiceContext(CFSTR("init.ess.apple.com"), NULL);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Policy pinned connection failed when pinning required");

#if !TARGET_OS_BRIDGE
    /* BridgeOS doesn't have pinning DB */
    policy = SecPolicyCreateSSL(true, CFSTR("profile.ess.apple.com"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Systemwide hostname pinned connection failed when pinning required");
#endif

    NSDictionary *policy_properties = @{
                                        (__bridge NSString *)kSecPolicyName : @"init.ess.apple.com",
                                        (__bridge NSString *)kSecPolicyPolicyName : @"IDS",
                                        };
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)policy_properties);
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Systemwide policy name pinned connection failed when pinning required");

    policy = SecPolicyCreateSSL(true, CFSTR("init.ess.apple.com"));
    SecPolicySetOptionsValue(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy_exception(policy, true), kSecTrustResultUnspecified, "Unpinned connection failed when pinning exception set");
}

static void test_escrow_with_anchor_roots(CFArrayRef anchors)
{
    SecCertificateRef escrowLeafCert = NULL;
    SecTrustResultType trustResult = kSecTrustResultUnspecified;
    SecPolicyRef policy = NULL;
    CFArrayRef certs = NULL;
    SecTrustRef trust = NULL;

    isnt(escrowLeafCert = SecCertificateCreateWithBytes(NULL, kEscrowLeafCert, sizeof(kEscrowLeafCert)),
        NULL, "could not create aCert from kEscrowLeafCert");

    certs = CFArrayCreate(NULL, (const void **)&escrowLeafCert, 1, NULL);

    isnt(policy = SecPolicyCreateWithProperties(kSecPolicyAppleEscrowService, NULL),
        NULL, "could not create Escrow policy for GM Escrow Leaf cert");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "could not create trust for escrow service test GM Escrow Leaf cert");

    SecTrustSetAnchorCertificates(trust, anchors);

    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate escrow service trust for GM Escrow Leaf cert");

    is_status(trustResult, kSecTrustResultUnspecified,
        "trust is not kSecTrustResultUnspecified for GM Escrow Leaf cert");

    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(escrowLeafCert);

}

static void test_pcs_escrow_with_anchor_roots(CFArrayRef anchors)
{
    SecCertificateRef leafCert = NULL;
    SecTrustResultType trustResult = kSecTrustResultUnspecified;
    SecPolicyRef policy = NULL;
    CFArrayRef certs = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    OSStatus status;

    isnt(leafCert = SecCertificateCreateWithBytes(NULL, kPCSEscrowLeafCert, sizeof(kPCSEscrowLeafCert)),
        NULL, "could not create leafCert from kPCSEscrowLeafCert");

    certs = CFArrayCreate(NULL, (const void **)&leafCert, 1, NULL);

    isnt(policy = SecPolicyCreateWithProperties(kSecPolicyApplePCSEscrowService, NULL),
        NULL, "could not create PCS Escrow policy for GM PCS Escrow Leaf cert");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "could not create trust for PCS escrow service test GM PCS Escrow Leaf cert");

    /* Set explicit verify date: Mar 18 2016. */
    isnt(date = CFDateCreate(NULL, 480000000.0), NULL, "create verify date");
    status = (date) ? SecTrustSetVerifyDate(trust, date) : errSecParam;
    ok_status(status, "set date");

    SecTrustSetAnchorCertificates(trust, anchors);

    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate PCS escrow service trust for GM PCS Escrow Leaf cert");

    is_status(trustResult, kSecTrustResultUnspecified,
        "trust is not kSecTrustResultUnspecified for GM PCS Escrow Leaf cert");

    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(leafCert);

}

#if !TARGET_OS_BRIDGE
/* Bridge OS doesn't have certificates bundle */
- (void)test_escrow
{
    CFArrayRef anchors = NULL;
    isnt(anchors = SecCertificateCopyEscrowRoots(kSecCertificateProductionEscrowRoot), NULL, "unable to get production anchors");
    test_escrow_with_anchor_roots(anchors);
    CFReleaseSafe(anchors);
}

- (void)test_pcs_escrow
{
    CFArrayRef anchors = NULL;
    isnt(anchors = SecCertificateCopyEscrowRoots(kSecCertificateProductionPCSEscrowRoot), NULL, "unable to get production PCS roots");
    test_pcs_escrow_with_anchor_roots(anchors);
    CFReleaseSafe(anchors);
}

- (void)test_escrow_roots
{
    CFArrayRef baselineRoots = NULL;
    isnt(baselineRoots = SecCertificateCopyEscrowRoots(kSecCertificateBaselineEscrowRoot), NULL, "unable to get baseline roots");
    ok(baselineRoots && CFArrayGetCount(baselineRoots) > 0, "baseline roots array empty");
    CFReleaseSafe(baselineRoots);

    CFArrayRef productionRoots = NULL;
    isnt(productionRoots = SecCertificateCopyEscrowRoots(kSecCertificateProductionEscrowRoot), NULL, "unable to get production roots");
    ok(productionRoots && CFArrayGetCount(productionRoots) > 0, "production roots array empty");
    CFReleaseSafe(productionRoots);

    CFArrayRef baselinePCSRoots = NULL;
    isnt(baselinePCSRoots = SecCertificateCopyEscrowRoots(kSecCertificateBaselinePCSEscrowRoot), NULL, "unable to get baseline PCS roots");
    ok(baselinePCSRoots && CFArrayGetCount(baselinePCSRoots) > 0, "baseline PCS roots array empty");
    CFReleaseSafe(baselinePCSRoots);

    CFArrayRef productionPCSRoots = NULL;
    isnt(productionPCSRoots = SecCertificateCopyEscrowRoots(kSecCertificateProductionPCSEscrowRoot), NULL, "unable to get production PCS roots");
    ok(productionPCSRoots && CFArrayGetCount(productionPCSRoots) > 0, "production PCS roots array empty");
    CFReleaseSafe(productionPCSRoots);
}
#endif // !TARGET_OS_BRIDGE

- (void)testPassbook {
    SecPolicyRef policy;

    CFDataRef goodSig = CFDataCreate(NULL, (const UInt8*) _USAirwaysCorrect_signature,
                                     sizeof(_USAirwaysCorrect_signature));
    CFDataRef goodManifest = CFDataCreate(NULL, (const UInt8*) _USAirwaysCorrect_manifest_json,
                                     sizeof(_USAirwaysCorrect_manifest_json));
    CFDataRef badSig = CFDataCreate(NULL, (const UInt8*) _USAirwaysWrongTeamID_signature,
                                    sizeof(_USAirwaysWrongTeamID_signature));
    CFDataRef badManifest = CFDataCreate(NULL, (const UInt8*) _USAirwaysWrongTeamID_manifest_json,
                                    sizeof(_USAirwaysWrongTeamID_manifest_json));

    /*
     OSStatus SecCMSVerifySignedData(CFDataRef message, CFDataRef detached_contents,
     SecPolicyRef policy, SecTrustRef *trustref, CFArrayRef additional_certificates,
     CFDataRef *attached_contents, CFDictionaryRef *message_attributes)
     */

    /* Case 1: verify signature with good values */
    isnt(policy = SecFrameworkPolicyCreatePassbookCardSigner(CFSTR("pass.com.apple.cardman"), CFSTR("A1B2C3D4E5")),
         NULL, "create policy");
    ok_status(SecCMSVerifySignedData(goodSig, goodManifest, policy, NULL, NULL, NULL, NULL), "verify signed data 1");
    CFReleaseNull(policy);
    CFRelease(goodManifest);
    CFRelease(goodSig);
    policy = NULL;

    /* Case 2: verify signature with bad values */
    isnt(policy = SecFrameworkPolicyCreatePassbookCardSigner(CFSTR("pass.com.apple.cardman"), CFSTR("IAMBOGUS")),
         NULL, "create policy");
    isnt(SecCMSVerifySignedData(badSig, badManifest, policy, NULL, NULL, NULL, NULL), errSecSuccess, "verify signed data 2");
    CFReleaseNull(policy);
    policy = NULL;


    /* Case 3: get trust reference back from SecCMSVerifySignedData and verify it ourselves */
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult;
    isnt(policy = SecFrameworkPolicyCreatePassbookCardSigner(CFSTR("pass.com.apple.cardman"), CFSTR("IAMBOGUS")),
         NULL, "create policy");
    ok_status(SecCMSVerifySignedData(badSig, badManifest, policy, &trust, NULL, NULL, NULL), "verify signed data 3");
    isnt(trust, NULL, "get trust");
    if (!trust) { goto errOut; }
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultRecoverableTrustFailure, "recoverable trustResult expected (ok)");

    /* Case 4: set test policy on the trust and evaluate */
    CFReleaseNull(policy);
    policy = SecPolicyCreatePassbookCardSigner(CFSTR("pass.com.apple.cardman"), CFSTR("IAMBOGUS"));
    ok_status(SecTrustSetPolicies(trust, policy));
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultRecoverableTrustFailure, "recoverable trustResult expected (ok)");

errOut:
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFRelease(badManifest);
    CFRelease(badSig);
    trust = NULL;
}

- (void)testiTunesStoreURLBag
{
    SecTrustRef trust;
    SecCertificateRef leaf, root;
    SecPolicyRef policy;
    CFDataRef urlBagData;
    CFDictionaryRef urlBagDict;

    isnt(urlBagData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, url_bag, sizeof(url_bag), kCFAllocatorNull), NULL,
        "load url bag");
    isnt(urlBagDict = CFPropertyListCreateWithData(kCFAllocatorDefault, urlBagData, kCFPropertyListImmutable, NULL, NULL), NULL,
        "parse url bag");
    CFReleaseSafe(urlBagData);
    CFArrayRef certs_data = CFDictionaryGetValue(urlBagDict, CFSTR("certs"));
    CFDataRef cert_data = CFArrayGetValueAtIndex(certs_data, 0);
    isnt(leaf = SecCertificateCreateWithData(kCFAllocatorDefault, cert_data), NULL, "create leaf");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, sITunesStoreRootCertificate, sizeof(sITunesStoreRootCertificate)), NULL, "create root");

    CFArrayRef certs = CFArrayCreate(kCFAllocatorDefault, (const void **)&leaf, 1, NULL);
    CFArrayRef anchors = CFArrayCreate(kCFAllocatorDefault, (const void **)&root, 1, NULL);
    CFDataRef signature = CFDictionaryGetValue(urlBagDict, CFSTR("signature"));
    CFDataRef bag = CFDictionaryGetValue(urlBagDict, CFSTR("bag"));

    isnt(policy = SecPolicyCreateiTunesStoreURLBag(), NULL, "create policy instance");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust for leaf");
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set iTMS anchor for evaluation");

    /* it'll have just expired */
    CFDateRef date = CFDateCreateForGregorianZuluMoment(NULL, 2008, 11, 7, 22, 0, 0);
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    CFReleaseSafe(date);

    SecTrustResultType trustResult;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
        "trust is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");
    SecKeyRef pub_key_leaf;
    isnt(pub_key_leaf = SecTrustCopyKey(trust), NULL, "get leaf pub key");
    if (!pub_key_leaf) { goto errOut; }
    CFErrorRef error = NULL;
    ok(SecKeyVerifySignature(pub_key_leaf, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1, bag, signature, &error),
       "verify signature on bag");
    CFReleaseNull(error);

errOut:
    CFReleaseSafe(pub_key_leaf);
    CFReleaseSafe(urlBagDict);
    CFReleaseSafe(certs);
    CFReleaseSafe(anchors);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(leaf);
    CFReleaseSafe(root);
}

- (void)testSetSHA256Pins
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _pallas_leaf, sizeof(_pallas_leaf));
    SecCertificateRef ca = SecCertificateCreateWithBytes(NULL, _pallas_ca, sizeof(_pallas_ca));
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    NSArray *certs = @[ (__bridge id)leaf, (__bridge id)ca];
    TestTrustEvaluation *test = [[TestTrustEvaluation alloc] initWithCertificates:certs
                                                                         policies:@[(__bridge id)policy]];
    [test setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:630000000]]; // December 18, 2020 at 8:00:00 AM PST

    // Baseline verification with no pins
    XCTAssert([test evaluate:nil]);

    // Bad leaf pin
    NSArray *pins = @[];
    SecPolicySetSHA256Pins(policy, (__bridge CFArrayRef)pins, NULL);
    SecTrustSetPolicies(test.trust, policy);
    XCTAssertFalse([test evaluate:nil]);

    // Bad CA pin
    SecPolicySetSHA256Pins(policy, NULL, (__bridge CFArrayRef)pins);
    SecTrustSetPolicies(test.trust, policy);
    XCTAssertFalse([test evaluate:nil]);

    // Correct leaf pin
    pins = @[ (__bridge_transfer NSData*)SecCertificateCopySubjectPublicKeyInfoSHA256Digest(leaf)];
    SecPolicySetSHA256Pins(policy, (__bridge CFArrayRef)pins, NULL);
    SecTrustSetPolicies(test.trust, policy);
    XCTAssert([test evaluate:nil]);

    // Correct CA pin
    pins = @[ (__bridge_transfer NSData*)SecCertificateCopySubjectPublicKeyInfoSHA256Digest(ca)];
    SecPolicySetSHA256Pins(policy, NULL, (__bridge CFArrayRef)pins);
    SecTrustSetPolicies(test.trust, policy);
    XCTAssert([test evaluate:nil]);
}

@end
