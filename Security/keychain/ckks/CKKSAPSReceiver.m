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

#import "keychain/ckks/CKKSAPSReceiver.h"
#import "keychain/ckks/CKKSCondition.h"
#import <CloudKit/CloudKit_Private.h>
#include <utilities/debugging.h>

@interface CKKSAPSReceiver()
// If we receive >0 notifications for a CKRecordZoneID that hasn't been registered yet, give them a fake update when they register
@property NSMutableSet<CKRecordZoneID*>* undeliveredUpdates;
@end

@implementation CKKSAPSReceiver

+ (instancetype)receiverForEnvironment:(NSString *)environmentName
                     namedDelegatePort:(NSString*)namedDelegatePort
                    apsConnectionClass:(Class<CKKSAPSConnection>)apsConnectionClass {
    static NSMutableDictionary<NSString*, CKKSAPSReceiver*>* environmentMap = nil;

    if(environmentName == nil) {
        secnotice("ckkspush", "No push environment; not bringing up APS.");
        return nil;
    }

    @synchronized([self class]) {
        if(environmentMap == nil) {
            environmentMap = [[NSMutableDictionary alloc] init];
        }

        CKKSAPSReceiver* recv = [environmentMap valueForKey: environmentName];

        if(recv == nil) {
            recv = [[CKKSAPSReceiver alloc] initWithEnvironmentName: environmentName namedDelegatePort:namedDelegatePort apsConnectionClass: apsConnectionClass];
            [environmentMap setValue: recv forKey: environmentName];
        }

        return recv;
    }
}

+ (dispatch_queue_t)apsDeliveryQueue {
    static dispatch_queue_t aps_dispatch_queue;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        aps_dispatch_queue = dispatch_queue_create("aps-callback-queue", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    });
    return aps_dispatch_queue;
}

- (instancetype)initWithEnvironmentName:(NSString*)environmentName
                      namedDelegatePort:(NSString*)namedDelegatePort
                     apsConnectionClass:(Class<CKKSAPSConnection>)apsConnectionClass {
    if(self = [super init]) {
        _apsConnectionClass = apsConnectionClass;
        _apsConnection = NULL;

        _undeliveredUpdates = [[NSMutableSet alloc] init];

        // APS might be slow. This doesn't need to happen immediately, so let it happen later.
        __weak __typeof(self) weakSelf = self;
        dispatch_async([CKKSAPSReceiver apsDeliveryQueue], ^{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                return;
            }
            strongSelf.apsConnection = [[strongSelf.apsConnectionClass alloc] initWithEnvironmentName:environmentName namedDelegatePort:namedDelegatePort queue:[CKKSAPSReceiver apsDeliveryQueue]];
            strongSelf.apsConnection.delegate = strongSelf;

            // The following string should match: [[NSBundle mainBundle] bundleIdentifier]
            NSString* topic = [kCKPushTopicPrefix stringByAppendingString:@"com.apple.securityd"];
            [strongSelf.apsConnection setEnabledTopics:@[topic]];
        });

        _zoneMap = [NSMapTable strongToWeakObjectsMapTable];
    }
    return self;
}

- (CKKSCondition*)registerReceiver:(id<CKKSZoneUpdateReceiver>)receiver forZoneID:(CKRecordZoneID *)zoneID {
    CKKSCondition* finished = [[CKKSCondition alloc] init];

    __weak __typeof(self) weakSelf = self;
    dispatch_async([CKKSAPSReceiver apsDeliveryQueue], ^{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            secerror("ckks: received registration for released CKKSAPSReceiver");
            return;
        }

        [self.zoneMap setObject:receiver forKey: zoneID];
        if([strongSelf.undeliveredUpdates containsObject:zoneID]) {
            [strongSelf.undeliveredUpdates removeObject:zoneID];

            // Now, send the receiver its fake notification!
            secerror("ckks: sending fake push to newly-registered zone(%@): %@", zoneID, receiver);
            [receiver notifyZoneChange:nil];
        }

        [finished fulfill];
    });

    return finished;
}

#pragma mark - APS Delegate callbacks

- (void)connection:(APSConnection *)connection didReceivePublicToken:(NSData *)publicToken {
    // no-op.
    secnotice("ckkspush", "CKKSAPSDelegate initiated: %@", connection);
}

- (void)connection:(APSConnection *)connection didReceiveToken:(NSData *)token forTopic:(NSString *)topic identifier:(NSString *)identifier {
    secnotice("ckkspush", "Received per-topic push token \"%@\" for topic \"%@\" identifier \"%@\" on connection %@", token, topic, identifier, connection);
}

- (void)connection:(APSConnection *)connection didReceiveIncomingMessage:(APSIncomingMessage *)message {
    secnotice("ckkspush", "CKKSAPSDelegate received a message: %@ on connection %@", message, connection);

    CKNotification* notification = [CKNotification notificationFromRemoteNotificationDictionary:message.userInfo];

    if(notification.notificationType == CKNotificationTypeRecordZone) {
        CKRecordZoneNotification* rznotification = (CKRecordZoneNotification*) notification;

        // Find receiever in map
        id<CKKSZoneUpdateReceiver> recv = [self.zoneMap objectForKey:rznotification.recordZoneID];
        if(recv) {
            [recv notifyZoneChange:rznotification];
        } else {
            secerror("ckks: received push for unregistered zone: %@", rznotification);
            if(rznotification.recordZoneID) {
                [self.undeliveredUpdates addObject: rznotification.recordZoneID];
            }
        }
    } else {
        secerror("ckks: unexpected notification: %@", notification);
    }
}

@end

#endif // OCTAGON
