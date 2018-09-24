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

#import "SecDbKeychainItemV7.h"
#import "SecKeybagSupport.h"
#import "SecItemServer.h"
#import "SecAccessControl.h"
#import "SecDbKeychainSerializedItemV7.h"
#import "SecDbKeychainSerializedAKSWrappedKey.h"
#import "SecDbKeychainSerializedMetadata.h"
#import "SecDbKeychainSerializedSecretData.h"
#import <notify.h>
#import <dispatch/dispatch.h>
#import <utilities/SecAKSWrappers.h>
#import <utilities/der_plist.h>
#import "sec_action.h"
#if !TARGET_OS_BRIDGE
#import <SecurityFoundation/SFEncryptionOperation.h>
#import <SecurityFoundation/SFKey_Private.h>
#import <SecurityFoundation/SFCryptoServicesErrors.h>
#endif
#import <Foundation/NSKeyedArchiver_Private.h>

#if USE_KEYSTORE
#import <Kernel/IOKit/crypto/AppleKeyStoreDefs.h>
#endif

#define KEYCHAIN_ITEM_PADDING_MODULUS 20

NSString* const SecDbKeychainErrorDomain = @"SecDbKeychainErrorDomain";
const NSInteger SecDbKeychainErrorDeserializationFailed = 1;

static NSString* const SecDBTamperCheck = @"TamperCheck";

#define BridgeCFErrorToNSErrorOut(nsErrorOut, CFErr) \
{ \
    if (nsErrorOut) { \
        *nsErrorOut = CFBridgingRelease(CFErr); \
        CFErr = NULL; \
    } \
    else { \
        CFReleaseNull(CFErr); \
    } \
}

#if TARGET_OS_BRIDGE

@implementation SecDbKeychainItemV7

- (instancetype)initWithData:(NSData*)data decryptionKeybag:(keybag_handle_t)decryptionKeybag error:(NSError**)error
{
    return nil;
}

- (instancetype)initWithSecretAttributes:(NSDictionary*)secretAttributes metadataAttributes:(NSDictionary*)metadataAttributes tamperCheck:(NSString*)tamperCheck keyclass:(keyclass_t)keyclass
{
    return nil;
}

- (NSDictionary*)metadataAttributesWithError:(NSError**)error
{
    return nil;
}

- (NSDictionary*)secretAttributesWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error
{
    return nil;
}

- (BOOL)deleteWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error
{
    return NO;
}

- (NSData*)encryptedBlobWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    return nil;
}

@end

#else

static NSDictionary* dictionaryFromDERData(NSData* data)
{
    NSDictionary* dict = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)data, 0, NULL, NULL);
    return [dict isKindOfClass:[NSDictionary class]] ? dict : nil;
}

typedef NS_ENUM(uint32_t, SecDbKeychainAKSWrappedKeyType) {
    SecDbKeychainAKSWrappedKeyTypeRegular,
    SecDbKeychainAKSWrappedKeyTypeRefKey
};

@interface SecDbKeychainAKSWrappedKey : NSObject

@property (readonly) NSData* wrappedKey;
@property (readonly) NSData* refKeyBlob;
@property (readonly) SecDbKeychainAKSWrappedKeyType type;

@property (readonly) NSData* serializedRepresentation;

- (instancetype)initWithData:(NSData*)data;
- (instancetype)initRegularWrappedKeyWithData:(NSData*)wrappedKey;
- (instancetype)initRefKeyWrappedKeyWithData:(NSData*)wrappedKey refKeyBlob:(NSData*)refKeyBlob;

@end

@interface SecDbKeychainMetadata : NSObject

@property (readonly) SFAuthenticatedCiphertext* ciphertext;
@property (readonly) SFAuthenticatedCiphertext* wrappedKey;
@property (readonly) NSString* tamperCheck;

@property (readonly) NSData* serializedRepresentation;

- (instancetype)initWithData:(NSData*)data;
- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKey:(SFAuthenticatedCiphertext*)wrappedKey tamperCheck:(NSString*)tamperCheck error:(NSError**)error;

@end

@interface SecDbKeychainSecretData : NSObject

@property (readonly) SFAuthenticatedCiphertext* ciphertext;
@property (readonly) SecDbKeychainAKSWrappedKey* wrappedKey;
@property (readonly) NSString* tamperCheck;

@property (readonly) NSData* serializedRepresentation;

- (instancetype)initWithData:(NSData*)data;
- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKey:(SecDbKeychainAKSWrappedKey*)wrappedKey tamperCheck:(NSString*)tamperCheck error:(NSError**)error;

@end

@implementation SecDbKeychainAKSWrappedKey {
    SecDbKeychainSerializedAKSWrappedKey* _serializedHolder;
}

- (instancetype)initRegularWrappedKeyWithData:(NSData*)wrappedKey
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedAKSWrappedKey alloc] init];
        _serializedHolder.wrappedKey = wrappedKey;
        _serializedHolder.type = SecDbKeychainAKSWrappedKeyTypeRegular;
    }

    return self;
}

- (instancetype)initRefKeyWrappedKeyWithData:(NSData*)wrappedKey refKeyBlob:(NSData*)refKeyBlob
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedAKSWrappedKey alloc] init];
        _serializedHolder.wrappedKey = wrappedKey;
        _serializedHolder.refKeyBlob = refKeyBlob;
        _serializedHolder.type = SecDbKeychainAKSWrappedKeyTypeRefKey;
    }

    return self;
}

- (instancetype)initWithData:(NSData*)data
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedAKSWrappedKey alloc] initWithData:data];
        if (!_serializedHolder.wrappedKey || (_serializedHolder.type == SecDbKeychainAKSWrappedKeyTypeRefKey && !_serializedHolder.refKeyBlob)) {
            self = nil;
        }
    }

    return self;
}

- (NSData*)serializedRepresentation
{
    return _serializedHolder.data;
}

- (NSData*)wrappedKey
{
    return _serializedHolder.wrappedKey;
}

- (NSData*)refKeyBlob
{
    return _serializedHolder.refKeyBlob;
}

- (SecDbKeychainAKSWrappedKeyType)type
{
    return _serializedHolder.type;
}

@end

@implementation SecDbKeychainMetadata {
    SecDbKeychainSerializedMetadata* _serializedHolder;
}

- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKey:(SFAuthenticatedCiphertext*)wrappedKey tamperCheck:(NSString*)tamperCheck error:(NSError**)error
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedMetadata alloc] init];
        _serializedHolder.ciphertext = [NSKeyedArchiver archivedDataWithRootObject:ciphertext requiringSecureCoding:YES error:error];
        _serializedHolder.wrappedKey = [NSKeyedArchiver archivedDataWithRootObject:wrappedKey requiringSecureCoding:YES error:error];
        _serializedHolder.tamperCheck = tamperCheck;
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (instancetype)initWithData:(NSData*)data
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedMetadata alloc] initWithData:data];
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (NSData*)serializedRepresentation
{
    return _serializedHolder.data;
}

- (SFAuthenticatedCiphertext*)ciphertext
{
    NSError* error = nil;
    SFAuthenticatedCiphertext* ciphertext =  [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_serializedHolder.ciphertext error:&error];
    if (!ciphertext) {
        secerror("SecDbKeychainItemV7: error deserializing ciphertext from metadata: %@", error);
    }

    return ciphertext;
}

- (SFAuthenticatedCiphertext*)wrappedKey
{
    NSError* error = nil;
    SFAuthenticatedCiphertext* wrappedKey =  [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_serializedHolder.wrappedKey error:&error];
    if (!wrappedKey) {
        secerror("SecDbKeychainItemV7: error deserializing wrappedKey from metadata: %@", error);
    }

    return wrappedKey;
}

- (NSString*)tamperCheck
{
    return _serializedHolder.tamperCheck;
}

@end

@implementation SecDbKeychainSecretData {
    SecDbKeychainSerializedSecretData* _serializedHolder;
}

- (instancetype)initWithCiphertext:(SFAuthenticatedCiphertext*)ciphertext wrappedKey:(SecDbKeychainAKSWrappedKey*)wrappedKey tamperCheck:(NSString*)tamperCheck error:(NSError**)error
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedSecretData alloc] init];
        _serializedHolder.ciphertext = [NSKeyedArchiver archivedDataWithRootObject:ciphertext requiringSecureCoding:YES error:error];
        _serializedHolder.wrappedKey = wrappedKey.serializedRepresentation;
        _serializedHolder.tamperCheck = tamperCheck;
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (instancetype)initWithData:(NSData*)data
{
    if (self = [super init]) {
        _serializedHolder = [[SecDbKeychainSerializedSecretData alloc] initWithData:data];
        if (!_serializedHolder.ciphertext || !_serializedHolder.wrappedKey || !_serializedHolder.tamperCheck) {
            self = nil;
        }
    }

    return self;
}

- (NSData*)serializedRepresentation
{
    return _serializedHolder.data;
}

- (SFAuthenticatedCiphertext*)ciphertext
{
    NSError* error = nil;
    SFAuthenticatedCiphertext* ciphertext =  [NSKeyedUnarchiver unarchivedObjectOfClass:[SFAuthenticatedCiphertext class] fromData:_serializedHolder.ciphertext error:&error];
    if (!ciphertext) {
        secerror("SecDbKeychainItemV7: error deserializing ciphertext from secret data: %@", error);
    }

    return ciphertext;
}

- (SecDbKeychainAKSWrappedKey*)wrappedKey
{
    return [[SecDbKeychainAKSWrappedKey alloc] initWithData:_serializedHolder.wrappedKey];
}

- (NSString*)tamperCheck
{
    return _serializedHolder.tamperCheck;
}

@end

//////  SecDbKeychainMetadataKeyStore

@interface SecDbKeychainMetadataKeyStore ()
- (SFAESKey*)keyForKeyclass:(keyclass_t)keyClass
                     keybag:(keybag_handle_t)keybag
               keySpecifier:(SFAESKeySpecifier*)keySpecifier
         createKeyIfMissing:(bool)createIfMissing
        overwriteCorruptKey:(bool)overwriteCorruptKey
                      error:(NSError**)error;
@end

static SecDbKeychainMetadataKeyStore* sharedStore = nil;
static dispatch_queue_t sharedMetadataStoreQueue;
static void initializeSharedMetadataStoreQueue(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedMetadataStoreQueue = dispatch_queue_create("metadata_store", DISPATCH_QUEUE_SERIAL);
    });
}

@implementation SecDbKeychainMetadataKeyStore {
    NSMutableDictionary* _keysDict;
    dispatch_queue_t _queue;
}

+ (void)resetSharedStore
{
    initializeSharedMetadataStoreQueue();
    dispatch_sync(sharedMetadataStoreQueue, ^{
        if(sharedStore) {
            dispatch_sync(sharedStore->_queue, ^{
                [sharedStore _onQueueDropAllKeys];
            });
        }
        sharedStore = nil;
    });
}

+ (instancetype)sharedStore
{
    __block SecDbKeychainMetadataKeyStore* ret;
    initializeSharedMetadataStoreQueue();
    dispatch_sync(sharedMetadataStoreQueue, ^{
        if(!sharedStore) {
            sharedStore = [[self alloc] _init];
        }

        ret = sharedStore;
    });

    return ret;
}

+ (bool)cachingEnabled
{
    return true;
}

