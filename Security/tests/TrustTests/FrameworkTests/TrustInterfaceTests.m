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
#include <Security/SecTrustInternal.h>
#include "OSX/utilities/SecCFWrappers.h"
#include "OSX/utilities/array_size.h"
#include "OSX/sec/ipc/securityd_client.h"

#include "../TestMacroConversions.h"
#include "../TrustEvaluationTestHelpers.h"
#include "TrustFrameworkTestCase.h"
#include "TrustInterfaceTests_data.h"

@interface TrustInterfaceTests : TrustFrameworkTestCase
@end

@implementation TrustInterfaceTests

- (void)testCreateWithCertificates {
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;

    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    const void *v_certs[] = { cert0, cert1 };

    certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(false, NULL);

    /* SecTrustCreateWithCertificates failures. */
    is_status(SecTrustCreateWithCertificates(kCFBooleanTrue, policy, &trust),
              errSecParam, "create trust with boolean instead of cert");
    is_status(SecTrustCreateWithCertificates(cert0, kCFBooleanTrue, &trust),
              errSecParam, "create trust with boolean instead of policy");

    NSArray *badValues = @[ [NSData dataWithBytes:_c0 length:sizeof(_c0)], [NSData dataWithBytes:_c1 length:sizeof(_c1)]];
    XCTAssert(errSecParam == SecTrustCreateWithCertificates((__bridge CFArrayRef)badValues, policy, &trust),
              "create trust with array of datas instead of certs");
    XCTAssert(errSecParam == SecTrustCreateWithCertificates(certs, (__bridge CFArrayRef)badValues, &trust),
              "create trust with array of datas instead of policies");

    /* SecTrustCreateWithCertificates using array of certs. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(certs);
    CFReleaseNull(policy);
}

- (void)testGetCertificate {
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;

    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    const void *v_certs[] = { cert0, cert1 };

    certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(false, NULL);

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* NOTE: prior to <rdar://11810677 SecTrustGetCertificateCount would return 1 at this point.
     * Now, however, we do an implicit SecTrustEvaluate to build the chain if it has not yet been
     * evaluated, so we now expect the full chain length. */
#if !TARGET_OS_BRIDGE
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");
#else
    /* bridgeOS has no system anchors, so trustd never finds the root */
    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");
#endif
    is(SecTrustGetCertificateAtIndex(trust, 0), cert0, "cert 0 is leaf");

    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(certs);
    CFReleaseNull(policy);
}

- (void)testRestoreOS {
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;
    SecTrustResultType trustResult = kSecTrustResultOtherError;

    /* Apr 14 2018. */
    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2018, 4, 14, 12, 0, 0),
         NULL, "create verify date");
    if (!date) { goto errOut; }
    
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    const void *v_certs[] = { cert0, cert1 };

    certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(false, NULL);

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    // Test Restore OS environment
    SecServerSetTrustdMachServiceName("com.apple.security.doesn't-exist");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust without securityd running");
    is_status(trustResult, kSecTrustResultInvalid, "trustResult is kSecTrustResultInvalid");
    is(SecTrustGetCertificateCount(trust), 1, "cert count is 1 without securityd running");
    SecKeyRef pubKey = NULL;
    ok(pubKey = SecTrustCopyKey(trust), "copy public key without securityd running");
    CFReleaseNull(pubKey);
    SecServerSetTrustdMachServiceName("com.apple.trustd");
    // End of Restore OS environment tests

errOut:
    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(certs);
    CFReleaseNull(policy);
}

- (void)testRestoreOSBag {
    SecTrustRef trust;
    SecCertificateRef leaf, root;
    SecPolicyRef policy;
    CFDataRef urlBagData;
    CFDictionaryRef urlBagDict;

    isnt(urlBagData = CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, url_bag, sizeof(url_bag), kCFAllocatorNull), NULL,
        "load url bag");
    isnt(urlBagDict = CFPropertyListCreateWithData(kCFAllocatorDefault, urlBagData, kCFPropertyListImmutable, NULL, NULL), NULL,
        "parse url bag");
    CFReleaseSafe(urlBagData);
    CFArrayRef certs_data = CFDictionaryGetValue(urlBagDict, CFSTR("certs"));
    CFDataRef cert_data = CFArrayGetValueAtIndex(certs_data, 0);
    isnt(leaf = SecCertificateCreateWithData(kCFAllocatorDefault, cert_data), NULL, "create leaf");
    isnt(root = SecCertificateCreateWithBytes(kCFAllocatorDefault, sITunesStoreRootCertificate, sizeof(sITunesStoreRootCertificate)), NULL, "create root");

    CFArrayRef certs = CFArrayCreate(kCFAllocatorDefault, (const void **)&leaf, 1, NULL);
    CFDataRef signature = CFDictionaryGetValue(urlBagDict, CFSTR("signature"));
    CFDataRef bag = CFDictionaryGetValue(urlBagDict, CFSTR("bag"));

    isnt(policy = SecPolicyCreateBasicX509(), NULL, "create policy instance");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust for leaf");

    // Test Restore OS environment bag signing verification
    SecServerSetTrustdMachServiceName("com.apple.security.doesn't-exist");
    SecTrustResultType trustResult;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    SecKeyRef pub_key_leaf;
    isnt(pub_key_leaf = SecTrustCopyKey(trust), NULL, "get leaf pub key");
    if (!pub_key_leaf) { goto errOut; }
    CFErrorRef error = NULL;
    ok(SecKeyVerifySignature(pub_key_leaf, kSecKeyAlgorithmRSASignatureMessagePKCS1v15SHA1, bag, signature, &error),
              "verify signature on bag");
    CFReleaseNull(error);
    SecServerSetTrustdMachServiceName("com.apple.trustd");
    // End of Restore OS environment tests

