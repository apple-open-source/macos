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

@interface TPPeerPermanentInfoTests : XCTestCase
@property (nonatomic, strong) TPPeerPermanentInfo* info;
@end

@implementation TPPeerPermanentInfoTests

- (void)setUp
{
    NSData *keyData = [@"key123" dataUsingEncoding:NSUTF8StringEncoding];
    TPDummySigningKey *key = [[TPDummySigningKey alloc] initWithPublicKeyData:keyData];
    
    self.info
    = [TPPeerPermanentInfo permanentInfoWithMachineID:@"machine123"
                                              modelID:@"iPhone1,1"
                                                epoch:7
                                      trustSigningKey:key
                                       peerIDHashAlgo:kTPHashAlgoSHA256
                                                error:NULL];
    XCTAssertNotNil(self.info);
}

- (void)testRoundTrip
{
    TPCounter epoch = 7;
    NSString *machineID = @"machine123";
    NSString *modelID = @"iPhone1,1";
    
    NSData *keyData = [@"key123" dataUsingEncoding:NSUTF8StringEncoding];
    
    TPPeerPermanentInfo *info2
    = [TPPeerPermanentInfo permanentInfoWithPeerID:self.info.peerID
                                permanentInfoPList:self.info.permanentInfoPList
                                  permanentInfoSig:self.info.permanentInfoSig
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    
    XCTAssertEqual(info2.epoch, epoch);
    XCTAssert([info2.machineID isEqualToString:machineID]);
    XCTAssert([info2.modelID isEqualToString:modelID]);
    XCTAssert([info2.trustSigningKey.publicKey isEqualToData:keyData]);
    
    XCTAssert([info2.peerID isEqualToString:self.info.peerID]);
    XCTAssert([info2.permanentInfoPList isEqualToData:self.info.permanentInfoPList]);
    XCTAssert([info2.permanentInfoSig isEqualToData:self.info.permanentInfoSig]);
}

- (void)testNonDictionary
{
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:@[ @"foo", @"bar"]
                                                              format:NSPropertyListXMLFormat_v1_0
                                                             options:0
                                                               error:NULL];
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"x"
                                permanentInfoPList:data
                                  permanentInfoSig:data
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info);
}

- (void)testBadMachineID
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"machineID": @5
                                                            }];
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"x"
                                permanentInfoPList:data
                                  permanentInfoSig:data
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info);
}

- (void)testBadModelID
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"machineID": @"aaa",
                                                            @"modelID": @5,
                                                            }];
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"x"
                                permanentInfoPList:data
                                  permanentInfoSig:data
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info);
}

- (void)testBadEpoch
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"machineID": @"aaa",
                                                            @"modelID": @"iPhone7,1",
                                                            @"epoch": @"five",
                                                            }];
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"x"
                                permanentInfoPList:data
                                  permanentInfoSig:data
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info);
}

- (void)testBadTrustSigningKey
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"machineID": @"aaa",
                                                            @"modelID": @"iPhone7,1",
                                                            @"epoch": @5,
                                                            @"trustSigningKey": @"foo",
                                                            }];
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"x"
                                permanentInfoPList:data
                                  permanentInfoSig:data
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info);
}

- (void)testBadTrustSigningKey2
{
    NSData *data = [TPUtils serializedPListWithDictionary:@{
                                                            @"machineID": @"aaa",
                                                            @"modelID": @"iPhone7,1",
                                                            @"epoch": @5,
                                                            @"trustSigningKey": [NSData data],
                                                            }];
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"x"
                                permanentInfoPList:data
                                  permanentInfoSig:data
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info);
}

- (void)testBadSignature
{
    TPPeerPermanentInfo *info2
    = [TPPeerPermanentInfo permanentInfoWithPeerID:self.info.peerID
                                permanentInfoPList:self.info.permanentInfoPList
                                  permanentInfoSig:[NSData data]
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info2);
}

- (void)testBadHashAlgo
{
    TPPeerPermanentInfo *info2
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"foo"
                                permanentInfoPList:self.info.permanentInfoPList
                                  permanentInfoSig:self.info.permanentInfoSig
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info2);
}

- (void)testBadPeerID
{
    TPPeerPermanentInfo *info2
    = [TPPeerPermanentInfo permanentInfoWithPeerID:@"SHA256:foo"
                                permanentInfoPList:self.info.permanentInfoPList
                                  permanentInfoSig:self.info.permanentInfoSig
                                        keyFactory:[TPDummySigningKeyFactory dummySigningKeyFactory]];
    XCTAssertNil(info2);
}

- (void)testSigningKeyIsUnavailable
{
    NSData *keyData = [@"key123" dataUsingEncoding:NSUTF8StringEncoding];
    TPDummySigningKey *key = [[TPDummySigningKey alloc] initWithPublicKeyData:keyData];
    key.privateKeyIsAvailable = NO;
    
    NSError *error = nil;
    TPPeerPermanentInfo *info
    = [TPPeerPermanentInfo permanentInfoWithMachineID:@"machine123"
                                              modelID:@"iPhone1,1"
                                                epoch:7
                                      trustSigningKey:key
                                       peerIDHashAlgo:kTPHashAlgoSHA256
                                                error:&error];
    XCTAssertNil(info);
    XCTAssertNotNil(error);
}

@end
