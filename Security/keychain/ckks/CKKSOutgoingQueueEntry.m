/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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

#include <AssertMacros.h>

#import <Foundation/Foundation.h>

#import "CKKSKeychainView.h"

#include <Security/SecItemPriv.h>

#include <utilities/SecDb.h>
#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecItemSchema.h"

#if OCTAGON

#import <CloudKit/CloudKit.h>
#import "CKKSOutgoingQueueEntry.h"
#import "CKKSItemEncrypter.h"
#import "CKKSKey.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ckks/CKKSViewManager.h"


@implementation CKKSOutgoingQueueEntry

- (NSString*)description {
    return [NSString stringWithFormat: @"<%@(%@): %@ %@ (%@)>",
            NSStringFromClass([self class]),
            self.item.zoneID.zoneName,
            self.action,
            self.item.uuid,
            self.state];
}

- (instancetype) initWithCKKSItem:(CKKSItem*) item
                           action: (NSString*) action
                            state: (NSString*) state
                        waitUntil: (NSDate*) waitUntil
                      accessGroup: (NSString*) accessgroup
{
    if((self = [super init])) {
        _item = item;
        _action = action;
        _state = state;
        _accessgroup = accessgroup;
        _waitUntil = waitUntil;
    }

    return self;
}

- (BOOL)isEqual: (id) object {
    if(![object isKindOfClass:[CKKSOutgoingQueueEntry class]]) {
        return NO;
    }

    CKKSOutgoingQueueEntry* obj = (CKKSOutgoingQueueEntry*) object;

    return ([self.item isEqual: obj.item] &&
            [self.action isEqual: obj.action] &&
            [self.state isEqual: obj.state] &&
           ((self.waitUntil == nil && obj.waitUntil == nil) || (fabs([self.waitUntil timeIntervalSinceDate: obj.waitUntil]) < 1)) &&
            [self.accessgroup isEqual: obj.accessgroup] &&
            true) ? YES : NO;
}


+ (CKKSKey*)keyForItem:(SecDbItemRef)item
                zoneID:(CKRecordZoneID*)zoneID
              keyCache:(CKKSMemoryKeyCache* _Nullable)keyCache
                 error:(NSError * __autoreleasing *)error
{
    if(!item) {
        ckkserror("ckks-key", zoneID, "Cannot select a key for no item!");
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSErrorUnexpectedNil
                                  description:@"can't pick a key class for an empty item"];
        }
        return nil;
    }

    CKKSKeyClass* class = nil;

    NSString* protection = (__bridge NSString*)SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);
    if([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleWhenUnlocked]) {
        class = SecCKKSKeyClassA;
    } else if([protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAlwaysPrivate] ||
              [protection isEqualToString: (__bridge NSString*)kSecAttrAccessibleAfterFirstUnlock]) {
        class = SecCKKSKeyClassC;
    } else {
        NSError* localError = [NSError errorWithDomain:CKKSErrorDomain
                                                  code:CKKSInvalidKeyClass
                                           description:[NSString stringWithFormat:@"can't pick key class for protection %@", protection]];
        ckkserror("ckks-key", zoneID, "can't pick key class: %@ %@", localError, item);
        if(error) {
            *error = localError;
        }

        return nil;
    }

    NSError* currentKeyError = nil;
    CKKSKey* key = nil;

    if(keyCache) {
        key = [keyCache currentKeyForClass:class zoneID:zoneID error:&currentKeyError];
    } else {
        key = [CKKSKey currentKeyForClass:class zoneID:zoneID error:&currentKeyError];
    }
    if(!key || currentKeyError) {
        ckkserror("ckks-key", zoneID, "Couldn't find current key for %@: %@", class, currentKeyError);

        if(error) {
            *error = currentKeyError;
        }
        return nil;
    }

    // and make sure it's unwrapped.
    NSError* loadedError = nil;
    if(![key ensureKeyLoaded:&loadedError]) {
        ckkserror("ckks-key", zoneID, "Couldn't load key(%@): %@", key, loadedError);
        if(error) {
            *error = loadedError;
        }
        return nil;
    }

    return key;
}

