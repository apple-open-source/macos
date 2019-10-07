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
#import "keychain/ot/OTSOSAdapter.h"

static NSString* const testContextID = @"Foo";
static NSString* const testContextForAcceptor = @"Acceptor";

static NSString* const testDSID = @"123456789";

static int _test_num = 0;
static NSString* _path;
static NSString* _dbPath;

static NSString* OTCKZoneName = @"OctagonTrust";

static NSString* const kOTRampZoneName = @"metadata_zone";
static NSString* const kOTRampForEnrollmentRecordName = @"metadata_rampstate_enroll";
static NSString* const kOTRampForRestoreRecordName = @"metadata_rampstate_restore";
static NSString* const kOTRampForCFURecordName = @"metadata_rampstate_cfu";

static NSString* kFeatureAllowedKey =       @"FeatureAllowed";
static NSString* kFeaturePromotedKey =      @"FeaturePromoted";
static NSString* kFeatureVisibleKey =       @"FeatureVisible";
static NSString* kRetryAfterKey =           @"RetryAfter";
static NSString* kRampPriorityKey =         @"RampPriority";

static NSString* OTCKRecordBottledPeerType = @"OTBottledPeer";

@implementation OTTestsBase

// Override our base class
-(NSSet*)managedViewList {
    return [NSSet setWithObject:@"keychain"];
}

+ (void)setUp {
    SecCKKSEnable();
    SecCKKSResetSyncing();
    [super setUp];
}

- (void)setUp
{
    [super setUp];
    
    self.continueAfterFailure = NO;
    NSError* error = nil;

    _path = @"/tmp/ottrusttests";
    _dbPath = [_path stringByAppendingFormat:@"/ottest.db.%d",_test_num++];
    
    XCTAssertTrue([[NSFileManager defaultManager] createDirectoryAtPath:_path withIntermediateDirectories:YES attributes:nil error:nil], @"directory created!");
    self.localStore = [[OTLocalStore alloc]initWithContextID:testContextID dsid:testDSID path:_dbPath error:&error];
    XCTAssertNil(error, "error should be nil");

    self.cloudStore = [[OTCloudStore alloc] initWithContainer:self.mockContainer
                                                     zoneName:OTCKZoneName
                                               accountTracker:self.mockAccountStateTracker
                                          reachabilityTracker:self.mockReachabilityTracker
                                                   localStore:self.localStore
                                                    contextID:testContextID
                                                         dsid:testDSID
                         fetchRecordZoneChangesOperationClass:self.mockFakeCKFetchRecordZoneChangesOperation
                                   fetchRecordsOperationClass:self.mockFakeCKFetchRecordZoneChangesOperation
                                          queryOperationClass:self.mockFakeCKQueryOperation
                            modifySubscriptionsOperationClass:self.mockFakeCKModifySubscriptionsOperation
                              modifyRecordZonesOperationClass:self.mockFakeCKFetchRecordsOperation
                                           apsConnectionClass:self.mockFakeCKModifySubscriptionsOperation
                                               operationQueue:nil];
  
    NSString* secretString = @"I'm a secretI'm a secretI'm a secretI'm a secretI'm a secretI'm a secret";
    self.secret = [[NSData alloc]initWithBytes:[secretString UTF8String] length:[secretString length]];
    
    self.context = [[OTContext alloc]initWithContextID:testContextID dsid:testDSID localStore:self.localStore cloudStore:self.cloudStore identityProvider:self error:&error];
    XCTAssertNil(error, "error should be nil");

    self.sosPeerID = @"spID";
    self.egoPeerID = @"egoPeerID";
    self.peerSigningKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    self.peerEncryptionKey = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    self.escrowKeys = [[OTEscrowKeys alloc]initWithSecret:self.secret dsid:testDSID error:&error];

    XCTAssertNotNil(self.context, @"context not initialized");
    
    self.otZoneID = [[CKRecordZoneID alloc] initWithZoneName:OTCKZoneName ownerName:CKCurrentUserDefaultName];
    
    XCTAssertNotNil(self.otZoneID, @"cloudkit record zone id is not initialized");
    
    self.otFakeZone = [[FakeCKZone alloc] initZone: self.otZoneID];
    XCTAssertNotNil(self.otFakeZone, @"fake ot zone is not initialized");
    
    self.zones[self.otZoneID] = self.otFakeZone;
    XCTAssertNotNil(self.zones, @"ot zones set is not initialized");

    self.rampZoneID = [[CKRecordZoneID alloc] initWithZoneName:kOTRampZoneName ownerName:CKCurrentUserDefaultName];
    self.rampZone = [[FakeCKZone alloc]initZone:self.rampZoneID];
    self.zones[self.rampZoneID] = self.rampZone;

    self.cfu = [self fakeRamp:kOTRampForCFURecordName
                  featureName:@"FAKE-cfu"
                accountTracker:self.accountStateTracker
            lockStateStracker:self.lockStateTracker
          reachabilityTracker:self.reachabilityTracker];
    self.enroll = [self fakeRamp:kOTRampForEnrollmentRecordName
                     featureName:@"FAKE-enroll"
                  accountTracker:self.accountStateTracker
               lockStateStracker:self.lockStateTracker
             reachabilityTracker:self.reachabilityTracker];
    self.restore = [self fakeRamp:kOTRampForRestoreRecordName
                      featureName:@"FAKE-restore"
                   accountTracker:self.accountStateTracker
                lockStateStracker:self.lockStateTracker
              reachabilityTracker:self.reachabilityTracker];

    self.scheduler = [[CKKSNearFutureScheduler alloc] initWithName: @"test" delay:50*NSEC_PER_MSEC keepProcessAlive:true
                                         dependencyDescriptionCode:CKKSResultDescriptionNone
                                                             block:^{
                                                                 [self.expectation fulfill];
                                                             }];
    self.manager = [[OTManager alloc] initWithContext:self.context
                                           localStore:self.localStore
                                               enroll:self.enroll
                                              restore:self.restore
                                                  cfu:self.cfu
                                         cfuScheduler:self.scheduler
                                           sosAdapter:[[OTSOSActualAdapter alloc] init]
                                       authKitAdapter:[[OTAuthKitActualAdapter alloc] init]
                                   apsConnectionClass:[FakeAPSConnection class]];
    [OTManager resetManager:true to:self.manager];

    self.cuttlefishContext = [self.manager contextForContainerName:OTCKContainerName
                                                         contextID:OTDefaultContext];
    
    id mockConnection = OCMPartialMock([[NSXPCConnection alloc] init]);
    OCMStub([mockConnection remoteObjectProxyWithErrorHandler:[OCMArg any]]).andCall(self, @selector(manager));
    self.otControl = [[OTControl alloc] initWithConnection:mockConnection sync:true];
    XCTAssertNotNil(self.otControl, "Should have received control object");

    [self.reachabilityTracker setNetworkReachability:true];
    [self.context.reachabilityTracker recheck];
    [self.cfu.reachabilityTracker recheck];
    [self.enroll.reachabilityTracker recheck];
    [self.restore.reachabilityTracker recheck];
}


