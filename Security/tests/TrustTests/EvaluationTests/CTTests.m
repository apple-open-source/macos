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
#import <XCTest/XCTest.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include "OSX/utilities/SecCFWrappers.h"
#include <Security/SecTrustSettings.h>
#include <Security/SecTrustSettingsPriv.h>
#include <Security/SecFramework.h>
#include "trust/trustd/OTATrustUtilities.h"

#if !TARGET_OS_BRIDGE
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"
#import <OCMock/OCMock.h>
#pragma clang diagnostic pop
#endif

#if TARGET_OS_IPHONE
#include <Security/SecTrustStore.h>
#else
#include <Security/SecKeychain.h>
#endif

#if !TARGET_OS_BRIDGE
#import <MobileAsset/MAAsset.h>
#import <MobileAsset/MAAssetQuery.h>
#endif

#import "TrustEvaluationTestCase.h"
#import "CTTests_data.h"
#include "../TestMacroConversions.h"

@interface CTTests : TrustEvaluationTestCase

@end

@implementation CTTests

+ (id) CF_RETURNS_RETAINED SecCertificateCreateFromResource:(NSString *)name {
    NSURL *url = [[NSBundle bundleForClass:[self class]] URLForResource:name withExtension:@".cer" subdirectory:@"si-82-sectrust-ct-data"];
    NSData *certData = [NSData dataWithContentsOfURL:url];
    SecCertificateRef cert = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData);
    return (__bridge id)cert;
}

+ (NSData *)DataFromResource:(NSString *)name {
    NSURL *url = [[NSBundle bundleForClass:[self class]] URLForResource:name withExtension:@".bin" subdirectory:@"si-82-sectrust-ct-data"];
    return [NSData dataWithContentsOfURL:url];
}

- (NSDictionary *)eval_ct_trust:(NSArray *)certs
                           sCTs:(NSArray *)scts
                  ocspResponses:(NSArray *)ocsps
                        anchors:(NSArray *)anchors
                  trustedCTLogs:(NSArray *)trustedLogs
                       hostname:(NSString *)hostname
                     verifyDate:(NSDate *)date {

    /* Create the trust object */
    TestTrustEvaluation *trust = nil;
    SecPolicyRef policy = SecPolicyCreateBasicX509(); // <rdar://problem/50066309> Need to generate new certs for CTTests
    XCTAssertNotNil(trust = [[TestTrustEvaluation alloc] initWithCertificates:certs policies:@[(__bridge id)policy]],
                    "create trust failed");
    CFReleaseNull(policy);
    if (!trust) { return nil; }

    /* Set the optional properties */
    if (scts) { trust.presentedSCTs = scts; }
    if (ocsps) { trust.ocspResponses = ocsps; }
    if (anchors) { trust.anchors = anchors; }
    if (trustedLogs) { trust.trustedCTLogs = trustedLogs; }
    if (date) { trust.verifyDate = date; }

    /* Evaluate */
    NSError *error = nil;
    XCTAssert([trust evaluate:&error], "failed trust evaluation: %@", error);
    return trust.resultDictionary;
}

static NSArray *anchors = nil;
static NSDate *date_20150307 = nil;
static NSDate *date_20160422 = nil;
static NSArray *trustedCTLogs = nil;

+ (void)setUp {
    [super setUp];
    SecCertificateRef anchor1 = (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"CA_alpha"];
    SecCertificateRef anchor2 =  (__bridge SecCertificateRef)[self SecCertificateCreateFromResource:@"CA_beta"];
    anchors = @[ (__bridge id)anchor1, (__bridge id)anchor2];
    CFReleaseNull(anchor1);
    CFReleaseNull(anchor2);
    date_20150307 = [NSDate dateWithTimeIntervalSinceReferenceDate:447450000.0]; // March 7, 2015 at 11:40:00 AM PST
    date_20160422 = [NSDate dateWithTimeIntervalSinceReferenceDate:483050000.0]; // April 22, 2016 at 1:33:20 PM PDT

    NSURL *trustedLogsURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"CTlogs"
                                                                     withExtension:@"plist"
                                                                      subdirectory:@"si-82-sectrust-ct-data"];
    trustedCTLogs = [NSArray arrayWithContentsOfURL:trustedLogsURL];

#if !TARGET_OS_BRIDGE
    /* Mock a successful mobile asset check-in so that we enforce CT */
    UpdateOTACheckInDate();
#endif
}

- (void)testOneEmbeddedSCT {
    NSDictionary *results = nil;
    SecCertificateRef certF = nil;
    isnt(certF = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverF"], NULL, "create certF");
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certF] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(certF);
}

- (void)testOnePresentedSCT {
    NSDictionary *results = nil;
    SecCertificateRef certD = nil;
    NSData *proofD = nil;
    isnt(certD = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverD"], NULL, "create certD");
    XCTAssertNotNil(proofD = [CTTests DataFromResource:@"serverD_proof"], "create proofD");
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certD] sCTs:@[proofD] ocspResponses:nil anchors:anchors
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(certD);
}

- (void)testTooFewEmbeddedSCTsForLifetime {
    NSDictionary *results = nil;
    SecCertificateRef leaf = nil, subCA = nil, root = nil;
    isnt(leaf = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"www_digicert_com_2015"], NULL, "create leaf");
    isnt(subCA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"digicert_sha2_ev_server_ca"], NULL, "create subCA");
    isnt(root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"digicert_ev_root_ca"], NULL, "create root");
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCA];
    XCTAssertNotNil(results = [self eval_ct_trust:certs
                                             sCTs:nil ocspResponses:nil anchors:@[(__bridge id)root]
                                    trustedCTLogs:nil hostname:@"www.digicert.com" verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(root);
}

- (void)testInvalidOCSPResponse {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil;
    NSData *proofA_1 = nil, *invalid_ocsp = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(invalid_ocsp = [CTTests DataFromResource:@"invalid_ocsp_response"], "create invalid ocsp response");
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:@[proofA_1] ocspResponses:@[invalid_ocsp] anchors:anchors
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(certA);
}

- (void)testOCSPResponseWithBadHash {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil;
    NSData *proofA_1 = nil, *invalid_ocsp = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(invalid_ocsp = [CTTests DataFromResource:@"bad_hash_ocsp_response"], "create ocsp response with bad hash");
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:@[proofA_1] ocspResponses:@[invalid_ocsp] anchors:anchors
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(certA);
}

- (void)testValidOCSPResponse {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil;
    NSData *proofA_1 = nil, *valid_ocsp = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(valid_ocsp = [CTTests DataFromResource:@"valid_ocsp_response"], "create invalid ocsp response");
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:@[proofA_1] ocspResponses:@[valid_ocsp] anchors:anchors
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(certA);
}

- (void)testTwoPresentedSCTs {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil;
    NSData *proofA_1 = nil, *proofA_2 = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(proofA_2 = [CTTests DataFromResource:@"serverA_proof_Bravo_3"], "create proofA_2");
    NSArray *scts = @[proofA_1, proofA_2];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
    CFReleaseNull(certA);
}

- (void)testThreeEmbeddedSCTs {
    NSDictionary *results = nil;
    SecCertificateRef leaf = nil, subCA = nil, root = nil;
    isnt(leaf = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"www_digicert_com_2016"], NULL, "create leaf");
    isnt(subCA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"digicert_sha2_ev_server_ca"], NULL, "create subCA");
    isnt(root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"digicert_ev_root_ca"], NULL, "create subCA");
    NSArray *certs = @[(__bridge id)leaf, (__bridge id)subCA];
    XCTAssertNotNil(results = [self eval_ct_trust:certs
                                             sCTs:nil ocspResponses:nil anchors:@[(__bridge id)root]
                                    trustedCTLogs:nil hostname:@"www.digicert.com" verifyDate:date_20160422]);
