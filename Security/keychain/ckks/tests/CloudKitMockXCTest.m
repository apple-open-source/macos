/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#import "CloudKitMockXCTest.h"

#import <ApplePushService/ApplePushService.h>
#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>
#import <CloudKit/CKContainer_Private.h>
#import <OCMock/OCMock.h>

#include "OSX/sec/securityd/Regressions/SecdTestKeychainUtilities.h"
#include <utilities/SecFileLocations.h>
#include <securityd/SecItemServer.h>

#if NO_SERVER
#include <securityd/spi.h>
#endif

#include <Security/SecureObjectSync/SOSViews.h>

#include <utilities/SecDb.h>
#include <securityd/SecItemServer.h>
#include <keychain/ckks/CKKS.h>
#include <keychain/ckks/CKKSViewManager.h>
#include <keychain/ckks/CKKSKeychainView.h>
#include <keychain/ckks/CKKSItem.h>
#include <keychain/ckks/CKKSOutgoingQueueEntry.h>
#include <keychain/ckks/CKKSKey.h>
#include "keychain/ckks/CKKSGroupOperation.h"
#include "keychain/ckks/CKKSLockStateTracker.h"

#import "MockCloudKit.h"

@interface BoolHolder : NSObject
@property bool state;
@end

@implementation BoolHolder
@end

// Inform OCMock about the internals of CKContainer
@interface CKContainer ()
- (void)_checkSelfCloudServicesEntitlement;
@end


@implementation CloudKitMockXCTest

+ (void)setUp {
    // Turn on testing
    SecCKKSTestsEnable();
    [super setUp];

#if NO_SERVER
    securityd_init_local_spi();
#endif
}