- (instancetype)_init
{
    if (self = [super init]) {
        _keysDict = [[NSMutableDictionary alloc] init];
        _queue = dispatch_queue_create("SecDbKeychainMetadataKeyStore", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        int token = 0;
        __weak __typeof(self) weakSelf = self;
        notify_register_dispatch(kUserKeybagStateChangeNotification, &token, _queue, ^(int inToken) {
            bool locked = true;
            CFErrorRef error = NULL;
            if (!SecAKSGetIsLocked(&locked, &error)) {
                secerror("SecDbKeychainMetadataKeyStore: error getting lock state: %@", error);
                CFReleaseNull(error);
            }

            if (locked) {
                [weakSelf _onQueueDropClassAKeys];
            }
        });
    }

    return self;
}

- (void)dropClassAKeys
{
    dispatch_sync(_queue, ^{
        [self _onQueueDropClassAKeys];
    });
}

- (void)_onQueueDropClassAKeys
{
    dispatch_assert_queue(_queue);

    secnotice("SecDbKeychainMetadataKeyStore", "dropping class A metadata keys");
    _keysDict[@(key_class_ak)] = nil;
    _keysDict[@(key_class_aku)] = nil;
    _keysDict[@(key_class_akpu)] = nil;
}

- (void)_onQueueDropAllKeys
{
    dispatch_assert_queue(_queue);

    secnotice("SecDbKeychainMetadataKeyStore", "dropping all metadata keys");
    [_keysDict removeAllObjects];
}

- (void)_updateActualKeyclassIfNeeded:(keyclass_t)actualKeyclassToWriteBackToDB keyclass:(keyclass_t)keyclass
{
    __block CFErrorRef cfError = NULL;

    secnotice("SecDbKeychainItemV7", "saving actualKeyclass %d for metadata keyclass %d", actualKeyclassToWriteBackToDB, keyclass);

    kc_with_dbt_non_item_tables(true, &cfError, ^bool(SecDbConnectionRef dbt) {
        __block bool actualKeyWriteBackOk = true;

        // we did not find an actualKeyclass entry in the db, so let's add one in now.
        NSString *sql = @"UPDATE metadatakeys SET actualKeyclass = ? WHERE keyclass = ? AND actualKeyclass IS NULL";
        __block CFErrorRef actualKeyWriteBackError = NULL;
        actualKeyWriteBackOk &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &actualKeyWriteBackError, ^(sqlite3_stmt* stmt) {
            actualKeyWriteBackOk &= SecDbBindInt(stmt, 1, actualKeyclassToWriteBackToDB, &actualKeyWriteBackError);
            actualKeyWriteBackOk &= SecDbBindInt(stmt, 2, keyclass, &actualKeyWriteBackError);
            actualKeyWriteBackOk &= SecDbStep(dbt, stmt, &actualKeyWriteBackError, ^(bool* stop) {
                // woohoo
            });
        });

        if (actualKeyWriteBackOk) {
            secnotice("SecDbKeychainItemV7", "successfully saved actualKeyclass %d for metadata keyclass %d", actualKeyclassToWriteBackToDB, keyclass);

        }
        else {
            // we can always try this again in the future if it failed
            secerror("SecDbKeychainItemV7: failed to save actualKeyclass %d for metadata keyclass %d; error: %@", actualKeyclassToWriteBackToDB, keyclass, actualKeyWriteBackError);
        }
        return actualKeyWriteBackOk;
    });
}

