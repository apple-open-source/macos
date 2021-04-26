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
#include "SecCertificatePriv.h"
#include "SecPolicyPriv.h"
#include "SecTrustPriv.h"
#include "OSX/utilities/SecCFWrappers.h"
#include <utilities/array_size.h>

#include "../TestMacroConversions.h"
#include "TrustEvaluationTestCase.h"
#include "iAPTests_data.h"

@interface iAPTests : TrustEvaluationTestCase
@end

@implementation iAPTests

- (void)testiAPNegativeSignatures {
    /* Test that we can handle and fix up negative integer value(s) in ECDSA signature */
    const void *negIntSigLeaf;
    isnt(negIntSigLeaf = SecCertificateCreateWithBytes(NULL, _leaf_NegativeIntInSig,
                                                       sizeof(_leaf_NegativeIntInSig)), NULL, "create negIntSigLeaf");
    CFArrayRef certs = NULL;
    isnt(certs = CFArrayCreate(NULL, &negIntSigLeaf, 1, &kCFTypeArrayCallBacks), NULL, "failed to create certs array");
    SecPolicyRef policy = NULL;
    isnt(policy = SecPolicyCreateiAP(), NULL, "failed to create policy");
    SecTrustRef trust = NULL;
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
              "create trust for negIntSigLeaf");
    
    const void *rootAACA2;
    isnt(rootAACA2 = SecCertificateCreateWithBytes(NULL, _root_AACA2,
                                                   sizeof(_root_AACA2)), NULL, "create rootAACA2");
    CFArrayRef anchors = NULL;
    isnt(anchors = CFArrayCreate(NULL, &rootAACA2, 1, &kCFTypeArrayCallBacks), NULL, "failed to create anchors array");
    if (!anchors) { goto errOut; }
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchor certificates");

    XCTAssert(SecTrustEvaluateWithError(trust, NULL), "trust evaluation failed");
    
errOut:
    CFReleaseNull(trust);
    CFReleaseNull(certs);
    CFReleaseNull(anchors);
    CFReleaseNull(negIntSigLeaf);
    CFReleaseNull(rootAACA2);
    CFReleaseNull(policy);
}

- (void)testiAPv1 {
    SecTrustRef trust;
    SecCertificateRef iAP1CA, iAP2CA, leaf0, leaf1;
    isnt(iAP1CA = SecCertificateCreateWithBytes(NULL, _iAP1CA, sizeof(_iAP1CA)),
        NULL, "create iAP1CA");
    isnt(iAP2CA = SecCertificateCreateWithBytes(NULL, _iAP2CA, sizeof(_iAP2CA)),
        NULL, "create iAP2CA");
    isnt(leaf0 = SecCertificateCreateWithBytes(NULL, _leaf0, sizeof(_leaf0)),
        NULL, "create leaf0");
    isnt(leaf1 = SecCertificateCreateWithBytes(NULL, _leaf1, sizeof(_leaf1)),
        NULL, "create leaf1");
    {
        // temporarily grab some stack space and fill it with 0xFF;
        // when we exit this scope, the stack pointer should shrink but leave the memory filled.
        // this tests for a stack overflow bug inside SecPolicyCreateiAP (rdar://16056248)
        char buf[2048];
        memset(buf, 0xFF, sizeof(buf));
    }
    SecPolicyRef policy = SecPolicyCreateiAP();
    const void *v_anchors[] = {
        iAP1CA,
        iAP2CA
    };
    CFArrayRef anchors = CFArrayCreate(NULL, v_anchors,
        array_size(v_anchors), NULL);
    CFArrayRef certs0 = CFArrayCreate(NULL, (const void **)&leaf0, 1, &kCFTypeArrayCallBacks);
    CFArrayRef certs1 = CFArrayCreate(NULL, (const void **)&leaf1, 1, &kCFTypeArrayCallBacks);
    ok_status(SecTrustCreateWithCertificates(certs0, policy, &trust), "create trust for leaf0");
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");

    /* Jan 1st 2008. */
    CFDateRef date = CFDateCreate(NULL, 220752000.0);
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    SecTrustResultType trustResult;
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
        "trust is kSecTrustResultUnspecified");

    is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");

    CFReleaseSafe(trust);
    ok_status(SecTrustCreateWithCertificates(certs1, policy, &trust), "create trust for leaf1");
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trust is kSecTrustResultUnspecified");

    CFReleaseSafe(anchors);
    CFReleaseSafe(certs1);
    CFReleaseSafe(certs0);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(leaf0);
    CFReleaseSafe(leaf1);
    CFReleaseSafe(iAP1CA);
    CFReleaseSafe(iAP2CA);
    CFReleaseSafe(date);
}

