//
//  SecCertificateSourceTests.m
//  Security
//

#import <Foundation/Foundation.h>
#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustSettings.h>
#include <utilities/SecCFWrappers.h>
#include <utilities/SecAppleAnchorPriv.h>

#import "trust/trustd/SecCertificateSource.h"
#import "trust/trustd/OTATrustUtilities.h"
#import "trust/trustd/SecTrustServer.h"
#import "trust/trustd/SecPolicyServer.h"

#import "TrustDaemonTestCase.h"

@interface SecCertificateSourceTests : TrustDaemonTestCase
@property NSMutableDictionary *certs;
@property NSMutableDictionary *anchorTables;
@property NSArray *testCases;
@end



@implementation SecCertificateSourceTests

- (void)setUp {
    self.certs = [NSMutableDictionary dictionary];
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:@"ConstrainedAnchorTests-data"];
    XCTAssertTrue([certURLs count] > 0, "Unable to find test certs in bundle.");

    for(NSURL *url in certURLs) {
        NSData *certData = [NSData dataWithContentsOfURL:url];
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
        NSData *certHash = CFBridgingRelease(SecCertificateCopySHA256Digest(cert));
        NSString *hashKey = CFBridgingRelease(CFDataCopyHexString((__bridge CFDataRef)certHash));
        self.certs[hashKey] = (__bridge id)cert;
        CFReleaseNull(cert);
    }

    self.anchorTables = [NSMutableDictionary dictionary];
    NSArray <NSURL *>* anchorTableURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".plist" subdirectory:@"ConstrainedAnchorTests-data"];
    XCTAssertTrue([anchorTableURLs count] > 0, "Unable to find test Anchor tables in bundle.");

    for (NSURL *url in anchorTableURLs) {
        NSString *tableName = [url lastPathComponent];
        if ([tableName isEqualToString:@"TestCases.plist"]) {
            self.testCases = [NSArray arrayWithContentsOfURL:url];
        } else {
            NSDictionary *anchorTable = [NSDictionary dictionaryWithContentsOfURL:url];
            self.anchorTables[tableName] = anchorTable;
        }
    }
}

+ (void)tearDown {
    /* Reset the anchor lookup table */
    SecOTAPKISetConstrainedAnchorLookupTable(NULL);
}

- (void)testCopyAnchorRecordsForCertificate {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    for (NSDictionary *testCase in self.testCases) {
        NSDictionary *anchorTable = self.anchorTables[testCase[@"plist"]];
        SecOTAPKISetConstrainedAnchorLookupTable((__bridge CFDictionaryRef)anchorTable);
        SecCertificateRef cert = (__bridge SecCertificateRef)self.certs[testCase[@"certHash"]];
        CFArrayRef records = CopyAnchorRecordsForCertificate(cert);
        NSNumber *expectedRecordCount = testCase[@"certRecordCount"];
        if (expectedRecordCount && expectedRecordCount.integerValue > 0) {
            XCTAssertNotEqual(NULL, records);
            XCTAssertEqual(CFArrayGetCount(records), expectedRecordCount.integerValue);
        } else {
            XCTAssertEqual(NULL, records);
        }
        CFReleaseNull(records);
    }
}

- (void)testCopyAnchorRecordsForSPKI {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    for (NSDictionary *testCase in self.testCases) {
        NSDictionary *anchorTable = self.anchorTables[testCase[@"plist"]];
        SecOTAPKISetConstrainedAnchorLookupTable((__bridge CFDictionaryRef)anchorTable);
        SecCertificateRef cert = (__bridge SecCertificateRef)self.certs[testCase[@"certHash"]];
        CFArrayRef records = CopyAnchorRecordsForSPKI(cert);
        NSNumber *expectedRecordCount = testCase[@"keyRecordCount"];
        if (expectedRecordCount && expectedRecordCount.integerValue > 0) {
            XCTAssertNotEqual(NULL, records);
            XCTAssertEqual(CFArrayGetCount(records), expectedRecordCount.integerValue);
        } else {
            XCTAssertEqual(NULL, records);
        }
        CFReleaseNull(records);
    }
}

