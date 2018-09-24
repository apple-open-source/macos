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

#import "CKKSViewManager.h"
#import "CKKSKeychainView.h"
#import "CKKSCurrentKeyPointer.h"
#import "CKKSKey.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#include <securityd/SecItemSchema.h>
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>

#include <CloudKit/CloudKit.h>
#include <CloudKit/CloudKit_Private.h>

@interface CKKSKey ()
@property CKKSAESSIVKey* aessivkey;
@end

@implementation CKKSKey

- (instancetype)init {
    self = [super init];
    return self;
}

- (instancetype) initSelfWrappedWithAESKey: (CKKSAESSIVKey*) aeskey
                                      uuid: (NSString*) uuid
                                  keyclass: (CKKSKeyClass*)keyclass
                                     state: (CKKSProcessedState*) state
                                    zoneID: (CKRecordZoneID*) zoneID
                           encodedCKRecord: (NSData*) encodedrecord
                                currentkey: (NSInteger) currentkey
{
    if(self = [super initWithUUID: uuid
                    parentKeyUUID: uuid
                           zoneID: zoneID
                  encodedCKRecord: encodedrecord
                          encItem: nil
                       wrappedkey: nil
                  generationCount: 0
                           encver: currentCKKSItemEncryptionVersion]) {
        _keyclass = keyclass;
        _currentkey = !!currentkey;
        _aessivkey = aeskey;
        _state = state;

        self.ckRecordType = SecCKRecordIntermediateKeyType;

        // Wrap the key with the key. Not particularly useful, but there you go.
        NSError* error = nil;
        [self wrapUnder: self error:&error];
        if(error != nil) {
            secerror("CKKSKey: Couldn't self-wrap key: %@", error);
            return nil;
        }
    }
    return self;
}

- (instancetype) initWrappedBy: (CKKSKey*) wrappingKey
                        AESKey: (CKKSAESSIVKey*) aeskey
                          uuid: (NSString*) uuid
                      keyclass: (CKKSKeyClass*)keyclass
                         state: (CKKSProcessedState*) state
                        zoneID: (CKRecordZoneID*) zoneID
               encodedCKRecord: (NSData*) encodedrecord
                    currentkey: (NSInteger) currentkey
{
    if(self = [super initWithUUID: uuid
                    parentKeyUUID: wrappingKey.uuid
                           zoneID: zoneID
                  encodedCKRecord: encodedrecord
                          encItem:nil
                       wrappedkey:nil
                  generationCount:0
                           encver:
               currentCKKSItemEncryptionVersion]) {
        _keyclass = keyclass;
        _currentkey = !!currentkey;
        _aessivkey = aeskey;
        _state = state;

        self.ckRecordType = SecCKRecordIntermediateKeyType;

        NSError* error = nil;
        [self wrapUnder: wrappingKey error:&error];
        if(error != nil) {
            secerror("CKKSKey: Couldn't wrap key with key: %@", error);
            return nil;
        }
    }
    return self;
}

- (instancetype) initWithWrappedAESKey: (CKKSWrappedAESSIVKey*) wrappedaeskey
                                  uuid: (NSString*) uuid
                         parentKeyUUID: (NSString*) parentKeyUUID
                              keyclass: (CKKSKeyClass*)keyclass
                                 state: (CKKSProcessedState*) state
                                zoneID: (CKRecordZoneID*) zoneID
                       encodedCKRecord: (NSData*) encodedrecord
                            currentkey: (NSInteger) currentkey
{
    if(self = [super initWithUUID:uuid
                    parentKeyUUID:parentKeyUUID
                           zoneID:zoneID
                  encodedCKRecord:encodedrecord
                          encItem:nil
                       wrappedkey:wrappedaeskey
                  generationCount:0
                           encver:currentCKKSItemEncryptionVersion]) {
        _keyclass = keyclass;
        _currentkey = !!currentkey;
        _aessivkey = nil;
        _state = state;

        self.ckRecordType = SecCKRecordIntermediateKeyType;
    }
    return self;
}

- (void)dealloc {
    [self zeroKeys];
}

- (void)zeroKeys {
    [self.aessivkey zeroKey];
}

- (bool)wrapsSelf {
    return [self.uuid isEqual: self.parentKeyUUID];
}

- (bool)wrapUnder: (CKKSKey*) wrappingKey error: (NSError * __autoreleasing *) error {
    self.wrappedkey = [wrappingKey wrapAESKey: self.aessivkey error:error];
    if(self.wrappedkey == nil) {
        secerror("CKKSKey: couldn't wrap key: %@", error ? *error : @"unknown error");
    } else {
        self.parentKeyUUID = wrappingKey.uuid;
    }
    return (self.wrappedkey != nil);
}

