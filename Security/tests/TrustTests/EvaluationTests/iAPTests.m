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
    SecCertificateRef batteryCA = NULL, batteryLeaf = NULL, utf8ComponentCert = NULL, nonComponent = NULL, propertiesComponentCert = NULL;
    isnt(batteryCA = SecCertificateCreateWithBytes(NULL, _componentCABattery, sizeof(_componentCABattery)),
         NULL, "create battery component CA cert");
    isnt(batteryLeaf = SecCertificateCreateWithBytes(NULL, _batteryLeaf, sizeof(_batteryLeaf)), NULL);
    isnt(utf8ComponentCert = SecCertificateCreateWithBytes(NULL, _component_leaf_UTF8String, sizeof(_component_leaf_UTF8String)), NULL);
    isnt(nonComponent = SecCertificateCreateWithBytes(NULL, _iAP2CA, sizeof(_iAP2CA)),
         NULL, "create non-component cert");
    isnt(propertiesComponentCert = SecCertificateCreateWithBytes(NULL, _component_leaf_properties, sizeof(_component_leaf_properties)), NULL, "create component cert with properties");

    CFStringRef componentType = NULL;
    isnt(componentType = SecCertificateCopyComponentType(batteryCA), NULL, "Get component type");
    ok(CFEqual(componentType, CFSTR("Battery")), "Got correct component type");
    CFReleaseNull(componentType);

    isnt(componentType = SecCertificateCopyComponentType(batteryLeaf), NULL, "Get component type");
    ok(CFEqual(componentType, CFSTR("Battery")), "Got correct component type");
    CFReleaseNull(componentType);

    isnt(componentType = SecCertificateCopyComponentType(utf8ComponentCert), NULL, "Get component type");
    ok(CFEqual(componentType, CFSTR("Watch MLB")), "Got correct component type");
    CFReleaseNull(componentType);

    is(componentType = SecCertificateCopyComponentType(nonComponent), NULL, "Get component type");

    isnt(componentType = SecCertificateCopyComponentType(propertiesComponentCert), NULL, "Get component type");
    ok(CFEqual(componentType, CFSTR("Battery")), "Got correct component type");
    CFReleaseNull(componentType);

    CFReleaseNull(batteryCA);
    CFReleaseNull(batteryLeaf);
    CFReleaseNull(utf8ComponentCert);
    CFReleaseNull(nonComponent);
    CFReleaseNull(propertiesComponentCert);
}

- (void)testCopyComponentAttributes {
    SecCertificateRef batteryLeaf = NULL, propertiesComponentCert = NULL, prodComponentCert = NULL, devComponentCert = NULL;
    isnt(batteryLeaf = SecCertificateCreateWithBytes(NULL, _batteryLeaf, sizeof(_batteryLeaf)), NULL);
    isnt(propertiesComponentCert = SecCertificateCreateWithBytes(NULL, _component_leaf_properties, sizeof(_component_leaf_properties)), NULL, "create component cert with properties");
    isnt(prodComponentCert = SecCertificateCreateWithBytes(NULL, _component_leaf_properties_prod, sizeof(_component_leaf_properties_prod)), NULL);
    isnt(devComponentCert = SecCertificateCreateWithBytes(NULL, _component_leaf_properties_dev, sizeof(_component_leaf_properties_dev)), NULL);

    CFDictionaryRef attributes = NULL;
    is(attributes = SecCertificateCopyComponentAttributes(batteryLeaf), NULL, "Found attributes where none present");
    isnt(attributes = SecCertificateCopyComponentAttributes(propertiesComponentCert), NULL, "Failed to find attributes");
    NSDictionary *attrs = CFBridgingRelease(attributes);
    XCTAssertEqual(attrs.count, 1);
    XCTAssertEqual([attrs objectForKey:@(1001)], @NO);

    attrs = CFBridgingRelease(SecCertificateCopyComponentAttributes(prodComponentCert));
    XCTAssertEqual(attrs.count, 1);
    XCTAssertEqual([attrs objectForKey:@(1001)], @YES);

    attrs = CFBridgingRelease(SecCertificateCopyComponentAttributes(devComponentCert));
    XCTAssertEqual(attrs.count, 1);
    XCTAssertEqual([attrs objectForKey:@(1001)], @NO);

    CFReleaseNull(batteryLeaf);
    CFReleaseNull(propertiesComponentCert);
    CFReleaseNull(prodComponentCert);
    CFReleaseNull(devComponentCert);
}