- (void)testCopyUsageConstraintsForCertificate {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    for (NSDictionary *testCase in self.testCases) {
        NSDictionary *anchorTable = self.anchorTables[testCase[@"plist"]];
        SecOTAPKISetConstrainedAnchorLookupTable((__bridge CFDictionaryRef)anchorTable);
        SecCertificateRef cert = (__bridge SecCertificateRef)self.certs[testCase[@"certHash"]];

        // Usage Constraints follow the *key* not the cert
        NSNumber *expectedRecordCount = testCase[@"keyRecordCount"];
        NSArray *usageConstraints = CFBridgingRelease(CopyUsageConstraintsForCertificate(cert));
        if (!expectedRecordCount || expectedRecordCount.integerValue == 0) {
            XCTAssertNil(usageConstraints);
            continue;
        }

        /* Determine which usage constraints we have*/
        bool hasSystemConstraint = false;
        bool hasCustomConstraint = false;
        NSMutableSet *policyConstraints = [NSMutableSet set];
        for (NSDictionary *setting in usageConstraints) {
            if (setting[(__bridge NSString*)kSecTrustSettingsPolicy] != nil) {
                hasCustomConstraint = true;
                [policyConstraints addObject:setting[(__bridge NSString*)kSecTrustSettingsPolicy]];
            } else {
                hasSystemConstraint = true;
            }
        }

        // Verify usage constraints we found against the expected constraints
        NSArray *expectedTypes = testCase[@"keyTypes"];
        if ([expectedTypes containsObject:@"system"]) {
            XCTAssert(hasSystemConstraint);
            NSSet *expectedPolicies = [NSSet setWithArray:testCase[@"oids"]];
            if (expectedPolicies) {
                XCTAssertEqualObjects(policyConstraints, expectedPolicies);
            }
        }
        if ([expectedTypes containsObject:@"custom"]) {
            XCTAssert(hasCustomConstraint);
            NSSet *expectedPolicies = [NSSet setWithArray:testCase[@"oids"]];
            XCTAssertEqualObjects(policyConstraints, expectedPolicies);
        }
    }
}

static void IsExpectedParentCertificate(void *context, CFArrayRef parents) {
    XCTAssertNotEqual(NULL, parents);
    if (parents) {
        XCTAssertGreaterThan(CFArrayGetCount(parents), 0);
        XCTAssertLessThanOrEqual(CFArrayGetCount(parents), 2);
        SecCertificateRef expectedCert = (SecCertificateRef)context;
        XCTAssert(CFArrayContainsValue(parents, CFRangeMake(0, CFArrayGetCount(parents)), expectedCert));
    }
}


- (void)testAppleAnchorSourceCopyParents {
    /* Hardcoded Apple Anchors
     * We should be able to find the "parents" of the built-in Apple roots and those
     * "parents" should match the roots themselves.
     */
    NSArray *anchors = (__bridge NSArray*)SecGetAppleTrustAnchors(true);
    for (id anchor in anchors) {
        SecCertificateRef cert = (__bridge SecCertificateRef)anchor;
        SecCertificateSourceCopyParents(kSecAppleAnchorSource, cert, cert, IsExpectedParentCertificate);
    }

    /* Go through some actual subCAs */
    SecCertificateRef legacyAnchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"AppleRootCA"
                                                                                           subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacyTestAnchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"TestAppleRootCA"
                                                                                               subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacyAnchorG3 = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"AppleRootG3"
                                                                                           subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacyTestAnchorG3 = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"TestAppleRootCAG3"
                                                                                               subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacySubCA = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"AppleSystemIntegration2CA"
                                                                                          subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacyG3SubCA = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"AppleSystemIntegrationCAG3"
                                                                                            subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacyTestSubCA = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"TestAppleSystemIntegration2CA"
                                                                                              subdirectory:@"si-20-sectrust-policies-data"];
    SecCertificateRef legacyTestG3SubCA = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"TestAppleSystemIntegrationCAG3"
                                                                                                subdirectory:@"si-20-sectrust-policies-data"];

    SecCertificateSourceCopyParents(kSecAppleAnchorSource, legacySubCA, legacyAnchor, IsExpectedParentCertificate);
    SecCertificateSourceCopyParents(kSecAppleAnchorSource, legacyG3SubCA, legacyAnchorG3, IsExpectedParentCertificate);
    SecCertificateSourceCopyParents(kSecAppleAnchorSource, legacyTestSubCA, legacyTestAnchor, IsExpectedParentCertificate);
    SecCertificateSourceCopyParents(kSecAppleAnchorSource, legacyTestG3SubCA, legacyTestAnchorG3, IsExpectedParentCertificate);

    CFReleaseNull(legacyAnchor);
    CFReleaseNull(legacyTestAnchor);
    CFReleaseNull(legacyAnchorG3);
    CFReleaseNull(legacyTestAnchorG3);
    CFReleaseNull(legacySubCA);
    CFReleaseNull(legacyG3SubCA);
    CFReleaseNull(legacyTestSubCA);
    CFReleaseNull(legacyTestG3SubCA);
}

