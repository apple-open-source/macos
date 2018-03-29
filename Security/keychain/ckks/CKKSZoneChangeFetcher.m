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
@end

@implementation CKKSZoneChangeFetchDependencyOperation
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
@property dispatch_queue_t queue;

@property NSError* lastCKFetchError;

@property CKKSFetchAllRecordZoneChangesOperation* currentFetch;
@property CKKSResultOperation* currentProcessResult;

@property NSMutableSet<CKKSFetchBecause*>* currentFetchReasons;
@property bool newRequests; // true if there's someone pending on successfulFetchDependency
@property bool newResyncRequests; // true if someone asked for a refetch operation
@property CKKSResultOperation* successfulFetchDependency;

@property CKKSNearFutureScheduler* fetchScheduler;

@property CKKSResultOperation* holdOperation;
@end

@implementation CKKSZoneChangeFetcher

- (instancetype)initWithCKKSKeychainView:(CKKSKeychainView*)ckks {
    if((self = [super init])) {
        _ckks = ckks;
        _zoneID = ckks.zoneID;

        _currentFetchReasons = [[NSMutableSet alloc] init];

        _name = [NSString stringWithFormat:@"zone-change-fetcher-%@", _zoneID.zoneName];
        _queue = dispatch_queue_create([_name UTF8String], DISPATCH_QUEUE_SERIAL);
        _successfulFetchDependency = [self createSuccesfulFetchDependency];

        _newRequests = false;

        // If we're testing, for the initial delay, use 0.2 second. Otherwise, 2s.
        dispatch_time_t initialDelay = (SecCKKSReduceRateLimiting() ? 200 * NSEC_PER_MSEC : 2 * NSEC_PER_SEC);

        // If we're testing, for the initial delay, use 2 second. Otherwise, 30s.
        dispatch_time_t continuingDelay = (SecCKKSReduceRateLimiting() ? 2 * NSEC_PER_SEC : 30 * NSEC_PER_SEC);

        __weak __typeof(self) weakSelf = self;
        _fetchScheduler = [[CKKSNearFutureScheduler alloc] initWithName:[NSString stringWithFormat:@"zone-change-fetch-scheduler-%@", self.zoneID.zoneName]
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

- (CKKSResultOperation*)requestSuccessfulFetch:(CKKSFetchBecause*)why {
    return [self requestSuccessfulFetch:why resync:false];
}

- (CKKSResultOperation*)requestSuccessfulResyncFetch:(CKKSFetchBecause*)why {
    return [self requestSuccessfulFetch:why resync:true];
}

- (CKKSResultOperation*)requestSuccessfulFetch:(CKKSFetchBecause*)why resync:(bool)resync {
    __block CKKSResultOperation* dependency = nil;
    dispatch_sync(self.queue, ^{
        dependency = self.successfulFetchDependency;
        self.newRequests = true;
        self.newResyncRequests |= resync;
        [self.currentFetchReasons addObject: why];

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

    CKKSResultOperation* dependency = self.successfulFetchDependency;

    CKKSKeychainView* ckks = self.ckks; // take a strong reference
    if(!ckks) {
        secerror("ckksfetcher: received a null CKKSKeychainView pointer; strange.");
        return;
    }

    ckksnotice("ckksfetcher", self.zoneID, "Starting a new fetch for %@", self.zoneID.zoneName);

    NSMutableSet<CKKSFetchBecause*>* lastFetchReasons = self.currentFetchReasons;
    self.currentFetchReasons = [[NSMutableSet alloc] init];
    if(self.newResyncRequests) {
        [lastFetchReasons addObject:CKKSFetchBecauseResync];
    }

    CKOperationGroup* operationGroup = [CKOperationGroup CKKSGroupWithName: [[lastFetchReasons sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"description" ascending:YES]]] componentsJoinedByString:@","]];

    CKKSFetchAllRecordZoneChangesOperation* fetchAllChanges = [[CKKSFetchAllRecordZoneChangesOperation alloc] initWithCKKSKeychainView:ckks
                                                                                                                          fetchReasons:lastFetchReasons
                                                                                                                      ckoperationGroup:operationGroup];
    if ([lastFetchReasons containsObject:CKKSFetchBecauseNetwork]) {
        [fetchAllChanges addNullableDependency: ckks.reachabilityTracker.reachablityDependency]; // wait on network, if its unavailable
    }
    [fetchAllChanges addNullableDependency: self.holdOperation];
    fetchAllChanges.resync = self.newResyncRequests;
    self.newResyncRequests = false;

    // Can't fetch until the zone is setup.
    [fetchAllChanges addNullableDependency:ckks.zoneSetupOperation];

    self.currentProcessResult = [CKKSResultOperation operationWithBlock: ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            secerror("ckksfetcher: Received a null self pointer; strange.");
            return;
        }

        CKKSKeychainView* blockckks = strongSelf.ckks; // take a strong reference
        if(!blockckks) {
            secerror("ckksfetcher: Received a null CKKSKeychainView pointer; strange.");
            return;
        }

        dispatch_sync(strongSelf.queue, ^{
            self.lastCKFetchError = fetchAllChanges.error;

            if(!fetchAllChanges.error) {
                // success! notify the listeners.
                [blockckks scheduleOperation: dependency];

                // Did new people show up and want another fetch?
                if(strongSelf.newRequests) {
                    [strongSelf.fetchScheduler trigger];
                }
            } else {
                // The operation errored. Chain the dependency on the current one...
                [dependency addSuccessDependency: strongSelf.successfulFetchDependency];
                [blockckks scheduleOperation: dependency];

                if([blockckks isFatalCKFetchError: fetchAllChanges.error]) {
                    ckkserror("ckksfetcher", strongSelf.zoneID, "Notified that %@ is a fatal error. Not restarting fetch.", fetchAllChanges.error);
                    return;
                }


                // And in a bit, try the fetch again.
                NSNumber* delaySeconds = fetchAllChanges.error.userInfo[CKErrorRetryAfterKey];
                if([fetchAllChanges.error.domain isEqual: CKErrorDomain] && delaySeconds) {
                    ckksnotice("ckksfetcher", strongSelf.zoneID, "Fetch failed with rate-limiting error, restarting in %@ seconds: %@", delaySeconds, fetchAllChanges.error);
                    [strongSelf.fetchScheduler waitUntil: NSEC_PER_SEC * [delaySeconds unsignedLongValue]];
                } else {
                    ckksnotice("ckksfetcher", strongSelf.zoneID, "Fetch failed with error, restarting soon: %@", fetchAllChanges.error);
                }

                // Add the failed fetch reasons to the new fetch reasons
                [strongSelf.currentFetchReasons unionSet:lastFetchReasons];
                // If its a network error, make next try depend on network availability
                if ([blockckks.reachabilityTracker isNetworkError:fetchAllChanges.error]) {
                    [strongSelf.currentFetchReasons addObject:CKKSFetchBecauseNetwork];
                } else {
                    [strongSelf.currentFetchReasons addObject:CKKSFetchBecausePreviousFetchFailed];
                }
                strongSelf.newRequests = true;
                strongSelf.newResyncRequests |= fetchAllChanges.resync;
                [strongSelf.fetchScheduler trigger];
            }
        });
    }];
    self.currentProcessResult.name = @"zone-change-fetcher-worker";
    [self.currentProcessResult addDependency: fetchAllChanges];

    [ckks scheduleOperation: self.currentProcessResult];

    self.currentFetch = fetchAllChanges;
    [ckks scheduleOperation: self.currentFetch];

    // creata a new fetch dependency, for all those who come in while this operation is executing
    self.newRequests = false;
    self.successfulFetchDependency = [self createSuccesfulFetchDependency];
}

-(CKKSZoneChangeFetchDependencyOperation*)createSuccesfulFetchDependency {
    CKKSZoneChangeFetchDependencyOperation* dep = [[CKKSZoneChangeFetchDependencyOperation alloc] init];
    __weak __typeof(dep) weakDep = dep;

    // Since these dependencies might chain, when one runs, break the chain.
    [dep addExecutionBlock:^{
        __strong __typeof(dep) strongDep = weakDep;

        // Remove all dependencies
        NSArray* deps = [strongDep.dependencies copy];
        for(NSOperation* op in deps) {
            [strongDep removeDependency: op];
        }
    }];
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


