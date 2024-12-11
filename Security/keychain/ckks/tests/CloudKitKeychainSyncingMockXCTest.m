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

#include <Security/SecCertificate.h>
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include <Security/SecCertificateRequest.h>
#pragma clang diagnostic pop

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wquoted-include-in-framework-header"
#import <OCMock/OCMock.h>
#pragma clang diagnostic pop

#import "keychain/ckks/tests/CloudKitMockXCTest.h"
#import "keychain/ckks/tests/CloudKitKeychainSyncingMockXCTest.h"

#import "keychain/securityd/SecItemSchema.h"
#import "keychain/securityd/SecItemServer.h"
#import "keychain/securityd/SecItemDb.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSMemoryKeyCache.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#include "keychain/ckks/tests/CKKSMockCuttlefishAdapter.h"

#import "keychain/ot/OTDefines.h"

#import "tests/secdmockaks/mockaks.h"
#import "ipc/securityd_client.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#import "keychain/SecureObjectSync/SOSAccount.h"
#pragma clang diagnostic pop

@interface CKKSControl (TestAccess)
@property NSXPCConnection *connection;
@end

@implementation ZoneKeys
- (instancetype)initLoadingRecordsFromZone:(FakeCKZone*)zone
                                 contextID:(NSString*)contextID
{
    if((self = [super initWithZoneID:zone.zoneID contextID:contextID])) {
        CKRecordID* currentTLKPointerID    = [[CKRecordID alloc] initWithRecordName:SecCKKSKeyClassTLK zoneID:zone.zoneID];
        CKRecordID* currentClassAPointerID = [[CKRecordID alloc] initWithRecordName:SecCKKSKeyClassA   zoneID:zone.zoneID];
        CKRecordID* currentClassCPointerID = [[CKRecordID alloc] initWithRecordName:SecCKKSKeyClassC   zoneID:zone.zoneID];

        CKRecord* currentTLKPointerRecord    = zone.currentDatabase[currentTLKPointerID];
        CKRecord* currentClassAPointerRecord = zone.currentDatabase[currentClassAPointerID];
        CKRecord* currentClassCPointerRecord = zone.currentDatabase[currentClassCPointerID];

        self.currentTLKPointer    = currentTLKPointerRecord ?    [[CKKSCurrentKeyPointer alloc] initWithCKRecord: currentTLKPointerRecord contextID:self.contextID] : nil;
        self.currentClassAPointer = currentClassAPointerRecord ? [[CKKSCurrentKeyPointer alloc] initWithCKRecord: currentClassAPointerRecord contextID:self.contextID] : nil;
        self.currentClassCPointer = currentClassCPointerRecord ? [[CKKSCurrentKeyPointer alloc] initWithCKRecord: currentClassCPointerRecord contextID:self.contextID] : nil;

        CKRecordID* currentTLKID    = self.currentTLKPointer.currentKeyUUID    ? [[CKRecordID alloc] initWithRecordName:self.currentTLKPointer.currentKeyUUID    zoneID:zone.zoneID] : nil;
        CKRecordID* currentClassAID = self.currentClassAPointer.currentKeyUUID ? [[CKRecordID alloc] initWithRecordName:self.currentClassAPointer.currentKeyUUID zoneID:zone.zoneID] : nil;
        CKRecordID* currentClassCID = self.currentClassCPointer.currentKeyUUID ? [[CKRecordID alloc] initWithRecordName:self.currentClassCPointer.currentKeyUUID zoneID:zone.zoneID] : nil;

        CKRecord* currentTLKRecord    = currentTLKID    ? zone.currentDatabase[currentTLKID]    : nil;
        CKRecord* currentClassARecord = currentClassAID ? zone.currentDatabase[currentClassAID] : nil;
        CKRecord* currentClassCRecord = currentClassCID ? zone.currentDatabase[currentClassCID] : nil;

        self.tlk =    currentTLKRecord ?    [[CKKSKey alloc] initWithCKRecord: currentTLKRecord contextID:self.contextID]    : nil;
        self.classA = currentClassARecord ? [[CKKSKey alloc] initWithCKRecord: currentClassARecord contextID:self.contextID] : nil;
        self.classC = currentClassCRecord ? [[CKKSKey alloc] initWithCKRecord: currentClassCRecord contextID:self.contextID] : nil;
    }
    return self;
}

@end

// No tests here, just helper functions
@implementation CloudKitKeychainSyncingMockXCTest

- (void)setUp {
    // Set any feature flags that must be true for the tests, regardless of the base OS.

#if TARGET_OS_IOS || TARGET_OS_TV
    SecSecuritySetPersonaMusr(NULL);
#endif

    // Use our superclass to create a fake keychain
    [super setUp];

    self.automaticallyBeginCKKSViewCloudKitOperation = true;
    self.suggestTLKUpload = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMStub([self.suggestTLKUpload trigger]);

    self.requestPolicyCheck = OCMClassMock([CKKSNearFutureScheduler class]);
    OCMStub([self.requestPolicyCheck trigger]);

    // If a subclass wants to fill these in before calling setUp, fine.
    self.ckksZones = self.ckksZones ?: [NSMutableSet set];
    self.ckksViews = self.ckksViews ?: [NSMutableSet set];
    self.keys = self.keys ?: [[NSMutableDictionary alloc] init];

    self.defaultCKKS.cuttlefishAdapter = [[CKKSMockCuttlefishAdapter alloc] init:self.zones zoneKeys:self.keys peerID:self.mockSOSAdapter.selfPeer.peerID];
    
    [SecMockAKS reset];

    // Set up a remote peer with no keys
    self.remoteSOSOnlyPeer = [[CKKSSOSPeer alloc] initWithSOSPeerID:@"remote-peer-with-no-keys"
                                                encryptionPublicKey:nil
                                                   signingPublicKey:nil
                                                           viewList:self.managedViewList];
    NSMutableSet<id<CKKSSOSPeerProtocol>>* currentPeers = [NSMutableSet setWithObject:self.remoteSOSOnlyPeer];
    self.mockSOSAdapter.trustedPeers = currentPeers;

    // Fake out whether class A keys can be loaded from the keychain.
    self.mockCKKSKeychainBackedKey = OCMClassMock([CKKSKeychainBackedKey class]);
    __weak __typeof(self) weakSelf = self;
    BOOL (^shouldFailKeychainQuery)(NSDictionary* query) = ^BOOL(NSDictionary* query) {
        __strong __typeof(self) strongSelf = weakSelf;
        return !!strongSelf.keychainFetchError;
    };

    OCMStub([self.mockCKKSKeychainBackedKey setKeyMaterialInKeychain:[OCMArg checkWithBlock:shouldFailKeychainQuery] error:[OCMArg anyObjectRef]]
            ).andCall(self, @selector(handleFailedSetKeyMaterialInKeychain:error:));

    OCMStub([self.mockCKKSKeychainBackedKey queryKeyMaterialInKeychain:[OCMArg checkWithBlock:shouldFailKeychainQuery] error:[OCMArg anyObjectRef]]
            ).andCall(self, @selector(handleFailedLoadKeyMaterialFromKeychain:error:));

    // Bring up a fake CKKSControl object
    id mockConnection = OCMPartialMock([[NSXPCConnection alloc] init]);
    OCMStub([mockConnection remoteObjectProxyWithErrorHandler:[OCMArg any]]).andCall(self, @selector(injectedManager));
    self.ckksControl = [[CKKSControl alloc] initWithConnection:mockConnection];
    XCTAssertNotNil(self.ckksControl, "Should have received control object");
}

- (void)tearDown {
    // Make sure the key state machines won't continue
    [self.defaultCKKS.stateMachine haltOperation];
    [self.ckksViews removeAllObjects];

    [super tearDown];
    self.keys = nil;

    // We're tearing down the tests; things that are non-nil in the tests should be thrown away
    // We can't rely on the test class being released, unforunately...
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    self.ckksControl = nil;

    [self.suggestTLKUpload stopMocking];
    self.suggestTLKUpload = nil;

    [self.requestPolicyCheck stopMocking];
    self.requestPolicyCheck = nil;

    [self.mockCKKSKeychainBackedKey stopMocking];
    self.mockCKKSKeychainBackedKey = nil;
#pragma clang diagnostic pop

    self.remoteSOSOnlyPeer = nil;

#if TARGET_OS_IOS || TARGET_OS_TV
    SecSecuritySetPersonaMusr(NULL);
#endif
}

- (void)startCKKSSubsystem
{
    [super startCKKSSubsystem];
    if(self.mockSOSAdapter.circleStatus == kSOSCCInCircle) {
        [self beginSOSTrustedOperationForAllViews];
    } else {
        [self endSOSTrustedOperationForAllViews];
    }
}

- (void)beginSOSTrustedOperationForAllViews {
    [self beginSOSTrustedViewOperation:self.defaultCKKS];
}

- (void)beginSOSTrustedViewOperation:(CKKSKeychainView*)ckks
{
    if(self.automaticallyBeginCKKSViewCloudKitOperation) {
        [ckks beginCloudKitOperation];
    }
    [ckks beginTrustedOperation:@[self.mockSOSAdapter]
               suggestTLKUpload:self.suggestTLKUpload
             requestPolicyCheck:self.requestPolicyCheck];
}

- (void)endSOSTrustedOperationForAllViews {
    [self endSOSTrustedViewOperation:self.defaultCKKS];
}

- (void)endSOSTrustedViewOperation:(CKKSKeychainView*)ckks
{
    if(self.automaticallyBeginCKKSViewCloudKitOperation) {
        [ckks beginCloudKitOperation];
    }
    [ckks endTrustedOperation];
}

- (void)verifyDatabaseMocks {
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
    [self waitForCKModifications];
}

