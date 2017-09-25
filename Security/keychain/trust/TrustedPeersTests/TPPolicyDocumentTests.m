/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#import <XCTest/XCTest.h>
#import <TrustedPeers/TrustedPeers.h>

@interface TPPolicyDocumentTests : XCTestCase

@end

@implementation TPPolicyDocumentTests

- (void)testRoundTrip
{
    TPPolicyDocument *doc1
    = [TPPolicyDocument policyDocWithVersion:1
                             modelToCategory:@[
                                               @{ @"prefix": @"iPhone",  @"category": @"full" },
                                               @{ @"prefix": @"iPad",    @"category": @"full" },
                                               @{ @"prefix": @"Mac",     @"category": @"full" },
                                               @{ @"prefix": @"iMac",    @"category": @"full" },
                                               @{ @"prefix": @"AppleTV", @"category": @"tv" },
                                               @{ @"prefix": @"Watch",   @"category": @"watch" },
                                               ]
                            categoriesByView:@{
                                               @"WiFi":              @[ @"full", @"tv", @"watch" ],
                                               @"SafariCreditCards": @[ @"full" ],
                                               @"PCSEscrow":         @[ @"full" ]
                                               }
                       introducersByCategory:@{
                                               @"full":  @[ @"full" ],
                                               @"tv":    @[ @"full", @"tv" ],
                                               @"watch": @[ @"full", @"watch" ]
                                               }
                                  redactions:@{
                                               @"foo": [@"bar" dataUsingEncoding:NSUTF8StringEncoding]
                                               }
                                    hashAlgo:kTPHashAlgoSHA256];
    
    
    TPPolicyDocument *doc2 = [TPPolicyDocument policyDocWithHash:doc1.policyHash pList:doc1.pList];
    XCTAssert([doc1 isEqualToPolicyDocument:doc2]);
    
    TPPolicyDocument *doc3 = [TPPolicyDocument policyDocWithHash:@"SHA256:foo" pList:doc1.pList];
    XCTAssertNil(doc3);
}

@end
