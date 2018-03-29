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

#import "CKKSTests.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>
#import <SecurityFoundation/SFKey.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

@interface CKKSManifestTests : CloudKitKeychainSyncingTestsBase {
    BOOL _manifestIsSetup;
    CKKSEgoManifest* _egoManifest;
    id _manifestMock;
    id _fakeSigner1Mock;
}

@end

@implementation CKKSManifestTests

- (NSArray*)mirrorItemsForExistingItems
{
    NSArray<CKRecord*>* records = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    NSMutableArray* items = [[NSMutableArray alloc] init];
    __weak __typeof(self) weakSelf = self;
    __block NSError* error = nil;
    [self.keychainView dispatchSync:^bool(void) {
        for (CKRecord* record in records) {
            CKKSMirrorEntry* mirrorEntry = [CKKSMirrorEntry tryFromDatabase:record.recordID.recordName zoneID:weakSelf.keychainZoneID error:&error];
            XCTAssertNil(error, @"error encountered trying to generate CKKSMirrorEntry: %@", error);
            XCTAssertNotNil(mirrorEntry, @"failed to generate mirror entry");
            if (mirrorEntry) {
                [items addObject:mirrorEntry.item];
            }
        }
        
        return YES;
    }];
    
    return items;
}

- (void)setUp
{
    // Lie to our superclass about how the world is
    (void)[CKKSManifest shouldSyncManifests]; // initialize.
    SecCKKSSetSyncManifests(false);
    SecCKKSSetEnforceManifests(false);

    [super setUp];

    // Should be removed when manifests is turned back on
    SFECKeyPair* keyPair = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    [CKKSManifestInjectionPointHelper registerEgoPeerID:@"MeMyselfAndI" keyPair:keyPair];

    // Always sync manifests, and never enforce them
    SecCKKSSetSyncManifests(true);
    SecCKKSSetEnforceManifests(false);

    XCTAssertTrue([CKKSManifest shouldSyncManifests], "Manifests syncing is enabled");
    XCTAssertFalse([CKKSManifest shouldEnforceManifests], "Manifests enforcement is disabled");

    NSError* error = nil;
    
    [CKKSManifestInjectionPointHelper setIgnoreChanges:NO];
    
    // Test starts with keys in CloudKit (so we can create items later)
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];

    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:self.keychainZoneID];

    [self addGenericPassword:@"data" account:@"first"];
    [self addGenericPassword:@"data" account:@"second"];
    [self addGenericPassword:@"data" account:@"third"];
    [self addGenericPassword:@"data" account:@"fourth"];
    NSUInteger passwordCount = 4u;
    
    [self checkGenericPassword:@"data" account:@"first"];
    [self checkGenericPassword:@"data" account:@"second"];
    [self checkGenericPassword:@"data" account:@"third"];
    [self checkGenericPassword:@"data" account:@"fourth"];
    
    [self expectCKModifyItemRecords:passwordCount currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    
    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];
    
    // Wait for uploads to happen
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
    int tlkshares = 1;
    int extraDeviceStates = 1;
    XCTAssertEqual(self.keychainZone.currentDatabase.count, SYSTEM_DB_RECORD_COUNT + passwordCount + tlkshares + extraDeviceStates, "Have 6+passwordCount objects in cloudkit");
    
    NSArray* items = [self mirrorItemsForExistingItems];
    _egoManifest = [CKKSEgoManifest newManifestForZone:self.keychainZoneID.zoneName withItems:items peerManifestIDs:@[] currentItems:@{} error:&error];
    XCTAssertNil(error, @"no error encountered generating ego manifest");
    XCTAssertNotNil(_egoManifest, @"generated ego manifest");
}

- (void)tearDown {
    _egoManifest = nil;
    [super tearDown];
}

- (void)testSaveEgoManifest
{
    if (![CKKSManifest shouldSyncManifests]) {
        return;
    }

    NSError* error;
    [_egoManifest saveToDatabase:&error];
    XCTAssertNil(error, @"error encountered attempting to save ego manifest: %@", error);
}

