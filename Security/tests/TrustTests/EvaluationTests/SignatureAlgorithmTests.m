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
#include <utilities/SecCFRelease.h>
#include <Security/SecTrustSettings.h>

#include "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "SignatureAlgorithmTests_data.h"

@interface SignatureAlgorithmTests : TrustEvaluationTestCase
@end

@implementation SignatureAlgorithmTests

- (void)testMD5Root {
    SecCertificateRef md5_root = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;
    
    require_action(md5_root = SecCertificateCreateWithBytes(NULL, _md5_root, sizeof(_md5_root)), errOut,
                   fail("failed to create md5 root cert"));
    
    require_action(anchors = CFArrayCreate(NULL, (const void **)&md5_root, 1, &kCFTypeArrayCallBacks), errOut,
                   fail("failed to create anchors array"));
    require_action(verifyDate = CFDateCreate(NULL, 550600000), errOut, fail("failed to make verification date")); // June 13, 2018
    
    /* Test self-signed MD5 cert. Should work since cert is a trusted anchor - rdar://39152516 */
    require_noerr_action(SecTrustCreateWithCertificates(md5_root, NULL, &trust), errOut,
                         fail("failed to create trust object"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut,
                         fail("faild to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut,
                         fail("failed to set verify date"));
    ok(SecTrustEvaluateWithError(trust, &error), "self-signed MD5 cert failed");
    is(error, NULL, "got a trust error for self-signed MD5 cert: %@", error);

errOut:
    CFReleaseNull(error);
    CFReleaseNull(trust);
    CFReleaseNull(anchors);
    CFReleaseNull(verifyDate);
    CFReleaseNull(md5_root);
}

- (void)testMD5Leaf {
    SecCertificateRef md5_leaf = NULL, sha256_root = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef anchors = NULL;
    CFDateRef verifyDate = NULL;
    CFErrorRef error = NULL;
    
    require_action(md5_leaf = SecCertificateCreateWithBytes(NULL, _md5_leaf, sizeof(_md5_leaf)), errOut,
                   fail("failed to create md5 leaf cert"));
    require_action(sha256_root = SecCertificateCreateWithBytes(NULL, _sha256_root, sizeof(_sha256_root)), errOut,
                   fail("failed to create sha256 root cert"));
    
    require_action(anchors = CFArrayCreate(NULL, (const void **)&sha256_root, 1, &kCFTypeArrayCallBacks), errOut,
                   fail("failed to create anchors array"));
    require_action(verifyDate = CFDateCreate(NULL, 550600000), errOut, fail("failed to make verification date")); // June 13, 2018
    
    /* Test non-self-signed MD5 cert. Should fail. */
    require_noerr_action(SecTrustCreateWithCertificates(md5_leaf, NULL, &trust), errOut,
                         fail("failed to create trust object"));
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut,
                         fail("faild to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, verifyDate), errOut,
                         fail("failed to set verify date"));
    is(SecTrustEvaluateWithError(trust, &error), false, "non-self-signed MD5 cert succeeded");
    if (error) {
        is(CFErrorGetCode(error), errSecInvalidDigestAlgorithm, "got wrong error code for MD5 leaf cert, got %ld, expected %d",
           (long)CFErrorGetCode(error), (int)errSecInvalidDigestAlgorithm);
    } else {
        fail("expected trust evaluation to fail and it did not.");
    }
    
errOut:
    CFReleaseNull(md5_leaf);
    CFReleaseNull(sha256_root);
    CFReleaseNull(anchors);
    CFReleaseNull(verifyDate);
    CFReleaseNull(trust);
    CFReleaseNull(error);
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

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have a system trust store
- (void)testSHA1_systemTrusted {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0]; // November 4, 2016 at 5:53:20 PM PDT
    SecCertificateRef sha1_leaf = SecCertificateCreateWithBytes(NULL, _badssl_sha1, sizeof(_badssl_sha1));
    SecCertificateRef sha1_int = SecCertificateCreateWithBytes(NULL, _digiCertSSCA, sizeof(_digiCertSSCA));
    NSArray *sha1_certs = @[ (__bridge id)sha1_leaf, (__bridge id)sha1_int];
    CFReleaseNull(sha1_leaf);
    CFReleaseNull(sha1_int);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("www.badssl.com"));
    XCTAssertFalse([self runTrust:sha1_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "system trusted SHA1 certs succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "system trusted SHA1 certs failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"www.badssl.com"]);
    XCTAssertFalse([self runTrust:sha1_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "system trusted SHA1 certs succeeded for EAP");
    CFReleaseNull(eapPolicy);
}
#endif // !TARGET_OS_BRIDGE

- (void)testSHA1_appTrustedLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0]; // November 4, 2016 at 5:53:20 PM PDT
    SecCertificateRef sha1_leaf = SecCertificateCreateWithBytes(NULL, _badssl_sha1, sizeof(_badssl_sha1));
    SecCertificateRef sha1_int = SecCertificateCreateWithBytes(NULL, _digiCertSSCA, sizeof(_digiCertSSCA));
    SecCertificateRef sha1_root = SecCertificateCreateWithBytes(NULL, _digiCertRoot, sizeof(_digiCertRoot));

    NSArray *sha1_certs = @[ (__bridge id)sha1_leaf, (__bridge id)sha1_int];
    NSArray *anchor = @[ (__bridge id)sha1_root ];
    CFReleaseNull(sha1_leaf);
    CFReleaseNull(sha1_int);
    CFReleaseNull(sha1_root);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("www.badssl.com"));
    XCTAssertFalse([self runTrust:sha1_certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "anchor trusted SHA1 certs succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "anchor trusted SHA1 certs failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"*.badssl.com", @"badssl.com"]);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "anchor trusted SHA1 certs failed for EAP");
    CFReleaseNull(eapPolicy);

    SecPolicyRef legacyPolicy = SecPolicyCreateLegacySSL(true, CFSTR("www.badssl.com"));
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:legacyPolicy verifyDate:verifyDate], "anchor trusted SHA1 certs failed for legacy SSL server");
    CFReleaseNull(legacyPolicy);

    SecPolicyRef legacyClientPolicy = SecPolicyCreateLegacySSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:legacyClientPolicy verifyDate:verifyDate], "anchor trusted SHA1 certs failed for legacy SSL client");
    CFReleaseNull(legacyClientPolicy);
}

