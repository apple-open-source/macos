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
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"

#if OCTAGON

@interface CKKSHealKeyHierarchyOperation ()
@property NSBlockOperation* cloudkitModifyOperationFinished;
@end

@implementation CKKSHealKeyHierarchyOperation
@synthesize intendedState = _intendedState;

- (instancetype)init {
    return nil;
}

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)dependencies
                                ckks:(CKKSKeychainView*)ckks
                           intending:(OctagonState*)intendedState
                          errorState:(OctagonState*)errorState
{
    if((self = [super init])) {
        _deps = dependencies;
        _ckks = ckks;

        _intendedState = intendedState;
        _nextState = errorState;
    }
    return self;
}

- (void)groupStart {
    /*
     * We've been invoked because something is wonky with the key hierarchy.
     *
     * Attempt to figure out what it is, and what we can do about it.
     *
     * The answer "nothing, everything is terrible" is acceptable.
     */

    WEAKIFY(self);

    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksheal", ckks, "no CKKS object");
        return;
    }

    if(self.cancelled) {
        ckksnotice("ckksheal", ckks, "CKKSHealKeyHierarchyOperation cancelled, quitting");
        return;
    }

    NSArray<CKKSPeerProviderState*>* currentTrustStates = self.deps.currentTrustStates;

    // Synchronous, on some thread. Get back on the CKKS queue for SQL thread-safety.
    [ckks dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
        if(self.cancelled) {
            ckksnotice("ckksheal", ckks, "CKKSHealKeyHierarchyOperation cancelled, quitting");
            return CKKSDatabaseTransactionRollback;
        }

        NSError* error = nil;

        CKKSCurrentKeySet* keyset = [CKKSCurrentKeySet loadForZone:ckks.zoneID];

        bool changedCurrentTLK = false;
        bool changedCurrentClassA = false;
        bool changedCurrentClassC = false;

        if(keyset.error) {
            self.error = keyset.error;
            ckkserror("ckksheal", ckks, "couldn't load current key set, attempting to proceed: %@", keyset.error);
        } else {
            ckksnotice("ckksheal", ckks, "Key set is %@", keyset);
        }

        // There's all sorts of brokenness that could exist. For now, we check for:
        //
        //   1. Current key pointers are nil.
        //   2. Keys do not exist in local keychain (but TLK does)
        //   3. Keys do not exist in local keychain (including TLK)
        //   4. Class A or Class C keys do not wrap immediately to top TLK.
        //

        if(keyset.currentTLKPointer && keyset.currentClassAPointer && keyset.currentClassCPointer &&
           (!keyset.tlk || !keyset.classA || !keyset.classC)) {
            // Huh. No keys, but some current key pointers? Weird.
            // If we haven't done one yet, initiate a refetch of everything from cloudkit, and write down that we did so
            if(!ckks.keyStateMachineRefetched) {
                ckksnotice("ckksheal", ckks, "Have current key pointers, but no keys. This is exceptional; requesting full refetch");
                self.nextState = SecCKKSZoneKeyStateNeedFullRefetch;
                return CKKSDatabaseTransactionCommit;
            }
        }

        // No current key records. That's... odd.
        if(!keyset.currentTLKPointer) {
            ckksnotice("ckksheal", ckks, "No current TLK pointer?");
            keyset.currentTLKPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassTLK currentKeyUUID:nil zoneID:ckks.zoneID encodedCKRecord:nil];
        }
        if(!keyset.currentClassAPointer) {
            ckksnotice("ckksheal", ckks, "No current ClassA pointer?");
            keyset.currentClassAPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassA currentKeyUUID:nil zoneID:ckks.zoneID encodedCKRecord:nil];
        }
        if(!keyset.currentClassCPointer) {
            ckksnotice("ckksheal", ckks, "No current ClassC pointer?");
            keyset.currentClassCPointer = [[CKKSCurrentKeyPointer alloc] initForClass: SecCKKSKeyClassC currentKeyUUID:nil zoneID:ckks.zoneID encodedCKRecord:nil];
        }


        if(keyset.currentTLKPointer.currentKeyUUID == nil || keyset.currentClassAPointer.currentKeyUUID == nil || keyset.currentClassCPointer.currentKeyUUID == nil ||
           keyset.tlk == nil || keyset.classA == nil || keyset.classC == nil ||
           ![keyset.classA.parentKeyUUID isEqualToString: keyset.tlk.uuid] || ![keyset.classC.parentKeyUUID isEqualToString: keyset.tlk.uuid]) {

            // The records exist, but are broken. Point them at something reasonable.
            NSArray<CKKSKey*>* keys = [CKKSKey allKeys:ckks.zoneID error:&error];

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
                    ckkserror("ckksheal", ckks, "key hierarchy has split: there's two top keys. Currently we don't handle this situation.");
                    self.error = [NSError errorWithDomain:CKKSErrorDomain
                                                     code:CKKSSplitKeyHierarchy
                                              description:[NSString stringWithFormat:@"Key hierarchy has split: %@ and %@ are roots", newTLK, topKey]];
                    self.nextState = SecCKKSZoneKeyStateError;
                    return CKKSDatabaseTransactionCommit;
                }
            }

            if(!newTLK) {
                // We don't have any TLKs lying around, but we're supposed to heal the key hierarchy. This isn't any good; let's wait for TLK creation.
                ckkserror("ckksheal", ckks, "No possible TLK found. Waiting for creation.");
                self.nextState = SecCKKSZoneKeyStateWaitForTLKCreation;
                return CKKSDatabaseTransactionCommit;
            }

            if(![newTLK validTLK:&error]) {
                // Something has gone horribly wrong. Enter error state.
                ckkserror("ckkskey", ckks, "CKKS claims %@ is not a valid TLK: %@", newTLK, error);
                self.error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"Invalid TLK from CloudKit (during heal)" underlying:error];
                self.nextState = SecCKKSZoneKeyStateError;
                return CKKSDatabaseTransactionCommit;
            }

            // This key is our proposed TLK.
            if(![newTLK tlkMaterialPresentOrRecoverableViaTLKShare:currentTrustStates
                                                          error:&error]) {
                // TLK is valid, but not present locally
                if(error && [self.deps.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckkskey", ckks, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", newTLK);
                    self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
                } else {
                    ckksnotice("ckkskey", ckks, "Received a TLK(%@) which we don't have in the local keychain: %@", newTLK, error);
                    self.error = error;
                    self.nextState = SecCKKSZoneKeyStateTLKMissing;
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
                newClassAKey = [CKKSKey randomKeyWrappedByParent:newTLK error:&error];
                [newClassAKey saveKeyMaterialToKeychain:&error];

                if(error && [ckks.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckksheal", ckks, "Couldn't create a new class A key, but keybag appears to be locked. Entering waitforunlock.");
                    self.error = error;
                    self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
                    return CKKSDatabaseTransactionCommit;
                } else if(error) {
                    ckkserror("ckksheal", ckks, "couldn't create new classA key: %@", error);
                    self.error = error;
                    self.nextState = SecCKKSZoneKeyStateError;
                    return CKKSDatabaseTransactionCommit;
                }

                keyset.classA = newClassAKey;
                keyset.currentClassAPointer.currentKeyUUID = newClassAKey.uuid;
                changedCurrentClassA = true;
            }
            if(!keyset.currentClassCPointer.currentKeyUUID) {
                newClassCKey = [CKKSKey randomKeyWrappedByParent:newTLK error:&error];
                [newClassCKey saveKeyMaterialToKeychain:&error];

                if(error && [ckks.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckksheal", ckks, "Couldn't create a new class C key, but keybag appears to be locked. Entering waitforunlock.");
                    self.error = error;
                    self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
                    return CKKSDatabaseTransactionCommit;
                } else if(error) {
                    ckkserror("ckksheal", ckks, "couldn't create new class C key: %@", error);
                    self.error = error;
                    self.nextState = SecCKKSZoneKeyStateError;
                    return CKKSDatabaseTransactionCommit;
                }

                keyset.classC = newClassCKey;
                keyset.currentClassCPointer.currentKeyUUID = newClassCKey.uuid;
                changedCurrentClassC = true;
            }

            ckksnotice("ckksheal", ckks, "Attempting to move to new key hierarchy: %@", keyset);

            // Note: we never make a new TLK here. So, don't save it back to CloudKit.
            //if(newTLK) {
            //    [recordsToSave addObject: [newTLK       CKRecordWithZoneID: ckks.zoneID]];
            //}
            if(newClassAKey) {
                [recordsToSave addObject: [newClassAKey CKRecordWithZoneID: ckks.zoneID]];
            }
            if(newClassCKey) {
                [recordsToSave addObject: [newClassCKey CKRecordWithZoneID: ckks.zoneID]];
            }

            if(changedCurrentTLK) {
                [recordsToSave addObject: [keyset.currentTLKPointer    CKRecordWithZoneID: ckks.zoneID]];
            }
            if(changedCurrentClassA) {
                [recordsToSave addObject: [keyset.currentClassAPointer CKRecordWithZoneID: ckks.zoneID]];
            }
            if(changedCurrentClassC) {
                [recordsToSave addObject: [keyset.currentClassCPointer CKRecordWithZoneID: ckks.zoneID]];
            }

            // We've selected a new TLK. Compute any TLKShares that should go along with it.
            NSSet<CKKSTLKShareRecord*>* tlkShares = [CKKSHealTLKSharesOperation createMissingKeyShares:keyset
                                                                                           trustStates:currentTrustStates
                                                                                                 error:&error];
            if(error) {
                ckkserror("ckksshare", ckks, "Unable to create TLK shares for new tlk: %@", error);
                return CKKSDatabaseTransactionRollback;
            }

            for(CKKSTLKShareRecord* share in tlkShares) {
                CKRecord* record = [share CKRecordWithZoneID:ckks.zoneID];
                [recordsToSave addObject: record];
            }

            // Kick off the CKOperation

            ckksnotice("ckksheal", ckks, "Saving new records %@", recordsToSave);

            // Use the spare operation trick to wait for the CKModifyRecordsOperation to complete
            self.cloudkitModifyOperationFinished = [NSBlockOperation named:@"heal-cloudkit-modify-operation-finished" withBlock:^{}];
            [self dependOnBeforeGroupFinished: self.cloudkitModifyOperationFinished];

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
            ckksnotice("ckksheal", ckks, "Operation group is %@", self.deps.ckoperationGroup);

            modifyRecordsOp.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
                STRONGIFY(self);
                CKKSKeychainView* blockCKKS = self.ckks;

                // These should all fail or succeed as one. Do the hard work in the records completion block.
                if(!error) {
                    ckksnotice("ckksheal", blockCKKS, "Successfully completed upload for %@", record.recordID.recordName);
                } else {
                    ckkserror("ckksheal", blockCKKS, "error on row: %@ %@", error, record);
                }
            };

            modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
                STRONGIFY(self);
                CKKSKeychainView* strongCKKS = self.ckks;
                if(!self) {
                    ckkserror_global("ckks", "received callback for released object");
                    return;
                }

                ckksnotice("ckksheal", strongCKKS, "Completed Key Heal CloudKit operation with error: %@", error);

                [strongCKKS dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    if(error == nil) {
                        [[CKKSAnalytics logger] logSuccessForEvent:CKKSEventProcessHealKeyHierarchy zoneName:ckks.zoneName];
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
                            ckkserror("ckksheal", strongCKKS, "couldn't save new key hierarchy to database; this is very bad: %@", localerror);
                            self.error = localerror;
                            self.nextState = SecCKKSZoneKeyStateError;
                            return CKKSDatabaseTransactionRollback;
                        } else {
                            // Everything is groovy. HOWEVER, we might still not have processed the keys. Ask for that!
                            self.nextState = SecCKKSZoneKeyStateProcess;
                        }
                    } else {
                        // ERROR. This isn't a total-failure error state, but one that should kick off a healing process.
                        [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:CKKSEventProcessHealKeyHierarchy zoneName:ckks.zoneName withAttributes:NULL];
                        ckkserror("ckksheal", strongCKKS, "couldn't save new key hierarchy to CloudKit: %@", error);
                        [strongCKKS _onqueueCKWriteFailed:error attemptedRecordsChanged:attemptedRecords];

                        self.nextState = SecCKKSZoneKeyStateNewTLKsFailed;
                    }
                    return CKKSDatabaseTransactionCommit;
                }];

                // Notify that we're done
                [self.operationQueue addOperation: self.cloudkitModifyOperationFinished];
            };

            [self.deps.ckdatabase addOperation:modifyRecordsOp];
            return true;
        }

        // Check if CKKS can recover this TLK.

        if(![keyset.tlk validTLK:&error]) {
            // Something has gone horribly wrong. Enter error state.
            ckkserror("ckkskey", ckks, "CKKS claims %@ is not a valid TLK: %@", keyset.tlk, error);
            self.error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"Invalid TLK from CloudKit (during heal)" underlying:error];
            self.nextState = SecCKKSZoneKeyStateError;
            return CKKSDatabaseTransactionCommit;
        }

        // This key is our proposed TLK.
        if(![keyset.tlk tlkMaterialPresentOrRecoverableViaTLKShare:currentTrustStates
                                                             error:&error]) {
            // TLK is valid, but not present locally
            if(error && [self.deps.lockStateTracker isLockedError:error]) {
                ckksnotice("ckkskey", ckks, "Received a TLK(%@), but keybag appears to be locked. Entering a waiting state.", keyset.tlk);
                self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
            } else {
                ckksnotice("ckkskey", ckks, "Received a TLK(%@) which we don't have in the local keychain: %@", keyset.tlk, error);
                self.error = error;
                self.nextState = SecCKKSZoneKeyStateTLKMissing;
            }
            return CKKSDatabaseTransactionCommit;
        }

        if(![self ensureKeyPresent:keyset.tlk]) {
            return CKKSDatabaseTransactionRollback;
        }

        if(![self ensureKeyPresent:keyset.classA]) {
            return CKKSDatabaseTransactionRollback;
        }

        if(![self ensureKeyPresent:keyset.classC]) {
            return CKKSDatabaseTransactionRollback;
        }

        // Seems good to us. Check if we're ready?
        self.nextState = self.intendedState;

        return CKKSDatabaseTransactionCommit;
    }];
}

