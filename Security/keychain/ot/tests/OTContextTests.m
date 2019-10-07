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
static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";

@interface UnitTestOTContext : OTTestsBase
@property (nonatomic, strong) OTBottledPeerRecord*  fakeBottledPeerRecord;
@end

@implementation UnitTestOTContext

- (void)setUp
{
    [super setUp];
    self.continueAfterFailure = NO;
}

- (void)tearDown
{
    self.zones = nil;
    self.operationQueue = nil;
    [super tearDown];
}

-(void) testEnroll
{
    NSError* error = nil;

    NSString* escrowRecordID = [self currentIdentity:&error].spID;
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(escrowRecordID, @"escrowRecordID should not be nil: %@", error);

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
    [self startCKKSSubsystem];

    OTPreflightInfo* info = nil;
    XCTAssertNotNil(info = [self.context preflightBottledPeer:testContextID entropy:self.secret error:&error], @"preflight sould return info:%@", error);
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(info, @"preflight info should not be nil: %@", error);
    XCTAssertNotNil(info.bottleID, @"escrowRecordID should not be nil: %@", error);
    XCTAssertNotNil(info.escrowedSigningSPKI, @"signingPubKey should be nil: %@", error);
    
    OTBottledPeerRecord* bprecord = [self.localStore readLocalBottledPeerRecordWithRecordID:info.bottleID error:&error];
    XCTAssertNotNil(bprecord, @"bprecord should not be nil: %@", error);
    
    XCTAssertTrue([self.context.cloudStore uploadBottledPeerRecord:bprecord escrowRecordID:escrowRecordID error:&error], @"launch should succeed");
    XCTAssertNil(error, @"error should be nil: %@", error);
    [self releaseCloudKitFetchHold];

    [self expectCKFetch];
    XCTAssertEqual( [[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count], (unsigned long)1, @"should have 1 record");
}

-(void) testEnrollAndRestore
{
    NSError* error = nil;
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    NSString* escrowRecordID = [self currentIdentity:&error].spID;
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(escrowRecordID, @"escrowRecordID should not be nil: %@", error);

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];
   
    [self startCKKSSubsystem];

    OTPreflightInfo* info = nil;
    XCTAssertNotNil(info = [self.context preflightBottledPeer:testContextID entropy:self.secret error:&error], @"preflight sould return info");
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(info, @"preflight info should not be nil: %@", error);
    XCTAssertNotNil(info.bottleID, @"escrowRecordID should not be nil: %@", error);
    XCTAssertNotNil(info.escrowedSigningSPKI, @"signingPubKey should be nil: %@", error);

    OTBottledPeerRecord* bprecord = [self.localStore readLocalBottledPeerRecordWithRecordID:info.bottleID error:&error];
    XCTAssertNotNil(bprecord, @"bprecord should not be nil: %@", error);
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];
    XCTAssertTrue([self.cloudStore uploadBottledPeerRecord:bprecord escrowRecordID:bprecord.escrowRecordID error:&error], @"should create bottled peer record");
    XCTAssertNil(error, "error should be nil");
    [self waitForCKModifications];

    [self releaseCloudKitFetchHold];

    OTBottledPeerSigned* bp = [self.context restoreFromEscrowRecordID:escrowRecordID secret:self.secret error:&error];
    [self waitForCKModifications];
    
    XCTAssertTrue( [[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count] == 1, @"should have 1 record");
    [self waitForCKModifications];

    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(bp, @"signed bottled peer should not be nil: %@", error);
    XCTAssertTrue([bp.bp.peerEncryptionKey isEqual:self.peerEncryptionKey], @"enrolled and restored peer encryption keys should match");
    XCTAssertTrue([bp.bp.peerSigningKey isEqual:self.peerSigningKey], @"enrolled and restored peer signing keys should match");
}

