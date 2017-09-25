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

@interface TPPeerTests : XCTestCase

@property (nonatomic, strong) TPPeer *peer;
@property (nonatomic, strong) TPDummySigningKey *goodKey;
@property (nonatomic, strong) TPDummySigningKey *badKey;

@end

@implementation TPPeerTests

- (void)setUp
{
    NSData *goodKeyData = [@"goodKey" dataUsingEncoding:NSUTF8StringEncoding];
    self.goodKey = [[TPDummySigningKey alloc] initWithPublicKeyData:goodKeyData];
    
    NSData *badKeyData = [@"badKey" dataUsingEncoding:NSUTF8StringEncoding];
    self.badKey = [[TPDummySigningKey alloc] initWithPublicKeyData:badKeyData];
    
    TPPeerPermanentInfo *permanentInfo;
    permanentInfo = [TPPeerPermanentInfo permanentInfoWithMachineID:@"A"
                                                            modelID:@"iPhone8,1"
                                                              epoch:1
                                                    trustSigningKey:self.goodKey
                                                     peerIDHashAlgo:kTPHashAlgoSHA256
                                                              error:NULL];
    self.peer = [[TPPeer alloc] initWithPermanentInfo:permanentInfo];
}

- (void)testBadDynamicInfoKey
{
    // Create a dynamicInfo with the wrong key
    TPPeerDynamicInfo *dynamicInfo = [TPPeerDynamicInfo dynamicInfoWithCircleID:@"123"
                                                                         clique:@"clique"
                                                                       removals:0
                                                                          clock:1
                                                                trustSigningKey:self.badKey
                                                                          error:NULL];
    XCTAssertEqual(TPResultSignatureMismatch, [self.peer updateDynamicInfo:dynamicInfo]);
}

- (void)testStableInfo
{
    TPPeerStableInfo *info1 = [TPPeerStableInfo stableInfoWithDict:@{ @"hello": @"world1" }
                                                             clock:1
                                                     policyVersion:1
                                                        policyHash:@""
                                                     policySecrets:nil
                                                   trustSigningKey:self.goodKey
                                                             error:NULL];
    XCTAssertEqual(TPResultOk, [self.peer updateStableInfo:info1]);
    
    // Attempt update without advancing clock
    TPPeerStableInfo *info2 = [TPPeerStableInfo stableInfoWithDict:@{ @"hello": @"world2" }
                                                             clock:1
                                                     policyVersion:1
                                                        policyHash:@""
                                                     policySecrets:nil
                                                   trustSigningKey:self.goodKey
                                                             error:NULL];
    XCTAssertEqual(TPResultClockViolation, [self.peer updateStableInfo:info2]);
    XCTAssertEqualObjects(self.peer.stableInfo, info1);

    // Advance
    TPPeerStableInfo *info3 = [TPPeerStableInfo stableInfoWithDict:@{ @"hello": @"world3" }
                                                             clock:3
                                                     policyVersion:1
                                                        policyHash:@""
                                                     policySecrets:nil
                                                   trustSigningKey:self.goodKey
                                                             error:NULL];
    XCTAssertEqual(TPResultOk, [self.peer updateStableInfo:info3]);

    // No change, should return OK
    XCTAssertEqual(TPResultOk, [self.peer updateStableInfo:info3]);

    // Attempt replay
    XCTAssertEqual(TPResultClockViolation, [self.peer updateStableInfo:info1]);
    XCTAssertEqualObjects(self.peer.stableInfo, info3);
    
    // Attempt update with wrong key
    TPPeerStableInfo *info4 = [TPPeerStableInfo stableInfoWithDict:@{ @"hello": @"world4" }
                                                             clock:4
                                                     policyVersion:1
                                                        policyHash:@""
                                                     policySecrets:nil
                                                   trustSigningKey:self.badKey
                                                             error:NULL];
    XCTAssertEqual(TPResultSignatureMismatch, [self.peer updateStableInfo:info4]);
    XCTAssertEqualObjects(self.peer.stableInfo, info3);
}

@end