- (void)setUp {
    [super setUp];

    // All tests start with the same flag set.
    SecCKKSTestResetFlags();
    SecCKKSTestSetDisableSOS(true);

    self.silentFetchesAllowed = true;

    __weak __typeof(self) weakSelf = self;
    self.operationQueue = [[NSOperationQueue alloc] init];
    self.operationQueue.maxConcurrentOperationCount = 1;

    self.zones = [[NSMutableDictionary alloc] init];

    self.mockDatabase = OCMStrictClassMock([CKDatabase class]);
    self.mockContainer = OCMClassMock([CKContainer class]);
    OCMStub([self.mockContainer containerWithIdentifier:[OCMArg isKindOfClass:[NSString class]]]).andReturn(self.mockContainer);
    OCMStub([self.mockContainer defaultContainer]).andReturn(self.mockContainer);
    OCMStub([self.mockContainer alloc]).andReturn(self.mockContainer);
    OCMStub([self.mockContainer containerIdentifier]).andReturn(SecCKKSContainerName);
    OCMStub([self.mockContainer initWithContainerID: [OCMArg any] options: [OCMArg any]]).andReturn(self.mockContainer);
    OCMStub([self.mockContainer privateCloudDatabase]).andReturn(self.mockDatabase);
    OCMStub([self.mockContainer serverPreferredPushEnvironmentWithCompletionHandler: ([OCMArg invokeBlockWithArgs:@"fake APS push string", [NSNull null], nil])]);

    // If you want to change this, you'll need to update the mock
    _ckDeviceID = @"fake-cloudkit-device-id";
    OCMStub([self.mockContainer fetchCurrentDeviceIDWithCompletionHandler: ([OCMArg invokeBlockWithArgs:self.ckDeviceID, [NSNull null], nil])]);

    self.accountStatus = CKAccountStatusAvailable;
    self.supportsDeviceToDeviceEncryption = YES;

    // Inject a fake operation dependency into the manager object, so that the tests can perform setup and mock expectations before zone setup begins
    // Also blocks all CK account state retrieval operations (but not circle status ones)
    self.ckaccountHoldOperation = [NSBlockOperation named:@"ckaccount-hold" withBlock:^{
        secnotice("ckks", "CKKS CK account status test hold released");
    }];

    OCMStub([self.mockContainer accountStatusWithCompletionHandler:
                    [OCMArg checkWithBlock:^BOOL(void (^passedBlock) (CKAccountStatus accountStatus,
                                                                      NSError * _Nullable error)) {

        if(passedBlock) {
            __strong __typeof(self) strongSelf = weakSelf;
            NSBlockOperation* fulfillBlock = [NSBlockOperation named:@"account-status-completion" withBlock: ^{
                passedBlock(weakSelf.accountStatus, nil);
            }];
            [fulfillBlock addDependency: strongSelf.ckaccountHoldOperation];
            [strongSelf.operationQueue addOperation: fulfillBlock];

            return YES;
        }
        return NO;
    }]]);

    OCMStub([self.mockContainer accountInfoWithCompletionHandler:
             [OCMArg checkWithBlock:^BOOL(void (^passedBlock) (CKAccountInfo* accountInfo,
                                                               NSError * error)) {
        __strong __typeof(self) strongSelf = weakSelf;
        if(passedBlock && strongSelf) {
            NSBlockOperation* fulfillBlock = [NSBlockOperation named:@"account-info-completion" withBlock: ^{
                __strong __typeof(self) blockStrongSelf = weakSelf;
                CKAccountInfo* account = [[CKAccountInfo alloc] init];
                account.accountStatus = blockStrongSelf.accountStatus;
                account.supportsDeviceToDeviceEncryption = blockStrongSelf.supportsDeviceToDeviceEncryption;
                account.accountPartition = CKAccountPartitionTypeProduction;
                passedBlock((CKAccountInfo*)account, nil);
            }];
            [fulfillBlock addDependency: strongSelf.ckaccountHoldOperation];
            [strongSelf.operationQueue addOperation: fulfillBlock];

            return YES;
        }
        return NO;
    }]]);

    self.circleStatus = kSOSCCInCircle;
    self.mockAccountStateTracker = OCMClassMock([CKKSCKAccountStateTracker class]);
    OCMStub([self.mockAccountStateTracker getCircleStatus]).andCall(self, @selector(circleStatus));

    // If we're in circle, come up with a fake circle id. Otherwise, return an error.
    self.circlePeerID = @"fake-circle-id";
    OCMStub([self.mockAccountStateTracker fetchCirclePeerID:
             [OCMArg checkWithBlock:^BOOL(void (^passedBlock) (NSString* peerID,
                                                               NSError * error)) {
        __strong __typeof(self) strongSelf = weakSelf;
        if(passedBlock && strongSelf) {
            if(strongSelf.circleStatus == kSOSCCInCircle) {
                passedBlock(strongSelf.circlePeerID, nil);
            } else {
                passedBlock(nil, [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey:@"no account, no circle id"}]);
            }

            return YES;
        }
        return NO;
    }]]);

    self.aksLockState = false; // Lie and say AKS is always unlocked
    self.mockLockStateTracker = OCMClassMock([CKKSLockStateTracker class]);
    OCMStub([self.mockLockStateTracker queryAKSLocked]).andCall(self, @selector(aksLockState));

    self.mockFakeCKModifyRecordZonesOperation = OCMClassMock([FakeCKModifyRecordZonesOperation class]);
    OCMStub([self.mockFakeCKModifyRecordZonesOperation ckdb]).andReturn(self.zones);

    self.mockFakeCKModifySubscriptionsOperation = OCMClassMock([FakeCKModifySubscriptionsOperation class]);
    OCMStub([self.mockFakeCKModifySubscriptionsOperation ckdb]).andReturn(self.zones);

    self.mockFakeCKFetchRecordZoneChangesOperation = OCMClassMock([FakeCKFetchRecordZoneChangesOperation class]);
    OCMStub([self.mockFakeCKFetchRecordZoneChangesOperation ckdb]).andReturn(self.zones);

    OCMStub([self.mockDatabase addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKFetchRecordZoneChangesOperation class]]) {
            if(strongSelf.silentFetchesAllowed) {
                matches = YES;

                FakeCKFetchRecordZoneChangesOperation *frzco = (FakeCKFetchRecordZoneChangesOperation *)obj;
                [strongSelf.operationQueue addOperation: frzco];
            }
        }
        return matches;
    }]]);


    self.testZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName];

    // Inject a fake operation dependency into the manager object, so that the tests can perform setup and mock expectations before zone setup begins
    // Also blocks all CK account state retrieval operations (but not circle status ones)
    self.ckksHoldOperation = [[NSBlockOperation alloc] init];
    [self.ckksHoldOperation addExecutionBlock:^{
        secnotice("ckks", "CKKS testing hold released");
    }];
    self.ckksHoldOperation.name = @"ckks-hold";

    self.mockCKKSViewManager = OCMClassMock([CKKSViewManager class]);
    OCMStub([self.mockCKKSViewManager viewList]).andCall(self, @selector(managedViewList));
    OCMStub([self.mockCKKSViewManager syncBackupAndNotifyAboutSync]);

    self.injectedManager = [[CKKSViewManager alloc] initWithContainerName:SecCKKSContainerName
                                                                   usePCS:SecCKKSContainerUsePCS
                                     fetchRecordZoneChangesOperationClass:[FakeCKFetchRecordZoneChangesOperation class]
                                        modifySubscriptionsOperationClass:[FakeCKModifySubscriptionsOperation class]
                                          modifyRecordZonesOperationClass:[FakeCKModifyRecordZonesOperation class]
                                                       apsConnectionClass:[FakeAPSConnection class]
                                                nsnotificationCenterClass:[FakeNSNotificationCenter class]
                                                            notifierClass:[FakeCKKSNotifier class]
                                                                setupHold:self.ckksHoldOperation];

    [CKKSViewManager resetManager:false setTo:self.injectedManager];

    // Make a new fake keychain
    NSString* smallName = [self.name componentsSeparatedByString:@" "][1];
    smallName = [smallName stringByReplacingOccurrencesOfString:@"]" withString:@""];

    NSString* tmp_dir = [NSString stringWithFormat: @"/tmp/%@.%X", smallName, arc4random()];
    [[NSFileManager defaultManager] createDirectoryAtPath:[NSString stringWithFormat: @"%@/Library/Keychains", tmp_dir] withIntermediateDirectories:YES attributes:nil error:NULL];

    SetCustomHomeURLString((__bridge CFStringRef) tmp_dir);
    SecKeychainDbReset(NULL);

    // Actually load the database.
    kc_with_dbt(true, NULL, ^bool (SecDbConnectionRef dbt) { return false; });
}

