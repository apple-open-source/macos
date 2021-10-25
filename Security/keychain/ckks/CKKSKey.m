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
#import "keychain/ckks/CKKSPeerProvider.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#include "keychain/securityd/SecItemSchema.h"
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include "OSX/sec/Security/SecItemShim.h"

#include <CloudKit/CloudKit.h>
#include <CloudKit/CloudKit_Private.h>

#import <Foundation/NSData_Private.h>

@interface CKKSKey ()
@property (nullable) CKKSKeychainBackedKey* keycore;
@property NSData* wrappedKeyDataBackingStore;

// make writable
@property NSString* uuid;
@property NSString* parentKeyUUID;
@property CKKSKeyClass* keyclass;
@end

@implementation CKKSKey

- (instancetype)init {
    if ((self = [super init])) {
    }
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
    if((self = [super initWithCKRecordType:SecCKRecordIntermediateKeyType
                           encodedCKRecord:encodedrecord
                                    zoneID:zoneID])) {

        _uuid = uuid;
        _parentKeyUUID = uuid;
        _keyclass = keyclass;

        NSError* localerror = nil;
        _keycore = [CKKSKeychainBackedKey keyWrappedBySelf:aeskey
                                                      uuid:uuid
                                                  keyclass:keyclass
                                                    zoneID:zoneID
                                                     error:&localerror];

        if(!_keycore) {
            return nil;
        }

        _wrappedKeyDataBackingStore = _keycore.wrappedkey.wrappedData;

        _currentkey = !!currentkey;
        _state = state;
    }
    return self;
}

- (instancetype)initWithWrappedKeyData:(NSData*)wrappedKeyData
                                  uuid:(NSString*)uuid
                         parentKeyUUID:(NSString*)parentKeyUUID
                              keyclass:(CKKSKeyClass*)keyclass
                                 state:(CKKSProcessedState*)state
                                zoneID:(CKRecordZoneID*)zoneID
                       encodedCKRecord:(NSData*)encodedrecord
                            currentkey:(NSInteger)currentkey
{
    if((self = [super initWithCKRecordType:SecCKRecordIntermediateKeyType
                           encodedCKRecord:encodedrecord
                                    zoneID:zoneID])) {

        _wrappedKeyDataBackingStore = wrappedKeyData;
        _uuid = uuid;
        _parentKeyUUID = parentKeyUUID;
        _keyclass = keyclass;

        _currentkey = !!currentkey;
        _state = state;
    }
    return self;
}

- (instancetype)initWithKeyCore:(CKKSKeychainBackedKey*)core
                          state:(CKKSProcessedState*)state
                     currentkey:(bool)currentkey
{
    if((self = [super initWithCKRecordType:SecCKRecordIntermediateKeyType
                           encodedCKRecord:nil
                                    zoneID:core.zoneID])) {
        _keycore = core;
        _wrappedKeyDataBackingStore = _keycore.wrappedkey.wrappedData;

        _uuid = core.uuid;
        _parentKeyUUID = core.parentKeyUUID;
        _keyclass = core.keyclass;

        _currentkey = currentkey;
        _state = state;
    }
    return self;
}

- (void)dealloc {
}

- (BOOL)isEqual:(id)object {
    if(![object isKindOfClass:[CKKSKey class]]) {
        return NO;
    }

    CKKSKey* obj = (CKKSKey*)object;

    // Equality ignores state, currentkey, and CK record differences. Be careful...
    return [self.uuid isEqualToString:obj.uuid] &&
        [self.parentKeyUUID isEqualToString:obj.parentKeyUUID] &&
        [self.wrappedKeyData isEqualToData:obj.wrappedKeyData] &&
        [self.zoneID isEqual:obj.zoneID] &&
        [self.keyclass isEqual:obj.keyclass]
        ? YES : NO;
}