errOut:
    CFReleaseSafe(pub_key_leaf);
    CFReleaseSafe(urlBagDict);
    CFReleaseSafe(certs);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(leaf);
    CFReleaseSafe(root);
}

- (void)testAnchorCerts {
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;
    CFDateRef date = NULL;

    /* Apr 14 2018. */
    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2018, 4, 14, 12, 0, 0),
         NULL, "create verify date");
    if (!date) { goto errOut; }
    
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    const void *v_certs[] = { cert0, cert1 };

    certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(false, NULL);

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    anchors = CFArrayCreate(NULL, (const void **)&cert1, 1, &kCFTypeArrayCallBacks);
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");

    CFReleaseNull(anchors);
    anchors = CFArrayCreate(NULL, NULL, 0, NULL);
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set empty anchors list");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    CFReleaseNull(anchors);

    ok_status(SecTrustSetAnchorCertificatesOnly(trust, false), "trust passed in anchors and system anchors");
#if !TARGET_OS_BRIDGE
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
#else
    /* BridgeOS has no system anchors */
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
#endif

    ok_status(SecTrustSetAnchorCertificatesOnly(trust, true), "only trust passed in anchors (default)");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

    ok_status(SecTrustSetAnchorCertificates(trust, NULL), "reset anchors");
#if !TARGET_OS_BRIDGE
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");
#else
    /* bridgeOS has no system anchors */
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");
#endif

    XCTAssert(errSecParam == SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)@[ [NSData dataWithBytes:_c0 length:sizeof(_c0)]]),
              "set anchor with data instead of certificate");
#if !TARGET_OS_BRIDGE
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");
#else
    /* bridgeOS has no system anchors */
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");
    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");
#endif

errOut:
    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(policy);
}

- (void)testInputCertificates {
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certificates = NULL;

    require(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)), errOut);
    require(policy = SecPolicyCreateBasicX509(), errOut);
    require_noerr(SecTrustCreateWithCertificates(cert0, policy, &trust), errOut);

    ok_status(SecTrustCopyInputCertificates(trust, &certificates), "SecTrustCopyInputCertificates failed");
    is(CFArrayGetCount(certificates), 1, "got too many input certs back");
    is(CFArrayGetValueAtIndex(certificates, 0), cert0, "wrong input cert");
    CFReleaseNull(certificates);

    XCTAssert(errSecParam == SecTrustAddToInputCertificates(trust, (__bridge CFDataRef)[NSData dataWithBytes:_c1 length:sizeof(_c1)]),
              "add data instead of cert");
    XCTAssert(errSecParam == SecTrustAddToInputCertificates(trust, (__bridge CFArrayRef)@[[NSData dataWithBytes:_c1 length:sizeof(_c1)]]),
              "add array of data instead of cert");

    ok_status(SecTrustAddToInputCertificates(trust, cert0), "SecTrustAddToInputCertificates failed");
    ok_status(SecTrustCopyInputCertificates(trust, &certificates), "SecTrustCopyInputCertificates failed");
    is(CFArrayGetCount(certificates), 2, "got wrong number of input certs back");
    is(CFArrayGetValueAtIndex(certificates, 0), cert0, "wrong input cert0");
    is(CFArrayGetValueAtIndex(certificates, 1), cert0, "wrong input cert0");

    ok_status(SecTrustAddToInputCertificates(trust, (__bridge CFArrayRef)@[ (__bridge id)cert1]), "SecTrustAddToInputCertificates failed");
    ok_status(SecTrustCopyInputCertificates(trust, &certificates), "SecTrustCopyInputCertificates failed");
    is(CFArrayGetCount(certificates), 3, "got wrong number of input certs back");
    is(CFArrayGetValueAtIndex(certificates, 0), cert0, "wrong input cert0");
    is(CFArrayGetValueAtIndex(certificates, 1), cert0, "wrong input cert0");
    is(CFArrayGetValueAtIndex(certificates, 2), cert1, "wrong input cert1");
#if !TARGET_OS_BRIDGE
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");
#else
    /* bridgeOS has no system anchors, so trustd never finds the root */
    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");
#endif

errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(certificates);
}

