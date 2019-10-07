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

@interface OTRampingUnitTests : OTTestsBase

@end

@implementation OTRampingUnitTests

- (void)setUp {
    [super setUp];
    self.continueAfterFailure = NO;
}

- (void)tearDown {
    [super tearDown];
}

-(void) testPreflightWithFeatureOnOn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];

    [self startCKKSSubsystem];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];

}

-(void) testBottleUpdateWithFeatureOnOn
{
    __block NSData* localEntropy = nil;
    __block NSString* localBottleID = nil;

    [self setUpRampRecordsInCloudKitWithFeatureOn];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];

    [self startCKKSSubsystem];

    //create a bottle
    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       localEntropy = entropy;
                                       localBottleID = bottleID;
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    //launch it
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];

    [self expectCKFetch];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    XCTAssertNil(error, "error should be nil");
                                                }];
}

-(void) testLaunchWithRampOn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
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

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];
    
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testRestoreWithRampOn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];
    
    __block NSData* localEntropy = nil;
    __block NSString* localBottleID = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
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

    __block NSData* localSigningKeyData = nil;
    __block NSData* localEncryptionKeyData = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl restore:testContextID
                       dsid:testDSID
                     secret:localEntropy
             escrowRecordID:self.sosPeerID
                      reply:^(NSData* signingKeyData, NSData* encryptionKeyData, NSError* _Nullable error) {
                          [self.spiBlockExpectation fulfill];
                          localSigningKeyData = signingKeyData;
                          localEncryptionKeyData = encryptionKeyData;
                          XCTAssertNotNil(signingKeyData, "Signing key data should not be nil");
                          XCTAssertNotNil(encryptionKeyData, "encryption key data should not be nil");
                          XCTAssertNil(error, "error should not be nil");
                      }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
    NSError* localError = nil;
    
    OTIdentity *ourSelf = [self currentIdentity:&localError];
    XCTAssertTrue([localSigningKeyData isEqualToData:[ourSelf.peerSigningKey.publicKey keyData]], @"signing keys should be equal!");
    XCTAssertTrue([localEncryptionKeyData isEqualToData:[ourSelf.peerEncryptionKey.publicKey keyData]], @"signing keys should be equal!");
}

-(void) testScrubWithRampOn
{
    [self setUpRampRecordsInCloudKitWithFeatureOn];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    __block NSString* localBottleID = nil;

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       localBottleID = bottleID;
                                       XCTAssertNotNil(entropy, "entropy should not be nil");
                                       XCTAssertNotNil(bottleID, "bottle id should not be nil");
                                       XCTAssertNotNil(signingPublicKey, "signing pub key should not be nil");
                                       XCTAssertNil(error, "error should be nil");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    self.spiBlockExpectation = [self expectationWithDescription:@"scrub scheduler fired"];

    [self.otControl scrubBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertNil(error, "error should be nil");
    }];
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    NSError* localError = nil;
    NSArray* bottles = [self.localStore readAllLocalBottledPeerRecords:&localError];
    XCTAssertNotNil(localError, "error should not be nil");
    XCTAssertTrue([bottles count] == 0, "should be 0 bottles");
}

-(void) testPreflightWithRampOff
{
    [self setUpRampRecordsInCloudKitWithFeatureOff];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
                                   }];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testBottleUpdateWithFeatureOff
{
    [self setUpRampRecordsInCloudKitWithFeatureOff];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    SFECKeyPair* newSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    SFECKeyPair* newEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    //update bottle
    [self.otControl handleIdentityChangeForSigningKey:newSigningKey
                                     ForEncryptionKey:newEncryptionKey
                                            ForPeerID:self.sosPeerID
                                                reply:^(BOOL result, NSError* _Nullable error){
                                                    XCTAssertNotNil(error, "error should be nil");
                                                    XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
                                                }];

}

-(void) testPreflightWithRecordNotThere
{
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertNotNil(error, "should not be nil");
                                   }];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testLaunchWithRampOff
{
    [self setUpRampRecordsInCloudKitWithFeatureOff];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer fired"];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
                                   }];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];


    self.spiBlockExpectation = [self expectationWithDescription:@"launch SPI fired"];

    NSString* localBottleID = @"random bottle id";
    [self.otControl launchBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
    }];

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}
-(void) testRestoreWithRampOff
{
    [self setUpRampRecordsInCloudKitWithFeatureOff];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
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
                          XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
                      }];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];
}

-(void) testScrubWithRampOff
{
    [self setUpRampRecordsInCloudKitWithFeatureOff];
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    self.spiBlockExpectation = [self expectationWithDescription:@"preflight bottled peer SPI fired"];

    [self.otControl preflightBottledPeer:testContextID
                                    dsid:testDSID
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       [self.spiBlockExpectation fulfill];
                                       XCTAssertNil(entropy, "entropy should be nil");
                                       XCTAssertNil(bottleID, "bottle id should be nil");
                                       XCTAssertNil(signingPublicKey, "signing pub key should be nil");
                                       XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
                                   }];

    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    __block NSString* localBottleID = @"random bottle id";
    self.spiBlockExpectation = [self expectationWithDescription:@"scrub bottled peer SPI fired"];

    [self.otControl scrubBottledPeer:testContextID bottleID:localBottleID reply:^(NSError * _Nullable error) {
        [self.spiBlockExpectation fulfill];
        XCTAssertEqual(error.code, OTErrorFeatureNotEnabled, "should return a OTErrorFeatureNotEnabled error");
    }];
    
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:1.0 handler:nil];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:4*NSEC_PER_SEC], @"Key state should have arrived at ready");
}

-(void) testRampFetchTimeout
{
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    __block NSError* localError = nil;

    [self holdCloudKitFetches];

    [self.otControl preflightBottledPeer:OTDefaultContext
                                    dsid:@"dsid"
                                   reply:^(NSData * _Nullable entropy, NSString * _Nullable bottleID, NSData * _Nullable signingPublicKey, NSError * _Nullable error) {
                                       localError = error;
                                       XCTAssertNil(entropy, "shouldn't return any entropy");
                                       XCTAssertNil(bottleID, "shouldn't return a bottle ID");
                                       XCTAssertNil(signingPublicKey, "shouldn't return a signingPublicKey");
                                       XCTAssertEqual(error.code, OTErrorCKTimeOut, "should return a OTErrorCKTimeout error");
                                   }];
}

-(void)testCFUWithRampOn
{
    NSError* localError = nil;

    [self setUpRampRecordsInCloudKitWithFeatureOn];

    [self startCKKSSubsystem];

    XCTAssertTrue([self.cfu checkRampStateWithError:&localError], @"should be true");
    XCTAssertNil(localError, "Should not have gotten an error checking ramp state (and getting true)");
}

-(void)testCFUWithRampOff
{
    NSError* localError = nil;
    [self setUpRampRecordsInCloudKitWithFeatureOff];

    [self startCKKSSubsystem];

    XCTAssertFalse([self.cfu checkRampStateWithError:&localError], @"should be false");
    XCTAssertNil(localError, "Should not have gotten an error checking ramp state (and getting false)");
}

-(void)testCFUWithNonExistentRampRecord
{
    NSError* localError = nil;

    [self startCKKSSubsystem];

    XCTAssertFalse([self.cfu checkRampStateWithError:&localError], @"should be false");
    XCTAssertNotNil(localError, "Should have gotten an error checking ramp state (and getting false)");
    XCTAssertEqual(localError.code, OTErrorRecordNotFound, "Error should be 'record not found'");
}

@end

#endif /* OCTAGON */

