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

static NSString* const testContextID = @"Foo";
static NSString* const testDSID = @"123456789";

static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";
/* Octagon Trust BottledPeerSchema  */
static NSString* OTCKRecordEscrowRecordID = @"escrowRecordID";
static NSString* OTCKRecordRecordID = @"bottledPeerRecordID";
static NSString* OTCKRecordSPID = @"spID";
static NSString* OTCKRecordEscrowSigningSPKI = @"escrowSigningSPKI";
static NSString* OTCKRecordPeerSigningSPKI = @"peerSigningSPKI";
static NSString* OTCKRecordEscrowSigningPubKey = @"escrowSigningPubKey";
static NSString* OTCKRecordPeerSigningPubKey = @"peerSigningPubKey";
static NSString* OTCKRecordSignatureFromEscrow = @"signatureUsingEscrow";
static NSString* OTCKRecordSignatureFromPeerKey = @"signatureUsingPeerKey";
static NSString* OTCKRecordBottle = @"bottle";

static NSString* OTCKRecordPeerID = @"peerID";

@interface OTLockStateNetworkingTests : OTTestsBase
@property (nonatomic, strong) OTBottledPeerRecord* fakeBottledPeerRecord;
@end

@implementation OTLockStateNetworkingTests

- (void)setUp {
    [super setUp];
    
    self.continueAfterFailure = NO;
    NSError* error = nil;
    
    OTBottledPeer *bp = [[OTBottledPeer alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionKey:self.peerEncryptionKey escrowKeys:self.escrowKeys error:&error];

    XCTAssertNotNil(bp, @"plaintext should not be nil");
    XCTAssertNil(error, @"error should be nil");
    XCTAssertNotNil(self.escrowKeys.signingKey, @"signing public key should not be nil");
    XCTAssertNotNil(self.escrowKeys.encryptionKey, @"encryption public key should not be nil");

    OTBottledPeerSigned *bpSigned = [[OTBottledPeerSigned alloc]initWithBottledPeer:bp escrowedSigningKey:self.escrowKeys.signingKey peerSigningKey:self.peerSigningKey error:&error];

    self.fakeBottledPeerRecord = [bpSigned asRecord:self.sosPeerID];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

}

- (void)tearDown {

    [super tearDown];
}

//Bottle Check tests

-(void) testGrabbingBottleLocallyCheckPerfectConditions
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];
    [self startCKKSSubsystem];

    __block NSData* localEntropy = nil;
    __block NSString* localBottleID = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

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

    self.spiBlockExpectation = [self expectationWithDescription:@"launch bottled peer fired"];

    [self expectAddedCKModifyRecords:@{OTCKRecordBottledPeerType: @1} holdFetch:NO];

    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    NSError* localError = nil;
    XCTAssertTrue([self.context doesThisDeviceHaveABottle:&localError] == BOTTLE, @"should have a bottle");
    XCTAssertNil(localError, "error should be nil");
}

-(void) testGrabbingBottleFromCloudKitCheckPerfectConditions
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType];
    newRecord[OTCKRecordPeerID] = self.fakeBottledPeerRecord.peerID;
    newRecord[OTCKRecordSPID] = @"spID";
    newRecord[OTCKRecordEscrowSigningSPKI] = self.fakeBottledPeerRecord.escrowedSigningSPKI;
    newRecord[OTCKRecordPeerSigningSPKI] = self.fakeBottledPeerRecord.peerSigningSPKI;
    newRecord[OTCKRecordEscrowRecordID] = self.fakeBottledPeerRecord.escrowRecordID;
    newRecord[OTCKRecordBottle] = self.fakeBottledPeerRecord.bottle;
    newRecord[OTCKRecordSignatureFromEscrow] = self.fakeBottledPeerRecord.signatureUsingEscrowKey;
    newRecord[OTCKRecordSignatureFromPeerKey] = self.fakeBottledPeerRecord.signatureUsingPeerKey;

    [self.otFakeZone addToZone:newRecord];

    [self startCKKSSubsystem];

    NSError* localError = nil;
    XCTAssertTrue([self.context doesThisDeviceHaveABottle:&localError] == BOTTLE, @"should have a bottle");
    XCTAssertNil(localError, "error should be nil");
}

-(void) testBottleCheckWhenLocked
{
    NSError* error = nil;
    self.aksLockState = true;
    [self.lockStateTracker recheck];
    [self setUpRampRecordsInCloudKitWithFeatureOff];

    XCTAssertTrue([self.context doesThisDeviceHaveABottle:&error] == UNCLEAR, @"bottle check should return unclear");

    XCTAssertNotNil(error, "error should not be nil");
    XCTAssertEqual(error.code, -25308, @"error should be interaction not allowed");
}