-(void)testEnrollAndRestoreFromCloudKit
{
    NSError* error = nil;
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
    [self startCKKSSubsystem];

    OTPreflightInfo* info = nil;
    XCTAssertNotNil(info = [self.context preflightBottledPeer:testContextID entropy:self.secret error:&error], @"preflight sould return info");
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(info, @"preflight info should not be nil: %@", error);
    XCTAssertNotNil(info.bottleID, @"bottleID should not be nil: %@", error);
    XCTAssertNotNil(info.escrowedSigningSPKI, @"signingPubKey should be nil: %@", error);

    OTBottledPeerRecord* bprecord = [self.localStore readLocalBottledPeerRecordWithRecordID:info.bottleID error:&error];
    XCTAssertNotNil(bprecord, @"bprecord should not be nil: %@", error);

    XCTAssertTrue([self.context.cloudStore uploadBottledPeerRecord:bprecord escrowRecordID:info.bottleID error:&error], @"launch should succeed");
    XCTAssertNil(error, @"error should be nil: %@", error);

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];
    
    XCTAssertTrue([[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count] > 0, @"should have multiple records");
    OTIdentity *identity = [self currentIdentity:&error];
   
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(self.escrowKeys, @"escrow keys should not be nil: %@", error);

    NSString* recordName = [OTBottledPeerRecord constructRecordID:identity.spID escrowSigningSPKI:[self.escrowKeys.signingKey.publicKey encodeSubjectPublicKeyInfo]];
    
    OTBottledPeerRecord *rec = [self.localStore readLocalBottledPeerRecordWithRecordID:recordName error:&error];
    
    XCTAssertNotNil(rec.signatureUsingEscrowKey, @"signatureUsingEscrow should not be nil: %@", error);
    
    XCTAssertNotNil(rec.signatureUsingPeerKey, @"signatureUsingPeerKey should not be nil: %@", error);
    
    XCTAssertNotNil(rec.bottle, @"bottle should not be nil: %@", error);


    OTBottledPeerSigned *bps = [[OTBottledPeerSigned alloc] initWithBottledPeerRecord:rec
                                                                           escrowKeys:self.escrowKeys
                                                                                error:&error];
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(bps, @"signed bottled peer should not be nil: %@", error);
    XCTAssertTrue([bps.bp.peerEncryptionKey isEqual:self.peerEncryptionKey], @"enrolled and restored peer encryption keys should match");
    XCTAssertTrue([bps.bp.peerSigningKey isEqual:self.peerSigningKey], @"enrolled and restored peer signing keys should match");
}

-(void) testScrubbing
{
    NSError* error = nil;

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
    [self startCKKSSubsystem];
    
    OTPreflightInfo* info = nil;
    XCTAssertNotNil(info = [self.context preflightBottledPeer:testContextID entropy:self.secret error:&error], @"preflight sould return info");
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(info, @"preflight info should not be nil: %@", error);
    XCTAssertNotNil(info.bottleID, @"escrowRecordID should not be nil: %@", error);
    XCTAssertNotNil(info.escrowedSigningSPKI, @"signingPubKey should be nil: %@", error);

    XCTAssertTrue([self.context scrubBottledPeer:testContextID bottleID:info.bottleID error:&error], @"scrubbing bottled peer should succeed");
    XCTAssertNil(error, @"error should be nil: %@", error);
    NSArray* list = [self.context.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error];
    XCTAssertTrue([list count] == 0, @"there should be 0 records in localstore");
}

-(void) testGettingListOfRecordIDS
{
    NSError* error = nil;

    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
    [self startCKKSSubsystem];
    
    OTPreflightInfo* info = nil;
    XCTAssertNotNil(info = [self.context preflightBottledPeer:testContextID entropy:self.secret error:&error], @"preflight sould return info");
    XCTAssertNil(error, @"error should be nil: %@", error);
    XCTAssertNotNil(info, @"preflight info should not be nil: %@", error);
    XCTAssertNotNil(info.bottleID, @"bottleID should not be nil: %@", error);
    XCTAssertNotNil(info.escrowedSigningSPKI, @"signingPubKey should be nil: %@", error);

    OTBottledPeerRecord* bprecord = [self.localStore readLocalBottledPeerRecordWithRecordID:info.bottleID error:&error];
    XCTAssertNotNil(bprecord, @"bprecord should not be nil: %@", error);
    
    XCTAssertTrue([self.context.cloudStore uploadBottledPeerRecord:bprecord escrowRecordID:info.bottleID error:&error], @"launch should succeed");
    XCTAssertNil(error, @"error should be nil: %@", error);

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];
    
    NSArray* list = [self.context.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error];
    XCTAssertNotNil(list, @"list should not be nil");
    XCTAssertTrue([list count] > 0, @"list of escrow record ids should not be empty");
}

- (nullable OTIdentity *)currentIdentity:(NSError**)error {

    return [[OTIdentity alloc]initWithPeerID:@"ego peer id" spID:@"sos peer id" peerSigningKey:self.peerSigningKey peerEncryptionkey:self.peerEncryptionKey error:error];
}

@end
#endif

