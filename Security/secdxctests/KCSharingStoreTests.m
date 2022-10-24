

#import <Security/Security.h>
#import <Security/SecEntitlements.h>
#import <Security/SecItemPriv.h>
#import <Security/SecKeyPriv.h>
#import <XCTest/XCTest.h>

#import "SecAKSObjCWrappers.h"

#include "ipc/server_security_helpers.h"
#include "keychain/securityd/SecItemDb.h"
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"

#import "KCSharingStore.h"
#import "KeychainXCTest.h"

#if KCSHARING

@interface KCSharingStoreTests : KeychainXCTest

@end

@implementation KCSharingStoreTests

- (void)setUp {
    [super setUp];
    SecAddLocalSecuritydXPCFakeEntitlement(kSecEntitlementKeychainAccessGroups, @YES);
}

- (void)tearDown {
    SecResetLocalSecuritydXPCFakeEntitlements();
    [super tearDown];
}

- (void)testMetadata {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    NSError *fetchMissingError;
    XCTAssertNil([store fetchValueForMetadataKey:@"some-key" error:&fetchMissingError], "Should not return value for missing metadata key");
    XCTAssertNotNil(fetchMissingError, "Should return error for missing metadata key");

    NSError *setError;
    XCTAssertTrue([store setValue:@123 forMetadataKey:@"some-key" error:&setError], "Should set value for metadata key");
    XCTAssertNil(setError, "Should not return error when setting value for metadata key");

    NSError *fetchPresentError;
    id newMetadata = [store fetchValueForMetadataKey:@"some-key" error:&fetchPresentError];
    XCTAssertEqualObjects(newMetadata, @123, "Should return value for metadata key");
    XCTAssertNil(fetchPresentError, "Should not return error for present metadata key");
}

