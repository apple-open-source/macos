/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <utilities/SecCFRelease.h>

#include "shared_regressions.h"

#include "si-97-sectrust-path-scoring.h"

static SecCertificateRef leaf = NULL;
static SecCertificateRef intSHA2 = NULL;
static SecCertificateRef intSHA1 = NULL;
static SecCertificateRef int1024 = NULL;
static SecCertificateRef rootSHA2 = NULL;
static SecCertificateRef rootSHA1 = NULL;
static SecCertificateRef root1024 = NULL;
static SecCertificateRef crossSHA2_SHA1 = NULL;
static SecCertificateRef crossSHA2_SHA2 = NULL;
static SecCertificateRef rootSHA2_2 = NULL;
static SecPolicyRef basicPolicy = NULL;
static SecPolicyRef sslPolicy = NULL;
static NSDate *verifyDate1 = nil;
static NSDate *verifyDate2 = nil;

static void setup_globals(void) {
    leaf = SecCertificateCreateWithBytes(NULL, _pathScoringLeaf, sizeof(_pathScoringLeaf));
    intSHA2 = SecCertificateCreateWithBytes(NULL, _pathScoringIntSHA2, sizeof(_pathScoringIntSHA2));
    intSHA1 = SecCertificateCreateWithBytes(NULL, _pathScoringIntSHA1, sizeof(_pathScoringIntSHA1));
    int1024 = SecCertificateCreateWithBytes(NULL, _pathScoringInt1024, sizeof(_pathScoringInt1024));
    rootSHA2 = SecCertificateCreateWithBytes(NULL, _pathScoringSHA2Root, sizeof(_pathScoringSHA2Root));
    rootSHA1 = SecCertificateCreateWithBytes(NULL, _pathScoringSHA1Root, sizeof(_pathScoringSHA1Root));
    root1024 = SecCertificateCreateWithBytes(NULL, _pathScoring1024Root, sizeof(_pathScoring1024Root));
    crossSHA2_SHA1 = SecCertificateCreateWithBytes(NULL, _pathScoringSHA2CrossSHA1, sizeof(_pathScoringSHA2CrossSHA1));
    crossSHA2_SHA2 = SecCertificateCreateWithBytes(NULL, _pathScoringSHA2CrossSHA2, sizeof(_pathScoringSHA2CrossSHA2));
    rootSHA2_2 = SecCertificateCreateWithBytes(NULL, _pathScoringSHA2Root2, sizeof(_pathScoringSHA2Root2));

    basicPolicy = SecPolicyCreateBasicX509();
    sslPolicy = SecPolicyCreateSSL(true, NULL);

    // May 1, 2016 at 5:53:20 AM PDT
    verifyDate1 = [NSDate dateWithTimeIntervalSinceReferenceDate:483800000.0];
    // May 27, 2016 at 4:20:50 PM PDT
    verifyDate2 = [NSDate dateWithTimeIntervalSinceReferenceDate:486084050.0];
}

static void cleanup_globals(void) {
    CFReleaseNull(leaf);
    CFReleaseNull(intSHA2);
    CFReleaseNull(intSHA1);
    CFReleaseNull(int1024);
    CFReleaseNull(rootSHA2);
    CFReleaseNull(rootSHA1);
    CFReleaseNull(root1024);
    CFReleaseNull(crossSHA2_SHA1);
    CFReleaseNull(crossSHA2_SHA2);
    CFReleaseNull(rootSHA2_2);

    CFReleaseNull(basicPolicy);
    CFReleaseNull(sslPolicy);
}

