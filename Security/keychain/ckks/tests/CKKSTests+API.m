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
#include "OSX/sec/Security/SecItemShim.h"

#include <Security/SecEntitlements.h>
#include <ipc/server_security_helpers.h>
#import <Foundation/NSXPCConnection_Private.h>

#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"

#import "keychain/ckks/CKKSControl.h"
#import "keychain/ckks/CloudKitCategories.h"

#import "keychain/ckks/tests/MockCloudKit.h"
#import "keychain/ckks/tests/CKKSTests.h"
#import "keychain/ckks/tests/CKKSTests+API.h"

@implementation CloudKitKeychainSyncingTestsBase (APITests)

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
              (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // In real code, you'd need to wait for the _SecItemAddAndNotifyOnSync callback to succeed before proceeding
    [self waitForExpectations:@[syncExpectation] timeout:20];

    return (NSDictionary*) CFBridgingRelease(result);
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
@end

@interface CloudKitKeychainSyncingAPITests : CloudKitKeychainSyncingTestsBase
@end

@implementation CloudKitKeychainSyncingAPITests
- (void)testSecuritydClientBringup {
#if 0
    CFErrorRef cferror = nil;
    xpc_endpoint_t endpoint = SecCreateSecuritydXPCServerEndpoint(&cferror);
    XCTAssertNil((__bridge id)cferror, "No error creating securityd endpoint");
    XCTAssertNotNil(endpoint, "Received securityd endpoint");
#endif

    NSXPCInterface *interface = [NSXPCInterface interfaceWithProtocol:@protocol(SecuritydXPCProtocol)];
    [SecuritydXPCClient configureSecuritydXPCProtocol: interface];
    XCTAssertNotNil(interface, "Received a configured CKKS interface");

#if 0
    NSXPCListenerEndpoint *listenerEndpoint = [[NSXPCListenerEndpoint alloc] init];
    [listenerEndpoint _setEndpoint:endpoint];

    NSXPCConnection* connection = [[NSXPCConnection alloc] initWithListenerEndpoint:listenerEndpoint];
    XCTAssertNotNil(connection , "Received an active connection");

    connection.remoteObjectInterface = interface;
#endif
}

- (void)testAddAndNotifyOnSync {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.defaultCKKS waitForKeyHierarchyReadiness];
    [self waitForCKModifications];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced properly");
        XCTAssertNil((__bridge NSError*)error, "No error syncing item");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 10);

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
}

- (void)testAddAndNotifyOnSyncSkipsQueue {
    // Use the PCS plaintext fields to determine which object is which
    SecResetLocalSecuritydXPCFakeEntitlements();
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSPlaintextFields, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSWriteCurrentItemPointers, kCFBooleanTrue);
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementPrivateCKKSReadCurrentItemPointers, kCFBooleanTrue);

    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];
    [self waitForCKModifications];
    OCMVerifyAllWithDelay(self.mockDatabase, 40);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateProcessOutgoingQueue];

    for(size_t count = 0; count < 150; count++) {
        [self addGenericPassword:@"data" account:[NSString stringWithFormat:@"account-delete-me-%03lu", count]];
    }

    NSMutableDictionary* query = [@{
        (id)kSecClass : (id)kSecClassGenericPassword,
        (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (id)kSecAttrAccount : @"testaccount",
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
        (id)kSecAttrPCSPlaintextPublicKey : [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced properly");
        XCTAssertNil((__bridge NSError*)error, "No error syncing item");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");


    XCTestExpectation* firstQueueOperation = [self expectationWithDescription: @"found the item in the first queue iteration"];
    [self expectCKModifyItemRecords:SecCKKSOutgoingQueueItemsAtOnce
           currentKeyPointerRecords:1
                             zoneID:self.keychainZoneID
                          checkItem:^BOOL(CKRecord * _Nonnull record) {
        if(record[SecCKRecordPCSPublicKey]) {
            [firstQueueOperation fulfill];
        }
        return YES;
    }];
    [self expectCKModifyItemRecords:51 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    // Release the hounds
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateProcessOutgoingQueue] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'CKKSStateProcessOutgoingQueue'");
    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateProcessOutgoingQueue];

    [self waitForExpectations:@[blockExpectation, firstQueueOperation] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 10);

    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:10*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
}

- (void)testAddAndNotifyOnSyncFailure {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

    // Due to item UUID selection, this item will be added with UUID 50184A35-4480-E8BA-769B-567CF72F1EC0.
    // Add it to CloudKit first!
    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"50184A35-4480-E8BA-769B-567CF72F1EC0"];
    [self.keychainZone addToZone: ckr];


    // Go for it!
    [self expectCKAtomicModifyItemRecordsUpdateFailure: self.keychainZoneID];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName, // @ fake view hint for fake view
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

    XCTAssertEqual(0, [self.defaultCKKS.loggedOut wait:20*NSEC_PER_SEC], "CKKS should positively log out");

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
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

- (void)testAddAndNotifyOnSyncAccountStatusUnclear {
    // Test starts with nothing in database, but CKKS hasn't been told we've logged out yet.
    // We expect no CKKS operations.
    self.accountStatus = CKAccountStatusNoAccount;
    self.silentFetchesAllowed = false;

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertFalse(didSync, "Item did not sync (with no iCloud account)");
        XCTAssertNotNil((__bridge NSError*)error, "Error exists syncing item while logged out");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    // And now, allow CKKS to discover we're logged out
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.defaultCKKS.loggedOut wait:20*NSEC_PER_SEC], "CKKS should positively log out");

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
}

