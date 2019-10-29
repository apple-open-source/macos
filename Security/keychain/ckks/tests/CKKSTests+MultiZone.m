
#if OCTAGON

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>
#import <notify.h>

#include <Security/SecItemPriv.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSManifest.h"

#import "keychain/ckks/tests/MockCloudKit.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include "keychain/SecureObjectSync/SOSAccountPriv.h"
#include "keychain/SecureObjectSync/SOSAccount.h"
#include "keychain/SecureObjectSync/SOSInternal.h"
#include "keychain/SecureObjectSync/SOSFullPeerInfo.h"
#pragma clang diagnostic pop

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#pragma clang diagnostic pop

@interface CloudKitKeychainSyncingMultiZoneTestsBase : CloudKitKeychainSyncingMockXCTest

@property CKRecordZoneID*      engramZoneID;
@property CKKSKeychainView*    engramView;
@property FakeCKZone*          engramZone;
@property (readonly) ZoneKeys* engramZoneKeys;

@property CKRecordZoneID*      manateeZoneID;
@property CKKSKeychainView*    manateeView;
@property FakeCKZone*          manateeZone;
@property (readonly) ZoneKeys* manateeZoneKeys;

@property CKRecordZoneID*      autoUnlockZoneID;
@property CKKSKeychainView*    autoUnlockView;
@property FakeCKZone*          autoUnlockZone;
@property (readonly) ZoneKeys* autoUnlockZoneKeys;

@property CKRecordZoneID*      healthZoneID;
@property CKKSKeychainView*    healthView;
@property FakeCKZone*          healthZone;
@property (readonly) ZoneKeys* healthZoneKeys;

@property CKRecordZoneID*      applepayZoneID;
@property CKKSKeychainView*    applepayView;
@property FakeCKZone*          applepayZone;
@property (readonly) ZoneKeys* applepayZoneKeys;

@property CKRecordZoneID*      homeZoneID;
@property CKKSKeychainView*    homeView;
@property FakeCKZone*          homeZone;
@property (readonly) ZoneKeys* homeZoneKeys;

@property CKRecordZoneID*      limitedZoneID;
@property CKKSKeychainView*    limitedView;
@property FakeCKZone*          limitedZone;
@property (readonly) ZoneKeys* limitedZoneKeys;

@end

@implementation CloudKitKeychainSyncingMultiZoneTestsBase
+ (void)setUp {
    SecCKKSEnable();
    SecCKKSResetSyncing();
    [super setUp];
}

