/*
 * Copyright (c) 2015 Apple Inc. All Rights Reserved.
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


#include "si-91-sectrust-ast2.h"

static void tests(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef cert0 = NULL, cert1 = NULL, rootcert = NULL;
    SecTrustResultType trustResult;
    CFDictionaryRef allowTestRoot = NULL;

    isnt(cert0 = SecCertificateCreateWithBytes(NULL, _ast2TestLeaf, sizeof(_ast2TestLeaf)), NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, _AppleTestServerAuthCA, sizeof(_AppleTestServerAuthCA)), NULL, "create cert1");
    isnt(rootcert = SecCertificateCreateWithBytes(NULL, _AppleTestRoot, sizeof(_AppleTestRoot)), NULL, "create root cert");

    const void *v_certs[] = { cert0, cert1 };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, sizeof(v_certs)/sizeof(*v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef anchor_certs = CFArrayCreate(NULL, (const void**)&rootcert, 1, &kCFTypeArrayCallBacks);

    /* Set explicit verify date: 15 Dec 2015 */
    CFDateRef date = NULL;
    isnt(date = CFDateCreate(NULL, 471907305.0), NULL, "Create verify date");

    /* Evaluate test certs with production policy. Should fail. */
    isnt(policy = SecPolicyCreateAppleAST2Service(CFSTR("ast2.test.domain.here"), NULL), NULL, "create prod policy");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetAnchorCertificates(trust, anchor_certs), "set anchor");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trustResult is kSecTrustResultRecoverableTrustFailure");

    CFReleaseSafe(trust);
    CFReleaseSafe(policy);

    /* Evaluate test certs with test root allowed */
    CFStringRef key = CFSTR("AppleServerAuthenticationAllowUATAST2");
    isnt(allowTestRoot = CFDictionaryCreate(NULL, (const void **)&key, (const void **)&kCFBooleanTrue, 1,
                                            &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks),
         NULL, "create context dictionary");
    isnt(policy = SecPolicyCreateAppleAST2Service(CFSTR("ast2.test.domain.here"), allowTestRoot), NULL, "create test policy");

    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    ok_status(SecTrustSetAnchorCertificates(trust, anchor_certs), "set anchor");
    ok_status(SecTrustSetVerifyDate(trust, date), "set date");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");

    CFReleaseSafe(date);
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(cert0);
    CFReleaseSafe(cert1);
    CFReleaseSafe(anchor_certs);
    CFReleaseSafe(rootcert);
    CFReleaseSafe(key);

}


int si_91_sectrust_ast2(int argc, char *const *argv)
{
    plan_tests(18);

    tests();

    return 0;
}