- (bool)unwrapSelfWithAESKey: (CKKSAESSIVKey*) unwrappingKey error: (NSError * __autoreleasing *) error {
    _aessivkey = [unwrappingKey unwrapAESKey:self.wrappedkey error:error];
    return (_aessivkey != nil);
}

+ (instancetype) loadKeyWithUUID: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    CKKSKey* key = [CKKSKey fromDatabase: uuid zoneID:zoneID error:error];

    // failed unwrapping means we can't return a key.
    if(![key ensureKeyLoaded:error]) {
        return nil;
    }
    return key;
}

+ (CKKSKey*) randomKeyWrappedByParent: (CKKSKey*) parentKey error: (NSError * __autoreleasing *) error {
    return [self randomKeyWrappedByParent: parentKey keyclass:parentKey.keyclass error:error];
}

+ (CKKSKey*) randomKeyWrappedByParent: (CKKSKey*) parentKey keyclass:(CKKSKeyClass*)keyclass error: (NSError * __autoreleasing *) error {
    CKKSAESSIVKey* aessivkey = [CKKSAESSIVKey randomKey];

    CKKSKey* key = [[CKKSKey alloc] initWrappedBy: parentKey
                                           AESKey: aessivkey
                                             uuid:[[NSUUID UUID] UUIDString]
                                         keyclass:keyclass
                                            state:SecCKKSProcessedStateLocal
                                           zoneID: parentKey.zoneID
                                  encodedCKRecord: nil
                                       currentkey: false];
    return key;
}

+ (instancetype)randomKeyWrappedBySelf: (CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error {
    CKKSAESSIVKey* aessivkey = [CKKSAESSIVKey randomKey];
    NSString* uuid = [[NSUUID UUID] UUIDString];

    CKKSKey* key = [[CKKSKey alloc] initSelfWrappedWithAESKey: aessivkey
                                                         uuid:uuid
                                                     keyclass:SecCKKSKeyClassTLK
                                                        state:SecCKKSProcessedStateLocal
                                                       zoneID: zoneID
                                              encodedCKRecord: nil
                                                   currentkey: false];
    return key;

}

- (CKKSKey*)topKeyInAnyState: (NSError * __autoreleasing *) error {
    // Recursively find the top-level key in the hierarchy, preferring 'remote' keys.
    if([self wrapsSelf]) {
        return self;
    }

    CKKSKey* remoteParent = [CKKSKey tryFromDatabaseWhere: @{@"UUID": self.parentKeyUUID, @"state": SecCKKSProcessedStateRemote} error: error];
    if(remoteParent) {
        return [remoteParent topKeyInAnyState: error];
    }

    // No remote parent. Fall back to anything.
    CKKSKey* parent = [CKKSKey fromDatabaseWhere: @{@"UUID": self.parentKeyUUID} error: error];
    if(parent) {
        return [parent topKeyInAnyState: error];
    }

    // No good. Error is already filled.
    return nil;
}

- (CKKSAESSIVKey*)ensureKeyLoaded: (NSError * __autoreleasing *) error {
    if(self.aessivkey) {
        return self.aessivkey;
    }

    // Attempt to load this key from the keychain
    NSError* keychainError = nil;
    if([self loadKeyMaterialFromKeychain:&keychainError]) {
        return self.aessivkey;
    }

    // Uhh, okay, if that didn't work, try to unwrap via the key hierarchy
    NSError* keyHierarchyError = nil;
    if([self unwrapViaKeyHierarchy:&keyHierarchyError]) {
        // Attempt to save this new key, but don't error if it fails
        NSError* resaveError = nil;
        if(![self saveKeyMaterialToKeychain:&resaveError] || resaveError) {
            secerror("ckkskey: Resaving missing key failed, continuing: %@", resaveError);
        }

        return self.aessivkey;
    }

    // Pick an error to report
    if(error) {
        *error = keyHierarchyError ? keyHierarchyError : keychainError;
    }

    return nil;
}


- (CKKSAESSIVKey*)unwrapViaKeyHierarchy: (NSError * __autoreleasing *) error {
    if(self.aessivkey) {
        return self.aessivkey;
    }

    NSError* localerror = nil;

    // Attempt to load this key from the keychain
    if([self loadKeyMaterialFromKeychain:&localerror]) {
        // Rad. Success!
        return self.aessivkey;
    }

    // First, check if we're a TLK.
    if([self.keyclass isEqual: SecCKKSKeyClassTLK]) {
        // Okay, not loading the key from the keychain above is an issue. If we have a parent key, then fall through to the recursion below.
        if(!self.parentKeyUUID || [self.parentKeyUUID isEqualToString: self.uuid]) {
            if(error) {
                *error = localerror;
            }
            return nil;
        }
    }

    // Recursively unwrap our parent.
    CKKSKey* parent = [CKKSKey fromDatabaseAnyState:self.parentKeyUUID zoneID:self.zoneID error:error];

    // TODO: do we need loop detection here?
    if(![parent unwrapViaKeyHierarchy: error]) {
        return nil;
    }

    _aessivkey = [parent unwrapAESKey:self.wrappedkey error:error];
    return self.aessivkey;
}

- (bool)trySelfWrappedKeyCandidate:(CKKSAESSIVKey*)candidate error:(NSError * __autoreleasing *) error {
    if(![self wrapsSelf]) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSKeyNotSelfWrapped userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"%@ is not self-wrapped", self]}];
        }
        return false;
    }

    CKKSAESSIVKey* unwrapped = [candidate unwrapAESKey:self.wrappedkey error:error];
    if(unwrapped && [unwrapped isEqual: candidate]) {
        _aessivkey = unwrapped;
        return true;
    }

    return false;
}

