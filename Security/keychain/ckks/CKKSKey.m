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
#import "keychain/categories/NSError+UsefulConstructors.h"
#include "keychain/securityd/SecItemSchema.h"
#include <Security/SecItem.h>
#include <Security/SecItemPriv.h>
#include "OSX/sec/Security/SecItemShim.h"

#include <CloudKit/CloudKit.h>
#include <CloudKit/CloudKit_Private.h>

#import <Foundation/NSData_Private.h>

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

        _keycore = [[CKKSKeychainBackedKey alloc] initSelfWrappedWithAESKey:aeskey
                                                                       uuid:uuid
                                                                   keyclass:keyclass
                                                                     zoneID:zoneID];
        if(!_keycore) {
            return nil;
        }

        _currentkey = !!currentkey;
        _state = state;
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
    if((self = [super initWithCKRecordType:SecCKRecordIntermediateKeyType
                          encodedCKRecord:encodedrecord
                                   zoneID:zoneID])) {
        _keycore = [[CKKSKeychainBackedKey alloc] initWrappedBy:wrappingKey.keycore
                                                         AESKey:aeskey
                                                           uuid:uuid
                                                       keyclass:keyclass
                                                         zoneID:zoneID];
        if(!_keycore) {
            return nil;
        }

        _currentkey = !!currentkey;
        _state = state;
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
    if((self = [super initWithCKRecordType:SecCKRecordIntermediateKeyType
                           encodedCKRecord:encodedrecord
                                    zoneID:zoneID])) {

        _keycore = [[CKKSKeychainBackedKey alloc] initWithWrappedAESKey:wrappedaeskey
                                                                   uuid:uuid
                                                          parentKeyUUID:parentKeyUUID
                                                               keyclass:keyclass
                                                                 zoneID:zoneID];

        _currentkey = !!currentkey;
        _state = state;
    }
    return self;
}

