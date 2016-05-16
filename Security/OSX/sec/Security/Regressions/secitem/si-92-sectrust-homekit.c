/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
 */

#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecCertificatePriv.h>
#include <Security/SecPolicyPriv.h>

#include "utilities/SecCFRelease.h"
#include "utilities/SecCFWrappers.h"

#include "Security_regressions.h"

#include "si-92-sectrust-homekit.h"

static void tests(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL, rootcert = NULL;
    SecTrustResultType trustResult;
    CFArrayRef certs = NULL, anchor_certs = NULL;

    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _AppleHomeKitUATServer, sizeof(_AppleHomeKitUATServer)), NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _AppleHomeKitCA, sizeof(_AppleHomeKitCA)), NULL, "create cert1");
    isnt(rootcert = SecCertificateCreateWithBytes(NULL, _AppleG3Root, sizeof(_AppleG3Root)), NULL, "create root cert");

    const void *v_certs[] = { cert0, cert1 };
    certs = CFArrayCreate(NULL, v_certs, sizeof(v_certs)/sizeof(*v_certs), &kCFTypeArrayCallBacks);
    anchor_certs = CFArrayCreate(NULL, (const void**)&rootcert, 1, &kCFTypeArrayCallBacks);

    /* Set explicit verify date: 12 February 2016 */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 476992610.0), NULL, "Create verify date");

    /* Evaluate production certs with policy. Should succeed.*/
    isnt(policy = SecPolicyCreateAppleHomeKitServerAuth(CFSTR("homekit.accessories-qa.apple.com")), NULL, "create policy");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetAnchorCertificates(trust, anchor_certs), "set anchor");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");

    CFReleaseSafe(trust);
    CFReleaseSafe(certs);
    CFReleaseSafe(cert0);
    CFReleaseSafe(cert1);
    CFReleaseSafe(anchor_certs);
    CFReleaseSafe(rootcert);

    /* Evaluate certs with a different profile against this test. Should fail. */
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _testLeaf, sizeof(_testLeaf)), NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _testServerAuthCA, sizeof(_testServerAuthCA)), NULL, "create cert1");
    isnt(rootcert = SecCertificateCreateWithBytes(NULL, _testRoot, sizeof(_testRoot)), NULL, "create root cert");

    const void *v_certs2[] = { cert0, cert1 };
    certs = CFArrayCreate(NULL, v_certs2, sizeof(v_certs2)/sizeof(*v_certs2), &kCFTypeArrayCallBacks);
    anchor_certs = CFArrayCreate(NULL, (const void**)&rootcert, 1, &kCFTypeArrayCallBacks);

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetAnchorCertificates(trust, anchor_certs), "set anchor");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trustResult is kSecTrustResultRecoverableTrustFailure");

    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(cert0);
    CFReleaseSafe(cert1);
    CFReleaseSafe(anchor_certs);
    CFReleaseSafe(rootcert);

}


int si_92_sectrust_homekit(int argc, char *const *argv)
{
    plan_tests(19);

    tests();

    return 0;
}
