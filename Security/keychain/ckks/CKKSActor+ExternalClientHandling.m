
#if OCTAGON

#import <dispatch/dispatch.h>

#import "keychain/ckks/CKKSExternalTLKClient.h"
#import "keychain/ckks/CKKSExternalTLKClient+Categories.h"
#import "keychain/ckks/CKKSActor+ExternalClientHandling.h"
#import "keychain/ckks/CKKSSecDbAdapter.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OctagonStateMachineHelpers.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

@implementation CKKSKeychainView (ExternalClientHandling)

- (CKKSKeychainViewState* _Nullable)externalManagedViewForRPC:(NSString*)viewName
                                                        error:(NSError**)error
{
    if(![self waitUntilReadyForRPCForOperation:@"external operation"
                                          fast:NO
                      errorOnNoCloudKitAccount:YES
                          errorOnPolicyMissing:YES
                                         error:error]) {
        return nil;
    }

    CKKSKeychainViewState* viewState = nil;
    for(CKKSKeychainViewState* vs in self.operationDependencies.allExternalManagedViews) {
        if([vs.zoneName isEqualToString:viewName]) {
            viewState = vs;
            break;
        }
    }

    if(!viewState) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNoSuchView
                                  description:[NSString stringWithFormat:@"Unknown external view: '%@'", viewName]];
        }
        return nil;
    }

    if(viewState.ckksManagedView) {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSErrorViewNotExternallyManaged
                                  description:[NSString stringWithFormat:@"View is not externally managed: '%@'", viewName]];
        }
        return nil;
    }

    return viewState;
}

- (void)resetExternallyManagedCloudKitView:(NSString*)viewName
                                     reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* error = nil;
    CKKSKeychainViewState* viewState = [self externalManagedViewForRPC:viewName error:&error];

    if(!viewState) {
        ckksnotice_global("ckkszone", "Can't reset CloudKit zone for view %@: %@", viewName, error);
        reply(error);
        return;
    }

    [self rpcResetCloudKit:[NSSet setWithObject:viewName] reply:reply];
}

#pragma mark - RPCs