- (void)setUp {
    SecCKKSSetSyncManifests(false);
    SecCKKSSetEnforceManifests(false);

    [super setUp];
    SecCKKSTestSetDisableSOS(false);

    // Wait for the ViewManager to be brought up
    XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:20*NSEC_PER_SEC], "No timeout waiting for SecCKKSInitialize");

    self.engramZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Engram" ownerName:CKCurrentUserDefaultName];
    self.engramZone = [[FakeCKZone alloc] initZone: self.engramZoneID];
    self.zones[self.engramZoneID] = self.engramZone;
    self.engramView = [[CKKSViewManager manager] findOrCreateView:@"Engram"];
    XCTAssertNotNil(self.engramView, "CKKSViewManager created the Engram view");
    [self.ckksViews addObject:self.engramView];
    [self.ckksZones addObject:self.engramZoneID];

    self.manateeZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Manatee" ownerName:CKCurrentUserDefaultName];
    self.manateeZone = [[FakeCKZone alloc] initZone: self.manateeZoneID];
    self.zones[self.manateeZoneID] = self.manateeZone;
    self.manateeView = [[CKKSViewManager manager] findOrCreateView:@"Manatee"];
    XCTAssertNotNil(self.manateeView, "CKKSViewManager created the Manatee view");
    [self.ckksViews addObject:self.manateeView];
    [self.ckksZones addObject:self.manateeZoneID];

    self.autoUnlockZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"AutoUnlock" ownerName:CKCurrentUserDefaultName];
    self.autoUnlockZone = [[FakeCKZone alloc] initZone: self.autoUnlockZoneID];
    self.zones[self.autoUnlockZoneID] = self.autoUnlockZone;
    self.autoUnlockView = [[CKKSViewManager manager] findOrCreateView:@"AutoUnlock"];
    XCTAssertNotNil(self.autoUnlockView, "CKKSViewManager created the AutoUnlock view");
    [self.ckksViews addObject:self.autoUnlockView];
    [self.ckksZones addObject:self.autoUnlockZoneID];

    self.healthZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Health" ownerName:CKCurrentUserDefaultName];
    self.healthZone = [[FakeCKZone alloc] initZone: self.healthZoneID];
    self.zones[self.healthZoneID] = self.healthZone;
    self.healthView = [[CKKSViewManager manager] findOrCreateView:@"Health"];
    XCTAssertNotNil(self.healthView, "CKKSViewManager created the Health view");
    [self.ckksViews addObject:self.healthView];
    [self.ckksZones addObject:self.healthZoneID];

    self.applepayZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"ApplePay" ownerName:CKCurrentUserDefaultName];
    self.applepayZone = [[FakeCKZone alloc] initZone: self.applepayZoneID];
    self.zones[self.applepayZoneID] = self.applepayZone;
    self.applepayView = [[CKKSViewManager manager] findOrCreateView:@"ApplePay"];
    XCTAssertNotNil(self.applepayView, "CKKSViewManager created the ApplePay view");
    [self.ckksViews addObject:self.applepayView];
    [self.ckksZones addObject:self.applepayZoneID];

    self.homeZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Home" ownerName:CKCurrentUserDefaultName];
    self.homeZone = [[FakeCKZone alloc] initZone: self.homeZoneID];
    self.zones[self.homeZoneID] = self.homeZone;
    self.homeView = [[CKKSViewManager manager] findOrCreateView:@"Home"];
    XCTAssertNotNil(self.homeView, "CKKSViewManager created the Home view");
    [self.ckksViews addObject:self.homeView];
    [self.ckksZones addObject:self.homeZoneID];

    self.limitedZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"LimitedPeersAllowed" ownerName:CKCurrentUserDefaultName];
    self.limitedZone = [[FakeCKZone alloc] initZone: self.limitedZoneID];
    self.zones[self.limitedZoneID] = self.limitedZone;
    self.limitedView = [[CKKSViewManager manager] findOrCreateView:@"LimitedPeersAllowed"];
    XCTAssertNotNil(self.limitedView, "should have a limited ckks view");
    XCTAssertNotNil(self.limitedView, "CKKSViewManager created the LimitedPeersAllowed view");
    [self.ckksViews addObject:self.limitedView];
    [self.ckksZones addObject:self.limitedZoneID];
}

+ (void)tearDown {
    SecCKKSTestSetDisableSOS(true);
    [super tearDown];
    SecCKKSResetSyncing();
}

- (void)tearDown {
    // If the test didn't already do this, allow each zone to spin up
    self.accountStatus = CKAccountStatusNoAccount;
    [self startCKKSSubsystem];

    [self.engramView halt];
    [self.engramView waitUntilAllOperationsAreFinished];
    self.engramView = nil;

    [self.manateeView halt];
    [self.manateeView waitUntilAllOperationsAreFinished];
    self.manateeView = nil;

    [self.autoUnlockView halt];
    [self.autoUnlockView waitUntilAllOperationsAreFinished];
    self.autoUnlockView = nil;

    [self.healthView halt];
    [self.healthView waitUntilAllOperationsAreFinished];
    self.healthView = nil;

    [self.applepayView halt];
    [self.applepayView waitUntilAllOperationsAreFinished];
    self.applepayView = nil;

    [self.homeView halt];
    [self.homeView waitUntilAllOperationsAreFinished];
    self.homeView = nil;

    [super tearDown];
}

- (ZoneKeys*)engramZoneKeys {
    return self.keys[self.engramZoneID];
}

- (ZoneKeys*)manateeZoneKeys {
    return self.keys[self.manateeZoneID];
}

- (void)saveFakeKeyHierarchiesToLocalDatabase {
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self createAndSaveFakeKeyHierarchy: zoneID];
    }
}

- (void)putFakeDeviceStatusesInCloudKit {
    [self putFakeDeviceStatusInCloudKit: self.engramZoneID];
    [self putFakeDeviceStatusInCloudKit: self.manateeZoneID];
    [self putFakeDeviceStatusInCloudKit: self.autoUnlockZoneID];
    [self putFakeDeviceStatusInCloudKit: self.healthZoneID];
    [self putFakeDeviceStatusInCloudKit: self.applepayZoneID];
    [self putFakeDeviceStatusInCloudKit: self.homeZoneID];
    [self putFakeDeviceStatusInCloudKit: self.limitedZoneID];
}

- (void)putFakeKeyHierachiesInCloudKit{
    [self putFakeKeyHierarchyInCloudKit: self.engramZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.manateeZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.autoUnlockZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.healthZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.applepayZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.homeZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.limitedZoneID];
}