- (void)testSetPolicies {
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL, replacementPolicy = NULL;
    SecTrustRef trust = NULL;
    CFDateRef date = NULL;
    CFArrayRef anchors = NULL, policies = NULL;

    require(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)), errOut);
    require(policy = SecPolicyCreateSSL(true, CFSTR("example.com")), errOut);
    require_noerr(SecTrustCreateWithCertificates(cert0, policy, &trust), errOut);

    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2018, 4, 14, 12, 0, 0),
         NULL, "create verify date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    anchors = CFArrayCreate(NULL, (const void **)&cert1, 1, &kCFTypeArrayCallBacks);
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    XCTAssertFalse(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

    /* replace with one policy */
    require(replacementPolicy = SecPolicyCreateSSL(true, CFSTR("store.apple.com")), errOut);
    ok_status(SecTrustSetPolicies(trust, replacementPolicy));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

    /* replace with policy array */
    CFReleaseNull(trust);
    require_noerr(SecTrustCreateWithCertificates(cert0, policy, &trust), errOut);
    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2018, 4, 14, 12, 0, 0),
         NULL, "create verify date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    policies = CFArrayCreate(kCFAllocatorDefault, (CFTypeRef*)&replacementPolicy, 1, &kCFTypeArrayCallBacks);
    ok_status(SecTrustSetPolicies(trust, policies));
    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "evaluate trust");

    /* replace with non-policy */
    XCTAssert(errSecParam == SecTrustSetPolicies(trust, cert0), "set with cert instead of policy");
    XCTAssert(errSecParam == SecTrustSetPolicies(trust, anchors), "set with array of certs instead of polciies");

    /* copy policies */
    CFReleaseNull(policies);
    ok_status(SecTrustCopyPolicies(trust, &policies));
    XCTAssertEqual(CFArrayGetCount(policies), 1, "one policy set");
    XCTAssertTrue(CFEqual(CFArrayGetValueAtIndex(policies, 0), replacementPolicy), "set policy is replacement policy");

errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(date);
    CFReleaseNull(anchors);
    CFReleaseNull(replacementPolicy);
    CFReleaseNull(policies);
}

- (void)testAsyncTrustEval {
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certificates = NULL;
    CFDateRef date = NULL;
    dispatch_queue_t queue = dispatch_queue_create("com.apple.trusttests.async", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

    XCTestExpectation *blockExpectation = [self expectationWithDescription:@"callback occurs"];

    require(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)), errOut);
    const void *v_certs[] = {
        cert0,
        cert1
    };
    certificates = CFArrayCreate(NULL, v_certs,
                                 array_size(v_certs),
                                 &kCFTypeArrayCallBacks);

    require(policy = SecPolicyCreateBasicX509(), errOut);
    require_noerr(SecTrustCreateWithCertificates(certificates, policy, &trust), errOut);

    /* Jul 30 2014. */
    require(date = CFDateCreateForGregorianZuluMoment(NULL, 2014, 7, 30, 12, 0, 0), errOut);
    require_noerr(SecTrustSetVerifyDate(trust, date), errOut);

    /* This shouldn't crash. */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    ok_status(SecTrustEvaluateAsync(trust, queue, ^(SecTrustRef  _Nonnull trustRef, SecTrustResultType trustResult) {
        if ((trustResult == kSecTrustResultProceed) || (trustResult == kSecTrustResultUnspecified)) {
            // Evaluation succeeded!
            SecKeyRef publicKey = SecTrustCopyKey(trustRef);
            XCTAssert(publicKey !=  NULL);
            CFReleaseSafe(publicKey);
        } else if (trustResult == kSecTrustResultRecoverableTrustFailure) {
            // Evaluation failed, but may be able to recover . . .
        } else {
            // Evaluation failed
        }
        [blockExpectation fulfill];
    }), "evaluate trust asynchronously");
    CFReleaseNull(trust);
#pragma clang diagnostic pop

    [self waitForExpectations:@[blockExpectation] timeout:1.0];

errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(policy);
    CFReleaseNull(certificates);
    CFReleaseNull(date);
}

- (void)testExpiredOnly {
    SecCertificateRef cert0 = NULL, cert1 = NULL, cert2 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certificates = NULL, roots = NULL;
    CFDateRef date = NULL;

    require(cert0 = SecCertificateCreateWithBytes(NULL, _expired_badssl, sizeof(_expired_badssl)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_dvss, sizeof(_comodo_rsa_dvss)), errOut);
    require(cert2 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_root, sizeof(_comodo_rsa_root)), errOut);

    const void *v_certs[] = {cert0, cert1 };
    certificates = CFArrayCreate(NULL, v_certs,
                                 array_size(v_certs),
                                 &kCFTypeArrayCallBacks);

    const void *v_roots[] = { cert2 };
    roots = CFArrayCreate(NULL, v_roots,
                          array_size(v_roots),
                          &kCFTypeArrayCallBacks);

    require(policy = SecPolicyCreateSSL(true, CFSTR("expired.badssl.com")), errOut);
    require_noerr(SecTrustCreateWithCertificates(certificates, policy, &trust), errOut);
    require_noerr(SecTrustSetAnchorCertificates(trust, roots), errOut);

    /* Mar 21 2017 (cert expired in 2015, so this will cause a validity error.) */
    require(date = CFDateCreateForGregorianZuluMoment(NULL, 2017, 3, 21, 12, 0, 0), errOut);
    require_noerr(SecTrustSetVerifyDate(trust, date), errOut);

    /* SecTrustIsExpiredOnly implicitly evaluates the trust */
    ok(SecTrustIsExpiredOnly(trust), "REGRESSION: has new error as well as expiration");

    CFReleaseNull(policy);
    require(policy = SecPolicyCreateSSL(true, CFSTR("expired.terriblessl.com")), errOut);
    require_noerr(SecTrustSetPolicies(trust, policy), errOut);
    /* expect a hostname mismatch as well as expiration */
    ok(!SecTrustIsExpiredOnly(trust), "REGRESSION: should have found multiple errors");

errOut:
    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(cert2);
    CFReleaseNull(policy);
    CFReleaseNull(certificates);
    CFReleaseNull(roots);
    CFReleaseNull(date);
}