- (void)proposeTLKForExternallyManagedView:(NSString*)viewName
                               proposedTLK:(CKKSExternalKey *)proposedTLK
                             wrappedOldTLK:(CKKSExternalKey * _Nullable)wrappedOldTLK
                                 tlkShares:(NSArray<CKKSExternalTLKShare*>*)shares
                                     reply:(void(^)(NSError* _Nullable error))reply
{
    NSError* error = nil;
    CKKSKeychainViewState* viewState = [self externalManagedViewForRPC:viewName error:&error];

    if(!viewState) {
        ckksnotice_global("ckkszone", "Can't propose TLKs for view %@: %@", viewName, error);
        reply(error);
        return;
    }

    WEAKIFY(self);
    OctagonStateTransitionGroupOperation* writeTLKToCloudKitOperation = [OctagonStateTransitionGroupOperation named:@"external-tlk-rpc"
                                                                                                          intending:CKKSStateReady
                                                                                                         errorState:CKKSStateReady
                                                                                                withBlockTakingSelf:^(OctagonStateTransitionGroupOperation * _Nonnull op) {
        STRONGIFY(self);
        [self.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
            NSError* tlkTranslationError = nil;
            CKKSKey* newTLK = [proposedTLK makeCKKSKey:viewState.zoneID error:&tlkTranslationError];

            if(!newTLK || tlkTranslationError) {
                ckkserror("ckks-se", viewState.zoneID, "Unable to make TLK: %@", tlkTranslationError);
                op.error = tlkTranslationError;
                return;
            }

            CKKSKey* classA = [proposedTLK makeFakeCKKSClassKey:SecCKKSKeyClassA zoneiD:viewState.zoneID error:&tlkTranslationError];
            CKKSKey* classC = [proposedTLK makeFakeCKKSClassKey:SecCKKSKeyClassC zoneiD:viewState.zoneID error:&tlkTranslationError];

            if(!classA || !classC) {
                ckkserror("ckks-se", viewState.zoneID, "Unable to make fake class keys: %@", tlkTranslationError);
                op.error = tlkTranslationError;
                return;
            }

            // If there's an old TLK, we need to load it for its stored CKRecord
            CKKSKey* oldTLKToWrite = nil;
            if(wrappedOldTLK) {
                NSError* loadError = nil;
                CKKSKey* existingOldTLK = [CKKSKey fromDatabase:wrappedOldTLK.uuid
                                                         zoneID:viewState.zoneID
                                                          error:&loadError];
                if(!existingOldTLK || loadError) {
                    ckkserror("ckks-se", viewState.zoneID, "Unable to load old TLK: %@", loadError);
                    reply(loadError);
                    return;
                }

                NSError* oldTLKError = nil;
                oldTLKToWrite = [wrappedOldTLK makeCKKSKey:viewState.zoneID error:&oldTLKError];

                if(!oldTLKToWrite || oldTLKError) {
                    ckkserror("ckks-se", viewState.zoneID, "Unable to convert old TLK: %@", oldTLKError);
                    op.error = oldTLKError;
                    return;
                }

                oldTLKToWrite.storedCKRecord = existingOldTLK.storedCKRecord;
            }

            NSMutableArray<CKKSTLKShareRecord*>* tlkShares = [NSMutableArray array];
            for(CKKSExternalTLKShare* externalShare in shares) {
                CKKSTLKShareRecord* share = [externalShare makeTLKShareRecord:viewState.zoneID];
                [tlkShares addObject:share];
            }

            ckksnotice("ckks-se", viewState.zoneID, "Trying to set TLK %@", newTLK);
            ckksnotice("ckks-se", viewState.zoneID, "Wrapped old TLK: %@", oldTLKToWrite);
            ckksnotice("ckks-se", viewState.zoneID, "TLKShares: %@", tlkShares);

            NSError* keyPointerError = nil;
            CKKSCurrentKeyPointer* currentTLKPointer = [CKKSCurrentKeyPointer forKeyClass:SecCKKSKeyClassTLK
                                                                              withKeyUUID:newTLK.uuid
                                                                                   zoneID:viewState.zoneID
                                                                                    error:&keyPointerError];

            if(!currentTLKPointer || keyPointerError) {
                ckkserror("ckks-se", viewState.zoneID, "Unable to create CKP: %@", keyPointerError);
                op.error = keyPointerError;
                return;
            }

            CKKSCurrentKeyPointer* currentClassAPointer = [CKKSCurrentKeyPointer forKeyClass:SecCKKSKeyClassA
                                                                                 withKeyUUID:classA.uuid
                                                                                      zoneID:viewState.zoneID
                                                                                       error:&keyPointerError];
            CKKSCurrentKeyPointer* currentClassCPointer = [CKKSCurrentKeyPointer forKeyClass:SecCKKSKeyClassC
                                                                                 withKeyUUID:classC.uuid
                                                                                      zoneID:viewState.zoneID
                                                                                       error:&keyPointerError];

            if(!currentClassAPointer || !currentClassCPointer) {
                ckkserror("ckks-se", viewState.zoneID, "Unable to create class CKP: %@", keyPointerError);
                op.error = keyPointerError;
                return;
            }

            NSMutableArray<CKRecord*>* recordsToSave = [NSMutableArray array];
            [recordsToSave addObject:[newTLK CKRecordWithZoneID:viewState.zoneID]];
            [recordsToSave addObject:[classA CKRecordWithZoneID:viewState.zoneID]];
            [recordsToSave addObject:[classC CKRecordWithZoneID:viewState.zoneID]];
            [recordsToSave addObject:[currentTLKPointer CKRecordWithZoneID:viewState.zoneID]];
            [recordsToSave addObject:[currentClassAPointer CKRecordWithZoneID:viewState.zoneID]];
            [recordsToSave addObject:[currentClassCPointer CKRecordWithZoneID:viewState.zoneID]];
            if(oldTLKToWrite) {
                [recordsToSave addObject:[oldTLKToWrite CKRecordWithZoneID:viewState.zoneID]];
            }

            for(CKKSTLKShareRecord* share in tlkShares) {
                [recordsToSave addObject:[share CKRecordWithZoneID:viewState.zoneID]];
            }

            NSMutableDictionary<CKRecordID*, CKRecord*>* savedRecordsDictionary = [NSMutableDictionary dictionary];
            for(CKRecord* record in recordsToSave) {
                savedRecordsDictionary[record.recordID] = record;
            }

            CKModifyRecordsOperation* modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave
                                                                                                     recordIDsToDelete:@[]];
            modifyRecordsOperation.atomic = TRUE;

            modifyRecordsOperation.configuration.automaticallyRetryNetworkFailures = NO;
            modifyRecordsOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
            modifyRecordsOperation.configuration.isCloudKitSupportOperation = YES;

            modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
            ckksnotice("ckks-se", viewState.zoneID, "QoS: %d; operation group is %@", (int)modifyRecordsOperation.qualityOfService, modifyRecordsOperation.group);
            ckksnotice("ckks-se", viewState.zoneID, "Beginning upload for %d records",
                       (int)recordsToSave.count);

            for(CKRecord* record in recordsToSave) {
                ckksinfo("ckks-se", record.recordID.zoneID, "Record to save: %@", record.recordID);
            }

            modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
                if(!error) {
                    ckksnotice("ckks-se", record.recordID.zoneID, "Record upload successful for %@ (%@)", record.recordID.recordName, record.recordChangeTag);
                } else {
                    ckkserror("ckks-se", record.recordID.zoneID, "error on row: %@ %@", error, record);
                }
            };

            CKKSResultOperation* waitUntilFinishedOp = [CKKSResultOperation named:@"wait-until-write-finished" withBlock:^{}];
            [op dependOnBeforeGroupFinished:waitUntilFinishedOp];

            modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
                STRONGIFY(self);

                [self.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    if(ckerror) {
                        ckkserror("ckks-se", viewState.zoneID, "error proposing new TLK: %@", ckerror);
                        [self.operationDependencies intransactionCKWriteFailed:ckerror attemptedRecordsChanged:savedRecordsDictionary];
                        op.error = ckerror;

                        [op runBeforeGroupFinished:waitUntilFinishedOp];
                        return CKKSDatabaseTransactionCommit;
                    }

                    ckksnotice("ckks-se", viewState.zoneID, "Completed uploading new TLK!");

                    for(CKRecord* record in savedRecords) {
                        [self.operationDependencies intransactionCKRecordChanged:record resync:false];
                    }

                    [op runBeforeGroupFinished:waitUntilFinishedOp];
                    return CKKSDatabaseTransactionCommit;
                }];
            };

            [self.operationDependencies.ckdatabase addOperation:modifyRecordsOperation];
        }];
    }];

    [self.stateMachine doSimpleStateMachineRPC:@"external-tlk-rpc"
                                            op:writeTLKToCloudKitOperation
                                  sourceStates:[NSSet setWithObject:CKKSStateReady]
                                         reply:reply];
}

