
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <notify.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingTestsBase.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ot/ObjCImprovements.h"

@interface CloudKitKeychainFetchTests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainFetchTests

- (void)testMoreComing {
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Cause a fetch to occur, so that the local CKKS instance is definitely up to date with what it has uploaded, so the next fetch will be entirely items
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"account1"]];
    FakeCKServerChangeToken* ck1 = ckzone.currentChangeToken;
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"account3"]];

    ckzone.limitFetchTo = ck1;

    self.silentFetchesAllowed = false;
    [self expectCKFetch];
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck1.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    NSTimeInterval delta = [ckzone.fetchRecordZoneChangesTimestamps[3] timeIntervalSinceDate:ckzone.fetchRecordZoneChangesTimestamps[2]];
    XCTAssertLessThan(delta, 2, "operation 2 and 3 should be back-to-back");

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];
    [self findGenericPassword: @"account2" expecting:errSecSuccess];
    [self findGenericPassword: @"account3" expecting:errSecSuccess];
}

- (void)testMoreComingRepeated {
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Cause a fetch to occur, so that the local CKKS instance is definitely up to date with what it has uploaded, so the next fetch will be entirely items
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"account1"]];
    FakeCKServerChangeToken* ck1 = ckzone.currentChangeToken;
    XCTAssert(ck1.forward == YES);
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"account3"]];
    FakeCKServerChangeToken* ck2 = ckzone.currentChangeToken;
    XCTAssert(ck2.forward == YES);
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D04" withAccount:@"account4"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D05" withAccount:@"account5"]];

    ckzone.limitFetchTo = ck1;

    self.silentFetchesAllowed = false;
    // This fetch will return up to ck1.
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        return YES;
    } runBeforeFinished:^{
        ckzone.limitFetchTo = ck2;
    }];

    // This fetch will return up to ck2.
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck1.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    // This fetch will return the final two items.
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck2.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    NSTimeInterval delta = [ckzone.fetchRecordZoneChangesTimestamps[3] timeIntervalSinceDate:ckzone.fetchRecordZoneChangesTimestamps[2]];
    XCTAssertLessThan(delta, 2, "operation 2 and 3 should be back-to-back");

    delta = [ckzone.fetchRecordZoneChangesTimestamps[4] timeIntervalSinceDate:ckzone.fetchRecordZoneChangesTimestamps[3]];
    XCTAssertLessThan(delta, 2, "operation 3 and 4 should be back-to-back");

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];
    [self findGenericPassword: @"account2" expecting:errSecSuccess];
    [self findGenericPassword: @"account3" expecting:errSecSuccess];
    [self findGenericPassword: @"account4" expecting:errSecSuccess];
    [self findGenericPassword: @"account5" expecting:errSecSuccess];
}

- (void)testMoreComingDespitePartialTimeout {
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Cause a fetch to occur, so that the local CKKS instance is definitely up to date with what it has uploaded, so the next fetch will be entirely items
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"account1"]];
    FakeCKServerChangeToken* ck1 = ckzone.currentChangeToken;
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"account3"]];

    // The fetch fails with partial results
    ckzone.limitFetchTo = ck1;
    ckzone.limitFetchError = [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:4]}];

    self.silentFetchesAllowed = false;
    [self expectCKFetch];
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck1.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];
    [self findGenericPassword: @"account2" expecting:errSecSuccess];
    [self findGenericPassword: @"account3" expecting:errSecSuccess];
}

- (void)testMoreComingWithFullFailure {
    WEAKIFY(self);
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Cause a fetch to occur, so that the local CKKS instance is definitely up to date with what it has uploaded, so the next fetch will be entirely items
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"account1"]];
    FakeCKServerChangeToken* ck1 = ckzone.currentChangeToken;
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"account3"]];

    // The fetch fails with partial results
    ckzone.limitFetchTo = ck1;
    ckzone.limitFetchError = [[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:4]}];

    self.silentFetchesAllowed = false;
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        return YES;
    } runBeforeFinished:^{
        STRONGIFY(self);
        // We want to fail with a full network failure error, but we explicitly don't want to set network reachability:
        // CKKS won't send the fetch in that case. So...
        [self.keychainZone failNextFetchWith:[NSError errorWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:NULL]];
        [self expectCKFetch];
    }];

    // Trigger a notification (with hilariously fake data)
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];

    // Wait for both fetches....
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Potential race here: we need to start this expectation before CKKS issues the fetch. With a 4s delay, this should be safe.

    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck1.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateProcessIncomingQueue];
    [self.reachabilityTracker setNetworkReachability:true];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateProcessIncomingQueue] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'processincomingqueue'");
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateProcessIncomingQueue];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];
    [self findGenericPassword: @"account2" expecting:errSecSuccess];
    [self findGenericPassword: @"account3" expecting:errSecSuccess];
}