#if TARGET_OS_BRIDGE
    /* BridgeOS doesn't have a root store or CT log list so default CT behavior (without input logs as above) is failed CT validation. */
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");
#else
     XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result");
#endif
#if TARGET_OS_WATCH
    /* WatchOS doesn't require OCSP for EV flag, so even though the OCSP responder no longer responds for this cert, it is EV on watchOS. */
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustExtendedValidation], @YES, "expected EV result");
#else
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected");
#endif
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(root);
}

- (void)testOtherCTCerts {
    SecCertificateRef cfCert = NULL;
    NSDictionary *results = nil;

#define TEST_CASE(x) \
do { \
    isnt(cfCert = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@#x], NULL, "create cfCert from " #x); \
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)cfCert] \
                                             sCTs:nil ocspResponses:nil anchors:anchors \
                                    trustedCTLogs:trustedCTLogs hostname:nil verifyDate:date_20150307]); \
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result"); \
    XCTAssertNil(results[(__bridge NSString*)kSecTrustExtendedValidation], "got EV result when no EV expected"); \
    CFReleaseNull(cfCert);  \
    results = nil; \
} while (0)

    TEST_CASE(server_1601);
    TEST_CASE(server_1603);
    TEST_CASE(server_1604);
    TEST_CASE(server_1701);
    TEST_CASE(server_1704);
    TEST_CASE(server_1705);
    TEST_CASE(server_1801);
    TEST_CASE(server_1804);
    TEST_CASE(server_1805);
    TEST_CASE(server_2001);

#undef TEST_CASE
}

- (void)testLogListParsing {
    NSDictionary *results = nil;
    SecCertificateRef certF = nil;
    isnt(certF = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverF"], NULL, "create certF");

    /* Empty Log List */
    NSArray *testLogList = @[];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certF] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:testLogList hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* Log not a dictionary */
    testLogList = @[@[]];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certF] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:testLogList hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* Log list missing "key" key */
    testLogList = @[@{@"test":@"test"}];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certF] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:testLogList hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* Value for "key" isn't a data object */
    testLogList = @[@{@"key":@"test"}];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certF] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:testLogList hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    CFReleaseNull(certF);
}

- (void) testPrecertsFail {
    SecCertificateRef precert = NULL, system_root = NULL;
    SecTrustRef trust = NULL;
    NSArray *precert_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:561540800.0]; // October 18, 2018 at 12:33:20 AM PDT
    CFErrorRef error = NULL;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(precert = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"precert"],
                   errOut, fail("failed to create precert"));

    precert_anchors = @[(__bridge id)system_root];
    require_noerr_action(SecTrustCreateWithCertificates(precert, NULL, &trust), errOut, fail("failed to create trust object"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)precert_anchors), errOut, fail("failed to set anchor certificate"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    is(SecTrustEvaluateWithError(trust, &error), false, "SECURITY: trust evaluation of precert succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecUnknownCriticalExtensionFlag, "got wrong error code for precert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecUnknownCriticalExtensionFlag);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }


errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(precert);
    CFReleaseNull(error);
}

+ (NSArray <NSDictionary *>*)setShardedTrustedLogs:(NSArray <NSDictionary *>*)trustedLogs startTime:(CFAbsoluteTime)startTime endTime:(CFAbsoluteTime)endTime {
    NSMutableArray <NSDictionary *>* shardedLogs = [trustedLogs mutableCopy];
    [trustedLogs enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        NSMutableDictionary *shardedLogData = [obj mutableCopy];
        shardedLogData[@"start_inclusive"] = [NSDate dateWithTimeIntervalSinceReferenceDate:startTime];
        shardedLogData[@"end_exclusive"] = [NSDate dateWithTimeIntervalSinceReferenceDate:endTime];
        [shardedLogs replaceObjectAtIndex:idx withObject:shardedLogData];
    }];
    return shardedLogs;
}

- (void)testShardedLogs {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil;
    NSData *proofA_1 = nil, *proofA_2 = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(proofA_2 = [CTTests DataFromResource:@"serverA_proof_Bravo_3"], "create proofA_2");
    NSArray *scts = @[proofA_1, proofA_2];

    /* Certificate expiry within temporal shard window */
    NSArray <NSDictionary *> *shardedCTLogs= [CTTests setShardedTrustedLogs:trustedCTLogs startTime:0.0 endTime:CFAbsoluteTimeGetCurrent()];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:shardedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result");

    /* Certificate expiry before temporal shard window */
    shardedCTLogs= [CTTests setShardedTrustedLogs:trustedCTLogs startTime:CFAbsoluteTimeGetCurrent() endTime:(CFAbsoluteTimeGetCurrent() + 24*3600)];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:shardedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* Certificate expiry after temporal shard window */
    shardedCTLogs= [CTTests setShardedTrustedLogs:trustedCTLogs startTime:0.0 endTime:(24*3600)];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:shardedCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    CFReleaseNull(certA);
}

+ (NSArray <NSDictionary *>*)setReadOnlyTrustedLogs:(NSArray <NSDictionary *>*)trustedLogs readOnlyTime:(CFAbsoluteTime)readOnlyTime
{
    NSMutableArray <NSDictionary *>*readOnlyLogs = [trustedLogs mutableCopy];
    [trustedLogs enumerateObjectsUsingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        NSMutableDictionary *logData = [obj mutableCopy];
        logData[@"frozen"] = [NSDate dateWithTimeIntervalSinceReferenceDate:readOnlyTime];
        [readOnlyLogs replaceObjectAtIndex:idx withObject:logData];
    }];
    return readOnlyLogs;
}

- (void)testReadOnlyLogs {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil;
    NSData *proofA_1 = nil, *proofA_2 = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(proofA_2 = [CTTests DataFromResource:@"serverA_proof_Bravo_3"], "create proofA_2");
    NSArray *scts = @[proofA_1, proofA_2];

    /* SCTs before read-only date */
    NSArray <NSDictionary *> *readOnlyCTLogs = [CTTests setReadOnlyTrustedLogs:trustedCTLogs readOnlyTime:CFAbsoluteTimeGetCurrent()];
    XCTAssertNotNil(readOnlyCTLogs);
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:readOnlyCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result");

    /* SCTs after read-only date */
    readOnlyCTLogs = [CTTests setReadOnlyTrustedLogs:trustedCTLogs readOnlyTime:0.0];
    XCTAssertNotNil(readOnlyCTLogs);
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:readOnlyCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    CFReleaseNull(certA);
}

