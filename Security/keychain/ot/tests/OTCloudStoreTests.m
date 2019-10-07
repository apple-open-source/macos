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

static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";
static NSString* OTCKRecordEscrowRecordID = @"escrowRecordID";

@interface OTCloudStoreUnitTests : OTTestsBase
@property (nonatomic, strong) OTBottledPeerRecord* fakeBottledPeerRecord;
@end

@implementation OTCloudStoreUnitTests


- (void)setUp {    
    [super setUp];
    self.continueAfterFailure = NO;
    self.fakeBottledPeerRecord = [[OTBottledPeerRecord alloc] init];
    self.fakeBottledPeerRecord.bottle = [@"bottled peer data" dataUsingEncoding:NSUTF8StringEncoding];
    self.fakeBottledPeerRecord.signatureUsingEscrowKey = [@"bottled peer escrow sig" dataUsingEncoding:NSUTF8StringEncoding];
    self.fakeBottledPeerRecord.signatureUsingPeerKey = [@"bottled peer peer sig" dataUsingEncoding:NSUTF8StringEncoding];
    self.fakeBottledPeerRecord.peerID = @"peer id";
    self.fakeBottledPeerRecord.spID = @"sos peer id";
    self.fakeBottledPeerRecord.escrowRecordID = @"escrowRecordID";
    self.fakeBottledPeerRecord.escrowedSigningSPKI = [@"escrowedSigningSPKI" dataUsingEncoding:NSUTF8StringEncoding];
    self.fakeBottledPeerRecord.peerSigningSPKI = [@"peerSigningSPKI" dataUsingEncoding:NSUTF8StringEncoding];
}

- (void)tearDown {
    self.zones = nil;
    self.operationQueue = nil;

    [super tearDown];
}

- (void)testWriteSameBottledPeerTwiceToFakeRecord {
    NSError* error = nil;
    
    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionaryWithObjectsAndKeys:[[NSNumber alloc] initWithInt:1], OTCKRecordBottledPeerType, nil];

    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
    [self startCKKSSubsystem];
    XCTAssertTrue([self.cloudStore uploadBottledPeerRecord:self.fakeBottledPeerRecord escrowRecordID:self.fakeBottledPeerRecord.escrowRecordID error:&error], @"should create bottled peer record");
    XCTAssertNil(error, "error should be nil");

    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];

    XCTAssertTrue([self.cloudStore uploadBottledPeerRecord:self.fakeBottledPeerRecord escrowRecordID:self.fakeBottledPeerRecord.escrowRecordID error:&error], @"should create bottled peer record");
    XCTAssertNil(error, "error should be nil");
    
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];
}

- (void)testWriteBottledPeerToFakeRecord {
    NSError* error = nil;
    
    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionary];
    recordDictionary[OTCKRecordBottledPeerType] = [[NSNumber alloc] initWithInt:1];
    
    [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
    [self startCKKSSubsystem];

    XCTAssertTrue([self.cloudStore uploadBottledPeerRecord:self.fakeBottledPeerRecord escrowRecordID:self.fakeBottledPeerRecord.escrowRecordID error:&error], @"should create bottled peer record");
    XCTAssertNil(error, "error should be nil");
    
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self releaseCloudKitFetchHold];
}

- (void)testWriteMultipleBottledPeersToSAMEFakeRecord {
    NSError* error = nil;
    
    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionary];
    
    recordDictionary[OTCKRecordBottledPeerType] = [[NSNumber alloc] initWithInt:1];

    [self startCKKSSubsystem];

    for(int i = 0; i < 10; i++){
        [self expectAddedCKModifyRecords:recordDictionary holdFetch:NO];

        XCTAssertTrue([self.cloudStore uploadBottledPeerRecord:self.fakeBottledPeerRecord escrowRecordID:self.fakeBottledPeerRecord.escrowRecordID error:&error], @"should create bottled peer record");

        [self waitForCKModifications];

        XCTAssertNil(error, "error should be nil");
        OCMVerifyAllWithDelay(self.mockDatabase, 8);
        [self releaseCloudKitFetchHold];
    }
}

