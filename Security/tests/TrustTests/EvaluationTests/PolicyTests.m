/*
 * Copyright (c) 2016-2023 Apple Inc. All Rights Reserved.
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
#include "PolicyTests_BIMI_data.h"
#include "PolicyTests_Parakeet_data.h"

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
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("p02-ckdatabasews.icloud.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:676000000.0]; // June 3, 2022 at 6:46:40 PM PDT
    NSArray *certs = nil, *enforcement_anchors = nil;
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];

    require_action(baltimoreRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"BaltimoreCyberTrustRoot"
                                                                                         subdirectory:(NSString *)kSecTrustTestPinningPolicyResources],
                   errOut, fail("failed to create geotrust root"));
    require_action(appleISTCA2 = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"AppleISTCA2G1-Baltimore"
                                                                                       subdirectory:(NSString *)kSecTrustTestPinningPolicyResources],
                   errOut, fail("failed to create apple IST CA"));
    require_action(pinnedNonCT = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"ckdatabasews"
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
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckLeafSPKISHA256, emptySPKISHA256);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckCAspkiSHA256, emptySPKISHA256);
    CFDictionaryRef policyOptions = SecPolicyGetOptions(policy);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);
    SecPolicyReconcilePinningRequiredIfInfoSpecified((CFMutableDictionaryRef)policyOptions);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), false);

    // kSecPolicyCheckPinningRequired overrules the other two policies.
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckLeafSPKISHA256, nonemtpySPKISHA256);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckCAspkiSHA256, emptySPKISHA256);
    policyOptions = SecPolicyGetOptions(policy);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), true);
    SecPolicyReconcilePinningRequiredIfInfoSpecified((CFMutableDictionaryRef)policyOptions);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckPinningRequired), true);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckLeafSPKISHA256), false);
    is(CFDictionaryContainsKey(policyOptions, kSecPolicyCheckCAspkiSHA256), false);

    // kSecPolicyCheckPinningRequired overrules the other two policies.
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckLeafSPKISHA256, emptySPKISHA256);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckCAspkiSHA256, nonemtpySPKISHA256);
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
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckLeafSPKISHA256, emptySPKISHA256);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckCAspkiSHA256, emptySPKISHA256);
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

- (void)testSecPolicyCreateSSLWithATSPinning
{
    // NSPinnedLeafIdentities
    CFDictionaryRef nsAppTransportSecurityDict = [self getNSAppTransportSecurityFromDictionaryInfoFile:@"NSPinnedDomains_leaf"];
    is(nsAppTransportSecurityDict != NULL, true);

    SecPolicyRef policy = SecPolicyCreateSSLWithATSPinning(true, CFSTR("example.org"), nsAppTransportSecurityDict);
    is(policy != NULL, true);
    CFDictionaryRef policyOptions = SecPolicyGetOptions(policy);
    XCTAssert([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckLeafSPKISHA256]);

    // NSPinnedCAIdentities
    nsAppTransportSecurityDict = [self getNSAppTransportSecurityFromDictionaryInfoFile:@"NSPinnedDomains_ca"];
    is(nsAppTransportSecurityDict != NULL, true);

    XCTAssert(SecPolicySetATSPinning(policy, nsAppTransportSecurityDict));
    policyOptions = SecPolicyGetOptions(policy);
    XCTAssert([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckCAspkiSHA256]);
    CFReleaseNull(policy);

    // Hostname mismatch
    policy = SecPolicyCreateSSLWithATSPinning(true, CFSTR("example.net"), nsAppTransportSecurityDict);
    XCTAssertNotEqual(policy, NULL);
    policyOptions = SecPolicyGetOptions(policy);
    XCTAssertFalse([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckCAspkiSHA256]);

    // Change hostname to match and update ATS rules
    XCTAssert(SecPolicySetSSLHostname(policy, CFSTR("example.org")));
    XCTAssert(SecPolicySetATSPinning(policy, nsAppTransportSecurityDict));
    policyOptions = SecPolicyGetOptions(policy);
    XCTAssert([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckCAspkiSHA256]);
    CFReleaseNull(policy);

    // No hostname
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
#ifndef __clang_analyzer__
    policy =  SecPolicyCreateSSLWithATSPinning(true, NULL, nsAppTransportSecurityDict);
#endif
#pragma clang diagnostic pop
    policyOptions = SecPolicyGetOptions(policy);
    XCTAssertFalse([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckCAspkiSHA256]);
    CFReleaseNull(policy);

    // No pinned domains in dictionary
    NSDictionary *nonPinningATSDict = @{ @"NSAppTransportSecurity" : @{} };
    policy = SecPolicyCreateSSLWithATSPinning(true, CFSTR("example.net"), (__bridge CFDictionaryRef)nonPinningATSDict);
    XCTAssertNotEqual(policy, NULL);
    policyOptions = SecPolicyGetOptions(policy);
    XCTAssertFalse([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckCAspkiSHA256]);
    XCTAssertFalse([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckLeafSPKISHA256]);

    XCTAssert(SecPolicySetATSPinning(policy, (__bridge CFDictionaryRef)nonPinningATSDict));
    policyOptions = SecPolicyGetOptions(policy);
    XCTAssertFalse([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckCAspkiSHA256]);
    XCTAssertFalse([[(__bridge NSDictionary *)policyOptions allKeys] containsObject:(__bridge NSString*)kSecPolicyCheckLeafSPKISHA256]);
    CFReleaseNull(policy);
}

- (CFDictionaryRef)getNSAppTransportSecurityFromDictionaryInfoFile:(NSString *)fileName
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

    return nsAppTransportSecurityDict;
}

- (CFDictionaryRef)getNSPinnedDomainsFromDictionaryInfoFile:(NSString *)fileName
{
    CFDictionaryRef nsAppTransportSecurityDict = [self getNSAppTransportSecurityFromDictionaryInfoFile:fileName];
    if (!nsAppTransportSecurityDict) {
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

- (NSData *)random
{
    uint8_t random[32];
    (void)SecRandomCopyBytes(kSecRandomDefault, sizeof(random), random);
    return [[NSData alloc] initWithBytes:random length:sizeof(random)];
}

- (void)testSetTransparentConnectionPins
{
#if TARGET_OS_BRIDGE // bridgeOS doesn't have Transparent Connections
    XCTSkip();
#endif
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
#if TARGET_OS_BRIDGE // bridgeOS doesn't have Transparent Connections
    XCTSkip();
#endif

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
#if TARGET_OS_BRIDGE // bridgeOS doesn't have Transparent Connections
    XCTSkip();
#endif

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
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultRecoverableTrustFailure, "Unpinned connection succeeeded when pinning required");

    policy = SecPolicyCreateAppleIDSServiceContext(CFSTR("init.ess.apple.com"), NULL);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Policy pinned connection failed when pinning required");

#if !TARGET_OS_BRIDGE
    /* BridgeOS doesn't have pinning DB */
    policy = SecPolicyCreateSSL(true, CFSTR("profile.ess.apple.com"));
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Systemwide hostname pinned connection failed when pinning required");
#endif

    NSDictionary *policy_properties = @{
                                        (__bridge NSString *)kSecPolicyName : @"init.ess.apple.com",
                                        (__bridge NSString *)kSecPolicyPolicyName : @"IDS",
                                        };
    policy = SecPolicyCreateWithProperties(kSecPolicyAppleSSL, (__bridge CFDictionaryRef)policy_properties);
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
    is(test_with_policy(policy), kSecTrustResultUnspecified, "Systemwide policy name pinned connection failed when pinning required");

    policy = SecPolicyCreateSSL(true, CFSTR("init.ess.apple.com"));
    SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckPinningRequired, kCFBooleanTrue);
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