- (CKKSKeychainBackedKey* _Nullable)getKeychainBackedKey:(NSError**)error
{
    if(self.keycore) {
        return self.keycore;
    }

    if(self.wrappedKeyDataBackingStore.length != CKKSWrappedKeySize) {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:errSecParam
                                  description:@"Wrong key size"];
        }
        return nil;
    }

    CKKSWrappedAESSIVKey* wrappedaeskey = [[CKKSWrappedAESSIVKey alloc] initWithData:self.wrappedKeyDataBackingStore];

    self.keycore = [[CKKSKeychainBackedKey alloc] initWithWrappedAESKey:wrappedaeskey
                                                                   uuid:self.uuid
                                                          parentKeyUUID:self.parentKeyUUID
                                                               keyclass:self.keyclass
                                                                 zoneID:self.zoneID];
    return self.keycore;
}

- (NSData*)wrappedKeyData
{
    if(self.keycore) {
        if(![self.keycore.wrappedkey.wrappedData isEqualToData:self.wrappedKeyDataBackingStore]) {
            ckkserror("ckkskey", self.zoneID, "Probable bug: wrapped key data does not match cached version");
            self.wrappedKeyDataBackingStore = self.keycore.wrappedkey.wrappedData;
        }

        return self.keycore.wrappedkey.wrappedData;
    } else {
        return self.wrappedKeyDataBackingStore;
    }
}

- (NSString*)zoneName
{
    return self.zoneID.zoneName;
}


- (bool)wrapsSelf {
    return [self.uuid isEqual:self.parentKeyUUID];
}

- (bool)wrapUnder: (CKKSKey*) wrappingKey error: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return false;
    }

    if(![self.keycore wrapUnder:wrappingKey.keycore error:error]) {
        return false;
    }

    self.parentKeyUUID = self.keycore.parentKeyUUID;
    self.wrappedKeyDataBackingStore = self.keycore.wrappedkey.wrappedData;
    return true;
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
    CKKSKeychainBackedKey* parentKeyCore = [parentKey getKeychainBackedKey:error];
    if(parentKeyCore == nil) {
        return nil;
    }

    CKKSKeychainBackedKey* randomKey = [CKKSKeychainBackedKey randomKeyWrappedByParent:parentKeyCore keyclass:keyclass error:error];

    if(randomKey == nil) {
        return nil;
    }

    return [[CKKSKey alloc] initWithKeyCore:randomKey
                                      state:SecCKKSProcessedStateLocal
                                 currentkey:false];
}

+ (instancetype)randomKeyWrappedBySelf: (CKRecordZoneID*) zoneID error: (NSError * __autoreleasing *) error {
    CKKSKeychainBackedKey* randomKey = [CKKSKeychainBackedKey randomKeyWrappedBySelf:zoneID error:error];
    if(randomKey == nil) {
        return nil;
    }

    return [[CKKSKey alloc] initWithKeyCore:randomKey
                                      state:SecCKKSProcessedStateLocal
                                 currentkey:false];
}

- (CKKSKey*)topKeyInAnyState: (NSError * __autoreleasing *) error {
    NSMutableSet<NSString*>* seenUUID = [[NSMutableSet alloc] init];
    CKKSKey* key = self;

    // Find the top-level key in the hierarchy.
    while (key) {
        if([key wrapsSelf]) {
            return key;
        }

        // Check for circular references.
        if([seenUUID containsObject:key.uuid]) {
            if (error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSCircularKeyReference
                                      description:@"Circular reference in key hierarchy"];
            }
            return nil;
        }

        [seenUUID addObject:key.uuid];

        // Prefer 'remote' parents.
        CKKSKey* parent = [CKKSKey tryFromDatabaseWhere: @{@"UUID": key.parentKeyUUID, @"state": SecCKKSProcessedStateRemote} error: error];

        // No remote parent. Fall back to anything.
        if(parent == nil) {
            parent = [CKKSKey fromDatabaseWhere: @{@"UUID": key.parentKeyUUID} error: error];
        }

        key = parent;
    }

    // Couldn't get the parent. Error is already filled.
    return nil;
}