- (CKKSWrappedAESSIVKey*)wrapAESKey: (CKKSAESSIVKey*) keyToWrap error: (NSError * __autoreleasing *) error {
    CKKSAESSIVKey* key = [self ensureKeyLoaded: error];
    CKKSWrappedAESSIVKey* wrappedkey = [key wrapAESKey: keyToWrap error:error];
    return wrappedkey;
}

- (CKKSAESSIVKey*)unwrapAESKey: (CKKSWrappedAESSIVKey*) keyToUnwrap error: (NSError * __autoreleasing *) error {
    CKKSAESSIVKey* key = [self ensureKeyLoaded: error];
    CKKSAESSIVKey* unwrappedkey = [key unwrapAESKey: keyToUnwrap error:error];
    return unwrappedkey;
}

- (NSData*)encryptData: (NSData*) plaintext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    CKKSAESSIVKey* key = [self ensureKeyLoaded: error];
    NSData* data = [key encryptData: plaintext authenticatedData:ad error:error];
    return data;
}

- (NSData*)decryptData: (NSData*) ciphertext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    CKKSAESSIVKey* key = [self ensureKeyLoaded: error];
    NSData* data = [key decryptData: ciphertext authenticatedData:ad error:error];
    return data;
}

/* Functions to load and save keys from the keychain (where we get to store actual key material!) */
- (bool)saveKeyMaterialToKeychain: (NSError * __autoreleasing *) error {
    return [self saveKeyMaterialToKeychain: true error: error];
}

