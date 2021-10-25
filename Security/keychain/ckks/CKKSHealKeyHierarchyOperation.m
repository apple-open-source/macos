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

#import "CKKSKeychainView.h"
#import "CKKSCurrentKeyPointer.h"
#import "CKKSKey.h"
#import "CKKSHealKeyHierarchyOperation.h"
#import "CKKSGroupOperation.h"
#import "CKKSAnalytics.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSHealTLKSharesOperation.h"
#import "keychain/ckks/CKKSIncomingQueueEntry.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"

#if OCTAGON

@interface CKKSHealKeyHierarchyOperation ()
@property BOOL allowFullRefetchResult;
@property BOOL newCloudKitRecordsWritten;
@property BOOL cloudkitWriteFailures;

@property CKKSResultOperation* setResultStateOperation;

@property NSHashTable* ckOperations;
@end

@implementation CKKSHealKeyHierarchyOperation
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
              allowFullRefetchResult:(BOOL)allowFullRefetchResult
                           intending:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;
        _allowFullRefetchResult = allowFullRefetchResult;

        _intendedState = intendedState;
        _nextState = errorState;

        _newCloudKitRecordsWritten = NO;
        _ckOperations = [NSHashTable weakObjectsHashTable];
    }
    return self;
}

- (void)groupStart {
    /*
     * We've been invoked because something is wonky with a key hierarchy.
     *
     * Attempt to figure out what it is, and what we can do about it.
     *
     * The answer "nothing, everything is terrible" is acceptable.
     */

    NSArray<CKKSPeerProviderState*>* currentTrustStates = nil;

    WEAKIFY(self);
    self.setResultStateOperation = [CKKSResultOperation named:@"determine-next-state" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);

        if(self.cloudkitWriteFailures) {
            ckksnotice_global("ckksheal", "Due to write failures, we'll try to fetch the current state");
           self.nextState = CKKSStateBeginFetch;

        } else if(self.newCloudKitRecordsWritten) {
            ckksnotice_global("ckksheal", "Some records were written! Process them");
            self.nextState = CKKSStateProcessReceivedKeys;

        } else {
            self.nextState = self.intendedState;
        }
    }];

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        if(![viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateUnhealthy]) {
            ckksnotice("ckksheal", viewState.zoneID, "View %@ is in okay state; ignoring", viewState);
            continue;
        }

        if(currentTrustStates == nil) {
            currentTrustStates = self.deps.currentTrustStates;
        }

        [self attemptToHealView:viewState
             currentTrustStates:currentTrustStates];
    }

    [self runBeforeGroupFinished:self.setResultStateOperation];
}

