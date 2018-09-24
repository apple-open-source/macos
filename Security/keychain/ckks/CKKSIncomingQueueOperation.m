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

#import "CKKSKeychainView.h"
#import "CKKSIncomingQueueOperation.h"
#import "CKKSIncomingQueueEntry.h"
#import "CKKSItemEncrypter.h"
#import "CKKSOutgoingQueueEntry.h"
#import "CKKSKey.h"
#import "CKKSManifest.h"
#import "CKKSAnalytics.h"
#import "CKKSPowerCollection.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"

#include <securityd/SecItemServer.h>
#include <securityd/SecItemDb.h>
#include <Security/SecItemPriv.h>

#include <utilities/SecADWrapper.h>

#if OCTAGON

@interface CKKSIncomingQueueOperation ()
@property bool newOutgoingEntries;
@property bool pendingClassAEntries;
@property bool missingKey;
@end

@implementation CKKSIncomingQueueOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks errorOnClassAFailure:(bool)errorOnClassAFailure {
    if(self = [super init]) {
        _ckks = ckks;

        // Can't process unless we have a reasonable key hierarchy.
        if(ckks.keyStateReadyDependency) {
            [self addDependency: ckks.keyStateReadyDependency];
        }

        [self addNullableDependency: ckks.holdIncomingQueueOperation];

        _errorOnClassAFailure = errorOnClassAFailure;
        _pendingClassAEntries = false;

        [self linearDependencies:ckks.incomingQueueOperations];

        if ([CKKSManifest shouldSyncManifests]) {
            __weak __typeof(self) weakSelf = self;
            __weak CKKSKeychainView* weakCKKS = ckks;
            CKKSResultOperation* updateManifestOperation = [CKKSResultOperation operationWithBlock:^{
                __strong __typeof(self) strongSelf = weakSelf;
                __strong CKKSKeychainView* strongCKKS = weakCKKS;
                __block NSError* error = nil;
                if (!strongCKKS || !strongSelf) {
                    ckkserror("ckksincoming", strongCKKS, "update manifest operation fired for released object");
                    return;
                }

                [strongCKKS dispatchSyncWithAccountKeys:^bool{
                    strongCKKS.latestManifest = [CKKSManifest latestTrustedManifestForZone:strongCKKS.zoneName error:&error];
                    if (error) {
                        strongSelf.error = error;
                        ckkserror("ckksincoming", strongCKKS, "failed to get latest manifest: %@", error);
                        return false;
                    }
                    else {
                        return true;
                    }
                }];
            }];
            updateManifestOperation.name = @"update-manifest-operation";

            [ckks scheduleOperation:updateManifestOperation];
            [self addSuccessDependency:updateManifestOperation];
        }
    }
    return self;
}

- (bool)processNewCurrentItemPointers:(NSArray<CKKSCurrentItemPointer*>*)queueEntries withManifest:(CKKSManifest*)manifest egoManifest:(CKKSEgoManifest*)egoManifest
{
    CKKSKeychainView* ckks = self.ckks;

    NSError* error = nil;
    for(CKKSCurrentItemPointer* p in queueEntries) {
        @autoreleasepool {
            if ([CKKSManifest shouldSyncManifests]) {
                if (![manifest validateCurrentItem:p withError:&error]) {
                    ckkserror("ckksincoming", ckks, "Unable to validate current item pointer (%@) against manifest (%@)", p, manifest);
                    if ([CKKSManifest shouldEnforceManifests]) {
                        return false;
                    }
                }
            }

            p.state = SecCKKSProcessedStateLocal;

            [p saveToDatabase:&error];
            ckksnotice("ckkspointer", ckks, "Saving new current item pointer: %@", p);
            if(error) {
                ckkserror("ckksincoming", ckks, "Error saving new current item pointer: %@ %@", error, p);
            }

            // Schedule a view change notification
            [ckks.notifyViewChangedScheduler trigger];
        }
    }

    if(queueEntries.count > 0) {
        // Schedule a view change notification
        [ckks.notifyViewChangedScheduler trigger];
    }

    return (error == nil);
}

