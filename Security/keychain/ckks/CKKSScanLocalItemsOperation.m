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
#import "keychain/ckks/CKKSMemoryKeyCache.h"
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

@property NSMutableSet<CKKSKeychainViewState*>* viewsWithNewCKKSEntries;
@end

@implementation CKKSScanLocalItemsOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}
- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                           intending:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
                    ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if((self = [super init])) {
        _deps = dependencies;
        _ckoperationGroup = ckoperationGroup;

        _nextState = errorState;
        _intendedState = intendedState;

        _viewsWithNewCKKSEntries = [NSMutableSet set];

        _recordsFound = 0;
        _recordsAdded = 0;
    }
    return self;
}

- (NSDictionary*)queryPredicatesForViewMapping:(NSSet<CKKSKeychainViewState*>*)activeViews
{
    if(activeViews.count > 1 || activeViews.count == 0) {
        ckksnotice_global("ckksscan", "Not acting on exactly one view; not limiting query: %@", activeViews);
        return @{};
    }

    CKKSKeychainViewState* onlyState = [activeViews allObjects][0];

    TPPBPolicyKeyViewMapping* viewRule = nil;

    // If there's more than one rule matching these views, then exit with an empty dictionary: the language doesn't support ORs.
    for(TPPBPolicyKeyViewMapping* mapping in self.deps.syncingPolicy.keyViewMapping) {
        if([mapping.view isEqualToString:onlyState.zoneID.zoneName]) {
            if(viewRule == nil) {
                viewRule = mapping;
            } else {
                // Too many rules for this view! Don't perform optimization.
                ckksnotice("ckksscan", onlyState.zoneID, "Too many policy rules for view %@", onlyState.zoneID.zoneName);
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
           [viewRule.matchingRule.match.regex isEqualToString:[NSString stringWithFormat:@"^%@$", onlyState.zoneID.zoneName]]) {
            return @{
                (id)kSecAttrSyncViewHint: onlyState.zoneName,
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
            ckksnotice_global("ckksscan", "Policy view rule is not a match against viewhint: %@", viewRule);
        }
    } else {
        ckksnotice_global("ckksscan", "Policy view rule is complex: %@", viewRule);
    }

    return @{};
}

- (BOOL)executeQuery:(NSDictionary*)queryPredicates readWrite:(bool)readWrite error:(NSError**)error block:(void (^_Nonnull)(SecDbItemRef item))block
{
    __block CFErrorRef cferror = NULL;
    __block bool ok = false;

    Query *q = query_create_with_limit((__bridge CFDictionaryRef)queryPredicates, NULL, kSecMatchUnlimited, NULL, &cferror);

    if(cferror) {
        ckkserror_global("ckksscan", "couldn't create query: %@", cferror);
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
        ckkserror_global("ckksscan", "couldn't execute query: %@", cferror);
        SecTranslateError(error, cferror);
        return NO;
    }

    return YES;
}

- (BOOL)onboardItemToCKKS:(SecDbItemRef)item
                viewState:(CKKSKeychainViewState*)viewState
                 keyCache:(CKKSMemoryKeyCache*)keyCache
                    error:(NSError**)error
{
    NSError* itemSaveError = nil;

    CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry withItem:item
                                                            action:SecCKKSActionAdd
                                                            zoneID:viewState.zoneID
                                                          keyCache:keyCache
                                                             error:&itemSaveError];

    if(itemSaveError) {
        ckkserror("ckksscan", viewState.zoneID, "Need to upload %@, but can't create outgoing entry: %@", item, itemSaveError);
        if(error) {
            *error = itemSaveError;
        }
        return NO;
    }

    ckksnotice("ckksscan", viewState.zoneID, "Syncing new item: %@", oqe);

    [oqe saveToDatabase:&itemSaveError];
    if(itemSaveError) {
        ckkserror("ckksscan", viewState.zoneID, "Need to upload %@, but can't save to database: %@", oqe, itemSaveError);
        self.error = itemSaveError;
        return NO;
    }

    [self.viewsWithNewCKKSEntries addObject:viewState];
    self.recordsAdded += 1;

    return YES;
}

- (void)onboardItemsInView:(CKKSKeychainViewState*)viewState
                     uuids:(NSSet<NSString*>*)uuids
                 itemClass:(NSString*)itemClass
          databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
{
    ckksnotice("ckksscan", viewState.zoneID, "Found %d missing %@ items", (int)uuids.count, itemClass);
    // Use one transaction for each item to allow for SecItem API calls to interleave
    for(NSString* itemUUID in uuids) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult {
            CKKSMemoryKeyCache* keyCache = [[CKKSMemoryKeyCache alloc] init];

            NSDictionary* queryAttributes = @{
                (id)kSecClass: itemClass,
                (id)kSecReturnRef: @(YES),
                (id)kSecAttrSynchronizable: @(YES),
                (id)kSecAttrTombstone: @(NO),
                (id)kSecAttrUUID: itemUUID,
            };

            ckksnotice("ckksscan", viewState.zoneID, "Onboarding %@ %@", itemClass, itemUUID);

            __block NSError* itemSaveError = nil;

            [self executeQuery:queryAttributes readWrite:true error:&itemSaveError block:^(SecDbItemRef itemToSave) {
                [self onboardItemToCKKS:itemToSave
                              viewState:viewState
                               keyCache:keyCache
                                  error:&itemSaveError];
            }];

            if(itemSaveError) {
                ckkserror("ckksscan", viewState.zoneID, "Need to upload %@, but can't save to database: %@", itemUUID, itemSaveError);
                self.error = itemSaveError;
                return CKKSDatabaseTransactionRollback;
            }

            return CKKSDatabaseTransactionCommit;
        }];
    }
}

