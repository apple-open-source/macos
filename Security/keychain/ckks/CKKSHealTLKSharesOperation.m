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

#if OCTAGON

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSHealTLKSharesOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSTLKShare.h"

#import "CKKSPowerCollection.h"

@interface CKKSHealTLKSharesOperation ()
@property NSBlockOperation* cloudkitModifyOperationFinished;
@property CKOperationGroup* ckoperationGroup;
@end

@implementation CKKSHealTLKSharesOperation

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
     * We've been invoked because something is wonky with the tlk shares.
     *
     * Attempt to figure out what it is, and what we can do about it.
     */

    __weak __typeof(self) weakSelf = self;

    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksshare", ckks, "no CKKS object");
        return;
    }

    if(self.cancelled) {
        ckksnotice("ckksshare", ckks, "CKKSHealTLKSharesOperation cancelled, quitting");
        return;
    }

    [ckks dispatchSyncWithAccountKeys: ^bool{
        if(self.cancelled) {
            ckksnotice("ckksshare", ckks, "CKKSHealTLKSharesOperation cancelled, quitting");
            return false;
        }

        NSError* error = nil;

        CKKSCurrentKeySet* keyset = [[CKKSCurrentKeySet alloc] initForZone:ckks.zoneID];

        if(keyset.error) {
            self.error = keyset.error;
            ckkserror("ckksshare", ckks, "couldn't load current keys: can't fix TLK shares");
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateUnhealthy withError:nil];
            return true;
        } else {
            ckksnotice("ckksshare", ckks, "Key set is %@", keyset);
        }

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventTLKShareProcessing zone:ckks.zoneName];

        // Okay! Perform the checks.
        if(![keyset.tlk loadKeyMaterialFromKeychain:&error] || error) {
            // Well, that's no good. We can't share a TLK we don't have.
            if([ckks.lockStateTracker isLockedError: error]) {
                ckkserror("ckksshare", ckks, "Keychain is locked: can't fix shares yet: %@", error);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateReadyPendingUnlock withError:nil];
            } else {
                // TODO go to waitfortlk
                ckkserror("ckksshare", ckks, "couldn't load current tlk from keychain: %@", error);
                [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateUnhealthy withError:nil];
            }
            return true;
        }

        NSSet<CKKSTLKShare*>* newShares = [ckks _onqueueCreateMissingKeyShares:keyset.tlk
                                                                         error:&error];
        if(error) {
            ckkserror("ckksshare", ckks, "Unable to create shares: %@", error);
            return false;
        }

        if(newShares.count == 0u) {
            ckksnotice("ckksshare", ckks, "Don't believe we need to change any TLKShares, stopping");
            [ckks _onqueueAdvanceKeyStateMachineToState:SecCKKSZoneKeyStateReady withError:nil];
            return true;
        }

        // Fire up our CloudKit operation!

        NSMutableArray<CKRecord *>* recordsToSave = [[NSMutableArray alloc] init];
        NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];
        NSMutableDictionary<CKRecordID*, CKRecord*>* attemptedRecords = [[NSMutableDictionary alloc] init];

        for(CKKSTLKShare* share in newShares) {
            CKRecord* record = [share CKRecordWithZoneID:ckks.zoneID];
            [recordsToSave addObject: record];
            attemptedRecords[record.recordID] = record;
        }

        // Use the spare operation trick to wait for the CKModifyRecordsOperation to complete
        self.cloudkitModifyOperationFinished = [NSBlockOperation named:@"heal-tlkshares-modify-operation-finished" withBlock:^{}];
        [self dependOnBeforeGroupFinished: self.cloudkitModifyOperationFinished];


        // Get the CloudKit operation ready...
        CKModifyRecordsOperation* modifyRecordsOp = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave
                                                                                          recordIDsToDelete:recordIDsToDelete];
        modifyRecordsOp.atomic = YES;
        modifyRecordsOp.longLived = NO;
        modifyRecordsOp.qualityOfService = NSQualityOfServiceUserInitiated;  // very important: get the TLKShares off-device ASAP
        modifyRecordsOp.group = self.ckoperationGroup;
        ckksnotice("ckksshare", ckks, "Operation group is %@", self.ckoperationGroup);

        modifyRecordsOp.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

            // These should all fail or succeed as one. Do the hard work in the records completion block.
            if(!error) {
                ckksinfo("ckksshare", blockCKKS, "Successfully completed upload for record %@", record.recordID.recordName);
            } else {
                ckkserror("ckksshare", blockCKKS, "error on row: %@ %@", record.recordID, error);
            }
        };

        modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf) {
                secerror("ckks: received callback for released object");
                return;
            }

            [strongCKKS dispatchSyncWithAccountKeys: ^bool {
                if(error == nil) {
                    // Success. Persist the records to the CKKS database
                    ckksnotice("ckksshare", strongCKKS, "Completed TLK Share heal operation with success");
                    NSError* localerror = nil;

                    // Save the new CKRecords to the database
                    for(CKRecord* record in savedRecords) {
                        CKKSTLKShare* savedShare = [[CKKSTLKShare alloc] initWithCKRecord:record];
                        [savedShare saveToDatabase:&localerror];

                        if(localerror) {
                            // No recovery from this, really...
                            ckkserror("ckksshare", strongCKKS, "Couldn't save new TLKShare record to database: %@", localerror);
                            localerror = nil;
                        } else {
                            ckksnotice("ckksshare", strongCKKS, "Successfully completed upload for %@", savedShare);
                        }
                    }

                    // Successfully sharing TLKs means we're now in ready!
                    [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateReady withError: nil];
                } else {
                    ckkserror("ckksshare", strongCKKS, "Completed TLK Share heal operation with error: %@", error);
                    [strongCKKS _onqueueCKWriteFailed:error attemptedRecordsChanged:attemptedRecords];
                    // Send the key state machine into tlksharesfailed
                    [strongCKKS _onqueueAdvanceKeyStateMachineToState: SecCKKSZoneKeyStateHealTLKSharesFailed withError: nil];
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