static bool testTrust(NSArray *certs, NSArray *anchors, SecPolicyRef policy,
                          NSDate *verifyDate, SecTrustResultType expectedResult,
                          NSArray *expectedChain) {
    bool testPassed = false;
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult = kSecTrustResultInvalid;
    require_noerr_string(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs,
                                                        policy,
                                                        &trust),
                         errOut, "failed to create trust ref");
    if (anchors) {
        require_noerr_string(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors),
                             errOut, "failed to set anchors");
    }
    require_noerr_string(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate),
                         errOut, "failed to set verify date");
    require_noerr_string(SecTrustEvaluate(trust, &trustResult),
                         errOut, "failed to evaluate trust");

    /* check result */
    if (expectedResult == kSecTrustResultUnspecified) {
        require_string(trustResult == expectedResult,
                       errOut, "unexpected untrusted chain");
    } else if (expectedResult == kSecTrustResultRecoverableTrustFailure) {
        require_string(trustResult == expectedResult,
                       errOut, "unexpected trusted chain");
    }

    /* check the chain that returned */
    require_string((NSUInteger)SecTrustGetCertificateCount(trust) == [expectedChain count],
                   errOut, "wrong number of certs in result chain");
    NSUInteger ix, count = [expectedChain count];
    for (ix = 0; ix < count; ix++) {
        require_string(CFEqual(SecTrustGetCertificateAtIndex(trust, ix),
                               (__bridge SecCertificateRef)[expectedChain objectAtIndex:ix]),
                       errOut, "chain didn't match expected");
    }
    testPassed = true;

errOut:
    CFReleaseNull(trust);
    return testPassed;
}

/* Path Scoring Hierarchy
 *                                         leaf
 *                           ^               ^         ^
 *                          /                |          \
 *               intSHA2                    intSHA1     int1024
 *           ^      ^      ^                 ^          ^
 *          /       |       \                |          |
 *  rootSHA2 crossSHA2_SHA1 crossSHA2_SHA2  rootSHA1    root1024
 *                  ^               ^
 *                  |               |
 *              rootSHA1      rootSHA2_2
 */

static void tests(SecPolicyRef policy) {
    NSArray *certs = nil;
    NSArray *anchors = nil;
    NSArray *chain = nil;
    SecTrustResultType expectedTrustResult = ((policy == basicPolicy) ? kSecTrustResultUnspecified :
                                              kSecTrustResultRecoverableTrustFailure);

    /* Choose a short chain over a long chain, when ending in a self-signed cert */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)crossSHA2_SHA2];
    anchors = @[(__bridge id)rootSHA2, (__bridge id)rootSHA2_2];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)rootSHA2];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose shorter chain over longer chain, SHA-2",
       (policy == basicPolicy) ? "accept" : "reject");

    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)intSHA1, (__bridge id)crossSHA2_SHA1];
    anchors = @[(__bridge id)rootSHA1];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA1, (__bridge id)rootSHA1];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose shorter chain over longer chain, SHA-1",
       (policy == basicPolicy) ? "accept" : "reject");

    /* Choose a SHA-2 chain over a SHA-1 chain */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)intSHA1];
    anchors = @[(__bridge id)rootSHA1, (__bridge id)rootSHA2];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)rootSHA2];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose SHA-2 chain over SHA-1 chain, order 1",
       (policy == basicPolicy) ? "accept" : "reject");

    certs = @[(__bridge id)leaf, (__bridge id)intSHA1, (__bridge id)intSHA2];
    anchors = @[(__bridge id)rootSHA2, (__bridge id)rootSHA1];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose SHA-2 chain over SHA-1 chain, order 2",
       (policy == basicPolicy) ? "accept" : "reject");

    /* Choose a longer SHA-2 chain over the shorter SHA-1 chain */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA1, (__bridge id)intSHA2, (__bridge id)crossSHA2_SHA2];
    anchors = @[(__bridge id)rootSHA1, (__bridge id)rootSHA2_2];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)crossSHA2_SHA2, (__bridge id)rootSHA2_2];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose longer SHA-2 chain over shorter SHA-1 chain",
       (policy == basicPolicy) ? "accept" : "reject");

    /* Choose 1024-bit temporally valid chain over 2048-bit invalid chain */
    certs = @[(__bridge id)leaf, (__bridge id)int1024, (__bridge id)intSHA1];
    anchors = @[(__bridge id)root1024, (__bridge id)rootSHA1];
    chain = @[(__bridge id)leaf, (__bridge id)int1024, (__bridge id)root1024];
    ok(testTrust(certs, anchors, policy, verifyDate2, expectedTrustResult, chain),
       "%s test: choose temporally valid chain over invalid chain",
       (policy == basicPolicy) ? "accept" : "reject");

    /* Choose an anchored chain over an unanchored chain */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)intSHA1, (__bridge id)rootSHA2];
    anchors = @[(__bridge id)rootSHA1];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA1, (__bridge id)rootSHA1];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose an anchored chain over an unanchored chain",
       (policy == basicPolicy) ? "accept" : "reject");

    /* Choose an anchored SHA-1 chain over an unanchored SHA-2 chain */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)intSHA1, (__bridge id)rootSHA2];
    anchors = @[(__bridge id)rootSHA1];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA1, (__bridge id)rootSHA1];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose anchored SHA-1 chain over unanchored SHA-2 chain",
       (policy == basicPolicy) ? "accept" : "reject");

    /* Choose an anchored SHA-1 cross-signed chain over unanchored SHA-2 chains */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)rootSHA2,
              (__bridge id)crossSHA2_SHA1, (__bridge id)crossSHA2_SHA2, (__bridge id)rootSHA2_2];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)crossSHA2_SHA1, (__bridge id)rootSHA1];
    ok(testTrust(certs, anchors, policy, verifyDate1, expectedTrustResult, chain),
       "%s test: choose anchored cross-signed chain over unanchored chains",
       (policy == basicPolicy) ? "accept" : "reject");
}