-(CKKSCKAccountStateTracker*)accountStateTracker {
    return self.injectedManager.accountTracker;
}

-(CKKSLockStateTracker*)lockStateTracker {
    return self.injectedManager.lockStateTracker;
}

-(NSSet*)managedViewList {
    return (NSSet*) CFBridgingRelease(SOSViewCopyViewSet(kViewSetCKKS));
}

-(void)expectCKFetch {
    // Create an object for the block to retain and modify
    BoolHolder* runAlready = [[BoolHolder alloc] init];

    __weak __typeof(self) weakSelf = self;
    [[self.mockDatabase expect] addOperation: [OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(self) strongSelf = weakSelf;
        if(runAlready.state) {
            return NO;
        }
        BOOL matches = NO;
        if ([obj isKindOfClass: [FakeCKFetchRecordZoneChangesOperation class]]) {
            matches = YES;
            runAlready.state = true;

            FakeCKFetchRecordZoneChangesOperation *frzco = (FakeCKFetchRecordZoneChangesOperation *)obj;
            [strongSelf.operationQueue addOperation: frzco];
        }
        return matches;
    }]];
}

- (void)startCKKSSubsystem {
    [self startCKAccountStatusMock];
    [self startCKKSSubsystemOnly];
}

- (void)startCKKSSubsystemOnly {
    // Note: currently, based on how we're mocking up the zone creation and zone subscription operation,
    // they will 'fire' before this method is called. It's harmless, since the mocks immediately succeed
    // and return; it's just a tad confusing.
    if([self.ckksHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckksHoldOperation];
    }
}

