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

#import "OTTestsBase.h"

/* Octagon Trust Local Context Record Constants  */
static NSString* OTCKRecordContextID = @"contextID";
static NSString* OTCKRecordDSID = @"accountDSID";
static NSString* OTCKRecordContextName = @"contextName";
static NSString* OTCKRecordZoneCreated = @"zoneCreated";
static NSString* OTCKRecordSubscribedToChanges = @"subscribedToChanges";
static NSString* OTCKRecordChangeToken = @"changeToken";
static NSString* OTCKRecordEgoPeerID = @"egoPeerID";
static NSString* OTCKRecordEgoPeerCreationDate = @"egoPeerCreationDate";
static NSString* OTCKRecordRecoverySigningSPKI = @"recoverySigningSPKI";
static NSString* OTCKRecordRecoveryEncryptionSPKI = @"recoveryEncryptionSPKI";
static NSString* OTCKRecordBottledPeerTableEntry = @"bottledPeer";

/* Octagon Trust Local Peer Record  */
static NSString* OTCKRecordPeerID = @"peerID";
static NSString* OTCKRecordPermanentInfo = @"permanentInfo";
static NSString* OTCKRecordStableInfo = @"stableInfo";
static NSString* OTCKRecordDynamicInfo = @"dynamicInfo";
static NSString* OTCKRecordRecoveryVoucher = @"recoveryVoucher";
static NSString* OTCKRecordIsEgoPeer = @"isEgoPeer";

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

static NSString* const testDSID = @"123456789";

@interface UnitTestOTLocalStore : OTTestsBase
@end

@implementation UnitTestOTLocalStore

- (void)setUp
{
    [super setUp];
   
    self.continueAfterFailure = NO;
}

- (void)tearDown
{
    [super tearDown];
}

-(void)testDBConnection
{
    NSError* error = nil;
    
    XCTAssertTrue([self.localStore closeDBWithError:&error], @"failed attempt at closing the db");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertTrue([self.localStore openDBWithError:&error], @"could not open db");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertTrue([self.localStore closeDBWithError:&error], @"failed attempt at closing the db");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertTrue([self.localStore openDBWithError:&error], @"could not open db");
    XCTAssertNil(error, @"error should be nil:%@", error);
}

-(void) testDBLocalContextRetrieval
{
    NSString* contextAndDSID = [NSString stringWithFormat:@"testContextRetreival-%@", testDSID];
    _SFECKeyPair *recoverySigningPublicKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    _SFECKeyPair *recoveryEncryptionPublicKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];

    NSError* error = nil;
    NSDictionary *attributes = @{
                                 OTCKRecordContextID : @"testContextRetreival",
                                 OTCKRecordDSID : testDSID,
                                 OTCKRecordContextName : @"newFoo",
                                 OTCKRecordZoneCreated : @(NO),
                                 OTCKRecordSubscribedToChanges : @(NO),
                                 OTCKRecordChangeToken : [NSData data],
                                 OTCKRecordEgoPeerID : @"OctagonPeerID",
                                 OTCKRecordEgoPeerCreationDate : [NSDate date],
                                 OTCKRecordRecoverySigningSPKI : [[recoverySigningPublicKey publicKey] keyData],
                                 OTCKRecordRecoveryEncryptionSPKI :[[recoveryEncryptionPublicKey publicKey] keyData]};

    XCTAssertTrue([self.localStore insertLocalContextRecord:attributes error:&error], @"inserting new context failed");
    XCTAssertNil(error, @"error should be nil:%@", error);

    OTContextRecord* record = [self.localStore readLocalContextRecordForContextIDAndDSID:contextAndDSID error:&error];
    XCTAssertNotNil(record, @"fetching attributes returned nil");
    XCTAssertNotNil(record.contextID, @"fetching attributes returned nil");
    XCTAssertNotNil(record.contextName, @"fetching attributes returned nil");
    XCTAssertNotNil(record.dsid, @"fetching attributes returned nil");
    XCTAssertNotNil(record.egoPeerCreationDate, @"fetching attributes returned nil");
    XCTAssertNotNil(record.egoPeerID, @"fetching attributes returned nil");
    XCTAssertNotNil(record.recoveryEncryptionSPKI, @"fetching attributes returned nil");
    XCTAssertNotNil(record.recoverySigningSPKI, @"fetching attributes returned nil");

    XCTAssertNil(error, @"failed to read local context for test local store");
    
    OTContextRecord* recordToTestEquality = [[OTContextRecord alloc]init];
    recordToTestEquality.contextName = @"newFoo";
    recordToTestEquality.contextID = @"testContextRetreival";
    recordToTestEquality.dsid = testDSID;
    recordToTestEquality.contextName = @"newFoo";
    recordToTestEquality.egoPeerID = @"OctagonPeerID";
    recordToTestEquality.recoveryEncryptionSPKI = [[recoveryEncryptionPublicKey publicKey] keyData];
    recordToTestEquality.recoverySigningSPKI = [[recoverySigningPublicKey publicKey] keyData];
    
    OTContextRecord* recordFromDB = [self.localStore readLocalContextRecordForContextIDAndDSID:contextAndDSID error:&error];
    XCTAssertTrue([recordFromDB isEqual:recordToTestEquality], @"OTContext should be equal");
}