- (bool)processQueueEntries:(NSArray<CKKSIncomingQueueEntry*>*)queueEntries withManifest:(CKKSManifest*)manifest egoManifest:(CKKSEgoManifest*)egoManifest
{
    CKKSKeychainView* ckks = self.ckks;

    NSMutableArray* newOrChangedRecords = [[NSMutableArray alloc] init];
    NSMutableArray* deletedRecordIDs = [[NSMutableArray alloc] init];

    for(id entry in queueEntries) {
        @autoreleasepool {
            if(self.cancelled) {
                ckksnotice("ckksincoming", ckks, "CKKSIncomingQueueOperation cancelled, quitting");
                return false;
            }

            NSError* error = nil;

            CKKSIncomingQueueEntry* iqe = (CKKSIncomingQueueEntry*) entry;
            ckksnotice("ckksincoming", ckks, "ready to process an incoming queue entry: %@ %@ %@", iqe, iqe.uuid, iqe.action);

            // Note that we currently unencrypt the item before deleting it, instead of just deleting it
            // This finds the class, which is necessary for the deletion process. We could just try to delete
            // across all classes, though...
            NSMutableDictionary* attributes = [[CKKSItemEncrypter decryptItemToDictionary: iqe.item error:&error] mutableCopy];
            if(!attributes || error) {
                if([ckks.lockStateTracker isLockedError:error]) {
                    NSError* localerror = nil;
                    ckkserror("ckksincoming", ckks, "Keychain is locked; can't decrypt IQE %@", iqe);
                    CKKSKey* key = [CKKSKey tryFromDatabase:iqe.item.parentKeyUUID zoneID:ckks.zoneID error:&localerror];
                    if(localerror || ([key.keyclass isEqualToString:SecCKKSKeyClassA] && self.errorOnClassAFailure)) {
                        self.error = error;
                    }

                    // If this isn't an error, make sure it gets processed later.
                    if([key.keyclass isEqualToString:SecCKKSKeyClassA] && !self.errorOnClassAFailure) {
                        self.pendingClassAEntries = true;
                    }

                } else if ([error.domain isEqualToString:@"securityd"] && error.code == errSecItemNotFound) {
                    ckkserror("ckksincoming", ckks, "Coudn't find key in keychain; will attempt to poke key hierarchy: %@", error)
                    self.missingKey = true;

                } else {
                    ckkserror("ckksincoming", ckks, "Couldn't decrypt IQE %@ for some reason: %@", iqe, error);
                    self.error = error;
                }
                self.errorItemsProcessed += 1;
                continue;
            }

            // Add the UUID (which isn't stored encrypted)
            [attributes setValue: iqe.item.uuid forKey: (__bridge NSString*) kSecAttrUUID];

            // Add the PCS plaintext fields, if they exist
            if(iqe.item.plaintextPCSServiceIdentifier) {
                [attributes setValue: iqe.item.plaintextPCSServiceIdentifier forKey: (__bridge NSString*) kSecAttrPCSPlaintextServiceIdentifier];
            }
            if(iqe.item.plaintextPCSPublicKey) {
                [attributes setValue: iqe.item.plaintextPCSPublicKey forKey: (__bridge NSString*) kSecAttrPCSPlaintextPublicKey];
            }
            if(iqe.item.plaintextPCSPublicIdentity) {
                [attributes setValue: iqe.item.plaintextPCSPublicIdentity forKey: (__bridge NSString*) kSecAttrPCSPlaintextPublicIdentity];
            }

            // This item is also synchronizable (by definition)
            [attributes setValue: @(YES) forKey: (__bridge NSString*) kSecAttrSynchronizable];

            NSString* classStr = [attributes objectForKey: (__bridge NSString*) kSecClass];
            if(![classStr isKindOfClass: [NSString class]]) {
                self.error = [NSError errorWithDomain:@"securityd"
                                                 code:errSecInternalError
                                             userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"Item did not have a reasonable class: %@", classStr]}];
                ckkserror("ckksincoming", ckks, "Synced item seems wrong: %@", self.error);
                self.errorItemsProcessed += 1;
                continue;
            }

            const SecDbClass * classP = !classStr ? NULL : kc_class_with_name((__bridge CFStringRef) classStr);

            if(!classP) {
                ckkserror("ckksincoming", ckks, "unknown class in object: %@ %@", classStr, iqe);
                iqe.state = SecCKKSStateError;
                [iqe saveToDatabase:&error];
                if(error) {
                    ckkserror("ckksincoming", ckks, "Couldn't save errored IQE to database: %@", error);
                    self.error = error;
                }
                self.errorItemsProcessed += 1;
                continue;
            }

            if([iqe.action isEqualToString: SecCKKSActionAdd] || [iqe.action isEqualToString: SecCKKSActionModify]) {
                BOOL requireManifestValidation = [CKKSManifest shouldEnforceManifests];
                BOOL manifestValidatesItem = [manifest validateItem:iqe.item withError:&error];

                if (!requireManifestValidation || manifestValidatesItem) {
                    [self _onqueueHandleIQEChange: iqe attributes:attributes class:classP];
                    [newOrChangedRecords addObject:[iqe.item CKRecordWithZoneID:ckks.zoneID]];
                }
                else {
                    ckkserror("ckksincoming", ckks, "could not validate incoming item against manifest with error: %@", error);
                    if (![self _onqueueUpdateIQE:iqe withState:SecCKKSStateUnauthenticated error:&error]) {
                        ckkserror("ckksincoming", ckks, "failed to save incoming item back to database in unauthenticated state with error: %@", error);
                        return false;
                    }
                    self.errorItemsProcessed += 1;
                    continue;
                }
            } else if ([iqe.action isEqualToString: SecCKKSActionDelete]) {
                BOOL requireManifestValidation = [CKKSManifest shouldEnforceManifests];
                BOOL manifestValidatesDelete = ![manifest itemUUIDExistsInManifest:iqe.uuid];
            
                if (!requireManifestValidation || manifestValidatesDelete) {
                    // if the item does not exist in the latest manifest, we're good to delete it
                    [self _onqueueHandleIQEDelete: iqe class:classP];
                    [deletedRecordIDs addObject:[[CKRecordID alloc] initWithRecordName:iqe.uuid zoneID:ckks.zoneID]];
                }
                else {
                    // if the item DOES exist in the manifest, we can't trust the deletion
                    ckkserror("ckksincoming", ckks, "could not validate incoming item deletion against manifest");
                    if (![self _onqueueUpdateIQE:iqe withState:SecCKKSStateUnauthenticated error:&error]) {
                        ckkserror("ckksincoming", ckks, "failed to save incoming item deletion back to database in unauthenticated state with error: %@", error);

                        self.errorItemsProcessed += 1;
                        return false;
                    }
                }
            }
        }
    }

    if(newOrChangedRecords.count > 0 || deletedRecordIDs > 0) {
        // Schedule a view change notification
        [ckks.notifyViewChangedScheduler trigger];
    }

    if(self.missingKey) {
        [ckks.pokeKeyStateMachineScheduler trigger];
    }

    if ([CKKSManifest shouldSyncManifests]) {
        [egoManifest updateWithNewOrChangedRecords:newOrChangedRecords deletedRecordIDs:deletedRecordIDs];
    }
    return true;
}