- (void)createClassCItemAndWaitForUpload:(CKRecordZoneID*)zoneID account:(NSString*)account {
    [self expectCKModifyItemRecords:1
           currentKeyPointerRecords:1
                             zoneID:zoneID
                          checkItem:[self checkClassCBlock:zoneID message:@"Object was encrypted under class C key in hierarchy"]];
    [self addGenericPassword: @"data" account: account];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

- (void)createClassAItemAndWaitForUpload:(CKRecordZoneID*)zoneID account:(NSString*)account {
    [self expectCKModifyItemRecords:1
           currentKeyPointerRecords:1
                             zoneID:zoneID
                          checkItem: [self checkClassABlock:zoneID message:@"Object was encrypted under class A key in hierarchy"]];
    [self addGenericPassword:@"asdf"
                     account:account
                    viewHint:nil
                      access:(id)kSecAttrAccessibleWhenUnlocked
                   expecting:errSecSuccess
                     message:@"Adding class A item"];
    OCMVerifyAllWithDelay(self.mockDatabase, 20);
}

// Helpers to handle 'failed' keychain loading and saving
- (bool)handleFailedLoadKeyMaterialFromKeychain:(NSDictionary*)query error:(NSError * __autoreleasing *) error {
    NSAssert(self.keychainFetchError != nil, @"must have a keychain error to error with");

    if(error) {
        *error = self.keychainFetchError;
    }
    return false;
}

- (bool)handleFailedSetKeyMaterialInKeychain:(NSDictionary*)query error:(NSError * __autoreleasing *) error {
    NSAssert(self.keychainFetchError != nil, @"must have a keychain error to error with");

    if(error) {
        *error = self.keychainFetchError;
    }
    return false;
}


- (ZoneKeys*)createFakeKeyHierarchy: (CKRecordZoneID*)zoneID oldTLK:(CKKSKey*) oldTLK {
    if(self.keys[zoneID]) {
        // Already created. Skip.
        return self.keys[zoneID];
    }

    NSString *contextID = CKKSDefaultContextID;
    NSError* error = nil;

    ZoneKeys* zonekeys = [[ZoneKeys alloc] initWithZoneID:zoneID contextID:contextID];

    zonekeys.tlk = [self fakeTLK:zoneID contextID:contextID];
    [zonekeys.tlk CKRecordWithZoneID: zoneID]; // no-op here, but memoize in the object
    zonekeys.currentTLKPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassTLK
                                                                   contextID:contextID
                                                              currentKeyUUID: zonekeys.tlk.uuid
                                                                      zoneID:zoneID
                                                             encodedCKRecord: nil];
    [zonekeys.currentTLKPointer CKRecordWithZoneID: zoneID];

    if(oldTLK) {
        zonekeys.rolledTLK = oldTLK;
        [zonekeys.rolledTLK wrapUnder: zonekeys.tlk error:&error];
        XCTAssertNotNil(zonekeys.rolledTLK, "Created a rolled TLK");
        XCTAssertNil(error, "No error creating rolled TLK");
    }

    zonekeys.classA = [CKKSKey randomKeyWrappedByParent: zonekeys.tlk keyclass:SecCKKSKeyClassA error:&error];
    XCTAssertNotNil(zonekeys.classA, "make Class A key");
    zonekeys.classA.currentkey = true;
    [zonekeys.classA CKRecordWithZoneID: zoneID];
    zonekeys.currentClassAPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassA
                                                                      contextID:contextID
                                                                 currentKeyUUID: zonekeys.classA.uuid
                                                                         zoneID:zoneID
                                                                encodedCKRecord: nil];
    [zonekeys.currentClassAPointer CKRecordWithZoneID: zoneID];

    zonekeys.classC = [CKKSKey randomKeyWrappedByParent: zonekeys.tlk keyclass:SecCKKSKeyClassC error:&error];
    XCTAssertNotNil(zonekeys.classC, "make Class C key");
    zonekeys.classC.currentkey = true;
    [zonekeys.classC CKRecordWithZoneID: zoneID];
    zonekeys.currentClassCPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassC
                                                                      contextID:contextID
                                                                 currentKeyUUID: zonekeys.classC.uuid
                                                                         zoneID:zoneID
                                                                encodedCKRecord: nil];
    [zonekeys.currentClassCPointer CKRecordWithZoneID: zoneID];

    self.keys[zoneID] = zonekeys;
    return zonekeys;
}

- (void)saveFakeKeyHierarchyToLocalDatabase: (CKRecordZoneID*)zoneID {
    ZoneKeys* zonekeys = [self createFakeKeyHierarchy: zoneID oldTLK:nil];

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult {
        NSError* error = nil;

        [zonekeys.tlk saveToDatabase:&error];
        XCTAssertNil(error, "TLK saved to database successfully");

        [zonekeys.classA saveToDatabase:&error];
        XCTAssertNil(error, "Class A key saved to database successfully");

        [zonekeys.classC saveToDatabase:&error];
        XCTAssertNil(error, "Class C key saved to database successfully");

        [zonekeys.currentTLKPointer saveToDatabase:&error];
        XCTAssertNil(error, "Current TLK pointer saved to database successfully");

        [zonekeys.currentClassAPointer saveToDatabase:&error];
        XCTAssertNil(error, "Current Class A pointer saved to database successfully");

        [zonekeys.currentClassCPointer saveToDatabase:&error];
        XCTAssertNil(error, "Current Class C pointer saved to database successfully");

        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)putFakeDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID zonekeys:(ZoneKeys*)zonekeys {
    // SOS peer IDs are written bare, missing the CKKSSOSPeerPrefix. Strip it here.
    NSString* peerID = self.remoteSOSOnlyPeer.peerID;
    if([peerID hasPrefix:CKKSSOSPeerPrefix]) {
        peerID = [peerID substringFromIndex:CKKSSOSPeerPrefix.length];
    }

    CKKSDeviceStateEntry* dse = [[CKKSDeviceStateEntry alloc] initForDevice:self.remoteSOSOnlyPeer.peerID
                                                                  contextID:self.defaultCKKS.operationDependencies.contextID
                                                                  osVersion:@"faux-version"
                                                             lastUnlockTime:nil
                                                              octagonPeerID:nil
                                                              octagonStatus:nil
                                                               circlePeerID:peerID
                                                               circleStatus:kSOSCCInCircle
                                                                   keyState:SecCKKSZoneKeyStateReady
                                                             currentTLKUUID:zonekeys.tlk.uuid
                                                          currentClassAUUID:zonekeys.classA.uuid
                                                          currentClassCUUID:zonekeys.classC.uuid
                                                                     zoneID:zoneID
                                                            encodedCKRecord:nil];
    [self.zones[zoneID] addToZone:dse zoneID:zoneID];
}

- (void)putFakeDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID {
    [self putFakeDeviceStatusInCloudKit:zoneID zonekeys:self.keys[zoneID]];
}

- (void)putFakeOctagonOnlyDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID zonekeys:(ZoneKeys*)zonekeys {
    CKKSDeviceStateEntry* dse = [[CKKSDeviceStateEntry alloc] initForDevice:self.remoteSOSOnlyPeer.peerID
                                                                  contextID:self.defaultCKKS.operationDependencies.contextID
                                                                  osVersion:@"faux-version"
                                                             lastUnlockTime:nil
                                                              octagonPeerID:@"octagon-fake-peer-id"
                                                              octagonStatus:[[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusIn]
                                                               circlePeerID:nil
                                                               circleStatus:kSOSCCError
                                                                   keyState:SecCKKSZoneKeyStateReady
                                                             currentTLKUUID:zonekeys.tlk.uuid
                                                          currentClassAUUID:zonekeys.classA.uuid
                                                          currentClassCUUID:zonekeys.classC.uuid
                                                                     zoneID:zoneID
                                                            encodedCKRecord:nil];
    [self.zones[zoneID] addToZone:dse zoneID:zoneID];
}

- (void)putFakeOctagonOnlyDeviceStatusInCloudKit:(CKRecordZoneID*)zoneID {
    [self putFakeOctagonOnlyDeviceStatusInCloudKit:zoneID zonekeys:self.keys[zoneID]];
}

- (void)putFakeKeyHierarchyInCloudKit: (CKRecordZoneID*)zoneID {
    ZoneKeys* zonekeys = [self createFakeKeyHierarchy: zoneID oldTLK:nil];
    XCTAssertNotNil(zonekeys, "failed to create fake key hierarchy for zoneID=%@", zoneID);
    XCTAssertNil(zonekeys.error, "should have no error creating a zonekeys");

    secnotice("fake-cloudkit", "new fake hierarchy: %@", zonekeys);

    FakeCKZone* zone = self.zones[zoneID];
    XCTAssertNotNil(zone, "failed to find zone %@", zoneID);

    dispatch_sync(zone.queue, ^{
        [zone _onqueueAddToZone:zonekeys.tlk    zoneID:zoneID];
        [zone _onqueueAddToZone:zonekeys.classA zoneID:zoneID];
        [zone _onqueueAddToZone:zonekeys.classC zoneID:zoneID];

        [zone _onqueueAddToZone:zonekeys.currentTLKPointer    zoneID:zoneID];
        [zone _onqueueAddToZone:zonekeys.currentClassAPointer zoneID:zoneID];
        [zone _onqueueAddToZone:zonekeys.currentClassCPointer zoneID:zoneID];

        if(zonekeys.rolledTLK) {
            [zone _onqueueAddToZone:zonekeys.rolledTLK zoneID:zoneID];
        }
    });
}

- (void)rollFakeKeyHierarchyInCloudKit: (CKRecordZoneID*)zoneID {
    ZoneKeys* zonekeys = self.keys[zoneID];
    self.keys[zoneID] = nil;

    CKKSKey* oldTLK = zonekeys.tlk;
    NSError* error = nil;
    [oldTLK ensureKeyLoadedForContextID:self.defaultCKKS.operationDependencies.contextID cache:nil error: &error];
    XCTAssertNil(error, "shouldn't error ensuring that the oldTLK has its key material");

    [self createFakeKeyHierarchy: zoneID oldTLK:oldTLK];
    [self putFakeKeyHierarchyInCloudKit: zoneID];
}

- (void)ensureZoneDeletionAllowed:(FakeCKZone*)zone {
    [super ensureZoneDeletionAllowed:zone];

    // Here's a hack: if we're deleting this zone, also drop the keys that used to be in it
    self.keys[zone.zoneID] = nil;
}

- (NSArray<CKRecord*>*)putKeySetInCloudKit:(CKKSCurrentKeySet*)keyset
{
    XCTAssertNotNil(keyset.tlk, "Should have a TLK to put a key set in CloudKit");
    CKRecordZoneID* zoneID = keyset.tlk.zoneID;
    XCTAssertNotNil(zoneID, "Should have a zoneID to put a key set in CloudKit");

    ZoneKeys* zonekeys = self.keys[zoneID];
    XCTAssertNil(zonekeys, "Should not already have zone keys when putting keyset in cloudkit");
    zonekeys = [[ZoneKeys alloc] initWithZoneID:zoneID
                                      contextID:self.defaultCKKS.operationDependencies.contextID];

    FakeCKZone* zone = self.zones[zoneID];
    // Cuttlefish makes this for you, but for now assert if there's an issue
    XCTAssertNotNil(zone, "Should already have a fakeckzone before putting a keyset in it");

    __block NSMutableArray<CKRecord*>* newRecords = [NSMutableArray array];
    dispatch_sync(zone.queue, ^{
        [newRecords addObject:[zone _onqueueAddToZone:keyset.tlk    zoneID:zoneID]];
        [newRecords addObject:[zone _onqueueAddToZone:keyset.classA zoneID:zoneID]];
        [newRecords addObject:[zone _onqueueAddToZone:keyset.classC zoneID:zoneID]];

        [newRecords addObject:[zone _onqueueAddToZone:keyset.currentTLKPointer    zoneID:zoneID]];
        [newRecords addObject:[zone _onqueueAddToZone:keyset.currentClassAPointer zoneID:zoneID]];
        [newRecords addObject:[zone _onqueueAddToZone:keyset.currentClassCPointer zoneID:zoneID]];

        // TODO handle a rolled TLK
        //if(zonekeys.rolledTLK) {
        //    [zone _onqueueAddToZone:zonekeys.rolledTLK zoneID:zoneID];
        //}

        zonekeys.tlk = keyset.tlk;
        zonekeys.classA = keyset.classA;
        zonekeys.classC = keyset.classC;
        self.keys[zoneID] = zonekeys;

        // Octagon uploads the pending TLKshares, not all of them
        for(CKKSTLKShareRecord* tlkshare in keyset.pendingTLKShares) {
            [newRecords addObject:[zone _onqueueAddToZone:tlkshare zoneID:zoneID]];
        }
    });

    return newRecords;
}

- (void)performOctagonTLKUpload:(NSSet<CKKSKeychainViewState*>*)views
{
    [self performOctagonTLKUpload:views afterUpload:nil];
}

- (void)performOctagonTLKUpload:(NSSet<CKKSKeychainViewState*>*)views afterUpload:(void (^_Nullable)(void))afterUpload
{
    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keysetOps = [NSMutableArray array];

    for(CKKSKeychainViewState* view in views) {
        if(view.ckksManagedView) {
            XCTAssertEqual(0, [view.keyHierarchyConditions[SecCKKSZoneKeyStateWaitForTLKCreation] wait:20*NSEC_PER_SEC], @"key state should enter 'waitfortlkcreation' (view %@)", view);
        }
    }
    XCTAssertEqual(0, [self.defaultCKKS.stateConditions[CKKSStateReady] wait:20*NSEC_PER_SEC], @"CKKS state machine should enter 'ready'");
    [keysetOps addObject:[self.defaultCKKS findKeySets:NO]];

    NSMutableArray<CKRecord*>* keyHierarchyRecords = [NSMutableArray array];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keysetOp in keysetOps) {
        // Wait until finished is usually a bad idea. We could rip this out into an operation if we'd like.
        [keysetOp waitUntilFinished];
        XCTAssertNil(keysetOp.error, "Should be no error fetching keysets from CKKS");

        NSMutableDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>* keysets = [NSMutableDictionary dictionary];
        for(CKRecordZoneID* zoneID in keysetOp.keysets) {
            for(CKKSKeychainViewState* viewState in views) {
                if([viewState.zoneID.zoneName isEqualToString:zoneID.zoneName]) {
                    XCTAssertTrue(keysetOp.keysets[zoneID].proposed, "Keyset we're writing to CK should be proposed");
                    keysets[zoneID] = keysetOp.keysets[zoneID];
                }
            }
        }

        NSArray<CKRecord*>* records = [self performOctagonKeySetWrites:keysets];
        [keyHierarchyRecords addObjectsFromArray:records];
    }

    if(afterUpload) {
        afterUpload();
    }

    // Tell our views about our shiny new records!
    [self.defaultCKKS receiveTLKUploadRecords:keyHierarchyRecords];
}

- (NSArray<CKRecord*>*)performOctagonKeySetWrites:(NSDictionary<CKRecordZoneID*, CKKSCurrentKeySet*>*)keysets
{
    NSMutableArray<CKRecord*>* records = [NSMutableArray array];
    for(CKRecordZoneID* zoneID in keysets) {
        CKKSCurrentKeySet* keyset = keysets[zoneID];

        NSArray<CKRecord*>* newRecords = [self putKeySetInCloudKit:keyset];
        [records addObjectsFromArray:newRecords];
    }

    return records;
}

- (void)saveTLKMaterialToKeychainSimulatingSOS: (CKRecordZoneID*)zoneID {

    XCTAssertNotNil(self.keys[zoneID].tlk, "Have a TLK to save for zone %@", zoneID);

    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        bool ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteSOSTransactionType, &cferror, ^bool {
            NSError* error = nil;
            [self.keys[zoneID].tlk saveKeyMaterialToKeychain: false error:&error];
            XCTAssertNil(error, @"Saved TLK material to keychain");
            return true;
        });
        return ok;
    });

    XCTAssertNil( (__bridge NSError*)cferror, @"no error with transaction");
    CFReleaseNull(cferror);
}
static SOSFullPeerInfoRef SOSCreateFullPeerInfoFromName(CFStringRef name,
                                                        SecKeyRef* outSigningKey,
                                                        SecKeyRef* outOctagonSigningKey,
                                                        SecKeyRef* outOctagonEncryptionKey,
                                                        CFErrorRef *error)
{
    SOSFullPeerInfoRef result = NULL;
    SecKeyRef publicKey = NULL;
    CFDictionaryRef gestalt = NULL;
    
    *outSigningKey = GeneratePermanentFullECKey(256, name, error);
    
    *outOctagonSigningKey = GeneratePermanentFullECKey(384, name, error);
    *outOctagonEncryptionKey = GeneratePermanentFullECKey(384, name, error);

    gestalt = SOSCreatePeerGestaltFromName(name);
    
    result = SOSFullPeerInfoCreate(NULL, gestalt, name, NULL, *outSigningKey,
                                   *outOctagonSigningKey, *outOctagonEncryptionKey,
                                   error);
    
    CFReleaseNull(gestalt);
    CFReleaseNull(publicKey);
    
    return result;
}
- (NSMutableArray<NSData *>*) SOSPiggyICloudIdentities
{
    SecKeyRef signingKey = NULL;
    SecKeyRef octagonSigningKey = NULL;
    SecKeyRef octagonEncryptionKey = NULL;
    NSMutableArray<NSData *>* icloudidentities = [NSMutableArray array];

    SOSFullPeerInfoRef fpi = SOSCreateFullPeerInfoFromName(CFSTR("Test Peer"), &signingKey, &octagonSigningKey, &octagonEncryptionKey, NULL);
    
    NSData *data = CFBridgingRelease(SOSPeerInfoCopyData(SOSFullPeerInfoGetPeerInfo(fpi), NULL));
    CFReleaseNull(fpi);
    if (data)
        [icloudidentities addObject:data];
    
    CFReleaseNull(signingKey);
    CFReleaseNull(octagonSigningKey);
    CFReleaseNull(octagonEncryptionKey);

    return icloudidentities;
}
static CFDictionaryRef SOSCreatePeerGestaltFromName(CFStringRef name)
{
    return CFDictionaryCreateForCFTypes(kCFAllocatorDefault,
                                        kPIUserDefinedDeviceNameKey, name,
                                        NULL);
}
-(NSMutableDictionary*)SOSPiggyBackCopyFromKeychain
{
    __block NSMutableDictionary *piggybackdata = [[NSMutableDictionary alloc] init];
    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        bool ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteSOSTransactionType, &cferror, ^bool {
            piggybackdata[@"idents"] = [self SOSPiggyICloudIdentities];
            piggybackdata[@"tlk"] = SOSAccountGetAllTLKs();

            return true;
        });
        return ok;
    });
    
    XCTAssertNil( (__bridge NSError*)cferror, @"no error with transaction");
    CFReleaseNull(cferror);
    return piggybackdata;
}