- (bool)ensureKeyPresent:(CKKSKey*)key {
    NSError* error = nil;
    CKKSKeychainView* ckks = self.ckks;

    [key loadKeyMaterialFromKeychain:&error];
    if(error) {
        ckkserror("ckksheal", ckks, "Couldn't load key(%@) from keychain. Attempting recovery: %@", key, error);
        error = nil;
        [key unwrapViaKeyHierarchy: &error];
        if(error) {
            if([ckks.lockStateTracker isLockedError:error]) {
                ckkserror("ckksheal", ckks, "Couldn't unwrap key(%@) using key hierarchy due to the lock state: %@", key, error);
                self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
                self.error = error;
                return false;
            }
            ckkserror("ckksheal", ckks, "Couldn't unwrap key(%@) using key hierarchy. Keys are broken, quitting: %@", key, error);
            self.error = error;
            self.nextState = SecCKKSZoneKeyStateError;
            self.error = error;
            return false;
        }
        [key saveKeyMaterialToKeychain:&error];
        if(error) {
            if([ckks.lockStateTracker isLockedError:error]) {
                ckkserror("ckksheal", ckks, "Couldn't save key(%@) to keychain due to the lock state: %@", key, error);
                self.nextState = SecCKKSZoneKeyStateWaitForUnlock;
                self.error = error;
                return false;
            }
            ckkserror("ckksheal", ckks, "Couldn't save key(%@) to keychain: %@", key, error);
            self.error = error;
            self.nextState = SecCKKSZoneKeyStateError;
            return false;
        }
    }
    return true;
}

- (void)cancel {
    [self.cloudkitModifyOperationFinished cancel];
    [super cancel];
}

@end;

#endif
