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

#import <XCTest/XCTest.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include "OSX/utilities/SecCFWrappers.h"

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "KeySizeTests_data.h"

@interface KeySizeTests : TrustEvaluationTestCase
@end

@implementation KeySizeTests

- (bool)run_chain_of_threetest:(NSData *)cert0 cert1:(NSData *)cert1 root:(NSData *)root
                        result:(bool)should_succeed failureReason:(NSString **)failureReason
{
    bool ok = false;
    
    const void *secCert0, *secCert1, *secRoot;
    isnt(secCert0 = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)cert0), NULL, "create leaf");
    isnt(secCert1 = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)cert1), NULL, "create subCA");
    isnt(secRoot  = SecCertificateCreateWithData(NULL, (__bridge CFDataRef)root),  NULL, "create root");
    
    const void *v_certs[] = { secCert0, secCert1 };
    CFArrayRef certs = NULL;
    isnt(certs = CFArrayCreate(NULL, v_certs, sizeof(v_certs)/sizeof(*v_certs), &kCFTypeArrayCallBacks),
         NULL, "failed to create cert array");
    CFArrayRef anchors = NULL;
    isnt(anchors = CFArrayCreate(NULL, &secRoot, 1, &kCFTypeArrayCallBacks), NULL, "failed to create anchors array");
    
    SecPolicyRef policy = NULL;
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "failed to create policy");
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 472100000.0), NULL, "failed to create date"); // 17 Dec 2015
    
    SecTrustRef trust = NULL;
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "failed to create trust");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "failed to set verify date");
    if (!anchors) { goto errOut; }
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "failed to set anchors");
    
    bool did_succeed = SecTrustEvaluateWithError(trust, NULL);
    is(SecTrustGetCertificateCount(trust), 3, "expected chain of 3");
    
    if (failureReason && should_succeed && !did_succeed) {
        *failureReason = CFBridgingRelease(SecTrustCopyFailureDescription(trust));
    } else if (failureReason && !should_succeed && did_succeed) {
        *failureReason = @"expected kSecTrustResultFatalTrustFailure";
    }
    
    if ((should_succeed && did_succeed) || (!should_succeed && !did_succeed)) {
        ok = true;
    }
    
errOut:
    CFReleaseNull(secCert0);
    CFReleaseNull(secCert1);
    CFReleaseNull(secRoot);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(date);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    
    return ok;
}

- (void)test8192BitKeySize {
    /* Test prt_forest_fi that have a 8k RSA key */
    const void *prt_forest_fi;
    isnt(prt_forest_fi = SecCertificateCreateWithBytes(NULL, prt_forest_fi_certificate,
                                                       sizeof(prt_forest_fi_certificate)), NULL, "create prt_forest_fi");
    CFArrayRef certs = NULL;
    isnt(certs = CFArrayCreate(NULL, &prt_forest_fi, 1, &kCFTypeArrayCallBacks), NULL, "failed to create cert array");
    SecPolicyRef policy = NULL;
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("owa.prt-forest.fi")), NULL, "failed to create policy");
    SecTrustRef trust = NULL;
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
              "create trust for ip client owa.prt-forest.fi");
    CFDateRef date = CFDateCreate(NULL, 391578321.0);
    ok_status(SecTrustSetVerifyDate(trust, date),
              "set owa.prt-forest.fi trust date to May 2013");
    
    SecKeyRef pubkey = SecTrustCopyKey(trust);
    isnt(pubkey, NULL, "pubkey returned");
    
    CFReleaseNull(certs);
    CFReleaseNull(prt_forest_fi);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(pubkey);
    CFReleaseNull(date);
}

