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
#import <IDS/IDS.h>

#import <utilities/debugging.h>

#import "SOSCloudKeychainConstants.h"
#import "SOSCloudKeychainClient.h"

#import "CKDStore.h"
#import "CKDAccount.h"
#import "CKDLockMonitor.h"
#import "XPCNotificationDispatcher.h"

#define XPROXYSCOPE "proxy"

typedef void (^FreshnessResponseBlock)(bool success, NSError *err);

@interface UbiqitousKVSProxy : NSObject<XPCNotificationListener, CKDLockListener>
{
    id currentiCloudToken;
    int callbackMethod;
}

@property (readonly) NSObject<CKDStore>* store;
@property (readonly) NSObject<CKDAccount>* account;
@property (readonly) NSObject<CKDLockMonitor>* lockMonitor;

@property (readonly) NSURL* persistenceURL;

@property (retain, nonatomic) NSMutableSet *alwaysKeys;
@property (retain, nonatomic) NSMutableSet *firstUnlockKeys;
@property (retain, nonatomic) NSMutableSet *unlockedKeys;

@property (atomic) bool seenKVSStoreChange;


@property (retain, nonatomic) NSMutableSet *pendingKeys;
@property (retain, nonatomic) NSMutableSet *shadowPendingKeys;

@property (retain, nonatomic) NSString *dsid;
@property (retain, nonatomic) NSString *accountUUID;

@property (retain, nonatomic) NSMutableSet<NSString*>* pendingSyncPeerIDs;
@property (retain, nonatomic) NSMutableSet<NSString*>* shadowPendingSyncPeerIDs;

@property (retain, nonatomic) NSMutableSet<NSString*>* pendingSyncBackupPeerIDs;
@property (retain, nonatomic) NSMutableSet<NSString*>* shadowPendingSyncBackupPeerIDs;

@property (atomic) bool ensurePeerRegistration;
@property (atomic) bool shadowEnsurePeerRegistration;

@property (atomic) bool inCallout;

@property (retain, nonatomic) NSMutableArray<FreshnessResponseBlock> *freshnessCompletions;
@property (atomic) dispatch_time_t nextFreshnessTime;

@property (atomic) dispatch_queue_t calloutQueue;

@property (atomic) dispatch_queue_t ckdkvsproxy_queue;
@property (atomic) dispatch_source_t penaltyTimer;
@property (atomic) bool penaltyTimerScheduled;
@property (retain, atomic) NSMutableDictionary *monitor;
@property (retain, atomic) NSDictionary *queuedMessages;

@property (copy, atomic) dispatch_block_t shadowFlushBlock;


- (NSString *)description;
- (instancetype)init NS_UNAVAILABLE;

+ (instancetype)withAccount:(NSObject<CKDAccount>*) account
                      store:(NSObject<CKDStore>*) store
                lockMonitor:(NSObject<CKDLockMonitor>*) lockMonitor
                persistence:(NSURL*) localPersistence;

- (instancetype)initWithAccount:(NSObject<CKDAccount>*) account
                          store:(NSObject<CKDStore>*) store
                    lockMonitor:(NSObject<CKDLockMonitor>*) lockMonitor
                    persistence:(NSURL*) localPersistence NS_DESIGNATED_INITIALIZER;

// Requests:

- (void)clearStore;
- (void)synchronizeStore;
- (id) objectForKey: (NSString*) key;
- (NSDictionary<NSString *, id>*) copyAsDictionary;
- (void)setObjectsFromDictionary:(NSDictionary<NSString*, NSObject*> *)otherDictionary;
- (void)waitForSynchronization:(void (^)(NSDictionary<NSString*, NSObject*> *results, NSError *err))handler;


// Callbacks from stores when things happen
- (void)storeKeysChanged: (NSSet<NSString*>*) changedKeys initial: (bool) initial;
- (void)storeAccountChanged;

- (void)requestEnsurePeerRegistration;

- (void)requestSyncWithPeerIDs: (NSArray<NSString*>*) peerIDs backupPeerIDs: (NSArray<NSString*>*) backupPeerIDs;
- (BOOL)hasSyncPendingFor: (NSString*) peerID;
- (BOOL)hasPendingKey: (NSString*) keyName;

- (void)registerAtTimeKeys:(NSDictionary*)keyparms;

- (NSSet*) keysForCurrentLockState;
- (void) intersectWithCurrentLockState: (NSMutableSet*) set;

- (NSMutableSet*) pendKeysAndGetNewlyPended: (NSSet*) keysToPend;

- (NSMutableSet*) pendingKeysForCurrentLockState;
- (NSMutableSet*) pendKeysAndGetPendingForCurrentLockState: (NSSet*) startingSet;

- (void)processPendingKeysForCurrentLockState;

- (void)registerKeys: (NSDictionary*)keys forAccount: (NSString*) accountUUID;

- (void)processKeyChangedEvent:(NSDictionary *)keysChangedInCloud;
- (NSMutableDictionary *)copyValues:(NSSet *)keysOfInterest;

- (void) doAfterFlush: (dispatch_block_t) block;
- (void) calloutWith: (void(^)(NSSet *pending, NSSet* pendingSyncIDs, NSSet* pendingBackupSyncIDs, bool ensurePeerRegistration, dispatch_queue_t queue, void(^done)(NSSet *handledKeys, NSSet *handledSyncs, bool handledEnsurePeerRegistration, NSError* error))) callout;
- (void) sendKeysCallout: (NSSet *(^)(NSSet* pending, NSError **error)) handleKeys;

- (void)recordWriteToKVS:(NSDictionary *)values;
- (NSDictionary*)recordHaltedValuesAndReturnValuesToSafelyWrite:(NSDictionary *)values;

@end
