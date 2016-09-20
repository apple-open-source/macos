/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "shared_regressions.h"

#include "si-87-sectrust-name-constraints.h"

static void test_att(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef leaf, int1, int2, cert3, root;
    SecTrustResultType trustResult;

	isnt(leaf = SecCertificateCreateWithBytes(NULL, att_leaf, sizeof(att_leaf)), NULL, "create att leaf");
	isnt(int1 = SecCertificateCreateWithBytes(NULL, att_intermediate1, sizeof(att_intermediate1)), NULL, "create att intermediate 1");
    isnt(int2 = SecCertificateCreateWithBytes(NULL, att_intermediate2, sizeof(att_intermediate2)), NULL, "create att intermediate 2");
    isnt(cert3 = SecCertificateCreateWithBytes(NULL, att_intermediate3, sizeof(att_intermediate3)), NULL, "create att intermediate 3");
    isnt(root = SecCertificateCreateWithBytes(NULL, att_root, sizeof(att_root)), NULL, "create att root");

    const void *v_certs[] = { leaf, int1, int2, cert3 };
    const void *v_roots[] = { root };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef roots = CFArrayCreate(NULL, v_roots, array_size(v_roots), &kCFTypeArrayCallBacks);

    /* Create SSL policy with specific hostname. */
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("nmd.mcd06643.sjc.wayport.net")), NULL, "create policy");

    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* Set explicit verify date: Aug 14 2015. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2015, 8, 14, 12, 0, 0), NULL, "create verify date");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    /* Provide root certificate. */
    ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
	is(SecTrustGetCertificateCount(trust), 5, "cert count is 5");

errOut:
    CFReleaseSafe(date);
	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(roots);
	CFReleaseSafe(root);
	CFReleaseSafe(cert3);
    CFReleaseSafe(int2);
    CFReleaseSafe(int1);
    CFReleaseSafe(leaf);
}

static void test_intel1(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef leaf, int1, int2, root;
    SecTrustResultType trustResult;

    isnt(leaf = SecCertificateCreateWithBytes(NULL, intel1_leaf, sizeof(intel1_leaf)), NULL, "create intel 1 leaf");
    isnt(int1 = SecCertificateCreateWithBytes(NULL, intel1_intermediate1, sizeof(intel1_intermediate1)), NULL, "create intel 1 intermediate 1");
    isnt(int2 = SecCertificateCreateWithBytes(NULL, intel_intermediate2, sizeof(intel_intermediate2)), NULL, "create intel intermediate 2");
    isnt(root = SecCertificateCreateWithBytes(NULL, intel_root, sizeof(intel_root)), NULL, "create intel root");

    const void *v_certs[] = { leaf, int1, int2 };
    const void *v_roots[] = { root };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef roots = CFArrayCreate(NULL, v_roots, array_size(v_roots), &kCFTypeArrayCallBacks);

    /* Create SSL policy with specific hostname. */
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("myctx.intel.com")), NULL, "create policy");

    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* Set explicit verify date: Sep 3 2015. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 463037436.0), NULL, "create verify date");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    /* Provide root certificate. */
    ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 4, "cert count is 4");

errOut:
    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(roots);
    CFReleaseSafe(root);
    CFReleaseSafe(int2);
    CFReleaseSafe(int1);
    CFReleaseSafe(leaf);
}

static void test_intel2(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef leaf, int1, int2, root;
    SecTrustResultType trustResult;

    isnt(leaf = SecCertificateCreateWithBytes(NULL, intel2_leaf, sizeof(intel2_leaf)), NULL, "create intel 2 leaf");
    isnt(int1 = SecCertificateCreateWithBytes(NULL, intel2_intermediate1, sizeof(intel2_intermediate1)), NULL, "create intel 2 intermediate 1");
    isnt(int2 = SecCertificateCreateWithBytes(NULL, intel_intermediate2, sizeof(intel_intermediate2)), NULL, "create intel intermediate 2");
    isnt(root = SecCertificateCreateWithBytes(NULL, intel_root, sizeof(intel_root)), NULL, "create intel root");

    const void *v_certs[] = { leaf, int1, int2 };
    const void *v_roots[] = { root };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef roots = CFArrayCreate(NULL, v_roots, array_size(v_roots), &kCFTypeArrayCallBacks);

    /* Create SSL policy with specific hostname. */
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("contact.intel.com")), NULL, "create policy");

    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* Set explicit verify date: Sep 3 2015. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 463037436.0), NULL, "create verify date");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    /* Provide root certificate. */
    ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 4, "cert count is 4");

errOut:
    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(roots);
    CFReleaseSafe(root);
    CFReleaseSafe(int2);
    CFReleaseSafe(int1);
    CFReleaseSafe(leaf);
}

