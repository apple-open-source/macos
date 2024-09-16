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
#import <Foundation/Foundation.h>

#import <archive.h>
#import <archive_entry.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecTrustInternal.h>
#include <Security/SecItem.h>
#include <utilities/SecCFWrappers.h>

#import "TrustEvaluationTestCase.h"
#include "NameConstraintsTests_data.h"
#include "../TestMacroConversions.h"

@interface NameConstraintsTests : TrustEvaluationTestCase
@end

@implementation NameConstraintsTests

- (void)testIntersectingConstraintsInSubCA {
    SecCertificateRef root = NULL, subca = NULL, leaf1 = NULL;
    NSArray *certs1 = nil, *test_anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:517282600.0]; // 23 May 2017

    require_action(root = SecCertificateCreateWithBytes(NULL, _test_root, sizeof(_test_root)), errOut,
                   fail("Failed to create root cert"));
    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf1 = SecCertificateCreateWithBytes(NULL, _test_leaf1, sizeof(_test_leaf1)), errOut,
                   fail("Failed to create leaf cert 1"));

    certs1 = @[(__bridge id)leaf1, (__bridge id)subca];
    test_anchors = @[(__bridge id)root];

    /* Test multiple pre-pended labels and intersecting name constraints in the subCA */
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)test_anchors), errOut,
                         fail("Failed to set anchors"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(leaf1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void) testNoPrePendedLabels {
    SecCertificateRef root = NULL, subca = NULL, leaf2 = NULL;
    NSArray *certs2 = nil, *test_anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:517282600.0]; // 23 May 2017

    require_action(root = SecCertificateCreateWithBytes(NULL, _test_root, sizeof(_test_root)), errOut,
                   fail("Failed to create root cert"));
    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf2 = SecCertificateCreateWithBytes(NULL, _test_leaf2, sizeof(_test_leaf2)), errOut,
                   fail("Failed to create leaf cert 2"));

    certs2 = @[(__bridge id)leaf2, (__bridge id)subca];
    test_anchors = @[(__bridge id)root];

    /* Test no pre-pended labels */
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs2,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 2"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)test_anchors), errOut,
                         fail("Failed to set anchors"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(leaf2);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void) testDNSLookingCommonName {
    SecCertificateRef root = NULL, subca = NULL, leaf3 = NULL;
    NSArray *certs3 = nil, *test_anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:548800000.0]; // 23 May 2018

    require_action(root = SecCertificateCreateWithBytes(NULL, _test_root, sizeof(_test_root)), errOut,
                   fail("Failed to create root cert"));
    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf3 = SecCertificateCreateWithBytes(NULL, _test_leaf3, sizeof(_test_leaf3)), errOut,
                   fail("Failed to create leaf cert 3"));

    certs3 = @[(__bridge id)leaf3, (__bridge id)subca];
    test_anchors = @[(__bridge id)root];

    /* Test DNS-looking Common Name */
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs3,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 3"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)test_anchors), errOut,
                         fail("Failed to set anchors"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(leaf3);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testUserAnchorWithNameConstraintsPass {
#if TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
    XCTSkip();
#endif
    SecCertificateRef subca = NULL, leaf1 = NULL;
    NSArray *certs1 = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:517282600.0]; // 23 May 2017
    id persistentRef = nil;

    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf1 = SecCertificateCreateWithBytes(NULL, _test_leaf1, sizeof(_test_leaf1)), errOut,
                   fail("Failed to create leaf cert 1"));

    certs1 = @[(__bridge id)leaf1, (__bridge id)subca];
    persistentRef = [self addTrustSettingsForCert:subca];  // subCA has name constraints
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    [self removeTrustSettingsForCert:subca persistentRef:persistentRef];
errOut:
    CFReleaseNull(subca);
    CFReleaseNull(leaf1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testAppAnchorWithNameConstraintsPass {
    SecCertificateRef subca = NULL, leaf1 = NULL;
    NSArray *certs1 = nil, *test_anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:517282600.0]; // 23 May 2017

    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf1 = SecCertificateCreateWithBytes(NULL, _test_leaf1, sizeof(_test_leaf1)), errOut,
                   fail("Failed to create leaf cert 1"));

    certs1 = @[(__bridge id)leaf1, (__bridge id)subca];
    test_anchors = @[(__bridge id)subca]; // subCA has name constraints

    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)test_anchors), errOut,
                         fail("Failed to set anchors"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
errOut:
    CFReleaseNull(subca);
    CFReleaseNull(leaf1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testUserAnchorWithNameConstraintsFail {
#if TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
    XCTSkip();
#endif
    SecCertificateRef subca = NULL, leaf4 = NULL;
    NSArray *certs1 = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // 23 May 2017
    id persistentRef = nil;

    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf4 = SecCertificateCreateWithBytes(NULL, _test_leaf4, sizeof(_test_leaf4)), errOut,
                   fail("Failed to create leaf cert 4"));

    certs1 = @[(__bridge id)leaf4, (__bridge id)subca];
    persistentRef = [self addTrustSettingsForCert:subca];  // subCA has name constraints
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 4"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    [self removeTrustSettingsForCert:subca persistentRef:persistentRef];
errOut:
    CFReleaseNull(subca);
    CFReleaseNull(leaf4);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testAppAnchorwithNameContraintsFail {
    SecCertificateRef subca = NULL, leaf4 = NULL;
    NSArray *certs1 = nil, *test_anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // 23 May 2017

    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf4 = SecCertificateCreateWithBytes(NULL, _test_leaf4, sizeof(_test_leaf4)), errOut,
                   fail("Failed to create leaf cert 4"));

    certs1 = @[(__bridge id)leaf4, (__bridge id)subca];
    test_anchors = @[(__bridge id)subca]; // subCA has name constraints

    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 4"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)test_anchors), errOut,
                         fail("Failed to set anchors"));
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
errOut:
    CFReleaseNull(subca);
    CFReleaseNull(leaf4);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testNameConstraintsPlist {
    NSURL *testPlist = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"debugging" withExtension:@"plist"
                                                          subdirectory:@"TestTrustEvaluation-data"];
    if (!testPlist) {
        testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"NameConstraints" withExtension:@"plist"
                                                              subdirectory:@"TestTrustEvaluation-data"];
    }
    if (!testPlist) {
        fail("Failed to get tests plist from %@", @"TestTrustEvaluation-data");
        return;
    }

    testsArray = [NSArray arrayWithContentsOfURL: testPlist];
    if (!testsArray) {
        fail("Failed to create array from plist");
        return;
    }

    [testsArray enumerateObjectsUsingBlock:^(NSDictionary *testDict, NSUInteger idx, BOOL * _Nonnull stop) {
        TestTrustEvaluation *testObj = [[TestTrustEvaluation alloc] initWithTrustDictionary:testDict];
        XCTAssertNotNil(testObj, "failed to create test object for %lu", (unsigned long)idx);

        NSError *testError = nil;
        XCTAssert([testObj evaluateForExpectedResults:&testError], "Test %@ failed: %@", testObj.fullTestName, testError);
        NSString *failDesc = CFBridgingRelease(SecTrustCopyFailureDescription([testObj trust]));
        XCTAssert([failDesc containsString:(id)kSecPolicyCheckNameConstraints]);
    }];
}

