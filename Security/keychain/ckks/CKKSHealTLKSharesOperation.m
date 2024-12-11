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
#import <os/feature_private.h>

#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSCurrentKeyPointer.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSHealTLKSharesOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSTLKShareRecord.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

#import "CKKSPowerCollection.h"

#define BATCH_SIZE (SecCKKSTestsEnabled() ? 10 : 1000)

@interface CKKSHealTLKSharesOperation ()
@property NSHashTable* ckOperations;
@property CKKSResultOperation* setResultStateOperation;

@property BOOL cloudkitWriteFailures;
@property BOOL failedDueToLockState;
@property BOOL failedDueToEssentialTrustState;
@end

@implementation CKKSHealTLKSharesOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)init {
    return nil;
}

- (instancetype)initWithDependencies:(CKKSOperationDependencies*)operationDependencies
                       intendedState:(CKKSState*)intendedState
                          errorState:(CKKSState*)errorState
{
    if(self = [super init]) {
        _deps = operationDependencies;

        _nextState = errorState;
        _intendedState = intendedState;

        _cloudkitWriteFailures = NO;
        _failedDueToLockState = NO;
        _failedDueToEssentialTrustState = NO;
        _ckOperations = [NSHashTable weakObjectsHashTable];
    }
    return self;
}

- (void)groupStart {
    WEAKIFY(self);
#if TARGET_OS_TV
    [self.deps.personaAdapter prepareThreadForKeychainAPIUseForPersonaIdentifier: nil];
#endif
    if (self.deps.syncingPolicy.isInheritedAccount) {
        secnotice("ckksshare", "Account is inherited, bailing out of healing TLKShares");
        self.nextState = self.intendedState;
        return;
    }

    [self.deps.overallLaunch addEvent:@"heal-tlk-shares-begin"];
    
    AAFAnalyticsEventSecurity *eventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{}
                                                                                       altDSID:self.deps.activeAccount.altDSID
                                                                                     eventName:kSecurityRTCEventNameHealTLKShares
                                                                               testsAreEnabled:SecCKKSTestsEnabled()
                                                                                      category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                    sendMetric:self.deps.sendMetric];

    self.setResultStateOperation = [CKKSResultOperation named:@"determine-next-state" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);

        [self.deps.overallLaunch addEvent:@"heal-tlk-shares-complete"];

        if(self.failedDueToEssentialTrustState) {
            self.nextState = CKKSStateLoseTrust;
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:nil];
        } else if(self.cloudkitWriteFailures) {
            ckksnotice_global("ckksheal", "Due to write failures, we'll retry later");
            self.nextState = CKKSStateHealTLKSharesFailed;
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:nil];
        } else {
            self.nextState = self.intendedState;
            if (self.failedDueToLockState) {
                [eventS addMetrics:@{kSecurityRTCFieldIsLocked:@(NO)}];
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:nil];
            } else {
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:nil];
            }
        }
    }];

    NSArray<CKKSPeerProviderState*>* trustStates = [self.deps currentTrustStates];

    int numViewsEvaluated = 0;
    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        if(![viewState.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady]) {
            ckkserror("ckksshare", viewState.zoneID, "View key state is %@; not checking TLK share validity", viewState);
            continue;
        }
        
        numViewsEvaluated++;
        [self checkAndHealTLKShares:viewState
                 currentTrustStates:trustStates];
    }

    [eventS addMetrics:@{kSecurityRTCFieldNumViews:@(numViewsEvaluated)}];

    if(self.failedDueToLockState) {
        // This is okay, but we need to recheck these after the next unlock.
        OctagonPendingFlag* whenUnlocked = [[OctagonPendingFlag alloc] initWithFlag:CKKSFlagKeyStateProcessRequested
                                                                         conditions:OctagonPendingConditionsDeviceUnlocked];
        [self.deps.flagHandler handlePendingFlag:whenUnlocked];
    }

    [self runBeforeGroupFinished:self.setResultStateOperation];
}

