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
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/analytics/SecEventMetric.h"
#import "keychain/analytics/SecMetrics.h"
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

@property bool moreComing;

// Holds the original change token that the client believes they have synced to
@property NSMutableDictionary<CKRecordZoneID*, CKServerChangeToken*>* originalChangeTokens;

@property CKKSResultOperation* fetchCompletedOperation;
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
        _changeTokens = [[NSMutableDictionary alloc] init];
        _originalChangeTokens = [[NSMutableDictionary alloc] init];

        _fetchCompletedOperation = [CKKSResultOperation named:@"record-zone-changes-completed" withBlock:^{}];

        _moreComing = false;
    }
    return self;
}

- (void)queryClientsForChangeTokens
{
    // Ask all our clients for their change tags

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
                if (self.changeTokens[clientZoneID]) {
                    options.previousServerChangeToken = self.changeTokens[clientZoneID];
                    secnotice("ckksfetch", "Using cached change token for %@: %@", clientZoneID, self.changeTokens[clientZoneID]);
                } else {
                    options.previousServerChangeToken = clientPreference.changeToken;
                }

                self.originalChangeTokens[clientZoneID] = options.previousServerChangeToken;
            }

            //if(options.previousServerChangeToken == nil) {
            //    nilChangeTag = true;
            //}

            self.allClientOptions[client.zoneID] = options;
        }
    }
}

- (void)groupStart {
    self.allClientOptions = [NSMutableDictionary dictionary];
    self.fetchedZoneIDs = [NSMutableArray array];

    [self queryClientsForChangeTokens];

    if(self.fetchedZoneIDs.count == 0) {
        // No clients actually want to fetch right now, so quit
        self.error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSNoFetchesRequested description:@"No clients want a fetch right now"];
        secnotice("ckksfetch", "Cancelling fetch: %@", self.error);
        return;
    }

    [self performFetch];
}

