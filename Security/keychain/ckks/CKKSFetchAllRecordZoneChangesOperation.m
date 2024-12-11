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
#import <os/feature_private.h>

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
#include "keychain/securityd/SecItemServer.h"

#import <KeychainCircle/SecurityAnalyticsConstants.h>
#import <KeychainCircle/SecurityAnalyticsReporterRTC.h>
#import <KeychainCircle/AAFAnalyticsEvent+Security.h>

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

@property NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clientMap;

@property CKOperationGroup* ckoperationGroup;
@property (assign) NSUInteger fetchedItems;
@property bool forceResync;

@property bool moreComing;

// Used for RTC Reporting purposes
@property NSString* altDSID;
@property bool sendMetric;

@property size_t totalModifications;
@property size_t totalDeletions;

// A zoneID is in this set if we're attempting to resync them
@property NSMutableSet<CKRecordZoneID*>* resyncingZones;

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
                        clientMap:(NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>*)clientMap
                     fetchReasons:(NSSet<CKKSFetchBecause*>*)fetchReasons
                       apnsPushes:(NSSet<CKRecordZoneNotification*>*)apnsPushes
                      forceResync:(bool)forceResync
                 ckoperationGroup:(CKOperationGroup*)ckoperationGroup
                          altDSID:(NSString*)altDSID
                       sendMetric:(bool)sendMetric
{
    if(self = [super init]) {
        _container = container;
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;

        _clientMap = clientMap;

        _ckoperationGroup = ckoperationGroup;
        _forceResync = forceResync;
        _fetchReasons = fetchReasons;
        _apnsPushes = apnsPushes;

        _modifications = [[NSMutableDictionary alloc] init];
        _deletions = [[NSMutableDictionary alloc] init];
        _changeTokens = [[NSMutableDictionary alloc] init];

        _resyncingZones = [NSMutableSet set];

        _totalModifications = 0;
        _totalDeletions = 0;

        _fetchCompletedOperation = [CKKSResultOperation named:@"record-zone-changes-completed" withBlock:^{}];

        _moreComing = false;
        _altDSID = altDSID;
        _sendMetric = sendMetric;

        // This operation might be needed during CKKS/Manatee bringup, which affects the user experience. We need to boost our CPU priority so CK will accept our network priority <rdar://problem/49086080>
        self.qualityOfService = NSQualityOfServiceUserInitiated;
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

        CKKSCloudKitFetchRequest* clientPreference = [client participateInFetch:clientZoneID];
        if(clientPreference.participateInFetch) {
            [self.fetchedZoneIDs addObject:clientZoneID];

            CKFetchRecordZoneChangesConfiguration* options = [[CKFetchRecordZoneChangesConfiguration alloc] init];

            if(!self.forceResync) {
                if (self.changeTokens[clientZoneID]) {
                    options.previousServerChangeToken = self.changeTokens[clientZoneID];
                    ckksnotice_global("ckksfetch", "Using cached change token for %@: %@", clientZoneID, self.changeTokens[clientZoneID]);
                } else {
                    options.previousServerChangeToken = clientPreference.changeToken;
                }
            }

            if(clientPreference.resync || self.forceResync) {
                [self.resyncingZones addObject:clientZoneID];
            }

            self.allClientOptions[clientZoneID] = options;
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
        ckksnotice_global("ckksfetch", "Cancelling fetch: %@", self.error);

        // Drop pointer to clients
        self.clientMap = @{};
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

    // Re-check in with our clients to be sure they want to be fetched.
    // Some views might decide they no longer need a fetch in order to prioritize other views.
    NSMutableArray<CKRecordZoneID*>* zoneIDsToStopFetching = [NSMutableArray array];
    for(CKRecordZoneID* zoneID in self.allClientOptions.allKeys) {
        BOOL keepZone = [self.clientMap[zoneID] zoneIsReadyForFetching:zoneID];

        if(!keepZone) {
            [zoneIDsToStopFetching addObject:zoneID];
        }
    }
    AAFAnalyticsEventSecurity* eventS = [[AAFAnalyticsEventSecurity alloc] initWithCKKSMetrics:@{kSecurityRTCFieldIsPrioritized:@NO}
                                                                                       altDSID:self.altDSID
                                                                                     eventName:kSecurityRTCEventNameZoneChangeFetch
                                                                               testsAreEnabled:SecCKKSTestsEnabled()
                                                                                      category:kSecurityRTCEventCategoryAccountDataAccessRecovery
                                                                                    sendMetric:self.sendMetric];

    if(zoneIDsToStopFetching.count > 0) {
        ckksnotice_global("ckksfetch", "Dropping the following zones from this fetch cycle: %@", zoneIDsToStopFetching);

        for(CKRecordZoneID* zoneID in zoneIDsToStopFetching) {
            [self.fetchedZoneIDs removeObject:zoneID];
            self.allClientOptions[zoneID] = nil;
        }
    }

    if(self.fetchedZoneIDs.count == 0) {
        ckksnotice_global("ckksfetch", "No zones to fetch. Skipping operation.");
        [self runBeforeGroupFinished:self.fetchCompletedOperation];
        return;
    }

    [eventS addMetrics:@{kSecurityRTCFieldNumViews:@(self.fetchedZoneIDs.count)}];

    ckksnotice_global("ckksfetch", "Beginning fetch: %@ options: %@",
                      self.fetchedZoneIDs,
                      self.allClientOptions);
    self.fetchRecordZoneChangesOperation = [[self.fetchRecordZoneChangesOperationClass alloc] initWithRecordZoneIDs:self.fetchedZoneIDs
                                                                                              configurationsByRecordZoneID:self.allClientOptions];

    self.fetchRecordZoneChangesOperation.fetchAllChanges = NO;
    self.fetchRecordZoneChangesOperation.configuration.isCloudKitSupportOperation = YES;
    self.fetchRecordZoneChangesOperation.group = self.ckoperationGroup;
    ckksnotice_global("ckksfetch", "Operation group is %@", self.ckoperationGroup);

//    @TODO: convert this to a bitfield somehow
//    event[kSecurityRTCFieldFetchReasons] = self.fetchReasons;

    if([self.fetchReasons containsObject:CKKSFetchBecauseAPIFetchRequest] ||
       [self.fetchReasons containsObject:CKKSFetchBecauseInitialStart] ||
       [self.fetchReasons containsObject:CKKSFetchBecauseMoreComing] ||
       [self.fetchReasons containsObject:CKKSFetchBecauseKeyHierarchy]) {

        // CKKSHighPriorityOperations default enabled
        // This operation might be needed during CKKS/Manatee bringup, which affects the user experience. Bump our priority to get it off-device and unblock Manatee access.
        self.fetchRecordZoneChangesOperation.qualityOfService = NSQualityOfServiceUserInitiated;
        [eventS addMetrics:@{kSecurityRTCFieldIsPrioritized:@(YES)}];
        
    }

    self.fetchRecordZoneChangesOperation.recordChangedBlock = ^(CKRecord *record) {
        STRONGIFY(self);
        ckksinfo_global("ckksfetch", "CloudKit notification: record changed(%@): %@", [record recordType], record);

        // Add this to the modifications, and remove it from the deletions
        self.modifications[record.recordID] = record;
        [self.deletions removeObjectForKey:record.recordID];
        self.fetchedItems++;
    };

    self.fetchRecordZoneChangesOperation.recordWithIDWasDeletedBlock = ^(CKRecordID *recordID, NSString *recordType) {
        STRONGIFY(self);
        ckksinfo_global("ckksfetch", "CloudKit notification: deleted record(%@): %@", recordType, recordID);

        // Add to the deletions, and remove any pending modifications
        [self.modifications removeObjectForKey: recordID];
        self.deletions[recordID] = [[CKKSCloudKitDeletion alloc] initWithRecordID:recordID recordType:recordType];
        self.fetchedItems++;
    };

    self.fetchRecordZoneChangesOperation.recordZoneChangeTokensUpdatedBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData) {
        STRONGIFY(self);

        ckksinfo_global("ckksfetch", "Received a new server change token (via block) for %@: %@ %@", recordZoneID, serverChangeToken, clientChangeTokenData);
        self.changeTokens[recordZoneID] = serverChangeToken;
    };

    self.fetchRecordZoneChangesOperation.recordZoneFetchCompletionBlock = ^(CKRecordZoneID *recordZoneID, CKServerChangeToken *serverChangeToken, NSData *clientChangeTokenData, BOOL moreComing, NSError * recordZoneError) {
        STRONGIFY(self);

        ckksinfo_global("ckksfetch", "Received a new server change token for %@: %@ %@", recordZoneID, serverChangeToken, clientChangeTokenData);
        self.changeTokens[recordZoneID] = serverChangeToken;
        self.allClientOptions[recordZoneID].previousServerChangeToken = serverChangeToken;

        self.moreComing |= moreComing;

        [eventS addMetrics:@{kSecurityRTCFieldFullFetch:@(self.moreComing)}];

        if(moreComing) {
            ckksnotice_global("ckksfetch", "more changes pending for %@, will start a new fetch at change token %@", recordZoneID, self.changeTokens[recordZoneID]);
        }

        ckksinfo("ckksfetch", recordZoneID, "Record zone fetch complete: changeToken=%@ clientChangeTokenData=%@ moreComing=%{BOOL}d error=%@", serverChangeToken, clientChangeTokenData,
                 moreComing,
                 recordZoneError);

        [self sendChangesToClient:recordZoneID moreComing:moreComing];
    };

    // Called with overall operation success. As I understand it, this block will be called for every operation.
    // In the case of, e.g., network failure, the recordZoneFetchCompletionBlock will not be called, but this one will.
    self.fetchRecordZoneChangesOperation.fetchRecordZoneChangesCompletionBlock = ^(NSError * _Nullable operationError) {
        STRONGIFY(self);
        if(!self) {
            ckkserror_global("ckksfetch", "received callback for released object");
            return;
        }

        // Count record changes per zone
        NSMutableDictionary<CKRecordZoneID*,NSNumber*>* recordChangesPerZone = [NSMutableDictionary dictionary];
        self.totalModifications += self.modifications.count;
        self.totalDeletions += self.deletions.count;

        // All of these should have been delivered by recordZoneFetchCompletionBlock; throw them away
        [self.modifications removeAllObjects];
        [self.deletions removeAllObjects];

        // If we were told that there were moreChanges coming for any zone, we'd like to fetch again.
        //  This is true if we recieve no error or a network timeout. Any other error should cause a failure.
        if(self.moreComing && (operationError == nil || [CKKSReachabilityTracker isNetworkFailureError:operationError])) {
            ckksnotice_global("ckksfetch", "Must issue another fetch (with potential error %@)", operationError);
            self.moreComing = false;
            [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:NO error:operationError];

            [self performFetch];
            return;
        }

        if(operationError) {
            self.error = operationError;
        }

        ckksnotice_global("ckksfetcher", "Record zone changes fetch complete: error=%@", operationError);

        [CKKSPowerCollection CKKSPowerEvent:kCKKSPowerEventFetchAllChanges
                                      count:self.fetchedItems];


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
                ckksnotice_global("ckksfetch", "Submitting post-fetch CKEventMetric due to notification %@", rz);

                // Schedule submitting this metric on another operation, so hopefully CK will have marked this fetch as done by the time that fires?
                CKEventMetric *metric = [[CKEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric.isPushTriggerFired = true;
                metric[@"push_token_uuid"] = rz.ckksPushTracingUUID;
                metric[@"push_received_date"] = rz.ckksPushReceivedDate;
                metric[@"push_event_name"] = @"CKKS Push";

                metric[@"fetch_error"] = operationError ? @1 : @0;
                metric[@"fetch_error_domain"] = operationError.domain;
                metric[@"fetch_error_code"] = [NSNumber numberWithLong:operationError.code];

                metric[@"total_modifications"] = @(self.totalModifications);
                metric[@"total_deletions"] = @(self.totalDeletions);
                for(CKRecordZoneID* zoneID in recordChangesPerZone) {
                    metric[zoneID.zoneName] = recordChangesPerZone[zoneID];
                }

                SecEventMetric *metric2 = [[SecEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric2[@"push_token_uuid"] = rz.ckksPushTracingUUID;
                metric2[@"push_received_date"] = rz.ckksPushReceivedDate;
                metric2[@"push_event_name"] = @"CKKS Push-webtunnel";

                metric2[@"fetch_error"] = operationError;

                metric2[@"total_modifications"] = @(self.totalModifications);
                metric2[@"total_deletions"] = @(self.totalDeletions);
                for(CKRecordZoneID* zoneID in recordChangesPerZone) {
                    metric2[zoneID.zoneName] = recordChangesPerZone[zoneID];
                }

                // Okay, we now have this metric. But, it's unclear if calling associateWithCompletedOperation in this block will work. So, do something silly with operation scheduling.
                // Grab pointers to these things
                CKContainer* container = self.container;
                CKDatabaseOperation<CKKSFetchRecordZoneChangesOperation>* rzcOperation = self.fetchRecordZoneChangesOperation;

                CKKSResultOperation* launchMetricOp = [CKKSResultOperation named:@"submit-metric" withBlock:^{
                    if(![metric associateWithCompletedOperation:rzcOperation]) {
                        ckkserror_global("ckksfetch", "Couldn't associate metric with operation: %@ %@", metric, rzcOperation);
                    }
                    [container submitEventMetric:metric];
                    [[SecMetrics managerObject] submitEvent:metric2];
                    ckksnotice_global("ckksfetch", "Metric submitted: %@", metric);
                }];
                [launchMetricOp addSuccessDependency:self.fetchCompletedOperation];

                [self.operationQueue addOperation:launchMetricOp];
            }
        }

        // Trigger the fake 'we're done' operation.
        [self runBeforeGroupFinished: self.fetchCompletedOperation];

        // Drop strong pointer to clients
        self.clientMap = @{};

        [eventS addMetrics:@{kSecurityRTCFieldNumKeychainItems:@(self.fetchedItems),
                             kSecurityRTCFieldTotalCKRecords:@(self.totalDeletions + self.totalModifications)}];
        [SecurityAnalyticsReporterRTC sendMetricWithEvent:eventS success:YES error:self.error];
    };

    [self dependOnBeforeGroupFinished:self.fetchCompletedOperation];
    [self dependOnBeforeGroupFinished:self.fetchRecordZoneChangesOperation];
    [self.container.privateCloudDatabase addOperation:self.fetchRecordZoneChangesOperation];
}

- (void)sendChangesToClient:(CKRecordZoneID*)recordZoneID moreComing:(BOOL)moreComing
{
    id<CKKSChangeFetcherClient> client = self.clientMap[recordZoneID];
    if(!client) {
        ckkserror_global("ckksfetch", "no client registered for %@, so why did we get any data?", recordZoneID);
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

    BOOL resync = [self.resyncingZones containsObject:recordZoneID];

    // Tell the client about these changes!
    [client changesFetched:zoneModifications
          deletedRecordIDs:zoneDeletions
                    zoneID:recordZoneID
            newChangeToken:self.changeTokens[recordZoneID]
                moreComing:moreComing
                    resync:resync];

    if(resync && !moreComing) {
        ckksnotice("ckksfetch", recordZoneID, "No more changes for zone; turning off resync bit");
        [self.resyncingZones removeObject:recordZoneID];
    }
}

- (void)cancel {
    [self.fetchRecordZoneChangesOperation cancel];
    [super cancel];
}

@end

#endif
