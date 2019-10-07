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

#import "SecDbBackupManager.h"

NSString* const KeychainBackupsErrorDomain = @"com.apple.security.keychain.backups";

// oink oink
@implementation SecDbBackupWrappedItemKey
+ (BOOL)supportsSecureCoding {
    return YES;
}
- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    [coder encodeObject:self.wrappedKey forKey:@"wrappedKey"];
    [coder encodeObject:self.baguuid forKey:@"baguuid"];
}

- (nullable instancetype)initWithCoder:(nonnull NSCoder *)coder {
    if (self = [super init]) {
        _wrappedKey = [coder decodeObjectOfClass:[NSData class] forKey:@"wrappedKey"];
        _baguuid = [coder decodeObjectOfClass:[NSData class] forKey:@"baguuid"];
    }
    return self;
}
@end

#if !SECDB_BACKUPS_ENABLED

@implementation SecDbBackupManager

+ (instancetype)manager
{
    return nil;
}

- (void)verifyBackupIntegrity:(bool)lightweight
                   completion:(void (^)(NSDictionary<NSString*, NSString*>* results, NSError* _Nullable error))completion
{
    completion(nil, [NSError errorWithDomain:KeychainBackupsErrorDomain
                                        code:SecDbBackupNotSupported
                                    userInfo:@{NSLocalizedDescriptionKey : @"platform doesn't do backups"}]);
}

- (SecDbBackupWrappedItemKey* _Nullable)wrapItemKey:(id)key forKeyclass:(keyclass_t)keyclass error:(NSError**)error
{
    return nil;
}

@end

bool SecDbBackupCreateOrLoadBackupInfrastructure(CFErrorRef* error)
{
    return true;
}

#else   // SECDB_BACKUPS_ENABLED is true, roll out the code

#import "SecDbBackupManager_Internal.h"
#include <CommonCrypto/CommonRandom.h>
#import <Foundation/NSData_Private.h>
#import "SecItemServer.h"
#import "SecItemDb.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "sec_action.h"
#import "SecItemServer.h"
#include "utilities/der_plist.h"

// TODO: fire off metric on outcome
bool SecDbBackupCreateOrLoadBackupInfrastructure(CFErrorRef* error)
{
    NSError* localError;
    bool ok = [[SecDbBackupManager manager] createOrLoadBackupInfrastructure:&localError];
    if (!ok) {
        // Translate this to intelligible constant in C, but other errors can be passed along
        if (localError.code == SecDbBackupKeychainLocked) {
            *error = CFErrorCreate(kCFAllocatorDefault, kSecErrorDomain, errSecInteractionNotAllowed, NULL);
        } else {
            *error = (CFErrorRef)CFBridgingRetain(localError);
        }
    }
    return ok;
}

// Reading from disk is relatively expensive. Keep wrapped key in memory and just delete the unwrapped copy on lock
@interface InMemoryKCSK : NSObject
@property aks_ref_key_t refKey;
@property (nonatomic) NSData* wrappedKey;
@property (nonatomic) SFECKeyPair* key;
@end

@implementation InMemoryKCSK
- (void)dealloc
{
    if (_refKey) {
        free(_refKey);
    }
}

+ (instancetype)kcskWithRefKey:(aks_ref_key_t)refKey wrappedKey:(NSData*)wrappedKey key:(SFECKeyPair*)key
{
    InMemoryKCSK* kcsk = [InMemoryKCSK new];
    kcsk.refKey = refKey;
    kcsk.wrappedKey = wrappedKey;
    kcsk.key = key;
    return kcsk;
}

@end

@interface SecDbBackupManager () {
    dispatch_queue_t _queue;
    keybag_handle_t _handle;
    SecDbBackupBagIdentity* _bagIdentity;
    NSMutableDictionary<NSNumber*, InMemoryKCSK*>* _cachedKCSKs;
}
@end

@implementation SecDbBackupManager

#pragma mark - Misc Helpers

- (NSData*)getSHA256OfData:(NSData*)data
{
    NSMutableData* digest = [[NSMutableData alloc] initWithLength:CC_SHA512_DIGEST_LENGTH];
    if (!CC_SHA512(data.bytes, (CC_LONG)data.length, digest.mutableBytes)) {
        return nil;
    }
    return digest;
}

- (void)setBagIdentity:(SecDbBackupBagIdentity *)bagIdentity
{
    _bagIdentity = bagIdentity;
}

- (SecDbBackupBagIdentity*)bagIdentity
{
    return _bagIdentity;
}

- (bool)fillError:(NSError**)error code:(enum SecDbBackupErrorCode)code underlying:(NSError*)underlying description:(NSString*)format, ... NS_FORMAT_FUNCTION(4, 5)
{
    if (error) {
        va_list ap;
        va_start(ap, format);
        NSString* desc = [[NSString alloc] initWithFormat:format arguments:ap];
        va_end(ap);
        if (underlying) {
            *error = [NSError errorWithDomain:KeychainBackupsErrorDomain code:code description:desc underlying:underlying];
        } else {
            *error = [NSError errorWithDomain:KeychainBackupsErrorDomain code:code description:desc];
        }
    }

    // analyzer gets upset when a method taking an error** doesn't return a value
    return true;
}

static SecDbBackupManager* staticManager;
+ (instancetype)manager
{
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        staticManager = [SecDbBackupManager new];
    });
    return staticManager;
}

// Testing only please
+ (void)resetManager
{
    if (staticManager) {
        staticManager = [SecDbBackupManager new];
    }
}

