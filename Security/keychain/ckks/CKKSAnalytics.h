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
#import "Analytics/SFAnalytics.h"

extern NSString* const CKKSAnalyticsInCircle;
extern NSString* const CKKSAnalyticsHasTLKs;
extern NSString* const CKKSAnalyticsSyncedClassARecently;
extern NSString* const CKKSAnalyticsSyncedClassCRecently;
extern NSString* const CKKSAnalyticsIncomingQueueIsErrorFree;
extern NSString* const CKKSAnalyticsOutgoingQueueIsErrorFree;
extern NSString* const CKKSAnalyticsInSync;
extern NSString* const CKKSAnalyticsValidCredentials;
extern NSString* const CKKSAnalyticsLastUnlock;
extern NSString* const CKKSAnalyticsLastKeystateReady;
extern NSString* const CKKSAnalyticsLastInCircle;

@class CKKSKeychainView;

@protocol CKKSAnalyticsFailableEvent <NSObject>
@end
typedef NSString<CKKSAnalyticsFailableEvent> CKKSAnalyticsFailableEvent;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassA;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessIncomingQueueClassC;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessOutgoingQueue;
extern CKKSAnalyticsFailableEvent* const CKKSEventUploadChanges;
extern CKKSAnalyticsFailableEvent* const CKKSEventStateError;
extern CKKSAnalyticsFailableEvent* const CKKSEventProcessHealKeyHierarchy;

extern CKKSAnalyticsFailableEvent* const OctagonEventPreflightBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventLaunchBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventScrubBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventSignIn;
extern CKKSAnalyticsFailableEvent* const OctagonEventSignOut;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoreBottle;
extern CKKSAnalyticsFailableEvent* const OctagonEventRamp;
extern CKKSAnalyticsFailableEvent* const OctagonEventBottleCheck;
extern CKKSAnalyticsFailableEvent* const OctagonEventCoreFollowUp;

extern CKKSAnalyticsFailableEvent* const OctagonEventRestoredSignedBottlePeer;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoredOctagonPeerEncryptionKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoredOctagonPeerSigningKey;
extern CKKSAnalyticsFailableEvent* const OctagonEventRestoreComplete;


@protocol CKKSAnalyticsSignpostEvent <NSObject>
@end
typedef NSString<CKKSAnalyticsSignpostEvent> CKKSAnalyticsSignpostEvent;
extern CKKSAnalyticsSignpostEvent* const CKKSEventPushNotificationReceived;
extern CKKSAnalyticsSignpostEvent* const CKKSEventItemAddedToOutgoingQueue;
extern CKKSAnalyticsSignpostEvent* const CKKSEventReachabilityTimerExpired;
extern CKKSAnalyticsSignpostEvent* const CKKSEventMissingLocalItemsFound;

@protocol CKKSAnalyticsActivity <NSObject>
@end
typedef NSString<CKKSAnalyticsActivity> CKKSAnalyticsActivity;
extern CKKSAnalyticsActivity* const CKKSActivityOTFetchRampState;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonSignIn;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonPreflightBottle;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonLaunchBottle;
extern CKKSAnalyticsActivity* const CKKSActivityOctagonRestore;
extern CKKSAnalyticsActivity* const CKKSActivityScrubBottle;
extern CKKSAnalyticsActivity* const CKKSActivityBottleCheck;

@interface CKKSAnalytics : SFAnalytics

+ (instancetype)logger;

- (void)logSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view;
- (void)logRecoverableError:(NSError*)error
                   forEvent:(CKKSAnalyticsFailableEvent*)event
                     inView:(CKKSKeychainView*)view
             withAttributes:(NSDictionary*)attributes;

- (void)logRecoverableError:(NSError*)error
                   forEvent:(CKKSAnalyticsFailableEvent*)event
                   zoneName:(NSString*)zoneName
             withAttributes:(NSDictionary *)attributes;


- (void)logUnrecoverableError:(NSError*)error
                     forEvent:(CKKSAnalyticsFailableEvent*)event
               withAttributes:(NSDictionary *)attributes;

- (void)logUnrecoverableError:(NSError*)error
                     forEvent:(CKKSAnalyticsFailableEvent*)event
                       inView:(CKKSKeychainView*)view
               withAttributes:(NSDictionary*)attributes;

- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event;
- (void)noteEvent:(CKKSAnalyticsSignpostEvent*)event inView:(CKKSKeychainView*)view;

- (void)setDateProperty:(NSDate*)date forKey:(NSString*)key inView:(CKKSKeychainView *)view;
- (NSDate *)datePropertyForKey:(NSString *)key inView:(CKKSKeychainView *)view;

@end

@interface CKKSAnalytics (UnitTesting)

- (NSDate*)dateOfLastSuccessForEvent:(CKKSAnalyticsFailableEvent*)event inView:(CKKSKeychainView*)view;
- (NSDictionary *)errorChain:(NSError *)error depth:(NSUInteger)depth;

@end

#endif