+ (instancetype)withItem:(SecDbItemRef)item
                  action:(NSString*)action
                  zoneID:(CKRecordZoneID*)zoneID
                keyCache:(CKKSMemoryKeyCache* _Nullable)keyCache
                   error:(NSError * __autoreleasing *)error
{
    CFErrorRef cferror = NULL;
    CKKSKey* key = nil;
    NSString* uuid = nil;
    NSString* accessgroup = nil;

    NSInteger newGenerationCount = -1;

    NSMutableDictionary* objd = nil;

    ckkserror("ckksitem", zoneID, "Creating a (%@) outgoing queue entry for: %@", action, item);

    NSError* keyError = nil;
    key = [self keyForItem:item
                    zoneID:zoneID
                  keyCache:keyCache
                     error:&keyError];
    if(!key || keyError) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:keyError.code description:@"No key for item" underlying:keyError];
        ckkserror("ckksitem", zoneID, "no key for item: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    objd = (__bridge_transfer NSMutableDictionary*) SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &cferror);
    if(!objd) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:CFErrorGetCode(cferror) description:@"Couldn't create object plist" underlying:(__bridge_transfer NSError*)cferror];
        ckkserror("ckksitem", zoneID, "no plist: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    // Object classes aren't in the item plist, set them specifically
    [objd setObject: (__bridge NSString*) item->class->name forKey: (__bridge NSString*) kSecClass];

    uuid = (__bridge_transfer NSString*) CFRetainSafe(SecDbItemGetValue(item, &v10itemuuid, &cferror));
    if(!uuid || cferror) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoUUIDOnItem description:@"No UUID for item" underlying:(__bridge_transfer NSError*)cferror];
        ckkserror("ckksitem", zoneID, "No UUID for item: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }
    if([uuid isKindOfClass:[NSNull class]]) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoUUIDOnItem description:@"UUID not found in object" underlying:nil];
        ckkserror("ckksitem", zoneID, "couldn't fetch UUID: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    accessgroup = (__bridge_transfer NSString*) CFRetainSafe(SecDbItemGetValue(item, &v6agrp, &cferror));
    if(!accessgroup || cferror) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:CFErrorGetCode(cferror) description:@"accessgroup not found in object" underlying:(__bridge_transfer NSError*)cferror];
        ckkserror("ckksitem", zoneID, "couldn't fetch access group from item: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }
    if([accessgroup isKindOfClass:[NSNull class]]) {
        // That's okay; this is only used for rate limiting.
        ckkserror("ckksitem", zoneID, "couldn't fetch accessgroup: %@", item);
        accessgroup = @"no-group";
    }

    CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid zoneID:zoneID error:error];

    // The action this change should be depends on any existing pending action, if any
    // Particularly, we need to coalesce (existing action, new action) to:
    //    (add, modify) => add
    //    (add, delete) => no-op
    //    (delete, add) => modify
    NSString* actualAction = action;

    NSError* fetchError = nil;
    CKKSOutgoingQueueEntry* existingOQE = [CKKSOutgoingQueueEntry tryFromDatabase:uuid state:SecCKKSStateNew zoneID:zoneID error:&fetchError];
    if(existingOQE) {
        if([existingOQE.action isEqual: SecCKKSActionAdd]) {
            if([action isEqual:SecCKKSActionModify]) {
                actualAction = SecCKKSActionAdd;
            } else if([action isEqual:SecCKKSActionDelete]) {
                // we're deleting an add. If there's a ckme, keep as a delete
                if(!ckme) {
                    // Otherwise, remove from outgoingqueue and don't make a new OQE.
                    [existingOQE deleteFromDatabase:error];
                    return nil;
                }
            }
        }

        if([existingOQE.action isEqual: SecCKKSActionDelete] && [action isEqual:SecCKKSActionAdd]) {
            actualAction = SecCKKSActionModify;
        }
    } else if(fetchError) {
        ckkserror("ckksitem", zoneID, "Unable to fetch an existing OQE due to error: %@", fetchError);
        fetchError = nil;

    } else {
        if(!ckme && [action isEqualToString:SecCKKSActionDelete]) {
            CKKSOutgoingQueueEntry* anyExistingOQE = [CKKSOutgoingQueueEntry tryFromDatabase:uuid zoneID:zoneID error:&fetchError];

            if(fetchError) {
                ckkserror("ckksitem", zoneID, "Unable to fetch an existing OQE (any state) due to error: %@", fetchError);
            } else if(!anyExistingOQE) {
                // This is a delete for an item which doesn't exist. Therefore, this is a no-op.
                ckkserror("ckksitem", zoneID, "Asked to delete a record for which we don't have a CKME or any OQE, ignoring: %@", uuid);
                return nil;
            }
        }
    }

    newGenerationCount = ckme ? ckme.item.generationCount : (NSInteger) 0; // TODO: this is wrong

    // Is this modification just changing the mdat? As a performance improvement, don't update the item in CK
    if(ckme && !existingOQE && [actualAction isEqualToString:SecCKKSActionModify]) {
        NSError* ckmeError = nil;
        NSMutableDictionary* mirror = [[CKKSItemEncrypter decryptItemToDictionary:ckme.item keyCache:keyCache error:&ckmeError] mutableCopy];
        NSMutableDictionary* objdCopy = [objd mutableCopy];

        if(ckmeError) {
            ckkserror("ckksitem", zoneID, "Unable to decrypt current CKME: %@", ckmeError);
        } else {
            mirror[(__bridge id)kSecAttrModificationDate] = nil;
            mirror[(__bridge id)kSecAttrSHA1] = nil;
            objdCopy[(__bridge id)kSecAttrModificationDate] = nil;
            objdCopy[(__bridge id)kSecAttrSHA1] = nil;

            if([mirror isEqualToDictionary:objdCopy]) {
                ckksnotice("ckksitem", zoneID, "Update to item only changes mdat; skipping %@", uuid);
                return nil;
            }
        }
    }

    // Pull out any unencrypted fields
    NSNumber* pcsServiceIdentifier = objd[(id)kSecAttrPCSPlaintextServiceIdentifier];
    objd[(id)kSecAttrPCSPlaintextServiceIdentifier] = nil;

    NSData* pcsPublicKey = objd[(id)kSecAttrPCSPlaintextPublicKey];
    objd[(id)kSecAttrPCSPlaintextPublicKey] = nil;

    NSData* pcsPublicIdentity = objd[(id)kSecAttrPCSPlaintextPublicIdentity];
    objd[(id)kSecAttrPCSPlaintextPublicIdentity] = nil;

    CKKSItem* baseitem = [[CKKSItem alloc] initWithUUID:uuid
                                          parentKeyUUID:key.uuid
                                                 zoneID:zoneID
                                        encodedCKRecord:nil
                                                encItem:nil
                                             wrappedkey:nil
                                        generationCount:newGenerationCount
                                                 encver:currentCKKSItemEncryptionVersion
                          plaintextPCSServiceIdentifier:pcsServiceIdentifier
                                  plaintextPCSPublicKey:pcsPublicKey
                             plaintextPCSPublicIdentity:pcsPublicIdentity];

    if(!baseitem) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:CKKSItemCreationFailure description:@"Couldn't create an item" underlying:nil];
        ckkserror("ckksitem", zoneID, "couldn't create an item: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSError* encryptionError = nil;
    CKKSItem* encryptedItem = [CKKSItemEncrypter encryptCKKSItem:baseitem
                                                  dataDictionary:objd
                                                updatingCKKSItem:ckme.item
                                                       parentkey:key
                                                        keyCache:keyCache
                                                           error:&encryptionError];

    if(!encryptedItem || encryptionError) {
        NSError* localerror = [NSError errorWithDomain:CKKSErrorDomain code:encryptionError.code description:@"Couldn't encrypt item" underlying:encryptionError];
        ckkserror("ckksitem", zoneID, "couldn't encrypt item: %@ %@", localerror, item);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    return [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:encryptedItem
                                                     action:actualAction
                                                      state:SecCKKSStateNew
                                                  waitUntil:nil
                                                accessGroup:accessgroup];
}

#pragma mark - Property access to underlying CKKSItem

-(NSString*)uuid {
    return self.item.uuid;
}

-(void)setUuid:(NSString *)uuid {
    self.item.uuid = uuid;
}

#pragma mark - Database Operations

+ (instancetype) fromDatabase: (NSString*) uuid state: (NSString*) state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fromDatabaseWhere: @{@"UUID": CKKSNilToNSNull(uuid), @"state": CKKSNilToNSNull(state), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error: error];
}