-(void) testBottleCheckWithNoNetwork
{
    NSError* error = nil;
    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];

    [self.reachabilityTracker setNetworkReachability:false];
    XCTAssertTrue([self.context doesThisDeviceHaveABottle:&error] == UNCLEAR, @"bottle check should return unclear");
    XCTAssertEqual(error.code, OTErrorNoNetwork, @"should have returned no network error");
}

-(void) testBottleCheckWhenNotSignedIn
{
    NSError* error = nil;

    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];

    XCTAssertTrue([self.context doesThisDeviceHaveABottle:&error] == UNCLEAR, @"bottle check should return unclear");
    XCTAssertEqual(error.code, OTErrorNotSignedIn, @"should have returned not signed in error");
}

//Bottle Update tests
-(void)testBottleUpdateNotSignedIn
{
    self.spiBlockExpectation = [self expectationWithDescription:@"handle identity change spis fired"];

    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusNoAccount;

    [self startCKKSSubsystem];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    [self.spiBlockExpectation fulfill];
                                                    XCTAssertTrue(result == NO, @"should return NO");
                                                    XCTAssertEqual(error.code, OTErrorNotSignedIn, @"should have returned not signed in error");
                                                }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testBottleUpdateWithNoNetwork
{
    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];

    [self.reachabilityTracker setNetworkReachability:false];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    XCTAssertEqual(error.code, OTErrorNoNetwork, @"should have returned OTErrorNoNetwork in error");
                                                }];
}

-(void) testBottleUpdateWhenLocked
{
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    XCTAssertEqual(error.code, errSecInteractionNotAllowed, @"should have returned errSecInteractionNotAllowed in error");
                                                }];
}

//Preflight tests
-(void)testPreflightNotSignedIn
{
    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusNoAccount;

    [self startCKKSSubsystem];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should not be nil");
                                       XCTAssertNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertEqual(error.code, OTErrorNotSignedIn, @"should have returned not signed in error");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

}

-(void) testPreflightWithNoNetwork
{
    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];

    [self.reachabilityTracker setNetworkReachability:false];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should not be nil");
                                       XCTAssertNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertEqual(error.code, OTErrorNoNetwork, @"should have returned OTErrorNoNetwork in error");
                                   }];

}

-(void) testPreflightWhenLocked
{
    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should not be nil");
                                       XCTAssertNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertEqual(error.code, errSecInteractionNotAllowed, @"should have returned errSecInteractionNotAllowed in error");
                                   }];
}

//Launch Bottle tests
-(void)testLaunchNotSignedIn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusNoAccount;

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertEqual(error.code, OTErrorNotSignedIn, "should return a OTErrorNotSignedIn error");
                                   }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    self.spiBlockExpectation = [self expectationWithDescription:@"launch SPI fired"];

    NSString* localBottleID = @"random bottle id";
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, OTErrorNotSignedIn, "should return a OTErrorNotSignedIn error");
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testLaunchWithNoNetwork
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];

    [self.reachabilityTracker setNetworkReachability:false];

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertEqual(error.code, OTErrorNoNetwork, "should return a OTErrorNoNetwork error");
                                   }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];


    self.spiBlockExpectation = [self expectationWithDescription:@"launch SPI fired"];

    NSString* localBottleID = @"random bottle id";
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, OTErrorNoNetwork, "should return a OTErrorNoNetwork error");
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testLaunchWhenLocked
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertEqual(error.code, errSecInteractionNotAllowed, "should return a errSecInteractionNotAllowed error");
                                   }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];


    self.spiBlockExpectation = [self expectationWithDescription:@"launch SPI fired"];

    NSString* localBottleID = @"random bottle id";
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, errSecInteractionNotAllowed, "should return a errSecInteractionNotAllowed error");
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

//Scrub tests
-(void)testScrubNotSignedIn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer SPI fired"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should be nil");
                                       XCTAssertNil(bottleID, "bottle id should be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should be nil");
                                       XCTAssertEqual(error.code, OTErrorNotSignedIn, "should return a OTErrorNotSignedIn error");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    __block NSString* localBottleID = @"random bottle id";
    self.spiBlockExpectation = [self expectationWithDescription:@"scrub bottled peer SPI fired"];

    [self.otControl scrubBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, OTErrorNotSignedIn, "should return a OTErrorNotSignedIn error");
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

}

-(void) testScrubWithNoNetwork
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusAvailable;

    [self.reachabilityTracker setNetworkReachability:false];

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer SPI fired"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should be nil");
                                       XCTAssertNil(bottleID, "bottle id should be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should be nil");
                                       XCTAssertEqual(error.code, OTErrorNoNetwork, "should return a OTErrorNoNetwork error");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    __block NSString* localBottleID = @"random bottle id";
    self.spiBlockExpectation = [self expectationWithDescription:@"scrub bottled peer SPI fired"];

    [self.otControl scrubBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, OTErrorNoNetwork, "should return a OTErrorNoNetwork error");
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testScrubWhenLocked
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer SPI fired"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should be nil");
                                       XCTAssertNil(bottleID, "bottle id should be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should be nil");
                                       XCTAssertEqual(error.code, errSecInteractionNotAllowed, "should return a errSecInteractionNotAllowed error");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    __block NSString* localBottleID = @"random bottle id";
    self.spiBlockExpectation = [self expectationWithDescription:@"scrub bottled peer SPI fired"];

    [self.otControl scrubBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, errSecInteractionNotAllowed, "should return a errSecInteractionNotAllowed error");
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

