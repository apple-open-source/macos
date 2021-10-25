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

#import "keychain/ckks/CKKSFixups.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSCurrentItemPointer.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"

@implementation CKKSFixups
+ (CKKSState* _Nullable)fixupOperation:(CKKSFixup)lastfixup
{
    if(lastfixup == CKKSCurrentFixupNumber) {
        return nil;
    }

    if(lastfixup < CKKSFixupRefetchCurrentItemPointers) {
        return CKKSStateFixupRefetchCurrentItemPointers;
    }

    if(lastfixup < CKKSFixupFetchTLKShares) {
        return CKKSStateFixupFetchTLKShares;
    }

    if(lastfixup < CKKSFixupLocalReload) {
        return CKKSStateFixupLocalReload;
    }

    if(lastfixup < CKKSFixupResaveDeviceStateEntries) {
        return CKKSStateFixupResaveDeviceStateEntries;
    }

    if(lastfixup < CKKSFixupDeleteAllCKKSTombstones) {
        return CKKSStateFixupDeleteAllCKKSTombstones;
    }

    return nil;
}
@end

#pragma mark - CKKSFixupRefetchAllCurrentItemPointers

@interface CKKSFixupRefetchAllCurrentItemPointers ()
@property CKOperationGroup* group;
@end

// In <rdar://problem/34916549> CKKS: current item pointer CKRecord resurrection,
//  We found that some devices could end up with multiple current item pointer records for a given record ID.
//  This fixup will fetch all CKRecords matching any existing current item pointer records, and then trigger processing.
@implementation CKKSFixupRefetchAllCurrentItemPointers
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                             ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if((self = [super init])) {
        _deps = operationDependencies;
        _group = ckoperationGroup;

        _intendedState = CKKSStateFixupFetchTLKShares;
        _nextState = CKKSStateError;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:RefetchAllCurrentItemPointers (%@)>", self.deps.views];
}
- (void)groupStart {
    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{

            NSError* error = nil;

            NSArray<CKKSCurrentItemPointer*>* cips = [CKKSCurrentItemPointer allInZone:viewState.zoneID error:&error];
            if(error) {
                ckkserror("ckksfixup", viewState.zoneID, "Couldn't fetch current item pointers: %@", error);
                return CKKSDatabaseTransactionRollback;
            }

            NSMutableSet<CKRecordID*>* recordIDs = [NSMutableSet set];
            for(CKKSCurrentItemPointer* cip in cips) {
                CKRecordID* recordID = cip.storedCKRecord.recordID;
                if(recordID) {
                    ckksnotice("ckksfixup", viewState.zoneID, "Re-fetching %@ for %@", recordID, cip);
                    [recordIDs addObject:recordID];
                } else {
                    ckkserror("ckksfixup", viewState.zoneID, "No record ID for stored %@", cip);
                }
            }

            if(recordIDs.count == 0) {
                ckksnotice("ckksfixup", viewState.zoneID, "No existing CIPs; fixup complete");
            }

            WEAKIFY(self);
            NSBlockOperation* doneOp = [NSBlockOperation named:@"fetch-records-operation-complete" withBlock:^{}];
            CKDatabaseOperation<CKKSFetchRecordsOperation>* fetch = [[self.deps.cloudKitClassDependencies.fetchRecordsOperationClass alloc] initWithRecordIDs:[recordIDs allObjects]];
            fetch.fetchRecordsCompletionBlock = ^(NSDictionary<CKRecordID *,CKRecord *> * _Nullable recordsByRecordID, NSError * _Nullable error) {
                STRONGIFY(self);

                [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    if(error) {
                        ckkserror("ckksfixup", viewState.zoneID, "Finished record fetch with error: %@", error);

                        NSDictionary<CKRecordID*,NSError*>* partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];
                        if([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorPartialFailure && partialErrors) {
                            // Check if any of these records no longer exist on the server
                            for(CKRecordID* recordID in partialErrors.keyEnumerator) {
                                NSError* recordError = partialErrors[recordID];
                                if(recordError && [recordError.domain isEqualToString:CKErrorDomain] && recordError.code == CKErrorUnknownItem) {
                                    ckkserror("ckksfixup", viewState.zoneID, "CloudKit believes %@ no longer exists", recordID);
                                    [self.deps intransactionCKRecordDeleted:recordID recordType:SecCKRecordCurrentItemType resync:true];
                                } else {
                                    ckkserror("ckksfixup", viewState.zoneID, "Unknown error for %@: %@", recordID, error);
                                    self.error = error;
                                }
                            }
                        } else {
                            self.error = error;
                        }
                    } else {
                        ckksnotice("ckksfixup", viewState.zoneID, "Finished record fetch successfully");
                    }

                    for(CKRecordID* recordID in recordsByRecordID) {
                        CKRecord* record = recordsByRecordID[recordID];
                        ckksnotice("ckksfixup", viewState.zoneID, "Recieved record %@", record);
                        [self.deps intransactionCKRecordChanged:record resync:true];
                    }

                    if(!self.error) {
                        // Now, update the zone state entry to be at this level
                        NSError* localerror = nil;
                        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:viewState.zoneID.zoneName error:&localerror];
                        ckse.lastFixup = CKKSFixupRefetchCurrentItemPointers;
                        [ckse saveToDatabase:&localerror];
                        if(localerror) {
                            ckkserror("ckksfixup", viewState.zoneID, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
                        } else {
                            ckksnotice("ckksfixup", viewState.zoneID, "Updated zone fixup state to CKKSFixupRefetchCurrentItemPointers");
                            self.nextState = self.intendedState;
                        }
                    }

                    [self runBeforeGroupFinished:doneOp];
                    return CKKSDatabaseTransactionCommit;
                }];
            };
            [self.deps.ckdatabase addOperation:fetch];
            [self dependOnBeforeGroupFinished:fetch];
            [self dependOnBeforeGroupFinished:doneOp];

            return CKKSDatabaseTransactionCommit;
        }];
    }
}
@end

