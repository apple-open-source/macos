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

#import <XCTest/XCTest.h>
#import "CloudKitMockXCTest.h"

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSItemEncrypter.h"

#include <securityd/SecItemServer.h>
#include <Security/SecItemPriv.h>
#include "OSX/sec/Security/SecItemShim.h"

@interface CloudKitKeychainAESSIVEncryptionTests : CloudKitMockXCTest
@end

@implementation CloudKitKeychainAESSIVEncryptionTests

+ (void)setUp {
    // We don't really want to spin up the whole machinery for the encryption tests
    SecCKKSDisable();

    [super setUp];
}

- (void)setUp {
    [super setUp];
}

- (void)tearDown {
    [super tearDown];
}

+ (void)tearDown {
    [super tearDown];
    SecCKKSResetSyncing();
}

- (void)testKeyGeneration {
    NSError* error = nil;
    CKKSAESSIVKey* key1 = [CKKSAESSIVKey randomKey:&error];
    XCTAssertNil(error, "Should be no error creating random key");
    CKKSAESSIVKey* key2 = [CKKSAESSIVKey randomKey:&error];
    XCTAssertNil(error, "Should be no error creating random key");

    CKKSAESSIVKey* fixedkey1 = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];
    XCTAssertNotNil(fixedkey1, "fixedkey1 generated from base64");
    CKKSAESSIVKey* fixedkey2 = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];
    XCTAssertNotNil(fixedkey2, "fixedkey2 generated from base64");

    XCTAssertEqualObjects(fixedkey1, fixedkey2, "matching fixed keys match");
    XCTAssertNotEqualObjects(fixedkey1, key1, "fixed key and random key do not match");
    XCTAssertNotEqualObjects(key1, key2, "two random keys do not match");

    XCTAssertNil([[CKKSAESSIVKey alloc] initWithBase64: @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA------AAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="], "Invalid base64 does not generate a key");
}

- (void)testBasicAESSIVEncryption {
    NSString* plaintext = @"plaintext is plain";
    NSData* plaintextData = [plaintext dataUsingEncoding: NSUTF8StringEncoding];

    NSError* error = nil;

    CKKSKey* key = [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="]
                                                         uuid:@"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"
                                                     keyclass:SecCKKSKeyClassC
                                                        state: SecCKKSProcessedStateLocal
                                                       zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                              encodedCKRecord: nil
                                                   currentkey: true];

    NSData* ciphertext = [key encryptData: plaintextData authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext, "Received a ciphertext");
    NSData* roundtrip = [key decryptData: ciphertext authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(plaintextData, roundtrip, "roundtripped data matches input");

    NSData* shortDecrypt = [key decryptData: [@"asdf" dataUsingEncoding:NSUTF8StringEncoding] authenticatedData:nil error:&error];
    XCTAssertNotNil(error, "Decrypting a short plaintext returned an error");
    XCTAssertNil(shortDecrypt, "Decrypting a short plaintext returned nil");
    error = nil;

    // Check that we're adding enough entropy
    NSData* ciphertextAgain = [key encryptData: plaintextData authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertextAgain, "Received a ciphertext");
    NSData* roundtripAgain = [key decryptData: ciphertextAgain authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtripAgain, "Received a plaintext");
    XCTAssertEqualObjects(plaintextData, roundtripAgain, "roundtripped data matches input");

    XCTAssertNotEqualObjects(ciphertext, ciphertextAgain, "two encryptions of same input produce different outputs");

    // Do it all again
    CKKSKey* key2 =  [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="]
                                                           uuid:@"f5e7f20f-0885-48f9-b75d-9f0cfd2171b6"
                                                       keyclass:SecCKKSKeyClassC
                                                          state: SecCKKSProcessedStateLocal
                                                         zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                                encodedCKRecord: nil
                                                     currentkey: true];

    NSData* ciphertext2 = [key2 encryptData: plaintextData authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext2, "Received a ciphertext");
    NSData* roundtrip2 = [key decryptData: ciphertext authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip2, "Received a plaintext");
    XCTAssertEqualObjects(plaintextData, roundtrip2, "roundtripped data matches input");

    XCTAssertNotEqualObjects(ciphertext, ciphertext2, "ciphertexts with distinct keys are distinct");
}

- (void)testAuthEncryption {
    NSString* plaintext = @"plaintext is plain";
    NSData* plaintextData = [plaintext dataUsingEncoding: NSUTF8StringEncoding];

    NSError* error = nil;

    CKKSKey* key = [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA=="]
                                                         uuid:@"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7"
                                                     keyclass:SecCKKSKeyClassC
                                                        state:SecCKKSProcessedStateLocal
                                                       zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                              encodedCKRecord:nil
                                                   currentkey:true];
    NSDictionary<NSString*, NSData*>* ad = @{ @"test": [@"data" dataUsingEncoding: NSUTF8StringEncoding] };

    NSData* ciphertext = [key encryptData: plaintextData authenticatedData: ad error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext, "Received a ciphertext");
    NSData* roundtrip = [key decryptData: ciphertext authenticatedData: ad error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(plaintextData, roundtrip, "roundtripped data matches input");

    // Without AD, decryption should fail
    roundtrip = [key decryptData: ciphertext authenticatedData: nil error: &error];
    XCTAssertNotNil(error, "Not passing in the authenticated data causes break");
    XCTAssertNil(roundtrip, "on error, don't receive plaintext");
    error = nil;

    roundtrip = [key decryptData: ciphertext authenticatedData: @{ @"test": [@"wrongdata" dataUsingEncoding: NSUTF8StringEncoding] } error: &error];
    XCTAssertNotNil(error, "Wrong authenticated data causes break");
    XCTAssertNil(roundtrip, "on error, don't receive plaintext");
    error = nil;
}

- (void)testDictionaryEncryption {
    NSDictionary<NSString*, NSData*>* plaintext = @{ @"test": [@"data" dataUsingEncoding: NSUTF8StringEncoding],
                                                     @"more": [@"testdata" dataUsingEncoding: NSUTF8StringEncoding] };
    NSDictionary<NSString*, NSData*>* roundtrip;

    NSError* error = nil;

    CKKSKey* key =  [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="]
                                                          uuid:@"f5e7f20f-0885-48f9-b75d-9f0cfd2171b6"
                                                      keyclass:SecCKKSKeyClassC
                                                         state: SecCKKSProcessedStateLocal
                                                        zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                               encodedCKRecord: nil
                                                    currentkey: true];

    NSData* ciphertext = [CKKSItemEncrypter encryptDictionary: plaintext key: key.aessivkey authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext, "Received a ciphertext");
    roundtrip = [CKKSItemEncrypter decryptDictionary: ciphertext key: key.aessivkey authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(plaintext, roundtrip, "roundtripped dictionary matches input");

    NSDictionary* authenticatedData = @{@"data": [@"auth" dataUsingEncoding: NSUTF8StringEncoding], @"moredata": [@"unauth" dataUsingEncoding: NSUTF8StringEncoding]};
    NSDictionary* unauthenticatedData = @{@"data": [@"notequal" dataUsingEncoding: NSUTF8StringEncoding], @"moredata": [@"unauth" dataUsingEncoding: NSUTF8StringEncoding]};

    NSData* authciphertext = [CKKSItemEncrypter encryptDictionary: plaintext key: key.aessivkey authenticatedData: authenticatedData error: &error];
    XCTAssertNil(error, "No error encrypting plaintext with authenticated data");
    XCTAssertNotNil(authciphertext, "Received a ciphertext");
    roundtrip = [CKKSItemEncrypter decryptDictionary: authciphertext key: key.aessivkey authenticatedData: authenticatedData error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip with authenticated data");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(plaintext, roundtrip, "roundtripped dictionary matches input");

    roundtrip = [CKKSItemEncrypter decryptDictionary: authciphertext key: key.aessivkey authenticatedData: unauthenticatedData error: &error];
    XCTAssertNotNil(error, "Error decrypting roundtrip with bad authenticated data");
    XCTAssertNil(roundtrip, "Did not receive a plaintext when authenticated data is wrong");
}

- (void)testKeyWrapping {
    CKKSAESSIVKey* key = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];

    CKKSAESSIVKey* keyToWrap = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];

    NSError* error = nil;

    CKKSWrappedAESSIVKey* wrappedKey = [key wrapAESKey: keyToWrap error:&error];
    XCTAssertNil(error, "no error wrapping key");
    XCTAssertNotNil(wrappedKey, "wrapped key was returned");

    XCTAssert(0 != memcmp(keyToWrap->key, (wrappedKey->key)+(CKKSWrappedKeySize - CKKSKeySize), CKKSKeySize), "wrapped key is different from original key");

    CKKSAESSIVKey* unwrappedKey = [key unwrapAESKey: wrappedKey error:&error];
    XCTAssertNil(error, "no error unwrapping key");
    XCTAssertNotNil(unwrappedKey, "unwrapped key was returned");

    XCTAssert(0 == memcmp(keyToWrap->key, unwrappedKey->key, CKKSKeySize), "unwrapped key matches original key");
    XCTAssertEqualObjects(keyToWrap, unwrappedKey, "unwrapped key matches original key");
}

- (void)testKeyWrappingFailure {
    CKKSAESSIVKey* key = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];

    CKKSAESSIVKey* keyToWrap = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];

    NSError* error = nil;

    CKKSWrappedAESSIVKey* wrappedKey = [key wrapAESKey: keyToWrap error:&error];
    XCTAssertNil(error, "no error wrapping key");
    XCTAssertNotNil(wrappedKey, "wrapped key was returned");

    XCTAssert(0 != memcmp(keyToWrap->key, (wrappedKey->key)+(CKKSWrappedKeySize - CKKSKeySize), CKKSKeySize), "wrapped key is different from original key");
    wrappedKey->key[0] ^= 0x1;

    CKKSAESSIVKey* unwrappedKey = [key unwrapAESKey: wrappedKey error:&error];
    XCTAssertNotNil(error, "error unwrapping key");
    XCTAssertNil(unwrappedKey, "unwrapped key was not returned in error case");
}

- (void)testKeyKeychainSaving {
    NSError* error = nil;
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];

    XCTAssertTrue([tlk saveKeyMaterialToKeychain:false error:&error], "should be able to save key material to keychain (without stashing)");
    XCTAssertNil(error, "tlk should save to database without error");
    XCTAssertTrue([tlk loadKeyMaterialFromKeychain:&error], "Should be able to reload key material");
    XCTAssertNil(error, "should be no error loading the tlk from the keychain");

    XCTAssertTrue([tlk saveKeyMaterialToKeychain:false error:&error], "should be able to save key material to keychain (without stashing)");
    XCTAssertNil(error, "tlk should save again to database without error");
    XCTAssertTrue([tlk loadKeyMaterialFromKeychain:&error], "Should be able to reload key material");
    XCTAssertNil(error, "should be no error loading the tlk from the keychain");

    [tlk deleteKeyMaterialFromKeychain:&error];
    XCTAssertNil(error, "tlk should be able to delete itself without error");

    XCTAssertFalse([tlk loadKeyMaterialFromKeychain:&error], "Should not able to reload key material");
    XCTAssertNotNil(error, "should be error loading the tlk from the keychain");
    error = nil;

    NSData* keydata = [@"asdf" dataUsingEncoding:NSUTF8StringEncoding];

    // Add an item using no viewhint that will conflict with itself upon a SecItemUpdate (internal builds only)
    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                                    (id)kSecUseDataProtectionKeychain : @YES,
                                    (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                                    (id)kSecAttrDescription: tlk.keyclass,
                                    (id)kSecAttrServer: tlk.zoneID.zoneName,
                                    (id)kSecAttrAccount: tlk.uuid,
                                    (id)kSecAttrPath: tlk.parentKeyUUID,
                                    (id)kSecAttrIsInvisible: @YES,
                                    (id)kSecValueData : keydata,
                                    (id)kSecAttrSynchronizable : (id)kCFBooleanTrue,
                                    } mutableCopy];
    XCTAssertEqual(errSecSuccess, SecItemAdd((__bridge CFDictionaryRef)query, NULL), "Should be able to add a conflicting item");

    XCTAssertTrue([tlk saveKeyMaterialToKeychain:false error:&error], "should be able to save key material to keychain (without stashing)");
    XCTAssertNil(error, "tlk should save to database without error");
    XCTAssertTrue([tlk loadKeyMaterialFromKeychain:&error], "Should be able to reload key material");
    XCTAssertNil(error, "should be no error loading the tlk from the keychain");

    XCTAssertTrue([tlk saveKeyMaterialToKeychain:false error:&error], "should be able to save key material to keychain (without stashing)");
    XCTAssertNil(error, "tlk should save again to database without error");
    XCTAssertTrue([tlk loadKeyMaterialFromKeychain:&error], "Should be able to reload key material");
    XCTAssertNil(error, "should be no error loading the tlk from the keychain");
}

