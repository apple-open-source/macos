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

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/ckks/CKKSItemEncrypter.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueOperation.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/ObjCImprovements.h"

#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include <Security/SecItemPriv.h>
#import "CKKSPowerCollection.h"
#import "utilities/SecCoreAnalytics.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#define OQEDELAY SecCKKSTestsEnabled() ? 10 : 30*60

@interface CKKSOutgoingQueueOperation ()
@property OctagonState* ckErrorState;
@end

@implementation CKKSOutgoingQueueOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                           intending:(OctagonState*)intending
                        ckErrorState:(OctagonState*)ckErrorState
                          errorState:(OctagonState*)errorState
{
    if(self = [super init]) {
        _deps = dependencies;

        _nextState = errorState;
        _intendedState = intending;
        _ckErrorState = ckErrorState;
    }
    return self;
}

+ (NSArray<CKKSOutgoingQueueEntry*>* _Nullable)ontransactionFetchEntries:(CKKSKeychainViewState*)viewState
                                                               contextID:(NSString*)contextID
                                                                   error:(NSError**)error
{
    NSSet<NSString*>* priorityUUIDs = [[CKKSViewManager manager] pendingCallbackUUIDs];

    NSMutableArray<CKKSOutgoingQueueEntry*>* priorityEntries = [NSMutableArray array];
    NSMutableSet<NSString*>* priorityEntryUUIDs = [NSMutableSet set];

    for(NSString* uuid in priorityUUIDs) {
        NSError* priorityFetchError = nil;
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:uuid
                                                                    contextID:contextID
                                                                       zoneID:viewState.zoneID
                                                                        error:&priorityFetchError];

        if(priorityFetchError) {
            ckkserror("ckksoutgoing", viewState.zoneID, "Unable to fetch priority uuid %@: %@", uuid, priorityFetchError);
            continue;
        }

        if(!oqe) {
            continue;
        }

        if(![oqe.state isEqualToString:SecCKKSStateNew]) {
            ckksnotice("ckksoutgoing", viewState.zoneID, "Priority uuid %@ is not in 'new': %@", uuid, oqe);
            continue;
        }

        if(oqe) {
            ckksnotice("ckksoutgoing", viewState.zoneID, "Found OQE  to fetch priority uuid %@: %@", uuid, priorityFetchError);
            [priorityEntries addObject:oqe];
            [priorityEntryUUIDs addObject:oqe.uuid];
        }
    }

    // We only actually care about queue items in the 'new' state
    NSArray<CKKSOutgoingQueueEntry*> * newEntries = [CKKSOutgoingQueueEntry fetch:SecCKKSOutgoingQueueItemsAtOnce
                                                                            state:SecCKKSStateNew
                                                                        contextID:contextID
                                                                           zoneID:viewState.zoneID
                                                                            error:error];
    if(!newEntries) {
        return nil;
    }

    // Build our list of oqes to transmit
    NSMutableArray<CKKSOutgoingQueueEntry*>* queueEntries = [priorityEntries mutableCopy];
    for(CKKSOutgoingQueueEntry* oqe in newEntries) {
        if(queueEntries.count >= SecCKKSOutgoingQueueItemsAtOnce) {
            break;
        } else {
            // We only need to add this OQE if it wasn't already there
            if(![priorityEntryUUIDs containsObject:oqe.uuid]) {
                [queueEntries addObject:oqe];
            }
        }
    }

    return queueEntries;
}

