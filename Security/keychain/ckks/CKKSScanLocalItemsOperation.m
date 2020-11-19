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

#if OCTAGON
#import <TrustedPeers/TPSyncingPolicy.h>
#import <TrustedPeers/TPPBPolicyKeyViewMapping.h>
#import <TrustedPeers/TPDictionaryMatchingRules.h>

#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSScanLocalItemsOperation.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"

#import "CKKSPowerCollection.h"

#include "keychain/securityd/SecItemSchema.h"
#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include <Security/SecItemPriv.h>
#include <utilities/SecInternalReleasePriv.h>
#import <IMCore/IMCore_Private.h>
#import <IMCore/IMCloudKitHooks.h>

@interface CKKSScanLocalItemsOperation ()
@property (assign) NSUInteger processedItems;

@property BOOL newCKKSEntries;
@end

@implementation CKKSScanLocalItemsOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}
- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                                ckks:(CKKSKeychainView*)ckks
                           intending:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                    ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if((self = [super init])) {
        _deps = dependencies;
        _ckks = ckks;
        _ckoperationGroup = ckoperationGroup;

        _nextState = errorState;
        _intendedState = intendedState;

        _recordsFound = 0;
        _recordsAdded = 0;
    }
    return self;
}

- (NSDictionary*)queryPredicatesForViewMapping {
    TPPBPolicyKeyViewMapping* viewRule = nil;

    // If there's more than one rule matching this view, then exit with an empty dictionary: the language doesn't support ORs.
    for(TPPBPolicyKeyViewMapping* mapping in [CKKSViewManager manager].policy.keyViewMapping) {
        if([mapping.view isEqualToString:self.deps.zoneID.zoneName]) {
            if(viewRule == nil) {
                viewRule = mapping;
            } else {
                // Too many rules for this view! Don't perform optimization.
                ckksnotice("ckksscan", self.deps.zoneID, "Too many policy rules for view %@", self.deps.zoneID.zoneName);
                return @{};
            }
        }
    }

    if(viewRule.hasMatchingRule &&
       viewRule.matchingRule.andsCount == 0 &&
       viewRule.matchingRule.orsCount == 0 &&
       !viewRule.matchingRule.hasNot &&
       !viewRule.matchingRule.hasExists &&
       viewRule.matchingRule.hasMatch) {
        if([((id)kSecAttrSyncViewHint) isEqualToString:viewRule.matchingRule.match.fieldName] &&
           [viewRule.matchingRule.match.regex isEqualToString:[NSString stringWithFormat:@"^%@$", self.deps.zoneID.zoneName]]) {
            return @{
                (id)kSecAttrSyncViewHint: self.deps.zoneID.zoneName,
            };
        } else if([((id)kSecAttrAccessGroup) isEqualToString:viewRule.matchingRule.match.fieldName] &&
                  [viewRule.matchingRule.match.regex isEqualToString:@"^com\\.apple\\.cfnetwork$"]) {
            // We can't match on any regex agrp match, because it might be some actually difficult regex. But, we know about this one!
            return @{
                (id)kSecAttrAccessGroup: @"com.apple.cfnetwork",
            };

        } else if([((id)kSecAttrAccessGroup) isEqualToString:viewRule.matchingRule.match.fieldName] &&
                  [viewRule.matchingRule.match.regex isEqualToString:@"^com\\.apple\\.safari\\.credit-cards$"]) {
            // We can't match on any regex agrp match, because it might be some actually difficult regex. But, we know about this one!
            return @{
                (id)kSecAttrAccessGroup: @"com.apple.safari.credit-cards",
            };

        } else {
            ckksnotice("ckksscan", self.deps.zoneID, "Policy view rule is not a match against viewhint: %@", viewRule);
        }
    } else {
        ckksnotice("ckksscan", self.deps.zoneID, "Policy view rule is complex: %@", viewRule);
    }

    return @{};
}

- (BOOL)executeQuery:(NSDictionary*)queryPredicates readWrite:(bool)readWrite error:(NSError**)error block:(void (^_Nonnull)(SecDbItemRef item))block
{
    __block CFErrorRef cferror = NULL;
    __block bool ok = false;

    Query *q = query_create_with_limit((__bridge CFDictionaryRef)queryPredicates, NULL, kSecMatchUnlimited, NULL, &cferror);

    if(cferror) {
        ckkserror("ckksscan", self.deps.zoneID, "couldn't create query: %@", cferror);
        SecTranslateError(error, cferror);
        return NO;
    }

    ok = kc_with_dbt(readWrite, &cferror, ^(SecDbConnectionRef dbt) {
        return SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
            block(item);
        });
    });

    if(readWrite) {
        ok = query_notify_and_destroy(q, ok, &cferror);
    } else {
        ok = query_destroy(q, &cferror);
    }

    if(cferror || !ok) {
        ckkserror("ckksscan", self.deps.zoneID, "couldn't execute query: %@", cferror);
        SecTranslateError(error, cferror);
        return NO;
    }

    return YES;
}

