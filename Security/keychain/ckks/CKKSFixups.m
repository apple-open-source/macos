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
+(CKKSGroupOperation*)fixup:(CKKSFixup)lastfixup for:(CKKSKeychainView*)keychainView
{
    if(lastfixup == CKKSCurrentFixupNumber) {
        return nil;
    }

    CKOperationGroup* fixupCKGroup = [CKOperationGroup CKKSGroupWithName:@"fixup"];
    CKKSGroupOperation* fixups = [[CKKSGroupOperation alloc] init];
    fixups.name = @"ckks-fixups";

    CKKSResultOperation* previousOp = keychainView.holdFixupOperation;

    if(lastfixup < CKKSFixupRefetchCurrentItemPointers) {
        CKKSResultOperation* refetch = [[CKKSFixupRefetchAllCurrentItemPointers alloc] initWithCKKSKeychainView:keychainView
                                                                                               ckoperationGroup:fixupCKGroup];
        [refetch addNullableDependency:previousOp];
        [fixups runBeforeGroupFinished:refetch];
        previousOp = refetch;
    }

    if(lastfixup < CKKSFixupFetchTLKShares) {
        CKKSResultOperation* fetchShares = [[CKKSFixupFetchAllTLKShares alloc] initWithCKKSKeychainView:keychainView
                                                                                       ckoperationGroup:fixupCKGroup];
        [fetchShares addNullableSuccessDependency:previousOp];
        [fixups runBeforeGroupFinished:fetchShares];
        previousOp = fetchShares;
    }

    if(lastfixup < CKKSFixupLocalReload) {
        CKKSResultOperation* localSync = [[CKKSFixupLocalReloadOperation alloc] initWithCKKSKeychainView:keychainView
                                                                                        ckoperationGroup:fixupCKGroup];
        [localSync addNullableSuccessDependency:previousOp];
        [fixups runBeforeGroupFinished:localSync];
        previousOp = localSync;
    }

    if(lastfixup < CKKSFixupResaveDeviceStateEntries) {
        CKKSResultOperation* resave = [[CKKSFixupResaveDeviceStateEntriesOperation alloc] initWithCKKSKeychainView:keychainView];

        [resave addNullableSuccessDependency:previousOp];
        [fixups runBeforeGroupFinished:resave];
        previousOp = resave;
    }

    (void)previousOp;
    return fixups;
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
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView
                        ckoperationGroup:(CKOperationGroup *)ckoperationGroup
{
    if((self = [super init])) {
        _ckks = keychainView;
        _group = ckoperationGroup;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:RefetchAllCurrentItemPointers (%@)>", self.ckks];
}
- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksfixup", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
        return;
    }

    [ckks dispatchSyncWithAccountKeys:^bool {
        NSError* error = nil;

        NSArray<CKKSCurrentItemPointer*>* cips = [CKKSCurrentItemPointer allInZone: ckks.zoneID error:&error];
        if(error) {
            ckkserror("ckksfixup", ckks, "Couldn't fetch current item pointers: %@", error);
            return false;
        }

        NSMutableSet<CKRecordID*>* recordIDs = [NSMutableSet set];
        for(CKKSCurrentItemPointer* cip in cips) {
            CKRecordID* recordID = cip.storedCKRecord.recordID;
            if(recordID) {
                ckksnotice("ckksfixup", ckks, "Re-fetching %@ for %@", recordID, cip);
                [recordIDs addObject:recordID];
            } else {
                ckkserror("ckksfixup", ckks, "No record ID for stored %@", cip);
            }
        }

        if(recordIDs.count == 0) {
            ckksnotice("ckksfixup", ckks, "No existing CIPs; fixup complete");
        }

        WEAKIFY(self);
        NSBlockOperation* doneOp = [NSBlockOperation named:@"fetch-records-operation-complete" withBlock:^{}];
        id<CKKSFetchRecordsOperation> fetch = [[ckks.cloudKitClassDependencies.fetchRecordsOperationClass alloc] initWithRecordIDs: [recordIDs allObjects]];
        fetch.fetchRecordsCompletionBlock = ^(NSDictionary<CKRecordID *,CKRecord *> * _Nullable recordsByRecordID, NSError * _Nullable error) {
            STRONGIFY(self);
            CKKSKeychainView* strongCKKS = self.ckks;

            [strongCKKS dispatchSync:^bool{
                if(error) {
                    ckkserror("ckksfixup", strongCKKS, "Finished record fetch with error: %@", error);

                    NSDictionary<CKRecordID*,NSError*>* partialErrors = error.userInfo[CKPartialErrorsByItemIDKey];
                    if([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorPartialFailure && partialErrors) {
                        // Check if any of these records no longer exist on the server
                        for(CKRecordID* recordID in partialErrors.keyEnumerator) {
                            NSError* recordError = partialErrors[recordID];
                            if(recordError && [recordError.domain isEqualToString:CKErrorDomain] && recordError.code == CKErrorUnknownItem) {
                                ckkserror("ckksfixup", strongCKKS, "CloudKit believes %@ no longer exists", recordID);
                                [strongCKKS _onqueueCKRecordDeleted: recordID recordType:SecCKRecordCurrentItemType resync:true];
                            } else {
                                ckkserror("ckksfixup", strongCKKS, "Unknown error for %@: %@", recordID, error);
                                self.error = error;
                            }
                        }
                    } else {
                        self.error = error;
                    }
                } else {
                    ckksnotice("ckksfixup", strongCKKS, "Finished record fetch successfully");
                }

                for(CKRecordID* recordID in recordsByRecordID) {
                    CKRecord* record = recordsByRecordID[recordID];
                    ckksnotice("ckksfixup", strongCKKS, "Recieved record %@", record);
                    [self.ckks _onqueueCKRecordChanged:record resync:true];
                }

                if(!self.error) {
                    // Now, update the zone state entry to be at this level
                    NSError* localerror = nil;
                    CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:strongCKKS.zoneName error:&localerror];
                    ckse.lastFixup = CKKSFixupRefetchCurrentItemPointers;
                    [ckse saveToDatabase:&localerror];
                    if(localerror) {
                        ckkserror("ckksfixup", strongCKKS, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
                    } else {
                        ckksnotice("ckksfixup", strongCKKS, "Updated zone fixup state to CKKSFixupRefetchCurrentItemPointers");
                    }
                }

                [self runBeforeGroupFinished:doneOp];
                return true;
            }];
        };
        [ckks.database addOperation: fetch];
        [self dependOnBeforeGroupFinished:fetch];
        [self dependOnBeforeGroupFinished:doneOp];

        return true;
    }];
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
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView
                        ckoperationGroup:(CKOperationGroup *)ckoperationGroup
{
    if((self = [super init])) {
        _ckks = keychainView;
        _group = ckoperationGroup;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:FetchAllTLKShares (%@)>", self.ckks];
}
- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksfixup", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:@"securityd" code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
        return;
    }

    [ckks dispatchSyncWithAccountKeys:^bool {
        WEAKIFY(self);
        NSBlockOperation* doneOp = [NSBlockOperation named:@"fetch-records-operation-complete" withBlock:^{}];

        NSPredicate *yes = [NSPredicate predicateWithValue:YES];
        CKQuery *query = [[CKQuery alloc] initWithRecordType:SecCKRecordTLKShareType predicate:yes];

        id<CKKSQueryOperation> fetch = [[ckks.cloudKitClassDependencies.queryOperationClass alloc] initWithQuery:query];
        fetch.zoneID = ckks.zoneID;
        fetch.desiredKeys = nil;

        fetch.recordFetchedBlock = ^(CKRecord * _Nonnull record) {
            STRONGIFY(self);
            CKKSKeychainView* strongCKKS = self.ckks;
            [strongCKKS dispatchSync:^bool{
                ckksnotice("ckksfixup", strongCKKS, "Recieved tlk share record from query: %@", record);

                [strongCKKS _onqueueCKRecordChanged:record resync:true];
                return true;
            }];
        };

        fetch.queryCompletionBlock = ^(CKQueryCursor * _Nullable cursor, NSError * _Nullable error) {
            STRONGIFY(self);
            CKKSKeychainView* strongCKKS = self.ckks;

            [strongCKKS dispatchSync:^bool{
                if(error) {
                    ckkserror("ckksfixup", strongCKKS, "Couldn't fetch all TLKShare records: %@", error);
                    self.error = error;
                    return false;
                }

                ckksnotice("ckksfixup", strongCKKS, "Successfully fetched TLKShare records (%@)", cursor);

                NSError* localerror = nil;
                CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:strongCKKS.zoneName error:&localerror];
                ckse.lastFixup = CKKSFixupFetchTLKShares;
                [ckse saveToDatabase:&localerror];
                if(localerror) {
                    ckkserror("ckksfixup", strongCKKS, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
                } else {
                    ckksnotice("ckksfixup", strongCKKS, "Updated zone fixup state to CKKSFixupFetchTLKShares");
                }
                return true;
            }];
            [self runBeforeGroupFinished:doneOp];
        };

        [ckks.database addOperation: fetch];
        [self dependOnBeforeGroupFinished:fetch];
        [self dependOnBeforeGroupFinished:doneOp];

        return true;
    }];
}
@end

#pragma mark - CKKSFixupLocalReloadOperation

@interface CKKSFixupLocalReloadOperation ()
@property CKOperationGroup* group;
@end

// In <rdar://problem/35540228> Server Generated CloudKit "Manatee Identity Lost"
// items could be deleted from the local keychain after CKKS believed they were already synced, and therefore wouldn't resync
// Perform a local resync operation
@implementation CKKSFixupLocalReloadOperation
- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView
                        ckoperationGroup:(CKOperationGroup *)ckoperationGroup
{
    if((self = [super init])) {
        _ckks = keychainView;
        _group = ckoperationGroup;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:LocalReload (%@)>", self.ckks];
}
- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;
    WEAKIFY(self);
    if(!ckks) {
        ckkserror("ckksfixup", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:CKKSErrorDomain code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
        return;
    }

    CKKSResultOperation* reload = [[CKKSReloadAllItemsOperation alloc] initWithCKKSKeychainView:ckks];
    [self runBeforeGroupFinished:reload];

    CKKSResultOperation* cleanup = [CKKSResultOperation named:@"local-reload-cleanup" withBlock:^{
        STRONGIFY(self);
        __strong __typeof(self.ckks) strongCKKS = self.ckks;
        [strongCKKS dispatchSync:^bool{
            if(reload.error) {
                ckkserror("ckksfixup", strongCKKS, "Couldn't perform a reload: %@", reload.error);
                self.error = reload.error;
                return false;
            }

            ckksnotice("ckksfixup", strongCKKS, "Successfully performed a reload fixup");

            NSError* localerror = nil;
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:strongCKKS.zoneName error:&localerror];
            ckse.lastFixup = CKKSFixupLocalReload;
            [ckse saveToDatabase:&localerror];
            if(localerror) {
                ckkserror("ckksfixup", strongCKKS, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
            } else {
                ckksnotice("ckksfixup", strongCKKS, "Updated zone fixup state to CKKSFixupLocalReload");
            }
            return true;
        }];
    }];
    [cleanup addNullableDependency:reload];
    [self runBeforeGroupFinished:cleanup];
}
@end

#pragma mark - CKKSFixupResaveDeviceStateEntriesOperation

@implementation CKKSFixupResaveDeviceStateEntriesOperation: CKKSGroupOperation

- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)keychainView
{
    if((self = [super init])) {
        _ckks = keychainView;
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat:@"<CKKSFixup:ResaveCDSE (%@)>", self.ckks];
}

- (void)groupStart {
    CKKSKeychainView* ckks = self.ckks;

    if(!ckks) {
        ckkserror("ckksfixup", ckks, "no CKKS object");
        self.error = [NSError errorWithDomain:CKKSErrorDomain code:errSecInternalError userInfo:@{NSLocalizedDescriptionKey: @"no CKKS object"}];
        return;
    }

    // This operation simply loads all CDSEs, remakes them from their CKRecord, and resaves them
    [ckks dispatchSync:^bool {
        NSError* error = nil;
        NSArray<CKKSDeviceStateEntry*>* cdses = [CKKSDeviceStateEntry allInZone:ckks.zoneID
                                                                          error:&error];

        if(error) {
            ckkserror("ckksfixup", ckks, "Unable to fetch all CDSEs: %@", error);
            self.error = error;
            return false;
        }

        for(CKKSDeviceStateEntry* cdse in cdses) {
            CKRecord* record = cdse.storedCKRecord;

            if(record) {
                [cdse setFromCKRecord:record];
                [cdse saveToDatabase:&error];
                if(error) {
                    ckkserror("ckksfixup", ckks, "Unable to save CDSE: %@", error);
                    self.error = error;
                    return false;
                }
            } else {
                ckksnotice("ckksfixup", ckks, "Saved CDSE has no stored record: %@", cdse);
            }
        }

        ckksnotice("ckksfixup", ckks, "Successfully performed a ResaveDeviceState fixup");

        NSError* localerror = nil;
        CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry fromDatabase:ckks.zoneName error:&localerror];
        ckse.lastFixup = CKKSFixupResaveDeviceStateEntries;
        [ckse saveToDatabase:&localerror];
        if(localerror) {
            ckkserror("ckksfixup", ckks, "Couldn't save CKKSZoneStateEntry(%@): %@", ckse, localerror);
            self.error = localerror;
            return false;
        }

        ckksnotice("ckksfixup", ckks, "Updated zone fixup state to CKKSFixupResaveDeviceStateEntries");

        return true;
    }];
}


@end

#endif // OCTAGON