- (bool)saveKeyMaterialToKeychain: (bool)stashTLK error:(NSError * __autoreleasing *) error {

    // Note that we only store the key class, view, UUID, parentKeyUUID, and key material in the keychain
    // Any other metadata must be stored elsewhere and filled in at load time.

    if(![self ensureKeyLoaded:error]) {
        // No key material, nothing to save to keychain.
        return false;
    }

    // iOS keychains can't store symmetric keys, so we're reduced to storing this key as a password
    NSData* keydata = [[[NSData alloc] initWithBytes:self.aessivkey->key length:self.aessivkey->size] base64EncodedDataWithOptions:0];
    NSMutableDictionary* query = [@{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
            (id)kSecAttrNoLegacy : @YES,
            (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
            (id)kSecAttrDescription: self.keyclass,
            (id)kSecAttrServer: self.zoneID.zoneName,
            (id)kSecAttrAccount: self.uuid,
            (id)kSecAttrPath: self.parentKeyUUID,
            (id)kSecAttrIsInvisible: @YES,
            (id)kSecValueData : keydata,
        } mutableCopy];

    // Only TLKs are synchronizable. Other keyclasses must synchronize via key hierarchy.
    if([self.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
        // Use PCS-MasterKey view so they'll be initial-synced under SOS.
        query[(id)kSecAttrSyncViewHint] = (id)kSecAttrViewHintPCSMasterKey;
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanTrue;
    }

    // Class C keys are accessible after first unlock; TLKs and Class A keys are accessible only when unlocked
    if([self.keyclass isEqualToString: SecCKKSKeyClassC]) {
        query[(id)kSecAttrAccessible] = (id)kSecAttrAccessibleAfterFirstUnlock;
    } else {
        query[(id)kSecAttrAccessible] = (id)kSecAttrAccessibleWhenUnlocked;
    }

    NSError* localError = nil;
    [CKKSKey setKeyMaterialInKeychain:query error:&localError];

    if(localError && error) {
        *error = [NSError errorWithDomain:@"securityd"
                                    code:localError.code
                                userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat:@"Couldn't save %@ to keychain: %d", self, (int)localError.code],
                                           NSUnderlyingErrorKey: localError,
                                           }];
    }

    // TLKs are synchronizable. Stash them nonsyncably nearby.
    // Don't report errors here.
    if(stashTLK && [self.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
        query = [@{
                   (id)kSecClass : (id)kSecClassInternetPassword,
                   (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                   (id)kSecAttrNoLegacy : @YES,
                   (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                   (id)kSecAttrDescription: [self.keyclass stringByAppendingString: @"-nonsync"],
                   (id)kSecAttrServer: self.zoneID.zoneName,
                   (id)kSecAttrAccount: self.uuid,
                   (id)kSecAttrPath: self.parentKeyUUID,
                   (id)kSecAttrIsInvisible: @YES,
                   (id)kSecValueData : keydata,
                   } mutableCopy];
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanFalse;

        NSError* stashError = nil;
        [CKKSKey setKeyMaterialInKeychain:query error:&localError];

        if(stashError) {
            secerror("ckkskey: Couldn't stash %@ to keychain: %@", self, stashError);
        }
    }

    return localError == nil;
}

+ (NSDictionary*)setKeyMaterialInKeychain:(NSDictionary*)query error:(NSError* __autoreleasing *)error {
    CFTypeRef result = NULL;
    OSStatus status = SecItemAdd((__bridge CFDictionaryRef)query, &result);

    NSError* localerror = nil;

    // Did SecItemAdd fall over due to an existing item?
    if(status == errSecDuplicateItem) {
        // Add every primary key attribute to this find dictionary
        NSMutableDictionary* findQuery = [[NSMutableDictionary alloc] init];
        findQuery[(id)kSecClass]              = query[(id)kSecClass];
        findQuery[(id)kSecAttrSynchronizable] = query[(id)kSecAttrSynchronizable];
        findQuery[(id)kSecAttrSyncViewHint]   = query[(id)kSecAttrSyncViewHint];
        findQuery[(id)kSecAttrAccessGroup]    = query[(id)kSecAttrAccessGroup];
        findQuery[(id)kSecAttrAccount]        = query[(id)kSecAttrAccount];
        findQuery[(id)kSecAttrServer]         = query[(id)kSecAttrServer];
        findQuery[(id)kSecAttrPath]           = query[(id)kSecAttrPath];

        NSMutableDictionary* updateQuery = [query mutableCopy];
        updateQuery[(id)kSecClass] = nil;

        status = SecItemUpdate((__bridge CFDictionaryRef)findQuery, (__bridge CFDictionaryRef)updateQuery);

        if(status) {
            localerror = [NSError errorWithDomain:@"securityd"
                                             code:status
                                      description:[NSString stringWithFormat:@"SecItemUpdate: %d", (int)status]];
        }
    } else {
        localerror = [NSError errorWithDomain:@"securityd"
                                         code:status
                                  description: [NSString stringWithFormat:@"SecItemAdd: %d", (int)status]];
    }

    if(status) {
        CFReleaseNull(result);

        if(error) {
            *error = localerror;
        }
        return false;
    }

    NSDictionary* resultDict = CFBridgingRelease(result);
    return resultDict;
}

+ (NSDictionary*)queryKeyMaterialInKeychain:(NSDictionary*)query  error:(NSError* __autoreleasing *)error {
    CFTypeRef result = NULL;
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if(status) {
        CFReleaseNull(result);

        if(error) {
            *error = [NSError errorWithDomain:@"securityd"
                                         code:status
                                     userInfo:@{NSLocalizedDescriptionKey:
                                                    [NSString stringWithFormat:@"SecItemCopyMatching: %d", (int)status]}];
        }
        return false;
    }

    NSDictionary* resultDict = CFBridgingRelease(result);
    return resultDict;
}

+ (NSDictionary*)fetchKeyMaterialItemFromKeychain:(CKKSKey*)key resave:(bool*)resavePtr error:(NSError* __autoreleasing *)error {
    NSMutableDictionary* query = [@{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecAttrNoLegacy : @YES,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrDescription: key.keyclass,
            (id)kSecAttrAccount: key.uuid,
            (id)kSecAttrServer: key.zoneID.zoneName,
            (id)kSecAttrPath: key.parentKeyUUID,
            (id)kSecReturnAttributes: @YES,
            (id)kSecReturnData: @YES,
        } mutableCopy];

    // Synchronizable items are only found if you request synchronizable items. Only TLKs are synchronizable.
    if([key.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanTrue;
    }

    NSError* localError = nil;
    NSDictionary* result = [self queryKeyMaterialInKeychain:query error:&localError];
    NSError* originalError = localError;

    // If we found the item or errored in some interesting way, return.
    if(result) {
        return result;
    }
    if(localError && localError.code != errSecItemNotFound) {
        if(error) {
            *error = [NSError errorWithDomain:@"securityd"
                                         code:localError.code
                                     userInfo:@{NSLocalizedDescriptionKey:
                                                    [NSString stringWithFormat:@"Couldn't load %@ from keychain: %d", key, (int)localError.code],
                                                NSUnderlyingErrorKey: localError,
                                                }];
        }
        return result;
    }
    localError = nil;

    if([key.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
        //didn't find a regular tlk?  how about a piggy?
        query = [@{
                   (id)kSecClass : (id)kSecClassInternetPassword,
                   (id)kSecAttrNoLegacy : @YES,
                   (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                   (id)kSecAttrDescription: [key.keyclass stringByAppendingString: @"-piggy"],
                   (id)kSecAttrSynchronizable : (id)kSecAttrSynchronizableAny,
                   (id)kSecAttrAccount: [NSString stringWithFormat: @"%@-piggy", key.uuid],
                   (id)kSecAttrServer: key.zoneID.zoneName,
                   (id)kSecReturnAttributes: @YES,
                   (id)kSecReturnData: @YES,
                   (id)kSecMatchLimit: (id)kSecMatchLimitOne,
                   } mutableCopy];

        result = [self queryKeyMaterialInKeychain:query error:&localError];
        if(localError == nil) {
            secnotice("ckkskey", "loaded a piggy TLK (%@)", key.uuid);

            if(resavePtr) {
                *resavePtr = true;
            }

            return result;
        }

        if(localError && localError.code != errSecItemNotFound) {
            if(error) {
                *error = [NSError errorWithDomain:@"securityd"
                                             code:localError.code
                                         userInfo:@{NSLocalizedDescriptionKey:
                                                        [NSString stringWithFormat:@"Couldn't load %@ from keychain: %d", key, (int)localError.code],
                                                    NSUnderlyingErrorKey: localError,
                                                    }];
            }
            return nil;
        }
    }

    localError = nil;

    // Try to load a stashed TLK
    if([key.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
        localError = nil;

        // Try to look for the non-syncable stashed tlk and resurrect it.
        query = [@{
            (id)kSecClass : (id)kSecClassInternetPassword,
            (id)kSecAttrNoLegacy : @YES,
            (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
            (id)kSecAttrDescription: [key.keyclass stringByAppendingString: @"-nonsync"],
            (id)kSecAttrServer: key.zoneID.zoneName,
            (id)kSecAttrAccount: key.uuid,
            (id)kSecReturnAttributes: @YES,
            (id)kSecReturnData: @YES,
            (id)kSecAttrSynchronizable: @NO,
        } mutableCopy];

        result = [self queryKeyMaterialInKeychain:query error:&localError];
        if(localError == nil) {
            secnotice("ckkskey", "loaded a stashed TLK (%@)", key.uuid);

            if(resavePtr) {
                *resavePtr = true;
            }

            return result;
        }

        if(localError && localError.code != errSecItemNotFound) {
            if(error) {
                *error = [NSError errorWithDomain:@"securityd"
                                             code:localError.code
                                         userInfo:@{NSLocalizedDescriptionKey:
                                                        [NSString stringWithFormat:@"Couldn't load %@ from keychain: %d", key, (int)localError.code],
                                                    NSUnderlyingErrorKey: localError,
                                                    }];
            }
            return nil;
        }
    }

    // We didn't early-return. Use whatever error the original fetch produced.
    if(error) {
        *error = [NSError errorWithDomain:@"securityd"
                                     code:originalError ? originalError.code : errSecParam
                              description:[NSString stringWithFormat:@"Couldn't load %@ from keychain: %d", key, (int)originalError.code]
                               underlying:originalError];
    }

    return result;
}

- (bool)loadKeyMaterialFromKeychain: (NSError * __autoreleasing *) error {
    bool resave = false;
    NSDictionary* result = [CKKSKey fetchKeyMaterialItemFromKeychain:self resave:&resave error:error];
    if(!result) {
        return false;
    }

    NSData* b64keymaterial = result[(id)kSecValueData];
    NSData* keymaterial = [[NSData alloc] initWithBase64EncodedData:b64keymaterial options:0];
    if(!keymaterial) {
        secnotice("ckkskey", "Unable to unbase64 key: %@", self);
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSKeyUnknownFormat
                                  description:[NSString stringWithFormat:@"unable to unbase64 key: %@", self]];
        }
        return false;
    }

    CKKSAESSIVKey* key = [[CKKSAESSIVKey alloc] initWithBytes: (uint8_t*) keymaterial.bytes len:keymaterial.length];
    self.aessivkey = key;

    if(resave) {
        secnotice("ckkskey", "Resaving %@ as per request", self);
        NSError* resaveError = nil;
        [self saveKeyMaterialToKeychain:&resaveError];
        if(resaveError) {
            secnotice("ckkskey", "Resaving %@ failed: %@", self, resaveError);
        }
    }

    return !!(self.aessivkey);
}

- (bool)deleteKeyMaterialFromKeychain: (NSError * __autoreleasing *) error {

    NSMutableDictionary* query = [@{
                                    (id)kSecClass : (id)kSecClassInternetPassword,
                                    (id)kSecAttrNoLegacy : @YES,
                                    (id)kSecAttrAccessGroup : @"com.apple.security.ckks",
                                    (id)kSecAttrDescription: self.keyclass,
                                    (id)kSecAttrAccount: self.uuid,
                                    (id)kSecAttrServer: self.zoneID.zoneName,
                                    (id)kSecReturnData: @YES,
                                    } mutableCopy];

    // Synchronizable items are only found if you request synchronizable items. Only TLKs are synchronizable.
    if([self.keyclass isEqualToString: SecCKKSKeyClassTLK]) {
        query[(id)kSecAttrSynchronizable] = (id)kCFBooleanTrue;
    }

    OSStatus status = SecItemDelete((__bridge CFDictionaryRef) query);

    if(status) {
        if(error) {
            *error = [NSError errorWithDomain:@"securityd"
                                         code:status
                                     userInfo:@{NSLocalizedDescriptionKey:
                                                    [NSString stringWithFormat:@"Couldn't delete %@ from keychain: %d", self, (int)status]}];
        }
        return false;
    }
    return true;
}

+ (instancetype)keyFromKeychain: (NSString*) uuid
                  parentKeyUUID: (NSString*) parentKeyUUID
                       keyclass: (CKKSKeyClass*)keyclass
                          state: (CKKSProcessedState*) state
                         zoneID: (CKRecordZoneID*) zoneID
                encodedCKRecord: (NSData*) encodedrecord
                     currentkey: (NSInteger) currentkey
                          error: (NSError * __autoreleasing *) error {
    CKKSKey* key = [[CKKSKey alloc] initWithWrappedAESKey:nil
                                                     uuid:uuid
                                            parentKeyUUID:parentKeyUUID
                                                 keyclass:keyclass
                                                    state:state
                                                   zoneID:zoneID
                                          encodedCKRecord:encodedrecord
                                               currentkey:currentkey];

    if(![key loadKeyMaterialFromKeychain:error]) {
        return nil;
    }

    return key;
}

+ (NSString*)isItemKeyForKeychainView: (SecDbItemRef) item {

    NSString* accessgroup = (__bridge NSString*) SecDbItemGetCachedValueWithName(item, kSecAttrAccessGroup);
    NSString* description = (__bridge NSString*) SecDbItemGetCachedValueWithName(item, kSecAttrDescription);
    NSString* server      = (__bridge NSString*) SecDbItemGetCachedValueWithName(item, kSecAttrServer);

    if(accessgroup && description && server &&
       ![accessgroup isEqual:[NSNull null]] &&
       ![description isEqual:[NSNull null]] &&
       ![server      isEqual:[NSNull null]] &&

       [accessgroup isEqualToString:@"com.apple.security.ckks"] &&
       ([description isEqualToString: SecCKKSKeyClassTLK] ||
        [description isEqualToString: [NSString stringWithFormat:@"%@-nonsync", SecCKKSKeyClassTLK]] ||
        [description isEqualToString: [NSString stringWithFormat:@"%@-piggy", SecCKKSKeyClassTLK]] ||
        [description isEqualToString: SecCKKSKeyClassA] ||
        [description isEqualToString: SecCKKSKeyClassC])) {

       // Certainly looks like us! Return the view name.
       return server;
    }

    // Never heard of this item.
    return nil;
}


/* Database functions only return keys marked 'local', unless otherwise specified. */

+ (instancetype) fromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"UUID": uuid, @"state":  SecCKKSProcessedStateLocal, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype) fromDatabaseAnyState: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"UUID": uuid, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype) tryFromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"UUID": uuid, @"state":  SecCKKSProcessedStateLocal, @"ckzone":zoneID.zoneName} error: error];
}

+ (instancetype) tryFromDatabaseAnyState: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"UUID": uuid, @"ckzone":zoneID.zoneName} error: error];
}