- (CKKSKeychainBackedKey* _Nullable)ensureKeyLoaded:(NSError * __autoreleasing *)error
{
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    NSError* keychainError = nil;

    CKKSAESSIVKey* sivkey = [self.keycore ensureKeyLoadedFromKeychain:&keychainError];
    if(sivkey) {
        return self.keycore;
    }

    // Uhh, okay, if that didn't work, try to unwrap via the key hierarchy
    NSError* keyHierarchyError = nil;
    if([self unwrapViaKeyHierarchy:&keyHierarchyError]) {
        // Attempt to save this new key, but don't error if it fails
        NSError* resaveError = nil;
        if(![self saveKeyMaterialToKeychain:&resaveError] || resaveError) {
            ckkserror("ckkskey", self.zoneID, "Resaving missing key failed, continuing: %@", resaveError);
        }

        return self.keycore;
    }

    // Pick an error to report
    if(error) {
        *error = keyHierarchyError ? keyHierarchyError : keychainError;
    }

    return nil;
}

- (CKKSAESSIVKey*)unwrapViaKeyHierarchy: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    if(self.keycore.aessivkey) {
        return self.keycore.aessivkey;
    }

    NSError* localerror = nil;

    // Attempt to load this key from the keychain
    if([self.keycore loadKeyMaterialFromKeychain:&localerror]) {
        // Rad. Success!
        return self.keycore.aessivkey;
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

    self.keycore.aessivkey = [parent unwrapAESKey:self.keycore.wrappedkey error:error];
    return self.keycore.aessivkey;
}

- (BOOL)unwrapViaTLKSharesTrustedBy:(NSArray<CKKSPeerProviderState*>*)trustStates
                              error:(NSError**)error
{
    NSError* localerror = nil;

    if(trustStates.count == 0u) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSLackingTrust
                                  description:@"No current trust states; can't unwrap TLK"];
        }
        return NO;
    }

    NSArray<CKKSTLKShareRecord*>* possibleShares = [CKKSTLKShareRecord allForUUID:self.uuid
                                                                           zoneID:self.zoneID
                                                                            error:&localerror];

    if(!possibleShares || localerror) {
        ckkserror("ckksshare", self, "Unable to load TLK shares for TLK(%@): %@", self, localerror);
        if(error) {
            *error = localerror;
        }
        return NO;
    }

    NSError* lastTrustStateError = nil;
    for(CKKSPeerProviderState* trustState in trustStates) {
        BOOL extracted = [trustState unwrapKey:self
                                    fromShares:possibleShares
                                         error:&localerror];

        if(!extracted || localerror) {
            ckkserror("ckksshare", self, "Failed to recover tlk (%@) from trust state (%@): %@", self.uuid, trustState, localerror);
            lastTrustStateError = localerror;
            localerror = nil;
        } else {
            ckkserror("ckksshare", self, "Recovered tlk (%@) from trust state (%@)", self.uuid, trustState);
            return YES;
        }
    }

    // Because there's at least one trustState, then either we returned the TLK above, or we filled in lastTrustStateError.
    if(error) {
        *error = lastTrustStateError;
    }

    return NO;
}

- (BOOL)validTLK:(NSError**)error
{
    if(![self wrapsSelf]) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain
                                                  code:CKKSKeyNotSelfWrapped
                                           description:[NSString stringWithFormat:@"Potential TLK %@ doesn't wrap itself: %@",
                                                        self,
                                                        self.parentKeyUUID]
                                            underlying:NULL];
        ckkserror("ckksshare", self, "Error with TLK: %@", localerror);
        if (error) {
            *error = localerror;
        }
        return NO;
    }

    return YES;
}