- (BOOL)onboardItemToCKKS:(SecDbItemRef)item error:(NSError**)error
{
    NSError* itemSaveError = nil;

    CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry withItem:item
                                                            action:SecCKKSActionAdd
                                                            zoneID:self.deps.zoneID
                                                             error:&itemSaveError];

    if(itemSaveError) {
        ckkserror("ckksscan", self.deps.zoneID, "Need to upload %@, but can't create outgoing entry: %@", item, itemSaveError);
        if(error) {
            *error = itemSaveError;
        }
        return NO;
    }

    ckksnotice("ckksscan", self.deps.zoneID, "Syncing new item: %@", oqe);

    [oqe saveToDatabase:&itemSaveError];
    if(itemSaveError) {
        ckkserror("ckksscan", self.deps.zoneID, "Need to upload %@, but can't save to database: %@", oqe, itemSaveError);
        self.error = itemSaveError;
        return NO;
    }

    self.newCKKSEntries = true;
    self.recordsAdded += 1;

    return YES;
}

- (void)onboardItemsWithUUIDs:(NSSet<NSString*>*)uuids itemClass:(NSString*)itemClass databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
{
    ckksnotice("ckksscan", self.deps.zoneID, "Found %d missing %@ items", (int)uuids.count, itemClass);
    // Use one transaction for each item to allow for SecItem API calls to interleave
    for(NSString* itemUUID in uuids) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult {
            NSDictionary* queryAttributes = @{
                (id)kSecClass: itemClass,
                (id)kSecReturnRef: @(YES),
                (id)kSecAttrSynchronizable: @(YES),
                (id)kSecAttrTombstone: @(NO),
                (id)kSecAttrUUID: itemUUID,
            };

            ckksnotice("ckksscan", self.deps.zoneID, "Onboarding %@ %@", itemClass, itemUUID);

            __block NSError* itemSaveError = nil;

            [self executeQuery:queryAttributes readWrite:false error:&itemSaveError block:^(SecDbItemRef itemToSave) {
                [self onboardItemToCKKS:itemToSave error:&itemSaveError];
            }];

            if(itemSaveError) {
                ckkserror("ckksscan", self.deps.zoneID, "Need to upload %@, but can't save to database: %@", itemUUID, itemSaveError);
                self.error = itemSaveError;
                return CKKSDatabaseTransactionRollback;
            }

            return CKKSDatabaseTransactionCommit;
        }];
    }
}

