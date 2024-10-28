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

#ifndef SFAnalytics_Internal_h
#define SFAnalytics_Internal_h

#if __OBJC2__

#import "SFAnalytics.h"

NS_ASSUME_NONNULL_BEGIN

@class SFAnalyticsSQLiteStore;

@interface SFAnalytics (Internal)

- (void)logMetric:(NSNumber*)metric withName:(NSString*)metricName oncePerReport:(BOOL)once;
+ (NSString*)hwModelID;

+ (NSString * _Nullable)underlyingErrors:(NSError *_Nullable)error;

- (void)drainLogQueue;

- (void)setDataProperty:(NSData* _Nullable)data forKey:(NSString*)key;
- (NSData* _Nullable)dataPropertyForKey:(NSString*)key;


@property (nonatomic, nullable) SFAnalyticsSQLiteStore* database;
@property (nonatomic) dispatch_queue_t queue;

@end

NS_ASSUME_NONNULL_END

#endif // objc2
#endif /* SFAnalytics_Internal_h */