- (SFAESKey*)keyForKeyclass:(keyclass_t)keyclass
                     keybag:(keybag_handle_t)keybag
               keySpecifier:(SFAESKeySpecifier*)keySpecifier
         createKeyIfMissing:(bool)createIfMissing
        overwriteCorruptKey:(bool)overwriteCorruptKey
                      error:(NSError**)error
{
    __block SFAESKey* key = nil;
    __block NSError* nsErrorLocal = nil;
    __block CFErrorRef cfError = NULL;
    static __thread BOOL reentrant = NO;

    NSAssert(!reentrant, @"re-entering -[%@ %@] - that shouldn't happen!", NSStringFromClass(self.class), NSStringFromSelector(_cmd));
    reentrant = YES;

#if USE_KEYSTORE
    if (keyclass > key_class_last) {
        // idea is that AKS may return a keyclass value with extra bits above key_class_last from aks_wrap_key, but we only keep metadata keys for the canonical key classes
        // so just sanitize all our inputs to the canonical values
        keyclass_t sanitizedKeyclass = keyclass & key_class_last;
        secinfo("SecDbKeychainItemV7", "sanitizing request for metadata keyclass %d to keyclass %d", keyclass, sanitizedKeyclass);
        keyclass = sanitizedKeyclass;
    }
#endif

    dispatch_sync(_queue, ^{
        // if we think we're locked, it's possible AKS will still give us access to keys, such as during backup,
        // but we should force AKS to be the truth and not used cached class A keys while locked
        bool allowKeyCaching = [SecDbKeychainMetadataKeyStore cachingEnabled];

        // However, we must not cache a newly-created key, just in case someone above us in the stack rolls back our database transaction and the stored key is lost.
        __block bool keyIsNewlyCreated = false;
#if 0
        // <rdar://problem/37523001> Fix keychain lock state check to be both secure and fast for EDU mode
        if (![SecDbKeychainItemV7 isKeychainUnlocked]) {
            [self _onQueueDropClassAKeys];
            allowKeyCaching = !(keyclass == key_class_ak || keyclass == key_class_aku || keyclass == key_class_akpu);
        }
#endif

        key = allowKeyCaching ? self->_keysDict[@(keyclass)] : nil;
        if (!key) {
            __block bool ok = true;
            __block bool metadataKeyDoesntAuthenticate = false;
            ok &= kc_with_dbt_non_item_tables(createIfMissing, &cfError, ^bool(SecDbConnectionRef dbt) {
                __block NSString* sql = [NSString stringWithFormat:@"SELECT data, actualKeyclass FROM metadatakeys WHERE keyclass = %d", keyclass];
                ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &cfError, ^(sqlite3_stmt *stmt) {
                    ok &= SecDbStep(dbt, stmt, &cfError, ^(bool *stop) {
                        NSData* wrappedKeyData = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 0) length:sqlite3_column_bytes(stmt, 0)];
                        NSMutableData* unwrappedKeyData = [NSMutableData dataWithLength:wrappedKeyData.length];
                        
                        keyclass_t actualKeyclass = sqlite3_column_int(stmt, 1);
                        
                        keyclass_t actualKeyclassToWriteBackToDB = 0;
                        keyclass_t keyclassForUnwrapping = actualKeyclass == 0 ? keyclass : actualKeyclass;
                        ok &= [SecDbKeychainItemV7 aksDecryptWithKeybag:keybag keyclass:keyclassForUnwrapping wrappedKeyData:wrappedKeyData outKeyclass:NULL unwrappedKey:unwrappedKeyData error:&nsErrorLocal];
                        if (ok) {
                            key = [[SFAESKey alloc] initWithData:unwrappedKeyData specifier:keySpecifier error:&nsErrorLocal];

                            if(!key) {
                                os_log_fault(secLogObjForScope("SecDbKeychainItemV7"), "Metadata class key (%d) decrypted, but didn't become a key: %@", keyclass, nsErrorLocal);
                            }
                            
                            if (actualKeyclass == 0) {
                                actualKeyclassToWriteBackToDB = keyclassForUnwrapping;
                            }
                        }
#if USE_KEYSTORE
                        else if (actualKeyclass == 0 && keyclass <= key_class_last) {
                            // in this case we might have luck decrypting with a key-rolled keyclass
                            keyclass_t keyrolledKeyclass = keyclass | (key_class_last + 1);
                            secerror("SecDbKeychainItemV7: failed to decrypt metadata key for class %d, but trying keyrolled keyclass (%d); error: %@", keyclass, keyrolledKeyclass, nsErrorLocal);
                            
                            // we don't want to pollute subsequent error-handling logic with what happens on our retry
                            // we'll give it a shot, and if it works, great - if it doesn't work, we'll just report that error in the log and move on
                            NSError* retryError = nil;
                            ok = [SecDbKeychainItemV7 aksDecryptWithKeybag:keybag keyclass:keyrolledKeyclass wrappedKeyData:wrappedKeyData outKeyclass:NULL unwrappedKey:unwrappedKeyData error:&retryError];
                            
                            if (ok) {
                                secerror("SecDbKeychainItemV7: successfully decrypted metadata key using keyrolled keyclass %d", keyrolledKeyclass);
                                key = [[SFAESKey alloc] initWithData:unwrappedKeyData specifier:keySpecifier error:&retryError];

                                if(!key) {
                                    os_log_fault(secLogObjForScope("SecDbKeychainItemV7"), "Metadata class key (%d) decrypted using keyrolled keyclass %d, but didn't become a key: %@", keyclass, keyrolledKeyclass, retryError);
                                    nsErrorLocal = retryError;
                                }
                            }
                            else {
                                secerror("SecDbKeychainItemV7: failed to decrypt metadata key with keyrolled keyclass %d; error: %@", keyrolledKeyclass, retryError);
                            }
                        }
#endif
                        
                        if (ok && key) {
                            if (actualKeyclassToWriteBackToDB > 0) {
                                // check if we have updated this keyclass or not already
                                static NSMutableDictionary* updated = NULL;
                                if (!updated) {
                                    updated = [NSMutableDictionary dictionary];
                                }
                                if (!updated[@(keyclass)]) {
                                    updated[@(keyclass)] = @YES;
                                    dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                                        [self _updateActualKeyclassIfNeeded:actualKeyclassToWriteBackToDB keyclass:keyclass];
                                    });
                                }
                            }
                        }
                        else {
                            if (nsErrorLocal && [nsErrorLocal.domain isEqualToString:(__bridge NSString*)kSecErrorDomain] && nsErrorLocal.code == errSecInteractionNotAllowed) {
                                static dispatch_once_t kclockedtoken;
                                static sec_action_t kclockedaction;
                                dispatch_once(&kclockedtoken, ^{
                                    kclockedaction = sec_action_create("keychainlockedlogmessage", 1);
                                    sec_action_set_handler(kclockedaction, ^{
                                        secerror("SecDbKeychainItemV7: failed to decrypt metadata key because the keychain is locked (%d)", (int)errSecInteractionNotAllowed);
                                    });
                                });
                                sec_action_perform(kclockedaction);
                            } else {
                                secerror("SecDbKeychainItemV7: failed to decrypt and create metadata key for class %d; error: %@", keyclass, nsErrorLocal);

                                // If this error is errSecDecode, then it's failed authentication and likely will forever. Other errors are scary.                                
                                metadataKeyDoesntAuthenticate = [nsErrorLocal.domain isEqualToString:NSOSStatusErrorDomain] && nsErrorLocal.code == errSecDecode;
                                if(metadataKeyDoesntAuthenticate) {
                                    os_log_fault(secLogObjForScope("SecDbKeychainItemV7"), "Metadata class key (%d) failed to decrypt: %@", keyclass, nsErrorLocal);
                                }
                            }
                        }
                    });
                });

                bool keyNotYetCreated = ok && !key;
                bool forceOverwriteBadKey = !key && metadataKeyDoesntAuthenticate && overwriteCorruptKey;

                if (createIfMissing && (keyNotYetCreated || forceOverwriteBadKey)) {
                    // we completed the database query, but no key exists or it's broken - we should create one
                    if(forceOverwriteBadKey) {
                        secerror("SecDbKeychainItemV7: metadata key is irreparably corrupt; throwing away forever");
                        // TODO: track this in LocalKeychainAnalytics
                    }

                    ok = true; // Reset 'ok': we have a second chance

                    key = [[SFAESKey alloc] initRandomKeyWithSpecifier:keySpecifier error:&nsErrorLocal];
                    keyIsNewlyCreated = true;

                    if (key) {
                        NSMutableData* wrappedKey = [NSMutableData dataWithLength:key.keyData.length + 40];
                        keyclass_t outKeyclass = keyclass;
                        ok &= [SecDbKeychainItemV7 aksEncryptWithKeybag:keybag keyclass:keyclass keyData:key.keyData outKeyclass:&outKeyclass wrappedKey:wrappedKey error:&nsErrorLocal];
                        if (ok) {
                            secinfo("SecDbKeychainItemV7", "attempting to save new metadata key for keyclass %d with actualKeyclass %d", keyclass, outKeyclass);
                            NSString* insertString = forceOverwriteBadKey ? @"INSERT OR REPLACE" : @"INSERT";
                            sql = [NSString stringWithFormat:@"%@ into metadatakeys (keyclass, actualKeyclass, data) VALUES (?, ?, ?)", insertString];
                            ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &cfError, ^(sqlite3_stmt* stmt) {
                                ok &= SecDbBindInt(stmt, 1, keyclass, &cfError);
                                ok &= SecDbBindInt(stmt, 2, outKeyclass, &cfError);
                                ok &= SecDbBindBlob(stmt, 3, wrappedKey.bytes, wrappedKey.length, SQLITE_TRANSIENT, NULL);
                                ok &= SecDbStep(dbt, stmt, &cfError, ^(bool *stop) {
                                    // woohoo
                                });
                            });

                            if (ok) {
                                secnotice("SecDbKeychainItemV7", "successfully saved new metadata key for keyclass %d", keyclass);
                            }
                            else {
                                secerror("SecDbKeychainItemV7: failed to save new metadata key for keyclass %d - probably there is already one in the database: %@", keyclass, cfError);
                            }
                        } else {
                            secerror("SecDbKeychainItemV7: unable to encrypt new metadata key(%d) with keybag(%d): %@", keyclass, keybag, nsErrorLocal);
                        }
                    }
                    else {
                        ok = false;
                    }
                } else if(!key) {
                    // No key, but we're not supposed to make one. Make an error if one doesn't yet exist.
                    ok = false;
                    if(!nsErrorLocal) {
                        nsErrorLocal = [NSError errorWithDomain:(id)kSecErrorDomain code:errSecDecode userInfo:@{NSLocalizedDescriptionKey: @"Unable to find or create a suitable metadata key"}];
                    }
                }

                return ok;
            });

            if (ok && key) {
                // We can't cache a newly-created key, just in case this db transaction is rolled back and we lose the persisted key.
                // Don't worry, we'll cache it as soon as it's used again.
                if (allowKeyCaching && !keyIsNewlyCreated) {
                    self->_keysDict[@(keyclass)] = key;
                    __weak __typeof(self) weakSelf = self;
                    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(60 * 5 * NSEC_PER_SEC)), self->_queue, ^{
                        [weakSelf _onQueueDropClassAKeys];
                    });
                }
            }
            else {
                key = nil;
            }
        }
    });

    reentrant = NO;

    if (error && nsErrorLocal) {
        *error = nsErrorLocal;
        CFReleaseNull(cfError);
    }
    else {
        BridgeCFErrorToNSErrorOut(error, cfError);
    }

    return key;
}