+ (NSArray<CKKSKey*>*)selfWrappedKeys:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"UUID": [CKKSSQLWhereObject op:@"=" string:@"parentKeyUUID"], @"state":  SecCKKSProcessedStateLocal, @"ckzone":zoneID.zoneName} error:error];
}

+ (instancetype) currentKeyForClass: (CKKSKeyClass*) keyclass zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    // Load the CurrentKey record, and find the key for it
    CKKSCurrentKeyPointer* ckp = [CKKSCurrentKeyPointer fromDatabase:keyclass zoneID:zoneID error:error];
    if(!ckp) {
        return nil;
    }
    return [self fromDatabase:ckp.currentKeyUUID zoneID:zoneID error:error];
}

+ (NSArray<CKKSKey*>*) currentKeysForClass: (CKKSKeyClass*) keyclass state:(NSString*) state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"keyclass": keyclass, @"currentkey": @"1", @"state":  state ? state : SecCKKSProcessedStateLocal, @"ckzone":zoneID.zoneName} error:error];
}

/* Returns all keys for a zone */
+ (NSArray<CKKSKey*>*)allKeys: (CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"ckzone":zoneID.zoneName} error:error];
}

/* Returns all keys marked 'remote', i.e., downloaded from CloudKit */
+ (NSArray<CKKSKey*>*)remoteKeys: (CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"state":  SecCKKSProcessedStateRemote, @"ckzone":zoneID.zoneName} error:error];
}

