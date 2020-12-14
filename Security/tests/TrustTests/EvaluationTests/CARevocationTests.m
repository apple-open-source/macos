/*
 * Copyright (c) 2020 Apple Inc. All Rights Reserved.
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
#include "trust/trustd/SecOCSPCache.h"

#import "TrustEvaluationTestCase.h"
#import "CARevocationTests_data.h"
#include "../TestMacroConversions.h"

@interface CARevocationTests : TrustEvaluationTestCase

@end

@implementation CARevocationTests

+ (id) CF_RETURNS_RETAINED SecCertificateCreateFromData:(uint8_t *)data length:(size_t)length
{
    if (!data || !length) { return NULL; }
    SecCertificateRef cert = SecCertificateCreateWithBytes(kCFAllocatorDefault, data, length);
    return (__bridge id)cert;
}

static NSArray *s_anchors = nil;
static NSDate *s_date_20201020 = nil;

- (void)setUp
{
    // Delete the OCSP cache between each test
    [super setUp];
    SecOCSPCacheDeleteContent(nil);

    SecCertificateRef root = (__bridge SecCertificateRef)[CARevocationTests SecCertificateCreateFromData:_acrootca length:sizeof(_acrootca)];
    s_anchors = @[ (__bridge id)root ];
    CFReleaseNull(root);
    s_date_20201020 = [NSDate dateWithTimeIntervalSinceReferenceDate:624900000.0];
}

/* Evaluate the given chain for SSL and return the trust results dictionary. */
- (NSDictionary *)eval_ca_trust:(NSArray *)certs
                        anchors:(NSArray *)anchors
                       hostname:(NSString *)hostname
                     verifyDate:(NSDate *)date
{
    /* Create the trust wrapper object */
    TestTrustEvaluation *trust = nil;
    SecPolicyRef policy = SecPolicyCreateSSL(true, (__bridge CFStringRef)hostname);
    XCTAssertNotNil(trust = [[TestTrustEvaluation alloc] initWithCertificates:certs policies:@[(__bridge id)policy]], "create trust failed");
    CFReleaseNull(policy);
    if (!trust) { return nil; }

    /* Set the optional properties */
    if (anchors) { trust.anchors = anchors; }
    if (date) { trust.verifyDate = date; }

    /* Evaluate */
    NSError *error = nil;
    XCTAssert([trust evaluate:&error], "failed trust evaluation: %@", error);
    return trust.resultDictionary;
}

