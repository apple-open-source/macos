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


#include "si-88-sectrust-vpnprofile.h"

static void tests(void)
{
    SecTrustRef trust = NULL;
    SecPolicyRef policy = NULL;
    SecCertificateRef cert0, cert1, cert2, cert3, rootcert;
    SecTrustResultType trustResult;
    
    //Evaluation should succeed for cert0 and cert1
    
    isnt(cert0 = SecCertificateCreateWithBytes(NULL, c0, sizeof(c0)), NULL, "create cert0");
    isnt(cert1 = SecCertificateCreateWithBytes(NULL, c1, sizeof(c1)), NULL, "create cert1");
    isnt(rootcert = SecCertificateCreateWithBytes(NULL, root, sizeof(root)), NULL, "create root cert");
    
    const void *v_certs[] = { cert0, cert1 };
    CFArrayRef certs = CFArrayCreate(NULL, v_certs, sizeof(v_certs)/sizeof(*v_certs), &kCFTypeArrayCallBacks);
    CFArrayRef anchor_certs = CFArrayCreate(NULL, (const void**)&rootcert, 1, &kCFTypeArrayCallBacks);
    
    /* Create AppleTV VPN profile signing policy instance. */
    isnt(policy = SecPolicyCreateAppleATVVPNProfileSigning(), NULL, "create policy");
    
    /* Create trust reference. */
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");
    
    ok_status(SecTrustSetAnchorCertificates(trust, anchor_certs), "set anchor");
    
    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultUnspecified, "trustResult is kSecTrustResultUnspecified");
    is(SecTrustGetCertificateCount(trust), 3, "cert count is 3");
    
    
    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(cert1);
    CFReleaseSafe(cert0);
    
    //Evaluation should fail for cert2 and cert3 (wrong OID, not Apple anchor)
    
    isnt(cert2 = SecCertificateCreateWithBytes(NULL, c2, sizeof(c2)), NULL, "create cert2");
    isnt(cert3 = SecCertificateCreateWithBytes(NULL, c3, sizeof(c3)), NULL, "create cert3");
    
    const void *v_certs2[] = { cert2, cert3 };
    certs = CFArrayCreate(NULL, v_certs2, sizeof(v_certs2)/sizeof(*v_certs2), &kCFTypeArrayCallBacks);
    
    isnt(policy = SecPolicyCreateAppleATVVPNProfileSigning(), NULL, "create policy");
    ok_status(SecTrustCreateWithCertificates(certs, policy, &trust), "create trust");

    ok_status(SecTrustEvaluate(trust, &trustResult), "evaluate trust");
    is_status(trustResult, kSecTrustResultRecoverableTrustFailure, "trustResult is kSecTrustResultRecoverableTrustFailure");

    CFReleaseSafe(trust);
    CFReleaseSafe(policy);
    CFReleaseSafe(certs);
    CFReleaseSafe(cert3);
    CFReleaseSafe(cert2);
}



int si_88_sectrust_vpnprofile(int argc, char *const *argv);

int si_88_sectrust_vpnprofile(int argc, char *const *argv)
{
    plan_tests(15);
    
    tests();
    
    return 0;
}