+ (NSArray <NSDictionary *>*)setRetiredTrustedLogs:(NSArray <NSDictionary *>*)trustedLogs retirementTime:(CFAbsoluteTime)retirementTime indexSet:(NSIndexSet *)indexSet
{
    __block NSMutableArray <NSDictionary *>*retiredLogs = [trustedLogs mutableCopy];
    [trustedLogs enumerateObjectsAtIndexes:indexSet options:0.0 usingBlock:^(NSDictionary * _Nonnull obj, NSUInteger idx, BOOL * _Nonnull stop) {
        NSMutableDictionary *logData = [obj mutableCopy];
        logData[@"expiry"] = [NSDate dateWithTimeIntervalSinceReferenceDate:retirementTime];
        [retiredLogs replaceObjectAtIndex:idx withObject:logData];
    }];
    return retiredLogs;
}

- (void)testRetiredLogs {
    NSDictionary *results = nil;
    SecCertificateRef certA = nil, server1601 = nil;
    NSData *proofA_1 = nil, *proofA_2 = nil;
    isnt(certA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"serverA"], NULL, "create certA");
    isnt(server1601 = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"server_1601"], NULL, "create server1601");
    XCTAssertNotNil(proofA_1 = [CTTests DataFromResource:@"serverA_proof_Alfa_3"], "create proofA_1");
    XCTAssertNotNil(proofA_2 = [CTTests DataFromResource:@"serverA_proof_Bravo_3"], "create proofA_2");
    NSArray *scts = @[proofA_1, proofA_2];

    NSArray <NSDictionary *> *retiredCTLogs = [CTTests setRetiredTrustedLogs:trustedCTLogs
                                                              retirementTime:CFAbsoluteTimeGetCurrent()
                                                                    indexSet:[NSIndexSet indexSetWithIndexesInRange:NSMakeRange(0, trustedCTLogs.count)]];
    /* presented SCTs from retired logs */
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)certA] sCTs:scts ocspResponses:nil anchors:anchors
                                    trustedCTLogs:retiredCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* all embedded SCTs from retired logs */
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)server1601] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:retiredCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* one embedded SCTs from retired log, before log retirement date (one once or currently qualified SCT) */
    retiredCTLogs = [CTTests setRetiredTrustedLogs:trustedCTLogs
                                    retirementTime:CFAbsoluteTimeGetCurrent()
                                          indexSet:[NSIndexSet indexSetWithIndex:0]];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)server1601] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:retiredCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result");

    /* one embedded SCT from retired log, after retirement date */
    retiredCTLogs = [CTTests setRetiredTrustedLogs:trustedCTLogs
                                    retirementTime:0.0
                                          indexSet:[NSIndexSet indexSetWithIndex:0]];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)server1601] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:retiredCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertNil(results[(__bridge NSString*)kSecTrustCertificateTransparency], "got CT result when no CT expected");

    /* one embedded SCT before retirement date, one embedded SCT before read-only date */
    retiredCTLogs = [CTTests setReadOnlyTrustedLogs:trustedCTLogs readOnlyTime:CFAbsoluteTimeGetCurrent()];
    retiredCTLogs = [CTTests setRetiredTrustedLogs:retiredCTLogs retirementTime:CFAbsoluteTimeGetCurrent() indexSet:[NSIndexSet indexSetWithIndex:0]];
    XCTAssertNotNil(results = [self eval_ct_trust:@[(__bridge id)server1601] sCTs:nil ocspResponses:nil anchors:anchors
                                    trustedCTLogs:retiredCTLogs hostname:nil verifyDate:date_20150307]);
    XCTAssertEqualObjects(results[(__bridge NSString*)kSecTrustCertificateTransparency], @YES, "expected CT result");

    CFReleaseNull(certA);
    CFReleaseNull(server1601);
}

//TODO: add more tests
// <rdar://problem/23849697> Expand unit tests for CT
// missing coverage:
//  -other signing algorithms
//  -v2 SCTs
//  -future timestamps
//  -SCT signature doesn't verify
//  -OCSP-delivered SCTs
//  -unknown logs (embedded, TLS, and OCSP)
//  -two SCTs from the same log (different timestamps)

@end

// MARK: -
// MARK: CT Enforcement Exceptions tests
@interface CTExceptionsTests : TrustEvaluationTestCase
@end

@implementation CTExceptionsTests
#if !TARGET_OS_BRIDGE // bridgeOS doesn't permit CT exceptions
+ (void)setUp {
    [super setUp];
    NSURL *trustedLogsURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"CTlogs"
                                                                     withExtension:@"plist"
                                                                      subdirectory:@"si-82-sectrust-ct-data"];
    trustedCTLogs = [NSArray arrayWithContentsOfURL:trustedLogsURL];
    NSData *rootHash = [NSData dataWithBytes:_system_root_hash length:sizeof(_system_root_hash)];
    CFPreferencesSetAppValue(CFSTR("TestCTRequiredSystemRoot"), (__bridge CFDataRef)rootHash, CFSTR("com.apple.security"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.security"));
}

- (void)testSetCTExceptions {
    CFErrorRef error = NULL;
    const CFStringRef TrustTestsAppID = CFSTR("com.apple.trusttests");
    const CFStringRef AnotherAppID = CFSTR("com.apple.security.not-this-one");
    CFDictionaryRef copiedExceptions = NULL;

    /* Verify no exceptions set */
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, NULL), NULL, "no exceptions set");
    if (copiedExceptions) {
        /* If we're starting out with exceptions set, a lot of the following will also fail, so just skip them */
        CFReleaseNull(copiedExceptions);
        return;
    }

    /* Set exceptions with specified AppID */
    NSDictionary *exceptions1 = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@"test.apple.com", @".test.apple.com"],
    };
    ok(SecTrustStoreSetCTExceptions(TrustTestsAppID, (__bridge CFDictionaryRef)exceptions1, &error),
       "failed to set exceptions for TrustTests: %@", error);

    /* Copy all exceptions (with only one set) */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    ok([exceptions1 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy this app's exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(TrustTestsAppID, &error),
       "failed to copy TrustTests' exceptions: %@", error);
    ok([exceptions1 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy a different app's exceptions */
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(AnotherAppID, &error), NULL,
       "failed to copy different app's exceptions: %@", error);
    CFReleaseNull(copiedExceptions);

    /* Set different exceptions with implied AppID */
    CFDataRef leafHash = SecSHA256DigestCreate(NULL, _system_after_leafSPKI, sizeof(_system_after_leafSPKI));
    NSDictionary *leafException = @{
        (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecCTExceptionsSPKIHashKey : (__bridge NSData*)leafHash,
    };
    NSDictionary *exceptions2 = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@".test.apple.com"],
        (__bridge NSString*)kSecCTExceptionsCAsKey : @[ leafException ]
    };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions2, &error),
       "failed to set exceptions for this app: %@", error);

    /* Ensure exceptions are replaced for TrustTests */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(TrustTestsAppID, &error),
       "failed to copy TrustTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Set exceptions with a different AppID */
    CFDataRef rootHash = SecSHA256DigestCreate(NULL, _system_rootSPKI, sizeof(_system_rootSPKI));
    NSDictionary *rootExceptions =  @{
        (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecCTExceptionsSPKIHashKey : (__bridge NSData*)rootHash,
    };
    NSDictionary *exceptions3 = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ rootExceptions ] };
    ok(SecTrustStoreSetCTExceptions(AnotherAppID, (__bridge CFDictionaryRef)exceptions3, &error),
       "failed to set exceptions for different app: %@", error);

    /* Copy only one of the app's exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(TrustTestsAppID, &error),
       "failed to copy TrustTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Set empty exceptions */
    NSDictionary *empty = @{};
    ok(SecTrustStoreSetCTExceptions(TrustTestsAppID, (__bridge CFDictionaryRef)empty, &error),
       "failed to set empty exceptions");

    /* Copy exceptions to ensure no change */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(TrustTestsAppID, &error),
       "failed to copy TrustTests' exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

    /* Copy all exceptions */
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    is(CFDictionaryGetCount(copiedExceptions), 2, "Got the wrong number of all exceptions");
    NSDictionary *nsCopiedExceptions = CFBridgingRelease(copiedExceptions);
    NSArray *domainExceptions = nsCopiedExceptions[(__bridge NSString*)kSecCTExceptionsDomainsKey];
    NSArray *caExceptions = nsCopiedExceptions[(__bridge NSString*)kSecCTExceptionsCAsKey];
    ok(domainExceptions && caExceptions, "Got both domain and CA exceptions");
    ok([domainExceptions count] == 1, "Got 1 domain exception");
    ok([caExceptions count] == 2, "Got 2 CA exceptions");
    ok([domainExceptions[0] isEqualToString:@".test.apple.com"], "domain exception is .test.apple.com");
    ok([caExceptions containsObject:leafException] && [caExceptions containsObject:rootExceptions], "got expected leaf and root CA exceptions");

    /* Reset other app's exceptions */
    ok(SecTrustStoreSetCTExceptions(AnotherAppID, NULL, &error),
       "failed to reset exceptions for different app: %@", error);
    ok(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, &error),
       "failed to copy all exceptions: %@", error);
    ok([exceptions2 isEqualToDictionary:(__bridge NSDictionary*)copiedExceptions],
       "got the wrong exceptions back");
    CFReleaseNull(copiedExceptions);

