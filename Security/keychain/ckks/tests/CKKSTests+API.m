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

#import <CloudKit/CloudKit.h>
#import <XCTest/XCTest.h>
#import <OCMock/OCMock.h>

#include <Security/SecItemPriv.h>
#include <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>
#import <Foundation/NSXPCConnection_Private.h>

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"

#import "keychain/ckks/tests/MockCloudKit.h"

#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CKKSTests+API.h"

@implementation CloudKitKeychainSyncingTests (APITests)

- (void)testSecuritydClientBringup {
    CFErrorRef cferror = nil;
    xpc_endpoint_t endpoint = SecCreateSecuritydXPCServerEndpoint(&cferror);
    XCTAssertNil((__bridge id)cferror, "No error creating securityd endpoint");
    XCTAssertNotNil(endpoint, "Received securityd endpoint");

    NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SecuritydXPCProtocol)];
    [SecuritydXPCClient configureSecuritydXPCProtocol: interface];
    XCTAssertNotNil(interface, "Received a configured CKKS interface");

    NSXPCListenerEndpoint *listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];
    [listenerEndpoint _setEndpoint:endpoint];

    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];
    XCTAssertNotNil(connection , "Received an active connection");

    connection.remoteObjectInterface = interface;
}

-(NSMutableDictionary*)pcsAddItemQuery:(NSString*)account
                                  data:(NSData*)data
                     serviceIdentifier:(NSNumber*)serviceIdentifier
                             publicKey:(NSData*)publicKey
                        publicIdentity:(NSData*)publicIdentity
{
    return [@{
              (id)kSecClass : (id)kSecClassGenericPassword,
              (id)kSecReturnPersistentRef: @YES,
              (id)kSecReturnAttributes: @YES,
              (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
              (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
              (id)kSecAttrAccount : account,
              (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
              (id)kSecValueData : data,
              (id)kSecAttrDeriveSyncIDFromItemAttributes : (id)kCFBooleanTrue,
              (id)kSecAttrPCSPlaintextServiceIdentifier : serviceIdentifier,
              (id)kSecAttrPCSPlaintextPublicKey : publicKey,
              (id)kSecAttrPCSPlaintextPublicIdentity : publicIdentity,
              } mutableCopy];
}

-(NSDictionary*)pcsAddItem:(NSString*)account
                      data:(NSData*)data
         serviceIdentifier:(NSNumber*)serviceIdentifier
                 publicKey:(NSData*)publicKey
            publicIdentity:(NSData*)publicIdentity
             expectingSync:(bool)expectingSync
{
    NSMutableDictionary* query = [self pcsAddItemQuery:account
                                                  data:data
                                     serviceIdentifier:(NSNumber*)serviceIdentifier
                                             publicKey:(NSData*)publicKey
                                        publicIdentity:(NSData*)publicIdentity];
    CFTypeRef result = NULL;
    XCTestExpectation* syncExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, &result, ^(bool didSync, CFErrorRef error) {
        if(expectingSync) {
            XCTAssertTrue(didSync, "Item synced");
            XCTAssertNil((__bridge NSError*)error, "No error syncing item");
        } else {
            XCTAssertFalse(didSync, "Item did not sync");
            XCTAssertNotNil((__bridge NSError*)error, "Error syncing item");
        }

        [syncExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    // Verify that the item was written to CloudKit
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // In real code, you'd need to wait for the _SecItemAddAndNotifyOnSync callback to succeed before proceeding
    [self waitForExpectations:@[syncExpectation] timeout:8.0];

    return (NSDictionary*) CFBridgingRelease(result);
}


- (void)testAddAndNotifyOnSync {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.keychainView waitForKeyHierarchyReadiness];
    [self waitForCKModifications];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced properly");
        XCTAssertNil((__bridge NSError*)error, "No error syncing item");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
}

- (void)testAddAndNotifyOnSyncFailure {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    // Due to item UUID selection, this item will be added with UUID DD7C2F9B-B22D-3B90-C299-E3B48174BFA3.
    // Add it to CloudKit first!
    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3"];
    [self.keychainZone addToZone: ckr];


    // Go for it!
    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertFalse(didSync, "Item did not sync (as expected)");
        XCTAssertNotNil((__bridge NSError*)error, "error exists when item fails to sync");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
    [self waitForCKModifications];
}

- (void)testAddAndNotifyOnSyncLoggedOut {
    // Test starts with nothing in database and the user logged out of CloudKit. We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.silentFetchesAllowed = false;
    [self startCKKSSubsystem];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertFalse(didSync, "Item did not sync (with no iCloud account)");
        XCTAssertNotNil((__bridge NSError*)error, "Error exists syncing item while logged out");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
}

- (BOOL (^) (CKRecord*)) checkPCSFieldsBlock: (CKRecordZoneID*) zoneID
                        PCSServiceIdentifier:(NSNumber*)servIdentifier
                                PCSPublicKey:(NSData*)publicKey
                           PCSPublicIdentity:(NSData*)publicIdentity
{
    __weak __typeof(self) weakSelf = self;
    return ^BOOL(CKRecord* record) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        XCTAssert([record[SecCKRecordPCSServiceIdentifier] isEqual: servIdentifier], "PCS Service identifier matches input");
        XCTAssert([record[SecCKRecordPCSPublicKey]         isEqual: publicKey],      "PCS Public Key matches input");
        XCTAssert([record[SecCKRecordPCSPublicIdentity]    isEqual: publicIdentity], "PCS Public Identity matches input");

        if([record[SecCKRecordPCSServiceIdentifier] isEqual: servIdentifier] &&
           [record[SecCKRecordPCSPublicKey]         isEqual: publicKey] &&
           [record[SecCKRecordPCSPublicIdentity]    isEqual: publicIdentity]) {
            return YES;
        } else {
            return NO;
        }
    };
}

- (void)testPCSUnencryptedFieldsAdd {

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrDeriveSyncIDFromItemAttributes : (id)kCFBooleanTrue,
                                    (id)kSecAttrPCSPlaintextServiceIdentifier : servIdentifier,
                                    (id)kSecAttrPCSPlaintextPublicKey : publicKey,
                                    (id)kSecAttrPCSPlaintextPublicIdentity : publicIdentity,
                                    } mutableCopy];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"SecItemAdd succeeded");

    // Verify that the item is written to CloudKit
    OCMVerifyAllWithDelay(self.mockDatabase, 4);

    CFTypeRef item = NULL;
    query[(id)kSecValueData] = nil;
    query[(id)kSecReturnAttributes] = @YES;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should still exist");

    NSDictionary* itemAttributes = (NSDictionary*) CFBridgingRelease(item);
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextServiceIdentifier], servIdentifier, "Service Identifier exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicKey],         publicKey,      "public key exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicIdentity],    publicIdentity, "public identity exists");

    // Find the item record in CloudKit. Since we're using kSecAttrDeriveSyncIDFromItemAttributes,
    // the record ID is likely DD7C2F9B-B22D-3B90-C299-E3B48174BFA3
    [self waitForCKModifications];
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[recordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    XCTAssertEqualObjects(record[SecCKRecordPCSServiceIdentifier], servIdentifier, "Service identifier sent to cloudkit");
    XCTAssertEqualObjects(record[SecCKRecordPCSPublicKey],         publicKey,      "public key sent to cloudkit");
    XCTAssertEqualObjects(record[SecCKRecordPCSPublicIdentity],    publicIdentity, "public identity sent to cloudkit");
}

- (void)testPCSUnencryptedFieldsModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitUntilAllOperationsAreFinished];

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrDeriveSyncIDFromItemAttributes : (id)kCFBooleanTrue,
                                    (id)kSecAttrPCSPlaintextServiceIdentifier : servIdentifier,
                                    (id)kSecAttrPCSPlaintextPublicKey : publicKey,
                                    (id)kSecAttrPCSPlaintextPublicIdentity : publicIdentity,
                                    } mutableCopy];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"SecItemAdd succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);
    [self waitForCKModifications];

    query[(id)kSecValueData] = nil;
    query[(id)kSecAttrPCSPlaintextServiceIdentifier] = nil;
    query[(id)kSecAttrPCSPlaintextPublicKey] = nil;
    query[(id)kSecAttrPCSPlaintextPublicIdentity] = nil;

    servIdentifier = @1;
    publicKey = [@"new public key" dataUsingEncoding:NSUTF8StringEncoding];

    NSNumber* newServiceIdentifier = @10;
    NSData* newPublicKey = [@"new public key" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* newPublicIdentity = [@"new public identity" dataUsingEncoding:NSUTF8StringEncoding];

    NSDictionary* update = @{
                             (id)kSecAttrPCSPlaintextServiceIdentifier : newServiceIdentifier,
                             (id)kSecAttrPCSPlaintextPublicKey : newPublicKey,
                             (id)kSecAttrPCSPlaintextPublicIdentity : newPublicIdentity,
                             };

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)newServiceIdentifier
                                                  PCSPublicKey:newPublicKey
                                             PCSPublicIdentity:newPublicIdentity]];

    XCTAssertEqual(errSecSuccess, SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef) update), @"SecItemUpdate succeeded");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    CFTypeRef item = NULL;
    query[(id)kSecValueData] = nil;
    query[(id)kSecReturnAttributes] = @YES;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should still exist");

    NSDictionary* itemAttributes = (NSDictionary*) CFBridgingRelease(item);
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextServiceIdentifier], newServiceIdentifier, "Service Identifier exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicKey],         newPublicKey,         "public key exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicIdentity],    newPublicIdentity,    "public identity exists");

    // Find the item record in CloudKit. Since we're using kSecAttrDeriveSyncIDFromItemAttributes,
    // the record ID is likely DD7C2F9B-B22D-3B90-C299-E3B48174BFA3
    [self waitForCKModifications];
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[recordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    XCTAssertEqualObjects(record[SecCKRecordPCSServiceIdentifier], newServiceIdentifier, "Service identifier sent to cloudkit");
    XCTAssertEqualObjects(record[SecCKRecordPCSPublicKey],         newPublicKey,         "public key sent to cloudkit");
    XCTAssertEqualObjects(record[SecCKRecordPCSPublicIdentity],    newPublicIdentity,    "public identity sent to cloudkit");
}

