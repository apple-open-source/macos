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

#import "SecdWatchdog.h"
#include <utilities/debugging.h>
#include <xpc/private.h>

#if !TARGET_OS_MAC
#import <CrashReporterSupport/CrashReporterSupport.h>
#endif

#define CPU_RUNTIME_SECONDS_BEFORE_WATCHDOG (60 * 20)
#define WATCHDOG_RESET_PERIOD (60 * 60 * 24)
#define WATCHDOG_CHECK_PERIOD (60 * 60)
#define WATCHDOG_CHECK_PERIOD_LEEWAY (60 * 10)
#define WATCHDOG_GRACEFUL_EXIT_LEEWAY (60 * 5)

NSString* const SecdWatchdogAllowedRuntime = @"allowed-runtime";
NSString* const SecdWatchdogResetPeriod = @"reset-period";
NSString* const SecdWatchdogCheckPeriod = @"check-period";
NSString* const SecdWatchdogGracefulExitTime = @"graceful-exit-time";

void SecdLoadWatchDog()
{
    (void)[SecdWatchdog watchdog];
}

@implementation SecdWatchdog {
    long _rusageBaseline;
    CFTimeInterval _lastCheckTime;
    dispatch_source_t _timer;

    long _runtimeSecondsBeforeWatchdog;
    long _resetPeriod;
    long _checkPeriod;
    long _checkPeriodLeeway;
    long _gracefulExitLeeway;
}

+ (instancetype)watchdog
{
    static SecdWatchdog* watchdog = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        watchdog = [[self alloc] init];
    });

    return watchdog;
}

- (instancetype)init
{
    if (self = [super init]) {
        _runtimeSecondsBeforeWatchdog = CPU_RUNTIME_SECONDS_BEFORE_WATCHDOG;
        _resetPeriod = WATCHDOG_RESET_PERIOD;
        _checkPeriod = WATCHDOG_CHECK_PERIOD;
        _checkPeriodLeeway = WATCHDOG_CHECK_PERIOD_LEEWAY;
        _gracefulExitLeeway = WATCHDOG_GRACEFUL_EXIT_LEEWAY;

        [self activateTimer];
    }

    return self;
}

- (void)activateTimer
{
    @synchronized (self) {
        struct rusage initialRusage;
        getrusage(RUSAGE_SELF, &initialRusage);
        _rusageBaseline = initialRusage.ru_utime.tv_sec;
        _lastCheckTime = CFAbsoluteTimeGetCurrent();

        __weak __typeof(self) weakSelf = self;
        _timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
        dispatch_source_set_timer(_timer, DISPATCH_TIME_NOW, _checkPeriod * NSEC_PER_SEC, _checkPeriodLeeway * NSEC_PER_SEC); // run once every hour, but give the timer lots of leeway to be power friendly
        dispatch_source_set_event_handler(_timer, ^{
            __strong __typeof(self) strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }

            struct rusage currentRusage;
            getrusage(RUSAGE_SELF, &currentRusage);

            @synchronized (self) {
                if (currentRusage.ru_utime.tv_sec > strongSelf->_rusageBaseline + strongSelf->_runtimeSecondsBeforeWatchdog) {
                    seccritical("SecWatchdog: watchdog has detected securityd/secd is using too much CPU - attempting to exit gracefully");
#if !TARGET_OS_MAC
                    WriteStackshotReport(@"securityd watchdog triggered", __sec_exception_code_Watchdog);
#endif
                    xpc_transaction_exit_clean(); // we've  used too much CPU - try to exit gracefully

                    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(strongSelf->_gracefulExitLeeway * NSEC_PER_SEC)), dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0), ^{
                        // if we still haven't exited gracefully after 5 minutes, time to die unceremoniously
                        seccritical("SecWatchdog: watchdog has failed to exit securityd/secd gracefully - exiting ungracefully");
                        exit(EXIT_FAILURE);
                    });
                }
                else {
                    CFTimeInterval currentTime = CFAbsoluteTimeGetCurrent();
                    if (currentTime > strongSelf->_lastCheckTime + strongSelf->_resetPeriod) {
                        // we made it through a 24 hour period - reset our timeout to 24 hours from now, and our cpu usage threshold to another 20 minutes
                        secinfo("SecWatchdog", "resetting watchdog monitoring interval ahead another 24 hours");
                        strongSelf->_lastCheckTime = currentTime;
                        strongSelf->_rusageBaseline = currentRusage.ru_utime.tv_sec;
                    }
                }
            }
        });
        dispatch_resume(_timer);
    }
}

- (NSDictionary*)watchdogParameters
{
    @synchronized (self) {
        return @{ SecdWatchdogAllowedRuntime : @(_runtimeSecondsBeforeWatchdog),
                  SecdWatchdogResetPeriod : @(_resetPeriod),
                  SecdWatchdogCheckPeriod : @(_checkPeriod),
                  SecdWatchdogGracefulExitTime : @(_gracefulExitLeeway) };
    }
}

- (BOOL)setWatchdogParameters:(NSDictionary*)parameters error:(NSError**)error
{
    NSMutableArray* failedParameters = [NSMutableArray array];
    @synchronized (self) {
        __weak __typeof(self) weakSelf = self;
        [parameters enumerateKeysAndObjectsUsingBlock:^(NSString* parameter, NSNumber* value, BOOL* stop) {
            __strong __typeof(self) strongSelf = weakSelf;
            if (!strongSelf) {
                return;
            }

            if ([parameter isEqualToString:SecdWatchdogAllowedRuntime] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_runtimeSecondsBeforeWatchdog = value.longValue;
            }
            else if ([parameter isEqualToString:SecdWatchdogResetPeriod] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_resetPeriod = value.longValue;
            }
            else if ([parameter isEqualToString:SecdWatchdogCheckPeriod] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_checkPeriod = value.longValue;
            }
            else if ([parameter isEqualToString:SecdWatchdogGracefulExitTime] && [value isKindOfClass:[NSNumber class]]) {
                strongSelf->_gracefulExitLeeway = value.longValue;
            }
            else {
                [failedParameters addObject:parameter];
            }
        }];

        dispatch_source_cancel(_timer);
        _timer = NULL;
    }

    [self activateTimer];

    if (failedParameters.count > 0) {
        if (error) {
            *error = [NSError errorWithDomain:@"com.apple.securityd.watchdog" code:0 userInfo:@{NSLocalizedDescriptionKey : [NSString stringWithFormat:@"failed to set parameters: %@", failedParameters]}];
        }

        return NO;
    }
    else {
        return YES;
    }
}

@end