#pragma mark - CKKSFixupFetchAllTLKShares

@interface CKKSFixupFetchAllTLKShares ()
@property CKOperationGroup* group;
@end

// In <rdar://problem/34901306> CKKSTLK: TLKShare CloudKit upload/download on TLK change, trust set addition
// We introduced TLKShare records.
// Older devices will throw them away, so on upgrade, they must refetch them
@implementation CKKSFixupFetchAllTLKShares
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                             ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if((self = [super init])) {
        _deps = operationDependencies;
        _group = ckoperationGroup;

        _intendedState = CKKSStateFixupLocalReload;
        _nextState = CKKSStateError;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:FetchAllTLKShares (%@)>", self.deps.views];
}
- (void)groupStart {
    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            WEAKIFY(self);
            NSBlockOperation* doneOp = [NSBlockOperation named:@"fetch-records-operation-complete" withBlock:^{}];

            NSPredicate *yes = [NSPredicate predicateWithValue:YES];
            CKQuery *query = [[CKQuery alloc] initWithRecordType:SecCKRecordTLKShareType predicate:yes];

            CKDatabaseOperation<CKKSQueryOperation>* fetch = [[self.deps.cloudKitClassDependencies.queryOperationClass alloc] initWithQuery:query];
            fetch.zoneID = viewState.zoneID;
            fetch.desiredKeys = nil;

            fetch.recordFetchedBlock = ^(CKRecord * _Nonnull record) {
                STRONGIFY(self);
                [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    ckksnotice("ckksfixup", viewState.zoneID, "Recieved tlk share record from query: %@", record);

                    [self.deps intransactionCKRecordChanged:record resync:true];
                    return CKKSDatabaseTransactionCommit;
                }];
            };

            fetch.queryCompletionBlock = ^(CKQueryCursor * _Nullable cursor, NSError * _Nullable error) {
                STRONGIFY(self);

                [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
                    if(error) {
                        ckkserror("ckksfixup", viewState.zoneID, "Couldn't fetch all TLKShare records: %@", error);
                        self.error = error;
                        return CKKSDatabaseTransactionRollback;
                    }

                    ckksnotice("ckksfixup", viewState.zoneID, "Successfully fetched TLKShare records (%@)", cursor);

                    NSError* localerror = nil;
                    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:viewState.zoneID.zoneName error:&localerror];
                    ckse.lastFixup = CKKSFixupFetchTLKShares;
                    [ckse saveToDatabase:&localerror];
                    if(localerror) {
                        ckkserror("ckksfixup", viewState.zoneID, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
                    } else {
                        ckksnotice("ckksfixup", viewState.zoneID, "Updated zone fixup state to CKKSFixupFetchTLKShares");
                        self.nextState = self.intendedState;
                    }
                    return CKKSDatabaseTransactionCommit;
                }];
                [self runBeforeGroupFinished:doneOp];
            };

            [self.deps.ckdatabase addOperation:fetch];
            [self dependOnBeforeGroupFinished:fetch];
            [self dependOnBeforeGroupFinished:doneOp];

            return CKKSDatabaseTransactionCommit;
        }];
    }
}
@end

#pragma mark - CKKSFixupLocalReloadOperation

@interface CKKSFixupLocalReloadOperation ()
@property CKOperationGroup* group;
@property CKKSFixup fixupNumber;
@end