#if !TARGET_OS_WATCH && !TARGET_OS_BRIDGE
/* watchOS and bridgeOS don't support networking in trustd */
- (void)testCARevocationAdditions
{
    /* Verify that the revocation server is potentially reachable */
    if (!ping_host("ocsp.apple.com")) {
        XCTAssert(false, "Unable to contact required network resource");
        return;
    }

    const CFStringRef TrustTestsAppID = CFSTR("com.apple.trusttests");
    //%%% TBD: add namespace tests using AnotherAppID
    //const CFStringRef AnotherAppID = CFSTR("com.apple.security.not-this-one");
    CFDictionaryRef copiedAdditions = NULL;
    CFErrorRef error = NULL;

    /* Verify no additions set initially */
    is(copiedAdditions = SecTrustStoreCopyCARevocationAdditions(NULL, NULL), NULL, "no revocation additions set");
    if (copiedAdditions) {
        CFReleaseNull(copiedAdditions);
        return;
    }

    NSDictionary *results = nil;
    SecCertificateRef leaf = NULL, ca = NULL;

    leaf = (__bridge SecCertificateRef)[CARevocationTests SecCertificateCreateFromData:_acleaf length:sizeof(_acleaf)];
    XCTAssertNotNil((__bridge id)leaf, "create leaf");
    ca = (__bridge SecCertificateRef)[CARevocationTests SecCertificateCreateFromData:_acserverca1 length:sizeof(_acserverca1)];
    XCTAssertNotNil((__bridge id)ca, "create ca");

    /* We do not expect a revocation check for this CA until explicitly set up */
    results = [self eval_ca_trust:@[(__bridge id)leaf, (__bridge id)ca] anchors:s_anchors hostname:@"radar.apple.com" verifyDate:s_date_20201020];
    XCTAssertNotNil(results, "failed to obtain trust results");
    XCTAssertNil(results[(__bridge NSString*)kSecTrustRevocationChecked], "revocation checked when not expected");

    /* Set addition for intermediate CA with implied AppID */
    CFDataRef caSPKIHash = SecCertificateCopySubjectPublicKeyInfoSHA256Digest(ca);
    NSDictionary *caAddition = @{
        (__bridge NSString*)kSecCARevocationHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecCARevocationSPKIHashKey : (__bridge NSData*)caSPKIHash,
    };
    NSDictionary *additions1 = @{
        (__bridge NSString*)kSecCARevocationAdditionsKey : @[ caAddition ]
    };
    ok(SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)additions1, &error), "failed to set ca addition for this app: %@", error);
    CFReleaseNull(caSPKIHash);

    /* Check that the additions were saved and can be retrieved */
    ok(copiedAdditions = SecTrustStoreCopyCARevocationAdditions(TrustTestsAppID, &error), "failed to copy ca additions for TrustTests app id: %@", error);
    /* Same as what we saved? */
    ok([additions1 isEqualToDictionary:(__bridge NSDictionary*)copiedAdditions], "got wrong additions back");
    CFReleaseNull(copiedAdditions);

    /* This time, the revocation check should take place on the leaf. */
    results = [self eval_ca_trust:@[(__bridge id)leaf, (__bridge id)ca] anchors:s_anchors hostname:@"radar.apple.com" verifyDate:s_date_20201020];
    XCTAssertNotNil(results, "failed to obtain trust results");
    // %%% This check should be replaced with a different key: rdar://70669949
    XCTAssertNil(results[(__bridge NSString*)kSecTrustRevocationChecked], "revocation not checked");

    /* Set empty array to clear the previously-set additions */
    NSDictionary *emptyAdditions = @{
        (__bridge NSString*)kSecCARevocationAdditionsKey : @[]
    };
    ok(SecTrustStoreSetCARevocationAdditions(TrustTestsAppID, (__bridge CFDictionaryRef)emptyAdditions, &error), "failed to set empty additions");
    /* Did the additions get cleared? */
    is(copiedAdditions = SecTrustStoreCopyCARevocationAdditions(TrustTestsAppID, &error), NULL, "additions still present after being cleared");
    CFReleaseNull(copiedAdditions);

    /* Set addition for root CA with implied AppID */
    CFDataRef rootSPKIHash = SecSHA256DigestCreate(NULL, _acrootca_spki, sizeof(_acrootca_spki));
    NSDictionary *rootAddition = @{
        (__bridge NSString*)kSecCARevocationHashAlgorithmKey : @"sha256",
        (__bridge NSString*)kSecCARevocationSPKIHashKey : (__bridge NSData*)rootSPKIHash,
    };
    NSDictionary *additions2 = @{
        (__bridge NSString*)kSecCARevocationAdditionsKey : @[ rootAddition ]
    };
    ok(SecTrustStoreSetCARevocationAdditions(NULL, (__bridge CFDictionaryRef)additions2, &error), "failed to set root addition for this app: %@", error);

    /* Check that the additions were saved and can be retrieved */
    ok(copiedAdditions = SecTrustStoreCopyCARevocationAdditions(TrustTestsAppID, &error), "failed to copy root additions for TrustTests app id: %@", error);
    /* Same as what we saved? */
    ok([additions2 isEqualToDictionary:(__bridge NSDictionary*)copiedAdditions], "got wrong additions back");
    CFReleaseNull(copiedAdditions);

    /* Clear OCSP cache so we know whether next evaluation attempts to check */
    SecOCSPCacheDeleteContent(nil);

    /* Revocation check should take place with a CA addition set for the root. */
    results = [self eval_ca_trust:@[(__bridge id)leaf, (__bridge id)ca] anchors:s_anchors hostname:@"radar.apple.com" verifyDate:s_date_20201020];
    XCTAssertNotNil(results, "failed to obtain trust results");
    // %%% This check should be replaced with a different key: rdar://70669949
    XCTAssertNotNil(results[(__bridge NSString*)kSecTrustRevocationChecked], "revocation not checked");

    CFReleaseNull(leaf);
    CFReleaseNull(ca);
}

#else // TARGET_OS_BRIDGE || TARGET_OS_WATCH
- (void)testSkipTests
{
    XCTAssert(true);
}
#endif // TARGET_OS_BRIDGE || TARGET_OS_WATCH
@end

