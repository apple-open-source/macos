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

#import <TargetConditionals.h>

#if !TARGET_OS_BRIDGE

#import "SecCDKeychain.h"
#import "SecCDKeychainManagedItem+CoreDataClass.h"
#import "SecCDKeychainManagedLookupEntry+CoreDataClass.h"
#import "SecCDKeychainManagedItemType+CoreDataClass.h"
#import "SecCDKeychainManagedAccessControlEntity+CoreDataClass.h"
#import "SecFileLocations.h"
#import "SecItemServer.h"
#import "SecItem.h"
#import "SecItemPriv.h"
#import <Security/SecBase.h>
#import "SFKeychainServer.h"
#import "CloudKitCategories.h"
#import "securityd_client.h"
#import "SecKeybagSupport.h"
#import "SecKeybagSupport.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import <SecurityFoundation/SFKeychain.h>
#import <SecurityFoundation/SFDigestOperation.h>
#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <CoreData/NSPersistentStoreCoordinator_Private.h>
#if USE_KEYSTORE
#if __has_include(<libaks_ref_key.h>)
#import <libaks_ref_key.h>
#endif
#endif
#import <Foundation/NSData_Private.h>
#import <notify.h>

static NSString* const SecCDKeychainErrorDomain = @"com.apple.security.cdkeychain";

static NSString* const SecCDKeychainEntityLookupEntry = @"LookupEntry";
static NSString* const SecCDKeychainEntityItem = @"Item";
static NSString* const SecCDKeychainEntityItemType = @"ItemType";
static NSString* const SecCDKeychainEntityTypeAccessControlEntity = @"AccessControlEntity";

static NSString* const SecCDKeychainItemMetadataSHA256 = @"SecCDKeychainItemMetadataSHA256";

static const NSInteger SecCDKeychainErrorDeserializing = 1;
static const NSInteger SecCDKeychainErrorInternal = 2;
//static const NSInteger SecCDKeychainErrorMetadataDoesNotMatch = 3;

SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeString = (SecCDKeychainLookupValueType*)@"SecCDKeychainLookupValueTypeString";
SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeData = (SecCDKeychainLookupValueType*)@"SecCDKeychainLookupValueTypeData";
SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeNumber = (SecCDKeychainLookupValueType*)@"SecCDKeychainLookupValueTypeNumber";
SecCDKeychainLookupValueType* const SecCDKeychainLookupValueTypeDate = (SecCDKeychainLookupValueType*)@"SecCDKeychainLookupValueTypeDate";

@interface SecCDKeychainItem ()

- (instancetype)initWithManagedItem:(SecCDKeychainManagedItem*)managedItem keychain:(SecCDKeychain*)keychain error:(NSError**)error;

- (NSString*)primaryKeyStringRepresentationWithError:(NSError**)error;
- (NSData*)encryptedSecretDataWithAttributeData:(NSData*)attributeData keybag:(keybag_handle_t)keybag error:(NSError**)error;

@end

@interface SecCDKeychainItemMetadata ()

@property (readonly, copy) NSSet<SecCDKeychainLookupTuple*>* lookupAttributesSet;
@property (readonly, copy) NSData* managedDataBlob;

- (instancetype)initWithItemType:(SecCDKeychainItemType*)itemType persistentID:(NSUUID*)persistentID attributes:(NSDictionary*)attributes lookupAttributes:(NSArray<SecCDKeychainLookupTuple*>*)lookupAttributes owner:(SecCDKeychainAccessControlEntity*)owner keyclass:(keyclass_t)keyclass;
- (instancetype)initWithManagedItem:(SecCDKeychainManagedItem*)managedItem keychain:(SecCDKeychain*)keychain error:(NSError**)error;

@end

@interface SecCDKeychainLookupTuple ()

+ (instancetype)lookupTupleWithManagedLookupEntry:(SecCDKeychainManagedLookupEntry*)lookupEntry;

@end

@interface SecCDKeychainItemType ()

- (SecCDKeychainManagedItemType*)managedItemTypeWithContext:(NSManagedObjectContext*)managedObjectContext error:(NSError**)error;

@end

@interface SecCDKeychainAccessControlEntity ()

- (instancetype)initWithManagedEntity:(SecCDKeychainManagedAccessControlEntity*)managedAccessControlEntity;

@end

@interface SecCDKeychainItemWrappedSecretData : NSObject <NSSecureCoding>

@property (readonly) SFAuthenticatedCiphertext* ciphertext;
@property (readonly) NSData* wrappedKeyData;
@property (readonly) NSData* refKeyBlob;

- (instancetype)init NS_UNAVAILABLE;
- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKeyData:(NSData*)wrappedKeyData refKeyBlob:(NSData*)refKeyBlob;

@end

@implementation SecCDKeychainItemWrappedSecretData {
    SFAuthenticatedCiphertext* _ciphertext;
    NSData* _wrappedKeyData;
    NSData* _refKeyBlob;
}

@synthesize ciphertext = _ciphertext;
@synthesize wrappedKeyData = _wrappedKeyData;
@synthesize refKeyBlob = _refKeyBlob;

+ (BOOL)supportsSecureCoding
{
    return YES;
}

- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKeyData:(NSData*)wrappedKeyData refKeyBlob:(NSData*)refKeyBlob
{
    if (self = [super init]) {
        _ciphertext = ciphertext;
        _wrappedKeyData = wrappedKeyData.copy;
        _refKeyBlob = refKeyBlob.copy;
    }

    return self;
}

- (instancetype)initWithCoder:(NSCoder*)coder
{
    if (self = [super init]) {
        _ciphertext = [coder decodeObjectOfClass:[SFAuthenticatedCiphertext class] forKey:@"SecCDKeychainItemCiphertext"];
        _wrappedKeyData = [coder decodeObjectOfClass:[NSData class] forKey:@"SecCDKeychainItemWrappedKey"];
        _refKeyBlob = [coder decodeObjectOfClass:[NSData class] forKey:@"SecCDKeychainItemRefKeyBlob"];

        if (!_ciphertext || !_wrappedKeyData || !_refKeyBlob) {
            self = nil;
            secerror("SecCDKeychain: failed to deserialize wrapped secret data");
            [coder failWithError:[NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorDeserializing userInfo:@{NSLocalizedDescriptionKey : @"failed to deserialize wrapped secret data"}]];
        }
    }

    return self;
}

- (void)encodeWithCoder:(NSCoder*)coder
{
    [coder encodeObject:_ciphertext forKey:@"SecCDKeychainItemCiphertext"];
    [coder encodeObject:_wrappedKeyData forKey:@"SecCDKeychainItemWrappedKey"];
    [coder encodeObject:_refKeyBlob forKey:@"SecCDKeychainItemRefKeyBlob"];
}

@end

#if USE_KEYSTORE

@implementation SecAKSRefKey {
    aks_ref_key_t _refKey;
}

- (instancetype)initWithKeybag:(keyclass_t)keybag keyclass:(keyclass_t)keyclass
{
    if (self = [super init]) {
        if (aks_ref_key_create(keybag, keyclass, key_type_sym, NULL, 0, &_refKey) != kAKSReturnSuccess) {
            self = nil;
        }
    }

    return self;
}

- (instancetype)initWithBlob:(NSData*)blob keybag:(keybag_handle_t)keybag
{
    if (self = [super init]) {
        if (aks_ref_key_create_with_blob(keybag, blob.bytes, blob.length, &_refKey) != kAKSReturnSuccess) {
            self = nil;
        }
    }

    return self;
}

- (void)dealloc
{
    aks_ref_key_free(&_refKey);
}

