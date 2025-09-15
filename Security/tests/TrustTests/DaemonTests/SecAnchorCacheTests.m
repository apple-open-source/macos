//
//  SecAnchorCacheTests.m
//  Security
//
//

#import <Foundation/Foundation.h>
#include <AssertMacros.h>
#import <XCTest/XCTest.h>

#import <Security/SecPolicyPriv.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/SecAppleAnchorPriv.h>
#include <utilities/SecCFWrappers.h>

#import "trust/trustd/SecAnchorCache.h"
#import "trust/trustd/SecCertificateSource.h"
#import "trust/trustd/OTATrustUtilities.h"

#import "TrustDaemonTestCase.h"

@interface AnchorCacheTests : TrustDaemonTestCase
@property NSMutableDictionary *certs;
@property NSMutableDictionary *anchorTables;
@property NSArray *testCases;
@end

@implementation AnchorCacheTests

+ (void)setUp {
    [super setUp];
    SecAnchorCacheInitialize();
}

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

- (void)testCopyAnchors {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    /* Apple Anchors */
    NSArray *anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleMobileAsset));
    XCTAssertNotNil(anchors);
    // Check harcoded anchors are a subset of the returned anchors
    NSArray *appleAnchors = (__bridge NSArray *)SecGetAppleTrustAnchors(false);
    XCTAssertGreaterThanOrEqual(anchors.count, appleAnchors.count);
    for (id anchor in appleAnchors) {
        XCTAssert([anchors containsObject:anchor]);
    }

    /* Constrained Anchors */
    anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleVerifiedMark));
    XCTAssertNotNil(anchors);
    XCTAssertGreaterThan(anchors.count, 0);

    anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleMDLTerminalAuth));
    XCTAssertNotNil(anchors);
    XCTAssertGreaterThan(anchors.count, 0);

    /* System anchors */
    anchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleX509Basic));
    XCTAssertNotNil(anchors);
    XCTAssertGreaterThan(anchors.count, 1);

    NSArray *ocspAnchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleOCSPSigner));
    XCTAssertNotNil(ocspAnchors);
    XCTAssertEqualObjects(anchors, ocspAnchors);

    /* Prime anchor cache and then copy anchors again */
    CFStringRef sectigoServerAuthRootLookupKey = CFSTR("6FAEB525494DCEC35FC629C946482912999E2EC8"); // Sectigo Public Server Authentication Root R46
    NSArray *parents = CFBridgingRelease(SecAnchorCacheCopyParentCertificates(sectigoServerAuthRootLookupKey));
    XCTAssertNotNil(parents);
    NSArray *cachedAnchors = CFBridgingRelease(SecAnchorCacheCopyAnchors(kSecPolicyAppleX509Basic));
    XCTAssertNotNil(cachedAnchors);
    XCTAssertEqualObjects(anchors, cachedAnchors);
}

