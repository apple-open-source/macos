/*
 * Copyright (c) 2012-2014 Apple Inc. All Rights Reserved.
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

//
//  CKDKVSProxy.h
//  ckd-xpc

#import <Foundation/Foundation.h>
#import <dispatch/queue.h>
#import <xpc/xpc.h>

#import <utilities/debugging.h>

#import "SOSCloudKeychainConstants.h"
#import "SOSCloudKeychainClient.h"

#define XPROXYSCOPE "proxy"

@interface UbiqitousKVSProxy  : NSObject
{
    id currentiCloudToken;
    CloudItemsChangedBlock itemsChangedCallback;
    int callbackMethod;
}

@property (retain, nonatomic) NSDictionary *keyParameterKeys;
@property (retain, nonatomic) NSDictionary *circleKeys;
@property (retain, nonatomic) NSDictionary *messageKeys;

@property (retain, nonatomic) NSMutableSet *alwaysKeys;
@property (retain, nonatomic) NSMutableSet *firstUnlockKeys;
@property (retain, nonatomic) NSMutableSet *unlockedKeys;
@property (retain, nonatomic) NSMutableSet *pendingKeys;
@property (retain, nonatomic) NSMutableSet *shadowPendingKeys;
@property (atomic) bool syncWithPeersPending;
@property (atomic) bool shadowSyncWithPeersPending;
@property (atomic) bool inCallout;
@property (atomic) bool oldInCallout;
@property (atomic) bool unlockedSinceBoot;
@property (atomic) bool isLocked;
@property (atomic) bool seenKVSStoreChange;
@property (atomic) bool ensurePeerRegistration;
@property (atomic) bool shadowEnsurePeerRegistration;
@property (atomic) dispatch_time_t nextFreshnessTime;

@property (atomic) dispatch_source_t syncTimer;
@property (atomic) bool syncTimerScheduled;
@property (atomic) dispatch_time_t deadline;
@property (atomic) dispatch_time_t lastSyncTime;
@property (atomic) dispatch_queue_t calloutQueue;
@property (atomic) dispatch_queue_t freshParamsQueue;

+ (UbiqitousKVSProxy *) sharedKVSProxy;
- (NSString *)description;
- (id)init;
- (void)streamEvent:(xpc_object_t)notification;
- (void)setItemsChangedBlock:(CloudItemsChangedBlock)itemsChangedBlock;

- (void)setObject:(id)obj forKey:(id)key;
- (id)get:(id)key;
- (NSDictionary *)getAll;
- (void)requestSynchronization:(bool)force;
- (void)waitForSynchronization:(NSArray *)keys handler:(void (^)(NSDictionary *values, NSError *err))handler;
- (void)clearStore;
- (void)setObjectsFromDictionary:(NSDictionary *)values;
- (void)removeObjectForKey:(NSString *)keyToRemove;
- (void)processAllItems;
- (void)requestSyncWithAllPeers;
- (void)requestEnsurePeerRegistration;

- (NSUbiquitousKeyValueStore *)cloudStore;

-(void)registerAtTimeKeys:(NSDictionary*)keyparms;

- (NSSet*) keysForCurrentLockState;
- (void) intersectWithCurrentLockState: (NSMutableSet*) set;

- (NSMutableSet*) pendKeysAndGetNewlyPended: (NSSet*) keysToPend;

- (NSMutableSet*) pendingKeysForCurrentLockState;
- (NSMutableSet*) pendKeysAndGetPendingForCurrentLockState: (NSSet*) startingSet;

- (void) processPendingKeysForCurrentLockState;

- (void)registerKeys: (NSDictionary*)keys;

- (NSDictionary *)localNotification:(NSDictionary *)localNotificationDict outFlags:(int64_t *)outFlags;

- (void)processKeyChangedEvent:(NSDictionary *)keysChangedInCloud;
- (NSMutableDictionary *)copyValues:(NSSet *)keysOfInterest;

- (void) doAfterFlush: (dispatch_block_t) block;

- (void) calloutWith: (void(^)(NSSet *pending, bool syncWithPeersPending, bool ensurePeerRegistration, dispatch_queue_t queue, void(^done)(NSSet *handledKeys, bool handledSyncWithPeers, bool handledEnsurePeerRegistration))) callout;
- (void) sendKeysCallout: (NSSet *(^)(NSSet* pending, NSError **error)) handleKeys;



@end