- (NSData*)wrappedDataForKey:(SFAESKey*)key
{
    void* wrappedKeyBytes = NULL;
    size_t wrappedKeyLength = 0;
    if (aks_ref_key_wrap(_refKey, NULL, 0, key.keyData.bytes, key.keyData.length, &wrappedKeyBytes, &wrappedKeyLength) == kAKSReturnSuccess) {
        return [NSData dataWithBytesNoCopy:wrappedKeyBytes length:wrappedKeyLength];
    }

    return nil;
}

- (SFAESKey*)keyWithWrappedData:(NSData*)wrappedKeyData
{
    void* unwrappedKeyBytes = NULL;
    size_t unwrappedKeyLength = 0;
    int aksResult = aks_ref_key_unwrap(_refKey, NULL, 0, wrappedKeyData.bytes, wrappedKeyData.length, &unwrappedKeyBytes, &unwrappedKeyLength);
    if (aksResult != kAKSReturnSuccess || !unwrappedKeyBytes) {
        return nil;
    }

    NSData* keyData = [NSData dataWithBytesNoCopy:unwrappedKeyBytes length:unwrappedKeyLength];
    NSError* error = nil;
    SFAESKey* key = [[SFAESKey alloc] initWithData:keyData specifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256] error:&error];
    if (!key) {
        secerror("SecCDKeychain: error creating AES key from unwrapped item key data with error: %@", error);
    }

    return key;
}

- (NSData*)refKeyBlob
{
    size_t refKeyBlobLength = 0;
    const uint8_t* refKeyBlobBytes = aks_ref_key_get_blob(_refKey, &refKeyBlobLength);
    return [NSData dataWithBytes:refKeyBlobBytes length:refKeyBlobLength];
}

@end

#endif // USE_KEYSTORE

@implementation SecCDKeychain {
    NSURL* _persistentStoreBaseURL;
    NSPersistentStoreCoordinator* _persistentStoreCoordinator;
    NSManagedObjectContext* _managedObjectContext;
    NSMutableDictionary* _managedItemTypeDict;
    NSMutableDictionary* _itemTypeDict;
    bool _encryptDatabase;
    dispatch_queue_t _queue;
    NSArray* _classAPersistentStores;
}

+ (SecCDKeychainLookupValueType*)lookupValueTypeForObject:(id)object
{
    if ([object isKindOfClass:[NSString class]]) {
        return SecCDKeychainLookupValueTypeString;
    }
    else if ([object isKindOfClass:[NSData class]]) {
        return SecCDKeychainLookupValueTypeData;
    }
    else if ([object isKindOfClass:[NSDate class]]) {
        return SecCDKeychainLookupValueTypeDate;
    }
    else if ([object isKindOfClass:[NSNumber class]]) {
        return SecCDKeychainLookupValueTypeNumber;
    }
    else {
        return nil;
    }
}

- (instancetype)initWithStorageURL:(NSURL*)persistentStoreURL modelURL:(NSURL*)managedObjectURL encryptDatabase:(bool)encryptDatabase
{
    if (!persistentStoreURL) {
        secerror("SecCDKeychain: no persistent store URL, so we can't create or open a database");
        return nil;
    }
    if (!managedObjectURL) {
        secerror("SecCDKeychain: no managed object model URL, so we can't create or open a database");
        return nil;
    }
    
    if (self = [super init]) {
        _queue = dispatch_queue_create("SecCDKeychain", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        _persistentStoreBaseURL = persistentStoreURL.copy;
        _encryptDatabase = encryptDatabase;

        NSManagedObjectModel* managedObjectModel = [[NSManagedObjectModel alloc] initWithContentsOfURL:managedObjectURL];
        _persistentStoreCoordinator = [[NSPersistentStoreCoordinator alloc] initWithManagedObjectModel:managedObjectModel];
    }

    return self;
}

- (NSData*)_onQueueGetDatabaseKeyDataWithError:(NSError**)error
{
    dispatch_assert_queue(_queue);
    NSData* keyData = nil;
    NSDictionary* databaseKeyQuery = @{ (id)kSecClass : (id)kSecClassGenericPassword,
                                        (id)kSecAttrAccessGroup : @"com.apple.security.securityd",
                                        (id)kSecAttrAccessible : (id)kSecAttrAccessibleWhenUnlocked,
                                        (id)kSecUseDataProtectionKeychain : @(YES),
                                        (id)kSecAttrService : @"com.apple.security.keychain.ak",
                                        (id)kSecReturnData : @(YES) };

    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)databaseKeyQuery, &result);

    if (status == errSecItemNotFound) {
        NSMutableDictionary* addKeyQuery = databaseKeyQuery.mutableCopy;
        [addKeyQuery removeObjectForKey:(id)kSecReturnData];

        uint8_t* keyBytes = malloc(16);
        if (SecRandomCopyBytes(NULL, 16, keyBytes) != 0) {
            secerror("SecCDKeychain: failed to create random key for CD database encryption - this means we won't be able to create a database");
            if (error) {
                *error = [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : @"failed to create random key for CD database encryption"}];
            }
            return nil;
        }

        keyData = [NSData _newZeroingDataWithBytesNoCopy:keyBytes length:16 deallocator:nil];
        addKeyQuery[(id)kSecValueData] = keyData;
        status = SecItemAdd((__bridge CFDictionaryRef)addKeyQuery, NULL);
        if (status == errSecSuccess) {
            return keyData;
        }
        else {
            secerror("SecCDKeychain: failed to save encryption key to keychain, so bailing on database creation; status: %d", (int)status);
            CFErrorRef cfError = NULL;
            SecError(status, &cfError, CFSTR("failed to save encryption key to keychain, so bailing on database creation"));
            if (error) {
                *error = CFBridgingRelease(cfError);
                cfError = NULL;
            }
            else {
                CFReleaseNull(cfError);
            }
            return nil;
        }
    }
    else if (status == errSecInteractionNotAllowed) {
        //// <rdar://problem/38972671> add SFKeychainErrorDeviceLocked

        secerror("SecCDKeychain: can't create a class A store right now because the keychain is locked");
        CFErrorRef cfError = NULL;
        SecError(status, &cfError, CFSTR("can't create a class A store right now because the keychain is locked"));
        if (error) {
            *error = CFBridgingRelease(cfError);
            cfError = NULL;
        }
        else {
            CFReleaseNull(cfError);
        }
        return nil;
    }
    else if (status == errSecSuccess) {
        if ([(__bridge id)result isKindOfClass:[NSData class]]) {
            return (__bridge_transfer NSData*)result;
        }
        else {
            if (error) {
                *error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"result of keychain query for database key is wrong kind of class: %@", [(__bridge id)result class]]}];
            }
            
            CFReleaseNull(result);
            return nil;
        }
    }
    else {
        secerror("failed to save or retrieve key for CD database encryption - this means we won't be able to create a database; status: %d", (int)status);
        if (error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain code:status userInfo:@{NSLocalizedDescriptionKey : @"failed to save or retrieve key for CD database encryption"}];
        }
        
        CFReleaseNull(result);
        return nil;
    }
}

