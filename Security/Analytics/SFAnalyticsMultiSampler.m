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

#import "SFAnalyticsMultiSampler+Internal.h"
#import "SFAnalytics+Internal.h"
#import "SFAnalyticsDefines.h"
#import "utilities/debugging.h"
#include <notify.h>
#include <dispatch/dispatch.h>

@implementation SFAnalyticsMultiSampler {
    NSTimeInterval _samplingInterval;
    dispatch_source_t _timer;
    NSString* _name;
    MultiSamplerDictionary (^_block)(void);
    int _notificationToken;
    Class _clientClass;
    BOOL _oncePerReport;
    BOOL _activeTimer;
}

@synthesize name = _name;
@synthesize samplingInterval = _samplingInterval;
@synthesize oncePerReport = _oncePerReport;

- (instancetype)initWithName:(NSString*)name interval:(NSTimeInterval)interval block:(MultiSamplerDictionary (^)(void))block clientClass:(Class)clientClass
{
    if (self = [super init]) {
        if (![clientClass isSubclassOfClass:[SFAnalytics class]]) {
            secerror("SFAnalyticsSampler created without valid client class (%@)", clientClass);
            return nil;
        }
        
        if (!name || (interval < 1.0f && interval != SFAnalyticsSamplerIntervalOncePerReport) || !block) {
            secerror("SFAnalyticsSampler created without proper data");
            return nil;
        }
        
        _clientClass = clientClass;
        _block = block;
        _name = name;
        _samplingInterval = interval;
        [self newTimer];
    }
    return self;
}

- (void)newTimer
{
    if (_activeTimer) {
        [self pauseSampling];
    }

    _oncePerReport = (_samplingInterval == SFAnalyticsSamplerIntervalOncePerReport);
    if (_oncePerReport) {
        [self setupOnceTimer];
    } else {
        [self setupPeriodicTimer];
    }
}

- (void)setupOnceTimer
{
    __weak __typeof(self) weakSelf = self;
    notify_register_dispatch(SFAnalyticsFireSamplersNotification, &_notificationToken, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(int token) {
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secnotice("SFAnalyticsSampler", "sampler went away before we could run its once-per-report block");
            notify_cancel(token);
            return;
        }

        MultiSamplerDictionary data = strongSelf->_block();
        [data enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSNumber * _Nonnull obj, BOOL * _Nonnull stop) {
            [[strongSelf->_clientClass logger] logMetric:obj withName:key oncePerReport:strongSelf->_oncePerReport];
        }];
    });
    _activeTimer = YES;
}

- (void)setupPeriodicTimer
{
    _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
    dispatch_source_set_timer(_timer, dispatch_walltime(0, _samplingInterval * NSEC_PER_SEC), _samplingInterval * NSEC_PER_SEC, _samplingInterval * NSEC_PER_SEC / 50.0);    // give 2% leeway on timer

    __weak __typeof(self) weakSelf = self;
    dispatch_source_set_event_handler(_timer, ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if (!strongSelf) {
            secnotice("SFAnalyticsSampler", "sampler went away before we could run its once-per-report block");
            return;
        }

        MultiSamplerDictionary data = strongSelf->_block();
        [data enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSNumber * _Nonnull obj, BOOL * _Nonnull stop) {
            [[strongSelf->_clientClass logger] logMetric:obj withName:key oncePerReport:strongSelf->_oncePerReport];
        }];
    });
    dispatch_resume(_timer);
    
    _activeTimer = YES;
}

- (void)setSamplingInterval:(NSTimeInterval)interval
{
    if (interval < 1.0f && !(interval == SFAnalyticsSamplerIntervalOncePerReport)) {
        secerror("SFAnalyticsSampler: interval %f is not supported", interval);
        return;
    }

    _samplingInterval = interval;
    [self newTimer];
}

- (NSTimeInterval)samplingInterval {
    return _samplingInterval;
}

- (MultiSamplerDictionary)sampleNow
{
    MultiSamplerDictionary data = _block();
    [data enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSNumber * _Nonnull obj, BOOL * _Nonnull stop) {
        [[self->_clientClass logger] logMetric:obj withName:key oncePerReport:self->_oncePerReport];
    }];
    return data;
}

- (void)pauseSampling
{
    if (!_activeTimer) {
        return;
    }

    if (_oncePerReport) {
        notify_cancel(_notificationToken);
        _notificationToken = 0;
    } else {
        dispatch_source_cancel(_timer);
    }
    _activeTimer = NO;
}

- (void)resumeSampling
{
    [self newTimer];
}

- (void)dealloc
{
    [self pauseSampling];
}

@end

#endif
