/*
 * Copyright (c) 2026 Apple Inc. All Rights Reserved.
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
 *
 */

#include <AssertMacros.h>
#include <XCTest/XCTest.h>
#include <os/transaction_private.h>

#include <Security/SecInternalReleasePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <utilities/SecAppleAnchorPriv.h>

#include "OSX/sec/ipc/securityd_client.h"
#include "trust/trustd/trustd_spi.h"

#include "../TrustEvaluationTestHelpers.h"

/* Common test cases for Apple Anchor framework interfaces
 * Adopted by TrustInterfaceXPCTests and TrustInterfaceTrustdTests in order
 * to run the tests both through the flow that XPC's to trustd and through
 * the flow that directly calls trustd spi. */
@interface TrustAppleAnchorTestCase : XCTestCase
@end

@implementation TrustAppleAnchorTestCase

- (id _Nullable) CF_RETURNS_RETAINED SecCertificateCreateFromResource:(NSString *)name
                                                         subdirectory:(NSString *)dir
{
    NSURL *url = [[NSBundle bundleForClass:[self class]] URLForResource:name withExtension:@".cer"
                                                           subdirectory:dir];
    NSData *certData = [NSData dataWithContentsOfURL:url];
    if (!certData) {
        return nil;
    }
    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (__bridge CFDataRef)certData);
    return (__bridge id)cert;
}

- (void)testCopyBuiltInAnchors_DisallowNonProduction {
    NSDictionary *anchors = CFBridgingRelease(SecCopyBuiltInAppleTrustAnchors(false));
    XCTAssertNotNil(anchors);
    uint8_t num_prod_anchors = 0;
    uint8_t num_test_anchors = 0;
    for (NSNumber *production in anchors.allValues) {
        if ([production boolValue]) {
            num_prod_anchors++;
        } else {
            num_test_anchors++;
        }
    }
    if (SecIsInternalRelease()) {
        XCTAssertEqual(num_prod_anchors, 6);
        XCTAssertGreaterThanOrEqual(num_test_anchors, 8);
    } else {
        XCTAssertEqual(num_prod_anchors, 6);
        XCTAssertEqual(num_test_anchors, 0);
    }
}

- (void)testCopyBuiltInAnchors_AllowNonProduction {
    NSDictionary *anchors = CFBridgingRelease(SecCopyBuiltInAppleTrustAnchors(true));
    XCTAssertNotNil(anchors);
    uint8_t num_prod_anchors = 0;
    uint8_t num_test_anchors = 0;
    for (NSNumber *production in anchors.allValues) {
        if ([production boolValue]) {
            num_prod_anchors++;
        } else {
            num_test_anchors++;
        }
    }
    if (SecIsInternalRelease()) {
        XCTAssertEqual(num_prod_anchors, 6);
        XCTAssertGreaterThanOrEqual(num_test_anchors, 8);
    } else {
        XCTAssertEqual(num_prod_anchors, 6);
        XCTAssertGreaterThanOrEqual(num_test_anchors, 8);
    }
}

