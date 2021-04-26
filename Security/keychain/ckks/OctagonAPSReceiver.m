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

#if OCTAGON

#import <objc/runtime.h>

#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/analytics/SecMetrics.h"
#import "keychain/analytics/SecEventMetric.h"
#import "keychain/ot/ObjCImprovements.h"
#import <CloudKit/CloudKit_Private.h>
#include <utilities/SecAKSWrappers.h>

@implementation CKRecordZoneNotification (CKKSPushTracing)
- (void)setCkksPushTracingEnabled:(BOOL)ckksPushTracingEnabled {
    objc_setAssociatedObject(self, "ckksPushTracingEnabled", ckksPushTracingEnabled ? @YES : @NO, OBJC_ASSOCIATION_RETAIN);
}

- (BOOL)ckksPushTracingEnabled {
    return !![objc_getAssociatedObject(self, "ckksPushTracingEnabled") boolValue];
}

- (void)setCkksPushTracingUUID:(NSString*)ckksPushTracingUUID {
    objc_setAssociatedObject(self, "ckksPushTracingUUID", ckksPushTracingUUID, OBJC_ASSOCIATION_RETAIN);
}

- (NSString*)ckksPushTracingUUID {
    return objc_getAssociatedObject(self, "ckksPushTracingUUID");
}

- (void)setCkksPushReceivedDate:(NSDate*)ckksPushReceivedDate {
    objc_setAssociatedObject(self, "ckksPushReceivedDate", ckksPushReceivedDate, OBJC_ASSOCIATION_RETAIN);
}

- (NSDate*)ckksPushReceivedDate {
    return objc_getAssociatedObject(self, "ckksPushReceivedDate");
}
@end


@interface OctagonAPSReceiver()

@property CKKSNearFutureScheduler *clearStalePushNotifications;

@property NSString* namedDelegatePort;
@property NSMutableDictionary<NSString*, id<OctagonAPSConnection>>* environmentMap;


// If we receive notifications for a record zone that hasn't been registered yet, send them a their updates when they register
@property NSMutableSet<CKRecordZoneNotification*>* undeliveredUpdates;

// Same, but for cuttlefish containers (and only remember that a push was received; don't remember the pushes themselves)
@property NSMutableSet<NSString*>* undeliveredCuttlefishUpdates;

@property (nullable) id<CKKSZoneUpdateReceiverProtocol> zoneUpdateReceiver;
@property NSMapTable<NSString*, id<OctagonCuttlefishUpdateReceiver>>* octagonContainerMap;
@end

@implementation OctagonAPSReceiver

+ (instancetype)receiverForNamedDelegatePort:(NSString*)namedDelegatePort
                          apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
{
    @synchronized([self class]) {
        NSMutableDictionary<NSString*, OctagonAPSReceiver*>* delegatePortMap = [self synchronizedGlobalDelegatePortMap];

        OctagonAPSReceiver* recv = delegatePortMap[namedDelegatePort];

        if(recv == nil) {
            recv = [[OctagonAPSReceiver alloc] initWithNamedDelegatePort:namedDelegatePort
                                                      apsConnectionClass:apsConnectionClass];
            delegatePortMap[namedDelegatePort] = recv;
        }

        return recv;
    }
}

+ (void)resetGlobalDelegatePortMap
{
    @synchronized (self) {
        [self resettableSynchronizedGlobalDelegatePortMap:YES];
    }
}

+ (NSMutableDictionary<NSString*, OctagonAPSReceiver*>*)synchronizedGlobalDelegatePortMap
{
    return [self resettableSynchronizedGlobalDelegatePortMap:NO];
}

+ (NSMutableDictionary<NSString*, OctagonAPSReceiver*>*)resettableSynchronizedGlobalDelegatePortMap:(BOOL)reset
{
    static NSMutableDictionary<NSString*, OctagonAPSReceiver*>* delegatePortMap = nil;

    if(delegatePortMap == nil || reset) {
        delegatePortMap = [[NSMutableDictionary alloc] init];
    }

    return delegatePortMap;
}

- (NSArray<NSString *>*)registeredPushEnvironments
{
    __block NSArray<NSString*>* environments = nil;
    dispatch_sync([OctagonAPSReceiver apsDeliveryQueue], ^{
        environments = [self.environmentMap allKeys];
    });
    return environments;
}

