/*
 * Copyright (c) 2022 Apple Inc. All Rights Reserved.
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

#import <Foundation/Foundation.h>

@class SECSFAEventRule;
@class SECSFAVersion;

NS_ASSUME_NONNULL_BEGIN

@interface SFAnalyticsMatchingRule: NSObject
@property NSString *eventName;
@property (readonly) SECSFAEventRule *rule;
+ (NSString *)armKeyForEventName:(NSString *)eventName;
@end

@protocol SFAnalyticsCollectionAction <NSObject>
- (BOOL)shouldRatelimit:(SFAnalytics*)logger rule:(SFAnalyticsMatchingRule*)rule;
- (void)autoBugCaptureWithType:(NSString *)type  subType:(NSString *)subType domain:(NSString *)domain;
- (void)tapToRadar:(NSString*)alert
       description:(NSString*)description
             radar:(NSString*)radar
     componentName:(NSString*)componentName
  componentVersion:(NSString*)componentVersion
       componentID:(NSString*)componentID
        attributes:(NSDictionary* _Nullable)attributes;

@end

typedef NSMutableDictionary<NSString*, NSMutableSet<SFAnalyticsMatchingRule*>*> SFAMatchingRules;

@interface SecSFAParsedCollection: NSObject
@property SFAMatchingRules *matchingRules;
@property NSMutableDictionary<NSString*,NSNumber*>* allowedEvents;
@property (readwrite) BOOL excludedVersion;
@end

@interface SFAnalyticsCollection : NSObject

- (instancetype)init;
- (instancetype)initWithActionInterface:(id<SFAnalyticsCollectionAction>)action
                                product:(NSString *)product
                                  build:(NSString *)build;

- (void)loadCollection:(SFAnalytics *)logger;
- (void)storeCollection:(NSData * _Nullable)data logger:(SFAnalytics *_Nullable)logger;
- (void)stopMetricCollection;

+ (SECSFAVersion *_Nullable)parseVersion:(NSString *)build platform:(NSString *)platform;


- (SFAnalyticsMetricsHookActions)match:(NSString*)eventName
                            eventClass:(SFAnalyticsEventClass)eventClass
                            attributes:(NSDictionary*)attributes
                                bucket:(SFAnalyticsTimestampBucket)timestampBucket
                                logger:(SFAnalytics *)logger;

- (SecSFAParsedCollection *_Nullable)parseCollection:(NSData *)data
                                        logger:(SFAnalytics *)logger;

// only for testing
@property (readonly) BOOL excludedVersion;
@property (readonly) SFAMatchingRules *matchingRules;
@property NSString *processName;
- (void)drainSetupQueue;

@end

NS_ASSUME_NONNULL_END