- (NSManagedObjectContext*)_onQueueGetManagedObjectContextWithError:(NSError* __autoreleasing *)error
{
    dispatch_assert_queue(_queue);

    if (!_managedObjectContext) {
        NSURL* akStoreURL = [_persistentStoreBaseURL URLByAppendingPathExtension:@"ak"];

        NSData* keyData = [self _onQueueGetDatabaseKeyDataWithError:error];
        if (!keyData) {
            return nil;
        }

        if (_encryptDatabase) {
            NSPersistentStore* classAStore = [_persistentStoreCoordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:akStoreURL options:@{NSSQLiteSEEKeychainItemOption : keyData} error:error];
            _classAPersistentStores = @[classAStore];
        }
        else {
            NSPersistentStore* classAStore = [_persistentStoreCoordinator addPersistentStoreWithType:NSSQLiteStoreType configuration:nil URL:akStoreURL options:nil error:error];
            _classAPersistentStores = @[classAStore];
        }

        NSManagedObjectContext* managedObjectContext = [[NSManagedObjectContext alloc] initWithConcurrencyType:NSPrivateQueueConcurrencyType];
        managedObjectContext.persistentStoreCoordinator = _persistentStoreCoordinator;

        // the first time around, setup our items types; grab old ones from the database and register any new ones
        // we can skip for subsequent loads of the store in the same run of securityd
        if (!_managedItemTypeDict || !_itemTypeDict) {
            _managedItemTypeDict = [NSMutableDictionary dictionary];
            _itemTypeDict = [NSMutableDictionary dictionary];
            [managedObjectContext performBlockAndWait:^{
                NSFetchRequest* managedItemTypeFetchRequest = [SecCDKeychainManagedItemType fetchRequest];
                NSArray<SecCDKeychainManagedItemType*>* managedItemTypes = [managedObjectContext executeFetchRequest:managedItemTypeFetchRequest error:error];
                if (managedItemTypes.count == 0) {
                    // TODO do some error handling here
                }
                else {
                    for (SecCDKeychainManagedItemType* managedItemType in managedItemTypes) {
                        NSMutableDictionary* itemTypeVersionDict = self->_managedItemTypeDict[managedItemType.name];
                        if (!itemTypeVersionDict) {
                            itemTypeVersionDict = [NSMutableDictionary dictionary];
                            self->_managedItemTypeDict[managedItemType.name] = itemTypeVersionDict;
                        }
                        itemTypeVersionDict[@(managedItemType.version)] = managedItemType;
                    }
                }

                [self registerItemType:[SecCDKeychainItemTypeCredential itemType] withManagedObjectContext:managedObjectContext];
            }];
        }

        _managedObjectContext = managedObjectContext;

        int token = 0;
        __weak __typeof(self) weakSelf = self;
        notify_register_dispatch(kUserKeybagStateChangeNotification, &token, _queue, ^(int t) {
            bool locked = true;
            CFErrorRef blockError = NULL;
            if (!SecAKSGetIsLocked(&locked, &blockError)) {
                secerror("SecDbKeychainMetadataKeyStore: error getting lock state: %@", blockError);
                CFReleaseNull(blockError);
            }

            if (locked) {
                [weakSelf _onQueueDropClassAPersistentStore];
            }
        });
    }

    return _managedObjectContext;
}

- (void)_onQueueDropClassAPersistentStore
{
    dispatch_assert_queue(_queue);
    for (NSPersistentStore* store in _classAPersistentStores) {
        NSError* error = nil;
        if (![_persistentStoreCoordinator removePersistentStore:store error:&error]) {
            secerror("SecCDKeychain: failed to remove persistent store with error: %@", error);
        }
    }

    _classAPersistentStores = nil;
    _managedObjectContext = nil;
    _persistentStoreCoordinator = nil;
}

- (dispatch_queue_t)_queue
{
    return _queue;
}

- (void)performOnManagedObjectQueue:(void (^)(NSManagedObjectContext* context, NSError* error))block
{
    __weak __typeof(self) weakSelf = self;
    dispatch_async(_queue, ^{
        NSError* error = nil;
        NSManagedObjectContext* context = [weakSelf _onQueueGetManagedObjectContextWithError:&error];
        if (context) {
            [context performBlock:^{
                block(context, nil);
            }];
        }
        else {
            if (!error) {
                error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal description:@"unknown error retrieving managed object context"];
            }
            block(nil, error);
        }
    });
}

- (void)performOnManagedObjectQueueAndWait:(void (^)(NSManagedObjectContext* context, NSError* error))block
{
    dispatch_sync(_queue, ^{
        NSError* error = nil;
        NSManagedObjectContext* context = [self _onQueueGetManagedObjectContextWithError:&error];
        if (context) {
            [context performBlockAndWait:^{
                block(context, nil);
            }];
        }
        else {
            block(nil, error);
        }
    });
}

- (NSString*)primaryKeyNameForItemTypeName:(NSString*)itemTypeName
{
    return [NSString stringWithFormat:@"SecCDKeychain-PrimaryKey-%@", itemTypeName];
}

- (bool)validateItemOwner:(SecCDKeychainAccessControlEntity*)owner withConnection:(SFKeychainServerConnection*)connection withError:(NSError**)error
{
    // in the future we may support more owner types than just an access group, but for now, access group or bust

    NSError* localError = nil;
    NSString* accessGroup = owner.entityType == SecCDKeychainAccessControlEntityTypeAccessGroup ? owner.stringRepresentation : nil;
    if (accessGroup) {
        if ([connection.clientAccessGroups containsObject:accessGroup]) {
            return true;
        }
        else {
            localError = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInvalidAccessGroup userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"client not in access group: %@", accessGroup]}];
        }
    }
    else {
        localError = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorMissingAccessGroup userInfo:@{NSLocalizedDescriptionKey : @"keychain item missing access group"}];
    }

    if (error) {
        *error = localError;
    }
    secerror("SecCDKeychain: failed to validate item owner with error: %@", localError);
    return false;
}

