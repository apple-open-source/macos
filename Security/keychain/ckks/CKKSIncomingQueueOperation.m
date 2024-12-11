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
#import "keychain/ckks/CKKSOperationDependencies.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSMemoryKeyCache.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTPersonaAdapter.h"
#import "keychain/ot/Affordance_OTConstants.h"

#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include <Security/SecItemPriv.h>

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#import <utilities/SecCoreAnalytics.h>

#if OCTAGON

@interface CKKSIncomingQueueOperation ()
@property bool newOutgoingEntries;
@property bool pendingClassAEntries;
@property NSError* pendingClassAEntriesError;

@property bool missingKey;

@property NSMutableSet<NSString*>* viewsToScan;

@property OctagonState* stateIfClassAItemsRemaining;
@end

@implementation CKKSIncomingQueueOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                           intending:(OctagonState*)intending
    pendingClassAItemsRemainingState:(OctagonState*)pendingClassAItemsState
                          errorState:(OctagonState*)errorState
           handleMismatchedViewItems:(bool)handleMismatchedViewItems
{
    if(self = [super init]) {
        _deps = dependencies;

        _intendedState = intending;
        _nextState = errorState;

        _stateIfClassAItemsRemaining = pendingClassAItemsState;

        _pendingClassAEntries = false;

        _handleMismatchedViewItems = handleMismatchedViewItems;

        _viewsToScan = [NSMutableSet set];

        // Use setter to access parent class field
        self.name = @"incoming-queue-operation";
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
                                                                   ckksOperationalDependencies:self.deps
                                                                                         error:&error];

            if(!attributes || error) {
                if([self.deps.lockStateTracker isLockedError:error]) {
                    NSError* localerror = nil;
                    ckkserror("ckksincoming", viewState.zoneID, "Keychain is locked; can't decrypt IQE %@", iqe);
                    CKKSKey* key = [CKKSKey tryFromDatabase:iqe.item.parentKeyUUID contextID:viewState.contextID zoneID:viewState.zoneID error:&localerror];


                    // If this isn't an error, make sure it gets processed later.
                    if([key.keyclass isEqualToString:SecCKKSKeyClassA]) {
                        self.pendingClassAEntries = true;
                        self.pendingClassAEntriesError = error;
                    }

                } else if ([error.domain isEqualToString:@"securityd"] && error.code == errSecItemNotFound) {
                    ckkserror("ckksincoming", viewState.zoneID, "Coudn't find key in keychain; will attempt to poke key hierarchy: %@", error)
                    self.missingKey = true;
                    self.error = error;

                } else {
                    // This is a more fatal error. Mark the item as broken.
                    ckkserror("ckksincoming", viewState.zoneID, "Couldn't decrypt IQE %@ for some reason: %@", iqe, error);

                    NSError* saveErrorError = nil;
                    iqe.state = SecCKKSStateError;
                    [iqe saveToDatabase:&saveErrorError];

                    if(saveErrorError) {
                        ckkserror("ckksincoming", viewState.zoneID, "Couldn't save IQE %@ as error for some reason: %@", iqe, saveErrorError);
                    }

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
            CFReleaseNull(cferror);
            return;
        }
        CFReleaseNull(cferror);

        BOOL itemWritten = [self _onqueueHandleIQEChange:iqe
                                                    item:item
                                               viewState:viewState
                                       sortedForThisView:NO
                                                keyCache:keyCache];
        if(itemWritten) {
            ckksnotice("ckksincoming", viewState.zoneID, "Wrote a keychain item that is actually for %@; requesting scan", intendedView);
            [self.viewsToScan addObject:intendedView];
        }

        CFReleaseNull(item);

    } else if ([iqe.action isEqualToString:SecCKKSActionDelete]) {
        NSError* loadError = nil;

        CKRecordZoneID* otherZoneID = [[CKRecordZoneID alloc] initWithZoneName:intendedView ownerName:CKCurrentUserDefaultName];
        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:iqe.uuid
                                                       contextID:self.deps.contextID
                                                          zoneID:otherZoneID
                                                           error:&loadError];

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
                           ckksOperationalDependencies:(CKKSOperationDependencies*)ckksOperationalDependencies
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
#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    NSSet<CKKSKeychainViewState*>* viewsToProcess = [self.deps readyAndSyncingViews];

    ckkserror_global("ckksincoming", "Going to process the incoming queues for %@", viewsToProcess);

    if(self.handleMismatchedViewItems) {
        ckkserror_global("ckksincoming", "Will handle mismatched view items along the way");
    }

    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldMissingKey: @(NO),
                                                                                                 kSecurityRTCFieldPendingClassA: @(NO),
                                                                                                 kSecurityRTCFieldNumViews: @(viewsToProcess.count)}
                                                                                       altDSID:self.deps.activeAccount.altDSID
                                                                                     eventName:kSecurityRTCEventNameProcessIncomingQueue
                                                                               testsAreEnabled:SecCKKSTestsEnabled()
                                                                                      category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                    sendMetric:self.deps.sendMetric];
    
    AAFAnalyticsEventSecurity *loadAndProcessIQEsEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldNumViews:@(viewsToProcess.count)}
                                                                                                         altDSID:self.deps.activeAccount.altDSID
                                                                                                       eventName:kSecurityRTCEventNameLoadAndProcessIQEs
                                                                                                 testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                        category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                      sendMetric:self.deps.sendMetric];
    
    [self.deps.overallLaunch addEvent:@"incoming-processing-begin"];

    long totalQueueEntries = 0;
    
    for(CKKSKeychainViewState* viewState in viewsToProcess) {
        [viewState.launch addEvent:@"incoming-processing-begin"];

        // First, process all item deletions.
        // Then, process all modifications and additions.
        // Therefore, if there's both a delete and a re-add of a single Primary Key item in the queue,
        // we should end up with the item still existing in the keychain afterward.
        // But, since we're dropping off the queue inbetween, we might accidentally tell our clients that
        // their item doesn't exist. Fixing that would take quite a bit of complexity and memory.

        long deletions = 0;
        long modifications = 0;

        BOOL success = [self loadAndProcessEntries:viewState withActionFilter:SecCKKSActionDelete totalQueueEntries:&deletions];
        totalQueueEntries += deletions;
        
        if(!success) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation (after processing deletes): %@", self.error);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:loadAndProcessIQEsEventS success:NO error:self.error];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
            return;
        }

        success = [self loadAndProcessEntries:viewState withActionFilter:nil totalQueueEntries:&modifications];
        totalQueueEntries += modifications;
        
        if(!success) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation (after processing all incoming entries): %@", self.error);
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:loadAndProcessIQEsEventS success:NO error:self.error];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
            return;
        }

        if(self.successfulItemsProcessed > 0 || self.errorItemsProcessed > 0) {
            ckksnotice("ckksincoming", viewState, "Processed %lu items in incoming queue (%lu errors)", (unsigned long)self.successfulItemsProcessed, (unsigned long)self.errorItemsProcessed);
        }
        
        if(![self fixMismatchedViewItems:viewState]) {
            ckksnotice("ckksincoming", viewState, "Early-exiting from IncomingQueueOperation due to failure fixing mismatched items");
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:loadAndProcessIQEsEventS success:YES error:nil];
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:self.error];
            return;
        }

        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;

            NSArray<CKKSCurrentItemPointer*>* newCIPs = [CKKSCurrentItemPointer remoteItemPointers:viewState.zoneID
                                                                                         contextID:viewState.contextID
                                                                                             error:&error];
            if(error || !newCIPs) {
                ckkserror("ckksincoming", viewState, "Could not load remote item pointers: %@", error);
            } else {
                if (![self processNewCurrentItemPointers:newCIPs viewState:viewState]) {
                    return CKKSDatabaseTransactionRollback;
                }

                if(newCIPs.count > 0) {
                    ckksnotice("ckksincoming", viewState, "Processed %lu items in CIP queue", (unsigned long)newCIPs.count);
                }
            }

            return CKKSDatabaseTransactionCommit;
        }];

    }

    // Average out number of queue entries processed per view
    int avgCKRecords = (viewsToProcess.count == 0) ? 0 : (int)totalQueueEntries / viewsToProcess.count;
    [loadAndProcessIQEsEventS addMetrics:@{
        kSecurityRTCFieldAvgCKRecords:@(avgCKRecords),
        kSecurityRTCFieldTotalCKRecords:@(totalQueueEntries)
    }];
    [SecurityAnalyticsReporterRTC sendMetricWithEvent:loadAndProcessIQEsEventS success:YES error:nil];
    
    if(self.newOutgoingEntries) {
        self.deps.currentOutgoingQueueOperationGroup = [CKOperationGroup CKKSGroupWithName:@"incoming-queue-response"];
        [self.deps.flagHandler handleFlag:CKKSFlagProcessOutgoingQueue];
    }

    if(self.missingKey) {
        self.nextState = CKKSStateProcessReceivedKeys;

    } else if(self.pendingClassAEntries) {
        OctagonPendingFlag* whenUnlocked = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagProcessIncomingQueue
                                                                         conditions:OctagonPendingConditionsDeviceUnlocked];
        [self.deps.flagHandler handlePendingFlag:whenUnlocked];

        self.error = self.pendingClassAEntriesError;
        self.nextState = self.stateIfClassAItemsRemaining;
    } else {
        self.nextState = self.intendedState;
    }

    if(self.viewsToScan.count > 0) {
        ckksnotice_global("ckksincoming", "Requesting scan for %@", self.viewsToScan);
        [self.deps.flagHandler handleFlag:CKKSFlagScanLocalItems];
    }

    CKKSAnalytics* logger = [CKKSAnalytics logger];

    for(CKKSKeychainViewState* viewState in viewsToProcess) {
        [viewState.launch addEvent:@"incoming-processed"];

        // This will produce slightly incorrect results when processing multiple zones...
        if (!self.error) {
            [logger logSuccessForEvent:CKKSEventProcessIncomingQueueClassC zoneName:viewState.zoneID.zoneName];

            if (!self.pendingClassAEntries) {
                [logger logSuccessForEvent:CKKSEventProcessIncomingQueueClassA zoneName:viewState.zoneID.zoneName];
            }
        } else {
            [logger logRecoverableError:self.error
                               forEvent:CKKSEventProcessIncomingQueueClassC
                               zoneName:viewState.zoneID.zoneName
                         withAttributes:NULL];

            // Don't log a "this device is locked" error as a Class A failure.
            if(![self.deps.lockStateTracker isLockedError:self.error]) {
                [logger logRecoverableError:self.error
                                   forEvent:CKKSEventProcessIncomingQueueClassA
                                   zoneName:viewState.zoneID.zoneName
                             withAttributes:NULL];
            }
            
        }
    }

    int avgSuccessfulItemsProcessedPerView = (self.deps.activeManagedViews.count == 0) ? 0 : (int)self.successfulItemsProcessed / self.deps.activeManagedViews.count;
    int avgErrorItemsProcessedPerView = (self.deps.activeManagedViews.count == 0) ? 0 : (int)self.errorItemsProcessed / self.deps.activeManagedViews.count;
    
    [eventS addMetrics:@{kSecurityRTCFieldPendingClassA: @(self.pendingClassAEntries),
                         kSecurityRTCFieldMissingKey: @(self.missingKey),
                         kSecurityRTCFieldAvgSuccessfulItemsProcessed: @(avgSuccessfulItemsProcessedPerView),
                         kSecurityRTCFieldAvgErrorItemsProcessed: @(avgErrorItemsProcessedPerView),
                         kSecurityRTCFieldSuccessfulItemsProcessed: @(self.successfulItemsProcessed),
                         kSecurityRTCFieldErrorItemsProcessed: @(self.errorItemsProcessed)
                       }];
    [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:!(self.pendingClassAEntries || self.missingKey) error:self.error];

    [self.deps.overallLaunch addEvent:@"incoming-processing-complete"];
}

