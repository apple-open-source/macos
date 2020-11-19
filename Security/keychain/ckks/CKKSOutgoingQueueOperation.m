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
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSOutgoingQueueEntry.h"
#import "keychain/ckks/CKKSOutgoingQueueOperation.h"
#import "keychain/ckks/CKKSReencryptOutgoingItemsOperation.h"
#import "keychain/ckks/CKKSStates.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ot/ObjCImprovements.h"

#include "keychain/securityd/SecItemServer.h"
#include "keychain/securityd/SecItemDb.h"
#include <Security/SecItemPriv.h>
#import "CKKSPowerCollection.h"
#import "utilities/SecCoreAnalytics.h"

@interface CKKSOutgoingQueueOperation()
@property CKModifyRecordsOperation* modifyRecordsOperation;
@end

@implementation CKKSOutgoingQueueOperation
@synthesize nextState = _nextState;
@synthesize intendedState = _intendedState;

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                                ckks:(CKKSKeychainView*)ckks
                           intending:(OctagonState*)intending
                          errorState:(OctagonState*)errorState
                    ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if(self = [super init]) {
        _deps = dependencies;
        _ckks = ckks;
        _ckoperationGroup = ckoperationGroup;

        _nextState = errorState;
        _intendedState = intending;

        [self addNullableDependency:ckks.holdOutgoingQueueOperation];

        // Depend on all previous CKKSOutgoingQueueOperations
        [self linearDependencies:ckks.outgoingQueueOperations];

        // We also depend on the view being setup and the key hierarchy being reasonable
        [self addNullableDependency:ckks.keyStateReadyDependency];
    }
    return self;
}

