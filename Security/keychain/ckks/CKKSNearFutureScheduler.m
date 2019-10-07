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

#import "CKKSNearFutureScheduler.h"
#import "CKKSCondition.h"
#import "keychain/ckks/NSOperationCategories.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ot/ObjCImprovements.h"
#include <os/transaction_private.h>

@interface CKKSNearFutureScheduler ()
@property NSString* name;
@property dispatch_time_t initialDelay;
@property dispatch_time_t continuingDelay;

@property NSInteger operationDependencyDescriptionCode;
@property CKKSResultOperation* operationDependency;
@property (nonnull) NSOperationQueue* operationQueue;

@property NSDate* predictedNextFireTime;
@property bool liveRequest;
@property CKKSCondition* liveRequestReceived; // Triggered when liveRequest goes to true.

@property dispatch_source_t timer;
@property dispatch_queue_t queue;

@property bool keepProcessAlive;
@property os_transaction_t transaction;
@end

@implementation CKKSNearFutureScheduler

-(instancetype)initWithName:(NSString*)name
                      delay:(dispatch_time_t)ns
           keepProcessAlive:(bool)keepProcessAlive
  dependencyDescriptionCode:(NSInteger)code
                      block:(void (^)(void))futureBlock
{
    return [self initWithName:name
                 initialDelay:ns
              continuingDelay:ns
             keepProcessAlive:keepProcessAlive
    dependencyDescriptionCode:code
                        block:futureBlock];
}

