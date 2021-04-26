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

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSUpdateCurrentItemPointerOperation.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"

#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemDb.h"
#include <Security/SecItemPriv.h>
#include "keychain/securityd/SecDbQuery.h"
#import <CloudKit/CloudKit.h>

@interface CKKSUpdateCurrentItemPointerOperation ()
@property (nullable) CKModifyRecordsOperation* modifyRecordsOperation;
@property (nullable) CKOperationGroup* ckoperationGroup;

@property (nonnull) NSString* accessGroup;

@property (nonnull) NSData* newerItemPersistentRef;
@property (nonnull) NSData* newerItemSHA1;
@property (nullable) NSData* oldItemPersistentRef;
@property (nullable) NSData* oldItemSHA1;

// Store these as properties, so we can release them in our -dealloc
@property (nullable) SecDbItemRef newItem;
@property (nullable) SecDbItemRef oldItem;
@end

@implementation CKKSUpdateCurrentItemPointerOperation

- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks
                                 newItem:(NSData*)newItemPersistentRef
                                    hash:(NSData*)newItemSHA1
                             accessGroup:(NSString*)accessGroup
                              identifier:(NSString*)identifier
                               replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                                    hash:(NSData*)oldItemSHA1
                        ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if((self = [super init])) {
        _ckks = ckks;

        _newerItemPersistentRef = newItemPersistentRef;
        _newerItemSHA1 = newItemSHA1;
        _oldItemPersistentRef = oldCurrentItemPersistentRef;
        _oldItemSHA1 = oldItemSHA1;

        _accessGroup = accessGroup;

        _currentPointerIdentifier = [NSString stringWithFormat:@"%@-%@", accessGroup, identifier];
    }
    return self;
}

- (void)dealloc {
    if(self) {
        CFReleaseNull(self->_newItem);
        CFReleaseNull(self->_oldItem);
    }
}

- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckkscurrent", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:errSecInternalError
                                  description:@"no CKKS object"];
        return;
    }

    WEAKIFY(self);

    [ckks dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        if(self.cancelled) {
            ckksnotice("ckkscurrent", ckks, "CKKSUpdateCurrentItemPointerOperation cancelled, quitting");
            return CKKSDatabaseTransactionRollback;
        }

        NSError* error = nil;
        CFErrorRef cferror = NULL;

        NSString* newItemUUID = nil;
        NSString* oldCurrentItemUUID = nil;

        self.newItem = [self _onqueueFindSecDbItem:self.newerItemPersistentRef accessGroup:self.accessGroup error:&error];
        if(!self.newItem || error) {
            ckksnotice("ckkscurrent", ckks, "Couldn't fetch new item, quitting: %@", error);
            self.error = error;
            return CKKSDatabaseTransactionRollback;
        }

        // Now that we're on the db queue, ensure that the given hashes for the items match the hashes as they are now.
        // That is, the items haven't changed since the caller knew about the item.
        NSData* newItemComputedSHA1 = (NSData*) CFBridgingRelease(CFRetainSafe(SecDbItemGetSHA1(self.newItem, &cferror)));
        if(!newItemComputedSHA1 || cferror ||
           ![newItemComputedSHA1 isEqual:self.newerItemSHA1]) {
            ckksnotice("ckkscurrent", ckks, "Hash mismatch for new item: %@ vs %@", newItemComputedSHA1, self.newerItemSHA1);
            self.error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSItemChanged
                                      description:@"New item has changed; hashes mismatch. Refetch and try again."
                                       underlying:(NSError*)CFBridgingRelease(cferror)];
            return CKKSDatabaseTransactionRollback;
        }

        newItemUUID = (NSString*) CFBridgingRelease(CFRetainSafe(SecDbItemGetValue(self.newItem, &v10itemuuid, &cferror)));
        if(!newItemUUID || cferror) {
            ckkserror("ckkscurrent", ckks, "Error fetching UUID for new item: %@", cferror);
            self.error = (NSError*) CFBridgingRelease(cferror);
            return CKKSDatabaseTransactionRollback;
        }

        // If the old item is nil, that's an indicator that the old item isn't expected to exist in the keychain anymore
        NSData* oldCurrentItemHash = nil;
        if(self.oldItemPersistentRef) {
            self.oldItem = [self _onqueueFindSecDbItem:self.oldItemPersistentRef accessGroup:self.accessGroup error:&error];
            if(!self.oldItem || error) {
                ckksnotice("ckkscurrent", ckks, "Couldn't fetch old item, quitting: %@", error);
                self.error = error;
                return CKKSDatabaseTransactionRollback;
            }

            oldCurrentItemHash = (NSData*) CFBridgingRelease(CFRetainSafe(SecDbItemGetSHA1(self.oldItem, &cferror)));
            if(!oldCurrentItemHash || cferror ||
               ![oldCurrentItemHash isEqual:self.oldItemSHA1]) {
                ckksnotice("ckkscurrent", ckks, "Hash mismatch for old item: %@ vs %@", oldCurrentItemHash, self.oldItemSHA1);
                self.error = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSItemChanged
                                          description:@"Old item has changed; hashes mismatch. Refetch and try again."
                                           underlying:(NSError*)CFBridgingRelease(cferror)];
                return CKKSDatabaseTransactionRollback;
            }

            oldCurrentItemUUID = (NSString*) CFBridgingRelease(CFRetainSafe(SecDbItemGetValue(self.oldItem, &v10itemuuid, &cferror)));
            if(!oldCurrentItemUUID || cferror) {
                ckkserror("ckkscurrent", ckks, "Error fetching UUID for old item: %@", cferror);
                self.error = (NSError*) CFBridgingRelease(cferror);
                return CKKSDatabaseTransactionRollback;
            }
        }

        //////////////////////////////
        // At this point, we've completed all the checks we need for the SecDbItems. Try to launch this boat!
        ckksnotice("ckkscurrent", ckks, "Setting current pointer for %@ to %@ (from %@)", self.currentPointerIdentifier, newItemUUID, oldCurrentItemUUID);

        // Ensure that there's no pending pointer update
        CKKSCurrentItemPointer* cipPending = [CKKSCurrentItemPointer tryFromDatabase:self.currentPointerIdentifier state:SecCKKSProcessedStateRemote zoneID:ckks.zoneID error:&error];
        if(cipPending) {
            self.error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSRemoteItemChangePending
                                      description:[NSString stringWithFormat:@"Update to current item pointer is pending."]];
            ckkserror("ckkscurrent", ckks, "Attempt to set a new current item pointer when one exists: %@", self.error);
            return CKKSDatabaseTransactionRollback;
        }

        CKKSCurrentItemPointer* cip = [CKKSCurrentItemPointer tryFromDatabase:self.currentPointerIdentifier state:SecCKKSProcessedStateLocal zoneID:ckks.zoneID error:&error];

        if(cip) {
            // Ensure that the itempointer matches the old item (and the old item exists)
            //
            // We might be in the dangling-pointer case, where the 'fetch' API has returned the client a nil value because we
            // have a CIP, but it points to a deleted keychain item.
            // In that case, we shouldn't error out.
            //
            if(oldCurrentItemHash && ![cip.currentItemUUID isEqualToString: oldCurrentItemUUID]) {

                ckksnotice("ckkscurrent", ckks, "current item pointer(%@) doesn't match user-supplied UUID (%@); rejecting change of current", cip, oldCurrentItemUUID);
                self.error = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSItemChanged
                                          description:[NSString stringWithFormat:@"Current pointer(%@) does not match user-supplied %@, aborting", cip, oldCurrentItemUUID]];
                return CKKSDatabaseTransactionRollback;
            }
            // Cool. Since you know what you're updating, you're allowed to update!
            cip.currentItemUUID = newItemUUID;

        } else if(oldCurrentItemUUID) {
            // Error case: the client thinks there's a current pointer, but we don't have one
            ckksnotice("ckkscurrent", ckks, "Requested to update a current item pointer but one doesn't exist at %@; rejecting change of current", self.currentPointerIdentifier);
            self.error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSItemChanged
                                      description:[NSString stringWithFormat:@"Current pointer(%@) does not match given value of '%@', aborting", cip, oldCurrentItemUUID]];
            return CKKSDatabaseTransactionRollback;
        } else {
            // No current item pointer? How exciting! Let's make you a nice new one.
            cip = [[CKKSCurrentItemPointer alloc] initForIdentifier:self.currentPointerIdentifier
                                                    currentItemUUID:newItemUUID
                                                              state:SecCKKSProcessedStateLocal
                                                             zoneID:ckks.zoneID
                                                    encodedCKRecord:nil];
            ckksnotice("ckkscurrent", ckks, "Creating a new current item pointer: %@", cip);
        }

        // Check if either item is currently in any sync queue, and fail if so
        NSArray* oqes = [CKKSOutgoingQueueEntry allUUIDs:ckks.zoneID error:&error];
        NSArray* iqes = [CKKSIncomingQueueEntry allUUIDs:ckks.zoneID error:&error];
        if([oqes containsObject:newItemUUID] || [iqes containsObject:newItemUUID]) {
            error = [NSError errorWithDomain:CKKSErrorDomain
                                        code:CKKSLocalItemChangePending
                                 description:[NSString stringWithFormat:@"New item(%@) is being synced; can't set current pointer.", newItemUUID]];
        }
        if([oqes containsObject:oldCurrentItemUUID] || [iqes containsObject:oldCurrentItemUUID]) {
            error = [NSError errorWithDomain:CKKSErrorDomain
                                        code:CKKSLocalItemChangePending
                                 description:[NSString stringWithFormat:@"Old item(%@) is being synced; can't set current pointer.", oldCurrentItemUUID]];
        }

        if(error) {
            ckkserror("ckkscurrent", ckks, "Error attempting to update current item pointer %@: %@", self.currentPointerIdentifier, error);
            self.error = error;
            return CKKSDatabaseTransactionRollback;
        }

        // Make sure the item is synced, though!
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry fromDatabase:cip.currentItemUUID zoneID:ckks.zoneID error:&error];
        if(!ckme || error) {
            ckkserror("ckkscurrent", ckks, "Error attempting to set a current item pointer to an item that isn't synced: %@ %@", cip, ckme);
            error = [NSError errorWithDomain:CKKSErrorDomain
                                        code:errSecItemNotFound
                                 description:[NSString stringWithFormat:@"No synced item matching (%@); can't set current pointer.", cip.currentItemUUID]
                                  underlying:error];

            self.error = error;
            return CKKSDatabaseTransactionRollback;
        }

        ckksnotice("ckkscurrent", ckks, "Saving new current item pointer %@", cip);

        NSMutableDictionary<CKRecordID*, CKRecord*>* recordsToSave = [[NSMutableDictionary alloc] init];
        CKRecord* record = [cip CKRecordWithZoneID:ckks.zoneID];
        recordsToSave[record.recordID] = record;

        // Start a CKModifyRecordsOperation to save this new/updated record.
        NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
        modifyComplete.name = @"updateCurrentItemPointer-modifyRecordsComplete";
        [self dependOnBeforeGroupFinished: modifyComplete];

        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave.allValues recordIDsToDelete:nil];
        self.modifyRecordsOperation.atomic = TRUE;
        // We're likely rolling a PCS identity, or creating a new one. User cares.
        self.modifyRecordsOperation.configuration.automaticallyRetryNetworkFailures = NO;
        self.modifyRecordsOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
        self.modifyRecordsOperation.configuration.isCloudKitSupportOperation = YES;

        self.modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
        self.modifyRecordsOperation.group = self.ckoperationGroup;

        self.modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            STRONGIFY(self);
            CKKSKeychainView* blockCKKS = self.ckks;

            if(!error) {
                ckksnotice("ckkscurrent", blockCKKS, "Current pointer upload successful for %@: %@", record.recordID.recordName, record);
            } else {
                ckkserror("ckkscurrent", blockCKKS, "error on row: %@ %@", error, record);
            }
        };

        self.modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            STRONGIFY(self);
            CKKSKeychainView* strongCKKS = self.ckks;
            if(!self || !strongCKKS) {
                ckkserror("ckkscurrent", strongCKKS, "received callback for released object");
                self.error = [NSError errorWithDomain:CKKSErrorDomain
                                                       code:errSecInternalError
                                                description:@"no CKKS object"];
                [strongCKKS scheduleOperation: modifyComplete];
                return;
            }

            if(ckerror) {
                ckkserror("ckkscurrent", strongCKKS, "CloudKit returned an error: %@", ckerror);
                self.error = ckerror;

                [strongCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    return [strongCKKS _onqueueCKWriteFailed:ckerror attemptedRecordsChanged:recordsToSave] ? CKKSDatabaseTransactionCommit : CKKSDatabaseTransactionRollback;
                }];

                [strongCKKS scheduleOperation: modifyComplete];
                return;
            }

            __block NSError* error = nil;

            [strongCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                for(CKRecord* record in savedRecords) {
                    // Save the item records
                    if([record.recordType isEqualToString: SecCKRecordCurrentItemType]) {
                        if([cip matchesCKRecord: record]) {
                            cip.storedCKRecord = record;
                            [cip saveToDatabase:&error];
                            if(error) {
                                ckkserror("ckkscurrent", strongCKKS, "Couldn't save new current pointer to database: %@", error);
                            }
                        } else {
                            ckkserror("ckkscurrent", strongCKKS, "CloudKit record does not match saved record, ignoring: %@ %@", record, cip);
                        }
                    }
                    else if ([CKKSManifest shouldSyncManifests] && [record.recordType isEqualToString:SecCKRecordManifestType]) {
                        CKKSManifest* manifest = [[CKKSManifest alloc] initWithCKRecord:record];
                        [manifest saveToDatabase:&error];
                        if (error) {
                            ckkserror("ckkscurrent", strongCKKS, "Couldn't save %@ to manifest: %@", record.recordID.recordName, error);
                            self.error = error;
                        }
                    }

                    // Schedule a 'view changed' notification
                    [strongCKKS.viewState.notifyViewChangedScheduler trigger];
                }
                return CKKSDatabaseTransactionCommit;
            }];

            self.error = error;
            [strongCKKS scheduleOperation: modifyComplete];
        };

        [self dependOnBeforeGroupFinished: self.modifyRecordsOperation];
        [ckks.operationDependencies.ckdatabase addOperation:self.modifyRecordsOperation];

        return CKKSDatabaseTransactionCommit;
    }];
}