-(void) testDBMultipleContexts
{
    NSError* error = nil;
    NSString* newFooContextAndDSID = [NSString stringWithFormat:@"newFoo-%@", testDSID];

    _SFECKeyPair *recoverySigningPublicKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    _SFECKeyPair *recoveryEncryptionPublicKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    NSDictionary *attributes = @{
                                 OTCKRecordContextID : @"newFoo",
                                 OTCKRecordContextName : @"newFoo",
                                 OTCKRecordDSID : testDSID,
                                 OTCKRecordZoneCreated : @(NO),
                                 OTCKRecordSubscribedToChanges : @(NO),
                                 OTCKRecordChangeToken : [NSData data],
                                 OTCKRecordEgoPeerID : @"OctagonPeerID",
                                 OTCKRecordEgoPeerCreationDate : [NSDate date],
                                 OTCKRecordRecoverySigningSPKI : [[recoverySigningPublicKey publicKey] keyData],    // FIXME not SPKI
                                 OTCKRecordRecoveryEncryptionSPKI : [[recoveryEncryptionPublicKey publicKey] keyData]};
    
    
    XCTAssertTrue([self.localStore insertLocalContextRecord:attributes error:&error], @"inserting new context failed");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    NSString* foo2ContextAndDSID = [NSString stringWithFormat:@"Foo2-%@", testDSID];
    attributes = @{
                   OTCKRecordContextID : @"Foo2",
                   OTCKRecordContextName : @"Foo2",
                   OTCKRecordDSID : testDSID,
                   OTCKRecordZoneCreated : @(NO),
                   OTCKRecordSubscribedToChanges : @(NO),
                   OTCKRecordChangeToken : [NSData data],
                   OTCKRecordEgoPeerID : @"OctagonPeerID2",
                   OTCKRecordEgoPeerCreationDate : [NSDate date],
                   OTCKRecordRecoverySigningSPKI : [[recoverySigningPublicKey publicKey] keyData],  // FIXME not SPKI
                   OTCKRecordRecoveryEncryptionSPKI :[[recoveryEncryptionPublicKey publicKey] keyData]};
    
    XCTAssertTrue([self.localStore insertLocalContextRecord:attributes error:&error], @"inserting new context failed");
    XCTAssertNil(error, @"error should be nil:%@", error);

    OTContextRecord* recordNewFoo = [self.localStore readLocalContextRecordForContextIDAndDSID:newFooContextAndDSID error:&error];

    XCTAssertNotNil(recordNewFoo, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.contextID, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.contextName, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.dsid, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.egoPeerCreationDate, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.egoPeerID, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.recoveryEncryptionSPKI, @"fetching attributes returned nil");
    XCTAssertNotNil(recordNewFoo.recoverySigningSPKI, @"fetching attributes returned nil");

    XCTAssertNil(error, @"failed to read local context for test local store");

    OTContextRecord* recordFoo2 = [self.localStore readLocalContextRecordForContextIDAndDSID:foo2ContextAndDSID error:&error];

    XCTAssertNotNil(recordFoo2, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.contextID, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.contextName, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.dsid, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.egoPeerCreationDate, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.egoPeerID, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.recoveryEncryptionSPKI, @"fetching attributes returned nil");
    XCTAssertNotNil(recordFoo2.recoverySigningSPKI, @"fetching attributes returned nil");
    XCTAssertNil(error, @"failed to read local context for test local store");

}

