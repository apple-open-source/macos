/*
 * Copyright (c) 2023 Apple Inc. All Rights Reserved.
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

NS_ASSUME_NONNULL_BEGIN

@interface AAFAnalyticsEventSecurity : NSObject

@property dispatch_queue_t queue;

- (instancetype)initWithKeychainCircleMetrics:(NSDictionary * _Nullable)metrics
                                      altDSID:(NSString * _Nullable)altDSID
                                       flowID:(NSString * _Nullable)flowID
                              deviceSessionID:(NSString * _Nullable)deviceSessionID
                                    eventName:(NSString *)eventName
                              testsAreEnabled:(BOOL)testsAreEnabled
                               canSendMetrics:(BOOL)canSendMetrics
                                     category:(NSNumber *)category;

- (instancetype)initWithCKKSMetrics:(NSDictionary * _Nullable)metrics
                            altDSID:(NSString *)altDSID
                          eventName:(NSString *)eventName
                    testsAreEnabled:(BOOL)testsAreEnabled
                           category:(NSNumber *)category
                         sendMetric:(BOOL)sendMetric;

- (instancetype)initWithKeychainCircleMetrics:(NSDictionary * _Nullable)metrics
                                      altDSID:(NSString * _Nullable)altDSID
                                    eventName:(NSString *)eventName
                                     category:(NSNumber *)category;
- (id)getEvent;
- (void)addMetrics:(NSDictionary*)metrics;
- (void)populateUnderlyingErrorsStartingWithRootError:(NSError* _Nullable)error;
- (BOOL)permittedToSendMetrics;

@end

NS_ASSUME_NONNULL_END
