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
#import "CKKSCurrentKeyPointer.h"
#import "CKKSOutgoingQueueOperation.h"
#import "CKKSIncomingQueueEntry.h"
#import "CKKSItemEncrypter.h"
#import "CKKSOutgoingQueueEntry.h"
#import "CKKSReencryptOutgoingItemsOperation.h"
#import "CKKSManifest.h"
#import "CKKSAnalyticsLogger.h"

#include <securityd/SecItemServer.h>
#include <securityd/SecItemDb.h>
#include <Security/SecItemPriv.h>
#include <utilities/SecADWrapper.h>
#import "CKKSPowerCollection.h"

#if OCTAGON

@interface CKKSOutgoingQueueOperation()
@property CKModifyRecordsOperation* modifyRecordsOperation;
@end

@implementation CKKSOutgoingQueueOperation

- (instancetype)init {
    if(self = [super init]) {
    }
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks ckoperationGroup:(CKOperationGroup*)ckoperationGroup {
    if(self = [super init]) {
        _ckks = ckks;
        _ckoperationGroup = ckoperationGroup;

        [self addNullableDependency:ckks.viewSetupOperation];
        [self addNullableDependency:ckks.holdOutgoingQueueOperation];

        // Depend on all previous CKKSOutgoingQueueOperations
        [self linearDependencies:ckks.outgoingQueueOperations];

        // We also depend on the view being setup and the key hierarchy being reasonable
        [self addNullableDependency:ckks.viewSetupOperation];
        [self addNullableDependency:ckks.keyStateReadyDependency];
    }
    return self;
}

- (void) groupStart {
    // Synchronous, on some thread. Get back on the CKKS queue for thread-safety.
    __weak __typeof(self) weakSelf = self;

    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksoutgoing", ckks, "no ckks object");
        return;
    }

    [ckks dispatchSyncWithAccountQueue: ^bool{
        ckks.lastOutgoingQueueOperation = self;
        if(self.cancelled) {
            ckksnotice("ckksoutgoing", ckks, "CKKSOutgoingQueueOperation cancelled, quitting");
            return false;
        }

        NSError* error = nil;

        // We only actually care about queue items in the 'new' state
        NSArray<CKKSOutgoingQueueEntry*> * queueEntries = [CKKSOutgoingQueueEntry fetch:SecCKKSOutgoingQueueItemsAtOnce state: SecCKKSStateNew zoneID:ckks.zoneID error:&error];

        if(error != nil) {
            ckkserror("ckksoutgoing", ckks, "Error fetching outgoing queue records: %@", error);
            self.error = error;
            return false;
        }

        ckksinfo("ckksoutgoing", ckks, "processing outgoing queue: %@", queueEntries);

        NSMutableDictionary<CKRecordID*, CKRecord*>* recordsToSave = [[NSMutableDictionary alloc] init];
        NSMutableSet<CKRecordID*>* oqesModified = [[NSMutableSet alloc] init];
        NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];

        CKKSCurrentKeyPointer* currentClassAKeyPointer = [CKKSCurrentKeyPointer fromDatabase: SecCKKSKeyClassA zoneID:ckks.zoneID error: &error];
        CKKSCurrentKeyPointer* currentClassCKeyPointer = [CKKSCurrentKeyPointer fromDatabase: SecCKKSKeyClassC zoneID:ckks.zoneID error: &error];
        NSMutableDictionary<CKKSKeyClass*, CKKSCurrentKeyPointer*>* currentKeysToSave = [[NSMutableDictionary alloc] init];
        bool needsReencrypt = false;

        if(error != nil) {
            ckkserror("ckksoutgoing", ckks, "Couldn't load current class keys: %@", error);
            return false;
        }

        for(CKKSOutgoingQueueEntry* oqe in queueEntries) {
            if(self.cancelled) {
                secdebug("ckksoutgoing", "CKKSOutgoingQueueOperation cancelled, quitting");
                return false;
            }

            CKKSOutgoingQueueEntry* inflight = [CKKSOutgoingQueueEntry tryFromDatabase: oqe.uuid state:SecCKKSStateInFlight zoneID:ckks.zoneID error: &error];
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
                    ckksnotice("ckksoutgoing", ckks, "Item's encryption key (%@ %@) is neither %@ or %@", oqe, oqe.item.parentKeyUUID, currentClassAKeyPointer, currentClassCKeyPointer);
                    [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateReencrypt error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", ckks, "couldn't save oqe to database: %@", error);
                        self.error = error;
                        error = nil;
                    }
                    needsReencrypt = true;
                    continue;
                }
            }

            if([oqe.action isEqualToString: SecCKKSActionAdd]) {
                CKRecord* record = [oqe.item CKRecordWithZoneID: ckks.zoneID];
                recordsToSave[record.recordID] = record;
                [oqesModified addObject: record.recordID];

                [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                if(error) {
                    ckkserror("ckksoutgoing", ckks, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                    self.error = error;
                }

            } else if ([oqe.action isEqualToString: SecCKKSActionDelete]) {
                [recordIDsToDelete addObject: [[CKRecordID alloc] initWithRecordName: oqe.item.uuid zoneID: ckks.zoneID]];

                [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                if(error) {
                    ckkserror("ckksoutgoing", ckks, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                }

            } else if ([oqe.action isEqualToString: SecCKKSActionModify]) {
                // Load the existing item from the ckmirror.
                CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase: oqe.item.uuid zoneID:ckks.zoneID error:&error];
                if(!ckme) {
                    // This is a problem: we have an update to an item that doesn't exist.
                    // Either: an Add operation we launched failed due to a CloudKit error (conflict?) and this is a follow-on update
                    //     Or: ?
                    ckkserror("ckksoutgoing", ckks, "update to a record that doesn't exist? %@", oqe.item.uuid);
                    // treat as an add.
                    CKRecord* record = [oqe.item CKRecordWithZoneID: ckks.zoneID];
                    recordsToSave[record.recordID] = record;
                    [oqesModified addObject: record.recordID];

                    [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", ckks, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                        self.error = error;
                    }
                } else {
                    if(![oqe.item.storedCKRecord.recordChangeTag isEqual: ckme.item.storedCKRecord.recordChangeTag]) {
                        // The mirror entry has updated since this item was created. If we proceed, we might end up with
                        // a badly-authenticated record.
                        ckksnotice("ckksoutgoing", ckks, "Record (%@)'s change tag doesn't match ckmirror's change tag, reencrypting", oqe);
                        [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateReencrypt error:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", ckks, "couldn't save oqe to database: %@", error);
                            self.error = error;
                            error = nil;
                        }
                        needsReencrypt = true;
                        continue;
                    }
                    // Grab the old ckrecord and update it
                    CKRecord* record = [oqe.item updateCKRecord: ckme.item.storedCKRecord zoneID: ckks.zoneID];
                    recordsToSave[record.recordID] = record;
                    [oqesModified addObject: record.recordID];

                    [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", ckks, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                    }
                }
            }
        }

        if(needsReencrypt) {
            ckksnotice("ckksoutgoing", ckks, "An item needs reencryption!");

            CKKSReencryptOutgoingItemsOperation* op = [[CKKSReencryptOutgoingItemsOperation alloc] initWithCKKSKeychainView:ckks ckoperationGroup:self.ckoperationGroup];
            [ckks scheduleOperation: op];
        }

        if([recordsToSave count] == 0 && [recordIDsToDelete count] == 0) {
            // Nothing to do! exit.
            ckksnotice("ckksoutgoing", ckks, "Nothing in outgoing queue to process");
            if(self.ckoperationGroup) {
                ckksnotice("ckksoutgoing", ckks, "End of operation group: %@", self.ckoperationGroup);
            }
            return true;
        }

        self.itemsProcessed = recordsToSave.count;

        NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
        modifyComplete.name = @"modifyRecordsComplete";
        [self dependOnBeforeGroupFinished: modifyComplete];

        if ([CKKSManifest shouldSyncManifests]) {
            if (ckks.egoManifest) {
                [ckks.egoManifest updateWithNewOrChangedRecords:recordsToSave.allValues deletedRecordIDs:recordIDsToDelete];
                for(CKRecord* record in [ckks.egoManifest allCKRecordsWithZoneID:ckks.zoneID]) {
                    recordsToSave[record.recordID] = record;
                }
                NSError* saveError = nil;
                if (![ckks.egoManifest saveToDatabase:&saveError]) {
                    self.error = saveError;
                    ckkserror("ckksoutgoing", ckks, "could not save ego manifest with error: %@", saveError);
                }
            }
            else {
                ckkserror("ckksoutgoing", ckks, "could not get current ego manifest to update");
            }
        }

        void (^modifyRecordsCompletionBlock)(NSArray<CKRecord*>*, NSArray<CKRecordID*>*, NSError*) = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf || !strongCKKS) {
                ckkserror("ckksoutgoing", strongCKKS, "received callback for released object");
                return;
            }

            [strongCKKS dispatchSync: ^bool{
                if(ckerror) {
                    ckkserror("ckksoutgoing", strongCKKS, "error processing outgoing queue: %@", ckerror);

                    // Tell CKKS about any out-of-date records
                    [strongCKKS _onqueueCKWriteFailed:ckerror attemptedRecordsChanged:recordsToSave];

                    // Check if these are due to key records being out of date. We'll see a CKErrorBatchRequestFailed, with a bunch of errors inside
                    if([ckerror.domain isEqualToString:CKErrorDomain] && (ckerror.code == CKErrorPartialFailure)) {
                        NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = ckerror.userInfo[CKPartialErrorsByItemIDKey];
                        ckksnotice("ckksoutgoing", strongCKKS, "failed records %@", failedRecords);
                        for(CKRecordID* recordID in failedRecords.allKeys) {
                            NSError* recordError = failedRecords[recordID];

                            if(recordError.code == CKErrorServerRecordChanged) {
                                if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
                                   [recordID.recordName isEqualToString: SecCKKSKeyClassC]) {
                                    // The current key pointers have updated without our knowledge, so CloudKit failed this operation. Mark all records as 'needs reencryption' and kick that off.
                                    [strongSelf _onqueueModifyAllRecordsAsReencrypt: failedRecords.allKeys];

                                    ckksnotice("ckksoutgoing", strongCKKS, "initiate key fetch and reencrypt");
                                    // Nudge the key state machine, so that it runs off to fetch the new keys
                                    [strongCKKS _onqueueKeyStateMachineRequestFetch];
                                    // This will wait for the key hierarchy to become 'ready'
                                    CKKSReencryptOutgoingItemsOperation* op = [[CKKSReencryptOutgoingItemsOperation alloc] initWithCKKSKeychainView:strongCKKS ckoperationGroup:strongSelf.ckoperationGroup];
                                    [strongCKKS scheduleOperation: op];

                                    // Quit the loop so we only do this once
                                    break;
                                } else {
                                    // CKErrorServerRecordChanged on an item update means that we've been overwritten.
                                    if([oqesModified containsObject:recordID]) {
                                        [self _onqueueModifyRecordAsError:recordID recordError:recordError];
                                    }
                                }
                            } else if(recordError.code == CKErrorBatchRequestFailed) {
                                // Also fine. This record only didn't succeed because something else failed.
                                // OQEs should be placed back into the 'new' state, unless they've been overwritten by a new OQE. Other records should be ignored.

                                if([oqesModified containsObject:recordID]) {
                                    NSError* error = nil;
                                    CKKSOutgoingQueueEntry* inflightOQE = [CKKSOutgoingQueueEntry tryFromDatabase:recordID.recordName state:SecCKKSStateInFlight zoneID:recordID.zoneID error:&error];
                                    CKKSOutgoingQueueEntry* newOQE = [CKKSOutgoingQueueEntry tryFromDatabase:recordID.recordName state:SecCKKSStateNew zoneID:recordID.zoneID error:&error];
                                    if(error) {
                                        ckkserror("ckksoutgoing", strongCKKS, "Couldn't try to fetch an overwriting OQE: %@", error);
                                    }

                                    if(newOQE) {
                                        ckksnotice("ckksoutgoing", strongCKKS, "New modification has come in behind failed change for %@; dropping failed change", inflightOQE);
                                        [strongCKKS _onqueueChangeOutgoingQueueEntry:inflightOQE toState:SecCKKSStateDeleted error:&error];
                                        if(error) {
                                            ckkserror("ckksoutgoing", strongCKKS, "Couldn't delete in-flight OQE: %@", error);
                                        }
                                    } else {
                                        [strongCKKS _onqueueChangeOutgoingQueueEntry:inflightOQE toState:SecCKKSStateNew error:&error];
                                    }
                                }

                            } else if ([recordID.recordName hasPrefix:@"Manifest:-:"] || [recordID.recordName hasPrefix:@"ManifestLeafRecord:-:"]) {
                                [[CKKSAnalyticsLogger logger] logSoftFailureForEventNamed:@"ManifestUpload" withAttributes:@{CKKSManifestZoneKey : strongCKKS.zoneID.zoneName, CKKSManifestSignerIDKey : strongCKKS.egoManifest.signerID, CKKSManifestGenCountKey : @(strongCKKS.egoManifest.generationCount)}];
                            } else {
                                // Some unknown error occurred on this record. If it's an OQE, move it to the error state.
                                ckkserror("ckksoutgoing", strongCKKS, "Unknown error on row: %@ %@", recordID, recordError);
                                if([oqesModified containsObject:recordID]) {
                                    [self _onqueueModifyRecordAsError:recordID recordError:recordError];
                                }
                            }
                        }
                    }

                    strongSelf.error = error;
                    return true;
                }

                ckksnotice("ckksoutgoing", strongCKKS, "Completed processing outgoing queue");
                NSError* error = NULL;
                CKKSPowerCollection *plstats = [[CKKSPowerCollection alloc] init];

                for(CKRecord* record in savedRecords) {
                    // Save the item records
                    if([record.recordType isEqualToString: SecCKRecordItemType]) {
                        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase: record.recordID.recordName state: SecCKKSStateInFlight zoneID:strongCKKS.zoneID error:&error];
                        [strongCKKS _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateDeleted error:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", strongCKKS, "Couldn't update %@ in outgoingqueue: %@", record.recordID.recordName, error);
                            strongSelf.error = error;
                        }
                        error = nil;
                        CKKSMirrorEntry* ckme = [[CKKSMirrorEntry alloc] initWithCKRecord: record];
                        [ckme saveToDatabase: &error];
                        if(error) {
                            ckkserror("ckksoutgoing", strongCKKS, "Couldn't save %@ to ckmirror: %@", record.recordID.recordName, error);
                            strongSelf.error = error;
                        }

                        [plstats storedOQE:oqe];

                    // And the CKCurrentKeyRecords (do we need to do this? Will the server update the change tag on a save which saves nothing?)
                    } else if([record.recordType isEqualToString: SecCKRecordCurrentKeyType]) {
                        CKKSCurrentKeyPointer* currentkey = [[CKKSCurrentKeyPointer alloc] initWithCKRecord: record];
                        [currentkey saveToDatabase: &error];
                        if(error) {
                            ckkserror("ckksoutgoing", strongCKKS, "Couldn't save %@ to currentkey: %@", record.recordID.recordName, error);
                            strongSelf.error = error;
                        }

                    } else if ([record.recordType isEqualToString:SecCKRecordDeviceStateType]) {
                        CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record];
                        [newcdse saveToDatabase:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", strongCKKS, "Couldn't save %@ to ckdevicestate: %@", record.recordID.recordName, error);
                            strongSelf.error = error;
                        }

                    } else if ([record.recordType isEqualToString:SecCKRecordManifestType]) {
                        [[CKKSAnalyticsLogger logger] logSuccessForEventNamed:@"ManifestUpload"];
                    } else if (![record.recordType isEqualToString:SecCKRecordManifestLeafType]) {
                        ckkserror("ckksoutgoing", strongCKKS, "unknown record type in results: %@", record);
                    }
                }

                // Delete the deleted record IDs
                for(CKRecordID* ckrecordID in deletedRecordIDs) {

                    NSError* error = nil;
                    CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase: ckrecordID.recordName state: SecCKKSStateInFlight zoneID:strongCKKS.zoneID error:&error];
                    [strongCKKS _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateDeleted error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", strongCKKS, "Couldn't delete %@ from outgoingqueue: %@", ckrecordID.recordName, error);
                        strongSelf.error = error;
                    }
                    error = nil;
                    CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase: ckrecordID.recordName zoneID:strongCKKS.zoneID error:&error];
                    [ckme deleteFromDatabase: &error];
                    if(error) {
                        ckkserror("ckksoutgoing", strongCKKS, "Couldn't delete %@ from ckmirror: %@", ckrecordID.recordName, error);
                        strongSelf.error = error;
                    }

                    [plstats deletedOQE:oqe];
                }

                [plstats commit];

                if(strongSelf.error) {
                    ckkserror("ckksoutgoing", strongCKKS, "Operation failed; rolling back: %@", strongSelf.error);
                    return false;
                }
                return true;
            }];


            [strongSelf.operationQueue addOperation: modifyComplete];
            // Kick off another queue process. We expect it to exit instantly, but who knows!
            [strongCKKS processOutgoingQueue:self.ckoperationGroup];
        };

        ckksinfo("ckksoutgoing", ckks, "Current keys to update: %@", currentKeysToSave);
        for(CKKSCurrentKeyPointer* keypointer in currentKeysToSave.allValues) {
            CKRecord* record = [keypointer CKRecordWithZoneID: ckks.zoneID];
            recordsToSave[record.recordID] = record;
        }

        // Piggyback on this operation to update our device state
        NSError* cdseError = nil;
        CKKSDeviceStateEntry* cdse = [ckks _onqueueCurrentDeviceStateEntry:&cdseError];
        CKRecord* cdseRecord = [cdse CKRecordWithZoneID: ckks.zoneID];
        if(cdseError) {
            ckkserror("ckksoutgoing", ckks, "Can't make current device state: %@", cdseError);
        } else if(!cdseRecord) {
            ckkserror("ckksoutgoing", ckks, "Can't make current device state cloudkit record, but no reason why");
        } else {
            // Add the CDSE to the outgoing records
            // TODO: maybe only do this every few hours?
            ckksnotice("ckksoutgoing", ckks, "Updating device state: %@", cdse);
            recordsToSave[cdseRecord.recordID] = cdseRecord;
        }

        ckksinfo("ckksoutgoing", ckks, "Saving records %@ to CloudKit zone %@", recordsToSave, ckks.zoneID);

        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave.allValues recordIDsToDelete:recordIDsToDelete];
        self.modifyRecordsOperation.atomic = TRUE;
        self.modifyRecordsOperation.timeoutIntervalForRequest = 2;
        self.modifyRecordsOperation.qualityOfService = NSQualityOfServiceUtility;
        self.modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
        self.modifyRecordsOperation.group = self.ckoperationGroup;
        ckksnotice("ckksoutgoing", ckks, "Operation group is %@", self.ckoperationGroup);

        self.modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

            if(!error) {
                ckksnotice("ckksoutgoing", blockCKKS, "Record upload successful for %@", record.recordID.recordName);
            } else {
                ckkserror("ckksoutgoing", blockCKKS, "error on row: %@ %@", record, error);
            }
        };

        self.modifyRecordsOperation.modifyRecordsCompletionBlock = modifyRecordsCompletionBlock;
        [self dependOnBeforeGroupFinished: self.modifyRecordsOperation];
        [ckks.database addOperation: self.modifyRecordsOperation];

        return true;
    }];
}

