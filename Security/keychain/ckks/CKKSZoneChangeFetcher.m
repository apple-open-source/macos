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

#import <dispatch/dispatch.h>

#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ckks/CKKSFetchAllRecordZoneChangesOperation.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/analytics/SecEventMetric.h"
#import "keychain/analytics/SecMetrics.h"
#import "keychain/ot/ObjCImprovements.h"

CKKSFetchBecause* const CKKSFetchBecauseAPNS = (CKKSFetchBecause*) @"apns";
CKKSFetchBecause* const CKKSFetchBecauseAPIFetchRequest = (CKKSFetchBecause*) @"api";
CKKSFetchBecause* const CKKSFetchBecauseSEAPIFetchRequest = (CKKSFetchBecause*) @"se-api";
CKKSFetchBecause* const CKKSFetchBecauseKeySetFetchRequest = (CKKSFetchBecause*) @"keyset";
CKKSFetchBecause* const CKKSFetchBecauseCurrentItemFetchRequest = (CKKSFetchBecause*) @"currentitemcheck";
CKKSFetchBecause* const CKKSFetchBecauseInitialStart = (CKKSFetchBecause*) @"initialfetch";
CKKSFetchBecause* const CKKSFetchBecausePreviousFetchFailed = (CKKSFetchBecause*) @"fetchfailed";
CKKSFetchBecause* const CKKSFetchBecauseNetwork = (CKKSFetchBecause*) @"network";
CKKSFetchBecause* const CKKSFetchBecauseKeyHierarchy = (CKKSFetchBecause*) @"keyhierarchy";
CKKSFetchBecause* const CKKSFetchBecauseTesting = (CKKSFetchBecause*) @"testing";
CKKSFetchBecause* const CKKSFetchBecauseResync = (CKKSFetchBecause*) @"resync";
CKKSFetchBecause* const CKKSFetchBecauseMoreComing = (CKKSFetchBecause*) @"more-coming";
CKKSFetchBecause* const CKKSFetchBecauseResolvingConflict = (CKKSFetchBecause*) @"conflict";
CKKSFetchBecause* const CKKSFetchBecausePeriodicRefetch = (CKKSFetchBecause*) @"periodic";
CKKSFetchBecause* const CKKSFetchBecauseOctagonPairingComplete = (CKKSFetchBecause*) @"octagon-pairing-complete";

#pragma mark - CKKSZoneChangeFetchDependencyOperation
@interface CKKSZoneChangeFetchDependencyOperation : CKKSResultOperation
@property (weak) CKKSZoneChangeFetcher* owner;
@end

@implementation CKKSZoneChangeFetchDependencyOperation
- (instancetype)init {
    if((self = [super init])) {
    }
    return self;
}

- (NSError* _Nullable)descriptionError {
    return [NSError errorWithDomain:CKKSResultDescriptionErrorDomain
                               code:CKKSResultDescriptionPendingSuccessfulFetch
                        description:@"Fetch failed"
                         underlying:self.owner.lastCKFetchError];
}

@end

#pragma mark - CKKSZoneChangeFetcher

@interface CKKSZoneChangeFetcher ()
@property NSString* name;
@property NSOperationQueue* operationQueue;
@property dispatch_queue_t queue;
@property BOOL halted;

@property NSError* lastCKFetchError;

@property NSMapTable<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clientMap;

@property CKKSFetchAllRecordZoneChangesOperation* currentFetch;
@property CKKSResultOperation* currentProcessResult;

@property NSMutableSet<CKKSFetchBecause*>* currentFetchReasons;
@property NSMutableSet<CKRecordZoneNotification*>* apnsPushes;
@property bool newRequests; // true if there's someone pending on successfulFetchDependency

// Used for RTC reporting
@property NSString* altDSID;

@property CKKSZoneChangeFetchDependencyOperation* successfulFetchDependency;

@property NSMutableSet<CKKSZoneChangeFetchDependencyOperation*>* inflightFetchDependencies;

@property (nullable) CKKSZoneChangeFetchDependencyOperation* inflightFetchDependency;

@property CKKSResultOperation* holdOperation;
@end

@implementation CKKSZoneChangeFetcher

