/*
 *  si-67-sectrust-blacklist.c
 *  regressions
 *
 *  Created by Conrad Sauerwald on 3/24/11.
 *  Copyright 2011 Apple Inc. All rights reserved.
 *
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/SecCertificate.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecInternal.h>
#include <Security/SecPolicyPriv.h>
#include <Security/SecTrust.h>
#include <stdlib.h>
#include <unistd.h>

#include "si-67-sectrust-blacklist/Global Trustee.cer.h"
#include "si-67-sectrust-blacklist/login.yahoo.com.1.cer.h"
#include "si-67-sectrust-blacklist/UTN-USERFirst-Hardware.cer.h"
#include "si-67-sectrust-blacklist/login.yahoo.com.2.cer.h"
#include "si-67-sectrust-blacklist/addons.mozilla.org.cer.h"
#include "si-67-sectrust-blacklist/login.yahoo.com.cer.h"
#include "si-67-sectrust-blacklist/login.live.com.cer.h"
#include "si-67-sectrust-blacklist/mail.google.com.cer.h"
#include "si-67-sectrust-blacklist/login.skype.com.cer.h"
#include "si-67-sectrust-blacklist/www.google.com.cer.h"

#include "Security_regressions.h"

static void validate_one_cert(uint8_t *data, size_t len, int chain_length, SecTrustResultType trust_result)
{
    SecTrustRef trust;
	SecCertificateRef cert;
    SecPolicyRef policy = SecPolicyCreateSSL(false, NULL);
    CFArrayRef certs;

	isnt(cert = SecCertificateCreateWithBytes(NULL, data, len),
		NULL, "create cert");
    certs = CFArrayCreate(NULL, (const void **)&cert, 1, NULL);
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust),
        "create trust with single cert");
	//CFDateRef date = CFDateCreate(NULL, 1301008576);
    //ok_status(SecTrustSetVerifyDate(trust, date), "set date");
    //CFRelease(date);

	SecTrustResultType trustResult;
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
	is(SecTrustGetCertificateCount(trust), chain_length, "cert count");
    is_status(trustResult, trust_result, "correct trustResult");
    CFRelease(trust);
    CFRelease(policy);
    CFRelease(certs);
    CFRelease(cert);
}

static void tests(void)
{
    validate_one_cert(Global_Trustee_cer, sizeof(Global_Trustee_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_yahoo_com_1_cer, sizeof(login_yahoo_com_1_cer), 3, kSecTrustResultFatalTrustFailure);
    /* this is the root, which isn't ok for ssl and fails here, but at the
       same time it proves that kSecTrustResultFatalTrustFailure isn't
       returned for policy failures that aren't blacklisting */
    validate_one_cert(login_yahoo_com_2_cer, sizeof(login_yahoo_com_2_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(addons_mozilla_org_cer, sizeof(addons_mozilla_org_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_yahoo_com_cer, sizeof(login_yahoo_com_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_live_com_cer, sizeof(login_live_com_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(mail_google_com_cer, sizeof(mail_google_com_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(login_skype_com_cer, sizeof(login_skype_com_cer), 3, kSecTrustResultFatalTrustFailure);
    validate_one_cert(www_google_com_cer, sizeof(www_google_com_cer), 3, kSecTrustResultFatalTrustFailure);
}

int si_67_sectrust_blacklist(int argc, char *const *argv)
{
	plan_tests(45);

	tests();

	return 0;
}