// As of [<rdar://problem/32558310> CKKS: Re-authenticate PCSPublicFields], these fields are NOT server-modifiable. This test proves it.
- (void)testPCSUnencryptedFieldsServerModifyFail {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrDeriveSyncIDFromItemAttributes : (id)kCFBooleanTrue,
                                    (id)kSecAttrPCSPlaintextServiceIdentifier : servIdentifier,
                                    (id)kSecAttrPCSPlaintextPublicKey : publicKey,
                                    (id)kSecAttrPCSPlaintextPublicIdentity : publicIdentity,
                                    } mutableCopy];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"SecItemAdd succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 4);
    [self waitForCKModifications];

    // Find the item record in CloudKit. Since we're using kSecAttrDeriveSyncIDFromItemAttributes,
    // the record ID is likely DD7C2F9B-B22D-3B90-C299-E3B48174BFA3
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: @"DD7C2F9B-B22D-3B90-C299-E3B48174BFA3" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[recordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    // Items are encrypted using encv2
    XCTAssertEqualObjects(record[SecCKRecordEncryptionVersionKey], [NSNumber numberWithInteger:(int) CKKSItemEncryptionVersion2], "Uploaded using encv2");

    if(!record) {
        // Test has already failed; find the record just to be nice.
        for(CKRecord* maybe in self.keychainZone.currentDatabase.allValues) {
            if(maybe[SecCKRecordPCSServiceIdentifier] != nil) {
                record = maybe;
            }
        }
    }

    NSNumber* newServiceIdentifier = @10;
    NSData* newPublicKey = [@"new public key" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* newPublicIdentity = [@"new public identity" dataUsingEncoding:NSUTF8StringEncoding];

    // Change the public key and public identity
    record = [record copyWithZone: nil];
    record[SecCKRecordPCSServiceIdentifier] = newServiceIdentifier;
    record[SecCKRecordPCSPublicKey] = newPublicKey;
    record[SecCKRecordPCSPublicIdentity] = newPublicIdentity;
    [self.keychainZone addToZone: record];

    // Trigger a notification
    [self.keychainView notifyZoneChange:nil];
    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    CFTypeRef item = NULL;
    query[(id)kSecValueData] = nil;
    query[(id)kSecAttrPCSPlaintextServiceIdentifier] = nil;
    query[(id)kSecAttrPCSPlaintextPublicKey] = nil;
    query[(id)kSecAttrPCSPlaintextPublicIdentity] = nil;
    query[(id)kSecReturnAttributes] = @YES;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should still exist");

    NSDictionary* itemAttributes = (NSDictionary*) CFBridgingRelease(item);
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextServiceIdentifier], servIdentifier, "service identifier is not updated");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicKey],         publicKey,      "public key not updated");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicIdentity],    publicIdentity, "public identity not updated");
}

