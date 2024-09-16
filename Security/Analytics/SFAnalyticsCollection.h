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

@class SECSFARule;

NS_ASSUME_NONNULL_BEGIN

@interface SFAnalyticsMatchingRule: NSObject
@property NSString *eventName;
@property (readonly) SECSFARule *rule;
@end

@protocol SFAnalyticsCollectionAction <NSObject>
- (void)autoBugCaptureWithType:(NSString *)type  subType:(NSString *)subType domain:(NSString *)domain;
- (void)tapToRadar:(NSString*)alert
       description:(NSString*)description
             radar:(NSString*)radar
     componentName:(NSString*)componentName
  componentVersion:(NSString*)componentVersion
       componentID:(NSString*)componentID
        attributes:(NSDictionary* _Nullable)attributes;

@end

@interface SFAnalyticsCollection : NSObject

- (instancetype)init;
- (instancetype)initWithActionInterface:(id<SFAnalyticsCollectionAction>)action;

- (void)loadCollection:(SFAnalytics *)logger;
- (void)storeCollection:(NSData * _Nullable)data logger:(SFAnalytics *_Nullable)logger;
- (void)stopMetricCollection;

- (SFAnalyticsMetricsHookActions)match:(NSString*)eventName
                            eventClass:(SFAnalyticsEventClass)eventClass
                            attributes:(NSDictionary*)attributes
                                bucket:(SFAnalyticsTimestampBucket)timestampBucket
                                logger:(SFAnalytics *)logger;

- (NSMutableDictionary<NSString*, NSMutableSet<SFAnalyticsMatchingRule*>*>* _Nullable)parseCollection:(NSData *)data
                                                                                               logger:(SFAnalytics *)logger;

@end

NS_ASSUME_NONNULL_END
