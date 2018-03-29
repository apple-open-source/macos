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

@interface CloudKitKeychainSyncingCurrentPointerAPITests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainSyncingCurrentPointerAPITests

-(void)fetchCurrentPointer:(bool)cached persistentRef:(NSData*)persistentRef
{
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"callback occurs"];
    SecItemFetchCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                            (__bridge CFStringRef)@"pcsservice",
                                            (__bridge CFStringRef)@"keychain",
                                            cached,
                                            ^(CFDataRef currentPersistentRef, CFErrorRef cferror) {
                                                XCTAssertNotNil((__bridge id)currentPersistentRef, "current item exists");
                                                XCTAssertNil((__bridge id)cferror, "no error exists when there's a current item");
                                                XCTAssertEqualObjects(persistentRef, (__bridge id)currentPersistentRef, "persistent ref matches expected persistent ref");
                                                [currentExpectation fulfill];
                                            });
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
}
-(void)fetchCurrentPointerExpectingError:(bool)fetchCloudValue
{
    XCTestExpectation* currentExpectation = [self expectationWithDescription: @"callback occurs"];
    TEST_API_AUTORELEASE_BEFORE(SecItemFetchCurrentItemAcrossAllDevices);
    SecItemFetchCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                            (__bridge CFStringRef)@"pcsservice",
                                            (__bridge CFStringRef)@"keychain",
                                            fetchCloudValue,
                                            ^(CFDataRef currentPersistentRef, CFErrorRef cferror) {
                                                XCTAssertNil((__bridge id)currentPersistentRef, "no current item exists");
                                                XCTAssertNotNil((__bridge id)cferror, "Error exists when there's a current item");
                                                [currentExpectation fulfill];
                                            });
    TEST_API_AUTORELEASE_AFTER(SecItemFetchCurrentItemAcrossAllDevices);
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
}

