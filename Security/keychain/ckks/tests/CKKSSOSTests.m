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
#include <Security/SecureObjectSync/SOSAccountPriv.h>
#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <Security/SecureObjectSync/SOSFullPeerInfo.h>
#pragma clang diagnostic pop

#include <Security/SecKey.h>
#include <Security/SecKeyPriv.h>
#pragma clang diagnostic pop

@interface CloudKitKeychainSyncingSOSIntegrationTests : CloudKitKeychainSyncingMockXCTest

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

@end

@implementation CloudKitKeychainSyncingSOSIntegrationTests
+ (void)setUp {
    SecCKKSEnable();
    SecCKKSResetSyncing();
    [super setUp];
}

- (void)setUp {
    // No manifests.
    (void)[CKKSManifest shouldSyncManifests]; // initialize.
    SecCKKSSetSyncManifests(false);
    SecCKKSSetEnforceManifests(false);

    [super setUp];
    SecCKKSTestSetDisableSOS(false);

    // Wait for the ViewManager to be brought up
    XCTAssertEqual(0, [self.injectedManager.completedSecCKKSInitialize wait:4*NSEC_PER_SEC], "No timeout waiting for SecCKKSInitialize");

    self.engramZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Engram" ownerName:CKCurrentUserDefaultName];
    self.engramZone = [[FakeCKZone alloc] initZone: self.engramZoneID];
    self.zones[self.engramZoneID] = self.engramZone;
    self.engramView = [[CKKSViewManager manager] findView:@"Engram"];
    XCTAssertNotNil(self.engramView, "CKKSViewManager created the Engram view");
    [self.ckksZones addObject:self.engramZoneID];

    self.manateeZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Manatee" ownerName:CKCurrentUserDefaultName];
    self.manateeZone = [[FakeCKZone alloc] initZone: self.manateeZoneID];
    self.zones[self.manateeZoneID] = self.manateeZone;
    self.manateeView = [[CKKSViewManager manager] findView:@"Manatee"];
    XCTAssertNotNil(self.manateeView, "CKKSViewManager created the Manatee view");
    [self.ckksZones addObject:self.manateeZoneID];

    self.autoUnlockZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"AutoUnlock" ownerName:CKCurrentUserDefaultName];
    self.autoUnlockZone = [[FakeCKZone alloc] initZone: self.autoUnlockZoneID];
    self.zones[self.autoUnlockZoneID] = self.autoUnlockZone;
    self.autoUnlockView = [[CKKSViewManager manager] findView:@"AutoUnlock"];
    XCTAssertNotNil(self.autoUnlockView, "CKKSViewManager created the AutoUnlock view");
    [self.ckksZones addObject:self.autoUnlockZoneID];

    self.healthZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"Health" ownerName:CKCurrentUserDefaultName];
    self.healthZone = [[FakeCKZone alloc] initZone: self.healthZoneID];
    self.zones[self.healthZoneID] = self.healthZone;
    self.healthView = [[CKKSViewManager manager] findView:@"Health"];
    XCTAssertNotNil(self.healthView, "CKKSViewManager created the Health view");
    [self.ckksZones addObject:self.healthZoneID];

    self.applepayZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"ApplePay" ownerName:CKCurrentUserDefaultName];
    self.applepayZone = [[FakeCKZone alloc] initZone: self.healthZoneID];
    self.zones[self.applepayZoneID] = self.applepayZone;
    self.applepayView = [[CKKSViewManager manager] findView:@"ApplePay"];
    XCTAssertNotNil(self.applepayView, "CKKSViewManager created the ApplePay view");
    [self.ckksZones addObject:self.applepayZoneID];
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

    [super tearDown];
}

- (ZoneKeys*)engramZoneKeys {
    return self.keys[self.engramZoneID];
}

- (ZoneKeys*)manateeZoneKeys {
    return self.keys[self.manateeZoneID];
}

-(void)saveFakeKeyHierarchiesToLocalDatabase {
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self createAndSaveFakeKeyHierarchy: zoneID];
    }
}

-(void)testAddEngramManateeItems {
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

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[manateeChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:1];
}