+ (dispatch_queue_t)apsDeliveryQueue {
    static dispatch_queue_t aps_dispatch_queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        aps_dispatch_queue = dispatch_queue_create("aps-callback-queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    });
    return aps_dispatch_queue;
}

- (BOOL) haveStalePushes
{
    __block BOOL haveStalePushes = NO;
    dispatch_sync([OctagonAPSReceiver apsDeliveryQueue], ^{
        haveStalePushes = (self.undeliveredUpdates.count || self.undeliveredCuttlefishUpdates.count);
    });
    return haveStalePushes;
}

- (NSArray<NSString*>*)cuttlefishPushTopics
{
    NSString* cuttlefishTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.security.cuttlefish"];

    // Currently cuttlefish pushes are sent to TPH. System XPC services can't properly register to be woken
    // at push time, so receive them for it.
    NSString* tphTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.TrustedPeersHelper"];

    return @[cuttlefishTopic, tphTopic];
}

- (instancetype)initWithNamedDelegatePort:(NSString*)namedDelegatePort
                       apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
{
    return [self initWithNamedDelegatePort:namedDelegatePort
                        apsConnectionClass:apsConnectionClass
                          stalePushTimeout:5*60*NSEC_PER_SEC];
}

- (instancetype)initWithNamedDelegatePort:(NSString*)namedDelegatePort
                       apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
                         stalePushTimeout:(uint64_t)stalePushTimeout
{
    if((self = [super init])) {
        _apsConnectionClass = apsConnectionClass;

        _undeliveredUpdates = [NSMutableSet set];
        _undeliveredCuttlefishUpdates = [[NSMutableSet alloc] init];

        _namedDelegatePort = namedDelegatePort;

        _environmentMap = [NSMutableDictionary dictionary];

        _octagonContainerMap = [NSMapTable strongToWeakObjectsMapTable];
        _zoneUpdateReceiver = nil;

        WEAKIFY(self);
        void (^clearPushBlock)(void) = ^{
            dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
                NSMutableSet<CKRecordZoneNotification*> *droppedUpdates;
                STRONGIFY(self);
                if (self == nil) {
                    return;
                }

                droppedUpdates = self.undeliveredUpdates;

                self.undeliveredUpdates = [NSMutableSet set];
                [self.undeliveredCuttlefishUpdates removeAllObjects];

                [self reportDroppedPushes:droppedUpdates];
            });
        };

        _clearStalePushNotifications = [[CKKSNearFutureScheduler alloc] initWithName: @"clearStalePushNotifications"
                                                                               delay:stalePushTimeout
                                                                    keepProcessAlive:false
                                                           dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                               block:clearPushBlock];
    }
    return self;
}

- (void)registerForEnvironment:(NSString*)environmentName
{
    WEAKIFY(self);

    // APS might be slow. This doesn't need to happen immediately, so let it happen later.
    dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
        STRONGIFY(self);
        if(!self) {
            return;
        }

        id<OctagonAPSConnection> apsConnection = self.environmentMap[environmentName];
        if(apsConnection) {
            // We've already set one of these up.
            return;
        }

        apsConnection = [[self.apsConnectionClass alloc] initWithEnvironmentName:environmentName namedDelegatePort:self.namedDelegatePort queue:[OctagonAPSReceiver apsDeliveryQueue]];
        self.environmentMap[environmentName] = apsConnection;

        apsConnection.delegate = self;

        // The following string should match: [[NSBundle mainBundle] bundleIdentifier]
        NSString* ckksTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.securityd"];

#if TARGET_OS_WATCH
        // Watches treat CKKS as opportunistic, and Octagon as normal priority.
        apsConnection.enabledTopics = [self cuttlefishPushTopics];
        apsConnection.opportunisticTopics = @[ckksTopic];
#else
        apsConnection.enabledTopics = [[self cuttlefishPushTopics] arrayByAddingObject:ckksTopic];
#if TARGET_OS_OSX
        apsConnection.darkWakeTopics = self.apsConnection.enabledTopics;
#endif // TARGET_OS_OSX

#endif // TARGET_OS_WATCH
    });
}

// Report that pushes we are dropping
- (void)reportDroppedPushes:(NSSet<CKRecordZoneNotification*>*)notifications
{
    bool hasBeenUnlocked = false;
    CFErrorRef error = NULL;

    /*
     * Let server know that device is not unlocked yet
     */

    (void)SecAKSGetHasBeenUnlocked(&hasBeenUnlocked, &error);
    CFReleaseNull(error);

    NSString *eventName = @"CKKS APNS Push Dropped";
    if (!hasBeenUnlocked) {
        eventName = @"CKKS APNS Push Dropped - never unlocked";
    }

    for (CKRecordZoneNotification *notification in notifications) {
        if (notification.ckksPushTracingEnabled) {
            ckksnotice_global("apsnotification", "Submitting initial CKEventMetric due to notification %@", notification);

            SecEventMetric *metric = [[SecEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
            metric[@"push_token_uuid"] = notification.ckksPushTracingUUID;
            metric[@"push_received_date"] = notification.ckksPushReceivedDate;

            metric[@"push_event_name"] = eventName;

            [[SecMetrics managerObject] submitEvent:metric];
        }
    }
}

- (CKKSCondition*)registerCKKSReceiver:(id<CKKSZoneUpdateReceiverProtocol>)receiver
{
    CKKSCondition* finished = [[CKKSCondition alloc] init];

    WEAKIFY(self);
    dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
        STRONGIFY(self);
        if(!self) {
            ckkserror_global("octagonpush", "received registration for released OctagonAPSReceiver");
            return;
        }

        ckksnotice_global("octagonpush", "Registering new CKKS push receiver: %@", receiver);

        self.zoneUpdateReceiver = receiver;

        NSMutableSet<CKRecordZoneNotification*>* currentPendingMessages = [self.undeliveredUpdates copy];
        [self.undeliveredUpdates removeAllObjects];

        for(CKRecordZoneNotification* message in currentPendingMessages.allObjects) {
            // Now, send the receiver its notification!
            ckkserror_global("octagonpush", "sending stored push(%@) to newly-registered receiver: %@", message, receiver);
            [receiver notifyZoneChange:message];
        }

        [finished fulfill];
    });

    return finished;
}