- (void)saveTLKsToKeychain{
    [self saveTLKMaterialToKeychain:self.engramZoneID];
    [self saveTLKMaterialToKeychain:self.manateeZoneID];
    [self saveTLKMaterialToKeychain:self.autoUnlockZoneID];
    [self saveTLKMaterialToKeychain:self.healthZoneID];
    [self saveTLKMaterialToKeychain:self.applepayZoneID];
    [self saveTLKMaterialToKeychain:self.homeZoneID];
    [self saveTLKMaterialToKeychain:self.limitedZoneID];
}

- (void)deleteTLKMaterialsFromKeychain{
    [self deleteTLKMaterialFromKeychain: self.engramZoneID];
    [self deleteTLKMaterialFromKeychain: self.manateeZoneID];
    [self deleteTLKMaterialFromKeychain: self.autoUnlockZoneID];
    [self deleteTLKMaterialFromKeychain: self.healthZoneID];
    [self deleteTLKMaterialFromKeychain: self.applepayZoneID];
    [self deleteTLKMaterialFromKeychain: self.homeZoneID];
    [self deleteTLKMaterialFromKeychain:self.limitedZoneID];
}

- (void)waitForKeyHierarchyReadinesses {
    [self.manateeView waitForKeyHierarchyReadiness];
    [self.engramView waitForKeyHierarchyReadiness];
    [self.autoUnlockView waitForKeyHierarchyReadiness];
    [self.healthView waitForKeyHierarchyReadiness];
    [self.applepayView waitForKeyHierarchyReadiness];
    [self.homeView waitForKeyHierarchyReadiness];
    [self.limitedView waitForKeyHierarchyReadiness];
}

- (void)expectCKKSTLKSelfShareUploads {
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

}

@end

@interface CloudKitKeychainSyncingMultiZoneTests : CloudKitKeychainSyncingMultiZoneTestsBase
@end

@implementation CloudKitKeychainSyncingMultiZoneTests

- (void)testAllViewsMakeNewKeyHierarchies {
    // Test starts with nothing anywhere

    // Due to our new cross-zone fetch system, CKKS should only issue one fetch for all zones
    // Since the tests can sometimes be slow, slow down the fetcher to normal speed
    [self.injectedManager.zoneChangeFetcher.fetchScheduler changeDelays:2*NSEC_PER_SEC continuingDelay:30*NSEC_PER_SEC];
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    [self startCKKSSubsystem];
    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should enter 'ready' for view %@", view);
    }
}

- (void)testAllViewsAcceptExistingKeyHierarchies {
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self putFakeKeyHierarchyInCloudKit:zoneID];
        [self saveTLKMaterialToKeychain:zoneID];
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    [self.injectedManager.zoneChangeFetcher.fetchScheduler changeDelays:2*NSEC_PER_SEC continuingDelay:30*NSEC_PER_SEC];
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    [self startCKKSSubsystem];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    for(CKKSKeychainView* view in self.ckksViews) {
        XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should enter 'ready' for view %@", view);
    }
}

- (void)testAddEngramManateeItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* engramChanged = [self expectChangeForView:self.engramZoneID.zoneName];
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];
    XCTestExpectation* manateeChanged = [self expectChangeForView:self.manateeZoneID.zoneName];

    // We expect a single record to be uploaded to the engram view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.engramZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-engram" viewHint:(NSString*) kSecAttrViewHintEngram];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[engramChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:1];

    pcsChanged = [self expectChangeForView:@"PCS"];

    // We expect a single record to be uploaded to the manatee view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.manateeZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-manatee" viewHint:(NSString*) kSecAttrViewHintManatee];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[manateeChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:1];
}

- (void)testAddAutoUnlockItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* autoUnlockChanged = [self expectChangeForView:self.autoUnlockZoneID.zoneName];
    // AutoUnlock is NOT is PCS view, so it should not send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];
    pcsChanged.inverted = YES;

    // We expect a single record to be uploaded to the AutoUnlock view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.autoUnlockZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintAutoUnlock];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[autoUnlockChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

- (void)testAddHealthItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* healthChanged = [self expectChangeForView:self.healthZoneID.zoneName];
    // Health is NOT is PCS view, so it should not send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];
    pcsChanged.inverted = YES;

    // We expect a single record to be uploaded to the Health view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.healthZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintHealth];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[healthChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

- (void)testAddApplePayItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* applepayChanged = [self expectChangeForView:self.applepayZoneID.zoneName];
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];

    // We expect a single record to be uploaded to the ApplePay view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.applepayZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintApplePay];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[applepayChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