- (void)testIncomingAndOutgoingNoConflicts {
    // This test simulates a sync, with one new incoming item and one new
    // outgoing item.

    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    // Make a new local passkey, and share it with a group.
    id newLocalPrivateKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
            (id)kSecAttrLabel: @"example.com",
            (id)kSecAttrApplicationTag: [@"com.example.some-unique-tag" dataUsingEncoding:NSUTF8StringEncoding],
        },
    }, NULL));
    XCTAssertNotNil(newLocalPrivateKey, "Should generate new local passkey");
    if (!newLocalPrivateKey) {
        return;
    }
    id sharedLocalPrivateKey = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",

        (id)kSecReturnAttributes: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedLocalPrivateKey, "Should share local passkey");
    if (!sharedLocalPrivateKey) {
        return;
    }

    // Make a new remote passkey, shared with the same group.
    KCSharingPrivateKeyCredential *newRemotePrivateKey = [[KCSharingPrivateKeyCredential alloc] initWithAttributes:@{
        (id)kSecAttrUUID: [NSUUID UUID].UUIDString,
        (id)kSecAttrGroupKitGroup: @"some-example-group",
        (id)kSecAttrAccessGroup: store.entryAccessGroup,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrApplicationTag: [@"some-tag" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrLabel: @"some-rp-id",
        (id)kSecAttrApplicationLabel: [@"pubkey-hash" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData: [@"privkey" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecAttrEffectiveKeySize: @256,
        (id)kSecAttrCreationDate: [NSDate date],
        (id)kSecAttrModificationDate: [NSDate date],
    } error:nil];
    XCTAssertNotNil(newRemotePrivateKey, @"Should construct new remote private key");
    if (!newRemotePrivateKey) {
        return;
    }
    KCSharingRemoteItem *newRemotePasskey = [[KCSharingRemoteItem alloc] initPasskeyWithPrivateKey:newRemotePrivateKey sidecar:nil error:nil];
    XCTAssertNotNil(newRemotePasskey, @"Should construct new remote passkey");
    if (!newRemotePasskey) {
        return;
    }
    CKRecord *newRemoteRecord = [[CKRecord alloc] initWithRecordType:KCSharingRecordTypeItem recordID:KCSharingNewRecordID(@"some-example-group")];
    newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyType] = @(KCSharingEntryTypePasskey);
    newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyPayload] = [newRemotePasskey proto].data;

    // Stage the local passkey for upload.
    XCTestExpectation *stageOutgoingExpectation = [self expectationWithDescription:@"Wait to stage new outgoing passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging outgoing changes should not return an error");
        [stageOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageOutgoingExpectation] timeout:2.5];

    NSError *fetchNewZoneIDsError;
    NSArray<CKRecordZoneID *> *newZoneIDs = [store fetchNewZoneIDsForOutgoingChangesWithOwnerName:CKCurrentUserDefaultName error:&fetchNewZoneIDsError];
    XCTAssertNotNil(newZoneIDs, "Should return new zone IDs to create");
    XCTAssertNil(fetchNewZoneIDsError, "Should not return error fetching new zone IDs to create");
    XCTAssertEqualObjects(newZoneIDs, @[[[CKRecordZoneID alloc] initWithZoneName:@"some-example-group" ownerName:CKCurrentUserDefaultName]], "Should return zone ID for new group");

    // Queue the incoming passkey.
    NSError *stageError;
    XCTAssertTrue([store stageIncomingRecord:newRemoteRecord error:&stageError], @"Should stage incoming queue entry");
    XCTAssertNil(stageError, @"Staging incoming queue entry should not return an error");

    NSArray<KCSharingOutgoingEntry *> *outgoingEntries = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 1, "Incoming queue should contain new remote passkey");
    XCTAssertEqual(outgoingEntries.count, 1, "Outgoing queue should contain new local passkey");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 0, "Mirror should be empty");

    NSError *mergeError;
    XCTAssertTrue([store mergeWithError:&mergeError], @"Should merge incoming and outgoing changes");
    XCTAssertNil(mergeError, @"Merging should not return an error");

    // Verify that `-fetchOutgoingChangesWithCursor:` returns the item that we
    // expect to upload, when we request changes for all zones...
    NSError *fetchOutgoingForAllZonesError;
    KCSharingOutgoingChangeset *outgoingChangesForAllZones = [store fetchOutgoingChangesWithCursor:[[KCSharingOutgoingChangesetCursor alloc] initWithDesiredZoneIDs:nil] maxChangeCount:-1 maxBatchSizeInBytes:-1 error:&fetchOutgoingForAllZonesError];
    XCTAssertEqual(outgoingChangesForAllZones.recordsToSave.count, 1);
    XCTAssertEqual(outgoingChangesForAllZones.recordIDsToDelete.count, 0);

    // ...And when we request changes for just that zone.
    NSError *fetchOutgoingForDesiredZonesError;
    KCSharingOutgoingChangeset *outgoingChangesForDesiredZones = [store fetchOutgoingChangesWithCursor:[[KCSharingOutgoingChangesetCursor alloc] initWithDesiredZoneIDs:[NSSet setWithArray:@[[[CKRecordZoneID alloc] initWithZoneName:@"some-example-group" ownerName:CKCurrentUserDefaultName]]]] maxChangeCount:-1 maxBatchSizeInBytes:-1 error:&fetchOutgoingForDesiredZonesError];
    XCTAssertEqual(outgoingChangesForDesiredZones.recordsToSave.count, 1);
    XCTAssertEqual(outgoingChangesForDesiredZones.recordIDsToDelete.count, 0);
    CKRecord *outgoingRecordInBatch = outgoingChangesForDesiredZones.recordsToSave.firstObject;
    XCTAssertNotNil(outgoingRecordInBatch);

    // ...And verify that we can also fetch one record.
    NSError *fetchOutgoingRecordError;
    CKRecord *outgoingRecord = [store fetchOutgoingRecordWithRecordID:outgoingRecordInBatch.recordID error:&fetchOutgoingRecordError];
    XCTAssertNil(fetchOutgoingRecordError);
    XCTAssertNotNil(outgoingRecord);

    // And write the outgoing item back to the mirror, to simulate a successful
    // upload.
    NSError *moveToMirrorError;
    XCTAssertTrue([store updateMirrorWithSavedRecords:@[outgoingRecord] deletedRecordIDs:nil error:&moveToMirrorError], "Should move outgoing item back to mirror");
    XCTAssertNil(moveToMirrorError, "Moving outgoing item back to mirror should not return an error");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 2, "Mirror should contain both items");
}

