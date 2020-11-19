/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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
#import <XCTest/XCTest.h>
#include "OSX/utilities/SecCFWrappers.h"
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicy.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustSettings.h>

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "VerifyDateTests_data.h"

@interface VerifyDateTests : TrustEvaluationTestCase
@end

@implementation VerifyDateTests
/* Test long-lived cert chain that expires in 9999 */

static SecTrustRef trust = nil;

+ (void)setUp {
    [super setUp];
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, longleaf, sizeof(longleaf));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, longroot, sizeof(longroot));
    NSArray *anchors = @[(__bridge id)root];

    SecTrustCreateWithCertificates(leaf, NULL, &trust);
    SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors);
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

+ (void)tearDown {
    CFReleaseNull(trust);
}

- (void)testPriorToNotBefore {
    CFDateRef date = NULL;
    /* September 4, 2013 (prior to "notBefore" date of 2 April 2014, should fail) */
    isnt(date = CFDateCreate(NULL, 400000000), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 23 Sep 2013");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust on 23 Sep 2013 and expect failure");
    CFReleaseNull(date);
}

- (void)testRecentWithinValidity {
    CFDateRef date = NULL;
    /* January 17, 2016 (recent date within validity period, should succeed) */
    isnt(date = CFDateCreate(NULL, 474747474), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 17 Jan 2016");
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust on 17 Jan 2016 and expect success");
    CFReleaseNull(date);
}

- (void)testFarFutureWithinValidity {
    CFDateRef date = NULL;
    /* December 20, 9999 (far-future date within validity period, should succeed) */
    isnt(date = CFDateCreate(NULL, 252423000000), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 20 Dec 9999");
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust on 20 Dec 9999 and expect success");
    CFReleaseNull(date);
}

- (void)testAfterNotAfter {
    CFDateRef date = NULL;
    /* January 12, 10000 (after the "notAfter" date of 31 Dec 9999, should fail) */
    isnt(date = CFDateCreate(NULL, 252425000000), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 12 Jan 10000");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust on 12 Jan 10000 and expect failure");
    CFReleaseNull(date);
}

@end

@interface ValidityPeriodRestrictionTests : TrustEvaluationTestCase
@end

@implementation ValidityPeriodRestrictionTests
// Note that the dates described in the test names are the issuance date not the VerifyDate

- (BOOL)runTrustEvaluation:(NSArray *)certs anchors:(NSArray *)anchors verifyTime:(NSTimeInterval)time error:(NSError **)error
{
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:time];
    SecTrustRef trustRef = NULL;
    BOOL result = NO;
    CFErrorRef cferror = NULL;

    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trustRef), errOut);
    require_noerr(SecTrustSetVerifyDate(trustRef, (__bridge CFDateRef)date), errOut);

    if (anchors) {
        require_noerr(SecTrustSetAnchorCertificates(trustRef, (__bridge CFArrayRef)anchors), errOut);
    }

    result = SecTrustEvaluateWithError(trustRef, &cferror);
    if (error && cferror) {
        *error = (__bridge NSError*)cferror;
    }

errOut:
    CFReleaseNull(policy);
    CFReleaseNull(trustRef);
    CFReleaseNull(cferror);
    return result;
}

- (BOOL)runTrustEvaluation:(NSArray *)certs anchors:(NSArray *)anchors error:(NSError **)error
{
    return [self runTrustEvaluation:certs anchors:anchors verifyTime:590000000.0 error:error]; // September 12, 2019 at 9:53:20 AM PDT
}