- (void)fixUUIDlessItemsInZone:(CKKSKeychainViewState*)viewState
                   primaryKeys:(NSMutableSet<NSDictionary*>*)primaryKeys
              databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
{
    ckksnotice("ckksscan", viewState.zoneID, "Found %d items missing UUIDs", (int)primaryKeys.count);

    if([primaryKeys count] == 0) {
        return;
    }

    [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        __block NSError* itemError = nil;

        __block CKKSMemoryKeyCache* keyCache = [[CKKSMemoryKeyCache alloc] init];

        for(NSDictionary* primaryKey in primaryKeys) {
            ckksnotice("ckksscan", viewState.zoneID, "Found item with no uuid: %@", primaryKey);

            __block CFErrorRef cferror = NULL;

            bool connectionSuccess = kc_with_dbt(true, &cferror, ^bool (SecDbConnectionRef dbt) {
                Query *q = query_create_with_limit((__bridge CFDictionaryRef)primaryKey, NULL, kSecMatchUnlimited, NULL, &cferror);

                if(!q || cferror) {
                    ckkserror("ckksscan", viewState.zoneID, "couldn't create query: %@", cferror);
                    return false;
                }

                __block bool ok = true;

                ok &= SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef uuidlessItem, bool *stop) {
                    NSString* uuid = [[NSUUID UUID] UUIDString];
                    NSDictionary* updates = @{(id)kSecAttrUUID: uuid};

                    ckksnotice("ckksscan", viewState.zoneID, "Assigning new UUID %@ for item %@", uuid, uuidlessItem);

                    SecDbItemRef new_item = SecDbItemCopyWithUpdates(uuidlessItem, (__bridge CFDictionaryRef)updates, &cferror);

                    if(!new_item) {
                        SecTranslateError(&itemError, cferror);
                        self.error = itemError;
                        ckksnotice("ckksscan", viewState.zoneID, "Unable to copy item with new UUID: %@", cferror);
                        return;
                    }

                    bool updateSuccess = kc_transaction_type(dbt, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, ^{
                        return SecDbItemUpdate(uuidlessItem, new_item, dbt, kCFBooleanFalse, q->q_uuid_from_primary_key, &cferror);
                    });

                    if(updateSuccess) {
                        [self onboardItemToCKKS:new_item
                                      viewState:viewState
                                       keyCache:keyCache
                                          error:&itemError];
                    } else {
                        ckksnotice("ckksscan", viewState.zoneID, "Unable to update item with new UUID: %@", cferror);
                    }

                    ok &= updateSuccess;
                });

                ok &= query_notify_and_destroy(q, ok, &cferror);

                return true;
            });

            if(!connectionSuccess) {
                ckkserror("ckksscan", viewState.zoneID, "couldn't execute query: %@", cferror);
                SecTranslateError(&itemError, cferror);
                self.error = itemError;
                return CKKSDatabaseTransactionRollback;
            }
        }

        return CKKSDatabaseTransactionCommit;
    }];
}