- (void)testRemoteChangeToMirrorItem {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    id newPasskey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
            (id)kSecAttrLabel: @"example.com",
            (id)kSecAttrApplicationTag: [@"com.example.some-unique-tag" dataUsingEncoding:NSUTF8StringEncoding],
        },
    }, NULL));
    XCTAssertNotNil(newPasskey, "Should generate new passkey");
    if (!newPasskey) {
        return;
    }

    NSDictionary *sharedPasskey = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",

        (id)kSecReturnAttributes: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedPasskey, "Should return new shared passkey");
    if (!sharedPasskey) {
        return;
    }

    // Write the new passkey to the mirror, to simulate an upload.
    XCTestExpectation *stageOutgoingExpectation = [self expectationWithDescription:@"Wait to stage new outgoing passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging outgoing changes should not return an error");
        [stageOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageOutgoingExpectation] timeout:2.5];
    NSArray<KCSharingOutgoingEntry *> *outgoingEntries = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual(outgoingEntries.count, 1, "Should stage outgoing item for upload");
    XCTAssertTrue([store updateMirrorWithSavedRecords:@[outgoingEntries.firstObject.record] deletedRecordIDs:nil error:nil], "Should write outgoing changed item to mirror");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain moved item");

    // Now update the shared passkey's label on the server.
    KCSharingOutgoingEntry *outgoingEntry = outgoingEntries.firstObject;
    KCSharingEntryRemoteItemResult *outgoingRemoteItemResult = [outgoingEntry remoteItemWithError:nil];
    XCTAssertNotNil(outgoingRemoteItemResult, "Should inflate remote item for outgoing entry");
    NSMutableDictionary *newAttributes = [[outgoingRemoteItemResult.remoteItem.privateKey attributesWithError:nil] mutableCopy];
    newAttributes[(id)kSecAttrLabel] = @"example.net";
    KCSharingPrivateKeyCredential *newRemotePrivateKey = [[KCSharingPrivateKeyCredential alloc] initWithAttributes:newAttributes error:nil];
    XCTAssertNotNil(newRemotePrivateKey, "Should create remote private key with new attributes");
    KCSharingRemoteItem *newRemotePasskey = [[KCSharingRemoteItem alloc] initPasskeyWithPrivateKey:newRemotePrivateKey sidecar:outgoingRemoteItemResult.remoteItem.sidecar error:nil];
    XCTAssertNotNil(newRemotePasskey, "Should create remote passkey with new private key");

    CKRecord *newRemoteRecord = outgoingEntry.record;
    newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyPayload] = [newRemotePasskey proto].data;

    XCTAssertTrue([store stageIncomingRecord:newRemoteRecord error:nil], "Should stage incoming queue entry for remotely changed item");

    NSError *mergeError;
    XCTAssertTrue([store mergeWithError:&mergeError], @"Should apply remote change to local keychain");
    XCTAssertNil(mergeError, @"Applying remote change should not return an error");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain updated item");

    CFTypeRef rawResult = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    }, &rawResult);
    XCTAssertEqual(status, errSecSuccess, @"Should find updated item in local keychain");

    NSDictionary *result = CFBridgingRelease(rawResult);
    XCTAssertEqualObjects(result[(id)kSecAttrLabel], @"example.net", "Updated local item should have new label");
}