- (void)testAddHomeItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* homeChanged = [self expectChangeForView:self.homeZoneID.zoneName];
    // Home is a now PCS view, so it should send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];

    // We expect a single record to be uploaded to the ApplePay view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.homeZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintHome];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[homeChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

- (void)testAddLimitedPeersAllowedItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* limitedChanged = [self expectChangeForView:self.limitedZoneID.zoneName];
    // LimitedPeersAllowed is a PCS view, so it should send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];

    // We expect a single record to be uploaded to the LimitedPeersOkay view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.limitedZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-limited-peers" viewHint:(NSString*) kSecAttrViewHintLimitedPeersAllowed];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[limitedChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

- (void)testAddOtherViewHintItem {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // We expect no uploads to CKKS.
    [self addGenericPassword: @"data" account: @"account-delete-me-no-viewhint"];
    [self addGenericPassword: @"data" account: @"account-delete-me-password" viewHint:(NSString*) kSOSViewAutofillPasswords];

    sleep(1);
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testReceiveItemInView {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.
    [self startCKKSSubsystem];

    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    [self waitForKeyHierarchyReadinesses];

    [self findGenericPassword:@"account-delete-me" expecting:errSecItemNotFound];

    CKRecord* ckr = [self createFakeRecord:self.engramZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.engramZone addToZone: ckr];

    XCTestExpectation* engramChanged = [self expectChangeForView:self.engramZoneID.zoneName];
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];

    // Trigger a notification (with hilariously fake data)
    [self.engramView notifyZoneChange:nil];

    [self.engramView waitForFetchAndIncomingQueueProcessing];
    [self findGenericPassword:@"account-delete-me" expecting:errSecSuccess];

    [self waitForExpectations:@[engramChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:1];
}

- (void)testRecoverFromCloudKitOldChangeTokenInKeyHierarchyFetch {
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];


    [self expectCKKSTLKSelfShareUploads];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self waitForKeyHierarchyReadinesses];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me" viewHint:(id)kSecAttrViewHintManatee];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Delete all old database states, to destroy the change tag validity
    [self.manateeZone.pastDatabases removeAllObjects];

    // We expect a total local flush and refetch
    self.silentFetchesAllowed = false;
    [self expectCKFetch]; // one to fail with a CKErrorChangeTokenExpired error
    [self expectCKFetch]; // and one to succeed

    [self.manateeView dispatchSyncWithAccountKeys: ^bool {
        [self.manateeView _onqueueKeyStateMachineRequestFetch];
        return true;
    }];

    XCTAssertEqual(0, [self.manateeView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter 'ready'");

    [self.manateeView waitForFetchAndIncomingQueueProcessing];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And check that a new upload happens just fine.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.manateeZoneID checkItem: [self checkClassABlock:self.manateeZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:(id)kSecAttrViewHintManatee
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testResetAllCloudKitZones {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];
    [self expectCKKSTLKSelfShareUploads];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self waitForKeyHierarchyReadinesses];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me" viewHint:(id)kSecAttrViewHintManatee];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // During the reset, Octagon will upload the key hierarchy, and then CKKS will upload the class C item

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // CKKS should issue exactly one deletion for all of these
    self.silentZoneDeletesAllowed = true;

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-all-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        secnotice("ckks", "Received a resetCloudKit callback");
        [resetExpectation fulfill];
    }];

    // Sneak in and perform Octagon's duties
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem: [self checkClassABlock:self.manateeZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:(id)kSecAttrViewHintManatee
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testResetAllCloudKitZonesWithPartialZonesMissing {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];
    [self expectCKKSTLKSelfShareUploads];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self waitForKeyHierarchyReadinesses];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me" viewHint:(id)kSecAttrViewHintManatee];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    self.zones[self.manateeZoneID] = nil;
    self.keys[self.manateeZoneID] = nil;
    self.zones[self.applepayZoneID] = nil;
    self.keys[self.applepayZoneID] = nil;

    // During the reset, Octagon will upload the key hierarchy, and then CKKS will upload the class C item
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // CKKS should issue exactly one deletion for all of these
    self.silentZoneDeletesAllowed = true;

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-all-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        secnotice("ckks", "Received a resetCloudKit callback");
        [resetExpectation fulfill];
    }];

    // Sneak in and perform Octagon's duties
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem: [self checkClassABlock:self.manateeZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:(id)kSecAttrViewHintManatee
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testResetMultiCloudKitZoneCloudKitRejects {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];
    [self expectCKKSTLKSelfShareUploads];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self waitForKeyHierarchyReadinesses];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me" viewHint:(id)kSecAttrViewHintManatee];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    self.nextModifyRecordZonesError = [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                       code:CKErrorZoneBusy
                                                                   userInfo:@{
                                                                              CKErrorRetryAfterKey: @(0.2),
                                                                              NSUnderlyingErrorKey: [[CKPrettyError alloc] initWithDomain:CKErrorDomain
                                                                                                                                     code:2029
                                                                                                                                 userInfo:nil],
                                                                              }];
    self.silentZoneDeletesAllowed = true;

    // During the reset, Octagon will upload the key hierarchy, and then CKKS will upload the class C item
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.manateeZoneID checkItem:[self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        secnotice("ckks", "Received a resetCloudKit callback");
        [resetExpectation fulfill];
    }];

    // Sneak in and perform Octagon's duties
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertNil(self.nextModifyRecordZonesError, "zone modification error should have been cleared");
}