- (void)SOSPiggyBackAddToKeychain:(NSDictionary*)piggydata{
    __block CFErrorRef cferror = NULL;
    kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        bool ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteSOSTransactionType, &cferror, ^bool {
            NSError* error = nil;
            NSArray* icloudidentities = piggydata[@"idents"];
            NSArray* tlk = piggydata[@"tlk"];

            SOSPiggyBackAddToKeychain(icloudidentities, tlk);
            
            XCTAssertNil(error, @"Saved TLK-piggy material to keychain");
            return true;
        });
        return ok;
    });
    
    XCTAssertNil( (__bridge NSError*)cferror, @"no error with transaction");
    CFReleaseNull(cferror);
}

- (void)saveTLKMaterialToKeychain: (CKRecordZoneID*)zoneID {
    NSError* error = nil;
    XCTAssertNotNil(self.keys[zoneID].tlk, "Have a TLK to save for zone %@", zoneID);

    // Don't make the stashed local copy of the TLK
    [self.keys[zoneID].tlk saveKeyMaterialToKeychain:false error:&error];
    XCTAssertNil(error, @"Saved TLK material to keychain");
}

- (void)deleteTLKMaterialFromKeychain: (CKRecordZoneID*)zoneID {
    NSError* error = nil;
    XCTAssertNotNil(self.keys[zoneID].tlk, "Have a TLK to save for zone %@", zoneID);
    [self.keys[zoneID].tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, @"Saved TLK material to keychain");
}

- (void)saveClassKeyMaterialToKeychain: (CKRecordZoneID*)zoneID {
    NSError* error = nil;
    XCTAssertNotNil(self.keys[zoneID].classA, "Have a Class A to save for zone %@", zoneID);
    [self.keys[zoneID].classA saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, @"Saved Class A material to keychain");

    XCTAssertNotNil(self.keys[zoneID].classC, "Have a Class C to save for zone %@", zoneID);
    [self.keys[zoneID].classC saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, @"Saved Class C material to keychain");
}


- (void)putTLKShareInCloudKit:(CKKSKey*)key
                         from:(id<CKKSSelfPeer>)sharingPeer
                           to:(id<CKKSPeer>)receivingPeer
                       zoneID:(CKRecordZoneID*)zoneID
{
    NSError* error = nil;

    CKKSKeychainBackedKey* keychainBackedKey = [key getKeychainBackedKey:&error];
    XCTAssertNotNil(keychainBackedKey, "Should have some keychain-backed key to share");
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");

    CKKSTLKShareRecord* share = [CKKSTLKShareRecord share:keychainBackedKey
                                                contextID:self.defaultCKKS.operationDependencies.contextID
                                                       as:sharingPeer
                                                       to:receivingPeer
                                                    epoch:-1
                                                 poisoned:0
                                                    error:&error];
    XCTAssertNil(error, "Should have been no error sharing a CKKSKey");
    XCTAssertNotNil(share, "Should be able to create a share");

    CKRecord* shareRecord = [share CKRecordWithZoneID: zoneID];
    XCTAssertNotNil(shareRecord, "Should have been able to create a CKRecord");

    FakeCKZone* zone = self.zones[zoneID];
    XCTAssertNotNil(zone, "Should have a zone to put a TLKShare in");
    [zone addToZone:shareRecord];

    ZoneKeys* keys = self.keys[zoneID];
    XCTAssertNotNil(keys, "Have a zonekeys object for this zone");
    keys.tlkShares = keys.tlkShares ? [keys.tlkShares arrayByAddingObject:share] : @[share];
}