- (void)tearDown
{
    NSError *error = nil;

    [_localStore removeAllBottledPeerRecords:&error];
    [_localStore deleteAllContexts:&error];
    
    _context = nil;
    _cloudStore = nil;
    _localStore = nil;
    _escrowKeys = nil;
    _peerSigningKey = nil;
    _peerEncryptionKey = nil;
    _otFakeZone = nil;
    _otZoneID = nil;

    _rampZone = nil;
    _rampZoneID = nil;
    _cfuRampRecord = nil;
    _enrollRampRecord = nil;
    _restoreRampRecord = nil;
    _scheduler = nil;

    [super tearDown];
}

- (OTRamp*)fakeRamp:(NSString*)recordName
        featureName:(NSString*)featureName
     accountTracker:(CKKSAccountStateTracker*)accountTracker
  lockStateStracker:(CKKSLockStateTracker*)lockStateTracker
reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
{

    OTRamp* ramp = [[OTRamp alloc]initWithRecordName:recordName
                                         featureName:featureName
                                           container:self.mockContainer
                                            database:self.mockDatabase
                                              zoneID:self.rampZoneID
                                      accountTracker:accountTracker
                                    lockStateTracker:lockStateTracker
                                 reachabilityTracker:reachabilityTracker
                    fetchRecordRecordsOperationClass:self.mockFakeCKFetchRecordsOperation];

    return ramp;
}

-(void) setUpRampRecordsInCloudKitWithFeatureOff
{
    CKRecordID* enrollRecordID = [[CKRecordID alloc] initWithRecordName:kOTRampForEnrollmentRecordName zoneID:self.rampZoneID];
    self.enrollRampRecord = [[CKRecord alloc] initWithRecordType:kOTRampForEnrollmentRecordName recordID:enrollRecordID];
    self.enrollRampRecord[kFeatureAllowedKey] = @NO;
    self.enrollRampRecord[kFeaturePromotedKey] = @NO;   //always false right now
    self.enrollRampRecord[kFeatureVisibleKey] = @NO;
    self.enrollRampRecord[kRetryAfterKey] = [[NSNumber alloc]initWithInt:3600];

    CKRecordID* restoreRecordID = [[CKRecordID alloc] initWithRecordName:kOTRampForRestoreRecordName zoneID:self.rampZoneID];
    self.restoreRampRecord = [[CKRecord alloc] initWithRecordType:kOTRampForEnrollmentRecordName recordID:restoreRecordID];
    self.restoreRampRecord[kFeatureAllowedKey] = @NO;
    self.restoreRampRecord[kFeaturePromotedKey] = @NO;  //always false right now
    self.restoreRampRecord[kFeatureVisibleKey] = @NO;
    self.restoreRampRecord[kRetryAfterKey] = [[NSNumber alloc]initWithInt:3600];

    CKRecordID* cfuRecordID = [[CKRecordID alloc] initWithRecordName:kOTRampForCFURecordName zoneID:self.rampZoneID];
    self.cfuRampRecord = [[CKRecord alloc] initWithRecordType:kOTRampForCFURecordName recordID:cfuRecordID];
    self.cfuRampRecord[kFeatureAllowedKey] = @NO;
    self.cfuRampRecord[kFeaturePromotedKey] = @NO;  //always false right now
    self.cfuRampRecord[kFeatureVisibleKey] = @NO;
    self.cfuRampRecord[kRetryAfterKey] = [[NSNumber alloc]initWithInt:3600];

    [self.rampZone addToZone:self.enrollRampRecord];
    [self.rampZone addToZone:self.restoreRampRecord];
    [self.rampZone addToZone:self.cfuRampRecord];
}