+ (instancetype) tryFromDatabase: (NSString*) uuid zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"UUID": CKKSNilToNSNull(uuid), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error: error];
}

+ (instancetype) tryFromDatabase: (NSString*) uuid state: (NSString*) state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self tryFromDatabaseWhere: @{@"UUID": CKKSNilToNSNull(uuid), @"state":CKKSNilToNSNull(state), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error: error];
}

+ (NSArray<CKKSOutgoingQueueEntry*>*) fetch:(ssize_t) n state: (NSString*) state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self fetch:n where: @{@"state":CKKSNilToNSNull(state), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSOutgoingQueueEntry*>*) allInState: (NSString*) state zoneID:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    return [self allWhere: @{@"state":CKKSNilToNSNull(state), @"ckzone":CKKSNilToNSNull(zoneID.zoneName)} error:error];
}

+ (NSArray<CKKSOutgoingQueueEntry*>*)allWithUUID:(NSString*)uuid states:(NSArray<NSString*>*)states zoneID:(CKRecordZoneID*)zoneID error:(NSError * __autoreleasing *)error
{
    return [self allWhere:@{@"UUID": CKKSNilToNSNull(uuid),
                            @"state": [[CKKSSQLWhereIn alloc] initWithValues:states],
                            @"ckzone":CKKSNilToNSNull(zoneID.zoneName)}
                    error:error];
}