- (void)test_escrow
{
#if TARGET_OS_BRIDGE
    /* Bridge OS doesn't have certificates bundle */
    XCTSkip();
#endif
    CFArrayRef anchors = NULL;
    isnt(anchors = SecCertificateCopyEscrowRoots(kSecCertificateProductionEscrowRoot), NULL, "unable to get production anchors");
    test_escrow_with_anchor_roots(anchors);
    CFReleaseSafe(anchors);
}

- (void)test_pcs_escrow
{
#if TARGET_OS_BRIDGE
    /* Bridge OS doesn't have certificates bundle */
    XCTSkip();
#endif
    CFArrayRef anchors = NULL;
    isnt(anchors = SecCertificateCopyEscrowRoots(kSecCertificateProductionPCSEscrowRoot), NULL, "unable to get production PCS roots");
    test_pcs_escrow_with_anchor_roots(anchors);
    CFReleaseSafe(anchors);
}

- (void)test_escrow_roots
{
#if TARGET_OS_BRIDGE
    /* Bridge OS doesn't have certificates bundle */
    XCTSkip();
#endif
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
    CFReleaseNull(policy);
}

static void test_shortcut_signing(CFDateRef date, bool disableTemporalCheck, bool useShortcutPolicy, bool expectedResult)
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    SecCertificateRef leaf = NULL;
    SecCertificateRef ca = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _aidvrs5_leaf, sizeof(_aidvrs5_leaf)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _aaica2_ca, sizeof(_aaica2_ca)), NULL, "ca");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    if (useShortcutPolicy) {
        policy = SecPolicyCreateAppleIDValidationShortcutSigningPolicy();
    } else {
        policy = SecPolicyCreateAppleIDValidationRecordSigningPolicy();
    }
    if (!policy) { goto exit; }
    if (disableTemporalCheck && !useShortcutPolicy) {
        // this call would be redundant if using the shortcut policy
        SecPolicySetOptionsValue_internal(policy, kSecPolicyCheckTemporalValidity, kCFBooleanFalse);
    }
    err = SecTrustCreateWithCertificates(certs, policy, &trust);
    is(err, errSecSuccess, "error creating trust reference");
    isnt(trust, NULL, "failed to obtain SecTrustRef");
    if (err != errSecSuccess || trust == NULL) { goto exit; }
    if (date) {
        is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
        if (err != errSecSuccess) { goto exit; }
    }
    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    is(expectedResult, errcode == errSecSuccess, "check error code");
    is(isTrusted, expectedResult, "check trust result");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
}