- (instancetype)initWithKeyCore:(CKKSKeychainBackedKey*)core
{
    if((self = [super initWithCKRecordType:SecCKRecordIntermediateKeyType
                           encodedCKRecord:nil
                                    zoneID:core.zoneID])) {
        _keycore = core;
        _currentkey = false;
        _state = SecCKKSProcessedStateRemote;
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
    return [self.keycore isEqual:obj.keycore] ? YES : NO;
}

// These used to be properties on CKKSKey, but are now properties on the actual key inside
- (NSString*)uuid
{
    return self.keycore.uuid;
}

- (NSString*)zoneName
{
    return self.keycore.zoneID.zoneName;
}

- (void)setUuid:(NSString *)uuid
{
    self.keycore.uuid = uuid;
}

- (NSString*)parentKeyUUID
{
    return self.keycore.parentKeyUUID;
}

- (void)setParentKeyUUID:(NSString *)parentKeyUUID
{
    self.keycore.parentKeyUUID = parentKeyUUID;
}

- (CKKSKeyClass*)keyclass
{
    return self.keycore.keyclass;
}

- (void)setKeyclass:(CKKSKeyClass*)keyclass
{
    self.keycore.keyclass = keyclass;
}

- (CKKSWrappedAESSIVKey*)wrappedkey
{
    return self.keycore.wrappedkey;
}

- (void)setWrappedkey:(CKKSWrappedAESSIVKey*)wrappedkey
{
    self.keycore.wrappedkey = wrappedkey;
}

- (CKKSAESSIVKey*)aessivkey
{
    return self.keycore.aessivkey;
}

- (bool)wrapsSelf {
    return [self.keycore wrapsSelf];
}

- (bool)wrapUnder: (CKKSKey*) wrappingKey error: (NSError * __autoreleasing *) error {
    return [self.keycore wrapUnder:wrappingKey.keycore error:error];
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
    CKKSAESSIVKey* aessivkey = [CKKSAESSIVKey randomKey:error];
    if(aessivkey == nil) {
        return nil;
    }

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
    CKKSAESSIVKey* aessivkey = [CKKSAESSIVKey randomKey:error];
    if(aessivkey == nil) {
        return nil;
    }

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

- (CKKSAESSIVKey*)ensureKeyLoaded: (NSError * __autoreleasing *) error {
    NSError* keychainError = nil;

    CKKSAESSIVKey* sivkey = [self.keycore ensureKeyLoaded:&keychainError];
    if(sivkey) {
        return sivkey;
    }

    // Uhh, okay, if that didn't work, try to unwrap via the key hierarchy
    NSError* keyHierarchyError = nil;
    if([self unwrapViaKeyHierarchy:&keyHierarchyError]) {
        // Attempt to save this new key, but don't error if it fails
        NSError* resaveError = nil;
        if(![self saveKeyMaterialToKeychain:&resaveError] || resaveError) {
            ckkserror("ckkskey", self.zoneID, "Resaving missing key failed, continuing: %@", resaveError);
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
    if([self.keycore loadKeyMaterialFromKeychain:&localerror]) {
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

    self.keycore.aessivkey = [parent unwrapAESKey:self.wrappedkey error:error];
    return self.aessivkey;
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
    // If we have the key material, then this TLK is considered valid.
    NSError* loadError = nil;
    CKKSAESSIVKey* loadedKey = [self ensureKeyLoaded:&loadError];
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
    return [self.keycore trySelfWrappedKeyCandidate:candidate error:error];
}

- (CKKSWrappedAESSIVKey*)wrapAESKey: (CKKSAESSIVKey*) keyToWrap error: (NSError * __autoreleasing *) error {
    return [self.keycore wrapAESKey:keyToWrap error:error];
}

- (CKKSAESSIVKey*)unwrapAESKey: (CKKSWrappedAESSIVKey*) keyToUnwrap error: (NSError * __autoreleasing *) error {
    return [self.keycore unwrapAESKey:keyToUnwrap error:error];
}

- (NSData*)encryptData: (NSData*) plaintext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    return [self.keycore encryptData:plaintext authenticatedData:ad error:error];
}

- (NSData*)decryptData: (NSData*) ciphertext authenticatedData: (NSDictionary<NSString*, NSData*>*) ad error: (NSError * __autoreleasing *) error {
    return [self.keycore decryptData:ciphertext authenticatedData:ad error:error];
}

/* Functions to load and save keys from the keychain (where we get to store actual key material!) */
- (BOOL)saveKeyMaterialToKeychain: (NSError * __autoreleasing *) error {
    return [self.keycore saveKeyMaterialToKeychain:true error: error];
}

- (BOOL)saveKeyMaterialToKeychain: (bool)stashTLK error:(NSError * __autoreleasing *) error {
    return [self.keycore saveKeyMaterialToKeychain:stashTLK error:error];
}

- (BOOL)loadKeyMaterialFromKeychain: (NSError * __autoreleasing *) error {
    return [self.keycore loadKeyMaterialFromKeychain:error];
}

- (BOOL)deleteKeyMaterialFromKeychain: (NSError * __autoreleasing *) error {
    return [self.keycore deleteKeyMaterialFromKeychain:error];
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
    return self.keycore.uuid;
}

- (void) setFromCKRecord: (CKRecord*) record {
    if(![record.recordType isEqual: SecCKRecordIntermediateKeyType]) {
        @throw [NSException
                exceptionWithName:@"WrongCKRecordTypeException"
                reason:[NSString stringWithFormat: @"CKRecordType (%@) was not %@", record.recordType, SecCKRecordIntermediateKeyType]
                userInfo:nil];
    }

    [self setStoredCKRecord: record];

    NSString* uuid = record.recordID.recordName;
    NSString* parentKeyUUID = nil;

    if(record[SecCKRecordParentKeyRefKey] != nil) {
        parentKeyUUID = [record[SecCKRecordParentKeyRefKey] recordID].recordName;
    } else {
        // We wrap ourself.
        parentKeyUUID = uuid;
    }

    NSString* keyclass = record[SecCKRecordKeyClassKey];
    CKKSWrappedAESSIVKey* wrappedkey =
        [[CKKSWrappedAESSIVKey alloc] initWithBase64:record[SecCKRecordWrappedKeyKey]];

    self.keycore = [[CKKSKeychainBackedKey alloc] initWithWrappedAESKey:wrappedkey
                                                                   uuid:uuid
                                                          parentKeyUUID:parentKeyUUID
                                                               keyclass:(CKKSKeyClass *)keyclass
                                                                 zoneID:record.recordID.zoneID];

    self.keyclass = record[SecCKRecordKeyClassKey];
    self.wrappedkey = [[CKKSWrappedAESSIVKey alloc] initWithBase64: record[SecCKRecordWrappedKeyKey]];

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
    record[SecCKRecordWrappedKeyKey] = [self.wrappedkey base64WrappedKey];

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

    if(![record[SecCKRecordWrappedKeyKey] isEqual: [self.wrappedkey base64WrappedKey]]) {
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
             @"wrappedkey": CKKSNilToNSNull([self.wrappedkey base64WrappedKey]),
             @"currentkey": self.currentkey ? @"1" : @"0"};
}

+ (instancetype)fromDatabaseRow:(NSDictionary<NSString*, CKKSSQLResult*>*)row {
    return [[CKKSKey alloc] initWithWrappedAESKey:row[@"wrappedkey"].asString ? [[CKKSWrappedAESSIVKey alloc] initWithBase64: row[@"wrappedkey"].asString] : nil
                                             uuid:row[@"UUID"].asString
                                    parentKeyUUID:row[@"parentKeyUUID"].asString
                                         keyclass:(CKKSKeyClass*)row[@"keyclass"].asString
                                            state:(CKKSProcessedState*)row[@"state"].asString
                                           zoneID:[[CKRecordZoneID alloc] initWithZoneName:row[@"ckzone"].asString ownerName:CKCurrentUserDefaultName]
                                  encodedCKRecord:row[@"ckrecord"].asBase64DecodedData
                                       currentkey:row[@"currentkey"].asNSInteger];

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
    proto.key = [NSData _newZeroingDataWithBytes:self.aessivkey->key length:self.aessivkey->size];

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