- (SecDbItemRef _Nullable)_onqueueFindSecDbItem:(NSData*)persistentRef accessGroup:(NSString*)accessGroup error:(NSError**)error {
    CKKSKeychainView* ckks = self.ckks;
    dispatch_assert_queue(ckks.queue);
    __block SecDbItemRef blockItem = NULL;
    CFErrorRef cferror = NULL;
    __block NSError* localerror = NULL;

    bool ok = kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
        // Find the items from their persistent refs.
        CFErrorRef blockcfError = NULL;
        Query *q = query_create_with_limit( (__bridge CFDictionaryRef) @{
                                                                         (__bridge NSString *)kSecValuePersistentRef : persistentRef,
                                                                         (__bridge NSString *)kSecAttrAccessGroup : accessGroup,
                                                                         },
                                           NULL,
                                           1,
                                           NULL, 
                                           &blockcfError);
        if(blockcfError || !q) {
            ckkserror("ckkscurrent", ckks, "couldn't create query for item persistentRef: %@", blockcfError);
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:errSecParam
                                      description:@"couldn't create query for new item pref"
                                       underlying:(NSError*)CFBridgingRelease(blockcfError)];
            return false;
        }

        if(!SecDbItemQuery(q, NULL, dbt, &blockcfError, ^(SecDbItemRef item, bool *stop) {
            blockItem = CFRetainSafe(item);
        })) {
            query_destroy(q, NULL);
            ckkserror("ckkscurrent", ckks, "couldn't run query for item pref: %@", blockcfError);
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:errSecParam
                                        description:@"couldn't run query for new item pref"
                                       underlying:(NSError*)CFBridgingRelease(blockcfError)];
            return false;
        }

        if(!query_destroy(q, &blockcfError)) {
            ckkserror("ckkscurrent", ckks, "couldn't destroy query for item pref: %@", blockcfError);
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:errSecParam
                                      description:@"couldn't destroy query for item pref"
                                       underlying:(NSError*)CFBridgingRelease(blockcfError)];
            return false;
        }
        return true;
    });

    if(!ok || localerror) {
        if(localerror) {
            ckkserror("ckkscurrent", ckks, "Query failed: %@", localerror);
            if(error) {
                *error = localerror;
            }
        } else {
            ckkserror("ckkscurrent", ckks, "Query failed, cferror is %@", cferror);
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:errSecParam
                                      description:@"couldn't run query"
                                       underlying:(NSError*)CFBridgingRelease(cferror)];
            if(*error) {
                *error = localerror;
            }
        }

        CFReleaseSafe(cferror);
        return false;
    }

    CFReleaseSafe(cferror);
    return blockItem;
}

@end

#endif // OCTAGON