- (void)testManifestValidatesItems
{
    if (![CKKSManifest shouldSyncManifests]) {
        return;
    }

    NSError* error = nil;
    for (CKKSItem* mirrorItem in [self mirrorItemsForExistingItems]) {
        XCTAssertTrue([_egoManifest validateItem:mirrorItem withError:&error], @"failed to validate item against manifest");
        XCTAssertNil(error, @"error generated attempting to validate item against manifest: %@", error);
    }
}

- (void)disable32852700testManifestDoesNotValidateUnknownItems
{
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    NSArray* knownItems = [self mirrorItemsForExistingItems];
    [self addGenericPassword:@"data" account:@"unknown_account"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
    [self waitForCKModifications];
    
    NSArray* newItems = [self mirrorItemsForExistingItems];
    CKKSItem* unknownItem = nil;
    for (CKKSItem* item in newItems) {
        BOOL found = NO;
        for (CKKSItem* knownItem in knownItems) {
            if ([knownItem.CKRecordName isEqualToString:item.CKRecordName]) {
                found = YES;
                break;
            }
        }
        
        if (!found) {
            unknownItem = item;
            break;
        }
    }
    
    NSError* error = nil;
    XCTAssertNotNil(_egoManifest, @"have an ego manifest");
    XCTAssertFalse([_egoManifest validateItem:unknownItem withError:&error], @"erroneously validated an unknown item against the manifest");
    XCTAssertNotNil(error, @"failed to generate error when trying to validate an unknown item against the manifest");
}

- (void)testManifestValidatesCurrentItems
{
    NSError* error = nil;
    CKKSItem* item = [[self mirrorItemsForExistingItems] firstObject];
    [_egoManifest setCurrentItemUUID:item.uuid forIdentifier:@"testCurrentItemIdentifier"];
    CKKSCurrentItemPointer* currentItemPointer = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"testCurrentItemIdentifier" currentItemUUID:item.uuid state:SecCKKSProcessedStateLocal zoneID:self.keychainZoneID encodedCKRecord:nil];
    XCTAssertTrue([_egoManifest validateCurrentItem:currentItemPointer withError:&error], @"failed to validate current item against manifest");
    XCTAssertNil(error, @"error encountered validating current item: %@", error);
}

- (void)testManifestDoesNotValidateUnknownCurrentItems
{
    NSError* error = nil;
    CKKSItem* item = [[self mirrorItemsForExistingItems] firstObject];
    CKKSCurrentItemPointer* currentItemPointer = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"testCurrentItemIdentifier" currentItemUUID:item.uuid state:SecCKKSProcessedStateLocal zoneID:self.keychainZoneID encodedCKRecord:nil];
    XCTAssertFalse([_egoManifest validateCurrentItem:currentItemPointer withError:&error], @"erroneously validated an uunknown current item against the manifest");
    XCTAssertNotNil(error, @"failed to generate error when trying to validate an unknown item against the manifest %@", error);
}

- (void)testEgoManifestValidates
{
    if (![CKKSManifest shouldSyncManifests]) {
        return;
    }

    NSError* error = nil;
    XCTAssertTrue([_egoManifest validateWithError:&error], @"failed to validate our own ego manifest");
    XCTAssertNil(error, @"error generated attempting to validate ego manifest: %@", error);
}

- (void)testEgoManifestCreatedOnUpload
{
    if (![CKKSManifest shouldSyncManifests]) {
        return;
    }

    CKKSEgoManifest* manifest = [CKKSEgoManifest tryCurrentEgoManifestForZone:self.keychainZoneID.zoneName];
    XCTAssertNotNil(manifest, @"created some items, but we don't have a current manifest");
}

- (void)disable32852700testGenerationCountIncreaseOnChanges
{
    if (![CKKSManifest shouldSyncManifests]) {
        return;
    }

    NSInteger initialManifestGenerationCount = [[CKKSEgoManifest tryCurrentEgoManifestForZone:self.keychainZoneID.zoneName] generationCount];
    
    [self addGenericPassword:@"data" account:@"GenerationCountIncrease"];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];
    OCMVerifyAllWithDelay(self.mockDatabase, 4);
    [self waitForCKModifications];
    
    NSInteger postUpdateGenerationCount = [[CKKSEgoManifest tryCurrentEgoManifestForZone:self.keychainZoneID.zoneName] generationCount];
    XCTAssertGreaterThan(postUpdateGenerationCount, initialManifestGenerationCount, @"the manifest after the update does not have a larger generation count as expected");
}