/* MARK: BetterTLS tests */
NSString *kSecTrustTestNameConstraintsResources = @"si-87-sectrust-name-constraints";
NSString *kSecTrustTestCertificates = @"TestCertificates";
NSString *kSecTrustTestIPAddress = @"52.20.118.238";
NSString *kSecTrustTestDNSAddress = @"test.nameconstraints.bettertls.com";
NSString *kSecTrustTestID = @"id";
NSString *kSecTrustTestDNSResult = @"dns";
NSString *kSecTrustTestIPResult = @"ip";
NSString *kSecTrustTestExpect = @"expect";
NSString *kSecTrustTestExpectFailure  = @"ERROR";
NSString *kSecTrustTestExpectSuccess = @"OK";
NSString *kSecTrustTestExpectMaybeSuccess = @"WEAK-OK";

static NSArray *anchors = nil;
static NSURL *tmpCertsDir = nil;

- (NSArray*)getTestsArray {
    NSURL *testPlist = nil;
    NSDictionary *testsDict = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"debugging" withExtension:@"plist"
                                                          subdirectory:kSecTrustTestNameConstraintsResources];
    if (!testPlist) {
        testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"expects" withExtension:@"plist"
                                                              subdirectory:kSecTrustTestNameConstraintsResources];
    }
    require_action_quiet(testPlist, exit,
                         fail("Failed to get tests plist from %@", kSecTrustTestNameConstraintsResources));
    testsDict = [NSDictionary dictionaryWithContentsOfURL:testPlist];
    require_action_quiet(testsDict, exit, fail("Failed to decode tests plist into dictionary"));

    testsArray = testsDict[@"expects"];
    require_action_quiet(testsArray, exit, fail("Failed to get expects array from test dictionary"));
    require_action_quiet([testsArray isKindOfClass:[NSArray class]], exit, fail("expected array of tests"));

exit:
    return testsArray;
}

