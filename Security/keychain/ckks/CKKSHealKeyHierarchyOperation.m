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
    [ckks dispatchSync: ^bool{
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
           keyset.tlk == nil || keyset.classA == nil || keyset.classC == nil) {

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
                    [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: [NSError errorWithDomain:@"securityd"
                                                                                                                         code:0
                                                                                                                     userInfo:@{NSLocalizedDescriptionKey:
                                                                                                                                    [NSString stringWithFormat:@"Key hierarchy has split: %@ and %@ are roots", newTLK, topKey]}]];
                    return true;
                }
            }

            if(![ckks checkTLK: newTLK error: &error]) {
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
                    NSError* newError = nil;
                    if(error) {
                        newError = [NSError errorWithDomain:@"securityd"
                                                       code:0
                                                   userInfo:@{NSLocalizedDescriptionKey: @"invalid TLK from CloudKit", NSUnderlyingErrorKey: error}];
                    } else {
                        newError = [NSError errorWithDomain:@"securityd"
                                                       code:0
                                                   userInfo:@{NSLocalizedDescriptionKey: @"invalid TLK from CloudKit"}];
                    }
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:newError];
                    return true;
                }
            }

            // We have our new TLK.
            keyset.currentTLKPointer.currentKeyUUID = newTLK.uuid;
            changedCurrentTLK = true;

            // Find some class A and class C keys directly under this one.
            for(CKKSKey* key in keys) {
                if([key.parentKeyUUID isEqualToString: newTLK.uuid]) {
                    if((keyset.currentClassAPointer.currentKeyUUID == nil || keyset.classA == nil) &&
                       [key.keyclass isEqualToString: SecCKKSKeyClassA]) {
                        keyset.currentClassAPointer.currentKeyUUID = key.uuid;
                        changedCurrentClassA = true;
                    }

                    if((keyset.currentClassCPointer.currentKeyUUID == nil || keyset.classC == nil) &&
                       [key.keyclass isEqualToString: SecCKKSKeyClassC]) {
                        keyset.currentClassCPointer.currentKeyUUID = key.uuid;
                        changedCurrentClassC = true;
                    }
                }
            }

            if(!keyset.currentClassAPointer.currentKeyUUID) {
                newClassAKey = [CKKSKey randomKeyWrappedByParent:newTLK error:&error];
                [newClassAKey saveKeyMaterialToKeychain:&error];

                if(error) {
                    ckkserror("ckksheal", ckks, "couldn't create new classA key: %@", error);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:[NSError errorWithDomain: @"securityd" code:0 userInfo:@{NSLocalizedDescriptionKey: @"couldn't create new classA key", NSUnderlyingErrorKey: error}]];
                }

                keyset.currentClassAPointer.currentKeyUUID = newClassAKey.uuid;
                changedCurrentClassA = true;
            }
            if(!keyset.currentClassCPointer.currentKeyUUID) {
                newClassCKey = [CKKSKey randomKeyWrappedByParent:newTLK error:&error];
                [newClassCKey saveKeyMaterialToKeychain:&error];

                if(error) {
                    ckkserror("ckksheal", ckks, "couldn't create new classC key: %@", error);
                    [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateError withError:[NSError errorWithDomain: @"securityd" code:0 userInfo:@{NSLocalizedDescriptionKey: @"couldn't create new classC key", NSUnderlyingErrorKey: error}]];
                }

                keyset.currentClassCPointer.currentKeyUUID = newClassCKey.uuid;
                changedCurrentClassC = true;
            }

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

            ckksinfo("ckksheal", ckks, "Saving new keys %@ to cloudkit %@", recordsToSave, ckks.database);

            // Use the spare operation trick to wait for the CKModifyRecordsOperation to complete
            self.cloudkitModifyOperationFinished = [NSBlockOperation named:@"heal-cloudkit-modify-operation-finished" withBlock:^{}];
            [self dependOnBeforeGroupFinished: self.cloudkitModifyOperationFinished];

            CKModifyRecordsOperation* modifyRecordsOp = nil;

            // Get the CloudKit operation ready...
            modifyRecordsOp = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave recordIDsToDelete:recordIDsToDelete];
            modifyRecordsOp.atomic = YES;
            modifyRecordsOp.longLived = NO; // The keys are only in memory; mark this explicitly not long-lived
            modifyRecordsOp.timeoutIntervalForRequest = 2;
            modifyRecordsOp.qualityOfService = NSQualityOfServiceUtility;  // relatively important. Use Utility.
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

                [strongCKKS dispatchSync: ^bool{
                    if(error == nil) {
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
                        ckkserror("ckksheal", strongCKKS, "couldn't save new key hierarchy to CloudKit: %@", error);
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

        [keyset.tlk loadKeyMaterialFromKeychain:&error];
        if(error && [ckks.lockStateTracker isLockedError:error]) {
            ckksnotice("ckkskey", ckks, "Failed to load TLK from keychain, keybag is locked. Entering WaitForUnlock: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:nil];
            return false;
        } else if(error) {
            ckkserror("ckksheal", ckks, "No TLK in keychain, triggering move to bad state: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateWaitForTLK withError: nil];
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
        ckkserror("ckksheal", ckks, "Couldn't load classC key from keychain. Attempting recovery: %@", error);
        error = nil;
        [key unwrapViaKeyHierarchy: &error];
        if(error) {
            ckkserror("ckksheal", ckks, "Couldn't unwrap class C key using key hierarchy. Keys are broken, quitting: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            self.error = error;
            return false;
        }
        [key saveKeyMaterialToKeychain:&error];
        if(error) {
            ckkserror("ckksheal", ckks, "Couldn't save class C key to keychain: %@", error);
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
