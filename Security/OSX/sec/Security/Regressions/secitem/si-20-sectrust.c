/*
 * Copyright (c) 2006-2010,2012-2015 Apple Inc. All Rights Reserved.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrustPriv.h>
#include <Security/SecItem.h>
#include <ipc/securityd_client.h>
#include <utilities/array_size.h>
#include <utilities/SecCFWrappers.h>
#include <stdlib.h>
#include <unistd.h>

#include "Security_regressions.h"
#include "si-20-sectrust.h"

/* Test basic add delete update copy matching stuff. */
static void basic_tests(void)
{
    SecTrustRef trust;
	SecCertificateRef cert0, cert1;
	isnt(cert0 = SecCertificateCreateWithBytes(NULL, _c0, sizeof(_c0)),
		NULL, "create cert0");
	isnt(cert1 = SecCertificateCreateWithBytes(NULL, _c1, sizeof(_c1)),
		NULL, "create cert1");
	const void *v_certs[] = {
		cert0,
		cert1
	};
    SecPolicyRef policy = SecPolicyCreateSSL(false, NULL);
    CFArrayRef certs = CFArrayCreate(NULL, v_certs,
		array_size(v_certs), NULL);

    /* SecTrustCreateWithCertificates failures. */
    is_status(SecTrustCreateWithCertificates(kCFBooleanTrue, policy, &trust),
        errSecParam, "create trust with boolean instead of cert");
    is_status(SecTrustCreateWithCertificates(cert0, kCFBooleanTrue, &trust),
        errSecParam, "create trust with boolean instead of policy");

	/* SecTrustCreateWithCertificates using array of certs. */
	ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    /* NOTE: prior to <rdar://11810677 SecTrustGetCertificateCount would return 1 at this point.
     * Now, however, we do an implicit SecTrustEvaluate to build the chain if it has not yet been
     * evaluated, so we now expect the full chain length.
     */
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");
    is(SecTrustGetCertificateAtIndex(trust, 0), cert0, "cert 0 is leaf");

	/* Jul 30 2014. */
    CFDateRef date = NULL;
    isnt(date = CFDateCreateForGregorianZuluMoment(NULL, 2014, 7, 30, 12, 0, 0),
         NULL, "create verify date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

	SecTrustResultType trustResult;

SKIP: {
#ifdef NO_SERVER
    skip("Can't fail to connect to securityd in NO_SERVER mode", 4, false);
#endif
    // Test Restore OS environment
    SecServerSetMachServiceName("com.apple.security.doesn't-exist");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust without securityd running");
    is_status(trustResult, kSecTrustResultInvalid, "trustResult is kSecTrustResultInvalid");
	is(SecTrustGetCertificateCount(trust), 1, "cert count is 1 without securityd running");
    SecKeyRef pubKey = NULL;
    ok(pubKey = SecTrustCopyPublicKey(trust), "copy public key without securityd running");
    CFReleaseNull(pubKey);
    SecServerSetMachServiceName(NULL);
    // End of Restore OS environment tests
}

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trustResult is kSecTrustResultUnspecified");

	is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");

	CFDataRef c0_serial = CFDataCreate(NULL, _c0_serial, sizeof(_c0_serial));
	CFDataRef serial;
	ok(serial = SecCertificateCopySerialNumber(cert0), "copy cert0 serial");
	ok(CFEqual(c0_serial, serial), "serial matches");

    CFArrayRef anchors = CFArrayCreate(NULL, (const void **)&cert1, 1, &kCFTypeArrayCallBacks);
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");
	is(SecTrustGetCertificateCount(trust), 2, "cert count is 2");

	CFReleaseSafe(anchors);
    anchors = CFArrayCreate(NULL, NULL, 0, NULL);
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set empty anchors list");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
		"trust is kSecTrustResultRecoverableTrustFailure");

	ok_status(SecTrustSetAnchorCertificatesOnly(trust, false), "trust passed in anchors and system anchors");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

	ok_status(SecTrustSetAnchorCertificatesOnly(trust, true), "only trust passed in anchors (default)");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
		"trust is kSecTrustResultRecoverableTrustFailure");

    /* Test cert_1 intermediate from the keychain. */
    CFReleaseSafe(trust);
    ok_status(SecTrustCreateWithCertificates(cert0, policy, &trust),
              "create trust with single cert0");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    // Add cert1
    CFDictionaryRef query = CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
        kSecClass, kSecClassCertificate, kSecValueRef, cert1, NULL);
    ok_status(SecItemAdd(query, NULL), "add cert1 to keychain");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    // Cleanup added cert1.
    ok_status(SecItemDelete(query), "remove cert1 from keychain");
    CFReleaseSafe(query);
    is_status(trustResult, kSecTrustResultUnspecified,
              "trust is kSecTrustResultUnspecified");
	is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");

    /* Set certs to be the xedge2 leaf. */
	CFReleaseSafe(certs);
	const void *cert_xedge2;
	isnt(cert_xedge2 = SecCertificateCreateWithBytes(NULL, xedge2_certificate,
        sizeof(xedge2_certificate)), NULL, "create cert_xedge2");
    certs = CFArrayCreate(NULL, &cert_xedge2, 1, NULL);

	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
	CFReleaseSafe(date);
    bool server = true;
    policy = SecPolicyCreateSSL(server, CFSTR("xedge2.apple.com"));
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust for ssl server xedge2.apple.com");

    /* This test uses a cert whose root is no longer in our trust store,
     * so we need to explicitly set it as a trusted anchor
     */
    SecCertificateRef _root;
    isnt(_root = SecCertificateCreateWithBytes(NULL, entrust1024RootCA, sizeof(entrust1024RootCA)),
         NULL, "create root");
    const void *v_roots[] = { _root };
    CFArrayRef _anchors;
    isnt(_anchors = CFArrayCreate(NULL, v_roots, array_size(v_roots), NULL),
         NULL, "create anchors");
    ok_status(SecTrustSetAnchorCertificates(trust, _anchors), "set anchors");

    /* Jan 1st 2009. */
    date = CFDateCreate(NULL, 252288000.0);
    ok_status(SecTrustSetVerifyDate(trust, date), "set xedge2 trust date to Jan 1st 2009");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate xedge2 trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
    server = false;
    policy = SecPolicyCreateSSL(server, CFSTR("xedge2.apple.com"));
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust for ssl client xedge2.apple.com");
    ok_status(SecTrustSetAnchorCertificates(trust, _anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set xedge2 trust date to Jan 1st 2009");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate xedge2 trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
		"trust is kSecTrustResultRecoverableTrustFailure");

	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
    server = true;
    policy = SecPolicyCreateIPSec(server, CFSTR("xedge2.apple.com"));
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust for ip server xedge2.apple.com");
    ok_status(SecTrustSetAnchorCertificates(trust, _anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set xedge2 trust date to Jan 1st 2009");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate xedge2 trust");
#if 0
    /* Although this shouldn't be a valid ipsec cert, since we no longer
       check for ekus in the ipsec policy it is. */
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure,
		"trust is kSecTrustResultRecoverableTrustFailure");
#else
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");
#endif

	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
    server = true;
    policy = SecPolicyCreateSSL(server, CFSTR("nowhere.com"));
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust for ssl server nowhere.com");
    SecPolicyRef replacementPolicy = SecPolicyCreateSSL(server, CFSTR("xedge2.apple.com"));
    SecTrustSetPolicies(trust, replacementPolicy);
    CFReleaseSafe(replacementPolicy);
    ok_status(SecTrustSetAnchorCertificates(trust, _anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set xedge2 trust date to Jan 1st 2009");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate xedge2 trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
    server = true;
    policy = SecPolicyCreateSSL(server, CFSTR("nowhere.com"));
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust for ssl server nowhere.com");
    SecPolicyRef replacementPolicy2 = SecPolicyCreateSSL(server, CFSTR("xedge2.apple.com"));
    CFArrayRef replacementPolicies = CFArrayCreate(kCFAllocatorDefault, (CFTypeRef*)&replacementPolicy2, 1, &kCFTypeArrayCallBacks);
    SecTrustSetPolicies(trust, replacementPolicies);
    CFReleaseSafe(replacementPolicy2);
    CFReleaseSafe(replacementPolicies);
    ok_status(SecTrustSetAnchorCertificates(trust, _anchors), "set anchors");
    ok_status(SecTrustSetVerifyDate(trust, date), "set xedge2 trust date to Jan 1st 2009");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate xedge2 trust");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

    /* Test self signed ssl cert with cert itself set as anchor. */
	CFReleaseSafe(trust);
	CFReleaseSafe(policy);
	CFReleaseSafe(certs);
	CFReleaseSafe(date);
	const void *garthc2;
    server = true;
	isnt(garthc2 = SecCertificateCreateWithBytes(NULL, garthc2_certificate,
        sizeof(garthc2_certificate)), NULL, "create garthc2");
    certs = CFArrayCreate(NULL, &garthc2, 1, NULL);
    policy = SecPolicyCreateSSL(server, CFSTR("garthc2.apple.com"));
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust for ip server garthc2.apple.com");
    date = CFDateCreate(NULL, 269568000.0);
    ok_status(SecTrustSetVerifyDate(trust, date),
        "set garthc2 trust date to Aug 2009");
    ok_status(SecTrustSetAnchorCertificates(trust, certs),
        "set garthc2 as anchor");
    ok_status(SecTrustEvaluate(trust, &trustResult),
        "evaluate self signed cert with cert as anchor");
    is_status(trustResult, kSecTrustResultUnspecified,
		"trust is kSecTrustResultUnspecified");

	CFReleaseSafe(garthc2);
	CFReleaseSafe(cert_xedge2);
	CFReleaseSafe(anchors);
	CFReleaseSafe(trust);
	CFReleaseSafe(serial);
	CFReleaseSafe(c0_serial);
	CFReleaseSafe(policy);
	CFReleaseSafe(certs);
	CFReleaseSafe(cert0);
	CFReleaseSafe(cert1);
	CFReleaseSafe(date);

    CFReleaseSafe(_root);
    CFReleaseSafe(_anchors);
}

static void rsa8k_tests(void)
{
    /* Test prt_forest_fi that have a 8k RSA key */
    const void *prt_forest_fi;
    isnt(prt_forest_fi = SecCertificateCreateWithBytes(NULL, prt_forest_fi_certificate,
                                                       sizeof(prt_forest_fi_certificate)), NULL, "create prt_forest_fi");
    CFArrayRef certs = NULL;
    isnt(certs = CFArrayCreate(NULL, &prt_forest_fi, 1, NULL), NULL, "failed to create cert array");
    SecPolicyRef policy = NULL;
    isnt(policy = SecPolicyCreateSSL(false, CFSTR("owa.prt-forest.fi")), NULL, "failed to create policy");
    SecTrustRef trust = NULL;
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
              "create trust for ip client owa.prt-forest.fi");
    CFDateRef date = CFDateCreate(NULL, 391578321.0);
    ok_status(SecTrustSetVerifyDate(trust, date),
              "set owa.prt-forest.fi trust date to May 2013");
    
    SecKeyRef pubkey = SecTrustCopyPublicKey(trust);
    isnt(pubkey, NULL, "pubkey returned");
    
    CFReleaseSafe(certs);
    CFReleaseNull(prt_forest_fi);
    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(pubkey);
    CFReleaseNull(date);
}

static void date_tests(void)
{
    /* Test long-lived cert chain that expires in 9999 */
    CFDateRef date = NULL;
    const void *leaf, *root;
    isnt(leaf = SecCertificateCreateWithBytes(NULL, longleaf, sizeof(longleaf)), NULL, "create leaf");
    isnt(root = SecCertificateCreateWithBytes(NULL, longroot, sizeof(longroot)), NULL, "create root");

    CFArrayRef certs = NULL;
    isnt(certs = CFArrayCreate(NULL, &leaf, 1, NULL), NULL, "failed to create cert array");
    CFArrayRef anchors = NULL;
    isnt(anchors = CFArrayCreate(NULL, &root, 1, NULL), NULL, "failed to create anchors array");

    SecPolicyRef policy = NULL;
    isnt(policy = SecPolicyCreateBasicX509(), NULL, "failed to create policy");
    SecTrustRef trust = NULL;
    SecTrustResultType trustResult;
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetAnchorCertificates(trust, anchors), "set anchors");

    /* September 4, 2013 (prior to "notBefore" date of 2 April 2014, should fail) */
    isnt(date = CFDateCreate(NULL, 400000000), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 23 Sep 2013");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust on 23 Sep 2013");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "expected kSecTrustResultRecoverableTrustFailure");
    CFReleaseNull(date);

    /* January 17, 2016 (recent date within validity period, should succeed) */
    isnt(date = CFDateCreate(NULL, 474747474), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 17 Jan 2016");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust on 17 Jan 2016");
    is_status(trustResult, kSecTrustResultUnspecified, "expected kSecTrustResultUnspecified");
    CFReleaseNull(date);

    /* December 20, 9999 (far-future date within validity period, should succeed) */
    isnt(date = CFDateCreate(NULL, 252423000000), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 20 Dec 9999");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust on 20 Dec 9999");
    is_status(trustResult, kSecTrustResultUnspecified, "expected kSecTrustResultUnspecified");
    CFReleaseNull(date);

    /* January 12, 10000 (after the "notAfter" date of 31 Dec 9999, should fail) */
    isnt(date = CFDateCreate(NULL, 252425000000), NULL, "failed to create date");
    ok_status(SecTrustSetVerifyDate(trust, date), "set trust date to 12 Jan 10000");
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust on 12 Jan 10000");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "expected kSecTrustResultRecoverableTrustFailure");
    CFReleaseNull(date);

    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(anchors);
    CFReleaseSafe(certs);
    CFReleaseNull(root);
    CFReleaseNull(leaf);
}

int si_20_sectrust(int argc, char *const *argv)
{
	plan_tests(101);

	basic_tests();
    rsa8k_tests();
    date_tests();

	return 0;
}