- (void)testSystemTrust_MoreThan5Years
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_66Months, sizeof(_testLeaf_66Months));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                   "system-trusted 66 month cert succeeded");

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_LessThan5Years_BeforeJul2016
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_5Years, sizeof(_testLeaf_5Years));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                  "system-trusted 5 year cert issued before 1 July 2016 failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_MoreThan39Months_AfterJul2016
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_4Years, sizeof(_testLeaf_4Years));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                   "system-trusted 4 year cert issued after 1 July 2016 succeeded");

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_LessThan39Months_BeforeMar2018
{
    // This cert should be valid
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_39Months, sizeof(_testLeaf_39Months));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                  "system-trusted 39 month cert issued before 1 Mar 2018 failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_MoreThan825Days_AfterMar2018
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                   "system-trusted 3 year cert issued after 1 Mar 2018 succeeded");

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_LessThan825Days_AfterMar2018
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_825Days, sizeof(_testLeaf_825Days));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                  "system-trusted 825 day cert issued after 1 Mar 2018 failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_MoreThan398Days_AfterSep2020
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_2Years, sizeof(_testLeaf_2Years));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf]
                                    anchors:@[(__bridge id)root]
                                 verifyTime:621000000.0 // September 5, 2020 at 5:00:00 AM PDT
                                      error:&error],
                  "system-trusted 2 year cert issued after 1 Sept 2020 failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_398Days_AfterSep2020
{
    [self setTestRootAsSystem:_testValidityPeriodsRootHash];
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_398Days, sizeof(_testLeaf_398Days));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf]
                                   anchors:@[(__bridge id)root]
                                verifyTime:621000000.0 // September 5, 2020 at 5:00:00 AM PDT
                                     error:&error],
                  "system-trusted 398 day cert issued after 1 Sept 2020 failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testAppTrustRoot_MoreThan825Days_AfterJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                   "app-trusted (root) 3 year cert issued after 1 Jul 2019 succeeded");

    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testAppTrustRoot_MoreThan825Days_BeforeJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_66Months, sizeof(_testLeaf_66Months));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                  "app-trusted (root) 66 month cert issued before 1 Jul 2019 failed: %@", error);

    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testAppTrustRoot_LessThan825Days_AfterJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_825Days, sizeof(_testLeaf_825Days));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                  "app-trusted (root) 825 day cert issued after 1 Jul 2019 failed: %@", error);

    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testAppTrustLeaf_MoreThan825Days_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)leaf] error:&error],
                   "app-trusted 3 year cert issued after 1 Jul 2019 succeeded");

    CFReleaseNull(leaf);
}

- (void)testAppTrustLeaf_MoreThan825Days_BeforeJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_66Months, sizeof(_testLeaf_66Months));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)leaf] error:&error],
                  "app-trusted 66 month cert issued before 1 Jul 2019 failed: %@", error);

    CFReleaseNull(leaf);
}

- (void)testAppTrustLeaf_LessThan825Days_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_825Days, sizeof(_testLeaf_825Days));

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)leaf] error:&error],
                  "app-trusted 825 day cert issued after 1 Jul 2019 failed: %@", error);

    CFReleaseNull(leaf);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
- (void)testUserTrustRoot_MoreThan825Days_AfterJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));
    id persistentRef = [self addTrustSettingsForCert:root];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:certs anchors:nil error:&error],
                  "user-trusted (root) 3 year cert issued after 1 Jul 2019 succeeded");

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testUserTrustRoot_MoreThan825Days_BeforeJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_66Months, sizeof(_testLeaf_66Months));
    id persistentRef = [self addTrustSettingsForCert:root];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:nil error:&error],
                  "user-trusted (root) 66 month cert issued before 1 Jul 2019 failed: %@", error);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testUserTrustRoot_LessThan825Days_AfterJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_825Days, sizeof(_testLeaf_825Days));
    id persistentRef = [self addTrustSettingsForCert:root];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:nil error:&error],
                  "app-trusted (root) 825 day cert issued after 1 Jul 2019 failed: %@", error);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

- (void)testUserTrustLeaf_MoreThan825Days_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));
    id persistentRef = [self addTrustSettingsForCert:leaf];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error],
                  "user-trusted leaf 3 year cert issued after 1 Jul 2019 failed: %@", error);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}

- (void)testUserTrustLeaf_MoreThan825Days_BeforeJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_66Months, sizeof(_testLeaf_66Months));
    id persistentRef = [self addTrustSettingsForCert:leaf];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error],
                  "user-trusted leaf 66 month cert issued before 1 Jul 2019 failed: %@", error);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}

- (void)testUserTrustLeaf_LessThan825Days_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_825Days, sizeof(_testLeaf_825Days));
    id persistentRef = [self addTrustSettingsForCert:leaf];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error],
                  "user-trusted leaf 825 day cert issued after 1 Jul 2019 failed: %@", error);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}

- (void)testUserDistrustLeaf_MoreThan825Days_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));
    id persistentRef = [self addTrustSettingsForCert:leaf trustSettings: @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny)}];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error],
                  "user-denied leaf 3 year cert issued after 1 Jul 2019 suceeded");

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}

- (void)testUserUnspecifiedLeaf_MoreThan825Days_AfterJul2019
{
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _testValidityPeriodsRoot, sizeof(_testValidityPeriodsRoot));
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _testLeaf_3Years, sizeof(_testLeaf_3Years));
    id persistentRef = [self addTrustSettingsForCert:leaf trustSettings: @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified)}];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)root] error:&error],
                  "user-unspecified trust leaf 3 year cert issued after 1 Jul 2019 succeeded");

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}
#endif // !TARGET_OS_BRIDGE

@end
