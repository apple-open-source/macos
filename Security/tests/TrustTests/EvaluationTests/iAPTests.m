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

#import <XCTest/XCTest.h>
#include "SecCertificatePriv.h"
#include "SecPolicyPriv.h"
#include "SecTrustPriv.h"
#include "OSX/utilities/SecCFWrappers.h"

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

@end