//Restore tests
-(void)testRestoreNotSignedIn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusNoAccount;

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"restore SPI fired"];

    [self.otControl restore:testContextID
                       dsid:testDSID
                     secret:self.secret
             escrowRecordID:self.sosPeerID
                      reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error) {
                          [self.spiBlockExpectation fulfill];
                          XCTAssertNil(signingKeyData, "Signing key data should be nil");
                          XCTAssertNil(encryptionKeyData, "encryption key data should be nil");
                          XCTAssertEqual(error.code, OTErrorNotSignedIn, "should return a OTErrorNotSignedIn error");
                      }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

}

-(void) testRestoreWithNoNetwork
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.accountStatus = CKAccountStatusAvailable;

    [self.reachabilityTracker setNetworkReachability:false];

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"restore SPI fired"];

    [self.otControl restore:testContextID
                       dsid:testDSID
                     secret:self.secret
             escrowRecordID:self.sosPeerID
                      reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error) {
                          [self.spiBlockExpectation fulfill];
                          XCTAssertNil(signingKeyData, "Signing key data should be nil");
                          XCTAssertNil(encryptionKeyData, "encryption key data should be nil");
                          XCTAssertEqual(error.code, OTErrorNoNetwork, "should return a OTErrorNoNetwork error");
                      }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testRestoreWhenLocked
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"restore SPI fired"];

    [self.otControl restore:testContextID
                       dsid:testDSID
                     secret:self.secret
             escrowRecordID:self.sosPeerID
                      reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error) {
                          [self.spiBlockExpectation fulfill];
                          XCTAssertNil(signingKeyData, "Signing key data should be nil");
                          XCTAssertNil(encryptionKeyData, "encryption key data should be nil");
                          XCTAssertEqual(error.code, errSecInteractionNotAllowed, "should return a errSecInteractionNotAllowed error");
                      }];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

//Generic Ramp tests
-(void)testEnrollRampNotSignedIn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    NSError* error = nil;
    NSInteger retryAfter = 0;

    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];

    [self.enroll checkRampState:&retryAfter networkBehavior:CKOperationDiscretionaryNetworkBehaviorNonDiscretionary error:&error];

    XCTAssertEqual(error.code, OTErrorNotSignedIn, "should return a OTErrorNotSignedIn error");

}

-(void) testEnrollRampWithNoNetwork
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    NSError* error = nil;
    NSInteger retryAfter = 0;

    self.accountStatus = CKAccountStatusAvailable;
    [self startCKKSSubsystem];

    [self.reachabilityTracker setNetworkReachability:false];

    [self.enroll checkRampState:&retryAfter networkBehavior:CKOperationDiscretionaryNetworkBehaviorNonDiscretionary error:&error];

    XCTAssertEqual(error.code, OTErrorNoNetwork, "should return a OTErrorNoNetwork error");
}

-(void) testEnrollRampWhenLocked
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    NSError* error = nil;
    NSInteger retryAfter = 0;

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    [self.enroll checkRampState:&retryAfter networkBehavior:CKOperationDiscretionaryNetworkBehaviorNonDiscretionary error:&error];

    XCTAssertEqual(error.code, errSecInteractionNotAllowed, "should return a errSecInteractionNotAllowed error");
}

-(void) testTimeBetweenCFUAttempts
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    NSError* error = nil;

    [self startCKKSSubsystem];

    [self.manager scheduledCloudKitRampCheck:&error];
    XCTAssertNotNil(error, "Should have had an error scheduling a ramp check");
    XCTAssertEqual(error.code, OTErrorNoBottlePeerRecords, "Error should be 'no bottled peer records'");
    XCTAssertNotNil(self.manager.lastPostedCoreFollowUp, "core followup should have been posted");
    NSDate* firstTime = self.manager.lastPostedCoreFollowUp;

    sleep(2);
    error = nil;

    [self.manager scheduledCloudKitRampCheck:&error];
    XCTAssertNotNil(error, "Should have had an error scheduling a ramp check");
    XCTAssertEqual(error.code, OTErrorNoBottlePeerRecords, "Error should be 'no bottled peer records'");
    XCTAssertNotNil(self.manager.lastPostedCoreFollowUp, "core followup should have been posted");
    NSDate* secondTime = self.manager.lastPostedCoreFollowUp;

    XCTAssertTrue([secondTime timeIntervalSinceDate:firstTime] >= 2, "time difference should be slightly more than 2 seconds");
}

@end
#endif