- (void)testIsAppleAnchorForPolicy {
    XCTSkipIf(TARGET_OS_BRIDGE); // BridgeOS doesn't support OTA trust store

    typedef struct {
        NSString *rootName;
        CFStringRef policyOid;
        SecAppleTrustAnchorFlags flags;
        bool frameworkResult;
    } IsAppleAnchorForPolicyTestCase;

    // Define test cases
    // Production roots should be trusted for all policies regardless of flags
    // Test roots should only be trusted when flags include test anchors (on internal) or allow non-production
    // OTA roots should only be trusted in the framework and should be constrained by policy
    // Fake roots should never be trusted
    IsAppleAnchorForPolicyTestCase isAppleAnchorTestCases[] = {
        // AppleRootG2 - Production root (built-in, legacy)
        {@"AppleRootG2", NULL, 0, true},
        {@"AppleRootG2", NULL, 1, true},
        {@"AppleRootG2", NULL, 2, true},
        {@"AppleRootG2", NULL, 3, true},
        {@"AppleRootG2", kSecPolicyAppleX509Basic, 0, true},
        {@"AppleRootG2", kSecPolicyAppleX509Basic, 1, true},
        {@"AppleRootG2", kSecPolicyAppleX509Basic, 2, true},
        {@"AppleRootG2", kSecPolicyAppleX509Basic, 3, true},
        {@"AppleRootG2", kSecPolicyAppleSSLServer, 0, true},
        {@"AppleRootG2", kSecPolicyAppleSSLServer, 1, true},
        {@"AppleRootG2", kSecPolicyAppleSSLServer, 2, true},
        {@"AppleRootG2", kSecPolicyAppleSSLServer, 3, true},
        {@"AppleRootG2", kSecPolicyAppleMobileAsset, 0, true},
        {@"AppleRootG2", kSecPolicyAppleMobileAsset, 1, true},
        {@"AppleRootG2", kSecPolicyAppleMobileAsset, 2, true},
        {@"AppleRootG2", kSecPolicyAppleMobileAsset, 3, true},
        {@"AppleRootG2", kSecPolicyAppleSoftwareSigning, 0, true},
        {@"AppleRootG2", kSecPolicyAppleSoftwareSigning, 1, true},
        {@"AppleRootG2", kSecPolicyAppleSoftwareSigning, 2, true},
        {@"AppleRootG2", kSecPolicyAppleSoftwareSigning, 3, true},

        // ApplePlatformBootstrapECCRoot-G1 - Production root (built-in, G1)
        {@"ApplePlatformBootstrapECCRoot-G1", NULL, 0, true},
        {@"ApplePlatformBootstrapECCRoot-G1", NULL, 1, true},
        {@"ApplePlatformBootstrapECCRoot-G1", NULL, 2, true},
        {@"ApplePlatformBootstrapECCRoot-G1", NULL, 3, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleX509Basic, 0, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleX509Basic, 1, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleX509Basic, 2, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleX509Basic, 3, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSSLServer, 0, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSSLServer, 1, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSSLServer, 2, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSSLServer, 3, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleMobileAsset, 0, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleMobileAsset, 1, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleMobileAsset, 2, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleMobileAsset, 3, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSoftwareSigning, 0, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSoftwareSigning, 1, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSoftwareSigning, 2, true},
        {@"ApplePlatformBootstrapECCRoot-G1", kSecPolicyAppleSoftwareSigning, 3, true},

        // ApplePlatformTLSECCRoot-G1 - OTA production root (constrained to SSL/TLS)
        // Should only be trusted for SSL/TLS policies in framework
        {@"ApplePlatformTLSECCRoot-G1", NULL, 0, true},  // NULL = any policy
        {@"ApplePlatformTLSECCRoot-G1", NULL, 1, true},
        {@"ApplePlatformTLSECCRoot-G1", NULL, 2, true},
        {@"ApplePlatformTLSECCRoot-G1", NULL, 3, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleX509Basic, 0, true},  // Basic is also treated as "any"
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleX509Basic, 1, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleX509Basic, 2, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleX509Basic, 3, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSSLServer, 0, true},  // SSL/TLS policy
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSSLServer, 1, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSSLServer, 2, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSSLServer, 3, true},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleMobileAsset, 0, false},  // Not SSL/TLS
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleMobileAsset, 1, false},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleMobileAsset, 2, false},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleMobileAsset, 3, false},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSoftwareSigning, 0, false},  // Not SSL/TLS
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSoftwareSigning, 1, false},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSoftwareSigning, 2, false},
        {@"ApplePlatformTLSECCRoot-G1", kSecPolicyAppleSoftwareSigning, 3, false},

        // TestAppleRootCA - built-in Test root (only on internal or with allowNonProduction)
        {@"TestAppleRootCA", NULL, 0, false},
        {@"TestAppleRootCA", NULL, 1, SecIsInternalRelease()},
        {@"TestAppleRootCA", NULL, 2, false},
        {@"TestAppleRootCA", NULL, 3, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleX509Basic, 0, false},
        {@"TestAppleRootCA", kSecPolicyAppleX509Basic, 1, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleX509Basic, 2, false},
        {@"TestAppleRootCA", kSecPolicyAppleX509Basic, 3, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleSSLServer, 0, false},
        {@"TestAppleRootCA", kSecPolicyAppleSSLServer, 1, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleSSLServer, 2, false},
        {@"TestAppleRootCA", kSecPolicyAppleSSLServer, 3, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleMobileAsset, 0, false},
        {@"TestAppleRootCA", kSecPolicyAppleMobileAsset, 1, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleMobileAsset, 2, false},
        {@"TestAppleRootCA", kSecPolicyAppleMobileAsset, 3, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleSoftwareSigning, 0, false},
        {@"TestAppleRootCA", kSecPolicyAppleSoftwareSigning, 1, SecIsInternalRelease()},
        {@"TestAppleRootCA", kSecPolicyAppleSoftwareSigning, 2, false},
        {@"TestAppleRootCA", kSecPolicyAppleSoftwareSigning, 3, SecIsInternalRelease()},

        // TestApplePlatformECCRoot-G1 - OTA test root (constrained to generic Apple policies)
        // Should only be trusted for generic Apple policies in framework on internal builds with appropriate flags
        {@"TestApplePlatformECCRoot-G1", NULL, 0, false,}, // NULL = any policy
        {@"TestApplePlatformECCRoot-G1", NULL, 1, SecIsInternalRelease()},
        {@"TestApplePlatformECCRoot-G1", NULL, 2, false},
        {@"TestApplePlatformECCRoot-G1", NULL, 3, SecIsInternalRelease()},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleX509Basic, 0, false}, // basic is also any policy
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleX509Basic, 1, SecIsInternalRelease()},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleX509Basic, 2, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleX509Basic, 3, SecIsInternalRelease()},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSSLServer, 0, false},  // Not for SSL/TLS
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSSLServer, 1, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSSLServer, 2, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSSLServer, 3, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleGenericApplePinned, 0, false},  // An Apple policy assigned to this root
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleGenericApplePinned, 1, SecIsInternalRelease()},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleGenericApplePinned, 2, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleGenericApplePinned, 3, SecIsInternalRelease()},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSoftwareSigning, 0, false},  // Apple policy not assigned to this root
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSoftwareSigning, 1, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSoftwareSigning, 2, false},
        {@"TestApplePlatformECCRoot-G1", kSecPolicyAppleSoftwareSigning, 3, false},

        // FakeAppleRootCA - Not an Apple root, should never be trusted
        {@"FakeAppleRootCA", NULL, 0, false},
        {@"FakeAppleRootCA", NULL, 1, false},
        {@"FakeAppleRootCA", NULL, 2, false},
        {@"FakeAppleRootCA", NULL, 3, false},
        {@"FakeAppleRootCA", kSecPolicyAppleX509Basic, 0, false},
        {@"FakeAppleRootCA", kSecPolicyAppleX509Basic, 1, false},
        {@"FakeAppleRootCA", kSecPolicyAppleX509Basic, 2, false},
        {@"FakeAppleRootCA", kSecPolicyAppleX509Basic, 3, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSSLServer, 0, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSSLServer, 1, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSSLServer, 2, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSSLServer, 3, false},
        {@"FakeAppleRootCA", kSecPolicyAppleMobileAsset, 0, false},
        {@"FakeAppleRootCA", kSecPolicyAppleMobileAsset, 1, false},
        {@"FakeAppleRootCA", kSecPolicyAppleMobileAsset, 2, false},
        {@"FakeAppleRootCA", kSecPolicyAppleMobileAsset, 3, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSoftwareSigning, 0, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSoftwareSigning, 1, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSoftwareSigning, 2, false},
        {@"FakeAppleRootCA", kSecPolicyAppleSoftwareSigning, 3, false},
    };

    size_t testCaseCount = sizeof(isAppleAnchorTestCases) / sizeof(isAppleAnchorTestCases[0]);
    
    // Loop through all test cases
    for (size_t i = 0; i < testCaseCount; i++) {
        IsAppleAnchorForPolicyTestCase testCase = isAppleAnchorTestCases[i];

        // Load the certificate
        SecCertificateRef cert = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:testCase.rootName
                                                                                       subdirectory:@"si-20-sectrust-policies-data"];
        XCTAssertNotNil((__bridge id)cert, @"Failed to load certificate %@", testCase.rootName);
        if (!cert) {
            continue;
        }

        // Call the function under test
        bool result = SecIsAppleTrustAnchorForPolicy(cert, testCase.policyOid, testCase.flags);

        // Verify the result
        const char *policyName = testCase.policyOid ? CFStringGetCStringPtr(testCase.policyOid, kCFStringEncodingUTF8) : "NULL";
        XCTAssertEqual(result, testCase.frameworkResult,
                       @"Unexpected result for cert=%@, policy=%s, flags=%u: got %d, expected %d (suite=%@)",
                       testCase.rootName,
                       policyName ? policyName : "NULL",
                       testCase.flags,
                       result,
                       testCase.frameworkResult,
                       NSStringFromClass([self class]));

        CFRelease(cert);
    }
}

