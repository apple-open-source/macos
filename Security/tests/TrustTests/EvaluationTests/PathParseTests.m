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
#include <XCTest/XCTest.h>
#include <Foundation/Foundation.h>
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
    CFReleaseNull(root);
}

- (void)testUnparseableExtensions {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _bad_extension_leaf, sizeof(_bad_extension_leaf));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _bad_extension_root, sizeof(_bad_extension_root));
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    CFErrorRef error = NULL;
    NSArray *anchors = @[(__bridge id)root];

    require_noerr_action(SecTrustCreateWithCertificates(leaf, policy, &trust), errOut,
                         fail("Unable to create trust with certificate with unparseable extension"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors),
                         errOut, fail("Unable to set trust anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)[NSDate dateWithTimeIntervalSinceReferenceDate:620000000.0]),
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

- (void)testDuplicateExtensions {
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _duplicate_extn_leaf, sizeof(_duplicate_extn_leaf));

    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)leaf] policies:nil];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:635000000]]; // February 14, 2021 at 4:53:20 AM PST
    [eval setAnchors:@[(__bridge id)leaf]];

    NSError *error = nil;
    XCTAssertFalse([eval evaluate:&error]);
    XCTAssertNotNil(error);
    XCTAssertEqual(error.code, errSecCertificateDuplicateExtension);

    CFReleaseNull(leaf);
}

// NSTask is only supported on these platforms
#if TARGET_OS_OSX || (defined(TARGET_OS_MACCATALYST) && TARGET_OS_MACCATALYST)
#define PLATFORM_HAS_NS_TASK 1
#endif
#if PLATFORM_HAS_NS_TASK

static int _runTask(NSTask *task, NSString **result) {
    NSPipe *outPipe = [NSPipe pipe];
    task.standardOutput = outPipe;
    task.standardError = outPipe;
    [task launch];
    [task waitUntilExit];

    int status = task.terminationStatus;
    NSFileHandle *read = [outPipe fileHandleForReading];
    NSString *output = [[NSString alloc] initWithData:[read readDataToEndOfFile] encoding:NSUTF8StringEncoding];
    [read closeFile];
    if (result) { *result = output; } else { output = nil; }
    return status;
}

static int _runLogModeCommand(NSString *mode, NSString **result) {
    NSTask *task = [[NSTask alloc] init];
    task.launchPath = @"/usr/bin/log";
    task.arguments = @[ @"config", @"--mode", mode ];
    return _runTask(task, result);
}

static int _runLogLookupCommand(NSString *query, NSString **result) {
    NSTask *task = [[NSTask alloc] init];
    NSString *predicate = [NSString stringWithFormat:@"subsystem==\"%@\" and eventMessage contains \"%@\"", @"com.apple.securityd", query];
    // note: we do not use the --process argument since it acts as OR
    // with the predicate, returning everything logged by the process
    task.launchPath = @"/usr/bin/log";
    task.arguments = @[ @"show", @"--debug", @"--info", @"--predicate", predicate, @"--last", @"1m" ];
    return _runTask(task, result);
}

static int _runEvaluateCommand(NSString *path, NSString **result) {
    NSTask *task = [[NSTask alloc] init];
#if TARGET_OS_OSX
    task.launchPath = @"/usr/bin/security";
#else
    task.launchPath = @"/usr/local/bin/security";
#endif
    task.arguments = @[ @"verify-cert", @"-c", path ];
    return _runTask(task, result);
}

static void _evaluateTrustForCertificate(NSURL *certURL, bool local) {
    // run security command to evaluate with installed trustd
    XCTAssertTrue(_runEvaluateCommand([certURL path], nil));
    if (!local) { return; }
    // perform local evaluation with libtrustd in the test app
    // (this logging seems unaffected by our private_data:off call!)
    SecCertificateRef cert = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    XCTAssert(cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)[NSData dataWithContentsOfURL:certURL]), "Unable to create cert from %@", certURL);
    XCTAssert(policy = SecPolicyCreateBasicX509());
    XCTAssert(errSecSuccess == SecTrustCreateWithCertificates(cert, policy, &trust), "Unable to create trust");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "Got wrong trust result for cert");
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(cert);
}

#endif // PLATFORM_HAS_NS_TASK

- (void)testPathLogPrivacy {
#if !PLATFORM_HAS_NS_TASK
    XCTSkip("Does not have NSTask");
#else
    // Test must run as root to enable/disable private logging
    XCTSkipIf(geteuid() != 0, @"Not running as root");

    NSString *output = nil;
    NSString *VIEWABLE_NAME = @"Leaf serial invalid policyssl complete smime"; // name we expect to find in the log
    NSString *REDACTED_NAME = @"Leaf serial invalid policyssl complete ssl"; // name we expect to NOT find in the log
    NSURL *cert1URL = [[NSBundle bundleForClass:[self class]] URLForResource:@"Valid-leaf-serial+invalid+policyssl+complete-smime" withExtension:@".cer" subdirectory:@"si-20-sectrust-policies-data"];
    NSURL *cert2URL = [[NSBundle bundleForClass:[self class]] URLForResource:@"Valid-leaf-serial+invalid+policyssl+complete-ssl" withExtension:@".cer" subdirectory:@"si-20-sectrust-policies-data"];

    // enable private data logging (expect command to return 0)
    XCTAssertFalse(_runLogModeCommand(@"private_data:on", nil));

    // evaluate first certificate, whose name is VIEWABLE_NAME
    _evaluateTrustForCertificate(cert1URL, 0);

    // expect to find VIEWABLE_NAME in the log within the past minute since we just evaluated it
    XCTAssertFalse(_runLogLookupCommand(VIEWABLE_NAME, &output));
#if RUNNING_IN_XCODE
    // expected log output is found when running this xctest in Xcode
    XCTAssertTrue(output && [output containsString:VIEWABLE_NAME]);
#endif
    output = nil;

    // disable private data logging (expect command to return 0)
    XCTAssertFalse(_runLogModeCommand(@"private_data:off", nil));

    // evaluate second certificate, whose name is REDACTED_NAME
    _evaluateTrustForCertificate(cert2URL, 0);

    // expect to NOT find REDACTED_NAME in the log within the past minute after we have evaluated it with private logging off
    XCTAssertFalse(_runLogLookupCommand(REDACTED_NAME, &output));
    XCTAssertFalse(output && [output containsString:REDACTED_NAME]);
    output = nil;

    // re-enable private data logging (expect command to return 0)
    XCTAssertFalse(_runLogModeCommand(@"private_data:on", nil));

#endif
    return;
}


@end