- (void)testShortcutSigning
{
    CFDateRef dateWithinRange =  CFDateCreate(NULL, 655245000.0); /* Oct 6 2021 */
    CFDateRef dateOutsideRange = CFDateCreate(NULL, 781500000.0); /* Oct 6 2025 */

    // expected success case using SecPolicyCreateAppleIDValidationRecordSigningPolicy
    // evaluation at time within validity period, with expiration check disabled
    test_shortcut_signing(dateWithinRange, true, false, true);

    // expected success case using SecPolicyCreateAppleIDValidationRecordSigningPolicy
    // evaluation at time outside validity period, with expiration check disabled
    test_shortcut_signing(dateOutsideRange, true, false, true);

    // expected failure case using SecPolicyCreateAppleIDValidationRecordSigningPolicy
    // evaluation at time outside validity period, without disabling expiration check
    test_shortcut_signing(dateOutsideRange, false, false, false);

    // expected success case using SecPolicyCreateAppleIDValidationShortcutSigningPolicy
    // evaluation at time within validity period
    test_shortcut_signing(dateWithinRange, true, true, true);

    // expected success case using SecPolicyCreateAppleIDValidationShortcutSigningPolicy
    // evaluation at time outside validity period
    test_shortcut_signing(dateOutsideRange, true, true, true);

    CFReleaseNull(dateWithinRange);
    CFReleaseNull(dateOutsideRange);
}

