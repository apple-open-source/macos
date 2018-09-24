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

#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSMirrorEntry.h"
#import "keychain/ckks/CKKSManifest.h"
#import "keychain/ckks/CKKSManifestLeafRecord.h"
#import "NSError+UsefulConstructors.h"
#import "CKKSPowerCollection.h"
#include <securityd/SecItemServer.h>

@implementation CKKSCloudKitFetchRequest
@end

@implementation CKKSCloudKitDeletion
- (instancetype)initWithRecordID:(CKRecordID*)recordID recordType:(NSString*)recordType
{
    if((self = [super init])) {
        _recordID = recordID;
        _recordType = recordType;
    }
    return self;
}
@end


@interface CKKSFetchAllRecordZoneChangesOperation()
@property CKDatabaseOperation<CKKSFetchRecordZoneChangesOperation>* fetchRecordZoneChangesOperation;
@property NSMutableDictionary<CKRecordZoneID*, CKFetchRecordZoneChangesConfiguration*>* allClientOptions;

@property CKOperationGroup* ckoperationGroup;
@property (assign) NSUInteger fetchedItems;
@property bool forceResync;
@end

@implementation CKKSFetchAllRecordZoneChangesOperation

// Sets up an operation to fetch all changes from the server, and collect them until we know if the fetch completes.
// As a bonus, you can depend on this operation without worry about NSOperation completion block dependency issues.

- (instancetype)init {
    return nil;
}

- (instancetype)initWithContainer:(CKContainer*)container
                       fetchClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
                          clients:(NSArray<id<CKKSChangeFetcherClient>>*)clients
                     fetchReasons:(NSSet<CKKSFetchBecause*>*)fetchReasons
                       apnsPushes:(NSSet<CKRecordZoneNotification*>*)apnsPushes
                      forceResync:(bool)forceResync
                 ckoperationGroup:(CKOperationGroup*)ckoperationGroup
{
    if(self = [super init]) {
        _container = container;
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;

        NSMutableDictionary* clientMap = [NSMutableDictionary dictionary];
        for(id<CKKSChangeFetcherClient> client in clients) {
            clientMap[client.zoneID] = client;
        }
        _clientMap = [clientMap copy];

        _ckoperationGroup = ckoperationGroup;
        _forceResync = forceResync;
        _fetchReasons = fetchReasons;
        _apnsPushes = apnsPushes;

        _modifications = [[NSMutableDictionary alloc] init];
        _deletions = [[NSMutableDictionary alloc] init];
    }
    return self;
}