- (void)testLocalChangeToMirrorItem {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    id newPasskey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
            (id)kSecAttrLabel: @"example.com",
            (id)kSecAttrApplicationTag: [@"com.example.some-unique-tag" dataUsingEncoding:NSUTF8StringEncoding],
        },
    }, NULL));
    XCTAssertNotNil(newPasskey, "Should generate new passkey");
    if (!newPasskey) {
        return;
    }

    NSDictionary *sharedPasskey = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",

        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedPasskey, "Should return new shared passkey");
    if (!sharedPasskey) {
        return;
    }

    XCTestExpectation *stageNewOutgoingExpectation = [self expectationWithDescription:@"Wait to stage new outgoing passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging outgoing changes should not return an error");
        [stageNewOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageNewOutgoingExpectation] timeout:2.5];
    NSArray<KCSharingOutgoingEntry *> *outgoingEntries = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual(outgoingEntries.count, 1, "Should stage outgoing item for upload");
    XCTAssertTrue([store updateMirrorWithSavedRecords:@[outgoingEntries.firstObject.record] deletedRecordIDs:nil error:nil], "Should write outgoing changed item to mirror");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain moved item");

    // Now update the shared passkey locally.
    OSStatus updateStatus = SecItemUpdate((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",
    }, (__bridge CFDictionaryRef)@{
        (id)kSecAttrLabel: @"apple.com",
    });
    XCTAssertEqual(updateStatus, errSecSuccess, "Should update shared passkey");

    // Stage the local passkey for upload.
    XCTestExpectation *stageUpdatedOutgoingExpectation = [self expectationWithDescription:@"Wait to stage updated shared passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging updated shared passkey should not return an error");
        [stageUpdatedOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageUpdatedOutgoingExpectation] timeout:2.5];

    CFTypeRef rawUpdatedLocalItem = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    }, &rawUpdatedLocalItem);
    XCTAssertEqual(status, errSecSuccess, @"Should find updated item in local keychain");
    NSDictionary *updatedLocalItem = CFBridgingRelease(rawUpdatedLocalItem);

    NSArray<KCSharingOutgoingEntry *> *outgoingEntriesAfterUpdate = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual(outgoingEntriesAfterUpdate.count, 1, "Outgoing queue should contain updated passkey");
    XCTAssertEqualObjects(outgoingEntriesAfterUpdate.firstObject.record.recordID, outgoingEntries.firstObject.record.recordID);
    KCSharingEntryRemoteItemResult *outgoingRemoteItemResult = [outgoingEntriesAfterUpdate.firstObject remoteItemWithError:nil];
    XCTAssertNotNil(outgoingRemoteItemResult);
    XCTAssertEqual(outgoingRemoteItemResult.remoteItem.type, KCSharingItemTypePasskey);

    NSDictionary *actualOutgoingPrivateKeyAttributes = [outgoingRemoteItemResult.remoteItem.privateKey attributesWithError:nil];
    NSDictionary *expectedOutgoingPrivateKeyAttributes = @{
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecAttrLabel: @"apple.com",

        (id)kSecAttrGroupKitGroup: updatedLocalItem[(id)kSecAttrGroupKitGroup],
        (id)kSecAttrKeyType: updatedLocalItem[(id)kSecAttrKeyType],
        (id)kSecAttrApplicationTag: updatedLocalItem[(id)kSecAttrApplicationTag],
        (id)kSecAttrApplicationLabel: updatedLocalItem[(id)kSecAttrApplicationLabel],
        (id)kSecValueData: updatedLocalItem[(id)kSecValueData],
        (id)kSecAttrKeySizeInBits: updatedLocalItem[(id)kSecAttrKeySizeInBits],
        (id)kSecAttrEffectiveKeySize: updatedLocalItem[(id)kSecAttrEffectiveKeySize],
        (id)kSecAttrCreationDate: updatedLocalItem[(id)kSecAttrCreationDate],
        (id)kSecAttrModificationDate: updatedLocalItem[(id)kSecAttrModificationDate],
        (id)kSecAttrSynchronizable: updatedLocalItem[(id)kSecAttrSynchronizable],
        (id)kSecAttrAccessGroup: updatedLocalItem[(id)kSecAttrAccessGroup],
        (id)kSecAttrKeyClass: [(NSNumber *)updatedLocalItem[(id)kSecAttrKeyClass] stringValue],
        (id)kSecAttrIsPermanent: updatedLocalItem[(id)kSecAttrIsPermanent],
        (id)kSecAttrIsPrivate: updatedLocalItem[(id)kSecAttrIsPrivate],
        (id)kSecAttrIsModifiable: updatedLocalItem[(id)kSecAttrIsModifiable],
        (id)kSecAttrIsSensitive: updatedLocalItem[(id)kSecAttrIsSensitive],
        (id)kSecAttrWasAlwaysSensitive: updatedLocalItem[(id)kSecAttrWasAlwaysSensitive],
        (id)kSecAttrIsExtractable: updatedLocalItem[(id)kSecAttrIsExtractable],
        (id)kSecAttrWasNeverExtractable: updatedLocalItem[(id)kSecAttrWasNeverExtractable],
        (id)kSecAttrCanEncrypt: updatedLocalItem[(id)kSecAttrCanEncrypt],
        (id)kSecAttrCanDecrypt: updatedLocalItem[(id)kSecAttrCanDecrypt],
        (id)kSecAttrCanDerive: updatedLocalItem[(id)kSecAttrCanDerive],
        (id)kSecAttrCanSign: updatedLocalItem[(id)kSecAttrCanSign],
        (id)kSecAttrCanVerify: updatedLocalItem[(id)kSecAttrCanVerify],
        (id)kSecAttrCanSignRecover: updatedLocalItem[(id)kSecAttrCanSignRecover],
        (id)kSecAttrCanVerifyRecover: updatedLocalItem[(id)kSecAttrCanVerifyRecover],
        (id)kSecAttrCanWrap: updatedLocalItem[(id)kSecAttrCanWrap],
        (id)kSecAttrCanUnwrap: updatedLocalItem[(id)kSecAttrCanUnwrap],
    };
    XCTAssertEqualObjects(actualOutgoingPrivateKeyAttributes, expectedOutgoingPrivateKeyAttributes);
}