- (void)insertItems:(NSArray<SecCDKeychainItem*>*)items withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(bool success, NSError* error))completionHandler
{
    __weak __typeof(self) weakSelf = self;
    [self performOnManagedObjectQueue:^(NSManagedObjectContext* managedObjectContext, NSError* managedObjectError) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secerror("SecCDKeychain: attempt to insert items into deallocated keychain instance");
            completionHandler(false, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : @"attempt to insert items into deallocated keychain instance"}]);
            return;
        }

        if (!managedObjectContext) {
            secerror("SecCDKeychain: insertItems: could not get managed object context");
            completionHandler(false, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal userInfo:@{ NSLocalizedDescriptionKey : @"insertItems: could not get managed object context", NSUnderlyingErrorKey : managedObjectError }]);
            return;
        }

        __block NSError* error = nil;
        __block bool success = true;
        for (SecCDKeychainItem* item in items) {
            if (![self validateItemOwner:item.owner withConnection:connection withError:&error]) {
                success = false;
                break;
            }

            NSString* itemTypeName = item.itemType.name;
            NSString* primaryKeyValue = [item primaryKeyStringRepresentationWithError:&error];
            if (primaryKeyValue) {
                NSFetchRequest* primaryKeyFetchRequest = [SecCDKeychainManagedLookupEntry fetchRequest];
                primaryKeyFetchRequest.predicate = [NSPredicate predicateWithFormat:@"lookupKey == %@ AND lookupValue == %@", [self primaryKeyNameForItemTypeName:itemTypeName], primaryKeyValue];
                SecCDKeychainManagedLookupEntry* managedLookupEntry = [[managedObjectContext executeFetchRequest:primaryKeyFetchRequest error:&error] firstObject];
                if (managedLookupEntry) {
                    secerror("SecCDKeychain: failed to unique item (%@) using primary keys: %@", item, item.itemType.primaryKeys);
                    success = false;
                    error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorDuplicateItem userInfo:@{ NSLocalizedDescriptionKey : [NSString stringWithFormat:@"failed to unique item (%@) using primary keys: %@", item, item.itemType.primaryKeys] }];
                    break;
                }
            }
            else {
                secerror("SecCDKeychain: error creating primary key string representation for item: %@; error: %@", item, error);
                success = false;
                break;
            }

            SecCDKeychainManagedItem* managedItem = [self managedItemWithItem:item withManagedObjectContext:managedObjectContext error:&error];
            if (!managedItem) {
                secerror("SecCDKeychain: error creating managed item for insertion: %@; item: %@", error, item);
                success = false;
                break;
            }

            // add a lookup entry for the primary key
            SecCDKeychainManagedLookupEntry* primaryKeyLookupEntry = [NSEntityDescription insertNewObjectForEntityForName:SecCDKeychainEntityLookupEntry inManagedObjectContext:managedObjectContext];
            primaryKeyLookupEntry.itemTypeName = itemTypeName;
            primaryKeyLookupEntry.lookupKey = [self primaryKeyNameForItemTypeName:itemTypeName];
            primaryKeyLookupEntry.lookupValue = primaryKeyValue;
            primaryKeyLookupEntry.lookupValueType = SecCDKeychainLookupValueTypeString;
            primaryKeyLookupEntry.systemEntry = YES;
            [primaryKeyLookupEntry addMatchingItemsObject:managedItem];
            secdebug("SecCDKeychain", "added primary key: %@ value: %@", primaryKeyLookupEntry.lookupKey, primaryKeyLookupEntry.lookupValue);

            // and add lookup entries for all the things we're supposed to be able to lookup on
            for (SecCDKeychainLookupTuple* lookupTuple in item.lookupAttributes) {
                NSFetchRequest* lookupEntryFetchRequest = [SecCDKeychainManagedLookupEntry fetchRequest];
                lookupEntryFetchRequest.predicate = [NSPredicate predicateWithFormat:@"lookupKey == %@ AND lookupValueType == %@ AND lookupValue == %@", lookupTuple.key, lookupTuple.valueType, lookupTuple.value];
                SecCDKeychainManagedLookupEntry* lookupEntry = [[managedObjectContext executeFetchRequest:lookupEntryFetchRequest error:&error] firstObject];
                if (error) {
                    secerror("SecCDKeychain: error fetching lookup entry during item insertion: %@", (__bridge CFErrorRef)error);
                    success = false;
                    break;
                }
                else if (!lookupEntry) {
                    // we didn't get an error, but we didn't find a lookup entry
                    // that means there simply wasn't one in the db, so we should create one
                    lookupEntry = [NSEntityDescription insertNewObjectForEntityForName:SecCDKeychainEntityLookupEntry inManagedObjectContext:managedObjectContext];
                    lookupEntry.itemTypeName = itemTypeName;
                    lookupEntry.lookupKey = lookupTuple.key;
                    lookupEntry.lookupValue = lookupTuple.stringRepresentation;
                    lookupEntry.lookupValueType = lookupTuple.valueType;
                    secdebug("SecCDKeychain", "added item for key (%@) : value (%@)", lookupEntry.lookupKey, lookupEntry.lookupValue);
                }

                [lookupEntry addMatchingItemsObject:managedItem];
            }

            if (!success) {
                break;
            }
        }

        if (success) {
            secnotice("SecCDKeychain", "saving managed object context for items: %@", items);
            success = [managedObjectContext save:&error];
            if (error) {
                secerror("SecCDKeychain: saving managed object context failed with error: %@", error);
            }
            else {
                secnotice("SecCDKeychain", "saving managed object context succeeded");
            }
        }

        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            completionHandler(success, error);
        });
    }];
}

- (SecCDKeychainManagedItem*)fetchManagedItemForPersistentID:(NSUUID*)persistentID withManagedObjectContext:(NSManagedObjectContext*)managedObjectContext error:(NSError**)error
{
    NSError* localError = nil;
    NSFetchRequest* lookupEntryFetchRequest = [SecCDKeychainManagedItem fetchRequest];
    lookupEntryFetchRequest.predicate = [NSPredicate predicateWithFormat:@"persistentID == %@", persistentID];
    SecCDKeychainManagedItem* managedItem = [[managedObjectContext executeFetchRequest:lookupEntryFetchRequest error:&localError] firstObject];
    
    if (error) {
        *error = localError;
    }
    
    return managedItem;
}

- (void)fetchItemForPersistentID:(NSUUID*)persistentID withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(SecCDKeychainItem* item, NSError* error))completionHandler
{
    __weak __typeof(self) weakSelf = self;
    [self performOnManagedObjectQueue:^(NSManagedObjectContext* managedObjectContext, NSError* managedObjectError) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secerror("SecCDKeychain: attempt to fetch item from deallocated keychain instance");
            completionHandler(false, [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : @"attempt to fetch item from deallocated keychain instance"}]);
            return;
        }

        if (!managedObjectContext) {
            secerror("SecCDKeychain: fetchItemForPersistentID: could not get managed object context");
            completionHandler(false, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal userInfo:@{ NSLocalizedDescriptionKey : @"fetchItemForPersistentID: could not get managed object context", NSUnderlyingErrorKey : managedObjectError }]);
            return;
        }
        
        NSError* error = nil;
        SecCDKeychainItem* fetchedItem = nil;
        SecCDKeychainManagedItem* managedItem = [self fetchManagedItemForPersistentID:persistentID withManagedObjectContext:managedObjectContext error:&error];
        if (error) {
            secerror("SecCDKeychain: error fetching item for persistent id: %@; error: %@", persistentID, error);
        }
        else if (!managedItem) {
            // we didn't get an error, but we didn't find an item
            // that means there simply wasn't one in the db, so this lookup finds nothing
            error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorItemNotFound userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"did not find any keychain items matching persistent ID: %@", persistentID.UUIDString]}];
        }
        else {
            fetchedItem = [[SecCDKeychainItem alloc] initWithManagedItem:managedItem keychain:self error:&error];
            if (!fetchedItem || error) {
                secerror("SecCDKeychain: failed to create SecCDKeychainItem from managed item with error: %@", error);
                fetchedItem = nil;
            }
            else if (![self validateItemOwner:fetchedItem.owner withConnection:connection withError:nil]) { // if we fail owner validation, we don't return an error; we pretend it didn't exist
                fetchedItem = nil;
                error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorItemNotFound userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"did not find any keychain items matching persistent ID: %@", persistentID.UUIDString]}];
            }
        }
        
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            completionHandler(fetchedItem, error);
        });
    }];
}

- (void)fetchItemsWithValue:(NSString*)value forLookupKey:(NSString*)lookupKey ofType:(SecCDKeychainLookupValueType*)lookupValueType withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(NSArray<SecCDKeychainItemMetadata*>* items, NSError* error))completionHandler
{
    __weak __typeof(self) weakSelf = self;
    [self performOnManagedObjectQueue:^(NSManagedObjectContext* managedObjectContext, NSError* managedObjectError) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secerror("SecCDKeychain: attempt to fetch items from deallocated keychain instance");
            completionHandler(false, [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : @"attempt to lookup items from deallocated keychain instance"}]);
            return;
        }

        if (!managedObjectContext) {
            secerror("SecCDKeychain: fetchItemsWithValue: could not get managed object context");
            completionHandler(false, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal userInfo:@{ NSLocalizedDescriptionKey : @"fetchItemsWithValue: could not get managed object context", NSUnderlyingErrorKey : managedObjectError }]);
            return;
        }

        NSError* error = nil;
        NSMutableArray* fetchedItems = [[NSMutableArray alloc] init];
        NSFetchRequest* lookupEntryFetchRequest = [SecCDKeychainManagedLookupEntry fetchRequest];
        lookupEntryFetchRequest.predicate = [NSPredicate predicateWithFormat:@"lookupKey == %@ AND lookupValueType == %@ AND lookupValue == %@", lookupKey, lookupValueType, value];
        NSArray* lookupEntries = [managedObjectContext executeFetchRequest:lookupEntryFetchRequest error:&error];
        if (error) {
            secerror("SecCDKeychain: error fetching lookup entry during item lookup: %@", error);
            fetchedItems = nil;
        }
        else if (lookupEntries.count == 0) {
            // we didn't get an error, but we didn't find a lookup entry
            // that means there simply wasn't one in the db, so this lookup finds nothing
            CFErrorRef cfError = NULL;
            SecError(errSecItemNotFound, &cfError, CFSTR("did not find any keychain items matching query"));
            error = CFBridgingRelease(cfError);
        }
        else {
            for (SecCDKeychainManagedLookupEntry* lookupEntry in lookupEntries) {
                for (SecCDKeychainManagedItem* item in lookupEntry.matchingItems) {
                    SecCDKeychainItemMetadata* fetchedItem = [[SecCDKeychainItemMetadata alloc] initWithManagedItem:item keychain:self error:&error];
                    if (!fetchedItem || error) {
                        secerror("SecCDKeychain: failed to create SecCDKeychainItemMetadata from managed item with error: %@", error);
                        fetchedItems = nil;
                        break;
                    }
                    else if (![self validateItemOwner:fetchedItem.owner withConnection:connection withError:nil]) { // not an error; just pretend it's not there
                        continue;
                    }

                    [fetchedItems addObject:fetchedItem];
                }
            }
        }
        
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            completionHandler(fetchedItems, error);
        });
    }];
}

