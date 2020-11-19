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
#import "keychain/ckks/tests/CKKSTests+MultiZone.h"

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

@interface CloudKitKeychainSyncingSOSIntegrationTests : CloudKitKeychainSyncingMultiZoneTestsBase
@end

@implementation CloudKitKeychainSyncingSOSIntegrationTests

- (void)testFindManateePiggyTLKs {
    [self saveFakeKeyHierarchyToLocalDatabase:self.manateeZoneID];
    [self saveTLKMaterialToKeychain:self.manateeZoneID];

    [self startCKKSSubsystem];

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

    [self startCKKSSubsystem];

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

    [self.homeZoneKeys.tlk loadKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "No error loading Home tlk from piggy contents");
}

-(void)testPiggybackingData{
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];

    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }
    [self startCKKSSubsystem];

    [self waitForKeyHierarchyReadinesses];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

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
    XCTAssertEqual([copiedTLKs count], 5u, "piggybacking should have gotten 5 TLKs across (but we have more than that elsewhere)");

    NSArray *copiediCloudidentities = result[@"idents"];
    XCTAssertNotNil(copiediCloudidentities, "idents not set");
    XCTAssertEqual([copiediCloudidentities count], [icloudidentities count], "ident count not same");
}


- (void)testPiggybackingTLKRequest {
    [self putFakeKeyHierachiesInCloudKit];
    [self saveTLKsToKeychain];

    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }
    [self startCKKSSubsystem];
    [self waitForKeyHierarchyReadinesses];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // The "tlk request" piggybacking session calls SOSAccountCopyInitialSyncData
    __block CFErrorRef cferror = NULL;
    NSData* piggybackingData = (NSData*) CFBridgingRelease(SOSAccountCopyInitialSyncData(nil, kSOSInitialSyncFlagTLKsRequestOnly, &cferror));

    XCTAssertEqual(cferror, NULL, "Should have no error fetching only the TLKs");
    XCTAssertNotNil(piggybackingData, "Should have received some sync data");

    const uint8_t* der = [piggybackingData bytes];
    const uint8_t *der_end = der + [piggybackingData length];

    NSDictionary *result = SOSPiggyCopyInitialSyncData(&der, der_end);
    XCTAssertNotNil(result, "Should be able to parse the piggybacking data");

    NSArray *copiedTLKs = result[@"tlks"];
    XCTAssertNotNil(copiedTLKs, "should have some tlks");
    XCTAssertEqual([copiedTLKs count], 1u, "piggybacking should have gotten 1 TLK");
    XCTAssertEqualObjects(copiedTLKs[0][@"srvr"], @"Passwords", "should have the passwords TLK only");
    NSData* keyData = copiedTLKs[0][@"v_Data"];
    XCTAssertNotNil(keyData, "Should have some key material");
    XCTAssertEqual([keyData length], 64, "Key material should be 64 bytes");

    NSArray *copiediCloudidentities = result[@"idents"];
    XCTAssertNotNil(copiediCloudidentities, "idents not set");
    XCTAssertEqual([copiediCloudidentities count], 0, "Should have no icloud identities");
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
        @{
            @"acct" : @"66666666",
            @"srvr" : @"Home",
            @"v_Data" : [NSData dataWithBytes:key length:sizeof(key)],
            @"auth" : @YES,
        },
    ];

    NSArray<NSDictionary *>* sortedTLKs = SOSAccountSortTLKS(tlks);
    XCTAssertNotNil(sortedTLKs, "sortedTLKs not set");

    // Home gets sorted into the middle, as the other Health and Manatee TLKs aren't 'authoritative'
    NSArray<NSString *> *expectedOrder = @[ @"11111111", @"22222222", @"33333333", @"66666666", @"44444444", @"55555555"];
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
    
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    
    // Verify that there are three local keys, and three local current key records
    __weak __typeof(self) weakSelf = self;
    [self.manateeView dispatchSyncWithReadOnlySQLTransaction:^{
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
                                (id)kSecUseDataProtectionKeychain : @YES,
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
    }];
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
    XCTAssertEqual(0, [self.manateeView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    
    // Now, save the TLKs to the keychain (to simulate them coming in later via piggybacking).
    for(CKRecordZoneID* zoneID in self.ckksZones) {
        [self expectCKKSTLKSelfShareUpload:zoneID];
    }

    [self SOSPiggyBackAddToKeychain:piggyData];
    [self waitForKeyHierarchyReadinesses];
    
    // We expect a single record to be uploaded for each key class
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.manateeZoneID checkItem: [self checkClassCBlock:self.manateeZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me-manatee" viewHint:(id)kSecAttrViewHintManatee];
    
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.manateeZoneID checkItem: [self checkClassABlock:self.manateeZoneID message:@"Object was encrypted under class A key in hierarchy"]];

    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:(id)kSecAttrViewHintManatee
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}
@end

#endif // OCTAGON

