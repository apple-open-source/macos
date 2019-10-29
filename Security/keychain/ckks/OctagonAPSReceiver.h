/*
 * Copyright (c) 2016 Apple Inc. All Rights Reserved.
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
#import <ApplePushService/ApplePushService.h>
#import <CloudKit/CloudKit.h>
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CloudKitDependencies.h"


NS_ASSUME_NONNULL_BEGIN

// APS is giving us a tracingUUID and a tracingEnabled bool, but our interfaces take a CKRecordZoneNotification. Add them to that class, then.
@interface CKRecordZoneNotification (CKKSPushTracing)
@property (nonatomic, assign) BOOL ckksPushTracingEnabled;
@property (nonatomic, strong, nullable) NSString* ckksPushTracingUUID;
@property (nonatomic, strong, nullable) NSDate* ckksPushReceivedDate;
@end

@protocol CKKSZoneUpdateReceiver <NSObject>
- (void)notifyZoneChange:(CKRecordZoneNotification* _Nullable)notification;
@end

@protocol OctagonCuttlefishUpdateReceiver <NSObject>
- (void)notifyContainerChange:(APSIncomingMessage* _Nullable)notification;
@end

@interface OctagonAPSReceiver : NSObject <APSConnectionDelegate>

// class dependencies (for injection)
@property (readonly) Class<OctagonAPSConnection> apsConnectionClass;
@property (nullable) id<OctagonAPSConnection> apsConnection;

@property (readonly) BOOL haveStalePushes;

+ (instancetype)receiverForEnvironment:(NSString*)environmentName
                     namedDelegatePort:(NSString*)namedDelegatePort
                    apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass;

- (CKKSCondition*)registerReceiver:(id<CKKSZoneUpdateReceiver>)receiver forZoneID:(CKRecordZoneID*)zoneID;
- (CKKSCondition*)registerCuttlefishReceiver:(id<OctagonCuttlefishUpdateReceiver>)receiver forContainerName:(NSString*)containerName;

// Test support:
- (instancetype)initWithEnvironmentName:(NSString*)environmentName
                      namedDelegatePort:(NSString*)namedDelegatePort
                     apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass;
// This is the queue that APNS will use send the notifications to us
+ (dispatch_queue_t)apsDeliveryQueue;

// timeout for stale push cache, in nanoseconds
+ (int64_t)stalePushTimeout;

@end

@interface OctagonAPSReceiver (Testing)
+ (void)resetGlobalEnviornmentMap;
@end

NS_ASSUME_NONNULL_END
#endif  // OCTAGON
