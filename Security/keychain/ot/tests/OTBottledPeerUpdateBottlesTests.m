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

#if OCTAGON

#import <Foundation/Foundation.h>

#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#import "OTTestsBase.h"
#import "keychain/ot/OTConstants.h"

static NSString* const testContextID = @"Foo";
static NSString* const testDSID = @"123456789";

static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";

@interface OTUpdateBottlesTests : OTTestsBase

@end

@implementation OTUpdateBottlesTests

- (void)setUp {
    [super setUp];
    self.continueAfterFailure = NO;
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
}

- (void)tearDown {
    [super tearDown];
}

-(void)testUpdatingExistingBottle
{
    __block NSData* localEntropy = nil;
    __block NSString* localBottleID = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight SPI should fire"];

    [self setUpRampRecordsInCloudKitWithFeatureOn];

    [self startCKKSSubsystem];

    //create a bottle
    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       localEntropy = entropy;
                                       localBottleID = bottleID;
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"launch bottle SPI should fire"];

    //launch it
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];

    [self expectCKFetch];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"updating identity SPI should fire"];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    [self.spiBlockExpectation fulfill];
                                                    XCTAssertTrue(result, "result should be YES");
                                                    XCTAssertNil(error, "error should be nil");
    }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void)testUpdatingNonExistentBottles
{
    self.spiBlockExpectation = [self expectationWithDescription:@"updating identity SPI should fire"];

    [self setUpRampRecordsInCloudKitWithFeatureOn];

    [self startCKKSSubsystem];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    [self.spiBlockExpectation fulfill];
                                                    XCTAssertNotNil(error, "error should not be nil");
                                                    XCTAssertFalse(result, "result should be NO");
                                                }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void)testUpdatingBottleMissingKeys
{
    __block NSData* localEntropy = nil;
    __block NSString* localBottleID = nil;
    NSError *localError = nil;
    self.spiBlockExpectation = [self expectationWithDescription:@"preflight SPI should fire"];

    [self setUpRampRecordsInCloudKitWithFeatureOn];

    [self startCKKSSubsystem];

    //create a bottle
    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       localEntropy = entropy;
                                       localBottleID = bottleID;
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"launch bottle SPI should fire"];

    //launch it
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];

    [self expectCKFetch];

    //delete escrow keys
    OTEscrowKeys* keySet =  [[OTEscrowKeys alloc] initWithSecret:localEntropy dsid:testDSID error:&localError];
    XCTAssertNotNil(keySet, "keySet should not be nil");
    XCTAssertNil(localError, "localError should be nil");

    NSMutableDictionary* query = [@{
               (id)kSecClass : (id)kSecClassKey,
               (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
               (id)kSecReturnAttributes: @YES,
               (id)kSecAttrSynchronizable : (id)kCFBooleanFalse,
               } mutableCopy];

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef)query);
    XCTAssertEqual(status, 0, @"status should be 0");

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"updating bottle SPI should fire"];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    [self.spiBlockExpectation fulfill];
                                                    XCTAssertFalse(result, "result should be NO");
                                                    XCTAssertNotNil(error, "error should not be nil");
                                                }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

}

-(void)testUpdatingMultipleBottlesForSamePeer
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    [self startCKKSSubsystem];

    __block NSData* localEntropy1 = nil;
    __block NSString* localBottleID1 = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight SPI should fire"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       localEntropy1 = entropy;
                                       localBottleID1 = bottleID;
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"launching bottle SPI should fire"];

    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID1 reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    __block NSData* localEntropy2 = nil;
    __block NSString* localBottleID2 = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight SPI should fire"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       localEntropy2 = entropy;
                                       localBottleID2 = bottleID;
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"launching bottle SPI should fire"];

    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID2 reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForCKModifications];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    self.spiBlockExpectation = [self expectationWithDescription:@"updating bottle SPI should fire"];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    [self.spiBlockExpectation fulfill];
                                                    XCTAssertTrue(result, "result should be YES");
                                                    XCTAssertNil(error, "error should be nil");
                                                }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}


@end

#endif