- (void)testKeyHierarchy {
    NSError* error = nil;
    NSData* testCKRecord = [@"nonsense" dataUsingEncoding:NSUTF8StringEncoding];
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];

    [tlk saveToDatabase:&error];
    [tlk saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, "tlk saved to database without error");

    CKKSKey* level1 = [CKKSKey randomKeyWrappedByParent: tlk keyclass:SecCKKSKeyClassA error:&error];
    level1.encodedCKRecord = testCKRecord;
    XCTAssertNotNil(level1, "level 1 key created");
    XCTAssertNil(error, "level 1 key created");

    [level1 saveToDatabase:&error];
    XCTAssertNil(error, "level 1 key saved to database without error");

    CKKSKey* level2 = [CKKSKey randomKeyWrappedByParent: level1 error:&error];
    level2.encodedCKRecord = testCKRecord;
    XCTAssertNotNil(level2, "level 2 key created");
    XCTAssertNil(error, "no error creating level 2 key");
    [level2 saveToDatabase:&error];
    XCTAssertNil(error, "level 2 key saved to database without error");

    NSString* level2UUID = level2.uuid;

    // Fetch the level2 key from the database.
    CKKSKey* extractedkey = [CKKSKey fromDatabase:level2UUID zoneID:self.testZoneID error:&error];
    [extractedkey unwrapViaKeyHierarchy: &error];
    XCTAssertNotNil(extractedkey, "could fetch key again");
    XCTAssertNil(error, "no error fetching key from database");

    CKKSAESSIVKey* extracedaeskey = [extractedkey ensureKeyLoaded:&error];
    XCTAssertNotNil(extractedkey, "fetched key could unwrap");
    XCTAssertNil(error, "no error forcing unwrap on fetched key");

    XCTAssertEqualObjects(level2.aessivkey, extracedaeskey, @"fetched aes key is equal to saved key");
}