- (BOOL)loadAndProcessEntries:(CKKSKeychainViewState*)viewState
             withActionFilter:(NSString* _Nullable)actionFilter
            totalQueueEntries:(long*)totalQueueEntries
{
    __block bool errored = false;

    // Now for the tricky bit: take and drop the account queue for each batch of queue entries
    // This is for peak memory concerns, but also to allow keychain API clients to make changes while we're processing many items
    // Note that IncomingQueueOperations are no longer transactional: they can partially succeed. This might make them harder to reason about.
    __block NSUInteger lastCount = SecCKKSIncomingQueueItemsAtOnce;
    __block NSString* lastMaxUUID = nil;

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;
    __block long numQueueEntries = 0;

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
                                               contextID:self.deps.contextID
                                                  zoneID:viewState.zoneID
                                                   error:&error];

            if(error != nil) {
                ckkserror("ckksincoming", viewState, "Error fetching incoming queue records: %@", error);
                self.error = error;
                return CKKSDatabaseTransactionRollback;
            }

            lastCount = queueEntries.count;
            numQueueEntries += queueEntries.count;
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
            *totalQueueEntries = numQueueEntries;
            return false;
        }
    }

    *totalQueueEntries = numQueueEntries;
    return true;
}
- (BOOL)fixMismatchedViewItems:(CKKSKeychainViewState*)viewState
{
    if(!self.handleMismatchedViewItems) {
        return YES;
    }

    AAFAnalyticsEventSecurity *fixMismatchedViewItemsEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{}
                                                                                                             altDSID:self.deps.activeAccount.altDSID
                                                                                                           eventName:kSecurityRTCEventNameFixMismatchedViewItems
                                                                                                     testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                            category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                          sendMetric:self.deps.sendMetric];

    ckksnotice("ckksincoming", viewState, "Handling policy-mismatched items");
    __block NSUInteger lastCount = SecCKKSIncomingQueueItemsAtOnce;
    __block NSString* lastMaxUUID = nil;
    __block BOOL errored = NO;
    __block long numMismatchedItems = 0;
    
    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    while(lastCount == SecCKKSIncomingQueueItemsAtOnce) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;
            NSArray<CKKSIncomingQueueEntry*>* queueEntries = [CKKSIncomingQueueEntry fetch:SecCKKSIncomingQueueItemsAtOnce
                                                                            startingAtUUID:lastMaxUUID
                                                                                     state:SecCKKSStateMismatchedView
                                                                                    action:nil
                                                                                 contextID:self.deps.contextID
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
            numMismatchedItems += queueEntries.count;
            
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

    if (numMismatchedItems > 0) {
        [fixMismatchedViewItemsEventS addMetrics:@{kSecurityRTCFieldNumMismatchedItems:@(numMismatchedItems)}];
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:fixMismatchedViewItemsEventS success:!errored error:self.error];
    }

    return !errored;
}

