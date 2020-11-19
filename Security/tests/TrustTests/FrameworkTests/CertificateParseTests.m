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
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>
#include <Security/SecFramework.h>
#include <utilities/SecCFRelease.h>
#include "../TestMacroConversions.h"

#include "TrustFrameworkTestCase.h"

const NSString *kSecTestParseFailureResources = @"si-18-certificate-parse/ParseFailureCerts";
const NSString *kSecTestParseSuccessResources = @"si-18-certificate-parse/ParseSuccessCerts";
const NSString *kSecTestKeyFailureResources = @"si-18-certificate-parse/KeyFailureCerts";
const NSString *kSecTestTODOFailureResources = @"si-18-certificate-parse/TODOFailureCerts";
const NSString *kSecTestExtensionFailureResources = @"si-18-certificate-parse/ExtensionFailureCerts";
const NSString *kSecTestNameFailureResources = @"si-18-certificate-parse/NameFailureCerts";

@interface CertificateParseTests : TrustFrameworkTestCase

@end

@implementation CertificateParseTests

- (void)testParseFailure {
    /* A bunch of certificates with different parsing errors */
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestParseFailureResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test failure certs in bundle.");
    
    if ([certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            is(cert, NULL, "Successfully parsed bad cert: %@", url);
            CFReleaseNull(cert);
        }];
    }
}

- (void)testParseSuccess {
    /* A bunch of certificates with different parsing variations */
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestParseSuccessResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test success certs in bundle.");
    
    if ([certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            isnt(cert, NULL, "Failed to parse good cert: %@", url);
            is(SecCertificateGetUnparseableKnownExtension(cert), kCFNotFound, "Found bad extension in good certs: %@", url);
            CFReleaseNull(cert);
        }];
    }
}

- (void)testKeyFailure {
    /* Parse failures that require public key extraction */
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestKeyFailureResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test key failure certs in bundle.");
    
    if ([certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            SecKeyRef pubkey = NULL;
            require_action(cert, blockOut,
                           fail("Failed to parse cert with SPKI error: %@", url));
            pubkey = SecCertificateCopyKey(cert);
            is(pubkey, NULL, "Successfully parsed bad SPKI: %@", url);
            
        blockOut:
            CFReleaseNull(cert);
            CFReleaseNull(pubkey);
        }];
    }
}

- (void)testTODOFailures {
    /* A bunch of certificates with different parsing errors that currently succeed. */
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestTODOFailureResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test TODO failure certs in bundle.");
    
    if ([certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            isnt(cert, NULL, "Successfully parsed bad TODO cert: %@", url);
            CFReleaseNull(cert);
        }];
    }
}

- (void)testUnparseableExtensions {
    /* A bunch of certificates with different parsing errors in known (but non-critical) extensions */
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestExtensionFailureResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test extension failure certs in bundle.");

    if ([certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            isnt(cert, NULL, "Failed to parse bad cert with unparseable extension: %@", url);
            isnt(SecCertificateGetUnparseableKnownExtension(cert), kCFNotFound, "Unable to find unparseable extension: %@", url);
            CFReleaseNull(cert);
        }];
    }
}

- (void)testUnparseableSubjectName {
    /* A bunch of certificates with different parsing errors the subject name */
    NSArray <NSURL *>* certURLs = [[NSBundle bundleForClass:[self class]]URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestNameFailureResources];
    XCTAssertTrue([certURLs count] > 0, "Unable to find parse test name failure certs in bundle.");

    if ([certURLs count] > 0) {
        [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
            NSData *certData = [NSData dataWithContentsOfURL:url];
            SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
            isnt(cert, NULL, "Failed to parse bad cert with unparseable name: %@", url);
            is(CFBridgingRelease(SecCertificateCopyCountry(cert)), nil, "Success parsing name for failure cert: %@", url);
            CFReleaseNull(cert);
        }];
    }
}

@end