- (void)putTLKSharesInCloudKit:(CKKSKey*)key
                          from:(id<CKKSSelfPeer>)sharingPeer
                        zoneID:(CKRecordZoneID*)zoneID
{
    NSSet* peers = [self.mockSOSAdapter.trustedPeers setByAddingObject:self.mockSOSAdapter.selfPeer];

    for(id<CKKSPeer> peer in peers) {
        // Can only send to peers with encryption keys
        if(peer.publicEncryptionKey) {
            [self putTLKShareInCloudKit:key from:sharingPeer to:peer zoneID:zoneID];
        }
    }
}

- (void)putSelfTLKSharesInCloudKit:(CKRecordZoneID*)zoneID {
    CKKSKey* tlk = self.keys[zoneID].tlk;
    XCTAssertNotNil(tlk, "Should have a TLK for zone %@", zoneID);
    [self putTLKSharesInCloudKit:tlk from:self.mockSOSAdapter.selfPeer zoneID:zoneID];
}

- (void)saveTLKSharesInLocalDatabase:(CKRecordZoneID*)zoneID {
    ZoneKeys* keys = self.keys[zoneID];
    XCTAssertNotNil(keys, "Have a zonekeys object for this zone");

    [CKKSSQLDatabaseObject performCKKSTransaction:^CKKSDatabaseTransactionResult{
        for(CKKSTLKShareRecord* share in keys.tlkShares) {
            NSError* error = nil;
            [share saveToDatabase:&error];
            XCTAssertNil(error, "Shouldn't have been an error saving a TLKShare to the database");
        }

        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)createAndSaveFakeKeyHierarchy: (CKRecordZoneID*)zoneID {
    // Put in CloudKit first, so the records on-disk will have the right change tags
    [self putFakeKeyHierarchyInCloudKit:       zoneID];
    [self saveFakeKeyHierarchyToLocalDatabase: zoneID];
    [self saveTLKMaterialToKeychain:           zoneID];
    [self saveClassKeyMaterialToKeychain:      zoneID];

    [self putSelfTLKSharesInCloudKit:zoneID];
    [self saveTLKSharesInLocalDatabase:zoneID];
}

// Override our base class here, but only for Keychain Views

- (void)expectCKModifyRecords:(NSDictionary<NSString*, NSNumber*>*)expectedRecordTypeCounts
      deletedRecordTypeCounts:(NSDictionary<NSString*, NSNumber*>*)expectedDeletedRecordTypeCounts
                       zoneID:(CKRecordZoneID*)zoneID
          checkModifiedRecord:(BOOL (^ _Nullable)(CKRecord*))checkRecord
        inspectOperationGroup:(void (^ _Nullable)(CKOperationGroup * _Nullable))inspectOperationGroup
         runAfterModification:(void (^ _Nullable) (void))afterModification
{

    void (^newAfterModification)(void) = afterModification;
    if([self.ckksZones containsObject:zoneID]) {
        __weak __typeof(self) weakSelf = self;
        newAfterModification = ^{
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            XCTAssertNotNil(strongSelf, "self exists");

            // Reach into our cloudkit database and extract the keys
            CKRecordID* currentTLKPointerID =    [[CKRecordID alloc] initWithRecordName:SecCKKSKeyClassTLK zoneID:zoneID];
            CKRecordID* currentClassAPointerID = [[CKRecordID alloc] initWithRecordName:SecCKKSKeyClassA   zoneID:zoneID];
            CKRecordID* currentClassCPointerID = [[CKRecordID alloc] initWithRecordName:SecCKKSKeyClassC   zoneID:zoneID];

            ZoneKeys* zonekeys = strongSelf.keys[zoneID];
            if(!zonekeys) {
                zonekeys = [[ZoneKeys alloc] initWithZoneID:zoneID
                                                  contextID:self.defaultCKKS.operationDependencies.contextID];
                strongSelf.keys[zoneID] = zonekeys;
            }

            XCTAssertNotNil(strongSelf.zones[zoneID].currentDatabase[currentTLKPointerID],    "Have a currentTLKPointer");
            XCTAssertNotNil(strongSelf.zones[zoneID].currentDatabase[currentClassAPointerID], "Have a currentClassAPointer");
            XCTAssertNotNil(strongSelf.zones[zoneID].currentDatabase[currentClassCPointerID], "Have a currentClassCPointer");
            XCTAssertNotNil(strongSelf.zones[zoneID].currentDatabase[currentTLKPointerID][SecCKRecordParentKeyRefKey],    "Have a currentTLKPointer parent");
            XCTAssertNotNil(strongSelf.zones[zoneID].currentDatabase[currentClassAPointerID][SecCKRecordParentKeyRefKey], "Have a currentClassAPointer parent");
            XCTAssertNotNil(strongSelf.zones[zoneID].currentDatabase[currentClassCPointerID][SecCKRecordParentKeyRefKey], "Have a currentClassCPointer parent");
            XCTAssertNotNil([strongSelf.zones[zoneID].currentDatabase[currentTLKPointerID][SecCKRecordParentKeyRefKey] recordID].recordName,    "Have a currentTLKPointer parent UUID");
            XCTAssertNotNil([strongSelf.zones[zoneID].currentDatabase[currentClassAPointerID][SecCKRecordParentKeyRefKey] recordID].recordName, "Have a currentClassAPointer parent UUID");
            XCTAssertNotNil([strongSelf.zones[zoneID].currentDatabase[currentClassCPointerID][SecCKRecordParentKeyRefKey] recordID].recordName, "Have a currentClassCPointer parent UUID");

            zonekeys.currentTLKPointer =    [[CKKSCurrentKeyPointer alloc] initWithCKRecord: strongSelf.zones[zoneID].currentDatabase[currentTLKPointerID] contextID:self.defaultCKKS.operationDependencies.contextID];
            zonekeys.currentClassAPointer = [[CKKSCurrentKeyPointer alloc] initWithCKRecord: strongSelf.zones[zoneID].currentDatabase[currentClassAPointerID] contextID:self.defaultCKKS.operationDependencies.contextID];
            zonekeys.currentClassCPointer = [[CKKSCurrentKeyPointer alloc] initWithCKRecord: strongSelf.zones[zoneID].currentDatabase[currentClassCPointerID] contextID:self.defaultCKKS.operationDependencies.contextID];

            XCTAssertNotNil(zonekeys.currentTLKPointer.currentKeyUUID,    "Have a currentTLKPointer current UUID");
            XCTAssertNotNil(zonekeys.currentClassAPointer.currentKeyUUID, "Have a currentClassAPointer current UUID");
            XCTAssertNotNil(zonekeys.currentClassCPointer.currentKeyUUID, "Have a currentClassCPointer current UUID");

            CKRecordID* currentTLKID =    [[CKRecordID alloc] initWithRecordName:zonekeys.currentTLKPointer.currentKeyUUID    zoneID:zoneID];
            CKRecordID* currentClassAID = [[CKRecordID alloc] initWithRecordName:zonekeys.currentClassAPointer.currentKeyUUID zoneID:zoneID];
            CKRecordID* currentClassCID = [[CKRecordID alloc] initWithRecordName:zonekeys.currentClassCPointer.currentKeyUUID zoneID:zoneID];

            zonekeys.tlk =    [[CKKSKey alloc] initWithCKRecord: strongSelf.zones[zoneID].currentDatabase[currentTLKID] contextID:self.defaultCKKS.operationDependencies.contextID];
            zonekeys.classA = [[CKKSKey alloc] initWithCKRecord: strongSelf.zones[zoneID].currentDatabase[currentClassAID] contextID:self.defaultCKKS.operationDependencies.contextID];
            zonekeys.classC = [[CKKSKey alloc] initWithCKRecord: strongSelf.zones[zoneID].currentDatabase[currentClassCID] contextID:self.defaultCKKS.operationDependencies.contextID];

            XCTAssertNotNil(zonekeys.tlk, "Have the current TLK");
            XCTAssertNotNil(zonekeys.classA, "Have the current Class A key");
            XCTAssertNotNil(zonekeys.classC, "Have the current Class C key");

            NSMutableArray<CKKSTLKShareRecord*>* shares = [NSMutableArray array];
            for(CKRecordID* recordID in strongSelf.zones[zoneID].currentDatabase.allKeys) {
                if([recordID.recordName hasPrefix: [CKKSTLKShareRecord ckrecordPrefix]]) {
                    CKKSTLKShareRecord* share = [[CKKSTLKShareRecord alloc] initWithCKRecord:strongSelf.zones[zoneID].currentDatabase[recordID] contextID:self.defaultCKKS.operationDependencies.contextID];
                    XCTAssertNotNil(share, "Should be able to parse a CKKSTLKShare CKRecord into a CKKSTLKShare");
                    [shares addObject:share];
                }
            }
            zonekeys.tlkShares = shares;

            if(afterModification) {
                afterModification();
            }
        };
    }

    [super expectCKModifyRecords:expectedRecordTypeCounts
         deletedRecordTypeCounts:expectedDeletedRecordTypeCounts
                          zoneID:zoneID
             checkModifiedRecord:checkRecord
           inspectOperationGroup:inspectOperationGroup
            runAfterModification:newAfterModification];
}

- (void)expectCKReceiveSyncKeyHierarchyError:(CKRecordZoneID*)zoneID {

    __weak __typeof(self) weakSelf = self;
    [[self.mockDatabase expect] addOperation:[OCMArg checkWithBlock:^BOOL(id obj) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        __block bool rejected = false;
        if ([obj isKindOfClass:[CKModifyRecordsOperation class]]) {
            CKModifyRecordsOperation *op = (CKModifyRecordsOperation *)obj;

            if(!op.atomic) {
                // We only care about atomic operations
                return NO;
            }

            // We want to only match zone updates pertaining to this zone
            for(CKRecord* record in op.recordsToSave) {
                if(![record.recordID.zoneID isEqual: zoneID]) {
                    return NO;
                }
            }

            // we oly want to match updates that are updating a class C or class A CKP
            bool updatingClassACKP = false;
            bool updatingClassCCKP = false;
            for(CKRecord* record in op.recordsToSave) {
                if([record.recordID.recordName isEqualToString:SecCKKSKeyClassA]) {
                    updatingClassACKP = true;
                }
                if([record.recordID.recordName isEqualToString:SecCKKSKeyClassC]) {
                    updatingClassCCKP = true;
                }
            }

            if(!updatingClassACKP && !updatingClassCCKP) {
                return NO;
            }

            FakeCKZone* zone = strongSelf.zones[zoneID];
            XCTAssertNotNil(zone, "Should have a zone for these records");
            if (zone == nil) {
                return NO;
            }

            // We only want to match if the synckeys aren't pointing correctly

            ZoneKeys* zonekeys = [[ZoneKeys alloc] initLoadingRecordsFromZone:zone
                                                                    contextID:CKKSMockCloudKitContextID];

            XCTAssertNotNil(zonekeys.currentTLKPointer,    "Have a currentTLKPointer");
            XCTAssertNotNil(zonekeys.currentClassAPointer, "Have a currentClassAPointer");
            XCTAssertNotNil(zonekeys.currentClassCPointer, "Have a currentClassCPointer");

            XCTAssertNotNil(zonekeys.tlk,    "Have the current TLK");
            XCTAssertNotNil(zonekeys.classA, "Have the current Class A key");
            XCTAssertNotNil(zonekeys.classC, "Have the current Class C key");

            // Ensure that either the Class A synckey or the class C synckey do not immediately wrap to the current TLK
            bool classALinkBroken = ![zonekeys.classA.parentKeyUUID isEqualToString:zonekeys.tlk.uuid];
            bool classCLinkBroken = ![zonekeys.classC.parentKeyUUID isEqualToString:zonekeys.tlk.uuid];

            // Neither synckey link is broken. Don't match this operation.
            if(!classALinkBroken && !classCLinkBroken) {
                return NO;
            }

            NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = [[NSMutableDictionary alloc] init];

            @synchronized(zone.currentDatabase) {
                for(CKRecord* record in op.recordsToSave) {
                    if(classALinkBroken && [record.recordID.recordName isEqualToString:SecCKKSKeyClassA]) {
                        failedRecords[record.recordID] = [strongSelf ckInternalServerExtensionError:CKKSServerUnexpectedSyncKeyInChain description:@"synckey record: current classA synckey does not point to current tlk synckey"];
                        rejected = true;
                    }
                    if(classCLinkBroken && [record.recordID.recordName isEqualToString:SecCKKSKeyClassC]) {
                        failedRecords[record.recordID] = [strongSelf ckInternalServerExtensionError:CKKSServerUnexpectedSyncKeyInChain description:@"synckey record: current classC synckey does not point to current tlk synckey"];
                        rejected = true;
                    }
                }
            }

            if(rejected) {
                [strongSelf rejectWrite: op failedRecords:failedRecords];
            }
        }
        return rejected ? YES : NO;
    }]];
}

- (void)checkNoCKKSDataForView:(CKKSKeychainViewState*)viewState
{
    // Test that there are no items in the database
    [self.defaultCKKS dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;
        NSArray<CKKSMirrorEntry*>* ckmes = [CKKSMirrorEntry allWithContextID:self.defaultCKKS.operationDependencies.contextID zoneID:viewState.zoneID error:&error];
        XCTAssertNil(error, "No error fetching CKMEs");
        XCTAssertEqual(ckmes.count, 0ul, "No CKMirrorEntries");

        NSArray<CKKSOutgoingQueueEntry*>* oqes = [CKKSOutgoingQueueEntry allWithContextID:self.defaultCKKS.operationDependencies.contextID zoneID:viewState.zoneID error:&error];
        XCTAssertNil(error, "No error fetching OQEs");
        XCTAssertEqual(oqes.count, 0ul, "No OutgoingQueueEntries");

        NSArray<CKKSIncomingQueueEntry*>* iqes = [CKKSIncomingQueueEntry allWithContextID:self.defaultCKKS.operationDependencies.contextID zoneID:viewState.zoneID error:&error];
        XCTAssertNil(error, "No error fetching IQEs");
        XCTAssertEqual(iqes.count, 0ul, "No IncomingQueueEntries");

        NSArray<CKKSKey*>* keys = [CKKSKey allWithContextID:self.defaultCKKS.operationDependencies.contextID zoneID:viewState.zoneID error:&error];
        XCTAssertNil(error, "No error fetching keys");
        XCTAssertEqual(keys.count, 0ul, "No CKKSKeys");

        NSArray<CKKSDeviceStateEntry*>* deviceStates = [CKKSDeviceStateEntry allInZone:viewState.zoneID error:&error];
        XCTAssertNil(error, "should be no error fetching device states");
        XCTAssertEqual(deviceStates.count, 0ul, "No Device State entries");
    }];
}

