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
#ifndef SFAnalytics_h
#define SFAnalytics_h

#import <Foundation/Foundation.h>
#import "SFAnalyticsSampler.h"
#import "SFAnalyticsMultiSampler.h"
#import "SFAnalyticsActivityTracker.h"

// this sampling interval will cause the sampler to run only at data reporting time
extern const NSTimeInterval SFAnalyticsSamplerIntervalOncePerReport;

@interface SFAnalytics : NSObject

+ (instancetype)logger;

+ (NSInteger)fuzzyDaysSinceDate:(NSDate*)date;
+ (void)addOSVersionToEvent:(NSMutableDictionary*)event;
// Help for the subclass to pick a prefered location
+ (NSString *)defaultAnalyticsDatabasePath:(NSString *)basename;

// Log event-based metrics: create an event corresponding to some event in your feature
// and call the appropriate method based on the successfulness of that event
- (void)logSuccessForEventNamed:(NSString*)eventName;
- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes;
- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary*)attributes;
// or just log an event if it is not failable
- (void)noteEventNamed:(NSString*)eventName;

- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError;
- (void)logResultForEvent:(NSString*)eventName hardFailure:(bool)hardFailure result:(NSError*)eventResultError withAttributes:(NSDictionary*)attributes;

// Track the state of a named value over time
- (SFAnalyticsSampler*)addMetricSamplerForName:(NSString*)samplerName withTimeInterval:(NSTimeInterval)timeInterval block:(NSNumber* (^)(void))block;
- (SFAnalyticsSampler*)existingMetricSamplerForName:(NSString*)samplerName;
- (void)removeMetricSamplerForName:(NSString*)samplerName;
// Same idea, but log multiple named values in a single block
- (SFAnalyticsMultiSampler*)AddMultiSamplerForName:(NSString*)samplerName withTimeInterval:(NSTimeInterval)timeInterval block:(NSDictionary<NSString*, NSNumber*>* (^)(void))block;
- (SFAnalyticsMultiSampler*)existingMultiSamplerForName:(NSString*)samplerName;
- (void)removeMultiSamplerForName:(NSString*)samplerName;

// Log measurements of arbitrary things
// System metrics measures how much time it takes to complete the action - possibly more in the future. The return value can be ignored if you only need to execute 1 block for your activity
- (SFAnalyticsActivityTracker*)logSystemMetricsForActivityNamed:(NSString*)eventName withAction:(void (^)(void))action;
- (void)logMetric:(NSNumber*)metric withName:(NSString*)metricName;



// --------------------------------
// Things below are for subclasses

// Override to create a concrete logger instance
@property (readonly, class) NSString* databasePath;

// Storing dates
- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key;
- (NSDate*)datePropertyForKey:(NSString*)key;

// --------------------------------
// Things below are for unit testing

- (void)removeState;    // removes DB object and any samplers

@end

#endif
#endif