- (void)attemptToHealView:(CKKSKeychainViewState*)viewState
       currentTrustStates:(NSArray<CKKSPeerProviderState*>*)currentTrustStates
{
    WEAKIFY(self);
    [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        ckksnotice("ckksheal", viewState.zoneID, "Attempting to heal %@", viewState);

        NSError* error = nil;

        CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID];

        bool changedCurrentTLK = false;
        bool changedCurrentClassA = false;
        bool changedCurrentClassC = false;

        if(keyset.error) {
            self.error = keyset.error;
            ckkserror("ckksheal", viewState.zoneID, "couldn't load current key set, attempting to proceed: %@", keyset.error);
        } else {
            ckksnotice("ckksheal", viewState.zoneID, "Key set is %@", keyset);
        }

        // There's all sorts of brokenness that could exist. For now, we check for:
        //
        //   1. Current key pointers are nil.
        //   2. Keys do not exist in local keychain (but TLK does)
        //   3. Keys do not exist in local keychain (including TLK)
        //   4. Class A or Class C keys do not wrap immediately to top TLK.
        //   5. Any incoming queue items that point to a key we don't recognize.
        //

        if(keyset.currentTLKPointer && keyset.currentClassAPointer && keyset.currentClassCPointer &&
           (!keyset.tlk || !keyset.classA || !keyset.classC)) {
            // Huh. No keys, but some current key pointers? Weird.
            // If we haven't done one yet, initiate a refetch of everything from cloudkit, and write down that we did so
            if(self.allowFullRefetchResult) {
                ckksnotice("ckksheal", viewState.zoneID, "Have current key pointers, but no keys. This is exceptional; requesting full refetch");
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateNeedFullRefetch;
                return CKKSDatabaseTransactionCommit;
            }
        }

        NSError* parentKeyUUIDsError = nil;
        BOOL allIQEsHaveKeys = [CKKSIncomingQueueEntry allIQEsHaveValidUnwrappingKeys:viewState.zoneID error:&parentKeyUUIDsError];

        if(parentKeyUUIDsError != nil) {
            ckkserror("ckksheal", viewState.zoneID, "Unable to determine if all IQEs have parent keys: %@", parentKeyUUIDsError);
        } else if(!allIQEsHaveKeys) {
            if(self.allowFullRefetchResult) {
                ckksnotice("ckksheal", viewState.zoneID, "We have some item that encrypts to a non-existent key. This is exceptional; requesting full refetch");
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateNeedFullRefetch;
                return CKKSDatabaseTransactionCommit;
            } else {
                ckksnotice("ckksheal", viewState.zoneID, "We have some item that encrypts to a non-existent key, but we cannot request a refetch! Possible inifinite-loop ahead");
            }
        }

        // No current key records. That's... odd.
        if(!keyset.currentTLKPointer) {
            ckksnotice("ckksheal", viewState.zoneID, "No current TLK pointer?");
            keyset.currentTLKPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassTLK currentKeyUUID:nil zoneID:viewState.zoneID encodedCKRecord:nil];
        }
        if(!keyset.currentClassAPointer) {
            ckksnotice("ckksheal", viewState.zoneID, "No current ClassA pointer?");
            keyset.currentClassAPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassA currentKeyUUID:nil zoneID:viewState.zoneID encodedCKRecord:nil];
        }
        if(!keyset.currentClassCPointer) {
            ckksnotice("ckksheal", viewState.zoneID, "No current ClassC pointer?");
            keyset.currentClassCPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassC currentKeyUUID:nil zoneID:viewState.zoneID encodedCKRecord:nil];
        }


        if(keyset.currentTLKPointer.currentKeyUUID == nil || keyset.currentClassAPointer.currentKeyUUID == nil || keyset.currentClassCPointer.currentKeyUUID == nil ||
           keyset.tlk == nil || keyset.classA == nil || keyset.classC == nil ||
           ![keyset.classA.parentKeyUUID isEqualToString: keyset.tlk.uuid] || ![keyset.classC.parentKeyUUID isEqualToString: keyset.tlk.uuid]) {

            // The records exist, but are broken. Point them at something reasonable.
            NSArray<CKKSKey*>* keys = [CKKSKey allKeys:viewState.zoneID error:&error];

            CKKSKey* newTLK = nil;
            CKKSKey* newClassAKey = nil;
            CKKSKey* newClassCKey = nil;

            NSMutableArray<CKRecord *>* recordsToSave = [[NSMutableArray alloc] init];
            NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];

            // Find the current top local key. That's our new TLK.
            for(CKKSKey* key in keys) {
                CKKSKey* topKey = [key topKeyInAnyState: &error];
                if(newTLK == nil) {
                    newTLK = topKey;
                } else if(![newTLK.uuid isEqualToString: topKey.uuid]) {
                    ckkserror("ckksheal", viewState.zoneID, "key hierarchy has split: there's two top keys. Currently we don't handle this situation.");
                    self.error = [NSError errorWithDomain:CKKSErrorDomain
                                                     code:CKKSSplitKeyHierarchy
                                              description:[NSString stringWithFormat:@"Key hierarchy has split: %@ and %@ are roots", newTLK, topKey]];
                    self.nextState = CKKSStateError;
                    return CKKSDatabaseTransactionCommit;
                }
            }

            if(!newTLK) {
                // We don't have any TLKs lying around, but we're supposed to heal the key hierarchy. This isn't any good; let's wait for TLK creation.
                ckkserror("ckksheal", viewState.zoneID, "No possible TLK found. Waiting for creation.");
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForTLKCreation;
                return CKKSDatabaseTransactionCommit;
            }

            if(![newTLK validTLK:&error]) {
                // Something has gone horribly wrong. Enter error state.
                ckkserror("ckkskey", viewState.zoneID, "CKKS claims %@ is not a valid TLK: %@", newTLK, error);
                self.error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"Invalid TLK from CloudKit (during heal)" underlying:error];
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                return CKKSDatabaseTransactionCommit;
            }

            // This key is our proposed TLK.
            if(![newTLK tlkMaterialPresentOrRecoverableViaTLKShare:currentTrustStates
                                                          error:&error]) {
                // TLK is valid, but not present locally
                if(error && [self.deps.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckkskey", viewState.zoneID, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", newTLK);
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;

                } else {
                    ckksnotice("ckkskey", viewState.zoneID, "Received a TLK(%@) which we don't have in the local keychain: %@", newTLK, error);
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateTLKMissing;
                }
                return CKKSDatabaseTransactionCommit;
            }

            // We have our new TLK.
            if(![keyset.currentTLKPointer.currentKeyUUID isEqualToString: newTLK.uuid]) {
                // And it's even actually new!
                keyset.tlk = newTLK;
                keyset.currentTLKPointer.currentKeyUUID = newTLK.uuid;
                changedCurrentTLK = true;
            }

            // Find some class A and class C keys directly under this one.
            for(CKKSKey* key in keys) {
                if([key.parentKeyUUID isEqualToString: newTLK.uuid]) {
                    if([key.keyclass isEqualToString: SecCKKSKeyClassA] &&
                           (keyset.currentClassAPointer.currentKeyUUID == nil ||
                            ![keyset.classA.parentKeyUUID isEqualToString: newTLK.uuid] ||
                            keyset.classA == nil)
                       ) {
                        keyset.classA = key;
                        keyset.currentClassAPointer.currentKeyUUID = key.uuid;
                        changedCurrentClassA = true;
                    }

                    if([key.keyclass isEqualToString: SecCKKSKeyClassC] &&
                            (keyset.currentClassCPointer.currentKeyUUID == nil ||
                             ![keyset.classC.parentKeyUUID isEqualToString: newTLK.uuid] ||
                             keyset.classC == nil)
                       ) {
                        keyset.classC = key;
                        keyset.currentClassCPointer.currentKeyUUID = key.uuid;
                        changedCurrentClassC = true;
                    }
                }
            }

            if(!keyset.currentClassAPointer.currentKeyUUID) {
                newClassAKey = [CKKSKey randomKeyWrappedByParent:newTLK keyclass:SecCKKSKeyClassA error:&error];
                [newClassAKey saveKeyMaterialToKeychain:&error];

                if(error && [self.deps.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckksheal", viewState.zoneID, "Couldn't create a new class A key, but keybag appears to be locked. Entering waitforunlock.");
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;
                    return CKKSDatabaseTransactionCommit;
                } else if(error) {
                    ckkserror("ckksheal", viewState.zoneID, "couldn't create new classA key: %@", error);
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                    return CKKSDatabaseTransactionCommit;
                }

                keyset.classA = newClassAKey;
                keyset.currentClassAPointer.currentKeyUUID = newClassAKey.uuid;
                changedCurrentClassA = true;
            }
            if(!keyset.currentClassCPointer.currentKeyUUID) {
                newClassCKey = [CKKSKey randomKeyWrappedByParent:newTLK keyclass:SecCKKSKeyClassC error:&error];
                [newClassCKey saveKeyMaterialToKeychain:&error];

                if(error && [self.deps.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckksheal", viewState.zoneID, "Couldn't create a new class C key, but keybag appears to be locked. Entering waitforunlock.");
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;
                    return CKKSDatabaseTransactionCommit;
                } else if(error) {
                    ckkserror("ckksheal", viewState.zoneID, "couldn't create new class C key: %@", error);
                    viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                    return CKKSDatabaseTransactionCommit;
                }

                keyset.classC = newClassCKey;
                keyset.currentClassCPointer.currentKeyUUID = newClassCKey.uuid;
                changedCurrentClassC = true;
            }

            ckksnotice("ckksheal", viewState.zoneID, "Attempting to move to new key hierarchy: %@", keyset);

            // Note: we never make a new TLK here. So, don't save it back to CloudKit.
            //if(newTLK) {
            //    [recordsToSave addObject: [newTLK       CKRecordWithZoneID: viewState.zoneID]];
            //}
            if(newClassAKey) {
                [recordsToSave addObject: [newClassAKey CKRecordWithZoneID: viewState.zoneID]];
            }
            if(newClassCKey) {
                [recordsToSave addObject: [newClassCKey CKRecordWithZoneID: viewState.zoneID]];
            }

            if(changedCurrentTLK) {
                [recordsToSave addObject: [keyset.currentTLKPointer    CKRecordWithZoneID: viewState.zoneID]];
            }
            if(changedCurrentClassA) {
                [recordsToSave addObject: [keyset.currentClassAPointer CKRecordWithZoneID: viewState.zoneID]];
            }
            if(changedCurrentClassC) {
                [recordsToSave addObject: [keyset.currentClassCPointer CKRecordWithZoneID: viewState.zoneID]];
            }

            // We've selected a new TLK. Compute any TLKShares that should go along with it.
            // Since we're on a transaction already on this thread, don't pass in a databaseProvider.
            NSSet<CKKSTLKShareRecord*>* tlkShares = [CKKSHealTLKSharesOperation createMissingKeyShares:keyset
                                                                                           trustStates:currentTrustStates
                                                                                      databaseProvider:nil
                                                                                                 error:&error];
            if(error) {
                ckkserror("ckksshare", viewState.zoneID, "Unable to create TLK shares for new tlk: %@", error);
                return CKKSDatabaseTransactionRollback;
            }

            for(CKKSTLKShareRecord* share in tlkShares) {
                CKRecord* record = [share CKRecordWithZoneID:viewState.zoneID];
                [recordsToSave addObject: record];
            }

            // Kick off the CKOperation

            ckksnotice("ckksheal", viewState.zoneID, "Saving new records %@", recordsToSave);

            // Use the spare operation trick to wait for the CKModifyRecordsOperation to complete
            NSBlockOperation* cloudKitModifyOperationFinished = [NSBlockOperation named:[NSString stringWithFormat:@"heal-cloudkit-modify-operation-finished-%@", viewState.zoneName]
                                                                              withBlock:^{}];
            [self dependOnBeforeGroupFinished:cloudKitModifyOperationFinished];

            CKModifyRecordsOperation* modifyRecordsOp = nil;

            NSMutableDictionary<CKRecordID*, CKRecord*>* attemptedRecords = [[NSMutableDictionary alloc] init];
            for(CKRecord* record in recordsToSave) {
                attemptedRecords[record.recordID] = record;
            }

            // Get the CloudKit operation ready...
            modifyRecordsOp = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave recordIDsToDelete:recordIDsToDelete];
            modifyRecordsOp.atomic = YES;
            modifyRecordsOp.longLived = NO; // The keys are only in memory; mark this explicitly not long-lived

            // This needs to happen for CKKS to be usable by PCS/cloudd. Make it happen.
            modifyRecordsOp.configuration.automaticallyRetryNetworkFailures = NO;
            modifyRecordsOp.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
            modifyRecordsOp.configuration.isCloudKitSupportOperation = YES;

            modifyRecordsOp.group = self.deps.ckoperationGroup;
            ckksnotice("ckksheal", viewState.zoneID, "Operation group is %@", self.deps.ckoperationGroup);

            modifyRecordsOp.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
                // These should all fail or succeed as one. Do the hard work in the records completion block.
                if(!error) {
                    ckksnotice("ckksheal", viewState.zoneID, "Successfully completed upload for %@", record.recordID.recordName);
                } else {
                    ckkserror("ckksheal", viewState.zoneID, "error on row: %@ %@", error, record);
                }
            };

            modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
                STRONGIFY(self);

                ckksnotice("ckksheal", viewState.zoneID, "Completed Key Heal CloudKit operation with error: %@", error);

                [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    if(error == nil) {
                        [[CKKSAnalytics logger] logSuccessForEvent:CKKSEventProcessHealKeyHierarchy zoneName:viewState.zoneID.zoneName];
                        // Success. Persist the keys to the CKKS database.

                        // Save the new CKRecords to the before persisting to database
                        for(CKRecord* record in savedRecords) {
                            if([newTLK matchesCKRecord: record]) {
                                newTLK.storedCKRecord = record;
                            } else if([newClassAKey matchesCKRecord: record]) {
                                newClassAKey.storedCKRecord = record;
                            } else if([newClassCKey matchesCKRecord: record]) {
                                newClassCKey.storedCKRecord = record;

                            } else if([keyset.currentTLKPointer matchesCKRecord: record]) {
                                keyset.currentTLKPointer.storedCKRecord = record;
                            } else if([keyset.currentClassAPointer matchesCKRecord: record]) {
                                keyset.currentClassAPointer.storedCKRecord = record;
                            } else if([keyset.currentClassCPointer matchesCKRecord: record]) {
                                keyset.currentClassCPointer.storedCKRecord = record;
                            }
                        }

                        NSError* localerror = nil;

                        [newTLK       saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];
                        [newClassAKey saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];
                        [newClassCKey saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];

                        [keyset.currentTLKPointer    saveToDatabase: &localerror];
                        [keyset.currentClassAPointer saveToDatabase: &localerror];
                        [keyset.currentClassCPointer saveToDatabase: &localerror];

                        // save all the TLKShares, too
                        for(CKKSTLKShareRecord* share in tlkShares) {
                            [share saveToDatabase:&localerror];
                        }

                        if(localerror != nil) {
                            ckkserror("ckksheal", viewState.zoneID, "couldn't save new key hierarchy to database; this is very bad: %@", localerror);
                            viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                            return CKKSDatabaseTransactionRollback;
                        } else {
                            // Everything is groovy. HOWEVER, we might still not have processed the keys. Ask for that!
                            viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateProcess;
                            self.newCloudKitRecordsWritten = YES;
                        }
                    } else {
                        // ERROR. This isn't a total-failure error state, but one that should kick off a healing process.
                        [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:CKKSEventProcessHealKeyHierarchy zoneName:viewState.zoneID.zoneName withAttributes:NULL];
                        ckkserror("ckksheal", viewState.zoneID, "couldn't save new key hierarchy to CloudKit: %@", error);

                        [self.deps intransactionCKWriteFailed:error attemptedRecordsChanged:attemptedRecords];

                        self.cloudkitWriteFailures = YES;
                    }
                    return CKKSDatabaseTransactionCommit;
                }];

                // Notify when we're done
                [self.operationQueue addOperation:cloudKitModifyOperationFinished];
            };

            [self.setResultStateOperation addDependency:cloudKitModifyOperationFinished];

            [modifyRecordsOp linearDependencies:self.ckOperations];
            [self.deps.ckdatabase addOperation:modifyRecordsOp];
            return true;
        }

        // Check if CKKS can recover this TLK.

        if(![keyset.tlk validTLK:&error]) {
            // Something has gone horribly wrong. Enter error state.
            NSError* logError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"Invalid TLK from CloudKit (during heal)" underlying:error];
            ckkserror("ckkskey", viewState.zoneID, "CKKS claims %@ is not a valid TLK: %@", keyset.tlk, logError);
            viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
            return CKKSDatabaseTransactionCommit;
        }

        // This key is our proposed TLK.
        if(![keyset.tlk tlkMaterialPresentOrRecoverableViaTLKShare:currentTrustStates
                                                             error:&error]) {
            // TLK is valid, but not present locally
            if(error && [self.deps.lockStateTracker isLockedError:error]) {
                ckksnotice("ckkskey", viewState.zoneID, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", keyset.tlk);
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;
            } else {
                ckksnotice("ckkskey", viewState.zoneID, "Received a TLK(%@) which we don't have in the local keychain: %@", keyset.tlk, error);
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateTLKMissing;
            }
            return CKKSDatabaseTransactionCommit;
        }

        if(![self ensureKeyPresent:keyset.tlk viewState:viewState]) {
            return CKKSDatabaseTransactionRollback;
        }

        if(![self ensureKeyPresent:keyset.classA viewState:viewState]) {
            return CKKSDatabaseTransactionRollback;
        }

        if(![self ensureKeyPresent:keyset.classC viewState:viewState]) {
            return CKKSDatabaseTransactionRollback;
        }

        viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateReady;
        return CKKSDatabaseTransactionCommit;
    }];
}