static void test_abb(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef leaf, int1, int2, root;
    SecTrustResultType trustResult;

    isnt(leaf = SecCertificateCreateWithBytes(NULL, _ABB_PKI_cert, sizeof(_ABB_PKI_cert)), NULL, "create ABB leaf");
    isnt(int1 = SecCertificateCreateWithBytes(NULL, _ABBIssuingCA6, sizeof(_ABBIssuingCA6)), NULL, "create ABB intermediate 1");
    isnt(int2 = SecCertificateCreateWithBytes(NULL, _ABBIntermediateCA3, sizeof(_ABBIntermediateCA3)), NULL, "create ABB intermediate 2");
    isnt(root = SecCertificateCreateWithBytes(NULL, _ABBRootCA, sizeof(_ABBRootCA)), NULL, "create ABB root");

    const void *v_certs[] = { leaf, int1, int2 };
    const void *v_roots[] = { root };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef roots = CFArrayCreate(NULL, v_roots, array_size(v_roots), &kCFTypeArrayCallBacks);

    /* Create SSL policy with specific hostname. */
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("pki.abb.com")), NULL, "create policy");

    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* Set explicit verify date: Sep 16 2015. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 464128479.0), NULL, "create verify date");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    /* Provide root certificate. */
    ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 4, "cert count is 4");

errOut:
    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(roots);
    CFReleaseSafe(root);
    CFReleaseSafe(int2);
    CFReleaseSafe(int1);
    CFReleaseSafe(leaf);
}

static void test_bechtel1(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef leaf, int1, int2, root;
    SecTrustResultType trustResult;

    isnt(leaf = SecCertificateCreateWithBytes(NULL, _bechtel_leaf_a, sizeof(_bechtel_leaf_a)), NULL, "create Bechtel leaf a");
    isnt(int1 = SecCertificateCreateWithBytes(NULL, _bechtel_int2a, sizeof(_bechtel_int2a)), NULL, "create Bechtel intermediate 2a");
    isnt(int2 = SecCertificateCreateWithBytes(NULL, _bechtel_int1, sizeof(_bechtel_int1)), NULL, "create Bechtel intermediate 1");
    isnt(root = SecCertificateCreateWithBytes(NULL, _bechtel_root, sizeof(_bechtel_root)), NULL, "create Bechtel root");

    const void *v_certs[] = { leaf, int1, int2 };
    const void *v_roots[] = { root };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef roots = CFArrayCreate(NULL, v_roots, array_size(v_roots), &kCFTypeArrayCallBacks);

    /* Create SSL policy with specific hostname. */
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("supplier.bechtel.com")), NULL, "create policy");

    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* Set explicit verify date: Sep 29 2015. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 465253810.0), NULL, "create verify date");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    /* Provide root certificate. */
    ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 4, "cert count is 4");

errOut:
    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(roots);
    CFReleaseSafe(root);
    CFReleaseSafe(int2);
    CFReleaseSafe(int1);
    CFReleaseSafe(leaf);
}

static void test_bechtel2(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef leaf, int1, int2, root;
    SecTrustResultType trustResult;

    isnt(leaf = SecCertificateCreateWithBytes(NULL, _bechtel_leaf_b, sizeof(_bechtel_leaf_b)), NULL, "create Bechtel leaf b");
    isnt(int1 = SecCertificateCreateWithBytes(NULL, _bechtel_int2b, sizeof(_bechtel_int2b)), NULL, "create Bechtel intermediate 2b");
    isnt(int2 = SecCertificateCreateWithBytes(NULL, _bechtel_int1, sizeof(_bechtel_int1)), NULL, "create Bechtel intermediate 1");
    isnt(root = SecCertificateCreateWithBytes(NULL, _bechtel_root, sizeof(_bechtel_root)), NULL, "create Bechtel root");

    const void *v_certs[] = { leaf, int1, int2 };
    const void *v_roots[] = { root };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, array_size(v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef roots = CFArrayCreate(NULL, v_roots, array_size(v_roots), &kCFTypeArrayCallBacks);

    /* Create SSL policy with specific hostname. */
    isnt(policy = SecPolicyCreateSSL(true, CFSTR("login.becpsn.com")), NULL, "create policy");

    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* Set explicit verify date: Sep 29 2015. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 465253810.0), NULL, "create verify date");
    if (!date) { goto errOut; }
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    /* Provide root certificate. */
    ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 4, "cert count is 4");

errOut:
    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(roots);
    CFReleaseSafe(root);
    CFReleaseSafe(int2);
    CFReleaseSafe(int1);
    CFReleaseSafe(leaf);
}

int si_87_sectrust_name_constraints(int argc, char *const *argv)
{
	plan_tests(73);

	test_att();
    test_intel1();
    test_intel2();
    test_abb();
    test_bechtel1();
    test_bechtel2();

	return 0;
}