- (CKKSCondition*)registerCuttlefishReceiver:(id<OctagonCuttlefishUpdateReceiver>)receiver
                            forContainerName:(NSString*)containerName
{
    CKKSCondition* finished = [[CKKSCondition alloc] init];

    WEAKIFY(self);
    dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
        STRONGIFY(self);
        if(!self) {
            ckkserror_global("octagonpush", "received registration for released OctagonAPSReceiver");
            return;
        }

        [self.octagonContainerMap setObject:receiver forKey:containerName];
        if([self.undeliveredCuttlefishUpdates containsObject:containerName]) {
            [self.undeliveredCuttlefishUpdates removeObject:containerName];

            // Now, send the receiver its fake notification!
            ckkserror_global("octagonpush", "sending fake push to newly-registered cuttlefish receiver(%@): %@", containerName, receiver);
            [receiver notifyContainerChange:nil];
        }

        [finished fulfill];
    });

    return finished;
}

#pragma mark - APS Delegate callbacks

- (void)connection:(APSConnection *)connection didReceivePublicToken:(NSData *)publicToken {
    // no-op.
    ckksnotice_global("octagonpush", "OctagonAPSDelegate initiated: %@", connection);
}

- (void)connection:(APSConnection *)connection didReceiveToken:(NSData *)token forTopic:(NSString *)topic identifier:(NSString *)identifier {
    ckksnotice_global("octagonpush", "Received per-topic push token \"%@\" for topic \"%@\" identifier \"%@\" on connection %@", token, topic, identifier, connection);
}

- (void)connection:(APSConnection *)connection didReceiveIncomingMessage:(APSIncomingMessage *)message {
    ckksnotice_global("octagonpush", "OctagonAPSDelegate received a message(%@): %@ ", message.topic, message.userInfo);

    // Report back through APS that we received a message
    if(message.tracingEnabled) {
        [connection confirmReceiptForMessage:message];
    }

    // Separate and handle cuttlefish notifications
    if(message.userInfo[@"cf"] != nil) {
        NSDictionary* cfInfo = message.userInfo[@"cf"];
        NSString* container = cfInfo[@"c"];

        ckksnotice_global("octagonpush", "Received a cuttlefish push to container %@", container);
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastOctagonPush];

        if(container) {
            id<OctagonCuttlefishUpdateReceiver> receiver = [self.octagonContainerMap objectForKey:container];

            if(receiver) {
                [receiver notifyContainerChange:message];
            } else {
                ckkserror_global("octagonpush", "received cuttlefish push for unregistered container: %@", container);
                [self.undeliveredCuttlefishUpdates addObject:container];
                [self.clearStalePushNotifications trigger];
            }
        } else {
            // APS stripped the container. Send a push to all registered containers.
            @synchronized(self.octagonContainerMap) {
                for(id<OctagonCuttlefishUpdateReceiver> receiver in [self.octagonContainerMap objectEnumerator]) {
                    [receiver notifyContainerChange:nil];
                }
            }
        }

        return;
    }

    CKNotification* notification = [CKNotification notificationFromRemoteNotificationDictionary:message.userInfo];

    if(notification.notificationType == CKNotificationTypeRecordZone) {
        CKRecordZoneNotification* rznotification = (CKRecordZoneNotification*) notification;
        rznotification.ckksPushTracingEnabled = message.tracingEnabled;
        rznotification.ckksPushTracingUUID = message.tracingUUID ? [[[NSUUID alloc] initWithUUIDBytes:message.tracingUUID.bytes] UUIDString] : nil;
        rznotification.ckksPushReceivedDate = [NSDate date];

        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastCKKSPush];

        // Find receiever in map
        id<CKKSZoneUpdateReceiverProtocol> recv = self.zoneUpdateReceiver;
        if(recv) {
            [recv notifyZoneChange:rznotification];
        } else {
            ckkserror_global("ckkspush", "received push for unregistered receiver: %@", rznotification);
            [self.undeliveredUpdates addObject:rznotification];
            [self.clearStalePushNotifications trigger];
        }
    } else {
        ckkserror_global("ckkspush", "unexpected notification: %@", notification);
    }
}

@end

#endif // OCTAGON
