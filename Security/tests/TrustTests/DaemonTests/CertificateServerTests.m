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
#import "trust/trustd/SecCertificateServer.h"
#import "trust/trustd/SecRevocationServer.h"

#import "TrustDaemonTestCase.h"
#import "CertificateServerTests_data.h"

@interface CertificateServerTests : TrustDaemonTestCase
@end

@implementation CertificateServerTests

- (SecCertificatePathVCRef)createPath {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _path_leaf_cert, sizeof(_path_leaf_cert));
    SecCertificateRef ca = SecCertificateCreateWithBytes(NULL, _path_ca_cert, sizeof(_path_ca_cert));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _path_root_cert, sizeof(_path_root_cert));
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

- (void)testCopyNotBefores {
    SecCertificatePathVCRef path = [self createPath];
    NSArray *notBefores = CFBridgingRelease(SecCertificatePathVCCopyNotBefores(path));
    XCTAssertNotNil(notBefores);
    XCTAssertEqual(notBefores.count, 3);
    XCTAssertEqualWithAccuracy([(NSDate *)notBefores[0] timeIntervalSinceReferenceDate],
                               SecCertificateNotValidBefore(SecCertificatePathVCGetCertificateAtIndex(path, 0)), 0.1);
    XCTAssertEqualWithAccuracy([(NSDate *)notBefores[1] timeIntervalSinceReferenceDate],
                               SecCertificateNotValidBefore(SecCertificatePathVCGetCertificateAtIndex(path, 1)), 0.1);
    XCTAssertEqualWithAccuracy([(NSDate *)notBefores[2] timeIntervalSinceReferenceDate],
                               SecCertificateNotValidBefore(SecCertificatePathVCGetCertificateAtIndex(path, 2)), 0.1);
}

- (void)testCopyNotAfters {
    SecCertificatePathVCRef path = [self createPath];
    NSArray *notAfters = CFBridgingRelease(SecCertificatePathVCCopyNotAfters(path));
    XCTAssertNotNil(notAfters);
    XCTAssertEqual(notAfters.count, 3);
    XCTAssertEqualWithAccuracy([(NSDate *)notAfters[0] timeIntervalSinceReferenceDate],
                               SecCertificateNotValidAfter(SecCertificatePathVCGetCertificateAtIndex(path, 0)), 0.1);
    XCTAssertEqualWithAccuracy([(NSDate *)notAfters[1] timeIntervalSinceReferenceDate],
                               SecCertificateNotValidAfter(SecCertificatePathVCGetCertificateAtIndex(path, 1)), 0.1);
    XCTAssertEqualWithAccuracy([(NSDate *)notAfters[2] timeIntervalSinceReferenceDate],
                               SecCertificateNotValidAfter(SecCertificatePathVCGetCertificateAtIndex(path, 2)), 0.1);
}

/* Mock OCSP behaviors in the validation context */
- (void)createAndPopulateRVCs:(SecCertificatePathVCRef)path
{
    SecCertificatePathVCAllocateRVCs(path, 2);
    SecOCSPResponseRef response_leaf = SecOCSPResponseCreate((__bridge CFDataRef)[NSData dataWithBytes:_ocsp_response_leaf
                                                                                                length:sizeof(_ocsp_response_leaf)]);
    SecOCSPResponseRef response_ca = SecOCSPResponseCreate((__bridge CFDataRef)[NSData dataWithBytes:_ocsp_response_ca
                                                                                              length:sizeof(_ocsp_response_ca)]);

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

// Earliest next update is from the leaf response
- (void)testEarliestNextUpdate {
    SecCertificatePathVCRef path = [self createPath];
    [self createAndPopulateRVCs:path];

    CFAbsoluteTime enu = SecCertificatePathVCGetEarliestNextUpdate(path);
    XCTAssertEqualObjects([NSDate dateWithTimeIntervalSinceReferenceDate:enu],
                          [NSDate dateWithTimeIntervalSinceReferenceDate:632142381.0]); // January 12, 2021 at 3:06:21 AM PST

    SecCertificatePathVCDeleteRVCs(path);

    CFRelease(path);
}

- (void)testCopyNextUpdates {
    SecCertificatePathVCRef path = [self createPath];
    [self createAndPopulateRVCs:path];

    NSArray *nextUpdates = CFBridgingRelease(SecCertificatePathVCCopyNextUpdates(path));
    XCTAssertNotNil(nextUpdates);
    XCTAssertEqual(nextUpdates.count, 2);
    XCTAssertEqualWithAccuracy([(NSDate *)nextUpdates[0] timeIntervalSinceReferenceDate], 632142381.0, 0.1); // Jan 12 11:06:21 2021 GMT
    XCTAssertEqualWithAccuracy([(NSDate *)nextUpdates[1] timeIntervalSinceReferenceDate], 632189761.0, 0.1); // Jan 13 00:16:01 2021 GMT
}

- (void)testCopyThisUpdates {
    SecCertificatePathVCRef path = [self createPath];
    [self createAndPopulateRVCs:path];

    NSArray *thisUpdates = CFBridgingRelease(SecCertificatePathVCCopyThisUpdates(path));
    XCTAssertNotNil(thisUpdates);
    XCTAssertEqual(thisUpdates.count, 2);
    XCTAssertEqualWithAccuracy([(NSDate *)thisUpdates[0] timeIntervalSinceReferenceDate], 632099181.0, 0.1); // Jan 11 23:06:21 2021 GMT
    XCTAssertEqualWithAccuracy([(NSDate *)thisUpdates[1] timeIntervalSinceReferenceDate], 632103361.0, 0.1); // Jan 12 00:16:01 2021 GMT
}

@end