/* Returns all keys marked 'local', i.e., processed in the past */
+ (NSArray<CKKSKey*>*)localKeys:  (CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"state":  SecCKKSProcessedStateLocal, @"ckzone":zoneID.zoneName} error:error];
}

- (bool)saveToDatabaseAsOnlyCurrentKeyForClassAndState: (NSError * __autoreleasing *) error {
    self.currentkey = true;

    // Find other keys for our key class
    NSArray<CKKSKey*>* keys = [CKKSKey currentKeysForClass: self.keyclass state: self.state zoneID:self.zoneID error:error];
    if(!keys) {
        return false;
    }

    for(CKKSKey* key in keys) {
        key.currentkey = false;
        if(![key saveToDatabase: error]) {
            return false;
        }
    }
    if(![self saveToDatabase: error]) {
        return false;
    }

    return true;
}

#pragma mark - CKRecord handling

- (void) setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqual: SecCKRecordIntermediateKeyType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordIntermediateKeyType]
                userInfo:nil];
    }

    [self setStoredCKRecord: record];

    self.uuid = [[record recordID] recordName];
    if(record[SecCKRecordParentKeyRefKey] != nil) {
        self.parentKeyUUID = [record[SecCKRecordParentKeyRefKey] recordID].recordName;
    } else {
        // We wrap ourself.
        self.parentKeyUUID = self.uuid;
    }

    self.keyclass = record[SecCKRecordKeyClassKey];
    self.wrappedkey = [[CKKSWrappedAESSIVKey alloc] initWithBase64: record[SecCKRecordWrappedKeyKey]];
}