- (BOOL (^) (CKRecord*)) checkClassABlock: (CKRecordZoneID*) zoneID message:(NSString*) message {
    __weak __typeof(self) weakSelf = self;
    return ^BOOL(CKRecord* record) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        ZoneKeys* zoneKeys = strongSelf.keys[zoneID];
        XCTAssertNotNil(zoneKeys, "Have zone keys for %@", zoneID);
        XCTAssertEqualObjects([record[SecCKRecordParentKeyRefKey] recordID].recordName, zoneKeys.classA.uuid, "%@", message);
        return [[record[SecCKRecordParentKeyRefKey] recordID].recordName isEqual: zoneKeys.classA.uuid];
    };
}

- (BOOL (^) (CKRecord*)) checkClassCBlock: (CKRecordZoneID*) zoneID message:(NSString*) message {
    __weak __typeof(self) weakSelf = self;
    return ^BOOL(CKRecord* record) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");

        ZoneKeys* zoneKeys = strongSelf.keys[zoneID];
        XCTAssertNotNil(zoneKeys, "Have zone keys for %@", zoneID);
        XCTAssertEqualObjects([record[SecCKRecordParentKeyRefKey] recordID].recordName, zoneKeys.classC.uuid, "%@", message);
        return [[record[SecCKRecordParentKeyRefKey] recordID].recordName isEqual: zoneKeys.classC.uuid];
    };
}

- (BOOL (^) (CKRecord*)) checkPasswordBlock:(CKRecordZoneID*)zoneID
                                    account:(NSString*)account
                                   password:(NSString*)password  {
    __weak __typeof(self) weakSelf = self;
    return ^BOOL(CKRecord* record) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        XCTAssertNotNil(strongSelf, "self exists");
        if (strongSelf == nil) {
            return NO;
        }

        ZoneKeys* zoneKeys = strongSelf.keys[zoneID];
        XCTAssertNotNil(zoneKeys, "Have zone keys for %@", zoneID);
        XCTAssertNotNil([record[SecCKRecordParentKeyRefKey] recordID].recordName, "Have a wrapping key");

        CKKSKey* key = nil;
        if([[record[SecCKRecordParentKeyRefKey] recordID].recordName isEqualToString: zoneKeys.classC.uuid]) {
            key = zoneKeys.classC;
        } else if([[record[SecCKRecordParentKeyRefKey] recordID].recordName isEqualToString: zoneKeys.classA.uuid]) {
            key = zoneKeys.classA;
        }
        XCTAssertNotNil(key, "Found a key via UUID");

        CKKSMirrorEntry* ckme = [[CKKSMirrorEntry alloc] initWithCKRecord:record contextID:self.defaultCKKS.operationDependencies.contextID];

        NSError* error = nil;
        NSDictionary* dict = [CKKSItemEncrypter decryptItemToDictionary:ckme.item keyCache:nil error:&error];
        XCTAssertNil(error, "No error decrypting item");
        XCTAssertEqualObjects(account, dict[(id)kSecAttrAccount], "Account matches");
        XCTAssertEqualObjects([password dataUsingEncoding:NSUTF8StringEncoding], dict[(id)kSecValueData], "Password matches");
        return YES;
    };
}

- (NSDictionary*)fakeRecordDictionary:(NSString*) account zoneID:(CKRecordZoneID*)zoneID {
    return [self fakeRecordDictionary:account password:nil zoneID:zoneID];
}

- (NSDictionary*)fakeRecordDictionary:(NSString* _Nullable)account password:(NSString* _Nullable)password zoneID:(CKRecordZoneID*)zoneID
{
    NSError* error = nil;

    /* Basically: @{
     @"acct"  : @"account-delete-me",
     @"agrp"  : @"com.apple.security.sos",
     @"cdat"  : @"2016-12-21 03:33:25 +0000",
     @"class" : @"genp",
     @"mdat"  : @"2016-12-21 03:33:25 +0000",
     @"musr"  : [[NSData alloc] init],
     @"pdmn"  : @"ak",
     @"sha1"  : [[NSData alloc] initWithBase64EncodedString: @"C3VWONaOIj8YgJjk/xwku4By1CY=" options:0],
     @"svce"  : @"",
     @"tomb"  : [NSNumber numberWithInt: 0],
     @"v_Data" : [@"data" dataUsingEncoding: NSUTF8StringEncoding],
     };
     TODO: this should be binary encoded instead of expanded, but the plist encoder should handle it fine */
    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"PD94bWwgdmVyc2lvbj0iMS4wIiBlbmNvZGluZz0iVVRGLTgiPz4KPCFET0NUWVBFIHBsaXN0IFBVQkxJQyAiLS8vQXBwbGUvL0RURCBQTElTVCAxLjAvL0VOIiAiaHR0cDovL3d3dy5hcHBsZS5jb20vRFREcy9Qcm9wZXJ0eUxpc3QtMS4wLmR0ZCI+CjxwbGlzdCB2ZXJzaW9uPSIxLjAiPgo8ZGljdD4KCTxrZXk+YWNjdDwva2V5PgoJPHN0cmluZz5hY2NvdW50LWRlbGV0ZS1tZTwvc3RyaW5nPgoJPGtleT5hZ3JwPC9rZXk+Cgk8c3RyaW5nPmNvbS5hcHBsZS5zZWN1cml0eS5zb3M8L3N0cmluZz4KCTxrZXk+Y2RhdDwva2V5PgoJPGRhdGU+MjAxNi0xMi0yMVQwMzozMzoyNVo8L2RhdGU+Cgk8a2V5PmNsYXNzPC9rZXk+Cgk8c3RyaW5nPmdlbnA8L3N0cmluZz4KCTxrZXk+bWRhdDwva2V5PgoJPGRhdGU+MjAxNi0xMi0yMVQwMzozMzoyNVo8L2RhdGU+Cgk8a2V5Pm11c3I8L2tleT4KCTxkYXRhPgoJPC9kYXRhPgoJPGtleT5wZG1uPC9rZXk+Cgk8c3RyaW5nPmFrPC9zdHJpbmc+Cgk8a2V5PnNoYTE8L2tleT4KCTxkYXRhPgoJQzNWV09OYU9JajhZZ0pqay94d2t1NEJ5MUNZPQoJPC9kYXRhPgoJPGtleT5zdmNlPC9rZXk+Cgk8c3RyaW5nPjwvc3RyaW5nPgoJPGtleT50b21iPC9rZXk+Cgk8aW50ZWdlcj4wPC9pbnRlZ2VyPgoJPGtleT52X0RhdGE8L2tleT4KCTxkYXRhPgoJWkdGMFlRPT0KCTwvZGF0YT4KPC9kaWN0Pgo8L3BsaXN0Pgo=" options:0];
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:itemdata
                                                                            options:0
                                                                             format:nil
                                                                              error:&error] mutableCopy];
    // Fix up dictionary
    item[@"agrp"] = @"com.apple.security.ckks";
    item[@"vwht"] = @"keychain";
    XCTAssertNil(error, "no error interpreting data as item");
    XCTAssertNotNil(item, "interpreted data as item");

    if(zoneID && ![zoneID.zoneName isEqualToString:@"keychain"]) {
        [item setObject: zoneID.zoneName forKey: (__bridge id) kSecAttrSyncViewHint];
    }

    if(account) {
        [item setObject: account forKey: (__bridge id) kSecAttrAccount];
    }
    if(password) {
        item[(id)kSecValueData] = [password dataUsingEncoding:NSUTF8StringEncoding];
    }
    return item;
}