- (void)startCKAccountStatusMock {
    // Note: currently, based on how we're mocking up the zone creation and zone subscription operation,
    // they will 'fire' before this method is called. It's harmless, since the mocks immediately succeed
    // and return; it's just a tad confusing.
    if([self.ckaccountHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckaccountHoldOperation];
    }
}

-(void)holdCloudKitModifications {
    self.ckModifyHoldOperation = [NSBlockOperation blockOperationWithBlock:^{
        secnotice("ckks", "Released CloudKit modification hold.");
    }];
}
-(void)releaseCloudKitModificationHold {
    if([self.ckModifyHoldOperation isPending]) {
        [self.operationQueue addOperation: self.ckModifyHoldOperation];
    }
}

- (void)expectCKModifyItemRecords: (NSUInteger) expectedNumberOfRecords currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords zoneID: (CKRecordZoneID*) zoneID {
    [self expectCKModifyItemRecords:expectedNumberOfRecords
           currentKeyPointerRecords:expectedCurrentKeyRecords
                             zoneID:zoneID
                          checkItem:nil];
}

- (void)expectCKModifyItemRecords: (NSUInteger) expectedNumberOfRecords currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords zoneID: (CKRecordZoneID*) zoneID checkItem: (BOOL (^)(CKRecord*)) checkItem {
    [self expectCKModifyItemRecords:expectedNumberOfRecords
                     deletedRecords:0
           currentKeyPointerRecords:expectedCurrentKeyRecords
                             zoneID:zoneID
                          checkItem:checkItem];
}

- (void)expectCKModifyItemRecords:(NSUInteger)expectedNumberOfModifiedRecords
                   deletedRecords:(NSUInteger)expectedNumberOfDeletedRecords
         currentKeyPointerRecords:(NSUInteger)expectedCurrentKeyRecords
                           zoneID:(CKRecordZoneID*)zoneID
                        checkItem:(BOOL (^)(CKRecord*))checkItem {
    // We're updating the device state type on every update, so add it in here
    NSMutableDictionary* expectedRecords = [@{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: expectedNumberOfModifiedRecords],
                                              SecCKRecordCurrentKeyType: [NSNumber numberWithUnsignedInteger: expectedCurrentKeyRecords],
                                              SecCKRecordDeviceStateType: [NSNumber numberWithUnsignedInt: 1],
                                              } mutableCopy];

    if(SecCKKSSyncManifests()) {
        expectedRecords[SecCKRecordManifestType] = [NSNumber numberWithInt: 1];
        expectedRecords[SecCKRecordManifestLeafType] = [NSNumber numberWithInt: 72];
    }

    NSDictionary* deletedRecords = nil;
    if(expectedNumberOfDeletedRecords != 0) {
        deletedRecords = @{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: expectedNumberOfDeletedRecords]};
    }

    [self expectCKModifyRecords:expectedRecords
        deletedRecordTypeCounts:deletedRecords
                         zoneID:zoneID
            checkModifiedRecord: ^BOOL (CKRecord* record){
                if([record.recordType isEqualToString: SecCKRecordItemType] && checkItem) {
                    return checkItem(record);
                } else {
                    return YES;
                }
            }
           runAfterModification:nil];
}