- (void)testComponentTypeTrust {
    SecCertificateRef leaf = NULL, prodLeaf = NULL, devLeaf = NULL, subCA = NULL, root = NULL;
    SecPolicyRef policy = SecPolicyCreateAppleComponentCertificate(NULL);
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
    isnt(prodLeaf = SecCertificateCreateWithBytes(NULL, _component_leaf_properties_prod, sizeof(_component_leaf_properties_prod)),
         NULL);
    isnt(devLeaf = SecCertificateCreateWithBytes(NULL, _component_leaf_properties_dev, sizeof(_component_leaf_properties_dev)),
         NULL);

    NSArray *testChain = @[(__bridge id)prodLeaf, (__bridge id)subCA];
    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:testChain
                                                                         policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)root]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:648000000.0]]; // July 14, 2021 at 5:00:00 PM PDT
    XCTAssertTrue([eval evaluate:nil]);

    testChain = @[(__bridge id)devLeaf, (__bridge id)subCA];
    eval = [[TestTrustEvaluation alloc] initWithCertificates:testChain
                                                    policies:@[(__bridge id)policy]];
    [eval setAnchors:@[(__bridge id)root]];
    [eval setVerifyDate:[NSDate dateWithTimeIntervalSinceReferenceDate:648000000.0]]; // July 14, 2021 at 5:00:00 PM PDT
    XCTAssertTrue([eval evaluate:nil]);

    /* Test Battery component certs meet component policy */
    certs = CFArrayCreateMutable(NULL, 2, &kCFTypeArrayCallBacks);
    CFArrayAppendValue(certs, leaf);
    CFArrayAppendValue(certs, subCA);
    anchors = CFArrayCreate(NULL, (const void **)&root, 1, &kCFTypeArrayCallBacks);
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
    CFReleaseNull(prodLeaf);
    CFReleaseNull(devLeaf);
}

