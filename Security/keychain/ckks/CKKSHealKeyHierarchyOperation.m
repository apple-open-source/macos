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

#if OCTAGON

@interface CKKSHealKeyHierarchyOperation ()
@property NSBlockOperation* cloudkitModifyOperationFinished;
@property CKOperationGroup* ckoperationGroup;
@end

@implementation CKKSHealKeyHierarchyOperation

- (instancetype)init {
    return nil;
}
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks ckoperationGroup:(CKOperationGroup*)ckoperationGroup {
    if(self = [super init]) {
        _ckks = ckks;
        _ckoperationGroup = ckoperationGroup;
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

    __weak __typeof(self) weakSelf = self;

    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksheal", ckks, "no CKKS object");
        return;
    }

    if(self.cancelled) {
        ckksnotice("ckksheal", ckks, "CKKSHealKeyHierarchyOperation cancelled, quitting");
        return;
    }

    // Synchronous, on some thread. Get back on the CKKS queue for SQL thread-safety.
    [ckks dispatchSyncWithAccountKeys: ^bool{
        if(self.cancelled) {
            ckksnotice("ckksheal", ckks, "CKKSHealKeyHierarchyOperation cancelled, quitting");
            return false;
        }

        NSError* error = nil;

        CKKSCurrentKeySet* keyset = [[CKKSCurrentKeySet alloc] initForZone:ckks.zoneID];

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
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateNeedFullRefetch withError:nil];
                return true;
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
                    [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: [NSError errorWithDomain:CKKSErrorDomain
                                                                                                                         code:CKKSSplitKeyHierarchy
                                                                                                                     userInfo:@{NSLocalizedDescriptionKey:
                                                                                                                                    [NSString stringWithFormat:@"Key hierarchy has split: %@ and %@ are roots", newTLK, topKey]}]];
                    return true;
                }
            }

            if(![ckks _onqueueWithAccountKeysCheckTLK: newTLK error: &error]) {
                // Was this error "I've never seen that TLK before in my life"? If so, enter the "wait for TLK sync" state.
                if(error && [error.domain isEqualToString: @"securityd"] && error.code == errSecItemNotFound) {
                    ckksnotice("ckksheal", ckks, "Received a TLK which we don't have in the local keychain(%@). Entering waitfortlk.", newTLK);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForTLK withError:nil];
                    return true;
                } else if(error && [ckks.lockStateTracker isLockedError:error]) {
                    ckksnotice("ckkskey", ckks, "Received a TLK(%@), but keybag appears to be locked. Entering WaitForUnlock.", newTLK);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:nil];
                    return true;

                } else {
                    // Otherwise, something has gone horribly wrong. enter error state.
                    ckkserror("ckksheal", ckks, "CKKS claims %@ is not a valid TLK: %@", newTLK, error);
                    NSError* newError = [NSError errorWithDomain:CKKSErrorDomain code:CKKSInvalidTLK description:@"Invalid TLK from CloudKit (during heal)" underlying:error];
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:newError];
                    return true;
                }
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
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:error];
                    return true;
                } else if(error) {
                    ckkserror("ckksheal", ckks, "couldn't create new classA key: %@", error);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:error];
                    return true;
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
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:error];
                    return true;
                } else if(error) {
                    ckkserror("ckksheal", ckks, "couldn't create new class C key: %@", error);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:error];
                    return true;
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
            NSSet<CKKSTLKShare*>* tlkShares = [ckks _onqueueCreateMissingKeyShares:keyset.tlk
                                                                             error:&error];
            if(error) {
                ckkserror("ckksshare", ckks, "Unable to create TLK shares for new tlk: %@", error);
                return false;
            }

            for(CKKSTLKShare* share in tlkShares) {
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
            modifyRecordsOp.qualityOfService = NSQualityOfServiceUserInitiated;  // This needs to happen for CKKS to be usable by PCS/cloudd. Make it happen.
            modifyRecordsOp.group = self.ckoperationGroup;
            ckksnotice("ckksheal", ckks, "Operation group is %@", self.ckoperationGroup);

            modifyRecordsOp.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
                __strong __typeof(weakSelf) strongSelf = weakSelf;
                __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

                // These should all fail or succeed as one. Do the hard work in the records completion block.
                if(!error) {
                    ckksnotice("ckksheal", blockCKKS, "Successfully completed upload for %@", record.recordID.recordName);
                } else {
                    ckkserror("ckksheal", blockCKKS, "error on row: %@ %@", error, record);
                }
            };

            modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
                __strong __typeof(weakSelf) strongSelf = weakSelf;
                __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
                if(!strongSelf) {
                    secerror("ckks: received callback for released object");
                    return;
                }

                ckksnotice("ckksheal", strongCKKS, "Completed Key Heal CloudKit operation with error: %@", error);

                [strongCKKS dispatchSyncWithAccountKeys: ^bool{
                    if(error == nil) {
                        [[CKKSAnalytics logger] logSuccessForEvent:CKKSEventProcessHealKeyHierarchy inView:ckks];
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
                        for(CKKSTLKShare* share in tlkShares) {
                            [share saveToDatabase:&localerror];
                        }

                        if(localerror != nil) {
                            ckkserror("ckksheal", strongCKKS, "couldn't save new key hierarchy to database; this is very bad: %@", localerror);
                            [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: localerror];
                            return false;
                        } else {
                            // Everything is groovy. HOWEVER, we might still not have processed the keys. Ask for that!
                            [strongCKKS _onqueueKeyStateMachineRequestProcess];
                            [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateReady withError: nil];
                        }
                    } else {
                        // ERROR. This isn't a total-failure error state, but one that should kick off a healing process.
                        [[CKKSAnalytics logger] logUnrecoverableError:error forEvent:CKKSEventProcessHealKeyHierarchy inView:ckks withAttributes:NULL];
                        ckkserror("ckksheal", strongCKKS, "couldn't save new key hierarchy to CloudKit: %@", error);
                        [strongCKKS _onqueueCKWriteFailed:error attemptedRecordsChanged:attemptedRecords];
                        [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateNewTLKsFailed withError: nil];
                    }
                    return true;
                }];

                // Notify that we're done
                [strongSelf.operationQueue addOperation: strongSelf.cloudkitModifyOperationFinished];
            };

            [ckks.database addOperation: modifyRecordsOp];
            return true;
        }

        // Check if CKKS can recover this TLK.
        bool haveTLK = [ckks _onqueueWithAccountKeysCheckTLK:keyset.tlk error:&error];
        if(error && [ckks.lockStateTracker isLockedError:error]) {
            ckksnotice("ckkskey", ckks, "Failed to load TLK from keychain, keybag is locked. Entering waitforunlock: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:nil];
            return false;
        } else if(error && error.code == errSecItemNotFound) {
            ckkserror("ckksheal", ckks, "CKKS couldn't find TLK, triggering move to wait state: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateWaitForTLK withError: nil];

        } else if(!haveTLK) {
            ckkserror("ckksheal", ckks, "CKKS errored examining TLK, triggering move to bad state: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:error];
            return false;
        }

        if(![self ensureKeyPresent:keyset.tlk]) {
            return false;
        }

        if(![self ensureKeyPresent:keyset.classA]) {
            return false;
        }

        if(![self ensureKeyPresent:keyset.classC]) {
            return false;
        }

        // Seems good to us. Check if we're ready?
        [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateReady withError: nil];

        return true;
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
            ckkserror("ckksheal", ckks, "Couldn't unwrap key(%@) using key hierarchy. Keys are broken, quitting: %@", key, error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            self.error = error;
            return false;
        }
        [key saveKeyMaterialToKeychain:&error];
        if(error) {
            ckkserror("ckksheal", ckks, "Couldn't save key(%@) to keychain: %@", key, error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            self.error = error;
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