- (void)groupStart
{
    WEAKIFY(self);

#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    NSHashTable<CKModifyRecordsOperation*>* ckWriteOperations = [NSHashTable weakObjectsHashTable];

    [self.deps.overallLaunch addEvent:@"process-outgoing-queue-begin"];

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        if (self.deps.syncingPolicy.isInheritedAccount || ![self.deps.syncingPolicy isSyncingEnabledForView:viewState.zoneID.zoneName]) {
            ckksnotice("ckksoutgoing", viewState, "Item syncing for this view is disabled");
            continue;
        }

        AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{}
                                                                                           altDSID:self.deps.activeAccount.altDSID
                                                                                         eventName:kSecurityRTCEventNameProcessOutgoingQueue
                                                                                   testsAreEnabled:SecCKKSTestsEnabled()
                                                                                          category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                        sendMetric:self.deps.sendMetric];

        // Each zone should get its own transaction, in case errors cause us to roll back queue modifications after marking entries as in-flight
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;

            NSArray<CKKSOutgoingQueueEntry*>* queueEntries = [CKKSOutgoingQueueOperation ontransactionFetchEntries:viewState
                                                                                                         contextID:self.deps.contextID
                                                                                                             error:&error];

            if(!queueEntries || error) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Error fetching outgoing queue records: %@", error);
                self.error = error;
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];
                return CKKSDatabaseTransactionRollback;
            }

            bool fullUpload = queueEntries.count >= SecCKKSOutgoingQueueItemsAtOnce;
            [eventS addMetrics:@{kSecurityRTCFieldNumKeychainItems:@(queueEntries.count), kSecurityRTCFieldIsFullUpload:@(fullUpload)}];

            [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventOutgoingQueue zone:viewState.zoneID.zoneName count:[queueEntries count]];

            ckksinfo("ckksoutgoing", viewState.zoneID, "processing outgoing queue: %@", queueEntries);

            NSMutableDictionary<CKRecordID*, CKRecord*>* recordsToSave = [[NSMutableDictionary alloc] init];
            NSMutableSet<CKRecordID*>* recordIDsModified = [[NSMutableSet alloc] init];
            NSMutableSet<CKKSOutgoingQueueEntry*>*oqesModified = [[NSMutableSet alloc] init];
            NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];

            CKKSCurrentKeyPointer* currentClassAKeyPointer = [CKKSCurrentKeyPointer fromDatabase:SecCKKSKeyClassA
                                                                                       contextID:viewState.contextID
                                                                                          zoneID:viewState.zoneID
                                                                                           error:&error];
            CKKSCurrentKeyPointer* currentClassCKeyPointer = [CKKSCurrentKeyPointer fromDatabase:SecCKKSKeyClassC
                                                                                       contextID:viewState.contextID
                                                                                         zoneID:viewState.zoneID
                                                                                           error:&error];
            NSMutableDictionary<CKKSKeyClass*, CKKSCurrentKeyPointer*>* currentKeysToSave = [[NSMutableDictionary alloc] init];
            bool needsReencrypt = false;

            if(error != nil) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't load current class keys: %@", error);
                self.error = error;
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:error];

                return CKKSDatabaseTransactionRollback;
            }

            for(CKKSOutgoingQueueEntry* oqe in queueEntries) {
                @autoreleasepool {
                    CKKSOutgoingQueueEntry* inflight = [CKKSOutgoingQueueEntry tryFromDatabase:oqe.uuid
                                                                                         state:SecCKKSStateInFlight
                                                                                     contextID:self.deps.contextID
                                                                                        zoneID:viewState.zoneID
                                                                                         error:&error];
                    if(!error && inflight) {
                        // There is an inflight request with this UUID. Leave this request in-queue until CloudKit returns and we resolve the inflight request.
                        continue;
                    }

                    // If this item is not a delete, check the encryption status of this item.
                    if(![oqe.action isEqualToString: SecCKKSActionDelete]) {
                        // Check if this item is encrypted under a current key
                        if([oqe.item.parentKeyUUID isEqualToString: currentClassAKeyPointer.currentKeyUUID]) {
                            // Excellent.
                            currentKeysToSave[SecCKKSKeyClassA] = currentClassAKeyPointer;
                        } else if([oqe.item.parentKeyUUID isEqualToString: currentClassCKeyPointer.currentKeyUUID]) {
                            // Works for us!
                            currentKeysToSave[SecCKKSKeyClassC] = currentClassCKeyPointer;
                        } else {
                            // This item is encrypted under an old key. Set it up for reencryption and move on.
                            ckksnotice("ckksoutgoing", viewState.zoneID, "Item's encryption key (%@ %@) is neither %@ or %@", oqe, oqe.item.parentKeyUUID, currentClassAKeyPointer, currentClassCKeyPointer);
                            [oqe intransactionMoveToState:SecCKKSStateReencrypt viewState:viewState error:&error];
                            if(error) {
                                ckkserror("ckksoutgoing", viewState.zoneID, "couldn't save oqe to database: %@", error);
                                self.error = error;
                                error = nil;
                            }
                            needsReencrypt = true;
                            continue;
                        }
                    }

                    if([oqe.action isEqualToString: SecCKKSActionAdd]) {
                        CKRecord* record = [oqe.item CKRecordWithZoneID:viewState.zoneID];
                        recordsToSave[record.recordID] = record;
                        [recordIDsModified addObject: record.recordID];
                        [oqesModified addObject:oqe];

                        [oqe intransactionMoveToState:SecCKKSStateInFlight viewState:viewState error:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", viewState.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                            self.error = error;
                        }

                    } else if ([oqe.action isEqualToString: SecCKKSActionDelete]) {
                        CKRecordID* recordIDToDelete = [[CKRecordID alloc] initWithRecordName:oqe.item.uuid zoneID:viewState.zoneID];
                        [recordIDsToDelete addObject: recordIDToDelete];
                        [recordIDsModified addObject: recordIDToDelete];
                        [oqesModified addObject:oqe];

                        [oqe intransactionMoveToState:SecCKKSStateInFlight viewState:viewState error:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", viewState.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                        }

                    } else if ([oqe.action isEqualToString: SecCKKSActionModify]) {
                        // Load the existing item from the ckmirror.
                        CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:oqe.item.uuid
                                                                       contextID:self.deps.contextID
                                                                          zoneID:viewState.zoneID
                                                                           error:&error];
                        if(!ckme) {
                            // This is a problem: we have an update to an item that doesn't exist.
                            // Either: an Add operation we launched failed due to a CloudKit error (conflict?) and this is a follow-on update
                            //     Or: ?
                            ckkserror("ckksoutgoing", viewState.zoneID, "update to a record that doesn't exist? %@", oqe.item.uuid);
                            // treat as an add.
                            CKRecord* record = [oqe.item CKRecordWithZoneID:viewState.zoneID];
                            recordsToSave[record.recordID] = record;
                            [recordIDsModified addObject: record.recordID];
                            [oqesModified addObject:oqe];

                            [oqe intransactionMoveToState:SecCKKSStateInFlight viewState:viewState error:&error];
                            if(error) {
                                ckkserror("ckksoutgoing", viewState.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                                self.error = error;
                            }
                        } else {
                            if(![oqe.item.storedCKRecord.recordChangeTag isEqual: ckme.item.storedCKRecord.recordChangeTag]) {
                                // The mirror entry has updated since this item was created. If we proceed, we might end up with
                                // a badly-authenticated record.
                                ckksnotice("ckksoutgoing", viewState.zoneID, "Record (%@)'s change tag doesn't match ckmirror's change tag, reencrypting", oqe);
                                [oqe intransactionMoveToState:SecCKKSStateReencrypt viewState:viewState error:&error];
                                if(error) {
                                    ckkserror("ckksoutgoing", viewState.zoneID, "couldn't save oqe to database: %@", error);
                                    self.error = error;
                                    error = nil;
                                }
                                needsReencrypt = true;
                                continue;
                            }
                            // Grab the old ckrecord and update it
                            CKRecord* record = [oqe.item updateCKRecord: ckme.item.storedCKRecord zoneID: viewState.zoneID];
                            recordsToSave[record.recordID] = record;
                            [recordIDsModified addObject: record.recordID];
                            [oqesModified addObject:oqe];

                            [oqe intransactionMoveToState:SecCKKSStateInFlight viewState:viewState error:&error];
                            if(error) {
                                ckkserror("ckksoutgoing", viewState.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                            }
                        }
                    }
                }
            }

            if(needsReencrypt) {
                ckksnotice("ckksoutgoing", viewState.zoneID, "An item needs reencryption!");
                [self.deps.flagHandler _onqueueHandleFlag:CKKSFlagItemReencryptionNeeded];
            }

            if([recordsToSave count] == 0 && [recordIDsToDelete count] == 0) {
                // Nothing to do! exit.
                ckksinfo("ckksoutgoing", viewState.zoneID, "Nothing in outgoing queue to process");
                if(self.deps.currentOutgoingQueueOperationGroup) {
                    ckksinfo("ckksoutgoing", viewState.zoneID, "End of operation group: %@", self.deps.currentOutgoingQueueOperationGroup);
                    self.deps.currentOutgoingQueueOperationGroup = nil;
                }

                [eventS addMetrics:@{kSecurityRTCFieldItemsToAdd:@0, kSecurityRTCFieldItemsToDelete:@0, kSecurityRTCFieldItemsToModify:@0, kSecurityRTCFieldErrorItemsProcessed:@0, kSecurityRTCFieldSuccessfulItemsProcessed:@0}];
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];

                self.nextState = self.intendedState;
                return true;
            }

            self.itemsProcessed = recordsToSave.count;

            ckksinfo("ckksoutgoing", viewState.zoneID, "Current keys to update: %@", currentKeysToSave);
            for(CKKSCurrentKeyPointer* keypointer in currentKeysToSave.allValues) {
                CKRecord* record = [keypointer CKRecordWithZoneID:viewState.zoneID];
                recordsToSave[record.recordID] = record;
            }

            // Piggyback on this operation to update our device state
            NSError* cdseError = nil;
            CKKSDeviceStateEntry* cdse = [CKKSDeviceStateEntry intransactionCreateDeviceStateForView:viewState
                                                                                      accountTracker:self.deps.accountStateTracker
                                                                                    lockStateTracker:self.deps.lockStateTracker
                                                                                               error:&cdseError];
            CKRecord* cdseRecord = [cdse CKRecordWithZoneID:viewState.zoneID];
            if(cdseError) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Can't make current device state: %@", cdseError);
            } else if(!cdseRecord) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Can't make current device state cloudkit record, but no reason why");
            } else {
                // Add the CDSE to the outgoing records
                // TODO: maybe only do this every few hours?
                ckksnotice("ckksoutgoing", viewState.zoneID, "Updating device state: %@", cdse);
                recordsToSave[cdseRecord.recordID] = cdseRecord;
            }

            ckksinfo("ckksoutgoing", viewState.zoneID, "Saving records %@ to CloudKit zone %@", recordsToSave, viewState.zoneID);
            AAFAnalyticsEventSecurity *uploadOQEsEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldNeedsReencryption:@(needsReencrypt),
                                                                                                                   kSecurityRTCFieldIsFullUpload:@(fullUpload)}
                                                                                                         altDSID:self.deps.activeAccount.altDSID
                                                                                                       eventName:kSecurityRTCEventNameUploadOQEsToCK
                                                                                                 testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                        category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                      sendMetric:self.deps.sendMetric];
            NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
            modifyComplete.name = @"modifyRecordsComplete";
            [self dependOnBeforeGroupFinished: modifyComplete];

            CKModifyRecordsOperation* modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave.allValues recordIDsToDelete:recordIDsToDelete];
            modifyRecordsOperation.atomic = TRUE;

            [modifyRecordsOperation linearDependencies:ckWriteOperations];

            // Until <rdar://problem/38725728> Changes to discretionary-ness (explicit or derived from QoS) should be "live", all requests should be nondiscretionary
            modifyRecordsOperation.configuration.isCloudKitSupportOperation = YES;

            modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
            modifyRecordsOperation.group = self.deps.currentOutgoingQueueOperationGroup;
            ckksnotice("ckksoutgoing", viewState.zoneID, "QoS: %d; operation group is %@", (int)modifyRecordsOperation.qualityOfService, modifyRecordsOperation.group);
            ckksnotice("ckksoutgoing", viewState.zoneID, "Beginning upload for %d records, deleting %d records",
                       (int)recordsToSave.count,
                       (int)recordIDsToDelete.count);

            for(CKRecordID* recordID in [recordsToSave allKeys]) {
                ckksinfo("ckksoutgoing", recordID.zoneID, "Record to save: %@", recordID);
            }
            for(CKRecordID* recordID in recordIDsToDelete) {
                ckksinfo("ckksoutgoing", recordID.zoneID, "Record to delete: %@", recordID);
            }

            __block int successfulItemsProcessed = 0;
            __block int errorItemsProcessed = 0;
            modifyRecordsOperation.perRecordSaveBlock = ^(CKRecordID *recordID, CKRecord * _Nullable record, NSError * _Nullable error) {
                if(!error) {
                    successfulItemsProcessed++;
                    ckksnotice("ckksoutgoing", recordID.zoneID, "Record upload successful for %@ (%@)", recordID.recordName, record.recordChangeTag);
                } else {
                    errorItemsProcessed++;
                    ckkserror("ckksoutgoing", recordID.zoneID, "error on row: %@ %@", error, recordID);
                }
            };

            modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
                STRONGIFY(self);
                [self modifyRecordsCompleted:viewState
                                  fullUpload:fullUpload
                               recordsToSave:recordsToSave
                           recordIDsToDelete:recordIDsToDelete
                           recordIDsModified:recordIDsModified
                              modifyComplete:modifyComplete
                                savedRecords:savedRecords
                            deletedRecordIDs:deletedRecordIDs
                                     ckerror:ckerror];

                [uploadOQEsEventS addMetrics:@{kSecurityRTCFieldItemsToAdd:@(recordsToSave.count),
                                               kSecurityRTCFieldItemsToDelete:@(recordIDsToDelete.count),
                                               kSecurityRTCFieldItemsToModify:@(recordIDsModified.count)}];
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:uploadOQEsEventS success:ckerror ? NO : YES error:ckerror];


                [eventS addMetrics:@{kSecurityRTCFieldItemsToAdd:@(recordsToSave.count), kSecurityRTCFieldItemsToDelete:@(recordIDsToDelete.count), kSecurityRTCFieldItemsToModify:@(recordIDsModified.count), kSecurityRTCFieldErrorItemsProcessed:@(errorItemsProcessed), kSecurityRTCFieldSuccessfulItemsProcessed:@(successfulItemsProcessed)}];
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:ckerror ? NO : YES error:ckerror];
            };
            [self dependOnBeforeGroupFinished:modifyRecordsOperation];
            [self.deps.ckdatabase addOperation:modifyRecordsOperation];

            return CKKSDatabaseTransactionCommit;
        }];
        
    }
}