- (void)testSHA1_appTrustedSelfSigned {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha1_cert = SecCertificateCreateWithBytes(NULL, _testSHA1SelfSigned, sizeof(_testSHA1SelfSigned));
    NSArray *sha1_certs = @[ (__bridge id)sha1_cert ];
    NSArray *anchor = @[ (__bridge id)sha1_cert ];
    CFReleaseNull(sha1_cert);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "anchor trusted self-signed SHA1 cert failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "anchor trusted self-signed SHA1 cert failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "anchor trusted self-signed SHA1 cert failed for EAP");
    CFReleaseNull(eapPolicy);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
- (void)testSHA1_trustSettingsOnRoot_TestLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha1_leaf = SecCertificateCreateWithBytes(NULL, _testSHA1Leaf, sizeof(_testSHA1Leaf));
    SecCertificateRef sha1_root = SecCertificateCreateWithBytes(NULL, _testRoot, sizeof(_testRoot));
    NSArray *sha1_certs = @[ (__bridge id)sha1_leaf, (__bridge id)sha1_root ];
    CFReleaseNull(sha1_leaf);

    id persistentRef = [self addTrustSettingsForCert:sha1_root];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertFalse([self runTrust:sha1_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on root, SHA1 leaf succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on root, SHA1 leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on root, SHA1 leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha1_root persistentRef:persistentRef];
    CFReleaseNull(sha1_root);
}

- (void)testSHA1_trustSettingsOnLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha1_leaf = SecCertificateCreateWithBytes(NULL, _testSHA1Leaf, sizeof(_testSHA1Leaf));
    NSArray *sha1_certs = @[ (__bridge id)sha1_leaf ];

    id persistentRef = [self addTrustSettingsForCert:sha1_leaf];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on SHA1 leaf failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on SHA1 leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on SHA1 leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha1_leaf persistentRef:persistentRef];
    CFReleaseNull(sha1_leaf);
}

