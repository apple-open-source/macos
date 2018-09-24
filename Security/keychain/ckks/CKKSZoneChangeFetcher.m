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

CKKSFetchBecause* const CKKSFetchBecauseAPNS = (CKKSFetchBecause*) @"apns";
CKKSFetchBecause* const CKKSFetchBecauseAPIFetchRequest = (CKKSFetchBecause*) @"api";
CKKSFetchBecause* const CKKSFetchBecauseCurrentItemFetchRequest = (CKKSFetchBecause*) @"currentitemcheck";
CKKSFetchBecause* const CKKSFetchBecauseInitialStart = (CKKSFetchBecause*) @"initialfetch";
CKKSFetchBecause* const CKKSFetchBecauseSecuritydRestart = (CKKSFetchBecause*) @"restart";
CKKSFetchBecause* const CKKSFetchBecausePreviousFetchFailed = (CKKSFetchBecause*) @"fetchfailed";
CKKSFetchBecause* const CKKSFetchBecauseNetwork = (CKKSFetchBecause*) @"network";
CKKSFetchBecause* const CKKSFetchBecauseKeyHierarchy = (CKKSFetchBecause*) @"keyhierarchy";
CKKSFetchBecause* const CKKSFetchBecauseTesting = (CKKSFetchBecause*) @"testing";
CKKSFetchBecause* const CKKSFetchBecauseResync = (CKKSFetchBecause*) @"resync";

#pragma mark - CKKSZoneChangeFetchDependencyOperation
@interface CKKSZoneChangeFetchDependencyOperation : CKKSResultOperation
@property CKKSZoneChangeFetcher* owner;
@property NSMutableArray<CKKSZoneChangeFetchDependencyOperation*>* chainDependents;
- (void)chainDependency:(CKKSZoneChangeFetchDependencyOperation*)newDependency;
@end

@implementation CKKSZoneChangeFetchDependencyOperation
- (instancetype)init {
    if((self = [super init])) {
        _chainDependents = [NSMutableArray array];
    }
    return self;
}

- (NSError* _Nullable)descriptionError {
    return [NSError errorWithDomain:CKKSResultDescriptionErrorDomain
                               code:CKKSResultDescriptionPendingSuccessfulFetch
                        description:@"Fetch failed"
                         underlying:self.owner.lastCKFetchError];
}

- (void)chainDependency:(CKKSZoneChangeFetchDependencyOperation*)newDependency {
    [self addSuccessDependency:newDependency];

    // There's no need to build a chain more than two links long. Move all our children up to depend on the new dependency.
    for(CKKSZoneChangeFetchDependencyOperation* op in self.chainDependents) {
        [newDependency.chainDependents addObject:op];
        [op addSuccessDependency:newDependency];
        [op removeDependency:self];
    }
    [self.chainDependents removeAllObjects];
}
@end

#pragma mark - CKKSZoneChangeFetcher

@interface CKKSZoneChangeFetcher ()
@property NSString* name;
@property NSOperationQueue* operationQueue;
@property dispatch_queue_t queue;

@property NSError* lastCKFetchError;

@property NSMapTable<CKRecordZoneID*, id<CKKSChangeFetcherClient>>* clientMap;

@property CKKSFetchAllRecordZoneChangesOperation* currentFetch;
@property CKKSResultOperation* currentProcessResult;

@property NSMutableSet<CKKSFetchBecause*>* currentFetchReasons;
@property NSMutableSet<CKRecordZoneNotification*>* apnsPushes;
@property bool newRequests; // true if there's someone pending on successfulFetchDependency
@property CKKSZoneChangeFetchDependencyOperation* successfulFetchDependency;

@property CKKSResultOperation* holdOperation;
@end

@implementation CKKSZoneChangeFetcher