- (instancetype)init
{
    if (!checkV12DevEnabled()) {
        return nil;
    }
    if (self = [super init]) {
        _queue = dispatch_queue_create("com.apple.security.secdbbackupmanager", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _handle = bad_keybag_handle;
        _cachedKCSKs = [NSMutableDictionary new];
    }
    return self;
}

- (SFECKeyPair*)getECKeyPairFromDERBytes:(void*)bytes length:(size_t)len error:(NSError**)error
{
    if (!bytes || len == 0) {
        [self fillError:error code:SecDbBackupInvalidArgument underlying:nil description:@"Need valid byte buffer to make EC keypair from"];
        return nil;
    }
    CFTypeRef cftype = NULL;
    CFErrorRef cferr = NULL;
    const uint8_t* derp = der_decode_plist(kCFAllocatorDefault, NSPropertyListImmutable, &cftype, &cferr, bytes, bytes + len);
    free(bytes);
    if (derp == NULL || derp != (bytes + len) || cftype == NULL) {
        [self fillError:error code:SecDbBackupMalformedKCSKDataOnDisk underlying:CFBridgingRelease(cferr) description:@"Unable to parse der data"];
        return nil;
    }

    return [[SFECKeyPair alloc] initWithData:(__bridge_transfer NSData*)cftype
                                   specifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]
                                       error:error];
}

#pragma mark - Fixup And Verification

- (void)verifyBackupIntegrity:(bool)lightweight
                   completion:(void (^)(NSDictionary<NSString*, NSString*>* _Nonnull, NSError * _Nullable))completion
{
    NSError* error = nil;
    completion(@{@"summary" : @"Unimplemented"}, error);
}

#pragma mark - Backup Bag Management

// Get the bag's UUID from AKS and the hash from provided data. This must always be the original bag's data
- (SecDbBackupBagIdentity*)bagIdentityWithHandle:(keybag_handle_t)handle data:(NSData*)data error:(NSError**)error {
    assert(data);
    secnotice("SecDbBackup", "Getting bag identity");
    SecDbBackupBagIdentity* identity = [SecDbBackupBagIdentity new];

    uuid_t uuid = {0};
    kern_return_t aksResult = aks_get_bag_uuid(handle, uuid);
    if (aksResult != kAKSReturnSuccess) {
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil description:@"Unable to get keybag UUID (%d)", aksResult];
        return nil;
    }
    identity.baguuid = [NSData dataWithBytes:uuid length:16];

    NSData* digest = [self getSHA256OfData:data];
    if (!digest) {
        [self fillError:error code:SecDbBackupCryptoFailure underlying:nil description:@"CC_SHA512 returned failure, can't get bag hash"];
        return nil;
    }
    identity.baghash = digest;

    secnotice("SecDbBackup", "Obtained bag identity: %@", identity);

    return identity;
}

- (NSData*)createBackupBagSecret:(NSError**)error
{
    uint8_t* data = calloc(1, BACKUPBAG_PASSPHRASE_LENGTH);
    if (!data) {
        return nil;     // Good luck allocating an error message
    }

    CCRNGStatus rngResult = CCRandomGenerateBytes(data, BACKUPBAG_PASSPHRASE_LENGTH);
    if (rngResult != kCCSuccess) {
        [self fillError:error code:SecDbBackupCryptoFailure underlying:nil description:@"Unable to generate random bytes (%d)", rngResult];
        return nil;
    }

    NSData* secret = [NSData _newZeroingDataWithBytesNoCopy:data length:BACKUPBAG_PASSPHRASE_LENGTH deallocator:NSDataDeallocatorNone];
    return secret;
}

- (keybag_handle_t)onQueueCreateBackupBagWithSecret:(NSData*)secret error:(NSError**)error
{
    dispatch_assert_queue(_queue);

    keybag_handle_t handle = bad_keybag_handle;
    kern_return_t aksresult = aks_create_bag(secret.bytes, BACKUPBAG_PASSPHRASE_LENGTH, kAppleKeyStoreAsymmetricBackupBag, &handle);
    if (aksresult != kAKSReturnSuccess) {
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil description:@"Unable to create keybag (%d)", aksresult];
        return bad_keybag_handle;
    }

    // Make secret keys unavailable. Causes pubkeys to be destroyed so reload bag before use
    aksresult = aks_lock_bag(handle);
    if (aksresult != kAKSReturnSuccess) {   // This would be rather surprising
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil description:@"Unable to lock keybag (%d)", aksresult];
        aks_unload_bag(handle);
        return bad_keybag_handle;
    }

    return handle;
}

- (BOOL)onQueueInTransaction:(SecDbConnectionRef)dbt saveBackupBag:(keybag_handle_t)handle asDefault:(BOOL)asDefault error:(NSError**)error
{
    dispatch_assert_queue(_queue);

    void* buf = NULL;
    int buflen = 0;
    kern_return_t aksResult = aks_save_bag(handle, &buf, &buflen);
    if (aksResult != kAKSReturnSuccess) {
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil description:@"Unable to serialize keybag (%d)", aksResult];
        return NO;
    }
    NSData* bagData = [NSData dataWithBytesNoCopy:buf length:buflen];

    SecDbBackupBagIdentity* bagIdentity = [self bagIdentityWithHandle:handle data:bagData error:error];
    if (!bagIdentity) {
        return NO;
    }
    SecDbBackupBag* bag = [SecDbBackupBag new];
    bag.bagIdentity = bagIdentity;
    bag.keybag = bagData;

    __block CFErrorRef cfError = NULL;
    __block bool ok = true;
    if (asDefault) {
        ok &= SecDbPrepare(dbt, CFSTR("UPDATE backupbags SET defaultvalue = 0 WHERE defaultvalue = 1"), &cfError, ^(sqlite3_stmt *stmt) {
            ok &= SecDbStep(dbt, stmt, &cfError, ^(bool *stop) {
                // FIXME: Should this be an error? Should something else happen?
                secwarning("SecDbBackup: Marking existing bag as non-default");
            });
        });
    }
    if (!ok) {
        return ok;
    }
    ok &= SecDbPrepare(dbt, CFSTR("INSERT INTO backupbags (backupUUID, backupbag, defaultvalue) VALUES (?,?,?)"), &cfError, ^(sqlite3_stmt *stmt) {
        ok &= SecDbBindObject(stmt, 1, (__bridge CFDataRef)bag.bagIdentity.baguuid, &cfError);
        ok &= SecDbBindObject(stmt, 2, (__bridge CFDataRef)bag.data, &cfError);
        ok &= SecDbBindInt(stmt, 3, asDefault ? 1 : 0, &cfError);
        ok &= SecDbStep(dbt, stmt, &cfError, NULL);
    });

    if (!ok) {
        secerror("SecDbBackup: unable to save keybag to disk: %@", cfError);
        [self fillError:error code:SecDbBackupWriteFailure underlying:CFBridgingRelease(cfError) description:@"Unable to save keybag to disk"];
    }

    return ok;
}

