/*
 * Copyright (c) 2016-2020 Apple Inc. All Rights Reserved.
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

#import "keychain/analytics/CKKSPowerCollection.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSIncomingQueueOperation.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSMemoryKeyCache.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/ObjCImprovements.h"

#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include <Security/SecItemPriv.h>

#import <utilities/SecCoreAnalytics.h>

#if OCTAGON

@interface CKKSIncomingQueueOperation ()
@property bool newOutgoingEntries;
@property bool pendingClassAEntries;
@property bool missingKey;

@property NSMutableSet<NSString*>* viewsToScan;
@end

@implementation CKKSIncomingQueueOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                           intending:(OctagonState*)intending
                          errorState:(OctagonState*)errorState
                errorOnClassAFailure:(bool)errorOnClassAFailure
           handleMismatchedViewItems:(bool)handleMismatchedViewItems
{
    if(self = [super init]) {
        _deps = dependencies;

        _intendedState = intending;
        _nextState = errorState;

        _errorOnClassAFailure = errorOnClassAFailure;
        _pendingClassAEntries = false;

        _handleMismatchedViewItems = handleMismatchedViewItems;

        _viewsToScan = [NSMutableSet set];
    }
    return self;
}

- (bool)processNewCurrentItemPointers:(NSArray<CKKSCurrentItemPointer*>*)queueEntries
                            viewState:(CKKSKeychainViewState*)viewState
{
    NSError* error = nil;
    for(CKKSCurrentItemPointer* p in queueEntries) {
        @autoreleasepool {
            p.state = SecCKKSProcessedStateLocal;

            [p saveToDatabase:&error];
            ckksnotice("ckkspointer", viewState, "Saving new current item pointer: %@", p);
            if(error) {
                ckkserror("ckksincoming", viewState, "Error saving new current item pointer: %@ %@", error, p);
            }
        }
    }

    if(queueEntries.count > 0) {
        [viewState.notifyViewChangedScheduler trigger];
    }

    return (error == nil);
}

- (bool)intransaction:(CKKSKeychainViewState*)viewState processQueueEntries:(NSArray<CKKSIncomingQueueEntry*>*)queueEntries
{
    NSMutableArray* newOrChangedRecords = [[NSMutableArray alloc] init];
    NSMutableArray* deletedRecordIDs = [[NSMutableArray alloc] init];

    CKKSMemoryKeyCache* keyCache = [[CKKSMemoryKeyCache alloc] init];

    for(id entry in queueEntries) {
        @autoreleasepool {
            NSError* error = nil;

            CKKSIncomingQueueEntry* iqe = (CKKSIncomingQueueEntry*) entry;
            ckksnotice("ckksincoming", viewState.zoneID, "ready to process an incoming queue entry: %@ %@ %@", iqe, iqe.uuid, iqe.action);

            // Note that we currently unencrypt the item before deleting it, instead of just deleting it
            // This finds the class, which is necessary for the deletion process. We could just try to delete
            // across all classes, though...
            NSDictionary* attributes = [CKKSIncomingQueueOperation decryptCKKSItemToAttributes:iqe.item
                                                                                      keyCache:keyCache
                                                                                         error:&error];

            if(!attributes || error) {
                if([self.deps.lockStateTracker isLockedError:error]) {
                    NSError* localerror = nil;
                    ckkserror("ckksincoming", viewState.zoneID, "Keychain is locked; can't decrypt IQE %@", iqe);
                    CKKSKey* key = [CKKSKey tryFromDatabase:iqe.item.parentKeyUUID zoneID:viewState.zoneID error:&localerror];
                    if(localerror || ([key.keyclass isEqualToString:SecCKKSKeyClassA] && self.errorOnClassAFailure)) {
                        self.error = error;
                    }

                    // If this isn't an error, make sure it gets processed later.
                    if([key.keyclass isEqualToString:SecCKKSKeyClassA] && !self.errorOnClassAFailure) {
                        self.pendingClassAEntries = true;
                    }

                } else if ([error.domain isEqualToString:@"securityd"] && error.code == errSecItemNotFound) {
                    ckkserror("ckksincoming", viewState.zoneID, "Coudn't find key in keychain; will attempt to poke key hierarchy: %@", error)
                    self.missingKey = true;
                    self.error = error;

                } else {
                    ckkserror("ckksincoming", viewState.zoneID, "Couldn't decrypt IQE %@ for some reason: %@", iqe, error);
                    self.error = error;
                }
                self.errorItemsProcessed += 1;
                continue;
            }

            NSString* classStr = [attributes objectForKey: (__bridge NSString*) kSecClass];
            if(![classStr isKindOfClass: [NSString class]]) {
                self.error = [NSError errorWithDomain:@"securityd"
                                                 code:errSecInternalError
                                             userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"Item did not have a reasonable class: %@", classStr]}];
                ckkserror("ckksincoming", viewState.zoneID, "Synced item seems wrong: %@", self.error);
                self.errorItemsProcessed += 1;
                continue;
            }

            const SecDbClass * classP = !classStr ? NULL : kc_class_with_name((__bridge CFStringRef) classStr);

            if(!classP) {
                ckkserror("ckksincoming", viewState.zoneID, "unknown class in object: %@ %@", classStr, iqe);
                iqe.state = SecCKKSStateError;
                [iqe saveToDatabase:&error];
                if(error) {
                    ckkserror("ckksincoming", viewState.zoneID, "Couldn't save errored IQE to database: %@", error);
                    self.error = error;
                }
                self.errorItemsProcessed += 1;
                continue;
            }

            NSString* intendedView = [self.deps.syncingPolicy mapDictionaryToView:attributes];
            if(![viewState.zoneID.zoneName isEqualToString:intendedView]) {
                if(self.handleMismatchedViewItems) {
                    [self _onqueueHandleMismatchedViewItem:iqe
                                                secDbClass:classP
                                                attributes:attributes
                                              intendedView:intendedView
                                                 viewState:viewState
                                                  keyCache:keyCache];
                } else {
                    ckksnotice("ckksincoming", viewState.zoneID, "Received an item (%@), but our current policy claims it should be in view %@", iqe.uuid, intendedView);

                    [self _onqueueUpdateIQE:iqe withState:SecCKKSStateMismatchedView error:&error];
                    if(error) {
                        ckkserror("ckksincoming", viewState.zoneID, "Couldn't save mismatched IQE to database: %@", error);
                        self.errorItemsProcessed += 1;
                        self.error = error;
                    }

                    [self.deps.requestPolicyCheck trigger];
                }
                continue;
            }

            if([iqe.action isEqualToString: SecCKKSActionAdd] || [iqe.action isEqualToString: SecCKKSActionModify]) {
                [self _onqueueHandleIQEChange:iqe
                                   attributes:attributes
                                        class:classP
                                    viewState:viewState
                            sortedForThisView:YES
                                     keyCache:keyCache];
                [newOrChangedRecords addObject:[iqe.item CKRecordWithZoneID:viewState.zoneID]];

            } else if ([iqe.action isEqualToString: SecCKKSActionDelete]) {
                [self _onqueueHandleIQEDelete:iqe
                                        class:classP
                                    viewState:viewState];
                [deletedRecordIDs addObject:[[CKRecordID alloc] initWithRecordName:iqe.uuid zoneID:viewState.zoneID]];
            }
        }
    }

    if(newOrChangedRecords.count > 0 || deletedRecordIDs > 0) {
        // Schedule a view change notification
        [viewState.notifyViewChangedScheduler trigger];
    }

    if(self.missingKey) {
        // TODO: will be removed when the IncomingQueueOperation is part of the state machine
        [self.deps.flagHandler _onqueueHandleFlag:CKKSFlagKeyStateProcessRequested];
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
    }

    return true;
}

- (void)_onqueueHandleMismatchedViewItem:(CKKSIncomingQueueEntry*)iqe
                              secDbClass:(const SecDbClass*)secDbClass
                              attributes:(NSDictionary*)attributes
                            intendedView:(NSString* _Nullable)intendedView
                               viewState:(CKKSKeychainViewState*)viewState
                                keyCache:(CKKSMemoryKeyCache*)keyCache
{
    ckksnotice("ckksincoming", viewState.zoneID, "Received an item (%@), which should be in view %@", iqe.uuid, intendedView);

    // Here's the plan:
    //
    // If this is an add or a modify, we will execute the modification _if we do not currently have this item_.
    // Then, ask the view that should handle this item to scan.
    //
    // When, we will leave the CloudKit record in the existing 'wrong' view.
    // This will allow garbage to collect, but should prevent item loss in complicated multi-device scenarios.
    //
    // If this is a deletion, then we will inspect the other zone's current on-disk state. If it knows about the item,
    // we will ignore the deletion from this view. Otherwise, we will proceed with the deletion.
    // Note that the deletion approach already ensures that the UUID of the deleted item matches the UUID of the CKRecord.
    // This protects against an item being in multiple views, and deleted from only one.

    if([iqe.action isEqualToString:SecCKKSActionAdd] || [iqe.action isEqualToString:SecCKKSActionModify]) {
        CFErrorRef cferror = NULL;
        SecDbItemRef item = SecDbItemCreateWithAttributes(NULL, secDbClass, (__bridge CFDictionaryRef) attributes, KEYBAG_DEVICE, &cferror);

        if(!item || cferror) {
            ckkserror("ckksincoming", viewState.zoneID, "Unable to create SecDbItemRef from IQE: %@", cferror);
            return;
        }

        [self _onqueueHandleIQEChange:iqe
                                 item:item
                            viewState:viewState
                    sortedForThisView:NO
                             keyCache:keyCache];
        [self.viewsToScan addObject:intendedView];

        CFReleaseNull(item);

    } else if ([iqe.action isEqualToString:SecCKKSActionDelete]) {
        NSError* loadError = nil;

        CKRecordZoneID* otherZoneID = [[CKRecordZoneID alloc] initWithZoneName:intendedView ownerName:CKCurrentUserDefaultName];
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:iqe.uuid zoneID:otherZoneID error:&loadError];

        if(!ckme || loadError) {
            ckksnotice("ckksincoming", viewState.zoneID, "Unable to load CKKSMirrorEntry from database* %@", loadError);
            return;
        }

        if(ckme) {
            ckksnotice("ckksincoming", viewState.zoneID, "Other view (%@) already knows about this item, dropping incoming queue entry: %@", intendedView, ckme);
            NSError* saveError = nil;
            [iqe deleteFromDatabase:&saveError];
            if(saveError) {
                ckkserror("ckksincoming", viewState.zoneID, "Unable to delete IQE: %@", saveError);
            }

        } else {
            ckksnotice("ckksincoming", viewState.zoneID, "Other view (%@) does not know about this item; processing delete for %@", intendedView, iqe);
            [self _onqueueHandleIQEDelete:iqe class:secDbClass viewState:viewState];
        }

    } else {
        // We don't recognize this action. Do nothing.
    }
}

+ (NSDictionary* _Nullable)decryptCKKSItemToAttributes:(CKKSItem*)item
                                              keyCache:(CKKSMemoryKeyCache*)keyCache
                                                 error:(NSError**)error
{
    NSMutableDictionary* attributes = [[CKKSItemEncrypter decryptItemToDictionary:item keyCache:keyCache error:error] mutableCopy];
    if(!attributes) {
        return nil;
    }

    // Add the UUID (which isn't stored encrypted)
    attributes[(__bridge NSString*)kSecAttrUUID] = item.uuid;

    // Add the PCS plaintext fields, if they exist
    if(item.plaintextPCSServiceIdentifier) {
        attributes[(__bridge NSString*)kSecAttrPCSPlaintextServiceIdentifier] = item.plaintextPCSServiceIdentifier;
    }
    if(item.plaintextPCSPublicKey) {
        attributes[(__bridge NSString*)kSecAttrPCSPlaintextPublicKey] = item.plaintextPCSPublicKey;
    }
    if(item.plaintextPCSPublicIdentity) {
        attributes[(__bridge NSString*)kSecAttrPCSPlaintextPublicIdentity] = item.plaintextPCSPublicIdentity;
    }

    // This item is also synchronizable (by definition)
    [attributes setValue:@(YES) forKey:(__bridge NSString*)kSecAttrSynchronizable];

    return attributes;
}

- (bool)_onqueueUpdateIQE:(CKKSIncomingQueueEntry*)iqe
                withState:(NSString*)newState
                    error:(NSError**)error
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

- (void)main
{
    WEAKIFY(self);
    self.completionBlock = ^(void) {
        STRONGIFY(self);
        if (!self) {
            ckkserror("ckksincoming", self.deps.zoneID, "received callback for released object");
            return;
        }

        CKKSAnalytics* logger = [CKKSAnalytics logger];

        for(CKKSKeychainViewState* viewState in self.deps.zones) {
            // This will produce slightly incorrect results when processing multiple zones...
            if (!self.error) {
                [logger logSuccessForEvent:CKKSEventProcessIncomingQueueClassC zoneName:viewState.zoneID.zoneName];

                if (!self.pendingClassAEntries) {
                    [logger logSuccessForEvent:CKKSEventProcessIncomingQueueClassA zoneName:viewState.zoneID.zoneName];
                }
            } else {
                [logger logRecoverableError:self.error
                                   forEvent:self.errorOnClassAFailure ? CKKSEventProcessIncomingQueueClassA : CKKSEventProcessIncomingQueueClassC
                                   zoneName:viewState.zoneID.zoneName
                             withAttributes:NULL];
            }
        }
    };

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    for(CKKSKeychainViewState* viewState in self.deps.zones) {
        if(![self.deps.syncingPolicy isSyncingEnabledForView:viewState.zoneName]) {
            ckkserror("ckksincoming", viewState, "Item syncing for this view is disabled");
            self.nextState = self.intendedState;
            continue;
        }

        ckksnotice("ckksincoming", viewState, "Processing incoming queue");

        // First, process all item deletions.
        // Then, process all modifications and additions.
        // Therefore, if there's both a delete and a re-add of a single Primary Key item in the queue,
        // we should end up with the item still existing in tthe keychain afterward.
        // But, since we're dropping off the queue inbetween, we might accidentally tell our clients that
        // their item doesn't exist. Fixing that would take quite a bit of complexity and memory.

        BOOL success = [self loadAndProcessEntries:viewState withActionFilter:SecCKKSActionDelete];
        if(!success) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation (after processing deletes): %@", self.error);
            return;
        }

        success = [self loadAndProcessEntries:viewState withActionFilter:nil];
        if(!success) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation (after processing all incoming entries): %@", self.error);
            return;
        }

        ckksnotice("ckksincoming", viewState, "Processed %lu items in incoming queue (%lu errors)", (unsigned long)self.successfulItemsProcessed, (unsigned long)self.errorItemsProcessed);

        if(![self fixMismatchedViewItems:viewState]) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation due to failure fixing mismatched items");
            return;
        }

        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;

            NSArray<CKKSCurrentItemPointer*>* newCIPs = [CKKSCurrentItemPointer remoteItemPointers:viewState.zoneID error:&error];
            if(error || !newCIPs) {
                ckkserror("ckksincoming", viewState, "Could not load remote item pointers: %@", error);
            } else {
                if (![self processNewCurrentItemPointers:newCIPs viewState:viewState]) {
                    return CKKSDatabaseTransactionRollback;
                }
                ckksnotice("ckksincoming", viewState, "Processed %lu items in CIP queue", (unsigned long)newCIPs.count);
            }

            return CKKSDatabaseTransactionCommit;
        }];

    }

    if(self.newOutgoingEntries) {
        self.deps.currentOutgoingQueueOperationGroup = [CKOperationGroup CKKSGroupWithName:@"incoming-queue-response"];
        [self.deps.flagHandler handleFlag:CKKSFlagProcessOutgoingQueue];
    }

    if(self.pendingClassAEntries) {
        OctagonPendingFlag* whenUnlocked = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagProcessIncomingQueue
                                                                         conditions:OctagonPendingConditionsDeviceUnlocked];
        [self.deps.flagHandler handlePendingFlag:whenUnlocked];
    }

    for(NSString* viewName in self.viewsToScan) {
        CKKSKeychainView* view = [[CKKSViewManager manager] findView:viewName];
        ckksnotice("ckksincoming", self.deps.zoneID, "Requesting scan for %@ (%@)", view, viewName);
        [view scanLocalItems:@"policy-mismatch"];
    }

    self.nextState = self.intendedState;
}

- (BOOL)loadAndProcessEntries:(CKKSKeychainViewState*)viewState withActionFilter:(NSString* _Nullable)actionFilter
{
    __block bool errored = false;

    // Now for the tricky bit: take and drop the account queue for each batch of queue entries
    // This is for peak memory concerns, but also to allow keychain API clients to make changes while we're processing many items
    // Note that IncomingQueueOperations are no longer transactional: they can partially succeed. This might make them harder to reason about.
    __block NSUInteger lastCount = SecCKKSIncomingQueueItemsAtOnce;
    __block NSString* lastMaxUUID = nil;

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    while(lastCount == SecCKKSIncomingQueueItemsAtOnce) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSArray<CKKSIncomingQueueEntry*> * queueEntries = nil;
            if(self.cancelled) {
                ckksnotice("ckksincoming", viewState, "CKKSIncomingQueueOperation cancelled, quitting");
                errored = true;
                return CKKSDatabaseTransactionRollback;
            }

            NSError* error = nil;

            queueEntries = [CKKSIncomingQueueEntry fetch:SecCKKSIncomingQueueItemsAtOnce
                                          startingAtUUID:lastMaxUUID
                                                   state:SecCKKSStateNew
                                                  action:actionFilter
                                                  zoneID:viewState.zoneID
                                                   error:&error];

            if(error != nil) {
                ckkserror("ckksincoming", viewState, "Error fetching incoming queue records: %@", error);
                self.error = error;
                return CKKSDatabaseTransactionRollback;
            }

            lastCount = queueEntries.count;

            if([queueEntries count] == 0) {
                // Nothing to do! exit.
                ckksinfo("ckksincoming", viewState, "Nothing in incoming queue to process (filter: %@)", actionFilter);
                return CKKSDatabaseTransactionCommit;
            }

            [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventIncommingQueue zone:viewState.zoneID.zoneName count:[queueEntries count]];

            if (![self intransaction:viewState processQueueEntries:queueEntries]) {
                ckksnotice("ckksincoming", viewState, "processQueueEntries didn't complete successfully");
                errored = true;
                return CKKSDatabaseTransactionRollback;
            }

            // Find the highest UUID for the next fetch.
            for(CKKSIncomingQueueEntry* iqe in queueEntries) {
                lastMaxUUID = ([lastMaxUUID compare:iqe.uuid] == NSOrderedDescending) ? lastMaxUUID : iqe.uuid;
            }

            return CKKSDatabaseTransactionCommit;
        }];

        if(errored) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation");
            return false;
        }
    }

    return true;
}
- (BOOL)fixMismatchedViewItems:(CKKSKeychainViewState*)viewState
{
    if(!self.handleMismatchedViewItems) {
        return YES;
    }

    ckksnotice("ckksincoming", viewState, "Handling policy-mismatched items");
    __block NSUInteger lastCount = SecCKKSIncomingQueueItemsAtOnce;
    __block NSString* lastMaxUUID = nil;
    __block BOOL errored = NO;

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    while(lastCount == SecCKKSIncomingQueueItemsAtOnce) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;
            NSArray<CKKSIncomingQueueEntry*>* queueEntries = [CKKSIncomingQueueEntry fetch:SecCKKSIncomingQueueItemsAtOnce
                                                                            startingAtUUID:lastMaxUUID
                                                                                     state:SecCKKSStateMismatchedView
                                                                                    action:nil
                                                                                    zoneID:viewState.zoneID
                                                                                     error:&error];
            if(error) {
                ckksnotice("ckksincoming", viewState, "Cannot fetch mismatched view items");
                self.error = error;
                errored = true;
                return CKKSDatabaseTransactionRollback;
            }

            lastCount = queueEntries.count;

            if(queueEntries.count == 0) {
                ckksnotice("ckksincoming",viewState, "No mismatched view items");
                return CKKSDatabaseTransactionCommit;
            }

            ckksnotice("ckksincoming", viewState, "Inspecting %lu mismatched items", (unsigned long)queueEntries.count);

            if (![self intransaction:viewState processQueueEntries:queueEntries]) {
                ckksnotice("ckksincoming", viewState, "processQueueEntries didn't complete successfully");
                errored = true;
                return CKKSDatabaseTransactionRollback;
            }

            for(CKKSIncomingQueueEntry* iqe in queueEntries) {
                lastMaxUUID = ([lastMaxUUID compare:iqe.uuid] == NSOrderedDescending) ? lastMaxUUID : iqe.uuid;
            }

            return CKKSDatabaseTransactionCommit;
        }];
    }

    return !errored;
}

- (void)_onqueueHandleIQEChange:(CKKSIncomingQueueEntry*)iqe
                     attributes:(NSDictionary*)attributes
                          class:(const SecDbClass *)classP
                      viewState:(CKKSKeychainViewState*)viewState
              sortedForThisView:(BOOL)sortedForThisView
                       keyCache:keyCache
{
    __block CFErrorRef cferror = NULL;
    SecDbItemRef item = SecDbItemCreateWithAttributes(NULL, classP, (__bridge CFDictionaryRef) attributes, KEYBAG_DEVICE, &cferror);

     if(!item || cferror) {
         ckkserror("ckksincoming", viewState.zoneID, "Unable to make SecDbItemRef out of attributes: %@", cferror);
         return;
     }
     CFReleaseNull(cferror);

     [self _onqueueHandleIQEChange:iqe
                              item:item
                         viewState:viewState
                 sortedForThisView:sortedForThisView
                          keyCache:keyCache];

    CFReleaseNull(item);
}

- (void)_onqueueHandleIQEChange:(CKKSIncomingQueueEntry*)iqe
                           item:(SecDbItemRef)item
                      viewState:(CKKSKeychainViewState*)viewState
              sortedForThisView:(BOOL)sortedForThisView
                       keyCache:(CKKSMemoryKeyCache*)keyCache
{
    bool ok = false;
    __block CFErrorRef cferror = NULL;
    __block NSError* error = NULL;

    if(SecDbItemIsTombstone(item)) {
        ckkserror("ckksincoming", viewState.zoneID, "Rejecting a tombstone item addition from CKKS(%@): %@", iqe.uuid, item);

        NSError* error = nil;
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry withItem:item
                                                                action:SecCKKSActionDelete
                                                                zoneID:viewState.zoneID
                                                              keyCache:keyCache
                                                                 error:&error];
        [oqe saveToDatabase:&error];

        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "Unable to save new deletion OQE: %@", error);
        } else {
            [iqe deleteFromDatabase: &error];
            if(error) {
                ckkserror("ckksincoming", viewState.zoneID, "couldn't delete CKKSIncomingQueueEntry: %@", error);
                self.error = error;
                self.errorItemsProcessed += 1;
            } else {
                self.successfulItemsProcessed += 1;
            }
        }
        self.newOutgoingEntries = true;

        return;
    }

    __block NSDate* moddate = (__bridge NSDate*) CFDictionaryGetValue(item->attributes, kSecAttrModificationDate);

    ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt){
        bool replaceok = SecDbItemInsertOrReplace(item, dbt, &cferror, ^(SecDbItemRef olditem, SecDbItemRef *replace) {
            // If the UUIDs do not match, then check to be sure that the local item is known to CKKS. If not, accept the cloud value.
            // Otherwise, when the UUIDs do not match, then select the item with the 'lower' UUID, and tell CKKS to
            //   delete the item with the 'higher' UUID.
            // Otherwise, the cloud wins.

            [SecCoreAnalytics sendEvent:SecCKKSAggdPrimaryKeyConflict event:@{SecCoreAnalyticsValue: @1}];

            // Note that SecDbItemInsertOrReplace CFReleases any replace pointer it's given, so, be careful

            if(!CFDictionaryContainsKey(olditem->attributes, kSecAttrUUID)) {
                // No UUID -> no good.
                ckksnotice("ckksincoming", viewState.zoneID, "Replacing item (it doesn't have a UUID) for %@", iqe.uuid);
                if(replace) {
                    *replace = CFRetainSafe(item);
                }
                return;
            }

            // If this item arrived in what we believe to be the wrong view, drop the modification entirely.
            if(!sortedForThisView) {
                ckksnotice("ckksincoming", viewState.zoneID, "Primary key conflict; dropping CK item (arriving from wrong view) %@", item);
                return;
            }

            CFStringRef itemUUID    = CFDictionaryGetValue(item->attributes,    kSecAttrUUID);
            CFStringRef olditemUUID = CFDictionaryGetValue(olditem->attributes, kSecAttrUUID);

            // Is the old item already somewhere in CKKS?
            NSError* ckmeError = nil;
            CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:(__bridge NSString*)olditemUUID
                                                              zoneID:viewState.zoneID
                                                               error:&ckmeError];

            if(ckmeError) {
                ckksnotice("ckksincoming", viewState.zoneID, "Unable to fetch ckme: %@", ckmeError);
                // We'll just have to assume that there is a CKME, and let the comparison analysis below win
            }

            CFComparisonResult compare = CFStringCompare(itemUUID, olditemUUID, 0);
            CKKSOutgoingQueueEntry* oqe = nil;
            if (compare == kCFCompareGreaterThan && (ckme || ckmeError)) {
                // olditem wins; don't change olditem; delete item
                ckksnotice("ckksincoming", viewState.zoneID, "Primary key conflict; dropping CK item %@", item);
                oqe = [CKKSOutgoingQueueEntry withItem:item
                                                action:SecCKKSActionDelete
                                                zoneID:viewState.zoneID
                                              keyCache:keyCache
                                                 error:&error];
                [oqe saveToDatabase: &error];
                self.newOutgoingEntries = true;
                moddate = nil;
            } else {
                // item wins, either due to the UUID winning or the item not being in CKKS yet
                ckksnotice("ckksincoming", viewState.zoneID, "Primary key conflict; replacing %@%@ with CK item %@",
                           ckme ? @"" : @"non-onboarded", olditem, item);
                if(replace) {
                    *replace = CFRetainSafe(item);
                    moddate = (__bridge NSDate*) CFDictionaryGetValue(item->attributes, kSecAttrModificationDate);
                }
                // delete olditem if UUID differs (same UUID is the normal update case)
                if (compare != kCFCompareEqualTo) {
                    oqe = [CKKSOutgoingQueueEntry withItem:olditem
                                                    action:SecCKKSActionDelete
                                                    zoneID:viewState.zoneID
                                                  keyCache:keyCache
                                                     error:&error];
                    [oqe saveToDatabase: &error];
                    self.newOutgoingEntries = true;
                }
            }
        });

        // SecDbItemInsertOrReplace returns an error even when it succeeds.
        if(!replaceok && SecErrorIsSqliteDuplicateItemError(cferror)) {
            CFReleaseNull(cferror);
            replaceok = true;
        }
        return replaceok;
    });

    if(cferror) {
        ckkserror("ckksincoming", viewState.zoneID, "couldn't process item from IncomingQueue: %@", cferror);
        SecTranslateError(&error, cferror);
        self.error = error;

        iqe.state = SecCKKSStateError;
        [iqe saveToDatabase:&error];
        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "Couldn't save errored IQE to database: %@", error);
            self.error = error;
        }
        return;
    }

    if(error) {
        ckkserror("ckksincoming", viewState.zoneID, "Couldn't handle IQE, but why?: %@", error);
        self.error = error;
        return;
    }

    if(ok) {
        ckksinfo("ckksincoming", viewState.zoneID, "Correctly processed an IQE; deleting");
        [iqe deleteFromDatabase: &error];

        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "couldn't delete CKKSIncomingQueueEntry: %@", error);
            self.error = error;
            self.errorItemsProcessed += 1;
        } else {
            self.successfulItemsProcessed += 1;
        }

        if(moddate) {
            // Log the number of ms it took to propagate this change
            uint64_t delayInMS = [[NSDate date] timeIntervalSinceDate:moddate] * 1000;
            [SecCoreAnalytics sendEvent:@"com.apple.self.deps.item.propagation" event:@{
                @"time" : @(delayInMS)
            }];

        }

    } else {
        ckksnotice("ckksincoming", viewState.zoneID, "IQE not correctly processed, but why? %@ %@", error, cferror);
        self.error = error;

        iqe.state = SecCKKSStateError;
        [iqe saveToDatabase:&error];
        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "Couldn't save errored IQE to database: %@", error);
            self.error = error;
        }

        self.errorItemsProcessed += 1;
    }
}

- (void)_onqueueHandleIQEDelete:(CKKSIncomingQueueEntry*)iqe
                          class:(const SecDbClass *)classP
                      viewState:(CKKSKeychainViewState*)viewState
{
    bool ok = false;
    __block CFErrorRef cferror = NULL;
    NSError* error = NULL;
    NSDictionary* queryAttributes = @{(__bridge NSString*) kSecClass: (__bridge NSString*) classP->name,
                                      (__bridge NSString*) kSecAttrUUID: iqe.uuid,
                                      (__bridge NSString*) kSecAttrSynchronizable: @(YES)};
    ckksnotice("ckksincoming", viewState.zoneID, "trying to delete with query: %@", queryAttributes);
    Query *q = query_create_with_limit( (__bridge CFDictionaryRef) queryAttributes, NULL, kSecMatchUnlimited, NULL, &cferror);
    q->q_tombstone_use_mdat_from_item = true;

    if(cferror) {
        ckkserror("ckksincoming", viewState.zoneID, "couldn't create query: %@", cferror);
        SecTranslateError(&error, cferror);
        self.error = error;
        return;
    }

    ok = kc_with_dbt(true, &cferror, ^(SecDbConnectionRef dbt) {
        return s3dl_query_delete(dbt, q, NULL, &cferror);
    });

    if(cferror) {
        if(CFErrorGetCode(cferror) == errSecItemNotFound) {
            ckkserror("ckksincoming", viewState.zoneID, "couldn't delete item (as it's already gone); this is okay: %@", cferror);
            ok = true;
            CFReleaseNull(cferror);
        } else {
            ckkserror("ckksincoming", viewState.zoneID, "couldn't delete item: %@", cferror);
            SecTranslateError(&error, cferror);
            self.error = error;
            query_destroy(q, NULL);
            return;
        }
    }


    ok = query_notify_and_destroy(q, ok, &cferror);

    if(cferror) {
        ckkserror("ckksincoming", viewState.zoneID, "couldn't delete query: %@", cferror);
        SecTranslateError(&error, cferror);
        self.error = error;
        return;
    }

    if(ok) {
        ckksnotice("ckksincoming", viewState.zoneID, "Correctly processed an IQE; deleting");
        [iqe deleteFromDatabase: &error];

        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "couldn't delete CKKSIncomingQueueEntry: %@", error);
            self.error = error;
            self.errorItemsProcessed += 1;
        } else {
            self.successfulItemsProcessed += 1;
        }
    } else {
        ckkserror("ckksincoming", viewState.zoneID, "IQE not correctly processed, but why? %@ %@", error, cferror);
        self.error = error;
        self.errorItemsProcessed += 1;
    }
}

@end;

#endif