- (void)ensureKeychainSaveLoad: (CKKSKey*) key {
    NSError* error = nil;
    [key saveToDatabase:&error];
    XCTAssertNil(error, "no error saving to database");
    [key saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, "no error saving to keychain");

    CKKSKey* loadedKey = [CKKSKey fromDatabase:key.uuid zoneID:self.testZoneID error:&error];
    XCTAssertNil(error, "no error loading from database");
    XCTAssertNotNil(loadedKey, "Received an item back from the database");

    XCTAssert([loadedKey loadKeyMaterialFromKeychain:&error], "could load key material back from keychain");
    XCTAssertNil(error, "no error loading key from keychain");

    XCTAssertEqualObjects(loadedKey.aessivkey, key.aessivkey, "Loaded key is identical after save/load");
}

- (void)testKeychainSave {
    NSError* error = nil;
    NSData* testCKRecord = [@"nonsense" dataUsingEncoding:NSUTF8StringEncoding];
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];
    [self ensureKeychainSaveLoad: tlk];

    // Ensure that Class A and Class C can do the same thing
    CKKSKey* classA = [CKKSKey randomKeyWrappedByParent: tlk keyclass:SecCKKSKeyClassA error:&error];
    classA.encodedCKRecord = testCKRecord;
    XCTAssertNil(error, "No error creating random class A key");
    [self ensureKeychainSaveLoad: classA];
    CKKSKey* classC = [CKKSKey randomKeyWrappedByParent: tlk keyclass:SecCKKSKeyClassC error:&error];
    classC.encodedCKRecord = testCKRecord;
    XCTAssertNil(error, "No error creating random class C key");
    [self ensureKeychainSaveLoad: classC];
}