- (CKRecord*) updateCKRecord: (CKRecord*) record zoneID: (CKRecordZoneID*) zoneID {
    if(![record.recordType isEqual: SecCKRecordIntermediateKeyType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordIntermediateKeyType]
                userInfo:nil];
    }

    // The parent key must exist in CloudKit, or this record save will fail.
    if([self.parentKeyUUID isEqual: self.uuid]) {
        // We wrap ourself. No parent.
        record[SecCKRecordParentKeyRefKey] = nil;
    } else {
        record[SecCKRecordParentKeyRefKey] = [[CKReference alloc] initWithRecordID: [[CKRecordID alloc] initWithRecordName: self.parentKeyUUID zoneID: zoneID] action: CKReferenceActionValidate];
    }

    [CKKSItem setOSVersionInRecord: record];

    record[SecCKRecordKeyClassKey] = self.keyclass;
    record[SecCKRecordWrappedKeyKey] = [self.wrappedkey base64WrappedKey];

    return record;
}

- (bool)matchesCKRecord:(CKRecord*)record {
    if(![record.recordType isEqual: SecCKRecordIntermediateKeyType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: self.uuid]) {
        secinfo("ckkskey", "UUID does not match");
        return false;
    }

    // For the parent key ref, ensure that if it's nil, we wrap ourself
    if(record[SecCKRecordParentKeyRefKey] == nil) {
        if(![self wrapsSelf]) {
            secinfo("ckkskey", "wrapping key reference (self-wrapped) does not match");
            return false;
        }

    } else {
        if(![[[record[SecCKRecordParentKeyRefKey] recordID] recordName] isEqualToString: self.parentKeyUUID]) {
            secinfo("ckkskey", "wrapping key reference (non-self-wrapped) does not match");
            return false;
        }
    }

    if(![record[SecCKRecordKeyClassKey] isEqual: self.keyclass]) {
        secinfo("ckkskey", "key class does not match");
        return false;
    }

    if(![record[SecCKRecordWrappedKeyKey] isEqual: [self.wrappedkey base64WrappedKey]]) {
        secinfo("ckkskey", "wrapped key does not match");
        return false;
    }

    return true;
}


#pragma mark - Utility

