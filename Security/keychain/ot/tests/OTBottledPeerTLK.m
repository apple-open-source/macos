/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
 *
 * @APPLEself.LICENSEself.HEADERself.START@
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
 * @APPLEself.LICENSEself.HEADERself.END@
 */

#if OCTAGON

#import "OTTestsBase.h"

@interface BottledPeerRestoreTLKTests : OTTestsBase
@property  CKKSSOSSelfPeer* remotePeer1;
@property  CKKSSOSPeer* remotePeer2;
@property  CKKSSOSSelfPeer* untrustedPeer;

@end

@implementation BottledPeerRestoreTLKTests

- (void)setUp
{
    [super setUp];

    //set up a bottled peer and stick it in localStore
    NSError* error = nil;

    self.remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:self.sosPeerID
                                                    encryptionKey:self.peerEncryptionKey
                                                       signingKey:self.peerSigningKey];

    [self.currentPeers addObject:self.remotePeer1];

    OTBottledPeer *bp = [[OTBottledPeer alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionKey:self.peerEncryptionKey escrowKeys:self.escrowKeys error:&error];

    XCTAssertNotNil(bp, @"plaintext should not be nil");
    XCTAssertNil(error, @"error should be nil");
    XCTAssertNotNil(self.escrowKeys.signingKey, @"signing public key should not be nil");
    XCTAssertNotNil(self.escrowKeys.encryptionKey, @"encryption public key should not be nil");

    OTBottledPeerSigned *bpSigned = [[OTBottledPeerSigned alloc]initWithBottledPeer:bp escrowedSigningKey:self.escrowKeys.signingKey peerSigningKey:self.peerSigningKey error:&error];

    OTBottledPeerRecord* record = [bpSigned asRecord:[self currentIdentity:&error].spID];
    self.recordName = record.recordName;
    
    OTIdentity* identity = [self currentIdentity:&error];
    [self.localStore insertBottledPeerRecord:record escrowRecordID:identity.spID error:&error];

    self.remotePeer1 = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"remote-peer1"
                                                    encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                       signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];

    self.remotePeer2 = [[CKKSSOSPeer alloc] initWithSOSPeerID:@"remote-peer2"
                                          encryptionPublicKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]].publicKey
                                             signingPublicKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]].publicKey];

    // Local SOS trusts these peers
    [self.currentPeers addObject:self.remotePeer1];
    [self.currentPeers addObject:self.remotePeer2];

    self.untrustedPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:@"untrusted-peer"
                                                      encryptionKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]
                                                         signingKey:[[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]]];
}

- (void)tearDown
{
    _remotePeer1 = nil;
    _remotePeer2 = nil;
    _untrustedPeer = nil;
    [super tearDown];
}

-(void) testTLKSharingWithRestoredBottledPeer
{
    NSError* error = nil;

    OTBottledPeerRecord *rec = [self.localStore readLocalBottledPeerRecordWithRecordID:self.recordName error:&error];
    XCTAssertNotNil(rec, @"rec should not be nil: %@", error);
    XCTAssertNil(error, @"error should be nil: %@", error);

    OTBottledPeerSigned *bps = [[OTBottledPeerSigned alloc] initWithBottledPeerRecord:rec
                                                                           escrowKeys:self.escrowKeys
                                                                                error:&error];
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(bps, @"signed bottled peer should not be nil: %@", error);
    XCTAssertTrue([bps.bp.peerEncryptionKey isEqual:self.peerEncryptionKey], @"enrolled and restored peer encryption keys should match");
    XCTAssertTrue([bps.bp.peerSigningKey isEqual:self.peerSigningKey], @"enrolled and restored peer signing keys should match");

    
    CKKSSelves* selves = [[CKKSViewManager manager] fetchSelfPeers:&error];
    XCTAssertNotNil(selves, @"selves should not be nil: %@", error);
    
    XCTAssertTrue([selves.allSelves count] == 2, @"should have 2 selves");
    NSArray *arrayOfSelves = [selves.allSelves allObjects];
    XCTAssertNotNil(arrayOfSelves, @"arrayOfSelves should not be nil: %@", error);

    CKKSSOSSelfPeer *ourRestoredPeer = [arrayOfSelves objectAtIndex:0];
    if([ourRestoredPeer.peerID isEqualToString:@"spid-local-peer"]){
        ourRestoredPeer = [arrayOfSelves objectAtIndex:1];
    }

    XCTAssertTrue([ourRestoredPeer.peerID containsString:self.sosPeerID], @"peer ids should match!");
    XCTAssertTrue([ourRestoredPeer.signingKey isEqual:self.peerSigningKey], @"signing keys should match");
    XCTAssertTrue([ourRestoredPeer.encryptionKey isEqual:self.peerEncryptionKey], @"encryption keys should match");

    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    // The CKKS subsystem should not try to write anything to the CloudKit database, but it should enter waitfortlk
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "Key state should become waitfortlk");

    // peer1 arrives to save the day
    // The CKKS subsystem should accept the keys, and share the TLK back to itself
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    [self putTLKSharesInCloudKit:self.keychainZoneKeys.tlk from:ourRestoredPeer zoneID:self.keychainZoneID];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should become ready");

    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

- (nullable OTIdentity *)currentIdentity:(NSError * _Nullable __autoreleasing * _Nullable)error {
    return [[OTIdentity alloc]initWithPeerID:@"ego peer id" spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionkey:self.peerEncryptionKey error:error];
}

@end
#endif