- (void)testEvaluateWithError {
    SecCertificateRef cert0 = NULL, cert1 = NULL, cert2 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certificates = NULL, roots = NULL;
    CFDateRef date = NULL, validDate = NULL;
    CFErrorRef error = NULL;

    require(cert0 = SecCertificateCreateWithBytes(NULL, _expired_badssl, sizeof(_expired_badssl)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_dvss, sizeof(_comodo_rsa_dvss)), errOut);
    require(cert2 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_root, sizeof(_comodo_rsa_root)), errOut);

    const void *v_certs[] = {
        cert0,
        cert1,
        cert2,
    };
    certificates = CFArrayCreate(NULL, v_certs,
                                 array_size(v_certs),
                                 &kCFTypeArrayCallBacks);

    const void *v_roots[] = {
        cert2
    };
    roots = CFArrayCreate(NULL, v_roots,
                          array_size(v_roots),
                          &kCFTypeArrayCallBacks);

    require(policy = SecPolicyCreateSSL(true, CFSTR("expired.badssl.com")), errOut);
    require_noerr(SecTrustCreateWithCertificates(certificates, policy, &trust), errOut);
    require_noerr(SecTrustSetAnchorCertificates(trust, roots), errOut);

    /* April 10 2015 (cert expired in 2015) */
    require(validDate = CFDateCreateForGregorianZuluMoment(NULL, 2015, 4, 10, 12, 0, 0), errOut);
    require_noerr(SecTrustSetVerifyDate(trust, validDate), errOut);

    is(SecTrustEvaluateWithError(trust, &error), true, "wrong result for valid cert");
    is(error, NULL, "set error for passing trust evaluation");
    CFReleaseNull(error);

    /* Mar 21 2017 (cert expired in 2015, so this will cause a validity error.) */
    require(date = CFDateCreateForGregorianZuluMoment(NULL, 2017, 3, 21, 12, 0, 0), errOut);
    require_noerr(SecTrustSetVerifyDate(trust, date), errOut);

    /* expect expiration error */
    is(SecTrustEvaluateWithError(trust, &error), false, "wrong result for expired cert");
    isnt(error, NULL, "failed to set error for failing trust evaluation");
    is(CFErrorGetCode(error), errSecCertificateExpired, "Got wrong error code for evaluation");
    CFReleaseNull(error);

    CFReleaseNull(policy);
    require(policy = SecPolicyCreateSSL(true, CFSTR("expired.terriblessl.com")), errOut);
    require_noerr(SecTrustSetPolicies(trust, policy), errOut);

    /* expect a hostname mismatch as well as expiration; hostname mismatch must be a higher priority */
    is(SecTrustEvaluateWithError(trust, &error), false, "wrong result for expired cert with hostname mismatch");
    isnt(error, NULL, "failed to set error for failing trust evaluation");
    is(CFErrorGetCode(error), errSecHostNameMismatch, "Got wrong error code for evaluation");
    CFReleaseNull(error);

    /* expect only a hostname mismatch*/
    require_noerr(SecTrustSetVerifyDate(trust, validDate), errOut);
    is(SecTrustEvaluateWithError(trust, &error), false, "wrong result for valid cert with hostname mismatch");
    isnt(error, NULL, "failed to set error for failing trust evaluation");
    is(CFErrorGetCode(error), errSecHostNameMismatch, "Got wrong error code for evaluation");
    CFReleaseNull(error);

    /* pinning failure */
    CFReleaseNull(policy);
    require(policy = SecPolicyCreateAppleSSLPinned(CFSTR("test"), CFSTR("expired.badssl.com"),
                                                   NULL, CFSTR("1.2.840.113635.100.6.27.1")), errOut);
    require_noerr(SecTrustSetPolicies(trust, policy), errOut);

    is(SecTrustEvaluateWithError(trust, &error), false, "wrong result for valid cert with pinning failure");
    isnt(error, NULL, "failed to set error for failing trust evaluation");
    CFIndex errorCode = CFErrorGetCode(error);
    // failed checks: AnchorApple, LeafMarkerOid, or IntermediateMarkerOid
    ok(errorCode == errSecMissingRequiredExtension || errorCode == errSecInvalidRoot, "Got wrong error code for evaluation");
    CFReleaseNull(error);

    /* trust nothing, trust errors higher priority than hostname mismatch */
    CFReleaseNull(policy);
    require(policy = SecPolicyCreateSSL(true, CFSTR("expired.terriblessl.com")), errOut);
    require_noerr(SecTrustSetPolicies(trust, policy), errOut);

    CFReleaseNull(roots);
    roots = CFArrayCreate(NULL, NULL, 0, &kCFTypeArrayCallBacks);
    require_noerr(SecTrustSetAnchorCertificates(trust, roots), errOut);
    is(SecTrustEvaluateWithError(trust, &error), false, "wrong result for expired cert with hostname mismatch");
    isnt(error, NULL, "failed to set error for failing trust evaluation");
    is(CFErrorGetCode(error), errSecNotTrusted, "Got wrong error code for evaluation");
    CFReleaseNull(error);

errOut:
    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(cert2);
    CFReleaseNull(policy);
    CFReleaseNull(certificates);
    CFReleaseNull(roots);
    CFReleaseNull(date);
    CFReleaseNull(validDate);
    CFReleaseNull(error);
}