#define check_errSecParam \
if (error) { \
is(CFErrorGetCode(error), errSecParam, "bad input produced unxpected error code: %ld", (long)CFErrorGetCode(error)); \
CFReleaseNull(error); \
} else { \
fail("expected failure to set NULL exceptions"); \
}

    /* Set exceptions with bad inputs */
    NSDictionary *badExceptions = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[@"test.apple.com", @".test.apple.com"],
        @"not a key": @"not a value",
    };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with unknown key");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey:@"test.apple.com" };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey: @[ @{} ] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad array value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @"test.apple.com" ] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad array value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @{
        (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
        @"not-a-key" : (__bridge NSData*)rootHash,
    }] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad CA dictionary value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @{
        (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
    }] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad CA dictionary value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey: @[ @{
        (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecCTExceptionsSPKIHashKey : (__bridge NSData*)rootHash,
        @"not-a-key":@"not-a-value"
    }] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with bad CA dictionary value");
    check_errSecParam

    badExceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey: @[ @".com" ] };
    is(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)badExceptions, &error), false,
       "set exceptions with TLD value");
    check_errSecParam
#undef check_errSecParam

    /* Remove exceptions using empty arrays */
    NSDictionary *emptyArrays = @{
        (__bridge NSString*)kSecCTExceptionsDomainsKey: @[],
        (__bridge NSString*)kSecCTExceptionsCAsKey : @[]
    };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)emptyArrays, &error),
       "failed to set empty array exceptions for this app: %@", error);
    is(copiedExceptions = SecTrustStoreCopyCTExceptions(NULL, NULL), NULL, "no exceptions set");

    CFReleaseNull(leafHash);
    CFReleaseNull(rootHash);
}

#define evalTrustExpectingError(errCode, ...) \
    is(SecTrustEvaluateWithError(trust, &error), false, __VA_ARGS__); \
    if (error) { \
        is(CFErrorGetCode(error), errCode, "got wrong error code, got %ld, expected %d", \
            (long)CFErrorGetCode(error), (int)errCode); \
    } else { \
        fail("expected trust evaluation to fail and it did not."); \
    } \
    CFReleaseNull(error);

- (void) testSpecificDomainExceptions {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *exceptions_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *exceptions = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after_scts"],
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    exceptions_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* superdomain exception without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    evalTrustExpectingError(errSecVerifyActionFailed, "superdomain exception unexpectedly succeeded");

    /* subdomain exceptions without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"one.ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "subdomain exception unexpectedly succeeded")

    /* no match without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"example.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "unrelated domain exception unexpectedly succeeded");

    /* matching domain without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "exact match domain exception did not apply");

    /* matching domain with CT succeeds */
    CFReleaseNull(trust);
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after_with_CT, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior
    is(SecTrustEvaluateWithError(trust, &error), true, "ct cert should always pass");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

- (void) testSubdomainExceptions {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *exceptions_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *exceptions = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after_scts"],
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    exceptions_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* superdomain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    is(SecTrustEvaluateWithError(trust, &error), true, "superdomain exception did not apply");

    /* exact domain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "exact domain exception did not apply");

    /* no match without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".example.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "unrelated domain exception unexpectedly succeeded");

    /* subdomain without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".one.ct.test.apple.com"] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "subdomain exception unexpectedly succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

- (void) testMixedDomainExceptions {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *exceptions_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *exceptions = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after_scts"],
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    exceptions_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* specific domain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"ct.test.apple.com", @".example.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    is(SecTrustEvaluateWithError(trust, &error), true, "one of exact domain exception did not apply");

    /* super domain exception without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@".apple.com", @"example.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "one of superdomain exception did not apply");

    /* both super domain and specific domain exceptions without CT succeeds */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"ct.test.apple.com", @".apple.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    is(SecTrustEvaluateWithError(trust, &error), true, "both domain exception did not apply");

    /* neither specific domain nor super domain exceptions without CT fails */
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsDomainsKey : @[@"apple.com", @".example.com" ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error), "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "no match domain unexpectedly succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

- (void) test_ct_leaf_exceptions {
    SecCertificateRef system_root = NULL, system_server_after = NULL, system_server_after_with_CT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *exceptions_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *leafException = nil, *exceptions = nil;
    NSData *leafHash = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));
    require_action(system_server_after_with_CT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after_scts"],
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    exceptions_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior

    /* set exception on leaf cert without CT */
    leafHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(system_server_after));
    leafException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                       (__bridge NSString*)kSecCTExceptionsSPKIHashKey : leafHash,
                       };
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ leafException ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);
    is(SecTrustEvaluateWithError(trust, &error), true, "leaf public key exception did not apply");

    /* set exception on leaf cert with CT */
    leafHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(system_server_after_with_CT));
    leafException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                       (__bridge NSString*)kSecCTExceptionsSPKIHashKey : leafHash,
                       };
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ leafException ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);
    SecTrustSetNeedsEvaluation(trust);
    evalTrustExpectingError(errSecVerifyActionFailed, "leaf cert with no public key exceptions succeeded");

    /* matching public key with CT succeeds */
    CFReleaseNull(trust);
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after_with_CT, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs")); // set trusted logs to trigger enforcing behavior
    is(SecTrustEvaluateWithError(trust, &error), true, "ct cert should always pass");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