- (void)testGetAppleAnchors {
    // Test with allowNonProduction = false
    CFArrayRef anchors_no_test = SecGetAppleTrustAnchors(false);
    XCTAssertNotNil((__bridge NSArray *)anchors_no_test);
    
    if (SecIsInternalRelease()) {
        // Note: On internal builds, allowNonProduction false is ignored and test anchors are still included
        XCTAssertGreaterThanOrEqual(CFArrayGetCount(anchors_no_test), 14, @"Internal builds should return all 14 built-in anchors");
    } else {
        XCTAssertEqual(CFArrayGetCount(anchors_no_test), 6, @"Non-internal builds should return only 6 production anchors when allowNonProduction is false");
    }
    
    // Verify all returned items are SecCertificateRef objects
    for (CFIndex i = 0; i < CFArrayGetCount(anchors_no_test); i++) {
        SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(anchors_no_test, i);
        XCTAssertEqual(CFGetTypeID(cert), SecCertificateGetTypeID(), @"Array should contain only SecCertificateRef objects");
    }
    
    // Test with allowNonProduction = true
    CFArrayRef anchors_with_test = SecGetAppleTrustAnchors(true);
    XCTAssertNotNil((__bridge NSArray *)anchors_with_test);

    XCTAssertGreaterThanOrEqual(CFArrayGetCount(anchors_with_test), 14, @"With allowNonProduction = true, should return all 14 built-in anchors");

    // Verify all returned items are SecCertificateRef objects
    for (CFIndex i = 0; i < CFArrayGetCount(anchors_with_test); i++) {
        SecCertificateRef cert = (SecCertificateRef)CFArrayGetValueAtIndex(anchors_with_test, i);
        XCTAssertEqual(CFGetTypeID(cert), SecCertificateGetTypeID(), @"Array should contain only SecCertificateRef objects");
    }

    if (SecIsInternalRelease()) {
        XCTAssertEqual(CFArrayGetCount(anchors_no_test), CFArrayGetCount(anchors_with_test),
                      @"On internal builds, both allowNonProduction values should return the same number of anchors");
    }
}

