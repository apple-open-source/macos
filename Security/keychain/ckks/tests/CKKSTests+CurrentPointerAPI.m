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

#include <Security/SecItemPriv.h>
#include <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSMirrorEntry.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/AutoreleaseTest.h"

#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CKKSTests+API.h"
#import "keychain/ot/Affordance_OTConstants.h"

@interface CloudKitKeychainSyncingCurrentPointerAPITests : CloudKitKeychainSyncingTestsBase
@property (nullable) CKRecordZoneID* protectedCloudStorageZoneID;
@end

@implementation CloudKitKeychainSyncingCurrentPointerAPITests

- (NSSet*)managedViewList
{
    NSMutableSet* viewSet = [[super managedViewList] mutableCopy];
    [viewSet addObject:@"ProtectedCloudStorage"];
    return viewSet;
}

- (void)setUp
{
    [super setUp];
    self.protectedCloudStorageZoneID = [[CKRecordZoneID alloc] initWithZoneName:@"ProtectedCloudStorage" ownerName:CKCurrentUserDefaultName];
    [self.ckksZones addObject:self.protectedCloudStorageZoneID];

    FakeCKZone* pcsZone = [[FakeCKZone alloc] initZone:self.protectedCloudStorageZoneID];
    self.zones[self.protectedCloudStorageZoneID] = pcsZone;
}

-(void)fetchCurrentPointer:(NSString*)view fetchCloudValue:(bool)fetchCloudValue persistentRef:(NSData*)persistentRef
{
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"callback occurs"];
    SecItemFetchCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                            (__bridge CFStringRef)@"pcsservice",
                                            (__bridge CFStringRef)view,
                                            fetchCloudValue,
                                            ^(CFDataRef currentPersistentRef, CFErrorRef cferror) {
                                                XCTAssertNotNil((__bridge id)currentPersistentRef, "current item exists");
                                                XCTAssertNil((__bridge id)cferror, "no error exists when there's a current item");
                                                XCTAssertEqualObjects(persistentRef, (__bridge id)currentPersistentRef, "persistent ref matches expected persistent ref");
                                                [currentExpectation fulfill];
                                            });
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
}

-(void)fetchCurrentPointer:(bool)fetchCloudValue persistentRef:(NSData*)persistentRef
{
    [self fetchCurrentPointer:@"keychain" fetchCloudValue:fetchCloudValue persistentRef:persistentRef];
}

-(void)fetchCurrentPointerData:(bool)fetchCloudValue persistentRef:(NSData*)persistentRef
{
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"callback occurs"];
    SecItemFetchCurrentItemDataAcrossAllDevices(@"com.apple.security.ckks",
                                                @"pcsservice",
                                                @"keychain",
                                                fetchCloudValue,
                                            ^(SecItemCurrentItemData *cip, NSError *error) {
                                                XCTAssertNotNil(cip, "current item exists");
                                                XCTAssertNotNil(cip.currentItemPointerModificationTime, "cip should have mtime");
                                                XCTAssertNil(error, "no error exists when there's a current item");
                                                XCTAssertEqualObjects(persistentRef, cip.persistentRef, "persistent ref matches expected persistent ref");
                                                [currentExpectation fulfill];
                                            });
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
}

-(void)fetchCurrentPointerExpectingError:(NSString*)view fetchCloudValue:(bool)fetchCloudValue
{
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"callback occurs"];
    TEST_API_AUTORELEASE_BEFORE(SecItemFetchCurrentItemAcrossAllDevices);
    SecItemFetchCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                            (__bridge CFStringRef)@"pcsservice",
                                            (__bridge CFStringRef)view,
                                            fetchCloudValue,
                                            ^(CFDataRef currentPersistentRef, CFErrorRef cferror) {
                                                XCTAssertNil((__bridge id)currentPersistentRef, "no current item exists");
                                                XCTAssertNotNil((__bridge id)cferror, "Error exists when there's a current item");
                                                [currentExpectation fulfill];
                                            });
    TEST_API_AUTORELEASE_AFTER(SecItemFetchCurrentItemAcrossAllDevices);
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
}

-(void)fetchCurrentPointerExpectingError:(bool)fetchCloudValue
{
    [self fetchCurrentPointerExpectingError:@"keychain" fetchCloudValue:fetchCloudValue];
}