- (void)fixUUIDlessItemsWithPrimaryKeys:(NSMutableSet<NSDictionary*>*)primaryKeys databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
{
    ckksnotice("ckksscan", self.deps.zoneID, "Found %d items missing UUIDs", (int)primaryKeys.count);

    if([primaryKeys count] == 0) {
        return;
    }

    [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        __block NSError* itemError = nil;

        for(NSDictionary* primaryKey in primaryKeys) {
            ckksnotice("ckksscan", self.deps.zoneID, "Found item with no uuid: %@", primaryKey);

            __block CFErrorRef cferror = NULL;

            bool connectionSuccess = kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
                Query *q = query_create_with_limit((__bridge CFDictionaryRef)primaryKey, NULL, kSecMatchUnlimited, NULL, &cferror);

                if(!q || cferror) {
                    ckkserror("ckksscan", self.deps.zoneID, "couldn't create query: %@", cferror);
                    return false;
                }

                __block bool ok = true;

                ok &= SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef uuidlessItem, bool *stop) {
                    NSString* uuid = [[NSUUID UUID] UUIDString];
                    NSDictionary* updates = @{(id)kSecAttrUUID: uuid};

                    ckksnotice("ckksscan", self.deps.zoneID, "Assigning new UUID %@ for item %@", uuid, uuidlessItem);

                    SecDbItemRef new_item = SecDbItemCopyWithUpdates(uuidlessItem, (__bridge CFDictionaryRef)updates, &cferror);

                    if(!new_item) {
                        SecTranslateError(&itemError, cferror);
                        self.error = itemError;
                        ckksnotice("ckksscan", self.deps.zoneID, "Unable to copy item with new UUID: %@", cferror);
                        return;
                    }

                    bool updateSuccess = kc_transaction_type(dbt, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, ^{
                        return SecDbItemUpdate(uuidlessItem, new_item, dbt, kCFBooleanFalse, q->q_uuid_from_primary_key, &cferror);
                    });

                    if(updateSuccess) {
                        [self onboardItemToCKKS:new_item error:&itemError];
                    } else {
                        ckksnotice("ckksscan", self.deps.zoneID, "Unable to update item with new UUID: %@", cferror);
                    }

                    ok &= updateSuccess;
                });

                ok &= query_notify_and_destroy(q, ok, &cferror);

                return true;
            });

            if(!connectionSuccess) {
                ckkserror("ckksscan", self.deps.zoneID, "couldn't execute query: %@", cferror);
                SecTranslateError(&itemError, cferror);
                self.error = itemError;
                return CKKSDatabaseTransactionRollback;
            }
        }

        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)retriggerMissingMirrorEntires:(NSSet<NSString*>*)mirrorUUIDs
                                 ckks:(CKKSKeychainView*)ckks
                     databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
{
    if (mirrorUUIDs.count > 0) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;
            ckkserror("ckksscan", self.deps.zoneID, "BUG: keychain missing %lu items from mirror and/or queues: %@", (unsigned long)mirrorUUIDs.count, mirrorUUIDs);
            self.missingLocalItemsFound = mirrorUUIDs.count;

            [[CKKSAnalytics logger] logMetric:[NSNumber numberWithUnsignedInteger:mirrorUUIDs.count] withName:CKKSEventMissingLocalItemsFound];

            for (NSString* uuid in mirrorUUIDs) {
                CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid zoneID:self.deps.zoneID error:&error];

                if(!ckme || error) {
                    ckkserror("ckksscan", self.deps.zoneID, "BUG: error fetching previously-extant CKME (uuid: %@) from database: %@", uuid, error);
                    self.error = error;
                } else {
                    [ckks _onqueueCKRecordChanged:ckme.item.storedCKRecord resync:true];
                }
            }

            // And, if you're not in the tests, try to collect a sysdiagnose I guess?
            // <rdar://problem/36166435> Re-enable IMCore autosysdiagnose capture to securityd
            //if(SecIsInternalRelease() && !SecCKKSTestsEnabled()) {
            //    [[IMCloudKitHooks sharedInstance] tryToAutoCollectLogsWithErrorString:@"35810558" sendLogsTo:@"rowdy_bot@icloud.com"];
            //}
            return CKKSDatabaseTransactionCommit;
        }];
    } else {
        ckksnotice("ckksscan", self.deps.zoneID,"No missing local items found");
    }
}