- (void)expectCKModifyKeyRecords: (NSUInteger) expectedNumberOfRecords currentKeyPointerRecords: (NSUInteger) expectedCurrentKeyRecords zoneID: (CKRecordZoneID*) zoneID {
    NSNumber* nkeys = [NSNumber numberWithUnsignedInteger: expectedNumberOfRecords];
    NSNumber* ncurrentkeys = [NSNumber numberWithUnsignedInteger: expectedCurrentKeyRecords];

    [self expectCKModifyRecords:@{SecCKRecordIntermediateKeyType: nkeys, SecCKRecordCurrentKeyType: ncurrentkeys}
        deletedRecordTypeCounts:nil
                         zoneID:zoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
}

- (void)expectCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*) expectedRecordTypeCounts
      deletedRecordTypeCounts:(NSDictionary<NSString*, NSNumber*>*) expectedDeletedRecordTypeCounts
                       zoneID:(CKRecordZoneID*) zoneID
          checkModifiedRecord:(BOOL (^)(CKRecord*)) checkModifiedRecord
         runAfterModification:(void (^) ())afterModification
{
    __weak __typeof(self) weakSelf = self;

    // Create an object for the block to retain and modify
    BoolHolder* runAlready = [[BoolHolder alloc] init];

    secnotice("fakecloudkit", "expecting an operation matching modifications: %@ deletions: %@",
              expectedRecordTypeCounts, expectedDeletedRecordTypeCounts);

    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        secnotice("fakecloudkit", "Received an operation, checking");
        __block bool matches = false;
        if(runAlready.state) {
            secnotice("fakecloudkit", "Run already, skipping");
            return NO;
        }

        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            XCTAssertNotNil(strongSelf, "self exists");

            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;
            matches = true;

            NSMutableDictionary<NSString*, NSNumber*>* modifiedRecordTypeCounts = [[NSMutableDictionary alloc] init];
            NSMutableDictionary<NSString*, NSNumber*>* deletedRecordTypeCounts = [[NSMutableDictionary alloc] init];

            // First: check if it matches. If it does, _then_ execute the operation.
            // Supports single-zone atomic writes only

            if(!op.atomic) {
                // We only care about atomic operations
                secnotice("fakecloudkit", "Not an atomic operation; quitting: %@", op);
                return NO;
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Have a zone for these records");

            for(CKRecord* record in op.recordsToSave) {
                if(![record.recordID.zoneID isEqual: zoneID]) {
                    secnotice("fakecloudkit", "Modified record zone ID mismatch: %@ %@", zoneID, record.recordID.zoneID);
                    return NO;
                }

                if([zone errorFromSavingRecord: record]) {
                    secnotice("fakecloudkit", "Record zone rejected record write: %@", record);
                    return NO;
                }

                NSNumber* currentCountNumber = modifiedRecordTypeCounts[record.recordType];
                NSUInteger currentCount = currentCountNumber ? [currentCountNumber unsignedIntegerValue] : 0;
                modifiedRecordTypeCounts[record.recordType] = [NSNumber numberWithUnsignedInteger: currentCount + 1];
            }

            for(CKRecordID* recordID in op.recordIDsToDelete) {
                if(![recordID.zoneID isEqual: zoneID]) {
                    matches = false;
                    secnotice("fakecloudkit", "Deleted record zone ID mismatch: %@ %@", zoneID, recordID.zoneID);
                }

                // Find the object in CloudKit, and record its type
                CKRecord* record = strongSelf.zones[zoneID].currentDatabase[recordID];
                if(record) {
                    NSNumber* currentCountNumber = deletedRecordTypeCounts[record.recordType];
                    NSUInteger currentCount = currentCountNumber ? [currentCountNumber unsignedIntegerValue] : 0;
                    deletedRecordTypeCounts[record.recordType] = [NSNumber numberWithUnsignedInteger: currentCount + 1];
                }
            }

            NSMutableDictionary* filteredExpectedRecordTypeCounts = [expectedRecordTypeCounts mutableCopy];
            for(NSString* key in filteredExpectedRecordTypeCounts.allKeys) {
                if([filteredExpectedRecordTypeCounts[key] isEqual: [NSNumber numberWithInt:0]]) {
                    filteredExpectedRecordTypeCounts[key] = nil;
                }
            }
            filteredExpectedRecordTypeCounts[SecCKRecordManifestType] = modifiedRecordTypeCounts[SecCKRecordManifestType];
            filteredExpectedRecordTypeCounts[SecCKRecordManifestLeafType] = modifiedRecordTypeCounts[SecCKRecordManifestLeafType];

            // Inspect that we have exactly the same records as we expect
            if(expectedRecordTypeCounts) {
                matches &= !![modifiedRecordTypeCounts isEqual: filteredExpectedRecordTypeCounts];
                if(!matches) {
                    secnotice("fakecloudkit", "Record number mismatch: %@ %@", modifiedRecordTypeCounts, filteredExpectedRecordTypeCounts);
                    return NO;
                }
            } else {
                matches &= op.recordsToSave.count == 0u;
                if(!matches) {
                    secnotice("fakecloudkit", "Record number mismatch: %@ 0", modifiedRecordTypeCounts);
                    return NO;
                }
            }
            if(expectedDeletedRecordTypeCounts) {
                matches &= !![deletedRecordTypeCounts  isEqual: expectedDeletedRecordTypeCounts];
                if(!matches) {
                    secnotice("fakecloudkit", "Deleted record number mismatch: %@ %@", deletedRecordTypeCounts, expectedDeletedRecordTypeCounts);
                    return NO;
                }
            } else {
                matches &= op.recordIDsToDelete.count == 0u;
                if(!matches) {
                    secnotice("fakecloudkit", "Deleted record number mismatch: %@ 0", deletedRecordTypeCounts);
                    return NO;
                }
            }

            // We have the right number of things, and their etags match. Ensure that they have the right etags
            if(matches && checkModifiedRecord) {
                // Clearly we have the right number of things. Call checkRecord on them...
                for(CKRecord* record in op.recordsToSave) {
                    matches &= !!(checkModifiedRecord(record));
                    if(!matches) {
                        secnotice("fakecloudkit", "Check record reports NO: %@ 0", record);
                        return NO;
                    }
                }
            }

            if(matches) {
                // Emulate cloudkit and schedule the operation for execution. Be sure to wait for this operation
                // if you'd like to read the data from this write.
                NSBlockOperation* ckop = [NSBlockOperation named:@"cloudkit-write" withBlock: ^{
                    @synchronized(zone.currentDatabase) {
                        NSMutableArray* savedRecords = [[NSMutableArray alloc] init];
                        for(CKRecord* record in op.recordsToSave) {
                            CKRecord* reflectedRecord = [record copy];
                            reflectedRecord.modificationDate = [NSDate date];

                            [zone addToZone: reflectedRecord];

                            [savedRecords addObject:reflectedRecord];
                            op.perRecordCompletionBlock(reflectedRecord, nil);
                        }
                        for(CKRecordID* recordID in op.recordIDsToDelete) {
                            // I don't believe CloudKit fails an operation if you delete a record that's not there, so:
                            [zone deleteCKRecordIDFromZone: recordID];
                        }

                        op.modifyRecordsCompletionBlock(savedRecords, op.recordIDsToDelete, nil);

                        if(afterModification) {
                            afterModification();
                        }

                        op.isFinished = YES;
                    }
                }];
                [ckop addNullableDependency:strongSelf.ckModifyHoldOperation];
                [strongSelf.operationQueue addOperation: ckop];
            }
        }
        if(matches) {
            runAlready.state = true;
        }
        return matches ? YES : NO;
    }]];
}

