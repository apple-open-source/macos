/*
 * Copyright (c) 2015-2019 Apple Inc. All Rights Reserved.
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
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCertificatePriv.h>
#include <utilities/SecCFWrappers.h>

#include "../TestMacroConversions.h"
#include "SSLPolicyTests_data.h"
#import "TrustEvaluationTestCase.h"

NSString * const testDirectory = @"ssl-policy-certs";

@interface SSLPolicyTests : TrustEvaluationTestCase
@end

@implementation SSLPolicyTests

- (void)testSSLPolicyCerts
{
    NSArray *testPlistURLs = [[NSBundle bundleForClass:[self class]] URLsForResourcesWithExtension:@"plist" subdirectory:testDirectory];
    if (testPlistURLs.count != 1) {
        fail("failed to find the test plist");
        return;
    }

    NSDictionary <NSString *,NSDictionary *>*tests_dictionary = [NSDictionary dictionaryWithContentsOfURL:testPlistURLs[0]];
    if (!tests_dictionary) {
        fail("failed to create test driver dictionary");
        return;
    }

    [tests_dictionary enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSDictionary * _Nonnull obj, BOOL * _Nonnull stop) {
        CFDictionaryRef test_info = (__bridge CFDictionaryRef)obj;
        CFStringRef test_name = (__bridge CFStringRef)key, file = NULL, reason = NULL, expectedResult = NULL, failureReason = NULL;
        CFErrorRef trustError = NULL;
        NSURL *cert_file_url = NULL;
        NSData *cert_data = nil;
        bool expectTrustSuccess = false;

        SecCertificateRef leaf = NULL, root = NULL;
        CFStringRef hostname = NULL;
        SecPolicyRef policy = NULL;
        SecTrustRef trust = NULL;
        CFArrayRef anchor_array = NULL;
        CFDateRef date = NULL;

        /* get filename in test dictionary */
        file = CFDictionaryGetValue(test_info, CFSTR("Filename"));
        require_action_quiet(file, cleanup, fail("%@: Unable to load filename from plist", test_name));

        /* get leaf certificate from file */
        cert_file_url = [[NSBundle bundleForClass:[self class]] URLForResource:(__bridge NSString *)file withExtension:@"cer" subdirectory:testDirectory];
        require_action_quiet(cert_file_url, cleanup, fail("%@: Unable to get url for cert file %@",
                                                          test_name, file));

        cert_data = [NSData dataWithContentsOfURL:cert_file_url];

        /* create certificates */
        leaf = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)cert_data);
        root = SecCertificateCreateWithBytes(NULL, _SSLTrustPolicyTestRootCA, sizeof(_SSLTrustPolicyTestRootCA));
        require_action_quiet(leaf && root, cleanup, fail("%@: Unable to create certificates", test_name));

        /* create policy */
        hostname = CFDictionaryGetValue(test_info, CFSTR("Hostname"));
        require_action_quiet(hostname, cleanup, fail("%@: Unable to load hostname from plist", test_name));

        policy = SecPolicyCreateSSL(true, hostname);
        require_action_quiet(policy, cleanup, fail("%@: Unable to create SSL policy with hostname %@",
                                                   test_name, hostname));

        /* create trust ref */
        OSStatus err = SecTrustCreateWithCertificates(leaf, policy, &trust);
        CFRelease(policy);
        require_noerr_action(err, cleanup, ok_status(err, "SecTrustCreateWithCertificates"));

        /* set anchor in trust ref */
        anchor_array = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks);
        require_action_quiet(anchor_array, cleanup, fail("%@: Unable to create anchor array", test_name));
        err = SecTrustSetAnchorCertificates(trust, anchor_array);
        require_noerr_action(err, cleanup, ok_status(err, "SecTrustSetAnchorCertificates"));

        /* set date in trust ref to 4 Sep 2015 */
        date = CFDateCreate(NULL, 463079909.0);
        require_action_quiet(date, cleanup, fail("%@: Unable to create verify date", test_name));
        err = SecTrustSetVerifyDate(trust, date);
        CFRelease(date);
        require_noerr_action(err, cleanup, ok_status(err, "SecTrustSetVerifyDate"));

        /* evaluate */
        bool is_valid = SecTrustEvaluateWithError(trust, &trustError);
        if (!is_valid) {
            failureReason = CFErrorCopyDescription(trustError);
        }

        /* get expected result for test */
        expectedResult = CFDictionaryGetValue(test_info, CFSTR("Result"));
        require_action_quiet(expectedResult, cleanup, fail("%@: Unable to get expected result",test_name));
        if (!CFStringCompare(expectedResult, CFSTR("kSecTrustResultUnspecified"), 0) ||
            !CFStringCompare(expectedResult, CFSTR("kSecTrustResultProceed"), 0)) {
            expectTrustSuccess = true;
        }

        /* process results */
        if (!CFDictionaryGetValueIfPresent(test_info, CFSTR("Reason"), (const void **)&reason)) {
            /* not a known failure */
            ok(is_valid == expectTrustSuccess, "%s %@%@",
               expectTrustSuccess ? "REGRESSION" : "SECURITY",
               test_name,
               trustError);
        } else if (reason) {
            /* known failure */
            XCTAssertTrue(true, "TODO test: %@", reason);
            ok(is_valid == expectTrustSuccess, "%@%@",
               test_name, expectTrustSuccess ? failureReason : CFSTR(" valid"));
        } else {
            fail("%@: unable to get reason for known failure", test_name);
        }

    cleanup:
        CFReleaseNull(leaf);
        CFReleaseNull(root);
        CFReleaseNull(trust);
        CFReleaseNull(anchor_array);
        CFReleaseNull(failureReason);
        CFReleaseNull(trustError);
    }];
}

