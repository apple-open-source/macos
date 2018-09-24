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

#if __OBJC2__

#import "SFAnalyticsActivityTracker.h"
#import "SFAnalyticsActivityTracker+Internal.h"
#import "SFAnalytics.h"
#import <mach/mach_time.h>
#import "utilities/debugging.h"

@implementation SFAnalyticsActivityTracker {
    dispatch_queue_t _queue;
    NSString* _name;
    Class _clientClass;
    NSNumber* _measurement;
    uint64_t _start;
    BOOL _canceled;
}

- (instancetype)initWithName:(NSString*)name clientClass:(Class)className {
    if (![name isKindOfClass:[NSString class]] || ![className isSubclassOfClass:[SFAnalytics class]] ) {
        secerror("Cannot instantiate SFActivityTracker without name and client class");
        return nil;
    }

    if (self = [super init]) {
        _queue = dispatch_queue_create("SFAnalyticsActivityTracker queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
        _name = name;
        _clientClass = className;
        _measurement = nil;
        _canceled = NO;
        _start = 0;
    }
    return self;
}

- (void)performAction:(void (^)(void))action
{
    [self start];
    dispatch_sync(_queue, ^{
        action();
    });
    [self stop];
}

- (void)start
{
    if (_canceled) {
        return;
    }
    NSAssert(_start == 0, @"SFAnalyticsActivityTracker user called start twice");
    _start = mach_absolute_time();
}

- (void)stop
{
    uint64_t end = mach_absolute_time();

    if (_canceled) {
        _start = 0;
        return;
    }
    NSAssert(_start != 0, @"SFAnalyticsActivityTracker user called stop w/o calling start");
    
    static mach_timebase_info_data_t sTimebaseInfo;
    if ( sTimebaseInfo.denom == 0 ) {
        (void)mach_timebase_info(&sTimebaseInfo);
    }

    _measurement = @([_measurement doubleValue] + (1.0f * (end - _start) * (1.0f * sTimebaseInfo.numer / sTimebaseInfo.denom)));
    _start = 0;
}

- (void)cancel
{
    _canceled = YES;
}

- (void)dealloc
{
    if (_start != 0) {
        [self stop];
    }
    if (!_canceled && _measurement != nil) {
        [[_clientClass logger] logMetric:_measurement withName:_name];
    }
}

@end

#endif