- (NSFileHandle *)openFileForWriting:(const char *)filename {
    NSFileHandle *fileHandle = NULL;
    NSURL *file = [NSURL URLWithString:[NSString stringWithCString:filename encoding:NSUTF8StringEncoding] relativeToURL:tmpCertsDir];
    int fd;
    fd = open([file fileSystemRepresentation], O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0  || lseek(fd, 0, SEEK_SET) < 0) {
        fail("unable to open file for archive");
    }
    if (fd >= 0) {
        close(fd);
    }

    NSError *error;
    fileHandle = [NSFileHandle fileHandleForWritingToURL:file error:&error];
    if (!fileHandle) {
        fail("unable to get file handle for %@\n\terror:%@", file, error);
    }

    return fileHandle;
}

- (BOOL)extract:(NSURL *)archive {
    BOOL result = NO;
    int r;
    struct archive_entry *entry;

    struct archive *a = archive_read_new();
    archive_read_support_filter_all(a);
    archive_read_support_format_tar(a);
    r = archive_read_open_filename(a, [archive fileSystemRepresentation], 16384);
    if (r != ARCHIVE_OK) {
        fail("unable to open archive");
        goto exit;
    }

    while((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        @autoreleasepool {
            const char *filename = archive_entry_pathname(entry);
            NSFileHandle *fh = [self openFileForWriting:filename];
            ssize_t size = 0;
            size_t bufsize = 4192;
            uint8_t *buf = calloc(bufsize, 1);
            for (;;) {
                size = archive_read_data(a, buf, bufsize);
                if (size < 0) {
                    fail("failed to read %s from archive", filename);
                    [fh closeFile];
                    goto exit;
                }
                if (size == 0) {
                    break;
                }
                [fh writeData:[NSData dataWithBytes:buf length:(size_t)size]];
            }
            free(buf);
            [fh closeFile];
        }
    }
    if (r != ARCHIVE_EOF) {
        fail("unable to read archive header");
    } else {
        result = YES;
    }

exit:
    archive_read_free(a);
    return result;
}

- (BOOL)untar_test_certs {
    NSError *error = nil;
    NSString* pid = [NSString stringWithFormat: @"tst-%d", [[NSProcessInfo processInfo] processIdentifier]];
    NSURL* tmpPidDirURL = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:pid isDirectory:YES];
    tmpCertsDir = [tmpPidDirURL URLByAppendingPathComponent:kSecTrustTestNameConstraintsResources isDirectory:YES];

    if (![[NSFileManager defaultManager] createDirectoryAtURL:tmpCertsDir
                                  withIntermediateDirectories:NO
                                                   attributes:NULL
                                                        error:&error]) {
        fail("unable to create temporary cert directory: %@", error);
        return NO;
    }

    NSURL *certs_tar = [[NSBundle bundleForClass:[self class]] URLForResource:kSecTrustTestCertificates withExtension:nil
                                                                 subdirectory:kSecTrustTestNameConstraintsResources];
    if(![self extract:certs_tar]) {
        return NO;
    }

    return YES;
}

- (BOOL)extractLeaf:(NSString *)filename andAppendTo:(NSMutableArray *)certs {
    NSString *fullFilename = [NSString stringWithFormat:@"%@.cer", filename];
    NSURL *leafURL = [tmpCertsDir URLByAppendingPathComponent:fullFilename];
    if (!leafURL) {
        fail("Failed to get leaf certificate for test id %@", filename);
        return NO;
    }
    NSData *leafData = [NSData dataWithContentsOfURL:leafURL];
    if (!leafData) {
        fail("Failed to get leaf certificate data for URL %@", leafURL);
        return NO;
    }
    SecCertificateRef cert = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)leafData);
    if (!leafData) {
        fail("Failed to create leaf cert for %@", leafURL);
        return NO;
    }
    [certs addObject:(__bridge id)cert];
    CFReleaseNull(cert);
    return YES;
}

- (BOOL)extractChain:(NSString *)filename andAppendTo:(NSMutableArray *)certs {
    NSString *fullFilename = [NSString stringWithFormat:@"%@.chain", filename];
    NSURL *chainURL = [tmpCertsDir URLByAppendingPathComponent:fullFilename];
    if (!chainURL) {
        fail("Failed to get chain URL for %@", filename);
        return NO;
    }
    NSString *chain = [NSString stringWithContentsOfURL:chainURL encoding:NSUTF8StringEncoding error:nil];
    if (!chain) {
        fail("Failed to get chain for %@", chainURL);
        return NO;
    }

    NSString *pattern = @"-----BEGIN CERTIFICATE-----.+?-----END CERTIFICATE-----\n";
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:pattern
                                                                           options:NSRegularExpressionDotMatchesLineSeparators|NSRegularExpressionUseUnixLineSeparators
                                                                             error:nil];
    [regex enumerateMatchesInString:chain options:0 range:NSMakeRange(0, [chain length])
                         usingBlock:^(NSTextCheckingResult * _Nullable result, NSMatchingFlags flags, BOOL * _Nonnull stop) {
                             NSString *certPEMString = [chain substringWithRange:[result range]];
                             NSData *certPEMData = [certPEMString dataUsingEncoding:NSUTF8StringEncoding];
                             SecCertificateRef cert = SecCertificateCreateWithPEM(NULL, (__bridge CFDataRef)certPEMData);
                             [certs addObject:(__bridge id)cert];
                             CFReleaseNull(cert);
                         }];
    return YES;
}