- (void)testAddAndNotifyOnSyncBeforeKeyHierarchyReady {
    // Test starts with a key hierarchy in cloudkit and the TLK having arrived
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    // But block CloudKit fetches (so the key hierarchy won't be ready when we add this new item)
    [self holdCloudKitFetches];

    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.defaultCKKS.loggedIn wait:20*NSEC_PER_SEC], "CKKS should log in");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateFetch] wait:20*NSEC_PER_SEC], @"Should have reached key state 'fetch', but no further");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateFetch] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'fetch', but no further");

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced");
        XCTAssertNil((__bridge NSError*)error, "Shouldn't have received an error syncing item");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    // We should be in the 'fetch' state, but no further
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateFetch] wait:20*NSEC_PER_SEC], @"Should have reached key state 'fetch', but no further");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateFetch] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'fetch', but no further");

    // When we release the fetch, the callback should still fire and the item should upload
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID];
    [self releaseCloudKitFetchHold];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Should have reached key state 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

    // Verify that the item was written to CloudKit
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForExpectationsWithTimeout:5.0 handler:nil];
}

- (void)testAddAndNotifyOnSyncBeforePolicyLoaded {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    [self startCKKSSubsystem];

    // Allow CKKS to spin up fully
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Simulate a daemon restart
    self.automaticallyBeginCKKSViewCloudKitOperation = false;
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSyncWithoutSettingPolicy:self.defaultCKKS];

    // Issue the query (to simulate the query starting securityd)
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced");
        XCTAssertNil((__bridge NSError*)error, "Shouldn't have received an error syncing item");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    // When the policy is loaded, the item should upload and the callback should fire
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    [self.defaultCKKS setCurrentSyncingPolicy:self.viewSortingPolicyForManagedViewList];
    self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];

    [self.defaultCKKS beginCloudKitOperation];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Should have reached key state 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self waitForExpectations:@[blockExpectation] timeout:5];
}

- (void)testAddAndNotifyOnSyncReaddAtSameUUIDAfterDeleteItem {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID]; // Make life easy for this test.

    __block CKRecordID* itemRecordID = nil;
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        itemRecordID = record.recordID;
        return YES;
    }];

    [self startCKKSSubsystem];

    // Let things shake themselves out.
    [self.defaultCKKS waitForKeyHierarchyReadiness];
    [self waitForCKModifications];

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                                    (id)kSecAttrAccount : @"testaccount",
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName,
                                    (id)kSecValueData : (id) [@"asdf" dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    XCTestExpectation* blockExpectation = [self expectationWithDescription: @"callback occurs"];

    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced properly");
        XCTAssertNil((__bridge NSError*)error, "No error syncing item");

        [blockExpectation fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 10);

    [self waitForExpectations:@[blockExpectation] timeout:5];

    // And the item is deleted
    [self expectCKDeleteItemRecords:1 zoneID:self.keychainZoneID];
    [self deleteGenericPassword:@"testaccount"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And the item is readded. It should come back to its previous UUID.
    XCTAssertNotNil(itemRecordID, "Should have an item record ID");
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:^BOOL(CKRecord * _Nonnull record) {
        XCTAssertEqualObjects(itemRecordID.recordName, record.recordID.recordName, "Uploaded item UUID should match previous upload");
        return YES;
    }];
    XCTestExpectation* blockExpectation2 = [self expectationWithDescription: @"callback occurs"];
    XCTAssertEqual(errSecSuccess, _SecItemAddAndNotifyOnSync((__bridge CFDictionaryRef) query, NULL, ^(bool didSync, CFErrorRef error) {
        XCTAssertTrue(didSync, "Item synced properly");
        XCTAssertNil((__bridge NSError*)error, "No error syncing item");

        [blockExpectation2 fulfill];
    }), @"_SecItemAddAndNotifyOnSync succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 10);
    [self waitForExpectations:@[blockExpectation2] timeout:5];
}

- (void)testPCSUnencryptedFieldsAdd {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

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
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName, // allows a CKKSScanOperation to find this item
                                    } mutableCopy];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"SecItemAdd succeeded");

    // Verify that the item is written to CloudKit
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    CFTypeRef item = NULL;
    query[(id)kSecValueData] = nil;
    query[(id)kSecReturnAttributes] = @YES;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should still exist");

    NSDictionary* itemAttributes = (NSDictionary*) CFBridgingRelease(item);
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextServiceIdentifier], servIdentifier, "Service Identifier exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicKey],         publicKey,      "public key exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicIdentity],    publicIdentity, "public identity exists");

    // Find the item record in CloudKit. Since we're using kSecAttrDeriveSyncIDFromItemAttributes,
    // the record ID is likely 50184A35-4480-E8BA-769B-567CF72F1EC0
    [self waitForCKModifications];
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
    CKRecord* record = self.keychainZone.currentDatabase[recordID];
    XCTAssertNotNil(record, "Found record in CloudKit at expected UUID");

    XCTAssertEqualObjects(record[SecCKRecordPCSServiceIdentifier], servIdentifier, "Service identifier sent to cloudkit");
    XCTAssertEqualObjects(record[SecCKRecordPCSPublicKey],         publicKey,      "public key sent to cloudkit");
    XCTAssertEqualObjects(record[SecCKRecordPCSPublicIdentity],    publicIdentity, "public identity sent to cloudkit");
}

