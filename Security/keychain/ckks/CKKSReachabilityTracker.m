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

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"

// force reachability timeout every now and then
#define REACHABILITY_TIMEOUT (12 * 3600 * NSEC_PER_SEC)

@interface CKKSReachabilityTracker ()
@property bool haveNetwork;
@property dispatch_queue_t queue;
@property NSOperationQueue* operationQueue;
@property (assign) SCNetworkReachabilityRef reachability;
@property dispatch_source_t timer;
@end

@implementation CKKSReachabilityTracker

static void
callout(SCNetworkReachabilityRef reachability,
        SCNetworkReachabilityFlags flags,
        void *context)
{
    CKKSReachabilityTracker *tracker = (__bridge id)context;
    [tracker _onqueueRecheck:flags];
}

- (instancetype)init {
    if((self = [super init])) {
        _queue = dispatch_queue_create("reachabiltity-tracker", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _operationQueue = [[NSOperationQueue alloc] init];

        dispatch_sync(_queue, ^{
            [self _onQueueResetReachabilityDependency];
        });

        __weak __typeof(self) weakSelf = self;

        if(!SecCKKSTestsEnabled()) {
            struct sockaddr_in zeroAddress;
            bzero(&zeroAddress, sizeof(zeroAddress));
            zeroAddress.sin_len = sizeof(zeroAddress);
            zeroAddress.sin_family = AF_INET;

            _reachability = SCNetworkReachabilityCreateWithAddress(NULL, (struct sockaddr *)&zeroAddress);

            SCNetworkReachabilityContext context = {0, (__bridge void *)(self), NULL, NULL, NULL};
            SCNetworkReachabilitySetDispatchQueue(_reachability, _queue);
            SCNetworkReachabilitySetCallback(_reachability, callout, &context);
        }

        [weakSelf recheck];
    }
    return self;
}

-(NSString*)description {
    return [NSString stringWithFormat: @"<CKKSReachabilityTracker: %@>", self.haveNetwork ? @"online" : @"offline"];
}

-(bool)currentReachability {
    __block bool currentReachability = false;
    dispatch_sync(self.queue, ^{
        currentReachability = self.haveNetwork;
    });
    return currentReachability;
}

-(void)_onQueueRunreachabilityDependency
{
    dispatch_assert_queue(self.queue);
    // We're have network now, or timer expired, either way, execute dependency
    if (self.reachabilityDependency) {
        [self.operationQueue addOperation: self.reachabilityDependency];
        self.reachabilityDependency = nil;
    }
    if (self.timer) {
        dispatch_source_cancel(self.timer);
        self.timer = nil;
    }
}

-(void)_onQueueResetReachabilityDependency {
    dispatch_assert_queue(self.queue);

    if(self.reachabilityDependency == nil || ![self.reachabilityDependency isPending]) {
        __weak __typeof(self) weakSelf = self;

        secnotice("ckksnetwork", "Network unavailable");
        self.reachabilityDependency = [CKKSResultOperation named:@"network-available-dependency" withBlock: ^{
            __typeof(self) strongSelf = weakSelf;
            if (strongSelf == nil) {
                return;
            }
            if (strongSelf.haveNetwork) {
                secnotice("ckksnetwork", "Network available");
            } else {
                secnotice("ckksnetwork", "Network still not available, retrying after waiting %2.1f hours",
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
            __typeof(self) strongSelf = weakSelf;
            if (strongSelf == nil) {
                return;
            }
            if (strongSelf.timer) {
                [[CKKSAnalytics logger] noteEvent:CKKSEventReachabilityTimerExpired];
                [strongSelf _onQueueRunreachabilityDependency];
            }
        });

        dispatch_source_set_timer(self.timer,
                                  dispatch_time(DISPATCH_TIME_NOW, REACHABILITY_TIMEOUT),
                                  DISPATCH_TIME_FOREVER, //one-shot
                                  30 * NSEC_PER_SEC);
        dispatch_resume(self.timer);
    }
}

-(void)_onqueueRecheck:(SCNetworkReachabilityFlags)flags {
    dispatch_assert_queue(self.queue);

    const SCNetworkReachabilityFlags reachabilityFlags =
        kSCNetworkReachabilityFlagsReachable
        | kSCNetworkReachabilityFlagsConnectionAutomatic
#if TARGET_OS_IPHONE
        | kSCNetworkReachabilityFlagsIsWWAN
#endif
        ;

    bool hadNetwork = self.haveNetwork;
    self.haveNetwork = !!(flags & reachabilityFlags);

    if(hadNetwork != self.haveNetwork) {
        if(self.haveNetwork) {
            // We're have network now
            [self _onQueueRunreachabilityDependency];
        } else {
            [self _onQueueResetReachabilityDependency];
        }
    }
}

+ (SCNetworkReachabilityFlags)getReachabilityFlags:(SCNetworkReachabilityRef)target
{
    SCNetworkReachabilityFlags flags;
    if (SCNetworkReachabilityGetFlags(target, &flags))
        return flags;
    return 0;
}

-(void)recheck {
    dispatch_sync(self.queue, ^{
        SCNetworkReachabilityFlags flags = [CKKSReachabilityTracker getReachabilityFlags:self.reachability];
        [self _onqueueRecheck:flags];
    });
}

-(bool)isNetworkError:(NSError *)error {
    if (error == nil)
        return false;
    return ([error.domain isEqualToString:CKErrorDomain] &&
            (error.code == CKErrorNetworkUnavailable
             || error.code == CKErrorNetworkFailure));
}

@end

#endif // OCTAGON