- (void)testRSAKeySizes {
    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf2048A length:sizeof(_leaf2048A)]
                              cert1:[NSData dataWithBytes:_int2048A length:sizeof(_int2048A)]
                               root:[NSData dataWithBytes:_root512 length:sizeof(_root512)]
                             result:false
                      failureReason:nil],
       "SECURITY: failed to detect weak root");

    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf2048B length:sizeof(_leaf2048B)]
                              cert1:[NSData dataWithBytes:_int512 length:sizeof(_int512)]
                               root:[NSData dataWithBytes:_root2048 length:sizeof(_root2048)]
                             result:false
                      failureReason:nil],
       "SECURITY: failed to detect weak intermediate");

    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf512 length:sizeof(_leaf512)]
                              cert1:[NSData dataWithBytes:_int2048B length:sizeof(_int2048B)]
                               root:[NSData dataWithBytes:_root2048 length:sizeof(_root2048)]
                             result:false
                      failureReason:nil],
       "SECURITY: failed to detect weak leaf");

    NSString *failureReason = nil;
    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf1024 length:sizeof(_leaf1024)]
                              cert1:[NSData dataWithBytes:_int2048B length:sizeof(_int2048B)]
                               root:[NSData dataWithBytes:_root2048 length:sizeof(_root2048)]
                             result:true
                      failureReason:&failureReason],
       "REGRESSION: key size test 1024-bit leaf: %@", failureReason);

    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf2048C length:sizeof(_leaf2048C)]
                              cert1:[NSData dataWithBytes:_int2048B length:sizeof(_int2048B)]
                               root:[NSData dataWithBytes:_root2048 length:sizeof(_root2048)]
                             result:true
                      failureReason:&failureReason],
       "REGRESSION: key size test 2048-bit leaf: %@", failureReason);
}

- (void)testECKeySizes {
    /* Because CoreCrypto does not support P128, we fail to chain if any CAs use weakly sized curves */
    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf128 length:sizeof(_leaf128)]
                              cert1:[NSData dataWithBytes:_int384B length:sizeof(_int384B)]
                               root:[NSData dataWithBytes:_root384 length:sizeof(_root384)]
                             result:false
                      failureReason:nil],
       "SECURITY: failed to detect weak leaf");

    NSString *failureReason = nil;
    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf192 length:sizeof(_leaf192)]
                              cert1:[NSData dataWithBytes:_int384B length:sizeof(_int384B)]
                               root:[NSData dataWithBytes:_root384 length:sizeof(_root384)]
                             result:true
                      failureReason:&failureReason],
       "REGRESSION: key size test 192-bit leaf: %@", failureReason);

    ok([self run_chain_of_threetest:[NSData dataWithBytes:_leaf384C length:sizeof(_leaf384C)]
                              cert1:[NSData dataWithBytes:_int384B length:sizeof(_int384B)]
                               root:[NSData dataWithBytes:_root384 length:sizeof(_root384)]
                             result:true
                      failureReason:&failureReason],
       "REGRESSION: key size test 384-bit leaf: %@", failureReason);
}

- (bool)runTrust:(NSArray *)certs
         anchors:(NSArray *)anchors
          policy:(SecPolicyRef)policy
      verifyDate:(NSDate *)date
{
    SecTrustRef trust = NULL;
    XCTAssert(errSecSuccess == SecTrustCreateWithCertificates((__bridge CFArrayRef)certs, policy, &trust));
    if (anchors) {
        XCTAssert(errSecSuccess == SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors));
    }
    XCTAssert(errSecSuccess == SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date));

    CFErrorRef error = NULL;
    bool result = SecTrustEvaluateWithError(trust, &error);
    CFReleaseNull(error);
    CFReleaseNull(trust);
    return result;
}