-(void)testAddAutoUnlockItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* autoUnlockChanged = [self expectChangeForView:self.autoUnlockZoneID.zoneName];
    // AutoUnlock is NOT is PCS view, so it should not send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];
    pcsChanged.inverted = YES;

    // We expect a single record to be uploaded to the AutoUnlock view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.autoUnlockZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintAutoUnlock];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[autoUnlockChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

-(void)testAddHealthItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* healthChanged = [self expectChangeForView:self.healthZoneID.zoneName];
    // Health is NOT is PCS view, so it should not send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];
    pcsChanged.inverted = YES;

    // We expect a single record to be uploaded to the Health view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.healthZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintHealth];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[healthChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}

-(void)testAddApplePayItems {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    XCTestExpectation* applepayChanged = [self expectChangeForView:self.applepayZoneID.zoneName];
    // ApplePay is NOT is PCS view, so it should not send the fake 'PCS' view notification
    XCTestExpectation* pcsChanged = [self expectChangeForView:@"PCS"];
    pcsChanged.inverted = YES;

    // We expect a single record to be uploaded to the ApplePay view.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.applepayZoneID];
    [self addGenericPassword: @"data" account: @"account-delete-me-autounlock" viewHint:(NSString*) kSecAttrViewHintApplePay];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[applepayChanged] timeout:1];
    [self waitForExpectations:@[pcsChanged] timeout:0.2];
}


