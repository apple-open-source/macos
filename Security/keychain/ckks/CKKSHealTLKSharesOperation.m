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

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSHealTLKSharesOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ot/ObjCImprovements.h"

#import "CKKSPowerCollection.h"

@interface CKKSHealTLKSharesOperation ()
@property NSBlockOperation* cloudkitModifyOperationFinished;
@end

@implementation CKKSHealTLKSharesOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)init {
    return nil;
}

- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                                         ckks:(CKKSKeychainView*)ckks
{
    if(self = [super init]) {
        _ckks = ckks;
        _deps = operationDependencies;

        _nextState = SecCKKSZoneKeyStateHealTLKSharesFailed;
        _intendedState = SecCKKSZoneKeyStateBecomeReady;
    }
    return self;
}

- (void)groupStart {
    /*
     * We've been invoked because something is wonky with the tlk shares.
     *
     * Attempt to figure out what it is, and what we can do about it.
     */

    WEAKIFY(self);

    if(self.cancelled) {
        ckksnotice("ckksshare", self.deps.zoneID, "CKKSHealTLKSharesOperation cancelled, quitting");
        return;
    }

    NSArray<CKKSPeerProviderState*>* trustStates = [self.deps currentTrustStates];

    NSError* error = nil;
    __block CKKSCurrentKeySet* keyset = nil;

    [self.deps.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        keyset = [CKKSCurrentKeySet loadForZone:self.deps.zoneID];
    }];

    if(keyset.error) {
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        self.error = keyset.error;
        ckkserror("ckksshare", self.deps.zoneID, "couldn't load current keys: can't fix TLK shares");
        return;
    } else {
        ckksnotice("ckksshare", self.deps.zoneID, "Key set is %@", keyset);
    }

    [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventTLKShareProcessing zone:self.deps.zoneID.zoneName];

    // Okay! Perform the checks.
    if(![keyset.tlk loadKeyMaterialFromKeychain:&error] || error) {
        // Well, that's no good. We can't share a TLK we don't have.
        if([self.deps.lockStateTracker isLockedError: error]) {
            ckkserror("ckksshare", self.deps.zoneID, "Keychain is locked: can't fix shares yet: %@", error);
            self.nextState = SecCKKSZoneKeyStateBecomeReady;
        } else {
            ckkserror("ckksshare", self.deps.zoneID, "couldn't load current tlk from keychain: %@", error);
            self.nextState = SecCKKSZoneKeyStateUnhealthy;
        }
        return;
    }

    NSSet<CKKSTLKShareRecord*>* newShares = [CKKSHealTLKSharesOperation createMissingKeyShares:keyset
                                                                                   trustStates:trustStates
                                                                                         error:&error];
    if(error) {
        ckkserror("ckksshare", self.deps.zoneID, "Unable to create shares: %@", error);
        self.nextState = SecCKKSZoneKeyStateUnhealthy;
        return;
    }

    if(newShares.count == 0u) {
        ckksnotice("ckksshare", self.deps.zoneID, "Don't believe we need to change any TLKShares, stopping");
        self.nextState = self.intendedState;
        return;
    }

    keyset.pendingTLKShares = [newShares allObjects];

    // Let's double-check: if we upload these TLKShares, will the world be right?
    BOOL newSharesSufficient = [CKKSHealTLKSharesOperation areNewSharesSufficient:keyset
                                                                      trustStates:trustStates
                                                                            error:&error];
    if(!newSharesSufficient || error) {
        ckksnotice("ckksshare", self.deps.zoneID, "New shares won't resolve the share issue; erroring to avoid infinite loops");
        self.nextState = SecCKKSZoneKeyStateError;
        return;
    }

    // Fire up our CloudKit operation!

    NSMutableArray<CKRecord *>* recordsToSave = [[NSMutableArray alloc] init];
    NSMutableArray<CKRecordID *>* recordIDsToDelete = [[NSMutableArray alloc] init];
    NSMutableDictionary<CKRecordID*, CKRecord*>* attemptedRecords = [[NSMutableDictionary alloc] init];

    ckksnotice("ckksshare", self.deps.zoneID, "Uploading %d new TLKShares", (unsigned int)newShares.count);
    for(CKKSTLKShareRecord* share in newShares) {
        ckksnotice("ckksshare", self.deps.zoneID, "Uploading TLKShare to %@ (as %@)", share.share.receiverPeerID, share.senderPeerID);

        CKRecord* record = [share CKRecordWithZoneID:self.deps.zoneID];
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

    // very important: get the TLKShares off-device ASAP
    modifyRecordsOp.configuration.automaticallyRetryNetworkFailures = NO;
    modifyRecordsOp.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    modifyRecordsOp.configuration.isCloudKitSupportOperation = YES;

    modifyRecordsOp.group = self.deps.ckoperationGroup;
    ckksnotice("ckksshare", self.deps.zoneID, "Operation group is %@", self.deps.ckoperationGroup);

    modifyRecordsOp.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
        STRONGIFY(self);

        // These should all fail or succeed as one. Do the hard work in the records completion block.
        if(!error) {
            ckksnotice("ckksshare", self.deps.zoneID, "Successfully completed upload for record %@", record.recordID.recordName);
        } else {
            ckkserror("ckksshare",  self.deps.zoneID, "error on row: %@ %@", record.recordID, error);
        }
    };

    modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
        STRONGIFY(self);

        CKKSKeychainView* strongCKKS = self.ckks;

        [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            if(error == nil) {
                // Success. Persist the records to the CKKS database
                ckksnotice("ckksshare",  self.deps.zoneID, "Completed TLK Share heal operation with success");
                NSError* localerror = nil;

                // Save the new CKRecords to the database
                for(CKRecord* record in savedRecords) {
                    CKKSTLKShareRecord* savedShare = [[CKKSTLKShareRecord alloc] initWithCKRecord:record];
                    bool saved = [savedShare saveToDatabase:&localerror];

                    if(!saved || localerror != nil) {
                        // erroring means we were unable to save the new TLKShare records to the database. This will cause us to try to reupload them. Fail.
                        // No recovery from this, really...
                        ckkserror("ckksshare", self.deps.zoneID, "Couldn't save new TLKShare record to database: %@", localerror);
                        self.error = localerror;
                        self.nextState = SecCKKSZoneKeyStateError;
                        return CKKSDatabaseTransactionCommit;

                    } else {
                        ckksnotice("ckksshare", self.deps.zoneID, "Successfully completed upload for %@", savedShare);
                    }
                }

                // Successfully sharing TLKs means we're now in ready!
                self.nextState = SecCKKSZoneKeyStateBecomeReady;
            } else {
                ckkserror("ckksshare", self.deps.zoneID, "Completed TLK Share heal operation with error: %@", error);
                [strongCKKS _onqueueCKWriteFailed:error attemptedRecordsChanged:attemptedRecords];
                // Send the key state machine into tlksharesfailed
                self.nextState = SecCKKSZoneKeyStateHealTLKSharesFailed;
            }
            return CKKSDatabaseTransactionCommit;
        }];

        // Notify that we're done
        [self.operationQueue addOperation: self.cloudkitModifyOperationFinished];
    };

    [self.deps.ckdatabase addOperation:modifyRecordsOp];
}