- (keybag_handle_t)onQueueLoadBackupBag:(NSUUID*)uuid error:(NSError**)error {
    dispatch_assert_queue(_queue);

    secnotice("SecDbBackup", "Attempting to load backup bag from disk");

    __block CFErrorRef localErr = NULL;
    __block bool ok = true;
    __block NSData* readUUID;
    __block NSData* readBagData;
    __block unsigned found = 0;
    ok &= kc_with_dbt_non_item_tables(false, &localErr, ^bool(SecDbConnectionRef dbt) {
        NSString* sql = [NSString stringWithFormat:@"SELECT backupUUID, backupbag FROM backupbags WHERE %@", uuid ? @"backupUUID = ?" : @"defaultvalue = 1"];
        ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &localErr, ^(sqlite3_stmt *stmt) {
            if (uuid) {
                unsigned char uuidbytes[UUIDBYTESLENGTH] = {0};
                [uuid getUUIDBytes:uuidbytes];
                ok &= SecDbBindBlob(stmt, 1, uuidbytes, UUIDBYTESLENGTH, SQLITE_TRANSIENT, &localErr);
            }
            ok &= SecDbStep(dbt, stmt, &localErr, ^(bool *stop) {
                if (found > 0) {        // For uuids this should have violated constraints
                    secerror("Encountered more than one backup bag by %@", uuid ? @"backupUUID" : @"defaultvalue");
                    *stop = true;
                }
                readUUID = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 0) length:sqlite3_column_bytes(stmt, 0)];
                readBagData = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 1) length:sqlite3_column_bytes(stmt, 1)];
                ++found;
            });
        });
        return ok;
    });

    if (!ok) {
        secerror("SecDbBackup: Unable to load backup bag from disk: %@", localErr);
        [self fillError:error code:SecDbBackupWriteFailure underlying:CFBridgingRelease(localErr) description:@"Unable to load backup bag from disk"];
        return bad_keybag_handle;
    }

    if (!found) {
        [self fillError:error code:SecDbBackupNoBackupBagFound underlying:nil description:@"No backup bag found to load from disk"];
        return bad_keybag_handle;
    } else if (found > 1) {
        [self fillError:error
                   code:uuid ? SecDbBackupDuplicateBagFound : SecDbBackupMultipleDefaultBagsFound
             underlying:nil
                description:@"More than one backup bag found"];
        return bad_keybag_handle;
    }

    if (!readUUID ||  readUUID.length != UUIDBYTESLENGTH || !readBagData || !readBagData.length) {
        [self fillError:error code:SecDbBackupMalformedBagDataOnDisk underlying:nil description:@"bags read from disk malformed"];
        return bad_keybag_handle;
    }

    SecDbBackupBag* readBag = [[SecDbBackupBag alloc] initWithData:readBagData];
    if (!readBag) {
        [self fillError:error code:SecDbBackupDeserializationFailure underlying:nil description:@"bag from disk does not deserialize"];
        return bad_keybag_handle;
    }

    secnotice("SecDbBackup", "Successfully read backup bag from disk; loading and verifying. Read bag ID: %@", readBag.bagIdentity);

    keybag_handle_t handle = bad_keybag_handle;
    kern_return_t aksResult = aks_load_bag(readBag.keybag.bytes, (int)readBag.keybag.length, &handle);
    if (aksResult != kAKSReturnSuccess) {
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil description:@"Unable to load bag from disk (%d)", aksResult];
        return bad_keybag_handle;
    }

    SecDbBackupBagIdentity* loadedID = [self bagIdentityWithHandle:handle data:readBag.keybag error:error];
    if (!loadedID) {
        aks_unload_bag(handle);
        return bad_keybag_handle;
    }

    if (memcmp(loadedID.baguuid.bytes, readBag.bagIdentity.baguuid.bytes, UUIDBYTESLENGTH) ||
        memcmp(loadedID.baguuid.bytes, readUUID.bytes, UUIDBYTESLENGTH)) {
        [self fillError:error code:SecDbBackupUUIDMismatch underlying:nil description:@"Loaded UUID does not match UUIDs on disk"];
        aks_unload_bag(handle);
        return bad_keybag_handle;
    }

    if (memcmp(loadedID.baghash.bytes, readBag.bagIdentity.baghash.bytes, CC_SHA512_DIGEST_LENGTH)) {
        [self fillError:error code:SecDbBackupDeserializationFailure underlying:nil description:@"Keybag hash does not match its identity's hash"];
        return bad_keybag_handle;
    }

    // TODO: verify that bag is still signed, rdar://problem/46702467

    secnotice("SecDbBackup", "Backup bag loaded and verified.");

    // Must load readBag's identity because the hash from AKS is unstable.
    // This is the hash of the original saved bag and is anchored in the KCSKes.
    _bagIdentity = readBag.bagIdentity;

    return handle;
}