- (BOOL)runTrustEvaluation:(NSArray *)certs anchors:(NSArray *)anchors error:(NSError **)error
{
    return [self runTrustEvaluation:certs anchors:anchors date:590000000.0 error:error]; // September 12, 2019 at 9:53:20 AM PDT
}

- (BOOL)runTrustEvaluation:(NSArray *)certs anchors:(NSArray *)anchors date:(NSTimeInterval)evalTime error:(NSError **)error
{
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:evalTime];
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

- (void)testSystemTrust_MissingEKU
{
    [self setTestRootAsSystem:_EKUTestSSLRootHash];
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _noEKU_BeforeJul2019, sizeof(_noEKU_BeforeJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "system-trusted missing EKU cert succeeded");

    [self removeTestRootAsSystem];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testSystemTrust_AnyEKU
{
    [self setTestRootAsSystem:_EKUTestSSLRootHash];
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _anyEKU_BeforeJul2019, sizeof(_anyEKU_BeforeJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "system-trusted Any EKU cert succeeded");

    [self removeTestRootAsSystem];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testSystemTrust_ServerAuthEKU
{
    [self setTestRootAsSystem:_EKUTestSSLRootHash];
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _serverEKU_BeforeJul2019, sizeof(_serverEKU_BeforeJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "system-trusted ServerAuth EKU cert failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testSystemTrust_subCAMissingEKU
{
    SecCertificateRef systemRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_Root"
                                                                                         subdirectory:testDirectory];
    NSData *rootHash = CFBridgingRelease(SecCertificateCopySHA256Digest(systemRoot));
    [self setTestRootAsSystem:(const uint8_t *)[rootHash bytes]];

    SecCertificateRef subCa = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_noEKU_ca"
                                                                                    subdirectory:testDirectory];
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_noEKU_leaf"
                                                                                    subdirectory:testDirectory];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCa];
    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)systemRoot] date:620000000.0 error:&error], //August 24, 2020 at 3:13:20 PM PDT
                  "system-trusted no EKU subCA cert failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(systemRoot);
    CFReleaseNull(subCa);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_subCAAnyEKU
{
    SecCertificateRef systemRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_Root"
                                                                                         subdirectory:testDirectory];
    NSData *rootHash = CFBridgingRelease(SecCertificateCopySHA256Digest(systemRoot));
    [self setTestRootAsSystem:(const uint8_t *)[rootHash bytes]];

    SecCertificateRef subCa = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_anyEKU_ca"
                                                                                    subdirectory:testDirectory];
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_anyEKU_leaf"
                                                                                    subdirectory:testDirectory];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCa];
    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)systemRoot] date:620000000.0 error:&error], //August 24, 2020 at 3:13:20 PM PDT
                  "system-trusted anyEKU subCA cert failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(systemRoot);
    CFReleaseNull(subCa);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_subCAServerAuthEKU
{
    SecCertificateRef systemRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_Root"
                                                                                         subdirectory:testDirectory];
    NSData *rootHash = CFBridgingRelease(SecCertificateCopySHA256Digest(systemRoot));
    [self setTestRootAsSystem:(const uint8_t *)[rootHash bytes]];

    SecCertificateRef subCa = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_ssl_ca"
                                                                                    subdirectory:testDirectory];
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_ssl_leaf"
                                                                                    subdirectory:testDirectory];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCa];
    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)systemRoot] date:620000000.0 error:&error], //August 24, 2020 at 3:13:20 PM PDT
                  "system-trusted SSL EKU subCA cert failed: %@", error);

    [self removeTestRootAsSystem];
    CFReleaseNull(systemRoot);
    CFReleaseNull(subCa);
    CFReleaseNull(leaf);
}