- (void)fetchExternallyManagedViewKeyHierarchy:(NSString*)viewName
                                    forceFetch:(BOOL)forceFetch
                                         reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                                         NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                                         NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                                         NSError* _Nullable error))reply
{
    NSError* error = nil;
    CKKSKeychainViewState* viewState = [self externalManagedViewForRPC:viewName error:&error];

    if(!viewState) {
        ckksnotice_global("ckkszone", "Can't fetch CloudKit zone for view %@: %@", viewName, error);
        reply(nil, nil, nil, error);
        return;
    }

    if(forceFetch) {
        [self fetchCloudKitExternallyManagedViewKeyHierarchy:viewState
                                                       reply:reply];
    } else {
        // If the client does not want a forced fetch, don't wait for a fetch, even if one is required.
        // They are probably expecting a quick return, and prefer to get an error back quickly.
        [self loadKeys:viewState
                 reply:reply];
    }
}

- (void)fetchCloudKitExternallyManagedViewKeyHierarchy:(CKKSKeychainViewState*)viewState
                                                 reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                                                 NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                                                 NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                                                 NSError* _Nullable error))reply
{
    CKKSResultOperation* fetchOp = [self rpcFetchBecause:CKKSFetchBecauseSEAPIFetchRequest];

    WEAKIFY(self);
    CKKSResultOperation* respondToRPCOp = [CKKSResultOperation named:@"rpc-response" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull op) {
        STRONGIFY(self);

        if(fetchOp.error) {
            ckkserror("ckks-se", viewState.zoneID, "Error loading TLK pointer for this zone: %@", fetchOp.error);
            reply(nil, nil, nil, fetchOp.error);
            return;
        }

        [self loadKeys:viewState
                 reply:reply];
    }];

    [respondToRPCOp addDependency:fetchOp];

    [self scheduleOperation:respondToRPCOp];
}