@end

@implementation SecDbKeychainItemV7 {
    SecDbKeychainSecretData* _encryptedSecretData;
    SecDbKeychainMetadata* _encryptedMetadata;
    NSDictionary* _secretAttributes;
    NSDictionary* _metadataAttributes;
    NSString* _tamperCheck;
    keyclass_t _keyclass;
    keybag_handle_t _keybag;
}

@synthesize keyclass = _keyclass;

+ (bool)aksEncryptWithKeybag:(keybag_handle_t)keybag keyclass:(keyclass_t)keyclass keyData:(NSData*)keyData outKeyclass:(keyclass_t*)outKeyclass wrappedKey:(NSMutableData*)wrappedKey error:(NSError**)error
{
    CFErrorRef cfError = NULL;
    bool result = ks_crypt(kAKSKeyOpEncrypt, keybag, keyclass, (uint32_t)keyData.length, keyData.bytes, outKeyclass, (__bridge CFMutableDataRef)wrappedKey, &cfError);
    BridgeCFErrorToNSErrorOut(error, cfError);
    return result;
}

+ (bool)aksDecryptWithKeybag:(keybag_handle_t)keybag keyclass:(keyclass_t)keyclass wrappedKeyData:(NSData*)wrappedKeyData outKeyclass:(keyclass_t*)outKeyclass unwrappedKey:(NSMutableData*)unwrappedKey error:(NSError**)error
{
    CFErrorRef cfError = NULL;
    bool result = ks_crypt(kAKSKeyOpDecrypt, keybag, keyclass, (uint32_t)wrappedKeyData.length, wrappedKeyData.bytes, outKeyclass, (__bridge CFMutableDataRef)unwrappedKey, &cfError);
    BridgeCFErrorToNSErrorOut(error, cfError);
    return result;
}

// bring back with <rdar://problem/37523001>
#if 0
+ (bool)isKeychainUnlocked
{
    return kc_is_unlocked();
}
#endif

- (instancetype)initWithData:(NSData*)data decryptionKeybag:(keybag_handle_t)decryptionKeybag error:(NSError**)error
{
    if (self = [super init]) {
        SecDbKeychainSerializedItemV7* serializedItem = [[SecDbKeychainSerializedItemV7 alloc] initWithData:data];
        if (serializedItem) {
            _keybag = decryptionKeybag;
            _encryptedSecretData = [[SecDbKeychainSecretData alloc] initWithData:serializedItem.encryptedSecretData];
            _encryptedMetadata = [[SecDbKeychainMetadata alloc] initWithData:serializedItem.encryptedMetadata];
            _keyclass = serializedItem.keyclass;
            if (![_encryptedSecretData.tamperCheck isEqualToString:_encryptedMetadata.tamperCheck]) {
                self = nil;
            }
        }
        else {
            self = nil;
        }
    }

    if (!self && error) {
        *error = [NSError errorWithDomain:(id)kCFErrorDomainOSStatus code:errSecItemNotFound userInfo:@{NSLocalizedDescriptionKey : @"failed to deserialize keychain item blob"}];
    }

    return self;
}

- (instancetype)initWithSecretAttributes:(NSDictionary*)secretAttributes metadataAttributes:(NSDictionary*)metadataAttributes tamperCheck:(NSString*)tamperCheck keyclass:(keyclass_t)keyclass
{
    NSParameterAssert(tamperCheck);

    if (self = [super init]) {
        _secretAttributes = secretAttributes ? secretAttributes.copy : [NSDictionary dictionary];
        _metadataAttributes = metadataAttributes ? metadataAttributes.copy : [NSDictionary dictionary];
        _tamperCheck = tamperCheck.copy;
        _keyclass = keyclass;
    }

    return self;
}

+ (SFAESKeySpecifier*)keySpecifier
{
    static SFAESKeySpecifier* keySpecifier = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        keySpecifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    });

    return keySpecifier;
}

+ (SFAuthenticatedEncryptionOperation*)encryptionOperation
{
    static SFAuthenticatedEncryptionOperation* encryptionOperation = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        encryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[self keySpecifier]];
    });

    return encryptionOperation;
}

+ (SFAuthenticatedEncryptionOperation*)decryptionOperation
{
    static SFAuthenticatedEncryptionOperation* decryptionOperation = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        decryptionOperation = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[self keySpecifier]];
    });

    return decryptionOperation;
}

