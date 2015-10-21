/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
 */

#include <Security/SecPolicyPriv.h>
#include <Security/SecInternal.h>
#include <Security/SecTrust.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecCertificatePriv.h>

#include "Security_regressions.h"

#include "si-86-sectrust-eap-tls.h"


static void tests(void)
{
	SecTrustRef trust = NULL;
	SecPolicyRef policy = NULL;
	SecCertificateRef leaf, root;
	SecTrustResultType trustResult;
	
	isnt(leaf = SecCertificateCreateWithBytes(NULL, _TestLeafCertificate, sizeof(_TestLeafCertificate)), NULL, "create leaf");
	isnt(root = SecCertificateCreateWithBytes(NULL, _TestRootCertificate, sizeof(_TestRootCertificate)), NULL, "create root");
	
	const void *v_certs[] = { leaf };
	const void *v_roots[] = { root };
	CFArrayRef certs = CFArrayCreate(NULL, v_certs, sizeof(v_certs)/sizeof(*v_certs), &kCFTypeArrayCallBacks);
	CFArrayRef roots = CFArrayCreate(NULL, v_roots, sizeof(v_roots)/sizeof(*v_roots), &kCFTypeArrayCallBacks);
	
	/* Create EAP policy with specific hostname. */
	CFStringRef host = CFSTR("test.apple.com");
	const void *v_names[] = { host };
	CFArrayRef names = CFArrayCreate(NULL, v_names, sizeof(v_names)/sizeof(*v_names), &kCFTypeArrayCallBacks);
	isnt(policy = SecPolicyCreateEAP(true, names), NULL, "create policy");
	
	/* Create trust reference. */
	ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

	/* Set explicit verify date: Sep 1 2015. */
	CFDateRef date = NULL;
	isnt(date = CFDateCreate(NULL, 462823871.0), NULL, "Create verify date");
	ok_status(SecTrustSetVerifyDate(trust, date), "set date");
	
	/* Provide root certificate. */
	ok_status(SecTrustSetAnchorCertificates(trust, roots), "set anchors");
	
	ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
	is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trustResult is kSecTrustResultRecoverableTrustFailure");
	is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");
	
	CFReleaseSafe(date);
	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
	CFReleaseSafe(certs);
	CFReleaseSafe(roots);
	CFReleaseSafe(names);
	CFReleaseSafe(root);
	CFReleaseSafe(leaf);
}

int si_86_sectrust_eap_tls(int argc, char *const *argv)
{
    plan_tests(10);

    tests();

    return 0;
}