- (void)testCopyAppleAnchors_ConsistentResults {
    XCTSkipIf(TARGET_OS_BRIDGE); // BridgeOS doesn't support OTA trust store

    // Call SecTrustCopyAppleAnchors twice with the same policyId and verify the results are
    // identical. On the XPC path this exercises the client-side TTL cache (rdar://174040872).
    CFStringRef policies[] = {
        kSecPolicyAppleX509Basic,
        kSecPolicyAppleGenericApplePinned,
        kSecPolicyAppleSoftwareSigning,
        NULL, // nil policyId
    };

    for (size_t i = 0; i < sizeof(policies) / sizeof(policies[0]); i++) {
        CFStringRef policy = policies[i];
        NSDictionary *first  = CFBridgingRelease(SecTrustCopyAppleAnchors(policy));
        NSDictionary *second = CFBridgingRelease(SecTrustCopyAppleAnchors(policy));
        XCTAssertNotNil(first,  @"First call returned nil for policy %@", policy);
        XCTAssertNotNil(second, @"Second call returned nil for policy %@", policy);
        XCTAssertEqualObjects(first, second, @"Repeated calls should return equal results for policy %@", policy);
    }
}
@end

/* The tests that XPC to the DUT's trustd */
@interface TrustAppleAnchorXPCTests : TrustAppleAnchorTestCase
@end

@implementation TrustAppleAnchorXPCTests
+ (void)setUp {
    /* XPC to trustd instead of using trustd built-in */
    gTrustd = NULL;
}
@end

/* The tests that run "within" trustd */
@interface TrustAppleAnchorTrustdTests : TrustAppleAnchorTestCase
@end

@implementation TrustAppleAnchorTrustdTests
static os_transaction_t transaction = nil;
+ (void)setUp {
    /* Setup the built-in trustd */
    transaction = os_transaction_create("com.apple.TrustTests.noExitWhenClean");
    NSURL *tmpDirURL = setUpTmpDir();
    trustd_init((__bridge CFURLRef) tmpDirURL);

    // "Disable" evaluation analytics (by making the sampling rate as low as possible)
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
    [defaults setInteger:INT32_MAX forKey:@"TrustEvaluationEventAnalyticsRate"];
    [defaults setInteger:INT32_MAX forKey:@"PinningEventAnalyticsRate"];
    [defaults setInteger:INT32_MAX forKey:@"SystemRootUsageEventAnalyticsRate"];
    [defaults setInteger:INT32_MAX forKey:@"TrustFailureEventAnalyticsRate"];
}
@end