- (void)testSHA1_trustSettingsSelfSigned {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha1_cert = SecCertificateCreateWithBytes(NULL, _testSHA1SelfSigned, sizeof(_testSHA1SelfSigned));
    NSArray *sha1_certs = @[ (__bridge id)sha1_cert ];

    id persistentRef = [self addTrustSettingsForCert:sha1_cert];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings self-signed SHA1 cert failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings self-signed SHA1 cert failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha1_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings self-signed SHA1 cert failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha1_cert persistentRef:persistentRef];
    CFReleaseNull(sha1_cert);
}

- (void)testSHA1_denyTrustSettings {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha1_leaf = SecCertificateCreateWithBytes(NULL, _testSHA1Leaf, sizeof(_testSHA1Leaf));
    NSArray *sha1_certs = @[ (__bridge id)sha1_leaf ];

    id persistentRef = [self addTrustSettingsForCert:sha1_leaf trustSettings: @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultDeny)}];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertFalse([self runTrust:sha1_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "deny trust settings on SHA1 leaf succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertFalse([self runTrust:sha1_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "deny trust settings on SHA1 leaf succeeded for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertFalse([self runTrust:sha1_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "deny trust settings on SHA1 leaf succeeded for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha1_leaf persistentRef:persistentRef];
    CFReleaseNull(sha1_leaf);
}

- (void)testSHA1_unspecifiedTrustSettings {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha1_leaf = SecCertificateCreateWithBytes(NULL, _testSHA1Leaf, sizeof(_testSHA1Leaf));
    SecCertificateRef sha1_root = SecCertificateCreateWithBytes(NULL, _testRoot, sizeof(_testRoot));
    NSArray *sha1_certs = @[ (__bridge id)sha1_leaf ];
    NSArray *anchor = @[ (__bridge id)sha1_root ];
    CFReleaseNull(sha1_root);

    id persistentRef = [self addTrustSettingsForCert:sha1_leaf trustSettings: @{ (__bridge NSString*)kSecTrustSettingsResult: @(kSecTrustSettingsResultUnspecified)}];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertFalse([self runTrust:sha1_certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "unspecified trust settings on SHA1 leaf succeeded for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "unspecified trust settings on SHA1 leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha1_certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "unspecified trust settings on SHA1 leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha1_leaf persistentRef:persistentRef];
    CFReleaseNull(sha1_leaf);
}
#endif // !TARGET_OS_BRIDGE

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have a system trust store
- (void)testSHA2_systemTrusted {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0]; // November 4, 2016 at 5:53:20 PM PDT

    SecCertificateRef sha2_leaf = SecCertificateCreateWithBytes(NULL, _badssl_sha2, sizeof(_badssl_sha2));
    SecCertificateRef sha2_int = SecCertificateCreateWithBytes(NULL, _COMODO_DV, sizeof(_COMODO_DV));
    NSArray *sha2_certs = @[ (__bridge id)sha2_leaf, (__bridge id)sha2_int];
    CFReleaseNull(sha2_leaf);
    CFReleaseNull(sha2_int);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("www.badssl.com"));
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "system trusted SHA2 certs failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "system trusted SHA2 certs failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"*.badssl.com", @"badssl.com"]);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "system trusted SHA2 certs failed for EAP");
    CFReleaseNull(eapPolicy);
}
#endif

- (void)testSHA2_appTrustedLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:500000000.0]; // November 4, 2016 at 5:53:20 PM PDT

    SecCertificateRef sha2_leaf = SecCertificateCreateWithBytes(NULL, _badssl_sha2, sizeof(_badssl_sha2));
    SecCertificateRef sha2_int = SecCertificateCreateWithBytes(NULL, _COMODO_DV, sizeof(_COMODO_DV));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _COMODO_root, sizeof(_COMODO_root));

    NSArray *sha2_certs = @[ (__bridge id)sha2_leaf, (__bridge id)sha2_int];
    NSArray *anchor = @[ (__bridge id)root ];

    CFReleaseNull(sha2_leaf);
    CFReleaseNull(sha2_int);
    CFReleaseNull(root);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("www.badssl.com"));
    XCTAssertTrue([self runTrust:sha2_certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "anchor trusted SHA2 certs failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha2_certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "anchor trusted SHA2 certs failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"*.badssl.com", @"badssl.com"]);
    XCTAssertTrue([self runTrust:sha2_certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "anchor trusted SHA2 certs failed for EAP");
    CFReleaseNull(eapPolicy);
}