- (BOOL)onQueueReloadDefaultBackupBagWithError:(NSError**)error
{
    if (_handle != bad_keybag_handle) {
        aks_unload_bag(_handle);
    }

    _handle = [self onQueueLoadBackupBag:nil error:error];
    return _handle != bad_keybag_handle;
}

#pragma mark - KCSK Management

- (SecDbBackupKeyClassSigningKey*)createKCSKForKeyClass:(keyclass_t)class withWrapper:(SFAESKey*)wrapper error:(NSError**)error
{
    if (!wrapper) {
        [self fillError:error code:SecDbBackupInvalidArgument underlying:nil description:@"Need wrapper for KCSK"];
        return nil;
    }

    SecDbBackupKeyClassSigningKey* kcsk = [SecDbBackupKeyClassSigningKey new];
    kcsk.keyClass = class;

    SFECKeyPair* keypair = [[SFECKeyPair alloc] initRandomKeyPairWithSpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    kcsk.publicKey = [keypair.publicKey.keyData copy];

    // Create a DER-encoded dictionary of bag identity
    void* der_blob;
    size_t der_len;
    CFErrorRef cfErr = NULL;
    NSDictionary* identDict = @{@"baguuid" : _bagIdentity.baguuid, @"baghash" : _bagIdentity.baghash};
    NSData* identData = (__bridge_transfer NSData*)CFPropertyListCreateDERData(NULL, (__bridge CFDictionaryRef)identDict, &cfErr);
    aks_operation_optional_params(NULL, 0, identData.bytes, identData.length, NULL, 0, &der_blob, &der_len);

    // Create ref key with embedded bag identity DER data
    aks_ref_key_t refkey = NULL;
    kern_return_t aksResult = aks_ref_key_create(KEYBAG_DEVICE, class, key_type_sym, der_blob, der_len, &refkey);
    free(der_blob);
    if (aksResult != kAKSReturnSuccess) {
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil
            description:@"Unable to create AKS ref key for KCSK class %d: %d", class, aksResult];
        return nil;
    }

    size_t refkeyblobsize = 0;
    const uint8_t* refkeyblob = aks_ref_key_get_blob(refkey, &refkeyblobsize);
    kcsk.aksRefKey = [NSData dataWithBytes:refkeyblob length:refkeyblobsize];

    size_t wrappedKeyLen = 0;
    void* wrappedKey = NULL;
    NSData* keypairAsData = keypair.keyData;
    aksResult = aks_ref_key_encrypt(refkey, NULL, 0, keypairAsData.bytes, keypairAsData.length, &wrappedKey, &wrappedKeyLen);
    if (aksResult != kAKSReturnSuccess) {
        [self fillError:error code:SecDbBackupAKSFailure underlying:nil
            description:@"Unable to encrypt KCSK class %d with AKS ref key: %d", class, aksResult];
        return nil;
    }
    aks_ref_key_free(&refkey);
    kcsk.aksWrappedKey = [NSData dataWithBytesNoCopy:wrappedKey length:wrappedKeyLen];

    // Also add DER-encoded bag identity here
    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc] initWithKeySpecifier:[[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256]];
    SFAuthenticatedCiphertext* backupwrapped = [op encrypt:keypair.keyData withKey:wrapper additionalAuthenticatedData:identData error:error];
    kcsk.backupWrappedKey = [NSKeyedArchiver archivedDataWithRootObject:backupwrapped requiringSecureCoding:YES error:error];
    if (!kcsk.backupWrappedKey) {
        return nil;
    }

    return kcsk;
}

- (BOOL)inTransaction:(SecDbConnectionRef)dbt writeKCSKToKeychain:(SecDbBackupKeyClassSigningKey*)kcsk error:(NSError**)error
{
    __block bool ok = true;
    __block CFErrorRef cfError = NULL;
    NSString* sql = @"INSERT INTO backupkeyclasssigningkeys (keyclass, backupUUID, signingkey) VALUES (?, ?, ?)";
    ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &cfError, ^(sqlite3_stmt *stmt) {
        ok &= SecDbBindInt(stmt, 1, kcsk.keyClass, &cfError);
        ok &= SecDbBindObject(stmt, 2, (__bridge CFTypeRef)(self->_bagIdentity.baguuid), &cfError);
        ok &= SecDbBindObject(stmt, 3, (__bridge CFTypeRef)(kcsk.data), &cfError);
        ok &= SecDbStep(dbt, stmt, &cfError, NULL);
    });

    if (!ok) {
        secerror("SecDbBackup: Unable to write KCSK for class %d to keychain: %@", kcsk.keyClass, cfError);
        [self fillError:error code:SecDbBackupWriteFailure underlying:CFBridgingRelease(cfError) description:@"Unable to write KCSK for class %d to keychain", kcsk.keyClass];
    }

    return ok;
}