- (void)testiAPv3 {
    SecCertificateRef v3CA = NULL, v3leaf = NULL;
    isnt(v3CA = SecCertificateCreateWithBytes(NULL, _v3ca, sizeof(_v3ca)),
         NULL, "create v3 CA");
    isnt(v3leaf = SecCertificateCreateWithBytes(NULL, _v3leaf, sizeof(_v3leaf)),
         NULL, "create v3leaf");

    /* Test v3 certs meet iAP policy */
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef date = NULL;
    SecTrustResultType trustResult;

    certs = CFArrayCreate(NULL, (const void **)&v3leaf, 1, &kCFTypeArrayCallBacks);
    anchors = CFArrayCreate(NULL, (const void **)&v3CA, 1, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateiAP();
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust ref");
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchor");
    ok(date = CFDateCreate(NULL, 484000000.0), "create date");  /* 3 May 2016 */
    if (!date) { goto trustFail; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set verify date");
    ok_status(SecTrustGetTrustResult(trust, &trustResult), "evaluate");
    is_status(trustResult, kSecTrustResultUnspecified, "trust is kSecTrustResultUnspecified");

    /* Test v3 certs fail iAP SW Auth policy */
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    policy = SecPolicyCreateiAPSWAuth();
    require_noerr(SecTrustCreateWithCertificates(certs, policy, &trust), trustFail);
    require_noerr(SecTrustSetAnchorCertificates(trust, anchors), trustFail);
    require_noerr(SecTrustSetVerifyDate(trust, date), trustFail);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), trustFail);
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trust is kSecTrustResultRecoverableTrustFailure");

trustFail:
    CFReleaseSafe(policy);
    CFReleaseSafe(trust);
    CFReleaseSafe(certs);
    CFReleaseSafe(anchors);
    CFReleaseSafe(date);

    /* Test interface for determining iAuth version */
    SecCertificateRef leaf0 = NULL, leaf1 = NULL;
    isnt(leaf0 = SecCertificateCreateWithBytes(NULL, _leaf0, sizeof(_leaf0)),
         NULL, "create leaf0");
    isnt(leaf1 = SecCertificateCreateWithBytes(NULL, _leaf1, sizeof(_leaf1)),
         NULL, "create leaf1");

    is_status(SecCertificateGetiAuthVersion(leaf0), kSeciAuthVersion2, "v2 certificate");
    is_status(SecCertificateGetiAuthVersion(leaf1), kSeciAuthVersion2, "v2 certificate");
    is_status(SecCertificateGetiAuthVersion(v3leaf), kSeciAuthVersion3, "v3 certificate");

    CFReleaseSafe(leaf0);
    CFReleaseSafe(leaf1);

    /* Test the extension-copying interface */
    CFDataRef extensionData = NULL;
    uint8_t extensionValue[32] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0A,
    };
    ok(extensionData = SecCertificateCopyiAPAuthCapabilities(v3leaf),
       "copy iAuthv3 extension data");
    is(CFDataGetLength(extensionData), 32, "compare expected size");
    is(memcmp(extensionValue, CFDataGetBytePtr(extensionData), 32), 0,
       "compare expected output");
    CFReleaseNull(extensionData);

    /* Test extension-copying interface with a malformed extension. */
    uint8_t extensionValue2[32] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x04,
    };
    SecCertificateRef malformedV3leaf = NULL;
    isnt(malformedV3leaf = SecCertificateCreateWithBytes(NULL, _malformedV3Leaf, sizeof(_malformedV3Leaf)),
         NULL, "create malformed v3 leaf");
    ok(extensionData = SecCertificateCopyiAPAuthCapabilities(malformedV3leaf),
       "copy iAuthv3 extension data for malformed leaf");
    is(CFDataGetLength(extensionData), 32, "compare expected size");
    is(memcmp(extensionValue2, CFDataGetBytePtr(extensionData), 32), 0,
       "compare expected output");
    CFReleaseNull(extensionData);
    CFReleaseNull(malformedV3leaf);
    CFReleaseSafe(v3leaf);
    CFReleaseSafe(v3CA);
}