- (void)testPCSUnencryptedFieldsModify {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];
    [self.defaultCKKS waitForKeyHierarchyReadiness];

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
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName, // allows a CKKSScanOperation to find this item
                                    } mutableCopy];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"SecItemAdd succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    query[(id)kSecValueData] = nil;
    query[(id)kSecAttrPCSPlaintextServiceIdentifier] = nil;
    query[(id)kSecAttrPCSPlaintextPublicKey] = nil;
    query[(id)kSecAttrPCSPlaintextPublicIdentity] = nil;

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
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    CFTypeRef item = NULL;
    query[(id)kSecValueData] = nil;
    query[(id)kSecReturnAttributes] = @YES;
    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &item), "item should still exist");

    NSDictionary* itemAttributes = (NSDictionary*) CFBridgingRelease(item);
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextServiceIdentifier], newServiceIdentifier, "Service Identifier exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicKey],         newPublicKey,         "public key exists");
    XCTAssertEqualObjects(itemAttributes[(id)kSecAttrPCSPlaintextPublicIdentity],    newPublicIdentity,    "public identity exists");

    // Find the item record in CloudKit. Since we're using kSecAttrDeriveSyncIDFromItemAttributes,
    // the record ID is likely 50184A35-4480-E8BA-769B-567CF72F1EC0
    [self waitForCKModifications];
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
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
    [self.defaultCKKS waitForKeyHierarchyReadiness];

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
                                    (id)kSecAttrSyncViewHint : self.keychainView.zoneName, // fake, for CKKSScanOperation
                                    } mutableCopy];

    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"SecItemAdd succeeded");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // Find the item record in CloudKit. Since we're using kSecAttrDeriveSyncIDFromItemAttributes,
    // the record ID is likely 50184A35-4480-E8BA-769B-567CF72F1EC0
    CKRecordID* recordID = [[CKRecordID alloc] initWithRecordName: @"50184A35-4480-E8BA-769B-567CF72F1EC0" zoneID:self.keychainZoneID];
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
    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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
    [self.defaultCKKS waitForKeyHierarchyReadiness];

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
                                                contextID:self.defaultCKKS.operationDependencies.contextID
                                                   zoneID:recordID.zoneID];
    CKKSKeychainBackedKey* itemkey = [CKKSKeychainBackedKey randomKeyWrappedByParent:[self.keychainZoneKeys.classC getKeychainBackedKey:&error]
                                                                                      error:&error];
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

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"key state should enter 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");

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
                                                contextID:self.defaultCKKS.operationDependencies.contextID
                                                   zoneID:recordID.zoneID];
    CKKSKeychainBackedKey* itemkey = [CKKSKeychainBackedKey randomKeyWrappedByParent:[self.keychainZoneKeys.classC getKeychainBackedKey:&error]
                                                                                      error:&error];
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

    [self.defaultCKKS.zoneChangeFetcher notifyZoneChange:nil];
    [self.defaultCKKS waitForFetchAndIncomingQueueProcessing];

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

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
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
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // After the local reset, we expect: a fetch, then nothing
    self.silentFetchesAllowed = false;
    [self expectCKFetch];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"local reset callback occurs"];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting local");
        [resetExpectation fulfill];
    }];
    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

-(void)testResetLocalWhileUntrusted {
    // We're "logged in to" cloudkit but not in circle.
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    self.silentFetchesAllowed = false;

    // Test starts with local TLK and key hierarchy in our fake cloudkit
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem. It should fetch once.
    [self expectCKFetch];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.loggedIn wait:500*NSEC_PER_MSEC], "Should have been told of a 'login' event on startup");

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should arrive at 'waitfortrust''");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust''");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    NSData* changeTokenData = [[[NSUUID UUID] UUIDString] dataUsingEncoding:NSUTF8StringEncoding];
    CKServerChangeToken* changeToken = [[CKServerChangeToken alloc] initWithData:changeTokenData];
    [self.defaultCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry contextID:self.defaultCKKS.operationDependencies.contextID zoneName:self.keychainView.zoneName];
        ckse.changeToken = changeToken;

        NSError* error = nil;
        [ckse saveToDatabase:&error];
        XCTAssertNil(error, "No error saving new zone state to database");
        return CKKSDatabaseTransactionCommit;
    }];

    // after the reset, CKKS should refetch what's available
    [self expectCKFetch];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"local reset callback occurs"];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting local");
        ckksnotice_global("ckks", "Received a rpcResetLocal callback");

        [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry contextID:self.defaultCKKS.operationDependencies.contextID zoneName:self.keychainView.zoneName];
            XCTAssertNotEqualObjects(changeToken, ckse.changeToken, "Change token is reset");
        }];

        [resetExpectation fulfill];
    }];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], @"Key state should arrive at 'waitfortrust''");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'waitfortrust''");

    // Now regain trust, and see what happens! It should use the existing fetch, pick up the old key hierarchy, and use it
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];

    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], @"Key state should arrive at 'ready''");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready''");

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFetchFailsDuringLogOut {
    // Test starts with local TLK and key hierarchy in our fake cloudkit
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];
    self.silentFetchesAllowed = false;

    // Spin up CKKS subsystem. It should fetch once.
    [self expectCKFetch];
    XCTAssertNotEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK shouldn't know the account state");
    [self.defaultCKKS.stateMachine testPauseStateMachineAfterEntering:CKKSStateFetch];
    [self startCKKSSubsystem];

    // Wait till we're fully logged in
    XCTAssertEqual(0, [self.defaultCKKS.loggedIn wait:2000*NSEC_PER_MSEC], "Should have been told of a 'login'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedOut wait:100*NSEC_PER_MSEC], "'logout' event should be reset");
    XCTAssertEqual(0, [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    // Simulate a cloudkit logout and NSNotification callback
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCKAccountStatusChangeAndWaitForSignal];
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self endSOSTrustedOperationForAllViews];

    // Log out event should be occurring
    XCTAssertEqual(0,    [self.defaultCKKS.loggedOut wait:2000*NSEC_PER_MSEC], "Should have been told of a 'logout'");
    XCTAssertNotEqual(0, [self.defaultCKKS.loggedIn wait:100*NSEC_PER_MSEC], "'login' event should be reset");
    XCTAssertEqual(0,    [self.defaultCKKS.accountStateKnown wait:50*NSEC_PER_MSEC], "CKK should know the account state");

    [self.defaultCKKS.stateMachine testReleaseStateMachinePause:CKKSStateFetch];

    // We shouldn't have added any data
    [self checkNoCKKSDataForView:self.keychainView];

    // And CKKS should be in a logged out state
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'logged out'");
}

- (void)testResetLocalWhileLoggedOut {
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    self.accountStatus = CKAccountStatusNoAccount;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    self.silentFetchesAllowed = false;

    // Test starts with local TLK and key hierarchy in our fake cloudkit
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.defaultCKKS.loggedOut wait:20*NSEC_PER_SEC], "CKKS should positively log out");
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], @"Key state should arrive at 'loggedout'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'loggedout'");

    // Can we reset local data while logged out?
    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"local reset callback occurs"];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting local");
        ckksnotice_global("ckks", "Received a rpcResetLocal callback");

        [resetExpectation fulfill];
    }];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], @"Key state should arrive at 'loggedout'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'loggedout'");
}

