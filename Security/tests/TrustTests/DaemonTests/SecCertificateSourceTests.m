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

#import "trust/trustd/SecCertificateSource.h"
#import "trust/trustd/OTATrustUtilities.h"

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

@end
