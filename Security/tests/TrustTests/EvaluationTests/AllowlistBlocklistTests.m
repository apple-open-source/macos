/*
* Copyright (c) 2011-2019 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>
#import <XCTest/XCTest.h>
#import <Foundation/Foundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <utilities/SecCFRelease.h>
#include <utilities/SecCFWrappers.h>

#import "../TestMacroConversions.h"
#import "TrustEvaluationTestCase.h"

#import "AllowlistBlocklistTests_data.h"

@interface BlocklistTests :TrustEvaluationTestCase
@end

@implementation BlocklistTests

- (void)validate_one_cert:(uint8_t *)data length:(size_t)len chain_length:(int)chain_length trustResult:(SecTrustResultType)trust_result
{
    SecTrustRef trust = NULL;
    SecCertificateRef cert = NULL, root = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(false, NULL);
    CFArrayRef certs = NULL;

    isnt(cert = SecCertificateCreateWithBytes(NULL, data, len),
        NULL, "create cert");
    isnt(root = SecCertificateCreateWithBytes(NULL, UTNHardware_cer, sizeof(UTNHardware_cer)), NULL);
    certs = CFArrayCreate(NULL, (const void **)&cert, 1, NULL);
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust with single cert");
    ok_status(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)@[(__bridge id)root]));

    SecTrustResultType trustResult;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is(SecTrustGetCertificateCount(trust), chain_length, "cert count");
    is_status(trustResult, trust_result, "correct trustResult");
    CFRelease(trust);
    CFRelease(policy);
    CFRelease(certs);
    CFRelease(cert);
    CFReleaseNull(root);
}

- (void)testBlocklistedCerts
{
    [self validate_one_cert:Global_Trustee_cer length:sizeof(Global_Trustee_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:login_yahoo_com_1_cer length:sizeof(login_yahoo_com_1_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];

    /* this is the root, which isn't ok for ssl and fails here, but at the
       same time it proves that kSecTrustResultFatalTrustFailure isn't
       returned for policy failures that aren't blocklisting */
    [self validate_one_cert:login_yahoo_com_2_cer length:sizeof(login_yahoo_com_2_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:addons_mozilla_org_cer length:sizeof(addons_mozilla_org_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:login_yahoo_com_cer length:sizeof(login_yahoo_com_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:login_live_com_cer length:sizeof(login_live_com_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:mail_google_com_cer length:sizeof(mail_google_com_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:login_skype_com_cer length:sizeof(login_skype_com_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
    [self validate_one_cert:www_google_com_cer length:sizeof(www_google_com_cer) chain_length:2 trustResult:kSecTrustResultFatalTrustFailure];
}

- (void)testDigicertMalaysia {
    SecPolicyRef sslPolicy = SecPolicyCreateSSL(false, 0);
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));

    /* Run the tests. */
    [self runCertificateTestForDirectory:sslPolicy subDirectory:@"DigicertMalaysia" verifyDate:testDate];

    CFReleaseSafe(sslPolicy);
}

#if !TARGET_OS_BRIDGE
/* BridgeOS doesn't have Valid -- the other Blocklist tests happen to pass because the certs fail for other
 * reasons (no root store, missing EKU, disallowed hash or key size). */
- (void)testDigiNotar {
    SecPolicyRef sslPolicy = SecPolicyCreateSSL(false, 0);
    NSDate *testDate = CFBridgingRelease(CFDateCreateForGregorianZuluDay(NULL, 2011, 9, 1));

    /* Run the tests. */
    [self runCertificateTestForDirectory:sslPolicy subDirectory:@"DigiNotar" verifyDate:testDate];

    CFReleaseSafe(sslPolicy);
}
#endif // !TARGET_OS_BRIDGE

@end

@interface AllowlistTests : TrustEvaluationTestCase
@end

@implementation AllowlistTests

#if !TARGET_OS_BRIDGE
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

- (void)testLeafOnAllowList
{
    SecCertificateRef certs[3];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFArrayRef certArray = NULL;
    CFArrayRef anchorsArray = NULL;

    isnt(certs[0] = createCertFromStaticData(leafOnAllowList_Cert, sizeof(leafOnAllowList_Cert)),
         NULL, "allowlist: create leaf cert");
    isnt(certs[1] = createCertFromStaticData(ca1_Cert, sizeof(ca1_Cert)),
         NULL, "allowlist: create intermediate ca 1");
    isnt(certs[2] = createCertFromStaticData(root_Cert, sizeof(root_Cert)),
         NULL, "allowlist: create root");

    isnt(certArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&certs[0], 3, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create cert array");

    /* create a trust reference with ssl policy */
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "allowlist: create policy");
    ok_status(SecTrustCreateWithCertificates(certArray, policy, &trust), "allowlist: create trust");

    /* set evaluate date: January 6, 2020 at 2:40:00 AM PST */
    isnt(date = CFDateCreate(NULL, 600000000.0), NULL, "allowlist: create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "allowlist: set verify date");

    /* use a known root CA at this point in time to anchor the chain */
    isnt(anchorsArray = CFArrayCreate(NULL, (const void **)&certs[2], 1, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create anchors array");
    ok_status((anchorsArray) ? SecTrustSetAnchorCertificates(trust, anchorsArray) : errSecParam, "allowlist: set anchors");

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "allowlist: evaluate");

    /* expected result is kSecTrustResultUnspecified since cert is on allow list and its issuer chains to a trusted root */
    ok(trustResult == kSecTrustResultUnspecified, "trustResult 4 expected (got %d)",
       (int)trustResult);

    /* clean up */
    for(CFIndex idx=0; idx < 3; idx++) {
        if (certs[idx]) { CFRelease(certs[idx]); }
    }
    if (policy) { CFRelease(policy); }
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }
    if (certArray) { CFRelease(certArray); }
    if (anchorsArray) { CFRelease(anchorsArray); }
}

- (void)testLeafNotOnAllowList
{
    SecCertificateRef certs[3];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFArrayRef certArray = NULL;
    CFArrayRef anchorsArray = NULL;

    isnt(certs[0] = createCertFromStaticData(leafNotOnAllowList_Cert, sizeof(leafNotOnAllowList_Cert)),
         NULL, "!allowlist: create leaf cert");
    isnt(certs[1] = createCertFromStaticData(ca1_Cert, sizeof(ca1_Cert)),
         NULL, "!allowlist: create intermediate ca 1");
    isnt(certs[2] = createCertFromStaticData(root_Cert, sizeof(root_Cert)),
         NULL, "!allowlist: create root");

    isnt(certArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&certs[0], 3, &kCFTypeArrayCallBacks),
         NULL, "!allowlist: create cert array");

    /* create a trust reference with basic policy */
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "!allowlist: create policy");
    ok_status(SecTrustCreateWithCertificates(certArray, policy, &trust), "!allowlist: create trust");

    /* set evaluate date: January 6, 2020 at 2:40:00 AM PST */
    isnt(date = CFDateCreate(NULL, 600000000.0), NULL, "allowlist: create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "!allowlist: set verify date");

    /* use a known root CA at this point in time to anchor the chain */
    isnt(anchorsArray = CFArrayCreate(NULL, (const void **)&certs[2], 1, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create anchors array");
    ok_status((anchorsArray) ? SecTrustSetAnchorCertificates(trust, anchorsArray) : errSecParam, "!allowlist: set anchors");

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "!allowlist: evaluate");

    /* expected result is kSecTrustResultFatalTrustFailure, since cert is not on allow list */
    ok(trustResult == kSecTrustResultFatalTrustFailure,
       "trustResult 6 expected (got %d)", (int)trustResult);

    /* clean up */
    for(CFIndex idx=0; idx < 3; idx++) {
        if (certs[idx]) { CFRelease(certs[idx]); }
    }
    if (policy) { CFRelease(policy); }
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }
    if (certArray) { CFRelease(certArray); }
    if (anchorsArray) { CFRelease(anchorsArray); }
}

- (void)testAllowListForRootCA
{
    SecCertificateRef certs[3];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFArrayRef certArray = NULL;
    CFArrayRef anchorsArray = NULL;

    isnt(certs[0] = createCertFromStaticData(leaf_subCANotOnAllowlist_Cert, sizeof(leaf_subCANotOnAllowlist_Cert)),
         NULL, "!allowlist: create leaf cert");
    isnt(certs[1] = createCertFromStaticData(subCANotOnAllowlist_Cert, sizeof(subCANotOnAllowlist_Cert)),
         NULL, "!allowlist: create intermediate ca 1");
    isnt(certs[2] = createCertFromStaticData(root_Cert, sizeof(root_Cert)),
         NULL, "!allowlist: create root");

    isnt(certArray = CFArrayCreate(kCFAllocatorDefault, (const void **)&certs[0], 3, &kCFTypeArrayCallBacks),
         NULL, "!allowlist: create cert array");

    /* create a trust reference with basic policy */
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "!allowlist: create policy");
    ok_status(SecTrustCreateWithCertificates(certArray, policy, &trust), "!allowlist: create trust");

    /* set evaluate date: January 6, 2020 at 2:40:00 AM PST */
    isnt(date = CFDateCreate(NULL, 600000000.0), NULL, "allowlist: create date");
    ok_status((date) ? SecTrustSetVerifyDate(trust, date) : errSecParam, "!allowlist: set verify date");

    /* use a known root CA at this point in time to anchor the chain */
    isnt(anchorsArray = CFArrayCreate(NULL, (const void **)&certs[2], 1, &kCFTypeArrayCallBacks),
         NULL, "allowlist: create anchors array");
    ok_status((anchorsArray) ? SecTrustSetAnchorCertificates(trust, anchorsArray) : errSecParam, "!allowlist: set anchors");

    SecTrustResultType trustResult = kSecTrustResultInvalid;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "!allowlist: evaluate");

    /* expected result is kSecTrustResultRecoverableTrustFailure (if issuer is distrusted)
     or kSecTrustResultFatalTrustFailure (if issuer is revoked), since cert is not on allow list */
    ok(trustResult == kSecTrustResultFatalTrustFailure,
       "trustResult 6 expected (got %d)", (int)trustResult);

    /* clean up */
    for(CFIndex idx=0; idx < 3; idx++) {
        if (certs[idx]) { CFRelease(certs[idx]); }
    }
    if (policy) { CFRelease(policy); }
    if (trust) { CFRelease(trust); }
    if (date) { CFRelease(date); }
    if (certArray) { CFRelease(certArray); }
    if (anchorsArray) { CFRelease(anchorsArray); }
}

- (void)testDateBasedAllowListForRootCA
{
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
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    is(trustResult, kSecTrustResultUnspecified, "leaf issued before cutoff failed evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Leaf issued after cutoff should fail */
    certs = @[(__bridge id)afterLeaf, (__bridge id)beforeInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "leaf issued after cutoff succeeded evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Intermediate issued after cutoff should fail (even for leaf issued before) */
    certs = @[(__bridge id)beforeLeaf, (__bridge id)afterInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "intermediate issued after cutoff succeeded evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Intermediate issued after cutoff should fail */
    certs = @[(__bridge id)afterLeaf, (__bridge id)afterInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    is(trustResult, kSecTrustResultFatalTrustFailure, "intermediate issued before cutoff succeeded evaluation");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* Leaf issued before cutoff should choose acceptable path */
    certs = @[(__bridge id)beforeLeaf, (__bridge id) afterInt, (__bridge id)beforeInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    is(trustResult, kSecTrustResultUnspecified, "leaf issued before cutoff failed evaluation (multi-path)");
    CFReleaseNull(trust);
    trustResult = kSecTrustResultInvalid;

    /* No good path for leaf issued after cutoff */
    certs = @[(__bridge id)afterLeaf, (__bridge id)beforeInt, (__bridge id)afterInt];
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
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

- (void)testLeafOnAllowListOtherFailures
{
    SecCertificateRef certs[3];
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    NSArray *anchors = nil, *certArray = nil;
    NSDate *verifyDate = nil;
    SecTrustResultType trustResult = kSecTrustResultInvalid;

    memset(certs, 0, 3 * sizeof(SecCertificateRef));

    require(certs[0] = SecCertificateCreateWithBytes(NULL, leafOnAllowList_Cert, sizeof(leafOnAllowList_Cert)), out);
    require(certs[1] = SecCertificateCreateWithBytes(NULL, ca1_Cert, sizeof(ca1_Cert)), out);
    require(certs[2] = SecCertificateCreateWithBytes(NULL, root_Cert, sizeof(root_Cert)), out);

    anchors = @[(__bridge id)certs[2]];
    certArray = @[(__bridge id)certs[0], (__bridge id)certs[1], (__bridge id)certs[2]];
    verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:600000000.0]; // January 6, 2020 at 2:40:00 AM PST

    /* Mismatched policy, should fail */
    require(policy = SecPolicyCreateSSL(true, (__bridge CFStringRef)@"example.com"), out);
    require_noerr(SecTrustCreateWithCertificates((__bridge CFArrayRef)certArray, policy, &trust), out);
    require_noerr(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), out);
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    ok(trustResult == kSecTrustResultRecoverableTrustFailure || trustResult == kSecTrustResultFatalTrustFailure,
       "hostname failure with cert on allow list succeeded evaluation");
    CFReleaseNull(policy);
    trustResult = kSecTrustResultInvalid;

    /* Expired, should fail */
    verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0]; // November 4, 2016 at 5:53:20 PM PDT
    require_noerr(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)verifyDate), out);
    require_noerr(SecTrustSetPolicies(trust, policy), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    ok(trustResult == kSecTrustResultRecoverableTrustFailure || trustResult == kSecTrustResultFatalTrustFailure,
       "EKU failure with cert on allow list succeeded evaluation");
    CFReleaseNull(policy);
    trustResult = kSecTrustResultInvalid;

    /* Apple pinning policy, should fail */
    require(policy = SecPolicyCreateAppleSSLPinned((__bridge CFStringRef)@"aPolicy",
                                                   (__bridge CFStringRef)@"example.com", NULL,
                                                   (__bridge CFStringRef)@"1.2.840.113635.100.6.27.12"), out);
    require_noerr(SecTrustSetPolicies(trust, policy), out);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), out);
    ok(trustResult == kSecTrustResultRecoverableTrustFailure || trustResult == kSecTrustResultFatalTrustFailure,
       "Apple pinning policy with cert on allow list succeeded evaluation");

out:
    CFReleaseNull(certs[0]);
    CFReleaseNull(certs[1]);
    CFReleaseNull(certs[2]);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}
#else /* TARGET_OS_BRIDGE */
/* Allowlists are provided by Valid, which is not supported on bridgeOS */
- (void)testSkipTests
{
    XCTAssert(true);
}
#endif

@end