- (void)testIncomingOnlyChange {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];
    
    KCSharingPrivateKeyCredential *incomingPrivateKey = [[KCSharingPrivateKeyCredential alloc] initWithAttributes:@{
        (id)kSecAttrGroupKitGroup: @"shared-group",
        (id)kSecAttrAccessGroup: store.entryAccessGroup,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrApplicationTag: [@"some-tag" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrLabel: @"some-rp-id",
        (id)kSecAttrApplicationLabel: [@"pubkey-hash" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecValueData: [@"privkey" dataUsingEncoding:NSUTF8StringEncoding],
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecAttrEffectiveKeySize: @256,
        (id)kSecAttrCreationDate: [NSDate date],
        (id)kSecAttrModificationDate: [NSDate date],
    } error:nil];
    XCTAssertNotNil(incomingPrivateKey, @"Should construct incoming private key");
    if (!incomingPrivateKey) {
        return;
    }
    KCSharingRemoteItem *incomingPasskey = [[KCSharingRemoteItem alloc] initPasskeyWithPrivateKey:incomingPrivateKey sidecar:nil error:nil];
    XCTAssertNotNil(incomingPasskey, @"Should construct incoming passkey");
    if (!incomingPasskey) {
        return;
    }

    CKRecord *newRemoteRecord = [[CKRecord alloc] initWithRecordType:KCSharingRecordTypeItem recordID:KCSharingNewRecordID(@"shared-group")];
    newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyType] = @(KCSharingEntryTypePasskey);
    newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyPayload] = [incomingPasskey proto].data;

    NSError *stageError;
    XCTAssertTrue([store stageIncomingRecord:newRemoteRecord error:&stageError], @"Should stage incoming queue entry");
    XCTAssertNil(stageError, @"Staging incoming queue entry should not return an error");

    NSError *mergeError;
    XCTAssertTrue([store mergeWithError:&mergeError], @"Should move incoming queue entry to local and mirror");
    XCTAssertNil(mergeError, @"Merging should not return an error");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain new item");

    CFTypeRef rawResult = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    }, &rawResult);
    XCTAssertEqual(status, errSecSuccess, @"Should find new item in local keychain");

    NSDictionary *result = CFBridgingRelease(rawResult);
    XCTAssertEqualObjects(result[(id)kSecAttrApplicationTag], incomingPrivateKey.applicationTag, "Application tag should match");
    XCTAssertEqualObjects(result[(id)kSecValueData], incomingPrivateKey.keyMaterial, "Key material should match");
}

- (void)testRemoteDeletionForExistingLocalItem {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    id newPasskey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
            (id)kSecAttrLabel: @"example.com",
            (id)kSecAttrApplicationTag: [@"com.example.some-unique-tag" dataUsingEncoding:NSUTF8StringEncoding],
        },
    }, NULL));
    XCTAssertNotNil(newPasskey, "Should generate new passkey");
    if (!newPasskey) {
        return;
    }

    NSDictionary *sharedPasskey = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",

        (id)kSecReturnAttributes: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedPasskey, "Should return new shared passkey");
    if (!sharedPasskey) {
        return;
    }

    // Write the new passkey to the mirror, to simulate an upload.
    XCTestExpectation *stageNewOutgoingExpectation = [self expectationWithDescription:@"Wait to stage new outgoing passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging outgoing changes should not return an error");
        [stageNewOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageNewOutgoingExpectation] timeout:2.5];
    NSArray<KCSharingOutgoingEntry *> *outgoingEntries = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual(outgoingEntries.count, 1, "Should stage outgoing item for upload");
    XCTAssertTrue([store updateMirrorWithSavedRecords:@[outgoingEntries.firstObject.record] deletedRecordIDs:nil error:nil], "Should write outgoing changed item to mirror");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain moved item");

    // Now delete the shared passkey on the server.
    KCSharingOutgoingEntry *outgoingEntry = outgoingEntries.firstObject;
    XCTAssertTrue([store stageIncomingDeletionForRecordID:outgoingEntry.record.recordID error:nil], "Should stage incoming deletion for item");

    NSError *mergeError;
    XCTAssertTrue([store mergeWithError:&mergeError], @"Should apply remote change to local keychain");
    XCTAssertNil(mergeError, @"Applying remote change should not return an error");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 0, "Mirror should be empty");

    CFTypeRef rawResult = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
    }, &rawResult);
    XCTAssertEqual(status, errSecItemNotFound, @"Should delete item from local Keychain");
}