- (void)testMFIv4Certs {
    SecCertificateRef chip_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_leaf, sizeof(_test_mfi4_accessory_leaf));
    SecCertificateRef chip_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_subca, sizeof(_test_mfi4_accessory_subca));
    SecCertificateRef baa_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_leaf, sizeof(_test_mfi4_attestation_leaf));
    SecCertificateRef baa_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_subca, sizeof(_test_mfi4_attestation_subca));
    SecCertificateRef provisioning_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_leaf, sizeof(_test_mfi4_provisioning_leaf));
    SecCertificateRef provisioning_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_subca, sizeof(_test_mfi4_provisioning_subca));

    XCTAssertEqual(kSeciAuthVersion4, SecCertificateGetiAuthVersion(chip_leaf));
    XCTAssertEqual(kSeciAuthInvalid, SecCertificateGetiAuthVersion(chip_ca));
    XCTAssertEqual(kSeciAuthVersion4, SecCertificateGetiAuthVersion(baa_leaf));
    XCTAssertEqual(kSeciAuthInvalid, SecCertificateGetiAuthVersion(baa_ca));
    XCTAssertEqual(kSeciAuthInvalid, SecCertificateGetiAuthVersion(provisioning_leaf));
    XCTAssertEqual(kSeciAuthInvalid, SecCertificateGetiAuthVersion(provisioning_ca));

    const uint8_t _test_expected_capabilities_chip[] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x01
    };
    NSData *expectedCapabilities = [NSData dataWithBytes:_test_expected_capabilities_chip length:sizeof(_test_expected_capabilities_chip)];

    NSData *capabilities = CFBridgingRelease(SecCertificateCopyiAPAuthCapabilities(chip_leaf));
    XCTAssertNotNil(capabilities);
    XCTAssertEqualObjects(capabilities, expectedCapabilities);

    const uint8_t _test_expected_capabilities_baa[] = {
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02,0x03
    };
    expectedCapabilities = [NSData dataWithBytes:_test_expected_capabilities_baa length:sizeof(_test_expected_capabilities_baa)];

    capabilities = CFBridgingRelease(SecCertificateCopyiAPAuthCapabilities(baa_leaf));
    XCTAssertNotNil(capabilities);
    XCTAssertEqualObjects(capabilities, expectedCapabilities);

    capabilities = CFBridgingRelease(SecCertificateCopyiAPAuthCapabilities(chip_ca));
    XCTAssertNil(capabilities);

    capabilities = CFBridgingRelease(SecCertificateCopyiAPAuthCapabilities(baa_ca));
    XCTAssertNil(capabilities);

    capabilities = CFBridgingRelease(SecCertificateCopyiAPAuthCapabilities(provisioning_leaf));
    XCTAssertNil(capabilities);

    capabilities = CFBridgingRelease(SecCertificateCopyiAPAuthCapabilities(provisioning_ca));
    XCTAssertNil(capabilities);

    CFReleaseNull(chip_leaf);
    CFReleaseNull(chip_ca);
    CFReleaseNull(baa_leaf);
    CFReleaseNull(baa_ca);
    CFReleaseNull(provisioning_leaf);
    CFReleaseNull(provisioning_ca);
}

- (void)testMFIv4Trust {
    SecCertificateRef chip_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_leaf, sizeof(_test_mfi4_accessory_leaf));
    SecCertificateRef chip_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_subca, sizeof(_test_mfi4_accessory_subca));
    SecCertificateRef baa_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_leaf, sizeof(_test_mfi4_attestation_leaf));
    SecCertificateRef baa_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_subca, sizeof(_test_mfi4_attestation_subca));
    SecCertificateRef provisioning_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_leaf, sizeof(_test_mfi4_provisioning_leaf));
    SecCertificateRef provisioning_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_subca, sizeof(_test_mfi4_provisioning_subca));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _test_mfi4_root, sizeof(_test_mfi4_root));

    NSArray *chipCerts = @[ (__bridge id)chip_leaf, (__bridge id)chip_ca ];
    NSArray *baaCerts = @[ (__bridge id)baa_leaf, (__bridge id)baa_ca ];
    NSArray *provisioningCerts = @[ (__bridge id)provisioning_leaf, (__bridge id)provisioning_ca ];
    SecPolicyRef iAPPolicy = SecPolicyCreateiAP();

    TestTrustEvaluation *eval = [[TestTrustEvaluation alloc] initWithCertificates:chipCerts
                                                                         policies:@[(__bridge id)iAPPolicy]];
    [eval setAnchors:@[(__bridge id)root]];
    XCTAssert([eval evaluate:nil]);

    eval = [[TestTrustEvaluation alloc] initWithCertificates:baaCerts
                                                    policies:@[(__bridge id)iAPPolicy]];
    [eval setAnchors:@[(__bridge id)root]];
    XCTAssertFalse([eval evaluate:nil]);

    eval = [[TestTrustEvaluation alloc] initWithCertificates:provisioningCerts
                                                    policies:@[(__bridge id)iAPPolicy]];
    [eval setAnchors:@[(__bridge id)root]];
    XCTAssertFalse([eval evaluate:nil]);

    CFReleaseNull(chip_leaf);
    CFReleaseNull(chip_ca);
    CFReleaseNull(baa_leaf);
    CFReleaseNull(baa_ca);
    CFReleaseNull(provisioning_leaf);
    CFReleaseNull(provisioning_ca);
    CFReleaseNull(root);
}