- (void)testSerialization {
    SecCertificateRef cert0 = NULL, cert1 = NULL, root = NULL;
    SecTrustRef trust = NULL, deserializedTrust = NULL;
    SecPolicyRef policy = NULL;
    CFArrayRef certs = NULL, anchors = NULL, deserializedCerts = NULL;
    CFDateRef date = NULL;
    CFDataRef serializedTrust = NULL;
    CFErrorRef error = NULL;

    require_action(cert0 = SecCertificateCreateWithBytes(NULL, _expired_badssl, sizeof(_expired_badssl)), errOut,
                   fail("unable to create cert"));
    require_action(cert1 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_dvss, sizeof(_comodo_rsa_dvss)), errOut,
                   fail("unable to create cert"));
    require_action(root = SecCertificateCreateWithBytes(NULL, _comodo_rsa_root, sizeof(_comodo_rsa_root)), errOut,
                   fail("unable to create cert"));

    const void *v_certs[] = { cert0, cert1 };
    require_action(certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks), errOut,
                   fail("unable to create array"));
    require_action(anchors = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks), errOut,
                   fail("unable to create anchors array"));
    require_action(date = CFDateCreateForGregorianZuluMoment(NULL, 2015, 4, 10, 12, 0, 0), errOut, fail("unable to create date"));
    
    require_action(policy = SecPolicyCreateBasicX509(), errOut, fail("unable to create policy"));

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "failed to create trust");
    require_noerr_action(SecTrustSetAnchorCertificates(trust, anchors), errOut,
                         fail("unable to set anchors"));
    require_noerr_action(SecTrustSetVerifyDate(trust, date), errOut, fail("unable to set verify date"));
    
    ok(serializedTrust = SecTrustSerialize(trust, NULL), "failed to serialize trust");
    ok(deserializedTrust = SecTrustDeserialize(serializedTrust, NULL), "Failed to deserialize trust");
    CFReleaseNull(serializedTrust);

    require_noerr_action(SecTrustCopyCustomAnchorCertificates(deserializedTrust, &deserializedCerts), errOut,
                         fail("unable to get anchors from deserialized trust"));
    ok(CFEqual(anchors, deserializedCerts), "Failed to get the same anchors after serialization/deserialization");
    CFReleaseNull(deserializedCerts);
    
    require_noerr_action(SecTrustCopyInputCertificates(trust, &deserializedCerts), errOut,
                         fail("unable to get input certificates from deserialized trust"));
    ok(CFEqual(certs, deserializedCerts), "Failed to get same input certificates after serialization/deserialization");
    CFReleaseNull(deserializedCerts);

    /* correct API behavior */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    is(SecTrustSerialize(NULL, &error), NULL, "serialize succeeded with null input");
    is(CFErrorGetCode(error), errSecParam, "Incorrect error code for bad serialization input");
    CFReleaseNull(error);
    is(SecTrustDeserialize(NULL, &error), NULL, "deserialize succeeded with null input");
    is(CFErrorGetCode(error), errSecParam, "Incorrect error code for bad deserialization input");
    CFReleaseNull(error);
#pragma clang diagnostic pop

errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(root);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(date);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(deserializedTrust);
}

- (void)testSerializationSCTs {
    SecCertificateRef certA = NULL, certCA_alpha = NULL, certCA_beta = NULL;
    NSURL *trustedLogsURL = [[NSBundle bundleForClass:[self class]] URLForResource:@"CTlogs"
                                                                     withExtension:@"plist"
                                                                      subdirectory:@"si-82-sectrust-ct-data"];
    CFArrayRef trustedLogs= CFBridgingRetain([NSArray arrayWithContentsOfURL:trustedLogsURL]);
    SecTrustRef trust = NULL, deserializedTrust = NULL;
    SecPolicyRef policy = SecPolicyCreateBasicX509(); // <rdar://problem/50066309> Need to generate new certs for CTTests
    NSData *proofA_1 = NULL, *proofA_2 = NULL;
    NSDate *date = [NSDate dateWithTimeIntervalSinceReferenceDate:447450000.0]; // March 7, 2015 at 11:40:00 AM PST
    CFErrorRef error = NULL;

    NSURL *url = [[NSBundle bundleForClass:[self class]] URLForResource:@"serverA" withExtension:@".cer" subdirectory:@"si-82-sectrust-ct-data"];
    NSData *certData = [NSData dataWithContentsOfURL:url];
    isnt(certA = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData),  NULL, "create certA");

    url = [[NSBundle bundleForClass:[self class]] URLForResource:@"CA_beta" withExtension:@".cer" subdirectory:@"si-82-sectrust-ct-data"];
    certData = [NSData dataWithContentsOfURL:url];
    isnt(certCA_alpha  = SecCertificateCreateWithData(kCFAllocatorDefault, (CFDataRef)certData),  NULL, "create ca-alpha cert");

    NSArray *anchors = @[ (__bridge id)certCA_alpha ];

    url = [[NSBundle bundleForClass:[self class]] URLForResource:@"serverA_proof_Alfa_3" withExtension:@".bin" subdirectory:@"si-82-sectrust-ct-data"];
    isnt(proofA_1 = [NSData dataWithContentsOfURL:url], NULL, "creat proofA_1");
    url = [[NSBundle bundleForClass:[self class]] URLForResource:@"serverA_proof_Bravo_3" withExtension:@".bin" subdirectory:@"si-82-sectrust-ct-data"];
    isnt(proofA_2 = [NSData dataWithContentsOfURL:url], NULL, "creat proofA_2");
    NSArray *scts = @[ proofA_1, proofA_2 ];

    /* Make a SecTrustRef and then serialize it */
    ok_status(SecTrustCreateWithCertificates(certA, policy, &trust), "failed to create trust object");
    ok_status(SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)anchors), "failed to set anchors");
    ok_status(SecTrustSetTrustedLogs(trust, trustedLogs), "failed to set trusted logs");
    ok_status(SecTrustSetVerifyDate(trust, (__bridge CFDateRef)date), "failed to set verify date");
    ok_status(SecTrustSetSignedCertificateTimestamps(trust, (__bridge CFArrayRef)scts), "failed to set SCTS");

    NSData *serializedTrust = CFBridgingRelease(SecTrustSerialize(trust, &error));
    isnt(serializedTrust, NULL, "failed to serialize trust: %@", error);

    /* Evaluate it to make sure it's CT */
    ok(SecTrustEvaluateWithError(trust, &error), "failed to evaluate trust: %@", error);
    NSDictionary *results = CFBridgingRelease(SecTrustCopyResult(trust));
    isnt(results[(__bridge NSString*)kSecTrustCertificateTransparency], NULL, "failed get CT result");
    ok([results[(__bridge NSString*)kSecTrustCertificateTransparency] boolValue], "CT failed");

    /* Make a new trust object by deserializing the previous trust object */
    ok(deserializedTrust = SecTrustDeserialize((__bridge CFDataRef)serializedTrust, &error), "failed to deserialize trust: %@", error);

    /* Evaluate the new one to make sure it's CT (because the SCTs were serialized) */
    ok(SecTrustEvaluateWithError(deserializedTrust, &error), "failed to evaluate trust: %@", error);
    results = CFBridgingRelease(SecTrustCopyResult(deserializedTrust));
    isnt(results[(__bridge NSString*)kSecTrustCertificateTransparency], NULL, "failed get CT result");
    ok([results[(__bridge NSString*)kSecTrustCertificateTransparency] boolValue], "CT failed");

    CFReleaseNull(certA);
    CFReleaseNull(certCA_alpha);
    CFReleaseNull(certCA_beta);
    CFReleaseNull(trustedLogs);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(deserializedTrust);
    CFReleaseNull(error);
}

