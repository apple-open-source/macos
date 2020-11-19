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

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#import <dispatch/dispatch.h>
#import <sys/types.h>
#import <sys/socket.h>
#import <netinet/in.h>

#import <nw/private.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/ot/ObjCImprovements.h"

// force reachability timeout every now and then
#define REACHABILITY_TIMEOUT (12 * 3600 * NSEC_PER_SEC)

@interface CKKSReachabilityTracker ()
@property bool haveNetwork;
@property dispatch_queue_t queue;
@property NSOperationQueue* operationQueue;
@property nw_path_monitor_t networkMonitor;
@property dispatch_source_t timer;
@end

@implementation CKKSReachabilityTracker

- (instancetype)init {
    if((self = [super init])) {
        _queue = dispatch_queue_create("reachabiltity-tracker", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _operationQueue = [[NSOperationQueue alloc] init];

        dispatch_sync(_queue, ^{
            [self _onQueueResetReachabilityDependency];
        });

        WEAKIFY(self);

        if(!SecCKKSTestsEnabled()) {
            _networkMonitor = nw_path_monitor_create();
            nw_path_monitor_set_queue(self.networkMonitor, _queue);
            nw_path_monitor_set_update_handler(self.networkMonitor, ^(nw_path_t  _Nonnull path) {
                STRONGIFY(self);
                bool networkAvailable = (nw_path_get_status(path) == nw_path_status_satisfied);

                ckksinfo_global("ckksnetwork", "nw_path update: network is %@", networkAvailable ? @"available" : @"unavailable");
                [self _onqueueSetNetworkReachability:networkAvailable];
            });
            nw_path_monitor_start(self.networkMonitor);
        }
    }
    return self;
}

- (NSString*)description {
    return [NSString stringWithFormat: @"<CKKSReachabilityTracker: %@>", self.haveNetwork ? @"online" : @"offline"];
}

- (bool)currentReachability {
    __block bool currentReachability = false;
    dispatch_sync(self.queue, ^{
        currentReachability = self.haveNetwork;
    });
    return currentReachability;
}

- (void)_onQueueRunReachabilityDependency
{
    dispatch_assert_queue(self.queue);
    // We have network now, or the timer expired. Either way, execute dependency
    if (self.reachabilityDependency) {
        [self.operationQueue addOperation: self.reachabilityDependency];
        self.reachabilityDependency = nil;
    }
    if (self.timer) {
        dispatch_source_cancel(self.timer);
        self.timer = nil;
    }
}

- (void)_onQueueResetReachabilityDependency {
    dispatch_assert_queue(self.queue);

    if(self.reachabilityDependency == nil || ![self.reachabilityDependency isPending]) {
        WEAKIFY(self);

        ckksnotice_global("network", "Network unavailable");
        self.reachabilityDependency = [CKKSResultOperation named:@"network-available-dependency" withBlock: ^{
            STRONGIFY(self);
            if (self.haveNetwork) {
                ckksnotice_global("network", "Network available");
            } else {
                ckksnotice_global("network", "Network still not available, retrying after waiting %2.1f hours",
                                  ((float)(REACHABILITY_TIMEOUT/NSEC_PER_SEC)) / 3600);
            }
        }];

        /*
         * Make sure we are not stuck forever and retry every REACHABILITY_TIMEOUT
         */
        self.timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                            0,
                                            (dispatch_source_timer_flags_t)0,
                                            self.queue);
        dispatch_source_set_event_handler(self.timer, ^{
            STRONGIFY(self);
            if (self == nil) {
                return;
            }
            if (self.timer) {
                [[CKKSAnalytics logger] noteEvent:CKKSEventReachabilityTimerExpired];
                [self _onQueueRunReachabilityDependency];
            }
        });

        dispatch_source_set_timer(self.timer,
                                  dispatch_time(DISPATCH_TIME_NOW, REACHABILITY_TIMEOUT),
                                  DISPATCH_TIME_FOREVER, //one-shot
                                  30 * NSEC_PER_SEC);
        dispatch_resume(self.timer);
    }
}

- (void)_onqueueSetNetworkReachability:(bool)haveNetwork {
    dispatch_assert_queue(self.queue);

    bool hadNetwork = self.haveNetwork;
    self.haveNetwork = !!(haveNetwork);

    if(hadNetwork != self.haveNetwork) {
        if(self.haveNetwork) {
            // We're have network now
            [self _onQueueRunReachabilityDependency];
        } else {
            [self _onQueueResetReachabilityDependency];
        }
    }
}

- (void)setNetworkReachability:(bool)reachable
{
    dispatch_sync(self.queue, ^{
        [self _onqueueSetNetworkReachability:reachable];
    });
}

- (bool)isNetworkError:(NSError *)error {
    return [CKKSReachabilityTracker isNetworkError:error];
}

+ (bool)isNetworkError:(NSError *)error {
    if (error == nil) {
        return false;
    }

    if([CKKSReachabilityTracker isNetworkFailureError:error]) {
        return true;
    }

    if ([error.domain isEqualToString:CKErrorDomain] &&
            (error.code == CKErrorNetworkUnavailable)) {
        return true;
    }

    if ([error.domain isEqualToString:NSURLErrorDomain] &&
        error.code == NSURLErrorTimedOut) {
        return true;
    }

    return false;
}

+ (bool)isNetworkFailureError:(NSError *)error
{
    if (error == nil) {
        return false;
    }
    if ([error.domain isEqualToString:CKErrorDomain] && error.code == CKErrorNetworkFailure) {
        return true;
    }

    return false;
}

@end

#endif // OCTAGON