- (void)testMFIv4Compression {
    SecCertificateRef chip_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_leaf, sizeof(_test_mfi4_accessory_leaf));
    SecCertificateRef chip_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_subca, sizeof(_test_mfi4_accessory_subca));
    SecCertificateRef baa_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_leaf, sizeof(_test_mfi4_attestation_leaf));
    SecCertificateRef baa_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_subca, sizeof(_test_mfi4_attestation_subca));
    SecCertificateRef provisioning_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_leaf, sizeof(_test_mfi4_provisioning_leaf));
    SecCertificateRef provisioning_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_subca, sizeof(_test_mfi4_provisioning_subca));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _test_mfi4_root, sizeof(_test_mfi4_root));

    // CoreTrust tests that the input/outputs are "correct", so we just want to test that the wrappers work
    NSData *compressedChipLeaf = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(chip_leaf));
    NSData *compressedChipCA = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(chip_ca));
    NSData *compressedBAALeaf = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(baa_leaf));
    NSData *compressedBAACA = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(baa_ca));
    NSData *compressedProvisioningLeaf = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(provisioning_leaf));
    NSData *compressedProvisioningCA = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(provisioning_ca));
    NSData *compressedRoot = CFBridgingRelease(SecCertificateCopyCompressedMFiCert(root));

    XCTAssertNotNil(compressedChipLeaf);
    XCTAssertNotNil(compressedChipCA);
    XCTAssertNotNil(compressedBAALeaf);
    XCTAssertNotNil(compressedBAACA);
    XCTAssertNil(compressedProvisioningLeaf);
    XCTAssertNil(compressedProvisioningCA);
    XCTAssertNil(compressedRoot);

    SecCertificateRef decompressedChipLeaf = SecCertificateCreateWithCompressedMFiCert((__bridge CFDataRef)compressedChipLeaf);
    SecCertificateRef decompressedChipCA = SecCertificateCreateWithCompressedMFiCert((__bridge CFDataRef)compressedChipCA);
    SecCertificateRef decompressedBAALeaf = SecCertificateCreateWithCompressedMFiCert((__bridge CFDataRef)compressedBAALeaf);
    SecCertificateRef decompressedBAACA = SecCertificateCreateWithCompressedMFiCert((__bridge CFDataRef)compressedBAACA);
    SecCertificateRef nullInput = SecCertificateCreateWithCompressedMFiCert((__bridge CFDataRef)compressedRoot);
    SecCertificateRef invalidInput = SecCertificateCreateWithCompressedMFiCert((__bridge CFDataRef)[NSData dataWithBytes:_test_mfi4_root length:sizeof(_test_mfi4_root)]);

    XCTAssertNotEqual(decompressedChipLeaf, NULL);
    XCTAssertNotEqual(decompressedChipCA, NULL);
    XCTAssertNotEqual(decompressedBAALeaf, NULL);
    XCTAssertNotEqual(decompressedBAACA, NULL);
    XCTAssertEqual(nullInput, NULL);
    XCTAssertEqual(invalidInput, NULL);

    XCTAssert(CFEqualSafe(chip_leaf, decompressedChipLeaf));
    XCTAssert(CFEqualSafe(chip_ca, decompressedChipCA));
    XCTAssert(CFEqualSafe(baa_leaf, decompressedBAALeaf));
    XCTAssert(CFEqualSafe(baa_ca, decompressedBAACA));

    CFReleaseNull(chip_leaf);
    CFReleaseNull(chip_ca);
    CFReleaseNull(baa_leaf);
    CFReleaseNull(baa_ca);
    CFReleaseNull(provisioning_leaf);
    CFReleaseNull(provisioning_ca);
    CFReleaseNull(root);
    CFReleaseNull(decompressedChipLeaf);
    CFReleaseNull(decompressedChipCA);
    CFReleaseNull(decompressedBAALeaf);
    CFReleaseNull(decompressedBAACA);
}