- (void)testTLSAnalytics {
    xpc_object_t metric = xpc_dictionary_create(NULL, NULL, 0);
    ok(metric != NULL);

    const char *TLS_METRIC_PROCESS_IDENTIFIER = "process";
    const char *TLS_METRIC_CIPHERSUITE = "cipher_name";
    const char *TLS_METRIC_PROTOCOL_VERSION = "version";
    const char *TLS_METRIC_SESSION_RESUMED = "resumed";

    xpc_dictionary_set_string(metric, TLS_METRIC_PROCESS_IDENTIFIER, "super awesome unit tester");
    xpc_dictionary_set_uint64(metric, TLS_METRIC_CIPHERSUITE, 0x0304);
    xpc_dictionary_set_uint64(metric, TLS_METRIC_PROTOCOL_VERSION, 0x0304);
    xpc_dictionary_set_bool(metric, TLS_METRIC_SESSION_RESUMED, false);
    // ... TLS would fill in the rest

    // Invoke the callback
    CFErrorRef error = NULL;
    bool reported = SecTrustReportTLSAnalytics(CFSTR("TLSConnectionEvent"), metric, &error);
    ok(reported, "Failed to report analytics with error %@", error);
}

- (void)testEvaluateFastAsync
{
    SecCertificateRef cert0 = NULL, cert1 = NULL, cert2 = NULL;
    SecPolicyRef policy = NULL;
    __block SecTrustRef trust = NULL;
    NSArray *certificates = nil, *roots = nil;
    NSDate *validDate = nil;
    dispatch_queue_t queue = dispatch_queue_create("com.apple.trusttests.EvalAsync", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    __block XCTestExpectation *blockExpectation = [self expectationWithDescription:@"callback occurs"];

    cert0 = SecCertificateCreateWithBytes(NULL, _expired_badssl, sizeof(_expired_badssl));
    cert1 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_dvss, sizeof(_comodo_rsa_dvss));
    cert2 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_root, sizeof(_comodo_rsa_root));

    certificates = @[ (__bridge id)cert0, (__bridge id)cert1 ];
    roots = @[ (__bridge id)cert2 ];

    policy = SecPolicyCreateSSL(true, CFSTR("expired.badssl.com"));
    XCTAssert(errSecSuccess == SecTrustCreateWithCertificates((__bridge CFArrayRef)certificates, policy, &trust));
    XCTAssert(errSecSuccess == SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)roots));

    /* April 10 2015 (cert expired in 2015) */
    validDate = CFBridgingRelease(CFDateCreateForGregorianZuluMoment(NULL, 2015, 4, 10, 12, 0, 0));
    XCTAssert(errSecSuccess == SecTrustSetVerifyDate(trust, (__bridge CFDateRef)validDate));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    XCTAssert(errSecParam == SecTrustEvaluateFastAsync(trust, NULL, ^(SecTrustRef  _Nonnull trustRef, SecTrustResultType trustResult) {
        XCTAssert(false, "callback called with invalid parameter");
    }));