- (NSDictionary*)fakeCertificateRecordDictionary:(NSData*)serialNumber zoneID:(CKRecordZoneID*)zoneID
{
    /*
     (lldb) po dict
     {
         agrp = "com.apple.security.ckks";
         cdat = "2021-11-12 21:15:24 +0000";
         cenc = 3;
         class = cert;
         ctyp = 3;
         issr = {length = 0, bytes = 0x};
         mdat = "2021-11-12 21:15:24 +0000";
         musr = {length = 0, bytes = 0x};
         pdmn = ck;
         pkhh = {length = 20, bytes = 0x6ab746c54112d11093801f7355431358267fea12};
         sha1 = {length = 20, bytes = 0x00365446ba08ea1d87a80cb373a0086deb7b5078};
         slnr = {length = 4, bytes = 0x31323334};
         subj = {length = 0, bytes = 0x};
         tomb = 0;
         "v_Data" = {length = 1149, bytes = 0x30820479 30820261 a0030201 02020101 ... 65df4f8f 4f69dae2 };
         vwht = keychain;
     }
     */
    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"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"

                                                           options:0];
    NSError* error = nil;
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:[CKKSItemEncrypter removePaddingFromData:itemdata]
                                                                            options:0
                                                                             format:nil
                                                                              error:&error] mutableCopy];
    XCTAssertNil(error, "no error interpreting data as item");
    XCTAssertNotNil(item, "interpreted data as item");

    if(zoneID && ![zoneID.zoneName isEqualToString:@"keychain"]) {
        [item setObject: zoneID.zoneName forKey: (__bridge id) kSecAttrSyncViewHint];
    }

    if(serialNumber) {
        item[(id)kSecAttrSerialNumber] = serialNumber;
    }

    return item;
}

- (NSDictionary*)fakeINetRecordDictionary:(NSString*)server
                                  account:(NSString*)account
                                   zoneID:(CKRecordZoneID*)zoneID
{
    /*
     {
     acct = "account-delete-me-local-addition";
     agrp = "com.apple.security.ckks";
     atyp = "";
     bin0 = {length = 10, bytes = 0x62696e30206669656c64};
     bin1 = {length = 10, bytes = 0x62696e31206669656c64};
     bin2 = {length = 10, bytes = 0x62696e32206669656c64};
     bin3 = {length = 10, bytes = 0x62696e33206669656c64};
     bini = {length = 10, bytes = 0x62696e68206669656c64};
     binn = {length = 10, bytes = 0x62696e6e206669656c64};
     cdat = "2022-04-11 23:45:47 +0000";
     class = inet;
     mdat = "2022-04-11 23:45:47 +0000";
     musr = {length = 0, bytes = 0x};
     path = "";
     pdmn = ck;
     port = 0;
     ptcl = 0;
     sdmn = "";
     sha1 = {length = 20, bytes = 0xee2534a55516ca83ba860f49e181975c09497a7c};
     srvr = "server-delete-me-local-addition";
     tomb = 0;
     vwht = keychain;
     }
    */
    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"YnBsaXN0MDDfEBYBAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhsXHB0eFx8gISIjJCUgIiYiJ1RzZG1uVGJpbjNVY2xhc3NUc3J2clRiaW4xVHBhdGhUYWdycFRwZG1uVGJpbmlUYXR5cFRiaW5uVG1kYXRUdndodFRwb3J0VGJpbjJUc2hhMVRtdXNyVGNkYXRUcHRjbFRiaW4wVHRvbWJUYWNjdFBKYmluMyBmaWVsZFRpbmV0XxAfc2VydmVyLWRlbGV0ZS1tZS1sb2NhbC1hZGRpdGlvbkpiaW4xIGZpZWxkXxAXY29tLmFwcGxlLnNlY3VyaXR5LmNra3NSY2tKYmluaCBmaWVsZEpiaW5uIGZpZWxkM0HEAnqVrGxdWGtleWNoYWluEABKYmluMiBmaWVsZE8QFO4lNKVVFsqDuoYPSeGBl1wJSXp8QEpiaW4wIGZpZWxkXxAgYWNjb3VudC1kZWxldGUtbWUtbG9jYWwtYWRkaXRpb24ACAA3ADwAQQBHAEwAUQBWAFsAYABlAGoAbwB0AHkAfgCDAIgAjQCSAJcAnAChAKYApwCyALcA2QDkAP4BAQEMARcBIAEpASsBNgFNAU4BWQAAAAAAAAIBAAAAAAAAACgAAAAAAAAAAAAAAAAAAAF8"
options:0];

    NSError* error = nil;
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:itemdata
                                                                            options:0
                                                                             format:nil
                                                                              error:&error] mutableCopy];
    XCTAssertNil(error, "no error interpreting data as item");
    XCTAssertNotNil(item, "interpreted data as item");

    if(zoneID && ![zoneID.zoneName isEqualToString:@"keychain"]) {
        [item setObject:zoneID.zoneName forKey:(__bridge id)kSecAttrSyncViewHint];
    }

    if(server) {
        item[(id)kSecAttrServer] = server;
    }
    if(account) {
        item[(id)kSecAttrAccount] = account;
    }

    return item;
}

- (NSDictionary*)fakeKeyRecordDictionary:(NSString*)label zoneID:(CKRecordZoneID*)zoneID
{
    /*
     (lldb) po dict
     {
     agrp = "com.apple.security.ckks";
     asen = 0;
     atag = "";
     bsiz = 4096;
     cdat = "2022-07-06 00:25:55 +0000";
     class = keys;
     crtr = 0;
     decr = 1;
     drve = 0;
     edat = "2001-01-01 00:00:00 +0000";
     encr = 0;
     esiz = 4096;
     extr = 1;
     kcls = 1;
     klbl = {length = 20, bytes = 0xb4bdbd651468fe08443c6d0c910ef3fe15c9619b};
     labl = testkey;
     mdat = "2022-07-06 00:25:55 +0000";
     modi = 1;
     musr = {length = 0, bytes = 0x};
     next = 0;
     pdmn = ck;
     perm = 1;
     priv = 1;
     sdat = "2001-01-01 00:00:00 +0000";
     sens = 0;
     sha1 = {length = 20, bytes = 0x05bf30bdf70266ded44932086fa59d595f742d2a};
     sign = 1;
     snrc = 0;
     tomb = 0;
     type = 42;
     unwp = 1;
     "v_Data" = {length = 2348, bytes = 0x30820928 02010002 82020100 ceaefca1 ... 13450b9d fd1d0bde };
     vrfy = 0;
     vwht = keychain;
     vyrc = 0;
     wrap = 0;
     }
     */

    NSData* itemdata = [[NSData alloc] initWithBase64EncodedString:@"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"

                                                           options:0];
    NSError* error = nil;
    NSMutableDictionary * item = [[NSPropertyListSerialization propertyListWithData:[CKKSItemEncrypter removePaddingFromData:itemdata]
                                                                            options:0
                                                                             format:nil
                                                                              error:&error] mutableCopy];
    XCTAssertNil(error, "no error interpreting data as item");
    XCTAssertNotNil(item, "interpreted data as item");

    if(zoneID && ![zoneID.zoneName isEqualToString:@"keychain"]) {
        [item setObject:zoneID.zoneName forKey:(__bridge id)kSecAttrSyncViewHint];
    }

    if(label) {
        item[(id)kSecAttrLabel] = label;
    }

    return item;
}


- (CKRecord*)createFakeTombstoneRecord:(CKRecordZoneID*)zoneID recordName:(NSString*)recordName account:(NSString*)account {
    NSMutableDictionary* item = [[self fakeRecordDictionary:account zoneID:zoneID] mutableCopy];
    item[@"tomb"] = @YES;

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:recordName zoneID:zoneID];
    return [self newRecord:ckrid withNewItemData:item];
}

- (CKRecord*)createFakeMultiuserRecord:(CKRecordZoneID*)zoneID musr:(NSUUID*)musruuid recordName:(NSString*)recordName account:(NSString*)account {
    NSMutableDictionary* item = [[self fakeRecordDictionary:account zoneID:zoneID] mutableCopy];

    uuid_t uuid;
    [musruuid getUUIDBytes:uuid];
    NSData* uuidData = [NSData dataWithBytes:uuid length:sizeof(uuid_t)];
    item[@"musr"] = uuidData;

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:recordName zoneID:zoneID];
    return [self newRecord:ckrid withNewItemData:item];
}

- (CKRecord*)createFakeRecord: (CKRecordZoneID*)zoneID recordName:(NSString*)recordName {
    return [self createFakeRecord: zoneID recordName:recordName withAccount: nil key:nil];
}

- (CKRecord*)createFakeRecord: (CKRecordZoneID*)zoneID recordName:(NSString*)recordName withAccount: (NSString*) account {
    return [self createFakeRecord: zoneID recordName:recordName withAccount:account key:nil];
}

- (CKRecord*)createFakeRecord: (CKRecordZoneID*)zoneID recordName:(NSString*)recordName withAccount: (NSString*) account key:(CKKSKey*)key {
    NSMutableDictionary* item = [[self fakeRecordDictionary: account zoneID:zoneID] mutableCopy];

    return [self createFakeRecord:zoneID recordName:recordName itemDictionary:item key:key plaintextPCSServiceIdentifier:nil plaintextPCSPublicKey:nil plaintextPCSPublicIdentity:nil];
}

- (CKRecord*)createFakeRecord:(CKRecordZoneID*)zoneID
                   recordName:(NSString*)recordName
                  withAccount:(NSString* _Nullable)account
                          key:(CKKSKey* _Nullable)key
plaintextPCSServiceIdentifier:(NSNumber*)pcsServiceIdentifier
        plaintextPCSPublicKey:(NSData*)pcsPublicKey
   plaintextPCSPublicIdentity:(NSData*)pcsPublicIdentity
{
    NSMutableDictionary* item = [[self fakeRecordDictionary: account password:nil zoneID:zoneID] mutableCopy];
    
    return [self createFakeRecord:zoneID recordName:recordName itemDictionary:item key:key plaintextPCSServiceIdentifier:pcsServiceIdentifier plaintextPCSPublicKey:pcsPublicKey plaintextPCSPublicIdentity:pcsPublicIdentity];
}