- (void)testWriteBottledPeersToDifferentFakeRecord {
    NSError* error = nil;
    
    NSMutableDictionary* recordDictionary = [NSMutableDictionary dictionary];
    
    recordDictionary[OTCKRecordBottledPeerType] = [[NSNumber alloc] initWithInt:1];
    
    [self startCKKSSubsystem];
    
    for(int i = 0; i < 10; i++){
        [self expectAddedCKModifyRecords:recordDictionary holdFetch:YES];
        NSString *escrowID = [NSString stringWithFormat:@"bp-sospeer%d-hash", i];
        self.fakeBottledPeerRecord.escrowRecordID = escrowID;
        XCTAssertTrue([self.cloudStore uploadBottledPeerRecord:self.fakeBottledPeerRecord escrowRecordID:escrowID error:&error], @"should create bottled peer record");
        [self waitForCKModifications];
        
        XCTAssertNil(error, "error should be nil");
        OCMVerifyAllWithDelay(self.mockDatabase, 8);
        [self releaseCloudKitFetchHold];
    }
    XCTAssertTrue( [[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count] == 10, @"should have 1 record");
}


- (void)testReadBottledPeerRecordFromCloudKit {
    NSError *error = nil;
    [self startCKKSSubsystem];
    
    CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType];
    newRecord[OTCKRecordEscrowRecordID] = @"escrowRecordID";
    [self.otFakeZone addToZone:newRecord];

    [self.cloudStore notifyZoneChange:nil];

    [self waitForCKModifications];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    XCTAssertTrue( [[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count] > 0, @"should have 1 record");
}

-(void) testOTCloudStoreDownloadBP{
    NSError* error = nil;
    [self startCKKSSubsystem];

    CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType];
    newRecord[OTCKRecordEscrowRecordID] = @"escrowRecordID";
    [self.otFakeZone addToZone:newRecord];
    
    XCTAssertTrue([self.cloudStore downloadBottledPeerRecord:&error] == YES, @"downloading records should succeed:%@", error);
    XCTAssertNil(error, @"error should be nil");
    
    [self waitForCKModifications];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    XCTAssertNil(error, "error should be nil");
    XCTAssertEqual([[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count], (unsigned long)1, @"should have 1 record");
    XCTAssertNil(error, "error should be nil");
}

-(void) testOTCloudStoreDownloadMultipleBP{
    NSError* error = nil;
    [self startCKKSSubsystem];
    
    for(int i = 0; i < 10; i++){
        CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType zoneID:self.otZoneID];
        newRecord[OTCKRecordEscrowRecordID] = [NSString stringWithFormat:@"escrowRecordID%d", i];
        [self.otFakeZone addToZone:newRecord];
    }
    [self waitForCKModifications];

    XCTAssertTrue([self.cloudStore downloadBottledPeerRecord:&error] == YES, @"downloading records should succeed:%@", error);
    XCTAssertNil(error, @"error should be nil");
    [self waitForCKModifications];
    
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    
    XCTAssertNil(error, "error should be nil");
    XCTAssertEqual( [[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count], (unsigned long)10, @"should have 1 record");
}

-(void) testOTCloudStoreUploadMultipleToSameRecord{
    NSError* error = nil;
    [self startCKKSSubsystem];
    CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType zoneID:self.otZoneID];
    newRecord[OTCKRecordEscrowRecordID] = @"escrowRecordID";
    for(int i = 0; i < 10; i++){
        [self.otFakeZone addToZone:newRecord];
    }
    [self waitForCKModifications];
    
    XCTAssertTrue([self.cloudStore downloadBottledPeerRecord:&error] == YES, @"downloading records should succeed:%@", error);
    XCTAssertNil(error, @"error should be nil");
    [self waitForCKModifications];
    
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    
    XCTAssertNil(error, "error should be nil");
    XCTAssertEqual([[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count], (unsigned long)1, @"should have 1 record");
}

-(void) testRemoveRecordIDs{

    [self startCKKSSubsystem];
    NSError *error = nil;
    CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType zoneID:self.otZoneID];
    newRecord[OTCKRecordEscrowRecordID] = @"escrowRecordID";
    [self expectCKFetch];

    [self.otFakeZone addToZone:newRecord];
    [self waitForCKModifications];

    [self.cloudStore notifyZoneChange:nil];
    [self waitForCKModifications];

    XCTAssertTrue( [[self.cloudStore retrieveListOfEligibleEscrowRecordIDs:&error] count] == 1, @"should have 1 record");

    [self expectCKFetch];
    XCTAssertTrue([self.cloudStore downloadBottledPeerRecord:&error] == YES, @"downloading records should succeed:%@", error);
    XCTAssertNil(error, @"error should be nil");
    [self waitForCKModifications];
}

-(void) testFetchTimeout
{
    [self startCKKSSubsystem];
    
    NSError* error = nil;
    CKRecord* newRecord = [[CKRecord alloc]initWithRecordType:OTCKRecordBottledPeerType zoneID:self.otZoneID];
    newRecord[OTCKRecordEscrowRecordID] = @"escrowRecordID";

    [self holdCloudKitFetches];

    [self.cloudStore downloadBottledPeerRecord:&error];

    XCTAssertNotNil(error, "error should not be nil");
    XCTAssertTrue([(NSString*)error.userInfo[@"NSLocalizedDescription"] isEqualToString:@"Operation(CKKSResultOperation(cloudkit-fetch-and-process-changes)) timed out waiting to start for [<CKKSResultOperation(fetch-and-process-updates-watcher): ready>]"], "expecting timed out error");
}

-(void) testModifyRecordsTimeout
{
    NSError* error = nil;

    [self expectAddedCKModifyRecords:@{OTCKRecordBottledPeerType: @1} holdFetch:NO];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self putSelfTLKSharesInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:4*NSEC_PER_SEC], @"Key state should have arrived at ready");

    [self holdCloudKitModifications];

    [self.cloudStore uploadBottledPeerRecord:self.fakeBottledPeerRecord
                              escrowRecordID:self.fakeBottledPeerRecord.escrowRecordID error:&error];

    XCTAssertNotNil(error, "error should not be nil");
    XCTAssertTrue([(NSString*)error.userInfo[@"NSLocalizedDescription"] isEqualToString:@"Operation(CKKSResultOperation(cloudkit-modify-changes)) timed out waiting to start for [<CKKSResultOperation(modify-records-watcher): ready>]"], "expecting timed out error");

    [self expectAddedCKModifyRecords:@{OTCKRecordBottledPeerType: @1} holdFetch:NO];

    [self releaseCloudKitModificationHold];
    [self waitForCKModifications];
}

@end

#endif /* OCTAGON */