- (void)modifyRecordsCompleted:(CKKSKeychainViewState*)viewState
                    fullUpload:(BOOL)fullUpload
                 recordsToSave:(NSDictionary<CKRecordID*, CKRecord*>*)recordsToSave
             recordIDsToDelete:(NSArray<CKRecordID*>*)recordIDsToDelete
             recordIDsModified:(NSSet<CKRecordID*>*)recordIDsModified
                modifyComplete:(NSOperation*)modifyComplete
                  savedRecords:(NSArray<CKRecord*>*)savedRecords
              deletedRecordIDs:(NSArray<CKRecordID*>*)deletedRecordIDs
                       ckerror:(NSError*)ckerror
{
    CKKSAnalytics* logger = [CKKSAnalytics logger];

    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        if(ckerror) {
            ckkserror("ckksoutgoing", viewState.zoneID, "error processing outgoing queue: %@", ckerror);

            [logger logRecoverableError:ckerror
                               forEvent:CKKSEventProcessOutgoingQueue
                               zoneName:viewState.zoneID.zoneName
                         withAttributes:NULL];

            // Tell CKKS about any out-of-date records
            [self.deps intransactionCKWriteFailed:ckerror attemptedRecordsChanged:recordsToSave];

            // Check if these are due to key records being out of date. We'll see a CKErrorBatchRequestFailed, with a bunch of errors inside
            if([ckerror.domain isEqualToString:CKErrorDomain] && (ckerror.code == CKErrorPartialFailure)) {
                NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = ckerror.userInfo[CKPartialErrorsByItemIDKey];

                bool askForReencrypt = false;

                if([self intransactionIsErrorBadEtagOnKeyPointersOnly:ckerror]) {
                    // The current key pointers have updated without our knowledge, so CloudKit failed this operation. Mark all records as 'needs reencryption' and kick that off.
                    ckksnotice("ckksoutgoing", viewState.zoneID, "Error is simply due to current key pointers changing; marking all records as 'needs reencrypt'");
                    [self _onqueueModifyAllRecords:failedRecords.allKeys
                                                as:SecCKKSStateReencrypt
                                         viewState:viewState];
                    askForReencrypt = true;

                } else if([self _onqueueIsErrorMissingSyncKey:ckerror]) {
                    ckksnotice("ckksoutgoing", viewState.zoneID, "Error is due to the key records missing. Marking all as 'needs reencrypt'");
                    [self _onqueueModifyAllRecords:failedRecords.allKeys
                                                as:SecCKKSStateReencrypt
                                         viewState:viewState];
                    askForReencrypt = true;

                } else {
                    // Iterate all failures, and reset each item
                    for(CKRecordID* recordID in failedRecords) {
                        NSError* recordError = failedRecords[recordID];

                        ckksnotice("ckksoutgoing", viewState.zoneID, "failed record: %@ %@", recordID, recordError);

                        if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorServerRecordChanged) {
                            if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
                               [recordID.recordName isEqualToString: SecCKKSKeyClassC]) {
                                askForReencrypt = true;
                            } else {
                                // CKErrorServerRecordChanged on an item update means that we've been overwritten.
                                if([recordIDsModified containsObject:recordID]) {
                                    [self _onqueueModifyRecordAsError:recordID
                                                          recordError:recordError
                                                            viewState:viewState];
                                }
                            }
                        } else if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorBatchRequestFailed) {
                            // Also fine. This record only didn't succeed because something else failed.
                            // OQEs should be placed back into the 'new' state, unless they've been overwritten by a new OQE. Other records should be ignored.

                            if([recordIDsModified containsObject:recordID]) {
                                NSError* localerror = nil;
                                CKKSOutgoingQueueEntry* inflightOQE = [CKKSOutgoingQueueEntry tryFromDatabase:recordID.recordName
                                                                                                        state:SecCKKSStateInFlight
                                                                                                    contextID:self.deps.contextID
                                                                                                       zoneID:recordID.zoneID
                                                                                                        error:&localerror];
                                [inflightOQE intransactionMoveToState:SecCKKSStateNew viewState:viewState error:&localerror];
                                if(localerror) {
                                    ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't clean up outgoing queue entry: %@", localerror);
                                }
                            }

                        } else {
                            // Some unknown error occurred on this record. If it's an OQE, move it to the error state.
                            ckkserror("ckksoutgoing", viewState.zoneID, "Unknown error on row: %@ %@", recordID, recordError);
                            if([recordIDsModified containsObject:recordID]) {
                                [self _onqueueModifyRecordAsError:recordID
                                                      recordError:recordError
                                                        viewState:viewState];
                            }
                        }
                    }
                }

                if(askForReencrypt) {
                    [self.deps.flagHandler _onqueueHandleFlag:CKKSFlagItemReencryptionNeeded];
                }
            } else if ([ckerror isCKInternalServerHTTPError]) {
                // Internal Server error - retry upload after 30 minutes.
                ckksnotice("ckks", viewState.zoneID, "CloudKit is presumably down; scheduling upload after %d seconds", OQEDELAY);
                [self _onqueueSaveRecordsWithDelay:[recordIDsModified allObjects]
                                         viewState:viewState];
            } else {
                // Some non-partial error occured. We should place all "inflight" OQEs back into the outgoing queue.
                ckksnotice("ckks", viewState.zoneID, "Error is scary: putting all inflight OQEs back into state 'new'");
                [self _onqueueModifyAllRecords:[recordIDsModified allObjects]
                                            as:SecCKKSStateNew
                                     viewState:viewState];
            }

            self.error = ckerror;
            return CKKSDatabaseTransactionCommit;
        }

        for(CKRecordID* deletedRecordID in deletedRecordIDs) {
            ckksnotice("ckksoutgoing", viewState.zoneID, "Record deletion successful for %@", deletedRecordID.recordName);
        }

        ckksnotice("ckksoutgoing", viewState.zoneID, "Completed processing outgoing queue (%d modifications, %d deletions)", (int)savedRecords.count, (int)deletedRecordIDs.count);
        NSError* localError = NULL;
        NSError* viewError = NULL;
        CKKSPowerCollection *plstats = [[CKKSPowerCollection alloc] init];

        AAFAnalyticsEventSecurity *saveCKMirrorEntriesEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldTotalCKRecords: @(savedRecords.count)}
                                                                                                              altDSID:self.deps.activeAccount.altDSID
                                                                                                            eventName:kSecurityRTCEventNameSaveCKMirrorEntries
                                                                                                      testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                             category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                           sendMetric:self.deps.sendMetric];
        
        for(CKRecord* record in savedRecords) {
            // Save the item records
            if([record.recordType isEqualToString: SecCKRecordItemType]) {
                CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:record.recordID.recordName
                                                                             state:SecCKKSStateInFlight
                                                                         contextID:self.deps.contextID
                                                                            zoneID:viewState.zoneID
                                                                             error:&localError];
                [oqe intransactionMoveToState:SecCKKSStateDeleted viewState:viewState error:&localError];
                if(localError) {
                    ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't update %@ in outgoingqueue: %@", record.recordID.recordName, localError);
                    self.error = viewError = localError;
                }
                localError = nil;
                CKKSMirrorEntry* ckme = [[CKKSMirrorEntry alloc] initWithCKRecord:record contextID:self.deps.contextID];
                [ckme saveToDatabase: &localError];
                if(localError) {
                    ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't save %@ to ckmirror: %@", record.recordID.recordName, localError);
                    self.error = viewError = localError;
                }

                [plstats storedOQE:oqe];

                // And the CKCurrentKeyRecords (do we need to do this? Will the server update the change tag on a save which saves nothing?)
            } else if([record.recordType isEqualToString: SecCKRecordCurrentKeyType]) {
                CKKSCurrentKeyPointer* currentkey = [[CKKSCurrentKeyPointer alloc] initWithCKRecord:record contextID:self.deps.contextID];
                [currentkey saveToDatabase: &localError];
                if(localError) {
                    ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't save %@ to currentkey: %@", record.recordID.recordName, localError);
                    self.error = viewError = localError;
                }

            } else if ([record.recordType isEqualToString:SecCKRecordDeviceStateType]) {
                CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record contextID:self.deps.contextID];
                [newcdse saveToDatabase:&localError];
                if(localError) {
                    ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't save %@ to ckdevicestate: %@", record.recordID.recordName, localError);
                    self.error = viewError = localError;
                }

            } else {
                ckkserror("ckksoutgoing", viewState.zoneID, "unknown record type in results: %@", record);
            }
        }
        localError = nil;
        
        // Delete the deleted record IDs
        for(CKRecordID* ckrecordID in deletedRecordIDs) {

            CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:ckrecordID.recordName
                                                                         state:SecCKKSStateInFlight
                                                                     contextID:self.deps.contextID
                                                                        zoneID:viewState.zoneID
                                                                         error:&localError];
            [oqe intransactionMoveToState:SecCKKSStateDeleted viewState:viewState error:&localError];
            if(localError) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't delete %@ from outgoingqueue: %@", ckrecordID.recordName, localError);
                self.error = viewError = localError;
            }
            localError = nil;
            CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:ckrecordID.recordName
                                                           contextID:self.deps.contextID
                                                              zoneID:viewState.zoneID
                                                               error:&localError];
            [ckme deleteFromDatabase: &localError];
            if(localError) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't delete %@ from ckmirror: %@", ckrecordID.recordName, localError);
                self.error = viewError = localError;
            }

            [plstats deletedOQE:oqe];
        }
        
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:saveCKMirrorEntriesEventS success:self.error ? NO : YES error:self.error];
        [plstats commit];

        [self.deps.overallLaunch addEvent:@"process-outgoing-queue-complete"];

        if(viewError) {
            ckkserror("ckksoutgoing", viewState.zoneID, "Operation failed; rolling back: %@", self.error);
            [logger logRecoverableError:self.error
                               forEvent:CKKSEventProcessOutgoingQueue
                               zoneName:viewState.zoneID.zoneName
                         withAttributes:NULL];
            return CKKSDatabaseTransactionRollback;
        } else {
            [logger logSuccessForEvent:CKKSEventProcessOutgoingQueue zoneName:viewState.zoneID.zoneName];
        }
        return CKKSDatabaseTransactionCommit;
    }];

    if(ckerror) {
        self.nextState = self.ckErrorState;
    } else {
        self.nextState = self.intendedState;
    }
    [self.operationQueue addOperation:modifyComplete];

    // If this was a "full upload", or we errored in any way, then kick off another queue process. There might be more items to send!
    ckksnotice("ckksoutgoing", viewState.zoneID, "Considering retry after a %@ upload with error: %@", fullUpload ? @"full" : @"non-full", self.error);
    if(fullUpload || self.error) {
        // If we think the network is iffy, though, wait for it to come back
        bool networkError = ckerror && [self.deps.reachabilityTracker isNetworkError:ckerror];

        // But wait for the network to be available!
        OctagonPendingFlag* pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagProcessOutgoingQueue
                                                                        conditions:networkError ? OctagonPendingConditionsNetworkReachable : 0
                                                                    delayInSeconds:.2];
        [self.deps.flagHandler handlePendingFlag:pendingFlag];

        if((fullUpload && self.error == nil) || [self isCKErrorBadEtagOnly:self.error]) {
            // Allow ourselves a quick upload token, since a successful full upload can be immediately followed by another upload, and an etag failure can be retried immediately
            OctagonPendingFlag* pendingFlag = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagOutgoingQueueOperationRateToken
                                                                            conditions:0
                                                                        delayInSeconds:.2];
            [self.deps.flagHandler handlePendingFlag:pendingFlag];
        }
    }
}