- (void)testApplePayModelSigning
{
    id leaf = [self SecCertificateCreateFromResource:@"ApplePayModelSigning" subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    id intermediate = [self SecCertificateCreateFromResource:@"AppleSystemIntegrationCA4" subdirectory:(NSString *)kSecTrustTestPinningPolicyResources];
    NSArray *certs = @[ leaf, intermediate];
    SecPolicyRef noExpirationPolicy = SecPolicyCreateApplePayModelSigning(false);
    SecPolicyRef expirationPolicy = SecPolicyCreateApplePayModelSigning(true);

    TestTrustEvaluation *test = [[TestTrustEvaluation alloc] initWithCertificates:certs policies:@[(__bridge id)noExpirationPolicy]];
    XCTAssert([test evaluate:nil]);

    NSError *error = nil;
    test = [[TestTrustEvaluation alloc] initWithCertificates:certs policies:@[(__bridge id)expirationPolicy]];
    XCTAssertFalse([test evaluate:&error]);
    XCTAssertNotNil(error);
    XCTAssertEqual(error.code, errSecCertificateExpired);

    CFBridgingRelease((__bridge SecCertificateRef)leaf);
    CFBridgingRelease((__bridge SecCertificateRef)intermediate);
    CFReleaseNull(noExpirationPolicy);
    CFReleaseNull(expirationPolicy);
}

- (void)testLargePolicyTreeLimit
{
    static const UInt8 *blobs[31] = {
        lptcert_00, lptcert_01, lptcert_02, lptcert_03, lptcert_04, lptcert_05, lptcert_06, lptcert_07,
        lptcert_08, lptcert_09, lptcert_10, lptcert_11, lptcert_12, lptcert_13, lptcert_14, lptcert_15,
        lptcert_16, lptcert_17, lptcert_18, lptcert_19, lptcert_20, lptcert_21, lptcert_22, lptcert_23,
        lptcert_24, lptcert_25, lptcert_26, lptcert_27, lptcert_28, lptcert_29, lptcert_30
    };
    CFIndex lengths[31] = {
        sizeof(lptcert_00), sizeof(lptcert_01), sizeof(lptcert_02), sizeof(lptcert_03),
        sizeof(lptcert_04), sizeof(lptcert_05), sizeof(lptcert_06), sizeof(lptcert_07),
        sizeof(lptcert_08), sizeof(lptcert_09), sizeof(lptcert_10), sizeof(lptcert_11),
        sizeof(lptcert_12), sizeof(lptcert_13), sizeof(lptcert_14), sizeof(lptcert_15),
        sizeof(lptcert_16), sizeof(lptcert_17), sizeof(lptcert_18), sizeof(lptcert_19),
        sizeof(lptcert_20), sizeof(lptcert_21), sizeof(lptcert_22), sizeof(lptcert_23),
        sizeof(lptcert_24), sizeof(lptcert_25), sizeof(lptcert_26), sizeof(lptcert_27),
        sizeof(lptcert_28), sizeof(lptcert_29), sizeof(lptcert_30)
    };
    CFAbsoluteTime startTime, finishTime;
    CFMutableArrayRef certs = NULL;
    SecPolicyRef policy = NULL;
    TestTrustEvaluation *test = nil;

    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    for (CFIndex idx=0; idx<31; idx++) {
        SecCertificateRef certificate = NULL;
        isnt(certificate = SecCertificateCreateWithBytes(kCFAllocatorDefault, blobs[idx], lengths[idx]), NULL, "cert");
        CFArrayAppendValue(certs, certificate);
        CFReleaseNull(certificate);
    }
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "policy");
    test = [[TestTrustEvaluation alloc] initWithCertificates:(__bridge NSArray*)certs policies:(__bridge id)policy];
    // the primary goal of this test is to check that evaluation completes within one second
    startTime = CFAbsoluteTimeGetCurrent();
    [test evaluate:nil];
    finishTime = CFAbsoluteTimeGetCurrent();
    XCTAssert(finishTime >= startTime && ((finishTime - startTime) <= 1));
exit:
    CFReleaseNull(policy);
    CFReleaseNull(certs);
}

