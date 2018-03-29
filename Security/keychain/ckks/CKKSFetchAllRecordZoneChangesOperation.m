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

#import <Foundation/Foundation.h>

#if OCTAGON

#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSManifestLeafRecord.h"
#import "CKKSPowerCollection.h"
#include <securityd/SecItemServer.h>


@interface CKKSFetchAllRecordZoneChangesOperation()
@property CKDatabaseOperation<CKKSFetchRecordZoneChangesOperation>* fetchRecordZoneChangesOperation;
@property CKOperationGroup* ckoperationGroup;
@property (assign) NSUInteger fetchedItems;
@end

@implementation CKKSFetchAllRecordZoneChangesOperation

// Sets up an operation to fetch all changes from the server, and collect them until we know if the fetch completes.
// As a bonus, you can depend on this operation without worry about NSOperation completion block dependency issues.

- (instancetype)init {
    return nil;
}

- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks
                            fetchReasons:(NSSet<CKKSFetchBecause*>*)fetchReasons
                        ckoperationGroup:(CKOperationGroup*)ckoperationGroup {

    if(self = [super init]) {
        _ckks = ckks;
        _ckoperationGroup = ckoperationGroup;
        _fetchReasons = fetchReasons;
        _zoneID = ckks.zoneID;

        _resync = false;

        _modifications = [[NSMutableDictionary alloc] init];
        _deletions = [[NSMutableDictionary alloc] init];

        // Can't fetch unless the zone is created.
        [self addNullableDependency:ckks.zoneSetupOperation];
    }
    return self;
}

- (void)_onqueueRecordsChanged:(NSArray*)records
{
    for (CKRecord* record in records) {
        [self.ckks _onqueueCKRecordChanged:record resync:self.resync];
    }
}

- (void)_updateLatestTrustedManifest
{
    CKKSKeychainView* ckks = self.ckks;
    NSError* error = nil;
    NSArray* pendingManifests = [CKKSPendingManifest all:&error];
    NSUInteger greatestKnownManifestGeneration = [CKKSManifest greatestKnownGenerationCount];
    for (CKKSPendingManifest* manifest in pendingManifests) {
        if (manifest.generationCount >= greatestKnownManifestGeneration) {
            [manifest commitToDatabaseWithError:&error];
        }
        else {
            // if this is an older generation, just get rid of it
            [manifest deleteFromDatabase:&error];
        }
    }

    if (![ckks _onqueueUpdateLatestManifestWithError:&error]) {
        self.error = error;
        ckkserror("ckksfetch", ckks, "failed to get latest manifest");
    }
}

- (void)_onqueueProcessRecordDeletions
{
    CKKSKeychainView* ckks = self.ckks;
    [self.deletions enumerateKeysAndObjectsUsingBlock:^(CKRecordID * _Nonnull recordID, NSString * _Nonnull recordType, BOOL * _Nonnull stop) {
        ckksinfo("ckksfetch", ckks, "Processing record deletion(%@): %@", recordType, recordID);

        // <rdar://problem/32475600> CKKS: Check Current Item pointers in the Manifest
        // TODO: check that these deletions match a manifest upload
        // Delegate these back up into the CKKS object for processing
        [ckks _onqueueCKRecordDeleted:recordID recordType:recordType resync:self.resync];
    }];
}

- (void)_onqueueScanForExtraneousLocalItems
{
    // TODO: must scan through all CKMirrorEntries and determine if any exist that CloudKit didn't tell us about
    CKKSKeychainView* ckks = self.ckks;
    NSError* error = nil;
    if(self.resync) {
        ckksnotice("ckksresync", ckks, "Comparing local UUIDs against the CloudKit list");
        NSMutableArray<NSString*>* uuids = [[CKKSMirrorEntry allUUIDs:ckks.zoneID error:&error] mutableCopy];

        for(NSString* uuid in uuids) {
            if([self.modifications objectForKey: [[CKRecordID alloc] initWithRecordName: uuid zoneID: ckks.zoneID]]) {
                ckksnotice("ckksresync", ckks, "UUID %@ is still in CloudKit; carry on.", uuid);
            } else {
                CKKSMirrorEntry* ckme = [CKKSMirrorEntry tryFromDatabase:uuid zoneID:ckks.zoneID error: &error];
                if(error != nil) {
                    ckkserror("ckksresync", ckks, "Couldn't read an item from the database, but it used to be there: %@ %@", uuid, error);
                    self.error = error;
                    continue;
                }
                if(!ckme) {
                    ckkserror("ckksresync", ckks, "Couldn't read ckme(%@) from database; continuing", uuid);
                    continue;
                }

                ckkserror("ckksresync", ckks, "BUG: Local item %@ not found in CloudKit, deleting", uuid);
                [ckks _onqueueCKRecordDeleted:ckme.item.storedCKRecord.recordID recordType:ckme.item.storedCKRecord.recordType resync:self.resync];
            }
        }
    }
}

