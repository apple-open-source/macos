/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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
 */

#include "shared_regressions.h"

#include <AssertMacros.h>
#import <Foundation/Foundation.h>

#include <utilities/SecInternalReleasePriv.h>
#include <utilities/SecCFRelease.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecCertificateInternal.h>

const NSString *kSecTestParseFailureResources = @"si-18-certificate-parse/ParseFailureCerts";
const NSString *kSecTestParseSuccessResources = @"si-18-certificate-parse/ParseSuccessCerts";
const NSString *kSecTestKeyFailureResources = @"si-18-certificate-parse/KeyFailureCerts";
const NSString *kSecTestPathFailureResources = @"si-18-certificate-parse/PathFailureCerts";
const NSString *kSecTestTODOFailureResources = @"si-18-certificate-parse/TODOFailureCerts";

static uint64_t num_certs() {
    uint64_t num_certs = 0;
    NSArray <NSURL *>* certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestParseFailureResources];
    num_certs += [certURLs count];
    certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestParseSuccessResources];
    num_certs += [certURLs count];
    certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestKeyFailureResources];
    num_certs += [certURLs count];
    certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestPathFailureResources];
    num_certs += [certURLs count];
    certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestTODOFailureResources];
    num_certs += [certURLs count];

    return num_certs;
}

static void test_parse_failure(void) {
    /* A bunch of certificates with different parsing errors */
    NSArray <NSURL *>* certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestParseFailureResources];
    require_action([certURLs count] > 0, testOut,
                   fail("Unable to find parse test failure certs in bundle."));

    [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
        NSData *certData = [NSData dataWithContentsOfURL:url];
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
        is(cert, NULL, "Successfully parsed bad cert: %@", url);
        CFReleaseNull(cert);
    }];

testOut:
    return;
}

static void test_parse_success(void) {
    /* A bunch of certificates with different parsing variations */
    NSArray <NSURL *>* certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestParseSuccessResources];
    require_action([certURLs count] > 0, testOut,
                   fail("Unable to find parse test failure certs in bundle."));

    [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
        NSData *certData = [NSData dataWithContentsOfURL:url];
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
        isnt(cert, NULL, "Failed to parse good cert: %@", url);
        CFReleaseNull(cert);
    }];

testOut:
    return;
}

static void test_key_failure(void) {
    /* Parse failures that require public key extraction */
    NSArray <NSURL *>* certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestKeyFailureResources];
    require_action([certURLs count] > 0, testOut,
                   fail("Unable to find parse test failure certs in bundle."));

    [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
        NSData *certData = [NSData dataWithContentsOfURL:url];
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
        SecKeyRef pubkey = NULL;
        require_action(cert, blockOut,
                       fail("Failed to parse cert with SPKI error: %@", url));
#if TARGET_OS_OSX
        pubkey = SecCertificateCopyPublicKey_ios(cert);
#else
        pubkey = SecCertificateCopyPublicKey(cert);
#endif
        is(pubkey, NULL, "Successfully parsed bad SPKI: %@", url);

    blockOut:
        CFReleaseNull(cert);
        CFReleaseNull(pubkey);
    }];

testOut:
    return;
}

static void test_path_parse_failure(void) {
    NSArray <NSURL *>* certURLs = nil;
    SecCertificateRef root = nil;

    NSURL *rootURL = [[NSBundle mainBundle] URLForResource:@"root" withExtension:@".cer" subdirectory:@"si-18-certificate-parse"];
    root = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)[NSData dataWithContentsOfURL:rootURL]);
    require_action(root, testOut,
                   fail("Unable to create root cert"));
    certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestPathFailureResources];
    require_action([certURLs count] > 0, testOut,
                   fail("Unable to find parse test failure certs in bundle."));

    [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
        NSData *certData = [NSData dataWithContentsOfURL:url];
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
        SecTrustRef trust = NULL;
        SecPolicyRef policy = SecPolicyCreateBasicX509();
        SecTrustResultType trustResult = kSecTrustResultInvalid;

        require_noerr_action(SecTrustCreateWithCertificates(cert, policy, &trust), blockOut,
                             fail("Unable to create trust with certificate: %@", url));
        require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)[NSArray arrayWithObject:(__bridge id)root]),
                             blockOut, fail("Unable to set trust in root cert: %@", url));
        require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)[NSDate dateWithTimeIntervalSinceReferenceDate:507200000.0]),
                             blockOut, fail("Unable to set verify date: %@", url));
        require_noerr_action(SecTrustEvaluate(trust, &trustResult), blockOut,
                             fail("Failed to evaluate trust with error: %@", url));
        is(trustResult, kSecTrustResultRecoverableTrustFailure, "Got wrong trust result (%d) for %@", trustResult, url);

        require_action(cert, blockOut,
                       fail("Failed to parse cert with SPKI error: %@", url));

    blockOut:
        CFReleaseNull(cert);
        CFReleaseNull(trust);
        CFReleaseNull(policy);
    }];


testOut:
    CFReleaseNull(root);
}

static void test_todo_failures(void) {
    /* A bunch of certificates with different parsing errors that currently succeed. */
    NSArray <NSURL *>* certURLs = [[NSBundle mainBundle] URLsForResourcesWithExtension:@".cer" subdirectory:(NSString *)kSecTestTODOFailureResources];
    require_action([certURLs count] > 0, testOut,
                   fail("Unable to find parse test failure certs in bundle."));

    [certURLs enumerateObjectsUsingBlock:^(NSURL *url, __unused NSUInteger idx, __unused BOOL *stop) {
        NSData *certData = [NSData dataWithContentsOfURL:url];
        SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)certData);
        {
            NSString *reason = [NSString stringWithFormat:@"Bad cert succeeds: %@",url];
            todo([reason UTF8String]);
            is(cert, NULL, "Successfully parsed bad cert: %@", url);
        }
        CFReleaseNull(cert);
    }];

testOut:
    return;
}

int si_18_certificate_parse(int argc, char *const *argv)
{

    plan_tests((int)num_certs());

    test_parse_failure();
    test_parse_success();
    test_key_failure();
    test_path_parse_failure();
    test_todo_failures();

    return 0;
}