- (void)_onqueueGenerateNewUUIDPersistentRefOnSecItem:(SecDbItemRef)item viewState:(CKKSKeychainViewState*)viewState {
    CFUUIDRef prefUUID = CFUUIDCreate(kCFAllocatorDefault);
    CFUUIDBytes uuidBytes = CFUUIDGetUUIDBytes(prefUUID);
    CFDataRef uuidData = CFDataCreate(kCFAllocatorDefault, (const void *)&uuidBytes, sizeof(uuidBytes));
    CFErrorRef setError = NULL;
    SecDbItemSetPersistentRef(item, uuidData, &setError);
    ckksinfo("ckksincoming", viewState.zoneID, "set a new persistentref UUID for item %@: %@", item, setError);
    CFReleaseNull(prefUUID);
    CFReleaseNull(uuidData);
    CFReleaseNull(setError);
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

    if (SecKeychainIsStaticPersistentRefsEnabled()) {
        [self _onqueueGenerateNewUUIDPersistentRefOnSecItem:item viewState:viewState];
    }

     if(!item || cferror) {
         ckkserror("ckksincoming", viewState.zoneID, "Unable to make SecDbItemRef out of attributes: %@", cferror);
         CFReleaseNull(cferror);
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

// return YES if any keychain item change was written to disk, NO otherwise (i.e. this change was dropped)
- (BOOL)_onqueueHandleIQEChange:(CKKSIncomingQueueEntry*)iqe
                           item:(SecDbItemRef)item
                      viewState:(CKKSKeychainViewState*)viewState
              sortedForThisView:(BOOL)sortedForThisView
                       keyCache:(CKKSMemoryKeyCache*)keyCache
{
    bool ok = false;
    __block CFErrorRef cferror = NULL;
    __block NSError* error = nil;

    if(SecDbItemIsTombstone(item)) {
        ckkserror("ckksincoming", viewState.zoneID, "Rejecting a tombstone item addition from CKKS(%@): " SECDBITEM_FMT, iqe.uuid, item);

        NSError* error = nil;
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry withItem:item
                                                                action:SecCKKSActionDelete
                                                             contextID:self.deps.contextID
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

        return NO;
    }

    if(!SecDbItemIsPrimaryUserItem(item)) {
        /*
         * CKKS never intended to sync multiple users in one account's CK zone. In case any previous-OS devices did or does upload
         * such an item, we want to delete the CK record, to maintain the semantics of the CK zones.
         *
         * But, we definitely do not want the uploading device to suddenly delete the multiuser item as a result of this: that'd look
         * like a data loss bug on the multiuser keychain for that device. Just deleting the CK Record will be treated as a deletion of
         * the item.
         *
         * Fortunately, previous OSes did not properly implement multiuser item deletion. When they constructed the query to find and
         * delete the keychain item, they only will find and delete items for the primary user, not any multiuser items. So, after we
         * delete the CKRecord, the remote device _will_ treat it as an incoming delete request, but will mishandle that delete request,
         * and so will not delete the multiuser item of interest.
         */

        ckkserror("ckksincoming", viewState.zoneID, "Rejecting a multiuser item addition from CKKS(%@): " SECDBITEM_FMT, iqe.uuid, item);
        NSError* error = nil;
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry withItem:item
                                                                action:SecCKKSActionDelete
                                                             contextID:self.deps.contextID
                                                                zoneID:viewState.zoneID
                                                              keyCache:keyCache
                                                                 error:&error];
        [oqe saveToDatabase:&error];

        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "Unable to save new deletion OQE: %@", error);
        } else {
            [iqe deleteFromDatabase:&error];
            if(error) {
                ckkserror("ckksincoming", viewState.zoneID, "couldn't delete CKKSIncomingQueueEntry: %@", error);
                self.error = error;
                self.errorItemsProcessed += 1;
            } else {
                self.successfulItemsProcessed += 1;
            }
        }
        self.newOutgoingEntries = true;

        return NO;
    }