- (void)testPCSFetchCurrentPointerCachedAndUncached {
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    [self.keychainView waitForKeyHierarchyReadiness];
    [self.keychainView waitUntilAllOperationsAreFinished]; // ensure everything finishes before we disallow fetches

    // Ensure that local queries don't hit the server.
    self.silentFetchesAllowed = false;
    [self fetchCurrentPointerExpectingError:false];

    // And ensure that global queries do.
    [self expectCKFetch];
    [self fetchCurrentPointerExpectingError:true];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self waitForCKModifications];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    CKRecord* currentItemPointer = self.keychainZone.currentDatabase[[[CKRecordID alloc] initWithRecordName:@"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID]];
    XCTAssertNotNil(currentItemPointer, "Found a CKRecord at the expected location in CloudKit");
    XCTAssertEqualObjects(currentItemPointer.recordType, SecCKRecordCurrentItemType, "Saved CKRecord is correct type");
    XCTAssertEqualObjects(((CKReference*)currentItemPointer[SecCKRecordItemRefKey]).recordID, pcsItemRecordID, "Current Item record points to correct record");

    // Check that the status APIs return the right value
    [self fetchCurrentPointer:false persistentRef:persistentRef];

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
    CKRecordID* pcsOtherItemRecordID = [[CKRecordID alloc] initWithRecordName: @"878BEAA6-1EE9-1079-1025-E6832AC8F2F3" zoneID:self.keychainZoneID];
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
        secnotice("ckks", "SHA1s don't match, but why?");
    }

    XCTestExpectation* otherSetCurrentExpectation = [self expectationWithDescription: @"callback occurs"];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self waitForCKModifications];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    currentItemPointer = self.keychainZone.currentDatabase[[[CKRecordID alloc] initWithRecordName:@"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID]];
    XCTAssertNotNil(currentItemPointer, "Found a CKRecord at the expected location in CloudKit");
    XCTAssertEqualObjects(currentItemPointer.recordType, SecCKRecordCurrentItemType, "Saved CKRecord is correct type");
    XCTAssertEqualObjects(((CKReference*)currentItemPointer[SecCKRecordItemRefKey]).recordID, pcsOtherItemRecordID, "Current Item record points to updated record");

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
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], @"Key state should become 'ready'");

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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
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
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], @"Key state should become 'ready'");

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self waitForCKModifications];

    [self waitForExpectationsWithTimeout:8.0 handler:nil];

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
    self.circleStatus = kSOSCCNotInCircle;
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
    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

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
    [self waitForExpectations:@[syncExpectation] timeout:2.0];

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

    [self waitForExpectations:@[setCurrentExpectation] timeout:8.0];
    SecResetLocalSecuritydXPCFakeEntitlements();
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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    CKRecordID* pcsItemRecordID2 = [[CKRecordID alloc] initWithRecordName: @"3AB8E78D-75AF-CFEF-F833-FA3E3E90978A" zoneID:self.keychainZoneID];
    CKRecord* record2 = self.keychainZone.currentDatabase[pcsItemRecordID2];
    XCTAssertNotNil(record2, "Found 2nd record in CloudKit at expected UUID");

    // Still no current pointer.
    [self fetchCurrentPointerExpectingError:false];

    // Another machine comes along and updates the pointer!
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                    currentItemUUID:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord = self.keychainZone.currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found record in CloudKit at expected UUID");

    // Ensure that receiving the current item pointer generates a notification
    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self waitForExpectations:@[keychainChanged] timeout:8];
    [self fetchCurrentPointer:false persistentRef:persistentRef];

    // And again!
    CKKSCurrentItemPointer* cip2 = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                    currentItemUUID:pcsItemRecordID2.recordName
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip2 CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID2 = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord2 = self.keychainZone.currentDatabase[currentPointerRecordID2];
    XCTAssertNotNil(currentPointerRecord2, "Found record in CloudKit at expected UUID");

    keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    // Still no current pointer.
    [self fetchCurrentPointerExpectingError:false];

    // Another machine comes along and updates the pointer!
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
                                                                    currentItemUUID:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3"
                                                                              state:SecCKKSProcessedStateRemote
                                                                             zoneID:self.keychainZoneID
                                                                    encodedCKRecord:nil];
    [self.keychainZone addToZone: [cip CKRecordWithZoneID:self.keychainZoneID]];
    CKRecordID* currentPointerRecordID = [[CKRecordID alloc] initWithRecordName: @"com.apple.security.ckks-pcsservice" zoneID:self.keychainZoneID];
    CKRecord* currentPointerRecord = self.keychainZone.currentDatabase[currentPointerRecordID];
    XCTAssertNotNil(currentPointerRecord, "Found record in CloudKit at expected UUID");

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    [self fetchCurrentPointer:false persistentRef:persistentRef];

    // Ensure that receiving the current item pointer generates a notification
    XCTestExpectation* keychainChanged = [self expectChangeForView:self.keychainZoneID.zoneName];

    // Another machine comes along and deletes the pointer!
    [self.keychainZone deleteCKRecordIDFromZone: currentPointerRecordID];
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];
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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    NSString* recordUUID = @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3";
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName:recordUUID zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    // Someone else sets the current record pointer
    CKKSCurrentItemPointer* cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:@"com.apple.security.ckks-pcsservice"
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];
    [self waitForExpectationsWithTimeout:8.0 handler:nil];

    [self.keychainView waitUntilAllOperationsAreFinished];

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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
           runAfterModification:nil];

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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForExpectationsWithTimeout:8.0 handler:nil];
    [self waitForCKModifications];

    // Set the 'was current' flag on the record
    CKRecord* modifiedRecord = [record copy];
    modifiedRecord[SecCKRecordServerWasCurrent] = [NSNumber numberWithInteger:10];
    [self.keychainZone addToZone:modifiedRecord];

    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Check that the number is on the CKKSMirrorEntry
    [self.keychainView dispatchSync: ^bool {
        NSError* error = nil;
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID error:&error];

        XCTAssertNil(error, "no error fetching ckme");
        XCTAssertNotNil(ckme, "Received a ckme");

        XCTAssertEqual(ckme.wasCurrent, 10u, "Properly received wasCurrent");

        return true;
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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

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
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];

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
    CKRecordID* pcsItemRecordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[pcsItemRecordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    [self expectCKModifyRecords:@{SecCKRecordCurrentItemType: [NSNumber numberWithUnsignedInteger: 1]}
        deletedRecordTypeCounts:nil
                         zoneID:self.keychainZoneID
            checkModifiedRecord:nil
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

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
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
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
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:8*NSEC_PER_SEC], "Key state should have become ready");
    [self.keychainView waitUntilAllOperationsAreFinished];

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
    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* mismatchedRecord = [self newRecord:ckrid withNewItemData:item];
    [self.keychainZone addToZone: mismatchedRecord];

    self.keychainView.holdIncomingQueueOperation = [CKKSResultOperation named:@"hold-incoming" withBlock:^{
        secnotice("ckks", "Releasing process incoming queue hold");
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

    NSData* persistentRef = result[(id)kSecValuePersistentRef];
    NSData* sha1 = result[(id)kSecAttrSHA1];

    // Ensure that fetching the item without grabbing data returns the same SHA1
    NSDictionary* prefquery = @{(id)kSecClass : (id)kSecClassGenericPassword,
                                (id)kSecReturnAttributes : @YES,
                                (id)kSecAttrSynchronizable : @YES,
                                (id)kSecAttrPersistentReference : persistentRef,
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
                  (id)kSecAttrSynchronizable : @YES,
                  (id)kSecAttrPersistentReference : persistentRef,
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

    [self waitForExpectations:@[setCurrentExpectation] timeout:8.0];

    // Now, release the incoming queue processing and retry the failure
    [self.operationQueue addOperation:self.keychainView.holdIncomingQueueOperation];
    [self.keychainView waitForOperationsOfClass:[CKKSIncomingQueueOperation class]];

    setCurrentExpectation = [self expectationWithDescription: @"callback occurs after incoming queue operation"];
    SecItemSetCurrentItemAcrossAllDevices((__bridge CFStringRef)@"com.apple.security.ckks",
                                          (__bridge CFStringRef)@"pcsservice",
                                          (__bridge CFStringRef)@"keychain",
                                          (__bridge CFDataRef)persistentRef,
                                          (__bridge CFDataRef)sha1, NULL, NULL, ^ (CFErrorRef cferror) {
                                              XCTAssertNotNil((__bridge NSError*)cferror, "Should error setting current item to hash of item which failed to sync (after incoming queue operation)");
                                              [setCurrentExpectation fulfill];
                                          });

    [self waitForExpectations:@[setCurrentExpectation] timeout:8.0];

    // Reissue a fetch and find the new persistent ref and sha1 for the item at this UUID
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

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

    [self waitForExpectations:@[newSetCurrentExpectation] timeout:8.0];

    SecResetLocalSecuritydXPCFakeEntitlements();
}

@end

#endif // OCTAGON