- (BOOL)ensureKeyPresent:(CKKSKey*)key viewState:(CKKSKeychainViewState*)viewState
{
    NSError* loadError = nil;

    if(![key loadKeyMaterialFromKeychain:&loadError]) {
        ckkserror("ckksheal", viewState.zoneID, "Couldn't load key(%@) from keychain. Attempting recovery: %@", key, loadError);

        NSError* unwrapError = nil;
        if(![key unwrapViaKeyHierarchy:&unwrapError]) {
            if([self.deps.lockStateTracker isLockedError:unwrapError]) {
                ckkserror("ckksheal", viewState.zoneID, "Couldn't unwrap key(%@) using key hierarchy due to the lock state: %@", key, unwrapError);
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;
                return false;
            }
            ckkserror("ckksheal", viewState.zoneID, "Couldn't unwrap key(%@) using key hierarchy. Keys are broken, quitting: %@", key, unwrapError);
            viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
            return false;
        }
        NSError* saveError = nil;
        if(![key saveKeyMaterialToKeychain:&saveError]) {
            if([self.deps.lockStateTracker isLockedError:saveError]) {
                ckkserror("ckksheal", viewState.zoneID, "Couldn't save key(%@) to keychain due to the lock state: %@", key, saveError);
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForUnlock;
                return false;
            }
            ckkserror("ckksheal", viewState.zoneID, "Couldn't save key(%@) to keychain: %@", key, saveError);
            viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
            return false;
        }
    }
    return true;
}

@end;

#endif
