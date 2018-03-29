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
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFSigningOperation.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "keychain/ot/OTBottledPeer.h"
#import "keychain/ot/OTBottledPeerSigned.h"

#import "keychain/ckks/CKKS.h"

static NSString* const testDSID = @"123456789";

@interface UnitTestOTBottledPeer : OTTestsBase

@end

@implementation UnitTestOTBottledPeer

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
    NSError* error = nil;
    
    self.sosPeerID = @"spID";
    self.egoPeerID = @"egoPeerID";
    self.peerSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    self.peerEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    self.escrowKeys = [[OTEscrowKeys alloc]initWithSecret:self.secret dsid:testDSID error:&error];
}

- (void)tearDown
{
    [super tearDown];
}
                 
-(void)testBottledPeerCreation
{
    NSError* error = nil;
    
    OTBottledPeer *bp = [[OTBottledPeer alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionKey:self.peerEncryptionKey escrowKeys:self.escrowKeys error:&error];
    
    XCTAssertNotNil(bp, @"bottled peer should not be nil");
    XCTAssertNil(error, @"error should be nil");
    XCTAssertNotNil(self.escrowKeys.signingKey, @"signing public key should not be nil");
    XCTAssertNotNil(self.escrowKeys.encryptionKey, @"encryption public key should not be nil");
    
}

-(void)testSignedBottledPeerCreation
{
    NSError* error = nil;
    
    OTBottledPeer *bp = [[OTBottledPeer alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionKey:self.peerEncryptionKey escrowKeys:self.escrowKeys error:&error];
    
    XCTAssertNotNil(bp, @"plaintext should not be nil");
    XCTAssertNil(error, @"error should be nil");
    XCTAssertNotNil(self.escrowKeys.signingKey, @"signing public key should not be nil");
    XCTAssertNotNil(self.escrowKeys.encryptionKey, @"encryption public key should not be nil");
    
    OTBottledPeerSigned *bpSigned = [[OTBottledPeerSigned alloc]initWithBottledPeer:bp escrowedSigningKey:self.escrowKeys.signingKey peerSigningKey:self.peerSigningKey error:&error];
    XCTAssertNil(error, @"error should be nil");
    XCTAssertNotNil(bpSigned, @"bottled peer signed should not be nil");

}

-(void)testCreatingBottledPeerFromRecord
{
    NSError* error = nil;
    OTBottledPeer *bp = [[OTBottledPeer alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionKey:self.peerEncryptionKey escrowKeys:self.escrowKeys error:&error];
    
    XCTAssertNotNil(bp, @"plaintext should not be nil");
    XCTAssertNil(error, @"error should be nil");
    XCTAssertNotNil(self.escrowKeys.signingKey, @"signing public key should not be nil");
    XCTAssertNotNil(self.escrowKeys.encryptionKey, @"encryption public key should not be nil");
    
    OTBottledPeerSigned *bpSigned = [[OTBottledPeerSigned alloc]initWithBottledPeer:bp escrowedSigningKey:self.escrowKeys.signingKey peerSigningKey:self.peerSigningKey error:&error];
    
    OTBottledPeerRecord* record = [bpSigned asRecord:@"escrowRecordID"];
    OTBottledPeerSigned *bpRestored = [[OTBottledPeerSigned alloc] initWithBottledPeerRecord:record escrowKeys:self.escrowKeys error:&error];
    XCTAssertNotNil(bpRestored, @"bottled peer signed should not be nil");
}

-(void)testRestoringBottledPeerSigned
{
    NSError* error = nil;
    OTBottledPeer *bp = [[OTBottledPeer alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionKey:self.peerEncryptionKey escrowKeys:self.escrowKeys error:&error];
    
    XCTAssertNotNil(bp, @"plaintext should not be nil");
    XCTAssertNil(error, @"error should be nil");
 
    SFECKeySpecifier *keySpecifier = [[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384];
    id<SFDigestOperation> digestOperation = [[SFSHA384DigestOperation alloc] init];
    SFEC_X962SigningOperation* xso = [[SFEC_X962SigningOperation alloc] initWithKeySpecifier:keySpecifier digestOperation:digestOperation];

    NSData* signatureUsingEscrow = [xso sign:bp.data withKey:self.escrowKeys.signingKey error:&error].signature;
    XCTAssertNil(error, @"error should not be nil");

    NSData* signatureUsingPeerKey = [xso sign:bp.data withKey:self.peerSigningKey error:&error].signature;
    XCTAssertNil(error, @"error should not be nil");

    XCTAssertNotNil(signatureUsingEscrow, @"signature using escrow signing key should not be nil");
    XCTAssertNotNil(signatureUsingPeerKey, @"signature using peer signing key should not be nil");
    
    
     OTBottledPeerSigned *bpSigned = [[OTBottledPeerSigned alloc]initWithBottledPeer:bp signatureUsingEscrow:signatureUsingEscrow signatureUsingPeerKey:signatureUsingPeerKey escrowedSigningPubKey:[self.escrowKeys.signingKey publicKey] error:&error];
     
     XCTAssertNotNil(bpSigned, @"bottled peer signed should not be nil");
    
    bpSigned = [[OTBottledPeerSigned alloc]initWithBottledPeer:bp signatureUsingEscrow:[NSData data] signatureUsingPeerKey:[NSData data] escrowedSigningPubKey:[self.escrowKeys.signingKey publicKey] error:&error];

    XCTAssertNil(bpSigned, @"bottled peer signed should be nil");
    XCTAssertNotNil(error, @"error should not be nil");

}

@end

#endif /* OCTAGON */