- (void)failNextZoneCreation:(CKRecordZoneID*)zoneID {
    XCTAssertNil(self.zones[zoneID], "Zone does not exist yet");
    self.zones[zoneID] = [[FakeCKZone alloc] initZone: zoneID];
    self.zones[zoneID].creationError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}];
}

// Report success, but don't actually create the zone.
// This way, you can find ZoneNotFound errors later on
- (void)failNextZoneCreationSilently:(CKRecordZoneID*)zoneID {
    XCTAssertNil(self.zones[zoneID], "Zone does not exist yet");
    self.zones[zoneID] = [[FakeCKZone alloc] initZone: zoneID];
    self.zones[zoneID].failCreationSilently = true;
}

- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID {
    XCTAssertNotNil(self.zones[zoneID], "Zone exists");
    self.zones[zoneID].subscriptionError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkUnavailable userInfo:@{}];
}

- (void)failNextZoneSubscription:(CKRecordZoneID*)zoneID withError:(NSError*)error {
    XCTAssertNotNil(self.zones[zoneID], "Zone exists");
    self.zones[zoneID].subscriptionError = error;
}

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID {
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:zoneID blockAfterReject:nil];
}

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID blockAfterReject: (void (^)())blockAfterReject {
    [self failNextCKAtomicModifyItemRecordsUpdateFailure:zoneID blockAfterReject:blockAfterReject withError:nil];
}