- (void)testFetchOnRestartWithMoreComing {
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Cause a fetch to occur, so that the local CKKS instance is definitely up to date with what it has uploaded, so the next fetch will be entirely items
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"account1"]];
    FakeCKServerChangeToken* ck1 = ckzone.currentChangeToken;

    // Allow CKKS to fetch fully, then fake on-disk that it received MoreComing.
    // (It's very hard to tear down the retry logic in-process)
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    // Ensure that we wait for the IncomingQueueOperation
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateProcessIncomingQueue];

    // Trigger a notification (with hilariously fake data)
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateProcessIncomingQueue] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'processincomingqueue'");
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateProcessIncomingQueue];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];

    // Now, edit the on-disk CKSE
    [self.defaultCKKS halt];

    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        NSError* error = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:self.defaultCKKS.operationDependencies.contextID zoneName:self.keychainZoneID.zoneName error:&error];

        XCTAssertNil(error, "no error pulling ckse from database");
        XCTAssertNotNil(ckse, "received a ckse");

        ckse.moreRecordsInCloudKit = YES;
        [ckse saveToDatabase: &error];
        XCTAssertNil(error, "no error saving to database");
        return CKKSDatabaseTransactionCommit;
    }];

    // CKKS should, upon restart, kick off a fetch starting from the previous checkpoint
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"account3"]];

    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // The fetch on restart with more-coming should be QoS UserInitiated, as CKKS won't be sure if this is part of the initial download (e.g. resuming after a crash)
        XCTAssertEqual(frzco.qualityOfService, NSQualityOfServiceUserInitiated, "QoS should be user-initiated");

        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck1.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];

    [self.defaultCKKS beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self findGenericPassword: @"account0" expecting:errSecSuccess];
    [self findGenericPassword: @"account1" expecting:errSecSuccess];
    [self findGenericPassword: @"account2" expecting:errSecSuccess];
    [self findGenericPassword: @"account3" expecting:errSecSuccess];
}

- (void)testMoreComingAsFirstFetch {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [self.keychainZone addToZone: [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"account0"]];
    [self.keychainZone addToZone: [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"account1"]];
    FakeCKServerChangeToken* ck1 = ckzone.currentChangeToken;
    [self.keychainZone addToZone: [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"account2"]];
    [self.keychainZone addToZone: [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"account3"]];

    ckzone.limitFetchTo = ck1;

    self.silentFetchesAllowed = false;
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // As part of the initial download, we should boost QoS
        XCTAssertEqual(frzco.qualityOfService, NSQualityOfServiceUserInitiated, "QoS should be user-initiated");
        return YES;
    } runBeforeFinished:^{}];
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // As part of the initial download, we should boost QoS
        XCTAssertEqual(frzco.qualityOfService, NSQualityOfServiceUserInitiated, "QoS should be user-initiated");

        // Assert that the fetch is happening with the change token we paused at before
        FakeCKServerChangeToken* changeToken = [FakeCKServerChangeToken decodeCKServerChangeToken:frzco.configurationsByRecordZoneID[self.keychainZoneID].previousServerChangeToken];
        if(changeToken && [changeToken.token isEqual:ck1.token]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self findGenericPassword:@"account0" expecting:errSecSuccess];
    [self findGenericPassword:@"account1" expecting:errSecSuccess];
    [self findGenericPassword:@"account2" expecting:errSecSuccess];
    [self findGenericPassword:@"account3" expecting:errSecSuccess];
}

- (void)testRepeatedRetries {
    [self putFakeKeyHierarchyInCloudKit: self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    FakeCKZone* ckzone = self.zones[self.keychainZoneID];

    [ckzone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:1]}]];
    [ckzone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:1]}]];
    [ckzone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain code:CKErrorNetworkFailure userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:1]}]];

    // Call direct to set our own timeouts
    CKKSResultOperation* fetchOp = [self.defaultCKKS rpcFetchAndProcessIncomingQueue:nil
                                                                             because:CKKSFetchBecauseTesting
                                                                errorOnClassAFailure:false];

    [fetchOp timeout:45*NSEC_PER_SEC];
    [fetchOp waitUntilFinished];
    XCTAssertNil(fetchOp.error, @"Should have no error fetching");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}


@end

#endif // OCTAGON

