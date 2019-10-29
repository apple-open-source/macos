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
#include <utilities/debugging.h>

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

// If we receive notifications for a record zone that hasn't been registered yet, send them a their updates when they register
@property NSMutableDictionary<NSString*, NSMutableSet<CKRecordZoneNotification*>*>* undeliveredUpdates;

// Same, but for cuttlefish containers (and only remember that a push was received; don't remember the pushes themselves)
@property NSMutableSet<NSString*>* undeliveredCuttlefishUpdates;

@property NSMapTable<NSString*, id<CKKSZoneUpdateReceiver>>* zoneMap;
@property NSMapTable<NSString*, id<OctagonCuttlefishUpdateReceiver>>* octagonContainerMap;
@end

@implementation OctagonAPSReceiver

+ (instancetype)receiverForEnvironment:(NSString *)environmentName
                     namedDelegatePort:(NSString*)namedDelegatePort
                    apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass
{
    if(environmentName == nil) {
        secnotice("octagonpush", "No push environment; not bringing up APS.");
        return nil;
    }

    @synchronized([self class]) {
        NSMutableDictionary<NSString*, OctagonAPSReceiver*>* environmentMap = [self synchronizedGlobalEnvironmentMap];

        OctagonAPSReceiver* recv = [environmentMap valueForKey: environmentName];

        if(recv == nil) {
            recv = [[OctagonAPSReceiver alloc] initWithEnvironmentName: environmentName namedDelegatePort:namedDelegatePort apsConnectionClass: apsConnectionClass];
            [environmentMap setValue: recv forKey: environmentName];
        }

        return recv;
    }
}

+ (void)resetGlobalEnviornmentMap
{
    @synchronized (self) {
        [self resettableSynchronizedGlobalEnvironmentMap:YES];
    }
}

+ (NSMutableDictionary<NSString*, OctagonAPSReceiver*>*)synchronizedGlobalEnvironmentMap
{
    return [self resettableSynchronizedGlobalEnvironmentMap:NO];
}

+ (NSMutableDictionary<NSString*, OctagonAPSReceiver*>*)resettableSynchronizedGlobalEnvironmentMap:(BOOL)reset
{
    static NSMutableDictionary<NSString*, OctagonAPSReceiver*>* environmentMap = nil;

    if(environmentMap == nil || reset) {
        environmentMap = [[NSMutableDictionary alloc] init];
    }

    return environmentMap;
}

+ (dispatch_queue_t)apsDeliveryQueue {
    static dispatch_queue_t aps_dispatch_queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        aps_dispatch_queue = dispatch_queue_create("aps-callback-queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    });
    return aps_dispatch_queue;
}

+ (int64_t)stalePushTimeout {
    return 5*60*NSEC_PER_SEC;
}

- (BOOL) haveStalePushes
{
    __block BOOL haveStalePushes = NO;
    dispatch_sync([OctagonAPSReceiver apsDeliveryQueue], ^{
        haveStalePushes = (self.undeliveredUpdates.count || self.undeliveredCuttlefishUpdates.count);
    });
    return haveStalePushes;
}

- (NSSet<NSString*>*)cuttlefishPushTopics
{
    NSString* cuttlefishTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.security.cuttlefish"];

    // Currently cuttlefish pushes are sent to TPH. System XPC services can't properly register to be woken
    // at push time, so receive them for it.
    NSString* tphTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.TrustedPeersHelper"];
    NSString* securitydTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.securityd"];

    return [NSSet setWithArray:@[cuttlefishTopic, tphTopic, securitydTopic]];
}

- (instancetype)initWithEnvironmentName:(NSString*)environmentName
                      namedDelegatePort:(NSString*)namedDelegatePort
                     apsConnectionClass:(Class<OctagonAPSConnection>)apsConnectionClass {
    if(self = [super init]) {
        _apsConnectionClass = apsConnectionClass;
        _apsConnection = NULL;

        _undeliveredUpdates = [NSMutableDictionary dictionary];
        _undeliveredCuttlefishUpdates = [[NSMutableSet alloc] init];

        // APS might be slow. This doesn't need to happen immediately, so let it happen later.
        WEAKIFY(self);
        dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
            STRONGIFY(self);
            if(!self) {
                return;
            }
            self.apsConnection = [[self.apsConnectionClass alloc] initWithEnvironmentName:environmentName namedDelegatePort:namedDelegatePort queue:[OctagonAPSReceiver apsDeliveryQueue]];
            self.apsConnection.delegate = self;

            // The following string should match: [[NSBundle mainBundle] bundleIdentifier]
            NSString* ckksTopic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.securityd"];

            NSArray* topics = [@[ckksTopic] arrayByAddingObjectsFromArray:[self cuttlefishPushTopics].allObjects];
            [self.apsConnection setEnabledTopics:topics];
#if TARGET_OS_OSX
            [self.apsConnection setDarkWakeTopics:topics];
#endif
        });

        _zoneMap = [NSMapTable strongToWeakObjectsMapTable];
        _octagonContainerMap = [NSMapTable strongToWeakObjectsMapTable];

        void (^clearPushBlock)(void) = ^{
            dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
                NSDictionary<NSString*, NSMutableSet<CKRecordZoneNotification*>*> *droppedUpdates;
                STRONGIFY(self);
                if (self == nil) {
                    return;
                }

                droppedUpdates = self.undeliveredUpdates;

                self.undeliveredUpdates = [NSMutableDictionary dictionary];
                [self.undeliveredCuttlefishUpdates removeAllObjects];

                dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                    STRONGIFY(self);
                    [self reportDroppedPushes:droppedUpdates];
                });
            });
        };

        _clearStalePushNotifications = [[CKKSNearFutureScheduler alloc] initWithName: @"clearStalePushNotifications"
                                                                               delay:[[self class] stalePushTimeout]
                                                                    keepProcessAlive:false
                                                           dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                               block:clearPushBlock];
    }
    return self;
}