- (void) test_ct_unconstrained_ca_exceptions {
    SecCertificateRef root = NULL, subca = NULL;
    SecCertificateRef server_matching = NULL, server_matching_with_CT = NULL, server_partial = NULL, server_no_match = NULL, server_no_org = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *exceptions_anchors = nil, *certs = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *caException = nil, *exceptions = nil;
    NSData *caHash = nil;

    require_action(root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(subca = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_unconstrained_subca"],
                   errOut, fail("failed to create subca"));
    require_action(server_matching = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_matching_orgs"],
                   errOut, fail("failed to create server cert with matching orgs"));
    require_action(server_matching_with_CT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_matching_orgs_scts"],
                   errOut, fail("failed to create server cert with matching orgs and scts"));
    require_action(server_partial = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_partial_orgs"],
                   errOut, fail("failed to create server cert with partial orgs"));
    require_action(server_no_match = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_nonmatching_orgs"],
                   errOut, fail("failed to create server cert with non-matching orgs"));
    require_action(server_no_org = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_no_orgs"],
                   errOut, fail("failed to create server cert with no orgs"));

    exceptions_anchors = @[ (__bridge id)root ];

#define createTrust(certs) \
CFReleaseNull(trust); \
require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust")); \
require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors")); \
require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date")); \
require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    /* Set exception on the subCA */
    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : @[ caException ] };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);

    /* Verify that non-CT cert with Orgs matching subCA passes */
    certs = @[ (__bridge id)server_matching, (__bridge id)subca];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "matching org subca exception did not apply: %@", error);

    /* Verify that CT cert with Orgs matching subCA passes */
    certs = @[ (__bridge id)server_matching_with_CT, (__bridge id)subca];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "CT matching org subca exception did not apply: %@", error);

    /* Verify that non-CT cert with partial Org match fails */
    certs = @[ (__bridge id)server_partial, (__bridge id)subca];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "partial matching org leaf succeeded");

    /* Verify that a non-CT cert with non-matching Org fails */
    certs = @[ (__bridge id)server_no_match, (__bridge id)subca];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "non-matching org leaf succeeded");

    /* Verify that a non-CT cert with no Org fails */
    certs = @[ (__bridge id)server_no_org, (__bridge id)subca];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "no org leaf succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

#undef createTrust

errOut:
    CFReleaseNull(root);
    CFReleaseNull(subca);
    CFReleaseNull(server_matching);
    CFReleaseNull(server_matching_with_CT);
    CFReleaseNull(server_partial);
    CFReleaseNull(server_no_match);
    CFReleaseNull(server_no_org);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