- (void)deleteItemWithPersistentID:(NSUUID*)persistentID withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(bool success, NSError* _Nullable error))completionHandler
{
    __weak __typeof(self) weakSelf = self;
    [self performOnManagedObjectQueue:^(NSManagedObjectContext* managedObjectContext, NSError* managedObjectError) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secerror("SecCDKeychain: attempt to fetch items from deallocated keychain instance");
            completionHandler(false, [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : @"attempt to insert items into deallocated keychain instance"}]);
            return;
        }

        if (!managedObjectContext) {
            secerror("SecCDKeychain: deleteItemWithPersistentID: could not get managed object context");
            completionHandler(false, [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorInternal userInfo:@{ NSLocalizedDescriptionKey : @"deleteItemWIthPersistentID: could not get managed object context", NSUnderlyingErrorKey : managedObjectError }]);
            return;
        }
        
        NSError* error = nil;
        SecCDKeychainManagedItem* managedItem = [self fetchManagedItemForPersistentID:persistentID withManagedObjectContext:managedObjectContext error:&error];
        bool success = false;
        if (managedItem && !error) {
            SecCDKeychainAccessControlEntity* owner = [[SecCDKeychainAccessControlEntity alloc] initWithManagedEntity:managedItem.owner];
            if ([self validateItemOwner:owner withConnection:connection withError:nil]) { // don't pass error here because we will treat it below as simply "item could not be found"
                for (SecCDKeychainManagedLookupEntry* lookupEntry in managedItem.lookupEntries.copy) {
                    [lookupEntry removeMatchingItemsObject:managedItem];
                    if (lookupEntry.matchingItems.count == 0) {
                        [managedObjectContext deleteObject:lookupEntry];
                    }
                }

                SecCDKeychainManagedAccessControlEntity* managedOwner = managedItem.owner;
                [managedOwner removeOwnedItemsObject:managedItem];
                [managedOwner removeAccessedItemsObject:managedItem];
                if (managedOwner.ownedItems.count == 0 && managedOwner.accessedItems == 0) {
                    [managedObjectContext deleteObject:managedOwner];
                }

                for (SecCDKeychainManagedAccessControlEntity* accessControlEntity in managedItem.accessControlList) {
                    [accessControlEntity removeAccessedItemsObject:managedItem];
                    if (accessControlEntity.ownedItems.count == 0 && accessControlEntity.accessedItems == 0) {
                        [managedObjectContext deleteObject:accessControlEntity];
                    }
                }

                [managedObjectContext deleteObject:managedItem];
                success = [managedObjectContext save:&error];
            }
            else {
                success = false;
            }
        }

        if (!success && !error) {
            secerror("SecCDKeychain: attempt to delete item with persistant identifier that could not be found: %@", persistentID);
            error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorItemNotFound userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"attempt to delete item with persistant identifier that could not be found: %@", persistentID]}];
        }
        
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            completionHandler(success, error);
        });
    }];
}

- (void)registerItemType:(SecCDKeychainItemType*)itemType withManagedObjectContext:(NSManagedObjectContext*)managedObjectContext
{
    _itemTypeDict[itemType.name] = itemType;

    NSMutableDictionary* itemTypeVersionDict = _managedItemTypeDict[itemType.name];
    if (!itemTypeVersionDict) {
        itemTypeVersionDict = [NSMutableDictionary dictionary];
        _managedItemTypeDict[itemType.name] = itemTypeVersionDict;
    }

    if (!itemTypeVersionDict[@(itemType.version)]) {
        NSError* error = nil;
        SecCDKeychainManagedItemType* managedItemType = [itemType managedItemTypeWithContext:managedObjectContext error:&error];
        if (managedItemType) {
            itemTypeVersionDict[@(itemType.version)] = managedItemType;
            [managedObjectContext save:&error];
        }

        if (!managedItemType || error) {
            secerror("SecCDKeychain: error registering managedItemType for for itemType: %@", itemType);
        }
    }
}

// this method is only for use by tests because built-in types should get registered at initial store setup
- (void)_registerItemTypeForTesting:(SecCDKeychainItemType*)itemType
{
    [self performOnManagedObjectQueueAndWait:^(NSManagedObjectContext* managedObjectContext, NSError* managedObjectError) {
        if (!managedObjectContext) {
            secerror("SecCDKeychain: _registerItemTypeForTesting: could not get managed object context");
            return;
        }

        [self registerItemType:itemType withManagedObjectContext:managedObjectContext];
    }];
}

- (SecCDKeychainItemType*)itemTypeForItemTypeName:(NSString*)itemTypeName
{
    return _itemTypeDict[itemTypeName];
}

- (SecCDKeychainManagedItem*)managedItemWithItem:(SecCDKeychainItem*)item withManagedObjectContext:(NSManagedObjectContext*)managedObjectContext error:(NSError**)error
{
    NSError* localError = nil;
    SecCDKeychainManagedItem* managedItem = [NSEntityDescription insertNewObjectForEntityForName:SecCDKeychainEntityItem inManagedObjectContext:managedObjectContext];
    managedItem.itemType = [[_managedItemTypeDict valueForKey:item.itemType.name] objectForKey:@(item.itemType.version)];
    managedItem.persistentID = item.metadata.persistentID;

    NSData* attributeData = [NSPropertyListSerialization dataWithPropertyList:item.attributes format:NSPropertyListBinaryFormat_v1_0 options:0 error:&localError];
    managedItem.metadata = attributeData;

    SecCDKeychainManagedAccessControlEntity* owner = [NSEntityDescription insertNewObjectForEntityForName:SecCDKeychainEntityTypeAccessControlEntity inManagedObjectContext:managedObjectContext];
    owner.type = (int32_t)item.owner.entityType;
    owner.stringRepresentation = item.owner.stringRepresentation;
    managedItem.owner = owner;
    [owner addOwnedItemsObject:managedItem];
    [owner addAccessedItemsObject:managedItem];

    // today, we only support the device keybag
    // someday, that will have to change
    managedItem.data = [item encryptedSecretDataWithAttributeData:attributeData keybag:KEYBAG_DEVICE error:&localError];

    if (error) {
        *error = localError;
    }

    return localError ? nil : managedItem;
}

@end

@implementation SecCDKeychainItemMetadata {
    SecCDKeychainItemType* _itemType;
    SecCDKeychainAccessControlEntity* _owner;
    NSUUID* _persistentID;
    NSDictionary* _attributes;
    NSSet<SecCDKeychainLookupTuple*>* _lookupAttributes;
    NSData* _managedDataBlob; // hold onto this to verify metadata been tampered with
    keyclass_t _keyclass;
}

