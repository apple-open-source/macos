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
#import "CKKSKey.h"
#import "CKKSNewTLKOperation.h"
#import "CKKSGroupOperation.h"
#import "CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"

#if OCTAGON

#import "keychain/ckks/CKKSTLKShare.h"

@interface CKKSNewTLKOperation ()
@property NSBlockOperation* cloudkitModifyOperationFinished;
@property CKOperationGroup* ckoperationGroup;
@end

@implementation CKKSNewTLKOperation

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
     * Rolling keys is an essential operation, and must be transactional: either completing successfully or
     * failing entirely. Also, in the case of failure, some other peer has beaten us to CloudKit and changed
     * the keys stored there (which we must now fetch and handle): the keys we attempted to upload are useless.

     * Therefore, we'll skip the normal OutgoingQueue behavior, and persist keys in-memory until such time as
     * CloudKit tells us the operation succeeds or fails, at which point we'll commit them or throw them away.
     *
     * Note that this means edge cases in the case of secd dying in the middle of this operation; our normal
     * retry mechanisms won't work. We'll have to make the policy decision to re-roll the keys if needed upon
     * the next launch of secd (or, the write will succeed after we die, and we'll handle receiving the CK
     * items as if a different peer uploaded them).
     */

    __weak __typeof(self) weakSelf = self;

    CKKSKeychainView* ckks = self.ckks;

    if(self.cancelled) {
        ckksnotice("ckkstlk", ckks, "CKKSNewTLKOperation cancelled, quitting");
        return;
    }

    if(!ckks) {
        ckkserror("ckkstlk", ckks, "no CKKS object");
        return;
    }

    // Synchronous, on some thread. Get back on the CKKS queue for SQL thread-safety.
    [ckks dispatchSyncWithAccountKeys: ^bool{
        if(self.cancelled) {
            ckksnotice("ckkstlk", ckks, "CKKSNewTLKOperation cancelled, quitting");
            return false;
        }

        ckks.lastNewTLKOperation = self;

        if(ckks.currentSelfPeersError) {
            if([ckks.lockStateTracker isLockedError: ckks.currentSelfPeersError]) {
                ckkserror("ckksshare", ckks, "Can't create new TLKs: keychain is locked so self peers are unavailable: %@", ckks.currentSelfPeersError);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:nil];
            } else {
                ckkserror("ckkstlk", ckks, "Couldn't create new TLKs because self peers aren't available: %@", ckks.currentSelfPeersError);
                [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateNewTLKsFailed withError: ckks.currentSelfPeersError];
            }
            self.error = ckks.currentSelfPeersError;
            return false;
        }
        if(ckks.currentTrustedPeersError) {
            if([ckks.lockStateTracker isLockedError: ckks.currentTrustedPeersError]) {
                ckkserror("ckksshare", ckks, "Can't create new TLKs: keychain is locked so trusted peers are unavailable: %@", ckks.currentTrustedPeersError);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateWaitForUnlock withError:nil];
            } else {
                ckkserror("ckkstlk", ckks, "Couldn't create new TLKs because trusted peers aren't available: %@", ckks.currentTrustedPeersError);
                [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateNewTLKsFailed withError: ckks.currentTrustedPeersError];
            }
            self.error = ckks.currentTrustedPeersError;
            return false;
        }

        NSError* error = nil;

        ckksinfo("ckkstlk", ckks, "Generating new TLK");

        // Promote to strong reference
        CKKSKeychainView* ckks = self.ckks;

        CKKSKey* newTLK = nil;
        CKKSKey* newClassAKey = nil;
        CKKSKey* newClassCKey = nil;
        CKKSKey* wrappedOldTLK = nil;

        NSMutableArray<CKRecord *>* recordsToSave = [[NSMutableArray alloc] init];
        NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];

        // Now, prepare data for the operation:

        // We must find the current TLK (to wrap it to the new TLK).
        NSError* localerror = nil;
        CKKSKey* oldTLK = [CKKSKey currentKeyForClass: SecCKKSKeyClassTLK zoneID:ckks.zoneID error: &localerror];
        if(localerror) {
            ckkserror("ckkstlk", ckks, "couldn't load the current TLK: %@", localerror);
            // TODO: not loading the old TLK is fine, but only if there aren't any TLKs
        }

        [oldTLK ensureKeyLoaded: &error];

        ckksnotice("ckkstlk", ckks, "Old TLK is: %@ %@", oldTLK, error);
        if(error != nil) {
            ckkserror("ckkstlk", ckks, "Couldn't fetch and unwrap old TLK: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            return false;
        }

        // Generate new hierarchy:
        //       newTLK
        //      /   |   \
        //     /    |    \
        //    /     |     \
        // oldTLK classA classC

        newTLK = [[CKKSKey alloc] initSelfWrappedWithAESKey:[CKKSAESSIVKey randomKey]
                                                       uuid:[[NSUUID UUID] UUIDString]
                                                   keyclass:SecCKKSKeyClassTLK
                                                      state:SecCKKSProcessedStateLocal
                                                     zoneID:ckks.zoneID
                                            encodedCKRecord:nil
                                                 currentkey:true];

        newClassAKey = [CKKSKey randomKeyWrappedByParent: newTLK keyclass: SecCKKSKeyClassA error: &error];
        newClassCKey = [CKKSKey randomKeyWrappedByParent: newTLK keyclass: SecCKKSKeyClassC error: &error];

        if(error != nil) {
            ckkserror("ckkstlk", ckks, "couldn't make new key hierarchy: %@", error);
            // TODO: this really isn't the error state, but a 'retry'.
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            return false;
        }

        CKKSCurrentKeyPointer* currentTLKPointer =    [CKKSCurrentKeyPointer forKeyClass: SecCKKSKeyClassTLK withKeyUUID:newTLK.uuid       zoneID:ckks.zoneID error: &error];
        CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer forKeyClass: SecCKKSKeyClassA   withKeyUUID:newClassAKey.uuid zoneID:ckks.zoneID error: &error];
        CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer forKeyClass: SecCKKSKeyClassC   withKeyUUID:newClassCKey.uuid zoneID:ckks.zoneID error: &error];

        if(error != nil) {
            ckkserror("ckkstlk", ckks, "couldn't make current key records: %@", error);
            // TODO: this really isn't the error state, but a 'retry'.
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            return false;
        }

        // Wrap old TLK under the new TLK
        wrappedOldTLK = [oldTLK copy];
        if(wrappedOldTLK) {
            [wrappedOldTLK ensureKeyLoaded: &error];
            if(error != nil) {
                ckkserror("ckkstlk", ckks, "couldn't unwrap TLK, aborting new TLK operation: %@", error);
                [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
                return false;
            }

            [wrappedOldTLK wrapUnder: newTLK error:&error];
            // TODO: should we continue in this error state? Might be required to fix broken TLKs/argue over which TLK should be used
            if(error != nil) {
                ckkserror("ckkstlk", ckks, "couldn't wrap oldTLK, aborting new TLK operation: %@", error);
                [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
                return false;
            }

            wrappedOldTLK.currentkey = false;
        }

        [recordsToSave addObject: [newTLK       CKRecordWithZoneID: ckks.zoneID]];
        [recordsToSave addObject: [newClassAKey CKRecordWithZoneID: ckks.zoneID]];
        [recordsToSave addObject: [newClassCKey CKRecordWithZoneID: ckks.zoneID]];

        [recordsToSave addObject: [currentTLKPointer    CKRecordWithZoneID: ckks.zoneID]];
        [recordsToSave addObject: [currentClassAPointer CKRecordWithZoneID: ckks.zoneID]];
        [recordsToSave addObject: [currentClassCPointer CKRecordWithZoneID: ckks.zoneID]];

        if(wrappedOldTLK) {
            [recordsToSave addObject: [wrappedOldTLK CKRecordWithZoneID: ckks.zoneID]];
        }

        // Save the proposed keys to the keychain. Note that we might reject this TLK later, but in that case, this TLK is just orphaned. No worries!
        ckksnotice("ckkstlk", ckks, "Saving new keys %@ to database %@", recordsToSave, ckks.database);

        [newTLK       saveKeyMaterialToKeychain: &error];
        [newClassAKey saveKeyMaterialToKeychain: &error];
        [newClassCKey saveKeyMaterialToKeychain: &error];
        if(error) {
            self.error = error;
            ckkserror("ckkstlk", ckks, "couldn't save new key material to keychain, aborting new TLK operation: %@", error);
            [ckks _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: error];
            return false;
        }

        // Generate the TLK sharing records for all trusted peers
        NSMutableSet<CKKSTLKShare*>* tlkShares = [NSMutableSet set];
        for(id<CKKSPeer> trustedPeer in ckks.currentTrustedPeers) {
            if(!trustedPeer.publicEncryptionKey) {
                ckksnotice("ckkstlk", ckks, "No need to make TLK for %@; they don't have any encryption keys", trustedPeer);
                continue;
            }

            ckksnotice("ckkstlk", ckks, "Generating TLK(%@) share for %@", newTLK, trustedPeer);
            CKKSTLKShare* share = [CKKSTLKShare share:newTLK as:ckks.currentSelfPeers.currentSelf to:trustedPeer epoch:-1 poisoned:0 error:&error];

            [tlkShares addObject:share];
            [recordsToSave addObject: [share CKRecordWithZoneID: ckks.zoneID]];
        }

        // Use the spare operation trick to wait for the CKModifyRecordsOperation to complete
        self.cloudkitModifyOperationFinished = [NSBlockOperation named:@"newtlk-cloudkit-modify-operation-finished" withBlock:^{}];
        [self dependOnBeforeGroupFinished: self.cloudkitModifyOperationFinished];

        CKModifyRecordsOperation* modifyRecordsOp = nil;

        // Get the CloudKit operation ready...
        modifyRecordsOp = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave recordIDsToDelete:recordIDsToDelete];
        modifyRecordsOp.atomic = YES;
        modifyRecordsOp.longLived = NO; // The keys are only in memory; mark this explicitly not long-lived
        modifyRecordsOp.qualityOfService = NSQualityOfServiceUserInitiated;  // This needs to happen before CKKS is available for PCS/CloudKit use.
        modifyRecordsOp.group = self.ckoperationGroup;
        ckksnotice("ckkstlk", ckks, "Operation group is %@", self.ckoperationGroup);

        NSMutableDictionary<CKRecordID*, CKRecord*>* attemptedRecords = [[NSMutableDictionary alloc] init];
        for(CKRecord* record in recordsToSave) {
            attemptedRecords[record] = record;
        }

        modifyRecordsOp.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

            // These should all fail or succeed as one. Do the hard work in the records completion block.
            if(!error) {
                ckksnotice("ckkstlk", blockCKKS, "Successfully completed upload for %@", record.recordID.recordName);
            } else {
                ckkserror("ckkstlk", blockCKKS, "error on row: %@ %@", error, record);
            }
        };


        modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf || !strongCKKS) {
                ckkserror("ckkstlk", strongCKKS, "received callback for released object");
                return;
            }

            [strongCKKS dispatchSyncWithAccountKeys: ^bool{
                if(ckerror == nil) {
                    ckksnotice("ckkstlk", strongCKKS, "Completed TLK CloudKit operation");

                    // Success. Persist the keys to the CKKS database.
                    NSError* localerror = nil;

                    // Save the new CKRecords to the before persisting to database
                    for(CKRecord* record in savedRecords) {
                        if([newTLK matchesCKRecord: record]) {
                            newTLK.storedCKRecord = record;
                        } else if([newClassAKey matchesCKRecord: record]) {
                            newClassAKey.storedCKRecord = record;
                        } else if([newClassCKey matchesCKRecord: record]) {
                            newClassCKey.storedCKRecord = record;
                        } else if([wrappedOldTLK matchesCKRecord: record]) {
                            wrappedOldTLK.storedCKRecord = record;

                        } else if([currentTLKPointer matchesCKRecord: record]) {
                            currentTLKPointer.storedCKRecord = record;
                        } else if([currentClassAPointer matchesCKRecord: record]) {
                            currentClassAPointer.storedCKRecord = record;
                        } else if([currentClassCPointer matchesCKRecord: record]) {
                            currentClassCPointer.storedCKRecord = record;
                        }

                        for(CKKSTLKShare* share in tlkShares) {
                            if([share matchesCKRecord: record]) {
                                share.storedCKRecord = record;
                            }
                        }
                    }

                    [newTLK       saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];
                    [newClassAKey saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];
                    [newClassCKey saveToDatabaseAsOnlyCurrentKeyForClassAndState: &localerror];

                    [currentTLKPointer    saveToDatabase: &localerror];
                    [currentClassAPointer saveToDatabase: &localerror];
                    [currentClassCPointer saveToDatabase: &localerror];

                    [wrappedOldTLK saveToDatabase: &localerror];

                    for(CKKSTLKShare* share in tlkShares) {
                        [share saveToDatabase:&localerror];
                    }

                    // TLKs are already saved in the local keychain; fire off a backup
                    CKKSNearFutureScheduler* tlkNotifier = strongCKKS.savedTLKNotifier;
                    ckksnotice("ckkstlk", strongCKKS, "triggering new TLK notification: %@", tlkNotifier);
                    [tlkNotifier trigger];

                    if(localerror != nil) {
                        ckkserror("ckkstlk", strongCKKS, "couldn't save new key hierarchy to database; this is very bad: %@", localerror);
                        [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateError withError: localerror];
                        return false;
                    } else {
                        // Everything is groovy.
                        [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateReady withError: nil];
                    }
                } else {
                    ckkserror("ckkstlk", strongCKKS, "couldn't save new key hierarchy to CloudKit: %@", ckerror);
                    [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateNewTLKsFailed withError: nil];

                    [strongCKKS _onqueueCKWriteFailed:ckerror attemptedRecordsChanged:attemptedRecords];

                    // Delete these keys from the keychain only if CloudKit positively told us the write failed.
                    // Other failures _might_ leave the writes written to cloudkit; who can say?
                    if([ckerror ckksIsCKErrorRecordChangedError]) {
                        NSError* localerror = nil;
                        [newTLK deleteKeyMaterialFromKeychain: &localerror];
                        [newClassAKey deleteKeyMaterialFromKeychain: &localerror];
                        [newClassCKey deleteKeyMaterialFromKeychain: &localerror];
                        if(localerror) {
                            ckkserror("ckkstlk", strongCKKS, "couldn't delete now-useless key material from keychain: %@", localerror);
                        }
                    } else {
                        ckksnotice("ckkstlk", strongCKKS, "Error is too scary; not deleting likely-useless key material from keychain");
                    }
                }
                return true;
            }];

            // Notify that we're done
            [strongSelf.operationQueue addOperation: strongSelf.cloudkitModifyOperationFinished];
        };

        [ckks.database addOperation: modifyRecordsOp];
        return true;
    }];
}

- (void)cancel {
    [self.cloudkitModifyOperationFinished cancel];
    [super cancel];
}

@end;

#endif
