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
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSItemEncrypter.h"

#import "CKKSPowerCollection.h"

#include <securityd/SecItemSchema.h>
#include <securityd/SecItemServer.h>
#include <securityd/SecItemDb.h>
#include <Security/SecItemPriv.h>
#include <utilities/SecInternalReleasePriv.h>
#import <IMCore/IMCore_Private.h>
#import <IMCore/IMCloudKitHooks.h>

@interface CKKSScanLocalItemsOperation ()
@property CKOperationGroup* ckoperationGroup;
@property (assign) NSUInteger processedItems;
@end

@implementation CKKSScanLocalItemsOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks ckoperationGroup:(CKOperationGroup*)ckoperationGroup {
    if(self = [super init]) {
        _ckks = ckks;
        _ckoperationGroup = ckoperationGroup;
        _recordsFound = 0;
        _recordsAdded = 0;
    }
    return self;
}

- (void) main {
    // Take a strong reference.
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksscan", ckks, "no CKKS object");
        return;
    }

    [ckks.launch addEvent:@"scan-local-items"];

    [ckks dispatchSyncWithAccountKeys: ^bool{
        if(self.cancelled) {
            ckksnotice("ckksscan", ckks, "CKKSScanLocalItemsOperation cancelled, quitting");
            return false;
        }
        ckks.lastScanLocalItemsOperation = self;

        NSMutableArray* itemsForManifest = [NSMutableArray array];

        // First, query for all synchronizable items
        __block CFErrorRef cferror = NULL;
        __block NSError* error = nil;
        __block bool newEntries = false;

        // We want this set to be empty after scanning, or else the keychain (silently) dropped something on the floor
        NSMutableSet<NSString*>* mirrorUUIDs = [NSMutableSet setWithArray:[CKKSMirrorEntry allUUIDs:ckks.zoneID error:&error]];

        // Must query per-class, so:
        const SecDbSchema *newSchema = current_schema();
        for (const SecDbClass *const *class = newSchema->classes; *class != NULL; class++) {
            cferror = NULL;

            if(!((*class)->itemclass)) {
                // Don't try to scan non-item 'classes'
                continue;
            }

            NSDictionary* queryAttributes = @{(__bridge NSString*) kSecClass: (__bridge NSString*) (*class)->name,
                                              (__bridge NSString*) kSecReturnRef: @(YES),
                                              (__bridge NSString*) kSecAttrSynchronizable: @(YES),
                                              (__bridge NSString*) kSecAttrTombstone: @(NO),
                                              // This works ~as long as~ item views are chosen by view hint only. It's a significant perf win, though.
                                              // <rdar://problem/32269541> SpinTracer: CKKSScanLocalItemsOperation expensive on M8 machines
                                              (__bridge NSString*) kSecAttrSyncViewHint: ckks.zoneName,
                                              };
            ckksinfo("ckksscan", ckks, "Scanning all synchronizable items for: %@", queryAttributes);

            Query *q = query_create_with_limit( (__bridge CFDictionaryRef) queryAttributes, NULL, kSecMatchUnlimited, &cferror);
            bool ok = false;

            if(cferror) {
                ckkserror("ckksscan", ckks, "couldn't create query: %@", cferror);
                SecTranslateError(&error, cferror);
                self.error = error;
                continue;
            }

            ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt) {
                return SecDbItemQuery(q, NULL, dbt, &cferror, ^(SecDbItemRef item, bool *stop) {
                    ckksnotice("ckksscan", ckks, "scanning item: %@", item);

                    self.processedItems += 1;

                    SecDbItemRef itemToSave = NULL;

                    // First check: is this a tombstone? If so, skip with prejudice.
                    if(SecDbItemIsTombstone(item)) {
                        ckksinfo("ckksscan", ckks, "Skipping tombstone %@", item);
                        return;
                    }

                    // Second check: is this item even for this view? If not, skip.
                    NSString* viewForItem = [[CKKSViewManager manager] viewNameForItem:item];
                    if(![viewForItem isEqualToString: ckks.zoneName]) {
                        ckksinfo("ckksscan", ckks, "Scanned item is for view %@, skipping", viewForItem);
                        return;
                    }

                    // Third check: is this item one of our keys for a view? If not, skip.
                    if([CKKSKey isItemKeyForKeychainView: item] != nil) {
                        ckksinfo("ckksscan", ckks, "Scanned item is a CKKS internal key, skipping");
                        return;
                    }

                    // Fourth check: does this item have a UUID? If not, ONBOARD!
                    NSString* uuid = (__bridge_transfer NSString*) CFRetain(SecDbItemGetValue(item, &v10itemuuid, &cferror));
                    if(!uuid || [uuid isEqual: [NSNull null]]) {
                        ckksnotice("ckksscan", ckks, "making new UUID for item %@", item);

                        uuid = [[NSUUID UUID] UUIDString];
                        NSDictionary* updates = @{(id) kSecAttrUUID: uuid};

                        SecDbItemRef new_item = SecDbItemCopyWithUpdates(item, (__bridge CFDictionaryRef) updates, &cferror);
                        if(SecErrorGetOSStatus(cferror) != errSecSuccess) {
                            ckkserror("ckksscan", ckks, "couldn't update item with new UUID: %@", cferror);
                            SecTranslateError(&error, cferror);
                            self.error = error;
                            CFReleaseNull(new_item);
                            return;
                        }

                        if (new_item) {
                            bool ok = kc_transaction_type(dbt, kSecDbExclusiveRemoteCKKSTransactionType, &cferror, ^{
                                return SecDbItemUpdate(item, new_item, dbt, kCFBooleanFalse, q->q_uuid_from_primary_key, &cferror);
                            });

                            if(!ok || SecErrorGetOSStatus(cferror) != errSecSuccess) {
                                ckkserror("ckksscan", ckks, "couldn't update item with new UUID: %@", cferror);
                                SecTranslateError(&error, cferror);
                                self.error = error;
                                CFReleaseNull(new_item);
                                return;
                            }
                        }
                        itemToSave = CFRetainSafe(new_item);
                        CFReleaseNull(new_item);

                    } else {
                        // Is there a known sync item with this UUID?
                        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase: uuid zoneID:ckks.zoneID error: &error];
                        if(ckme != nil) {
                            if ([CKKSManifest shouldSyncManifests]) {
                                [itemsForManifest addObject:ckme.item];
                            }
                            [mirrorUUIDs removeObject:uuid];
                            ckksinfo("ckksscan", ckks, "Existing mirror entry with UUID %@", uuid);

                            if([self areEquivalent:item ckksItem:ckme.item]) {
                                // Fair enough.
                                return;
                            } else {
                                ckksnotice("ckksscan", ckks, "Existing mirror entry with UUID %@ does not match local item", uuid);
                            }
                        }

                        // We don't care about the oqe state here, just that one exists
                        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase: uuid zoneID:ckks.zoneID error: &error];
                        if(oqe != nil) {
                            ckksnotice("ckksscan", ckks, "Existing outgoing queue entry with UUID %@", uuid);
                            // If its state is 'new', mark down that we've seen new entries that need processing
                            newEntries |= !![oqe.state isEqualToString: SecCKKSStateNew];
                            return;
                        }

                        itemToSave = CFRetainSafe(item);
                    }

                    // Hurray, we can help!
                    self.recordsFound += 1;

                    CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry withItem: itemToSave action: SecCKKSActionAdd ckks:ckks error: &error];

                    if(error) {
                        ckkserror("ckksscan", ckks, "Need to upload %@, but can't create outgoing entry: %@", item, error);
                        self.error = error;
                        CFReleaseNull(itemToSave);
                        return;
                    }

                    ckksnotice("ckksscan", ckks, "Syncing new item: %@", oqe);
                    CFReleaseNull(itemToSave);

                    [oqe saveToDatabase: &error];
                    if(error) {
                        ckkserror("ckksscan", ckks, "Need to upload %@, but can't save to database: %@", oqe, error);
                        self.error = error;
                        return;
                    }
                    newEntries = true;
                    if ([CKKSManifest shouldSyncManifests]) {
                        [itemsForManifest addObject:oqe.item];
                    }

                    self.recordsAdded += 1;
                });
            });

            if(cferror || !ok) {
                ckkserror("ckksscan", ckks, "error processing or finding items: %@", cferror);
                SecTranslateError(&error, cferror);
                self.error = error;
                query_destroy(q, NULL);
                continue;
            }

            ok = query_notify_and_destroy(q, ok, &cferror);

            if(cferror || !ok) {
                ckkserror("ckksscan", ckks, "couldn't delete query: %@", cferror);
                SecTranslateError(&error, cferror);
                self.error = error;
                continue;
            }
        }

        // We're done checking local keychain for extra items, now let's make sure the mirror doesn't have extra items, either
        if (mirrorUUIDs.count > 0) {
            ckksnotice("ckksscan", ckks, "keychain missing %lu items from mirror, proceeding with queue scanning", (unsigned long)mirrorUUIDs.count);
            [mirrorUUIDs minusSet:[NSSet setWithArray:[CKKSIncomingQueueEntry allUUIDs:ckks.zoneID error:&error]]];
            if (error) {
                ckkserror("ckksscan", ckks, "unable to inspect incoming queue: %@", error);
                self.error = error;
                return false;
            }

            [mirrorUUIDs minusSet:[NSSet setWithArray:[CKKSOutgoingQueueEntry allUUIDs:ckks.zoneID error:&error]]];
            if (error) {
                ckkserror("ckksscan", ckks, "unable to inspect outgoing queue: %@", error);
                self.error = error;
                return false;
            }

            if (mirrorUUIDs.count > 0) {
                ckkserror("ckksscan", ckks, "BUG: keychain missing %lu items from mirror and/or queues: %@", (unsigned long)mirrorUUIDs.count, mirrorUUIDs);
                self.missingLocalItemsFound = mirrorUUIDs.count;

                [[CKKSAnalytics logger] logMetric:[NSNumber numberWithUnsignedInteger:mirrorUUIDs.count] withName:CKKSEventMissingLocalItemsFound];

                for (NSString* uuid in mirrorUUIDs) {
                    CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid zoneID:ckks.zoneID error:&error];
                    [ckks _onqueueCKRecordChanged:ckme.item.storedCKRecord resync:true];
                }

                // And, if you're not in the tests, try to collect a sysdiagnose I guess?
                // <rdar://problem/36166435> Re-enable IMCore autosysdiagnose capture to securityd
                //if(SecIsInternalRelease() && !SecCKKSTestsEnabled()) {
                //    [[IMCloudKitHooks sharedInstance] tryToAutoCollectLogsWithErrorString:@"35810558" sendLogsTo:@"rowdy_bot@icloud.com"];
                //}
            } else {
                ckksnotice("ckksscan", ckks, "No missing local items found");
            }
        }

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventScanLocalItems  zone:ckks.zoneName count:self.processedItems];

        if ([CKKSManifest shouldSyncManifests]) {
            // TODO: this manifest needs to incorporate peer manifests
            CKKSEgoManifest* manifest = [CKKSEgoManifest newManifestForZone:ckks.zoneName withItems:itemsForManifest peerManifestIDs:@[] currentItems:@{} error:&error];
            if (!manifest || error) {
                ckkserror("ckksscan", ckks, "could not create manifest: %@", error);
                self.error = error;
                return false;
            }

            [manifest saveToDatabase:&error];
            if (error) {
                ckkserror("ckksscan", ckks, "could not save manifest to database: %@", error);
                self.error = error;
                return false;
            }

            ckks.egoManifest = manifest;
        }

        if(newEntries) {
            // Schedule a "view changed" notification
            [ckks.notifyViewChangedScheduler trigger];

            // notify CKKS that it should process these new entries
            [ckks processOutgoingQueue:self.ckoperationGroup];
        }

        if(self.missingLocalItemsFound > 0) {
            [ckks processIncomingQueue:false];
        }

        ckksnotice("ckksscan", ckks, "Completed scan");
        ckks.droppedItems = false;
        return true;
    }];
}

- (BOOL)areEquivalent:(SecDbItemRef)item ckksItem:(CKKSItem*)ckksItem
{
    CKKSKeychainView* ckks = self.ckks;

    NSError* localerror = nil;
    NSDictionary* attributes = [CKKSIncomingQueueOperation decryptCKKSItemToAttributes:ckksItem error:&localerror];
    if(!attributes || localerror) {
        ckksnotice("ckksscan", ckks, "Could not decrypt item for comparison: %@", localerror);
        return YES;
    }

    CFErrorRef cferror = NULL;
    NSDictionary* objdict = (NSMutableDictionary*)CFBridgingRelease(SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &cferror));
    localerror = (NSError*)CFBridgingRelease(cferror);

    if(!objdict || localerror) {
        ckksnotice("ckksscan", ckks, "Could not get item contents for comparison: %@", localerror);

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