- (void)_onqueueModifyRecordAsError:(CKRecordID*)recordID
                        recordError:(NSError*)itemerror
                          viewState:(CKKSKeychainViewState*)viewState
{
    NSError* error = nil;
    uint64_t count = 0;

    // At this stage, cloudkit doesn't give us record types
    if([recordID.recordName isEqualToString:SecCKKSKeyClassA] ||
       [recordID.recordName isEqualToString:SecCKKSKeyClassC] ||
       [recordID.recordName hasPrefix:@"Manifest:-:"] ||
       [recordID.recordName hasPrefix:@"ManifestLeafRecord:-:"]) {
        // Nothing to do here. We need a whole key refetch and synchronize.
    } else {
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName
                                                                     state:SecCKKSStateInFlight
                                                                 contextID:self.deps.contextID
                                                                    zoneID:viewState.zoneID
                                                                     error:&error];
        [oqe intransactionMarkAsError:itemerror
                            viewState:viewState
                                error:&error];
        if(error) {
            ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't set OQE %@ as error: %@", recordID.recordName, error);
            self.error = error;
        }
        count ++;
    }
}

- (void)_onqueueSaveRecordsWithDelay:(NSArray<CKRecordID*>*)recordIDs
                           viewState:(CKKSKeychainViewState*)viewState
{
    NSError* error = nil;
    NSDate* processTime = [NSDate dateWithTimeIntervalSinceNow:OQEDELAY];
    for(CKRecordID* recordID in recordIDs) {
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName
                                                                     state:SecCKKSStateInFlight
                                                                 contextID:self.deps.contextID
                                                                    zoneID:viewState.zoneID
                                                                     error:&error];

        if(error || !oqe) {
            ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't fetch OQE %@: %@", recordID.recordName, error);
            self.error = error;
            continue;
        }

        oqe.waitUntil = processTime;
        ckksnotice("ckksoutgoing", viewState.zoneID, "Saving OQE %@ scheduled for retry at: %@", recordID.recordName, oqe.waitUntil);

        [oqe intransactionMoveToState:SecCKKSStateNew
                            viewState:viewState
                                error:&error];
        if(error) {
            ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't save OQE %@ as %@: %@", recordID.recordName, SecCKKSStateNew, error);
            self.error = error;
        }
    }
}