- (void)failNextCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID blockAfterReject: (void (^)())blockAfterReject withError:(NSError*)error {
    __weak __typeof(self) weakSelf = self;

    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        __block bool rejected = false;
        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;

            if(!op.atomic) {
                // We only care about atomic operations
                return NO;
            }

            // We want to only match zone updates pertaining to this zone
            for(CKRecord* record in op.recordsToSave) {
                if(![record.recordID.zoneID isEqual: zoneID]) {
                    return NO;
                }
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Have a zone for these records");

            rejected = true;

            if(error) {
                [strongSelf rejectWrite: op withError:error];
            } else {
                NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = [[NSMutableDictionary alloc] init];
                [strongSelf rejectWrite: op failedRecords:failedRecords];
            }

            if(blockAfterReject) {
                blockAfterReject();
            }
        }
        return rejected ? YES : NO;
    }]];
}

- (void)expectCKAtomicModifyItemRecordsUpdateFailure:(CKRecordZoneID*)zoneID {
    __weak __typeof(self) weakSelf = self;

    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        __block bool rejected = false;
        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;

            if(!op.atomic) {
                // We only care about atomic operations
                return NO;
            }

            // We want to only match zone updates pertaining to this zone
            for(CKRecord* record in op.recordsToSave) {
                if(![record.recordID.zoneID isEqual: zoneID]) {
                    return NO;
                }
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Have a zone for these records");

            NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = [[NSMutableDictionary alloc] init];

            @synchronized(zone.currentDatabase) {
                for(CKRecord* record in op.recordsToSave) {
                    // Check if we should allow this transaction
                    NSError* recordSaveError = [zone errorFromSavingRecord: record];
                    if(recordSaveError) {
                        failedRecords[record.recordID] = recordSaveError;
                        rejected = true;
                    }
                }
            }

            if(rejected) {
                [strongSelf rejectWrite: op failedRecords:failedRecords];
            }
        }
        return rejected ? YES : NO;
    }]];
}

-(void)rejectWrite:(CKModifyRecordsOperation*)op withError:(NSError*)error {
    // Emulate cloudkit and schedule the operation for execution. Be sure to wait for this operation
    // if you'd like to read the data from this write.
    NSBlockOperation* ckop = [NSBlockOperation named:@"cloudkit-reject-write-error" withBlock: ^{
        op.modifyRecordsCompletionBlock(nil, nil, error);
        op.isFinished = YES;
    }];
    [ckop addNullableDependency: self.ckModifyHoldOperation];
    [self.operationQueue addOperation: ckop];
}

-(void)rejectWrite:(CKModifyRecordsOperation*)op failedRecords:(NSMutableDictionary<CKRecordID*, NSError*>*)failedRecords {
    // Add the batch request failed errors
    for(CKRecord* record in op.recordsToSave) {
        NSError* exists = failedRecords[record.recordID];
        if(!exists) {
            // TODO: might have important userInfo, but we're not mocking that yet
            failedRecords[record.recordID] = [[CKPrettyError alloc] initWithDomain: CKErrorDomain code: CKErrorBatchRequestFailed userInfo: @{}];
        }
    }

    NSError* error = [[CKPrettyError alloc] initWithDomain: CKErrorDomain code: CKErrorPartialFailure userInfo: @{CKPartialErrorsByItemIDKey: failedRecords}];

    // Emulate cloudkit and schedule the operation for execution. Be sure to wait for this operation
    // if you'd like to read the data from this write.
    NSBlockOperation* ckop = [NSBlockOperation named:@"cloudkit-reject-write" withBlock: ^{
        op.modifyRecordsCompletionBlock(nil, nil, error);
        op.isFinished = YES;
    }];
    [ckop addNullableDependency: self.ckModifyHoldOperation];
    [self.operationQueue addOperation: ckop];
}