- (instancetype)initWithContainer:(CKContainer*)container
                       fetchClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
              reachabilityTracker:(CKKSReachabilityTracker *)reachabilityTracker
                          altDSID:(NSString*)altDSID
                       sendMetric:(bool)sendMetric
{
    if((self = [super init])) {
        _container = container;
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _reachabilityTracker = reachabilityTracker;

        _currentFetchReasons = [[NSMutableSet alloc] init];
        _apnsPushes = [[NSMutableSet alloc] init];

        _clientMap = [NSMapTable strongToWeakObjectsMapTable];

        _name = @"zone-change-fetcher";
        _queue = dispatch_queue_create([_name UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _operationQueue = [[NSOperationQueue alloc] init];
        _successfulFetchDependency = [self createSuccessfulFetchDependency];

        _inflightFetchDependencies = [NSMutableSet set];
        _inflightFetchDependency = nil;

        _newRequests = false;
        _altDSID = altDSID;
        _sendMetric = sendMetric;

        // If we're testing, for the initial delay, use 0.1 second. Otherwise, 2s.
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 100 * NSEC_PER_MSEC : 2 * NSEC_PER_SEC);

        // If we're testing, for the maximum delay, use 6 second. Otherwise, 2m.
        dispatch_time_t maximumDelay = (SecCKKSReduceRateLimiting() ? 6 * NSEC_PER_SEC : 120 * NSEC_PER_SEC);

        WEAKIFY(self);
        _fetchScheduler = [[CKKSNearFutureScheduler alloc] initWithName:@"zone-change-fetch-scheduler"
                                                           initialDelay:initialDelay
                                                     exponentialBackoff:2
                                                           maximumDelay:maximumDelay
                                                       keepProcessAlive:false
                                              dependencyDescriptionCode:CKKSResultDescriptionPendingZoneChangeFetchScheduling
                                                                  block:^{
                                                                      STRONGIFY(self);
                                                                      [self maybeCreateNewFetch];
                                                                  }];
    }
    return self;
}

- (NSString*)description {
    NSDate* nextFetchAt = self.fetchScheduler.nextFireTime;
    if(nextFetchAt) {
        NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
        [dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
        return [NSString stringWithFormat: @"<CKKSZoneChangeFetcher(%@): next fetch at %@", self.name, [dateFormatter stringFromDate: nextFetchAt]];
    } else {
        return [NSString stringWithFormat: @"<CKKSZoneChangeFetcher(%@): no pending fetches", self.name];
    }
}

- (void)registerClient:(id<CKKSChangeFetcherClient>)client zoneID:(CKRecordZoneID*)zoneID
{
    @synchronized(self.clientMap) {
        [self.clientMap setObject:client forKey:zoneID];
    }
}

- (NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>*)strongClientMap
{
    NSMutableDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clients = [NSMutableDictionary dictionary];
    @synchronized (self.clientMap) {
        for(CKRecordZoneID* key in [self.clientMap keyEnumerator]) {
            id<CKKSChangeFetcherClient> client = [self.clientMap objectForKey:key];

            if(client) {
                clients[key] = client;
            }
        }
    }
    return clients;
}

- (CKKSResultOperation*)requestSuccessfulFetch:(CKKSFetchBecause*)why {
    return [self requestSuccessfulFetchForManyReasons:[NSSet setWithObject:why]];
}

- (void)notifyZoneChange:(CKRecordZoneNotification* _Nullable)notification
{
    ckksnotice_global("ckkspush", "received a zone change notification for %@ %@", self, notification);
    [self requestFetchDueToAPNS:notification];
}

- (CKKSResultOperation*)requestFetchDueToAPNS:(CKRecordZoneNotification*)notification
{
    __block BOOL notReady = YES;

    // make sure we don't hold the self.queue when we call out to clients since that will lead
    // to lock inversions

    NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clients = [self strongClientMap];

    for(CKRecordZoneID* zoneID in clients.allKeys) {
        id<CKKSChangeFetcherClient> client = clients[zoneID];
        if([client zoneIsReadyForFetching:zoneID]) {
            notReady = NO;
            break;
        }
    }

    dispatch_sync(self.queue, ^{

        if(notification) {
            [self.apnsPushes addObject:notification];
            if(notification.ckksPushTracingEnabled) {
                // Report that we saw this notification before doing anything else
                ckksnotice_global("ckksfetch", "Submitting initial CKEventMetric due to notification %@", notification);

                CKEventMetric *metric = [[CKEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric.isPushTriggerFired = true;
                metric[@"push_token_uuid"] = notification.ckksPushTracingUUID;
                metric[@"push_received_date"] = notification.ckksPushReceivedDate;
                metric[@"push_event_name"] = @"CKKS APNS Push Received";
                metric[@"zones_status"] = notReady ? @"not-ready" : @"ready";

                [self.container submitEventMetric:metric];

                SecEventMetric *metric2 = [[SecEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric2[@"push_token_uuid"] = notification.ckksPushTracingUUID;
                metric2[@"push_received_date"] = notification.ckksPushReceivedDate;
                metric2[@"push_event_name"] = @"CKKS APNS Push Received-webtunnel";
                metric[@"zones_status"] = notReady ? @"not-ready" : @"ready";

                [[SecMetrics managerObject] submitEvent:metric2];
            }
        }

    });

    if (notReady) {
        ckksnotice_global("ckksfetch", "Skipping fetching size no zone is ready");
        return NULL;
    }

    return [self requestSuccessfulFetchForManyReasons:[NSSet setWithObject:CKKSFetchBecauseAPNS]];
}

- (CKKSResultOperation*)requestSuccessfulFetchForManyReasons:(NSSet<CKKSFetchBecause*>*)why
{
    __block CKKSResultOperation* dependency = nil;
    dispatch_sync(self.queue, ^{
        dependency = self.successfulFetchDependency;
        self.newRequests = true;
        [self.currentFetchReasons unionSet:why];

        [self.fetchScheduler trigger];
    });

    return dependency;
}

- (CKKSResultOperation* _Nullable)inflightFetch
{
    __block CKKSResultOperation* dependency = nil;
    dispatch_sync(self.queue, ^{

        // If we'll have a new fetch in the future, return its status.
        if(self.newRequests || self.inflightFetchDependency == nil) {
            dependency = self.successfulFetchDependency;
        } else {
            // Otherwise, return the last triggered fetch
            dependency = self.inflightFetchDependency;
        }
    });

    return dependency;
}

-(void)maybeCreateNewFetchOnQueue {
    dispatch_assert_queue(self.queue);

    if(self.halted) {
        ckksnotice_global("ckksfetcher", "Halted; not starting a new fetch");
        return;
    }

    if(self.newRequests &&
       (self.currentFetch == nil || [self.currentFetch isFinished]) &&
       (self.currentProcessResult == nil || [self.currentProcessResult isFinished])) {
        [self _onqueueCreateNewFetch];
    }
}

-(void)maybeCreateNewFetch {
    dispatch_sync(self.queue, ^{
        [self maybeCreateNewFetchOnQueue];
    });
}

-(void)_onqueueCreateNewFetch {
    dispatch_assert_queue(self.queue);

    WEAKIFY(self);

    CKKSZoneChangeFetchDependencyOperation* dependency = self.successfulFetchDependency;
    self.inflightFetchDependency = dependency;
    [self.inflightFetchDependencies addObject:dependency];

    // create a new fetch dependency, for all those who come in while this operation is executing
    self.newRequests = false;
    self.successfulFetchDependency = [self createSuccessfulFetchDependency];

    NSMutableSet<CKKSFetchBecause*>* lastFetchReasons = self.currentFetchReasons;
    self.currentFetchReasons = [[NSMutableSet alloc] init];

    NSString *reasonsString = [[lastFetchReasons sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"description" ascending:YES]]] componentsJoinedByString:@","];

    ckksnotice_global("ckksfetcher", "Starting a new fetch, reasons: %@", reasonsString);

    NSMutableSet<CKRecordZoneNotification*>* lastAPNSPushes = self.apnsPushes;
    self.apnsPushes = [[NSMutableSet alloc] init];

    CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName: reasonsString];

    NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clientMap = [self strongClientMap];

    if(clientMap.count == 0u) {
        ckksnotice_global("ckksfetcher", "No clients");
        // Nothing to do, really.
    }

    CKKSFetchAllRecordZoneChangesOperation* fetchAllChanges = [[CKKSFetchAllRecordZoneChangesOperation alloc] initWithContainer:self.container
                                                                                                                     fetchClass:self.fetchRecordZoneChangesOperationClass
                                                                                                                      clientMap:clientMap
                                                                                                                   fetchReasons:lastFetchReasons
                                                                                                                     apnsPushes:lastAPNSPushes
                                                                                                                    forceResync:false
                                                                                                               ckoperationGroup:operationGroup
                                                                                                                        altDSID:self.altDSID
                                                                                                                     sendMetric:self.sendMetric
    ];

    if ([lastFetchReasons containsObject:CKKSFetchBecauseNetwork]) {
        ckksnotice_global("ckksfetcher", "blocking fetch on network reachability");
        [fetchAllChanges addNullableDependency: self.reachabilityTracker.reachabilityDependency]; // wait on network, if its unavailable
    }
    [fetchAllChanges addNullableDependency: self.holdOperation];

    self.currentProcessResult = [CKKSResultOperation operationWithBlock: ^{
        STRONGIFY(self);
        if(!self) {
            ckkserror_global("ckksfetcher", "Received a null self pointer; strange.");
            return;
        }

        bool attemptAnotherFetch = false;
        if(fetchAllChanges.error != nil) {
            ckkserror_global("ckksfetcher", "Interrogating clients about fetch error: %@", fetchAllChanges.error);

            // Check in with clients: should we keep fetching for them?
            // Copy the client map so we can iterate without thread-safety issues. We can't be on a serial queue when we upcall -shouldRetryAfterFetchError
            // to the client, on pain of potential deadlock with -registerClient.
            NSDictionary<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* threadClientMap = [self strongClientMap];

            for(CKRecordZoneID* zoneID in fetchAllChanges.fetchedZoneIDs) {
                id<CKKSChangeFetcherClient> client = [threadClientMap objectForKey:zoneID];
                if(client) {
                    attemptAnotherFetch |= [client shouldRetryAfterFetchError:fetchAllChanges.error
                                                                       zoneID:zoneID];
                }
            }
        }

        dispatch_sync(self.queue, ^{
            self.lastCKFetchError = fetchAllChanges.error;

            if(fetchAllChanges.error == nil) {
                // success! notify the listeners.

                for(CKKSZoneChangeFetchDependencyOperation* dependencyOp in self.inflightFetchDependencies) {
                    [self.operationQueue addOperation:dependencyOp];
                }
                [self.inflightFetchDependencies removeAllObjects];
                self.currentFetch = nil;

                // Did new people show up and want another fetch?
                if(self.newRequests) {
                    [self.fetchScheduler trigger];
                }
            } else {
                // The operation errored.

                if(!attemptAnotherFetch) {
                    ckkserror_global("ckksfetcher", "All clients thought %@ is a fatal error. Not restarting fetch.", fetchAllChanges.error);
                    return;
                }

                // And in a bit, try the fetch again.
                NSTimeInterval delay = CKRetryAfterSecondsForError(fetchAllChanges.error);
                if (delay) {
                    ckksnotice_global("ckksfetcher", "Fetch failed with rate-limiting error, restarting in %.1f seconds: %@", delay, fetchAllChanges.error);
                    [self.fetchScheduler waitUntil:NSEC_PER_SEC * delay];
                } else {
                    ckksnotice_global("ckksfetcher", "Fetch failed with error, restarting soon: %@", fetchAllChanges.error);
                }

                // Add the failed fetch reasons to the new fetch reasons
                [self.currentFetchReasons unionSet:lastFetchReasons];
                [self.apnsPushes unionSet:lastAPNSPushes];

                // If its a network error, make next try depend on network availability
                if ([self.reachabilityTracker isNetworkError:fetchAllChanges.error]) {
                    [self.currentFetchReasons addObject:CKKSFetchBecauseNetwork];
                } else {
                    [self.currentFetchReasons addObject:CKKSFetchBecausePreviousFetchFailed];
                }
                self.newRequests = true;
                [self.fetchScheduler trigger];
            }
        });
    }];

    // now let new new fetch go and process it's results
    self.currentProcessResult.name = @"zone-change-fetcher-worker";
    [self.currentProcessResult addDependency: fetchAllChanges];

    [self.operationQueue addOperation:self.currentProcessResult];

    self.currentFetch = fetchAllChanges;
    [self.operationQueue addOperation:self.currentFetch];
}

-(CKKSZoneChangeFetchDependencyOperation*)createSuccessfulFetchDependency {
    CKKSZoneChangeFetchDependencyOperation* dep = [[CKKSZoneChangeFetchDependencyOperation alloc] init];

    dep.name = @"successful-fetch-dependency";
    dep.descriptionErrorCode = CKKSResultDescriptionPendingSuccessfulFetch;
    dep.owner = self;

    return dep;
}

- (void)holdFetchesUntil:(CKKSResultOperation*)holdOperation {
    self.holdOperation = holdOperation;
}

-(void)cancel {
    [self.fetchScheduler cancel];
}

- (void)halt
{
    [self.fetchScheduler cancel];
    dispatch_sync(self.queue, ^{
        self.halted = YES;
    });

    [self.currentFetch cancel];
    if(self.holdOperation) {
        [self.currentFetch removeDependency:self.holdOperation];
    }
    [self.currentFetch waitUntilFinished];
    [self.currentProcessResult waitUntilFinished];
}

@end

#endif