- (void) test_ct_constrained_ca_exceptions {
    SecCertificateRef root = NULL, org_constrained_subca = NULL;
    SecCertificateRef constraint_pass_server = NULL, constraint_pass_server_ct = NULL, constraint_fail_server = NULL;
    SecCertificateRef dn_constrained_subca = NULL, dn_constrained_server = NULL, dn_constrained_server_mismatch = NULL;
    SecCertificateRef dns_constrained_subca = NULL, dns_constrained_server = NULL, dns_constrained_server_mismatch = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *exceptions_anchors = nil, *certs = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    NSDictionary *caException = nil, *exceptions = nil;
    NSMutableArray *caExceptions = [NSMutableArray array];
    NSData *caHash = nil;

    require_action(root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(org_constrained_subca = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_subca"],
                   errOut, fail("failed to create org-constrained subca"));
    require_action(constraint_pass_server = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_server"],
                   errOut, fail("failed to create constrained non-CT leaf"));
    require_action(constraint_pass_server_ct = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_server_scts"],
                   errOut, fail("failed to create constrained CT leaf"));
    require_action(constraint_fail_server= (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_fail_server"],
                   errOut, fail("failed to create constraint failure leaf"));
    require_action(dn_constrained_subca = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_no_org_subca"],
                   errOut, fail("failed to create dn-constrained subca"));
    require_action(dn_constrained_server = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_no_org_server"],
                   errOut, fail("failed to create dn-constrained leaf"));
    require_action(dn_constrained_server_mismatch = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_no_org_server_mismatch"],
                   errOut, fail("failed to create dn-constrained leaf with mismatched orgs"));
    require_action(dns_constrained_subca = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_no_dn_subca"],
                   errOut, fail("failed to create dns-constrained subca"));
    require_action(dns_constrained_server = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_no_dn_server"],
                   errOut, fail("failed to create dns-constrained leaf"));
    require_action(dns_constrained_server_mismatch = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_constrained_no_dn_server_mismatch"],
                   errOut, fail("failed to create dns-constrained leaf with mismatched orgs"));

    exceptions_anchors = @[ (__bridge id)root ];

    /* Set exception on the subCAs */
    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(org_constrained_subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    [caExceptions addObject:caException];

    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(dn_constrained_subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    [caExceptions addObject:caException];

    caHash = CFBridgingRelease(SecCertificateCopySubjectPublicKeyInfoSHA256Digest(dns_constrained_subca));
    caException = @{ (__bridge NSString*)kSecCTExceptionsHashAlgorithmKey : @"sha256",
                     (__bridge NSString*)kSecCTExceptionsSPKIHashKey : caHash,
                     };
    [caExceptions addObject:caException];
    exceptions = @{ (__bridge NSString*)kSecCTExceptionsCAsKey : caExceptions };
    ok(SecTrustStoreSetCTExceptions(NULL, (__bridge CFDictionaryRef)exceptions, &error),
       "failed to set exceptions: %@", error);

#define createTrust(certs) \
CFReleaseNull(trust); \
require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust")); \
require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)exceptions_anchors), errOut, fail("failed to set anchors")); \
require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date")); \
require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    /* Verify org-constrained non-CT leaf passes */
    certs = @[ (__bridge id)constraint_pass_server, (__bridge id)org_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org constrained exception did not apply: %@", error);

    /* Verify org-constrained CT leaf passes */
    certs = @[ (__bridge id)constraint_pass_server_ct, (__bridge id)org_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org constrained exception did not apply: %@", error);

    /* Verify org-constrained non-CT leaf with wrong org fails */
    certs = @[ (__bridge id)constraint_fail_server, (__bridge id)org_constrained_subca ];
    createTrust(certs);
    evalTrustExpectingError(errSecInvalidName, "leaf failing name constraints succeeded");

    /* Verify dn-constrained (but not with org) non-CT leaf with matching orgs succeeds */
    certs = @[ (__bridge id)dn_constrained_server, (__bridge id)dn_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org match exception did not apply: %@", error);

    /* Verify dn-constrained (but not with org) non-CT leaf without matching orgs fails */
    certs = @[ (__bridge id)dn_constrained_server_mismatch, (__bridge id)dn_constrained_subca ];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "dn name constraints with no org succeeded");

    /* Verify dns-constrained (no DN constraints) non-CT leaf with matching orgs succeeds */
    certs = @[ (__bridge id)dns_constrained_server, (__bridge id)dns_constrained_subca ];
    createTrust(certs);
    is(SecTrustEvaluateWithError(trust, &error), true, "org match exception did not apply: %@", error);

    /* Verify dns-constrained (no DN constraints) non-CT leaf without matching orgs fails*/
    certs = @[ (__bridge id)dns_constrained_server_mismatch, (__bridge id)dns_constrained_subca ];
    createTrust(certs);
    evalTrustExpectingError(errSecVerifyActionFailed, "dns name constraints with no DN constraint succeeded");

    ok(SecTrustStoreSetCTExceptions(NULL, NULL, &error), "failed to reset exceptions: %@", error);

#undef createTrust

errOut:
    CFReleaseNull(root);
    CFReleaseNull(org_constrained_subca);
    CFReleaseNull(constraint_pass_server);
    CFReleaseNull(constraint_pass_server_ct);
    CFReleaseNull(constraint_fail_server);
    CFReleaseNull(dn_constrained_subca);
    CFReleaseNull(dn_constrained_server);
    CFReleaseNull(dn_constrained_server_mismatch);
    CFReleaseNull(dns_constrained_subca);
    CFReleaseNull(dns_constrained_server);
    CFReleaseNull(dns_constrained_server_mismatch);
    CFReleaseNull(trust);
    CFReleaseNull(policy);
    CFReleaseNull(error);
}

#else // TARGET_OS_BRIDGE
- (void)testSkipTests
{
    XCTAssert(true);
}
#endif // TARGET_OS_BRIDGE
@end

// MARK: -
// MARK: CT Enforcement tests

@interface CTEnforcementTests : TrustEvaluationTestCase
@end

static NSArray *keychainCerts = nil;

@implementation CTEnforcementTests
+ (void)setUp {
    [super setUp];
    NSURL *trustedLogsURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"CTlogs"
                                                                     withExtension:@"plist"
                                                                      subdirectory:@"si-82-sectrust-ct-data"];
    trustedCTLogs = [NSArray arrayWithContentsOfURL:trustedLogsURL];

    // set test root to be a fake system root
    SecCertificateRef system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"];
    NSData *rootHash = [NSData dataWithBytes:_system_root_hash length:sizeof(_system_root_hash)];
    CFPreferencesSetAppValue(CFSTR("TestCTRequiredSystemRoot"), (__bridge CFDataRef)rootHash, CFSTR("com.apple.security"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.security"));
    CFReleaseNull(system_root);
}

#if !TARGET_OS_BRIDGE
/* Skip tests on bridgeOS where we don't do MobileAsset updates */
- (void)testNoMACheckIn {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    /* Mock a failing MobileAsset so we don't enforce via MobileAsset */
    id mockFailedMA = OCMClassMock([MAAsset class]);
    OCMStub([mockFailedMA startCatalogDownload:[OCMArg any]
                                       options:[OCMArg any]
                                          then:([OCMArg invokeBlockWithArgs:OCMOCK_VALUE((NSInteger){MADownloadFailed}), nil])]);
    SecOTAPKIResetCurrentAssetVersion(NULL);

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // Out-of-date asset, test system cert after date without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with out-of-date asset");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testKillSwitch {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    /* Mock setting a kill switch */
    UpdateKillSwitch((__bridge NSString *)kOTAPKIKillSwitchCT, true);

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // CT kill switch enabled so test system cert after date without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with out-of-date asset");

    /* Remove the kill switch */
    UpdateKillSwitch((__bridge NSString *)kOTAPKIKillSwitchCT, false);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void) testWithMACheckIn {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;

    /* Mock a successful mobile asset check-in so that we enforce CT */
    XCTAssertTrue(UpdateOTACheckInDate(), "failed to set check-in date as now");

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // test system cert after date without CT fails (with check-in)
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag-date non-CT cert with in-date asset succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}
#endif // !TARGET_OS_BRIDGE

- (void)testWithTrustedLogs {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test system cert after date without CT fails (with trusted logs)
#if !TARGET_OS_BRIDGE
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag-date non-CT cert with trusted logs succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
#else
    /* BridgeOS doesn't enforce */
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert with trusted logs failed");
#endif

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}

- (void)testExpiredCert {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:570000000.0]; // January 24, 2019 at 12:20:00 AM EST
    CFErrorRef error = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test expired system cert after date without CT passes with only expired error
    ok(SecTrustIsExpiredOnly(trust), "expired non-CT cert had non-expiration errors");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}

- (void)testTrustExceptions {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    CFDataRef exceptions = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test system cert after date without CT fails (with check-in)
#if !TARGET_OS_BRIDGE
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag-date non-CT cert with trusted logs succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
#else
    /* BridgeOS doesn't enforce */
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert with trusted logs failed");
#endif

    // test exceptions for failing cert passes
    exceptions = SecTrustCopyExceptions(trust);
    ok(SecTrustSetExceptions(trust, exceptions), "failed to set exceptions for failing non-CT cert");
    CFReleaseNull(exceptions);
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with exceptions set");
    SecTrustSetExceptions(trust, NULL);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
    CFReleaseNull(exceptions);
}

- (void) testCATrustSettings {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    id persistentRef = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test system cert + enterprise anchor after date without CT fails
#if !TARGET_OS_BRIDGE
    persistentRef = [self addTrustSettingsForCert:system_root];
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag date non-CT cert with enterprise root trust succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
    [self removeTrustSettingsForCert:system_root persistentRef:persistentRef];
#else
    /* BridgeOS doesn't enforce (and doesn't use trust settings) */
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert with trusted logs failed");
#endif

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}

- (void) testAppSystem {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test app anchor for failing cert passes
    enforce_anchors = @[ (__bridge id)system_server_after ];
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with server cert app anchor");
errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}

#if !TARGET_OS_BRIDGE
/* bridgeOS doens't have trust settings */
- (void) testLeafTrustSettings {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;
    id persistentRef = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test trust settings for failing cert passes
    persistentRef = [self addTrustSettingsForCert:system_server_after];
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with server cert enterprise anchor");
    [self removeTrustSettingsForCert:system_server_after persistentRef:persistentRef];

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}
#endif // !TARGET_OS_BRIDGE

- (void) testEAPPolicy {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateEAP(true, NULL);
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // EAP, test system cert after date without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with EAP cert");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// Test pinning policy name
- (void) testPinningPolicy {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set policy name
    require_noerr_action(SecTrustSetPinningPolicyName(trust, CFSTR("a-policy-name")), errOut, fail("failed to set policy name"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // pinning policy, test system cert after date without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert failed with EAP cert");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// test system cert after date with CT passes
- (void) testAfterWithCT {
    SecCertificateRef system_root = NULL, system_server_after_with_CT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after_with_CT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after_scts"],
                   errOut, fail("failed to create system server cert issued after flag day with SCTs"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after_with_CT, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date CT cert failed");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after_with_CT);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// test system cert before date without CT passes
- (void) testBefore {
    SecCertificateRef system_root = NULL, system_server_before = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    SecTrustRef trust = NULL;
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_before = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_before"],
                   errOut, fail("failed to create system server cert issued before flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_before, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "system pre-flag-date non-CT cert failed");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_before);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

#if !TARGET_OS_BRIDGE
// test enterprise (non-public) after date without CT passes, except on bridgeOS which doesn't have trust settings
- (void) testEnterpriseCA {
    SecCertificateRef user_root = NULL, user_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    id persistentRef = nil;

    require_action(user_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_user_root"],
                   errOut, fail("failed to create user root"));
    require_action(user_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_user_server_after"],
                   errOut, fail("failed to create user server cert issued after flag day"));

    persistentRef = [self addTrustSettingsForCert:user_root];
    require_noerr_action(SecTrustCreateWithCertificates(user_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust,(__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "non-system post-flag-date non-CT cert failed with enterprise anchor");
    [self removeTrustSettingsForCert:user_root persistentRef:persistentRef];

errOut:
    CFReleaseNull(user_root);
    CFReleaseNull(user_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}
#endif // !TARGET_OS_BRIDGE

// test app anchor (non-public) after date without CT passes
- (void) testAppCA {
    SecCertificateRef user_root = NULL, user_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("ct.test.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    NSArray *enforce_anchors = nil;

    require_action(user_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_user_root"],
                   errOut, fail("failed to create user root"));
    require_action(user_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_user_server_after"],
                   errOut, fail("failed to create user server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)user_root ];
    require_noerr_action(SecTrustCreateWithCertificates(user_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust,(__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "non-system post-flag-date non-CT cert failed with enterprise anchor");

errOut:
    CFReleaseNull(user_root);
    CFReleaseNull(user_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// test apple anchor after date without CT passes
- (void) testAppleAnchor {
    SecCertificateRef appleServerAuthCA = NULL, apple_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("bbasile-test.scv.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    NSArray *certs = nil;

    require_action(appleServerAuthCA = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_apple_ca"],
                   errOut, fail("failed to create apple server auth CA"));
    require_action(apple_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_apple_server_after"],
                   errOut, fail("failed to create apple server cert issued after flag day"));

    certs = @[ (__bridge id)apple_server_after, (__bridge id)appleServerAuthCA ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "apple post-flag-date non-CT cert failed");

errOut:
    CFReleaseNull(appleServerAuthCA);
    CFReleaseNull(apple_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// test apple subCA after date without CT fails
- (void) testAppleSubCAException {
    SecCertificateRef geoTrustRoot = NULL, appleISTCA8G1 = NULL, deprecatedSSLServer = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("bbasile-test.scv.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:576000000.0]; // April 3, 2019 at 9:00:00 AM PDT
    NSArray *certs = nil, *enforcement_anchors = nil;

    require_action(geoTrustRoot = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"GeoTrustPrimaryCAG2"],
                   errOut, fail("failed to create geotrust root"));
    require_action(appleISTCA8G1 = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"AppleISTCA8G1"],
                   errOut, fail("failed to create apple IST CA"));
    require_action(deprecatedSSLServer = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"deprecatedSSLServer"],
                   errOut, fail("failed to create deprecated SSL Server cert"));

    certs = @[ (__bridge id)deprecatedSSLServer, (__bridge id)appleISTCA8G1 ];
    enforcement_anchors = @[ (__bridge id)geoTrustRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforcement_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
#if !TARGET_OS_BRIDGE
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "apple public post-flag-date non-CT cert passed");
#endif

errOut:
    CFReleaseNull(geoTrustRoot);
    CFReleaseNull(appleISTCA8G1);
    CFReleaseNull(deprecatedSSLServer);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void) testBasejumper {
    SecCertificateRef baltimoreRoot = NULL, appleISTCA2 = NULL, deprecatedSSLServer = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("basejumper.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:576000000.0]; // April 3, 2019 at 9:00:00 AM PDT
    NSArray *certs = nil, *enforcement_anchors = nil;

    require_action(baltimoreRoot = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"BaltimoreCyberTrustRoot"],
                   errOut, fail("failed to create geotrust root"));
    require_action(appleISTCA2 = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"AppleISTCA2_Baltimore"],
                   errOut, fail("failed to create apple IST CA"));
    require_action(deprecatedSSLServer = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"basejumper"],
                   errOut, fail("failed to create deprecated SSL Server cert"));

    certs = @[ (__bridge id)deprecatedSSLServer, (__bridge id)appleISTCA2 ];
    enforcement_anchors = @[ (__bridge id)baltimoreRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforcement_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "non-CT basejumper cert failed");

#if !TARGET_OS_BRIDGE
    // bridgeOS doesn't ever enforce CT
    // Test with generic CT allowlist disable
    CFPreferencesSetAppValue(CFSTR("DisableCTAllowlist"), kCFBooleanTrue, CFSTR("com.apple.security"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.security"));
    SecTrustSetNeedsEvaluation(trust);
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "non-CT basejumper succeeded with allowlist disabled");
    CFPreferencesSetAppValue(CFSTR("DisableCTAllowlist"), kCFBooleanFalse, CFSTR("com.apple.security"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.security"));

    // Test with Apple allowlist disable
    CFPreferencesSetAppValue(CFSTR("DisableCTAllowlistApple"), kCFBooleanTrue, CFSTR("com.apple.security"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.security"));
    SecTrustSetNeedsEvaluation(trust);
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "non-CT basejumper succeeded with Apple allowlist disabled");
    CFPreferencesSetAppValue(CFSTR("DisableCTAllowlistApple"), kCFBooleanFalse, CFSTR("com.apple.security"));
    CFPreferencesAppSynchronize(CFSTR("com.apple.security"));
#endif // !TARGET_OS_BRIDGE

errOut:
    CFReleaseNull(baltimoreRoot);
    CFReleaseNull(appleISTCA2);
    CFReleaseNull(deprecatedSSLServer);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// test google subCA after date without CT fails
- (void) testGoogleSubCAException {
    SecCertificateRef globalSignRoot = NULL, googleIAG3 = NULL, google = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("www.google.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    NSArray *certs = nil, *enforcement_anchors = nil;

    require_action(globalSignRoot = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"GlobalSignRootCAR2"],
                   errOut, fail("failed to create geotrust root"));
    require_action(googleIAG3 = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"GoogleIAG3"],
                   errOut, fail("failed to create apple IST CA"));
    require_action(google = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"google"],
                   errOut, fail("failed to create livability cert"));

    certs = @[ (__bridge id)google, (__bridge id)googleIAG3 ];
    enforcement_anchors = @[ (__bridge id)globalSignRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforcement_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
#if !TARGET_OS_BRIDGE
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "google public post-flag-date non-CT cert passed");
#endif

errOut:
    CFReleaseNull(globalSignRoot);
    CFReleaseNull(googleIAG3);
    CFReleaseNull(google);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

// If pinning is disabled, pinned hostnames should continue to be exempt from CT
- (void) testSystemwidePinningDisable {
    SecCertificateRef baltimoreRoot = NULL, appleISTCA2 = NULL, pinnedNonCT = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = SecPolicyCreateSSL(true, CFSTR("iphonesubmissions.apple.com"));
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:580000000.0]; // May 19, 2019 at 4:06:40 PM PDT
    NSArray *certs = nil, *enforcement_anchors = nil;
    NSUserDefaults *defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];

    require_action(baltimoreRoot = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"BaltimoreCyberTrustRoot"],
                   errOut, fail("failed to create geotrust root"));
    require_action(appleISTCA2 = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"AppleISTCA2_Baltimore"],
                   errOut, fail("failed to create apple IST CA"));
    require_action(pinnedNonCT = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"iphonesubmissions"],
                   errOut, fail("failed to create deprecated SSL Server cert"));

    certs = @[ (__bridge id)pinnedNonCT, (__bridge id)appleISTCA2 ];
    enforcement_anchors = @[ (__bridge id)baltimoreRoot ];
    require_noerr_action(SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforcement_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "pinned non-CT cert failed");

    // Test with pinning disabled
    [defaults setBool:YES forKey:@"AppleServerAuthenticationNoPinning"];
    [defaults synchronize];
    SecTrustSetNeedsEvaluation(trust);
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "pinned non-CT failed with pinning disabled");
    [defaults setBool:NO forKey:@"AppleServerAuthenticationNoPinning"];
    [defaults synchronize];

errOut:
    CFReleaseNull(baltimoreRoot);
    CFReleaseNull(appleISTCA2);
    CFReleaseNull(pinnedNonCT);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void) testCheckCTRequired {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;

    // A policy with CTRequired set
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecPolicySetOptionsValue(policy, kSecPolicyCheckCTRequired, kCFBooleanTrue);

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert issued after flag day"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test system cert without CT fails (with trusted logs)
#if !TARGET_OS_BRIDGE
    is(SecTrustEvaluateWithError(trust, &error), false, "system post-flag-date non-CT cert with trusted logs succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecNotTrusted, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecNotTrusted);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
#else
    /* BridgeOS doesn't enforce */
    ok(SecTrustEvaluateWithError(trust, NULL), "system post-flag-date non-CT cert with trusted logs failed");
#endif

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);

}

@end

// MARK: -
// MARK: Non-TLS CT tests

@interface NonTlsCTTests : TrustEvaluationTestCase
@end

@implementation NonTlsCTTests
+ (void)setUp {
    [super setUp];
    NSURL *trustedLogsURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"CTlogs"
                                                                     withExtension:@"plist"
                                                                      subdirectory:@"si-82-sectrust-ct-data"];
    trustedCTLogs = [NSArray arrayWithContentsOfURL:trustedLogsURL];
}

- (SecPolicyRef)nonTlsCTRequiredPolicy
{
    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecPolicySetOptionsValue(policy, kSecPolicyCheckNonTlsCTRequired, kCFBooleanTrue);
    return policy;
}

#if !TARGET_OS_BRIDGE
/* Skip tests on bridgeOS where we don't do MobileAsset updates */
- (void)testNoMACheckIn {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = [self nonTlsCTRequiredPolicy];
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    /* Mock a failing MobileAsset so we don't enforce via MobileAsset */
    id mockFailedMA = OCMClassMock([MAAsset class]);
    OCMStub([mockFailedMA startCatalogDownload:[OCMArg any]
                                       options:[OCMArg any]
                                          then:([OCMArg invokeBlockWithArgs:OCMOCK_VALUE((NSInteger){MADownloadFailed}), nil])]);
    SecOTAPKIResetCurrentAssetVersion(NULL);

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create server cert"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // Out-of-date asset, test cert without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "non-CT cert failed with out-of-date asset");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testKillSwitch {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = [self nonTlsCTRequiredPolicy];
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    /* Mock setting a kill switch */
    UpdateKillSwitch((__bridge NSString *)kOTAPKIKillSwitchNonTLSCT, true);

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // CT kill switch enabled so test cert without CT passes
    ok(SecTrustEvaluateWithError(trust, NULL), "non-CT cert failed with kill switch enabled");

    /* Remove the kill switch */
    UpdateKillSwitch((__bridge NSString *)kOTAPKIKillSwitchNonTLSCT, false);

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void) testWithMACheckIn {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = [self nonTlsCTRequiredPolicy];
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;

    /* Mock a successful mobile asset check-in so that we enforce CT */
    XCTAssertTrue(UpdateOTACheckInDate(), "failed to set check-in date as now");

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // test system cert after date without CT fails (with check-in)
    is(SecTrustEvaluateWithError(trust, &error), false, "non-CT cert with in-date asset succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}
#endif // !TARGET_OS_BRIDGE

- (void)testWithTrustedLogs {
    SecCertificateRef system_root = NULL,  system_server_after = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = [self nonTlsCTRequiredPolicy];
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT
    CFErrorRef error = nil;

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(system_server_after = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after"],
                   errOut, fail("failed to create system server cert"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(system_server_after, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));

    // set trusted logs to trigger enforcing behavior
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));

    // test system cert without CT fails (with trusted logs)
    is(SecTrustEvaluateWithError(trust, &error), false, "non-CT cert with trusted logs succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecVerifyActionFailed, "got wrong error code for non-ct cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecVerifyActionFailed);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(system_server_after);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(error);
}

- (void) testSuccess {
    SecCertificateRef system_root = NULL, leaf = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = [self nonTlsCTRequiredPolicy];
    NSArray *enforce_anchors = nil;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:562340800.0]; // October 27, 2018 at 6:46:40 AM PDT

    require_action(system_root = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_root"],
                   errOut, fail("failed to create system root"));
    require_action(leaf = (__bridge SecCertificateRef)[CTTests SecCertificateCreateFromResource:@"enforcement_system_server_after_scts"],
                   errOut, fail("failed to create system server cert"));

    enforce_anchors = @[ (__bridge id)system_root ];
    require_noerr_action(SecTrustCreateWithCertificates(leaf, policy, &trust), errOut, fail("failed to create trust"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)enforce_anchors), errOut, fail("failed to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), errOut, fail("failed to set verify date"));
    require_noerr_action(SecTrustSetTrustedLogs(trust, (__bridge CFArrayRef)trustedCTLogs), errOut, fail("failed to set trusted logs"));
    ok(SecTrustEvaluateWithError(trust, NULL), "CT cert failed");

errOut:
    CFReleaseNull(system_root);
    CFReleaseNull(leaf);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

@end
