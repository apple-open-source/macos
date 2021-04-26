/*
* Copyright (c) 2021 Apple Inc. All Rights Reserved.
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
#import <Security/SecCertificatePriv.h>
#import <Security/SecTrustPriv.h>
#import "trust/trustd/SecCertificateServer.h"
#import "trust/trustd/SecRevocationServer.h"

#import "TrustDaemonTestCase.h"
#import "TrustServerTests_data.h"

@interface TrustServerTests : TrustDaemonTestCase
@end

@implementation TrustServerTests

- (SecCertificatePathVCRef)createPath {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _trust_server_leaf_cert, sizeof(_trust_server_leaf_cert));
    SecCertificateRef ca = SecCertificateCreateWithBytes(NULL, _trust_server_ca_cert, sizeof(_trust_server_ca_cert));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _trust_server_root_cert, sizeof(_trust_server_root_cert));
    SecCertificatePathVCRef path = SecCertificatePathVCCreate(NULL, leaf, NULL);
    SecCertificatePathVCRef path2 = SecCertificatePathVCCreate(path, ca, NULL);
    CFRelease(path);
    SecCertificatePathVCRef result = SecCertificatePathVCCreate(path2, root, NULL);
    CFRelease(path2);
    CFRelease(root);
    CFRelease(ca);
    CFRelease(leaf);
    return result;
}

- (SecPathBuilderRef)createBuilderForPath:(SecCertificatePathVCRef)path {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _trust_server_leaf_cert, sizeof(_trust_server_leaf_cert));
    NSArray *certs = @[(__bridge id)leaf];
    CFRelease(leaf);
    SecPathBuilderRef builder = SecPathBuilderCreate(NULL, NULL, (__bridge CFArrayRef)certs, NULL, false, false, NULL, NULL, NULL, NULL, 0.0, NULL, NULL, NULL, NULL);
    // Set path
    SecPathBuilderSetPath(builder, path);

    // Set result
    SecPVCRef pvc = SecPathBuilderGetPVCAtIndex(builder, 0);
    pvc->result = kSecTrustResultProceed;

    // Set the best path for reporting
    SecPathBuilderDidValidatePath(builder);

    return builder;
}

/* Mock OCSP behaviors in the validation context */
- (void)createAndPopulateRVCs:(SecCertificatePathVCRef)path
{
    SecCertificatePathVCAllocateRVCs(path, 2);
    SecOCSPResponseRef response_leaf = SecOCSPResponseCreate((__bridge CFDataRef)[NSData dataWithBytes:_trust_server_ocsp_response_leaf
                                                                                                length:sizeof(_trust_server_ocsp_response_leaf)]);
    SecOCSPResponseRef response_ca = SecOCSPResponseCreate((__bridge CFDataRef)[NSData dataWithBytes:_trust_server_ocsp_response_ca
                                                                                              length:sizeof(_trust_server_ocsp_response_ca)]);

    // Set expire time on responses
    (void)SecOCSPResponseCalculateValidity(response_leaf, 0.0, 0.0, 632104000.0); // January 11, 2021 at 4:26:40 PM PST
    (void)SecOCSPResponseCalculateValidity(response_ca, 0.0, 0.0, 632104000.0);
    for (int i = 0; i < 2; i++) {
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, i);
        rvc->certIX = i;
        rvc->done = false;

        SecORVCRef orvc = NULL;
        orvc = malloc(sizeof(struct OpaqueSecORVC));
        memset(orvc, 0, sizeof(struct OpaqueSecORVC));
        orvc->rvc = rvc;
        orvc->certIX = i;

        rvc->orvc = orvc;

        if (i == 0) {
            orvc->thisUpdate = SecOCSPResponseProducedAt(response_leaf);
            orvc->nextUpdate = SecOCSPResponseGetExpirationTime(response_leaf);
        } else {
            orvc->thisUpdate = SecOCSPResponseProducedAt(response_ca);
            orvc->nextUpdate = SecOCSPResponseGetExpirationTime(response_ca);
        }
    }

    SecOCSPResponseFinalize(response_leaf);
    SecOCSPResponseFinalize(response_ca);
}