-(void) testRowUpdates
{
    NSError* error = nil;
    NSString* escrowRecordID = @"escrow record 1";
    NSString* escrowRecordID2 = @"escrow record 2";
    NSString* escrowRecordID3 = @"escrow record 3";

    OTBottledPeerRecord* record = [[OTBottledPeerRecord alloc]init];
    OTBottledPeerRecord* record2 = [[OTBottledPeerRecord alloc]init];
    OTBottledPeerRecord* record3 = [[OTBottledPeerRecord alloc]init];

    record.escrowRecordID = escrowRecordID;
    record2.escrowRecordID = escrowRecordID2;
    record3.escrowRecordID = escrowRecordID3;
    
    record.escrowedSigningSPKI = [@"escrowedSigingSPKI" dataUsingEncoding:NSUTF8StringEncoding];
    record2.escrowedSigningSPKI = [@"escrowedSigingSPI" dataUsingEncoding:NSUTF8StringEncoding];
    record3.escrowedSigningSPKI = [@"escrowedSigingSPKI" dataUsingEncoding:NSUTF8StringEncoding];

    XCTAssertTrue([self.localStore insertBottledPeerRecord:record escrowRecordID:escrowRecordID error:&error]);
    XCTAssertNil(error, @"error should be nil:%@", error);

    XCTAssertTrue([self.localStore insertBottledPeerRecord:record2 escrowRecordID:escrowRecordID2 error:&error]);
    XCTAssertNil(error, @"error should be nil:%@", error);

    XCTAssertTrue([self.localStore insertBottledPeerRecord:record3 escrowRecordID:escrowRecordID3 error:&error]);
    XCTAssertNil(error, @"error should be nil:%@", error);


    OTBottledPeerRecord *bp = [self.localStore readLocalBottledPeerRecordWithRecordID:record.recordName error:&error];
    XCTAssertNotNil(bp);
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    OTBottledPeerRecord *bp2 = [self.localStore readLocalBottledPeerRecordWithRecordID:record2.recordName error:&error];
    XCTAssertNotNil(bp2);
    XCTAssertNil(error, @"error should be nil:%@", error);
  
    OTBottledPeerRecord *bp3 = [self.localStore readLocalBottledPeerRecordWithRecordID:record3.recordName error:&error];
    XCTAssertNotNil(bp3);
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertTrue([self.localStore updateLocalContextRecordRowWithContextID:self.localStore.contextID columnName:OTCKRecordContextName newValue:(void*)@"SuperSuperFoo" error:&error], @"could not update column:%@ with value:%@", OTCKRecordContextName, @"SuperSuperFoo");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertTrue([self.localStore updateLocalContextRecordRowWithContextID:self.localStore.contextID columnName:OTCKRecordEgoPeerID newValue:(void*)@"NewPeerID" error:&error], @"could not update column:%@ with value:%@", OTCKRecordEgoPeerID, @"NewPeerID");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertTrue([self.localStore updateLocalContextRecordRowWithContextID:self.localStore.contextID columnName:OTCKRecordRecoverySigningSPKI newValue:(void*)[[NSData alloc]initWithBase64EncodedString:@"I'm a string" options:NSDataBase64DecodingIgnoreUnknownCharacters] error:&error], @"could not update column:%@ with value:%@", OTCKRecordContextName, @"NewPeerID");
    XCTAssertNil(error, @"error should be nil:%@", error);
    
    XCTAssertFalse([self.localStore updateLocalContextRecordRowWithContextID:self.localStore.contextID columnName:@"ColumnName" newValue:(void*)@"value" error:&error], @"could not update column:%@ with value:%@", @"ColumnName", @"value");
    XCTAssertNotNil(error, @"error should not be nil: %@", error);
}

@end

#endif /* OCTAGON */