-(void) setUpRampRecordsInCloudKitWithFeatureOn
{
    CKRecordID* enrollRecordID = [[CKRecordID alloc] initWithRecordName:kOTRampForEnrollmentRecordName zoneID:self.rampZoneID];
    self.enrollRampRecord = [[CKRecord alloc] initWithRecordType:kOTRampForEnrollmentRecordName recordID:enrollRecordID];
    self.enrollRampRecord[kFeatureAllowedKey] = @YES;
    self.enrollRampRecord[kFeaturePromotedKey] = @NO;   //always false right now
    self.enrollRampRecord[kFeatureVisibleKey] = @YES;
    self.enrollRampRecord[kRetryAfterKey] = [[NSNumber alloc]initWithInt:3600];

    CKRecordID* restoreRecordID = [[CKRecordID alloc] initWithRecordName:kOTRampForRestoreRecordName zoneID:self.rampZoneID];
    self.restoreRampRecord = [[CKRecord alloc] initWithRecordType:kOTRampForEnrollmentRecordName recordID:restoreRecordID];
    self.restoreRampRecord[kFeatureAllowedKey] = @YES;
    self.restoreRampRecord[kFeaturePromotedKey] = @NO;  //always false right now
    self.restoreRampRecord[kFeatureVisibleKey] = @YES;
    self.restoreRampRecord[kRetryAfterKey] = [[NSNumber alloc]initWithInt:3600];

    CKRecordID* cfuRecordID = [[CKRecordID alloc] initWithRecordName:kOTRampForCFURecordName zoneID:self.rampZoneID];
    self.cfuRampRecord = [[CKRecord alloc] initWithRecordType:kOTRampForCFURecordName recordID:cfuRecordID];
    self.cfuRampRecord[kFeatureAllowedKey] = @YES;
    self.cfuRampRecord[kFeaturePromotedKey] = @NO;  //always false right now
    self.cfuRampRecord[kFeatureVisibleKey] = @YES;
    self.cfuRampRecord[kRetryAfterKey] = [[NSNumber alloc]initWithInt:3600];

    [self.rampZone addToZone:self.enrollRampRecord];
    [self.rampZone addToZone:self.restoreRampRecord];
    [self.rampZone addToZone:self.cfuRampRecord];
}


-(void)expectAddedCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*)records holdFetch:(BOOL)shouldHoldTheFetch
{
    __weak __typeof(self) weakSelf = self;
    
    [self expectCKModifyRecords:records
        deletedRecordTypeCounts:nil
                         zoneID:self.otZoneID
            checkModifiedRecord:^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: OTCKRecordBottledPeerType]) {
                    return YES;
                } else { //not a Bottled Peer Record Type
                    return NO;
                }
            }
           runAfterModification:^{
               __strong __typeof(self) strongSelf = weakSelf;
               if(shouldHoldTheFetch){
                   [strongSelf holdCloudKitFetches];
               }

           }
     ];
}

-(void)expectDeletedCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*)records holdFetch:(BOOL)shouldHoldTheFetch
{
    __weak __typeof(self) weakSelf = self;
    
    [self expectCKModifyRecords:[NSMutableDictionary dictionary]
        deletedRecordTypeCounts:records
                         zoneID:self.otZoneID
            checkModifiedRecord:^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: OTCKRecordBottledPeerType]) {
                    return YES;
                } else { //not a Bottled Peer Record Type
                    return NO;
                }
            }
           runAfterModification:^{
               __strong __typeof(self) strongSelf = weakSelf;
               if(shouldHoldTheFetch){
                   [strongSelf holdCloudKitFetches];
               }
           }
     ];
}

- (nullable OTIdentity *)currentIdentity:(NSError * _Nullable __autoreleasing * _Nullable)error {
    return [[OTIdentity alloc]initWithPeerID:self.egoPeerID spID:self.sosPeerID peerSigningKey:self.peerSigningKey peerEncryptionkey:self.peerEncryptionKey error:error];
}

@end
#endif