- (void)testAnchorRecordsPermittedByPolicy {
#if TARGET_OS_BRIDGE
    /* bridgeOS doesn't use trust store */
    XCTSkip();
#endif
    NSArray *testPolicyIds = @[
        /* System Policies */
        (__bridge NSString*)kSecPolicyAppleX509Basic,
        (__bridge NSString*)kSecPolicyAppleSSLServer,
        (__bridge NSString*)kSecPolicyAppleSSLClient,
        /* Custom Policies */
        (__bridge NSString*)kSecPolicyAppleQiSigning,
        (__bridge NSString*)kSecPolicyAppleVerifiedMark,
        (__bridge NSString*)kSecPolicyAppleQWAC,
        (__bridge NSString*)kSecPolicyApple3PMobileAsset,
        /* Apple Policies */
        (__bridge NSString*)kSecPolicyAppleMobileAsset,
        (__bridge NSString*)kSecPolicyAppleGenericApplePinned,
        (__bridge NSString*)kSecPolicyAppleSoftwareSigning,
    ];

    for (NSDictionary *testCase in self.testCases) {
        NSDictionary *anchorTable = self.anchorTables[testCase[@"plist"]];
        SecOTAPKISetConstrainedAnchorLookupTable((__bridge CFDictionaryRef)anchorTable);

        SecCertificateRef cert = (__bridge SecCertificateRef)self.certs[testCase[@"certHash"]];
        NSArray *records = CFBridgingRelease(CopyAnchorRecordsForCertificate(cert));
        NSNumber *expectedRecordCount = testCase[@"certRecordCount"];
        if (expectedRecordCount && expectedRecordCount.integerValue > 0) {
            XCTAssertNotNil(records);
            XCTAssertEqual(records.count, expectedRecordCount.unsignedIntegerValue);

            NSArray *expectedTypes = testCase[@"certTypes"];
            for (NSString *testPolicyId in testPolicyIds) {
                bool systemPolicy = !SecPolicyUsesConstrainedAnchors((__bridge CFStringRef)testPolicyId);
                bool applePolicy = SecPolicyUsesAppleAnchors((__bridge CFStringRef)testPolicyId);
                bool customPolicy = !applePolicy && SecPolicyUsesConstrainedAnchors((__bridge CFStringRef)testPolicyId);

                NSArray<NSDictionary*>*permittedRecords = [SecAnchorCache anchorRecordsPermitttedForPolicy:records policyId:testPolicyId];
                /* expected types matches the type of the policy */
                if ((customPolicy && [expectedTypes containsObject:@"custom"]) ||
                    (systemPolicy && [expectedTypes containsObject:@"system"]) ||
                    (applePolicy && [expectedTypes containsObject:@"platform"])) {

                    /* In most cases the expected key oids match the cert oids but one case
                     * exercises different custom oids for different certs. */
                    NSArray *expectedOids = testCase[@"certOids"];
                    if (!expectedOids) {
                        expectedOids = testCase[@"oids"];
                    }
                    NSArray *expectedSystemOids = testCase[@"systemOids"];

                    if (customPolicy && ![expectedOids containsObject:testPolicyId]) {
                        /* Custom record types must not return if policy is not in expected OIDs */
                        XCTAssertNil(permittedRecords);
                    } else if (systemPolicy && expectedSystemOids.count > 0 &&
                               ![expectedSystemOids containsObject:testPolicyId]) {
                        /* Constrained system anchors must not return if policy is not in expected OIDs */
                        XCTAssertNil(permittedRecords);
                    } else if (applePolicy && expectedOids.count > 0 &&
                               ![expectedOids containsObject:testPolicyId]) {
                        /* Constrained platform anchors must not return if policy is not in expected OIDs */
                        XCTAssertNil(permittedRecords);
                    } else {
                        XCTAssertNotNil(permittedRecords);
                    }

                    for (NSDictionary *record in permittedRecords) {
                        // All permitted records must be expected types
                        NSString *type = record[@"type"];
                        if (systemPolicy) { XCTAssertEqualObjects(@"system", type); }
                        if (customPolicy) { XCTAssertEqualObjects(@"custom", type); }
                        if (applePolicy) { XCTAssertEqualObjects(@"platform", type); }

                        NSArray *recordOids = record[@"oids"];
                        if (recordOids.count < 1) {
                            /* Records for custom policies must specify oids,
                             * but platform or system records don't need constraints defined */
                            XCTAssert(systemPolicy || applePolicy);
                        } else {
                            /* But if record specifies OID constraints this policy must be one
                             * and this policy must have been listed as expected */
                            XCTAssert([recordOids containsObject:testPolicyId]);
                            XCTAssert([expectedOids containsObject:testPolicyId]);
                        }
                    }
                } else {
                    // There shouldn't be any records matching the policy id
                    XCTAssertNil(permittedRecords);
                }
            }
        } else {
            XCTAssertNil(records);
        }
    }
}

@end