- (NSDictionary*)receiveOneItemInManifest:(BOOL)inManifest
{
    if (!inManifest) {
        [CKKSManifestInjectionPointHelper setIgnoreChanges:YES];
    }
    
    NSError* error = nil;
    
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : @"account-delete-me",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };
    
    CFTypeRef item = NULL;
    XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should not yet exist");
    
    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    
    SFECKeyPair* keyPair = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    CKKSEgoManifest* manifest = [CKKSEgoManifest newFakeManifestForZone:self.keychainZoneID.zoneName withItemRecords:@[ckr] currentItems:@{} signerID:@"FakeSigner-1" keyPair:keyPair error:&error];
    [manifest saveToDatabase:&error];
    XCTAssertNil(error, @"error encountered generating fake manifest: %@", error);
    XCTAssertNotNil(manifest, @"failed to generate a fake manifest");
    
    [self.keychainZone addToZone:ckr];
    for (CKRecord* record in [manifest allCKRecordsWithZoneID:self.keychainZoneID]) {
        [self.keychainZone addToZone:record];
    }
    
    // Trigger a notification (with hilariously fake data)
    [self.keychainView notifyZoneChange:nil];
    
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
    
    return query;
}

// <rdar://problem/35102286> Fix primary keys in ckmanifest sql table
// The ckmanifest table only has a single primary key column, so each new manifest written appears to overwrite older manifests
// This is causing this test to fail, since the local peer overwrites the manifest created by FakeSigner-1.
// Disable this test until manifests come back.
- (void)disable35102286testReceiveManifest
{
    if (![CKKSManifest shouldSyncManifests]) {
        return;
    }

    NSError* error = nil;
    
    NSDictionary* itemQuery = [self receiveOneItemInManifest:YES];
    CFTypeRef item = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)itemQuery, &item), "item should exist now");
    
    CKKSManifest* fetchedManifest = [CKKSManifest manifestForZone:self.keychainZoneID.zoneName peerID:@"FakeSigner-1" error:&error];
    XCTAssertNil(error, @"error encountered fetching expected manifest from database: %@", error);
    XCTAssertNotNil(fetchedManifest, @"failed to fetch manifest expected in database");
    
    CKKSMirrorEntry* fetchedEntry = [CKKSMirrorEntry tryFromDatabase:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" zoneID:self.keychainZoneID error:&error];
    XCTAssertNil(error, @"error encountered fetching expected item from database: %@", error);
    XCTAssertNotNil(fetchedEntry, @"failed to fetch item expected in database");
    
    XCTAssertTrue([fetchedManifest validateWithError:&error], @"failed to validate fetched manifest with error: %@", error);
    XCTAssertTrue([fetchedManifest validateItem:fetchedEntry.item withError:&error], @"failed to validate item (%@) against manifest with error: %@", fetchedEntry, error);
}

- (void)testUnauthorizedItemRejected
{
    if (![CKKSManifest shouldSyncManifests] || ![CKKSManifest shouldEnforceManifests]) {
        return;
    }

    NSDictionary* itemQuery = [self receiveOneItemInManifest:NO];
    CFTypeRef item = NULL;
    XCTAssertNotEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)itemQuery, &item), "item should have been rejected");
}

- (void)testSaveManifestWithNilValues
{
    SFECKeyPair* keyPair = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    CKKSManifest* manifest = [CKKSEgoManifest newFakeManifestForZone:@"SomeZone" withItemRecords:@[] currentItems:@{} signerID:@"BadBoy" keyPair:keyPair error:nil];
    [manifest nilAllIvars];
    XCTAssertNil(manifest.zoneID);
    XCTAssertNil(manifest.signerID);
    XCTAssertNoThrow([manifest saveToDatabase:nil]);
}

@end

#endif // OCTAGON