#if TARGET_OS_TV
    //now set the item's musr value
    CFErrorRef setError = NULL;
    bool setResult = SecDbItemSetValueWithName(item, kSecAttrMultiUser, (__bridge CFDataRef)self.deps.keychainMusrForCurrentAccount, &setError);
    if (!setResult || setError) {
        ckkserror("ckksincoming", viewState.zoneID, "Unable to set musr %@ on item " SECDBITEM_FMT ", error: %@", self.deps.keychainMusrForCurrentAccount, item, setError);
        if (setError) {
            self.error = (NSError*) CFBridgingRelease(setError);
        } else {
            self.error = [NSError errorWithDomain:@"securityd"
                                             code:errSecInternalError
                                         userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"Failed to set musr on item"]}];
        }
        self.errorItemsProcessed += 1;
        return NO;
    }
#endif
    
    __block BOOL conflictedIncorrectlySortedItem = NO;
    __block BOOL keptOldItem = NO;
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
                ckksnotice("ckksincoming", viewState.zoneID, "Primary key conflict; dropping CK item (arriving from wrong view) " SECDBITEM_FMT, item);
                conflictedIncorrectlySortedItem = YES;
                return;
            }

            CFStringRef itemUUID    = CFDictionaryGetValue(item->attributes,    kSecAttrUUID);
            CFStringRef olditemUUID = CFDictionaryGetValue(olditem->attributes, kSecAttrUUID);

            // Is the old item already somewhere in CKKS?
            NSError* ckmeError = nil;
            CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:(__bridge NSString*)olditemUUID
                                                           contextID:self.deps.contextID
                                                              zoneID:viewState.zoneID
                                                               error:&ckmeError];

            if(ckmeError) {
                ckksnotice("ckksincoming", viewState.zoneID, "Unable to fetch ckme for old item %@: %@", olditem, ckmeError);
                // We'll just have to assume that there is a CKME, and let the comparison analysis below win
            }

            CFComparisonResult compare = CFStringCompare(itemUUID, olditemUUID, 0);
            CKKSOutgoingQueueEntry* oqe = nil;
            if (compare == kCFCompareGreaterThan && (ckme || ckmeError)) {
                // olditem wins; don't change olditem; delete incoming item; re-affirm existence of olditem
                ckksnotice("ckksincoming", viewState.zoneID, "Primary key conflict; deleting incoming CK item (%@)" SECDBITEM_FMT "in favor of old item (%@)" SECDBITEM_FMT , itemUUID, item, olditemUUID, olditem);
                oqe = [CKKSOutgoingQueueEntry withItem:item
                                                action:SecCKKSActionDelete
                                             contextID:self.deps.contextID
                                                zoneID:viewState.zoneID
                                              keyCache:keyCache
                                                 error:&error];
                [oqe saveToDatabase: &error];
                self.newOutgoingEntries = true;
                moddate = nil;

                CKKSOutgoingQueueEntry* oldItemOQE = [CKKSOutgoingQueueEntry withItem:olditem
                                                                               action:SecCKKSActionAdd
                                                                            contextID:self.deps.contextID
                                                                               zoneID:viewState.zoneID
                                                                             keyCache:keyCache
                                                                                error:&error];
                [oldItemOQE saveToDatabase: &error];
                keptOldItem = YES;
            } else {
                // item wins, either due to the new UUID winning or the olditem not being in CKKS yet
                ckksnotice("ckksincoming", viewState.zoneID, "Primary key conflict; replacing %@ with CK item",
                           ckme ? @"" : @"non-onboarded");
                if(replace) {
                    *replace = CFRetainSafe(item);
                    moddate = (__bridge NSDate*) CFDictionaryGetValue(item->attributes, kSecAttrModificationDate);
                }
                // delete olditem if UUID differs (same UUID is the normal update case)
                if (compare != kCFCompareEqualTo) {
                    ckksnotice("ckksincoming", viewState.zoneID, "UUID of olditem (%@) is higher than UUID of incoming item (%@), issuing deletion of olditem: " SECDBITEM_FMT, olditemUUID, itemUUID, olditem);
                    oqe = [CKKSOutgoingQueueEntry withItem:olditem
                                                    action:SecCKKSActionDelete
                                                 contextID:self.deps.contextID
                                                    zoneID:viewState.zoneID
                                                  keyCache:keyCache
                                                     error:&error];
                    [oqe saveToDatabase: &error];
                    self.newOutgoingEntries = true;
                }
            }
            if (SecKeychainIsStaticPersistentRefsEnabled()) {
                //now evaluate UUID based persistent refs
                NSData* oldItemPersistentRef = (__bridge NSData*) SecDbItemGetCachedValueWithName(olditem, kSecAttrPersistentReference);

                if (replace && *replace == nil && (!oldItemPersistentRef || [oldItemPersistentRef length] != PERSISTENT_REF_UUID_BYTES_LENGTH)) {
                    [self _onqueueGenerateNewUUIDPersistentRefOnSecItem:olditem viewState:viewState];
                    if (replace) {
                        *replace = CFRetainSafe(olditem);
                    }
                } else if (replace && *replace != NULL && oldItemPersistentRef && [oldItemPersistentRef length] == PERSISTENT_REF_UUID_BYTES_LENGTH) {
                    CFErrorRef setError = NULL;
                    SecDbItemSetPersistentRef(*replace, (__bridge CFDataRef)oldItemPersistentRef, &setError);
                    if (setError) {
                        ckksnotice("ckksincoming", viewState.zoneID, "error setting uuid persistent ref: %@", setError);
                        CFReleaseNull(setError);
                    }
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

    if(!ok || cferror) {
        ckkserror("ckksincoming", viewState.zoneID, "couldn't process item from IncomingQueue: %@", cferror);
        if (cferror) {
            SecTranslateError(&error, cferror);
            self.error = error;
        } else {
            self.error = [NSError errorWithDomain:@"securityd"
                                             code:errSecInternalError
                                         userInfo:@{NSLocalizedDescriptionKey : @"kc_with_dbt failed without error"}];
        }

        iqe.state = SecCKKSStateError;
        [iqe saveToDatabase:&error];
        if(error) {
            ckkserror("ckksincoming", viewState.zoneID, "Couldn't save errored IQE to database: %@", error);
            self.error = error;
        }
        return NO;
    }

    if(error) {
        ckkserror("ckksincoming", viewState.zoneID, "Couldn't handle IQE, but why?: %@", error);
        self.error = error;
        return NO;
    }

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
        [SecCoreAnalytics sendEvent:@"com.apple.ckks.item.propagation" event:@{
                @"time" : @(delayInMS),
                    @"view" : viewState.zoneID.zoneName,
                    }];
    }

    // If we ignored the update, return NO. Otherwise, return YES.
    if(conflictedIncorrectlySortedItem || keptOldItem) {
        return NO;
    } else {
        return YES;
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
            self.errorItemsProcessed += 1;
            self.error = error;
            query_destroy(q, NULL);
            return;
        }
    }


    ok = query_notify_and_destroy(q, ok, &cferror);

    if(!ok || cferror) {
        ckkserror("ckksincoming", viewState.zoneID, "couldn't delete query: %@", cferror);
        if (cferror) {
            SecTranslateError(&error, cferror);
            self.error = error;
        } else {
            self.error = [NSError errorWithDomain:@"securityd"
                                             code:errSecInternalError
                                         userInfo:@{NSLocalizedDescriptionKey : @"query_notify_and_destroy failed without error"}];
        }
        return;
    }
    ckksnotice("ckksincoming", viewState.zoneID, "Correctly processed an IQE; deleting");
    [iqe deleteFromDatabase: &error];

    if(error) {
        ckkserror("ckksincoming", viewState.zoneID, "couldn't delete CKKSIncomingQueueEntry: %@", error);
        self.error = error;
        self.errorItemsProcessed += 1;
    } else {
        self.successfulItemsProcessed += 1;
    }
}

@end;

#endif