- (void)cancel {
    [self.cloudkitModifyOperationFinished cancel];
    [super cancel];
}

+ (BOOL)areNewSharesSufficient:(CKKSCurrentKeySet*)keyset
                   trustStates:(NSArray<CKKSPeerProviderState*>*)trustStates
                         error:(NSError* __autoreleasing*)error
{
    for(CKKSPeerProviderState* trustState in trustStates) {
        NSError* localError = nil;
        NSSet<id<CKKSPeer>>* peersMissingShares = [trustState findPeersMissingTLKSharesFor:keyset
                                                                                     error:&localError];
        if(peersMissingShares == nil || localError) {
            if(trustState.essential) {
                if(error) {
                    *error = localError;
                }
                return NO;
            } else {
                ckksnotice("ckksshare", keyset.tlk, "Failed to find peers for nonessential system: %@", trustState);
                // Not a hard failure.
            }
        }

        if(peersMissingShares.count > 0) {
            ckksnotice("ckksshare", keyset.tlk, "New share set is missing shares for peers: %@", peersMissingShares);
            return NO;
        }
    }

    return YES;
}


+ (NSSet<CKKSTLKShareRecord*>* _Nullable)createMissingKeyShares:(CKKSCurrentKeySet*)keyset
                                                    trustStates:(NSArray<CKKSPeerProviderState*>*)trustStates
                                                          error:(NSError* __autoreleasing*)error
{
    NSError* localerror = nil;
    NSSet<CKKSTLKShareRecord*>* newShares = nil;

    // If any one of our trust states succeed, this function doesn't have an error
    for(CKKSPeerProviderState* trustState in trustStates) {
        NSError* stateError = nil;

        NSSet<CKKSTLKShareRecord*>* newTrustShares = [self createMissingKeyShares:keyset
                                                                            peers:trustState
                                                                            error:&stateError];


        if(newTrustShares && !stateError) {
            newShares = newShares ? [newShares setByAddingObjectsFromSet:newTrustShares] : newTrustShares;
        } else {
            ckksnotice("ckksshare", keyset.tlk, "Unable to create shares for trust set %@: %@", trustState, stateError);
            if(localerror == nil) {
                localerror = stateError;
            }
        }
    }

    // Only report an error if none of the trust states were able to succeed
    if(newShares) {
        return newShares;
    } else {
        if(error && localerror) {
            *error = localerror;
        }
        return nil;
    }
}

+ (NSSet<CKKSTLKShareRecord*>*)createMissingKeyShares:(CKKSCurrentKeySet*)keyset
                                                peers:(CKKSPeerProviderState*)trustState
                                                error:(NSError* __autoreleasing*)error
{
    NSError* localerror = nil;
    if(![keyset.tlk ensureKeyLoaded:&localerror]) {
        ckkserror("ckksshare", keyset.tlk, "TLK not loaded; cannot make shares for peers: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSSet<id<CKKSPeer>>* remainingPeers = [trustState findPeersMissingTLKSharesFor:keyset
                                                                             error:&localerror];
    if(!remainingPeers) {
        ckkserror("ckksshare", keyset.tlk, "Unable to find peers missing TLKShares: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSMutableSet<CKKSTLKShareRecord*>* newShares = [NSMutableSet set];

    for(id<CKKSPeer> peer in remainingPeers) {
        if(!peer.publicEncryptionKey) {
            ckksnotice("ckksshare", keyset.tlk, "No need to make TLK for %@; they don't have any encryption keys", peer);
            continue;
        }

        // Create a share for this peer.
        ckksnotice("ckksshare", keyset.tlk, "Creating share of %@ as %@ for %@", keyset.tlk, trustState.currentSelfPeers.currentSelf, peer);
        CKKSTLKShareRecord* newShare = [CKKSTLKShareRecord share:keyset.tlk
                                                              as:trustState.currentSelfPeers.currentSelf
                                                              to:peer
                                                           epoch:-1
                                                        poisoned:0
                                                           error:&localerror];

        if(localerror) {
            ckkserror("ckksshare", keyset.tlk, "Couldn't create new share for %@: %@", peer, localerror);
            if(error) {
                *error = localerror;
            }
            return nil;
        }

        [newShares addObject: newShare];
    }

    return newShares;
}

@end;

#endif