- (void)testPCSFetchCurrentPointerCachedAndUncached {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    [self.defaultCKKS waitForKeyHierarchyReadiness];
    [self.defaultCKKS waitUntilAllOperationsAreFinished]; // ensure everything finishes before we disallow fetches

    // Ensure that local queries don't hit the server.
    self.silentFetchesAllowed = false;
    [self fetchCurrentPointerExpectingError:false];

    // And ensure that global queries do.
    [self expectCKFetch];
    [self fetchCurrentPointerExpectingError:true];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerAddAndUpdate {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    XCTestExpectation* keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");
    [self waitForExpectations:@[keychainChanged] timeout:8];

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    XCTAssertNotNil(persistentRef, "Item should have a pRef");

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    // Set the 'current' pointer.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    // Ensure that setting the current pointer sends a notification
    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    TEST_API_AUTORELEASE_BEFORE(SecItemSetCurrentItemAcrossAllDevices);
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNil(error, "No error setting current item");
                                              [setCurrentExpectation fulfill];
                                          });
    TEST_API_AUTORELEASE_AFTER(SecItemSetCurrentItemAcrossAllDevices);
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self waitForCKModifications];

    // Test for rdar://83903981 (Manatee identity creation race)
    XCTestExpectation* badSetCurrentExpectation = [self expectationWithDescription:@"callback occurs"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "expected an error trying to set current item again");
                                              XCTAssertEqualObjects(error.domain, CKKSErrorDomain, "unexpected error domain");
                                              XCTAssertEqual(error.code, CKKSItemChanged, "unexpected error code");
                                              [badSetCurrentExpectation fulfill];
                                          });
    [self waitForExpectations:@[badSetCurrentExpectation] timeout:2];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    CKRecord* currentItemPointer = self.keychainZone.currentDatabase[[[CKRecordID alloc] initWithRecordName:@"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID]];
    XCTAssertNotNil(currentItemPointer, "Found a CKRecord at the expected location in CloudKit");
    XCTAssertEqualObjects(currentItemPointer.recordType, SecCKRecordCurrentItemType, "Saved CKRecord is correct type");
    XCTAssertEqualObjects(((CKReference*)currentItemPointer[SecCKRecordItemRefKey]).recordID, pcsItemRecordID, "Current Item record points to correct record");
    NSDate *oldModificationTime = currentItemPointer.modificationDate;
    XCTAssertNotNil(oldModificationTime, "Have modification time");


    // Check that the status APIs return the right value
    [self fetchCurrentPointer:false persistentRef:persistentRef];
    [self fetchCurrentPointerData:false persistentRef:persistentRef];

    // Rad. If we got here, adding a new current item pointer works. Let's see if we can modify one.
    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    result = [self pcsAddItem:@"tOTHER-ITEM"
                         data:[@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding]
            serviceIdentifier:(NSNumber*)servIdentifier
                    publicKey:(NSData*)publicKey
               publicIdentity:(NSData*)publicIdentity
                expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");
    [self waitForExpectations:@[keychainChanged] timeout:8];

    // Check that the record is where we expect it
    [self waitForCKModifications];
    CKRecordID* pcsOtherItemRecordID = [[CKRecordID alloc] initWithRecordName: @"2DEA6136-2505-6BFD-E3E8-B44A6E39C3B5" zoneID:self.keychainZoneID];
    CKRecord* recordOther = self.keychainZone.currentDatabase[pcsOtherItemRecordID];
    XCTAssertNotNil(recordOther, "Found other record in CloudKit at expected UUID");

    NSData* otherPersistentRef = result[(id)kSecValuePersistentRef];
    NSData* otherSha1 = result[(id)kSecAttrSHA1];

    // change the 'current' pointer.

    // Refetch the old item's hash, just in case it's changed (it does, about 50% of the time. I'm not sure why).
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) @{
                                                                                   (id)kSecValuePersistentRef : persistentRef,
                                                                                   (id)kSecReturnAttributes : @YES,
                                                                                   }, &cfresult), "Found original item by persistent reference");

    XCTAssertNotNil((__bridge id)cfresult, "Received an item by finding persistent reference");
    NSData* actualSHA1 = CFBridgingRelease(CFRetainSafe(CFDictionaryGetValue(cfresult, kSecAttrSHA1)));
    XCTAssertNotNil(actualSHA1, "Have a SHA1 for the original item");
    CFReleaseNull(cfresult);

    if(![actualSHA1 isEqual:sha1]) {
        ckksnotice_global("ckks", "SHA1s don't match, but why?");
    }

    XCTestExpectation* otherSetCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    // Ensure that setting the current pointer sends a notification
    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)otherPersistentRef,
                                          (__bridge CFDataRef)otherSha1,
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)actualSHA1, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNil(error, "No error setting current item");
                                              [otherSetCurrentExpectation fulfill];
                                          });
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self waitForCKModifications];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    currentItemPointer = self.keychainZone.currentDatabase[[[CKRecordID alloc] initWithRecordName:@"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID]];
    XCTAssertNotNil(currentItemPointer, "Found a CKRecord at the expected location in CloudKit");
    XCTAssertEqualObjects(currentItemPointer.recordType, SecCKRecordCurrentItemType, "Saved CKRecord is correct type");
    XCTAssertEqualObjects(((CKReference*)currentItemPointer[SecCKRecordItemRefKey]).recordID, pcsOtherItemRecordID, "Current Item record points to updated record");
    NSDate *newModificationTime = currentItemPointer.modificationDate;
    XCTAssertNotNil(newModificationTime, "Have modification time");

    XCTAssertEqual([newModificationTime compare:oldModificationTime], NSOrderedDescending, "new mtime should be newer then old");

    // And: again
    [self fetchCurrentPointer:false persistentRef:otherPersistentRef];
    [self fetchCurrentPointer:true persistentRef:otherPersistentRef];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerAddMissingItem {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self fetchCurrentPointerExpectingError:false];

    NSData* fakepersistentRef = [@"not a real pref" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* fakesha1 = [@"not a real sha1" dataUsingEncoding:NSUTF8StringEncoding];

    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    TEST_API_AUTORELEASE_BEFORE(SecItemSetCurrentItemAcrossAllDevices);
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)fakepersistentRef,
                                          (__bridge CFDataRef)fakesha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "Should error setting current item to a nonexistent item");
                                              [setCurrentExpectation fulfill];
                                          });
    TEST_API_AUTORELEASE_AFTER(SecItemSetCurrentItemAcrossAllDevices);
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerAddMissingOldItem {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self fetchCurrentPointerExpectingError:false];

    XCTestExpectation* keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");
    [self waitForExpectations:@[keychainChanged] timeout:8];

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    // Set the 'current' pointer.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    NSData* fakepersistentRef = [@"not a real pref" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* fakesha1 = [@"not a real sha1" dataUsingEncoding:NSUTF8StringEncoding];

    TEST_API_AUTORELEASE_BEFORE(SecItemSetCurrentItemAcrossAllDevices);
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1,
                                          (__bridge CFDataRef)fakepersistentRef,
                                          (__bridge CFDataRef)fakesha1,
                                          ^(CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "Should error setting current item when passing garbage for old item");
                                              [setCurrentExpectation fulfill];
                                          });
    TEST_API_AUTORELEASE_AFTER(SecItemSetCurrentItemAcrossAllDevices);
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self waitForExpectations:@[setCurrentExpectation] timeout:8];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerAddNoCloudKitAccount {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    // Entirely signed out of iCloud. all current record writes should fail.
    self.accountStatus = CKAccountStatusNoAccount;

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    // Should NOT add an item to CloudKit
    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:false];
    XCTAssertNotNil(result, "Received result from adding item");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    // Set the 'current' pointer. This should fail.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "Error setting current item with no CloudKit account");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectationsWithTimeout:8.0 handler:nil];
    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerAddNonSyncItem {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

    NSMutableDictionary* query = [self pcsAddItemQuery:@"account"
                                                  data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                                     serviceIdentifier:servIdentifier
                                             publicKey:publicKey
                                        publicIdentity:publicIdentity];
    query[(id)kSecAttrSynchronizable] = @NO;

    CFTypeRef cfresult = NULL;
    XCTestExpectation* syncExpectation = [self expectationWithDescription: @"_SecItemAddAndNotifyOnSync callback occured"];

    // Note that you will NOT receive a notification here, since you're adding a nonsync item
    syncExpectation.inverted = YES;

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, &cfresult, ^(bool didSync, CFErrorRef error) {
        XCTAssertFalse(didSync, "Item did not sync");
        XCTAssertNotNil((__bridge NSError*)error, "Error syncing item");

        [syncExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    // We don't expect this callback to fire, so give it a second or so
    [self waitForExpectations:@[syncExpectation] timeout:20];

    NSDictionary* result = CFBridgingRelease(cfresult);

    XCTAssertNotNil(result, "Received result from adding item");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    // Set the 'current' pointer. This should fail.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "Error setting current item to nonsyncable item");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[setCurrentExpectation] timeout:20];
    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerFailQuickly {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssert([self.defaultCKKS waitForKeyHierarchyReadiness], "All views should become 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:@"ProtectedCloudStorage" fetchCloudValue:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.protectedCloudStorageZoneID
                          checkItem: [self checkPCSFieldsBlock:self.protectedCloudStorageZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    // Add the item
    NSMutableDictionary* query = [self pcsAddItemQuery:@"testaccount"
                                                  data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                                     serviceIdentifier:servIdentifier
                                             publicKey:publicKey
                                        publicIdentity:publicIdentity];
    query[(id)kSecAttrSyncViewHint] = @"ProtectedCloudStorage";

    CFTypeRef cfresult = NULL;
    XCTestExpectation* syncExpectation = [self expectationWithDescription: @"_SecItemAddAndNotifyOnSync callback occured"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, &cfresult, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "item expected to sync");
        XCTAssertNil((__bridge NSError*)error, "item expected to sync without error");

        [syncExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    [self waitForExpectations:@[syncExpectation] timeout:20];
    [self waitForCKModifications];

    // Verify that item exists at expected UUID
    CKRecordID* pcsRecordID = [[CKRecordID alloc] initWithRecordName:@"D006BA7B-BEB1-C11C-EAE3-67496860B0DD" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* pcsRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[pcsRecordID];
    XCTAssertNotNil(pcsRecord, "Found PCS record in CloudKit at expected UUID");

    NSDictionary* result = CFBridgingRelease(cfresult);
    XCTAssertNotNil(result, "Received result from adding item");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];

    // Manually add current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"D006BA7B-BEB1-C11C-EAE3-67496860B0DD"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");

    // Ensure that receiving the current item pointer generates a notification
    XCTestExpectation* keychainChanged = [self expectChangeForView:self.protectedCloudStorageZoneID.zoneName];
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self waitForExpectations:@[keychainChanged] timeout:8];

    // Make sure we can find the current item pointer (either way)
    [self fetchCurrentPointer:@"ProtectedCloudStorage" fetchCloudValue:false persistentRef:persistentRef];
    [self fetchCurrentPointer:@"ProtectedCloudStorage" fetchCloudValue:true persistentRef:persistentRef];

    // Now, restart with no policy...
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSyncWithoutSettingPolicy:self.defaultCKKS];

    // Make sure we can find the current item pointer
    [self fetchCurrentPointer:@"ProtectedCloudStorage" fetchCloudValue:false persistentRef:persistentRef];
    // Make sure we can't find the current item pointer from the cloud
    [self fetchCurrentPointerExpectingError:@"ProtectedCloudStorage" fetchCloudValue:true];
}

- (void)testPCSFetchIdentityByKeyOutOfBand {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D90" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];

    [self startCKKSSubsystem];

    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchPCSIdentityOutOfBand callback occured"];

    CKKSPCSIdentityQuery* query = [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier accessGroup:@"com.apple.security.ckks" publicKey:[publicKey base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName];
    NSArray* queries = @[query];
    
    SecItemFetchPCSIdentityOutOfBand(queries, true, ^(NSArray<CKKSPCSIdentityQueryResult *> *pcsIdentities, NSError *error) {
        XCTAssertNil(error, "Should not have errored");
        XCTAssertNotEqual(pcsIdentities.count, 0, "Items should have been returned");
        XCTAssertEqualObjects(pcsIdentities[0].serviceNumber, servIdentifier, "PCS Identity was found");
        XCTAssertEqualObjects(pcsIdentities[0].publicKey, query.publicKey, "Correct public key was returned");
        XCTAssertNotNil(pcsIdentities[0].decryptedRecord, "Record for PCS Identity was found");
        [currentExpectation fulfill];
    });

    [self waitForExpectations:@[currentExpectation] timeout:10];
    
}

- (void)testFetchPCSIdentityByKeyRecordNotFound {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D90" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.self.protectedCloudStorageZoneID] addToZone:record];

    [self startCKKSSubsystem];

    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchPCSIdentityOutOfBand callback occured"];
    
    CKKSPCSIdentityQuery* query = [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:@(0) accessGroup:@"com.apple.security.ckks" publicKey:[publicKey base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName];
    NSArray* queries = @[query];
    
    SecItemFetchPCSIdentityOutOfBand(queries, true, ^(NSArray<CKKSPCSIdentityQueryResult *> *pcsIdentities, NSError *error) {
        XCTAssertNotNil(error, "Should have errored");
        XCTAssertEqual(error.code, CKKSNoSuchRecord, "PCS Item does not exist");
        XCTAssertEqual(pcsIdentities.count, 0, "Items should not have been returned");
        [currentExpectation fulfill];
    });
    
    [self waitForExpectations:@[currentExpectation] timeout:10];
    
}

- (void)testFetchPCSIdentityByKeyMultipleItemsSucceeds {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    // Test that we can get another identity for the same service
    NSNumber* servIdentifier2 = @3;
    NSData* publicKey2 = [@"qwertyuiop" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity2 = [@"otherdata" dataUsingEncoding:NSUTF8StringEncoding];
    
    NSNumber* servIdentifier3 = @4;
    NSData* publicKey3 = [@"zxcvbnm" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity3 = [@"someotherdata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D90" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    CKRecord* record2 = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D94" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier2 plaintextPCSPublicKey: publicKey2 plaintextPCSPublicIdentity: publicIdentity2];
    CKRecord* record3 = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D98" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier3 plaintextPCSPublicKey: publicKey3 plaintextPCSPublicIdentity: publicIdentity3];

    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record2];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record3];

    [self startCKKSSubsystem];

    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchPCSIdentityOutOfBand callback occured"];

    NSArray<CKKSPCSIdentityQuery*>* queries = @[
        [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier accessGroup:@"com.apple.security.ckks" publicKey:[publicKey base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName],
        [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier2 accessGroup:@"com.apple.security.ckks" publicKey:[publicKey2 base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName],
        [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier3 accessGroup:@"com.apple.security.ckks" publicKey:[publicKey3 base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName],
    ];

    SecItemFetchPCSIdentityOutOfBand(queries, true, ^(NSArray<CKKSPCSIdentityQueryResult *> *pcsIdentities, NSError *error) {
        XCTAssertNil(error, "Should not have errored");
        XCTAssertEqual(pcsIdentities.count, 3, "Items should have been returned");
        
        // Unsure if these are guaranteed to be in order
        XCTAssertEqual(pcsIdentities[0].serviceNumber, servIdentifier, "PCS Identity was found");
        XCTAssertEqualObjects(pcsIdentities[0].publicKey, queries[0].publicKey, "Returned public key matches expected public key");
        XCTAssertNotNil(pcsIdentities[0].decryptedRecord, "Record for PCS Identity was found");

        XCTAssertEqual(pcsIdentities[1].serviceNumber, servIdentifier2, "PCS Identity was found");
        XCTAssertEqualObjects(pcsIdentities[1].publicKey, queries[1].publicKey, "Returned public key matches expected public key");
        XCTAssertNotNil(pcsIdentities[1].decryptedRecord, "Record for PCS Identity was found");

        XCTAssertEqualObjects(pcsIdentities[2].serviceNumber, servIdentifier3, "PCS Identity was found");
        XCTAssertEqualObjects(pcsIdentities[2].publicKey, queries[2].publicKey, "Returned public key matches expected public key");
        XCTAssertNotNil(pcsIdentities[2].decryptedRecord, "Record for PCS Identity was found");
        
        [currentExpectation fulfill];
    });

    [self waitForExpectations:@[currentExpectation] timeout:10];
    
}


- (void)testFetchPCSIdentityByKeyMultipleItemsFailsOnErroneousZone {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    NSNumber* servIdentifier2 = @4;
    NSData* publicKey2 = [@"qwertyuiop" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity2 = [@"otherdata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D90" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    CKRecord* record2 = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D94" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier2 plaintextPCSPublicKey: publicKey2 plaintextPCSPublicIdentity: publicIdentity2];

    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record2];

    [self startCKKSSubsystem];

    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchPCSIdentityOutOfBand callback occured"];
    
    NSArray* queries = @[
        [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier accessGroup:@"com.apple.security.ckks" publicKey:[publicKey base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName],
        [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier2 accessGroup:@"com.apple.security.ckks" publicKey:[publicKey2 base64EncodedStringWithOptions:0] zoneID:@"zone-does-not-exist"]
    ];

    SecItemFetchPCSIdentityOutOfBand(queries, true, ^(NSArray<CKKSPCSIdentityQueryResult *> *pcsIdentities, NSError *error) {
        XCTAssertNotNil(error, "Should have errored");
        XCTAssertEqual(error.code, CKKSNoSuchView, "Zone does not exist");
        XCTAssertEqual(pcsIdentities.count, 0, "Items should not have been returned");
        [currentExpectation fulfill];
    });
    
    [self waitForExpectations:@[currentExpectation] timeout:10];
    
}

- (void)testFetchCurrentItemOutOfBand {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    // Create item record
    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];

    // Create current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");
    
    [self startCKKSSubsystem];
    
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchCurrentItemOutOfBand callback occured"];

    NSArray<CKKSCurrentItemQuery*>* queries = @[
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName]
    ];
    
    SecItemFetchCurrentItemOutOfBand(queries, true, ^(NSArray<CKKSCurrentItemQueryResult *> *currentItems, NSError *error) {
        XCTAssertNil(error, "Should not have errored");
        XCTAssertEqual(currentItems.count, 1, "Item was returned");
        XCTAssertEqualObjects(currentItems[0].identifier, queries[0].identifier, "Correct item returned");
        XCTAssertNotNil(currentItems[0].decryptedRecord, "Item record returned");
        [currentExpectation fulfill];
    });
    
    [self waitForExpectations:@[currentExpectation] timeout:10];
}


- (void)testFetchCurrentItemOutOfBandNoSuchRecord {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];

    // Create current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");
    
    [self startCKKSSubsystem];
    
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchCurrentItemOutOfBand callback occured"];

    NSArray<CKKSCurrentItemQuery*>* queries = @[
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"does-not-exist" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName]
    ];
    
    SecItemFetchCurrentItemOutOfBand(queries, true, ^(NSArray<CKKSCurrentItemQueryResult *> *currentItems, NSError *error) {
        XCTAssertNotNil(error, "Should have errored");
        XCTAssertEqual(error.code, CKKSNoSuchRecord, "Record does not exist");
        XCTAssertEqual(currentItems.count, 0, "No items were returned");
        [currentExpectation fulfill];
    });
    
    [self waitForExpectations:@[currentExpectation] timeout:10];
}


- (void)testFetchCurrentItemsMultipleItemsSucceeds {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    // Add first record
    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];

    // Create current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");
    
    // Add second record
    NSNumber* servIdentifier2 = @4;
    NSData* publicKey2 = [@"qwertyuiop" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity2 = [@"otherdata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record2 = [self createFakeRecord:self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D94" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier2 plaintextPCSPublicKey: publicKey2 plaintextPCSPublicIdentity: publicIdentity2];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record2];
    
    // Create current item pointer
    CKKSCurrentItemPointer* cip2 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice2"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D94"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip2 CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID2 = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice2" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord2 = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID2];
    XCTAssertNotNil(currentPointerRecord2, "Found current item pointer in CloudKit");
    
    [self startCKKSSubsystem];
    
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchCurrentItemOutOfBand callback occured"];

    NSArray<CKKSCurrentItemQuery*>* queries = @[
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName],
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice2" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName]
    ];
    
    SecItemFetchCurrentItemOutOfBand(queries, true, ^(NSArray<CKKSCurrentItemQueryResult *> *currentItems, NSError *error) {
        XCTAssertNil(error, "Should not have errored");
        XCTAssertEqual(currentItems.count, 2, "Items were returned");
        XCTAssertEqualObjects(currentItems[0].identifier, queries[0].identifier, "Correct item returned");
        XCTAssertNotNil(currentItems[0].decryptedRecord, "Item record returned");
        XCTAssertEqualObjects(currentItems[1].identifier, queries[1].identifier, "Correct item returned");
        XCTAssertNotNil(currentItems[1].decryptedRecord, "Item record returned");
        [currentExpectation fulfill];
    });
    
    [self waitForExpectations:@[currentExpectation] timeout:10];
}


- (void)testFetchCurrentItemsOutOfBandBadPointer {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    // Add first record
    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record = [self createFakeRecord: self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    
    // Create current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"does-not-exist"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");
    
    // Add second record
    NSNumber* servIdentifier2 = @4;
    NSData* publicKey2 = [@"qwertyuiop" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity2 = [@"otherdata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record2 = [self createFakeRecord: self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D94" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier2 plaintextPCSPublicKey: publicKey2 plaintextPCSPublicIdentity: publicIdentity2];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record2];
    
    // Create current item pointer
    CKKSCurrentItemPointer* cip2 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice-2"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D94"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip2 CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID2 = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice-2" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord2 = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID2];
    XCTAssertNotNil(currentPointerRecord2, "Found current item pointer in CloudKit");
    
    [self startCKKSSubsystem];
    
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchCurrentItemOutOfBand callback occured"];
    
    NSArray<CKKSCurrentItemQuery*>* queries = @[
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName],
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice-2" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName]
    ];

    
    SecItemFetchCurrentItemOutOfBand(queries, true, ^(NSArray<CKKSCurrentItemQueryResult *> *currentItems, NSError *error) {
        XCTAssertNotNil(error, "Should have errored");
        XCTAssertEqual(error.code, CKKSNoSuchRecord, "Bad CIP");
        XCTAssertEqual(currentItems.count, 0, "No items were returned");
        [currentExpectation fulfill];
    });

    [self waitForExpectations:@[currentExpectation] timeout:10];
}

- (void)testFetchIdentitiesOutOfBandDisallowed {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    CKRecord* record = [self createFakeRecord: self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D90" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    [self startCKKSSubsystem];
    
    // Ask CKKS to fetch & wait for it to finish processing incoming queue
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
        
    XCTestExpectation* OOBExpectation = [self expectationWithDescription: @"fetchPCSIdentityOutOfBand callback occured"];

    CKKSPCSIdentityQuery* query = [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier accessGroup:@"com.apple.security.ckks" publicKey:[publicKey base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName];
    NSArray* queries = @[query];
    
    SecItemFetchPCSIdentityOutOfBand(queries, false, ^(NSArray<CKKSPCSIdentityQueryResult *> *pcsIdentities, NSError *error) {
        XCTAssertNotNil(error, "Should have errored due to CKKS ready state");
        XCTAssertEqual(error.code, CKKSErrorOutOfBandFetchingDisallowed);
        XCTAssertEqual(pcsIdentities.count, 0, "No identities should have been returned");
        [OOBExpectation fulfill];
    });

    [self waitForExpectations:@[OOBExpectation] timeout:10];
}

- (void)testFetchIdentitiesOutOfBandDisallowedAfterRestart {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];

    CKRecord* record = [self createFakeRecord: self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D90" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    [self startCKKSSubsystem];
    
    // Ask CKKS to fetch & wait for it to finish processing incoming queue
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    
    // Now simulate a restart of securityd:
    // Tear down the CKKS object
    [self.defaultCKKS halt];
    
    // Bring CKKS back up
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    
    // We want to ensure that the key hierarchy state machine doesn't progress
    [self holdCloudKitFetches];
    
    // Verify that OOB fetch is disallowed.
    XCTestExpectation* OOBExpectation = [self expectationWithDescription: @"fetchPCSIdentityOutOfBand callback occured"];

    CKKSPCSIdentityQuery* query = [[CKKSPCSIdentityQuery alloc] initWithServiceNumber:servIdentifier accessGroup:@"com.apple.security.ckks" publicKey:[publicKey base64EncodedStringWithOptions:0] zoneID:self.protectedCloudStorageZoneID.zoneName];
    NSArray* queries = @[query];
    
    SecItemFetchPCSIdentityOutOfBand(queries, false, ^(NSArray<CKKSPCSIdentityQueryResult *> *pcsIdentities, NSError *error) {
        XCTAssertNotNil(error, "Should have errored due to CKKS ready state");
        XCTAssertEqual(error.code, CKKSErrorOutOfBandFetchingDisallowed);
        XCTAssertEqual(pcsIdentities.count, 0, "No identities should have been returned");
        [OOBExpectation fulfill];
    });

    [self waitForExpectations:@[OOBExpectation] timeout:10];
}


- (void)testFetchCurrentItemsOutOfBandDisallowed {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);
    
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    
    // Create item record
    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record = [self createFakeRecord: self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    
    // Create current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");
    
    [self startCKKSSubsystem];
    
    // Ask CKKS to fetch & wait for it to finish processing incoming queue
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchCurrentItemOutOfBand callback occured"];

    NSArray<CKKSCurrentItemQuery*>* queries = @[
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName]
    ];
    
    SecItemFetchCurrentItemOutOfBand(queries, false, ^(NSArray<CKKSCurrentItemQueryResult *> *currentItems, NSError *error) {
        XCTAssertNotNil(error, "Should have errored due to CKKS ready state");
        XCTAssertEqual(error.code, CKKSErrorOutOfBandFetchingDisallowed);
        XCTAssertEqual(currentItems.count, 0, "No items were returned");
        [currentExpectation fulfill];
    });
    
    [self waitForExpectations:@[currentExpectation] timeout:10];
}

- (void)testFetchCurrentItemOutofBandDisallowedAfterRestart {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);
    
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test (prevent spurious errors)
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    
    // Create item record
    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];
    
    CKRecord* record = [self createFakeRecord: self.protectedCloudStorageZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91" withAccount: nil key: nil plaintextPCSServiceIdentifier: servIdentifier plaintextPCSPublicKey: publicKey plaintextPCSPublicIdentity: publicIdentity];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:record];
    
    // Create current item pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"7B598D31-F9C5-481E-98AC-5A507CDB2D91"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.protectedCloudStorageZoneID
                                                                    encodedCKRecord:nil];
    [self.zones[self.protectedCloudStorageZoneID] addToZone:[cip CKRecordWithZoneID:self.protectedCloudStorageZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.protectedCloudStorageZoneID];
    CKRecord* currentPointerRecord = self.zones[self.protectedCloudStorageZoneID].currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found current item pointer in CloudKit");
    
    [self startCKKSSubsystem];
    
    // Ask CKKS to fetch & wait for it to finish processing incoming queue
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    
    // Now simulate a restart of securityd:
    // Tear down the CKKS object
    [self.defaultCKKS halt];
    
    // Bring CKKS back up
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    
    // We want to ensure that the key hierarchy state machine doesn't progress
    [self holdCloudKitFetches];
    
    // Verify that OOB fetch is disallowed.
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"fetchCurrentItemOutOfBand callback occured"];
    NSArray<CKKSCurrentItemQuery*>* queries = @[
        [[CKKSCurrentItemQuery alloc] initWithIdentifier:@"pcsservice" accessGroup: @"com.apple.security.ckks" zoneID:self.protectedCloudStorageZoneID.zoneName]
    ];
    
    SecItemFetchCurrentItemOutOfBand(queries, false, ^(NSArray<CKKSCurrentItemQueryResult *> *currentItems, NSError *error) {
        XCTAssertNotNil(error, "Should have errored due to CKKS ready state");
        XCTAssertEqual(error.code, CKKSErrorOutOfBandFetchingDisallowed);
        XCTAssertEqual(currentItems.count, 0, "No items were returned");
        [currentExpectation fulfill];
    });

    [self waitForExpectations:@[currentExpectation] timeout:10];
}

- (void)testPCSCurrentPointerReceive {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    XCTestExpectation* keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");
    NSData* persistentRef = result[(id)kSecValuePersistentRef];

    [self waitForExpectations:@[keychainChanged] timeout:8];

    // And a second item
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];
    result = [self pcsAddItem:@"testaccount2"
                         data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
            serviceIdentifier:(NSNumber*)servIdentifier
                    publicKey:(NSData*)publicKey
               publicIdentity:(NSData*)publicIdentity
                expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");
    NSData* persistentRef2 = result[(id)kSecValuePersistentRef];

    // Check that the records are where we expect them in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    CKRecordID* pcsItemRecordID2 = [[CKRecordID alloc] initWithRecordName: @"10E76B80-CE1C-A52A-B0CB-462A2EBA05AF" zoneID:self.keychainZoneID];
    CKRecord* record2 = self.keychainZone.currentDatabase[pcsItemRecordID2];
    XCTAssertNotNil(record2, "Found 2nd record in CloudKit at expected UUID");

    // Still no current pointer.
    [self fetchCurrentPointerExpectingError:false];

    // Another machine comes along and updates the pointer!
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"50184A35-4480-E8BA-769B-567CF72F1EC0"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord = self.keychainZone.currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found record in CloudKit at expected UUID");

    // Ensure that receiving the current item pointer generates a notification
    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self fetchCurrentPointer:false persistentRef:persistentRef];

    // And again!
    CKKSCurrentItemPointer* cip2 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                           contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:pcsItemRecordID2.recordName
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip2 CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID2 = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord2 = self.keychainZone.currentDatabase[currentPointerRecordID2];
    XCTAssertNotNil(currentPointerRecord2, "Found record in CloudKit at expected UUID");

    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self fetchCurrentPointer:false persistentRef:persistentRef2];

    SecResetLocalSecuritydXPCFakeEntitlements();
}


- (void)testPCSCurrentPointerReceiveDelete {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");
    NSData* persistentRef = result[(id)kSecValuePersistentRef];

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    // Still no current pointer.
    [self fetchCurrentPointerExpectingError:false];

    // Another machine comes along and updates the pointer!
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:@"50184A35-4480-E8BA-769B-567CF72F1EC0"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord = self.keychainZone.currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found record in CloudKit at expected UUID");

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    [self fetchCurrentPointer:false persistentRef:persistentRef];

    // Ensure that receiving the current item pointer generates a notification
    XCTestExpectation* keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    // Another machine comes along and deletes the pointer!
    [self.keychainZone deleteCKRecordIDFromZone: currentPointerRecordID];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];
    [self waitForExpectations:@[keychainChanged] timeout:8];

    [self fetchCurrentPointerExpectingError:false];

    SecResetLocalSecuritydXPCFakeEntitlements();
}


- (void)testPCSCurrentPointerRecoverFromRecordExistsError {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    NSString* recordUUID = @"50184A35-4480-E8BA-769B-567CF72F1EC0";
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName:recordUUID zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    // Someone else sets the current record pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                          contextID:self.defaultCKKS.operationDependencies.contextID
                                                                    currentItemUUID:recordUUID
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    XCTAssertNotNil(cip, "Should have created a CIP");
    CKRecord* cipRecord = [cip CKRecordWithZoneID:self.keychainZoneID];
    XCTAssertNotNil(cipRecord, "Should have created a CKRecord for this CIP");
    [self.keychainZone addToZone: cipRecord];

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    // Set the 'current' pointer.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "Should have received an error setting current item (because of conflict)");
                                              [setCurrentExpectation fulfill];
                                          });
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    [self fetchCurrentPointer:false persistentRef:persistentRef];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentPointerWasCurrent {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    void (^setWasCurrentAndEtag)(void) = ^{
        // Set the 'was current' flag on the record
        CKRecord* modifiedRecord = [record copy];
        modifiedRecord[SecCKRecordServerWasCurrent] = [NSNumber numberWithInteger:10];
        [self.keychainZone addToZone:modifiedRecord];
    };

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:setWasCurrentAndEtag];

    // Set the 'current' pointer.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNil(error, "No error setting current item");
                                              [setCurrentExpectation fulfill];
                                          });
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
    [self waitForCKModifications];

    // Check that the number is on the CKKSMirrorEntry (before fetch)
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:@"50184A35-4480-E8BA-769B-567CF72F1EC0"
                                                    contextID:self.defaultCKKS.operationDependencies.contextID
                                                       zoneID:self.keychainZoneID
                                                        error:&error];

        XCTAssertNil(error, "no error fetching ckme");
        XCTAssertNotNil(ckme, "Received a ckme");

        XCTAssertEqual(ckme.wasCurrent, 10u, "Properly received wasCurrent (before fetch)");
    }];

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Check that the number is on the CKKSMirrorEntry (after fetch)
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:@"50184A35-4480-E8BA-769B-567CF72F1EC0"
                                                    contextID:self.defaultCKKS.operationDependencies.contextID
                                                       zoneID:self.keychainZoneID
                                                        error:&error];

        XCTAssertNil(error, "no error fetching ckme");
        XCTAssertNotNil(ckme, "Received a ckme");

        XCTAssertEqual(ckme.wasCurrent, 10u, "Properly received wasCurrent");
    }];
}