- (void)_onqueueModifyAllRecords:(NSArray<CKRecordID*>*)recordIDs
                              as:(CKKSItemState*)state
                       viewState:(CKKSKeychainViewState*)viewState
{
    NSError* error = nil;
    uint64_t count = 0;

    for(CKRecordID* recordID in recordIDs) {
        // At this stage, cloudkit doesn't give us record types
        if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
           [recordID.recordName isEqualToString: SecCKKSKeyClassC]) {
            // Nothing to do here. We need a whole key refetch and synchronize.
        } else {
            CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName
                                                                         state:SecCKKSStateInFlight
                                                                     contextID:self.deps.contextID
                                                                        zoneID:viewState.zoneID
                                                                         error:&error];
            [oqe intransactionMoveToState:state
                                viewState:viewState
                                    error:&error];
            if(error) {
                ckkserror("ckksoutgoing", viewState.zoneID, "Couldn't set OQE %@ as %@: %@", recordID.recordName, state, error);
                self.error = error;
            }
            count ++;
        }
    }

    if([state isEqualToString:SecCKKSStateReencrypt]) {
        [SecCoreAnalytics sendEvent:SecCKKSAggdItemReencryption event:@{SecCoreAnalyticsValue: [NSNumber numberWithUnsignedInteger:(NSUInteger)count]}];
    }
}

