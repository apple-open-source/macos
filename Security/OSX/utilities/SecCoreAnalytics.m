/*
 * Copyright (c) 2018 Apple Inc. All Rights Reserved.
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

#import "SecCoreAnalytics.h"
#import <CoreAnalytics/CoreAnalytics.h>
#import <SoftLinking/SoftLinking.h>
#import <Availability.h>
#import <sys/sysctl.h>
#import <os/log.h>
#import <limits.h>

NSString* const SecCoreAnalyticsValue = @"value";


void SecCoreAnalyticsSendValue(CFStringRef _Nonnull eventName, int64_t value)
{
    [SecCoreAnalytics sendEvent:(__bridge NSString*)eventName
                          event:@{
                              SecCoreAnalyticsValue: [NSNumber numberWithLong:value],
                          }];
}

// Round up to the nearest power of two. If x is a power of two,
// return x. If x is zero, return zero. If x > 2^63, return UINT64_MAX
// (which is technically not a power of two). For example:
//
// ceil2n(0) -> 0
// ceil2n(1) -> 1
// ceil2n(2) -> 2
// ceil2n(3) -> 4
// ceil2n(4) -> 4
// ceil2n(5) -> 8
// ...
static
uint64_t ceil2n(uint64_t x)
{
    if (__builtin_popcountll(x) < 2) {
        return x;
    }

    int clz = __builtin_clzll(x);
    if (clz == 0) {
        return UINT64_MAX;
    }

    return 1ULL << (64 - clz);
}

static
int sysctl_read(const char *name, void *valp, size_t size)
{
    size_t tmp_size = size;
    int rv = sysctlbyname(name, valp, &tmp_size, NULL, 0);

    if (rv < 0) {
        return rv;
    }

    if (tmp_size != size) {
        return -1;
    }

    return rv;
}

#define SYSCTL_READ(type, varname, sysctlname)                          \
    type varname;                                                       \
    if (sysctl_read(sysctlname, &varname, sizeof(varname)) < 0) {       \
        os_log_error(OS_LOG_DEFAULT, "failed to read sysctl %s", sysctlname); \
        return;                                                         \
    }                                                                   \
    while (0)

static
void SecCoreAnalyticsSendKernEntropyHealthAnalytics(void)
{
    SYSCTL_READ(int, startup_done, "kern.entropy.health.startup_done");

    SYSCTL_READ(uint32_t, adaptive_proportion_test_failure_count, "kern.entropy.health.adaptive_proportion_test.failure_count");
    SYSCTL_READ(uint32_t, adaptive_proportion_test_max_observation_count, "kern.entropy.health.adaptive_proportion_test.max_observation_count");
    SYSCTL_READ(uint32_t, adaptive_proportion_test_reset_count, "kern.entropy.health.adaptive_proportion_test.reset_count");
    SYSCTL_READ(uint32_t, repetition_count_test_failure_count, "kern.entropy.health.repetition_count_test.failure_count");
    SYSCTL_READ(uint32_t, repetition_count_test_max_observation_count, "kern.entropy.health.repetition_count_test.max_observation_count");
    SYSCTL_READ(uint32_t, repetition_count_test_reset_count, "kern.entropy.health.repetition_count_test.reset_count");

    // Round up to next power of two.
    adaptive_proportion_test_reset_count = (uint32_t)ceil2n(adaptive_proportion_test_reset_count);

    // Round up to next power of two.
    repetition_count_test_reset_count = (uint32_t)ceil2n(repetition_count_test_reset_count);

    // Default to not submitting uptime, except on failure.
    int uptime = -1;

    if (adaptive_proportion_test_failure_count > 0 || repetition_count_test_failure_count > 0) {
        time_t now;
        time(&now);

        SYSCTL_READ(struct timeval, boottime, "kern.boottime");

        // Submit uptime in minutes.
        uptime = (int)((now - boottime.tv_sec) / 60);
    }

    [SecCoreAnalytics sendEventLazy:@"com.apple.kern.entropyHealth" builder:^NSDictionary<NSString *,NSObject *> * _Nonnull{
        return @{
            @"uptime" : @(uptime),
            @"startup_done" : @(startup_done),
            @"adaptive_proportion_failure_count" : @(adaptive_proportion_test_failure_count),
            @"adaptive_proportion_max_observation_count" : @(adaptive_proportion_test_max_observation_count),
            @"adaptive_proportion_reset_count" : @(adaptive_proportion_test_reset_count),
            @"repetition_failure_count" : @(repetition_count_test_failure_count),
            @"repetition_max_observation_count" : @(repetition_count_test_max_observation_count),
            @"repetition_reset_count" : @(repetition_count_test_reset_count)
        };
    }];
}

static
void SecCoreAnalyticsSendKernEntropyFilterAnalytics(void)
{
    SYSCTL_READ(uint64_t, rejected_sample_count, "kern.entropy.filter.rejected_sample_count");
    SYSCTL_READ(uint64_t, total_sample_count, "kern.entropy.filter.total_sample_count");

    double rejection_rate = (double)rejected_sample_count / total_sample_count;

    total_sample_count = ceil2n(total_sample_count);

    [SecCoreAnalytics sendEventLazy:@"com.apple.kern.entropy.filter" builder:^NSDictionary<NSString *,NSObject *> * _Nonnull{
        return @{
            @"rejection_rate" : @(rejection_rate),
            @"total_sample_count" : @(total_sample_count)
        };
    }];
}

void SecCoreAnalyticsSendKernEntropyAnalytics(void)
{
    SecCoreAnalyticsSendKernEntropyHealthAnalytics();
    SecCoreAnalyticsSendKernEntropyFilterAnalytics();
}

@implementation SecCoreAnalytics

SOFT_LINK_OPTIONAL_FRAMEWORK(PrivateFrameworks, CoreAnalytics);

SOFT_LINK_FUNCTION(CoreAnalytics, AnalyticsSendEvent, soft_AnalyticsSendEvent, \
    void, (NSString* eventName, NSDictionary<NSString*,NSObject*>* eventPayload),(eventName, eventPayload));
SOFT_LINK_FUNCTION(CoreAnalytics, AnalyticsSendEventLazy, soft_AnalyticsSendEventLazy, \
    void, (NSString* eventName, NSDictionary<NSString*,NSObject*>* (^eventPayloadBuilder)(void)),(eventName, eventPayloadBuilder));

+ (void)sendEvent:(NSString*) eventName event:(NSDictionary<NSString*,NSObject*>*)event
{
    if (isCoreAnalyticsAvailable()) {
        soft_AnalyticsSendEvent(eventName, event);
    }
}

+ (void)sendEventLazy:(NSString*) eventName builder:(NSDictionary<NSString*,NSObject*>* (^)(void))builder
{
    if (isCoreAnalyticsAvailable()) {
        soft_AnalyticsSendEventLazy(eventName, builder);
    }
}

@end
