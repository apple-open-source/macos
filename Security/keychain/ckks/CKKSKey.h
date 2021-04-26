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
#import "keychain/ckks/CKKSKeychainBackedKey.h"
#import "keychain/ckks/CKKSSIV.h"

#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/proto/generated_source/CKKSSerializedKey.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSPeerProviderState;

@interface CKKSKey : CKKSCKRecordHolder
@property CKKSKeychainBackedKey* keycore;

@property NSString* uuid;
@property NSString* parentKeyUUID;
@property (copy) CKKSKeyClass* keyclass;

@property (copy) CKKSWrappedAESSIVKey* wrappedkey;
@property (nullable, readonly) CKKSAESSIVKey* aessivkey;

@property (copy) CKKSProcessedState* state;
@property bool currentkey;

@property (readonly) NSString* zoneName;

// Fetches and attempts to unwrap this key for use
+ (instancetype _Nullable)loadKeyWithUUID:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

// Creates new random keys, in the parent's zone
+ (instancetype _Nullable)randomKeyWrappedByParent:(CKKSKey*)parentKey error:(NSError* __autoreleasing*)error;
+ (instancetype _Nullable)randomKeyWrappedByParent:(CKKSKey*)parentKey
                                          keyclass:(CKKSKeyClass*)keyclass
                                             error:(NSError* __autoreleasing*)error;

// Creates a new random key that wraps itself
+ (instancetype _Nullable)randomKeyWrappedBySelf:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

/* Helper functions for persisting key material in the keychain */
- (BOOL)saveKeyMaterialToKeychain:(NSError* __autoreleasing*)error;
- (BOOL)saveKeyMaterialToKeychain:(bool)stashTLK
                            error:(NSError* __autoreleasing*)error;  // call this to not stash a non-syncable TLK, if that's what you want

- (BOOL)loadKeyMaterialFromKeychain:(NSError* __autoreleasing*)error;
- (BOOL)deleteKeyMaterialFromKeychain:(NSError* __autoreleasing*)error;
+ (NSString* _Nullable)isItemKeyForKeychainView:(SecDbItemRef)item;

+ (instancetype _Nullable)keyFromKeychain:(NSString*)uuid
                            parentKeyUUID:(NSString*)parentKeyUUID
                                 keyclass:(CKKSKeyClass*)keyclass
                                    state:(CKKSProcessedState*)state
                                   zoneID:(CKRecordZoneID*)zoneID
                          encodedCKRecord:(NSData* _Nullable)encodedrecord
                               currentkey:(NSInteger)currentkey
                                    error:(NSError* __autoreleasing*)error;


// Returns false if this key is not a valid TLK for any reason.
- (BOOL)validTLK:(NSError**)error;

// First, attempts to load the key from the keychain. If it isn't present, this will
// load the TLKShares for this key from the database, then attempts to use them to unwrap this key.
// If no TLKShares are trusted, returns an error.
- (BOOL)tlkMaterialPresentOrRecoverableViaTLKShare:(NSArray<CKKSPeerProviderState*>*)trustStates
                                             error:(NSError**)error;

+ (instancetype _Nullable)fromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype _Nullable)fromDatabaseAnyState:(NSString*)uuid
                                        zoneID:(CKRecordZoneID*)zoneID
                                         error:(NSError* __autoreleasing*)error;
+ (instancetype _Nullable)tryFromDatabase:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (instancetype _Nullable)tryFromDatabaseAnyState:(NSString*)uuid zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (NSArray<CKKSKey*>* _Nullable)selfWrappedKeys:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (instancetype _Nullable)currentKeyForClass:(CKKSKeyClass*)keyclass zoneID:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSArray<CKKSKey*>* _Nullable)currentKeysForClass:(CKKSKeyClass*)keyclass
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
                          encodedCKRecord:(NSData* _Nullable)encodedrecord
                               currentkey:(NSInteger)currentkey;

- (instancetype)initWrappedBy:(CKKSKey*)wrappingKey
                       AESKey:(CKKSAESSIVKey*)aeskey
                         uuid:(NSString*)uuid
                     keyclass:(CKKSKeyClass*)keyclass
                        state:(CKKSProcessedState*)state
                       zoneID:(CKRecordZoneID*)zoneID
              encodedCKRecord:(NSData* _Nullable)encodedrecord
                   currentkey:(NSInteger)currentkey;

- (instancetype)initWithWrappedAESKey:(CKKSWrappedAESSIVKey* _Nullable)wrappedaeskey
                                 uuid:(NSString*)uuid
                        parentKeyUUID:(NSString*)parentKeyUUID
                             keyclass:(CKKSKeyClass*)keyclass
                                state:(CKKSProcessedState*)state
                               zoneID:(CKRecordZoneID*)zoneID
                      encodedCKRecord:(NSData* _Nullable)encodedrecord
                           currentkey:(NSInteger)currentkey;

- (instancetype)initWithKeyCore:(CKKSKeychainBackedKey*)core;

/* Returns true if we believe this key wraps itself. */
- (bool)wrapsSelf;

- (CKKSKey* _Nullable)topKeyInAnyState:(NSError* __autoreleasing*)error;

// Attempts checks if the AES key is already loaded, or attempts to load it from the keychain. Returns false if it fails.
- (CKKSAESSIVKey* _Nullable)ensureKeyLoaded:(NSError* __autoreleasing*)error;

// Attempts to unwrap this key via unwrapping its wrapping keys via the key hierarchy.
- (CKKSAESSIVKey* _Nullable)unwrapViaKeyHierarchy:(NSError* __autoreleasing*)error;

// On a self-wrapped key, determine if this AES-SIV key is the self-wrapped key.
// If it is, save the key as this CKKSKey's unwrapped key.
- (bool)trySelfWrappedKeyCandidate:(CKKSAESSIVKey*)candidate error:(NSError* __autoreleasing*)error;

- (CKKSWrappedAESSIVKey*)wrapAESKey:(CKKSAESSIVKey*)keyToWrap error:(NSError* __autoreleasing*)error;
- (CKKSAESSIVKey*)unwrapAESKey:(CKKSWrappedAESSIVKey*)keyToUnwrap error:(NSError* __autoreleasing*)error;

- (bool)wrapUnder:(CKKSKey*)wrappingKey error:(NSError* __autoreleasing*)error;

- (NSData* _Nullable)encryptData:(NSData*)plaintext
               authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                           error:(NSError* __autoreleasing*)error;
- (NSData* _Nullable)decryptData:(NSData*)ciphertext
               authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                           error:(NSError* __autoreleasing*)error;

- (NSData* _Nullable)serializeAsProtobuf:(NSError* __autoreleasing*)error;
+ (CKKSKey* _Nullable)loadFromProtobuf:(NSData*)data error:(NSError* __autoreleasing*)error;

+ (NSNumber* _Nullable)counts:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;
+ (NSDictionary<NSString*, NSNumber*>*)countsByClass:(CKRecordZoneID*)zoneID error:(NSError* __autoreleasing*)error;

+ (BOOL)intransactionRecordChanged:(CKRecord*)record
                            resync:(BOOL)resync
                       flagHandler:(id<OctagonStateFlagHandler>)flagHandler
                             error:(NSError**)error;
@end

NS_ASSUME_NONNULL_END

#endif