- (NSDictionary*)metadataAttributesWithError:(NSError**)error
{
    if (!_metadataAttributes) {
        SFAESKey* metadataClassKey = [self metadataClassKeyWithKeybag:_keybag
                                                   createKeyIfMissing:false
                                                  overwriteCorruptKey:false
                                                                error:error];
        if (metadataClassKey) {
            NSError* localError = nil;
            NSData* keyData = [[self.class decryptionOperation] decrypt:_encryptedMetadata.wrappedKey withKey:metadataClassKey error:&localError];
            if (!keyData) {
                secerror("SecDbKeychainItemV7: error unwrapping item metadata key (class %d, bag %d): %@", (int)self.keyclass, _keybag, localError);
                // TODO: track this in LocalKeychainAnalytics
                if (error) {
                    CFErrorRef secError = (CFErrorRef)CFBridgingRetain(localError); // this makes localError become the underlying error
                    SecError(errSecDecode, &secError, CFSTR("failed to unwrap item metadata key"));
                    *error = CFBridgingRelease(secError);
                }
                return nil;
            }
            SFAESKey* key = [[SFAESKey alloc] initWithData:keyData specifier:[self.class keySpecifier] error:error];
            if (!key) {
                return nil;
            }

            NSData* metadata = [[self.class decryptionOperation] decrypt:_encryptedMetadata.ciphertext withKey:key error:&localError];
            if (!metadata) {
                secerror("SecDbKeychainItemV7: error decrypting metadata content: %@", localError);
                if (error) {
                    CFErrorRef secError = (CFErrorRef)CFBridgingRetain(localError); // this makes localError become the underlying error
                    SecError(errSecDecode, &secError, CFSTR("failed to decrypt item metadata contents"));
                    *error = CFBridgingRelease(secError);
                }
                return nil;
            }
            NSMutableDictionary* decryptedAttributes = dictionaryFromDERData(metadata).mutableCopy;
            NSString* tamperCheck = decryptedAttributes[SecDBTamperCheck];
            if ([tamperCheck isEqualToString:_encryptedMetadata.tamperCheck]) {
                [decryptedAttributes removeObjectForKey:SecDBTamperCheck];
                _metadataAttributes = decryptedAttributes;
            }
            else {
                secerror("SecDbKeychainItemV7: tamper check failed for metadata decryption, expected %@ found %@", tamperCheck, _encryptedMetadata.tamperCheck);
                if (error) {
                    CFErrorRef secError = NULL;
                    SecError(errSecDecode, &secError, CFSTR("tamper check failed for metadata decryption"));
                    *error = CFBridgingRelease(secError);
                }
            }
        }
    }

    return _metadataAttributes;
}

- (NSDictionary*)secretAttributesWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error
{
    if (!_secretAttributes) {
        SFAESKey* key = [self unwrapFromAKS:_encryptedSecretData.wrappedKey accessControl:accessControl acmContext:acmContext callerAccessGroups:callerAccessGroups delete:NO error:error];
        if (key) {
            NSError* localError = nil;
            NSData* secretDataWithPadding = [[self.class decryptionOperation] decrypt:_encryptedSecretData.ciphertext withKey:key error:&localError];
            if (!secretDataWithPadding) {
                secerror("SecDbKeychainItemV7: error decrypting item secret data contents: %@", localError);
                if (error) {
                    CFErrorRef secError = (CFErrorRef)CFBridgingRetain(localError); // this makes localError become the underlying error
                    SecError(errSecDecode, &secError, CFSTR("error decrypting item secret data contents"));
                    *error = CFBridgingRelease(secError);
                }
                return nil;
            }
            int8_t paddingLength = *((int8_t*)secretDataWithPadding.bytes + secretDataWithPadding.length - 1);
            NSData* secretDataWithoutPadding = [secretDataWithPadding subdataWithRange:NSMakeRange(0, secretDataWithPadding.length - paddingLength)];

            NSMutableDictionary* decryptedAttributes = dictionaryFromDERData(secretDataWithoutPadding).mutableCopy;
            NSString* tamperCheck = decryptedAttributes[SecDBTamperCheck];
            if ([tamperCheck isEqualToString:_encryptedSecretData.tamperCheck]) {
                [decryptedAttributes removeObjectForKey:SecDBTamperCheck];
                _secretAttributes = decryptedAttributes;
            }
            else {
                secerror("SecDbKeychainItemV7: tamper check failed for secret data decryption, expected %@ found %@", tamperCheck, _encryptedMetadata.tamperCheck);
            }
        }
    }

    return _secretAttributes;
}

- (BOOL)deleteWithAcmContext:(NSData*)acmContext accessControl:(SecAccessControlRef)accessControl callerAccessGroups:(NSArray*)callerAccessGroups error:(NSError**)error
{
    NSError* localError = nil;
    (void)[self unwrapFromAKS:_encryptedSecretData.wrappedKey accessControl:accessControl acmContext:acmContext callerAccessGroups:callerAccessGroups delete:YES error:&localError];
    if (localError) {
        secerror("SecDbKeychainItemV7: failed to delete item secret key from aks");
        if (error) {
            *error = localError;
        }

        return NO;
    }

    return YES;
}

- (NSData*)encryptedBlobWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    NSError* localError = nil;
    BOOL success = [self encryptMetadataWithKeybag:keybag error:&localError];
    if (!success || !_encryptedMetadata || localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    success = [self encryptSecretDataWithKeybag:keybag accessControl:accessControl acmContext:acmContext error:&localError];
    if (!success || !_encryptedSecretData || localError) {
        if (error) {
            *error = localError;
        }
        return nil;
    }

    SecDbKeychainSerializedItemV7* serializedItem = [[SecDbKeychainSerializedItemV7 alloc] init];
    serializedItem.encryptedMetadata = self.encryptedMetadataBlob;
    serializedItem.encryptedSecretData = self.encryptedSecretDataBlob;
    serializedItem.keyclass = _keyclass;
    return serializedItem.data;
}

- (NSData*)encryptedMetadataBlob
{
    return _encryptedMetadata.serializedRepresentation;
}

- (NSData*)encryptedSecretDataBlob
{
    return _encryptedSecretData.serializedRepresentation;
}

