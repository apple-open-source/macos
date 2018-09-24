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

#import "SecKeybagSupport.h"

#if !TARGET_OS_BRIDGE

#if USE_KEYSTORE
#import <libaks.h>
#import <libaks_ref_key.h>
#endif

#import <Foundation/Foundation.h>
#import <CoreData/CoreData.h>
#import <SecurityFoundation/APIMacros.h>

@class SecCDKeychainItemMetadata;
@class SecCDKeychainLookupTuple;
@class SecCDKeychainManagedItemType;
@class SecCDKeychainAccessControlEntity;
@class SFKeychainServerConnection;
@class SFAESKey;

NS_ASSUME_NONNULL_BEGIN

@class SecCDKeychainItem;

@protocol SecCDKeychainLookupValueType <NSObject>
@end
typedef NSString<SecCDKeychainLookupValueType> SecCDKeychainLookupValueType;

extern SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeString;
extern SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeData;
extern SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeNumber;
extern SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeDate;
extern SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeArray;
extern SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeDictionary;

@interface SecCDKeychain : NSObject

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithStorageURL:(NSURL*)persistentStoreURL modelURL:(NSURL*)managedObjectURL encryptDatabase:(bool)encryptDatabase;

- (void)insertItems:(NSArray<SecCDKeychainItem*>*)items withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(bool success, NSError* _Nullable error))completionHandler;

- (void)fetchItemForPersistentID:(NSUUID*)persistentID withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(SecCDKeychainItem* _Nullable item, NSError* _Nullable error))completionHandler;
- (void)fetchItemsWithValue:(NSString*)value forLookupKey:(NSString*)lookupKey ofType:(SecCDKeychainLookupValueType*)lookupValueType withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error))completionHandler;

- (void)deleteItemWithPersistentID:(NSUUID*)persistentID withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(bool success, NSError* _Nullable error))completionHandler;

@end

@interface SecCDKeychainItemType : NSObject

@property (readonly, copy) NSString* name;
@property (readonly) int32_t version;

// for both primaryKeys and syncableKeys, nil means "all the attributes"
@property (readonly, copy, nullable) NSArray* primaryKeys;
@property (readonly, copy, nullable) NSArray* syncableKeys;

@property (readonly) SecCDKeychainManagedItemType* managedItemType;

// subclasses must override
+ (nullable instancetype)itemType;
+ (nullable instancetype)itemTypeForVersion:(int32_t)version;

// to be called only by subclass implementations of +itemType
- (instancetype)_initWithName:(NSString*)name version:(int32_t)version primaryKeys:(nullable NSArray*)primaryKeys syncableKeys:(nullable NSArray*)syncableKeys;

@end

@interface SecCDKeychainItemMetadata : NSObject

@property (readonly) SecCDKeychainItemType* itemType;
@property (readonly) SecCDKeychainAccessControlEntity* owner;
@property (readonly) NSUUID* persistentID;
@property (readonly, copy) NSDictionary* attributes;
@property (readonly, copy) NSArray<SecCDKeychainLookupTuple*>* lookupAttributes;
@property (readonly) keyclass_t keyclass;

- (instancetype)init NS_UNAVAILABLE;
- (void)fetchFullItemWithKeychain:(SecCDKeychain*)keychain withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(SecCDKeychainItem* _Nullable item, NSError* _Nullable error))completionHandler;

@end

@interface SecCDKeychainItem : NSObject

@property (readonly) SecCDKeychainItemType* itemType;
@property (readonly) SecCDKeychainAccessControlEntity* owner;
@property (readonly) NSUUID* persistentID;
@property (readonly) NSDictionary* attributes;
@property (readonly) NSArray<SecCDKeychainLookupTuple*>* lookupAttributes;
@property (readonly) keyclass_t keyclass;
@property (readonly) NSDictionary* secrets;

@property (readonly) SecCDKeychainItemMetadata* metadata;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initItemType:(SecCDKeychainItemType*)itemType withPersistentID:(NSUUID*)persistentID attributes:(NSDictionary*)attributes lookupAttributes:(nullable NSArray<SecCDKeychainLookupTuple*>*)lookupAttributes secrets:(NSDictionary*)secrets owner:(SecCDKeychainAccessControlEntity*)owner keyclass:(keyclass_t)keyclass;

@end

@interface SecCDKeychainLookupTuple : NSObject

@property (readonly, copy) NSString* key;
@property (readonly, copy) id<NSCopying, NSObject> value;
@property (readonly, copy) SecCDKeychainLookupValueType* valueType;
@property (readonly, copy) NSString* stringRepresentation;

+ (instancetype)lookupTupleWithKey:(NSString*)key value:(id<NSCopying, NSObject>)value;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithKey:(NSString*)key value:(id<NSCopying, NSObject>)value;

@end

typedef NS_ENUM(NSInteger, SecCDKeychainAccessControlEntityType) {
    SecCDKeychainAccessControlEntityTypeAccessGroup = 0,
};

@interface SecCDKeychainAccessControlEntity : NSObject

@property (nonatomic, readonly) SecCDKeychainAccessControlEntityType entityType;
@property (nonatomic, readonly) NSString* stringRepresentation;

+ (instancetype)accessControlEntityWithType:(SecCDKeychainAccessControlEntityType)type stringRepresentation:(NSString*)stringRepresentation;

- (instancetype)init NS_UNAVAILABLE;

@end

#if USE_KEYSTORE

@protocol SecAKSRefKey <NSObject>

@property (readonly) NSData* refKeyBlob;

- (instancetype)initWithKeybag:(keybag_handle_t)keybag keyclass:(keyclass_t)keyclass;
- (instancetype)initWithBlob:(NSData*)blob keybag:(keybag_handle_t)keybag;

- (nullable NSData*)wrappedDataForKey:(SFAESKey*)key;
- (nullable SFAESKey*)keyWithWrappedData:(NSData*)wrappedKeyData;

@end

@interface SecAKSRefKey : NSObject <SecAKSRefKey>
@end

#endif // USE_KEYSTORE

NS_ASSUME_NONNULL_END

#endif // !TARGET_OS_BRIDGE