- (InMemoryKCSK*)onQueueReadKCSKFromDiskForClass:(keyclass_t)keyclass error:(NSError**)error
{
    __block bool ok = true;
    __block CFErrorRef cfError = NULL;
    __block NSData* readUUID;
    __block NSData* readKCSK;
    NSString* sql = @"SELECT backupUUID, signingkey FROM backupkeyclasssigningkeys WHERE keyclass = ?";
    ok &= kc_with_dbt_non_item_tables(NO, &cfError, ^bool(SecDbConnectionRef dbt) {
        ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &cfError, ^(sqlite3_stmt *stmt) {
            ok &= SecDbBindInt(stmt, 1, keyclass, &cfError);
            ok &= SecDbStep(dbt, stmt, &cfError, ^(bool *stop) {
                readUUID = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 0) length:sqlite3_column_bytes(stmt, 0)];
                readKCSK = [[NSData alloc] initWithBytes:sqlite3_column_blob(stmt, 1) length:sqlite3_column_bytes(stmt, 1)];
            });
        });
        return ok;
    });
    if (!readKCSK || !readUUID) {
        [self fillError:error code:SecDbBackupNoKCSKFound underlying:nil description:@"KCSK for class %d not on disk", keyclass];
        return nil;
    }

    SecDbBackupKeyClassSigningKey* kcsk = [[SecDbBackupKeyClassSigningKey alloc] initWithData:readKCSK];
    if (!kcsk) {
        [self fillError:error code:SecDbBackupMalformedKCSKDataOnDisk underlying:nil description:@"Retrieved KCSK blob but it didn't become a KCSK"];
        return nil;
    }

    aks_ref_key_t refkey = NULL;
    kern_return_t aksResult = aks_ref_key_create_with_blob(KEYBAG_DEVICE, kcsk.aksRefKey.bytes, kcsk.aksRefKey.length, &refkey);
    if (aksResult != kAKSReturnSuccess) {
        [self fillError:error
                   code:SecDbBackupAKSFailure
             underlying:nil
            description:@"Failed to create refkey from KCSK blob for class %d: %d", keyclass, aksResult];
        return nil;
    }

    size_t externalDataLen = 0;
    const uint8_t* externalData = aks_ref_key_get_external_data(refkey, &externalDataLen);
    NSData* derBagIdent = [NSData dataWithBytes:externalData length:externalDataLen];
    CFErrorRef cfErr = NULL;
    NSDictionary* identData = (__bridge_transfer NSDictionary*)CFPropertyListCreateWithDERData(NULL, (__bridge CFDataRef)derBagIdent, 0, NULL, &cfErr);
    if (!identData || ![_bagIdentity.baghash isEqualToData:identData[@"baghash"]] || ![_bagIdentity.baguuid isEqualToData:identData[@"baguuid"]]) {
        secerror("SecDbBackup: KCSK ref key embedded bag identity does not match loaded bag. %@ vs %@", identData, _bagIdentity);
        [self fillError:error code:SecDbBackupMalformedKCSKDataOnDisk underlying:CFBridgingRelease(cfErr) description:@"KCSK ref key embedded bag identity does not match loaded bag."];
        return nil;
    }

    // AKS refkey claims in its external data to belong to our backup bag. Let's see if the claim holds up: use the key.
    void* keypairBytes = NULL;
    size_t keypairLength = 0;
    aksResult = aks_ref_key_decrypt(refkey, NULL, 0, kcsk.aksWrappedKey.bytes, kcsk.aksWrappedKey.length, &keypairBytes, &keypairLength);
    if (aksResult == kSKSReturnNoPermission) {
        [self fillError:error code:SecDbBackupKeychainLocked underlying:nil description:@"Unable to unwrap KCSK private key for class %d. Locked", keyclass];
        return nil;
    } else if (aksResult != kAKSReturnSuccess) {
        // Failure could indicate key was corrupted or tampered with
        [self fillError:error
                   code:SecDbBackupAKSFailure
             underlying:nil
            description:@"AKS did not unwrap KCSK private key for class %d: %d", keyclass, aksResult];
        return nil;
    }

    SFECKeyPair* keypair = [self getECKeyPairFromDERBytes:keypairBytes length:keypairLength error:error];
    return keypair ? [InMemoryKCSK kcskWithRefKey:refkey wrappedKey:kcsk.aksWrappedKey key:keypair] : nil;
}

- (SFECKeyPair*)onQueueFetchKCSKForKeyclass:(keyclass_t)keyclass error:(NSError**)error
{
    assert(error);
    assert(_bagIdentity);
    assert(_handle != bad_keybag_handle);

    InMemoryKCSK* cached = _cachedKCSKs[@(keyclass)];
    if (cached.key) {
        return cached.key;
    }

    if (cached) {
        secnotice("SecDbBackup", "Cached but wrapped KCSK found for class %d, unwrapping", keyclass);
        void* keybytes = NULL;
        size_t keylen = 0;
        kern_return_t aksResult = aks_ref_key_decrypt(cached.refKey, NULL, 0, cached.wrappedKey.bytes, cached.wrappedKey.length, &keybytes, &keylen);
        if (aksResult == kAKSReturnSuccess) {
            cached.key = [self getECKeyPairFromDERBytes:keybytes length:keylen error:error];
            return cached.key;
        } else {
            secerror("SecDbBackup: Cached KCSK isn't unwrapping key material. This is a bug.");
        }
    }

    secnotice("SecDbBackup", "No cached KCSK for class %d, reading from disk", keyclass);
    cached = [self onQueueReadKCSKFromDiskForClass:keyclass error:error];
    if (!cached.key) {
        secerror("SecDbBackup: Failed to obtain KCSK for class %d: %@", keyclass, *error);
        if ((*error).code != SecDbBackupKeychainLocked) {
            seccritical("SecDbBackup: KCSK unavailable, cannot backup-wrap class %d items. Need to perform recovery.", keyclass);
            // TODO: We're borked. Need to recover from this.
        }
    }
    _cachedKCSKs[@(keyclass)] = cached;
    return cached.key;
}

#pragma mark - Recovery Set Management

- (BOOL)onQueueInTransaction:(SecDbConnectionRef)dbt writeRecoverySetToKeychain:(SecDbBackupRecoverySet*)set error:(NSError**)error
{
    dispatch_assert_queue(_queue);
    __block bool ok = true;
    __block CFErrorRef cfError = NULL;

    NSString* sql = @"INSERT INTO backuprecoverysets (backupUUID, recoverytype, recoveryset) VALUES (?, ?, ?)";
    ok &= SecDbPrepare(dbt, (__bridge CFStringRef)sql, &cfError, ^(sqlite3_stmt *stmt) {
        ok &= SecDbBindObject(stmt, 1, (__bridge CFDataRef)set.bagIdentity.baguuid, &cfError);
        ok &= SecDbBindObject(stmt, 2, (__bridge CFNumberRef)@(set.recoveryType), &cfError);
        ok &= SecDbBindObject(stmt, 3, (__bridge CFDataRef)set.data, &cfError);
        ok &= SecDbStep(dbt, stmt, &cfError, NULL);
    });

    if (!ok) {
        secerror("SecDbBackup: Unable to write recovery set to keychain: %@", cfError);
        [self fillError:error code:SecDbBackupWriteFailure underlying:CFBridgingRelease(cfError) description:@"Unable to write recovery set to keychain"];
    }

    return ok;
}