- (CKRecord*)createFakeRecord:(CKRecordZoneID*)zoneID
                   recordName:(NSString*)recordName
               itemDictionary:(NSDictionary*)itemDictionary
                          key:(CKKSKey* _Nullable)key
plaintextPCSServiceIdentifier:(NSNumber* _Nullable)pcsServiceIdentifier
        plaintextPCSPublicKey:(NSData* _Nullable)pcsPublicKey
   plaintextPCSPublicIdentity:(NSData* _Nullable)pcsPublicIdentity
{
    NSMutableDictionary* item = [itemDictionary mutableCopy];

    // class c items should be class c
    if([key.keyclass isEqualToString:SecCKKSKeyClassC]) {
        item[(__bridge NSString*)kSecAttrAccessible] = @"ck";
    }

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:recordName zoneID:zoneID];
    if (key) {
        return [self newRecord:ckrid withNewItemData:item key:key plaintextPCSServiceIdentifier:pcsServiceIdentifier plaintextPCSPublicKey:pcsPublicKey plaintextPCSPublicIdentity:pcsPublicIdentity];
    } else {
        ZoneKeys* zonekeys = self.keys[ckrid.zoneID];
        XCTAssertNotNil(zonekeys, "Have zone keys for zone");
        XCTAssertNotNil(zonekeys.classC, "Have class C key for zone");
        return [self newRecord:ckrid withNewItemData:item key:zonekeys.classC plaintextPCSServiceIdentifier:pcsServiceIdentifier plaintextPCSPublicKey:pcsPublicKey plaintextPCSPublicIdentity:pcsPublicIdentity];
    }
}

- (CKRecord*)newRecord: (CKRecordID*) recordID withNewItemData:(NSDictionary*) dictionary {
    ZoneKeys* zonekeys = self.keys[recordID.zoneID];
    XCTAssertNotNil(zonekeys, "Have zone keys for zone");
    XCTAssertNotNil(zonekeys.classC, "Have class C key for zone");

    return [self newRecord:recordID withNewItemData:dictionary key:zonekeys.classC plaintextPCSServiceIdentifier:nil plaintextPCSPublicKey:nil plaintextPCSPublicIdentity:nil];
}

- (CKKSItem*)newItem:(CKRecordID*)recordID withNewItemData:(NSDictionary*)dictionary key:(CKKSKey*)key 
plaintextPCSServiceIdentifier:(NSNumber* _Nullable)plaintextPCSServiceIdentifier plaintextPCSPublicKey:(NSData* _Nullable)plaintextPCSPublicKey plaintextPCSPublicIdentity:(NSData* _Nullable)plaintextPCSPublicIdentity
{
    NSError* error = nil;
    CKKSItem* cipheritem = [CKKSItemEncrypter encryptCKKSItem:[[CKKSItem alloc] initWithUUID:recordID.recordName
                                                                               parentKeyUUID:key.uuid
                                                                                   contextID:self.defaultCKKS.operationDependencies.contextID
                                                                                      zoneID:recordID.zoneID
                                                                             encodedCKRecord:nil encItem:nil wrappedkey:nil generationCount:0 encver:CKKSItemEncryptionVersion2 plaintextPCSServiceIdentifier:plaintextPCSServiceIdentifier plaintextPCSPublicKey:plaintextPCSPublicKey plaintextPCSPublicIdentity:plaintextPCSPublicIdentity]
                                               dataDictionary:dictionary
                                             updatingCKKSItem:nil
                                                    parentkey:key
                                                     keyCache:nil
                                                        error:&error];

    XCTAssertNil(error, "encrypted item with class c key");
    XCTAssertNotNil(cipheritem, "Have an encrypted item");

    return cipheritem;
}

- (CKRecord*)newRecord: (CKRecordID*) recordID withNewItemData:(NSDictionary*) dictionary key:(CKKSKey*)key
plaintextPCSServiceIdentifier:(NSNumber*)plaintextPCSServiceIdentifier plaintextPCSPublicKey:(NSData*)plaintextPCSPublicKey plaintextPCSPublicIdentity:(NSData*)plaintextPCSPublicIdentity
{

    CKKSItem* item = [self newItem:recordID withNewItemData:dictionary key:key
     plaintextPCSServiceIdentifier:plaintextPCSServiceIdentifier plaintextPCSPublicKey:plaintextPCSPublicKey plaintextPCSPublicIdentity:plaintextPCSPublicIdentity
    ];

    CKRecord* ckr = [item CKRecordWithZoneID: recordID.zoneID];
    XCTAssertNotNil(ckr, "Created a CKRecord");
    return ckr;
}

- (void)addItemToCloudKitZone:(NSDictionary*)itemDict recordName:(NSString*)recordName zoneID:(CKRecordZoneID*)zoneID
{
    FakeCKZone* zone = self.zones[zoneID];
    XCTAssertNotNil(zone, "Should have a zone for %@", zoneID);

    CKRecordID* ckrid = [[CKRecordID alloc] initWithRecordName:recordName zoneID:zoneID];
    CKRecord* record = [self newRecord:ckrid withNewItemData:itemDict];

    [zone addToZone:record];
}

- (NSDictionary*)decryptRecord: (CKRecord*) record {
    CKKSItem* item = [[CKKSItem alloc] initWithCKRecord:record contextID:self.defaultCKKS.operationDependencies.contextID];

    NSError* error = nil;

    NSDictionary* ret = [CKKSItemEncrypter decryptItemToDictionary: item keyCache:nil error:&error];
    XCTAssertNil(error);
    XCTAssertNotNil(ret);
    return ret;
}

- (void)addGenericPassword:(NSString*)password
                   account:(NSString*)account
                    access:(NSString*)access
                  viewHint:(NSString* _Nullable)viewHint
               accessGroup:(NSString* _Nullable)accessGroup
                 expecting:(OSStatus)status
                   message:(NSString*)message
{
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessible: (id)access,
                                    (id)kSecAttrAccount : account,
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    (id)kSecValueData : (id) [password dataUsingEncoding:NSUTF8StringEncoding],
                                    } mutableCopy];

    query[(id)kSecAttrAccessGroup] = accessGroup ?: @"com.apple.security.ckks";

    if(viewHint) {
        query[(id)kSecAttrSyncViewHint] = viewHint;
    } else {
        // Fake it as 'keychain'. This lets CKKSScanLocalItemsOperation for the test-only 'keychain' view find items which would normally not have a view hint.
        query[(id)kSecAttrSyncViewHint] = @"keychain";
    }

    XCTAssertEqual(status, SecItemAdd((__bridge CFDictionaryRef) query, NULL), @"%@", message);
}

- (void)addGenericPassword: (NSString*) password account: (NSString*) account viewHint: (NSString*) viewHint access: (NSString*) access expecting: (OSStatus) status message: (NSString*) message {
    [self addGenericPassword:password
                     account:account
                      access:access
                    viewHint:viewHint
                 accessGroup:nil
                   expecting:status
                     message:message];
}

- (void)addGenericPassword: (NSString*) password account: (NSString*) account expecting: (OSStatus) status message: (NSString*) message {
    [self addGenericPassword:password account:account viewHint:nil access:(id)kSecAttrAccessibleAfterFirstUnlock expecting:errSecSuccess message:message];

}

- (void)addGenericPassword: (NSString*) password account: (NSString*) account {
    [self addGenericPassword:password account:account viewHint:nil access:(id)kSecAttrAccessibleAfterFirstUnlock expecting:errSecSuccess message:@"Add item to keychain"];
}

- (void)addGenericPassword: (NSString*) password account: (NSString*) account viewHint:(NSString*)viewHint {
    [self addGenericPassword:password account:account viewHint:viewHint access:(id)kSecAttrAccessibleAfterFirstUnlock expecting:errSecSuccess message:@"Add item to keychain with a viewhint"];
}


- (void)addGenericPassword:(NSString*)password account:(NSString*)account accessGroup:(NSString*)accessGroup
{
    [self addGenericPassword:password
                     account:account
                      access:(id)kSecAttrAccessibleAfterFirstUnlock
                    viewHint:nil
                 accessGroup:accessGroup
                   expecting:errSecSuccess
                     message:@"Add item to keychain with an access group"];
}

- (void)addRandomPrivateKeyWithAccessGroup:(NSString *)accessGroup message:(NSString*)message {
    CFErrorRef cfError = NULL;
    id privateKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain : @YES,
        (id)kSecAttrSynchronizable : @YES,
        (id)kSecAttrKeyType : (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits : @256,
        (id)kSecPrivateKeyAttrs : @{
            (id)kSecAttrIsPermanent : @YES,
            (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
            (id)kSecAttrAccessGroup : accessGroup,
            (id)kSecAttrSyncViewHint : @"keychain",
        },
    }, &cfError));
    NSError *error = CFBridgingRelease(cfError);
    XCTAssertNotNil(privateKey, "%@", message);
    XCTAssertNil(error, "Should not return error adding random private key");
}

- (void)updateGenericPassword: (NSString*) newPassword account: (NSString*)account {
    NSDictionary* query = @{
                                    (id)kSecClass : (id)kSecClassGenericPassword,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrAccount : account,
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    };

    NSDictionary* update = @{
                                    (id)kSecValueData : (id) [newPassword dataUsingEncoding:NSUTF8StringEncoding],
                                    };
    XCTAssertEqual(errSecSuccess, SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef) update), @"Updating item %@ to %@", account, newPassword);
}

- (void)updateAccountOfGenericPassword:(NSString*)newAccount
                               account:(NSString*)account {
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            };

    NSDictionary* update = @{
                             (id)kSecAttrAccount : (id) newAccount,
                             };
    XCTAssertEqual(errSecSuccess, SecItemUpdate((__bridge CFDictionaryRef) query, (__bridge CFDictionaryRef) update), @"Updating item %@ to account %@", account, newAccount);
}

- (void)deleteGenericPassword: (NSString*) account {
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            };

    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef) query), @"Deleting item %@", account);
}

- (void)deleteGenericPasswordWithoutTombstones:(NSString*)account {
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecUseTombstones: @NO,
                            };

    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef) query), @"Deleting item %@", account);
}

- (void)findGenericPassword: (NSString*) account expecting: (OSStatus) status {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };
    XCTAssertEqual(status, SecItemCopyMatching((__bridge CFDictionaryRef) query, NULL), "Finding item %@", account);
}

- (NSData*)findGenericPasswordPersistentReference: (NSString*) account expecting: (OSStatus) status {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecReturnPersistentRef : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
    };
    CFTypeRef pref = NULL;
    XCTAssertEqual(status, SecItemCopyMatching((__bridge CFDictionaryRef) query, &pref), "Finding item %@", account);
    XCTAssertNotNil((__bridge id)pref, "persistent ref should not be nil");
    return CFBridgingRelease(pref);
}

