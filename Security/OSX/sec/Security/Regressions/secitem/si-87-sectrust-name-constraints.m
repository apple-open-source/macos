/*
 * Copyright (c) 2015-2017 Apple Inc. All Rights Reserved.
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>

#import <archive.h>
#import <archive_entry.h>

#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <utilities/SecCFWrappers.h>

#include "shared_regressions.h"

#include "si-87-sectrust-name-constraints.h"


static void tests(void) {
    SecCertificateRef root = NULL, subca = NULL, leaf1 = NULL, leaf2 = NULL;
    NSArray *certs1 = nil, *certs2, *anchors = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:517282600.0]; // 23 May 2017

    require_action(root = SecCertificateCreateWithBytes(NULL, _test_root, sizeof(_test_root)), errOut,
                   fail("Failed to create root cert"));
    require_action(subca = SecCertificateCreateWithBytes(NULL, _test_intermediate, sizeof(_test_intermediate)), errOut,
                   fail("Failed to create subca cert"));
    require_action(leaf1 = SecCertificateCreateWithBytes(NULL, _test_leaf1, sizeof(_test_leaf1)), errOut,
                   fail("Failed to create leaf cert 1"));
    require_action(leaf2 = SecCertificateCreateWithBytes(NULL, _test_leaf2, sizeof(_test_leaf2)), errOut,
                   fail("Failed to create leaf cert 2"));

    certs1 = @[(__bridge id)leaf1, (__bridge id)subca];
    certs2 = @[(__bridge id)leaf2, (__bridge id)subca];
    anchors = @[(__bridge id)root];

    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs1,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut,
                         fail("Failed to set anchors"));
    require_noerr_action(SecTrustEvaluate(trust, &trustResult), errOut,
                         fail("Failed to evaluate trust"));
    is(trustResult, kSecTrustResultUnspecified, "Got wrong trust result for leaf 1");

    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs2,
                                                        policy, &trust), errOut,
                         fail("Failed to create trust for leaf 1"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut,
                         fail("Failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), errOut,
                         fail("Failed to set anchors"));
    require_noerr_action(SecTrustEvaluate(trust, &trustResult), errOut,
                         fail("Failed to evaluate trust"));
    is(trustResult, kSecTrustResultUnspecified, "Got wrong trust result for leaf 1");

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(leaf1);
    CFReleaseNull(leaf2);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
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

static NSArray *getTestsArray(void) {
    NSURL *testPlist = nil;
    NSDictionary *testsDict = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle mainBundle] URLForResource:@"debugging" withExtension:@"plist"
                                         subdirectory:kSecTrustTestNameConstraintsResources];
    if (!testPlist) {
        testPlist = [[NSBundle mainBundle] URLForResource:@"expects" withExtension:@"plist"
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

static NSFileHandle *openFileForWriting(const char *filename) {
    NSFileHandle *fileHandle = NULL;
    NSURL *file = [NSURL URLWithString:[NSString stringWithCString:filename encoding:NSUTF8StringEncoding] relativeToURL:tmpCertsDir];
    int fd;
    off_t off;
    fd = open([file fileSystemRepresentation], O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0  || (off = lseek(fd, 0, SEEK_SET)) < 0) {
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

static BOOL
extract(NSURL *archive) {
    BOOL result = NO;
    int r;
    struct archive_entry *entry;

    struct archive *a = archive_read_new();
    archive_read_support_compression_all(a);
    archive_read_support_format_tar(a);
    r = archive_read_open_filename(a, [archive fileSystemRepresentation], 16384);
    if (r != ARCHIVE_OK) {
        fail("unable to open archive");
        goto exit;
    }

    while((r = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        @autoreleasepool {
            const char *filename = archive_entry_pathname(entry);
            NSFileHandle *fh = openFileForWriting(filename);
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
                [fh writeData:[NSData dataWithBytes:buf length:size]];
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
    archive_read_finish(a);
    return result;
}

static BOOL untar_test_certs(void) {
    NSError *error = nil;
    tmpCertsDir = [[NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES] URLByAppendingPathComponent:kSecTrustTestNameConstraintsResources isDirectory:YES];

    if (![[NSFileManager defaultManager] createDirectoryAtURL:tmpCertsDir
                                  withIntermediateDirectories:NO
                                                   attributes:NULL
                                                        error:&error]) {
        fail("unable to create temporary cert directory: %@", error);
        return NO;
    }

    NSURL *certs_tar = [[NSBundle mainBundle] URLForResource:kSecTrustTestCertificates withExtension:nil
                                                subdirectory:kSecTrustTestNameConstraintsResources];
    if(!extract(certs_tar)) {
        return NO;
    }

    return YES;
}

static BOOL extractLeaf(NSString *filename, NSMutableArray *certs) {
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

static BOOL extractChain(NSString *filename, NSMutableArray *certs) {
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

static BOOL getAnchor(void) {
    NSURL *rootURL = [[NSBundle mainBundle] URLForResource:@"root" withExtension:@"cer"
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

static BOOL testTrust(NSArray *certs, NSString *hostname) {
    if (!anchors && !getAnchor()) {
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
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunguarded-availability-new"
    result = SecTrustEvaluateWithError(trust, nil);
#pragma clang diagnostic pop

exit:
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    return result;
}

void (^runNameConstraintsTestForObject)(id, NSUInteger, BOOL *) =
^(NSDictionary *testDict, NSUInteger idx, BOOL *stop) {
    @autoreleasepool {
        /* Get the certificates */
        NSNumber *testNum = testDict[kSecTrustTestID];
        NSString *fileName = [NSString stringWithFormat:@"%@",testNum];
        NSMutableArray *certificates = [NSMutableArray array];
        if (!extractLeaf(fileName, certificates) || !extractChain(fileName, certificates)) {
            return;
        }

        /* Test DNS address */
        NSDictionary *dnsDict = testDict[kSecTrustTestDNSResult];
        BOOL result = testTrust(certificates, kSecTrustTestDNSAddress);
        NSString *dnsExpectedResult = dnsDict[kSecTrustTestExpect];
        if ([dnsExpectedResult isEqualToString:kSecTrustTestExpectFailure]) {
            is(result, NO,
               "Test DNS id: %@. Expected %@. Got %d", testNum, dnsExpectedResult, result);
        } else if ([dnsExpectedResult isEqualToString:kSecTrustTestExpectSuccess]) {
            is(result, YES,
               "Test DNS id: %@. Expected %@. Got %d", testNum, dnsExpectedResult, result);
        } else if ([dnsExpectedResult isEqualToString:kSecTrustTestExpectMaybeSuccess]) {
            /* These are "OK" but it's acceptable to reject them */
            pass();
        }

        /* Test IP address */
        NSDictionary *ipDict = testDict[kSecTrustTestIPResult];
        result = testTrust(certificates, kSecTrustTestIPAddress);
        NSString *ipExpectedResult = ipDict[kSecTrustTestExpect];
        if ([ipExpectedResult isEqualToString:kSecTrustTestExpectFailure]) {
            is(result, NO,
               "Test IP id: %@. Expected %@. Got %d", testNum, ipExpectedResult, result);
        } else if ([ipExpectedResult isEqualToString:kSecTrustTestExpectSuccess]) {
            is(result, YES,
               "Test IP id: %@. Expected %@. Got %d", testNum, ipExpectedResult, result);
        } else if ([ipExpectedResult isEqualToString:kSecTrustTestExpectMaybeSuccess]) {
            /* These are "OK" but it's acceptable to reject them */
            pass();
        }
    }
};

static void cleanup(NSURL *tmpDir) {
    [[NSFileManager defaultManager] removeItemAtURL:tmpDir error:nil];
}

int si_87_sectrust_name_constraints(int argc, char *const *argv)
{
    NSArray *testsArray = getTestsArray();
	plan_tests(2 + (int)(2 * [testsArray count]));
	tests();

    if(untar_test_certs()) {
        [testsArray enumerateObjectsUsingBlock:runNameConstraintsTestForObject];
    }
    cleanup(tmpCertsDir);

	return 0;
}