- (instancetype)initWithContainer:(CKContainer*)container
                       fetchClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
              reachabilityTracker:(CKKSReachabilityTracker *)reachabilityTracker
{
    if((self = [super init])) {
        _container = container;
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _reachabilityTracker = reachabilityTracker;

        _currentFetchReasons = [[NSMutableSet alloc] init];
        _apnsPushes = [[NSMutableSet alloc] init];

        _clientMap = [NSMapTable strongToWeakObjectsMapTable];

        _name = @"zone-change-fetcher";
        _queue = dispatch_queue_create([_name UTF8String], DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];
        _successfulFetchDependency = [self createSuccesfulFetchDependency];

        _newRequests = false;

        // If we're testing, for the initial delay, use 0.2 second. Otherwise, 2s.
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 200 * NSEC_PER_MSEC : 2 * NSEC_PER_SEC);

        // If we're testing, for the initial delay, use 2 second. Otherwise, 30s.
        dispatch_time_t continuingDelay = (SecCKKSReduceRateLimiting() ? 2 * NSEC_PER_SEC : 30 * NSEC_PER_SEC);

        __weak __typeof(self) weakSelf = self;
        _fetchScheduler = [[CKKSNearFutureScheduler alloc] initWithName:@"zone-change-fetch-scheduler"
                                                           initialDelay:initialDelay
                                                        continuingDelay:continuingDelay
                                                       keepProcessAlive:false
                                              dependencyDescriptionCode:CKKSResultDescriptionPendingZoneChangeFetchScheduling
                                                                  block:^{
                                                                      [weakSelf maybeCreateNewFetch];
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

- (void)registerClient:(id<CKKSChangeFetcherClient>)client
{
    @synchronized(self.clientMap) {
        [self.clientMap setObject:client forKey:client.zoneID];
    }
}


- (CKKSResultOperation*)requestSuccessfulFetch:(CKKSFetchBecause*)why {
    return [self requestSuccessfulFetchForManyReasons:[NSSet setWithObject:why]];
}

- (CKKSResultOperation*)requestSuccessfulFetchForManyReasons:(NSSet<CKKSFetchBecause*>*)why
{
    return [self requestSuccessfulFetchForManyReasons:why apns:nil];
}

- (CKKSResultOperation*)requestSuccessfulFetchDueToAPNS:(CKRecordZoneNotification*)notification
{
    return [self requestSuccessfulFetchForManyReasons:[NSSet setWithObject:CKKSFetchBecauseAPNS] apns:notification];
}

- (CKKSResultOperation*)requestSuccessfulFetchForManyReasons:(NSSet<CKKSFetchBecause*>*)why apns:(CKRecordZoneNotification*)notification
{
    __block CKKSResultOperation* dependency = nil;
    dispatch_sync(self.queue, ^{
        dependency = self.successfulFetchDependency;
        self.newRequests = true;
        [self.currentFetchReasons unionSet:why];
        if(notification) {
            [self.apnsPushes addObject:notification];

            if(notification.ckksPushTracingEnabled) {
                // Report that we saw this notification before doing anything else
                secnotice("ckksfetch", "Submitting initial CKEventMetric due to notification %@", notification);

                CKEventMetric *metric = [[CKEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric.isPushTriggerFired = true;
                metric[@"push_token_uuid"] = notification.ckksPushTracingUUID;
                metric[@"push_received_date"] = notification.ckksPushReceivedDate;
                metric[@"push_event_name"] = @"CKKS APNS Push Received";

                [self.container submitEventMetric:metric];
            }

        }

        [self.fetchScheduler trigger];
    });

    return dependency;
}

-(void)maybeCreateNewFetch {
    dispatch_sync(self.queue, ^{
        if(self.newRequests &&
           (self.currentFetch == nil || [self.currentFetch isFinished]) &&
           (self.currentProcessResult == nil || [self.currentProcessResult isFinished])) {
            [self _onqueueCreateNewFetch];
        }
    });
}

-(void)_onqueueCreateNewFetch {
    dispatch_assert_queue(self.queue);

    __weak __typeof(self) weakSelf = self;

    CKKSZoneChangeFetchDependencyOperation* dependency = self.successfulFetchDependency;

    secnotice("ckksfetcher", "Starting a new fetch");

    NSMutableSet<CKKSFetchBecause*>* lastFetchReasons = self.currentFetchReasons;
    self.currentFetchReasons = [[NSMutableSet alloc] init];

    NSMutableSet<CKRecordZoneNotification*>* lastAPNSPushes = self.apnsPushes;
    self.apnsPushes = [[NSMutableSet alloc] init];

    CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName: [[lastFetchReasons sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"description" ascending:YES]]] componentsJoinedByString:@","]];

    NSMutableArray<id<CKKSChangeFetcherClient>>* clients = [NSMutableArray array];
    @synchronized(self.clientMap) {
        for(id<CKKSChangeFetcherClient> client in [self.clientMap objectEnumerator]) {
            if(client != nil) {
                [clients addObject:client];
            }
        }
    }

    if(clients.count == 0u) {
        // Nothing to do, really.
    }

    CKKSFetchAllRecordZoneChangesOperation* fetchAllChanges = [[CKKSFetchAllRecordZoneChangesOperation alloc] initWithContainer:self.container
                                                                                                                     fetchClass:self.fetchRecordZoneChangesOperationClass
                                                                                                                        clients:clients
                                                                                                                   fetchReasons:lastFetchReasons
                                                                                                                     apnsPushes:lastAPNSPushes
                                                                                                                    forceResync:false
                                                                                                               ckoperationGroup:operationGroup];

    if ([lastFetchReasons containsObject:CKKSFetchBecauseNetwork]) {
        [fetchAllChanges addNullableDependency: self.reachabilityTracker.reachabilityDependency]; // wait on network, if its unavailable
    }
    [fetchAllChanges addNullableDependency: self.holdOperation];

    self.currentProcessResult = [CKKSResultOperation operationWithBlock: ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            secerror("ckksfetcher: Received a null self pointer; strange.");
            return;
        }

        dispatch_sync(strongSelf.queue, ^{
            self.lastCKFetchError = fetchAllChanges.error;

            if(!fetchAllChanges.error) {
                // success! notify the listeners.
                [self.operationQueue addOperation: dependency];

                // Did new people show up and want another fetch?
                if(strongSelf.newRequests) {
                    [strongSelf.fetchScheduler trigger];
                }
            } else {
                // The operation errored. Chain the dependency on the current one...
                [dependency chainDependency:strongSelf.successfulFetchDependency];
                [strongSelf.operationQueue addOperation: dependency];

                // Check in with clients: should we keep fetching for them?
                bool attemptAnotherFetch = false;
                @synchronized(self.clientMap) {
                    for(CKRecordZoneID* zoneID in fetchAllChanges.fetchedZoneIDs) {
                        id<CKKSChangeFetcherClient> client = [self.clientMap objectForKey:zoneID];
                        if(client) {
                            attemptAnotherFetch |= [client notifyFetchError:fetchAllChanges.error];
                        }
                    }
                }

                if(!attemptAnotherFetch) {
                    secerror("ckksfetcher: All clients thought %@ is a fatal error. Not restarting fetch.", fetchAllChanges.error);
                    return;
                }

                // And in a bit, try the fetch again.
                NSNumber* delaySeconds = fetchAllChanges.error.userInfo[CKErrorRetryAfterKey];
                if([fetchAllChanges.error.domain isEqual: CKErrorDomain] && delaySeconds) {
                    secnotice("ckksfetcher", "Fetch failed with rate-limiting error, restarting in %@ seconds: %@", delaySeconds, fetchAllChanges.error);
                    [strongSelf.fetchScheduler waitUntil: NSEC_PER_SEC * [delaySeconds unsignedLongValue]];
                } else {
                    secnotice("ckksfetcher", "Fetch failed with error, restarting soon: %@", fetchAllChanges.error);
                }

                // Add the failed fetch reasons to the new fetch reasons
                [strongSelf.currentFetchReasons unionSet:lastFetchReasons];
                [strongSelf.apnsPushes unionSet:lastAPNSPushes];

                // If its a network error, make next try depend on network availability
                if ([self.reachabilityTracker isNetworkError:fetchAllChanges.error]) {
                    [strongSelf.currentFetchReasons addObject:CKKSFetchBecauseNetwork];
                } else {
                    [strongSelf.currentFetchReasons addObject:CKKSFetchBecausePreviousFetchFailed];
                }
                strongSelf.newRequests = true;
                [strongSelf.fetchScheduler trigger];
            }
        });
    }];
    self.currentProcessResult.name = @"zone-change-fetcher-worker";
    [self.currentProcessResult addDependency: fetchAllChanges];

    [self.operationQueue addOperation:self.currentProcessResult];

    self.currentFetch = fetchAllChanges;
    [self.operationQueue addOperation:self.currentFetch];

    // creata a new fetch dependency, for all those who come in while this operation is executing
    self.newRequests = false;
    self.successfulFetchDependency = [self createSuccesfulFetchDependency];
}

-(CKKSZoneChangeFetchDependencyOperation*)createSuccesfulFetchDependency {
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

@end

#endif