- (SecDbBackupRecoverySet*)onQueueInTransaction:(SecDbConnectionRef)dbt createRecoverySetWithBagSecret:(NSData*)secret
                     forType:(SecDbBackupRecoveryType)type error:(NSError**)error
{
    dispatch_assert_queue(_queue);
    if (!secret) {
        [self fillError:error code:SecDbBackupInvalidArgument underlying:nil description:@"Can't create recovery set without secret"];
        return nil;
    }

    SecDbBackupRecoverySet* set;
    switch (type) {
        case SecDbBackupRecoveryTypeAKS:
            set = [self inTransaction:dbt createAKSTypeRecoverySetWithBagSecret:secret handle:_handle error:error];
            break;
        case SecDbBackupRecoveryTypeCylon:
            secerror("SecDbBackup: Cylon recovery type not yet implemented");
            [self fillError:error code:SecDbBackupUnknownOption underlying:nil description:@"Recovery type Cylon not yet implemented"];
            break;
        case SecDbBackupRecoveryTypeRecoveryKey:
            secerror("SecDbBackup: RecoveryKey recovery type not yet implemented");
            [self fillError:error code:SecDbBackupUnknownOption underlying:nil description:@"Recovery type RecoveryKey not yet implemented"];
            break;
        default:
            secerror("SecDbBackup: Unknown type %ld", (long)type);
            [self fillError:error code:SecDbBackupUnknownOption underlying:nil description:@"Recovery type %li unknown", (long)type];
            break;
    }

    return set;
}

- (SecDbBackupRecoverySet*)inTransaction:(SecDbConnectionRef)dbt createAKSTypeRecoverySetWithBagSecret:(NSData*)secret handle:(keybag_handle_t)handle error:(NSError**)error
{
    SecDbBackupRecoverySet* set = [SecDbBackupRecoverySet new];
    set.recoveryType = SecDbBackupRecoveryTypeAKS;
    set.bagIdentity = _bagIdentity;

    NSError* cryptoError;
    SFAESKeySpecifier* specifier = [[SFAESKeySpecifier alloc] initWithBitSize:SFAESKeyBitSize256];
    SFAESKey* KCSKSecret = [[SFAESKey alloc] initRandomKeyWithSpecifier:specifier error:&cryptoError];
    if (!KCSKSecret) {
        [self fillError:error code:SecDbBackupCryptoFailure underlying:cryptoError description:@"Unable to create AKS recovery set"];
        return nil;
    }

    // We explicitly do NOT want akpu. For the rest, create and write all KCSKs
    for (NSNumber* class in @[@(key_class_ak), @(key_class_ck), @(key_class_dk), @(key_class_aku), @(key_class_cku), @(key_class_dku)]) {
        SecDbBackupKeyClassSigningKey* kcsk = [self createKCSKForKeyClass:[class intValue] withWrapper:KCSKSecret error:error];
        if (!kcsk) {
            secerror("SecDbBackup: Unable to create KCSK for class %@: %@", class, *error);
            return nil;
        }
        if (![self inTransaction:dbt writeKCSKToKeychain:kcsk error:error]) {
            secerror("SecDbBackup: Unable to write KCSK for class %@ to keychain: %@", class, *error);
            return nil;
        }
    }

    SFAESKey* recoverykey = [[SFAESKey alloc] initRandomKeyWithSpecifier:specifier error:&cryptoError];
    if (!recoverykey) {
        [self fillError:error code:SecDbBackupCryptoFailure underlying:cryptoError description:@"Unable to create recovery key"];
        return nil;
    }

    SFAuthenticatedEncryptionOperation* op = [[SFAuthenticatedEncryptionOperation alloc]
                                              initWithKeySpecifier:[[SFAESKeySpecifier alloc]
                                                                    initWithBitSize:SFAESKeyBitSize256]];
    SFAuthenticatedCiphertext* wrappedsecret = [op encrypt:secret withKey:recoverykey error:&cryptoError];
    if (!wrappedsecret) {
        secerror("SecDbBackup: Unable to wrap keybag secret: %@", cryptoError);
        [self fillError:error code:SecDbBackupCryptoFailure underlying:cryptoError description:@"Unable to wrap keybag secret"];
        return nil;
    }
    set.wrappedBagSecret = [NSKeyedArchiver archivedDataWithRootObject:wrappedsecret requiringSecureCoding:YES error:error];

    SFAuthenticatedCiphertext* wrappedkcsksecret = [op encrypt:KCSKSecret.keyData withKey:recoverykey error:&cryptoError];
    if (!wrappedkcsksecret) {
        secerror("SecDbBackup: Unable to wrap KCSK secret: %@", cryptoError);
        [self fillError:error code:SecDbBackupCryptoFailure underlying:cryptoError description:@"Unable to wrap KCSK secret"];
        return nil;
    }
    set.wrappedKCSKSecret = [NSKeyedArchiver archivedDataWithRootObject:wrappedkcsksecret requiringSecureCoding:YES error:error];

    NSMutableData* wrappedrecoverykey = [[NSMutableData alloc] initWithLength:APPLE_KEYSTORE_MAX_SYM_WRAPPED_KEY_LEN];
    if (![SecAKSObjCWrappers aksEncryptWithKeybag:KEYBAG_DEVICE keyclass:key_class_aku plaintext:recoverykey.keyData outKeyclass:nil ciphertext:wrappedrecoverykey error:&cryptoError]) {
        secerror("SecDbBackup: Unable to wrap recovery key to AKS: %@", cryptoError);
        [self fillError:error code:SecDbBackupAKSFailure underlying:cryptoError description:@"Unable to wrap recovery key to AKS"];
        return nil;
    }
    set.wrappedRecoveryKey = [wrappedrecoverykey copy];

    return set;
}