- (void)performFetch
{
    WEAKIFY(self);

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

    secnotice("ckksfetch", "Beginning fetch with discretionary network (%d): %@", (int)networkBehavior, self.allClientOptions);
    self.fetchRecordZoneChangesOperation = [[self.fetchRecordZoneChangesOperationClass alloc] initWithRecordZoneIDs:self.fetchedZoneIDs
                                                                                              configurationsByRecordZoneID:self.allClientOptions];

    self.fetchRecordZoneChangesOperation.fetchAllChanges = NO;
    self.fetchRecordZoneChangesOperation.configuration.discretionaryNetworkBehavior = networkBehavior;
    self.fetchRecordZoneChangesOperation.configuration.isCloudKitSupportOperation = YES;
    self.fetchRecordZoneChangesOperation.group = self.ckoperationGroup;
    secnotice("ckksfetch", "Operation group is %@", self.ckoperationGroup);

    self.fetchRecordZoneChangesOperation.recordChangedBlock = ^(CKRecord *record) {
        STRONGIFY(self);
        secinfo("ckksfetch", "CloudKit notification: record changed(%@): %@", [record recordType], record);

        // Add this to the modifications, and remove it from the deletions
        self.modifications[record.recordID] = record;
        [self.deletions removeObjectForKey:record.recordID];
        self.fetchedItems++;
    };

    self.fetchRecordZoneChangesOperation.recordWithIDWasDeletedBlock = ^(CKRecordID *recordID, NSString *recordType) {
        STRONGIFY(self);
        secinfo("ckksfetch", "CloudKit notification: deleted record(%@): %@", recordType, recordID);

        // Add to the deletions, and remove any pending modifications
        [self.modifications removeObjectForKey: recordID];
        self.deletions[recordID] = [[CKKSCloudKitDeletion alloc] initWithRecordID:recordID recordType:recordType];
        self.fetchedItems++;
    };

    self.fetchRecordZoneChangesOperation.recordZoneChangeTokensUpdatedBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData) {
        STRONGIFY(self);

        secinfo("ckksfetch", "Received a new server change token (via block) for %@: %@ %@", recordZoneID, serverChangeToken, clientChangeTokenData);
        self.changeTokens[recordZoneID] = serverChangeToken;
    };

    self.fetchRecordZoneChangesOperation.recordZoneFetchCompletionBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData, BOOL moreComing, NSError * recordZoneError) {
        STRONGIFY(self);

        secnotice("ckksfetch", "Received a new server change token for %@: %@ %@", recordZoneID, serverChangeToken, clientChangeTokenData);
        self.changeTokens[recordZoneID] = serverChangeToken;
        self.allClientOptions[recordZoneID].previousServerChangeToken = serverChangeToken;

        self.moreComing |= moreComing;
        if(moreComing) {
            secnotice("ckksfetch", "more changes pending for %@, will start a new fetch at change token %@", recordZoneID, self.changeTokens[recordZoneID]);
        }

        ckksnotice("ckksfetch", recordZoneID, "Record zone fetch complete: changeToken=%@ clientChangeTokenData=%@ moreComing=%@ error=%@", serverChangeToken, clientChangeTokenData,
                   moreComing ? @"YES" : @"NO",
                   recordZoneError);
    };

    // Called with overall operation success. As I understand it, this block will be called for every operation.
    // In the case of, e.g., network failure, the recordZoneFetchCompletionBlock will not be called, but this one will.
    self.fetchRecordZoneChangesOperation.fetchRecordZoneChangesCompletionBlock = ^(NSError * _Nullable operationError) {
        STRONGIFY(self);
        if(!self) {
            secerror("ckksfetch: received callback for released object");
            return;
        }

        // If we were told that there were moreChanges coming for any zone, we'd like to fetch again.
        //  This is true if we recieve no error or a network timeout. Any other error should cause a failure.
        if(self.moreComing && (operationError == nil || [CKKSReachabilityTracker isNetworkFailureError:operationError])) {
            secnotice("ckksfetch", "Must issue another fetch (with potential error %@)", operationError);
            self.moreComing = false;
            [self performFetch];
            return;
        }

        if(operationError) {
            self.error = operationError;
        } else {
            secnotice("ckksfetch", "Advising clients of fetched changes");
            [self sendAllChangesToClients];
        }

        secnotice("ckksfetch", "Record zone changes fetch complete: error=%@", operationError);

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventFetchAllChanges
                                      count:self.fetchedItems];

        // Count record changes per zone
        NSMutableDictionary<CKRecordZoneID*,NSNumber*>* recordChangesPerZone = [NSMutableDictionary dictionary];
        NSNumber* totalModifications = [NSNumber numberWithUnsignedLong:self.modifications.count];
        NSNumber* totalDeletions = [NSNumber numberWithUnsignedLong:self.deletions.count];

        for(CKRecordID* recordID in self.modifications) {
            NSNumber* last = recordChangesPerZone[recordID.zoneID];
            recordChangesPerZone[recordID.zoneID] = [NSNumber numberWithUnsignedLong:1+(last ? [last unsignedLongValue] : 0)];
        }
        for(CKRecordID* recordID in self.deletions) {
            NSNumber* last = recordChangesPerZone[recordID.zoneID];
            recordChangesPerZone[recordID.zoneID] = [NSNumber numberWithUnsignedLong:1+(last ? [last unsignedLongValue] : 0)];
        }

        for(CKRecordZoneNotification* rz in self.apnsPushes) {
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

                SecEventMetric *metric2 = [[SecEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric2[@"push_token_uuid"] = rz.ckksPushTracingUUID;
                metric2[@"push_received_date"] = rz.ckksPushReceivedDate;
                metric2[@"push_event_name"] = @"CKKS Push-webtunnel";

                metric2[@"fetch_error"] = operationError;

                metric2[@"total_modifications"] = totalModifications;
                metric2[@"total_deletions"] = totalDeletions;
                for(CKRecordZoneID* zoneID in recordChangesPerZone) {
                    metric2[zoneID.zoneName] = recordChangesPerZone[zoneID];
                }

                // Okay, we now have this metric. But, it's unclear if calling associateWithCompletedOperation in this block will work. So, do something silly with operation scheduling.
                // Grab pointers to these things
                CKContainer* container = self.container;
                CKDatabaseOperation<CKKSFetchRecordZoneChangesOperation>* rzcOperation = self.fetchRecordZoneChangesOperation;

                CKKSResultOperation* launchMetricOp = [CKKSResultOperation named:@"submit-metric" withBlock:^{
                    if(![metric associateWithCompletedOperation:rzcOperation]) {
                        secerror("ckksfetch: Couldn't associate metric with operation: %@ %@", metric, rzcOperation);
                    }
                    [container submitEventMetric:metric];
                    [[SecMetrics managerObject] submitEvent:metric2];
                    secnotice("ckksfetch", "Metric submitted: %@", metric);
                }];
                [launchMetricOp addSuccessDependency:self.fetchCompletedOperation];

                [self.operationQueue addOperation:launchMetricOp];
            }
        }

        // Don't need these any more; save some memory
        [self.modifications removeAllObjects];
        [self.deletions removeAllObjects];

        // Trigger the fake 'we're done' operation.
        [self runBeforeGroupFinished: self.fetchCompletedOperation];
    };

    [self dependOnBeforeGroupFinished:self.fetchCompletedOperation];
    [self dependOnBeforeGroupFinished:self.fetchRecordZoneChangesOperation];
    [self.container.privateCloudDatabase addOperation:self.fetchRecordZoneChangesOperation];
}