- (void)loadKeys:(CKKSKeychainViewState*)viewState
           reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                           NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                           NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                           NSError* _Nullable error))reply
{
    [self.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{
        NSError* error = nil;

        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state:viewState.zoneName];
        if(ckse.changeToken == nil) {
            error = [NSError errorWithDomain:CKKSErrorDomain
                                        code:CKKSErrorFetchNotCompleted
                                 description:@"Initial fetch results not present; cannot provide accurate answer about TLK state"];
            ckkserror("ckks-se", viewState.zoneID, "Haven't successfully completed a fetch for this zone; returning %@", error);
                     reply(nil, nil, nil, error);
            return;
        }

        CKKSCurrentKeyPointer* currentTLKPointer = [CKKSCurrentKeyPointer tryFromDatabase:SecCKKSKeyClassTLK zoneID:viewState.zoneID error:&error];

        if(error != nil) {
            // No tlk pointer!
            ckkserror("ckks-se", viewState.zoneID, "Error loading TLK pointer for this zone: %@", error);
            reply(nil, nil, nil, error);
            return;
        }

        if(!currentTLKPointer) {
            // Returning all nils at adopter request.
            ckkserror("ckks-se", viewState.zoneID, "No TLK pointer for this zone");
            reply(nil, nil, nil, nil);
            return;
        }

        CKKSKey* tlk = [CKKSKey fromDatabaseAnyState:currentTLKPointer.currentKeyUUID
                                              zoneID:viewState.zoneID
                                               error:&error];

        if(!tlk || error) {
            ckkserror("ckks-se", viewState.zoneID, "No TLK for this zone");
            reply(nil, nil, nil, error);
            return;
        }

        CKKSExternalKey* rettlk = [[CKKSExternalKey alloc] initWithViewName:viewState.zoneID.zoneName
                                                                        tlk:tlk];

        NSArray<CKKSTLKShareRecord*>* tlkShares = [CKKSTLKShareRecord allForUUID:currentTLKPointer.currentKeyUUID
                                                                          zoneID:viewState.zoneID
                                                                           error:&error];
        if(!tlkShares || error) {
            ckkserror("ckks-se", viewState.zoneID, "Unable to load TLKShares for zone: %@", error);
            reply(nil, nil, nil, error);
            return;
        }

        // TODO: Ignore rolled TLKs for now

        NSMutableArray<CKKSExternalTLKShare*>* rettlkShares = [NSMutableArray array];
        for(CKKSTLKShareRecord* tlkShareRecord in tlkShares) {
            CKKSExternalTLKShare* external = [[CKKSExternalTLKShare alloc] initWithViewName:viewState.zoneID.zoneName
                                                                                   tlkShare:tlkShareRecord.share];
            [rettlkShares addObject:external];
        }

        reply(rettlk, @[], rettlkShares, nil);
    }];
}