- (void)testSystemTrust_subCA_SMIME_EKU
{
    SecCertificateRef systemRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_Root"
                                                                                         subdirectory:testDirectory];
    NSData *rootHash = CFBridgingRelease(SecCertificateCopySHA256Digest(systemRoot));
    [self setTestRootAsSystem:(const uint8_t *)[rootHash bytes]];

    SecCertificateRef subCa = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_smime_ca"
                                                                                    subdirectory:testDirectory];
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_smime_leaf"
                                                                                    subdirectory:testDirectory];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCa];
    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:certs anchors:@[(__bridge id)systemRoot] date:620000000.0 error:&error], //August 24, 2020 at 3:13:20 PM PDT
                   "system-trusted SMIME subCA cert succeeded");
    XCTAssertNotNil(error);
    if (error) {
        XCTAssertEqual(error.code, errSecInvalidExtendedKeyUsage);
    }

    [self removeTestRootAsSystem];
    CFReleaseNull(systemRoot);
    CFReleaseNull(subCa);
    CFReleaseNull(leaf);
}

- (void)testAppTrust_subCA_SMIME_EKU
{
    SecCertificateRef systemRoot = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_Root"
                                                                                         subdirectory:testDirectory];
    SecCertificateRef subCa = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_smime_ca"
                                                                                    subdirectory:testDirectory];
    SecCertificateRef leaf = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"subCA_EKU_smime_leaf"
                                                                                    subdirectory:testDirectory];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCa];
    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)systemRoot] date:620000000.0 error:&error], //August 24, 2020 at 3:13:20 PM PDT
                  "app-trusted smime subCA cert failed: %@", error);

    CFReleaseNull(systemRoot);
    CFReleaseNull(subCa);
    CFReleaseNull(leaf);
}

// Other app trust of root SSL EKU tests of certs issued before July 2019 occur in testSSLPolicyCerts (Test4, Test17)

- (void)testAppTrustRoot_AnyEKU_BeforeJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _anyEKU_BeforeJul2019, sizeof(_anyEKU_BeforeJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "app-trusted root, anyEKU leaf failed: %@", error);

    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testAppTrustRoot_MissingEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _noEKU_AfterJul2019, sizeof(_noEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "app-trusted root, missing EKU leaf succeeded");

    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testAppTrustLegacy_MissingEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _noEKU_AfterJul2019, sizeof(_noEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    SecPolicyRef policy = SecPolicyCreateLegacySSL(true, CFSTR("example.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:590000000.0]; // September 12, 2019 at 9:53:20 AM PDT
    SecTrustRef trustRef = NULL;
    CFErrorRef cferror = NULL;

    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trustRef), errOut);
    require_noerr(SecTrustSetVerifyDate(trustRef, (__bridge CFDateRef)date), errOut);
    require_noerr(SecTrustSetAnchorCertificates(trustRef, (__bridge CFArrayRef)@[(__bridge id)root]), errOut);

    XCTAssertTrue(SecTrustEvaluateWithError(trustRef, &cferror));

errOut:
    CFReleaseNull(policy);
    CFReleaseNull(trustRef);
    CFReleaseNull(cferror);
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testAppTrustRoot_AnyEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _anyEKU_AfterJul2019, sizeof(_anyEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "app-trusted root, anyEKU leaf succeeded");

    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testAppTrustRoot_ServerAuthEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _serverEKU_AfterJul2019, sizeof(_serverEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:@[(__bridge id)root] error:&error], "app-trusted root, serverAuth EKU leaf failed: %@", error);

    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testAppTrustLeaf_MissingEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _noEKU_AfterJul2019, sizeof(_noEKU_AfterJul2019));

    NSError *error = nil;
    XCTAssertFalse([self runTrustEvaluation:@[(__bridge id)leaf] anchors:@[(__bridge id)leaf] error:&error], "app-trusted missing EKU leaf succeeded");

    CFReleaseNull(leaf);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't support trust settings