// In <rdar://problem/35540228> Server Generated CloudKit "Manatee Identity Lost"
// items could be deleted from the local keychain after CKKS believed they were already synced, and therefore wouldn't resync
// Perform a local resync operation
//
// In <rdar://problem/60650208> CKKS: adjust UUID and tombstone handling
// some CKKS participants uploaded entries with tomb=1 to CKKS
// Performing a local reload operation should find and delete those entries
@implementation CKKSFixupLocalReloadOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
                                  fixupNumber:(CKKSFixup)fixupNumber
                             ckoperationGroup:(CKOperationGroup *)ckoperationGroup
                                     entering:(CKKSState*)state
{
    if((self = [super init])) {
        _deps = operationDependencies;
        _fixupNumber = fixupNumber;
        _group = ckoperationGroup;

        _intendedState = state;
        _nextState = CKKSStateError;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:LocalReload (%d)(%@)>", (int)self.fixupNumber, self.deps.views];
}
- (void)groupStart {
    WEAKIFY(self);

    CKKSResultOperation* reload = [[CKKSReloadAllItemsOperation alloc] initWithOperationDependencies:self.deps];
    [self runBeforeGroupFinished:reload];

    CKKSResultOperation* cleanup = [CKKSResultOperation named:@"local-reload-cleanup" withBlock:^{
        STRONGIFY(self);

        if(reload.error) {
            ckkserror_global("ckksfixup", "Couldn't perform a reload: %@", reload.error);
            self.error = reload.error;
            return;
        }

        [self.deps.databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
                ckksnotice("ckksfixup", viewState.zoneID, "Successfully performed a reload fixup. New fixup number is %d",
                           (int)self.fixupNumber);

                NSError* localerror = nil;
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:viewState.zoneID.zoneName error:&localerror];
                ckse.lastFixup = self.fixupNumber;
                [ckse saveToDatabase:&localerror];
                if(localerror) {
                    ckkserror("ckksfixup", viewState.zoneID, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
                } else {
                    ckksnotice("ckksfixup", viewState.zoneID, "Updated zone fixup state to CKKSFixupLocalReload");
                    self.nextState = self.intendedState;
                }
            }
            return CKKSDatabaseTransactionCommit;
        }];
    }];
    [cleanup addNullableDependency:reload];
    [self runBeforeGroupFinished:cleanup];
}
@end

#pragma mark - CKKSFixupResaveDeviceStateEntriesOperation

@implementation CKKSFixupResaveDeviceStateEntriesOperation: CKKSGroupOperation
@synthesize intendedState = _intendedState;
@synthesize nextState = _nextState;

- (instancetype)initWithOperationDependencies:(CKKSOperationDependencies*)operationDependencies
{
    if((self = [super init])) {
        _deps = operationDependencies;

        _intendedState = CKKSStateFixupDeleteAllCKKSTombstones;
        _nextState = CKKSStateError;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:ResaveCDSE (%@)>", self.deps.views];
}

- (void)groupStart {
    // This operation simply loads all CDSEs, remakes them from their CKRecord, and resaves them

    id<CKKSDatabaseProviderProtocol> databaseProvider = self.deps.databaseProvider;

    for(CKKSKeychainViewState* viewState in self.deps.activeManagedViews) {
        [databaseProvider dispatchSyncWithSQLTransaction:^CKKSDatabaseTransactionResult{
            NSError* error = nil;
            NSArray<CKKSDeviceStateEntry*>* cdses = [CKKSDeviceStateEntry allInZone:viewState.zoneID
                                                                              error:&error];

            if(error) {
                ckkserror("ckksfixup", viewState.zoneID, "Unable to fetch all CDSEs: %@", error);
                self.error = error;
                return CKKSDatabaseTransactionRollback;
            }

            for(CKKSDeviceStateEntry* cdse in cdses) {
                CKRecord* record = cdse.storedCKRecord;

                if(record) {
                    [cdse setFromCKRecord:record];
                    [cdse saveToDatabase:&error];
                    if(error) {
                        ckkserror("ckksfixup", viewState.zoneID, "Unable to save CDSE: %@", error);
                        self.error = error;
                        return CKKSDatabaseTransactionRollback;
                    }
                } else {
                    ckksnotice("ckksfixup", viewState.zoneID, "Saved CDSE has no stored record: %@", cdse);
                }
            }

            ckksnotice("ckksfixup", viewState.zoneID, "Successfully performed a ResaveDeviceState fixup");

            NSError* localerror = nil;
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:viewState.zoneID.zoneName error:&localerror];
            ckse.lastFixup = CKKSFixupResaveDeviceStateEntries;
            [ckse saveToDatabase:&localerror];
            if(localerror) {
                ckkserror("ckksfixup", viewState.zoneID, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
                self.error = localerror;
                return CKKSDatabaseTransactionRollback;
            }

            ckksnotice("ckksfixup", viewState.zoneID, "Updated zone fixup state to CKKSFixupResaveDeviceStateEntries");
            self.nextState = self.intendedState;

            return CKKSDatabaseTransactionCommit;
        }];
    }
}


@end

#endif // OCTAGON
