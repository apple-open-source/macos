/*
 * Copyright (c) 2006-2016 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <utilities/array_size.h>
#include <utilities/SecCFRelease.h>
#include <stdlib.h>
#include <unistd.h>

#include "shared_regressions.h"

#include "si-22-sectrust-iap.h"

static void tests(void)
{
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
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

	is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");

	CFReleaseSafe(trust);
    ok_status(SecTrustCreateWithCertificates(certs1, policy, &trust), "create trust for leaf1");
	ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
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

static void test_v3(void) {
    SecCertificateRef v3CA = NULL, v3leaf = NULL;
    isnt(v3CA = SecCertificateCreateWithBytes(NULL, _v3ca, sizeof(_v3ca)),
         NULL, "create v3leaf");
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
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate");
    is_status(trustResult, kSecTrustResultUnspecified, "trust is kSecTrustResultUnspecified");

trustFail:
    CFReleaseSafe(policy);
    CFReleaseSafe(trust);
    CFReleaseSafe(certs);
    CFReleaseSafe(anchors);
    CFReleaseSafe(date);

#if TARGET_OS_IPHONE
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
#endif
    CFReleaseSafe(v3leaf);
    CFReleaseSafe(v3CA);
}

int si_22_sectrust_iap(int argc, char *const *argv)
{
#if TARGET_OS_IPHONE
	plan_tests(14+20);
#else
    plan_tests(14+8);
#endif

	tests();
    test_v3();

	return 0;
}
