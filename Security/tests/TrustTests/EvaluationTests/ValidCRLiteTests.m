/*
 *  Copyright (c) 2017-2019 Apple Inc. All Rights Reserved.
 *
 */

#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>

#include <Security/Security.h>
#include <Security/SecTrust.h>
#include <Security/SecPolicy.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>
#include "trust/trustd/OTATrustUtilities.h"
#include "featureflags/featureflags.h"
#import "trust/trustd/SecRevocationDb.h"

#import "../TestMacroConversions.h"
#import "TrustEvaluationTestCase.h"

enum {
    kBasicPolicy = 0,
    kSSLServerPolicy = 1,
};

@interface ValidCRLiteTests : TrustEvaluationTestCase
@property NSData *updateData;
@end

@implementation ValidCRLiteTests

- (void)setUp
{
    [super setUp];

    self.updateData = [NSData dataWithContentsOfURL:[[NSBundle bundleForClass:[self class]] URLForResource:@"v0-recent" withExtension:@"bin" subdirectory:@"CRLiteTests-data"]];
    
    /* Override the FF and turn on CRLite + enforcement */
    _SecTrustUseCRLiteSetOverride(true);
    _SecTrustUseCRLiteEnforcementSetOverride(true);

    /* Set the Valid generation to 8 */
    SecRevocationDbSetGeneration(8);
}

- (void)tearDown
{
    /* Reset the FF */
    _SecTrustUseCRLiteClearOverride();
    _SecTrustUseCRLiteEnforcementClearOverride();

    /* Reset the Valid generation */
    SecRevocationDbSetGeneration(kValidUpdateCurrentGeneration);
    
    /* Reset Valid DB */
    __block CFErrorRef error = NULL;
    SecRevocationDbFullReset(&error);
    XCTAssertNil((__bridge NSError*)error, "Should be no error resetting the Valid DB");

}

- (void) run_valid_trust_test:(SecCertificateRef)leaf
                           ca:(SecCertificateRef)ca
                        subca:(SecCertificateRef)subca
                      anchors:(CFArrayRef)anchors
                         date:(CFDateRef)date
                     policyID:(CFIndex)policyID
                     expected:(SecTrustResultType)expected
                    test_name:(const char *)test_name
{
    CFArrayRef policies=NULL;
    SecPolicyRef policy=NULL;
    SecTrustRef trust=NULL;
    SecTrustResultType trustResult;
    CFMutableArrayRef certs=NULL;

    printf("Starting %s\n", test_name);
    isnt(certs = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks), NULL, "create cert array");
    if (certs) {
        if (leaf) {
            CFArrayAppendValue(certs, leaf);
        }
        if (ca) {
            CFArrayAppendValue(certs, ca);
        }
        if (subca) {
            CFArrayAppendValue(certs, subca);
        }
    }

    if (policyID == kSSLServerPolicy) {
        isnt(policy = SecPolicyCreateSSL(true, NULL), NULL, "create ssl policy");
    } else {
        isnt(policy = SecPolicyCreateBasicX509(), NULL, "create basic policy");
    }
    isnt(policies = CFArrayCreate(kCFAllocatorDefault, (const void **)&policy, 1, &kCFTypeArrayCallBacks), NULL, "create policies");
    ok_status(SecTrustCreateWithCertificates(certs, policies, &trust), "create trust");

    assert(trust); // silence analyzer
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    ok(trustResult == expected, "trustResult %d expected (got %d) for %s",
       (int)expected, (int)trustResult, test_name);

    CFReleaseSafe(certs);
    CFReleaseSafe(policy);
    CFReleaseSafe(policies);
    CFReleaseSafe(trust);
}

- (SecCertificateRef) CF_RETURNS_RETAINED createCertFromResource:(NSString *)name
{
    id cert = [self SecCertificateCreateFromPEMResource:name subdirectory:@"si-88-sectrust-valid-data"];
    return (__bridge SecCertificateRef)cert;
}