- (BOOL)encryptMetadataWithKeybag:(keybag_handle_t)keybag error:(NSError**)error
{
    SFAESKey* key = [[SFAESKey alloc] initRandomKeyWithSpecifier:[self.class keySpecifier] error:error];
    if (!key) {
        return NO;
    }
    SFAuthenticatedEncryptionOperation* encryptionOperation = [self.class encryptionOperation];

    NSMutableDictionary* attributesToEncrypt = _metadataAttributes.mutableCopy;
    attributesToEncrypt[SecDBTamperCheck] = _tamperCheck;
    NSData* metadata = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)attributesToEncrypt, NULL);
    SFAuthenticatedCiphertext* ciphertext = [encryptionOperation encrypt:metadata withKey:key error:error];

    SFAESKey* metadataClassKey = [self metadataClassKeyWithKeybag:keybag
                                               createKeyIfMissing:true
                                              overwriteCorruptKey:true
                                                            error:error];
    if (metadataClassKey) {
        SFAuthenticatedCiphertext* wrappedKey = [encryptionOperation encrypt:key.keyData withKey:metadataClassKey error:error];
        _encryptedMetadata = [[SecDbKeychainMetadata alloc] initWithCiphertext:ciphertext wrappedKey:wrappedKey tamperCheck:_tamperCheck error:error];
    }

    return _encryptedMetadata != nil;
}

- (BOOL)encryptSecretDataWithKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    SFAESKey* key = [[SFAESKey alloc] initRandomKeyWithSpecifier:[self.class keySpecifier] error:error];
    if (!key) {
        return NO;
    }
    SFAuthenticatedEncryptionOperation* encryptionOperation = [self.class encryptionOperation];

    NSMutableDictionary* attributesToEncrypt = _secretAttributes.mutableCopy;
    attributesToEncrypt[SecDBTamperCheck] = _tamperCheck;
    NSMutableData* secretData = [(__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)attributesToEncrypt, NULL) mutableCopy];

    int8_t paddingLength = KEYCHAIN_ITEM_PADDING_MODULUS - (secretData.length % KEYCHAIN_ITEM_PADDING_MODULUS);
    int8_t paddingBytes[KEYCHAIN_ITEM_PADDING_MODULUS];
    for (int i = 0; i < KEYCHAIN_ITEM_PADDING_MODULUS; i++) {
        paddingBytes[i] = paddingLength;
    }
    [secretData appendBytes:paddingBytes length:paddingLength];

    SFAuthenticatedCiphertext* ciphertext = [encryptionOperation encrypt:secretData withKey:key error:error];
    SecDbKeychainAKSWrappedKey* wrappedKey = [self wrapToAKS:key withKeybag:keybag accessControl:accessControl acmContext:acmContext error:error];

    _encryptedSecretData = [[SecDbKeychainSecretData alloc] initWithCiphertext:ciphertext wrappedKey:wrappedKey tamperCheck:_tamperCheck error:error];
    return _encryptedSecretData != nil;
}

- (SFAESKey*)metadataClassKeyWithKeybag:(keybag_handle_t)keybag
                     createKeyIfMissing:(bool)createIfMissing
                    overwriteCorruptKey:(bool)force
                                  error:(NSError**)error
{
    return [[SecDbKeychainMetadataKeyStore sharedStore] keyForKeyclass:_keyclass
                                                                keybag:keybag
                                                          keySpecifier:[self.class keySpecifier]
                                                    createKeyIfMissing:(bool)createIfMissing
                                                   overwriteCorruptKey:force
                                                                 error:error];
}