- (void) groupStart {
    WEAKIFY(self);

    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksoutgoing", self.deps.zoneID, "no ckks object");
        return;
    }

    if(!ckks.itemSyncingEnabled) {
        ckkserror("ckksoutgoing", self.deps.zoneID, "Item syncing for this view is disabled");
        return;
    }

    ckks.lastOutgoingQueueOperation = self;

    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{

        NSError* error = nil;

        NSSet<NSString*>* priorityUUIDs = [[CKKSViewManager manager] pendingCallbackUUIDs];

        NSMutableArray<CKKSOutgoingQueueEntry*>* priorityEntries = [NSMutableArray array];
        NSMutableSet<NSString*>* priorityEntryUUIDs = [NSMutableSet set];

        for(NSString* uuid in priorityUUIDs) {
            NSError* priorityFetchError = nil;
            CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry tryFromDatabase:uuid zoneID:self.deps.zoneID error:&priorityFetchError];

            if(priorityFetchError) {
                ckkserror("ckksoutgoing", self.deps.zoneID, "Unable to fetch priority uuid %@: %@", uuid, priorityFetchError);
                continue;
            }

            if(!oqe) {
                continue;
            }

            if(![oqe.state isEqualToString:SecCKKSStateNew]) {
                ckksnotice("ckksoutgoing", self.deps.zoneID, "Priority uuid %@ is not in 'new': %@", uuid, oqe);
                continue;
            }

            if(oqe) {
                ckksnotice("ckksoutgoing", self.deps.zoneID, "Found OQE  to fetch priority uuid %@: %@", uuid, priorityFetchError);
                [priorityEntries addObject:oqe];
                [priorityEntryUUIDs addObject:oqe.uuid];
            }
        }

        // We only actually care about queue items in the 'new' state
        NSArray<CKKSOutgoingQueueEntry*> * newEntries = [CKKSOutgoingQueueEntry fetch:SecCKKSOutgoingQueueItemsAtOnce
                                                                                state:SecCKKSStateNew
                                                                               zoneID:self.deps.zoneID
                                                                                error:&error];
        if(error != nil) {
            ckkserror("ckksoutgoing", self.deps.zoneID, "Error fetching outgoing queue records: %@", error);
            self.error = error;
            return CKKSDatabaseTransactionRollback;
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

        bool fullUpload = queueEntries.count >= SecCKKSOutgoingQueueItemsAtOnce;

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventOutgoingQueue zone:self.deps.zoneID.zoneName count:[queueEntries count]];

        ckksinfo("ckksoutgoing", self.deps.zoneID, "processing outgoing queue: %@", queueEntries);

        NSMutableDictionary<CKRecordID*, CKRecord*>* recordsToSave = [[NSMutableDictionary alloc] init];
        NSMutableSet<CKRecordID*>* recordIDsModified = [[NSMutableSet alloc] init];
        NSMutableSet<CKKSOutgoingQueueEntry*>*oqesModified = [[NSMutableSet alloc] init];
        NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];

        CKKSCurrentKeyPointer* currentClassAKeyPointer = [CKKSCurrentKeyPointer fromDatabase:SecCKKSKeyClassA zoneID:self.deps.zoneID error:&error];
        CKKSCurrentKeyPointer* currentClassCKeyPointer = [CKKSCurrentKeyPointer fromDatabase:SecCKKSKeyClassC zoneID:self.deps.zoneID error:&error];
        NSMutableDictionary<CKKSKeyClass*, CKKSCurrentKeyPointer*>* currentKeysToSave = [[NSMutableDictionary alloc] init];
        bool needsReencrypt = false;

        if(error != nil) {
            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't load current class keys: %@", error);
            return CKKSDatabaseTransactionRollback;
        }

        for(CKKSOutgoingQueueEntry* oqe in queueEntries) {
            if(self.cancelled) {
                secdebug("ckksoutgoing", "CKKSOutgoingQueueOperation cancelled, quitting");
                return CKKSDatabaseTransactionRollback;
            }

            CKKSOutgoingQueueEntry* inflight = [CKKSOutgoingQueueEntry tryFromDatabase: oqe.uuid state:SecCKKSStateInFlight zoneID:self.deps.zoneID error:&error];
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
                    ckksnotice("ckksoutgoing", self.deps.zoneID, "Item's encryption key (%@ %@) is neither %@ or %@", oqe, oqe.item.parentKeyUUID, currentClassAKeyPointer, currentClassCKeyPointer);
                    [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateReencrypt error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", self.deps.zoneID, "couldn't save oqe to database: %@", error);
                        self.error = error;
                        error = nil;
                    }
                    needsReencrypt = true;
                    continue;
                }
            }

            if([oqe.action isEqualToString: SecCKKSActionAdd]) {
                CKRecord* record = [oqe.item CKRecordWithZoneID:self.deps.zoneID];
                recordsToSave[record.recordID] = record;
                [recordIDsModified addObject: record.recordID];
                [oqesModified addObject:oqe];

                [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                if(error) {
                    ckkserror("ckksoutgoing", self.deps.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                    self.error = error;
                }

            } else if ([oqe.action isEqualToString: SecCKKSActionDelete]) {
                CKRecordID* recordIDToDelete = [[CKRecordID alloc] initWithRecordName:oqe.item.uuid zoneID:self.deps.zoneID];
                [recordIDsToDelete addObject: recordIDToDelete];
                [recordIDsModified addObject: recordIDToDelete];
                [oqesModified addObject:oqe];

                [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                if(error) {
                    ckkserror("ckksoutgoing", self.deps.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                }

            } else if ([oqe.action isEqualToString: SecCKKSActionModify]) {
                // Load the existing item from the ckmirror.
                CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:oqe.item.uuid zoneID:self.deps.zoneID error:&error];
                if(!ckme) {
                    // This is a problem: we have an update to an item that doesn't exist.
                    // Either: an Add operation we launched failed due to a CloudKit error (conflict?) and this is a follow-on update
                    //     Or: ?
                    ckkserror("ckksoutgoing", self.deps.zoneID, "update to a record that doesn't exist? %@", oqe.item.uuid);
                    // treat as an add.
                    CKRecord* record = [oqe.item CKRecordWithZoneID:self.deps.zoneID];
                    recordsToSave[record.recordID] = record;
                    [recordIDsModified addObject: record.recordID];
                    [oqesModified addObject:oqe];

                    [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", self.deps.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                        self.error = error;
                    }
                } else {
                    if(![oqe.item.storedCKRecord.recordChangeTag isEqual: ckme.item.storedCKRecord.recordChangeTag]) {
                        // The mirror entry has updated since this item was created. If we proceed, we might end up with
                        // a badly-authenticated record.
                        ckksnotice("ckksoutgoing", self.deps.zoneID, "Record (%@)'s change tag doesn't match ckmirror's change tag, reencrypting", oqe);
                        [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateReencrypt error:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", self.deps.zoneID, "couldn't save oqe to database: %@", error);
                            self.error = error;
                            error = nil;
                        }
                        needsReencrypt = true;
                        continue;
                    }
                    // Grab the old ckrecord and update it
                    CKRecord* record = [oqe.item updateCKRecord: ckme.item.storedCKRecord zoneID: self.deps.zoneID];
                    recordsToSave[record.recordID] = record;
                    [recordIDsModified addObject: record.recordID];
                    [oqesModified addObject:oqe];

                    [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateInFlight error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", self.deps.zoneID, "couldn't save state for CKKSOutgoingQueueEntry: %@", error);
                    }
                }
            }
        }

        if(needsReencrypt) {
            ckksnotice("ckksoutgoing", self.deps.zoneID, "An item needs reencryption!");
            [self.deps.flagHandler _onqueueHandleFlag:CKKSFlagItemReencryptionNeeded];
        }

        if([recordsToSave count] == 0 && [recordIDsToDelete count] == 0) {
            // Nothing to do! exit.
            ckksnotice("ckksoutgoing", self.deps.zoneID, "Nothing in outgoing queue to process");
            if(self.ckoperationGroup) {
                ckksinfo("ckksoutgoing", self.deps.zoneID, "End of operation group: %@", self.ckoperationGroup);
            }
            return true;
        }

        self.itemsProcessed = recordsToSave.count;

        NSBlockOperation* modifyComplete = [[NSBlockOperation alloc] init];
        modifyComplete.name = @"modifyRecordsComplete";
        [self dependOnBeforeGroupFinished: modifyComplete];

        void (^modifyRecordsCompletionBlock)(NSArray<CKRecord*>*, NSArray<CKRecordID*>*, NSError*) = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            STRONGIFY(self);
            CKKSKeychainView* strongCKKS = self.ckks;
            if(!self || !strongCKKS) {
                ckkserror("ckksoutgoing", self.deps.zoneID, "received callback for released object");
                return;
            }

            CKKSAnalytics* logger = [CKKSAnalytics logger];

            [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                if(ckerror) {
                    ckkserror("ckksoutgoing", self.deps.zoneID, "error processing outgoing queue: %@", ckerror);

                    [logger logRecoverableError:ckerror
                                       forEvent:CKKSEventProcessOutgoingQueue
                                       zoneName:self.deps.zoneID.zoneName
                                 withAttributes:NULL];

                    // Tell CKKS about any out-of-date records
                    [strongCKKS _onqueueCKWriteFailed:ckerror attemptedRecordsChanged:recordsToSave];

                    // Check if these are due to key records being out of date. We'll see a CKErrorBatchRequestFailed, with a bunch of errors inside
                    if([ckerror.domain isEqualToString:CKErrorDomain] && (ckerror.code == CKErrorPartialFailure)) {
                        NSMutableDictionary<CKRecordID*, NSError*>* failedRecords = ckerror.userInfo[CKPartialErrorsByItemIDKey];

                        bool askForReencrypt = false;

                        if([self _onqueueIsErrorBadEtagOnKeyPointersOnly:ckerror]) {
                            // The current key pointers have updated without our knowledge, so CloudKit failed this operation. Mark all records as 'needs reencryption' and kick that off.
                            ckksnotice("ckksoutgoing", self.deps.zoneID, "Error is simply due to current key pointers changing; marking all records as 'needs reencrypt'");
                            [self _onqueueModifyAllRecords:failedRecords.allKeys as:SecCKKSStateReencrypt];
                            askForReencrypt = true;

                        } else if([self _onqueueIsErrorMissingSyncKey:ckerror]) {
                            ckksnotice("ckksoutgoing", self.deps.zoneID, "Error is due to the key records missing. Marking all as 'needs reencrypt'");
                            [self _onqueueModifyAllRecords:failedRecords.allKeys as:SecCKKSStateReencrypt];
                            askForReencrypt = true;

                        } else {
                            // Iterate all failures, and reset each item
                            for(CKRecordID* recordID in failedRecords) {
                                NSError* recordError = failedRecords[recordID];

                                ckksnotice("ckksoutgoing", self.deps.zoneID, "failed record: %@ %@", recordID, recordError);

                                if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorServerRecordChanged) {
                                    if([recordID.recordName isEqualToString: SecCKKSKeyClassA] ||
                                       [recordID.recordName isEqualToString: SecCKKSKeyClassC]) {
                                        // Note that _onqueueCKWriteFailed is responsible for kicking the key state machine, so we don't need to do it here.
                                        askForReencrypt = true;
                                    } else {
                                        // CKErrorServerRecordChanged on an item update means that we've been overwritten.
                                        if([recordIDsModified containsObject:recordID]) {
                                            [self _onqueueModifyRecordAsError:recordID recordError:recordError];
                                        }
                                    }
                                } else if([recordError.domain isEqualToString: CKErrorDomain] && recordError.code == CKErrorBatchRequestFailed) {
                                    // Also fine. This record only didn't succeed because something else failed.
                                    // OQEs should be placed back into the 'new' state, unless they've been overwritten by a new OQE. Other records should be ignored.

                                    if([recordIDsModified containsObject:recordID]) {
                                        NSError* localerror = nil;
                                        CKKSOutgoingQueueEntry* inflightOQE = [CKKSOutgoingQueueEntry tryFromDatabase:recordID.recordName state:SecCKKSStateInFlight zoneID:recordID.zoneID error:&localerror];
                                        [strongCKKS _onqueueChangeOutgoingQueueEntry:inflightOQE toState:SecCKKSStateNew error:&localerror];
                                        if(localerror) {
                                            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't clean up outgoing queue entry: %@", localerror);
                                        }
                                    }

                                } else {
                                    // Some unknown error occurred on this record. If it's an OQE, move it to the error state.
                                    ckkserror("ckksoutgoing", self.deps.zoneID, "Unknown error on row: %@ %@", recordID, recordError);
                                    if([recordIDsModified containsObject:recordID]) {
                                        [self _onqueueModifyRecordAsError:recordID recordError:recordError];
                                    }
                                }
                            }
                        }

                        if(askForReencrypt) {
                            [self.deps.flagHandler _onqueueHandleFlag:CKKSFlagItemReencryptionNeeded];
                        }
                    } else {
                        // Some non-partial error occured. We should place all "inflight" OQEs back into the outgoing queue.
                        ckksnotice("ckks", self.deps.zoneID, "Error is scary: putting all inflight OQEs back into state 'new'");
                        [self _onqueueModifyAllRecords:[recordIDsModified allObjects] as:SecCKKSStateNew];
                    }

                    self.error = ckerror;
                    return CKKSDatabaseTransactionCommit;
                }

                for(CKRecordID* deletedRecordID in deletedRecordIDs) {
                    ckksnotice("ckksoutgoing", self.deps.zoneID, "Record deletion successful for %@", deletedRecordID.recordName);
                }

                ckksnotice("ckksoutgoing", self.deps.zoneID, "Completed processing outgoing queue (%d modifications, %d deletions)", (int)savedRecords.count, (int)deletedRecordIDs.count);
                NSError* error = NULL;
                CKKSPowerCollection *plstats = [[CKKSPowerCollection alloc] init];

                for(CKRecord* record in savedRecords) {
                    // Save the item records
                    if([record.recordType isEqualToString: SecCKRecordItemType]) {
                        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:record.recordID.recordName
                                                                                     state:SecCKKSStateInFlight
                                                                                    zoneID:self.deps.zoneID
                                                                                     error:&error];
                        [strongCKKS _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateDeleted error:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't update %@ in outgoingqueue: %@", record.recordID.recordName, error);
                            self.error = error;
                        }
                        error = nil;
                        CKKSMirrorEntry* ckme = [[CKKSMirrorEntry alloc] initWithCKRecord: record];
                        [ckme saveToDatabase: &error];
                        if(error) {
                            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't save %@ to ckmirror: %@", record.recordID.recordName, error);
                            self.error = error;
                        }

                        [plstats storedOQE:oqe];

                    // And the CKCurrentKeyRecords (do we need to do this? Will the server update the change tag on a save which saves nothing?)
                    } else if([record.recordType isEqualToString: SecCKRecordCurrentKeyType]) {
                        CKKSCurrentKeyPointer* currentkey = [[CKKSCurrentKeyPointer alloc] initWithCKRecord: record];
                        [currentkey saveToDatabase: &error];
                        if(error) {
                            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't save %@ to currentkey: %@", record.recordID.recordName, error);
                            self.error = error;
                        }

                    } else if ([record.recordType isEqualToString:SecCKRecordDeviceStateType]) {
                        CKKSDeviceStateEntry* newcdse = [[CKKSDeviceStateEntry alloc] initWithCKRecord:record];
                        [newcdse saveToDatabase:&error];
                        if(error) {
                            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't save %@ to ckdevicestate: %@", record.recordID.recordName, error);
                            self.error = error;
                        }

                    } else if ([record.recordType isEqualToString:SecCKRecordManifestType]) {

                    } else if (![record.recordType isEqualToString:SecCKRecordManifestLeafType]) {
                        ckkserror("ckksoutgoing", self.deps.zoneID, "unknown record type in results: %@", record);
                    }
                }

                // Delete the deleted record IDs
                for(CKRecordID* ckrecordID in deletedRecordIDs) {

                    NSError* error = nil;
                    CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:ckrecordID.recordName
                                                                                 state:SecCKKSStateInFlight
                                                                                zoneID:self.deps.zoneID
                                                                                 error:&error];
                    [strongCKKS _onqueueChangeOutgoingQueueEntry:oqe toState:SecCKKSStateDeleted error:&error];
                    if(error) {
                        ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't delete %@ from outgoingqueue: %@", ckrecordID.recordName, error);
                        self.error = error;
                    }
                    error = nil;
                    CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase: ckrecordID.recordName zoneID:self.deps.zoneID error:&error];
                    [ckme deleteFromDatabase: &error];
                    if(error) {
                        ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't delete %@ from ckmirror: %@", ckrecordID.recordName, error);
                        self.error = error;
                    }

                    [plstats deletedOQE:oqe];
                }

                [plstats commit];

                if(self.error) {
                    ckkserror("ckksoutgoing", self.deps.zoneID, "Operation failed; rolling back: %@", self.error);
                    [logger logRecoverableError:self.error
                                       forEvent:CKKSEventProcessOutgoingQueue
                                       zoneName:strongCKKS.zoneName
                                 withAttributes:NULL];
                    return CKKSDatabaseTransactionRollback;
                } else {
                    [logger logSuccessForEvent:CKKSEventProcessOutgoingQueue zoneName:strongCKKS.zoneName];
                }
                return CKKSDatabaseTransactionCommit;
            }];

            self.nextState = self.intendedState;
            [self.operationQueue addOperation: modifyComplete];

            // If this was a "full upload", or we errored in any way, then kick off another queue process. There might be more items to send!
            if(fullUpload || self.error) {
                // If we think the network is iffy, though, wait for it to come back
                CKKSResultOperation* possibleNetworkDependency = nil;
                CKKSReachabilityTracker* reachabilityTracker = strongCKKS.reachabilityTracker;
                if(ckerror && [reachabilityTracker isNetworkError:ckerror]) {
                    possibleNetworkDependency = reachabilityTracker.reachabilityDependency;
                }

                [strongCKKS processOutgoingQueueAfter:possibleNetworkDependency
                                        requiredDelay:fullUpload && !ckerror ? NSEC_PER_MSEC * 100 : DISPATCH_TIME_FOREVER
                                     ckoperationGroup:self.ckoperationGroup];
            }
        };

        ckksinfo("ckksoutgoing", self.deps.zoneID, "Current keys to update: %@", currentKeysToSave);
        for(CKKSCurrentKeyPointer* keypointer in currentKeysToSave.allValues) {
            CKRecord* record = [keypointer CKRecordWithZoneID:self.deps.zoneID];
            recordsToSave[record.recordID] = record;
        }

        // Piggyback on this operation to update our device state
        NSError* cdseError = nil;
        CKKSDeviceStateEntry* cdse = [ckks _onqueueCurrentDeviceStateEntry:&cdseError];
        CKRecord* cdseRecord = [cdse CKRecordWithZoneID:self.deps.zoneID];
        if(cdseError) {
            ckkserror("ckksoutgoing", self.deps.zoneID, "Can't make current device state: %@", cdseError);
        } else if(!cdseRecord) {
            ckkserror("ckksoutgoing", self.deps.zoneID, "Can't make current device state cloudkit record, but no reason why");
        } else {
            // Add the CDSE to the outgoing records
            // TODO: maybe only do this every few hours?
            ckksnotice("ckksoutgoing", self.deps.zoneID, "Updating device state: %@", cdse);
            recordsToSave[cdseRecord.recordID] = cdseRecord;
        }

        ckksinfo("ckksoutgoing", self.deps.zoneID, "Saving records %@ to CloudKit zone %@", recordsToSave, self.deps.zoneID);

        self.modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave.allValues recordIDsToDelete:recordIDsToDelete];
        self.modifyRecordsOperation.atomic = TRUE;

        // Until <rdar://problem/38725728> Changes to discretionary-ness (explicit or derived from QoS) should be "live", all requests should be nondiscretionary
        self.modifyRecordsOperation.configuration.automaticallyRetryNetworkFailures = NO;
        self.modifyRecordsOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
        self.modifyRecordsOperation.configuration.isCloudKitSupportOperation = YES;

        self.modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
        self.modifyRecordsOperation.group = self.ckoperationGroup;
        ckksnotice("ckksoutgoing", self.deps.zoneID, "QoS: %d; operation group is %@", (int)self.modifyRecordsOperation.qualityOfService, self.modifyRecordsOperation.group);
        ckksnotice("ckksoutgoing", self.deps.zoneID, "Beginning upload for %d records, deleting %d records",
                   (int)recordsToSave.count,
                   (int)recordIDsToDelete.count);

        for(CKRecordID* recordID in [recordsToSave allKeys]) {
            ckksinfo("ckksoutgoing", self.deps.zoneID, "Record to save: %@", recordID);
        }
        for(CKRecordID* recordID in recordIDsToDelete) {
            ckksinfo("ckksoutgoing", self.deps.zoneID, "Record to delete: %@", recordID);
        }

        self.modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            STRONGIFY(self);

            if(!error) {
                ckksnotice("ckksoutgoing", self.deps.zoneID, "Record upload successful for %@ (%@)", record.recordID.recordName, record.recordChangeTag);
            } else {
                ckkserror("ckksoutgoing", self.deps.zoneID, "error on row: %@ %@", error, record);
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
        ckkserror("ckksoutgoing", self.deps.zoneID, "no CKKS object");
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
        CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName state:SecCKKSStateInFlight zoneID:self.deps.zoneID error:&error];
        [ckks _onqueueErrorOutgoingQueueEntry: oqe itemError: itemerror error:&error];
        if(error) {
            ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't set OQE %@ as error: %@", recordID.recordName, error);
            self.error = error;
        }
        count ++;
    }
}


- (void)_onqueueModifyAllRecords:(NSArray<CKRecordID*>*)recordIDs as:(CKKSItemState*)state {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksoutgoing", self.deps.zoneID, "no CKKS object");
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
            CKKSOutgoingQueueEntry* oqe = [CKKSOutgoingQueueEntry fromDatabase:recordID.recordName state:SecCKKSStateInFlight zoneID:self.deps.zoneID error:&error];
            [ckks _onqueueChangeOutgoingQueueEntry:oqe toState:state error:&error];
            if(error) {
                ckkserror("ckksoutgoing", self.deps.zoneID, "Couldn't set OQE %@ as %@: %@", recordID.recordName, state, error);
                self.error = error;
            }
            count ++;
        }
    }

    if([state isEqualToString:SecCKKSStateReencrypt]) {
        [SecCoreAnalytics sendEvent:SecCKKSAggdItemReencryption event:@{SecCoreAnalyticsValue: [NSNumber numberWithUnsignedInteger:count]}];
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

- (bool)_onqueueIsErrorBadEtagOnKeyPointersOnly:(NSError*)ckerror {
    CKKSKeychainView* ckks = self.ckks;
    dispatch_assert_queue(ckks.queue);
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

@end;

#endif