- (NSString*)description {
    return [NSString stringWithFormat: @"<%@(%@): %@ (%@,%@:%d)>",
            NSStringFromClass([self class]),
            self.zoneID.zoneName,
            self.uuid,
            self.keyclass,
            self.state,
            self.currentkey];
}

#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*) sqlTable {
    return @"synckeys";
}

+ (NSArray<NSString*>*) sqlColumns {
    return @[@"UUID", @"parentKeyUUID", @"ckzone", @"ckrecord", @"keyclass", @"state", @"currentkey", @"wrappedkey"];
}

- (NSDictionary<NSString*,NSString*>*) whereClauseToFindSelf {
    return @{@"UUID": self.uuid, @"state": self.state, @"ckzone":self.zoneID.zoneName};
}

- (NSDictionary<NSString*,NSString*>*) sqlValues {
    return @{@"UUID": self.uuid,
             @"parentKeyUUID": self.parentKeyUUID ? self.parentKeyUUID : self.uuid, // if we don't have a parent, we wrap ourself.
             @"ckzone": CKKSNilToNSNull(self.zoneID.zoneName),
             @"ckrecord": CKKSNilToNSNull([self.encodedCKRecord base64EncodedStringWithOptions:0]),
             @"keyclass": CKKSNilToNSNull(self.keyclass),
             @"state": CKKSNilToNSNull(self.state),
             @"wrappedkey": CKKSNilToNSNull([self.wrappedkey base64WrappedKey]),
             @"currentkey": self.currentkey ? @"1" : @"0"};
}

+ (instancetype) fromDatabaseRow: (NSDictionary*) row {
    return [[CKKSKey alloc] initWithWrappedAESKey: row[@"wrappedkey"] ? [[CKKSWrappedAESSIVKey alloc] initWithBase64: row[@"wrappedkey"]] : nil
                                             uuid: row[@"UUID"]
                                    parentKeyUUID: row[@"parentKeyUUID"]
                                         keyclass: row[@"keyclass"]
                                            state: row[@"state"]
                                           zoneID: [[CKRecordZoneID alloc] initWithZoneName: row[@"ckzone"] ownerName:CKCurrentUserDefaultName]
                                  encodedCKRecord: [[NSData alloc] initWithBase64EncodedString: row[@"ckrecord"] options:0]
                                       currentkey: [row[@"currentkey"] integerValue]];

}

+ (NSDictionary<NSString*,NSNumber*>*)countsByClass:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    NSMutableDictionary* results = [[NSMutableDictionary alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [[self class] sqlTable]
                                        where: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns: @[@"keyclass", @"state", @"count(rowid)"]
                                      groupBy: @[@"keyclass", @"state"]
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary* row) {
                                       results[[NSString stringWithFormat: @"%@-%@", row[@"state"], row[@"keyclass"]]] =
                                            [NSNumber numberWithInteger: [row[@"count(rowid)"] integerValue]];
                                   }
                                        error: error];
    return results;
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSKey *keyCopy = [super copyWithZone:zone];
    keyCopy->_aessivkey = _aessivkey;
    keyCopy->_state = _state;
    keyCopy->_keyclass = _keyclass;
    keyCopy->_currentkey = _currentkey;
    return keyCopy;
}

- (NSData*)serializeAsProtobuf: (NSError * __autoreleasing *) error {
    if(![self ensureKeyLoaded:error]) {
        return nil;
    }
    CKKSSerializedKey* proto = [[CKKSSerializedKey alloc] init];

    proto.uuid = self.uuid;
    proto.zoneName = self.zoneID.zoneName;
    proto.keyclass = self.keyclass;
    proto.key = [[NSData alloc] initWithBytes:self.aessivkey->key length:self.aessivkey->size];

    return proto.data;
}

+ (CKKSKey*)loadFromProtobuf:(NSData*)data error:(NSError* __autoreleasing *)error {
    CKKSSerializedKey* key = [[CKKSSerializedKey alloc] initWithData: data];
    if(key && key.uuid && key.zoneName && key.keyclass && key.key) {
        return [[CKKSKey alloc] initSelfWrappedWithAESKey:[[CKKSAESSIVKey alloc] initWithBytes:(uint8_t*)key.key.bytes len:key.key.length]
                                                     uuid:key.uuid
                                                 keyclass:(CKKSKeyClass*)key.keyclass // TODO sanitize
                                                    state:SecCKKSProcessedStateRemote
                                                   zoneID:[[CKRecordZoneID alloc] initWithZoneName:key.zoneName
                                                                                         ownerName:CKCurrentUserDefaultName]
                                          encodedCKRecord:nil
                                               currentkey:false];
    }

    if(error) {
        *error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSProtobufFailure description:@"Data failed to parse as a CKKSSerializedKey"];
    }
    return nil;
}

@end

#endif // OCTAGON