- (SecDbKeychainAKSWrappedKey*)wrapToAKS:(SFAESKey*)key withKeybag:(keybag_handle_t)keybag accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext error:(NSError**)error
{
    NSData* keyData = key.keyData;

#if USE_KEYSTORE
    NSDictionary* constraints = (__bridge NSDictionary*)SecAccessControlGetConstraints(accessControl);
    if (constraints) {
        aks_ref_key_t refKey = NULL;
        CFErrorRef cfError = NULL;
        NSData* authData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)@{(id)kAKSKeyAcl : constraints}, &cfError);

        if (!acmContext || !SecAccessControlIsBound(accessControl)) {
            secerror("SecDbKeychainItemV7: access control error");
            if (error) {
                CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
                ks_access_control_needed_error(&cfError, accessControlData, SecAccessControlIsBound(accessControl) ? kAKSKeyOpEncrypt : CFSTR(""));
                CFReleaseNull(accessControlData);
            }

            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        void* aksParams = NULL;
        size_t aksParamsLength = 0;
        aks_operation_optional_params(0, 0, authData.bytes, authData.length, acmContext.bytes, (int)acmContext.length, &aksParams, &aksParamsLength);

        int aksResult = aks_ref_key_create(keybag, _keyclass, key_type_sym, aksParams, aksParamsLength, &refKey);
        if (aksResult != 0) {
            CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
            create_cferror_from_aks(aksResult, kAKSKeyOpEncrypt, keybag, _keyclass, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
            CFReleaseNull(accessControlData);
            free(aksParams);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        size_t wrappedKeySize = 0;
        void* wrappedKeyBytes = NULL;
        aksResult = aks_ref_key_encrypt(refKey, aksParams, aksParamsLength, keyData.bytes, keyData.length, &wrappedKeyBytes, &wrappedKeySize);
        if (aksResult != 0) {
            CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
            create_cferror_from_aks(aksResult, kAKSKeyOpEncrypt, keybag, _keyclass, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
            CFReleaseNull(accessControlData);
            free(aksParams);
            aks_ref_key_free(&refKey);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }
        free(aksParams);

        BridgeCFErrorToNSErrorOut(error, cfError);

        NSData* wrappedKey = [[NSData alloc] initWithBytesNoCopy:wrappedKeyBytes length:wrappedKeySize];

        size_t refKeyBlobLength = 0;
        const void* refKeyBlobBytes = aks_ref_key_get_blob(refKey, &refKeyBlobLength);
        NSData* refKeyBlob = [[NSData alloc] initWithBytesNoCopy:(void*)refKeyBlobBytes length:refKeyBlobLength];
        aks_ref_key_free(&refKey);
        return [[SecDbKeychainAKSWrappedKey alloc] initRefKeyWrappedKeyWithData:wrappedKey refKeyBlob:refKeyBlob];
    }
    else {
        NSMutableData* wrappedKey = [[NSMutableData alloc] initWithLength:(size_t)keyData.length + 40];
        bool success = [self.class aksEncryptWithKeybag:keybag keyclass:_keyclass keyData:keyData outKeyclass:&_keyclass wrappedKey:wrappedKey error:error];
        return success ? [[SecDbKeychainAKSWrappedKey alloc] initRegularWrappedKeyWithData:wrappedKey] : nil;
    }
#else
    NSMutableData* wrappedKey = [[NSMutableData alloc] initWithLength:(size_t)keyData.length + 40];
    bool success = [self.class aksEncryptWithKeybag:keybag keyclass:_keyclass keyData:keyData outKeyclass:&_keyclass wrappedKey:wrappedKey error:error];
    return success ? [[SecDbKeychainAKSWrappedKey alloc] initRegularWrappedKeyWithData:wrappedKey] : nil;
#endif
}

- (SFAESKey*)unwrapFromAKS:(SecDbKeychainAKSWrappedKey*)wrappedKey accessControl:(SecAccessControlRef)accessControl acmContext:(NSData*)acmContext callerAccessGroups:(NSArray*)callerAccessGroups delete:(BOOL)delete error:(NSError**)error
{
    NSData* wrappedKeyData = wrappedKey.wrappedKey;

    if (wrappedKey.type == SecDbKeychainAKSWrappedKeyTypeRegular) {
        NSMutableData* unwrappedKey = [NSMutableData dataWithCapacity:wrappedKeyData.length + 40];
        unwrappedKey.length = wrappedKeyData.length + 40;
        bool result = [self.class aksDecryptWithKeybag:_keybag keyclass:_keyclass wrappedKeyData:wrappedKeyData outKeyclass:&_keyclass unwrappedKey:unwrappedKey error:error];
        if (result) {
            return [[SFAESKey alloc] initWithData:unwrappedKey specifier:[self.class keySpecifier] error:error];
        }
        else {
            return nil;
        }
    }
#if USE_KEYSTORE
    else if (wrappedKey.type == SecDbKeychainAKSWrappedKeyTypeRefKey) {
        aks_ref_key_t refKey = NULL;
        aks_ref_key_create_with_blob(_keybag, wrappedKey.refKeyBlob.bytes, wrappedKey.refKeyBlob.length, &refKey);

        CFErrorRef cfError = NULL;
        size_t refKeyExternalDataLength = 0;
        const uint8_t* refKeyExternalDataBytes = aks_ref_key_get_external_data(refKey, &refKeyExternalDataLength);
        if (!refKeyExternalDataBytes) {
            aks_ref_key_free(&refKey);
            return nil;
        }
        NSDictionary* aclDict = nil;
        der_decode_plist(NULL, kCFPropertyListImmutable, (CFPropertyListRef*)(void*)&aclDict, &cfError, refKeyExternalDataBytes, refKeyExternalDataBytes + refKeyExternalDataLength);
        if (!aclDict) {
            SecError(errSecDecode, &cfError, CFSTR("SecDbKeychainItemV7: failed to decode acl dict"));
        }
        SecAccessControlSetConstraints(accessControl, (__bridge CFDictionaryRef)aclDict);
        if (!SecAccessControlGetConstraint(accessControl, kAKSKeyOpEncrypt)) {
            SecAccessControlAddConstraintForOperation(accessControl, kAKSKeyOpEncrypt, kCFBooleanTrue, &cfError);
        }

        size_t derPlistLength = der_sizeof_plist((__bridge CFPropertyListRef)callerAccessGroups, &cfError);
        NSMutableData* accessGroupDERData = [[NSMutableData alloc] initWithLength:derPlistLength];
        der_encode_plist((__bridge CFPropertyListRef)callerAccessGroups, &cfError, accessGroupDERData.mutableBytes, accessGroupDERData.mutableBytes + derPlistLength);
        void* aksParams = NULL;
        size_t aksParamsLength = 0;
        aks_operation_optional_params(accessGroupDERData.bytes, derPlistLength, NULL, 0, acmContext.bytes, (int)acmContext.length, &aksParams, &aksParamsLength);

        void* unwrappedKeyDERData = NULL;
        size_t unwrappedKeyDERLength = 0;
        int aksResult = aks_ref_key_decrypt(refKey, aksParams, aksParamsLength, wrappedKeyData.bytes, wrappedKeyData.length, &unwrappedKeyDERData, &unwrappedKeyDERLength);
        if (aksResult != 0) {
            CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
            create_cferror_from_aks(aksResult, kAKSKeyOpDecrypt, 0, 0, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
            CFReleaseNull(accessControlData);
            aks_ref_key_free(&refKey);
            free(aksParams);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }
        if (!unwrappedKeyDERData) {
            SecError(errSecDecode, &cfError, CFSTR("SecDbKeychainItemV7: failed to decrypt item, Item can't be decrypted due to failed decode der, so drop the item."));
            aks_ref_key_free(&refKey);
            free(aksParams);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        CFPropertyListRef unwrappedKeyData = NULL;
        der_decode_plist(NULL, kCFPropertyListImmutable, &unwrappedKeyData, &cfError, unwrappedKeyDERData, unwrappedKeyDERData + unwrappedKeyDERLength);
        SFAESKey* result = nil;
        if ([(__bridge NSData*)unwrappedKeyData isKindOfClass:[NSData class]]) {
            result = [[SFAESKey alloc] initWithData:(__bridge NSData*)unwrappedKeyData specifier:[self.class keySpecifier] error:error];
            CFReleaseNull(unwrappedKeyDERData);
        }
        else {
            SecError(errSecDecode, &cfError, CFSTR("SecDbKeychainItemV7: failed to decrypt item, Item can't be decrypted due to failed decode der, so drop the item."));
            aks_ref_key_free(&refKey);
            free(aksParams);
            free(unwrappedKeyDERData);
            BridgeCFErrorToNSErrorOut(error, cfError);
            return nil;
        }

        if (delete) {
            aksResult = aks_ref_key_delete(refKey, aksParams, aksParamsLength);
            if (aksResult != 0) {
                CFDataRef accessControlData = SecAccessControlCopyData(accessControl);
                create_cferror_from_aks(aksResult, kAKSKeyOpDelete, 0, 0, accessControlData, (__bridge CFDataRef)acmContext, &cfError);
                CFReleaseNull(accessControlData);
                aks_ref_key_free(&refKey);
                free(aksParams);
                free(unwrappedKeyDERData);
                BridgeCFErrorToNSErrorOut(error, cfError);
                return nil;
            }
        }

        BridgeCFErrorToNSErrorOut(error, cfError);
        aks_ref_key_free(&refKey);
        free(aksParams);
        free(unwrappedKeyDERData);
        return result;
    }
#endif
    else {
        return nil;
    }
}

@end

#endif // TARGET_OS_BRIDGE