-(void)testResetLocalMultipleTimes {
    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We're going to request a bunch of CloudKit resets, but hold them from finishing
    [self holdCloudKitFetches];

    XCTestExpectation* resetExpectation0 = [self expectationWithDescription: @"reset callback(0) occurs"];
    XCTestExpectation* resetExpectation1 = [self expectationWithDescription: @"reset callback(1) occurs"];
    XCTestExpectation* resetExpectation2 = [self expectationWithDescription: @"reset callback(2) occurs"];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "should receive no error resetting local");
        secnotice("ckksreset", "Received a rpcResetLocal(0) callback");
        [resetExpectation0 fulfill];
    }];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "should receive no error resetting local");
        secnotice("ckksreset", "Received a rpcResetLocal(1) callback");
        [resetExpectation1 fulfill];
    }];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "should receive no error resetting local");
        secnotice("ckksreset", "Received a rpcResetLocal(2) callback");
        [resetExpectation2 fulfill];
    }];

    // After the reset(s), we expect no uploads. Let the resets flow!
    [self releaseCloudKitFetchHold];
    [self waitForExpectations:@[resetExpectation0, resetExpectation1, resetExpectation2] timeout:20];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

-(void)testResetCloudKitZone {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    // During the reset, Octagon will upload the key hierarchy, and then CKKS will upload the class C item
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        ckksnotice_global("ckks", "Received a resetCloudKit callback");
        [resetExpectation fulfill];
    }];

    // Sneak in and perform Octagon's duties
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
}

- (void)testResetCloudKitZoneCloudKitRejects {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.nextModifyRecordZonesError = [[NSError alloc] initWithDomain:CKErrorDomain
                                                                 code:CKErrorZoneBusy
                                                             userInfo:@{
        CKErrorRetryAfterKey: @(0.2),
        NSUnderlyingErrorKey: [[NSError alloc] initWithDomain:CKErrorDomain
                                                         code:2029
                                                     userInfo:nil],
    }];
    self.silentZoneDeletesAllowed = true;

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // During the reset, Octagon will upload the key hierarchy, and then CKKS will upload the class C item
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        ckksnotice_global("ckks", "Received a resetCloudKit callback");
        [resetExpectation fulfill];
    }];

    // Sneak in and perform Octagon's duties
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    XCTAssertNil(self.nextModifyRecordZonesError, "Record zone modification error should have been cleared");
}

- (void)testResetCloudKitZoneDuringWaitForTLK {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // Test starts with nothing in database, but one in our fake CloudKit.
    // No TLK, though!
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // No records should be uploaded
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS should have entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Restart CKKS to really get in the spirit of waitfortlk (and get a pending processOutgoingQueue operation going)
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];
    self.ckksViews = [NSMutableSet setWithObject:self.keychainView];

    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    [self.defaultCKKS rpcProcessOutgoingQueue:nil operationGroup:nil];
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    // Now, reset everything. The outgoingOp should get cancelled.
    // We expect a key hierarchy upload, and then the class C item upload
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        [resetExpectation fulfill];
    }];

    // Sneak in and perform Octagon's duties
    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And adding another item works too
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

/*
 * This test doesn't work, since the resetLocal fails. CKKS gets back into waitfortlk
 * but that isn't considered a successful resetLocal.
 *
- (void)testResetLocalDuringWaitForTLK {
    // Test starts with nothing in database, but one in our fake CloudKit.
    // No TLK, though!
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // No records should be uploaded
    [self addGenericPassword: @"data" account: @"account-delete-me"];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS should have entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortlk");

    // Restart CKKS to really get in the spirit of waitfortlk (and get a pending processOutgoingQueue operation going)
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSync:self.defaultCKKS];
    self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    CKKSOutgoingQueueOperation* outgoingOp = [self.defaultCKKS processOutgoingQueue:nil];
    XCTAssertTrue([outgoingOp isPending], "outgoing queue processing should be on hold");

    // Now, reset everything. The outgoingOp should get cancelled.
    // We expect a key hierarchy upload, and then the class C item upload
    [self expectCKModifyKeyRecords: 3 currentKeyPointerRecords: 3 tlkShareRecords:3 zoneID:self.keychainZoneID];
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID
                          checkItem: [self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetLocal:nil reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting local");
        [resetExpectation fulfill];
    }];
    [self waitForExpectations:@[resetExpectation] timeout:20];

    XCTAssertTrue([outgoingOp isCancelled], "old stuck ProcessOutgoingQueue should be cancelled");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // And adding another item works too
    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem: [self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}*/