- (void)testAppleAnchorSourceUsageConstraints {
    /* Hardcoded Apple Anchors */
    NSArray *anchors = (__bridge NSArray*)SecGetAppleTrustAnchors(true);
    for (id anchor in anchors) {
        SecCertificateRef cert = (__bridge SecCertificateRef)anchor;
        NSArray *constraints = CFBridgingRelease(SecCertificateSourceCopyUsageConstraints(kSecAppleAnchorSource, cert));
        XCTAssertNotNil(constraints);
        XCTAssertEqual(constraints.count, 0);
    }

    /* Non-Apple Anchor */
    SecCertificateRef nonAppleAnchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"DigiCertGlobalRootG3"
                                                                                            subdirectory:@"si-20-sectrust-policies-data"];
    NSArray *constraints = CFBridgingRelease(SecCertificateSourceCopyUsageConstraints(kSecAppleAnchorSource, nonAppleAnchor));
    XCTAssertNil(constraints);
    CFReleaseNull(nonAppleAnchor);

#if !TARGET_OS_BRIDGE
    /* OTA Apple Anchors */
    SecCertificateRef g1Anchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"ApplePlatformTLSECCRoot-G1"
                                                                                       subdirectory:@"si-20-sectrust-policies-data"];
    constraints = CFBridgingRelease(SecCertificateSourceCopyUsageConstraints(kSecAppleAnchorSource, g1Anchor));
    XCTAssertNotNil(constraints);
    XCTAssertGreaterThan(constraints.count, 1); // at least TLS Server Auth and Pinned SSL
    CFReleaseNull(g1Anchor);
#endif // !TARGET_OS_BRIDGE
}

- (void)testAppleAnchorSourceContains {
    SecPVCRef pvc = (SecPVCRef)malloc(sizeof(struct OpaqueSecPVC));
    SecPolicyRef policy = SecPolicyCreateSSL(true, NULL);
    NSArray *policies = @[(__bridge id)policy];
    SecPVCInit(pvc, NULL, (__bridge CFArrayRef)policies);
    NSArray *anchors = (__bridge NSArray*)SecGetAppleTrustAnchors(true);
    for (id anchor in anchors) {
        SecCertificateRef cert = (__bridge SecCertificateRef)anchor;
        XCTAssert(SecCertificateSourceContains(kSecAppleAnchorSource, cert, NULL));
        XCTAssert(SecCertificateSourceContains(kSecAppleAnchorSource, cert, pvc));
    }

    /* Non-Apple Anchor */
    SecCertificateRef nonAppleAnchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"DigiCertGlobalRootG3"
                                                                                            subdirectory:@"si-20-sectrust-policies-data"];
    XCTAssertFalse(SecCertificateSourceContains(kSecAppleAnchorSource, nonAppleAnchor, NULL));
    XCTAssertFalse(SecCertificateSourceContains(kSecAppleAnchorSource, nonAppleAnchor, pvc));
    CFReleaseNull(nonAppleAnchor);

#if !TARGET_OS_BRIDGE
    /* OTA Apple Anchors */
    SecCertificateRef g1TLSAnchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"ApplePlatformTLSECCRoot-G1"
                                                                                       subdirectory:@"si-20-sectrust-policies-data"];
    XCTAssert(SecCertificateSourceContains(kSecAppleAnchorSource, g1TLSAnchor, NULL));
    XCTAssert(SecCertificateSourceContains(kSecAppleAnchorSource, g1TLSAnchor, pvc));
    CFReleaseNull(g1TLSAnchor);

    SecCertificateRef g1CSAnchor = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"ApplePlatformCodeSigningECCRoot-G1"
                                                                                       subdirectory:@"si-20-sectrust-policies-data"];
    XCTAssert(SecCertificateSourceContains(kSecAppleAnchorSource, g1CSAnchor, NULL));
    XCTAssertFalse(SecCertificateSourceContains(kSecAppleAnchorSource, g1CSAnchor, pvc));
    CFReleaseNull(g1CSAnchor);
#endif // !TARGET_OS_BRIDGE

    free(pvc);
    CFReleaseNull(policy);
}

@end