#pragma mark - Backup System Initialization / Maintenance

- (BOOL)createOrLoadBackupInfrastructure:(NSError**)error {
    assert(error);
    __block BOOL ok = true;
    __block NSError* localError;
    dispatch_sync(_queue, ^{
        ok = [self onQueueCreateOrLoadBackupInfrastructure:&localError];
    });

    if (localError) {
        *error = localError;
    }
    return ok;
}

// TODO: if this creates the infrastructure, kick off a fixup routine
// TODO: if not, make sure we actually delete stuff. Nested transactions are not a thing (use checkpointing or delete explicitly)

- (BOOL)onQueueCreateOrLoadBackupInfrastructure:(NSError**)error {
    dispatch_assert_queue(_queue);
    assert(error);
    if (self->_handle != bad_keybag_handle) {
        return true;
    }

    self->_handle = [self onQueueLoadBackupBag:nil error:error];
    if (self->_handle != bad_keybag_handle) {
        secnotice("SecDbBackup", "Keybag found and loaded");
        return true;
    } else if (self->_handle == bad_keybag_handle && (*error).code != SecDbBackupNoBackupBagFound) {
        return false;
    }
    *error = nil;

    __block BOOL ok = YES;
    __block CFErrorRef cfError = NULL;
    __block NSError* localError;
    secnotice("SecDbBackup", "CreateOrLoad: No backup bag found, attempting to create new infrastructure");
    if (ok && !SecAKSDoWithUserBagLockAssertion(&cfError, ^{
        ok &= kc_with_dbt_non_item_tables(YES, &cfError, ^bool(SecDbConnectionRef dbt) {
            ok &= kc_transaction(dbt, &cfError, ^bool{
                NSData* secret = [self createBackupBagSecret:&localError];
                if (!secret) {
                    return false;
                }

                self->_handle = [self onQueueCreateBackupBagWithSecret:secret error:&localError];
                if (self->_handle == bad_keybag_handle) {
                    return false;
                }
                secnotice("SecDbBackup", "CreateOrLoad: Successfully created backup bag");

                if (![self onQueueInTransaction:dbt saveBackupBag:self->_handle asDefault:YES error:&localError]) {
                    return false;
                }
                secnotice("SecDbBackup", "CreateOrLoad: Successfully saved backup bag");

                if (![self onQueueReloadDefaultBackupBagWithError:&localError]) {
                    return false;
                }
                secnotice("SecDbBackup", "CreateOrLoad: Successfully reloaded backup bag");

                SecDbBackupRecoverySet* set = [self onQueueInTransaction:dbt
                                          createRecoverySetWithBagSecret:secret
                                                                 forType:SecDbBackupRecoveryTypeAKS
                                                                   error:&localError];
                if (!set) {
                secnotice("SecDbBackup", "CreateOrLoad: Successfully created recovery set");
                    return false;
                }

                if (![self onQueueInTransaction:dbt writeRecoverySetToKeychain:set error:&localError]) {
                    return false;
                }
                secnotice("SecDbBackup", "CreateOrLoad: Successfully saved recovery set");

                return true;
            });
            return ok;
        });
    })) {   // could not perform action with lock assertion
        static dispatch_once_t once;
        static sec_action_t action;
        dispatch_once(&once, ^{
            action = sec_action_create("keybag_locked_during_backup_setup_complaint", 5);
            sec_action_set_handler(action, ^{
                secerror("SecDbBackup: Cannot obtain AKS lock assertion so cannot setup backup infrastructure");
            });
        });
        sec_action_perform(action);
        [self fillError:&localError code:SecDbBackupKeychainLocked underlying:nil
            description:@"Unable to initialize backup infrastructure, keychain locked"];
        ok = NO;
    }

    if (!ok) {
        self->_bagIdentity = nil;
        aks_unload_bag(self->_handle);
        self->_handle = bad_keybag_handle;
    }

    if (ok) {
        secnotice("SecDbBackup", "Hurray! Successfully created backup infrastructure");
    } else {
        assert(localError || cfError);
        if (localError) {
            secerror("SecDbBackup: Could not initialize backup infrastructure: %@", localError);
            *error = localError;
        } else if (cfError) {
            secerror("SecDbBackup: Could not initialize backup infrastructure: %@", cfError);
            [self fillError:error code:SecDbBackupSetupFailure underlying:CFBridgingRelease(cfError)
                description:@"Unable to initialize backup infrastructure"];
        } else {
            secerror("SecDbBackup: Could not initialize backup infrastructure but have no error");
            [self fillError:error code:SecDbBackupSetupFailure underlying:nil
                description:@"Unable to initialize backup infrastructure (not sure why)"];
        }
        CFReleaseNull(cfError);
    }

    return ok;
}

#pragma mark - Item Encryption

