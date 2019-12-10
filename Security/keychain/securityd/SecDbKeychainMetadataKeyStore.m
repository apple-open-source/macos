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

#import "SecDbKeychainMetadataKeyStore.h"
#import <dispatch/dispatch.h>
#import <utilities/SecAKSWrappers.h>
#import <notify.h>
#import "SecItemServer.h"
#import "SecAKSObjCWrappers.h"
#import "sec_action.h"

static SecDbKeychainMetadataKeyStore* sharedStore = nil;
static dispatch_queue_t sharedMetadataStoreQueue;
static void initializeSharedMetadataStoreQueue(void) {
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedMetadataStoreQueue = dispatch_queue_create("metadata_store", DISPATCH_QUEUE_SERIAL);
    });
}

@interface SecDbKeychainMetadataKeyStore ()
@property dispatch_queue_t queue;
@property NSMutableDictionary* keysDict;
@property int keybagNotificationToken;
@end

@implementation SecDbKeychainMetadataKeyStore

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
        _keybagNotificationToken = NOTIFY_TOKEN_INVALID;

        __weak __typeof(self) weakSelf = self;
        notify_register_dispatch(kUserKeybagStateChangeNotification, &_keybagNotificationToken, _queue, ^(int inToken) {
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

- (void)dealloc {
    if (_keybagNotificationToken != NOTIFY_TOKEN_INVALID) {
        notify_cancel(_keybagNotificationToken);
        _keybagNotificationToken = NOTIFY_TOKEN_INVALID;
    }
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

    keyclass = SecAKSSanitizedKeyclass(keyclass);

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
                        ok &= [SecAKSObjCWrappers aksDecryptWithKeybag:keybag keyclass:keyclassForUnwrapping ciphertext:wrappedKeyData outKeyclass:NULL plaintext:unwrappedKeyData error:&nsErrorLocal];
                        if (ok) {
                            key = [[SFAESKey alloc] initWithData:unwrappedKeyData specifier:keySpecifier error:&nsErrorLocal];

                            if(!key) {
                                if (__security_simulatecrash_enabled()) {
                                    os_log_fault(secLogObjForScope("SecDbKeychainItemV7"), "Metadata class key (%d) decrypted, but didn't become a key: %@", keyclass, nsErrorLocal);
                                }
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
                            ok = [SecAKSObjCWrappers aksDecryptWithKeybag:keybag keyclass:keyrolledKeyclass ciphertext:wrappedKeyData outKeyclass:NULL plaintext:unwrappedKeyData error:&retryError];

                            if (ok) {
                                secerror("SecDbKeychainItemV7: successfully decrypted metadata key using keyrolled keyclass %d", keyrolledKeyclass);
                                key = [[SFAESKey alloc] initWithData:unwrappedKeyData specifier:keySpecifier error:&retryError];

                                if(!key) {
                                    if (__security_simulatecrash_enabled()) {
                                        os_log_fault(secLogObjForScope("SecDbKeychainItemV7"),
                                                     "Metadata class key (%d) decrypted using keyrolled keyclass %d, but didn't become a key: %@",
                                                     keyclass, keyrolledKeyclass, retryError);
                                    }
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
                                    if (__security_simulatecrash_enabled()) {
                                        os_log_fault(secLogObjForScope("SecDbKeychainItemV7"), "Metadata class key (%d) failed to decrypt: %@", keyclass, nsErrorLocal);
                                    }
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
                        ok &= [SecAKSObjCWrappers aksEncryptWithKeybag:keybag keyclass:keyclass plaintext:key.keyData outKeyclass:&outKeyclass ciphertext:wrappedKey error:&nsErrorLocal];
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