- (void)main
{
    if(SecCKKSTestsEnabled() && SecCKKSTestSkipScan()) {
        ckksnotice("ckksscan", self.deps.zoneID, "Scan cancelled by test request");
        return;
    }

    // We need to not be jetsamed while running this
    os_transaction_t transaction = os_transaction_create([[NSString stringWithFormat:@"com.apple.securityd.ckks.scan.%@", self.deps.zoneID] UTF8String]);

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;
    CKKSKeychainView* ckks = self.ckks;

    [self.deps.launch addEvent:@"scan-local-items"];

    // A map of ItemClass -> Set of found UUIDs
    NSMutableDictionary<NSString*, NSMutableSet<NSString*>*>* itemUUIDsNotYetInCKKS = [NSMutableDictionary dictionary];

    // A list of primary keys of items that fit in this view, but have no UUIDs
    NSMutableSet<NSDictionary*>* primaryKeysWithNoUUIDs = [NSMutableSet set];

    // We want this set to be empty after scanning, or else the keychain (silently) dropped something on the floor
    NSMutableSet<NSString*>* mirrorUUIDs = [NSMutableSet set];

    [databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        // First, query for all synchronizable items
        __block NSError* error = nil;

        [mirrorUUIDs addObjectsFromArray:[CKKSMirrorEntry allUUIDs:self.deps.zoneID error:&error]];

        // Must query per-class, so:
        const SecDbSchema *newSchema = current_schema();
        for (const SecDbClass *const *class = newSchema->classes; *class != NULL; class++) {
            if(!((*class)->itemclass)) {
                // Don't try to scan non-item 'classes'
                continue;
            }

            NSString* itemClass = (__bridge NSString*)(*class)->name;

            NSMutableDictionary* queryAttributes = [
                                                    @{(__bridge NSString*)kSecClass: itemClass,
                                                      (__bridge NSString*)kSecReturnRef: @(YES),
                                                      (__bridge NSString*)kSecAttrSynchronizable: @(YES),
                                                      (__bridge NSString*)kSecAttrTombstone: @(NO),
                                                    } mutableCopy];

            NSDictionary* extraQueryPredicates = [self queryPredicatesForViewMapping];
            [queryAttributes addEntriesFromDictionary:extraQueryPredicates];

            ckksnotice("ckksscan", self.deps.zoneID, "Scanning all synchronizable %@ items(%@) for: %@", itemClass, self.name, queryAttributes);

            [self executeQuery:queryAttributes readWrite:false error:&error block:^(SecDbItemRef item) {
                ckksnotice("ckksscan", self.deps.zoneID, "scanning item: %@", item);

                self.processedItems += 1;

                // First check: is this a tombstone? If so, skip with prejudice.
                if(SecDbItemIsTombstone(item)) {
                    ckksinfo("ckksscan", self.deps.zoneID, "Skipping tombstone %@", item);
                    return;
                }

                // Second check: is this item a CKKS key for a view? If so, skip.
                if([CKKSKey isItemKeyForKeychainView:item] != nil) {
                    ckksinfo("ckksscan", self.deps.zoneID, "Scanned item is a CKKS internal key, skipping");
                    return;
                }

                // Third check: What view is this for?
                NSString* viewForItem = [[CKKSViewManager manager] viewNameForItem:item];
                if(![viewForItem isEqualToString:self.deps.zoneID.zoneName]) {
                    ckksinfo("ckksscan", self.deps.zoneID, "Scanned item is for view %@, skipping", viewForItem);
                    return;
                }

                // Fourth check: does this item have a UUID? If not, mark for later onboarding.
                CFErrorRef cferror = NULL;

                NSString* uuid = (__bridge_transfer NSString*) CFRetain(SecDbItemGetValue(item, &v10itemuuid, &cferror));
                if(!uuid || [uuid isEqual: [NSNull null]]) {
                    ckksnotice("ckksscan", self.deps.zoneID, "making new UUID for item %@: %@", item, cferror);

                    NSMutableDictionary* primaryKey = [(NSDictionary*)CFBridgingRelease(SecDbItemCopyPListWithMask(item, kSecDbPrimaryKeyFlag, &cferror)) mutableCopy];

                    // Class is an important part of a primary key, SecDb
                    primaryKey[(id)kSecClass] = itemClass;

                    if(SecErrorGetOSStatus(cferror) != errSecSuccess) {
                        ckkserror("ckksscan", self.deps.zoneID, "couldn't copy UUID-less item's primary key: %@", cferror);
                        SecTranslateError(&error, cferror);
                        self.error = error;
                        return;
                    }

                    [primaryKeysWithNoUUIDs addObject:primaryKey];
                    return;
                }

                // Is there a known sync item with this UUID?
                CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid
                                                                  zoneID:self.deps.zoneID
                                                                   error:&error];
                if(ckme != nil) {
                    [mirrorUUIDs removeObject:uuid];
                    ckksinfo("ckksscan", self.deps.zoneID, "Existing mirror entry with UUID %@", uuid);

                    if([self areEquivalent:item ckksItem:ckme.item]) {
                        // Fair enough.
                        return;
                    } else {
                        ckksnotice("ckksscan", self.deps.zoneID, "Existing mirror entry with UUID %@ does not match local item", uuid);
                    }
                }

                // We don't care about the oqe state here, just that one exists
                CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:uuid
                                                                               zoneID:self.deps.zoneID
                                                                                error:&error];
                if(oqe != nil) {
                    ckksnotice("ckksscan", self.deps.zoneID, "Existing outgoing queue entry with UUID %@", uuid);
                    // If its state is 'new', mark down that we've seen new entries that need processing
                    self.newCKKSEntries |= !![oqe.state isEqualToString:SecCKKSStateNew];
                    return;
                }

                // Hurray, we can help!
                ckksnotice("ckksscan", self.deps.zoneID, "Item(%@) is new; will attempt to add to CKKS", uuid);
                self.recordsFound += 1;

                NSMutableSet<NSString*>* classUUIDs = itemUUIDsNotYetInCKKS[itemClass];
                if(!classUUIDs) {
                    classUUIDs = [NSMutableSet set];
                    itemUUIDsNotYetInCKKS[itemClass] = classUUIDs;
                }
                [classUUIDs addObject:uuid];
            }];
        }

        // We're done checking local keychain for extra items, now let's make sure the mirror doesn't have extra items that the keychain doesn't have, either
        if (mirrorUUIDs.count > 0) {
            ckksnotice("ckksscan", self.deps.zoneID, "keychain missing %lu items from mirror, proceeding with queue scanning", (unsigned long)mirrorUUIDs.count);
            [mirrorUUIDs minusSet:[NSSet setWithArray:[CKKSIncomingQueueEntry allUUIDs:self.deps.zoneID error:&error]]];
            if (error) {
                ckkserror("ckksscan",  self.deps.zoneID, "unable to inspect incoming queue: %@", error);
                self.error = error;
                return;
            }

            [mirrorUUIDs minusSet:[NSSet setWithArray:[CKKSOutgoingQueueEntry allUUIDs:self.deps.zoneID error:&error]]];
            if (error) {
                ckkserror("ckksscan", self.deps.zoneID, "unable to inspect outgoing queue: %@", error);
                self.error = error;
                return;
            }
        }

        // Drop off of read-only transaction
    }];

    if(self.error) {
        ckksnotice("ckksscan", self.deps.zoneID, "Exiting due to previous error: %@", self.error);
        return;
    }

    ckksnotice("ckksscan", self.deps.zoneID, "Found %d item classes with missing items", (int)itemUUIDsNotYetInCKKS.count);

    for(NSString* itemClass in [itemUUIDsNotYetInCKKS allKeys]) {
        [self onboardItemsWithUUIDs:itemUUIDsNotYetInCKKS[itemClass] itemClass:itemClass databaseProvider:databaseProvider];
    }

    [self fixUUIDlessItemsWithPrimaryKeys:primaryKeysWithNoUUIDs databaseProvider:databaseProvider];

    [self retriggerMissingMirrorEntires:mirrorUUIDs
                                   ckks:ckks
                       databaseProvider:databaseProvider];

    [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventScanLocalItems zone:self.deps.zoneID.zoneName count:self.processedItems];

    // Write down that a scan occurred
    [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        CKKSZoneStateEntry* zoneState = [CKKSZoneStateEntry state:self.deps.zoneID.zoneName];

        zoneState.lastLocalKeychainScanTime = [NSDate now];

        NSError* saveError = nil;
        [zoneState saveToDatabase:&saveError];

        if(saveError) {
            ckkserror("ckksscan", self.deps.zoneID, "Unable to save 'scanned' bit: %@", saveError);
        } else {
            ckksnotice("ckksscan", self.deps.zoneID, "Saved scanned status.");
        }

        return CKKSDatabaseTransactionCommit;
    }];

    if(self.newCKKSEntries) {
        // Schedule a "view changed" notification
        [self.deps.notifyViewChangedScheduler trigger];

        // notify CKKS that it should process these new entries
        [ckks processOutgoingQueue:self.ckoperationGroup];
        // TODO: self.nextState = SecCKKSZoneKeyStateProcessOutgoingQueue;
    } else {
        self.nextState = self.intendedState;
    }

    if(self.missingLocalItemsFound > 0) {
        [ckks processIncomingQueue:false];
        // TODO [self.deps.flagHandler _onqueueHandleFlag:CKKSFlagProcessIncomingQueue];
    }

    ckksnotice("ckksscan", self.deps.zoneID, "Completed scan");
    (void)transaction;
}