- (SecDbBackupWrappedItemKey*)wrapItemKey:(SFAESKey*)key forKeyclass:(keyclass_t)keyclass error:(NSError**)error
{
    assert(error);
    if (keyclass == key_class_akpu) {
        secwarning("SecDbBackup: Don't tempt me Frodo!");
        [self fillError:error code:SecDbBackupInvalidArgument underlying:nil description:@"Do not call wrapItemKey with class akpu"];
        return nil;
    }

    if (![self createOrLoadBackupInfrastructure:error]) {
        if ((*error).domain != (__bridge NSString*)kSecErrorDomain || (*error).code != errSecInteractionNotAllowed) {
            secerror("SecDbBackup: Could not create/load backup infrastructure: %@", *error);
        }
        return nil;
    }

    __block SecDbBackupWrappedItemKey* backupWrappedKey;

    __block NSMutableData* wrappedKey = [NSMutableData dataWithLength:APPLE_KEYSTORE_MAX_ASYM_WRAPPED_KEY_LEN];
    __block NSError* localError;
    dispatch_sync(_queue, ^{
        if (![self onQueueCreateOrLoadBackupInfrastructure:&localError]) {
            return;
        }
        if (![SecAKSObjCWrappers aksEncryptWithKeybag:self->_handle
                                             keyclass:keyclass
                                            plaintext:key.keyData
                                          outKeyclass:nil
                                           ciphertext:wrappedKey
                                                error:&localError]) {
            return;
        }
        SFSignedData* wrappedAndSigned = [self onQueueSignData:wrappedKey withKCSKForKeyclass:keyclass error:&localError];
        if (!wrappedAndSigned) {
            if (localError.code != SecDbBackupKeychainLocked) {
                secerror("SecDbBackup: Unable to sign item for class %d: %@", keyclass, localError);
                return;
            }
        }
        backupWrappedKey = [SecDbBackupWrappedItemKey new];
        backupWrappedKey.wrappedKey = [NSKeyedArchiver archivedDataWithRootObject:wrappedAndSigned requiringSecureCoding:YES error:&localError];
        backupWrappedKey.baguuid = self->_bagIdentity.baguuid;
    });

    if (localError) {
        secerror("SecDbBackup: Unable to wrap-and-sign item of class %d: %@", keyclass, localError);
        *error = localError;
        return nil;
    }

    return backupWrappedKey;
}

- (SFSignedData*)onQueueSignData:(NSMutableData*)data withKCSKForKeyclass:(keyclass_t)keyclass error:(NSError**)error
{
    SFECKeyPair* kcsk = [self onQueueFetchKCSKForKeyclass:keyclass error:error];
    if (!kcsk) {
        return nil;
    }

    SFEC_X962SigningOperation* op = [[SFEC_X962SigningOperation alloc] initWithKeySpecifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]];
    return [op sign:data withKey:kcsk error:error];
}

#pragma mark - Testing Helpers

- (keybag_handle_t)createBackupBagWithSecret:(NSData*)secret error:(NSError**)error
{
    assert(error);
    __block keybag_handle_t handle = bad_keybag_handle;
    __block NSError* localError;
    dispatch_sync(_queue, ^{
        handle = [self onQueueCreateBackupBagWithSecret:secret error:&localError];
    });
    if (localError) {
        *error = localError;
    } else if (handle == bad_keybag_handle) {
        [self fillError:error code:SecDbBackupTestCodeFailure underlying:nil description:@"Unable to create backup bag, but no reason"];
    }
    return handle;
}

- (BOOL)saveBackupBag:(keybag_handle_t)handle asDefault:(BOOL)asDefault error:(NSError**)error
{
    assert(error);
    __block bool ok = true;
    __block NSError* localErr;
    __block CFErrorRef cfError = NULL;
    dispatch_sync(_queue, ^{
        ok &= kc_with_dbt_non_item_tables(YES, &cfError, ^bool(SecDbConnectionRef dbt) {
            ok &= kc_transaction(dbt, &cfError, ^bool{
                ok &= [self onQueueInTransaction:dbt saveBackupBag:handle asDefault:asDefault error:&localErr];
                return ok;
            });
            return ok;
        });
    });

    if (!ok) {
        if (cfError) {
            [self fillError:error code:SecDbBackupTestCodeFailure underlying:CFBridgingRelease(cfError) description:@"Unable to save keybag to disk"];
        } else if (localErr) {
            *error = localErr;
        } else if (!localErr) {
            [self fillError:error code:SecDbBackupTestCodeFailure underlying:nil description:@"Unable to save keybag to disk but who knows why"];
        }
    }
    return ok;
}

- (keybag_handle_t)loadBackupBag:(NSUUID*)uuid error:(NSError**)error {
    __block keybag_handle_t handle = bad_keybag_handle;
    __block NSError* localError;
    dispatch_sync(_queue, ^{
        handle = [self onQueueLoadBackupBag:uuid error:&localError];
    });
    if (error && localError) {
        *error = localError;
    }
    return handle;
}

- (SecDbBackupRecoverySet*)createRecoverySetWithBagSecret:(NSData*)secret forType:(SecDbBackupRecoveryType)type error:(NSError**)error
{
    __block SecDbBackupRecoverySet* set;
    __block BOOL ok = YES;
    __block NSError* localError;
    __block CFErrorRef cfError = NULL;
    dispatch_sync(_queue, ^{
        ok &= kc_with_dbt_non_item_tables(true, &cfError, ^bool(SecDbConnectionRef dbt) {
            ok &= kc_transaction(dbt, &cfError, ^bool{
                set = [self onQueueInTransaction:dbt createRecoverySetWithBagSecret:secret forType:type error:&localError];
                return set != nil;
            });
            return ok;
        });
    });
    if (error && cfError) {
        *error = CFBridgingRelease(cfError);
    } else if (error && localError) {
        *error = localError;
    }
    CFReleaseNull(cfError);

    return set;
}

- (SFECKeyPair*)fetchKCSKForKeyclass:(keyclass_t)keyclass error:(NSError**)error
{
    __block SFECKeyPair* keypair;
    __block NSError* localError;
    dispatch_sync(_queue, ^{
        keypair = [self onQueueFetchKCSKForKeyclass:keyclass error:&localError];
    });
    if (localError && error) {
        *error = localError;
    }

    return keypair;
}

@end

#endif // SECDB_BACKUPS_ENABLED
