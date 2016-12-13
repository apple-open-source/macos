/*
 *  si-84-sectrust-allowlist.c
 *  Security
 *
 * Copyright (c) 2015-2016 Apple Inc. All Rights Reserved.
 */

#include <AssertMacros.h>
#import <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <utilities/SecCFRelease.h>
#include <AssertMacros.h>

#include "shared_regressions.h"

#include "si-84-sectrust-allowlist/cnnic_certs.h"
#include "si-84-sectrust-allowlist/wosign_certs.h"
#include "si-84-sectrust-allowlist/date_testing_certs.h"


static SecCertificateRef createCertFromStaticData(const UInt8 *certData, CFIndex certLength)
{
    SecCertificateRef cert = NULL;
    CFDataRef data = CFDataCreateWithBytesNoCopy(NULL, certData, certLength, kCFAllocatorNull);
    if (data) {
        cert = SecCertificateCreateWithData(NULL, data);
        CFRelease(data);
    }
    return cert;
}

static void TestLeafOnAllowList()
{
    SecCertificateRef certs[4];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFArrayRef certArray = NULL;
    CFArrayRef anchorsArray = NULL;

    isnt(certs[0] = createCertFromStaticData(leafOnAllowList_Cert, sizeof(leafOnAllowList_Cert)),
         NULL, "allowlist: create leaf cert");
    isnt(certs[1] = createCertFromStaticData(ca1_Cert, sizeof(ca1_Cert)),
         NULL, "allowlist: create intermediate ca 1");
    isnt(certs[2] = createCertFromStaticData(ca2_Cert, sizeof(ca2_Cert)),
         NULL, "allowlist: create intermediate ca 2");
    isnt(certs[3] = createCertFromStaticData(root_Cert, sizeof(root_Cert)),
         NULL, "allowlist: create root");

    isnt(certArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&certs[0], 4, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create cert array");

    /* create a trust reference with basic policy */
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "allowlist: create policy");
    ok_status(SecTrustCreateWithCertificates(certArray, policy, &trust), "allowlist: create trust");

    /* set evaluate date: September 12, 2016 at 1:30:00 PM PDT */
    isnt(date = CFDateCreate(NULL, 495405000.0), NULL, "allowlist: create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "allowlist: set verify date");

    /* use a known root CA at this point in time to anchor the chain */
    isnt(anchorsArray = CFArrayCreate(NULL, (const void **)&certs[3], 1, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create anchors array");
    ok_status((anchorsArray) ? SecTrustSetAnchorCertificates(trust, anchorsArray) : errSecParam, "allowlist: set anchors");

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    ok_status(SecTrustEvaluate(trust, &trustResult), "allowlist: evaluate");

    /* expected result is kSecTrustResultUnspecified since cert is on allow list and its issuer chains to a trusted root */
    ok(trustResult == kSecTrustResultUnspecified, "trustResult 4 expected (got %d)",
       (int)trustResult);

    /* clean up */
    for(CFIndex idx=0; idx < 4; idx++) {
        if (certs[idx]) { CFRelease(certs[idx]); }
    }
    if (policy) { CFRelease(policy); }
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }
    if (certArray) { CFRelease(certArray); }
    if (anchorsArray) { CFRelease(anchorsArray); }
}

static void TestLeafNotOnAllowList()
{
    SecCertificateRef certs[4];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFArrayRef certArray = NULL;
    CFArrayRef anchorsArray = NULL;

    isnt(certs[0] = createCertFromStaticData(leafNotOnAllowList_Cert, sizeof(leafNotOnAllowList_Cert)),
         NULL, "!allowlist: create leaf cert");
    isnt(certs[1] = createCertFromStaticData(ca1_Cert, sizeof(ca1_Cert)),
         NULL, "!allowlist: create intermediate ca 1");
    isnt(certs[2] = createCertFromStaticData(ca2_Cert, sizeof(ca2_Cert)),
         NULL, "!allowlist: create intermediate ca 2");
    isnt(certs[3] = createCertFromStaticData(root_Cert, sizeof(root_Cert)),
         NULL, "!allowlist: create root");

    isnt(certArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&certs[0], 4, &kCFTypeArrayCallBacks),
         NULL, "!allowlist: create cert array");

    /* create a trust reference with basic policy */
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "!allowlist: create policy");
    ok_status(SecTrustCreateWithCertificates(certArray, policy, &trust), "!allowlist: create trust");

    /* set evaluate date: September 7, 2016 at 9:00:00 PM PDT */
    isnt(date = CFDateCreate(NULL, 495000000.0), NULL, "!allowlist: create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "!allowlist: set verify date");

    /* use a known root CA at this point in time to anchor the chain */
    isnt(anchorsArray = CFArrayCreate(NULL, (const void **)&certs[3], 1, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create anchors array");
    ok_status((anchorsArray) ? SecTrustSetAnchorCertificates(trust, anchorsArray) : errSecParam, "!allowlist: set anchors");

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    ok_status(SecTrustEvaluate(trust, &trustResult), "!allowlist: evaluate");

    /* expected result is kSecTrustResultRecoverableTrustFailure (if issuer is distrusted)
     or kSecTrustResultFatalTrustFailure (if issuer is revoked), since cert is not on allow list */
    ok(trustResult == kSecTrustResultRecoverableTrustFailure ||
       trustResult == kSecTrustResultFatalTrustFailure,
       "trustResult 5 or 6 expected (got %d)", (int)trustResult);

    /* clean up */
    for(CFIndex idx=0; idx < 4; idx++) {
        if (certs[idx]) { CFRelease(certs[idx]); }
    }
    if (policy) { CFRelease(policy); }
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }
    if (certArray) { CFRelease(certArray); }
    if (anchorsArray) { CFRelease(anchorsArray); }
}

static void TestAllowListForRootCA(void)
{
    SecCertificateRef test0[2] = {NULL,NULL};
    SecCertificateRef test1[2] = {NULL,NULL};
    SecCertificateRef test1e[2] = {NULL,NULL};
    SecCertificateRef test2[2] = {NULL,NULL};
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    SecTrustResultType trustResult;

    isnt(test0[0] = createCertFromStaticData(cert0, sizeof(cert0)),
            NULL, "create first leaf");
    isnt(test1[0] = createCertFromStaticData(cert1, sizeof(cert1)),
         NULL, "create second leaf");
    isnt(test1e[0] = createCertFromStaticData(cert1_expired, sizeof(cert1_expired)),
         NULL, "create second leaf (expired)");
    isnt(test2[0] = createCertFromStaticData(cert2, sizeof(cert2)),
         NULL, "create third leaf");

    isnt(test0[1] = createCertFromStaticData(intermediate0, sizeof(intermediate0)),
         NULL, "create intermediate");
    isnt(test1[1] = createCertFromStaticData(intermediate1, sizeof(intermediate1)),
         NULL, "create intermediate");
    isnt(test1e[1] = createCertFromStaticData(intermediate1, sizeof(intermediate1)),
         NULL, "create intermediate");
    isnt(test2[1] = createCertFromStaticData(intermediate2, sizeof(intermediate2)),
         NULL, "create intermediate");

    CFArrayRef certs0 = CFArrayCreate(kCFAllocatorDefault, (const void **)test0, 2, &kCFTypeArrayCallBacks);
    CFArrayRef certs1 = CFArrayCreate(kCFAllocatorDefault, (const void **)test1, 2, &kCFTypeArrayCallBacks);
    CFArrayRef certs1e = CFArrayCreate(kCFAllocatorDefault, (const void **)test1e, 2, &kCFTypeArrayCallBacks);
    CFArrayRef certs2 = CFArrayCreate(kCFAllocatorDefault, (const void **)test2, 2, &kCFTypeArrayCallBacks);

    /*
     * Whitelisted certificates issued by untrusted root CA.
     */
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "create policy");
    ok_status(SecTrustCreateWithCertificates(certs0, policy, &trust), "create trust");
    /* set evaluate date within validity range: September 12, 2016 at 1:30:00 PM PDT */
    isnt(date = CFDateCreate(NULL, 495405000.0), NULL, "create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "set verify date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultUnspecified, "trustResult 4 expected (got %d)",
       (int)trustResult);
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }

    ok_status(SecTrustCreateWithCertificates(certs1, policy, &trust), "create trust");
    /* set evaluate date within validity range: September 12, 2016 at 1:30:00 PM PDT */
    isnt(date = CFDateCreate(NULL, 495405000.0), NULL, "create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "set verify date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultUnspecified, "trustResult 4 expected (got %d)",
       (int)trustResult);
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }

    ok_status(SecTrustCreateWithCertificates(certs2, policy, &trust), "create trust");
    /* set evaluate date within validity range: September 12, 2016 at 1:30:00 PM PDT */
    isnt(date = CFDateCreate(NULL, 495405000.0), NULL, "create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "set verify date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultUnspecified, "trustResult 4 expected (got %d)",
       (int)trustResult);
    /*
     * Same certificate, on allow list but past expiration. Expect to fail.
     */
    if (date) { CFRelease(date); }
    isnt(date = CFDateCreate(NULL, 667680000.0), NULL, "create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "set date to far future so certs are expired");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultRecoverableTrustFailure, "trustResult 5 expected (got %d)",
       (int)trustResult);
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }

    /*
     * Expired certificate not on allow list. Expect to fail.
     */
    ok_status(SecTrustCreateWithCertificates(certs1e, policy, &trust), "create trust");
    /* set evaluate date within validity range: September 12, 2016 at 1:30:00 PM PDT */
    isnt(date = CFDateCreate(NULL, 495405000.0), NULL, "create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "set verify date");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    ok(trustResult == kSecTrustResultRecoverableTrustFailure, "trustResult 5 expected (got %d)",
       (int)trustResult);
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }


    /* Clean up. */
    if (policy) { CFRelease(policy); }
    if (certs0) { CFRelease(certs0); }
    if (certs1) { CFRelease(certs1); }
    if (certs1e) { CFRelease(certs1e); }
    if (certs2) { CFRelease(certs2); }

    if (test0[0]) { CFRelease(test0[0]); }
    if (test0[1]) { CFRelease(test0[1]); }
    if (test1[0]) { CFRelease(test1[0]); }
    if (test1[1]) { CFRelease(test1[1]); }
    if (test1e[0]) { CFRelease(test1e[0]); }
    if (test1e[1]) { CFRelease(test1e[1]); }
    if (test2[0]) { CFRelease(test2[0]); }
    if (test2[1]) { CFRelease(test2[1]); }
}