- (void)checkAndHealTLKShares:(CKKSKeychainViewState*)viewState
           currentTrustStates:(NSArray<CKKSPeerProviderState*>*)currentTrustStates
{
    __block CKKSCurrentKeySet* keyset = nil;

    [self.deps.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        keyset = [CKKSCurrentKeySet loadForZone:viewState.zoneID
                                      contextID:viewState.contextID ];
    }];

    if(keyset.error) {
        viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateUnhealthy;
        ckkserror("ckksshare", viewState.zoneID, "couldn't load current keys: can't fix TLK shares");
        return;
    } else {
        ckksnotice("ckksshare", viewState.zoneID, "Key set is %@", keyset);
    }

    [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventTLKShareProcessing zone:viewState.zoneID.zoneName];

    AAFAnalyticsEventSecurity *createMissingSharesEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldIsLocked:@(NO)}
                                                                                                          altDSID:self.deps.activeAccount.altDSID
                                                                                                        eventName:kSecurityRTCEventNameCreateMissingTLKShares
                                                                                                  testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                         category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                       sendMetric:self.deps.sendMetric];

    @autoreleasepool {
        // Okay! Perform the checks.
        NSError* tlkLoadError = nil;
        if(![keyset.tlk loadKeyMaterialFromKeychain:&tlkLoadError] || tlkLoadError) {
            // Well, that's no good. We can't share a TLK we don't have.
            if([self.deps.lockStateTracker isLockedError:tlkLoadError]) {
                ckkserror("ckksshare", viewState.zoneID, "Keychain is locked: can't fix shares yet: %@", tlkLoadError);
                self.failedDueToLockState = true;
                [createMissingSharesEventS addMetrics:@{kSecurityRTCFieldIsLocked:@(YES)}];
            } else {
                ckkserror("ckksshare", viewState.zoneID, "couldn't load current tlk from keychain: %@", tlkLoadError);
                viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateUnhealthy;
            }
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:createMissingSharesEventS success:NO error:tlkLoadError];
            return;
        }
    }

    if(SecCKKSTestsEnabled() && SecCKKSTestSkipTLKHealing()) {
        ckkserror("ckksnotice", viewState.zoneID, "Test has requested no TLK share healing!");
        return;
    }

    NSError* createSharesError = nil;
    NSMutableSet<CKKSTLKShareRecord*>* newShares = [NSMutableSet set];

    for(CKKSPeerProviderState* trustState in currentTrustStates) {
        @autoreleasepool {
            NSError* stateError = nil;
            NSSet<CKKSTLKShareRecord*>* newTrustShares = [CKKSHealTLKSharesOperation createMissingKeyShares:keyset
                                                                                                      peers:trustState
                                                                                           databaseProvider:self.deps.databaseProvider
                                                                                                      error:&stateError];

            if(newTrustShares && !stateError) {
                [newShares unionSet:newTrustShares];
            } else {
                ckksnotice("ckksshare", keyset.tlk, "Unable to create shares for trust set %@: %@", trustState, stateError);
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:createMissingSharesEventS success:NO error:stateError];
                if(trustState.essential) {
                    if(([stateError.domain isEqualToString:TrustedPeersHelperErrorDomain] && stateError.code == TrustedPeersHelperErrorNoPreparedIdentity) ||
                       ([stateError.domain isEqualToString:CKKSErrorDomain] && stateError.code == CKKSLackingTrust) ||
                       ([stateError.domain isEqualToString:CKKSErrorDomain] && stateError.code == CKKSNoPeersAvailable)) {
                        ckkserror("ckksshare", viewState.zoneID, "Unable to create shares due to some trust issue: %@", createSharesError);

                        viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateWaitForTrust;
                        self.failedDueToEssentialTrustState = YES;
                        [SecurityAnalyticsReporterRTC sendMetricWithEvent:createMissingSharesEventS success:NO error:createSharesError];
                        return;

                    } else {
                        ckkserror("ckksshare", viewState.zoneID, "Unable to create shares: %@", createSharesError);
                        viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateUnhealthy;
                        [SecurityAnalyticsReporterRTC sendMetricWithEvent:createMissingSharesEventS success:NO error:nil];
                        return;
                    }
                }
                // Not essential means we don't want to early-exit
            }
        }
    }

    if(newShares.count == 0u) {
        ckksnotice("ckksshare", viewState.zoneID, "Don't believe we need to change any TLKShares, stopping");
        return;
    }

    [createMissingSharesEventS addMetrics:@{kSecurityRTCFieldNewTLKShares:@(newShares.count)}];
    [SecurityAnalyticsReporterRTC sendMetricWithEvent:createMissingSharesEventS success:YES error:nil];
    
    keyset.pendingTLKShares = [newShares allObjects];

    // Let's double-check: if we upload these TLKShares, will the world be right?
    NSError* sufficientSharesError = nil;
    BOOL newSharesSufficient = [self areNewSharesSufficient:keyset
                                                trustStates:currentTrustStates
                                                      error:&sufficientSharesError];

    if(!newSharesSufficient || sufficientSharesError) {
        ckksnotice("ckksshare", viewState.zoneID, "New shares won't resolve the share issue; erroring to avoid infinite loops: %@", sufficientSharesError);
        viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
        return;
    }

    // Fire up our CloudKit operations!
    __block int numSavedRecords = 0;
    __block BOOL didSucceed = YES;

    AAFAnalyticsEventSecurity *uploadMissingTLKSharesEventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldIsPrioritized:@(NO)}
                                                                                                             altDSID:self.deps.activeAccount.altDSID
                                                                                                           eventName:kSecurityRTCEventNameUploadMissingTLKShares
                                                                                                     testsAreEnabled:SecCKKSTestsEnabled()
                                                                                                            category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                                          sendMetric:self.deps.sendMetric];
    
    // Batch our TLKShare uploads.
    NSMutableArray<CKRecord*>* newTLKShareRecords = [[NSMutableArray alloc] init];
    for (CKKSTLKShareRecord* share in newShares) {
        [newTLKShareRecords addObject:[share CKRecordWithZoneID:viewState.zoneID]];
    }
    
    for (NSUInteger batchNumber = 0; batchNumber*BATCH_SIZE < newShares.count; batchNumber++) {
        NSUInteger startIndx = batchNumber*BATCH_SIZE;
        
        // Use the spare operation trick to wait for the CKModifyRecordsOperation to complete
        CKKSResultOperation* cloudkitModifyOperationFinished = [CKKSResultOperation named:[NSString stringWithFormat:@"heal-tlkshares-%@", viewState.zoneID.zoneName] withBlock:^{
            // If this is the last batch of uploads, send our event for TLK Shares upload.
            if ((newShares.count - startIndx) <= BATCH_SIZE) {
                [SecurityAnalyticsReporterRTC sendMetricWithEvent:uploadMissingTLKSharesEventS success:didSucceed error:nil];
            }
        }];
        [self dependOnBeforeGroupFinished: cloudkitModifyOperationFinished];

        // Gather this operation's TLK Shares to upload
        NSMutableDictionary<CKRecordID*, CKRecord*>* attemptedRecords = [[NSMutableDictionary alloc] init];
        NSArray<CKRecord*>* recordsToSave = [newTLKShareRecords subarrayWithRange:NSMakeRange(startIndx, MIN(BATCH_SIZE, newShares.count - startIndx))];
        
        for (CKRecord* attemptedRecord in recordsToSave) {
            attemptedRecords[attemptedRecord.recordID] = attemptedRecord;
        }

        // Get the CloudKit operation ready...
        CKModifyRecordsOperation* modifyRecordsOp = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave
                                                                                          recordIDsToDelete:nil];
        modifyRecordsOp.atomic = YES;
        modifyRecordsOp.longLived = NO;

        // very important: get the TLKShares off-device ASAP
        modifyRecordsOp.configuration.isCloudKitSupportOperation = YES;

        // CKKSSetHighPriorityOperations is default enabled
        // This operation might be needed during CKKS/Manatee bringup, which affects the user experience. Bump our priority to get it off-device and unblock Manatee access.
        modifyRecordsOp.qualityOfService = NSQualityOfServiceUserInitiated;

        modifyRecordsOp.perRecordSaveBlock = ^(CKRecordID *recordID, CKRecord * _Nullable record, NSError * _Nullable error) {
            // These should all fail or succeed as one. Do the hard work in the records completion block.
            if(!error) {
                ckksnotice("ckksshare", viewState.zoneID, "Successfully completed upload for record %@", recordID.recordName);
            } else {
                ckkserror("ckksshare",  viewState.zoneID, "error on row: %@ %@", recordID, error);
            }
        };

        WEAKIFY(self);

        modifyRecordsOp.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *error) {
            STRONGIFY(self);

            [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                if(error == nil) {
                    // Success. Persist the records to the CKKS database
                    ckksnotice("ckksshare",  viewState.zoneID, "Completed TLK Share heal operation with success");
                    NSError* localerror = nil;

                    numSavedRecords += savedRecords.count;

                    // Save the new CKRecords to the database
                    for(CKRecord* record in savedRecords) {
                        CKKSTLKShareRecord* savedShare = [[CKKSTLKShareRecord alloc] initWithCKRecord:record contextID:self.deps.contextID];
                        bool saved = [savedShare saveToDatabase:&localerror];

                        if(!saved || localerror != nil) {
                            // erroring means we were unable to save the new TLKShare records to the database. This will cause us to try to reupload them. Fail.
                            // No recovery from this, really...
                            ckkserror("ckksshare", viewState.zoneID, "Couldn't save new TLKShare record to database: %@", localerror);
                            viewState.viewKeyHierarchyState = SecCKKSZoneKeyStateError;
                            didSucceed = NO;
                            [uploadMissingTLKSharesEventS populateUnderlyingErrorsStartingWithRootError:localerror];
                            return CKKSDatabaseTransactionCommit;

                        } else {
                            ckksnotice("ckksshare", viewState.zoneID, "Successfully completed upload for %@", savedShare);
                        }
                    }

                } else {
                    ckkserror("ckksshare", viewState.zoneID, "Completed TLK Share heal operation with error: %@", error);
                    [uploadMissingTLKSharesEventS populateUnderlyingErrorsStartingWithRootError:error];
                    [self.deps intransactionCKWriteFailed:error attemptedRecordsChanged:attemptedRecords];
                    self.cloudkitWriteFailures = true;
                }
                return CKKSDatabaseTransactionCommit;
            }];

            // Notify that this operation is done
            [self.operationQueue addOperation:cloudkitModifyOperationFinished];
        };

        [modifyRecordsOp linearDependencies:self.ckOperations];
        [self.setResultStateOperation addDependency:cloudkitModifyOperationFinished];
        [self.deps.ckdatabase addOperation:modifyRecordsOp];
    }

}