- (void)_onqueueModifyRecordAsError:(CKRecordID*)recordID recordError:(NSError*)itemerror {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksoutgoing", ckks, "no CKKS object");
        return;
    }

    dispatch_assert_queue(ckks.queue);

    NSError* error = nil;
    uint64_t count = 0;

    // At this stage, cloudkit doesn't give us record types
    if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
       [recordID.recordName isEqualToString: SecCKKSKeyClassC] ||
       [recordID.recordName hasPrefix:@"Manifest:-:"] ||
       [recordID.recordName hasPrefix:@"ManifestLeafRecord:-:"]) {
        // Nothing to do here. We need a whole key refetch and synchronize.
    } else {
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName state: SecCKKSStateInFlight zoneID:ckks.zoneID error:&error];
        [ckks _onqueueErrorOutgoingQueueEntry: oqe itemError: itemerror error:&error];
        if(error) {
            ckkserror("ckksoutgoing", ckks, "Couldn't set OQE %@ as error: %@", recordID.recordName, error);
            self.error = error;
        }
        count ++;
    }
}


- (void)_onqueueModifyAllRecordsAsReencrypt: (NSArray<CKRecordID*>*) recordIDs {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksoutgoing", ckks, "no CKKS object");
        return;
    }

    dispatch_assert_queue(ckks.queue);

    NSError* error = nil;
    uint64_t count = 0;

    for(CKRecordID* recordID in recordIDs) {
        // At this stage, cloudkit doesn't give us record types
        if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
           [recordID.recordName isEqualToString: SecCKKSKeyClassC]) {
            // Nothing to do here. We need a whole key refetch and synchronize.
        } else {
            CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName state: SecCKKSStateInFlight zoneID:ckks.zoneID error:&error];
            [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateReencrypt error:&error];
            if(error) {
                ckkserror("ckksoutgoing", ckks, "Couldn't set OQE %@ as reencrypt: %@", recordID.recordName, error);
                self.error = error;
            }
            count ++;
        }
    }

    SecADAddValueForScalarKey((__bridge CFStringRef) SecCKKSAggdItemReencryption, count);
}

@end;

#endif