- (void)modifyTLKSharesForExternallyManagedView:(NSString*)viewName
                                         adding:(NSArray<CKKSExternalTLKShare*>*)sharesToAdd
                                       deleting:(NSArray<CKKSExternalTLKShare*>*)sharesToDelete
                                          reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* error = nil;
    CKKSKeychainViewState* viewState = [self externalManagedViewForRPC:viewName error:&error];

    if(!viewState) {
        ckksnotice_global("ckkszone", "Can't modify CloudKit zone for view %@: %@", viewName, error);
        reply(error);
        return;
    }

    WEAKIFY(self);
    OctagonStateTransitionGroupOperation* writeTLKToCloudKitOperation = [OctagonStateTransitionGroupOperation named:@"external-tlk-rpc"
                                                                                                          intending:CKKSStateReady
                                                                                                         errorState:CKKSStateReady
                                                                                                withBlockTakingSelf:^(OctagonStateTransitionGroupOperation * _Nonnull op) {
        STRONGIFY(self);
        [self.databaseProvider dispatchSyncWithReadOnlySQLTransaction:^{

            // Are all the TLKShares for the current TLK?
            NSError* pointerLoadError = nil;
            CKKSCurrentKeyPointer* currentTLK = [CKKSCurrentKeyPointer tryFromDatabase:SecCKKSKeyClassTLK
                                                                                zoneID:viewState.zoneID
                                                                                 error:&pointerLoadError];

            if(!currentTLK || !currentTLK.currentKeyUUID || pointerLoadError) {
                ckkserror("ckks-se", viewState.zoneID, "Unable to load currentTLK: %@", pointerLoadError);
                op.error = pointerLoadError;
                return;
            }

            NSMutableArray<CKRecord*>* recordsToSave = [NSMutableArray array];
            for(CKKSExternalTLKShare* externalShare in sharesToAdd) {
                CKKSTLKShareRecord* record = [externalShare makeTLKShareRecord:viewState.zoneID];

                if(![record.tlkUUID isEqualToString:currentTLK.currentKeyUUID]) {
                    ckkserror("ckks-se", viewState.zoneID, "TLKShare is not for the current TLK(%@): %@", currentTLK.currentKeyUUID, externalShare);
                    op.error = [NSError errorWithDomain:CKKSErrorDomain
                                                   code:CKKSErrorTLKMismatch
                                            description:[NSString stringWithFormat:@"TLKShare is not for current TLK %@", currentTLK.currentKeyUUID]];
                    return;
                }

                [recordsToSave addObject:[record CKRecordWithZoneID:viewState.zoneID]];
            }

            NSMutableArray<CKRecordID*>* recordIDsToDelete = [NSMutableArray array];
            for(CKKSExternalTLKShare* externalShare in sharesToDelete) {
                CKKSTLKShareRecord* converted = [externalShare makeTLKShareRecord:viewState.zoneID];

                NSError* loadError = nil;
                CKKSTLKShareRecord* loaded = [CKKSTLKShareRecord fromDatabase:converted.tlkUUID
                                                               receiverPeerID:converted.share.receiverPeerID
                                                                 senderPeerID:converted.share.senderPeerID
                                                                       zoneID:viewState.zoneID
                                                                        error:&loadError];

                if(!loaded || loadError) {
                    ckkserror("ckks-se", viewState.zoneID, "Unable to load TLKShare (to delete): %@ %@", externalShare, loadError);
                    op.error = loadError;
                    return;
                }

                CKRecord* record = [loaded CKRecordWithZoneID:viewState.zoneID];
                [recordIDsToDelete addObject:record.recordID];
            }

            if(recordsToSave.count == 0 && recordIDsToDelete.count == 0) {
                ckksnotice("ckks-se", viewState.zoneID, "Requested modifications are a no-op; claiming success");
                return;
            }

            CKModifyRecordsOperation* modifyRecordsOperation = [[CKModifyRecordsOperation alloc] initWithRecordsToSave:recordsToSave
                                                                                                     recordIDsToDelete:recordIDsToDelete];
            modifyRecordsOperation.atomic = TRUE;

            modifyRecordsOperation.configuration.automaticallyRetryNetworkFailures = NO;
            modifyRecordsOperation.configuration.discretionaryNetworkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
            modifyRecordsOperation.configuration.isCloudKitSupportOperation = YES;

            modifyRecordsOperation.savePolicy = CKRecordSaveIfServerRecordUnchanged;
            ckksnotice("ckks-se", viewState.zoneID, "QoS: %d; operation group is %@", (int)modifyRecordsOperation.qualityOfService, modifyRecordsOperation.group);
            ckksnotice("ckks-se", viewState.zoneID, "Beginning upload for %d records, deleting %d records",
                       (int)recordsToSave.count,
                       (int)recordIDsToDelete.count);

            for(CKRecord* record in recordsToSave) {
                ckksinfo("ckks-se", record.recordID.zoneID, "Record to save: %@", record.recordID);
            }
            for(CKRecordID* recordID in recordIDsToDelete) {
                ckksinfo("ckks-se", recordID.zoneID, "Record to delete: %@", recordID);
            }

            modifyRecordsOperation.perRecordCompletionBlock = ^(CKRecord *record, NSError * _Nullable error) {
                if(!error) {
                    ckksnotice("ckks-se", record.recordID.zoneID, "Record upload successful for %@ (%@)", record.recordID.recordName, record.recordChangeTag);
                } else {
                    ckkserror("ckks-se", record.recordID.zoneID, "error on row: %@ %@", error, record);
                }
            };

            CKKSResultOperation* waitUntilFinishedOp = [CKKSResultOperation named:@"wait-until-write-finished" withBlock:^{}];
            [op dependOnBeforeGroupFinished:waitUntilFinishedOp];

            modifyRecordsOperation.modifyRecordsCompletionBlock = ^(NSArray<CKRecord *> *savedRecords, NSArray<CKRecordID *> *deletedRecordIDs, NSError *ckerror) {
                STRONGIFY(self);

                [self.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    if(ckerror) {
                        ckkserror("ckks-se", viewState.zoneID, "error proposing new TLK: %@", ckerror);
                        op.error = ckerror;
                        [op runBeforeGroupFinished:waitUntilFinishedOp];
                        return CKKSDatabaseTransactionCommit;
                    }

                    ckksnotice("ckks-se", viewState.zoneID, "Completed modifying TLK share records!");

                    for(CKRecord* record in savedRecords) {
                        [self.operationDependencies intransactionCKRecordChanged:record resync:false];
                    }

                    // Because we know that the only thing we're deleting are TLKShares, we can cheat here
                    for(CKRecordID* recordID in deletedRecordIDs) {
                        [self.operationDependencies intransactionCKRecordDeleted:recordID recordType:SecCKRecordTLKShareType resync:false];
                    }

                    [op runBeforeGroupFinished:waitUntilFinishedOp];
                    return CKKSDatabaseTransactionCommit;
                }];
            };

            [self.operationDependencies.ckdatabase addOperation:modifyRecordsOperation];
        }];
    }];


    [self.stateMachine doSimpleStateMachineRPC:@"external-tlkshare-modification-rpc"
                                            op:writeTLKToCloudKitOperation
                                  sourceStates:[NSSet setWithObject:CKKSStateReady]
                                         reply:reply];
}

@end

#endif // OCTAGON
