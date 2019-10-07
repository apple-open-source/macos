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
#include <Security/SecCertificatePriv.h>
#include <utilities/SecCFRelease.h>
#include "../TestMacroConversions.h"

#include "TrustEvaluationTestCase.h"
#include "PathParseTests_data.h"

const NSString *kSecTestPathFailureResources = @"si-18-certificate-parse/PathFailureCerts";

@interface PathParseTests : TrustEvaluationTestCase

@end

@implementation PathParseTests

- (void)testPathParseFailure {
    NSArray <NSURL *>* certURLs = nil;
    SecCertificateRef root = nil;
    
    NSURL *rootURL = [[NSBundle bundleForClass:[self class]]URLForResource:@"root" withExtension:@".cer" subdirectory:@"si-18-certificate-parse"];
    XCTAssert(root = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)[NSData dataWithContentsOfURL:rootURL]), "Unable to create root cert");
    certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestPathFailureResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test failure certs in bundle.");
    
    if (root && [certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            SecTrustRef trust = NULL;
            SecPolicyRef policy = SecPolicyCreateBasicX509();
            
            require_noerr_action(SecTrustCreateWithCertificates(cert, policy, &trust), blockOut,
                                 fail("Unable to create trust with certificate: %@", url));
            require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)[NSArray arrayWithObject:(__bridge id)root]),
                                 blockOut, fail("Unable to set trust in root cert: %@", url));
            require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)[NSDate dateWithTimeIntervalSinceReferenceDate:507200000.0]),
                                 blockOut, fail("Unable to set verify date: %@", url));
            XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "Got wrong trust result for %@", url);
            
            require_action(cert, blockOut,
                           fail("Failed to parse cert with SPKI error: %@", url));
            
        blockOut:
            CFReleaseNull(cert);
            CFReleaseNull(trust);
            CFReleaseNull(policy);
        }];
    }
}

- (void)testUnparseableExtensions {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _bad_extension_leaf, sizeof(_bad_extension_leaf));
    SecCertificateRef root = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    CFErrorRef error = NULL;

    NSURL *rootURL = [[NSBundle bundleForClass:[self class]]URLForResource:@"root" withExtension:@".cer" subdirectory:@"si-18-certificate-parse"];
    XCTAssert(root = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)[NSData dataWithContentsOfURL:rootURL]), "Unable to create root cert");
    NSArray *anchors = @[(__bridge id)root];

    require_noerr_action(SecTrustCreateWithCertificates(leaf, policy, &trust), errOut,
                         fail("Unable to create trust with certificate with unparseable extension"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors),
                         errOut, fail("Unable to set trust anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)[NSDate dateWithTimeIntervalSinceReferenceDate:507200000.0]),
                         errOut, fail("Unable to set verify date"));
    XCTAssertFalse(SecTrustEvaluateWithError(trust, &error), "Got wrong trust result cert");
    XCTAssert(error != NULL);
    XCTAssert(CFErrorGetCode(error) == errSecUnknownCertExtension);

errOut:
    CFReleaseNull(leaf);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}

@end
