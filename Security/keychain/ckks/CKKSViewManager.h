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
#import <Security/CKKSControlProtocol.h>
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
#import "keychain/ot/OTPersonaAdapter.h"

NS_ASSUME_NONNULL_BEGIN

@class CKKSKeychainView, CKKSRateLimiter, TPSyncingPolicy;

@interface CKKSViewManager : NSObject <CKKSControlProtocol>

@property CKContainer* container;
@property CKKSAccountStateTracker* accountTracker;
@property CKKSLockStateTracker* lockStateTracker;
@property CKKSReachabilityTracker *reachabilityTracker;
@property id<OTPersonaAdapter> personaAdapter;
@property CKKSZoneChangeFetcher* zoneChangeFetcher;
@property CKKSZoneModifier* zoneModifier;

// Signaled when SecCKKSInitialize is complete, as it's async and likes to fire after tests are complete
@property CKKSCondition* completedSecCKKSInitialize;

@property CKKSRateLimiter* globalRateLimiter;

@property id<OTSOSAdapter> sosPeerAdapter;

- (instancetype)initWithContainer:(CKContainer*)container
                       sosAdapter:(id<OTSOSAdapter> _Nullable)sosAdapter
              accountStateTracker:(CKKSAccountStateTracker*)accountTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                   personaAdapter:(id<OTPersonaAdapter>)personaAdapter
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies;

- (CKKSKeychainView* _Nullable)ckksAccountSyncForContainer:(NSString*)container
                                                 contextID:(NSString*)contextID;

- (void)setupAnalytics;

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

+ (instancetype)manager;

// Called by XPC every 24 hours
- (void)xpc24HrNotification;

// Notify sbd to re-backup.
- (void)notifyNewTLKsInKeychain;
- (void)syncBackupAndNotifyAboutSync;

// allow user blocking operation to block on trust status trying to sort it-self out the
// first time after launch, only waits the the initial call
- (BOOL)waitForTrustReady;

// Extract TLKs for sending to some peer. Pass restrictToPolicy=True if you want to restrict the returned TLKs
// to what the current policy indicates (allowing to prioritize transferred TLKs)
- (NSArray<CKKSKeychainBackedKey*>* _Nullable)currentTLKsFilteredByPolicy:(BOOL)restrictToPolicy error:(NSError**)error;

// Interfaces to examine sync callbacks
- (SecBoolNSErrorCallback _Nullable)claimCallbackForUUID:(NSString* _Nullable)uuid;
- (NSSet<NSString*>*)pendingCallbackUUIDs;
+ (void)callSyncCallbackWithErrorNoAccount:(SecBoolNSErrorCallback)syncCallback;
@end

@interface CKKSViewManager (Testing)
- (void)haltAll;
- (void)dropAllActors;

// Note: this CKKSKeychainView _must_ correspond to the default container and contextID
- (CKKSKeychainView*)restartCKKSAccountSync:(CKKSKeychainView*)view;
- (CKKSKeychainView*)restartCKKSAccountSyncWithoutSettingPolicy:(CKKSKeychainView*)view;
@end
NS_ASSUME_NONNULL_END

#else
@interface CKKSViewManager : NSObject
@end
#endif  // OCTAGON