-(void)testPCSCurrentPointerWriteFailure {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];

    // Set the 'current' pointer.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNotNil(error, "Error setting current item when the write fails");
                                              [setCurrentExpectation fulfill];
                                          });
    OCMVerifyAllWithDelay(self.mockDatabase, 40);

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

- (void)testPCSCurrentRecoverFromDanglingPointer {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:10*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Ensure there's no current pointer
    [self fetchCurrentPointerExpectingError:false];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSDictionary* result = [self pcsAddItem:@"testaccount"
                                       data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:true];
    XCTAssertNotNil(result, "Received result from adding item");

    // Check that the record is where we expect it in CloudKit
    [self waitForCKModifications];
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    // Set the 'current' pointer.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    // Ensure that setting the current pointer sends a notification
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNil(error, "No error setting current item");
                                              [setCurrentExpectation fulfill];
                                          });
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    // Delete the keychain item
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef)@{
                                                                            (id)kSecClass : (id)kSecClassGenericPassword,
                                                                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                                                            (id)kSecAttrAccount:@"testaccount",
                                                                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                                                            }), "Should receive no error deleting item");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now, fetch the current pointer: we should get an error
    [self fetchCurrentPointerExpectingError:false];

    // Setting the current item pointer again, using a NULL old value, should work.
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    result = [self pcsAddItem:@"testaccount2"
                         data:[@"asdf" dataUsingEncoding:NSUTF8StringEncoding]
            serviceIdentifier:(NSNumber*)servIdentifier
                    publicKey:(NSData*)publicKey
               publicIdentity:(NSData*)publicIdentity
                expectingSync:true];
    XCTAssertNotNil(result, "Should have result from adding item2");

    persistentRef = result[(id)kSecValuePersistentRef];
    sha1 = result[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    // Set the 'current' pointer.
    setCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    // Ensure that setting the current pointer sends a notification
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              NSError* error = (__bridge NSError*)cferror;
                                              XCTAssertNil(error, "No error setting current item");
                                              [setCurrentExpectation fulfill];
                                          });
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