- (void)groupStart {
    __weak __typeof(self) weakSelf = self;

    // Ask all our clients for their change tags
    self.allClientOptions = [NSMutableDictionary dictionary];
    self.fetchedZoneIDs = [NSMutableArray array];

    // Unused until [<rdar://problem/38725728> Changes to discretionary-ness (explicit or derived from QoS) should be "live"] has happened and we can determine network
    // discretionaryness.
    //bool nilChangeTag = false;

    for(CKRecordZoneID* clientZoneID in self.clientMap) {
        id<CKKSChangeFetcherClient> client = self.clientMap[clientZoneID];

        CKKSCloudKitFetchRequest* clientPreference = [client participateInFetch];
        if(clientPreference.participateInFetch) {
            [self.fetchedZoneIDs addObject:client.zoneID];

            CKFetchRecordZoneChangesConfiguration* options = [[CKFetchRecordZoneChangesConfiguration alloc] init];

            if(!self.forceResync) {
                options.previousServerChangeToken = clientPreference.changeToken;
            }

            //if(options.previousServerChangeToken == nil) {
            //    nilChangeTag = true;
            //}

            self.allClientOptions[client.zoneID] = options;
        }
    }

    if(self.fetchedZoneIDs.count == 0) {
        // No clients actually want to fetch right now, so quit
        self.error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoFetchesRequested description:@"No clients want a fetch right now"];
        secnotice("ckksfetch", "Cancelling fetch: %@", self.error);
        return;
    }

    // Compute the network discretionary approach this fetch will take.
    // For now, everything is nondiscretionary, because we can't afford to block a nondiscretionary request later.
    // Once [<rdar://problem/38725728> Changes to discretionary-ness (explicit or derived from QoS) should be "live"] happens, we can come back through and make things
    // discretionary, but boost them later.
    //
    // Rules:
    //  If there's a nil change tag, go to nondiscretionary. This is likely a zone bringup (which happens during iCloud sign-in) or a resync (which happens due to user input)
    //  If the fetch reasons include an API fetch, an initial start or a key hierarchy fetch, become nondiscretionary as well.

    CKOperationDiscretionaryNetworkBehavior networkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    //if(nilChangeTag ||
    //   [self.fetchReasons containsObject:CKKSFetchBecauseAPIFetchRequest] ||
    //   [self.fetchReasons containsObject:CKKSFetchBecauseInitialStart] ||
    //   [self.fetchReasons containsObject:CKKSFetchBecauseKeyHierarchy]) {
    //    networkBehavior = CKOperationDiscretionaryNetworkBehaviorNonDiscretionary;
    //}

    secnotice("ckks", "Beginning fetch with discretionary network (%d): %@", (int)networkBehavior, self.allClientOptions);
    self.fetchRecordZoneChangesOperation = [[self.fetchRecordZoneChangesOperationClass alloc] initWithRecordZoneIDs:self.fetchedZoneIDs
                                                                                              configurationsByRecordZoneID:self.allClientOptions];

    self.fetchRecordZoneChangesOperation.fetchAllChanges = YES;
    self.fetchRecordZoneChangesOperation.configuration.discretionaryNetworkBehavior = networkBehavior;
    self.fetchRecordZoneChangesOperation.group = self.ckoperationGroup;
    secnotice("ckksfetch", "Operation group is %@", self.ckoperationGroup);

    self.fetchRecordZoneChangesOperation.recordChangedBlock = ^(CKRecord *record) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        secinfo("ckksfetch", "CloudKit notification: record changed(%@): %@", [record recordType], record);

        // Add this to the modifications, and remove it from the deletions
        strongSelf.modifications[record.recordID] = record;
        [strongSelf.deletions removeObjectForKey:record.recordID];
        strongSelf.fetchedItems++;
    };

    self.fetchRecordZoneChangesOperation.recordWithIDWasDeletedBlock = ^(CKRecordID *recordID, NSString *recordType) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        secinfo("ckksfetch", "CloudKit notification: deleted record(%@): %@", recordType, recordID);

        // Add to the deletions, and remove any pending modifications
        [strongSelf.modifications removeObjectForKey: recordID];
        strongSelf.deletions[recordID] = [[CKKSCloudKitDeletion alloc] initWithRecordID:recordID recordType:recordType];
        strongSelf.fetchedItems++;
    };

    self.fetchRecordZoneChangesOperation.recordZoneChangeTokensUpdatedBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;

        secinfo("ckksfetch", "Received a new server change token for %@: %@ %@", recordZoneID, serverChangeToken, clientChangeTokenData);
        strongSelf.changeTokens[recordZoneID] = serverChangeToken;
    };

    self.fetchRecordZoneChangesOperation.recordZoneFetchCompletionBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData, BOOL moreComing, NSError * recordZoneError) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            secerror("ckksfetch: received callback for released object");
            return;
        }

        id<CKKSChangeFetcherClient> client = strongSelf.clientMap[recordZoneID];
        if(!client) {
            secerror("ckksfetch: no client registered for %@, so why did we get any data?", recordZoneID);
            return;
        }

        // First, filter the modifications and deletions for this zone
        NSMutableArray<CKRecord*>* zoneModifications = [NSMutableArray array];
        NSMutableArray<CKKSCloudKitDeletion*>* zoneDeletions = [NSMutableArray array];

        [strongSelf.modifications enumerateKeysAndObjectsUsingBlock:^(CKRecordID* _Nonnull recordID,
                                                                      CKRecord* _Nonnull record,
                                                                      BOOL* stop) {
            if([recordID.zoneID isEqual:recordZoneID]) {
                 ckksinfo("ckksfetch", recordZoneID, "Sorting record modification %@: %@", recordID, record);
                [zoneModifications addObject:record];
            }
        }];

        [strongSelf.deletions enumerateKeysAndObjectsUsingBlock:^(CKRecordID* _Nonnull recordID,
                                                                  CKKSCloudKitDeletion* _Nonnull deletion,
                                                                  BOOL* _Nonnull stop) {
            if([recordID.zoneID isEqual:recordZoneID]) {
                ckksinfo("ckksfetch", recordZoneID, "Sorting record deletion %@: %@", recordID, deletion);
                [zoneDeletions addObject:deletion];
            }
        }];

        ckksnotice("ckksfetch", recordZoneID, "Record zone fetch complete: changeToken=%@ clientChangeTokenData=%@ changed=%lu deleted=%lu error=%@", serverChangeToken, clientChangeTokenData,
            (unsigned long)zoneModifications.count,
            (unsigned long)zoneDeletions.count,
            recordZoneError);

        if(recordZoneError == nil) {
            // Tell the client about these changes!
            [client changesFetched:zoneModifications
                  deletedRecordIDs:zoneDeletions
                    oldChangeToken:strongSelf.allClientOptions[recordZoneID].previousServerChangeToken
                    newChangeToken:serverChangeToken];
            ckksnotice("ckksfetch", recordZoneID, "Finished processing fetch");
        }
    };

    // Completion blocks don't count for dependencies. Use this intermediate operation hack instead.
    CKKSResultOperation* recordZoneChangesCompletedOperation = [CKKSResultOperation named:@"record-zone-changes-completed" withBlock:^{}];

    // Called with overall operation success. As I understand it, this block will be called for every operation.
    // In the case of, e.g., network failure, the recordZoneFetchCompletionBlock will not be called, but this one will.
    self.fetchRecordZoneChangesOperation.fetchRecordZoneChangesCompletionBlock = ^(NSError * _Nullable operationError) {
        __strong __typeof(weakSelf) strongSelf = weakSelf;
        if(!strongSelf) {
            secerror("ckksfetch: received callback for released object");
            return;
        }

        secnotice("ckksfetch", "Record zone changes fetch complete: error=%@", operationError);
        if(operationError) {
            strongSelf.error = operationError;
        }

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventFetchAllChanges
                                      count:strongSelf.fetchedItems];

        // Count record changes per zone
        NSMutableDictionary<CKRecordZoneID*,NSNumber*>* recordChangesPerZone = [NSMutableDictionary dictionary];
        NSNumber* totalModifications = [NSNumber numberWithUnsignedLong:strongSelf.modifications.count];
        NSNumber* totalDeletions = [NSNumber numberWithUnsignedLong:strongSelf.deletions.count];

        for(CKRecordID* recordID in strongSelf.modifications) {
            NSNumber* last = recordChangesPerZone[recordID.zoneID];
            recordChangesPerZone[recordID.zoneID] = [NSNumber numberWithUnsignedLong:1+(last ? [last unsignedLongValue] : 0)];
        }
        for(CKRecordID* recordID in strongSelf.deletions) {
            NSNumber* last = recordChangesPerZone[recordID.zoneID];
            recordChangesPerZone[recordID.zoneID] = [NSNumber numberWithUnsignedLong:1+(last ? [last unsignedLongValue] : 0)];
        }

        for(CKRecordZoneNotification* rz in strongSelf.apnsPushes) {
            if(rz.ckksPushTracingEnabled) {
                secnotice("ckksfetch", "Submitting post-fetch CKEventMetric due to notification %@", rz);

                // Schedule submitting this metric on another operation, so hopefully CK will have marked this fetch as done by the time that fires?
                CKEventMetric *metric = [[CKEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric.isPushTriggerFired = true;
                metric[@"push_token_uuid"] = rz.ckksPushTracingUUID;
                metric[@"push_received_date"] = rz.ckksPushReceivedDate;
                metric[@"push_event_name"] = @"CKKS Push";

                metric[@"fetch_error"] = operationError ? @1 : @0;
                metric[@"fetch_error_domain"] = operationError.domain;
                metric[@"fetch_error_code"] = [NSNumber numberWithLong:operationError.code];

                metric[@"total_modifications"] = totalModifications;
                metric[@"total_deletions"] = totalDeletions;
                for(CKRecordZoneID* zoneID in recordChangesPerZone) {
                    metric[zoneID.zoneName] = recordChangesPerZone[zoneID];
                }

                // Okay, we now have this metric. But, it's unclear if calling associateWithCompletedOperation in this block will work. So, do something silly with operation scheduling.
                // Grab pointers to these things
                CKContainer* container = strongSelf.container;
                CKDatabaseOperation<CKKSFetchRecordZoneChangesOperation>* rzcOperation = strongSelf.fetchRecordZoneChangesOperation;

                CKKSResultOperation* launchMetricOp = [CKKSResultOperation named:@"submit-metric" withBlock:^{
                    if(![metric associateWithCompletedOperation:rzcOperation]) {
                        secerror("ckksfetch: Couldn't associate metric with operation: %@ %@", metric, rzcOperation);
                    }
                    [container submitEventMetric:metric];
                    secnotice("ckksfetch", "Metric submitted: %@", metric);
                }];
                [launchMetricOp addSuccessDependency:recordZoneChangesCompletedOperation];

                [strongSelf.operationQueue addOperation:launchMetricOp];
            }
        }

        // Don't need these any more; save some memory
        [strongSelf.modifications removeAllObjects];
        [strongSelf.deletions removeAllObjects];

        // Trigger the fake 'we're done' operation.
        [strongSelf runBeforeGroupFinished: recordZoneChangesCompletedOperation];
    };

    [self dependOnBeforeGroupFinished:recordZoneChangesCompletedOperation];
    [self dependOnBeforeGroupFinished:self.fetchRecordZoneChangesOperation];

    [self.container.privateCloudDatabase addOperation:self.fetchRecordZoneChangesOperation];
}

- (void)cancel {
    [self.fetchRecordZoneChangesOperation cancel];
    [super cancel];
}

@end

#endif
