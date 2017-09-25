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

#import <Foundation/Foundation.h>

#if OCTAGON
#import "Analytics/SFAnalyticsLogger.h"

@class CKKSKeychainView;

@protocol CKKSAnalyticsFailableEvent
@end
typedef NSString<CKKSAnalyticsFailableEvent> CKKSAnalyticsFailableEvent;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC;
extern CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges;

@protocol CKKSAnalyticsSignpostEvent
@end
typedef NSString<CKKSAnalyticsSignpostEvent> CKKSAnalyticsSignpostEvent;
extern CKKSAnalyticsSignpostEvent* const CKKSEventPushNotificationReceived;
extern CKKSAnalyticsSignpostEvent* const CKKSEventItemAddedToOutgoingQueue;

@interface CKKSAnalyticsLogger : SFAnalyticsLogger

+ (instancetype)logger;

- (void)logSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view;
- (void)logRecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view;
- (void)logUnrecoverableError:(NSError*)error forEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view;

- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event inView:(CKKSKeychainView*)view;

@end

@interface CKKSAnalyticsLogger (UniteTesting)

- (NSDate*)dateOfLastSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view;

@end

#endif