- (BOOL) getAnchor {
    NSURL *rootURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"root" withExtension:@"cer"
                                                               subdirectory:kSecTrustTestNameConstraintsResources];
    if (!rootURL) {
        fail("Failed to get root cert");
        return NO;
    }
    NSData *rootData = [NSData dataWithContentsOfURL:rootURL];
    SecCertificateRef root = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)rootData);
    if (!root) {
        fail("failed to create root cert");
        return NO;
    }
    anchors = [NSArray arrayWithObject:(__bridge id)root];
    CFReleaseNull(root);
    return YES;
}

- (BOOL) runTrustForCerts:(NSArray *)certs hostname:(NSString *)hostname {
    if (!anchors && ![self getAnchor]) {
        return NO;
    }
    BOOL result = NO;
    SecPolicyRef policy = SecPolicyCreateSSL(true, (__bridge CFStringRef)hostname);
    SecTrustRef trust = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:531900000.0]; /* November 8, 2017 at 10:00:00 PM PST */
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), exit,
                         fail("Failed to create trust ref"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), exit,
                         fail("Failed to add anchor"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), exit,
                         fail("Failed to set verify date"));
    result = SecTrustEvaluateWithError(trust, nil);

exit:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    return result;
}

- (void) testBetterTLS {
#if TARGET_OS_WATCH
    // Skip test on watchOS due to size constraints (rdar://66792084)
    XCTSkip();
#endif
    NSArray<NSDictionary *> *testsArray = [self getTestsArray];
    if([self untar_test_certs]) {
        [testsArray enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull testDict, NSUInteger idx, BOOL * _Nonnull stop) {
            @autoreleasepool {
                /* Get the certificates */
                NSNumber *testNum = testDict[kSecTrustTestID];
                NSString *fileName = [NSString stringWithFormat:@"%@",testNum];
                NSMutableArray *certificates = [NSMutableArray array];
                if (![self extractLeaf:fileName andAppendTo:certificates] || ![self extractChain:fileName andAppendTo:certificates]) {
                    return;
                }

                /* Test DNS address */
                NSDictionary *dnsDict = testDict[kSecTrustTestDNSResult];
                BOOL result = [self runTrustForCerts:certificates hostname:kSecTrustTestDNSAddress];
                NSString *dnsExpectedResult = dnsDict[kSecTrustTestExpect];
                if ([dnsExpectedResult isEqualToString:kSecTrustTestExpectFailure]) {
                    is(result, NO,
                       "Test DNS id: %@. Expected %@. Got %d", testNum, dnsExpectedResult, result);
                } else if ([dnsExpectedResult isEqualToString:kSecTrustTestExpectSuccess]) {
                    is(result, YES,
                       "Test DNS id: %@. Expected %@. Got %d", testNum, dnsExpectedResult, result);
                } else if ([dnsExpectedResult isEqualToString:kSecTrustTestExpectMaybeSuccess]) {
                    /* These are "OK" but it's acceptable to reject them */
                }

                /* Test IP address */
                NSDictionary *ipDict = testDict[kSecTrustTestIPResult];
                result = [self runTrustForCerts:certificates hostname:kSecTrustTestIPAddress];
                NSString *ipExpectedResult = ipDict[kSecTrustTestExpect];
                if ([ipExpectedResult isEqualToString:kSecTrustTestExpectFailure]) {
                    is(result, NO,
                       "Test IP id: %@. Expected %@. Got %d", testNum, ipExpectedResult, result);
                } else if ([ipExpectedResult isEqualToString:kSecTrustTestExpectSuccess]) {
                    is(result, YES,
                       "Test IP id: %@. Expected %@. Got %d", testNum, ipExpectedResult, result);
                } else if ([ipExpectedResult isEqualToString:kSecTrustTestExpectMaybeSuccess]) {
                    /* These are "OK" but it's acceptable to reject them */
                }
            }
        }];
    }

    [[NSFileManager defaultManager] removeItemAtURL:tmpCertsDir error:nil];
}

@end