-(void)testAddOtherViewHintItem {
    [self saveFakeKeyHierarchiesToLocalDatabase]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // We expect no uploads to CKKS.
    [self addGenericPassword: @"data" account: @"account-delete-me-no-viewhint"];
    [self addGenericPassword: @"data" account: @"account-delete-me-password" viewHint:(NSString*) kSOSViewAutofillPasswords];

    sleep(1);
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
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

- (void)testFindManateePiggyTLKs {
    [self saveFakeKeyHierarchyToLocalDatabase:self.manateeZoneID];
    [self saveTLKMaterialToKeychain:self.manateeZoneID];

    NSDictionary* piggyTLKs = [self SOSPiggyBackCopyFromKeychain];

    [self deleteTLKMaterialFromKeychain:self.manateeZoneID];

    [self SOSPiggyBackAddToKeychain:piggyTLKs];

    NSError* error = nil;
    [self.manateeZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading tlk from piggy contents");
}

- (void)testFindPiggyTLKs {
    [self putFakeKeyHierachiesInCloudKit];
    [self putFakeDeviceStatusesInCloudKit];
    [self saveTLKsToKeychain];

    NSDictionary* piggyTLKs = [self SOSPiggyBackCopyFromKeychain];

    [self deleteTLKMaterialsFromKeychain];

    [self SOSPiggyBackAddToKeychain:piggyTLKs];

    NSError* error = nil;
    [self.manateeZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading manatee tlk from piggy contents");

    [self.engramZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading engram tlk from piggy contents");

    [self.autoUnlockZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading AutoUnlock tlk from piggy contents");

    [self.healthZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading Health tlk from piggy contents");

    [self.applepayZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading ApplePay tlk from piggy contents");
}

-(NSString*)fileForStorage
{
    static dispatch_once_t onceToken;
    static NSString *tempPath = NULL;
    dispatch_once(&onceToken, ^{
        tempPath = [[[[NSFileManager defaultManager] temporaryDirectory] URLByAppendingPathComponent:@"PiggyPacket"] path];

    });
    return tempPath;
}

-(void)testPiggybackingData{
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];

    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }
    [self startCKKSSubsystem];

    [self waitForKeyHierarchyReadinesses];

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    /*
     * Pull data from keychain and view manager
     */

    NSDictionary* piggydata = [self SOSPiggyBackCopyFromKeychain];
    NSArray<NSData *>* icloudidentities = piggydata[@"idents"];
    NSArray<NSDictionary *>* tlks = piggydata[@"tlk"];

    XCTAssertEqual([tlks count], [[self.injectedManager viewList] count], "TLKs not same as views");

    XCTAssertNotNil(tlks, "tlks not set");
    XCTAssertNotEqual([tlks count], (NSUInteger)0, "0 tlks");
    XCTAssertNotNil(icloudidentities, "idents not set");
    XCTAssertNotEqual([icloudidentities count], (NSUInteger)0, "0 icloudidentities");

    NSData *initial = SOSPiggyCreateInitialSyncData(icloudidentities, tlks);

    XCTAssertNotNil(initial, "Initial not set");
    BOOL writeStatus = [initial writeToFile:[self fileForStorage] options:NSDataWritingAtomic error: nil];
    XCTAssertTrue(writeStatus, "had trouble writing to disk");
    XCTAssertNotEqual((int)[initial length], 0, "initial sync data is greater than 0");

    /*
     * Check that they make it accross
     */

    const uint8_t* der = [initial bytes];
    const uint8_t *der_end = der + [initial length];

    NSDictionary *result = SOSPiggyCopyInitialSyncData(&der, der_end);
    XCTAssertNotNil(result, "Initial not set");
    NSArray *copiedTLKs = result[@"tlks"];
    XCTAssertNotNil(copiedTLKs, "tlks not set");
    XCTAssertEqual([copiedTLKs count], [tlks count], "tlks count not same");

    NSArray *copiediCloudidentities = result[@"idents"];
    XCTAssertNotNil(copiediCloudidentities, "idents not set");
    XCTAssertEqual([copiediCloudidentities count], [icloudidentities count], "ident count not same");
}

-(void)testVerifyTLKSorting {
    char key[32*2] = {0};
    NSArray<NSDictionary *> *tlks = @[
        @{
            @"acct" : @"11111111",
            @"srvr" : @"Manatee",
            @"v_Data" : [NSData dataWithBytes:key length:sizeof(key)],
            @"auth" : @YES,
        },
        @{
            @"acct" : @"55555555",
            @"srvr" : @"Health",
            @"v_Data" : [NSData dataWithBytes:key length:sizeof(key)],
        },
        @{
            @"acct" : @"22222222",
            @"srvr" : @"Engram",
            @"v_Data" : [NSData dataWithBytes:key length:sizeof(key)],
            @"auth" : @YES,
        },
        @{
            @"acct" : @"44444444",
            @"srvr" : @"Manatee",
            @"v_Data" : [NSData dataWithBytes:key length:sizeof(key)],
        },
        @{
            @"acct" : @"33333333",
            @"srvr" : @"Health",
            @"v_Data" : [NSData dataWithBytes:key length:sizeof(key)],
            @"auth" : @YES,
        },
    ];

    NSArray<NSDictionary *>* sortedTLKs = SOSAccountSortTLKS(tlks);
    XCTAssertNotNil(sortedTLKs, "sortedTLKs not set");

    NSArray<NSString *> *expectedOrder = @[ @"11111111", @"22222222", @"33333333", @"44444444", @"55555555"];
    [sortedTLKs enumerateObjectsUsingBlock:^(NSDictionary *tlk, NSUInteger idx, BOOL * _Nonnull stop) {
        NSString *uuid = tlk[@"acct"];
        XCTAssertEqualObjects(uuid, expectedOrder[idx], "wrong order");
    }];
}


- (void)testAcceptExistingPiggyKeyHierarchy {
    // Test starts with no keys in CKKS database, but one in our fake CloudKit.
    // Test also begins with the TLK having arrived in the local keychain (via SOS)
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];
    NSDictionary* piggyTLKS = [self SOSPiggyBackCopyFromKeychain];
    [self SOSPiggyBackAddToKeychain:piggyTLKS];
    [self deleteTLKMaterialsFromKeychain];

    // The CKKS subsystem should write a TLK Share for each view
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    [self.manateeView waitForKeyHierarchyReadiness];
    
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    
    // Verify that there are three local keys, and three local current key records
    __weak __typeof(self) weakSelf = self;
    [self.manateeView dispatchSync: ^bool{
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");
        
        NSError* error = nil;
        
        NSArray<CKKSKey*>* keys = [CKKSKey localKeys:strongSelf.manateeZoneID error:&error];
        XCTAssertNil(error, "no error fetching keys");
        XCTAssertEqual(keys.count, 3u, "Three keys in local database");
        
        NSArray<CKKSCurrentKeyPointer*>* currentkeys = [CKKSCurrentKeyPointer all:strongSelf.manateeZoneID error:&error];
        XCTAssertNil(error, "no error fetching current keys");
        XCTAssertEqual(currentkeys.count, 3u, "Three current key pointers in local database");

        // Ensure that the manatee syncable TLK is created from a piggy
        NSDictionary* query = @{
                                (id)kSecClass : (id)kSecClassInternetPassword,
                                (id)kSecAttrNoLegacy : @YES,
                                (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                (id)kSecAttrDescription: SecCKKSKeyClassTLK,
                                (id)kSecAttrAccount: strongSelf.manateeZoneKeys.tlk.uuid,
                                (id)kSecAttrServer: strongSelf.manateeZoneID.zoneName,
                                (id)kSecAttrSynchronizable: @YES,
                                (id)kSecReturnAttributes: @YES,
                                (id)kSecReturnData: @YES,
                                };
        CFTypeRef result = nil;
        XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)query, &result), "Found a syncable TLK");
        XCTAssertNotNil((__bridge id) result, "Received a result from SecItemCopyMatching");
        CFReleaseNull(result);
        
        return false;
    }];
}

- (void)putFakeDeviceStatusesInCloudKit {
    [self putFakeDeviceStatusInCloudKit: self.engramZoneID];
    [self putFakeDeviceStatusInCloudKit: self.manateeZoneID];
    [self putFakeDeviceStatusInCloudKit: self.autoUnlockZoneID];
    [self putFakeDeviceStatusInCloudKit: self.healthZoneID];
    [self putFakeDeviceStatusInCloudKit: self.applepayZoneID];
}

-(void)putFakeKeyHierachiesInCloudKit{
    [self putFakeKeyHierarchyInCloudKit: self.engramZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.manateeZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.autoUnlockZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.healthZoneID];
    [self putFakeKeyHierarchyInCloudKit: self.applepayZoneID];
}
-(void)saveTLKsToKeychain{
    [self saveTLKMaterialToKeychain:self.engramZoneID];
    [self saveTLKMaterialToKeychain:self.manateeZoneID];
    [self saveTLKMaterialToKeychain:self.autoUnlockZoneID];
    [self saveTLKMaterialToKeychain:self.healthZoneID];
    [self saveTLKMaterialToKeychain:self.applepayZoneID];
}
-(void)deleteTLKMaterialsFromKeychain{
    [self deleteTLKMaterialFromKeychain: self.engramZoneID];
    [self deleteTLKMaterialFromKeychain: self.manateeZoneID];
    [self deleteTLKMaterialFromKeychain: self.autoUnlockZoneID];
    [self deleteTLKMaterialFromKeychain: self.healthZoneID];
    [self deleteTLKMaterialFromKeychain: self.applepayZoneID];
}

-(void)waitForKeyHierarchyReadinesses {
    [self.manateeView waitForKeyHierarchyReadiness];
    [self.engramView waitForKeyHierarchyReadiness];
    [self.autoUnlockView waitForKeyHierarchyReadiness];
    [self.healthView waitForKeyHierarchyReadiness];
    [self.applepayView waitForKeyHierarchyReadiness];
}

-(void)testAcceptExistingAndUsePiggyKeyHierarchy {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierachiesInCloudKit];
    [self putFakeDeviceStatusesInCloudKit];
    [self saveTLKsToKeychain];
    NSDictionary* piggyData = [self SOSPiggyBackCopyFromKeychain];
    [self deleteTLKMaterialsFromKeychain];
    
    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];
    
    // The CKKS subsystem should not try to write anything to the CloudKit database.
    XCTAssertEqual(0, [self.manateeView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:400*NSEC_PER_SEC], "CKKS entered waitfortlk");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    
    // Now, save the TLKs to the keychain (to simulate them coming in later via piggybacking).
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    [self SOSPiggyBackAddToKeychain:piggyData];
    [self waitForKeyHierarchyReadinesses];
    
    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.manateeZoneID checkItem: [self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-manatee" viewHint:(id)kSecAttrViewHintManatee];
    
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.manateeZoneID checkItem: [self checkClassABlock:self.manateeZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:(id)kSecAttrViewHintManatee
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}
@end

#endif // OCTAGON