- (BOOL)areEquivalent:(SecDbItemRef)item ckksItem:(CKKSItem*)ckksItem
{
    NSError* localerror = nil;
    NSDictionary* attributes = [CKKSIncomingQueueOperation decryptCKKSItemToAttributes:ckksItem error:&localerror];
    if(!attributes || localerror) {
        ckksnotice("ckksscan", self.deps.zoneID, "Could not decrypt item for comparison: %@", localerror);
        return YES;
    }

    CFErrorRef cferror = NULL;
    NSDictionary* objdict = (NSMutableDictionary*)CFBridgingRelease(SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &cferror));
    localerror = (NSError*)CFBridgingRelease(cferror);

    if(!objdict || localerror) {
        ckksnotice("ckksscan", self.deps.zoneID, "Could not get item contents for comparison: %@", localerror);

        // Fail open: assert that this item doesn't match
        return NO;
    }

    for(id key in objdict) {
        // Okay, but seriously storing dates as floats was a mistake.
        // Don't compare cdat and mdat, as they'll usually be different.
        // Also don't compare the sha1, as it hashes that double.
        if([key isEqual:(__bridge id)kSecAttrCreationDate] ||
           [key isEqual:(__bridge id)kSecAttrModificationDate] ||
           [key isEqual:(__bridge id)kSecAttrSHA1]) {
            continue;
        }

        id value = objdict[key];
        id attributesValue = attributes[key];

        if(![value isEqual:attributesValue]) {
            return NO;
        }
    }

    return YES;
}

@end;

#endif
