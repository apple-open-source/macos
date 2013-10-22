/*
 * Copyright (c) 2012 Apple Computer, Inc. All Rights Reserved.
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
@property (retain, nonatomic) NSString *myID;
@property (retain, nonatomic) NSArray *keysToRead;
@property (retain, nonatomic) NSArray *keysToWrite;
@property (retain, nonatomic) NSArray *keysReadWrite;

@property (retain, nonatomic) NSSet *alwaysKeys;
@property (retain, nonatomic) NSSet *firstUnlockKeys;
@property (retain, nonatomic) NSSet *unlockedKeys;
@property (retain, nonatomic) NSMutableSet *pendingKeys;
@property (retain, nonatomic) NSMutableSet *shadowPendingKeys;
@property (atomic) bool syncWithPeersPending;
@property (atomic) bool shadowSyncWithPeersPending;
@property (atomic) bool inCallout;
@property (atomic) bool unlockedSinceBoot;
@property (atomic) bool isLocked;
@property (atomic) bool seenKVSStoreChange;

@property (atomic) dispatch_source_t syncTimer;
@property (atomic) bool syncTimerScheduled;
@property (atomic) dispatch_time_t deadline;
@property (atomic) dispatch_time_t lastSyncTime;

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

- (NSUbiquitousKeyValueStore *)cloudStore;

- (NSSet*) keysForCurrentLockState;
- (NSSet*) filterKeySetWithLockState: (NSSet*) startingSet;
- (NSDictionary *)registerKeysAndGet:(BOOL)getNewKeysOnly always:(NSArray *)keysToRegister reqFirstUnlock:(NSArray *)reqFirstUnlock reqUnlocked:(NSArray *)reqUnlocked;

- (NSDictionary *)localNotification:(NSDictionary *)localNotificationDict outFlags:(int64_t *)outFlags;
- (void)processKeyChangedEvent:(NSDictionary *)keysChangedInCloud;
- (NSMutableDictionary *)copyChangedValues:(NSSet *)keysOfInterest;
- (void)setParams:(NSDictionary *)paramsDict;

@end