- (bool)_onqueueUpdateIQE:(CKKSIncomingQueueEntry*)iqe withState:(NSString*)newState error:(NSError**)error
{
    if (![iqe.state isEqualToString:newState]) {
        NSMutableDictionary* oldWhereClause = iqe.whereClauseToFindSelf.mutableCopy;
        oldWhereClause[@"state"] = iqe.state;
        iqe.state = newState;
        if ([iqe saveToDatabase:error]) {
            if (![CKKSSQLDatabaseObject deleteFromTable:[iqe.class sqlTable] where:oldWhereClause connection:NULL error:error]) {
                return false;
            }
        }
        else {
            return false;
        }
    }

    return true;
}

- (void) main {
    // Synchronous, on some thread. Get back on the CKKS queue for thread-safety.
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksincoming", ckks, "no CKKS object");
        return;
    }

    __weak __typeof(self) weakSelf = self;
    self.completionBlock = ^(void) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            ckkserror("ckksincoming", ckks, "received callback for released object");
            return;
        }

        CKKSAnalytics* logger = [CKKSAnalytics logger];

        if (!strongSelf.error) {
            [logger logSuccessForEvent:CKKSEventProcessIncomingQueueClassC inView:ckks];
            if (!strongSelf.pendingClassAEntries) {
                [logger logSuccessForEvent:CKKSEventProcessIncomingQueueClassA inView:ckks];
            }
        } else {
            [logger logRecoverableError:strongSelf.error
                               forEvent:strongSelf.errorOnClassAFailure ? CKKSEventProcessIncomingQueueClassA : CKKSEventProcessIncomingQueueClassC
                                 inView:ckks
                         withAttributes:NULL];
        }
    };

    [ckks dispatchSync: ^bool{
        if(self.cancelled) {
            ckksnotice("ckksincoming", ckks, "CKKSIncomingQueueOperation cancelled, quitting");
            return false;
        }
        ckks.lastIncomingQueueOperation = self;

        ckksnotice("ckksincoming", ckks, "Processing incoming queue");

        if ([CKKSManifest shouldSyncManifests]) {
            if (!ckks.latestManifest) {
                // Until we can make manifests in our unit tests, we can't abort here
                ckkserror("ckksincoming", ckks, "no manifest in ckks");
            }
            if (!ckks.egoManifest) {
                ckkserror("ckksincoming", ckks, "no ego manifest in ckks");
            }
        }

        bool ok = true; // Should commit transaction?
        __block NSError* error = nil;

        if ([CKKSManifest shouldSyncManifests]) {
            NSInteger unauthenticatedItemCount = [CKKSIncomingQueueEntry countByState:SecCKKSStateUnauthenticated zone:ckks.zoneID error:&error];
            if (error || unauthenticatedItemCount < 0) {
                ckkserror("ckksincoming", ckks, "Error fetching incoming queue state counts: %@", error);
                self.error = error;
                return false;
            }

            // take any existing unauthenticated entries and put them back in the new state
            NSArray<CKKSIncomingQueueEntry*>* unauthenticatedEntries = nil;
            NSString* lastMaxUUID = nil;
            NSInteger numEntriesProcessed = 0;
            while (numEntriesProcessed < unauthenticatedItemCount &&  (unauthenticatedEntries == nil || unauthenticatedEntries.count == SecCKKSIncomingQueueItemsAtOnce)) {
                if(self.cancelled) {
                    ckksnotice("ckksincoming", ckks, "CKKSIncomingQueueOperation cancelled, quitting");
                    return false;
                }

                unauthenticatedEntries = [CKKSIncomingQueueEntry fetch:SecCKKSIncomingQueueItemsAtOnce
                                                        startingAtUUID:lastMaxUUID
                                                                 state:SecCKKSStateUnauthenticated
                                                                zoneID:ckks.zoneID
                                                                 error:&error];
                if (error) {
                    ckkserror("ckksincoming", ckks, "Error fetching unauthenticated queue records: %@", error);
                    self.error = error;
                    return false;
                }

                if (unauthenticatedEntries.count == 0) {
                    ckksinfo("ckksincoming", ckks, "No unauthenticated entries in incoming queue to process");
                    break;
                }

                for (CKKSIncomingQueueEntry* unauthenticatedEntry in unauthenticatedEntries) {
                    if (![self _onqueueUpdateIQE:unauthenticatedEntry withState:SecCKKSStateNew error:&error]) {
                        ckkserror("ckksincoming", ckks, "Error saving unauthenticated entry back to new state: %@", error);
                        self.error = error;
                        return false;
                    }

                    lastMaxUUID = ([lastMaxUUID compare:unauthenticatedEntry.uuid] == NSOrderedDescending) ? lastMaxUUID : unauthenticatedEntry.uuid;
                }
            }
        }

        // Iterate through all incoming queue entries a chunk at a time (for peak memory concerns)
        NSArray<CKKSIncomingQueueEntry*> * queueEntries = nil;
        NSString* lastMaxUUID = nil;
        while(queueEntries == nil || queueEntries.count == SecCKKSIncomingQueueItemsAtOnce) {
            if(self.cancelled) {
                ckksnotice("ckksincoming", ckks, "CKKSIncomingQueueOperation cancelled, quitting");
                return false;
            }

            queueEntries = [CKKSIncomingQueueEntry fetch: SecCKKSIncomingQueueItemsAtOnce
                                          startingAtUUID:lastMaxUUID
                                                   state:SecCKKSStateNew
                                                  zoneID:ckks.zoneID
                                                   error: &error];

            if(error != nil) {
                ckkserror("ckksincoming", ckks, "Error fetching incoming queue records: %@", error);
                self.error = error;
                return false;
            }

            if([queueEntries count] == 0) {
                // Nothing to do! exit.
                ckksnotice("ckksincoming", ckks, "Nothing in incoming queue to process");
                break;
            }

            [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventOutgoingQueue zone:ckks.zoneName count:[queueEntries count]];

            if (![self processQueueEntries:queueEntries withManifest:ckks.latestManifest egoManifest:ckks.egoManifest]) {
                ckksnotice("ckksincoming", ckks, "processQueueEntries didn't complete successfully");
                return false;
            }

            // Find the highest UUID for the next fetch.
            for(CKKSIncomingQueueEntry* iqe in queueEntries) {
                lastMaxUUID = ([lastMaxUUID compare:iqe.uuid] == NSOrderedDescending) ? lastMaxUUID : iqe.uuid;
            };
        }

        // Process other queues: CKKSCurrentItemPointers
        ckksnotice("ckksincoming", ckks, "Processed %lu items in incoming queue (%lu errors)", (unsigned long)self.successfulItemsProcessed, (unsigned long)self.errorItemsProcessed);

        NSArray<CKKSCurrentItemPointer*>* newCIPs = [CKKSCurrentItemPointer remoteItemPointers:ckks.zoneID error:&error];
        if(error || !newCIPs) {
            ckkserror("ckksincoming", ckks, "Could not load remote item pointers: %@", error);
        } else {
            if (![self processNewCurrentItemPointers:newCIPs withManifest:ckks.latestManifest egoManifest:ckks.egoManifest]) {
                return false;
            }
            ckksnotice("ckksincoming", ckks, "Processed %lu items in CIP queue", (unsigned long)newCIPs.count);
        }

        if(self.newOutgoingEntries) {
            // No operation group
            [ckks processOutgoingQueue:nil];
        }

        if(self.pendingClassAEntries) {
            [self.ckks processIncomingQueueAfterNextUnlock];
        }

        return ok;
    }];
}