#pragma clang diagnostic pop

    /* expect success */
    dispatch_async(queue, ^{
        XCTAssert(errSecSuccess == SecTrustEvaluateFastAsync(trust, queue, ^(SecTrustRef  _Nonnull trustRef, SecTrustResultType trustResult) {
            XCTAssert(trustRef != NULL);
            XCTAssert(trustResult == kSecTrustResultUnspecified);
            [blockExpectation fulfill];
        }));
    });

    [self waitForExpectations:@[blockExpectation] timeout:1.0];

    /* Mar 21 2017 (cert expired in 2015, so this will cause a validity error.) */
    validDate = CFBridgingRelease(CFDateCreateForGregorianZuluMoment(NULL, 2017, 3, 21, 12, 0, 0));
    XCTAssert(errSecSuccess == SecTrustSetVerifyDate(trust, (__bridge CFDateRef)validDate));

    /* expect failure */
    blockExpectation = [self expectationWithDescription:@"callback occurs"];
    dispatch_async(queue, ^{
        XCTAssert(errSecSuccess == SecTrustEvaluateFastAsync(trust, queue, ^(SecTrustRef  _Nonnull trustRef, SecTrustResultType trustResult) {
            XCTAssert(trustRef != NULL);
            XCTAssert(trustResult == kSecTrustResultRecoverableTrustFailure);
            [blockExpectation fulfill];
        }));
    });

    [self waitForExpectations:@[blockExpectation] timeout:1.0];

    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(cert2);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testEvaluateAsyncWithError
{
    SecCertificateRef cert0 = NULL, cert1 = NULL, cert2 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    NSArray *certificates = nil, *roots = nil;
    NSDate *validDate = nil;
    dispatch_queue_t queue = dispatch_queue_create("com.apple.trusttests.EvalAsync", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    __block XCTestExpectation *blockExpectation = [self expectationWithDescription:@"callback occurs"];

    cert0 = SecCertificateCreateWithBytes(NULL, _expired_badssl, sizeof(_expired_badssl));
    cert1 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_dvss, sizeof(_comodo_rsa_dvss));
    cert2 = SecCertificateCreateWithBytes(NULL, _comodo_rsa_root, sizeof(_comodo_rsa_root));

    certificates = @[ (__bridge id)cert0, (__bridge id)cert1 ];
    roots = @[ (__bridge id)cert2 ];

    policy = SecPolicyCreateSSL(true, CFSTR("expired.badssl.com"));
    XCTAssert(errSecSuccess == SecTrustCreateWithCertificates((__bridge CFArrayRef)certificates, policy, &trust));
    XCTAssert(errSecSuccess == SecTrustSetAnchorCertificates(trust, (__bridge CFArrayRef)roots));

    /* April 10 2015 (cert expired in 2015) */
    validDate = CFBridgingRelease(CFDateCreateForGregorianZuluMoment(NULL, 2015, 4, 10, 12, 0, 0));
    XCTAssert(errSecSuccess == SecTrustSetVerifyDate(trust, (__bridge CFDateRef)validDate));

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    XCTAssert(errSecParam == SecTrustEvaluateAsyncWithError(trust, NULL, ^(SecTrustRef  _Nonnull trustRef, bool result, CFErrorRef  _Nullable error) {
        XCTAssert(false, "callback called with invalid parameter");
    }));
#pragma clang diagnostic pop

    /* expect success */
    dispatch_async(queue, ^{
        XCTAssert(errSecSuccess == SecTrustEvaluateAsyncWithError(trust, queue, ^(SecTrustRef  _Nonnull trustRef, bool result, CFErrorRef  _Nullable error) {
            XCTAssert(trust != NULL);
            XCTAssertTrue(result);
            XCTAssert(error == NULL);
            [blockExpectation fulfill];
        }));
    });

    [self waitForExpectations:@[blockExpectation] timeout:1.0];

    /* Mar 21 2017 (cert expired in 2015, so this will cause a validity error.) */
    validDate = CFBridgingRelease(CFDateCreateForGregorianZuluMoment(NULL, 2017, 3, 21, 12, 0, 0));
    XCTAssert(errSecSuccess == SecTrustSetVerifyDate(trust, (__bridge CFDateRef)validDate));

    /* expect expiration error */
    blockExpectation = [self expectationWithDescription:@"callback occurs"];
    dispatch_async(queue, ^{
        XCTAssert(errSecSuccess == SecTrustEvaluateAsyncWithError(trust, queue, ^(SecTrustRef  _Nonnull trustRef, bool result, CFErrorRef  _Nullable error) {
            XCTAssert(trust != NULL);
            XCTAssertFalse(result);
            XCTAssert(error != NULL);
            XCTAssert(CFErrorGetCode(error) == errSecCertificateExpired);
            [blockExpectation fulfill];
        }));
    });

    [self waitForExpectations:@[blockExpectation] timeout:1.0];

    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(cert2);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testCopyProperties_ios
{
    /* Test null input */
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
#if TARGET_OS_IPHONE
    XCTAssertEqual(NULL, SecTrustCopyProperties(NULL));
#else
    XCTAssertEqual(NULL, SecTrustCopyProperties_ios(NULL));
#endif
#pragma clang diagnostic pop

    NSURL *testPlist = nil;
    NSArray *testsArray = nil;

    testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:@"debugging" withExtension:@"plist"
                                                          subdirectory:@"TestCopyProperties_ios-data"];
    if (!testPlist) {
        testPlist = [[NSBundle bundleForClass:[self class]] URLForResource:nil withExtension:@"plist"
                                                              subdirectory:(NSString *)@"TestCopyProperties_ios-data"];
    }
    if (!testPlist) {
        fail("Failed to get tests plist from TestCopyProperties-data");
        return;
    }

    testsArray = [NSArray arrayWithContentsOfURL: testPlist];
    if (!testsArray) {
        fail("Failed to create array from plist");
        return;
    }

    [testsArray enumerateObjectsUsingBlock:^(NSDictionary *testDict, NSUInteger idx, BOOL * _Nonnull stop) {
        TestTrustEvaluation *testObj = [[TestTrustEvaluation alloc] initWithTrustDictionary:testDict];
        XCTAssertNotNil(testObj, "failed to create test object for %lu", (unsigned long)idx);

#if TARGET_OS_BRIDGE
        // Skip disabled bridgeOS tests on bridgeOS
        if (testObj.bridgeOSDisabled) {
            return;
        }
#endif

#if TARGET_OS_IPHONE
        NSArray *properties = CFBridgingRelease(SecTrustCopyProperties(testObj.trust));
#else
        NSArray *properties = CFBridgingRelease(SecTrustCopyProperties_ios(testObj.trust));
#endif

        if (testDict[@"ExpectedProperties"]) {
            XCTAssertEqualObjects(testDict[@"ExpectedProperties"], properties, @"%@ test failed", testObj.fullTestName);
        } else {
            XCTAssertNil(properties, @"%@ test failed", testObj.fullTestName);
        }
    }];
}