- (void)testMFIv4Chaining {
    SecCertificateRef chip_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_leaf, sizeof(_test_mfi4_accessory_leaf));
    SecCertificateRef chip_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_accessory_subca, sizeof(_test_mfi4_accessory_subca));
    SecCertificateRef baa_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_leaf, sizeof(_test_mfi4_attestation_leaf));
    SecCertificateRef baa_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_attestation_subca, sizeof(_test_mfi4_attestation_subca));
    SecCertificateRef provisioning_leaf = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_leaf, sizeof(_test_mfi4_provisioning_leaf));
    SecCertificateRef provisioning_ca = SecCertificateCreateWithBytes(NULL, _test_mfi4_provisioning_subca, sizeof(_test_mfi4_provisioning_subca));
    SecCertificateRef root = SecCertificateCreateWithBytes(NULL, _test_mfi4_root, sizeof(_test_mfi4_root));

    XCTAssertNotEqual(NULL, SecCertificateGetAuthorityKeyID(chip_leaf));
    XCTAssertNotEqual(NULL, SecCertificateGetSubjectKeyID(chip_ca));
    XCTAssertEqualObjects((__bridge NSData*)SecCertificateGetAuthorityKeyID(chip_leaf),
                          (__bridge NSData*)SecCertificateGetSubjectKeyID(chip_ca));
    XCTAssertNotEqual(NULL, SecCertificateGetAuthorityKeyID(chip_ca));
    XCTAssertNotEqual(NULL, SecCertificateGetSubjectKeyID(root));
    XCTAssertEqualObjects((__bridge NSData*)SecCertificateGetAuthorityKeyID(chip_ca),
                          (__bridge NSData*)SecCertificateGetSubjectKeyID(root));

    XCTAssertNotEqual(NULL, SecCertificateGetAuthorityKeyID(baa_leaf));
    XCTAssertNotEqual(NULL, SecCertificateGetSubjectKeyID(baa_ca));
    XCTAssertEqualObjects((__bridge NSData*)SecCertificateGetAuthorityKeyID(baa_leaf),
                          (__bridge NSData*)SecCertificateGetSubjectKeyID(baa_ca));
    XCTAssertNotEqual(NULL, SecCertificateGetAuthorityKeyID(baa_ca));
    XCTAssertNotEqual(NULL, SecCertificateGetSubjectKeyID(root));
    XCTAssertEqualObjects((__bridge NSData*)SecCertificateGetAuthorityKeyID(baa_ca),
                          (__bridge NSData*)SecCertificateGetSubjectKeyID(root));

    XCTAssertNotEqual(NULL, SecCertificateGetAuthorityKeyID(provisioning_leaf));
    XCTAssertNotEqual(NULL, SecCertificateGetSubjectKeyID(provisioning_ca));
    XCTAssertEqualObjects((__bridge NSData*)SecCertificateGetAuthorityKeyID(provisioning_leaf),
                          (__bridge NSData*)SecCertificateGetSubjectKeyID(provisioning_ca));
    XCTAssertNotEqual(NULL, SecCertificateGetAuthorityKeyID(provisioning_ca));
    XCTAssertNotEqual(NULL, SecCertificateGetSubjectKeyID(root));
    XCTAssertEqualObjects((__bridge NSData*)SecCertificateGetAuthorityKeyID(provisioning_ca),
                          (__bridge NSData*)SecCertificateGetSubjectKeyID(root));

    CFReleaseNull(chip_leaf);
    CFReleaseNull(chip_ca);
    CFReleaseNull(baa_leaf);
    CFReleaseNull(baa_ca);
    CFReleaseNull(provisioning_leaf);
    CFReleaseNull(provisioning_ca);
    CFReleaseNull(root);
}

@end