- (void)_onqueueHandleIQEChange: (CKKSIncomingQueueEntry*) iqe attributes:(NSDictionary*)attributes class:(const SecDbClass *)classP {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksincoming", ckks, "no CKKS object");
        return;
    }

    dispatch_assert_queue(ckks.queue);

    bool ok = false;
    __block CFErrorRef cferror = NULL;
    __block NSError* error = NULL;

    SecDbItemRef item = SecDbItemCreateWithAttributes(NULL, classP, (__bridge CFDictionaryRef) attributes, KEYBAG_DEVICE, &cferror);

    __block NSDate* moddate = (__bridge NSDate*) CFDictionaryGetValue(item->attributes, kSecAttrModificationDate);

    ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt){
        bool replaceok = SecDbItemInsertOrReplace(item, dbt, &cferror, ^(SecDbItemRef olditem, SecDbItemRef *replace) {
            // If the UUIDs do not match, then select the item with the 'lower' UUID, and tell CKKS to
            //   delete the item with the 'higher' UUID.
            // Otherwise, the cloud wins.

            SecADAddValueForScalarKey((__bridge CFStringRef) SecCKKSAggdPrimaryKeyConflict,1);

            // Note that SecDbItemInsertOrReplace CFReleases any replace pointer it's given, so, be careful

            if(!CFDictionaryContainsKey(olditem->attributes, kSecAttrUUID)) {
                // No UUID -> no good.
                ckksnotice("ckksincoming", ckks, "Replacing item (it doesn't have a UUID) for %@", iqe.uuid);
                if(replace) {
                    *replace = CFRetainSafe(item);
                }
                return;
            }

            CFStringRef itemUUID    = CFDictionaryGetValue(item->attributes,    kSecAttrUUID);
            CFStringRef olditemUUID = CFDictionaryGetValue(olditem->attributes, kSecAttrUUID);

            CFComparisonResult compare = CFStringCompare(itemUUID, olditemUUID, 0);
            CKKSOutgoingQueueEntry* oqe = nil;
            switch(compare) {
                case kCFCompareLessThan:
                    // item wins; delete olditem
                    ckksnotice("ckksincoming", ckks, "Primary key conflict; replacing %@ with CK item %@", olditem, item);
                    if(replace) {
                        *replace = CFRetainSafe(item);
                        moddate = (__bridge NSDate*) CFDictionaryGetValue(item->attributes, kSecAttrModificationDate);
                    }

                    oqe = [CKKSOutgoingQueueEntry withItem:olditem action:SecCKKSActionDelete ckks:ckks error:&error];
                    [oqe saveToDatabase: &error];
                    self.newOutgoingEntries = true;
                    break;
                case kCFCompareGreaterThan:
                    // olditem wins; don't change olditem; delete item
                    ckksnotice("ckksincoming", ckks, "Primary key conflict; dropping CK item %@", item);

                    oqe = [CKKSOutgoingQueueEntry withItem:item action:SecCKKSActionDelete ckks:ckks error:&error];
                    [oqe saveToDatabase: &error];
                    self.newOutgoingEntries = true;
                    moddate = nil;
                    break;

                case kCFCompareEqualTo:
                    // remote item wins; this is the normal update case
                    ckksnotice("ckksincoming", ckks, "Primary key conflict; replacing %@ with CK item %@", olditem, item);
                    if(replace) {
                        *replace = CFRetainSafe(item);
                        moddate = (__bridge NSDate*) CFDictionaryGetValue(item->attributes, kSecAttrModificationDate);
                    }
                    break;
            }
        });

        // SecDbItemInsertOrReplace returns an error even when it succeeds.
        if(!replaceok && SecErrorIsSqliteDuplicateItemError(cferror)) {
            CFReleaseNull(cferror);
            replaceok = true;
        }
        return replaceok;
    });

    CFReleaseNull(item);

    if(cferror) {
        ckkserror("ckksincoming", ckks, "couldn't process item from IncomingQueue: %@", cferror);
        SecTranslateError(&error, cferror);
        self.error = error;

        iqe.state = SecCKKSStateError;
        [iqe saveToDatabase:&error];
        if(error) {
            ckkserror("ckksincoming", ckks, "Couldn't save errored IQE to database: %@", error);
            self.error = error;
        }
        return;
    }

    if(error) {
        ckkserror("ckksincoming", ckks, "Couldn't handle IQE, but why?: %@", error);
        self.error = error;
        return;
    }

    if(ok) {
        ckksinfo("ckksincoming", ckks, "Correctly processed an IQE; deleting");
        [iqe deleteFromDatabase: &error];

        if(error) {
            ckkserror("ckksincoming", ckks, "couldn't delete CKKSIncomingQueueEntry: %@", error);
            self.error = error;
            self.errorItemsProcessed += 1;
        } else {
            self.successfulItemsProcessed += 1;
        }

        if(moddate) {
            // Log the number of seconds it took to propagate this change
            uint64_t secondsDelay = (uint64_t) ([[NSDate date] timeIntervalSinceDate:moddate]);
            SecADClientPushValueForDistributionKey((__bridge CFStringRef) SecCKKSAggdPropagationDelay, secondsDelay);
        }

    } else {
        ckksnotice("ckksincoming", ckks, "IQE not correctly processed, but why? %@ %@", error, cferror);
        self.error = error;

        iqe.state = SecCKKSStateError;
        [iqe saveToDatabase:&error];
        if(error) {
            ckkserror("ckksincoming", ckks, "Couldn't save errored IQE to database: %@", error);
            self.error = error;
        }

        self.errorItemsProcessed += 1;
    }
}