-(void)testPCSUnencryptedFieldsRecieveUnauthenticatedFields {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    NSError* error = nil;

    // Manually encrypt an item
    NSString* recordName = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.keychainZoneID];
    NSDictionary* item = [self fakeRecordDictionary: @"account-delete-me" zoneID:self.keychainZoneID];
    CKKSItem* cipheritem = [[CKKSItem alloc] initWithUUID:recordID.recordName
                                            parentKeyUUID:self.keychainZoneKeys.classC.uuid
                                                   zoneID:recordID.zoneID];
    CKKSKey* itemkey = [CKKSKey randomKeyWrappedByParent: self.keychainZoneKeys.classC error:&error];
    XCTAssertNotNil(itemkey, "Got a key");
    cipheritem.wrappedkey = itemkey.wrappedkey;
    XCTAssertNotNil(cipheritem.wrappedkey, "Got a wrapped key");

    cipheritem.encver = CKKSItemEncryptionVersion1;

    // This item has the PCS public fields, but they are not authenticated
    cipheritem.plaintextPCSServiceIdentifier = servIdentifier;
    cipheritem.plaintextPCSPublicKey = publicKey;
    cipheritem.plaintextPCSPublicIdentity = publicIdentity;

    NSDictionary<NSString*, NSData*>* authenticatedData = [cipheritem makeAuthenticatedDataDictionaryUpdatingCKKSItem: nil encryptionVersion:CKKSItemEncryptionVersion1];
    cipheritem.encitem = [CKKSItemEncrypter encryptDictionary:item key:itemkey.aessivkey authenticatedData:authenticatedData error:&error];
    XCTAssertNil(error, "no error encrypting object");
    XCTAssertNotNil(cipheritem.encitem, "Recieved ciphertext");

    [self.keychainZone addToZone:[cipheritem CKRecordWithZoneID: recordID.zoneID]];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    NSDictionary* query = @{(id)kSecClass: (id)kSecClassGenericPassword,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrAccount: @"account-delete-me",
                            (id)kSecMatchLimit: (id)kSecMatchLimitOne,
                            };
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Found synced item");

    NSDictionary* result = CFBridgingRelease(cfresult);
    XCTAssertEqualObjects(result[(id)kSecAttrPCSPlaintextServiceIdentifier], servIdentifier, "Received PCS service identifier");
    XCTAssertEqualObjects(result[(id)kSecAttrPCSPlaintextPublicKey],         publicKey,      "Received PCS public key");
    XCTAssertEqualObjects(result[(id)kSecAttrPCSPlaintextPublicIdentity],    publicIdentity, "Received PCS public identity");
}

