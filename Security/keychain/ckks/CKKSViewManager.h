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

#include "keychain/securityd/SecDbItem.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSCloudKitClassDependencies.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSControlProtocol.h"
#import "keychain/ckks/CKKSLockStateTracker.h"
#import "keychain/ckks/CKKSReachabilityTracker.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSPeer.h"
#import "keychain/ckks/CKKSRateLimiter.h"
#import "keychain/ckks/CloudKitDependencies.h"
#import "keychain/ckks/CKKSZoneChangeFetcher.h"
#import "keychain/ckks/CKKSZoneModifier.h"
#import "keychain/ckks/CKKSKeychainBackedKey.h"

#import "keychain/ot/OTSOSAdapter.h"
#import "keychain/ot/OTDefines.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKeychainView, CKKSRateLimiter, TPSyncingPolicy;

@interface CKKSViewManager : NSObject <CKKSControlProtocol>

@property CKContainer* container;
@property CKKSAccountStateTracker* accountTracker;
@property CKKSLockStateTracker* lockStateTracker;
@property CKKSReachabilityTracker *reachabilityTracker;
@property CKKSZoneChangeFetcher* zoneChangeFetcher;
@property CKKSZoneModifier* zoneModifier;

// Signaled when SecCKKSInitialize is complete, as it's async and likes to fire after tests are complete
@property CKKSCondition* completedSecCKKSInitialize;

@property CKKSRateLimiter* globalRateLimiter;

@property id<OTSOSAdapter> sosPeerAdapter;

@property (readonly, nullable) TPSyncingPolicy* policy;

@property (readonly) NSMutableDictionary<NSString*, CKKSKeychainView*>* views;

- (instancetype)initWithContainer:(CKContainer*)container
                       sosAdapter:(id<OTSOSAdapter> _Nullable)sosAdapter
              accountStateTracker:(CKKSAccountStateTracker*)accountTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies;

// Note: findView will not wait for any views to be created. You must handle
// states where the daemon has not entirely started up yourself
- (CKKSKeychainView* _Nullable)findView:(NSString*)viewName;

// Similar to findView, but will create the view if it's not already present.
- (CKKSKeychainView*)findOrCreateView:(NSString*)viewName;

// findViewOrError will wait for the Syncing Policy to be loaded, which
// creates all views. Don't call this from any important queues.
- (CKKSKeychainView* _Nullable)findView:(NSString*)viewName error:(NSError**)error;

- (void)setView:(CKKSKeychainView*)obj;
- (void)clearView:(NSString*)viewName;

- (NSSet<CKKSKeychainView*>*)currentViews;

- (void)setupAnalytics;

- (NSString* _Nullable)viewNameForItem:(SecDbItemRef)item;

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

- (void)registerSyncStatusCallback:(NSString*)uuid callback:(SecBoolNSErrorCallback)callback;

// Cancels pending operations owned by this view manager
- (void)cancelPendingOperations;

+ (instancetype)manager;

// Called by XPC every 24 hours
- (void)xpc24HrNotification;

// Returns the current set of views
- (NSSet<NSString*>*)viewList;

- (NSSet<NSString*>*)defaultViewList;

// Call this to set the syncing views+policy that this manager will use.
// If beginCloudKitOperationOfAllViews has previously been called, then any new views created
// as a result of this call will begin CK operation.
- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy;

// Similar to above, but please only pass policyIsFresh=YES if Octagon has contacted cuttlefish immediately previously
// Returns YES if the view set has changed as part of this set
- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy policyIsFresh:(BOOL)policyIsFresh;

- (void)clearAllViews;

// Create all views, but don't begin CK/network operations
// Remove as part of <rdar://problem/57768740> CKKS: ensure we collect keychain changes made before policy is loaded from disk
- (void)createViews;

// Call this to begin CK operation of all views
// This bit will be 'sticky', in that any new views created with also begin cloudkit operation immediately.
// (clearAllViews will reset this bit.)
- (void)beginCloudKitOperationOfAllViews;

// Notify sbd to re-backup.
- (void)notifyNewTLKsInKeychain;
- (void)syncBackupAndNotifyAboutSync;

// allow user blocking operation to block on trust status trying to sort it-self out the
// first time after launch, only waits the the initial call
- (BOOL)waitForTrustReady;

// Helper function to make CK containers
+ (CKContainer*)makeCKContainer:(NSString*)containerName
                         usePCS:(bool)usePCS;

// Checks featureflags to return whether we should use policy-based views, or use the hardcoded list
- (BOOL)useCKKSViewsFromPolicy;

// Extract TLKs for sending to some peer. Pass restrictToPolicy=True if you want to restrict the returned TLKs
// to what the current policy indicates (allowing to prioritize transferred TLKs)
- (NSArray<CKKSKeychainBackedKey*>* _Nullable)currentTLKsFilteredByPolicy:(BOOL)restrictToPolicy error:(NSError**)error;

// Interfaces to examine sync callbacks
- (SecBoolNSErrorCallback _Nullable)claimCallbackForUUID:(NSString* _Nullable)uuid;
- (NSSet<NSString*>*)pendingCallbackUUIDs;
+ (void)callSyncCallbackWithErrorNoAccount:(SecBoolNSErrorCallback)syncCallback;
@end

@interface CKKSViewManager (Testing)
- (void)setOverrideCKKSViewsFromPolicy:(BOOL)value;
- (void)resetSyncingPolicy;

- (void)haltAll;
- (CKKSKeychainView*)restartZone:(NSString*)viewName;
- (void)haltZone:(NSString*)viewName;

// If set, any set passed to setSyncingViews will be intersected with this set
- (void)setSyncingViewsAllowList:(NSSet<NSString*>* _Nullable)viewNames;
@end
NS_ASSUME_NONNULL_END

#else
@interface CKKSViewManager : NSObject
@end
#endif  // OCTAGON