- (void)retriggerMissingMirrorEntries:(NSSet<NSString*>*)mirrorUUIDs
                     databaseProvider:(id<CKKSDatabaseProviderProtocol>)databaseProvider
{
    if (mirrorUUIDs.count > 0) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;
            ckkserror_global("ckksscan", "BUG: keychain missing %lu items from mirror and/or queues: %@", (unsigned long)mirrorUUIDs.count, mirrorUUIDs);
            self.missingLocalItemsFound = mirrorUUIDs.count;

            [[CKKSAnalytics logger] logMetric:[NSNumber numberWithUnsignedInteger:mirrorUUIDs.count] withName:CKKSEventMissingLocalItemsFound];

            for (NSString* uuid in mirrorUUIDs) {
                NSArray<CKKSMirrorEntry*>* ckmes = [CKKSMirrorEntry allWithUUID:uuid error:&error];

                if(!ckmes || error) {
                    ckkserror_global("ckksscan", "BUG: error fetching previously-extant CKME (uuid: %@) from database: %@", uuid, error);
                    self.error = error;
                } else {
                    for(CKKSMirrorEntry* ckme in ckmes) {
                        [self.deps intransactionCKRecordChanged:ckme.item.storedCKRecord resync:true];
                    }
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
        ckksnotice_global("ckksscan", "No missing local items found");
    }
}

- (void)main
{
    if(SecCKKSTestsEnabled() && SecCKKSTestSkipScan()) {
        ckksnotice_global("ckksscan", "Scan cancelled by test request");
        self.nextState = self.intendedState;
        return;
    }

    // We need to not be jetsamed while running this
    os_transaction_t transaction = os_transaction_create("com.apple.securityd.ckks.scan");

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    // A map of CKKSViewState -> ItemClass -> Set of found UUIDs
    NSMutableDictionary<CKKSKeychainViewState*, NSMutableDictionary<NSString*, NSMutableSet<NSString*>*>*>* itemUUIDsNotYetInCKKS = [NSMutableDictionary dictionary];

    // A map of CKKSViewState -> list of primary keys of items that fit in this view, but have no UUIDs
    NSMutableDictionary<CKKSKeychainViewState*, NSMutableSet<NSDictionary*>*>* primaryKeysWithNoUUIDs = [NSMutableDictionary dictionary];

    // We want this set to be empty after scanning, or else the keychain (silently) dropped something on the floor
    NSMutableSet<NSString*>* mirrorUUIDs = [NSMutableSet set];

    ckksnotice_global("ckksscan", "Scanning for views: %@", self.deps.activeManagedViews);

    NSMutableSet<CKRecordZoneID*>* allZoneIDs = [NSMutableSet set];
    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        [allZoneIDs addObject:viewState.zoneID];
    }

    [databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        // First, query for all synchronizable items
        __block NSError* error = nil;

        NSError* ckmeError = nil;
        [mirrorUUIDs unionSet:[CKKSMirrorEntry allUUIDsInZones:allZoneIDs error:&ckmeError]];

        if(ckmeError) {
            ckkserror_global("ckksscan", "Unable to load CKMirrorEntries: %@", ckmeError);
        }

        __block CKKSMemoryKeyCache* keyCache = [[CKKSMemoryKeyCache alloc] init];

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

            NSDictionary* extraQueryPredicates = [self queryPredicatesForViewMapping:self.deps.views];
            [queryAttributes addEntriesFromDictionary:extraQueryPredicates];

            ckksnotice_global("ckksscan", "Scanning all synchronizable %@ items(%@) for: %@", itemClass, self.name, queryAttributes);

            [self executeQuery:queryAttributes readWrite:false error:&error block:^(SecDbItemRef item) {
                ckksnotice_global("ckksscan", "scanning item: %@", item);

                self.processedItems += 1;

                // First check: is this a tombstone, marked this-device-only, or for a non-primary user? If so, skip with prejudice.
                if(SecDbItemIsTombstone(item)) {
                    ckksinfo_global("ckksscan", "Skipping tombstone %@", item);
                    return;
                }

                NSString* protection = (__bridge NSString*)SecDbItemGetCachedValueWithName(item, kSecAttrAccessible);
                if(!([protection isEqualToString:(__bridge NSString*)kSecAttrAccessibleWhenUnlocked] ||
                     [protection isEqualToString:(__bridge NSString*)kSecAttrAccessibleAfterFirstUnlock] ||
                     [protection isEqualToString:(__bridge NSString*)kSecAttrAccessibleAlwaysPrivate])) {
                    ckksnotice_global("ckksscan", "skipping sync of device-bound(%@) item", protection);
                    return;
                }

                // Note: I don't expect that this will ever fire, because the query as created will only find primary-user items. But, it's here as a seatbelt!
                if(!SecDbItemIsPrimaryUserItem(item)) {
                    ckksnotice_global("ckksscan", "Ignoring syncable keychain item for non-primary account: %@", item);
                    return;
                }

                // Second check: is this item a CKKS key for a view? If so, skip.
                if([CKKSKey isItemKeyForKeychainView:item] != nil) {
                    ckksinfo_global("ckksscan", "Scanned item is a CKKS internal key, skipping");
                    return;
                }

                // Third check: What view is this for?
                NSString* viewForItem = [self.deps viewNameForItem:item];
                CKKSKeychainViewState* viewStateForItem = nil;
                for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
                    if([viewState.zoneID.zoneName isEqualToString:viewForItem]) {
                        viewStateForItem = viewState;
                        break;
                    }
                }

                if(viewStateForItem == nil) {
                    ckksinfo_global("ckksscan", "Scanned item is for view %@, skipping", viewForItem);

                    // todo: if this is a view that's active but not ready, we should scan it later...

                    return;
                }

                // Fourth check: does this item have a UUID? If not, mark for later onboarding.
                CFErrorRef cferror = NULL;

                NSString* uuid = (__bridge_transfer NSString*) CFRetain(SecDbItemGetValue(item, &v10itemuuid, &cferror));
                if(!uuid || [uuid isEqual: [NSNull null]]) {
                    ckksnotice("ckksscan", viewStateForItem.zoneID, "making new UUID for item %@: %@", item, cferror);

                    NSMutableDictionary* primaryKey = [(NSDictionary*)CFBridgingRelease(SecDbItemCopyPListWithMask(item, kSecDbPrimaryKeyFlag, &cferror)) mutableCopy];

                    // Class is an important part of a primary key, SecDb
                    primaryKey[(id)kSecClass] = itemClass;

                    if(SecErrorGetOSStatus(cferror) != errSecSuccess) {
                        ckkserror("ckksscan", viewStateForItem.zoneID, "couldn't copy UUID-less item's primary key: %@", cferror);
                        SecTranslateError(&error, cferror);
                        self.error = error;
                        return;
                    }

                    NSMutableSet* primaryKeysWithNoUUIDsForView = primaryKeysWithNoUUIDs[viewStateForItem];
                    if(primaryKeysWithNoUUIDsForView == nil) {
                        primaryKeysWithNoUUIDsForView = [NSMutableSet set];
                        primaryKeysWithNoUUIDs[viewStateForItem] = primaryKeysWithNoUUIDsForView;
                    }
                    [primaryKeysWithNoUUIDsForView addObject:primaryKey];
                    return;
                }

                // Is there a known sync item with this UUID?
                CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid
                                                                  zoneID:viewStateForItem.zoneID
                                                                   error:&error];
                if(ckme != nil) {
                    [mirrorUUIDs removeObject:uuid];
                    ckksinfo("ckksscan", viewStateForItem.zoneID, "Existing mirror entry with UUID %@", uuid);

                    if([self areEquivalent:item ckksItem:ckme.item keyCache:keyCache]) {
                        // Fair enough.
                        return;
                    } else {
                        ckksnotice("ckksscan", viewStateForItem.zoneID, "Existing mirror entry with UUID %@ does not match local item", uuid);
                    }
                }

                // We don't care about the oqe state here, just that one exists
                CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:uuid
                                                                               zoneID:viewStateForItem.zoneID
                                                                                error:&error];
                if(oqe != nil) {
                    ckksnotice("ckksscan", viewStateForItem.zoneID, "Existing outgoing queue entry with UUID %@", uuid);
                    // If its state is 'new', mark down that we've seen new entries that need processing
                    if([oqe.state isEqualToString:SecCKKSStateNew]) {
                        [self.viewsWithNewCKKSEntries addObject:viewStateForItem];
                    }

                    return;
                }

                // Hurray, we can help!
                ckksnotice("ckksscan", viewStateForItem.zoneID, "Item(%@) is new; will attempt to add to CKKS", uuid);
                self.recordsFound += 1;

                NSMutableDictionary* itemClassMap = itemUUIDsNotYetInCKKS[viewStateForItem];
                if(!itemClassMap) {
                    itemClassMap = [NSMutableDictionary dictionary];
                    itemUUIDsNotYetInCKKS[viewStateForItem] = itemClassMap;
                }

                NSMutableSet<NSString*>* classUUIDs = itemClassMap[itemClass];
                if(!classUUIDs) {
                    classUUIDs = [NSMutableSet set];
                    itemClassMap[itemClass] = classUUIDs;
                }
                [classUUIDs addObject:uuid];
            }];
        }

        // We're done checking local keychain for extra items, now let's make sure the mirror doesn't have extra items that the keychain doesn't have, either
        if (mirrorUUIDs.count > 0) {
            ckksnotice_global("ckksscan", "keychain missing %lu items from mirror, proceeding with queue scanning", (unsigned long)mirrorUUIDs.count);

            [mirrorUUIDs minusSet:[CKKSIncomingQueueEntry allUUIDsInZones:allZoneIDs error:&error]];
            if (error) {
                ckkserror_global("ckksscan", "unable to inspect incoming queue: %@", error);
                self.error = error;
                return;
            }

            [mirrorUUIDs minusSet:[CKKSOutgoingQueueEntry allUUIDsInZones:allZoneIDs error:&error]];
            if (error) {
                ckkserror_global("ckksscan", "unable to inspect outgoing queue: %@", error);
                self.error = error;
                return;
            }
        }

        // Drop off of read-only transaction
    }];

    if(self.error) {
        ckksnotice_global("ckksscan", "Exiting due to previous error: %@", self.error);
        return;
    }

    ckksnotice_global("ckksscan", "Found %d views with missing items for %@", (int)itemUUIDsNotYetInCKKS.count, self.deps.activeManagedViews);
    for(CKKSKeychainViewState* viewState in [itemUUIDsNotYetInCKKS allKeys]) {
        NSMutableDictionary* itemClassesForView = itemUUIDsNotYetInCKKS[viewState];

        for(NSString* itemClass in [itemClassesForView allKeys]) {
            NSMutableSet<NSString*>* missingUUIDs = itemClassesForView[itemClass];
            ckksnotice("ckksscan", viewState, "Found %d missing %@ items for %@", (int)missingUUIDs.count, itemClass, viewState);
            [self onboardItemsInView:viewState
                               uuids:missingUUIDs
                           itemClass:itemClass
                    databaseProvider:databaseProvider];
        }
    }

    for(CKKSKeychainViewState* viewState in [primaryKeysWithNoUUIDs allKeys]) {
        [self fixUUIDlessItemsInZone:viewState
                         primaryKeys:primaryKeysWithNoUUIDs[viewState]
                    databaseProvider:databaseProvider];
    }

    [self retriggerMissingMirrorEntries:mirrorUUIDs
                       databaseProvider:databaseProvider];

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventScanLocalItems zone:viewState.zoneID.zoneName count:self.processedItems];
    }

    // Write down that a scan occurred
    [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
            if(![viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady]) {
                ckksnotice("ckksscan", viewState.zoneID, "View wasn't ready for scan");
                continue;
            }

            [viewState.launch addEvent:@"scan-local-items"];

            CKKSZoneStateEntry* zoneState = [CKKSZoneStateEntry state:viewState.zoneID.zoneName];

            zoneState.lastLocalKeychainScanTime = [NSDate now];

            NSError* saveError = nil;
            [zoneState saveToDatabase:&saveError];

            if(saveError) {
                ckkserror("ckksscan", viewState.zoneID, "Unable to save 'scanned' bit: %@", saveError);
            } else {
                ckksnotice("ckksscan", viewState.zoneID, "Saved scanned status.");
            }
        }

        return CKKSDatabaseTransactionCommit;
    }];

    if(self.viewsWithNewCKKSEntries.count > 0) {
        for(CKKSKeychainViewState* viewState in self.viewsWithNewCKKSEntries) {
            // Schedule a "view changed" notification
            [viewState.notifyViewChangedScheduler trigger];
        }

        // notify CKKS that it should process these new entries
        if(self.ckoperationGroup) {
            ckkserror_global("ckksscan", "Transferring ckoperation group %@", self.ckoperationGroup);
            self.deps.currentOutgoingQueueOperationGroup = self.ckoperationGroup;
        }
        [self.deps.flagHandler handleFlag:CKKSFlagProcessOutgoingQueue];
        // TODO self.nextState = CKKSStateProcessOutgoingQueue;
    }

    self.nextState = self.intendedState;

    if(self.missingLocalItemsFound > 0) {
        [self.deps.flagHandler handleFlag:CKKSFlagProcessIncomingQueue];
    }

    ckksnotice_global("ckksscan", "Completed scan");
    (void)transaction;
}

- (BOOL)areEquivalent:(SecDbItemRef)item
             ckksItem:(CKKSItem*)ckksItem
             keyCache:(CKKSMemoryKeyCache*)keyCache
{
    NSError* localerror = nil;

    NSDictionary* attributes = [CKKSIncomingQueueOperation decryptCKKSItemToAttributes:ckksItem
                                                                              keyCache:keyCache
                                                                                 error:&localerror];
    if(!attributes || localerror) {
        ckksnotice("ckksscan", ckksItem.zoneID, "Could not decrypt item for comparison: %@", localerror);
        return YES;
    }

    CFErrorRef cferror = NULL;
    NSDictionary* objdict = (NSMutableDictionary*)CFBridgingRelease(SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &cferror));
    localerror = (NSError*)CFBridgingRelease(cferror);

    if(!objdict || localerror) {
        ckksnotice("ckksscan", ckksItem.zoneID, "Could not get item contents for comparison: %@", localerror);

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