- (void)testCopyKey
{
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;

    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
         NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
         NULL, "create cert1");
    const void *v_certs[] = { cert0, cert1 };

    certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateSSL(false, NULL);

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    SecKeyRef trustPubKey = NULL, certPubKey = NULL;
    ok(trustPubKey = SecTrustCopyKey(trust), "copy public key without securityd running");
    ok(certPubKey = SecCertificateCopyKey(cert0));
    XCTAssert(CFEqualSafe(trustPubKey, certPubKey));


    CFReleaseNull(trustPubKey);
    CFReleaseNull(certPubKey);
    CFReleaseNull(trust);
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(certs);
    CFReleaseNull(policy);
}

- (void)testSetNetworkFetchAllowed
{
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;

    require(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)), errOut);
    require(policy = SecPolicyCreateSSL(true, CFSTR("example.com")), errOut);
    require_noerr(SecTrustCreateWithCertificates(cert0, policy, &trust), errOut);

    Boolean curAllow, allow;
    ok_status(SecTrustGetNetworkFetchAllowed(trust, &curAllow));
    allow = !curAllow; /* flip it and see if the setting sticks */
    ok_status(SecTrustSetNetworkFetchAllowed(trust, allow));
    ok_status(SecTrustGetNetworkFetchAllowed(trust, &curAllow));
    is((allow == curAllow), true, "network fetch toggle");

    /* <rdar://39514416> ensure trust with revocation policy returns the correct status */
    SecPolicyRef revocation = SecPolicyCreateRevocation(kSecRevocationUseAnyAvailableMethod);
    ok_status(SecTrustSetPolicies(trust, revocation));
    ok_status(SecTrustGetNetworkFetchAllowed(trust, &curAllow));
    is(curAllow, true, "network fetch set for revocation policy");

    SecPolicyRef basic = SecPolicyCreateBasicX509();
    CFMutableArrayRef policies = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(policies, basic);
    CFArrayAppendValue(policies, revocation);
    ok_status(SecTrustSetPolicies(trust, policies));
    ok_status(SecTrustGetNetworkFetchAllowed(trust, &curAllow));
    is(curAllow, true, "network fetch set for basic+revocation policy");
    CFReleaseNull(revocation);
    CFReleaseNull(basic);
    CFReleaseNull(policies);

    revocation = SecPolicyCreateRevocation(kSecRevocationNetworkAccessDisabled);
    ok_status(SecTrustSetPolicies(trust, revocation));
    ok_status(SecTrustGetNetworkFetchAllowed(trust, &curAllow));
    is(curAllow, false, "network fetch not set for revocation policy");
    CFReleaseNull(revocation);

errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

- (void)testSetOCSPResponses
{
    SecCertificateRef cert0 = NULL, cert1 = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;

    require(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)), errOut);
    require(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)), errOut);
    require(policy = SecPolicyCreateSSL(true, CFSTR("example.com")), errOut);
    require_noerr(SecTrustCreateWithCertificates(cert0, policy, &trust), errOut);

    CFDataRef resp = (CFDataRef) CFDataCreateMutable(NULL, 0);
    CFDataIncreaseLength((CFMutableDataRef)resp, 64); /* arbitrary length, zero-filled data */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    // NULL passed as 'trust' newly generates a warning, we need to suppress it in order to compile
    is_status(SecTrustSetOCSPResponse(NULL, resp), errSecParam, "SecTrustSetOCSPResponse param 1 check OK");
#pragma clang diagnostic pop
    is_status(SecTrustSetOCSPResponse(trust, NULL), errSecSuccess, "SecTrustSetOCSPResponse param 2 check OK");
    is_status(SecTrustSetOCSPResponse(trust, resp), errSecSuccess, "SecTrustSetOCSPResponse OK");
    CFReleaseSafe(resp);

errOut:
    CFReleaseNull(cert0);
    CFReleaseNull(cert1);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

@end