#pragma mark - CKKSSQLDatabaseObject methods

+ (NSString*)sqlTable {
    return @"outgoingqueue";
}

+ (NSArray<NSString*>*)sqlColumns {
    return [[CKKSItem sqlColumns] arrayByAddingObjectsFromArray: @[@"action", @"state", @"waituntil", @"accessgroup"]];
}

- (NSDictionary<NSString*,NSString*>*)whereClauseToFindSelf {
    return @{@"UUID": self.uuid, @"state": self.state, @"ckzone":self.item.zoneID.zoneName};
}

- (NSDictionary<NSString*,NSString*>*)sqlValues {
    NSISO8601DateFormatter* dateFormat = [[NSISO8601DateFormatter alloc] init];

    NSMutableDictionary* values = [[self.item sqlValues] mutableCopy];
    values[@"action"] = self.action;
    values[@"state"] = self.state;
    values[@"waituntil"] = CKKSNilToNSNull(self.waitUntil ? [dateFormat stringFromDate: self.waitUntil] : nil);
    values[@"accessgroup"] = self.accessgroup;

    return values;
}


+ (instancetype)fromDatabaseRow:(NSDictionary<NSString *, CKKSSQLResult*>*) row {
    return [[CKKSOutgoingQueueEntry alloc] initWithCKKSItem:[CKKSItem fromDatabaseRow: row]
                                                     action:row[@"action"].asString
                                                      state:row[@"state"].asString
                                                  waitUntil:row[@"waituntil"].asISO8601Date
                                                accessGroup:row[@"accessgroup"].asString];
}

+ (NSDictionary<NSString*,NSNumber*>*)countsByStateInZone:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    NSMutableDictionary* results = [[NSMutableDictionary alloc] init];

    [CKKSSQLDatabaseObject queryDatabaseTable: [[self class] sqlTable]
                                        where: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName)}
                                      columns: @[@"state", @"count(rowid)"]
                                      groupBy: @[@"state"]
                                      orderBy:nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       results[row[@"state"].asString] = row[@"count(rowid)"].asNSNumberInteger;
                                   }
                                        error: error];
    return results;
}

