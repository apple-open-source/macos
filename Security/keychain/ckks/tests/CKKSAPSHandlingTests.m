/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFDigestOperation.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSAPSReceiver.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSTLKShare.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ot/OTDefines.h"

#import "keychain/ckks/tests/CKKSAPSReceiverTests.h"

@interface CKKSAPSHandlingTests : CloudKitKeychainSyncingTestsBase
@property CKRecordZoneID*      manateeZoneID;
@property CKKSKeychainView*    manateeView;
@property FakeCKZone*          manateeZone;
@property (readonly) ZoneKeys* manateeZoneKeys;
@end


@implementation CKKSAPSHandlingTests
// We really just want two views here
- (NSSet*)managedViewList {
    return [NSSet setWithObjects:@"keychain", @"Manatee", nil];
}

- (void)setUp {
    [super setUp];

    // Wait for the ViewManager to be brought up
    XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:20*NSEC_PER_SEC], "No timeout waiting for SecCKKSInitialize");

    self.manateeZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Manatee" ownerName:CKCurrentUserDefaultName];
    [self.ckksZones addObject:self.manateeZoneID];
    self.manateeZone = [[FakeCKZone alloc] initZone: self.manateeZoneID];
    self.zones[self.manateeZoneID] = self.manateeZone;
    self.manateeView = [[CKKSViewManager manager] findView:@"Manatee"];
    [self.ckksViews addObject:self.manateeView];
    XCTAssertNotNil(self.manateeView, "CKKSViewManager created the Manatee view");
}

- (void)tearDown {
    // If the test didn't already do this, allow each zone to spin up
    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];
    
    [self.manateeView halt];
    [self.manateeView waitUntilAllOperationsAreFinished];
    self.manateeView = nil;

    [super tearDown];
}