- (BOOL)tlkMaterialPresentOrRecoverableViaTLKShare:(NSArray<CKKSPeerProviderState*>*)trustStates
                                             error:(NSError**)error
{
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return NO;
    }

    // If we have the key material, then this TLK is considered valid.
    NSError* loadError = nil;
    CKKSAESSIVKey* loadedKey = nil;
    if([self ensureKeyLoaded:&loadError]) {
        // Should just return what was loaded in ensureKeyLoaded
        loadedKey = [self.keycore ensureKeyLoadedFromKeychain:&loadError];
    }

    if(!loadedKey || loadError) {
        if(loadError.code == errSecInteractionNotAllowed) {
            ckkserror("ckksshare", self, "Unable to load key due to lock state: %@", loadError);
            if(error) {
                *error = loadError;
            }

            return NO;
        }

        ckkserror("ckksshare", self, "Do not yet have this key in the keychain: %@", loadError);
        // Fall through to attempt to recover the TLK via shares below
    } else {
        bool result = [self trySelfWrappedKeyCandidate:loadedKey error:&loadError];
        if(result) {
            // We have a key, and it can decrypt itself.
            return YES;
        } else {
            ckkserror("ckksshare", self, "Some key is present, but the key is not self-wrapped: %@", loadError);
            // Key seems broken. Fall through.
        }
    }

    NSError* localerror = nil;
    BOOL success = [self unwrapViaTLKSharesTrustedBy:trustStates
                                               error:&localerror];

    if(!success || localerror) {
        ckkserror("ckksshare", self, "Failed to unwrap tlk(%@) via shares: %@", self.uuid, localerror);
        if(error) {
            *error = localerror;
        }
        return NO;
    }

    success = [self saveKeyMaterialToKeychain:true error:&localerror];

    if(!success || localerror) {
        ckkserror("ckksshare", self, "Errored saving TLK to keychain: %@", localerror);

        if(error) {
            *error = localerror;
            return NO;
        }
    }

    return YES;
}

- (bool)trySelfWrappedKeyCandidate:(CKKSAESSIVKey*)candidate error:(NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    return [self.keycore trySelfWrappedKeyCandidate:candidate error:error];
}

- (CKKSWrappedAESSIVKey* _Nullable)wrapAESKey: (CKKSAESSIVKey*) keyToWrap error: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    return [self.keycore wrapAESKey:keyToWrap error:error];
}

- (CKKSAESSIVKey* _Nullable)unwrapAESKey: (CKKSWrappedAESSIVKey*) keyToUnwrap error: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    return [self.keycore unwrapAESKey:keyToUnwrap error:error];
}

- (NSData*)encryptData: (NSData*) plaintext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    return [self.keycore encryptData:plaintext authenticatedData:ad error:error];
}

- (NSData*)decryptData: (NSData*) ciphertext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return nil;
    }

    return [self.keycore decryptData:ciphertext authenticatedData:ad error:error];
}

/* Functions to load and save keys from the keychain (where we get to store actual key material!) */
- (BOOL)saveKeyMaterialToKeychain: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return NO;
    }

    return [self.keycore saveKeyMaterialToKeychain:true error: error];
}

- (BOOL)saveKeyMaterialToKeychain: (bool)stashTLK error:(NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return NO;
    }

    return [self.keycore saveKeyMaterialToKeychain:stashTLK error:error];
}

- (BOOL)loadKeyMaterialFromKeychain: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return NO;
    }

    return [self.keycore loadKeyMaterialFromKeychain:error];
}

- (BOOL)deleteKeyMaterialFromKeychain: (NSError * __autoreleasing *) error {
    /* Ensure that self.keycore is filled in */
    if(nil == [self getKeychainBackedKey:error]) {
        return NO;
    }

    return [self.keycore deleteKeyMaterialFromKeychain:error];
}

+ (NSString* _Nullable)isItemKeyForKeychainView:(SecDbItemRef)item {

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
    return [self allWhere: @{@"UUID": [CKKSSQLWhereColumn op:CKKSSQLWhereComparatorEquals
                                                      column:CKKSSQLWhereColumnNameParentKeyUUID],
                             @"state":  SecCKKSProcessedStateLocal,
                             @"ckzone":zoneID.zoneName}
                    error:error];
}

