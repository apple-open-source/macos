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

- (SecCertificatePathVCRef)createReversePath {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _path_leaf_cert, sizeof(_path_leaf_cert));
    SecCertificateRef ca = SecCertificateCreateWithBytes(NULL, _path_ca_cert, sizeof(_path_ca_cert));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _path_root_cert, sizeof(_path_root_cert));
    SecCertificatePathVCRef path = SecCertificatePathVCCreate(NULL, root, NULL);
    SecCertificatePathVCRef path2 = SecCertificatePathVCCreate(path, ca, NULL);
    CFRelease(path);
    SecCertificatePathVCRef result = SecCertificatePathVCCreate(path2, leaf, NULL);
    CFRelease(path2);
    CFRelease(root);
    CFRelease(ca);
    CFRelease(leaf);
    return result;
}

- (void)testGetMaximumNotBefore {
    SecCertificatePathVCRef path = [self createPath];
    CFAbsoluteTime notBefore = SecCertificatePathVCGetMaximumNotBefore(path);
    XCTAssertEqualObjects([NSDate dateWithTimeIntervalSinceReferenceDate:notBefore],
                          [NSDate dateWithTimeIntervalSinceReferenceDate:626290914.0]); // November 5, 2020 at 9:41:54 AM PST
    CFRelease(path);

    // Reverse path should produce same result
    path = [self createReversePath];
    notBefore = SecCertificatePathVCGetMaximumNotBefore(path);
    XCTAssertEqualObjects([NSDate dateWithTimeIntervalSinceReferenceDate:notBefore],
                          [NSDate dateWithTimeIntervalSinceReferenceDate:626290914.0]); // November 5, 2020 at 9:41:54 AM PST
    CFRelease(path);
}

- (void)testGetMinimumNotAfter {
    SecCertificatePathVCRef path = [self createPath];
    CFAbsoluteTime notAfter = SecCertificatePathVCGetMinimumNotAfter(path);
    XCTAssertEqualObjects([NSDate dateWithTimeIntervalSinceReferenceDate:notAfter],
                          [NSDate dateWithTimeIntervalSinceReferenceDate:660418914.0]); // December 5, 2021 at 9:41:54 AM PST
    CFRelease(path);

    // Reverse path should produce same result
    path = [self createReversePath];
    notAfter = SecCertificatePathVCGetMinimumNotAfter(path);
    XCTAssertEqualObjects([NSDate dateWithTimeIntervalSinceReferenceDate:notAfter],
                          [NSDate dateWithTimeIntervalSinceReferenceDate:660418914.0]); // December 5, 2021 at 9:41:54 AM PST
    CFRelease(path);
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

// Latest this update is from the CA response
- (void)testLatestThisUpdate {
    SecCertificatePathVCRef path = [self createPath];
    [self createAndPopulateRVCs:path];

    CFAbsoluteTime ltu = SecCertificatePathVCGetLatestThisUpdate(path);
    XCTAssertEqualObjects([NSDate dateWithTimeIntervalSinceReferenceDate:ltu],
                          [NSDate dateWithTimeIntervalSinceReferenceDate:632103361.0]); // January 11, 2021 at 4:16:01 PM PST

    SecCertificatePathVCDeleteRVCs(path);
    CFRelease(path);
}

@end