- (void)testMultiZoneDeviceStateUploadGood {
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];
    [self expectCKKSTLKSelfShareUploads];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    [self waitForKeyHierarchyReadinesses];

    for(CKKSKeychainView* view in self.ckksViews) {
        [self expectCKModifyRecords:@{SecCKRecordDeviceStateType: [NSNumber numberWithInt:1]}
            deletedRecordTypeCounts:nil
                             zoneID:view.zoneID
                checkModifiedRecord:nil
               runAfterModification:nil];
    }

    [self.injectedManager xpc24HrNotification];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testMultiZoneResync {
    // Set up
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];
    [self expectCKKSTLKSelfShareUploads];

    // Put sample data in zones, and save off a change token for later fetch shenanigans
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D00" withAccount:@"manatee0"]];
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D01" withAccount:@"manatee1"]];
    CKServerChangeToken* manateeChangeToken1 = self.manateeZone.currentChangeToken;
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D02" withAccount:@"manatee2"]];
    [self.manateeZone addToZone:[self createFakeRecord:self.manateeZoneID recordName:@"7B598D31-0000-0000-0000-5A507ACB2D03" withAccount:@"manatee3"]];

    [self.healthZone addToZone:[self createFakeRecord:self.healthZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D00" withAccount:@"health0"]];
    [self.healthZone addToZone:[self createFakeRecord:self.healthZoneID recordName:@"7B598D31-0000-0000-FFFF-5A507ACB2D01" withAccount:@"health1"]];

    [self startCKKSSubsystem];
    [self waitForKeyHierarchyReadinesses];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.manateeView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self.healthView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    [self findGenericPassword:@"manatee0" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee1" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee2" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee3" expecting:errSecSuccess];
    [self findGenericPassword:@"health0" expecting:errSecSuccess];
    [self findGenericPassword:@"health1" expecting:errSecSuccess];

    // Now, we resync. But, the manatee zone comes down in two fetches
    self.silentFetchesAllowed = false;
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is a refetch
        CKServerChangeToken* changeToken = frzco.configurationsByRecordZoneID[self.manateeZoneID].previousServerChangeToken;
        if(changeToken == nil) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];
    [self expectCKFetchWithFilter:^BOOL(FakeCKFetchRecordZoneChangesOperation * _Nonnull frzco) {
        // Assert that the fetch is happening with the change token we paused at before
        CKServerChangeToken* changeToken = frzco.configurationsByRecordZoneID[self.manateeZoneID].previousServerChangeToken;
        if(changeToken && [changeToken isEqual:manateeChangeToken1]) {
            return YES;
        } else {
            return NO;
        }
    } runBeforeFinished:^{}];

    self.manateeZone.limitFetchTo = manateeChangeToken1;

    // Attempt to trigger simultaneous key state resyncs. This is a horrible hack...
    [self.manateeView dispatchSyncWithAccountKeys:^bool {
        self.manateeView.keyStateFullRefetchRequested = YES;
        [self.manateeView _onqueueAdvanceKeyStateMachineToState:nil withError:nil];
        return true;
    }];
    [self.healthView dispatchSyncWithAccountKeys:^bool {
        self.healthView.keyStateFullRefetchRequested = YES;
        [self.healthView _onqueueAdvanceKeyStateMachineToState:nil withError:nil];
        return true;
    }];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self.manateeView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];
    [self.healthView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    // And all items should still exist
    [self findGenericPassword:@"manatee0" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee1" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee2" expecting:errSecSuccess];
    [self findGenericPassword:@"manatee3" expecting:errSecSuccess];
    [self findGenericPassword:@"health0" expecting:errSecSuccess];
    [self findGenericPassword:@"health1" expecting:errSecSuccess];
}

@end

#endif // OCTAGON