- (void)sendAllChangesToClients
{
    for(CKRecordZoneID* clientZoneID in self.clientMap) {
        [self sendChangesToClient:clientZoneID];
    }
}

- (void)sendChangesToClient:(CKRecordZoneID*)recordZoneID
{
    id<CKKSChangeFetcherClient> client = self.clientMap[recordZoneID];
    if(!client) {
        secerror("ckksfetch: no client registered for %@, so why did we get any data?", recordZoneID);
        return;
    }

    // First, filter the modifications and deletions for this zone
    NSMutableArray<CKRecord*>* zoneModifications = [NSMutableArray array];
    NSMutableArray<CKKSCloudKitDeletion*>* zoneDeletions = [NSMutableArray array];

    [self.modifications enumerateKeysAndObjectsUsingBlock:^(CKRecordID* _Nonnull recordID,
                                                            CKRecord* _Nonnull record,
                                                            BOOL* stop) {
        if([recordID.zoneID isEqual:recordZoneID]) {
            ckksinfo("ckksfetch", recordZoneID, "Sorting record modification %@: %@", recordID, record);
            [zoneModifications addObject:record];
        }
    }];

    [self.deletions enumerateKeysAndObjectsUsingBlock:^(CKRecordID* _Nonnull recordID,
                                                        CKKSCloudKitDeletion* _Nonnull deletion,
                                                        BOOL* _Nonnull stop) {
        if([recordID.zoneID isEqual:recordZoneID]) {
            ckksinfo("ckksfetch", recordZoneID, "Sorting record deletion %@: %@", recordID, deletion);
            [zoneDeletions addObject:deletion];
        }
    }];

    ckksnotice("ckksfetch", recordZoneID, "Delivering fetched changes: changed=%lu deleted=%lu",
               (unsigned long)zoneModifications.count, (unsigned long)zoneDeletions.count);

    // Tell the client about these changes!
    [client changesFetched:zoneModifications
          deletedRecordIDs:zoneDeletions
            oldChangeToken:self.originalChangeTokens[recordZoneID]
            newChangeToken:self.changeTokens[recordZoneID]];
}

- (void)cancel {
    [self.fetchRecordZoneChangesOperation cancel];
    [super cancel];
}

@end

#endif