+ (instancetype _Nullable)currentKeyForClass:(CKKSKeyClass*)keyclass
                                      zoneID:(CKRecordZoneID*)zoneID
                                       error:(NSError *__autoreleasing*)error
{
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

- (NSString*)CKRecordName
{
    return self.uuid;
}

- (void) setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqual: SecCKRecordIntermediateKeyType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordIntermediateKeyType]
                userInfo:nil];
    }

    [self setStoredCKRecord: record];

    self.uuid = record.recordID.recordName;

    if(record[SecCKRecordParentKeyRefKey] != nil) {
        self.parentKeyUUID = [record[SecCKRecordParentKeyRefKey] recordID].recordName;
    } else {
        // We wrap ourself.
        self.parentKeyUUID = self.uuid;
    }

    self.keyclass = record[SecCKRecordKeyClassKey];
    self.wrappedKeyDataBackingStore = [[NSData alloc] initWithBase64EncodedString:record[SecCKRecordWrappedKeyKey] options:0];

    // We will need to re-create the keycore later, if applicable.
    self.keycore = nil;

    self.state = SecCKKSProcessedStateRemote;
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
    record[SecCKRecordWrappedKeyKey] = [self.wrappedKeyData base64EncodedStringWithOptions:0];

    return record;
}

- (bool)matchesCKRecord:(CKRecord*)record {
    if(![record.recordType isEqual: SecCKRecordIntermediateKeyType]) {
        return false;
    }

    if(![record.recordID.recordName isEqualToString: self.uuid]) {
        ckksinfo_global("ckkskey", "UUID does not match");
        return false;
    }

    // For the parent key ref, ensure that if it's nil, we wrap ourself
    if(record[SecCKRecordParentKeyRefKey] == nil) {
        if(![self wrapsSelf]) {
            ckksinfo_global("ckkskey", "wrapping key reference (self-wrapped) does not match");
            return false;
        }

    } else {
        if(![[[record[SecCKRecordParentKeyRefKey] recordID] recordName] isEqualToString: self.parentKeyUUID]) {
            ckksinfo_global("ckkskey", "wrapping key reference (non-self-wrapped) does not match");
            return false;
        }
    }

    if(![record[SecCKRecordKeyClassKey] isEqual: self.keyclass]) {
        ckksinfo_global("ckkskey", "key class does not match");
        return false;
    }

    if(![record[SecCKRecordWrappedKeyKey] isEqual: [self.wrappedKeyData base64EncodedStringWithOptions:0]]) {
        ckksinfo_global("ckkskey", "wrapped key does not match");
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
             @"wrappedkey": CKKSNilToNSNull([self.wrappedKeyData base64EncodedDataWithOptions:0]),
             @"currentkey": self.currentkey ? @"1" : @"0"};
}

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*, CKKSSQLResult*>*)row {
    return [[CKKSKey alloc] initWithWrappedKeyData:row[@"wrappedkey"].asBase64DecodedData
                                              uuid:row[@"UUID"].asString
                                     parentKeyUUID:row[@"parentKeyUUID"].asString
                                          keyclass:(CKKSKeyClass*)row[@"keyclass"].asString
                                             state:(CKKSProcessedState*)row[@"state"].asString
                                            zoneID:[[CKRecordZoneID alloc] initWithZoneName:row[@"ckzone"].asString ownerName:CKCurrentUserDefaultName]
                                   encodedCKRecord:row[@"ckrecord"].asBase64DecodedData
                                        currentkey:row[@"currentkey"].asNSInteger];
}

+ (NSNumber* _Nullable)counts:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error
{
    __block NSNumber *result = nil;

    [CKKSSQLDatabaseObject queryDatabaseTable:[[self class] sqlTable]
                                        where:@{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns:@[@"count(rowid)"]
                                      groupBy:nil
                                      orderBy:nil
                                        limit:-1
                                   processRow:^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       result = row[@"count(rowid)"].asNSNumberInteger;
                                   }
                                        error: error];
    return result;
}

+ (NSDictionary<NSString*,NSNumber*>*)countsByClass:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    NSMutableDictionary* results = [[NSMutableDictionary alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [[self class] sqlTable]
                                        where: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns: @[@"keyclass", @"state", @"count(rowid)"]
                                      groupBy: @[@"keyclass", @"state"]
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[[NSString stringWithFormat: @"%@-%@", row[@"state"].asString, row[@"keyclass"].asString]] =
                                            row[@"count(rowid)"].asNSNumberInteger;
                                   }
                                        error: error];
    return results;
}