- (void)testMFiSWAuthTrust {
    SecCertificateRef sw_auth_test_CA = NULL, sw_auth_test_leaf = NULL;
    isnt(sw_auth_test_CA = SecCertificateCreateWithBytes(NULL, _iAPSWAuthTestRoot, sizeof(_iAPSWAuthTestRoot)),
         NULL, "create sw auth test ca");
    isnt(sw_auth_test_leaf = SecCertificateCreateWithBytes(NULL, _iAPSWAuth_leaf, sizeof(_iAPSWAuth_leaf)),
         NULL, "create sw auth leaf");

    /* Test SW Auth certs meet iAP SW Auth policy */
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFArrayRef certs = NULL, anchors = NULL;
    CFDateRef date = NULL;
    SecTrustResultType trustResult;

    certs = CFArrayCreate(NULL, (const void **)&sw_auth_test_leaf, 1, &kCFTypeArrayCallBacks);
    anchors = CFArrayCreate(NULL, (const void **)&sw_auth_test_CA, 1, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateiAPSWAuth();
    require_noerr(SecTrustCreateWithCertificates(certs, policy, &trust), trustFail);
    require_noerr(SecTrustSetAnchorCertificates(trust, anchors), trustFail);
    require(date = CFDateCreate(NULL, 530000000.0), trustFail);  /* 17 Oct 2017, BEFORE issuance */
    require_noerr(SecTrustSetVerifyDate(trust, date), trustFail);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), trustFail);
    is_status(trustResult, kSecTrustResultUnspecified, "trust is kSecTrustResultUnspecified");

    /* Test SW Auth certs fail iAP policy */
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    policy = SecPolicyCreateiAP();
    require_noerr(SecTrustCreateWithCertificates(certs, policy, &trust), trustFail);
    require_noerr(SecTrustSetAnchorCertificates(trust, anchors), trustFail);
    require_noerr(SecTrustSetVerifyDate(trust, date), trustFail);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), trustFail);
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trust is kSecTrustResultRecoverableTrustFailure");

    /* Test SW Auth certs fail when not-yet-valid with expiration check */
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    policy = SecPolicyCreateiAPSWAuthWithExpiration(true);
    require_noerr(SecTrustCreateWithCertificates(certs, policy, &trust), trustFail);
    require_noerr(SecTrustSetAnchorCertificates(trust, anchors), trustFail);
    require_noerr(SecTrustSetVerifyDate(trust, date), trustFail);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), trustFail);
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trust is kSecTrustResultRecoverableTrustFailure");

trustFail:
    CFReleaseSafe(policy);
    CFReleaseSafe(trust);
    CFReleaseSafe(certs);
    CFReleaseSafe(anchors);
    CFReleaseSafe(date);
    CFReleaseSafe(sw_auth_test_CA);
    CFReleaseSafe(sw_auth_test_leaf);
}