-(instancetype)initWithName:(NSString*)name
               initialDelay:(dispatch_time_t)initialDelay
            continuingDelay:(dispatch_time_t)continuingDelay
           keepProcessAlive:(bool)keepProcessAlive
  dependencyDescriptionCode:(NSInteger)code
                      block:(void (^)(void))futureBlock
{
    if((self = [super init])) {
        _name = name;

        _queue = dispatch_queue_create([[NSString stringWithFormat:@"near-future-scheduler-%@",name] UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _initialDelay = initialDelay;
        _continuingDelay = continuingDelay;
        _futureBlock = futureBlock;

        _liveRequest = false;
        _liveRequestReceived = [[CKKSCondition alloc] init];
        _predictedNextFireTime = nil;

        _keepProcessAlive = keepProcessAlive;

        _operationQueue = [[NSOperationQueue alloc] init];
        _operationDependencyDescriptionCode = code;
        _operationDependency = [self makeOperationDependency];
    }
    return self;
}

- (void)changeDelays:(dispatch_time_t)initialDelay continuingDelay:(dispatch_time_t)continuingDelay
{
    dispatch_sync(self.queue, ^{
        self.initialDelay = initialDelay;
        self.continuingDelay = continuingDelay;
    });
}

- (CKKSResultOperation*)makeOperationDependency {
    CKKSResultOperation* op = [CKKSResultOperation named:[NSString stringWithFormat:@"nfs-%@", self.name] withBlock:^{}];
    op.descriptionErrorCode = self.operationDependencyDescriptionCode;
    return op;
}

-(NSString*)description {
    NSDate* nextAt = self.nextFireTime;
    if(nextAt) {
        NSDateFormatter* dateFormatter = [[NSDateFormatter alloc] init];
        [dateFormatter setDateFormat:@"yyyy-MM-dd HH:mm:ss"];
        return [NSString stringWithFormat: @"<CKKSNearFutureScheduler(%@): next at %@", self.name, [dateFormatter stringFromDate: nextAt]];
    } else {
        return [NSString stringWithFormat: @"<CKKSNearFutureScheduler(%@): no pending attempts", self.name];
    }
}

- (NSDate*)nextFireTime {
    // If we have a live request, send the next fire time back. Otherwise, wait a tiny tiny bit to see if we receive a request.
    if(self.liveRequest) {
        return self.predictedNextFireTime;
    } else if([self.liveRequestReceived wait:50*NSEC_PER_USEC] == 0) {
        return self.predictedNextFireTime;
    }

    return nil;
}

-(void)waitUntil:(uint64_t)delay {
    dispatch_sync(self.queue, ^{
        [self _onqueueTrigger:delay maximumDelay:DISPATCH_TIME_FOREVER];
    });
}

- (void)triggerAt:(uint64_t)delay {
    WEAKIFY(self);
    dispatch_async(self.queue, ^{
        STRONGIFY(self);
        self.liveRequest = true;
        [self.liveRequestReceived fulfill];
        [self _onqueueTrigger:(delay == DISPATCH_TIME_FOREVER ? DISPATCH_TIME_NOW : delay) maximumDelay:delay];
    });
}

-(void)_onqueueTimerTick {
    dispatch_assert_queue(self.queue);

    if(self.liveRequest) {
        // Put a new dependency in place, and save the old one for execution
        NSOperation* dependency = self.operationDependency;
        self.operationDependency = [self makeOperationDependency];

        self.futureBlock();
        self.liveRequest = false;
        self.liveRequestReceived = [[CKKSCondition alloc] init];
        self.transaction = nil;

        [self.operationQueue addOperation: dependency];

        self.predictedNextFireTime = [NSDate dateWithTimeIntervalSinceNow: (NSTimeInterval) ((double) self.continuingDelay) / (double) NSEC_PER_SEC];
    } else {
        // The timer has fired with no requests to call the block. Cancel it.
        dispatch_source_cancel(self.timer);
        self.predictedNextFireTime = nil;
    }
}

-(void)trigger {
    WEAKIFY(self);
    dispatch_async(self.queue, ^{
        STRONGIFY(self);
        // The timer tick should call the block!
        self.liveRequest = true;
        [self.liveRequestReceived fulfill];

        [self _onqueueTrigger:DISPATCH_TIME_NOW maximumDelay:DISPATCH_TIME_FOREVER];
    });
}

-(void)_onqueueTrigger:(dispatch_time_t)requestedDelay maximumDelay:(dispatch_time_t)maximumDelay {
    dispatch_assert_queue(self.queue);
    WEAKIFY(self);

    // If we don't have one already, set up an os_transaction
    if(self.keepProcessAlive && self.transaction == nil) {
        self.transaction = os_transaction_create([[NSString stringWithFormat:@"com.apple.securityd.%@",self.name] UTF8String]);
    }

    if(requestedDelay != DISPATCH_TIME_NOW && self.predictedNextFireTime != nil) {
        NSDate* delayTime = [NSDate dateWithTimeIntervalSinceNow: (NSTimeInterval) ((double) requestedDelay) / (double) NSEC_PER_SEC];
        if([delayTime compare:self.predictedNextFireTime] != NSOrderedDescending) {
            // The next fire time is after this delay. Do nothing with the request.
        } else {
            // Need to cancel the timer and reset it below.
            dispatch_source_cancel(self.timer);
            self.predictedNextFireTime = nil;
        }
    }

    if(maximumDelay != DISPATCH_TIME_FOREVER && self.predictedNextFireTime != nil) {
        NSDate* delayTime = [NSDate dateWithTimeIntervalSinceNow: (NSTimeInterval) ((double) requestedDelay) / (double) NSEC_PER_SEC];
        if([delayTime compare:self.predictedNextFireTime] != NSOrderedDescending) {
            // Need to cancel the timer and reset it below.
            dispatch_source_cancel(self.timer);
            self.predictedNextFireTime = nil;
        } else {
            // The next fire time is before the maximum delay. Do nothing with the request.
        }
    }

    // Check if the timer is alive
    if(self.timer != nil && 0 == dispatch_source_testcancel(self.timer)) {
        // timer is alive, do nothing
    } else {
        // start the timer
        self.timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                        0,
                                        (dispatch_source_timer_flags_t)0,
                                        self.queue);
        dispatch_source_set_event_handler(self.timer, ^{
            STRONGIFY(self);
            [self _onqueueTimerTick];
        });

        dispatch_time_t actualDelay = self.initialDelay;
        if(requestedDelay != DISPATCH_TIME_NOW) {
            actualDelay = MAX(actualDelay, requestedDelay);
        }
        if(maximumDelay != DISPATCH_TIME_FOREVER) {
            actualDelay = MIN(actualDelay, maximumDelay);
        }

        dispatch_source_set_timer(self.timer,
                                  dispatch_walltime(NULL, actualDelay),
                                  self.continuingDelay,
                                  50 * NSEC_PER_MSEC);
        dispatch_resume(self.timer);

        self.predictedNextFireTime = [NSDate dateWithTimeIntervalSinceNow: (NSTimeInterval) ((double) actualDelay) / (double) NSEC_PER_SEC];
    };
}

-(void)cancel {
    dispatch_sync(self.queue, ^{
            if(self.timer != nil && 0 == dispatch_source_testcancel(self.timer)) {
                dispatch_source_cancel(self.timer);
            }
    });
}

@end

#endif // OCTAGON