-(void)testPCSUnencryptedFieldsRecieveAuthenticatedFields {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.keychainView waitForKeyHierarchyReadiness];

    NSNumber* servIdentifier = @3;
    NSData* publicKey = [@"asdfasdf" dataUsingEncoding:NSUTF8StringEncoding];
    NSData* publicIdentity = [@"somedata" dataUsingEncoding:NSUTF8StringEncoding];

    NSError* error = nil;

    // Manually encrypt an item
    NSString* recordName = @"7B598D31-F9C5-481E-98AC-5A507ACB2D85";
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName:recordName zoneID:self.keychainZoneID];
    NSDictionary* item = [self fakeRecordDictionary: @"account-delete-me" zoneID:self.keychainZoneID];
    CKKSItem* cipheritem = [[CKKSItem alloc] initWithUUID:recordID.recordName
                                            parentKeyUUID:self.keychainZoneKeys.classC.uuid
                                                   zoneID:recordID.zoneID];
    CKKSKey* itemkey = [CKKSKey randomKeyWrappedByParent: self.keychainZoneKeys.classC error:&error];
    XCTAssertNotNil(itemkey, "Got a key");
    cipheritem.wrappedkey = itemkey.wrappedkey;
    XCTAssertNotNil(cipheritem.wrappedkey, "Got a wrapped key");

    cipheritem.encver = CKKSItemEncryptionVersion2;

    // This item has the PCS public fields, and they are authenticated (since we're using v2)
    cipheritem.plaintextPCSServiceIdentifier = servIdentifier;
    cipheritem.plaintextPCSPublicKey = publicKey;
    cipheritem.plaintextPCSPublicIdentity = publicIdentity;

    // Use version 2, so PCS plaintext fields will be authenticated
    NSMutableDictionary<NSString*, NSData*>* authenticatedData = [[cipheritem makeAuthenticatedDataDictionaryUpdatingCKKSItem: nil encryptionVersion:CKKSItemEncryptionVersion2] mutableCopy];

    cipheritem.encitem = [CKKSItemEncrypter encryptDictionary:item key:itemkey.aessivkey authenticatedData:authenticatedData error:&error];
    XCTAssertNil(error, "no error encrypting object");
    XCTAssertNotNil(cipheritem.encitem, "Recieved ciphertext");

    [self.keychainZone addToZone:[cipheritem CKRecordWithZoneID: recordID.zoneID]];

    [self.keychainView waitForFetchAndIncomingQueueProcessing];

    NSDictionary* query = @{(id)kSecClass: (id)kSecClassGenericPassword,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrAccount: @"account-delete-me",
                            (id)kSecMatchLimit: (id)kSecMatchLimitOne,
                            };
    CFTypeRef cfresult = NULL;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &cfresult), "Found synced item");

    NSDictionary* result = CFBridgingRelease(cfresult);
    XCTAssertEqualObjects(result[(id)kSecAttrPCSPlaintextServiceIdentifier], servIdentifier, "Received PCS service identifier");
    XCTAssertEqualObjects(result[(id)kSecAttrPCSPlaintextPublicKey],         publicKey,      "Received PCS public key");
    XCTAssertEqualObjects(result[(id)kSecAttrPCSPlaintextPublicIdentity],    publicIdentity, "Received PCS public identity");

    // Test that if this item is updated, it remains encrypted in v2
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkPCSFieldsBlock:self.keychainZoneID
                                          PCSServiceIdentifier:(NSNumber *)servIdentifier
                                                  PCSPublicKey:publicKey
                                             PCSPublicIdentity:publicIdentity]];
    [self updateGenericPassword:@"different password" account:@"account-delete-me"];

    OCMVerifyAllWithDelay(self.mockDatabase, 4);
    [self waitForCKModifications];

    CKRecord* newRecord = self.keychainZone.currentDatabase[recordID];
    XCTAssertEqualObjects(newRecord[SecCKRecordPCSServiceIdentifier], servIdentifier, "Didn't change service identifier");
    XCTAssertEqualObjects(newRecord[SecCKRecordPCSPublicKey],         publicKey,      "Didn't change public key");
    XCTAssertEqualObjects(newRecord[SecCKRecordPCSPublicIdentity],    publicIdentity, "Didn't change public identity");
    XCTAssertEqualObjects(newRecord[SecCKRecordEncryptionVersionKey], [NSNumber numberWithInteger:(int) CKKSItemEncryptionVersion2], "Uploaded using encv2");
}