- (void)testUserTrustRoot_MissingEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _noEKU_AfterJul2019, sizeof(_noEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    id persistentRef = [self addTrustSettingsForCert:root];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:nil error:&error], "user-trusted root, missing EKU leaf failed: %@", error);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testUserTrustRoot_AnyEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _anyEKU_AfterJul2019, sizeof(_anyEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    id persistentRef = [self addTrustSettingsForCert:root];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:nil error:&error], "user-trusted root, anyEKU leaf failed: %@", error);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testUserTrustRoot_ServerAuthEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _serverEKU_AfterJul2019, sizeof(_serverEKU_AfterJul2019));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _EKUTestSSLRoot, sizeof(_EKUTestSSLRoot));
    id persistentRef = [self addTrustSettingsForCert:root];
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)root];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:certs anchors:nil error:&error], "user-trusted root, serverAuth EKU leaf failed: %@", error);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(leaf);
    CFReleaseNull(root);
}

- (void)testUserTrustLeaf_MissingEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _noEKU_AfterJul2019, sizeof(_noEKU_AfterJul2019));
    id persistentRef = [self addTrustSettingsForCert:leaf];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error], "user-trusted  missing EKU leaf failed: %@", error);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}

- (void)testUserTrustLeaf_AnyEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _anyEKU_AfterJul2019, sizeof(_anyEKU_AfterJul2019));
    id persistentRef = [self addTrustSettingsForCert:leaf];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error], "user-trusted anyEKU leaf failed: %@", error);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}

- (void)testUserTrustLeaf_ServerAuthEKU_AfterJul2019
{
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _serverEKU_AfterJul2019, sizeof(_serverEKU_AfterJul2019));
    id persistentRef = [self addTrustSettingsForCert:leaf];

    NSError *error = nil;
    XCTAssertTrue([self runTrustEvaluation:@[(__bridge id)leaf] anchors:nil error:&error], "user-trusted serverAuth EKU leaf failed: %@", error);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}
#endif // !TARGET_OS_BRIDGE

- (void)testIPAddressInDNSField
{
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _ipAddress_dnsField, sizeof(_ipAddress_dnsField));
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("10.0.0.1"));
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)cert]
                                                                         policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)cert]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]];
    XCTAssertFalse([eval evaluate:nil]);

    CFReleaseNull(cert);
    CFReleaseNull(policy);
}

- (void)testIPAddressInSAN_Match
{
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _ipAddress_SAN, sizeof(_ipAddress_SAN));
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("10.0.0.1"));
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)cert]
                                                                         policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)cert]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]];
    XCTAssert([eval evaluate:nil]);

    CFReleaseNull(cert);
    CFReleaseNull(policy);
}

- (void)testIPAddressInSAN_Mismatch
{
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _ipAddress_SAN, sizeof(_ipAddress_SAN));
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("10.0.0.2"));
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)cert]
                                                                         policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)cert]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]];
    XCTAssertFalse([eval evaluate:nil]);

    CFReleaseNull(cert);
    CFReleaseNull(policy);
}

- (void)testIPAddressInCN
{
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _ipAddress_CN, sizeof(_ipAddress_CN));
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("10.0.0.1"));
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)cert]
                                                                         policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)cert]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]];
    XCTAssertFalse([eval evaluate:nil]);

    CFReleaseNull(cert);
    CFReleaseNull(policy);
}

- (void)testBadIPAddressInSAN
{
    SecCertificateRef cert = SecCertificateCreateWithBytes(NULL, _ipAddress_bad, sizeof(_ipAddress_bad));
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("10.0.0.1"));
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[(__bridge id)cert]
                                                                         policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)cert]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]];
    XCTAssertFalse([eval evaluate:nil]);

    CFReleaseNull(cert);
    CFReleaseNull(policy);
}

@end