- (void)_onqueueHandleIQEDelete: (CKKSIncomingQueueEntry*) iqe class:(const SecDbClass *)classP {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksincoming", ckks, "no CKKS object");
        return;
    }

    dispatch_assert_queue(ckks.queue);

    bool ok = false;
    __block CFErrorRef cferror = NULL;
    NSError* error = NULL;
    NSDictionary* queryAttributes = @{(__bridge NSString*) kSecClass: (__bridge NSString*) classP->name,
                                      (__bridge NSString*) kSecAttrUUID: iqe.uuid,
                                      (__bridge NSString*) kSecAttrSyncViewHint: ckks.zoneID.zoneName,
                                      (__bridge NSString*) kSecAttrSynchronizable: @(YES)};
    ckksnotice("ckksincoming", ckks, "trying to delete with query: %@", queryAttributes);
    Query *q = query_create_with_limit( (__bridge CFDictionaryRef) queryAttributes, NULL, kSecMatchUnlimited, &cferror);


    if(cferror) {
        ckkserror("ckksincoming", ckks, "couldn't create query: %@", cferror);
        SecTranslateError(&error, cferror);
        self.error = error;
        return;
    }

    ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt) {
        return s3dl_query_delete(dbt, q, NULL, &cferror);
    });

    if(cferror) {
        if(CFErrorGetCode(cferror) == errSecItemNotFound) {
            ckkserror("ckksincoming", ckks, "couldn't delete item (as it's already gone); this is okay: %@", cferror);
            ok = true;
            CFReleaseNull(cferror);
        } else {
            ckkserror("ckksincoming", ckks, "couldn't delete item: %@", cferror);
            SecTranslateError(&error, cferror);
            self.error = error;
            query_destroy(q, NULL);
            return;
        }
    }


    ok = query_notify_and_destroy(q, ok, &cferror);

    if(cferror) {
        ckkserror("ckksincoming", ckks, "couldn't delete query: %@", cferror);
        SecTranslateError(&error, cferror);
        self.error = error;
        return;
    }

    if(ok) {
        ckksnotice("ckksincoming", ckks, "Correctly processed an IQE; deleting");
        [iqe deleteFromDatabase: &error];

        if(error) {
            ckkserror("ckksincoming", ckks, "couldn't delete CKKSIncomingQueueEntry: %@", error);
            self.error = error;
            self.errorItemsProcessed += 1;
        } else {
            self.successfulItemsProcessed += 1;
        }
    } else {
        ckkserror("ckksincoming", ckks, "IQE not correctly processed, but why? %@ %@", error, cferror);
        self.error = error;
        self.errorItemsProcessed += 1;
    }
}

@end;

#endif