-(void)testResetLocal {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // After the local reset, we expect: a fetch, then nothing
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    dispatch_semaphore_t resetSemaphore = dispatch_semaphore_create(0);
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting local");
        secnotice("ckks", "Received a rpcResetLocal callback");
        dispatch_semaphore_signal(resetSemaphore);
    }];

    XCTAssertEqual(0, dispatch_semaphore_wait(resetSemaphore, 4*NSEC_PER_SEC), "Semaphore wait did not time out");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

-(void)testResetLocalWhileLoggedOut {
    // We're "logged in to" cloudkit but not in circle.
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    self.silentFetchesAllowed = false;

    // Test starts with local TLK and key hierarhcy in our fake cloudkit
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    NSData* changeTokenData = [[[NSUUID UUID] UUIDString] dataUsingEncoding:NSUTF8StringEncoding];
    CKServerChangeToken* changeToken = [[CKServerChangeToken alloc] initWithData:changeTokenData];
    [self.keychainView dispatchSync: ^bool{
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.keychainView.zoneName];
        ckse.changeToken = changeToken;

        NSError* error = nil;
        [ckse saveToDatabase:&error];
        XCTAssertNil(error, "No error saving new zone state to database");
    }];

    dispatch_semaphore_t resetSemaphore = dispatch_semaphore_create(0);
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        secnotice("ckks", "Received a rpcResetLocal callback");
        dispatch_semaphore_signal(resetSemaphore);
    }];

    XCTAssertEqual(0, dispatch_semaphore_wait(resetSemaphore, 400*NSEC_PER_SEC), "Semaphore wait did not time out");

    [self.keychainView dispatchSync: ^bool{
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:self.keychainView.zoneName];
        XCTAssertNotEqualObjects(changeToken, ckse.changeToken, "Change token is reset");
    }];

    // Now log in, and see what happens! It should re-fetch, pick up the old key hierarchy, and use it
    self.silentFetchesAllowed = true;
    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

-(void)testResetCloudKitZone {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // We expect a key hierarchy upload, and then the class C item upload
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];
        [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    dispatch_semaphore_t resetSemaphore = dispatch_semaphore_create(0);
    [self.injectedManager rpcResetCloudKit:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        secnotice("ckks", "Received a resetCloudKit callback");
        dispatch_semaphore_signal(resetSemaphore);
    }];

    XCTAssertEqual(0, dispatch_semaphore_wait(resetSemaphore, 4*NSEC_PER_SEC), "Semaphore wait did not time out");

    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

-(void)testResetCloudKitZoneWhileLoggedOut {
    // We're "logged in to" cloudkit but not in circle.
    self.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    self.silentFetchesAllowed = false;

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    XCTAssertNotNil(self.keychainZone.currentDatabase, "Zone exists");
    XCTAssertNotNil(self.keychainZone.currentDatabase[ckr.recordID], "An item exists in the fake zone");

    dispatch_semaphore_t resetSemaphore = dispatch_semaphore_create(0);
    [self.injectedManager rpcResetCloudKit:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        secnotice("ckks", "Received a resetCloudKit callback");
        dispatch_semaphore_signal(resetSemaphore);
    }];

    XCTAssertEqual(0, dispatch_semaphore_wait(resetSemaphore, 400*NSEC_PER_SEC), "Semaphore wait did not time out");

    XCTAssertNil(self.keychainZone.currentDatabase, "No zone anymore!");
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    // Now log in, and see what happens! It should create the zone again and upload a whole new key hierarchy
    self.silentFetchesAllowed = true;
    self.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 zoneID:self.keychainZoneID];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 8);
}

@end

#endif // OCTAGON