- (void)testCKKSKeyProtobuf {
    NSError* error = nil;
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];

    NSData* tlkPersisted = [tlk serializeAsProtobuf:&error];
    XCTAssertNil(error, "Shouldn't have been an error serializing to protobuf");
    XCTAssertNotNil(tlkPersisted, "Should have gotten some protobuf data back");

    CKKSKey* otherKey = [CKKSKey loadFromProtobuf:tlkPersisted error:&error];
    XCTAssertNil(error, "Shouldn't have been an error serializing from protobuf");
    XCTAssertNotNil(otherKey, "Should have gotten some protobuf data back");

    XCTAssertEqualObjects(tlk.uuid, otherKey.uuid, "Should have gotten the same UUID");
    XCTAssertEqualObjects(tlk.keyclass, otherKey.keyclass, "Should have gotten the same key class");
    XCTAssertEqualObjects(tlk.zoneID, otherKey.zoneID, "Should have gotten the same zoneID");
    XCTAssertEqualObjects(tlk.aessivkey, otherKey.aessivkey, "Should have gotten the same underlying key back");
    XCTAssertEqualObjects(tlk, otherKey, "Should have gotten the same key");
}

- (BOOL)tryDecryptWithProperAuthData:(CKKSItem*)ciphertext plaintext:(NSDictionary<NSString*, NSData*>*)plaintext {
    NSDictionary<NSString*, NSData*>* roundtrip;
    NSError *error = nil;
    roundtrip = [CKKSItemEncrypter decryptItemToDictionary: (CKKSItem*) ciphertext error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(plaintext, roundtrip, "roundtripped dictionary matches input");
    return error == nil && roundtrip != nil && [plaintext isEqualToDictionary:roundtrip];
}

- (BOOL)tryDecryptWithBrokenAuthData:(CKKSItem *)ciphertext {
    NSDictionary<NSString*, NSData*>* brokenAuthentication;
    NSError *error = nil;
    brokenAuthentication = [CKKSItemEncrypter decryptItemToDictionary: (CKKSItem*) ciphertext error: &error];
    XCTAssertNotNil(error, "Error exists decrypting ciphertext with bad authenticated data: %@", error);
    XCTAssertNil(brokenAuthentication, "Did not receive a plaintext if authenticated data was mucked with");
    return error != nil && brokenAuthentication == nil;
}

- (void)testItemDictionaryEncryption {
    NSDictionary<NSString*, NSData*>* plaintext = @{ @"test": [@"data" dataUsingEncoding: NSUTF8StringEncoding],
                                                     @"more": [@"testdata" dataUsingEncoding: NSUTF8StringEncoding] };
    NSError* error = nil;
    NSString *uuid = @"8b2aeb7f-4af3-43e9-b6e6-70d5c728ebf7";

    CKKSKey* key = [self fakeTLK:self.testZoneID];
    [key saveToDatabase: &error];
    [key saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, @"could save the fake TLK to the database");

    CKKSItem* ciphertext = [CKKSItemEncrypter encryptCKKSItem: [[CKKSItem alloc] initWithUUID:uuid
                                                                                parentKeyUUID:key.uuid
                                                                                       zoneID:self.testZoneID]
                                               dataDictionary:plaintext
                                             updatingCKKSItem:nil
                                                    parentkey:key
                                                        error:&error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext, "Received a ciphertext");
    XCTAssertEqual(ciphertext.encver, currentCKKSItemEncryptionVersion, "Encryption sets the current protocol version");

    [self tryDecryptWithProperAuthData:ciphertext plaintext:plaintext];

    // Make sure these fields are authenticated and that authentication works.
    // Messing with them should make the item not decrypt.
    ciphertext.generationCount = 100;
    XCTAssertTrue([self tryDecryptWithBrokenAuthData:ciphertext], "Decryption with broken authentication data fails");
    ciphertext.generationCount = 0;
    XCTAssertTrue([self tryDecryptWithProperAuthData:ciphertext plaintext:plaintext], "Decryption with authentication data succeeds");

    ciphertext.encver += 1;
    XCTAssertTrue([self tryDecryptWithBrokenAuthData:ciphertext], "Decryption with broken authentication data fails");
    ciphertext.encver -= 1;
    XCTAssertTrue([self tryDecryptWithProperAuthData:ciphertext plaintext:plaintext], "Decryption with authentication data succeeds");

    ciphertext.uuid = @"x";
    XCTAssertTrue([self tryDecryptWithBrokenAuthData:ciphertext], "Decryption with broken authentication data fails");
    ciphertext.uuid = uuid;
    XCTAssertTrue([self tryDecryptWithProperAuthData:ciphertext plaintext:plaintext], "Decryption with authentication data succeeds");
}

- (void)testEncryptionVersions {
    NSDictionary<NSString*, NSData*>* plaintext = @{ @"test": [@"data" dataUsingEncoding: NSUTF8StringEncoding],
                                                     @"more": [@"testdata" dataUsingEncoding: NSUTF8StringEncoding] };
    NSDictionary<NSString*, NSData*>* output;
    NSError *error = nil;
    NSData* data = [NSPropertyListSerialization dataWithPropertyList:plaintext
                                                              format:NSPropertyListBinaryFormat_v1_0
                                                             options:0
                                                               error:&error];
    XCTAssertNil(error);
    CKKSKey* key = [self fakeTLK:self.testZoneID];
    [key saveToDatabase: &error];
    [key saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, @"could save the fake TLK to the database");

    CKKSAESSIVKey* keyToWrap = [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="];
    CKKSWrappedAESSIVKey* wrappedKey = [key wrapAESKey: keyToWrap error:&error];
    XCTAssertNil(error, "no error wrapping key");
    XCTAssertNotNil(wrappedKey, "wrapped key was returned");
    CKKSItem* baseitem = [[CKKSItem alloc] initWithUUID:@"abc"
                                          parentKeyUUID:key.uuid
                                                 zoneID:self.testZoneID
                                                encItem:data
                                             wrappedkey:wrappedKey
                                        generationCount:0
                                                 encver:CKKSItemEncryptionVersionNone];
    XCTAssertNotNil(baseitem, "Constructed CKKSItem");

    // First try versionNone. Should fail, we don't support unencrypted data
    output = [CKKSItemEncrypter decryptItemToDictionary:baseitem error:&error];
    XCTAssert(error, "Did not failed to decrypt v0 item");
    XCTAssertNil(output, "Did not failed to decrypt v0 item");
    error = nil;
    output = nil;

    // Then try version1. Should take actual decryption path and fail because there's no properly encrypted data.
    baseitem.encver = CKKSItemEncryptionVersion1;
    output = [CKKSItemEncrypter decryptItemToDictionary:baseitem error:&error];
    XCTAssertNotNil(error, "Taking v1 codepath without encrypted item fails");
    XCTAssertEqualObjects(error.localizedDescription, @"could not ccsiv_crypt", "Error specifically failure to ccsiv_crypt");
    XCTAssertNil(output, "Did not receive output from failed decryption call");
    error = nil;
    output = nil;

    // Finally, some unknown version should fail immediately
    baseitem.encver = 100;
    output = [CKKSItemEncrypter decryptItemToDictionary:baseitem error:&error];
    XCTAssertNotNil(error);
    NSString *errstr = [NSString stringWithFormat:@"%@", error.localizedDescription];
    NSString *expected = @"Unrecognized encryption version: 100";
    XCTAssertEqualObjects(expected, errstr, "Error is specific to unrecognized version failure");
    XCTAssertNil(output);
}

- (void)testKeychainPersistence {

    NSString* plaintext = @"plaintext is plain";
    NSData* plaintextData = [plaintext dataUsingEncoding: NSUTF8StringEncoding];

    NSError* error = nil;

    NSString* uuid = @"f5e7f20f-0885-48f9-b75d-9f0cfd2171b6";

    CKKSKey* key =  [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="]
                                                           uuid:uuid
                                                       keyclass:SecCKKSKeyClassA
                                                          state:SecCKKSProcessedStateLocal
                                                         zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                                encodedCKRecord: nil
                                                     currentkey: true];

    NSData* ciphertext = [key encryptData: plaintextData authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext, "Received a ciphertext");
    NSData* roundtrip = [key decryptData: ciphertext authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(plaintextData, roundtrip, "roundtripped data matches input");

    // Check that there is no key material in the keychain
    CKKSKey* reloadedKey = [CKKSKey keyFromKeychain:uuid
                                      parentKeyUUID:uuid
                                           keyclass:SecCKKSKeyClassA
                                              state:SecCKKSProcessedStateLocal
                                             zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                    encodedCKRecord:nil
                                         currentkey:true
                                              error:&error];

    XCTAssertNotNil(error, "error exists when there's nothing in the keychain");
    XCTAssertNil(reloadedKey, "no key object when there's nothing in the keychain");
    error = nil;

    [key saveKeyMaterialToKeychain:&error];
    XCTAssertNil(error, "Could save key material to keychain");

    // Reload the key material and check that it works
    reloadedKey = [CKKSKey keyFromKeychain:uuid
                             parentKeyUUID:uuid
                                  keyclass:SecCKKSKeyClassA
                                     state:SecCKKSProcessedStateLocal
                                    zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                           encodedCKRecord:nil
                                currentkey:true
                                     error:&error];

    XCTAssertNil(error, "No error loading key from keychain");
    XCTAssertNotNil(reloadedKey, "Could load key from keychain");

    NSData* ciphertext2 = [reloadedKey encryptData: plaintextData authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext2, "Received a ciphertext");
    NSData* roundtrip2 = [reloadedKey decryptData: ciphertext2 authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip2, "Received a plaintext");
    XCTAssertEqualObjects(plaintextData, roundtrip2, "roundtripped data matches input");

    XCTAssertEqualObjects(key.aessivkey, reloadedKey.aessivkey, "reloaded AES key is equal to generated key");

    [key deleteKeyMaterialFromKeychain: &error];
    XCTAssertNil(error, "could delete key material from keychain");

    // Check that there is no key material in the keychain
    // Note that TLKs will be stashed (and deleteKeyMaterial won't delete the stash), and so this test would fail for a TLK

    reloadedKey = [CKKSKey keyFromKeychain:uuid
                             parentKeyUUID:uuid
                                  keyclass:SecCKKSKeyClassA
                                     state:SecCKKSProcessedStateLocal
                                    zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                           encodedCKRecord:nil
                                currentkey:true
                                     error:&error];

    XCTAssertNotNil(error, "error exists when there's nothing in the keychain");
    XCTAssertNil(reloadedKey, "no key object when there's nothing in the keychain");
    error = nil;
}

- (void)testCKKSKeyTrialSelfWrapped {
    NSError* error = nil;
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];
    XCTAssertTrue([tlk wrapsSelf], "TLKs should wrap themselves");

    CKRecord* record = [tlk CKRecordWithZoneID:self.testZoneID];
    XCTAssertNotNil(record, "TLKs should know how to turn themselves into CKRecords");
    CKKSKey* receivedTLK = [[CKKSKey alloc] initWithCKRecord:record];
    XCTAssertNotNil(receivedTLK, "Keys should know how to recover themselves from CKRecords");

    XCTAssertTrue([receivedTLK wrapsSelf], "TLKs should wrap themselves, even when received from CloudKit");

    XCTAssertFalse([receivedTLK ensureKeyLoaded:&error], "Received keys can't load themselves when there's no key data");
    XCTAssertNotNil(error, "Error should exist when a key fails to load itself");
    error = nil;

    XCTAssertTrue([receivedTLK trySelfWrappedKeyCandidate:tlk.aessivkey error:&error], "Shouldn't be an error when we give a CKKSKey its key");
    XCTAssertNil(error, "Shouldn't be an error giving a CKKSKey its key material");

    XCTAssertTrue([receivedTLK ensureKeyLoaded:&error], "Once a CKKSKey has its key material, it doesn't need to load it again");
    XCTAssertNil(error, "Shouldn't be an error loading a loaded CKKSKey");
}

- (void)testCKKSKeyTrialSelfWrappedFailure {
    NSError* error = nil;
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];
    XCTAssertTrue([tlk wrapsSelf], "TLKs should wrap themselves");

    CKRecord* record = [tlk CKRecordWithZoneID:self.testZoneID];
    XCTAssertNotNil(record, "TLKs should know how to turn themselves into CKRecords");
    CKKSKey* receivedTLK = [[CKKSKey alloc] initWithCKRecord:record];
    XCTAssertNotNil(receivedTLK, "Keys should know how to recover themselves from CKRecords");

    XCTAssertTrue([receivedTLK wrapsSelf], "TLKs should wrap themselves, even when received from CloudKit");

    XCTAssertFalse([receivedTLK ensureKeyLoaded:&error], "Received keys can't load themselves when there's no key data");
    XCTAssertNotNil(error, "Error should exist when a key fails to load itself");
    error = nil;

    XCTAssertFalse([receivedTLK trySelfWrappedKeyCandidate:[[CKKSAESSIVKey alloc] initWithBase64: @"aaaaaZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="] error:&error], "Should be an error when we give a CKKSKey the wrong key");
    XCTAssertNotNil(error, "Should be an error giving a CKKSKey the wrong key material");

    XCTAssertFalse([receivedTLK ensureKeyLoaded:&error], "Received keys can't load themselves when there's no key data");
    XCTAssertNotNil(error, "Error should exist when a key fails to load itself");
    error = nil;
}

- (void)testCKKSKeyTrialNotSelfWrappedFailure {
    NSError* error = nil;
    CKKSKey* tlk =  [self fakeTLK:self.testZoneID];
    XCTAssertTrue([tlk wrapsSelf], "TLKs should wrap themselves");

    CKKSKey* classC = [CKKSKey randomKeyWrappedByParent: tlk keyclass:SecCKKSKeyClassC error:&error];
    XCTAssertFalse([classC wrapsSelf], "Wrapped keys should not wrap themselves");

    XCTAssertTrue([classC ensureKeyLoaded:&error], "Once a CKKSKey has its key material, it doesn't need to load it again");
    XCTAssertNil(error, "Shouldn't be an error loading a loaded CKKSKey");

    XCTAssertFalse([classC trySelfWrappedKeyCandidate:classC.aessivkey error:&error], "Should be an error when we attempt to trial a key on a non-self-wrapped key");
    XCTAssertNotNil(error, "Should be an error giving a CKKSKey the wrong key material");
    XCTAssertEqual(error.code, CKKSKeyNotSelfWrapped, "Should have gotten CKKSKeyNotSelfWrapped as an error");
    error = nil;

    // But, since we didn't throw away its key, it's still loaded
    XCTAssertTrue([classC ensureKeyLoaded:&error], "Once a CKKSKey has its key material, it doesn't need to load it again");
    XCTAssertNil(error, "Shouldn't be an error loading a loaded CKKSKey");
}

- (BOOL)padAndUnpadDataWithLength:(NSUInteger)dataLength blockSize:(NSUInteger)blockSize extra:(BOOL)extra {
    // Test it works
    NSMutableData *data = [NSMutableData dataWithLength:dataLength];
    memset((unsigned char *)[data mutableBytes], 0x55, dataLength);
    NSMutableData *orig = [data mutableCopy];
    NSData *padded = [CKKSItemEncrypter padData:data blockSize:blockSize additionalBlock:extra];
    XCTAssertNotNil(padded, "Padding never returns nil");
    XCTAssertEqualObjects(data, orig, "Input object unmodified");
    XCTAssertTrue(padded.length % blockSize == 0, "Padded data aligns on %lu-byte blocksize", (unsigned long)blockSize);
    XCTAssertTrue(padded.length > data.length, "At least one byte of padding has been added");
    NSData *unpadded = [CKKSItemEncrypter removePaddingFromData:padded];
    XCTAssertNotNil(unpadded, "Successfully removed padding again");

    // Test it fails by poking some byte in the padding
    NSMutableData *glitch = [NSMutableData dataWithData:padded];
    NSUInteger offsetFromTop = glitch.length - arc4random_uniform((unsigned)(glitch.length - data.length)) - 1;
    uint8_t poke = ((uint8_t)arc4random_uniform(0xFF) & 0x7E) + 1; // This gets most of the values while excluding 0 and 0x80
    unsigned char *bytes = [glitch mutableBytes];
    bytes[offsetFromTop] = poke;
    XCTAssertNil([CKKSItemEncrypter removePaddingFromData:glitch], "Cannot remove broken padding (len %lu, dlen %lu, plen %lu glitchidx %lu, glitchval 0x%x)", (unsigned long)glitch.length, (unsigned long)data.length, (unsigned long)glitch.length - data.length, (unsigned long)offsetFromTop, poke);

    return padded && unpadded && [unpadded isEqual:data];
}

- (void)testPadding {
	[self runPaddingTest:NO];
	[self runPaddingTest:YES];

    NSData *data = nil;
    XCTAssertNil([CKKSItemEncrypter removePaddingFromData:[NSData data]], "zero data valid ?");

    data = [CKKSItemEncrypter removePaddingFromData:[NSData dataWithBytes:"\x80" length:1]];
    XCTAssert(data && data.length == 0, "data wrong size");

    data = [CKKSItemEncrypter removePaddingFromData:[NSData dataWithBytes:"\x80\x00" length:2]];
    XCTAssert(data && data.length == 0, "data wrong size");
    data = [CKKSItemEncrypter removePaddingFromData:[NSData dataWithBytes:"\x80\x00\x00" length:3]];
    XCTAssert(data && data.length == 0, "data wrong size");
    data = [CKKSItemEncrypter removePaddingFromData:[NSData dataWithBytes:"\x80\x80\x80" length:3]];
    XCTAssert(data && data.length == 2, "data wrong size");
    data = [CKKSItemEncrypter removePaddingFromData:[NSData dataWithBytes:"\x80\x80\x00" length:3]];
    XCTAssert(data && data.length == 1, "data wrong size");
    data = [CKKSItemEncrypter removePaddingFromData:[NSData dataWithBytes:"\x00\x80\x00" length:3]];
    XCTAssert(data && data.length == 1, "data wrong size");

}

- (void)runPaddingTest:(BOOL)extra {

	// Aligned, arbitrary lengths
	for (int idx = 1; idx <= 128; ++idx) {
		XCTAssertTrue([self padAndUnpadDataWithLength:idx blockSize:idx extra:extra], "Padding aligned data succeeds");
	}

	// Off-by-one, arbitrary lengths
	for (int idx = 1; idx <= 128; ++idx) {
		XCTAssertTrue([self padAndUnpadDataWithLength:idx - 1 blockSize:idx extra:extra], "Padding aligned data succeeds");
		XCTAssertTrue([self padAndUnpadDataWithLength:idx + 1 blockSize:idx extra:extra], "Padding aligned data succeeds");
	}

	// Misaligned, arbitrary lengths
	for (int idx = 1; idx <= 1000; ++idx) {
		NSUInteger dataSize = arc4random_uniform(128) + 1;
		NSUInteger blockSize = arc4random_uniform(128) + 1;
		XCTAssertTrue([self padAndUnpadDataWithLength:dataSize blockSize:blockSize extra:extra], "Padding data lenght %lu to blockSize %lu succeeds", (unsigned long)dataSize, (unsigned long)blockSize);
	}

	// Special case: blocksize 0 results in 1 byte of padding always
	NSMutableData *data = [NSMutableData dataWithLength:23];
	memset((unsigned char *)[data mutableBytes], 0x55, 23);
	NSData *padded = [CKKSItemEncrypter padData:data blockSize:0 additionalBlock:extra];
	XCTAssertNotNil(padded, "Padding never returns nil");
    XCTAssertTrue(padded.length == data.length + extra ? 2 : 1, "One byte of padding has been added, 2 if extra padding");
	NSData *unpadded = [CKKSItemEncrypter removePaddingFromData:padded];
	XCTAssertNotNil(unpadded, "Successfully removed padding again");
	XCTAssertEqualObjects(data, unpadded, "Data effectively unmodified through padding-unpadding trip");

	// Nonpadded data
	unpadded = [CKKSItemEncrypter removePaddingFromData:data];
	XCTAssertNil(unpadded, "Cannot remove padding where none exists");

	// Feeding nil
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
	padded = [CKKSItemEncrypter padData:nil blockSize:0 additionalBlock:extra];
	XCTAssertNotNil(padded, "padData always returns a data object");
    XCTAssertEqual(padded.length, extra ? 2ul : 1ul, "Length of padded nil object is padding byte only--two if extra");
	unpadded = [CKKSItemEncrypter removePaddingFromData:nil];
	XCTAssertNil(unpadded, "Removing padding from nil is senseless");
#pragma clang diagnostic pop
}

- (BOOL)encryptAndDecryptDictionary:(NSDictionary<NSString*, NSData*>*)data key:(CKKSKey *)key {
    NSDictionary<NSString*, NSData*>* roundtrip;
    NSError *error = nil;
    NSData* ciphertext = [CKKSItemEncrypter encryptDictionary: data key: key.aessivkey authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error encrypting plaintext");
    XCTAssertNotNil(ciphertext, "Received a ciphertext");
    // AES-SIV adds 32 bytes, need to subtract them
    XCTAssertTrue((ciphertext.length - 32) % SecCKKSItemPaddingBlockSize == 0, "Ciphertext aligned on %lu-byte boundary", (unsigned long)SecCKKSItemPaddingBlockSize);
    roundtrip = [CKKSItemEncrypter decryptDictionary: ciphertext key: key.aessivkey authenticatedData: nil error: &error];
    XCTAssertNil(error, "No error decrypting roundtrip");
    XCTAssertNotNil(roundtrip, "Received a plaintext");
    XCTAssertEqualObjects(data, roundtrip, "roundtripped dictionary matches input");
    return (ciphertext.length  - 32) % SecCKKSItemPaddingBlockSize == 0 && roundtrip && error == nil && [data isEqualToDictionary:roundtrip];
}

- (void)testDictionaryPadding {
    // Pad a bunch of bytes to nearest boundary
    NSDictionary<NSString*, NSData*>* unaligned_74 = @{ @"test": [@"data" dataUsingEncoding: NSUTF8StringEncoding],
                                                     @"more": [@"testdata" dataUsingEncoding: NSUTF8StringEncoding] };
    // Pad precisely one byte
    NSDictionary<NSString*, NSData*>* unaligned_79 = @{ @"test12345": [@"data" dataUsingEncoding: NSUTF8StringEncoding],
                                                      @"more": [@"testdata" dataUsingEncoding: NSUTF8StringEncoding] };
    // Already on boundary, pad until next boundary
    NSDictionary<NSString*, NSData*>* aligned_80 = @{ @"test123456": [@"data" dataUsingEncoding: NSUTF8StringEncoding],
                                                      @"more": [@"testdata" dataUsingEncoding: NSUTF8StringEncoding] };
    CKKSKey* key =  [[CKKSKey alloc] initSelfWrappedWithAESKey: [[CKKSAESSIVKey alloc] initWithBase64: @"uImdbZ7Zg+6WJXScTnRBfNmoU1UiMkSYxWc+d1Vuq3IFn2RmTRkTdWTe3HmeWo1pAomqy+upK8KHg2PGiRGhqg=="]
                                                          uuid:@"f5e7f20f-0885-48f9-b75d-9f0cfd2171b6"
                                                      keyclass:SecCKKSKeyClassC
                                                         state:SecCKKSProcessedStateLocal
                                                        zoneID:[[CKRecordZoneID alloc] initWithZoneName:@"testzone" ownerName:CKCurrentUserDefaultName]
                                               encodedCKRecord:nil
                                                    currentkey:true];

    XCTAssertTrue([self encryptAndDecryptDictionary:unaligned_74 key:key], "Roundtrip with unaligned data succeeds");
    XCTAssertTrue([self encryptAndDecryptDictionary:unaligned_79 key:key], "Roundtrip with unaligned data succeeds");
    XCTAssertTrue([self encryptAndDecryptDictionary:aligned_80 key:key], "Roundtrip with aligned data succeeds");
}

- (void)testCKKSKeychainBackedKeySerialization {
    NSError* error = nil;
    CKKSKeychainBackedKey* tlk =  [self fakeTLK:self.testZoneID].keycore;
    CKKSKeychainBackedKey* classC = [CKKSKeychainBackedKey randomKeyWrappedByParent:tlk keyclass:SecCKKSKeyClassC error:&error];
    XCTAssertNil(error, "Should be no error creating classC key");

    XCTAssertTrue([tlk saveKeyMaterialToKeychain:&error], "Should be able to save tlk key material to keychain");
    XCTAssertNil(error, "Should be no error saving key material");

    XCTAssertTrue([classC saveKeyMaterialToKeychain:&error], "Should be able to save classC key material to keychain");
    XCTAssertNil(error, "Should be no error saving key material");

    NSKeyedArchiver* encoder = [[NSKeyedArchiver alloc] initRequiringSecureCoding:YES];
    [encoder encodeObject:tlk forKey:@"tlk"];
    [encoder encodeObject:classC forKey:@"classC"];
    NSData* data = encoder.encodedData;
    XCTAssertNotNil(data, "encoding should have produced some data");

    NSKeyedUnarchiver* decoder = [[NSKeyedUnarchiver alloc] initForReadingFromData:data error:nil];
    CKKSKeychainBackedKey* decodedTLK = [decoder decodeObjectOfClass: [CKKSKeychainBackedKey class] forKey:@"tlk"];
    CKKSKeychainBackedKey* decodedClassC = [decoder decodeObjectOfClass: [CKKSKeychainBackedKey class] forKey:@"classC"];

    XCTAssertEqualObjects(tlk, decodedTLK, "TLKs should transit NSSecureCoding without changes");
    XCTAssertEqualObjects(classC, decodedClassC, "Class C keys should transit NSSecureCoding without changes");

    // Now, check that they can load the key material

    XCTAssertTrue([decodedTLK loadKeyMaterialFromKeychain:&error], "Should be able to load tlk key material from keychain");
    XCTAssertNil(error, "Should be no error from loading key material");

    XCTAssertTrue([decodedClassC loadKeyMaterialFromKeychain:&error], "Should be able to load classC key material from keychain");
    XCTAssertNil(error, "Should be no error from loading key material");
}

@end

#endif // OCTAGON