@synthesize itemType = _itemType;
@synthesize owner = _owner;
@synthesize persistentID = _persistentID;
@synthesize attributes = _attributes;
@synthesize lookupAttributesSet = _lookupAttributes;
@synthesize managedDataBlob = _managedDataBlob;
@synthesize keyclass = _keyclass;

- (instancetype)initWithItemType:(SecCDKeychainItemType*)itemType persistentID:(NSUUID*)persistentID attributes:(NSDictionary*)attributes lookupAttributes:(NSArray<SecCDKeychainLookupTuple*>*)lookupAttributes owner:(SecCDKeychainAccessControlEntity*)owner keyclass:(keyclass_t)keyclass
{
    if (self = [super init]) {
        _itemType = itemType;
        _owner = owner;
        _persistentID = persistentID.copy;
        _attributes = attributes.copy;
        _keyclass = keyclass;

        if (lookupAttributes) {
            _lookupAttributes = [NSSet setWithArray:lookupAttributes];
        }
        else {
            NSMutableSet* newLookupAttributes = [[NSMutableSet alloc] init];
            [_attributes enumerateKeysAndObjectsUsingBlock:^(NSString* key, id value, BOOL* stop) {
                SecCDKeychainLookupTuple* lookupTuple = [SecCDKeychainLookupTuple lookupTupleWithKey:key value:value];
                [newLookupAttributes addObject:lookupTuple];
            }];
            _lookupAttributes = newLookupAttributes.copy;
        }
    }
    
    return self;
}

- (instancetype)initWithManagedItem:(SecCDKeychainManagedItem*)managedItem keychain:(SecCDKeychain*)keychain error:(NSError**)error
{
    if (self = [super init]) {
        _itemType = [keychain itemTypeForItemTypeName:managedItem.itemType.name];
        _persistentID = managedItem.persistentID.copy;
        _managedDataBlob = managedItem.metadata;
        _attributes = [NSPropertyListSerialization propertyListWithData:_managedDataBlob options:NSPropertyListImmutable format:NULL error:error];
        _owner = [[SecCDKeychainAccessControlEntity alloc] initWithManagedEntity:managedItem.owner];

        NSMutableSet* lookupAttributes = [NSMutableSet set];
        for (SecCDKeychainManagedLookupEntry* lookupEntry in managedItem.lookupEntries) {
            if (!lookupEntry.systemEntry) {
                [lookupAttributes addObject:[SecCDKeychainLookupTuple lookupTupleWithManagedLookupEntry:lookupEntry]];
            }
        }
        _lookupAttributes = lookupAttributes.copy;
        
        if (!_itemType || !_persistentID || !_owner || ![_attributes isKindOfClass:[NSDictionary class]]) {
            if (error) {
                *error = [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorDeserializing userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"failed to deserialize SecCDKeychainItemMetadata with item type (%@) persistentID: %@ owner: %@", _itemType, _persistentID, _owner]}];
            }
            self = nil;;
        }
    }
    
    return self;
}

- (BOOL)isEqual:(SecCDKeychainItemMetadata*)object
{
    return [_itemType isEqual:object.itemType] &&
           [_owner isEqual:object.owner] &&
           [_persistentID isEqual:object.persistentID] &&
           [_attributes isEqual:object.attributes] &&
           [_lookupAttributes isEqual:object.lookupAttributesSet];
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"%@: itemType:(%@) owner:(%@) persistentID:(%@)\n attributes: %@\n lookup attributes: %@", [super description], self.itemType, self.owner, self.persistentID, self.attributes, self.lookupAttributes];
}

- (void)fetchFullItemWithKeychain:(SecCDKeychain*)keychain withConnection:(SFKeychainServerConnection*)connection completionHandler:(void (^)(SecCDKeychainItem* _Nullable item, NSError* _Nullable error))completionHandler
{
    [keychain fetchItemForPersistentID:_persistentID withConnection:connection completionHandler:completionHandler];
}

- (NSArray<SecCDKeychainLookupTuple*>*)lookupAttributes
{
    return _lookupAttributes.allObjects;
}

- (NSArray*)primaryKeys
{
    return _itemType.primaryKeys;
}

@end

@implementation SecCDKeychainItem {
    SecCDKeychainItemMetadata* _metadata;
    NSData* _encryptedSecretData;
    NSDictionary* _secrets;
}

@synthesize metadata = _metadata;
@synthesize secrets = _secrets;

- (instancetype)initItemType:(SecCDKeychainItemType*)itemType withPersistentID:(NSUUID*)persistentID attributes:(NSDictionary*)attributes lookupAttributes:(NSArray<SecCDKeychainLookupTuple*>*)lookupAttributes secrets:(NSDictionary*)secrets owner:(SecCDKeychainAccessControlEntity*)owner keyclass:(keyclass_t)keyclass
{
    if (self = [super init]) {
        _secrets = secrets.copy;
        _metadata = [[SecCDKeychainItemMetadata alloc] initWithItemType:itemType persistentID:persistentID attributes:attributes lookupAttributes:lookupAttributes owner:owner keyclass:keyclass];
        if (!_metadata) {
            self = nil;
        }
    }

    return self;
}

- (instancetype)initWithManagedItem:(SecCDKeychainManagedItem*)managedItem keychain:(SecCDKeychain*)keychain error:(NSError**)error
{
    if (self = [super init]) {
        NSError* localError;
        _metadata = [[SecCDKeychainItemMetadata alloc] initWithManagedItem:(SecCDKeychainManagedItem*)managedItem keychain:keychain error:&localError];
        if (!_metadata) {
            if (error) {
                // TODO: add a negative unit test

                *error = [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorDeserializing userInfo:@{NSLocalizedDescriptionKey : @"could not create SecCDKeychainItem from managed item - managed item was malformed"}];
            }
            return nil;
        }

        _secrets = [self secretsFromEncryptedData:managedItem.data withKeybag:KEYBAG_DEVICE error:&localError];
        if (!_secrets) {
            if (error) {
                *error = [NSError errorWithDomain:SFKeychainErrorDomain code:SFKeychainErrorItemDecryptionFailed userInfo:@{ NSLocalizedDescriptionKey : @"could not decrypt secrets for item", NSUnderlyingErrorKey : localError }];
            }
            return nil;
        }
    }

    return self;
}

- (BOOL)isEqual:(SecCDKeychainItem*)object
{
    return [object isKindOfClass:[SecCDKeychainItem class]]
        && [_metadata isEqual:object.metadata]
        && [_secrets isEqual:object.secrets];
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"%@: itemType:(%@) persistentID:(%@)\n owner: %@\n attributes: %@\n lookup attributes: %@\nprimaryKeys: %@", [super description], self.itemType, self.persistentID, self.owner, self.attributes, self.lookupAttributes, self.primaryKeys];
}

- (SecCDKeychainItemType*)itemType
{
    return _metadata.itemType;
}

- (SecCDKeychainAccessControlEntity*)owner
{
    return _metadata.owner;
}

- (NSUUID*)persistentID
{
    return _metadata.persistentID;
}

- (NSDictionary*)attributes
{
    return _metadata.attributes;
}

- (NSArray<SecCDKeychainLookupTuple*>*)lookupAttributes
{
    return _metadata.lookupAttributes;
}

- (NSArray*)primaryKeys
{
    return _metadata.primaryKeys;
}