- (BOOL)_onqueueIsErrorMissingSyncKey:(NSError*)ckerror {
    if([ckerror.domain isEqualToString:CKErrorDomain] && (ckerror.code == CKErrorPartialFailure)) {
        NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = ckerror.userInfo[CKPartialErrorsByItemIDKey];

        for(CKRecordID* recordID in failedRecords) {
            NSError* recordError = failedRecords[recordID];

            if([recordError isCKKSServerPluginError:CKKSServerMissingRecord]) {
                ckksnotice("ckksoutgoing", recordID.zoneID, "Error is a 'missing record' error: %@", recordError);
                return YES;
            }
        }
    }

    return NO;
}

- (bool)intransactionIsErrorBadEtagOnKeyPointersOnly:(NSError*)ckerror {
    bool anyOtherErrors = false;

    if([ckerror.domain isEqualToString:CKErrorDomain] && (ckerror.code == CKErrorPartialFailure)) {
        NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = ckerror.userInfo[CKPartialErrorsByItemIDKey];

        for(CKRecordID* recordID in failedRecords) {
            NSError* recordError = failedRecords[recordID];

            if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorServerRecordChanged) {
                if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
                   [recordID.recordName isEqualToString: SecCKKSKeyClassC]) {
                    // this is fine!
                } else {
                    // Some record other than the key pointers changed.
                    anyOtherErrors |= true;
                    break;
                }
            } else {
                // Some other error than ServerRecordChanged
                anyOtherErrors |= true;
                break;
            }
        }
    }

    return !anyOtherErrors;
}

- (bool)isCKErrorBadEtagOnly:(NSError*)ckerror {
    if([ckerror.domain isEqualToString:CKErrorDomain] && (ckerror.code == CKErrorPartialFailure)) {
        bool anyOtherErrors = false;

        NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = ckerror.userInfo[CKPartialErrorsByItemIDKey];
        if(failedRecords == nil) {
            return false;
        }

        for(CKRecordID* recordID in failedRecords) {
            NSError* recordError = failedRecords[recordID];

            if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorServerRecordChanged) {
                // this is fine!
            } else if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorBatchRequestFailed) {
                // also fine!
            } else {
                // Some other error than ServerRecordChanged + CKErrorBatchRequestFailed
                anyOtherErrors |= true;
                break;
            }
        }

        return !anyOtherErrors;
    } else {
        return false;
    }
}

@end

#endif