-(void)testPCSCurrentSetConflictedItemAsCurrent {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // Before CKKS can add the item, shove a conflicting one into CloudKit
    NSError* error = nil;

    NSString* account = @"testaccount";

    // Create an item in CloudKit that will conflict in both UUID and primary key
    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"YnBsaXN0MDDbAQIDBAUGBwgJCgsMDQ4PEBESEhMUFVZ2X0RhdGFUYWNjdFR0b21iVHN2Y2VUc2hhMVRtdXNyVGNkYXRUbWRhdFRwZG1uVGFncnBVY2xhc3NEYXNkZlt0ZXN0YWNjb3VudBAAUE8QFF7OzuEEGWTTwzzSp/rjY6ubHW2rQDNBv7zNQtQUQFJja18QF2NvbS5hcHBsZS5zZWN1cml0eS5ja2tzVGdlbnAIHyYrMDU6P0RJTlNZXmpsbYSFjpGrAAAAAAAAAQEAAAAAAAAAFgAAAAAAAAAAAAAAAAAAALA=" options:0];
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:itemdata
                                                                           options:0
                                                                            format:nil
                                                                             error:&error] mutableCopy];
    XCTAssertNil(error, "Error should be nil parsing base64 item");

    item[@"v_Data"] = [@"conflictingdata" dataUsingEncoding:NSUTF8StringEncoding];
    item[@"vwht"] = @"keychain";
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* mismatchedRecord = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone: mismatchedRecord];

    self.defaultCKKS.holdIncomingQueueOperation = [CKKSResultOperation named:@"hold-incoming" withBlock:^{
        ckksnotice_global("ckks", "Releasing process incoming queue hold");
    }];

    NSData* firstItemData = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];

    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    NSDictionary* result = [self pcsAddItem:account
                                       data:firstItemData
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:false];
    XCTAssertNotNil(result, "Should receive result from adding item");
    if (result == nil) {
        return;
    }

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    // Ensure that fetching the item without grabbing data returns the same SHA1
    NSDictionary* prefquery = @{(id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecReturnAttributes : @YES,
                                (id)kSecValuePersistentRef : persistentRef,
                                (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                                };
    CFTypeRef prefresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)prefquery, &prefresult), "Should be able to find item by persistent ref");
    NSDictionary* newPersistentRefResult = (NSDictionary*) CFBridgingRelease(prefresult);
    prefresult = NULL;
    XCTAssertNotNil(newPersistentRefResult, "Should have received item attributes");
    XCTAssertEqualObjects(newPersistentRefResult[(id)kSecAttrSHA1], sha1, "SHA1 should match between Add and Find (with data)");
    XCTAssertNil(newPersistentRefResult[(id)kSecValueData], "Should have returned no data");

    // Ensure that fetching the item and grabbing data returns the same SHA1
    prefquery = @{(id)kSecClass : (id)kSecClassGenericPassword,
                  (id)kSecReturnAttributes : @YES,
                  (id)kSecReturnData : @YES,
                  (id)kSecValuePersistentRef : persistentRef,
                  (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                  };
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)prefquery, &prefresult), "Should be able to find item by persistent ref");
    newPersistentRefResult = (NSDictionary*) CFBridgingRelease(prefresult);
    XCTAssertNotNil(newPersistentRefResult, "Should have received item attributes");
    XCTAssertEqualObjects(newPersistentRefResult[(id)kSecAttrSHA1], sha1, "SHA1 should match between Add and Find (with data)");
    XCTAssertEqualObjects(newPersistentRefResult[(id)kSecValueData], firstItemData, "Should have returned data matching the item we put in");

    // Set the current pointer to the result of adding this item. This should fail.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs before incoming queue operation"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNotNil((__bridge NSError*)cferror, "Should error setting current item to hash of item which failed to sync (before incoming queue operation)");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[setCurrentExpectation] timeout:20];

    // Now, release the incoming queue processing and retry the failure
    [self.operationQueue addOperation:self.defaultCKKS.holdIncomingQueueOperation];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");

    setCurrentExpectation = [self expectationWithDescription: @"callback occurs after incoming queue operation"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNotNil((__bridge NSError*)cferror, "Should error setting current item to hash of item which failed to sync (after incoming queue operation)");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[setCurrentExpectation] timeout:20];

    // Reissue a fetch and find the new persistent ref and sha1 for the item at this UUID
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // The conflicting item update should have won
    [self checkGenericPassword:@"conflictingdata" account:account];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecReturnPersistentRef: @YES,
                            };

    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Finding item %@", account);
    NSDictionary* newResult = CFBridgingRelease(cfresult);
    XCTAssertNotNil(newResult, "Received an item");

    NSData* newPersistentRef = newResult[(id)kSecValuePersistentRef];
    NSData* newSha1 = newResult[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    XCTestExpectation* newSetCurrentExpectation = [self expectationWithDescription: @"callback occurs"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)newPersistentRef,
                                          (__bridge CFDataRef)newSha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNil((__bridge NSError*)cferror, "Shouldn't error setting current item");
                                              [newSetCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[newSetCurrentExpectation] timeout:20];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

// TODO: add test for SecItemUnsetCurrentItemsAcrossAllDevices

- (void) testMaintainSameUUIDPrefDuringItemConflict
{
    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(true);

    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self createAndSaveFakeKeyHierarchy:self.protectedCloudStorageZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should have become ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");
    [self.defaultCKKS waitUntilAllOperationsAreFinished];

    // Before CKKS can add the item, shove a conflicting one into CloudKit
    NSError* error = nil;

    NSString* account = @"testaccount";

    // Create an item in CloudKit that will conflict in both UUID and primary key
    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"YnBsaXN0MDDbAQIDBAUGBwgJCgsMDQ4PEBESEhMUFVZ2X0RhdGFUYWNjdFR0b21iVHN2Y2VUc2hhMVRtdXNyVGNkYXRUbWRhdFRwZG1uVGFncnBVY2xhc3NEYXNkZlt0ZXN0YWNjb3VudBAAUE8QFF7OzuEEGWTTwzzSp/rjY6ubHW2rQDNBv7zNQtQUQFJja18QF2NvbS5hcHBsZS5zZWN1cml0eS5ja2tzVGdlbnAIHyYrMDU6P0RJTlNZXmpsbYSFjpGrAAAAAAAAAQEAAAAAAAAAFgAAAAAAAAAAAAAAAAAAALA=" options:0];
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:itemdata
                                                                           options:0
                                                                            format:nil
                                                                             error:&error] mutableCopy];
    XCTAssertNil(error, "Error should be nil parsing base64 item");

    item[@"v_Data"] = [@"conflictingdata" dataUsingEncoding:NSUTF8StringEncoding];
    item[@"vwht"] = @"keychain";
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* mismatchedRecord = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone: mismatchedRecord];

    self.defaultCKKS.holdIncomingQueueOperation = [CKKSResultOperation named:@"hold-incoming" withBlock:^{
        ckksnotice_global("ckks", "Releasing process incoming queue hold");
    }];

    NSData* firstItemData = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];

    [self expectCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    NSDictionary* result = [self pcsAddItem:account
                                       data:firstItemData
                          serviceIdentifier:(NSNumber*)servIdentifier
                                  publicKey:(NSData*)publicKey
                             publicIdentity:(NSData*)publicIdentity
                              expectingSync:false];
    XCTAssertNotNil(result, "Should receive result from adding item");
    if (result == nil) {
        return;
    }

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    XCTAssertNotNil(persistentRef, "persistentRef should not be nil");
    XCTAssertEqual([persistentRef length], 20, "persistent reference should have a length of 20");

    NSData* sha1 = result[(id)kSecAttrSHA1];

    // Ensure that fetching the item without grabbing data returns the same SHA1
    NSDictionary* prefquery = @{(id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecReturnAttributes : @YES,
                                (id)kSecValuePersistentRef : persistentRef,
                                (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                                };
    CFTypeRef prefresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)prefquery, &prefresult), "Should be able to find item by persistent ref");
    NSDictionary* newPersistentRefResult = (NSDictionary*) CFBridgingRelease(prefresult);
    prefresult = NULL;
    XCTAssertNotNil(newPersistentRefResult, "Should have received item attributes");
    XCTAssertEqualObjects(newPersistentRefResult[(id)kSecAttrSHA1], sha1, "SHA1 should match between Add and Find (with data)");
    XCTAssertNil(newPersistentRefResult[(id)kSecValueData], "Should have returned no data");

    // Ensure that fetching the item and grabbing data returns the same SHA1
    prefquery = @{(id)kSecClass : (id)kSecClassGenericPassword,
                  (id)kSecReturnAttributes : @YES,
                  (id)kSecReturnData : @YES,
                  (id)kSecValuePersistentRef : persistentRef,
                  (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                  };
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef)prefquery, &prefresult), "Should be able to find item by persistent ref");
    newPersistentRefResult = (NSDictionary*) CFBridgingRelease(prefresult);
    XCTAssertNotNil(newPersistentRefResult, "Should have received item attributes");
    XCTAssertEqualObjects(newPersistentRefResult[(id)kSecAttrSHA1], sha1, "SHA1 should match between Add and Find (with data)");
    XCTAssertEqualObjects(newPersistentRefResult[(id)kSecValueData], firstItemData, "Should have returned data matching the item we put in");

    // Set the current pointer to the result of adding this item. This should fail.
    XCTestExpectation* setCurrentExpectation = [self expectationWithDescription: @"callback occurs before incoming queue operation"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNotNil((__bridge NSError*)cferror, "Should error setting current item to hash of item which failed to sync (before incoming queue operation)");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[setCurrentExpectation] timeout:20];

    // Now, release the incoming queue processing and retry the failure
    [self.operationQueue addOperation:self.defaultCKKS.holdIncomingQueueOperation];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    setCurrentExpectation = [self expectationWithDescription: @"callback occurs after incoming queue operation"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNotNil((__bridge NSError*)cferror, "Should error setting current item to hash of item which failed to sync (after incoming queue operation)");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[setCurrentExpectation] timeout:20];

    // Reissue a fetch and find the new persistent ref and sha1 for the item at this UUID
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // The conflicting item update should have won
    [self checkGenericPassword:@"conflictingdata" account:account];

    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecReturnPersistentRef: @YES,
                            };

    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Finding item %@", account);
    NSDictionary* newResult = CFBridgingRelease(cfresult);
    XCTAssertNotNil(newResult, "Received an item");

    NSData* newPersistentRef = newResult[(id)kSecValuePersistentRef];
    XCTAssertNotNil(newPersistentRef, "newPersistentRef should not be nil");
    XCTAssertEqual([newPersistentRef length], 20, "newPersistentRef reference should have a length of 20");

    NSData* newSha1 = newResult[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
          inspectOperationGroup:nil
           runAfterModification:nil];

    XCTestExpectation* newSetCurrentExpectation = [self expectationWithDescription: @"callback occurs"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)newPersistentRef,
                                          (__bridge CFDataRef)newSha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNil((__bridge NSError*)cferror, "Shouldn't error setting current item");
                                              [newSetCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[newSetCurrentExpectation] timeout:20];

    SecResetLocalSecuritydXPCFakeEntitlements();

    //ensure cloudkit doesn't any persistent references
    NSArray<CKRecord*>* items = [self.keychainZone.currentDatabase.allValues filteredArrayUsingPredicate: [NSPredicate predicateWithFormat:@"self.recordType like %@", SecCKRecordItemType]];
    XCTAssertEqual(items.count, 1, "Have %d Items in cloudkit", 1);

    CKRecord* delete = items[0];
    NSDictionary* decryptedItem = [self decryptRecord: delete];

    NSString* prefFromCloudKit = decryptedItem[(__bridge id) kSecAttrPersistentReference];
    XCTAssertNil(prefFromCloudKit, "persistent reference uuid should be nil");

    SecKeychainSetOverrideStaticPersistentRefsIsEnabled(false);
}

@end

#endif // OCTAGON