- (void)testMFiSWAuthCert {
    SecCertificateRef good_leaf = NULL, bad_leaf = NULL;
    isnt(good_leaf = SecCertificateCreateWithBytes(NULL, _iAPSWAuth_leaf, sizeof(_iAPSWAuth_leaf)),
         NULL, "create good iAP SW Auth cert");
    isnt(bad_leaf = SecCertificateCreateWithBytes(NULL, _malformed_iAPSWAuth_leaf, sizeof(_malformed_iAPSWAuth_leaf)),
         NULL, "create bad iAP SW Auth cert");

    /* Test Auth version interface */
    ok(SecCertificateGetiAuthVersion(good_leaf) == kSeciAuthVersionSW, "Get version of well-formed SW Auth cert");
    ok(SecCertificateGetiAuthVersion(bad_leaf) == kSeciAuthVersionSW, "Get version of malformed SW Auth cert");

    /* Test extension copying with malformed extensions */
    is(SecCertificateCopyiAPSWAuthCapabilities(bad_leaf, kSeciAPSWAuthGeneralCapabilities), NULL,
       "Fail to get capabilities of malformed SW auth cert");
    is(SecCertificateCopyiAPSWAuthCapabilities(bad_leaf, kSeciAPSWAuthAirPlayCapabilities), NULL,
       "Fail to get AirPlay capabilities of malformed SW auth cert");
    is(SecCertificateCopyiAPSWAuthCapabilities(bad_leaf, kSeciAPSWAuthHomeKitCapabilities), NULL,
       "Fail to get HomeKit capabilities of malformed SW auth cert");

    uint8_t byte0 = 0x00;
    uint8_t byte1 = 0x01;
    CFDataRef data0 = CFDataCreate(NULL, &byte0, 1);
    CFDataRef data1 = CFDataCreate(NULL, &byte1, 1);

    /* Test extension copying with well-formed extensions */
    CFDataRef extensionValue = NULL;
    isnt(extensionValue = SecCertificateCopyiAPSWAuthCapabilities(good_leaf, kSeciAPSWAuthGeneralCapabilities), NULL,
         "Get capabilities of well-formed SW auth cert");
    ok(CFEqual(extensionValue, data1), "Got correct general extension value");
    CFReleaseNull(extensionValue);

    isnt(extensionValue = SecCertificateCopyiAPSWAuthCapabilities(good_leaf, kSeciAPSWAuthAirPlayCapabilities), NULL,
         "Get AirPlay capabilities of well-formed SW auth cert");
    ok(CFEqual(extensionValue, data0), "Got correct AirPlay extension value");
    CFReleaseNull(extensionValue);

    isnt(extensionValue = SecCertificateCopyiAPSWAuthCapabilities(good_leaf, kSeciAPSWAuthHomeKitCapabilities), NULL,
         "Get capabilities of well-formed SW auth cert");
    ok(CFEqual(extensionValue, data1), "Got correct HomeKit extension value");
    CFReleaseNull(extensionValue);

    CFReleaseNull(good_leaf);
    CFReleaseNull(bad_leaf);
    CFReleaseNull(data0);
    CFReleaseNull(data1);
}

- (void) testComponentTypeCerts {
    SecCertificateRef batteryCA = NULL, nonComponent = NULL;
    isnt(batteryCA = SecCertificateCreateWithBytes(NULL, _componentCABattery, sizeof(_componentCABattery)),
         NULL, "create battery component CA cert");
    isnt(nonComponent = SecCertificateCreateWithBytes(NULL, _iAP2CA, sizeof(_iAP2CA)),
         NULL, "create non-component cert");

    CFStringRef componentType = NULL;
    isnt(componentType = SecCertificateCopyComponentType(batteryCA), NULL, "Get component type");
    ok(CFEqual(componentType, CFSTR("Battery")), "Got correct component type");
    CFReleaseNull(componentType);

    is(componentType = SecCertificateCopyComponentType(nonComponent), NULL, "Get component type");

    CFReleaseNull(batteryCA);
    CFReleaseNull(nonComponent);
}

- (void)testComponentTypeTrust {
    SecCertificateRef leaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = NULL;
    SecTrustRef trust = NULL;
    CFMutableArrayRef certs = NULL;
    CFArrayRef anchors = NULL;
    CFDateRef date = NULL;
    SecTrustResultType trustResult;

    isnt(leaf = SecCertificateCreateWithBytes(NULL, _batteryLeaf, sizeof(_batteryLeaf)),
         NULL, "create battery leaf");
    isnt(subCA = SecCertificateCreateWithBytes(NULL, _componentCABattery, sizeof(_componentCABattery)),
         NULL, "create battery subCA");
    isnt(root = SecCertificateCreateWithBytes(NULL, _componentRoot, sizeof(_componentRoot)),
         NULL, "create component root");

    /* Test Battery component certs meet component policy */
    certs = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, subCA);
    anchors = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks);
    policy = SecPolicyCreateAppleComponentCertificate(NULL);
    require_noerr(SecTrustCreateWithCertificates(certs, policy, &trust), trustFail);
    require_noerr(SecTrustSetAnchorCertificates(trust, anchors), trustFail);
    require(date = CFDateCreate(NULL, 576000000.0), trustFail);  /* April 3, 2019 at 9:00:00 AM PDT */
    require_noerr(SecTrustSetVerifyDate(trust, date), trustFail);
    require_noerr(SecTrustGetTrustResult(trust, &trustResult), trustFail);
    is_status(trustResult, kSecTrustResultUnspecified, "trust is kSecTrustResultUnspecified");

trustFail:
    CFReleaseNull(leaf);
    CFReleaseNull(subCA);
    CFReleaseNull(root);
    CFReleaseNull(date);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
}

@end