- (NSString*)primaryKeyStringRepresentationWithError:(NSError**)error
{
    NSDictionary* attributes = _metadata.attributes;
    NSArray* primaryKeys = _metadata.primaryKeys.count > 0 ? _metadata.primaryKeys : attributes.allKeys;
    NSArray* sortedPrimaryKeys = [primaryKeys sortedArrayUsingComparator:^NSComparisonResult(NSString* firstKey, NSString* secondKey) {
        return [firstKey compare:secondKey options:NSForcedOrderingSearch];
    }];

    SFSHA256DigestOperation* digest = [[SFSHA256DigestOperation alloc] init];
    [digest addData:[_metadata.owner.stringRepresentation dataUsingEncoding:NSUTF8StringEncoding]];

    for (NSString* key in sortedPrimaryKeys) {
        [digest addData:[key dataUsingEncoding:NSUTF8StringEncoding]];

        id value = attributes[key];
        if ([value isKindOfClass:[NSData class]]) {
            [digest addData:value];
        }
        else if ([value isKindOfClass:[NSString class]]) {
            [digest addData:[value dataUsingEncoding:NSUTF8StringEncoding]];
        }
        else {
            NSData* valueData = [NSKeyedArchiver archivedDataWithRootObject:value requiringSecureCoding:YES error:error];
            if (valueData) {
                [digest addData:valueData];
            }
            else {
                return nil;
            }
        }
    }

    return  [[digest hashValue] base64EncodedStringWithOptions:0];
}

- (NSData*)encryptedSecretDataWithAttributeData:(NSData*)attributeData keybag:(keybag_handle_t)keybag error:(NSError**)error
{
#if USE_KEYSTORE
    NSError* localError = nil;
    NSString* errorDescription = nil;

    SFAESKeySpecifier* keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAESKey* randomKey = [[SFAESKey alloc] initRandomKeyWithSpecifier:keySpecifier error:&localError];
    if (!randomKey) {
        secerror("SecCDKeychain: failed to create random key for encrypting item with error: %@", localError);
        errorDescription =  @"failed to create random key for encrypting item";
    }

    NSData* itemSecrets = [NSPropertyListSerialization dataWithPropertyList:_secrets format:NSPropertyListBinaryFormat_v1_0 options:0 error:&localError];
    if (!itemSecrets) {
        secerror("SecCDKeychain: failed to serialize item secrets dictionary with error: %@", localError);
        errorDescription = @"failed to serialize item secrets dictionary";
    }

    SFAuthenticatedEncryptionOperation* encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:keySpecifier];
    SFAuthenticatedCiphertext* ciphertext = nil;
    if (randomKey && itemSecrets) {
        NSData* attributeDataSHA256 = [SFSHA256DigestOperation digest:attributeData];
        ciphertext = [encryptionOperation encrypt:itemSecrets withKey:randomKey additionalAuthenticatedData:attributeDataSHA256 error:&localError];
        if (!ciphertext) {
            secerror("SecCDKeychain: failed to encrypt item secret data with error: %@", localError);
            errorDescription = @"failed to encrypt item secret data";
        }
    }

    SecAKSRefKey* refKey = nil;
    NSData* refKeyBlobData =  nil;

    if (ciphertext) {
        refKey = [[SecAKSRefKey alloc] initWithKeybag:keybag keyclass:_metadata.keyclass];
        refKeyBlobData = refKey.refKeyBlob;
        if (!refKey || !refKeyBlobData) {
            secerror("SecCDKeychain: failed to create refKey");
            errorDescription = @"failed to create refKey";
        }
    }

    NSData* wrappedKeyData = nil;
    if (refKey && refKeyBlobData) {
        wrappedKeyData = [refKey wrappedDataForKey:randomKey];
        if (!wrappedKeyData) {
            secerror("SecCDKeychain: failed to encrypt item");
            errorDescription = @"failed to encrypt item";
        }
    }

    if (wrappedKeyData) {
        SecCDKeychainItemWrappedSecretData* wrappedSecretData = [[SecCDKeychainItemWrappedSecretData alloc] initWithCiphertext:ciphertext wrappedKeyData:wrappedKeyData refKeyBlob:refKeyBlobData];
        NSData* wrappedSecretDataBlob = [NSKeyedArchiver archivedDataWithRootObject:wrappedSecretData requiringSecureCoding:YES error:&localError];
        if (wrappedSecretDataBlob) {
            return wrappedSecretDataBlob;
        }
        else {
            secerror("SecCDKeychain: failed to serialize item secret data blob with error: %@", localError);
            errorDescription = @"failed to serialize item secret data blob";
        }
    }

    if (error) {
        NSDictionary* userInfo = localError ? @{ NSUnderlyingErrorKey : localError, NSLocalizedDescriptionKey : errorDescription } : @{NSLocalizedDescriptionKey : errorDescription};
        *error = [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorInternal userInfo:userInfo];
    }

#endif
    return nil;
}

- (NSDictionary*)secretsFromEncryptedData:(NSData*)secretData withKeybag:(keybag_handle_t)keybag error:(NSError**)error
{
#if USE_KEYSTORE
    SecCDKeychainItemWrappedSecretData* wrappedSecretData = [NSKeyedUnarchiver unarchivedObjectOfClass:[SecCDKeychainItemWrappedSecretData class] fromData:secretData error:error];
    if (!wrappedSecretData) {
        secerror("SecCDKeychain: failed to deserialize item wrapped secret data");
        return nil;
    }

    SecAKSRefKey* refKey = [[SecAKSRefKey alloc] initWithBlob:wrappedSecretData.refKeyBlob keybag:keybag];
    if (!refKey) {
        secerror("SecCDKeychain: failed to create refKey for unwrapping item secrets");
        if (error) {
            *error = [NSError errorWithDomain:SecCDKeychainErrorDomain code:SecCDKeychainErrorInternal userInfo:@{NSLocalizedDescriptionKey : @"failed to create refKey for unwrapping item secrets"}];
        }
        return nil;
    }

    SFAESKey* key = [refKey keyWithWrappedData:wrappedSecretData.wrappedKeyData];
    if (!key) {
        secerror("SecCDKeychain: failed to create item key for decryption");
        return nil;
    }

    SFAESKeySpecifier* keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAuthenticatedEncryptionOperation* encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:keySpecifier];
    NSData* metadataSHA256 = [SFSHA256DigestOperation digest:_metadata.managedDataBlob];
    NSData* decryptedSecretData = [encryptionOperation decrypt:wrappedSecretData.ciphertext withKey:key additionalAuthenticatedData:metadataSHA256 error:error];
    if (!decryptedSecretData) {
        secerror("SecCDKeychain: failed to decrypt item secret data");
        return nil;
    }
    
    NSDictionary* secrets = [NSPropertyListSerialization propertyListWithData:decryptedSecretData options:0 format:NULL error:error];
    if (![secrets isKindOfClass:[NSDictionary class]]) {
        secerror("SecCDKeychain: failed to deserialize item decrypted secret data");
        return nil;
    }
    
    return secrets;
#else
    return nil;
#endif
}

// TODO: get back to this as part of CKKS integration work

//- (NSDictionary*)attributesPropertyListWithError:(NSError**)error
//{
//    __block NSDictionary* (^dictionaryToPropertyList)(NSDictionary*) = NULL;
//    __block NSArray* (^arrayToPropertyList)(NSArray*) = NULL;
//
//    dictionaryToPropertyList = ^(NSDictionary* dict) {
//        NSMutableDictionary* propertyList = [NSMutableDictionary dictionary];
//        [dict enumerateKeysAndObjectsUsingBlock:^(NSString* key, id object, BOOL* stop) {
//            Class objectClass = [object class];
//            if ([objectClass isKindOfClass:[NSString class]] || [objectClass isKindOfClass:[NSData class]] || [objectClass isKindOfClass:[NSDate class]] || [objectClass isKindOfClass:[NSNumber class]]) {
//                propertyList[key] = object;
//            }
//            else if ([objectClass isKindOfClass:[NSDictionary class]]){
//                NSDictionary* objectAsPropertyList = dictionaryToPropertyList(object);
//                if (objectAsPropertyList) {
//                    propertyList[key] = objectAsPropertyList;
//                }
//                else {
//                    *stop = YES;
//                }
//            }
//            else if ([objectClass isKindOfClass:[NSArray class]]) {
//                NSArray* objectAsPropertyList = arrayToPropertyList(object);
//                if (objectAsPropertyList) {
//                    propertyList[key] = objectAsPropertyList;
//                }
//                else {
//                    *stop = YES;
//                }
//            }
//            else if ([object conformsToProtocol:@protocol(NSSecureCoding)]) {
//                NSData*
//            }
//        }];
//    }
//}

