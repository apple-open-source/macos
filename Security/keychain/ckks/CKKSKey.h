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

#import <Foundation/Foundation.h>

#import "keychain/ckks/CKKSItem.h"
#import "keychain/ckks/CKKSSIV.h"

#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/proto/source/CKKSSerializedKey.h"

@interface CKKSKey : CKKSItem

@property (readonly) CKKSAESSIVKey* aessivkey;

@property (copy) CKKSProcessedState* state;
@property (copy) CKKSKeyClass* keyclass;
@property bool currentkey;

// Fetches and attempts to unwrap this key for use
+ (instancetype)loadKeyWithUUID:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

// Creates new random keys, in the parent's zone
+ (instancetype)randomKeyWrappedByParent:(CKKSKey*)parentKey error:(NSError* __autoreleasing*)error;
+ (instancetype)randomKeyWrappedByParent:(CKKSKey*)parentKey
                                keyclass:(CKKSKeyClass*)keyclass
                                   error:(NSError* __autoreleasing*)error;

// Creates a new random key that wraps itself
+ (instancetype)randomKeyWrappedBySelf:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

/* Helper functions for persisting key material in the keychain */
- (bool)saveKeyMaterialToKeychain:(NSError* __autoreleasing*)error;
- (bool)saveKeyMaterialToKeychain:(bool)stashTLK
                            error:(NSError* __autoreleasing*)error;  // call this to not stash a non-syncable TLK, if that's what you want

- (bool)loadKeyMaterialFromKeychain:(NSError* __autoreleasing*)error;
- (bool)deleteKeyMaterialFromKeychain:(NSError* __autoreleasing*)error;
+ (NSString*)isItemKeyForKeychainView:(SecDbItemRef)item;

// Class methods to help tests
+ (NSDictionary*)setKeyMaterialInKeychain:(NSDictionary*)query error:(NSError* __autoreleasing*)error;
+ (NSDictionary*)queryKeyMaterialInKeychain:(NSDictionary*)query error:(NSError* __autoreleasing*)error;

+ (instancetype)keyFromKeychain:(NSString*)uuid
                  parentKeyUUID:(NSString*)parentKeyUUID
                       keyclass:(CKKSKeyClass*)keyclass
                          state:(CKKSProcessedState*)state
                         zoneID:(CKRecordZoneID*)zoneID
                encodedCKRecord:(NSData*)encodedrecord
                     currentkey:(NSInteger)currentkey
                          error:(NSError* __autoreleasing*)error;


+ (instancetype)fromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype)tryFromDatabaseAnyState:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSKey*>*)selfWrappedKeys:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (instancetype)currentKeyForClass:(CKKSKeyClass*)keyclass zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSArray<CKKSKey*>*)currentKeysForClass:(CKKSKeyClass*)keyclass
                                    state:(CKKSProcessedState*)state
                                   zoneID:(CKRecordZoneID*)zoneID
                                    error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSKey*>*)allKeys:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSArray<CKKSKey*>*)remoteKeys:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSArray<CKKSKey*>*)localKeys:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

- (bool)saveToDatabaseAsOnlyCurrentKeyForClassAndState:(NSError* __autoreleasing*)error;

- (instancetype)init NS_UNAVAILABLE;

- (instancetype)initSelfWrappedWithAESKey:(CKKSAESSIVKey*)aeskey
                                     uuid:(NSString*)uuid
                                 keyclass:(CKKSKeyClass*)keyclass
                                    state:(CKKSProcessedState*)state
                                   zoneID:(CKRecordZoneID*)zoneID
                          encodedCKRecord:(NSData*)encodedrecord
                               currentkey:(NSInteger)currentkey;

- (instancetype)initWrappedBy:(CKKSKey*)wrappingKey
                       AESKey:(CKKSAESSIVKey*)aeskey
                         uuid:(NSString*)uuid
                     keyclass:(CKKSKeyClass*)keyclass
                        state:(CKKSProcessedState*)state
                       zoneID:(CKRecordZoneID*)zoneID
              encodedCKRecord:(NSData*)encodedrecord
                   currentkey:(NSInteger)currentkey;

- (instancetype)initWithWrappedAESKey:(CKKSWrappedAESSIVKey*)wrappedaeskey
                                 uuid:(NSString*)uuid
                        parentKeyUUID:(NSString*)parentKeyUUID
                             keyclass:(CKKSKeyClass*)keyclass
                                state:(CKKSProcessedState*)state
                               zoneID:(CKRecordZoneID*)zoneID
                      encodedCKRecord:(NSData*)encodedrecord
                           currentkey:(NSInteger)currentkey;

/* Returns true if we believe this key wraps itself. */
- (bool)wrapsSelf;

- (void)zeroKeys;

- (CKKSKey*)topKeyInAnyState:(NSError* __autoreleasing*)error;

// Attempts checks if the AES key is already loaded, or attempts to load it from the keychain. Returns false if it fails.
- (CKKSAESSIVKey*)ensureKeyLoaded:(NSError* __autoreleasing*)error;

// Attempts to unwrap this key via unwrapping its wrapping keys via the key hierarchy.
- (CKKSAESSIVKey*)unwrapViaKeyHierarchy:(NSError* __autoreleasing*)error;

// On a self-wrapped key, determine if this AES-SIV key is the self-wrapped key.
// If it is, save the key as this CKKSKey's unwrapped key.
- (bool)trySelfWrappedKeyCandidate:(CKKSAESSIVKey*)candidate error:(NSError* __autoreleasing*)error;

- (CKKSWrappedAESSIVKey*)wrapAESKey:(CKKSAESSIVKey*)keyToWrap error:(NSError* __autoreleasing*)error;
- (CKKSAESSIVKey*)unwrapAESKey:(CKKSWrappedAESSIVKey*)keyToUnwrap error:(NSError* __autoreleasing*)error;

- (bool)wrapUnder:(CKKSKey*)wrappingKey error:(NSError* __autoreleasing*)error;
- (bool)unwrapSelfWithAESKey:(CKKSAESSIVKey*)unwrappingKey error:(NSError* __autoreleasing*)error;

- (NSData*)encryptData:(NSData*)plaintext
     authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                 error:(NSError* __autoreleasing*)error;
- (NSData*)decryptData:(NSData*)ciphertext
     authenticatedData:(NSDictionary<NSString*, NSData*>*)ad
                 error:(NSError* __autoreleasing*)error;

- (NSData*)serializeAsProtobuf:(NSError* __autoreleasing*)error;
+ (CKKSKey*)loadFromProtobuf:(NSData*)data error:(NSError* __autoreleasing*)error;

+ (NSDictionary<NSString*, NSNumber*>*)countsByClass:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
@end

#endif
