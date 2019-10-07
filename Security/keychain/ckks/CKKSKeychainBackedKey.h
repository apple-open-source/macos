/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSRecordHolder.h"
#import "keychain/ckks/CKKSSIV.h"
#import "keychain/ckks/proto/generated_source/CKKSSerializedKey.h"

NS_ASSUME_NONNULL_BEGIN

// Important note: while this class does conform to NSSecureCoding,
// for safety reasons encoding a CKKSKeychainBackedKey will ~not~
// encode the aessivkey. If you want your receiver to have access
// to the original key material, they must successfully call
// loadKeyMaterialFromKeychain.

@interface CKKSKeychainBackedKey : NSObject <NSCopying, NSSecureCoding>
@property NSString* uuid;
@property NSString* parentKeyUUID;
@property CKKSKeyClass* keyclass;
@property CKRecordZoneID* zoneID;

// Actual key material
@property CKKSWrappedAESSIVKey* wrappedkey;
@property (nullable) CKKSAESSIVKey* aessivkey;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype _Nullable)initSelfWrappedWithAESKey:(CKKSAESSIVKey*)aeskey
                                               uuid:(NSString*)uuid
                                           keyclass:(CKKSKeyClass*)keyclass
                                             zoneID:(CKRecordZoneID*)zoneID;

- (instancetype _Nullable)initWrappedBy:(CKKSKeychainBackedKey*)wrappingKey
                                 AESKey:(CKKSAESSIVKey*)aessivkey
                                   uuid:(NSString*)uuid
                               keyclass:(CKKSKeyClass*)keyclass
                                 zoneID:(CKRecordZoneID*)zoneID;

- (instancetype)initWithWrappedAESKey:(CKKSWrappedAESSIVKey* _Nullable)wrappedaeskey
                                 uuid:(NSString*)uuid
                        parentKeyUUID:(NSString*)parentKeyUUID
                             keyclass:(CKKSKeyClass*)keyclass
                               zoneID:(CKRecordZoneID*)zoneID;

// Creates new random keys, in the parent's zone
+ (instancetype _Nullable)randomKeyWrappedByParent:(CKKSKeychainBackedKey*)parentKey
                                             error:(NSError* __autoreleasing*)error;

+ (instancetype _Nullable)randomKeyWrappedByParent:(CKKSKeychainBackedKey*)parentKey
                                          keyclass:(CKKSKeyClass*)keyclass
                                             error:(NSError* __autoreleasing*)error;

// Creates a new random key that wraps itself
+ (instancetype _Nullable)randomKeyWrappedBySelf:(CKRecordZoneID*)zoneID
                                           error:(NSError* __autoreleasing*)error;

/* Helper functions for persisting key material in the keychain */
- (BOOL)saveKeyMaterialToKeychain:(NSError* __autoreleasing*)error;
- (BOOL)saveKeyMaterialToKeychain:(bool)stashTLK
                            error:(NSError* __autoreleasing*)error;  // call this to not stash a non-syncable TLK, if that's what you want

- (BOOL)loadKeyMaterialFromKeychain:(NSError* __autoreleasing*)error;
- (BOOL)deleteKeyMaterialFromKeychain:(NSError* __autoreleasing*)error;

// Class methods to help tests
+ (NSDictionary* _Nullable)setKeyMaterialInKeychain:(NSDictionary*)query
                                              error:(NSError* __autoreleasing*)error;

+ (NSDictionary* _Nullable)queryKeyMaterialInKeychain:(NSDictionary*)query
                                                error:(NSError* __autoreleasing*)error;

+ (instancetype _Nullable)keyFromKeychain:(NSString*)uuid
                            parentKeyUUID:(NSString*)parentKeyUUID
                                 keyclass:(CKKSKeyClass*)keyclass
                                   zoneID:(CKRecordZoneID*)zoneID
                                    error:(NSError* __autoreleasing*)error;

/* Returns true if we believe this key wraps itself. */
- (bool)wrapsSelf;

// Attempts checks if the AES key is already loaded, or attempts to load it from the keychain. Returns nil if it fails.
- (CKKSAESSIVKey* _Nullable)ensureKeyLoaded:(NSError* __autoreleasing*)error;

// On a self-wrapped key, determine if this AES-SIV key is the self-wrapped key.
// If it is, save the key as this CKKSKey's unwrapped key.
- (bool)trySelfWrappedKeyCandidate:(CKKSAESSIVKey*)candidate
                             error:(NSError* __autoreleasing*)error;

- (CKKSWrappedAESSIVKey* _Nullable)wrapAESKey:(CKKSAESSIVKey*)keyToWrap
                                        error:(NSError* __autoreleasing*)error;

- (CKKSAESSIVKey* _Nullable)unwrapAESKey:(CKKSWrappedAESSIVKey*)keyToUnwrap
                                   error:(NSError* __autoreleasing*)error;

- (bool)wrapUnder:(CKKSKeychainBackedKey*)wrappingKey
            error:(NSError* __autoreleasing*)error;

- (bool)unwrapSelfWithAESKey:(CKKSAESSIVKey*)unwrappingKey
                       error:(NSError* __autoreleasing*)error;

- (NSData* _Nullable)encryptData:(NSData*)plaintext
               authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                           error:(NSError* __autoreleasing*)error;

- (NSData* _Nullable)decryptData:(NSData*)ciphertext
               authenticatedData:(NSDictionary<NSString*, NSData*>* _Nullable)ad
                           error:(NSError* __autoreleasing*)error;

- (NSData* _Nullable)serializeAsProtobuf:(NSError* __autoreleasing*)error;

+ (CKKSKeychainBackedKey* _Nullable)loadFromProtobuf:(NSData*)data
                                               error:(NSError* __autoreleasing*)error;
@end

// Useful when sending keys across interface boundaries
@interface CKKSKeychainBackedKeySet : NSObject <NSSecureCoding>
@property CKKSKeychainBackedKey* tlk;
@property CKKSKeychainBackedKey* classA;
@property CKKSKeychainBackedKey* classC;
@property BOOL newUpload;

- (instancetype)initWithTLK:(CKKSKeychainBackedKey*)tlk
                     classA:(CKKSKeychainBackedKey*)classA
                     classC:(CKKSKeychainBackedKey*)classC
                  newUpload:(BOOL)newUpload;
@end


NS_ASSUME_NONNULL_END

#endif
