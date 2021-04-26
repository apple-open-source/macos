/*
* Copyright (c) 2006-2010,2012-2019 Apple Inc. All Rights Reserved.
*/

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Security/SecCertificatePriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include "OSX/utilities/array_size.h"
#include "OSX/utilities/SecCFWrappers.h"

#import "../TestMacroConversions.h"
#import "TrustEvaluationTestCase.h"

#import "ExceptionTests_data.h"

@interface TrustExceptionTests : TrustEvaluationTestCase
@end

@implementation TrustExceptionTests

static NSArray *certs = nil;
static NSDate *date = nil;

+ (void)setUp
{
    [super setUp];
    SecCertificateRef cert0 = SecCertificateCreateWithBytes(NULL, _exception_cert0, sizeof(_exception_cert0));
    SecCertificateRef cert1 = SecCertificateCreateWithBytes(NULL, _exception_cert1, sizeof(_exception_cert1));
    certs = @[ (__bridge id)cert0, (__bridge id)cert1 ];
    date = [NSDate dateWithTimeIntervalSinceReferenceDate:545000000.0]; /* April 9, 2018 at 1:53:20 PM PDT */

    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
}

#if !TARGET_OS_BRIDGE
// bridgeOS doesn't have a system root store
- (void)testPassingTrust
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("store.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    ok(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultUnspecified,
            "trust is kSecTrustResultUnspecified");
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}
#endif

- (void)testFailingTrust
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

- (void)testNewTrustObjectSameFailure
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    /* new trust with the same failing policy and certs should pass */
    CFReleaseNull(trust);
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

#if !TARGET_OS_BRIDGE
// bridgeOS always has an AnchorTrusted error due to lack of a system root store
- (void)testIntroduceNewAnchorTrustedFailure
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    // new AnchorTrusted failure
    ok_status(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)@[]), "set empty anchor list");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trust is kSecTrustResultRecoverableTrustFailure");

    // fix AnchorTrusted failure
    ok_status(SecTrustSetAnchorCertificatesOnly(trust, false), "trust passed in anchors and system anchors");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}
#endif

- (void)testIntroduceNewExpiredFailure
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    // new expiry failure
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)[NSDate dateWithTimeIntervalSinceReferenceDate:667680000.0]),
              "set date to far future so certs are expired");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trust is kSecTrustResultRecoverableTrustFailure");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

- (void)testNewTrustObjectNewHostnameFailure
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("store.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust with hostname mismatch");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    /* exceptions from the old trust evaluation should fail */
    SecTrustResultType trustResult;
    ok(SecTrustSetExceptions(trust, exceptions), "set old exceptions");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);

    /* we should be able to get new exceptions and pass */
    CFReleaseNull(exceptions);
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

- (void)testClearExceptions
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    XCTAssertFalse(SecTrustSetExceptions(trust, NULL));
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

- (void)testWrongCertForExceptions
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");

    SecTrustResultType trustResult;
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    ok_status(SecTrustGetTrustResult(trust, &trustResult));
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);
    CFDataRef exceptions;
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    /* new trust with the same failing policy and certs should pass */
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    SecCertificateRef sscert0 = SecCertificateCreateWithBytes(NULL, _exception_self_signed, sizeof(_exception_self_signed));
    policy = SecPolicyCreateSSL(false, CFSTR("self-signed.ssltest.apple.com"));
    ok_status(SecTrustCreateWithCertificates(sscert0, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");
    XCTAssertFalse(SecTrustSetExceptions(trust, exceptions), "set exceptions fails for other cert");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure);

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

- (void)testExtensionsEpoch
{
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult;
    CFDataRef exceptions = NULL;

    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("badstore.apple.com"));
    ok_status(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "set date");
    ok(exceptions = SecTrustCopyExceptions(trust), "create exceptions");

    /* Test the uninitialized extensions epoch. */
    CFErrorRef exceptionResetCountError = NULL;
    uint64_t exceptionResetCount = SecTrustGetExceptionResetCount(&exceptionResetCountError);
    ok(exceptionResetCount == 0, "exception reset count is uninitialized");
    CFReleaseNull(exceptionResetCountError);
    is(SecTrustGetExceptionResetCount(&exceptionResetCountError), exceptionResetCount, "SecTrustGetExceptionResetCount is idempotent");
    ok(SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultProceed, "trust is kSecTrustResultProceed");

    /* Test increasing the extensions epoch. */
    CFReleaseNull(exceptionResetCountError);
    ok_status(SecTrustIncrementExceptionResetCount(&exceptionResetCountError), "increase exception reset count");
    CFReleaseNull(exceptionResetCountError);
    is(SecTrustGetExceptionResetCount(&exceptionResetCountError), 1 + exceptionResetCount, "exception reset count is 1 + previous count");

    /* Test trust evaluation under a future extensions epoch. */
    ok(!SecTrustSetExceptions(trust, exceptions), "set exceptions");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trust is kSecTrustResultRecoverableTrustFailure");

    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(exceptions);
}

#if !TARGET_OS_BRIDGE
// bridgeOS doesn't support Valid
- (void)testFatalResultsNonOverride
{
    id root = [self SecCertificateCreateFromPEMResource:@"ca-ki" subdirectory:@"si-88-sectrust-valid-data"];
    id revokedLeaf = [self SecCertificateCreateFromPEMResource:@"leaf-ki-revoked1" subdirectory:@"si-88-sectrust-valid-data"];
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:@[revokedLeaf, root] policies:nil];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:542400000.0]]; // March 10, 2018 at 10:40:00 AM PST
    [eval setAnchors:@[root]];
    XCTAssertFalse([eval evaluate:nil]);
    XCTAssertEqual(eval.trustResult, kSecTrustResultFatalTrustFailure);

    /* try to set exceptions on the trust and ensure it still fails */
    NSData *exceptions = CFBridgingRelease(SecTrustCopyExceptions(eval.trust));
    XCTAssertNotNil(exceptions);
    XCTAssert(SecTrustSetExceptions(eval.trust, (__bridge CFDataRef)exceptions));
    XCTAssertFalse([eval evaluate:nil]);
    XCTAssertEqual(eval.trustResult, kSecTrustResultFatalTrustFailure);
}
#endif

@end