- (void)testLocalChangeConflictWithRemoteDeletion {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    id newPasskey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
            (id)kSecAttrLabel: @"example.com",
            (id)kSecAttrApplicationTag: [@"com.example.some-unique-tag" dataUsingEncoding:NSUTF8StringEncoding],
        },
    }, NULL));
    XCTAssertNotNil(newPasskey, "Should generate new passkey");
    if (!newPasskey) {
        return;
    }

    NSDictionary *sharedPasskey = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",

        (id)kSecReturnAttributes: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedPasskey, "Should return new shared passkey");
    if (!sharedPasskey) {
        return;
    }

    // Write the new passkey to the mirror, to simulate an upload.
    XCTestExpectation *stageNewOutgoingExpectation = [self expectationWithDescription:@"Wait to stage new outgoing passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging outgoing changes should not return an error");
        [stageNewOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageNewOutgoingExpectation] timeout:2.5];
    NSArray<KCSharingOutgoingEntry *> *outgoingEntries = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual(outgoingEntries.count, 1, "Should stage outgoing item for upload");
    XCTAssertTrue([store updateMirrorWithSavedRecords:@[outgoingEntries.firstObject.record] deletedRecordIDs:nil error:nil], "Should write outgoing changed item to mirror");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain moved item");

    // Now update the shared passkey's label on the server...
    {
        KCSharingOutgoingEntry *outgoingEntry = outgoingEntries.firstObject;
        KCSharingEntryRemoteItemResult *outgoingRemoteItemResult = [outgoingEntry remoteItemWithError:nil];
        XCTAssertNotNil(outgoingRemoteItemResult, "Should inflate remote item for outgoing entry");
        NSMutableDictionary *newAttributes = [[outgoingRemoteItemResult.remoteItem.privateKey attributesWithError:nil] mutableCopy];
        newAttributes[(id)kSecAttrLabel] = @"example.net";
        KCSharingPrivateKeyCredential *newRemotePrivateKey = [[KCSharingPrivateKeyCredential alloc] initWithAttributes:newAttributes error:nil];
        XCTAssertNotNil(newRemotePrivateKey, "Should create remote private key with new attributes");
        KCSharingRemoteItem *newRemotePasskey = [[KCSharingRemoteItem alloc] initPasskeyWithPrivateKey:newRemotePrivateKey sidecar:outgoingRemoteItemResult.remoteItem.sidecar error:nil];
        XCTAssertNotNil(newRemotePasskey, "Should create remote passkey with new private key");
        CKRecord *newRemoteRecord = outgoingEntry.record;
        newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyPayload] = [newRemotePasskey proto].data;
        XCTAssertTrue([store stageIncomingRecord:newRemoteRecord error:nil], "Should stage incoming queue entry for remotely changed item");
    }

    // And delete the shared passkey locally.
    NSDictionary *locallyDeletedQuery = @{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrGroupKitGroup: @"some-example-group",
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecReturnAttributes: @YES,
        (id)kSecMatchLimit: (id)kSecMatchLimitAll,
    };
    {
        OSStatus deleteStatus = SecItemDelete((__bridge CFDictionaryRef)@{
            (id)kSecUseDataProtectionKeychain: @YES,
            (id)kSecAttrSynchronizable: @NO,
            (id)kSecClass: (id)kSecClassKey,
            (id)kSecAttrGroupKitGroup: @"some-example-group",
        });
        XCTAssertEqual(deleteStatus, errSecSuccess, "Should delete shared passkey");

        OSStatus copyMatchingStatus = SecItemCopyMatching((__bridge CFDictionaryRef)locallyDeletedQuery, NULL);
        XCTAssertEqual(copyMatchingStatus, errSecItemNotFound, @"Should delete item locally");

        // Stage the local passkey for upload.
        XCTestExpectation *stageOutgoingDeletionExpectation = [self expectationWithDescription:@"Wait to stage new outgoing deletion"];
        [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
            XCTAssertNil(error, "Staging outgoing deletion should not return an error");
            [stageOutgoingDeletionExpectation fulfill];
        }];
        [self waitForExpectations:@[stageOutgoingDeletionExpectation] timeout:2.5];
    }

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 1, "Incoming queue should contain remote change");
    // TODO: Assert that the outgoing queue contains the local deletion.

    NSError *mergeError;
    XCTAssertTrue([store mergeWithError:&mergeError], @"Should apply conflicting remote changes to local keychain");
    XCTAssertNil(mergeError, @"Applying conflicting remote changes should not return an error");

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain resurrected item");

    CFTypeRef rawResult = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)locallyDeletedQuery, &rawResult);
    XCTAssertEqual(status, errSecSuccess, @"Should delete item from local Keychain");
    NSArray<NSDictionary *> *result = CFBridgingRelease(rawResult);
    XCTAssertEqual(result.count, 1);
    XCTAssertEqualObjects(result.firstObject[(id)kSecAttrLabel], @"example.net", "Resurrected local item should have new label");
}