- (void)testSHA2_appTrustedSelfSigned {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha2_cert = SecCertificateCreateWithBytes(NULL, _testSHA2SelfSigned, sizeof(_testSHA2SelfSigned));
    NSArray *sha2_certs = @[ (__bridge id)sha2_cert ];
    NSArray *anchor = @[ (__bridge id)sha2_cert ];
    CFReleaseNull(sha2_cert);

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha2_certs anchors:anchor policy:serverPolicy verifyDate:verifyDate], "anchor trusted self-signed SHA2 cert failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha2_certs anchors:anchor policy:clientPolicy verifyDate:verifyDate], "anchor trusted self-signed SHA2 cert failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha2_certs anchors:anchor policy:eapPolicy verifyDate:verifyDate], "anchor trusted self-signed SHA2 cert failed for EAP");
    CFReleaseNull(eapPolicy);
}

#if !TARGET_OS_BRIDGE // bridgeOS doesn't have trust settings
- (void)testSHA2_trustSettingsOnRoot_TestLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha2_leaf = SecCertificateCreateWithBytes(NULL, _testSHA2Leaf, sizeof(_testSHA2Leaf));
    SecCertificateRef sha2_root = SecCertificateCreateWithBytes(NULL, _testRoot, sizeof(_testRoot));
    NSArray *sha2_certs = @[ (__bridge id)sha2_leaf, (__bridge id)sha2_root ];
    CFReleaseNull(sha2_leaf);

    id persistentRef = [self addTrustSettingsForCert:sha2_root];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on root, SHA2 leaf failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on root, SHA2 leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on root, SHA2 leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha2_root persistentRef:persistentRef];
    CFReleaseNull(sha2_root);
}

- (void)testSHA2_trustSettingsOnLeaf {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha2_leaf = SecCertificateCreateWithBytes(NULL, _testSHA2Leaf, sizeof(_testSHA2Leaf));
    NSArray *sha2_certs = @[ (__bridge id)sha2_leaf ];

    id persistentRef = [self addTrustSettingsForCert:sha2_leaf];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings on SHA2 leaf failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings on SHA2 leaf failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings on SHA2 leaf failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha2_leaf persistentRef:persistentRef];
    CFReleaseNull(sha2_leaf);
}

- (void)testSHA2_trustSettingsSelfSigned {
    NSDate *verifyDate = [NSDate dateWithTimeIntervalSinceReferenceDate:578000000.0]; // April 26, 2019 at 12:33:20 PM PDT
    SecCertificateRef sha2_cert = SecCertificateCreateWithBytes(NULL, _testSHA2SelfSigned, sizeof(_testSHA2SelfSigned));
    NSArray *sha2_certs = @[ (__bridge id)sha2_cert ];

    id persistentRef = [self addTrustSettingsForCert:sha2_cert];

    SecPolicyRef serverPolicy = SecPolicyCreateSSL(true, CFSTR("example.com"));
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:serverPolicy verifyDate:verifyDate], "trust settings self-signed SHA2 cert failed for SSL server");
    CFReleaseNull(serverPolicy);

    SecPolicyRef clientPolicy = SecPolicyCreateSSL(false, NULL);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:clientPolicy verifyDate:verifyDate], "trust settings self-signed SHA2 cert failed for SSL client");
    CFReleaseNull(clientPolicy);

    SecPolicyRef eapPolicy = SecPolicyCreateEAP(true, (__bridge CFArrayRef)@[@"example.com"]);
    XCTAssertTrue([self runTrust:sha2_certs anchors:nil policy:eapPolicy verifyDate:verifyDate], "trust settings self-signed SHA2 cert failed for EAP");
    CFReleaseNull(eapPolicy);

    [self removeTrustSettingsForCert:sha2_cert persistentRef:persistentRef];
    CFReleaseNull(sha2_cert);
}
#endif // !TARGET_OS_BRIDGE

@end