- (void)test1024_appTrustedLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf1024SSL, sizeof(_leaf1024SSL));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _rootSSL, sizeof(_rootSSL));

    NSArray *certs = @[ (__bridge id)leaf];
    NSArray *anchor = @[ (__bridge id)root ];
    CFReleaseNull(leaf);
    CFReleaseNull(root);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertFalse([self runTrust:certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "anchor trusted 1024-bit cert succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "anchor trusted 1024-bit cert failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "anchor trusted 1024-bit cert failed for EAP");
    CFReleaseNull(eapPolicy);

    SecPolicyRef legacyPolicy = SecPolicyCreateLegacySSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:legacyPolicy verifyDate:verifyDate], "anchor trusted 1024-bit cert failed for legacy SSL policy");
    CFReleaseNull(legacyPolicy);

    SecPolicyRef legacyClientPolicy = SecPolicyCreateLegacySSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:legacyClientPolicy verifyDate:verifyDate], "anchor trusted 1024-bit cert failed for legacy SSL client policy");
    CFReleaseNull(legacyClientPolicy);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
- (void)test1024_trustSettingsOnRoot_TestLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf1024SSL, sizeof(_leaf1024SSL));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _rootSSL, sizeof(_rootSSL));
    NSArray *certs = @[ (__bridge id)leaf, (__bridge id)root ];
    CFReleaseNull(leaf);

    id persistentRef = [self addTrustSettingsForCert:root];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertFalse([self runTrust:certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on root, 1024-bit leaf succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on root, 1024-bit leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on root, 1024-bit leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(root);
}

- (void)test1024_trustSettingsOnLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf1024SSL, sizeof(_leaf1024SSL));
    NSArray *certs = @[ (__bridge id)leaf ];

    id persistentRef = [self addTrustSettingsForCert:leaf];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on 1024-bit leaf failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on 1024-bit leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on 1024-bit leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}
#endif // !TARGET_OS_BRIDGE

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have a system trust store
- (void)test2048_systemTrusted {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0]; // April 26, 2019 at 12:33:20 PM PDT

    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf2048SystemTrust, sizeof(_leaf2048SystemTrust));
    SecCertificateRef sha2_int = SecCertificateCreateWithBytes(NULL, _int2048SystemTrust, sizeof(_int2048SystemTrust));
    NSArray *certs = @[ (__bridge id)leaf, (__bridge id)sha2_int];
    CFReleaseNull(leaf);
    CFReleaseNull(sha2_int);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("www.badssl.com"));
    XCTAssertTrue([self runTrust:certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "system trusted 2048-bit certs failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "system trusted 2048-bit certs failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"*.badssl.com", @"badssl.com"]);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "system trusted 2048-bit certs failed for EAP");
    CFReleaseNull(eapPolicy);
}
#endif // !TARGET_OS_BRIDGE

- (void)test2048_appTrustedLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf2048SSL, sizeof(_leaf2048SSL));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _rootSSL, sizeof(_rootSSL));

    NSArray *certs = @[ (__bridge id)leaf];
    NSArray *anchor = @[ (__bridge id)root ];
    CFReleaseNull(leaf);
    CFReleaseNull(root);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "anchor trusted 2048-bit cert failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "anchor trusted 2048-bit cert failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "anchor trusted 2048-bit cert failed for EAP");
    CFReleaseNull(eapPolicy);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
- (void)test2048_trustSettingsOnRoot_TestLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf2048SSL, sizeof(_leaf2048SSL));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _rootSSL, sizeof(_rootSSL));
    NSArray *certs = @[ (__bridge id)leaf, (__bridge id)root ];
    CFReleaseNull(leaf);

    id persistentRef = [self addTrustSettingsForCert:root];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on root, 2048-bit leaf failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on root, 2048-bit leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on root, 2048-bit leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:root persistentRef:persistentRef];
    CFReleaseNull(root);
}

- (void)test2048_trustSettingsOnLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef leaf = SecCertificateCreateWithBytes(NULL, _leaf2048SSL, sizeof(_leaf2048SSL));
    NSArray *certs = @[ (__bridge id)leaf ];

    id persistentRef = [self addTrustSettingsForCert:leaf];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on 2048-bit leaf failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on 2048-bit leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on 2048-bit leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:leaf persistentRef:persistentRef];
    CFReleaseNull(leaf);
}
#endif // !TARGET_OS_BRIDGE

@end