@end

@implementation SecCDKeychainLookupTuple {
    NSString* _key;
    id<NSCopying, NSObject> _value;
    SecCDKeychainLookupValueType* _valueType;
}

@synthesize key = _key;
@synthesize value = _value;
@synthesize valueType = _valueType;

+ (instancetype)lookupTupleWithKey:(NSString*)key value:(id<NSCopying, NSObject>)value
{
    return [[self alloc] initWithKey:key value:value];
}

+ (instancetype)lookupTupleWithManagedLookupEntry:(SecCDKeychainManagedLookupEntry*)lookupEntry
{
    NSString* valueString = lookupEntry.lookupValue;
    id value;
    NSString* valueType = lookupEntry.lookupValueType;
    if ([valueType isEqualToString:SecCDKeychainLookupValueTypeString]) {
        value = valueString;
    }
    else {
        NSData* valueData = [[NSData alloc] initWithBase64EncodedString:valueString options:0];

        if ([valueType isEqualToString:SecCDKeychainLookupValueTypeData]) {
            value = valueData;
        }
        else if ([valueType isEqualToString:SecCDKeychainLookupValueTypeDate]) {
            // TODO: error parameter
            value = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSDate class] fromData:valueData error:nil];
        }
        else if ([valueType isEqualToString:SecCDKeychainLookupValueTypeNumber]) {
            value = [NSKeyedUnarchiver unarchivedObjectOfClass:[NSNumber class] fromData:valueData error:nil];
        }
        else {
            // TODO: error here
            value = nil;
        }
    }

    return value ? [[self alloc] initWithKey:lookupEntry.lookupKey value:value] : nil;
}

- (instancetype)initWithKey:(NSString*)key value:(id<NSCopying, NSObject>)value
{
    if (self = [super init]) {
        SecCDKeychainLookupValueType* valueType = [SecCDKeychain lookupValueTypeForObject:value];
        BOOL zeroLengthValue = ([valueType isEqualToString:SecCDKeychainLookupValueTypeString] && [(NSString*)value length] == 0) || ([valueType isEqualToString:SecCDKeychainLookupValueTypeData] && [(NSData*)value length] == 0);
        if (valueType && !zeroLengthValue) {
            _key = key.copy;
            _value = [value copyWithZone:nil];
            _valueType = valueType.copy;
        }
        else {
            // TODO: add an error parameter to this method
            self = nil;
        }
    }

    return self;
}

- (BOOL)isEqual:(SecCDKeychainLookupTuple*)object
{
    return [_key isEqualToString:object.key] && [_value isEqual:object.value] && [_valueType isEqualToString:object.valueType];
}

- (NSUInteger)hash
{
    return _key.hash ^ _value.hash ^ _valueType.hash;
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"%@ : %@ [%@]", _key, _value, _valueType];
}

- (NSString*)stringRepresentation
{
    if ([_valueType isEqualToString:SecCDKeychainLookupValueTypeString]) {
        return (NSString*)_value;
    }
    else if ([_valueType isEqualToString:SecCDKeychainLookupValueTypeData]) {
        return [(NSData*)_value base64EncodedStringWithOptions:0];
    }
    else {
        return [[NSKeyedArchiver archivedDataWithRootObject:_value requiringSecureCoding:YES error:nil] base64EncodedStringWithOptions:0];
    }
}

@end

@implementation SecCDKeychainItemType {
    NSString* _name;
    int32_t _version;
    NSSet* _primaryKeys;
    NSSet* _syncableKeys;
    SecCDKeychainManagedItemType* _managedItemType;
}

@synthesize name = _name;
@synthesize version = _version;

+ (instancetype)itemType
{
    return nil;
}

+ (nullable instancetype)itemTypeForVersion:(int32_t)version
{
    return [self itemType];
}

- (instancetype)_initWithName:(NSString*)name version:(int32_t)version primaryKeys:(NSArray*)primaryKeys syncableKeys:(NSArray*)syncableKeys
{
    if (self = [super init]) {
        _name = name.copy;
        _version = version;
        _primaryKeys = [NSSet setWithArray:primaryKeys];
        _syncableKeys = [NSSet setWithArray:syncableKeys];
    }

    return self;
}

- (BOOL)isEqual:(SecCDKeychainItemType*)object
{
    return [object isKindOfClass:[SecCDKeychainItemType class]] &&
           [_name isEqualToString:object.name] &&
           _version == object.version &&
           [_primaryKeys isEqualToSet:object->_primaryKeys] &&
           [_syncableKeys isEqualToSet:object->_syncableKeys];
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"%@ | %d", _name, _version];
}

- (NSString*)debugDescription
{
    return [NSString stringWithFormat:@"%@\n name: %@\n version: %d\n primaryKeys: %@\n syncableKeys: %@", [super debugDescription], _name, _version, _primaryKeys, _syncableKeys];
}

- (NSArray*)primaryKeys
{
    return _primaryKeys.allObjects;
}

- (NSArray*)syncableKeys
{
    return _syncableKeys.allObjects;
}

- (SecCDKeychainManagedItemType*)managedItemTypeWithContext:(NSManagedObjectContext*)managedObjectContext error:(NSError**)error
{
    NSError* localError = nil;
    SecCDKeychainManagedItemType* managedItemType = [NSEntityDescription insertNewObjectForEntityForName:SecCDKeychainEntityItemType inManagedObjectContext:managedObjectContext];
    managedItemType.name = _name;
    managedItemType.version = _version;
    managedItemType.primaryKeys = [NSPropertyListSerialization dataWithPropertyList:_primaryKeys.allObjects format:NSPropertyListBinaryFormat_v1_0 options:0 error:&localError];
    managedItemType.syncableKeys = [NSPropertyListSerialization dataWithPropertyList:_syncableKeys.allObjects format:NSPropertyListBinaryFormat_v1_0 options:0 error:&localError];

    if (error) {
        *error = localError;
    }

    return localError ? nil : managedItemType;
}

@end

@implementation SecCDKeychainAccessControlEntity {
    SecCDKeychainAccessControlEntityType _entityType;
    NSString* _stringRepresentation;
}

@synthesize entityType = _entityType;
@synthesize stringRepresentation = _stringRepresentation;

+ (instancetype)accessControlEntityWithType:(SecCDKeychainAccessControlEntityType)type stringRepresentation:(NSString*)stringRepresentation
{
    return [[self alloc] _initWithEntityType:type stringRepresentation:stringRepresentation];
}

- (instancetype)_initWithEntityType:(SecCDKeychainAccessControlEntityType)type stringRepresentation:(NSString*)stringRepresentation
{
    if (self = [super init]) {
        _entityType = type;
        _stringRepresentation = stringRepresentation.copy;
    }

    return self;
}

- (instancetype)initWithManagedEntity:(SecCDKeychainManagedAccessControlEntity*)managedAccessControlEntity
{
    return [self _initWithEntityType:managedAccessControlEntity.type stringRepresentation:managedAccessControlEntity.stringRepresentation];
}

- (BOOL)isEqual:(SecCDKeychainAccessControlEntity*)object
{
    return _entityType == object.entityType && [_stringRepresentation isEqualToString:object.stringRepresentation];
}

@end

#endif // TARGET_OS_BRIDGE
