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
#import <Foundation/Foundation.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <utilities/SecCFWrappers.h>

#import "TrustEvaluationTestCase.h"

@interface NISTTests : TrustEvaluationTestCase
@end

@implementation NISTTests

- (void)testPKITSCerts {
    SecPolicyRef basicPolicy = SecPolicyCreateBasicX509();
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));

    /* Run the tests. */
    [self runCertificateTestForDirectory:basicPolicy subDirectory:@"nist-certs" verifyDate:testDate];

    CFReleaseSafe(basicPolicy);
}

- (void)testNoBasicConstraintsAnchor_UserTrusted {
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"InvalidMissingbasicConstraintsTest1EE"
                                                                                   subdirectory:@"nist-certs"];
    SecCertificateRef ca = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"MissingbasicConstraintsCACert"
                                                                                 subdirectory:@"nist-certs"];
    SecTrustRef trust = NULL;
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)ca];

    XCTAssertEqual(errSecSuccess, SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, NULL, &trust));
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));
    XCTAssertEqual(errSecSuccess, SecTrustSetVerifyDate(trust, (__bridge CFDateRef)testDate));

    id persistentRef = [self addTrustSettingsForCert:ca];
    CFErrorRef error = nil;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, &error));
    XCTAssertNotEqual(error, NULL);
    if (error) {
        XCTAssertEqual(CFErrorGetCode(error), errSecNoBasicConstraints);
    }

    [self removeTrustSettingsForCert:ca persistentRef:persistentRef];
    CFReleaseNull(leaf);
    CFReleaseNull(ca);
    CFReleaseNull(error);
}

- (void)testNoBasicConstraintsAnchor_AppTrusted {
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"InvalidMissingbasicConstraintsTest1EE"
                                                                                   subdirectory:@"nist-certs"];
    SecCertificateRef ca = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"MissingbasicConstraintsCACert"
                                                                                 subdirectory:@"nist-certs"];
    SecTrustRef trust = NULL;
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)ca];
    NSArray *anchor = @[(__bridge id)ca];

    XCTAssertEqual(errSecSuccess, SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, NULL, &trust));
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));
    XCTAssertEqual(errSecSuccess, SecTrustSetVerifyDate(trust, (__bridge CFDateRef)testDate));
    XCTAssertEqual(errSecSuccess, SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchor));

    CFErrorRef error = nil;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, &error));
    XCTAssertNotEqual(error, NULL);
    if (error) {
        XCTAssertEqual(CFErrorGetCode(error), errSecNoBasicConstraints);
    }

    CFReleaseNull(leaf);
    CFReleaseNull(ca);
    CFReleaseNull(error);
}

- (void)testNotCABasicConstraintsAnchor_UserTrusted {
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"InvalidcAFalseTest2EE"
                                                                                   subdirectory:@"nist-certs"];
    SecCertificateRef ca = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"basicConstraintsCriticalcAFalseCACert"
                                                                                 subdirectory:@"nist-certs"];
    SecTrustRef trust = NULL;
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)ca];

    XCTAssertEqual(errSecSuccess, SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, NULL, &trust));
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));
    XCTAssertEqual(errSecSuccess, SecTrustSetVerifyDate(trust, (__bridge CFDateRef)testDate));

    id persistentRef = [self addTrustSettingsForCert:ca];
    CFErrorRef error = nil;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, &error));
    XCTAssertNotEqual(error, NULL);
    if (error) {
        XCTAssertEqual(CFErrorGetCode(error), errSecNoBasicConstraintsCA);
    }

    [self removeTrustSettingsForCert:ca persistentRef:persistentRef];
    CFReleaseNull(leaf);
    CFReleaseNull(ca);
    CFReleaseNull(error);
}

- (void)testNotCABasicConstraintsAnchor_AppTrusted {
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"InvalidcAFalseTest2EE"
                                                                                   subdirectory:@"nist-certs"];
    SecCertificateRef ca = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"basicConstraintsCriticalcAFalseCACert"
                                                                                 subdirectory:@"nist-certs"];
    SecTrustRef trust = NULL;
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)ca];
    NSArray *anchor = @[(__bridge id)ca];

    XCTAssertEqual(errSecSuccess, SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, NULL, &trust));
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));
    XCTAssertEqual(errSecSuccess, SecTrustSetVerifyDate(trust, (__bridge CFDateRef)testDate));
    XCTAssertEqual(errSecSuccess, SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchor));

    CFErrorRef error = nil;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, &error));
    XCTAssertNotEqual(error, NULL);
    if (error) {
        XCTAssertEqual(CFErrorGetCode(error), errSecNoBasicConstraintsCA);
    }

    CFReleaseNull(leaf);
    CFReleaseNull(ca);
    CFReleaseNull(error);
}

@end