static void accept_tests(void) {
    tests(basicPolicy);

}

static void reject_tests(void) {
    /* The leaf certificate is a client SSL certificate, and will fail the sslPolicy. */
    tests(sslPolicy);

    /* reject only tests */
    NSArray *certs = nil;
    NSArray *anchors = nil;
    NSArray *chain = nil;

    /* Choose a 2048-bit chain over a 1024-bit chain */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)int1024];
    anchors = @[(__bridge id)rootSHA2, (__bridge id)root1024];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)rootSHA2];
    ok(testTrust(certs, anchors, sslPolicy, verifyDate1, kSecTrustResultRecoverableTrustFailure, chain),
       "reject test: choose 2048-bit chain over 1024-bit chain, order 1");

    certs = @[(__bridge id)leaf, (__bridge id)int1024, (__bridge id)intSHA2];
    anchors = @[(__bridge id)root1024, (__bridge id)rootSHA2];
    ok(testTrust(certs, anchors, sslPolicy, verifyDate1, kSecTrustResultRecoverableTrustFailure, chain),
       "reject test: choose 2048-bit chain over 1024-bit chain, order 2");

    /* Choose a complete chain over an incomplete chain */
    certs = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)intSHA1, (__bridge id)rootSHA1];
    anchors = @[];
    chain = @[(__bridge id)leaf, (__bridge id)intSHA1, (__bridge id)rootSHA1];
    ok(testTrust(certs, anchors, sslPolicy, verifyDate1, kSecTrustResultRecoverableTrustFailure, chain),
       "reject test: choose a chain that ends in a self-signed cert over one that doesn't");

    /* Choose a long chain over a short chain when not ending with a self-signed cert */
    certs = @[(__bridge id)leaf, (__bridge id)crossSHA2_SHA2, (__bridge id)intSHA2];
    anchors = nil;
    chain = @[(__bridge id)leaf, (__bridge id)intSHA2, (__bridge id)crossSHA2_SHA2];
    ok(testTrust(certs, anchors, sslPolicy, verifyDate1, kSecTrustResultRecoverableTrustFailure, chain),
       "reject test: choose longer chain over shorter chain, no roots");
}

int si_97_sectrust_path_scoring(int argc, char *const *argv)
{
    plan_tests(2*9 + 4);

    @autoreleasepool {
        setup_globals();
        accept_tests();
        reject_tests();
        cleanup_globals();
    }

    return 0;
}
