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

#include <securityd/SecDbItem.h>
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSAPSReceiver.h"
#import "keychain/ckks/CKKSCKAccountStateTracker.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSControlProtocol.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSRateLimiter.h"
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ot/OTDefines.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKeychainView, CKKSRateLimiter;

@interface CKKSViewManager : NSObject <CKKSControlProtocol, CKKSPeerProvider>

@property CKContainer* container;
@property CKKSCKAccountStateTracker* accountTracker;
@property CKKSLockStateTracker* lockStateTracker;
@property CKKSReachabilityTracker *reachabilityTracker;
@property CKKSZoneChangeFetcher* zoneChangeFetcher;
@property bool initializeNewZones;

// Signaled when SecCKKSInitialize is complete, as it's async and likes to fire after tests are complete
@property CKKSCondition* completedSecCKKSInitialize;

@property CKKSRateLimiter* globalRateLimiter;

- (instancetype)initCloudKitWithContainerName:(NSString*)containerName usePCS:(bool)usePCS;
- (instancetype)initWithContainerName:(NSString*)containerName
                                  usePCS:(bool)usePCS
    fetchRecordZoneChangesOperationClass:(Class<CKKSFetchRecordZoneChangesOperation>)fetchRecordZoneChangesOperationClass
              fetchRecordsOperationClass:(Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                     queryOperationClass:(Class<CKKSQueryOperation>)queryOperationClass
       modifySubscriptionsOperationClass:(Class<CKKSModifySubscriptionsOperation>)modifySubscriptionsOperationClass
         modifyRecordZonesOperationClass:(Class<CKKSModifyRecordZonesOperation>)modifyRecordZonesOperationClass
                      apsConnectionClass:(Class<CKKSAPSConnection>)apsConnectionClass
               nsnotificationCenterClass:(Class<CKKSNSNotificationCenter>)nsnotificationCenterClass
                           notifierClass:(Class<CKKSNotifier>)notifierClass;

- (CKKSKeychainView*)findView:(NSString*)viewName;
- (CKKSKeychainView*)findOrCreateView:(NSString*)viewName;
- (void)setView:(CKKSKeychainView*)obj;
- (void)clearView:(NSString*)viewName;

- (NSDictionary<NSString*, NSString*>*)activeTLKs;

// Call this to bring zones up (and to do so automatically in the future)
- (void)initializeZones;

- (NSString*)viewNameForItem:(SecDbItemRef)item;

- (void)handleKeychainEventDbConnection:(SecDbConnectionRef)dbconn
                                 source:(SecDbTransactionSource)txionSource
                                  added:(SecDbItemRef _Nullable)added
                                deleted:(SecDbItemRef _Nullable)deleted;

- (void)setCurrentItemForAccessGroup:(NSData* _Nonnull)newItemPersistentRef
                                hash:(NSData*)newItemSHA1
                         accessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                            viewHint:(NSString*)viewHint
                           replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                                hash:(NSData* _Nullable)oldItemSHA1
                            complete:(void (^)(NSError* operror))complete;

- (void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                          identifier:(NSString*)identifier
                            viewHint:(NSString*)viewHint
                     fetchCloudValue:(bool)fetchCloudValue
                            complete:(void (^)(NSString* uuid, NSError* operror))complete;

- (NSString*)viewNameForAttributes:(NSDictionary*)item;

- (void)registerSyncStatusCallback:(NSString*)uuid callback:(SecBoolNSErrorCallback)callback;

// Cancels pending operations owned by this view manager
- (void)cancelPendingOperations;

// Use these to acquire (and set) the singleton
+ (instancetype)manager;
+ (instancetype _Nullable)resetManager:(bool)reset setTo:(CKKSViewManager* _Nullable)obj;

// Called by XPC every 24 hours
- (void)xpc24HrNotification;

/* White-box testing only */
- (CKKSKeychainView*)restartZone:(NSString*)viewName;

// Returns the viewList for a CKKSViewManager
- (NSSet*)viewList;

// Notify sbd to re-backup.
- (void)notifyNewTLKsInKeychain;
- (void)syncBackupAndNotifyAboutSync;

// Fetch peers from SOS
- (CKKSSelves* _Nullable)fetchSelfPeers:(NSError* __autoreleasing*)error;
- (NSSet<id<CKKSPeer>>* _Nullable)fetchTrustedPeers:(NSError* __autoreleasing*)error;

// For mocking purposes
- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError**)error;
- (NSSet<id<CKKSSelfPeer>>*)pastSelves:(NSError**)error;
- (NSArray<NSDictionary*>* _Nullable)loadRestoredBottledKeysOfType:(OctagonKeyType)keyType error:(NSError**)error;

- (void)sendSelfPeerChangedUpdate;
- (void)sendTrustedPeerSetChangedUpdate;

@end
NS_ASSUME_NONNULL_END

#else
@interface CKKSViewManager : NSObject
@end
#endif  // OCTAGON