-(void)testResetCloudKitZoneWhileUntrusted {
    self.silentZoneDeletesAllowed = true;

    // We're "logged in to" cloudkit but not in circle.
    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // Since CKKS is untrusted, it'll fetch the zone but then get stuck
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortrust'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'waitfortrust'");

    CKRecord* ckr = [self createFakeRecord: self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85"];
    [self.keychainZone addToZone: ckr];

    XCTAssertNotNil(self.keychainZone.currentDatabase, "Zone exists");
    XCTAssertNotNil(self.keychainZone.currentDatabase[ckr.recordID], "An item exists in the fake zone");

    // Make the zone, so we know if it was deleted
    self.keychainZone.flag = true;

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"reset callback occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting cloudkit");
        ckksnotice_global("ckks", "Received a resetCloudKit callback");
        [resetExpectation fulfill];
    }];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], "CKKS entered 'waitfortlkcreation'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    XCTAssertFalse(self.keychainZone.flag, "Zone was deleted at some point");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    // Now log in, and see what happens! It should create the zone again and upload a whole new key hierarchy
    self.silentFetchesAllowed = true;
    self.mockSOSAdapter.circleStatus = kSOSCCInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];
    [self beginSOSTrustedViewOperation:self.defaultCKKS];

    [self performOctagonTLKUpload:self.ckksViews];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords: 1 currentKeyPointerRecords: 1 zoneID:self.keychainZoneID checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testResetCloudKitZoneMultipleTimes {
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMExpect([self.suggestTLKUpload trigger]);

    self.silentZoneDeletesAllowed = true;

    // Test starts with nothing in database, but one in our fake CloudKit.
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self saveTLKMaterialToKeychainSimulatingSOS:self.keychainZoneID];

    // Spin up CKKS subsystem.
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: @"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];

    // We're going to request a bunch of CloudKit resets, but hold them from finishing
    [self holdCloudKitFetches];

    XCTestExpectation* resetExpectation0 = [self expectationWithDescription: @"reset callback(0) occurs"];
    XCTestExpectation* resetExpectation1 = [self expectationWithDescription: @"reset callback(1) occurs"];
    XCTestExpectation* resetExpectation2 = [self expectationWithDescription: @"reset callback(2) occurs"];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "should receive no error resetting cloudkit");
        secnotice("ckksreset", "Received a resetCloudKit(0) callback");
        [resetExpectation0 fulfill];
    }];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "should receive no error resetting cloudkit");
        secnotice("ckksreset", "Received a resetCloudKit(1) callback");
        [resetExpectation1 fulfill];
    }];
    [self.injectedManager rpcResetCloudKit:nil reason:@"reset-test" reply:^(NSError* result) {
        XCTAssertNil(result, "should receive no error resetting cloudkit");
        secnotice("ckksreset", "Received a resetCloudKit(2) callback");
        [resetExpectation2 fulfill];
    }];

    // After the reset(s), we expect a key hierarchy upload, and then the class C item upload
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];

    // And let the resets flow
    [self releaseCloudKitFetchHold];

    OCMVerifyAllWithDelay(self.suggestTLKUpload, 10);
    [self performOctagonTLKUpload:self.ckksViews];
    
    [self waitForExpectations:@[resetExpectation0, resetExpectation1, resetExpectation2] timeout:20];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    OCMVerifyAllWithDelay(self.mockDatabase, 20);

    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID
                          checkItem:[self checkClassABlock:self.keychainZoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:@"account-class-A"
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCFetchAndProcessWhileCloudKitNotResponding {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    [self holdCloudKitFetches];

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFetchAndProcessChanges:nil reply:^(NSError * _Nullable error) {
        // done! we should have an underlying error of "fetch isn't working"
        XCTAssertNotNil(error, "Should have received an error attempting to fetch and process");
        XCTAssertEqualObjects(error.domain, CKKSResultErrorDomain, "Should have received a CKKS result error attempting to fetch and process");
        XCTAssertEqual(error.code, CKKSResultTimedOut, "Should have received a CKKS result error attempting to fetch and process");
        NSError* underlying = error.userInfo[NSUnderlyingErrorKey];
        XCTAssertNil(underlying, "Should have not received an underlying error");

        /*
        // Until the CKKS state machine merges with the fetcher, we can't reasonably provide this info
        XCTAssertEqualObjects(underlying.domain, CKKSResultDescriptionErrorDomain, "Underlying error should be CKKSResultDescriptionErrorDomain");
        XCTAssertEqual(underlying.code, CKKSResultDescriptionPendingSuccessfulFetch, "Underlying error should be 'pending fetch'");
         */
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    [self releaseCloudKitFetchHold];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCFetchAndProcessWhileCloudKitErroring {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    [self.keychainZone failNextFetchWith:[[NSError alloc] initWithDomain:CKErrorDomain
                                                                    code:CKErrorRequestRateLimited
                                                                userInfo:@{CKErrorRetryAfterKey : [NSNumber numberWithInt:30]}]];

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFetchAndProcessChanges:nil reply:^(NSError * _Nullable error) {
        // done! we should have an underlying error of "fetch isn't working"
        XCTAssertNotNil(error, "Should have received an error attempting to fetch and process");
        NSError* underlying = error.userInfo[NSUnderlyingErrorKey];
        XCTAssertNil(underlying, "Should not have received an underlying error");

        /*
         Until the CKKS state machine merges with the fetcher, we can't reasonably provide this info
        XCTAssertEqualObjects(underlying.domain, CKKSResultDescriptionErrorDomain, "Underlying error should be CKKSResultDescriptionErrorDomain");
        XCTAssertEqual(underlying.code, CKKSResultDescriptionPendingSuccessfulFetch, "Underlying error should be 'pending fetch'");

        NSError* underunderlying = underlying.userInfo[NSUnderlyingErrorKey];
        XCTAssertNotNil(underunderlying, "Should have received another layer of underlying error");
        XCTAssertEqualObjects(underunderlying.domain, CKErrorDomain, "Underlying error should be CKErrorDomain");
        XCTAssertEqual(underunderlying.code, CKErrorRequestRateLimited, "Underlying error should be 'rate limited'");
         */

        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCFetchAndProcessWhileInWaitForTLK {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFetchAndProcessChanges:nil reply:^(NSError * _Nullable error) {
        // done! we should have an underlying error of "fetch isn't working"
        XCTAssertNotNil(error, "Should have received an error attempting to fetch and process");

        XCTAssertEqualObjects(error.domain, CKKSResultErrorDomain, "Should have received a CKKS result error");
        XCTAssertEqual(error.code, CKKSResultSubresultError, "Should have received a CKKS result error code");

        NSError* underlying = error.userInfo[NSUnderlyingErrorKey];
        XCTAssertNotNil(underlying, "Should have received an underlying error");
        XCTAssertEqualObjects(underlying.domain, CKKSErrorDomain, "Underlying error should be from the CKKS error domain");
        XCTAssertEqual(underlying.code, CKKSKeysMissing, "Underlying error should be 'keys missing'");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCFetchAndProcessClassAChangesWhileLocked {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    self.aksLockState = true;
    [self.lockStateTracker recheck];

    CKRecord* ckr = [self createFakeRecord:self.keychainZoneID recordName:@"7B598D31-F9C5-481E-98AC-5A507ACB2D85" withAccount:@"account-should-exist" key:self.keychainZoneKeys.classA];
    [self.keychainZone addToZone:ckr];

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFetchAndProcessClassAChanges:nil reply:^(NSError * _Nullable error) {
        XCTAssertNotNil(error, "Should have received an error attempting to fetch and process");
        NSError* underlying = error.userInfo[NSUnderlyingErrorKey];
        XCTAssertNotNil(underlying, "Should have received an underlying error");
        XCTAssertEqualObjects(underlying.domain, @"securityd", "Underlying error should be 'securityd' domain");
        XCTAssertEqual(underlying.code, errSecInteractionNotAllowed, "Underlying error should be 'device locked'");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    [self releaseCloudKitFetchHold];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCFetchAndProcessChangesIfNoRecentFetch {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFetchAndProcessChanges:nil reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should have received no error");
        [callbackOccurs fulfill];
    }];

    // Wait for the first fetch to finish so that the next call will skip the fetch.
    [self waitForExpectations:@[callbackOccurs] timeout:20];

    CKKSCondition* incomingQueue = self.defaultCKKS.stateConditions[CKKSStateProcessIncomingQueue];

    XCTestExpectation* callbackOccurs2 = [self expectationWithDescription:@"callback-occurs"];
    // Ensure that this call does NOT fetch.
    self.silentFetchesAllowed = false;
    [self.ckksControl rpcFetchAndProcessChangesIfNoRecentFetch:nil reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should have received no error");
        [callbackOccurs2 fulfill];
        self.silentFetchesAllowed = true;
    }];
    [self waitForExpectations:@[callbackOccurs2] timeout:20];

    // Verify that the incoming queue (if any) was processed.
    XCTAssertEqual(0, [incomingQueue wait:20*NSEC_PER_SEC], @"CKKS state machine should process the incoming queue");
}

- (void)testRPCPushWithFailingWrite {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS should enter ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    self.defaultCKKS.holdOutgoingQueueOperation = [CKKSResultOperation named:@"outgoing-queue-hold" withBlock:^{}];

    [self failNextCKAtomicModifyItemRecordsUpdateFailure:self.keychainZoneID];
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID];

    [self addGenericPassword: @"data" account: @"account-delete-me"];

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcPushOutgoingChanges:nil reply:^(NSError * _Nullable error) {
        // done! we should have an underlying error of "atomic write failed"
        XCTAssertNotNil(error, "Should have received an error due to the failed push");
        NSError* underlying = error.userInfo[NSUnderlyingErrorKey];
        XCTAssertNotNil(underlying, "Should have received an underlying error");
        XCTAssertEqualObjects(underlying.domain, CKErrorDomain, "Underlying error should be CKErrorDomain");
        XCTAssertEqual(underlying.code, CKErrorPartialFailure, "Underlying error should be 'CKErrorPartialFailure'");
        [callbackOccurs fulfill];
    }];

    [self.operationQueue addOperation:self.defaultCKKS.holdOutgoingQueueOperation];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCPushWhileInWaitForTLK {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcPushOutgoingChanges:nil reply:^(NSError * _Nullable error) {
        // done! we should have an underlying error of "fetch isn't working"
        XCTAssertNotNil(error, "Should have received an error attempting to push a broken zone");
        NSError* underlying = error.userInfo[NSUnderlyingErrorKey];
        XCTAssertNotNil(underlying, "Should have received an underlying error");
        XCTAssertEqualObjects(underlying.domain, CKKSErrorDomain, "Should be a CKKSErrorDomain error");
        XCTAssertEqual(underlying.code, CKKSKeysMissing, "Should be a 'keys missing' error");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCTLKMissingWhenMissing {
    // Bring CKKS up in waitfortlk
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcTLKMissing:@"keychain" reply:^(bool missing) {
        XCTAssertTrue(missing, "TLKs should be missing");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCTLKMissingWhenFound {
    // Bring CKKS up in 'ready'
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready''");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready''");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcTLKMissing:@"keychain" reply:^(bool missing) {
        XCTAssertFalse(missing, "TLKs should not be missing");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCKnownBadStateWhenNoCloudKit {
    self.accountStatus = CKAccountStatusNoAccount;

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS entered loggedout");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateLoggedOut] wait:20*NSEC_PER_SEC], "CKKS state machine should enter loggedout");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcKnownBadState:@"keychain" reply:^(CKKSKnownBadState result) {
        XCTAssertEqual(result, CKKSKnownStateNoCloudKitAccount, "State should be 'no cloudkit account'");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCKnownBadStateWhenNoTrust {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    self.mockSOSAdapter.circleStatus = kSOSCCNotInCircle;
    [self.accountStateTracker notifyCircleStatusChangeAndWaitForSignal];

    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS entered waitfortrust");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateWaitForTrust] wait:20*NSEC_PER_SEC], "CKKS state machine should enter waitfortrust");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcKnownBadState:@"keychain" reply:^(CKKSKnownBadState result) {
        XCTAssertEqual(result, CKKSKnownStateWaitForOctagon, "State should be 'waitforoctagon'");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCKnownBadStateWhenTLKsMissing {
    // Bring CKKS up in waitfortlk
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLK] wait:20*NSEC_PER_SEC], "CKKS entered waitfortlk");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcKnownBadState:@"keychain" reply:^(CKKSKnownBadState result) {
        XCTAssertEqual(result, CKKSKnownStateTLKsMissing, "TLKs should be missing");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCKnownBadStateWhenInWaitForUnlock {
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self putFakeDeviceStatusInCloudKit:self.keychainZoneID];

    // Bring CKKS up in 'waitforunlock'
    self.aksLockState = true;
    [self.lockStateTracker recheck];
    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for the unlock dependency. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForUnlock] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitforunlock");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcKnownBadState:@"keychain" reply:^(CKKSKnownBadState result) {
        XCTAssertEqual(result, CKKSKnownStateWaitForUnlock, "known state should be wait for unlock");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCKnownBadStateWhenInWaitForUpload {
    // Bring CKKS up in 'waitfortupload'
    self.aksLockState = true;
    [self.lockStateTracker recheck];
    [self startCKKSSubsystem];

    // Wait for the key hierarchy state machine to get stuck waiting for Octagon. No uploads should occur.
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"Key state should get stuck in waitfortlkcreation");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcKnownBadState:@"keychain" reply:^(CKKSKnownBadState result) {
        XCTAssertEqual(result, CKKSKnownStateWaitForOctagon, "known state should be wait for Octagon");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRPCKnownBadStateWhenInGoodState {
    // Bring CKKS up in 'ready'
    [self putFakeKeyHierarchyInCloudKit:self.keychainZoneID];
    [self saveTLKMaterialToKeychain:self.keychainZoneID];
    [self expectCKKSTLKSelfShareUpload:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready''");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready''");

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];

    [self.ckksControl rpcKnownBadState:@"keychain" reply:^(CKKSKnownBadState result) {
        XCTAssertEqual(result, CKKSKnownStatePossiblyGood, "known state should not be possibly-good");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];

    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testRpcStatus {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // Let things shake themselves out.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should return to 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should return to 'ready'");
    [self waitForCKModifications];

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcStatus:@"keychain" reply:^(NSArray<NSDictionary*>* result, NSError* error) {
        XCTAssertNil(error, "should be no error fetching status for keychain");

        // Ugly "global" hack
        XCTAssertEqual(result.count, 2u, "Should have received two result dictionaries back");
        NSDictionary* keychainStatus = result[1];

        XCTAssertNotNil(keychainStatus, "Should have received at least one zone status back");
        XCTAssertEqualObjects(keychainStatus[@"view"], @"keychain", "Should have received status for the keychain view");
        XCTAssertEqualObjects(keychainStatus[@"keystate"], SecCKKSZoneKeyStateReady, "Should be in 'ready' status");
        XCTAssertNotNil(keychainStatus[@"ckmirror"], "Status should have any ckmirror");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
}

- (void)testRpcFastStatus {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    [self startCKKSSubsystem];

    // Let things shake themselves out.
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "Key state should return to 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should return to 'ready'");
    [self waitForCKModifications];

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFastStatus:@"keychain" reply:^(NSArray<NSDictionary*>* result, NSError* error) {
        XCTAssertNil(error, "should be no error fetching status for keychain");

        // Ugly "global" hack
        XCTAssertEqual(result.count, 1u, "Should have received one result dictionaries back");
        NSDictionary* keychainStatus = result[0];

        XCTAssertNotNil(keychainStatus, "Should have received at least one zone status back");
        XCTAssertEqualObjects(keychainStatus[@"view"], @"keychain", "Should have received status for the keychain view");
        XCTAssertEqualObjects(keychainStatus[@"keystate"], SecCKKSZoneKeyStateReady, "Should be in 'ready' status");
        XCTAssertNil(keychainStatus[@"ckmirror"], "fastStatus should not have any ckmirror");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
}


- (void)testRpcStatusWaitsForAccountDetermination {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Set up the account state callbacks to happen in half a second
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (500 * NSEC_PER_MSEC)), dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        // Let CKKS come up (simulating daemon starting due to RPC)
        [self startCKKSSubsystem];
    });

    // Before CKKS figures out we're in an account, fire off the status RPC.
    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    // rdar://80603312: This test fails intermittently on slower devices if the
    // state machine doesn't reach "ready" in the default timeout (1 second), so
    // we give it some extra time.
    [self.ckksControl rpcStatus:@"keychain" fast:NO waitForNonTransientState:3*NSEC_PER_SEC reply:^(NSArray<NSDictionary*>* result, NSError* error) {
        XCTAssertNil(error, "should be no error fetching status for keychain");

        // Ugly "global" hack
        XCTAssertEqual(result.count, 2u, "Should have received two result dictionaries back");
        NSDictionary* keychainStatus = result[1];

        XCTAssertNotNil(keychainStatus, "Should have received at least one zone status back");
        XCTAssertEqualObjects(keychainStatus[@"view"], @"keychain", "Should have received status for the keychain view");
        XCTAssertEqualObjects(keychainStatus[@"keystate"], SecCKKSZoneKeyStateReady, "Should be in 'ready' status");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
}

- (void)testRpcStatusIsFastDuringError {
    [self createAndSaveFakeKeyHierarchy: self.keychainZoneID]; // Make life easy for this test.

    // Let CKKS come up, then force it into error
    [self startCKKSSubsystem];
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    OctagonStateTransitionOperation* op = [OctagonStateTransitionOperation named:@"enter" entering:CKKSStateError];
    OctagonStateTransitionRequest* request = [[OctagonStateTransitionRequest alloc] init:@"enter-wait-for-trust"
                                                                            sourceStates:CKKSAllStates()
                                                                             serialQueue:self.defaultCKKS.queue
                                                                            transitionOp:op];
    [self.defaultCKKS.stateMachine handleExternalRequest:request
                                            startTimeout:10*NSEC_PER_SEC];

    self.keychainView.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateError] wait:20*NSEC_PER_SEC], "CKKS entered 'error'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateError] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'error'");

    // Fire off the status RPC; it should return immediately
    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcStatus:@"keychain" reply:^(NSArray<NSDictionary*>* result, NSError* error) {
        XCTAssertNil(error, "should be no error fetching status for keychain");

        // Ugly "global" hack
        XCTAssertEqual(result.count, 2u, "Should have received two result dictionaries back");
        NSDictionary* keychainStatus = result[1];

        XCTAssertNotNil(keychainStatus, "Should have received at least one zone status back");
        XCTAssertEqualObjects(keychainStatus[@"view"], @"keychain", "Should have received status for the keychain view");
        XCTAssertEqualObjects(keychainStatus[@"keystate"], SecCKKSZoneKeyStateError, "Should be in 'ready' status");
        XCTAssertEqualObjects(keychainStatus[@"keystate"], CKKSStateError, "Should be in 'ready' status");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
}

- (void)testResetLocalAPIWakesDaemon {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword:@"data" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    // Now, simulate a restart: all views suddenly go missing from the ViewManager
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSyncWithoutSettingPolicy:self.defaultCKKS];
    [self.defaultCKKS beginCloudKitOperation];

    // And a reset-local API call wakes the daemon
    // The policy arrives after 500ms
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (0.5 * NSEC_PER_SEC)), dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        [self.defaultCKKS setCurrentSyncingPolicy:self.viewSortingPolicyForManagedViewList];
        self.ckksViews = [self.defaultCKKS.operationDependencies.allCKKSManagedViews mutableCopy];
        self.keychainView = [self.defaultCKKS viewStateForName:self.keychainZoneID.zoneName];
        [self beginSOSTrustedOperationForAllViews];
    });

    XCTestExpectation* resetExpectation = [self expectationWithDescription: @"local reset callback occurs"];
    [self.injectedManager rpcResetLocal:self.keychainZoneID.zoneName reply:^(NSError* result) {
        XCTAssertNil(result, "no error resetting local");
        [resetExpectation fulfill];
    }];

    [self waitForExpectations:@[resetExpectation] timeout:20];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testPushAPIWakesDaemon {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword:@"data" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    // Now, simulate a restart: all views suddenly go missing from the ViewManager
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSyncWithoutSettingPolicy:self.defaultCKKS];
    [self.defaultCKKS beginCloudKitOperation];

    // And a fetch API call wakes the daemon
    // The policy arrives after 500msx
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (0.5 * NSEC_PER_SEC)), dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        [self.defaultCKKS setCurrentSyncingPolicy:self.viewSortingPolicyForManagedViewList];
        self.ckksViews = [self.defaultCKKS.operationDependencies.allCKKSManagedViews mutableCopy];
        [self beginSOSTrustedOperationForAllViews];
    });

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcPushOutgoingChanges:nil reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should have received no error");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:60];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testFetchAPIWakesDaemon {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    // We expect a single record to be uploaded
    [self expectCKModifyItemRecords:1 currentKeyPointerRecords:1 zoneID:self.keychainZoneID checkItem:[self checkClassCBlock:self.keychainZoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword:@"data" account:@"account-delete-me"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered 'ready'");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter 'ready'");

    // Now, simulate a restart: all views suddenly go missing from the ViewManager
    self.defaultCKKS = [self.injectedManager restartCKKSAccountSyncWithoutSettingPolicy:self.defaultCKKS];
    [self.defaultCKKS beginCloudKitOperation];

    // And a fetch API call wakes the daemon
    // The policy arrives after 500ms
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (0.5 * NSEC_PER_SEC)), dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        [self.defaultCKKS setCurrentSyncingPolicy:self.viewSortingPolicyForManagedViewList];
        self.ckksViews = [self.defaultCKKS.operationDependencies.allCKKSManagedViews mutableCopy];
        [self beginSOSTrustedOperationForAllViews];
    });

    XCTestExpectation* callbackOccurs = [self expectationWithDescription:@"callback-occurs"];
    [self.ckksControl rpcFetchAndProcessChanges:nil reply:^(NSError * _Nullable error) {
        XCTAssertNil(error, "Should have received no error");
        [callbackOccurs fulfill];
    }];

    [self waitForExpectations:@[callbackOccurs] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)testAPIFromNondefaultPersona {
    [self createAndSaveFakeKeyHierarchy:self.keychainZoneID];
    [self startCKKSSubsystem];

    XCTAssertEqual(0, [self.keychainView.keyHierarchyConditions[SecCKKSZoneKeyStateReady] wait:20*NSEC_PER_SEC], "CKKS entered ready");
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], "CKKS state machine should enter ready");

    self.mockPersonaAdapter.isDefaultPersona = NO;

    XCTestExpectation* statusCallbackOccurs = [self expectationWithDescription:@"status-callback-occurs"];
    [self.ckksControl rpcStatus:@"keychain" reply:^(NSArray<NSDictionary*>* result, NSError* error) {
        XCTAssertNotNil(error, "Should have received an error attempting to fetch and process from a non-default persona");
#if TARGET_OS_TV
        XCTAssertEqualObjects(error.domain, OctagonErrorDomain, "Should have received an Octagon error");
        XCTAssertEqual(error.code, OctagonErrorNoSuchContext, "Should have received a OctagonErrorNoSuchContext error");
#else
        XCTAssertEqualObjects(error.domain, CKKSErrorDomain, "Should have received a CKKS result error");
        XCTAssertEqual(error.code, CKKSErrorNotSupported, "Should have received a CKKSErrorNotSupported error");
#endif
        [statusCallbackOccurs fulfill];
    }];

    XCTestExpectation* rpcFetchCallbackOccurs = [self expectationWithDescription:@"rpcfetch-callback-occurs"];
    [self.ckksControl rpcFetchAndProcessChanges:nil reply:^(NSError * _Nullable error) {
        XCTAssertNotNil(error, "Should have received an error attempting to fetch and process from a non-default persona");
#if TARGET_OS_TV
        XCTAssertEqualObjects(error.domain, OctagonErrorDomain, "Should have received a Octagon error");
        XCTAssertEqual(error.code, OctagonErrorNoSuchContext, "Should have received a OctagonErrorNoSuchContext error");
#else
        XCTAssertEqualObjects(error.domain, CKKSErrorDomain, "Should have received a CKKS result error");
        XCTAssertEqual(error.code, CKKSErrorNotSupported, "Should have received a CKKSErrorNotSupported error");
#endif
        [rpcFetchCallbackOccurs fulfill];
    }];

    [self waitForExpectations:@[statusCallbackOccurs, rpcFetchCallbackOccurs] timeout:20];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

@end

#endif // OCTAGON
