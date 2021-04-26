//
//  secstaticcode_integration.m
//  secsecstaticcodeapitest
//
//  Copyright 2021 Apple Inc. All rights reserved.
//
#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecStaticCode.h>

#import "secstaticcode.h"
#import "codesigning_tests_shared.h"

static void
RevokedBinaryTraversalTest(NSURL *contentRoot)
{
    NSDictionary<NSString *, NSNumber *> *gTestPaths = @{
        // This resource file has a bad signature that will fail validation, but not in a fatal way.
        @"traversal/KV-badsig.app": @(errSecSuccess),
        // These are all hiding revoked binaries in various places for different types of discovery.
        @"traversal/KV-badfile.app": @(CSSMERR_TP_CERT_REVOKED),
        @"traversal/KV-badlink.app": @(CSSMERR_TP_CERT_REVOKED),
        @"traversal/KV-badspot.app": @(CSSMERR_TP_CERT_REVOKED),
    };

    TEST_START("kSecCSEnforceRevocationChecks finds revoked binaries inside bundles");

    for (NSString *path in gTestPaths.allKeys) {
        SecStaticCodeRef codeRef = NULL;
        OSStatus status;

        NSNumber *expected = gTestPaths[path];
        INFO(@"Test case: %@, %@", path, expected);

        NSURL *url = [contentRoot URLByAppendingPathComponent:path];
        status = SecStaticCodeCreateWithPath((__bridge CFURLRef)url, kSecCSDefaultFlags, &codeRef);
        TEST_CASE_EXPR_JUMP(status == errSecSuccess, lb_next);

        status = SecStaticCodeCheckValidity(codeRef, kSecCSEnforceRevocationChecks, NULL);
        INFO(@"validation result: %d", status);
        TEST_CASE(status == expected.integerValue, "validation succeeds with expected result");

lb_next:
        if (codeRef) {
            CFRelease(codeRef);
        }
    }
    return;
}

int
run_integration_tests(const char *root)
{
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:root]];
    NSLog(@"Running integration test with content root: %@", url);

    RevokedBinaryTraversalTest(url);
    return 0;
}