static void TestDateBasedAllowListForRootCA(void) {
    SecCertificateRef root = NULL, beforeInt = NULL, afterInt = NULL,
    beforeLeaf = NULL, afterLeaf = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    NSArray *anchors = nil, *certs = nil;
    NSDate *verifyDate = nil;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    require(root = SecCertificateCreateWithBytes(NULL, _datetest_root, sizeof(_datetest_root)), out);
    require(beforeInt = SecCertificateCreateWithBytes(NULL, _datetest_before_int, sizeof(_datetest_before_int)), out);
    require(afterInt = SecCertificateCreateWithBytes(NULL, _datetest_after_int, sizeof(_datetest_after_int)), out);
    require(beforeLeaf = SecCertificateCreateWithBytes(NULL, _datetest_before_leaf, sizeof(_datetest_before_leaf)), out);
    require(afterLeaf = SecCertificateCreateWithBytes(NULL, _datetest_after_leaf, sizeof(_datetest_after_leaf)), out);

    anchors = @[(__bridge id)root];
    require(policy = SecPolicyCreateSSL(true, CFSTR("testserver.apple.com")), out);
    verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:504000000.0];  /* 21 Dec 2016 */

    /* Leaf issued before cutoff should pass */
    certs = @[(__bridge id)beforeLeaf, (__bridge id)beforeInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultUnspecified, "leaf issued before cutoff failed evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Leaf issued after cutoff should fail */
    certs = @[(__bridge id)afterLeaf, (__bridge id)beforeInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "leaf issued after cutoff succeeded evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Intermediate issued after cutoff should fail (even for leaf issued before) */
    certs = @[(__bridge id)beforeLeaf, (__bridge id)afterInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "intermediate issued after cutoff succeeded evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Intermediate issued after cutoff should fail */
    certs = @[(__bridge id)afterLeaf, (__bridge id)afterInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "intermediate issued before cutoff succeeded evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Leaf issued before cutoff should choose acceptable path */
    certs = @[(__bridge id)beforeLeaf, (__bridge id) afterInt, (__bridge id)beforeInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultUnspecified, "leaf issued before cutoff failed evaluation (multi-path)");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* No good path for leaf issued after cutoff */
    certs = @[(__bridge id)afterLeaf, (__bridge id)beforeInt, (__bridge id)afterInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "leaf issued after cutoff succeeded evaluation (multi-path)");

out:
    CFReleaseNull(root);
    CFReleaseNull(beforeInt);
    CFReleaseNull(afterInt);
    CFReleaseNull(beforeLeaf);
    CFReleaseNull(afterLeaf);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void TestLeafOnAllowListOtherFailures(void)
{
    SecCertificateRef certs[4];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    NSArray *anchors = nil, *certArray = nil;
    NSDate *verifyDate = nil;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    memset(certs, 0, 4 * sizeof(SecCertificateRef));

    require(certs[0] = SecCertificateCreateWithBytes(NULL, leafOnAllowList_Cert, sizeof(leafOnAllowList_Cert)), out);
    require(certs[1] = SecCertificateCreateWithBytes(NULL, ca1_Cert, sizeof(ca1_Cert)), out);
    require(certs[2] = SecCertificateCreateWithBytes(NULL, ca2_Cert, sizeof(ca2_Cert)), out);
    require(certs[3] = SecCertificateCreateWithBytes(NULL, root_Cert, sizeof(root_Cert)), out);

    anchors = @[(__bridge id)certs[3]];
    certArray = @[(__bridge id)certs[0], (__bridge id)certs[1], (__bridge id)certs[2], (__bridge id)certs[3]];
    verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:495405000.0];

    /* Mismatched hostname, should fail */
    require(policy = SecPolicyCreateSSL(true, (__bridge CFStringRef)@"wrong.hostname.com"), out);
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certArray, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultRecoverableTrustFailure, "hostname failure with cert on allow list succeeded evaluation");
    CFReleaseNull(policy);
    trustResult = kSecTrustResultInvalid;

    /* Wrong EKU, should fail */
    require(policy = SecPolicyCreateCodeSigning(), out);
    require_noerr(SecTrustSetPolicies(trust, policy), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultRecoverableTrustFailure, "EKU failure with cert on allow list succeeded evaluation");
    CFReleaseNull(policy);
    trustResult = kSecTrustResultInvalid;

    /* Apple pinning policy, should fail */
    require(policy = SecPolicyCreateAppleSSLPinned((__bridge CFStringRef)@"aPolicy",
                                                   (__bridge CFStringRef)@"telegram.im", NULL,
                                                   (__bridge CFStringRef)@"1.2.840.113635.100.6.27.12"), out);
    require_noerr(SecTrustSetPolicies(trust, policy), out);
    require_noerr(SecTrustEvaluate(trust, &trustResult), out);
    is(trustResult, kSecTrustResultRecoverableTrustFailure, "Apple pinning policy with cert on allow list succeeded evaluation");

    out:
    CFReleaseNull(certs[0]);
    CFReleaseNull(certs[1]);
    CFReleaseNull(certs[2]);
    CFReleaseNull(certs[3]);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

static void tests(void)
{
    TestAllowListForRootCA();
    TestLeafOnAllowList();
    TestLeafNotOnAllowList();
    TestDateBasedAllowListForRootCA();
    TestLeafOnAllowListOtherFailures();
}

int si_84_sectrust_allowlist(int argc, char *const *argv)
{
    plan_tests(68);
    tests();

    return 0;
}
