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
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecPolicyPriv.h>
#include "OSX/utilities/array_size.h"
#include "OSX/utilities/SecCFWrappers.h"

#import "TrustEvaluationTestCase.h"
#include "../TestMacroConversions.h"
#include "EvaluationBasicTests_data.h"

@interface EvaluationBasicTests : TrustEvaluationTestCase
@end

@implementation EvaluationBasicTests

- (void)testOptionalPolicyCheck {
    SecCertificateRef cert0 = NULL, cert1 = NULL, root = NULL;
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef date = NULL;
    
    require_action(cert0 = SecCertificateCreateWithBytes(NULL, _eval_expired_badssl, sizeof(_eval_expired_badssl)), errOut,
                   fail("unable to create cert"));
    require_action(cert1 = SecCertificateCreateWithBytes(NULL, _eval_comodo_rsa_dvss, sizeof(_eval_comodo_rsa_dvss)), errOut,
                   fail("unable to create cert"));
    require_action(root = SecCertificateCreateWithBytes(NULL, _eval_comodo_rsa_root, sizeof(_eval_comodo_rsa_root)), errOut,
                   fail("unable to create cert"));
    
    const void *v_certs[] = { cert0, cert1 };
    require_action(certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks), errOut,
                   fail("unable to create array"));
    require_action(anchors = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks), errOut,
                   fail("unable to create anchors array"));
    require_action(date = CFDateCreateForGregorianZuluMoment(NULL, 2015, 4, 10, 12, 0, 0), errOut, fail("unable to create date"));
    
    require_action(policy = SecPolicyCreateBasicX509(), errOut, fail("unable to create policy"));
    SecPolicySetOptionsValue(policy, CFSTR("not-a-policy-check"), kCFBooleanTrue);
    
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "failed to create trust");
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut,
                         fail("unable to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, date), errOut, fail("unable to set verify date"));
    
#if NDEBUG
    ok(SecTrustEvaluateWithError(trust, NULL), "Trust evaluation failed");
#else
    is(SecTrustEvaluateWithError(trust, NULL), false, "Expect failure in Debug config");
#endif
    
errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(root);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(date);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

#if !TARGET_OS_BRIDGE
- (void)testIntermediateFromKeychain {
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL, framework_cert1 = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    CFDictionaryRef query = NULL;
    
    /* Apr 14 2018. */
    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2018, 4, 14, 12, 0, 0),
         NULL, "create verify date");
    if (!date) { goto errOut; }
    
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _eval_c0, sizeof(_eval_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _eval_c1, sizeof(_eval_c1)),
         NULL, "create cert1");
    policy = SecPolicyCreateSSL(false, NULL);
    
    /* Test cert_1 intermediate from the keychain. */
    ok_status(SecTrustCreateWithCertificates(cert0, policy, &trust),
              "create trust with single cert0");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustSetNetworkFetchAllowed(trust, false), "set no network fetch allowed");
    
    // Add cert1 to the keychain
    isnt(framework_cert1 = SecFrameworkCertificateCreate(_eval_c1, sizeof(_eval_c1)),
         NULL, "create framework cert1");
    query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                         kSecClass, kSecClassCertificate,
                                         kSecValueRef, framework_cert1,
                                         kSecAttrAccessGroup, CFSTR("com.apple.trusttests"),
#if TARGET_OS_OSX
                                         kSecUseDataProtectionKeychain, kCFBooleanTrue,
#endif
                                         NULL);
    ok_status(SecItemAdd(query, NULL), "add cert1 to keychain");
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust and expect success");
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");

    // Cleanup added cert1.
    ok_status(SecItemDelete(query), "remove cert1 from keychain");
    CFReleaseNull(query);
    CFReleaseNull(framework_cert1);
    
errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(certs);
    CFReleaseNull(date);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}
#endif /* !TARGET_OS_BRIDGE */

- (void)testSelfSignedAnchor {
    SecCertificateRef garthc2 =  NULL;
    CFArrayRef certs = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;

    isnt(garthc2 = SecCertificateCreateWithBytes(NULL, _selfSignedAnchor,
                                                 sizeof(_selfSignedAnchor)), NULL, "create self-signed anchor");
    certs = CFArrayCreate(NULL, (const void **)&garthc2, 1, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(true, NULL);
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
              "create trust for self-signed anchor");
    date = CFDateCreate(NULL, 578000000.0); // April 26, 2019 at 12:33:20 PM PDT
    ok_status(SecTrustSetVerifyDate(trust, date),
              "set garthc2 trust date to April 2019");
    ok_status(SecTrustSetAnchorCertificates(trust, certs),
              "set garthc2 as anchor");
    XCTAssert(SecTrustEvaluateWithError(trust, NULL),
              "evaluate self signed cert with cert as anchor");
    
    CFReleaseNull(garthc2);
    CFReleaseNull(certs);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(date);
}

@end
