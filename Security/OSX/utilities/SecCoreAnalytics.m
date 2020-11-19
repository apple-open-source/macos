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

NSString* const SecCoreAnalyticsValue = @"value";


void SecCoreAnalyticsSendValue(CFStringRef _Nonnull eventName, int64_t value)
{
    [SecCoreAnalytics sendEvent:(__bridge NSString*)eventName
                          event:@{
                              SecCoreAnalyticsValue: [NSNumber numberWithLong:value],
                          }];
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