- (void)testPasskeyWithSidecar {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    id newPrivateKey = CFBridgingRelease(SecKeyCreateRandomKey((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrKeyType: (id)kSecAttrKeyTypeECSECPrimeRandom,
        (id)kSecAttrKeySizeInBits: @256,
        (id)kSecPrivateKeyAttrs: @{
            (id)kSecAttrIsPermanent: @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
            (id)kSecAttrLabel: @"subdomain.example.com",
            (id)kSecAttrApplicationTag: [@"com.example.some-unique-tag" dataUsingEncoding:NSUTF8StringEncoding],
        },
    }, NULL));
    XCTAssertNotNil(newPrivateKey, "Should generate new passkey");
    if (!newPrivateKey) {
        return;
    }
    NSData *applicationLabel = CFBridgingRelease(SecKeyCopyPublicKeyHash((__bridge SecKeyRef)newPrivateKey));
    NSDictionary *sidecarContents = @{
        @"notes": @"This password isn't 'beef stew', so it's stroganoff!",
    };
    CFTypeRef rawNewSidecar = NULL;
    (void)SecItemAdd((__bridge CFDictionaryRef)@{
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
        (id)kSecReturnAttributes: @YES,

        // These are the same attributes that
        // `-[NSURLCredentialStorage safari_setSynchronizableSidecar:credential:forHTMLFormProtectionSpace:]`
        // (in Safari's `WBSNSURLCredentialStorageExtras.m`) sets for sidecars.
        (id)kSecAttrAccount: [applicationLabel base64EncodedStringWithOptions:0],
        (id)kSecValueData: [NSPropertyListSerialization dataWithPropertyList:sidecarContents format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil],
        (id)kSecAttrLabel: @"Password Manager Metadata: example.com (...)",
        (id)kSecAttrDescription: @"Password Manager Metadata",
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: store.sidecarAccessGroup,

        // These attributes are set by `queryForPasswordManagerSidecarInProtectionSpace`,
        // also in `WBSNSURLCredentialStorageExtras.m`.
        (id)kSecClass: (id)kSecClassInternetPassword,
        (id)kSecAttrAuthenticationType: (id)kSecAttrAuthenticationTypeHTMLForm,
        (id)kSecAttrProtocol: (id)kSecAttrProtocolHTTP,
        // The private key has a subdomain in its relying party ID (label), but
        // this sidecar is for the high-level domain. Since we don't match on
        // the server for passkey sidecars, we should still use it.
        (id)kSecAttrServer: @"example.com",
        (id)kSecAttrPort: @443,
    }, &rawNewSidecar);
    NSDictionary *newSidecar = CFBridgingRelease(rawNewSidecar);
    XCTAssertNotNil(newSidecar, "Should add new sidecar");

    // Now share both the private key and sidecar.
    NSDictionary *sharedPrivateKey = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassKey,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrApplicationTag: @"com.example.some-unique-tag",

        (id)kSecReturnAttributes: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedPrivateKey, "Should return shared private key clone");
    if (!sharedPrivateKey) {
        return;
    }
    NSDictionary *sharedSidecar = CFBridgingRelease(SecItemCloneToGroupKitGroup((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassInternetPassword,
        (id)kSecAttrSynchronizable: @YES,
        (id)kSecAttrAccessGroup: @"com.apple.security.securityd",
        (id)kSecAttrServer: @"example.com",

        (id)kSecReturnAttributes: @YES,
    }, CFSTR("some-example-group"), NULL));
    XCTAssertNotNil(sharedSidecar, "Should return shared sidecar clone");
    if (!sharedSidecar) {
        return;
    }

    // Stage the passkey for upload.
    XCTestExpectation *stageNewOutgoingExpectation = [self expectationWithDescription:@"Wait to stage new shared passkey"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging new outgoing shared passkey should not return an error");
        [stageNewOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[stageNewOutgoingExpectation] timeout:2.5];

    // We should have one record, with a passkey and a sidecar.
    NSArray<KCSharingOutgoingEntry *> *outgoingEntries = [store fetchAllOutgoingEntriesWithError:nil];
    XCTAssertEqual(outgoingEntries.count, 1, "Should stage outgoing passkey for upload");
    KCSharingEntryRemoteItemResult *outgoingRemoteItemResult = [outgoingEntries.firstObject remoteItemWithError:nil];
    XCTAssertNotNil(outgoingRemoteItemResult.remoteItem, "Should inflate remote item for outgoing passkey");
    XCTAssertEqual(outgoingRemoteItemResult.remoteItem.type, KCSharingItemTypePasskey);
    XCTAssertNotNil(outgoingRemoteItemResult.remoteItem.privateKey);
    XCTAssertNotNil(outgoingRemoteItemResult.remoteItem.sidecar);

    // Write the new passkey into the mirror, to simulate a successful upload.
    XCTAssertTrue([store updateMirrorWithSavedRecords:@[outgoingEntries.firstObject.record] deletedRecordIDs:nil error:nil], "Should write outgoing passkey to mirror");

    // The passkey is in the mirror now, so we shouldn't have any local changes.
    // Stage the local passkey for upload.
    XCTestExpectation *duplicateStageOutgoingExpectation = [self expectationWithDescription:@"Wait for staging after simulated upload"];
    [store stageNewOutgoingChangesWithCompletion:^(bool hasChanges, NSError *error) {
        XCTAssertNil(error, "Staging nothing after simulated upload should not return an error");
        [duplicateStageOutgoingExpectation fulfill];
    }];
    [self waitForExpectations:@[duplicateStageOutgoingExpectation] timeout:2.5];

    XCTAssertEqual([store fetchAllIncomingEntriesWithError:nil].count, 0, "Incoming queue should be empty");
    XCTAssertEqual([store fetchAllOutgoingEntriesWithError:nil].count, 0, "Outgoing queue should be empty");
    XCTAssertEqual([store fetchAllMirrorEntriesWithError:nil].count, 1, "Mirror should contain new passkey");

    // Now change the sidecar's contents remotely...
    {
        NSMutableDictionary *newRemoteSidecarAttributes = [[outgoingRemoteItemResult.remoteItem.sidecar attributesWithError:nil] mutableCopy];
        newRemoteSidecarAttributes[(id)kSecValueData] = [NSPropertyListSerialization dataWithPropertyList:@{
            @"notes": @"I have no beef with this password",
        } format:NSPropertyListBinaryFormat_v1_0 options:0 error:nil];
        KCSharingSidecarCredential *newRemoteSidecar = [[KCSharingSidecarCredential alloc] initWithAttributes:newRemoteSidecarAttributes error:nil];
        XCTAssertNotNil(newRemoteSidecar, "Should create remote sidecar with new attributes");
        KCSharingRemoteItem *newRemotePasskey = [[KCSharingRemoteItem alloc] initPasskeyWithPrivateKey:outgoingRemoteItemResult.remoteItem.privateKey sidecar:newRemoteSidecar error:nil];
        XCTAssertNotNil(newRemotePasskey, "Should create remote passkey with new sidecar");
        CKRecord *newRemoteRecord = outgoingEntries.firstObject.record;
        newRemoteRecord.encryptedValues[KCSharingRecordFieldKeyPayload] = [newRemotePasskey proto].data;

        XCTAssertTrue([store stageIncomingRecord:newRemoteRecord error:nil], "Should stage incoming queue entry for remotely changed passkey");
    }

    // Merge the remote changes into our local Keychain.
    NSError *mergeError;
    XCTAssertTrue([store mergeWithError:&mergeError], @"Should apply sidecar changes to local Keychain");
    XCTAssertNil(mergeError, @"Applying remote changes should not return an error");

    CFTypeRef rawUpdatedLocalSidecars = NULL;
    (void)SecItemCopyMatching((__bridge CFDictionaryRef)@{
        (id)kSecClass: (id)kSecClassInternetPassword,
        (id)kSecUseDataProtectionKeychain: @YES,
        (id)kSecAttrAccessGroup: store.sidecarAccessGroup,
        (id)kSecAttrSynchronizable: @NO,
        (id)kSecAttrGroupKitGroup: @"some-example-group",
        (id)kSecAttrServer: @"example.com",

        (id)kSecMatchLimit: (id)kSecMatchLimitAll,

        (id)kSecReturnAttributes: @YES,
        (id)kSecReturnData: @YES,
    }, &rawUpdatedLocalSidecars);
    NSArray *updatedLocalSidecars = CFBridgingRelease(rawUpdatedLocalSidecars);
    XCTAssertNotNil(updatedLocalSidecars);
    XCTAssertEqual(updatedLocalSidecars.count, 1);

    // `-[WBSSavedAccountStore _allSidecarsFromKeychain]` decodes the property
    // list with the same options.
    NSDictionary *updatedLocalSidecarContents = [NSPropertyListSerialization propertyListWithData:updatedLocalSidecars.firstObject[(id)kSecValueData] options:0 format:NULL error:nil];
    XCTAssertEqualObjects(updatedLocalSidecarContents[@"notes"], @"I have no beef with this password", "Should update local sidecar notes with remote contents");
}

- (void)testFetchOutgoingRecordWithRecordID {
    KCSharingStore *store = [[KCSharingStore alloc] initWithConnection:nil entryAccessGroup:@"com.apple.security.securityd" sidecarAccessGroup:@"com.apple.security.securityd" metadataDomain:nil];

    CKRecordID *randomRecordID = [[CKRecordID alloc] initWithRecordName:[NSUUID UUID].UUIDString];
    NSError *noEntryError;
    XCTAssertNil([store fetchOutgoingRecordWithRecordID:randomRecordID error:&noEntryError], "Should not return record for unknown record ID");
    XCTAssertNotNil(noEntryError, "Should return error for unknown record ID");
}

@end

#endif  // KCSHARING