- (void)checkGenericPassword: (NSString*) password account: (NSString*) account {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassGenericPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrAccount : account,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            (id)kSecReturnData : (id)kCFBooleanTrue,
                            };
    CFTypeRef result = NULL;

    XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &result), "Item %@ should exist", account);
    XCTAssertNotNil((__bridge id)result, "Should have received an item");

    NSString* storedPassword = [[NSString alloc] initWithData: (__bridge NSData*) result encoding: NSUTF8StringEncoding];
    XCTAssertNotNil(storedPassword, "Password should parse as a UTF8 password");

    XCTAssertEqualObjects(storedPassword, password, "Stored password should match received password");
}

- (void)checkGenericPasswordStoredUUID:(NSString*)uuid account:(NSString*)account {
    NSDictionary* queryAttributes = @{(id)kSecClass: (id)kSecClassGenericPassword,
                                      (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                      (id)kSecAttrAccount : account,
                                      (id)kSecAttrSynchronizable: @(YES),
                                      (id)kSecReturnAttributes : @(YES),
                                      (id)kSecReturnData : (id)kCFBooleanTrue,
    };

    __block CFErrorRef cferror = nil;
    Query *q = query_create_with_limit( (__bridge CFDictionaryRef)queryAttributes, NULL, kSecMatchUnlimited, NULL, &cferror);
    XCTAssertNil((__bridge id)cferror, "Should be no error creating query");
    CFReleaseNull(cferror);

    __block size_t count = 0;

    bool ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt) {
        return SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
            count += 1;

            NSString* itemUUID = (NSString*) CFBridgingRelease(CFRetain(SecDbItemGetValue(item, &v10itemuuid, &cferror)));
            XCTAssertEqualObjects(uuid, itemUUID, "Item uuid should match expectation");
        });
    });

    XCTAssertTrue(ok, "query should have been successful");
    XCTAssertNil((__bridge id)cferror, "Should be no error performing query");
    CFReleaseNull(cferror);

    XCTAssertEqual(count, 1, "Should have processed one item");
}

- (void)setGenericPasswordStoredUUID:(NSString*)uuid account:(NSString*)account {
    NSDictionary* queryAttributes = @{(id)kSecClass: (id)kSecClassGenericPassword,
                                      (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                      (id)kSecAttrAccount : account,
                                      (id)kSecAttrSynchronizable: @(YES),
                                      (id)kSecReturnAttributes : @(YES),
                                      (id)kSecReturnData : (id)kCFBooleanTrue,
    };

    __block CFErrorRef cferror = nil;
    Query *q = query_create_with_limit( (__bridge CFDictionaryRef)queryAttributes, NULL, kSecMatchUnlimited, NULL, &cferror);
    XCTAssertNil((__bridge id)cferror, "Should be no error creating query");
    CFReleaseNull(cferror);

    __block size_t count = 0;

    bool ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt) {
        return SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
            count += 1;

            NSDictionary* updates = @{(id) kSecAttrUUID: uuid};

            SecDbItemRef new_item = SecDbItemCopyWithUpdates(item, (__bridge CFDictionaryRef)updates, &cferror);
            XCTAssertTrue(new_item != NULL, "Should be able to create a new item");

            bool updateOk = kc_transaction_type(dbt, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, ^{
                return SecDbItemUpdate(item, new_item, dbt, kCFBooleanFalse, q->q_uuid_from_primary_key, &cferror);
            });
            XCTAssertTrue(updateOk, "Should be able to update item");

            return;
        });
    });

    XCTAssertTrue(ok, "query should have been successful");
    XCTAssertNil((__bridge id)cferror, "Should be no error performing query");
    CFReleaseNull(cferror);

    XCTAssertEqual(count, 1, "Should have processed one item");
}

- (void)addCertificateWithLabel:(NSString*)label serialNumber:(NSData*)serial
{
    CFErrorRef cferror = nil;

    SecKeyRef privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeRSA,
        (__bridge id)kSecAttrKeySizeInBits : @(4096),
    }, &cferror);

    XCTAssertEqual(cferror, NULL, "Should have no error creating a key");

    SecKeyRef publicKey = SecKeyCopyPublicKey(privateKey);
    NSArray *blankSubject = [NSArray array];
    NSDictionary *certParameters = @{
        (id)kSecCertificateLifetime : @(315576000),
    };

    // Note that there isn't a parameter for the serial number, so the generated cert won't match the metadata in the keychain. oh well.
    SecCertificateRef cert = SecGenerateSelfSignedCertificate((__bridge CFArrayRef)blankSubject,
                                                              (__bridge CFDictionaryRef)certParameters,
                                                              publicKey,
                                                              privateKey);

    // Add cert with blank subject to keychain
    NSDictionary *query = @{(id)kSecValueRef: (__bridge id)cert,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecReturnPersistentRef: @YES,
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
                            (id)kSecAttrSynchronizable: @YES,
                            (id)kSecAttrSyncViewHint: @"keychain",
                            (id)kSecAttrSerialNumber: serial,
    };

    CFDataRef persistentRef = nil;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, (CFTypeRef *)&persistentRef);
    XCTAssertEqual(status, errSecSuccess, "Should be able to add an identity to the keychain");
    XCTAssertNotEqual(persistentRef, NULL, "Should have gotten something back from SecItemAdd");

    CFReleaseNull(persistentRef);
    CFReleaseNull(privateKey);
    CFReleaseNull(publicKey);
    CFReleaseNull(cert);
}

- (void)findCertificateWithSerialNumber:(NSData*)serial expecting:(OSStatus)status
{
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassCertificate,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrSerialNumber : serial,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecMatchLimit : (id)kSecMatchLimitOne,
                            };
    XCTAssertEqual(status, SecItemCopyMatching((__bridge CFDictionaryRef) query, NULL), "Finding certificate %@", serial);
}

- (void)deleteCertificateWithSerialNumber:(NSData*)serial
{
    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassCertificate,
                            (id)kSecAttrSerialNumber: serial,
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            };

    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef) query), @"Deleting certificate %@", serial);
}

- (void)addKeyWithLabel:(NSString*)label
{
    CFErrorRef cferror = nil;

    SecKeyRef privateKey = SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (__bridge id)kSecAttrKeyType : (__bridge id)kSecAttrKeyTypeRSA,
        (__bridge id)kSecAttrKeySizeInBits : @(4096),
    }, &cferror);

    XCTAssertEqual(cferror, NULL, "Should have no error creating a key");
    XCTAssertNotNil((__bridge id)privateKey, "Should have a private key");

    NSDictionary *query = @{
        (__bridge id)kSecValueRef: (__bridge id)privateKey,
        (__bridge id)kSecAttrLabel : label,
        (__bridge id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (__bridge id)kSecReturnPersistentRef: @YES,
        (__bridge id)kSecAttrAccessible: (id)kSecAttrAccessibleAfterFirstUnlock,
        (__bridge id)kSecAttrSynchronizable: @YES,
        (__bridge id)kSecAttrSyncViewHint: @"keychain",
    };

    CFDataRef persistentRef = nil;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, (CFTypeRef *)&persistentRef);
    XCTAssertEqual(status, errSecSuccess, "Should be able to add a key to the keychain");
    XCTAssertNotEqual(persistentRef, NULL, "Should have gotten something back from SecItemAdd");

    CFReleaseNull(persistentRef);
    CFReleaseNull(privateKey);
}

- (void)findKeyWithLabel:(NSString*)label expecting:(OSStatus)status
{
    NSDictionary *query = @{
        (__bridge id)kSecClass : (id)kSecClassKey,
        (__bridge id)kSecAttrAccessGroup : @"com.apple.security.ckks",
        (__bridge id)kSecAttrLabel : label,
        (__bridge id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
        (__bridge id)kSecMatchLimit : (id)kSecMatchLimitOne,
        (__bridge id)kSecReturnAttributes : @(YES),
    };

    CFTypeRef ret = NULL;
    XCTAssertEqual(status, SecItemCopyMatching((__bridge CFDictionaryRef)query, &ret), "Finding key %@", label);
    CFReleaseNull(ret);
}

- (void)deleteKeyithLabel:(NSString*)label
{
    NSDictionary* query = @{
        (id)kSecClass : (id)kSecClassKey,
        (__bridge id)kSecAttrLabel : label,
        (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
    };

    XCTAssertEqual(errSecSuccess, SecItemDelete((__bridge CFDictionaryRef) query), @"Deleting key %@", label);
}

-(XCTestExpectation*)expectChangeForView:(NSString*)view {
    NSString* notification = [NSString stringWithFormat: @"com.apple.security.view-change.%@", view];
    return [self expectationForNotification:notification object:nil handler:^BOOL(NSNotification * _Nonnull nsnotification) {
        ckksnotice_global("ckks", "Got a notification for %@: %@", notification, nsnotification);
        return YES;
    }];
}

- (XCTestExpectation*)expectLibNotifyReadyForView:(NSString*)viewName
{
    NSString* notification = [NSString stringWithFormat: @"com.apple.security.view-ready.%@", viewName];
    return [self expectationForNotification:notification object:nil handler:^BOOL(NSNotification * _Nonnull nsnotification) {
        ckksnotice_global("ckks", "Got a 'ready' notification for %@: %@", notification, nsnotification);
        return YES;
    }];
}

- (XCTestExpectation*)expectNSNotificationReadyForView:(NSString*)viewName
{
    return [self expectationForNotification:@"com.apple.security.view-become-ready"
                                     object:nil
                                    handler:^BOOL(NSNotification * _Nonnull notification) {
        NSString* notifiedView = notification.userInfo[@"view"];
        if(notifiedView == nil) {
            return NO;
        }

        return [viewName isEqualToString:notifiedView];
    }];
}

- (void)expectCKKSTLKSelfShareUpload:(CKRecordZoneID*)zoneID {
    [self expectCKModifyKeyRecords:0 currentKeyPointerRecords:0 tlkShareRecords:1 zoneID:zoneID];
}

- (void)checkNSyncableTLKsInKeychain:(size_t)n {
    NSDictionary *query = @{(id)kSecClass : (id)kSecClassInternetPassword,
                            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                            (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                            (id)kSecAttrDescription: SecCKKSKeyClassTLK,
                            (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                            (id)kSecReturnAttributes : (id)kCFBooleanTrue,
                            };
    CFTypeRef result = NULL;

    if(n == 0) {
        XCTAssertEqual(errSecItemNotFound, SecItemCopyMatching((__bridge CFDictionaryRef) query, &result), "Should have found no TLKs");
    } else {
        XCTAssertEqual(errSecSuccess, SecItemCopyMatching((__bridge CFDictionaryRef) query, &result), "Should have found TLKs");
        NSArray* items = (NSArray*) CFBridgingRelease(result);

        XCTAssertEqual(items.count, n, "Should have received %lu items", (unsigned long)n);
    }
}

@end

#endif // OCTAGON
