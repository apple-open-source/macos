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

#include "shared_regressions.h"

#import <AssertMacros.h>
#import <Foundation/Foundation.h>

#import <Security/SecCMS.h>
#import <Security/SecTrust.h>
#import <Security/SecTrustPriv.h>
#include <utilities/SecCFRelease.h>

#import "si-25-cms-skid.h"

static void test_cms_verification(void)
{
    NSData *content = [NSData dataWithBytes:_content length:sizeof(_content)];
    NSData *signedData = [NSData dataWithBytes:_signedData length:sizeof(_signedData)];

    SecPolicyRef policy = SecPolicyCreateBasicX509();
    SecTrustRef trust = NULL;
    CFArrayRef certificates = NULL;

    ok_status(SecCMSVerify((__bridge CFDataRef)signedData, (__bridge CFDataRef)content, policy, &trust, NULL), "verify CMS message");

    /* verify that CMS stack found the certs in the CMS (using the SKID) and stuck them in the trust ref */
    ok_status(SecTrustCopyInputCertificates(trust, &certificates), "copy input certificates");
    CFIndex expectedCertCount = 4;
#if TARGET_OS_IPHONE
    /* The legacy iOS implementation re-orders the 3 certs, whereas the legacy macOS and new implementations
     * prepend the signer cert to all certificates in the CMS object */
    if (!useMessageSecurityEnabled()) {
        expectedCertCount = 3;
    }
#endif
    is(CFArrayGetCount(certificates), expectedCertCount, "%d certs in the cms", (int)expectedCertCount);

    CFReleaseNull(policy);
    CFReleaseNull(trust);
    CFReleaseNull(certificates);
}

int si_25_cms_skid(int argc, char *const *argv)
{
    plan_tests(3);

    test_cms_verification();

    return 0;
}