- (void)createAndPopulateRVCs:(SecCertificatePathVCRef)path notBefore:(CFAbsoluteTime)notBefore notAfter:(CFAbsoluteTime)notAfter
{
    NSLog(@"thisUpdate: %f, nextUpdate: %f", notBefore, notAfter);
    SecCertificatePathVCAllocateRVCs(path, 2);
    for (int i = 0; i < 2; i++) {
        SecRVCRef rvc = SecCertificatePathVCGetRVCAtIndex(path, i);
        rvc->certIX = i;
        rvc->done = false;

        SecORVCRef orvc = NULL;
        orvc = malloc(sizeof(struct OpaqueSecORVC));
        memset(orvc, 0, sizeof(struct OpaqueSecORVC));
        orvc->rvc = rvc;
        orvc->certIX = i;
        orvc->thisUpdate = notBefore;
        orvc->nextUpdate = notAfter;

        rvc->orvc = orvc;
    }
}

- (void)testReportResult_NoRevocationResults {
    SecCertificatePathVCRef path = [self createPath];
    SecPathBuilderRef builder = [self createBuilderForPath:path];
    XCTAssertFalse(SecPathBuilderReportResult(builder));
    NSDictionary *info = (__bridge NSDictionary*)SecPathBuilderGetInfo(builder);
    XCTAssertNotNil(info);

    // For a non-EV, non-CT, no-revocation builder, only the trust result validity should be set
    XCTAssertEqual(info.count, 2);

    /* NotBefore inputs: leaf cert notBefore at 626290914.0; now - leeway; no latest thisUpdate
     * Maximum of these will be now - leeway */
    NSTimeInterval notBefore = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotBefore] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notBefore, CFAbsoluteTimeGetCurrent() - TRUST_TIME_LEEWAY, 0.5); // to within 1/2 second accuracy

    /* NotAfter inputs: leaf cert notAfter at 660418914.0; now + leeway; no earliest nextUpdate
     * Minimum of these will be (now + leeway) until current time is (notAfter - leeway) where it will switch to the notAfter.
     * Then after the cert expires, it will change back to (now + leeway) as we correct for an inverted validity period. */
    NSTimeInterval notAfter = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotAfter] timeIntervalSinceReferenceDate];
    if (CFAbsoluteTimeGetCurrent() < (660418914.0 - TRUST_TIME_LEEWAY)) {
        // Well before leaf expiry
        XCTAssertEqualWithAccuracy(notAfter, CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY, 0.5);
    } else if (CFAbsoluteTimeGetCurrent() < 660418914.0 + TRUST_TIME_LEEWAY) {
        // Until the cert has expired, the notAfter date bounds the trust result
        XCTAssertEqualWithAccuracy(notAfter, 660418914.0, 0.1);
    } else {
        // Once the cert expires we just use the leeway again
        XCTAssertEqualWithAccuracy(notAfter, CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY, 0.5);
    }

    SecPathBuilderDestroy(builder);
    free(builder);
    CFRelease(path);
}

- (void)testReportResult_RevocationResults_InvertedWindows_Expired {
    SecCertificatePathVCRef path = [self createPath];
    [self createAndPopulateRVCs:path];
    SecPathBuilderRef builder = [self createBuilderForPath:path];
    XCTAssertFalse(SecPathBuilderReportResult(builder));
    NSDictionary *info = (__bridge NSDictionary*)SecPathBuilderGetInfo(builder);
    XCTAssertNotNil(info);

    // For a non-EV, non-CT builder with revocation checked, so we should have the trust result validity and revocation keys
    XCTAssertEqual(info.count, 6);

    /* NotBefore inputs: leaf cert notBefore at 626290914.0; now - 1:15; OCSP response latest thisUpdate at 632103361.0
     * Maximum of these will be now - 1:15 */
    NSTimeInterval notBefore = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotBefore] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notBefore, CFAbsoluteTimeGetCurrent() - TRUST_TIME_LEEWAY, 0.5); // to within 1/2 second accuracy

    /* NotAfter inputs: leaf cert notAfter at 660418914.0; now + 1:15; OCSP response earliest nextUpdate at  632142381.0
     * Minimum of these will be the earliest next update, but because the notBefore is after that, we'll adjust
     * the notAfter to now + 1:15 */
    NSTimeInterval notAfter = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotAfter] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notAfter, CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY, 0.5);

    XCTAssert([(NSNumber*)info[(__bridge NSString*)kSecTrustInfoRevocationKey] boolValue]);
    XCTAssert([(NSNumber*)info[(__bridge NSString*)kSecTrustRevocationChecked] boolValue]);

    XCTAssertEqualObjects((NSDate*)info[(__bridge NSString*)kSecTrustInfoRevocationValidUntilKey], [NSDate dateWithTimeIntervalSinceReferenceDate:632142381.0]);
    XCTAssertEqualObjects((NSDate*)info[(__bridge NSString*)kSecTrustRevocationValidUntilDate], [NSDate dateWithTimeIntervalSinceReferenceDate:632142381.0]);

    SecPathBuilderDestroy(builder);
    free(builder);
    CFRelease(path);
}

