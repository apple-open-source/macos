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
#import <Security/SFAnalyticsSampler.h>
#import <Security/SFAnalyticsMultiSampler.h>
#import <Security/SFAnalyticsActivityTracker.h>

NS_ASSUME_NONNULL_BEGIN

// this sampling interval will cause the sampler to run only at data reporting time
extern const NSTimeInterval SFAnalyticsSamplerIntervalOncePerReport;

typedef NS_ENUM(uint32_t, SFAnalyticsTimestampBucket) {
    SFAnalyticsTimestampBucketSecond = 0,
    SFAnalyticsTimestampBucketMinute = 1,
    SFAnalyticsTimestampBucketHour = 2,
};

@protocol SFAnalyticsProtocol <NSObject>
+ (id<SFAnalyticsProtocol> _Nullable)logger;

- (void)logResultForEvent:(NSString*)eventName
              hardFailure:(bool)hardFailure
                   result:(NSError* _Nullable)eventResultError;
- (void)logResultForEvent:(NSString*)eventName
              hardFailure:(bool)hardFailure
                   result:(NSError* _Nullable)eventResultError
           withAttributes:(NSDictionary* _Nullable)attributes;

- (SFAnalyticsMultiSampler* _Nullable)AddMultiSamplerForName:(NSString *)samplerName
                                            withTimeInterval:(NSTimeInterval)timeInterval
                                                       block:(NSDictionary<NSString *,NSNumber *> *(^)(void))block;

- (SFAnalyticsActivityTracker* _Nullable)logSystemMetricsForActivityNamed:(NSString*)eventName
                                                               withAction:(void (^ _Nullable)(void))action;
- (SFAnalyticsActivityTracker* _Nullable)startLogSystemMetricsForActivityNamed:(NSString *)eventName;
@end

@interface SFAnalytics : NSObject <SFAnalyticsProtocol>

+ (instancetype _Nullable)logger;

+ (NSInteger)fuzzyDaysSinceDate:(NSDate*)date;
+ (void)addOSVersionToEvent:(NSMutableDictionary*)event;
// Help for the subclass to pick a prefered location
+ (NSString *)defaultAnalyticsDatabasePath:(NSString *)basename;

+ (NSString *)defaultProtectedAnalyticsDatabasePath:(NSString *)basename uuid:(NSUUID * __nullable)userUuid;
+ (NSString *)defaultProtectedAnalyticsDatabasePath:(NSString *)basename; // uses current user UUID for path

- (void)dailyCoreAnalyticsMetrics:(NSString *)eventName;

// Log event-based metrics: create an event corresponding to some event in your feature
// and call the appropriate method based on the successfulness of that event
- (void)logSuccessForEventNamed:(NSString*)eventName;
- (void)logSuccessForEventNamed:(NSString*)eventName timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket;

- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary* _Nullable)attributes;
- (void)logHardFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary* _Nullable)attributes timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket;

- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary* _Nullable)attributes;
- (void)logSoftFailureForEventNamed:(NSString*)eventName withAttributes:(NSDictionary* _Nullable)attributes timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket;

// or just log an event if it is not failable
- (void)noteEventNamed:(NSString*)eventName;
- (void)noteEventNamed:(NSString*)eventName timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket;

- (void)logResultForEvent:(NSString*)eventName
              hardFailure:(bool)hardFailure
                   result:(NSError* _Nullable)eventResultError;
- (void)logResultForEvent:(NSString*)eventName
              hardFailure:(bool)hardFailure
                   result:(NSError* _Nullable)eventResultError
          timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket;
- (void)logResultForEvent:(NSString*)eventName
              hardFailure:(bool)hardFailure
                   result:(NSError* _Nullable)eventResultError
           withAttributes:(NSDictionary* _Nullable)attributes;
- (void)logResultForEvent:(NSString*)eventName
              hardFailure:(bool)hardFailure
                   result:(NSError* _Nullable)eventResultError
           withAttributes:(NSDictionary* _Nullable)attributes
          timestampBucket:(SFAnalyticsTimestampBucket)timestampBucket;

// Track the state of a named value over time
- (SFAnalyticsSampler* _Nullable)addMetricSamplerForName:(NSString*)samplerName
                                        withTimeInterval:(NSTimeInterval)timeInterval
                                                   block:(NSNumber* (^)(void))block;
- (SFAnalyticsSampler* _Nullable)existingMetricSamplerForName:(NSString*)samplerName;
- (void)removeMetricSamplerForName:(NSString*)samplerName;
// Same idea, but log multiple named values in a single block
- (SFAnalyticsMultiSampler* _Nullable)AddMultiSamplerForName:(NSString*)samplerName
                                            withTimeInterval:(NSTimeInterval)timeInterval
                                                       block:(NSDictionary<NSString*, NSNumber*>* (^)(void))block;
- (SFAnalyticsMultiSampler*)existingMultiSamplerForName:(NSString*)samplerName;
- (void)removeMultiSamplerForName:(NSString*)samplerName;

// Log measurements of arbitrary things
// System metrics measures how much time it takes to complete the action - possibly more in the future. The return value can be ignored if you only need to execute 1 block for your activity
- (SFAnalyticsActivityTracker* _Nullable)logSystemMetricsForActivityNamed:(NSString*)eventName
                                                               withAction:(void (^ _Nullable)(void))action;

// Same as above, but automatically starts the tracker, since you haven't given it any action to perform
- (SFAnalyticsActivityTracker* _Nullable)startLogSystemMetricsForActivityNamed:(NSString *)eventName;

- (void)logMetric:(NSNumber*)metric withName:(NSString*)metricName;


// --------------------------------
// Things below are for subclasses

// Override to create a concrete logger instance
@property (readonly, class, nullable) NSString* databasePath;

// Storing dates
- (void)setDateProperty:(NSDate* _Nullable)date forKey:(NSString*)key;
- (NSDate* _Nullable)datePropertyForKey:(NSString*)key;

- (void)incrementIntegerPropertyForKey:(NSString*)key;
- (void)setNumberProperty:(NSNumber* _Nullable)number forKey:(NSString*)key;
- (NSNumber * _Nullable)numberPropertyForKey:(NSString*)key;


// --------------------------------
// Things below are for unit testing

- (void)removeState;    // removes DB object and any samplers

@end

NS_ASSUME_NONNULL_END
#endif
#endif
