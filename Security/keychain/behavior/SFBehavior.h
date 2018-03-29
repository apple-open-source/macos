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

#import <Foundation/Foundation.h>

typedef NS_ENUM(uint32_t, SFBehaviorRamping) {
    SFBehaviorRampingDisabled = 0,      /* must not be enabled */
    SFBehaviorRampingEnabled = 1,       /* unconditionally enabled */
    SFBehaviorRampingPromoted = 2,      /* should be promoted by application */
    SFBehaviorRampingVisible = 3,       /* allowed to enabled */
};

@interface SFBehavior : NSObject

+ (SFBehavior *)behaviorFamily:(NSString *)family;
- (instancetype)init NS_UNAVAILABLE;

/*
 * Ramping control controlled by CloudKit and configuration
 *
 *   Return the current ramping state, can be called as often as clients want, state is cached
 *   and fetched in background (returning SFBehaviorRampingDisabled) until server changes the value.
 *
 *   Ramping always go from Disable -> Visiable -> Promoted -> Enabled, can can skip over steps in-between.
 *
 *   The feature can also go from { Visiable, Promoted, Enabled } -> Disabled if the feature is disabled
 *
 *   Passing in force will for fetching the value from the server and bypass all caching, this will
 *   take its sweet time, so don't block UI on this operation, using force is not recommended.
 */
- (SFBehaviorRamping)ramping:(NSString *)feature force:(bool)force;

/*
 *  This feature is assumed to be enabled unless disabled by configuration.
 */
- (bool)featureEnabled:(NSString *)feature;
/*
 * This feature is assumed to be disabled unless enabled by configuration.
 */
- (bool)featureDisabled:(NSString *)feature;

/*
 * Fetch configuration values that might be changed from server configuration
 */
- (NSNumber *)configurationNumber:(NSString *)configuration defaultValue:(NSNumber *)defaultValue;
- (NSString *)configurationString:(NSString *)configuration defaultValue:(NSString *)defaultValue;

@end

#endif