- (BOOL)areNewSharesSufficient:(CKKSCurrentKeySet*)keyset
                   trustStates:(NSArray<CKKSPeerProviderState*>*)trustStates
                         error:(NSError* __autoreleasing*)error
{
    for(CKKSPeerProviderState* trustState in trustStates) {
        NSError* localError = nil;
        NSSet<id<CKKSPeer>>* peersMissingShares = [CKKSHealTLKSharesOperation filterTrustedPeers:trustState
                                                                             missingTLKSharesFor:keyset
                                                                                databaseProvider:self.deps.databaseProvider
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
                                               databaseProvider:(id<CKKSDatabaseProviderProtocol> _Nullable)databaseProvider
                                                          error:(NSError* __autoreleasing*)error
{
    NSError* localerror = nil;
    NSSet<CKKSTLKShareRecord*>* newShares = nil;

    // If any one of our trust states succeed, this function doesn't have an error
    for(CKKSPeerProviderState* trustState in trustStates) {
        NSError* stateError = nil;

        NSSet<CKKSTLKShareRecord*>* newTrustShares = [self createMissingKeyShares:keyset
                                                                            peers:trustState
                                                                 databaseProvider:databaseProvider
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
                                     databaseProvider:(id<CKKSDatabaseProviderProtocol> _Nullable)databaseProvider
                                                error:(NSError* __autoreleasing*)error
{
    NSError* localerror = nil;
    CKKSKeychainBackedKey* keychainBackedTLK = [keyset.tlk ensureKeyLoadedForContextID:keyset.zoneID.ownerName
                                                                                 cache:nil
                                                                                 error:&localerror];

    if(keychainBackedTLK == nil) {
        ckkserror("ckksshare", keyset.tlk, "TLK not loaded; cannot make shares for peers: %@", localerror);
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    NSSet<id<CKKSPeer>>* remainingPeers = [self filterTrustedPeers:trustState
                                               missingTLKSharesFor:keyset
                                                  databaseProvider:databaseProvider
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
        CKKSTLKShareRecord* newShare = [CKKSTLKShareRecord share:keychainBackedTLK
                                                       contextID:keyset.tlk.contextID
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

// For this key, who doesn't yet have a valid CKKSTLKShare for it?
// Note that we really want a record sharing the TLK to ourselves, so this function might return
// a non-empty set even if all peers have the TLK: it wants us to make a record for ourself.
+ (NSSet<id<CKKSPeer>>* _Nullable)filterTrustedPeers:(CKKSPeerProviderState*)peerState
                                 missingTLKSharesFor:(CKKSCurrentKeySet*)keyset
                                    databaseProvider:(id<CKKSDatabaseProviderProtocol> _Nullable)databaseProvider
                                               error:(NSError**)error
{
    if(peerState.currentTrustedPeersError) {
        ckkserror("ckksshare", keyset.tlk, "Couldn't find missing shares because trusted peers aren't available: %@", peerState.currentTrustedPeersError);
        if(error) {
            *error = peerState.currentTrustedPeersError;
        }
        return [NSSet set];
    }
    if(peerState.currentSelfPeersError) {
        ckkserror("ckksshare", keyset.tlk, "Couldn't find missing shares because self peers aren't available: %@", peerState.currentSelfPeersError);
        if(error) {
            *error = peerState.currentSelfPeersError;
        }
        return [NSSet set];
    }

    NSMutableSet<id<CKKSPeer>>* peersMissingShares = [NSMutableSet set];

    // Ensure that the 'self peer' is one of the current trusted peers. Otherwise, any TLKShare we create
    // won't be considered trusted the next time through...
    if(![peerState.currentTrustedPeerIDs containsObject:peerState.currentSelfPeers.currentSelf.peerID]) {
        ckkserror("ckksshare", keyset.tlk, "current self peer (%@) is not in the set of trusted peers: %@",
                  peerState.currentSelfPeers.currentSelf.peerID,
                  peerState.currentTrustedPeerIDs);

        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSLackingTrust
                                  description:[NSString stringWithFormat:@"current self peer (%@) is not in the set of trusted peers",
                                               peerState.currentSelfPeers.currentSelf.peerID]];
        }

        return nil;
    }

    for(id<CKKSRemotePeerProtocol> peer in peerState.currentTrustedPeers) {
        if(![peer shouldHaveView:keyset.tlk.zoneName]) {
            ckkserror("ckksshare", keyset.tlk, "Peer (%@) is not supposed to have view, skipping", peer);
            continue;
        }

        __block NSError* loadError = nil;
        __block NSArray<CKKSTLKShareRecord*>* existingTLKSharesForPeer = nil;

        // If our caller provided a databaseProvider, use it. Otherwise, just assume that we can load from a DB in this thread already.
        if(databaseProvider == nil) {
            @autoreleasepool {
                existingTLKSharesForPeer = [CKKSTLKShareRecord allFor:peer.peerID
                                                            contextID:keyset.tlk.contextID
                                                              keyUUID:keyset.tlk.uuid
                                                               zoneID:keyset.tlk.zoneID
                                                                error:&loadError];
            }
        } else {
            [databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
                @autoreleasepool {
                    existingTLKSharesForPeer = [CKKSTLKShareRecord allFor:peer.peerID
                                                                contextID:keyset.tlk.contextID
                                                                  keyUUID:keyset.tlk.uuid
                                                                   zoneID:keyset.tlk.zoneID
                                                                    error:&loadError];
                }
            }];
        }

        if(existingTLKSharesForPeer == nil || loadError != nil) {
            ckkserror("ckksshare", keyset.tlk, "Unable to load existing TLKShares for peer (%@): %@", peer, loadError);
            continue;
        }

        NSArray<CKKSTLKShareRecord*>* tlkShares = [existingTLKSharesForPeer arrayByAddingObjectsFromArray:keyset.pendingTLKShares ?: @[]];

        // Determine if we think this peer has enough things shared to them
        bool alreadyShared = false;
        NSMutableArray<CKKSTLKShareRecord*>* existingInsufficientShares = [NSMutableArray array];

        for(CKKSTLKShareRecord* existingShare in tlkShares) {
            @autoreleasepool {
                // Ensure this share is to this peer...
                if(![existingShare.share.receiverPeerID isEqualToString:peer.peerID]) {
                    continue;
                }

                // If an SOS Peer sent this share, is its signature still valid? Or did the signing key change?
                if([existingShare.senderPeerID hasPrefix:CKKSSOSPeerPrefix]) {
                    NSError* signatureError = nil;
                    if(![existingShare signatureVerifiesWithPeerSet:peerState.currentTrustedPeers error:&signatureError]) {
                        ckksnotice("ckksshare", keyset.tlk, "Existing TLKShare's signature doesn't verify with current peer set: %@ %@", signatureError, existingShare);
                        continue;
                    }
                }

                if([existingShare.tlkUUID isEqualToString:keyset.tlk.uuid] && [peerState.currentTrustedPeerIDs containsObject:existingShare.senderPeerID]) {
                    // Was this shared to us?
                    if([peer.peerID isEqualToString:peerState.currentSelfPeers.currentSelf.peerID]) {
                        // We only count this as 'found' if we did the sharing and it's to our current keys
                        NSData* currentKey = peerState.currentSelfPeers.currentSelf.publicEncryptionKey.keyData;

                        if([existingShare.senderPeerID isEqualToString:peerState.currentSelfPeers.currentSelf.peerID] &&
                           [existingShare.share.receiverPublicEncryptionKeySPKI isEqual:currentKey]) {
                            ckksnotice("ckksshare", keyset.tlk, "Local peer %@ is shared %@ via self: %@", peer, keyset.tlk.uuid, existingShare);
                            alreadyShared = true;
                            break;
                        } else {
                            [existingInsufficientShares addObject:existingShare];
                        }

                    } else {
                        // Was this shared to the remote peer's current keys?
                        NSData* currentKeySPKI = peer.publicEncryptionKey.keyData;

                        if([existingShare.share.receiverPublicEncryptionKeySPKI isEqual:currentKeySPKI]) {
                            // Some other peer has a trusted share. Cool!
                            ckksnotice("ckksshare", keyset.tlk, "Peer %@ is shared %@ via trusted %@", peer, keyset.tlk.uuid, existingShare);
                            alreadyShared = true;
                            break;
                        } else {
                            ckksnotice("ckksshare", keyset.tlk, "Peer %@ has a share for %@, but to old keys: %@", peer, keyset.tlk.uuid, existingShare);
                            [existingInsufficientShares addObject:existingShare];
                        }
                    }
                }
            }
        }

        if(!alreadyShared) {
            ckksnotice("ckksshare", keyset.tlk, "Peer %@ is shared %@ via insufficient shares: %@", peer, keyset.tlk.uuid, existingInsufficientShares);

            // Add this peer to our set, if it has an encryption key to receive the share
            if(peer.publicEncryptionKey) {
                [peersMissingShares addObject:peer];
            }
        }
    }

    if(peersMissingShares.count > 0u) {
        // Log each and every one of the things
        ckksnotice("ckksshare", keyset.tlk, "Missing TLK shares for %lu peers: %@", (unsigned long)peersMissingShares.count, peersMissingShares);
        ckksnotice("ckksshare", keyset.tlk, "Self peers are (%@) %@", peerState.currentSelfPeersError ?: @"no error", peerState.currentSelfPeers);
        ckksnotice("ckksshare", keyset.tlk, "Trusted peers are (%@) %@", peerState.currentTrustedPeersError ?: @"no error", peerState.currentTrustedPeers);
    }

    return peersMissingShares;
}

@end;

#endif