- (instancetype)copyWithZone:(NSZone *)zone {
    CKKSKey *keyCopy = [super copyWithZone:zone];
    keyCopy->_keycore = [_keycore copyWithZone:zone];

    keyCopy->_state = _state;
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
    proto.key = [NSData _newZeroingDataWithBytes:self.keycore.aessivkey->key length:self.keycore.aessivkey->size];

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


+ (BOOL)intransactionRecordChanged:(CKRecord*)record
                            resync:(BOOL)resync
                       flagHandler:(id<OctagonStateFlagHandler> _Nullable)flagHandler
                             error:(NSError**)error
{
    NSError* localerror = nil;

    if(resync) {
        NSError* resyncerror = nil;

        CKKSKey* key = [CKKSKey tryFromDatabaseAnyState:record.recordID.recordName zoneID:record.recordID.zoneID error:&resyncerror];
        if(resyncerror) {
            ckkserror("ckksresync", record.recordID.zoneID, "error loading key: %@", resyncerror);
        }
        if(!key) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: No sync key matching resynced CloudKit record: %@", record);
        } else if(![key matchesCKRecord:record]) {
            ckkserror("ckksresync", record.recordID.zoneID, "BUG: Local sync key doesn't match resynced CloudKit record(s): %@ %@", key, record);
        } else {
            ckksnotice("ckksresync", record.recordID.zoneID, "Already know about this sync key, skipping update: %@", record);
            return YES;
        }
    }

    CKKSKey* remotekey = [[CKKSKey alloc] initWithCKRecord:record];

    // Do we already know about this key?
    CKKSKey* possibleLocalKey = [CKKSKey tryFromDatabase:remotekey.uuid
                                                  zoneID:record.recordID.zoneID
                                                   error:&localerror];
    if(localerror) {
        ckkserror("ckkskey", record.recordID.zoneID, "Error finding existing local key for %@: %@", remotekey, localerror);
        // Go on, assuming there isn't a local key

        localerror = nil;
    } else if(possibleLocalKey && [possibleLocalKey matchesCKRecord:record]) {
        // Okay, nothing new here. Update the CKRecord and move on.
        // Note: If the new record doesn't match the local copy, we have to go through the whole dance below
        possibleLocalKey.storedCKRecord = record;
        bool newKeySaved = [possibleLocalKey saveToDatabase:&localerror];

        if(!newKeySaved || localerror) {
            ckkserror("ckkskey", record.recordID.zoneID, "Couldn't update existing key: %@: %@", possibleLocalKey, localerror);
            if(error) {
                *error = localerror;
            }
            return NO;
        }
        return YES;
    }

    // Drop into the synckeys table as a 'remote' key, then ask for a rekey operation.
    remotekey.state = SecCKKSProcessedStateRemote;
    remotekey.currentkey = false;

    bool remoteKeySaved = [remotekey saveToDatabase:&localerror];
    if(!remoteKeySaved || localerror) {
        ckkserror("ckkskey", record.recordID.zoneID, "Couldn't save key record to database: %@: %@", remotekey, localerror);
        ckksinfo("ckkskey", record.recordID.zoneID, "CKRecord was %@", record);

        if(error) {
            *error = localerror;
        }
        return NO;
    }

    // We've saved a new key in the database; trigger a rekey operation.
    [flagHandler _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];

    return YES;
}

+ (BOOL)intransactionRecordDeleted:(CKRecordID*)recordID
                             error:(NSError**)error
{
    do {
        NSError* localError = nil;
        CKKSKey* key = [CKKSKey tryFromDatabaseAnyState:recordID.recordName
                                                 zoneID:recordID.zoneID
                                                  error:&localError];

        if(!key) {
            if(localError) {
                ckkserror("ckkskey", recordID.zoneID, "Couldn't load key record from database: %@: %@", recordID, localError);
                if(error) {
                    *error = localError;
                }
                return NO;
            }

            return YES;
        }

        NSError* deleteError = nil;
        [key deleteFromDatabase:&deleteError];
        if(deleteError) {
            ckkserror("ckkskey", recordID.zoneID, "Couldn't delete key record from database: %@: %@", recordID, deleteError);
            return NO;
        }

    } while(true);
}

@end

#endif // OCTAGON