- (void)testReportResult_RevocationResults_InvertedWindows_FutureValidity {
    SecCertificatePathVCRef path = [self createPath];
    CFAbsoluteTime thisUpdate = CFAbsoluteTimeGetCurrent() + 400;
    CFAbsoluteTime nextUpdate = CFAbsoluteTimeGetCurrent() + 100;
    [self createAndPopulateRVCs:path
                      notBefore:thisUpdate
                       notAfter:nextUpdate];
    SecPathBuilderRef builder = [self createBuilderForPath:path];
    XCTAssertFalse(SecPathBuilderReportResult(builder));
    NSDictionary *info = (__bridge NSDictionary*)SecPathBuilderGetInfo(builder);
    XCTAssertNotNil(info);

    // For a non-EV, non-CT builder with revocation checked, so we should have the trust result validity and revocation keys
    XCTAssertEqual(info.count, 6);

    /* Because thisUpdate and nextUpdate are flipped and both in the future, we should keep the nextUpdate as notAfter
     * and use "now" as the notBefore */
    NSTimeInterval notBefore = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotBefore] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notBefore, CFAbsoluteTimeGetCurrent(), 0.5); // to within 1/2 second accuracy
    NSTimeInterval notAfter = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotAfter] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notAfter, nextUpdate, 0.1);

    SecPathBuilderDestroy(builder);
    free(builder);
    CFRelease(path);
}

- (void)testReportResult_RevocationResults_InvertedWindows_Between {
    SecCertificatePathVCRef path = [self createPath];
    CFAbsoluteTime thisUpdate = CFAbsoluteTimeGetCurrent() + 400;
    CFAbsoluteTime nextUpdate = CFAbsoluteTimeGetCurrent() - 100;
    [self createAndPopulateRVCs:path
                      notBefore:thisUpdate
                       notAfter:nextUpdate];
    SecPathBuilderRef builder = [self createBuilderForPath:path];
    XCTAssertFalse(SecPathBuilderReportResult(builder));
    NSDictionary *info = (__bridge NSDictionary*)SecPathBuilderGetInfo(builder);
    XCTAssertNotNil(info);

    // For a non-EV, non-CT builder with revocation checked, so we should have the trust result validity and revocation keys
    XCTAssertEqual(info.count, 6);

    /* Because thisUpdate and nextUpdate are flipped, with notBefore in the future but notAfter in the past,
     * we should flip the two */
    NSTimeInterval notBefore = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotBefore] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notBefore, nextUpdate, 0.1); // to within 1/2 second accuracy
    NSTimeInterval notAfter = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotAfter] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notAfter, thisUpdate, 0.1);

    SecPathBuilderDestroy(builder);
    free(builder);
    CFRelease(path);
}

- (void)testReportResult_RevocationResults_Expired {
    SecCertificatePathVCRef path = [self createPath];
    CFAbsoluteTime thisUpdate = CFAbsoluteTimeGetCurrent() - 400;
    CFAbsoluteTime nextUpdate = CFAbsoluteTimeGetCurrent() - 100;
    [self createAndPopulateRVCs:path
                      notBefore:thisUpdate
                       notAfter:nextUpdate];
    SecPathBuilderRef builder = [self createBuilderForPath:path];
    XCTAssertFalse(SecPathBuilderReportResult(builder));
    NSDictionary *info = (__bridge NSDictionary*)SecPathBuilderGetInfo(builder);
    XCTAssertNotNil(info);

    // For a non-EV, non-CT builder with revocation checked, so we should have the trust result validity and revocation keys
    XCTAssertEqual(info.count, 6);

    /* thisUpdate and nextUpdate are both in the recent past, so we should adjust the validity period to
     * be in the present */
    NSTimeInterval notBefore = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotBefore] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notBefore, nextUpdate, 0.1);
    NSTimeInterval notAfter = [(NSDate *)info[(__bridge NSString*)kSecTrustInfoResultNotAfter] timeIntervalSinceReferenceDate];
    XCTAssertEqualWithAccuracy(notAfter, CFAbsoluteTimeGetCurrent() + TRUST_TIME_LEEWAY, 0.5);

    SecPathBuilderDestroy(builder);
    free(builder);
    CFRelease(path);
}

@end