// Report that pushes we are dropping
- (void)reportDroppedPushes:(NSDictionary<NSString*, NSMutableSet<CKRecordZoneNotification*>*>*)notifications
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

    for (NSString *zone in notifications) {
        for (CKRecordZoneNotification *notification in notifications[zone]) {
            if (notification.ckksPushTracingEnabled) {
                secnotice("apsnotification", "Submitting initial CKEventMetric due to notification %@", notification);

                SecEventMetric *metric = [[SecEventMetric alloc] initWithEventName:@"APNSPushMetrics"];
                metric[@"push_token_uuid"] = notification.ckksPushTracingUUID;
                metric[@"push_received_date"] = notification.ckksPushReceivedDate;

                metric[@"push_event_name"] = eventName;

                [[SecMetrics managerObject] submitEvent:metric];
            }
        }
    }
}



- (CKKSCondition*)registerReceiver:(id<CKKSZoneUpdateReceiver>)receiver forZoneID:(CKRecordZoneID *)zoneID {
    CKKSCondition* finished = [[CKKSCondition alloc] init];

    WEAKIFY(self);
    dispatch_async([OctagonAPSReceiver apsDeliveryQueue], ^{
        STRONGIFY(self);
        if(!self) {
            secerror("ckks: received registration for released OctagonAPSReceiver");
            return;
        }

        [self.zoneMap setObject:receiver forKey: zoneID.zoneName];

        NSMutableSet<CKRecordZoneNotification*>* currentPendingMessages = self.undeliveredUpdates[zoneID.zoneName];
        [self.undeliveredUpdates removeObjectForKey:zoneID.zoneName];

        for(CKRecordZoneNotification* message in currentPendingMessages.allObjects) {
            // Now, send the receiver its notification!
            secerror("ckks: sending stored push(%@) to newly-registered zone(%@): %@", message, zoneID.zoneName, receiver);
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
            secerror("octagon: received registration for released OctagonAPSReceiver");
            return;
        }

        [self.octagonContainerMap setObject:receiver forKey:containerName];
        if([self.undeliveredCuttlefishUpdates containsObject:containerName]) {
            [self.undeliveredCuttlefishUpdates removeObject:containerName];

            // Now, send the receiver its fake notification!
            secerror("octagon: sending fake push to newly-registered cuttlefish receiver(%@): %@", containerName, receiver);
            [receiver notifyContainerChange:nil];
        }

        [finished fulfill];
    });

    return finished;
}

#pragma mark - APS Delegate callbacks

- (void)connection:(APSConnection *)connection didReceivePublicToken:(NSData *)publicToken {
    // no-op.
    secnotice("octagonpush", "OctagonAPSDelegate initiated: %@", connection);
}

- (void)connection:(APSConnection *)connection didReceiveToken:(NSData *)token forTopic:(NSString *)topic identifier:(NSString *)identifier {
    secnotice("octagonpush", "Received per-topic push token \"%@\" for topic \"%@\" identifier \"%@\" on connection %@", token, topic, identifier, connection);
}

- (void)connection:(APSConnection *)connection didReceiveIncomingMessage:(APSIncomingMessage *)message {
    secnotice("octagonpush", "OctagonAPSDelegate received a message(%@): %@ ", message.topic, message.userInfo);

    // Report back through APS that we received a message
    if(message.tracingEnabled) {
        [connection confirmReceiptForMessage:message];
    }

    NSSet<NSString*>* cuttlefishTopics = [self cuttlefishPushTopics];

    // Separate and handle cuttlefish notifications
    if([cuttlefishTopics containsObject:message.topic] && [message.userInfo objectForKey:@"cf"]) {
        NSDictionary* cfInfo = message.userInfo[@"cf"];
        NSString* container = cfInfo[@"c"];

        secnotice("octagonpush", "Received a cuttlefish push to container %@", container);
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastOctagonPush];

        if(container) {
            id<OctagonCuttlefishUpdateReceiver> receiver = [self.octagonContainerMap objectForKey:container];

            if(receiver) {
                [receiver notifyContainerChange:message];
            } else {
                secerror("octagonpush: received cuttlefish push for unregistered container: %@", container);
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
        id<CKKSZoneUpdateReceiver> recv = [self.zoneMap objectForKey:rznotification.recordZoneID.zoneName];
        if(recv) {
            [recv notifyZoneChange:rznotification];
        } else {
            secerror("ckks: received push for unregistered zone: %@", rznotification);
            if(rznotification.recordZoneID) {
                NSMutableSet<CKRecordZoneNotification*>* currentPendingMessages = self.undeliveredUpdates[rznotification.recordZoneID.zoneName];
                if(currentPendingMessages) {
                    [currentPendingMessages addObject:rznotification];
                } else {
                    self.undeliveredUpdates[rznotification.recordZoneID.zoneName] = [NSMutableSet setWithObject:rznotification];
                    [self.clearStalePushNotifications trigger];
                }
            }
        }
    } else {
        secerror("ckks: unexpected notification: %@", notification);
    }
}

@end

#endif // OCTAGON