- (void)groupStart {
    __weak __typeof(self) weakSelf = self;

    CKKSKeychainView* ckks = self.ckks;
    if(!ckks) {
        ckkserror("ckksresync", ckks, "no CKKS object");
        return;
    }

    [ckks dispatchSync: ^bool{
        ckks.lastRecordZoneChangesOperation = self;

        NSError* error = nil;
        NSQualityOfService qos = NSQualityOfServiceUtility;

        CKFetchRecordZoneChangesOptions* options = [[CKFetchRecordZoneChangesOptions alloc] init];
        if(self.resync) {
            ckksnotice("ckksresync", ckks, "Beginning resync fetch!");

            options.previousServerChangeToken = nil;

            // currently, resyncs are user initiated (or the key hierarchy is upset, which is implicitly user initiated)
            qos = NSQualityOfServiceUserInitiated;
        } else {
            // This is the normal case: fetch only the delta since the last fetch
            CKKSZoneStateEntry* ckse = [CKKSZoneStateEntry state: ckks.zoneName];
            if(error || !ckse) {
                ckkserror("ckksfetch", ckks, "couldn't fetch zone status for %@: %@", ckks.zoneName, error);
                self.error = error;
                return false;
            }

            // If this is the first sync, or an API fetch, use QoS userInitated
            if(ckse.changeToken == nil || [self.fetchReasons containsObject:CKKSFetchBecauseAPIFetchRequest]) {
                qos = NSQualityOfServiceUserInitiated;
            }

            options.previousServerChangeToken = ckse.changeToken;
        }

        ckksnotice("ckksfetch", ckks, "Beginning fetch(%@) starting at change token %@ with QoS %d", ckks.zoneName, options.previousServerChangeToken, (int)qos);

        self.fetchRecordZoneChangesOperation = [[ckks.fetchRecordZoneChangesOperationClass alloc] initWithRecordZoneIDs: @[ckks.zoneID] optionsByRecordZoneID:@{ckks.zoneID: options}];

        self.fetchRecordZoneChangesOperation.fetchAllChanges = YES;
        self.fetchRecordZoneChangesOperation.qualityOfService = qos;
        self.fetchRecordZoneChangesOperation.group = self.ckoperationGroup;
        ckksnotice("ckksfetch", ckks, "Operation group is %@", self.ckoperationGroup);

        self.fetchRecordZoneChangesOperation.recordChangedBlock = ^(CKRecord *record) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf) {
                ckkserror("ckksfetch", strongCKKS, "received callback for released object");
                return;
            }

            ckksinfo("ckksfetch", strongCKKS, "CloudKit notification: record changed(%@): %@", [record recordType], record);

            // Add this to the modifications, and remove it from the deletions
            [strongSelf.modifications setObject: record forKey: record.recordID];
            [strongSelf.deletions removeObjectForKey: record.recordID];
            strongSelf.fetchedItems++;
        };

        self.fetchRecordZoneChangesOperation.recordWithIDWasDeletedBlock = ^(CKRecordID *recordID, NSString *recordType) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf) {
                ckkserror("ckksfetch", strongCKKS, "received callback for released object");
                return;
            }

            ckksinfo("ckksfetch", strongCKKS, "CloudKit notification: deleted record(%@): %@", recordType, recordID);

            // Add to the deletions, and remove any pending modifications
            [strongSelf.modifications removeObjectForKey: recordID];
            [strongSelf.deletions setObject: recordType forKey: recordID];
            strongSelf.fetchedItems++;
        };

        // This class only supports fetching from a single zone, so we can ignore recordZoneID
        self.fetchRecordZoneChangesOperation.recordZoneChangeTokensUpdatedBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf) {
                ckkserror("ckksfetch", strongCKKS, "received callback for released object");
                return;
            }

            ckksinfo("ckksfetch", strongCKKS, "Received a new server change token: %@ %@", serverChangeToken, clientChangeTokenData);
            strongSelf.serverChangeToken = serverChangeToken;
        };

        // Completion blocks don't count for dependencies. Use this intermediate operation hack instead.
        NSBlockOperation* recordZoneChangesCompletedOperation = [[NSBlockOperation alloc] init];
        recordZoneChangesCompletedOperation.name = @"record-zone-changes-completed";

        self.fetchRecordZoneChangesOperation.recordZoneFetchCompletionBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData, BOOL moreComing, NSError * recordZoneError) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) blockCKKS = strongSelf.ckks;

            if(!strongSelf) {
                ckkserror("ckksfetch", blockCKKS, "received callback for released object");
                return;
            }

            if(!blockCKKS) {
                ckkserror("ckksfetch", blockCKKS, "no CKKS object");
                return;
            }

            ckksnotice("ckksfetch", blockCKKS, "Record zone fetch complete: changeToken=%@ clientChangeTokenData=%@ changed=%lu deleted=%lu error=%@", serverChangeToken, clientChangeTokenData,
                (unsigned long)strongSelf.modifications.count,
                (unsigned long)strongSelf.deletions.count,
                recordZoneError);

            // Completion! Mark these down.
            if(recordZoneError) {
                strongSelf.error = recordZoneError;
            }
            strongSelf.serverChangeToken = serverChangeToken;

            if(recordZoneError != nil) {
                // An error occurred. All our fetches are useless. Skip to the end.
            } else {
                // Commit these changes!
                __block NSError* error = nil;

                NSMutableDictionary<NSString*, NSMutableArray*>* changedRecordsDict = [[NSMutableDictionary alloc] init];

                [blockCKKS dispatchSyncWithAccountKeys:^bool{
                    // let's process records in a specific order by type
                    // 1. Manifest leaf records, without which the manifest master records are meaningless
                    // 2. Manifest master records, which will be used to validate incoming items
                    // 3. Intermediate key records
                    // 4. Current key records
                    // 5. Item records

                    [strongSelf.modifications enumerateKeysAndObjectsUsingBlock:^(CKRecordID* _Nonnull recordID, CKRecord* _Nonnull record, BOOL* stop) {
                        ckksinfo("ckksfetch", blockCKKS, "Sorting record modification %@: %@", recordID, record);
                        NSMutableArray* changedRecordsByType = changedRecordsDict[record.recordType];
                        if(!changedRecordsByType) {
                            changedRecordsByType = [[NSMutableArray alloc] init];
                            changedRecordsDict[record.recordType] = changedRecordsByType;
                        };

                        [changedRecordsByType addObject:record];
                    }];

                    if ([CKKSManifest shouldSyncManifests]) {
                        if (!strongSelf.resync) {
                            [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordManifestLeafType]];
                            [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordManifestType]];
                        }

                        [strongSelf _updateLatestTrustedManifest];
                    }

                    [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordIntermediateKeyType]];
                    [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordCurrentKeyType]];
                    [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordItemType]];
                    [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordCurrentItemType]];
                    [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordDeviceStateType]];
                    [strongSelf _onqueueRecordsChanged:changedRecordsDict[SecCKRecordTLKShareType]];

                    [strongSelf _onqueueProcessRecordDeletions];
                    [strongSelf _onqueueScanForExtraneousLocalItems];

                    CKKSZoneStateEntry* state = [CKKSZoneStateEntry state: blockCKKS.zoneName];
                    state.lastFetchTime = [NSDate date]; // The last fetch happened right now!
                    if(strongSelf.serverChangeToken) {
                        ckksdebug("ckksfetch", blockCKKS, "Zone change fetch complete: saving new server change token: %@", strongSelf.serverChangeToken);
                        state.changeToken = strongSelf.serverChangeToken;
                    }
                    [state saveToDatabase:&error];
                    if(error) {
                        ckkserror("ckksfetch", blockCKKS, "Couldn't save new server change token: %@", error);
                        strongSelf.error = error;
                    }

                    if(error) {
                        ckkserror("ckksfetch", blockCKKS, "horrible error occurred: %@", error);
                        strongSelf.error = error;
                        return false;
                    }

                    ckksnotice("ckksfetch", blockCKKS, "Finished processing fetch for %@", recordZoneID);

                    return true;
                }];
            }
        };

        // Called with overall operation success. As I understand it, this block will be called for every operation.
        // In the case of, e.g., network failure, the recordZoneFetchCompletionBlock will not be called, but this one will.
        self.fetchRecordZoneChangesOperation.fetchRecordZoneChangesCompletionBlock = ^(NSError * _Nullable operationError) {
            __strong __typeof(weakSelf) strongSelf = weakSelf;
            __strong __typeof(strongSelf.ckks) strongCKKS = strongSelf.ckks;
            if(!strongSelf) {
                ckkserror("ckksfetch", strongCKKS, "received callback for released object");
                return;
            }

            ckksnotice("ckksfetch", strongCKKS, "Record zone changes fetch complete: error=%@", operationError);
            if(operationError) {
                strongSelf.error = operationError;
            }

            [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventFetchAllChanges zone:ckks.zoneName count:strongSelf.fetchedItems];


            // Trigger the fake 'we're done' operation.
            [strongSelf runBeforeGroupFinished: recordZoneChangesCompletedOperation];
        };

        [self dependOnBeforeGroupFinished: recordZoneChangesCompletedOperation];
        [self dependOnBeforeGroupFinished: self.fetchRecordZoneChangesOperation];
        [ckks.database addOperation: self.fetchRecordZoneChangesOperation];
        return true;
    }];
}

- (void)cancel {
    [self.fetchRecordZoneChangesOperation cancel];
    [super cancel];
}

@end

#endif