- (void)testSendPushMetricUponRequest {
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self putFakeKeyHierarchyInCloudKit:zoneID];
        [self saveTLKMaterialToKeychain:zoneID];
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    [self startCKKSSubsystem];

    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should enter 'ready' for view %@", view);
    }

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    CKRecord* keychainRecord = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"keychain-view"];
    [self.keychainZone addToZone: keychainRecord];

    // Manatee gets one too!
    CKRecord* manateeRecord = [self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-F9C5-481E-98AC-000000000000" withAccount:@"manatee-view"];
    [self.manateeZone addToZone:manateeRecord];

    // Trigger a notification just for keychain zone. Both keychain and Manatee should process their incoming queues, and receive their items.
    CKKSResultOperation* keychainProcessOp = [self.keychainView resultsOfNextProcessIncomingQueueOperation];
    XCTAssertNotNil(keychainProcessOp, "Should have gotten a promise operation from Keychain");
    CKKSResultOperation* manateeProcessOp = [self.manateeView resultsOfNextProcessIncomingQueueOperation];
    XCTAssertNotNil(manateeProcessOp, "Should have gotten a promise operation from Manatee");

    // But if something goes wrong, don't block the whole test. Only way to do that is to make more operations, since there's no guarantee that the process ops above will ever be added
    // to a queue (and thus become 'finished')
    CKKSResultOperation* keychainProcessTimeoutOp = [CKKSResultOperation named:@"keychain-timeout" withBlock:^{}];
    [keychainProcessTimeoutOp timeout:20*NSEC_PER_SEC];
    [keychainProcessTimeoutOp addSuccessDependency:keychainProcessOp];
    [self.operationQueue addOperation:keychainProcessTimeoutOp];

    CKKSResultOperation* manateeProcessTimeoutOp = [CKKSResultOperation named:@"manatee-timeout" withBlock:^{}];
    [manateeProcessTimeoutOp timeout:20*NSEC_PER_SEC];
    [manateeProcessTimeoutOp addSuccessDependency:manateeProcessOp];
    [self.operationQueue addOperation:manateeProcessTimeoutOp];

    APSIncomingMessage* apsMessage = [CKKSAPSReceiverTests messageForZoneID:self.keychainZoneID];
    NSUUID* nsuuid = [NSUUID UUID];
    uuid_t uuid = {0};
    [nsuuid getUUIDBytes:(unsigned char*)&uuid];
    NSData* uuidData = [NSData dataWithBytes:&uuid length:sizeof(uuid)];

    apsMessage.tracingUUID = uuidData;
    apsMessage.tracingEnabled = YES;

    // Inject a message at the APS layer
    // Because we can only make APS receivers once iCloud tells us the push environment after sign-in, we can't use our normal injection strategy, and fell back on global state.
    CKKSAPSReceiver* apsReceiver = [CKKSAPSReceiver receiverForEnvironment:self.apsEnvironment
                                                         namedDelegatePort:SecCKKSAPSNamedPort
                                                        apsConnectionClass:[FakeAPSConnection class]];
    XCTAssertNotNil(apsReceiver, "Should have gotten an APS receiver");

    // Also, CKKS should handle this in one single fetch
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    // Expect two metric pushes, one from receiving the push and one from after we finish the fetch
    // AFAICT there's no way to introspect a metric object to ensure we did it right
    OCMExpect([self.mockContainerExpectations submitEventMetric:[OCMArg any]]);
    OCMExpect([self.mockContainerExpectations submitEventMetric:[OCMArg any]]);

    // Launch!
    [apsReceiver connection:nil didReceiveIncomingMessage:apsMessage];

    OCMVerifyAllWithDelay(self.mockContainerExpectations, 16);

    // Now, wait for both views to run their processing
    [keychainProcessTimeoutOp waitUntilFinished];
    XCTAssertNil(keychainProcessTimeoutOp.error, "Shouldn't have been any error processing incoming queue (keychain)");

    [manateeProcessTimeoutOp waitUntilFinished];
    XCTAssertNil(manateeProcessTimeoutOp.error, "Shouldn't have been any error processing incoming queue (manatee)");

    [self findGenericPassword:@"keychain-view" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee-view" expecting:errSecSuccess];
}

- (void)testDoNotSendPushMetricWithoutRequest {
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self putFakeKeyHierarchyInCloudKit:zoneID];
        [self saveTLKMaterialToKeychain:zoneID];
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    [self startCKKSSubsystem];

    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should enter 'ready' for view %@", view);
    }

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    CKRecord* keychainRecord = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"keychain-view"];
    [self.keychainZone addToZone: keychainRecord];

     // Trigger a notification just for keychain zone. Both keychain and Manatee should process their incoming queues, and receive their items.
    CKKSResultOperation* keychainProcessOp = [self.keychainView resultsOfNextProcessIncomingQueueOperation];
    XCTAssertNotNil(keychainProcessOp, "Should have gotten a promise operation from Keychain");

    // But if something goes wrong, don't block the whole test. Only way to do that is to make more operations, since there's no guarantee that the process ops above will ever be added
    // to a queue (and thus become 'finished')
    CKKSResultOperation* keychainProcessTimeoutOp = [CKKSResultOperation named:@"keychain-timeout" withBlock:^{}];
    [keychainProcessTimeoutOp timeout:20*NSEC_PER_SEC];
    [keychainProcessTimeoutOp addSuccessDependency:keychainProcessOp];
    [self.operationQueue addOperation:keychainProcessTimeoutOp];

    APSIncomingMessage* apsMessage = [CKKSAPSReceiverTests messageForZoneID:self.keychainZoneID];
    NSUUID* nsuuid = [NSUUID UUID];
    uuid_t uuid = {0};
    [nsuuid getUUIDBytes:(unsigned char*)&uuid];
    NSData* uuidData = [NSData dataWithBytes:&uuid length:sizeof(uuid)];

    apsMessage.tracingUUID = uuidData;
    apsMessage.tracingEnabled = NO;

    // Inject a message at the APS layer
    // Because we can only make APS receivers once iCloud tells us the push environment after sign-in, we can't use our normal injection strategy, and fell back on global state.
    CKKSAPSReceiver* apsReceiver = [CKKSAPSReceiver receiverForEnvironment:self.apsEnvironment
                                                         namedDelegatePort:SecCKKSAPSNamedPort
                                                        apsConnectionClass:[FakeAPSConnection class]];
    XCTAssertNotNil(apsReceiver, "Should have gotten an APS receiver");

    // Also, CKKS should handle this in one single fetch
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    // Any metric push is verboten
    OCMReject([self.mockContainerExpectations submitEventMetric:[OCMArg any]]);

    // Launch!
    [apsReceiver connection:nil didReceiveIncomingMessage:apsMessage];
    OCMVerifyAllWithDelay(self.mockContainerExpectations, 16);

    // Now, wait for both views to run their processing
    [keychainProcessTimeoutOp waitUntilFinished];
    XCTAssertNil(keychainProcessTimeoutOp.error, "Shouldn't have been any error processing incoming queue (keychain)");

    [self findGenericPassword:@"keychain-view" expecting:errSecSuccess];
}

@end

#endif