+ (NSInteger)countByState:(CKKSItemState *)state zone:(CKRecordZoneID*)zoneID error: (NSError * __autoreleasing *) error {
    __block NSInteger result = -1;

    [CKKSSQLDatabaseObject queryDatabaseTable: [[self class] sqlTable]
                                        where: @{@"ckzone": CKKSNilToNSNull(zoneID.zoneName), @"state": state }
                                      columns: @[@"count(*)"]
                                      groupBy: nil
                                      orderBy: nil
                                        limit: -1
                                   processRow: ^(NSDictionary<NSString*, CKKSSQLResult*>* row) {
                                       result = row[@"count(*)"].asNSInteger;
                                   }
                                        error: error];
    return result;
}


#pragma mark - State Transitions

- (BOOL)intransactionMoveToState:(NSString*)state
                       viewState:(CKKSKeychainViewState*)viewState
                           error:(NSError**)error
{
    NSError* localerror = nil;

    if([state isEqualToString:SecCKKSStateDeleted]) {
        // Hurray, this must be a success
        SecBoolNSErrorCallback syncCallback = [[CKKSViewManager manager] claimCallbackForUUID:self.uuid];
        if(syncCallback) {
            syncCallback(true, nil);
        }

        [self deleteFromDatabase:&localerror];
        if(localerror) {
            ckkserror("ckks", viewState, "Couldn't delete %@: %@", self, localerror);
        }

    } else if([self.state isEqualToString:SecCKKSStateInFlight] && [state isEqualToString:SecCKKSStateNew]) {
        // An in-flight OQE is moving to new? See if it's been superceded
        CKKSOutgoingQueueEntry* newOQE = [CKKSOutgoingQueueEntry tryFromDatabase:self.uuid
                                                                           state:SecCKKSStateNew
                                                                          zoneID:viewState.zoneID
                                                                           error:&localerror];
        if(localerror) {
            ckkserror("ckksoutgoing", viewState, "Couldn't fetch an overwriting OQE, assuming one doesn't exist: %@", localerror);
            newOQE = nil;
        }

        if(newOQE) {
            ckksnotice("ckksoutgoing", viewState, "New modification has come in behind inflight %@; dropping failed change", self);
            // recurse for that lovely code reuse
            [self intransactionMoveToState:SecCKKSStateDeleted
                                 viewState:viewState
                                     error:&localerror];
            if(localerror) {
                ckkserror("ckksoutgoing", viewState, "Couldn't delete in-flight OQE: %@", localerror);
            }
        } else {
            self.state = state;
            [self saveToDatabase:&localerror];
            if(localerror) {
                ckkserror("ckks", viewState, "Couldn't save %@ as %@: %@", self, state, localerror);
            }
        }

    } else {
        self.state = state;
        [self saveToDatabase: &localerror];
        if(localerror) {
            ckkserror("ckks", viewState, "Couldn't save %@ as %@: %@", self, state, localerror);
        }
    }

    if(error && localerror) {
        *error = localerror;
    }
    return localerror == nil;
}

- (BOOL)intransactionMarkAsError:(NSError*)itemError
                       viewState:(CKKSKeychainViewState*)viewState
                           error:(NSError* __autoreleasing*)error
{
    SecBoolNSErrorCallback callback = [[CKKSViewManager manager] claimCallbackForUUID:self.uuid];
    if(callback) {
        callback(false, itemError);
    }
    NSError* localerror = nil;

    // Now, delete the OQE: it's never coming back
    [self deleteFromDatabase:&localerror];
    if(localerror) {
        ckkserror("ckks", viewState, "Couldn't delete %@ (due to error %@): %@", self, itemError, localerror);
    }

    if(error && localerror) {
        *error = localerror;
    }
    return localerror == nil;
}

@end

#endif