- (void)test_crlite
{
#if TARGET_OS_BRIDGE
    /* Valid is not supported on bridgeOS */
    XCTSkip();
#endif
    
    /* Can't test this if we don't have CRLite support */
    if (!SecRevocationDbHasCRLiteSupport()) {
        XCTSkip();
        return;
    }

    /* Ingest the test CRLite update */
    XCTAssertNotNil(self.updateData);
    SecValidUpdateVerifyAndIngest((__bridge CFDataRef)self.updateData, CFSTR("https://valid-qa.apple.com/carry"), true);

    /* Run the tests */
    SecCertificateRef leaf_not_covered=NULL, leaf_not_revoked=NULL, leaf_revoked=NULL;
    SecCertificateRef issuer_not_covered=NULL, issuer_not_revoked=NULL, issuer_revoked=NULL;

    isnt(leaf_not_covered = [self createCertFromResource:@"crlite-cert-known-not-covered"], NULL, "create known not covered cert");
    isnt(leaf_not_revoked = [self createCertFromResource:@"crlite-cert-known-not-revoked"], NULL, "create known not revoked cert");
    isnt(leaf_revoked = [self createCertFromResource:@"crlite-cert-known-revoked"], NULL, "create known revoked cert");
    isnt(issuer_not_covered = [self createCertFromResource:@"crlite-issuer-known-not-covered"], NULL, "create known not covered issuer cert");
    isnt(issuer_not_revoked = [self createCertFromResource:@"crlite-issuer-known-not-revoked"], NULL, "create known not revoked issuer cert");
    isnt(issuer_revoked = [self createCertFromResource:@"crlite-issuer-known-revoked"], NULL, "create known revoked issuer cert");

    CFMutableArrayRef anchors=NULL;
    CFCalendarRef cal = NULL;
    CFAbsoluteTime at;
    CFDateRef date_20250909 = NULL; // a date when our test certs would all be valid, in the absence of Valid db info

    isnt(cal = CFCalendarCreateWithIdentifier(kCFAllocatorDefault, kCFGregorianCalendar), NULL, "create calendar");
    ok(CFCalendarComposeAbsoluteTime(cal, &at, "yMd", 2025, 9, 9), "create verify absolute time 20250909");
    isnt(date_20250909 = CFDateCreate(kCFAllocatorDefault, at), NULL, "create verify date date_20250909");

    /* Case 1: leaf_not_covered */
    /* -- OK: not covered by CRLite */
    [self run_valid_trust_test:leaf_not_covered ca:issuer_not_covered subca:NULL anchors:anchors date:date_20250909 policyID:kBasicPolicy expected:kSecTrustResultUnspecified test_name:"crlite-known-not-covered"];

    /* Case 2: leaf_revoked */
    /* -- BAD: covered by CRLite */
    [self run_valid_trust_test:leaf_revoked ca:issuer_revoked subca:NULL anchors:anchors date:date_20250909 policyID:kBasicPolicy expected:kSecTrustResultFatalTrustFailure test_name:"crlite-known-revoked"];

    /* Case 3: leaf_not_revoked */
    /* -- OK: cert is not revoked according to CRLite */
    [self run_valid_trust_test:leaf_not_revoked ca:issuer_not_revoked subca:NULL anchors:anchors date:date_20250909 policyID:kBasicPolicy expected:kSecTrustResultUnspecified test_name:"crlite-known-not-revoked"];

    CFReleaseSafe(anchors);
    CFReleaseSafe(cal);
    CFReleaseSafe(date_20250909);
    CFReleaseSafe(leaf_not_covered);
    CFReleaseSafe(leaf_not_revoked);
    CFReleaseSafe(leaf_revoked);
    CFReleaseSafe(issuer_not_covered);
    CFReleaseSafe(issuer_not_revoked);
    CFReleaseSafe(issuer_revoked);
}

@end