- (void)expectCKDeleteItemRecords:(NSUInteger)expectedNumberOfRecords
                           zoneID:(CKRecordZoneID*) zoneID {

    // We're updating the device state type on every update, so add it in here
    NSMutableDictionary* expectedRecords = [@{
                                              SecCKRecordDeviceStateType: [NSNumber numberWithUnsignedInt: 1],
                                              } mutableCopy];
    if(SecCKKSSyncManifests()) {
        // TODO: this really shouldn't be 2.
        expectedRecords[SecCKRecordManifestType] = [NSNumber numberWithInt: 2];
        expectedRecords[SecCKRecordManifestLeafType] = [NSNumber numberWithInt: 72];
    }

    [self expectCKModifyRecords:expectedRecords
        deletedRecordTypeCounts:@{SecCKRecordItemType: [NSNumber numberWithUnsignedInteger: expectedNumberOfRecords]}
                         zoneID:zoneID
            checkModifiedRecord:nil
           runAfterModification:nil];
}

-(void)waitForCKModifications {
    // CloudKit modifications are put on the local queue.
    // This is heavyweight but should suffice.
    [self.operationQueue waitUntilAllOperationsAreFinished];
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.

    if(SecCKKSIsEnabled()) {
        // Ensure we don't have any blocking operations
        [self startCKKSSubsystem];

        [self waitForCKModifications];

        XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:2*NSEC_PER_SEC],
            "Timeout did not occur waiting for SecCKKSInitialize");

        // Make sure this happens before teardown.
        XCTAssertEqual(0, [self.accountStateTracker.finishedInitialCalls wait:1*NSEC_PER_SEC], "Account state tracker initialized itself");
    }

    [super tearDown];

    [self.injectedManager cancelPendingOperations];
    [CKKSViewManager resetManager:true setTo:nil];
    self.injectedManager = nil;

    [self.mockAccountStateTracker stopMocking];
    self.mockAccountStateTracker = nil;

    [self.mockLockStateTracker stopMocking];
    self.mockLockStateTracker = nil;

    [self.mockCKKSViewManager stopMocking];
    self.mockCKKSViewManager = nil;

    [self.mockFakeCKModifyRecordZonesOperation stopMocking];
    self.mockFakeCKModifyRecordZonesOperation = nil;

    [self.mockFakeCKModifySubscriptionsOperation stopMocking];
    self.mockFakeCKModifySubscriptionsOperation = nil;

    [self.mockFakeCKFetchRecordZoneChangesOperation stopMocking];
    self.mockFakeCKFetchRecordZoneChangesOperation = nil;

    [self.mockDatabase stopMocking];
    self.mockDatabase = nil;

    [self.mockContainer stopMocking];
    self.mockContainer = nil;

    self.zones = nil;
    self.operationQueue = nil;
    self.ckksHoldOperation = nil;
    self.ckaccountHoldOperation = nil;

    SecCKKSTestResetFlags();
}

- (CKKSKey*) fakeTLK: (CKRecordZoneID*)zoneID {
    CKKSKey* key = [[CKKSKey alloc] initSelfWrappedWithAESKey:[[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="]
                                                         uuid:[[NSUUID UUID] UUIDString]
                                                     keyclass:SecCKKSKeyClassTLK
                                                        state: SecCKKSProcessedStateLocal
                                                       zoneID:zoneID
                                              encodedCKRecord: nil
                                                   currentkey: true];
    [key CKRecordWithZoneID: zoneID];
    return key;
}


@end

#endif
