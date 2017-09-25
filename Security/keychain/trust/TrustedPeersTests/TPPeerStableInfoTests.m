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
#import "TPDummySigningKey.h"

@interface TPPeerStableInfoTests : XCTestCase

@end

@implementation TPPeerStableInfoTests

- (void)testSigningKeyIsUnavailable
{
    NSData *keyData = [@"key123" dataUsingEncoding:NSUTF8StringEncoding];
    TPDummySigningKey *key = [[TPDummySigningKey alloc] initWithPublicKeyData:keyData];
    key.privateKeyIsAvailable = NO;
    
    NSError *error = nil;
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithDict:@{}
                                     clock:1
                             policyVersion:1
                                policyHash:@"foo"
                             policySecrets:nil
                           trustSigningKey:key
                                     error:&error];
    XCTAssertNil(info);
    XCTAssertNotNil(error);
}

- (void)testNonDictionary
{
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:@[ @"foo", @"bar"]
                                                              format:NSPropertyListXMLFormat_v1_0
                                                             options:0
                                                               error:NULL];
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithPListData:data
                                  stableInfoSig:data];
    XCTAssertNil(info);
}

- (void)testBadClock
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"clock": @"five"
                                                            }];
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithPListData:data
                                  stableInfoSig:data];
    XCTAssertNil(info);
}

- (void)testBadPolicyVersion
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"clock": @5,
                                                            @"policyVersion": @"five",
                                                            }];
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithPListData:data
                                  stableInfoSig:data];
    XCTAssertNil(info);
}

- (void)testBadPolicyHash
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"clock": @5,
                                                            @"policyVersion": @5,
                                                            @"policyHash": @5
                                                            }];
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithPListData:data
                                  stableInfoSig:data];
    XCTAssertNil(info);
}

- (void)testBadSecrets
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"clock": @5,
                                                            @"policyVersion": @5,
                                                            @"policyHash": @"foo",
                                                            @"policySecrets": @5
                                                            }];
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithPListData:data
                                  stableInfoSig:data];
    XCTAssertNil(info);
}

- (void)testBadSecretData
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"clock": @5,
                                                            @"policyVersion": @5,
                                                            @"policyHash": @"foo",
                                                            @"policySecrets": @{
                                                                    @"foo": @5
                                                                    }
                                                            }];
    TPPeerStableInfo *info
    = [TPPeerStableInfo stableInfoWithPListData:data
                                  stableInfoSig:data];
    XCTAssertNil(info);
}

@end