- (void)testMDLTerminalAuth
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdlterminalauth_leaf, sizeof(mdlterminalauth_leaf)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdlterminalauth_ca, sizeof(mdlterminalauth_ca)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdlterminalauth_root, sizeof(mdlterminalauth_root)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // check EKU extension value, but don't check for leaf BC extension
    isnt(policy = SecPolicyCreateMDLTerminalAuth(true, false), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Apr 15 2023
    isnt(date = CFDateCreate(NULL, 703290000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    is(errcode, errSecSuccess, "check error code is success");
    is(isTrusted, true, "check trust result is true");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
}

- (void)testMDLTerminalAuthInvalidKeyUsage
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdltest_invalid_ku_leaf, sizeof(mdltest_invalid_ku_leaf)), NULL, "leaf");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdltest_root, sizeof(mdltest_root)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);

    // check EKU extension value, but don't check for leaf BC extension
    isnt(policy = SecPolicyCreateMDLTerminalAuth(true, false), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Apr 15 2023
    isnt(date = CFDateCreate(NULL, 703290000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    isnt(errcode, errSecSuccess, "check error code is non-success");
    isnt(isTrusted, true, "check trust result is false");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(root);
}

- (void)testMDLTerminalAuthUnknownCritical
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdltest_unknown_critical_leaf, sizeof(mdltest_unknown_critical_leaf)), NULL, "leaf");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, mdltest_root, sizeof(mdltest_root)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);

    // check EKU extension value, but don't check for leaf BC extension
    isnt(policy = SecPolicyCreateMDLTerminalAuth(true, false), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Apr 15 2023
    isnt(date = CFDateCreate(NULL, 703290000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    isnt(errcode, errSecSuccess, "check error code is non-success");
    isnt(isTrusted, true, "check trust result is false");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(root);
}

- (void)testVerifiedMarkSHA1
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    CFDataRef svg = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(svg = CFDataCreate(kCFAllocatorDefault, _BIMITests_apple_svg, sizeof(_BIMITests_apple_svg)), NULL, "svg");
    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_Apple_Inc_VMC_cer, sizeof(_BIMITests_Apple_Inc_VMC_cer)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_DigiCert_VMC_CA1_cer, sizeof(_BIMITests_DigiCert_VMC_CA1_cer)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_DigiCert_VMC_Root_cer, sizeof(_BIMITests_DigiCert_VMC_Root_cer)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // Verified Mark policy checks the given SVG data is verified
    // (this certificate uses a SHA-1 digest)
    isnt(policy = SecPolicyCreateVerifiedMark(CFSTR("apple.com"), svg), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Sep 4 2023
    isnt(date = CFDateCreate(NULL, 715540000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    is(errcode, errSecSuccess, "check error code is success");
    is(isTrusted, true, "check trust result is true");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
    CFReleaseSafe(svg);
}

- (void)testVerifiedMarkSHA1WithInvalidRepresentation
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    CFDataRef svg = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(svg = CFDataCreate(kCFAllocatorDefault, _BIMITests_apple_altered_svg, sizeof(_BIMITests_apple_altered_svg)), NULL, "svg");
    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_Apple_Inc_VMC_cer, sizeof(_BIMITests_Apple_Inc_VMC_cer)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_DigiCert_VMC_CA1_cer, sizeof(_BIMITests_DigiCert_VMC_CA1_cer)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_DigiCert_VMC_Root_cer, sizeof(_BIMITests_DigiCert_VMC_Root_cer)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // Verified Mark policy checks the given SVG data is verified
    // (this certificate uses a SHA-1 digest, and the input SVG has been modified to not match it)
    isnt(policy = SecPolicyCreateVerifiedMark(CFSTR("apple.com"), svg), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Sep 4 2023
    isnt(date = CFDateCreate(NULL, 715540000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    isnt(errcode, errSecSuccess, "check error code is NOT success");
    is(isTrusted, false, "check trust result is false");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
    CFReleaseSafe(svg);
}

- (void)testVerifiedMarkSHA256
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    CFDataRef svg = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(svg = CFDataCreate(kCFAllocatorDefault, _BIMITests_boa_svg, sizeof(_BIMITests_boa_svg)), NULL, "svg");
    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_BoA_VMC_cer, sizeof(_BIMITests_BoA_VMC_cer)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_Entrust_VMC2_cer, sizeof(_BIMITests_Entrust_VMC2_cer)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_Entrust_VMRC1_cer, sizeof(_BIMITests_Entrust_VMRC1_cer)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // Verified Mark policy checks the given SVG data is verified
    // (this certificate uses a SHA-256 digest)
    isnt(policy = SecPolicyCreateVerifiedMark(CFSTR("bankofamerica.com"), svg), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Sep 4 2023
    isnt(date = CFDateCreate(NULL, 715540000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    is(errcode, errSecSuccess, "check error code is success");
    is(isTrusted, true, "check trust result is true");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
    CFReleaseSafe(svg);
}

- (void)testVerifiedMarkSHA256WithInvalidRepresentation
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    CFDataRef svg = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(svg = CFDataCreate(kCFAllocatorDefault, _BIMITests_boa_altered_svg, sizeof(_BIMITests_boa_altered_svg)), NULL, "svg");
    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_BoA_VMC_cer, sizeof(_BIMITests_BoA_VMC_cer)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_Entrust_VMC2_cer, sizeof(_BIMITests_Entrust_VMC2_cer)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _BIMITests_Entrust_VMRC1_cer, sizeof(_BIMITests_Entrust_VMRC1_cer)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // Verified Mark policy checks the given SVG data is verified
    // (this certificate uses a SHA-256 digest, and the input SVG has been modified to not match it)
    isnt(policy = SecPolicyCreateVerifiedMark(CFSTR("apple.com"), svg), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Sep 4 2023
    isnt(date = CFDateCreate(NULL, 715540000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    isnt(errcode, errSecSuccess, "check error code is NOT success");
    is(isTrusted, false, "check trust result is false");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
    CFReleaseSafe(svg);
}

- (void)testParakeetSigning
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _USAVZ_leaf_cer, sizeof(_USAVZ_leaf_cer)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _USAVZ_ca_cer, sizeof(_USAVZ_ca_cer)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _USAVZ_root_cer, sizeof(_USAVZ_root_cer)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // create policy
    isnt(policy = SecPolicyCreateParakeetSigning(), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Nov 29 2023
    isnt(date = CFDateCreate(NULL, 723000000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    is(errcode, errSecSuccess, "check error code is success");
    is(isTrusted, true, "check trust result is true");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
}

- (void)testParakeetSigningInvalidKeyUsage
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _USAVZ_leaf_cer_invalid, sizeof(_USAVZ_leaf_cer_invalid)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _USAVZ_ca_cer_invalid, sizeof(_USAVZ_ca_cer_invalid)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _USAVZ_root_cer_invalid, sizeof(_USAVZ_root_cer_invalid)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // create policy
    isnt(policy = SecPolicyCreateParakeetSigning(), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date: Nov 29 2023
    isnt(date = CFDateCreate(NULL, 723000000.0), NULL, "create verify date");
    if (!date) { goto exit; }
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    isnt(errcode, errSecSuccess, "check error code is not success"); /* expect failure */
    is(isTrusted, false, "check trust result is false");
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(date);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
}

- (void)runParakeetService:(CFStringRef)hostname
               withContext:(CFDictionaryRef __nullable)context
                    onDate:(CFDateRef)date
              expectSuccess:(BOOL)expectSuccess
{
    OSStatus err;
    CFIndex errcode = errSecSuccess;
    CFMutableArrayRef certs = NULL;
    CFMutableArrayRef anchors = NULL;
    SecCertificateRef leaf = NULL, ca = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFErrorRef error = NULL;
    bool isTrusted = false;

    isnt(leaf = SecCertificateCreateWithBytes(kCFAllocatorDefault, _Syn_leaf_cer, sizeof(_Syn_leaf_cer)), NULL, "leaf");
    isnt(ca = SecCertificateCreateWithBytes(kCFAllocatorDefault, _Syn_ca_cer, sizeof(_Syn_ca_cer)), NULL, "ca");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, _Syn_root_cer, sizeof(_Syn_root_cer)), NULL, "root");
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "certs");
    if (!certs) { goto exit; }
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, ca);

    // create policy
    isnt(policy = SecPolicyCreateParakeetService(hostname, context), NULL, "policy");
    if (!policy) { goto exit; }

    is(err = SecTrustCreateWithCertificates(certs, policy, &trust), errSecSuccess, "trust");
    if (err != errSecSuccess || trust == NULL) { goto exit; }

    // set anchors
    isnt(anchors = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "anchors");
    if (!anchors) { goto exit; }
    CFArrayAppendValue(anchors, root);
    is(err = SecTrustSetAnchorCertificates(trust, anchors), errSecSuccess, "set anchors");
    if (err != errSecSuccess) { goto exit; }

    // set verify date
    is(err = SecTrustSetVerifyDate(trust, date), errSecSuccess, "set verify date");
    if (err != errSecSuccess) { goto exit; }

    isTrusted = SecTrustEvaluateWithError(trust, &error);
    if (!isTrusted) {
        if (error) {
            errcode = CFErrorGetCode(error);
        } else {
            errcode = errSecInternalComponent;
        }
    }
    if (expectSuccess) {
        is(errcode, errSecSuccess, "check error code is success");
        is(isTrusted, true, "check trust result is true");
    } else {
        isnt(errcode, errSecSuccess, "check error code is not success");
        is(isTrusted, false, "check trust result is false");
    }
exit:
    CFReleaseSafe(error);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseSafe(leaf);
    CFReleaseSafe(ca);
    CFReleaseSafe(root);
}

-(void)testParakeetService
{
    const CFStringRef validName = CFSTR("mccmnc310890.smh.syniverse.com");
    const CFStringRef bogusName = CFSTR("mccxxx420420.smh.syniverse.com");
    CFDateRef freshDate = CFDateCreate(NULL, 731600420.0); /* Mar 08 2024 */
    CFDateRef staleDate = CFDateCreate(NULL, 735350420.0); /* Apr 20 2024 */
    NSDictionary *fresh_context = @{
        @"fresh": (id)kCFBooleanTrue,
    };
    NSDictionary *fresh_verify_context = @{
        @"fresh": (id)kCFBooleanTrue,
        @"verify": (__bridge NSDate*)freshDate,
    };
    NSDictionary *stale_verify_context = @{
        @"fresh": (id)kCFBooleanTrue,
        @"verify": (__bridge NSDate*)staleDate,
    };

    // hostname present in SAN, should succeed
    [self runParakeetService:validName withContext:NULL onDate:freshDate expectSuccess:YES];
    // hostname not present in SAN, should fail
    [self runParakeetService:bogusName withContext:NULL onDate:freshDate expectSuccess:NO];

    // perform fresh check for notBefore date less than 2 days old from freshDate
    // (expect fail since default check is based on system time and our test cert is older than that)
    [self runParakeetService:validName withContext:(__bridge CFDictionaryRef)fresh_context onDate:freshDate expectSuccess:NO];
    // perform fresh check for notBefore date over 2 days old from staleDate, should fail
    [self runParakeetService:validName withContext:(__bridge CFDictionaryRef)fresh_context onDate:staleDate expectSuccess:NO];
    // perform fresh check with verify date less than 2 days old from notBefore date
    [self runParakeetService:validName withContext:(__bridge CFDictionaryRef)fresh_verify_context onDate:freshDate expectSuccess:YES];
    // perform fresh check with verify date over 2 days old from notBeforeDate
    [self runParakeetService:validName withContext:(__bridge CFDictionaryRef)stale_verify_context onDate:staleDate expectSuccess:NO];

    CFReleaseSafe(freshDate);
    CFReleaseSafe(staleDate);
}

@end
