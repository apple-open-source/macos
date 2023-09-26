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

#import <os/feature_private.h>

#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ckks/CKKSExternalTLKClient.h"
#import "keychain/ckks/CKKSActor+ExternalClientHandling.h"
#import "keychain/ckks/CKKSSecDbAdapter.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/analytics/SecEventMetric.h"
#import "keychain/analytics/SecMetrics.h"

#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/OTControl.h"
#import "keychain/ot/ObjCImprovements.h"
#import "keychain/ot/OTPersonaAdapter.h"

#import "keychain/trust/TrustedPeers/TPSyncingPolicy.h"

#import "SecEntitlements.h"

#include "keychain/securityd/SecDbItem.h"
#include "keychain/securityd/SecDbKeychainItem.h"
#include "keychain/securityd/SecItemSchema.h"
#include <Security/SecureObjectSync/SOSViews.h>

#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>

#include "keychain/SecureObjectSync/SOSAccount.h"
#include <Security/SecItemBackup.h>

#if OCTAGON
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "CKKSAnalytics.h"
#endif

#if !OCTAGON
@interface CKKSViewManager () <NSXPCListenerDelegate>
#else
@interface CKKSViewManager () <NSXPCListenerDelegate,
                               CKKSCloudKitAccountStateListener>

@property NSXPCListener *listener;

@property (nullable) NSSet<NSString*>* viewAllowList;

// Once you set these, all CKKSKeychainViews created will use them
@property CKKSCloudKitClassDependencies* cloudKitClassDependencies;

@property NSMutableDictionary<NSString*, SecBoolNSErrorCallback>* pendingSyncCallbacks;
@property NSOperationQueue* operationQueue;


// Make writable
@property (nullable) TPSyncingPolicy* policy;
@property CKKSCondition* policyLoaded;

#endif
@end

#if OCTAGON
@interface CKKSViewManager (lockstateTracker) <CKKSLockStateNotification>
@end
#endif

@implementation CKKSViewManager
#if OCTAGON

- (instancetype)initWithContainer:(CKContainer*)container
                       sosAdapter:(id<OTSOSAdapter> _Nullable)sosAdapter
              accountStateTracker:(CKKSAccountStateTracker*)accountTracker
                 lockStateTracker:(CKKSLockStateTracker*)lockStateTracker
              reachabilityTracker:(CKKSReachabilityTracker*)reachabilityTracker
                   personaAdapter:(id<OTPersonaAdapter>)personaAdapter
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
                  accountsAdapter:(id<OTAccountsAdapter>)accountsAdapter
{
    if(self = [super init]) {
        _cloudKitClassDependencies = cloudKitClassDependencies;
        _sosPeerAdapter = sosAdapter;

        _viewAllowList = nil;
        _container = container;
        _accountTracker = accountTracker;
        _lockStateTracker = lockStateTracker;
        [_lockStateTracker addLockStateObserver:self];
        _reachabilityTracker = reachabilityTracker;
        _personaAdapter = personaAdapter;
        _accountsAdapter = accountsAdapter;
        
        // Ensure that the APS receiver comes up, even if we don't have a client for it yet
        (void)[OctagonAPSReceiver receiverForNamedDelegatePort:SecCKKSAPSNamedPort
                                            apsConnectionClass:cloudKitClassDependencies.apsConnectionClass];

        _operationQueue = [[NSOperationQueue alloc] init];

        _pendingSyncCallbacks = [[NSMutableDictionary alloc] init];

        _completedSecCKKSInitialize = [[CKKSCondition alloc] init];

        _policy = nil;
        _policyLoaded = [[CKKSCondition alloc] init];

        _listener = [NSXPCListener anonymousListener];
        _listener.delegate = self;
        [_listener resume];

        // Start listening for CK account status (for sync callbacks)
        [_accountTracker registerForNotificationsOfCloudKitAccountStatusChange:self];
    }
    return self;
}

- (BOOL)allowClientRPC:(NSError**)error
{
    if (!OctagonSupportsPersonaMultiuser()) {
        if(![self.personaAdapter currentThreadIsForPrimaryiCloudAccount]) {
            secnotice("ckks", "Rejecting client RPC for non-primary persona");
            if(error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain code:CKKSErrorNotSupported description:@"CKKS APIs do not support non-primary users"];
            }
            return NO;
        }
    }
    
    return YES;
}

- (BOOL)waitForTrustReady {
    static dispatch_once_t onceToken;
    __block BOOL success = YES;
    dispatch_once(&onceToken, ^{
        OTManager* manager = [OTManager manager];
        if (![manager waitForReady:[[OTControlArguments alloc] init] wait:2*NSEC_PER_SEC]) {
            success = NO;
        }
    });
    return success;
}

- (void)setupAnalytics
{
    WEAKIFY(self);

    // Tests shouldn't continue here; it leads to entitlement crashes with CloudKit if the mocks aren't enabled when this function runs
    if(SecCKKSTestsEnabled()) {
        return;
    }

    [[CKKSAnalytics logger] AddMultiSamplerForName:@"CKKS-healthSummary" withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
        STRONGIFY(self);
        if(!self) {
            return nil;
        }

        NSError* sosCircleError = nil;
        SOSCCStatus sosStatus = [self.sosPeerAdapter circleStatus:&sosCircleError];
        if(sosCircleError) {
            ckkserror_global("manager", " couldn't fetch sos status for SF report: %@", sosCircleError);
        }

        NSMutableDictionary* values = [NSMutableDictionary dictionary];
        BOOL inCircle = (sosStatus == kSOSCCInCircle);
        if (inCircle) {
            [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastInCircle];
        }
        values[CKKSAnalyticsInCircle] = @(inCircle);

        BOOL validCredentials = self.accountTracker.currentCKAccountInfo.hasValidCredentials;
        if (!validCredentials) {
            values[CKKSAnalyticsValidCredentials] = @(validCredentials);
        }

        NSArray<NSString *>* keys = @[ CKKSAnalyticsLastUnlock, CKKSAnalyticsLastInCircle];
        for (NSString * key in keys) {
            NSDate *date = [[CKKSAnalytics logger] datePropertyForKey:key];
            values[key] = @([CKKSAnalytics fuzzyDaysSinceDate:date]);
        }
        return values;
    }];

    CKKSKeychainView* ckks = [self ckksAccountSyncForContainer:SecCKKSContainerName
                                                     contextID:OTDefaultContext];
    if(!ckks) {
        return;
    }

    for (NSString* viewName in [ckks viewList]) {
        [[CKKSAnalytics logger] AddMultiSamplerForName:[NSString stringWithFormat:@"CKKS-%@-healthSummary", viewName]
                                      withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport
                                                 block:^NSDictionary<NSString *,NSNumber *> *{
            STRONGIFY(self);
            if(!self) {
                return nil;
            }

            NSError* sosCircleError = nil;
            SOSCCStatus sosStatus = [self.sosPeerAdapter circleStatus:&sosCircleError];
            if(sosCircleError) {
                ckkserror_global("manager", " couldn't fetch sos status for SF report: %@", sosCircleError);
            }
            BOOL inCircle = (sosStatus == kSOSCCInCircle);
            NSMutableDictionary* values = [NSMutableDictionary dictionary];
            CKKSKeychainViewState* view = [ckks viewStateForName:viewName];
            if(!view) {
                return values;
            }

            NSDate* dateOfLastSyncClassA = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassA zoneName:view.zoneName];
            NSDate* dateOfLastSyncClassC = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassC zoneName:view.zoneName];
            NSDate* dateOfLastKSR = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastKeystateReady zoneName:view.zoneName];

            NSInteger fuzzyDaysSinceClassASync = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastSyncClassA];
            NSInteger fuzzyDaysSinceClassCSync = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastSyncClassC];
            NSInteger fuzzyDaysSinceKSR = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastKSR];
            [values setValue:@(fuzzyDaysSinceClassASync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassASync", viewName]];
            [values setValue:@(fuzzyDaysSinceClassCSync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassCSync", viewName]];
            [values setValue:@(fuzzyDaysSinceKSR) forKey:[NSString stringWithFormat:@"%@-daysSinceLastKeystateReady", viewName]];

            NSError* ckmeError = nil;
            NSNumber* syncedItemRecords = [CKKSMirrorEntry countsWithContextID:view.contextID
                                                                        zoneID:view.zoneID
                                                                         error:&ckmeError];
            if(ckmeError || !syncedItemRecords) {
                ckkserror_global("manager", "couldn't fetch CKMirror counts for %@: %@", view.zoneID, ckmeError);
            } else {
                ckksnotice("metrics", view, "View has %@ item ckrecords", syncedItemRecords);
                values[[NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsNumberOfSyncItems]] = [CKKSAnalytics fuzzyNumber:syncedItemRecords];
            }

            NSError* tlkShareCountError = nil;
            NSNumber* tlkShareCount = [CKKSTLKShareRecord countsWithContextID:view.contextID
                                                                       zoneID:view.zoneID
                                                                        error:&tlkShareCountError];
            if(tlkShareCountError || !tlkShareCount) {
                ckkserror_global("manager", "couldn't fetch CKKSTLKShare counts for %@: %@", view.zoneID, tlkShareCountError);
            } else {
                ckksnotice("metrics", view, "View has %@ tlkshares", tlkShareCount);
                values[[NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsNumberOfTLKShares]] = [CKKSAnalytics fuzzyNumber:tlkShareCount];
            }

            NSError* syncKeyCountError = nil;
            NSNumber* syncKeyCount = [CKKSKey countsWithContextID:view.contextID
                                                           zoneID:view.zoneID
                                                            error:&syncKeyCountError];
            if(syncKeyCountError || !syncKeyCount) {
                ckkserror_global("manager", "couldn't fetch CKKSKey counts for %@: %@", view.zoneID, syncKeyCountError);
            } else {
                ckksnotice("metrics", view, "View has %@ sync keys", syncKeyCount);
                values[[NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsNumberOfSyncKeys]] = syncKeyCount;
            }

            BOOL hasTLKs = [view.viewKeyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady];
            BOOL syncedClassARecently = fuzzyDaysSinceClassASync >= 0 && fuzzyDaysSinceClassASync < 7;
            BOOL syncedClassCRecently = fuzzyDaysSinceClassCSync >= 0 && fuzzyDaysSinceClassCSync < 7;
            BOOL incomingQueueIsErrorFree = ckks.lastIncomingQueueOperation.error == nil;
            BOOL outgoingQueueIsErrorFree = ckks.lastOutgoingQueueOperation.error == nil;

            NSString* hasTLKsKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsHasTLKs];
            NSString* syncedClassARecentlyKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsSyncedClassARecently];
            NSString* syncedClassCRecentlyKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsSyncedClassCRecently];
            NSString* incomingQueueIsErrorFreeKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsIncomingQueueIsErrorFree];
            NSString* outgoingQueueIsErrorFreeKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsOutgoingQueueIsErrorFree];

            values[hasTLKsKey] = @(hasTLKs);
            values[syncedClassARecentlyKey] = @(syncedClassARecently);
            values[syncedClassCRecentlyKey] = @(syncedClassCRecently);
            values[incomingQueueIsErrorFreeKey] = @(incomingQueueIsErrorFree);
            values[outgoingQueueIsErrorFreeKey] = @(outgoingQueueIsErrorFree);

            BOOL weThinkWeAreInSync = inCircle && hasTLKs && syncedClassARecently && syncedClassCRecently && incomingQueueIsErrorFree && outgoingQueueIsErrorFree;
            NSString* inSyncKey = [NSString stringWithFormat:@"%@-%@", viewName, CKKSAnalyticsInSync];
            values[inSyncKey] = @(weThinkWeAreInSync);

            return values;
        }];
    }
}

dispatch_queue_t globalZoneStateQueue = NULL;
dispatch_once_t globalZoneStateQueueOnce;

// We can't load the rate limiter in an init method, as the method might end up calling itself (if the database layer isn't yet initialized).
// Lazy-load it here.
- (CKKSRateLimiter*)getGlobalRateLimiter {
    dispatch_once(&globalZoneStateQueueOnce, ^{
        globalZoneStateQueue = dispatch_queue_create("CKKS global zone state", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
    });

    if(_globalRateLimiter != nil) {
        return _globalRateLimiter;
    }

    __block CKKSRateLimiter* blocklimit = nil;

    dispatch_sync(globalZoneStateQueue, ^{
        NSError* error = nil;

        // Special object containing state for all zones. Currently, just the rate limiter.
        CKKSZoneStateEntry* allEntry = [CKKSZoneStateEntry tryFromDatabase:CKKSDefaultContextID zoneName:@"all" error:&error];

        if(error) {
            ckkserror_global("manager", " couldn't load global zone state: %@", error);
        }

        if(!error && allEntry.rateLimiter) {
            blocklimit = allEntry.rateLimiter;
        } else {
            blocklimit = [[CKKSRateLimiter alloc] init];
        }
    });
    _globalRateLimiter = blocklimit;
    return _globalRateLimiter;
}

- (void)lockStateChangeNotification:(bool)unlocked
{
    if (unlocked) {
        [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastUnlock];
    }
}

- (CKKSKeychainView*)restartCKKSAccountSync:(CKKSKeychainView*)view
{
    TPSyncingPolicy* policy = view.syncingPolicy;

    CKKSKeychainView* restartedView = [self restartCKKSAccountSyncWithoutSettingPolicy:view];
    [restartedView setCurrentSyncingPolicy:policy policyIsFresh:NO];
    return restartedView;
}

- (CKKSKeychainView* _Nullable)ckksAccountSyncForContainer:(NSString*)container
                                                 contextID:(NSString*)contextID
{
    CKKSKeychainView* retval = [[OTManager manager] ckksAccountSyncForContainer:container
                                                                      contextID:contextID
                                                                possibleAccount:nil];
    if(retval == nil) {
        ckksnotice_global("ckksAccountSyncForContainer", "failed to get CKKSKeychainView");
    }
    return retval;
}

- (CKKSKeychainView*)restartCKKSAccountSyncWithoutSettingPolicy:(CKKSKeychainView*)view
{
    return [[OTManager manager] restartCKKSAccountSyncWithoutSettingPolicy:view];
}

- (void)registerSyncStatusCallback: (NSString*) uuid callback: (SecBoolNSErrorCallback) callback {
    // Someone is requesting future notification of this item.
    @synchronized(self.pendingSyncCallbacks) {
        ckksnotice_global("ckkscallback", "registered callback for UUID: %@", uuid);
        self.pendingSyncCallbacks[uuid] = callback;
    }
}

- (SecBoolNSErrorCallback _Nullable)claimCallbackForUUID:(NSString* _Nullable)uuid
{
    if(!uuid) {
        return nil;
    }

    @synchronized(self.pendingSyncCallbacks) {
        SecBoolNSErrorCallback callback = self.pendingSyncCallbacks[uuid];

        if(callback) {
            ckksnotice_global("ckkscallback", "fetched UUID: %@", uuid);
        }

        self.pendingSyncCallbacks[uuid] = nil;
        return callback;
    }
}

- (BOOL)peekCallbackForUUID:(NSString* _Nullable)uuid
{
    if(!uuid) {
        return NO;
    }

    @synchronized(self.pendingSyncCallbacks) {
        return [self.pendingSyncCallbacks.allKeys containsObject:uuid];
    }
}

- (NSSet<NSString*>*)pendingCallbackUUIDs
{
    @synchronized(self.pendingSyncCallbacks) {
        return [[self.pendingSyncCallbacks allKeys] copy];
    }
}

- (void)cloudkitAccountStateChange:(CKAccountInfo* _Nullable)oldAccountInfo to:(CKAccountInfo*)currentAccountInfo
{
    if(currentAccountInfo.accountStatus == CKAccountStatusAvailable && currentAccountInfo.hasValidCredentials) {
        // Account is okay!
    } else {
        @synchronized(self.pendingSyncCallbacks) {
            if(self.pendingSyncCallbacks.count > 0) {
                ckksnotice_global("ckkscallback", "No CK account; failing all pending sync callbacks");

                for(NSString* uuid in [self.pendingSyncCallbacks allKeys]) {
                    [CKKSViewManager callSyncCallbackWithErrorNoAccount:self.pendingSyncCallbacks[uuid]];
                }

                [self.pendingSyncCallbacks removeAllObjects];
            }
        }
    }
}

+ (void)callSyncCallbackWithErrorNoAccount:(SecBoolNSErrorCallback)syncCallback
{
    // I don't love using this domain, but PCS depends on it
    syncCallback(false, [NSError errorWithDomain:@"securityd"
                                            code:errSecNotLoggedIn
                                     description:@"No iCloud account available; item is not expected to sync"]);
}

- (void) handleKeychainEventDbConnection: (SecDbConnectionRef) dbconn source:(SecDbTransactionSource)txionSource added: (SecDbItemRef) added deleted: (SecDbItemRef) deleted {
    
    NSUUID* addedMuserUUID = nil;
    NSUUID* deletedMuserUUID = nil;
    BOOL isAddedSingleUserKeychainUUID = NO;
    BOOL isDeletedSingleUserKeychainUUID = NO;
    
    if (!added && !deleted) {
        ckkserror_global("handleKeychainEventDbConnection", "both added and deleted SecDbItemRefs are nil, returning");
        return;
    }
    
    if (added) {
        NSData* addedMUSRData = (__bridge NSData*)SecDbItemGetValue(added, &v8musr, NULL);
        if ([addedMUSRData length] > 0) {
            addedMuserUUID = addedMUSRData ? [[NSUUID alloc] initWithUUIDBytes: addedMUSRData.bytes] : nil;
        } else if ([addedMUSRData isEqualToData: (__bridge NSData*)SecMUSRGetSingleUserKeychainUUID()]) {
            isAddedSingleUserKeychainUUID = YES;
        }
    }
    
    if (deleted) {
        NSData* deletedMUSRData = (__bridge NSData*)SecDbItemGetValue(deleted, &v8musr, NULL);
        if ([deletedMUSRData length] > 0) {
            deletedMuserUUID = deletedMUSRData ? [[NSUUID alloc] initWithUUIDBytes: deletedMUSRData.bytes] : nil;
        } else if ([deletedMUSRData isEqualToData: (__bridge NSData*)SecMUSRGetSingleUserKeychainUUID()]) {
            isDeletedSingleUserKeychainUUID = YES;
        }
    }
    
    if (addedMuserUUID && deletedMuserUUID) {
        if (![addedMuserUUID.UUIDString isEqualToString:deletedMuserUUID.UUIDString]) {
            ckkserror_global("handleKeychainEventDbConnection", "musr for added and deleted are different. added's musr:%@, deleted's musr: %@", addedMuserUUID, deletedMuserUUID);
            return;
        }
    }
    
    if (added && deleted && (isAddedSingleUserKeychainUUID != isDeletedSingleUserKeychainUUID)) {
        ckkserror_global("handleKeychainEventDbConnection", "added's SingleUserKeychainUUID is different from deleted's. added's musr:%d, deleted's musr: %d", isAddedSingleUserKeychainUUID, isDeletedSingleUserKeychainUUID);
        return;
    }
    
    NSString* persona = nil;
    
    // if added and deleted are populated, it should have the same musr value, so we can just pick either to use
    if (addedMuserUUID || deletedMuserUUID) {
        persona = addedMuserUUID ? addedMuserUUID.UUIDString : deletedMuserUUID.UUIDString;
    }
    
    CKKSKeychainView* ckks = nil;
    
    if (persona == nil || isAddedSingleUserKeychainUUID) {
        ckks = [[OTManager manager] ckksAccountSyncForContainer:SecCKKSContainerName
                                                      contextID:OTDefaultContext
                                                possibleAccount:nil];
    } else if (OctagonSupportsPersonaMultiuser()) {
        NSArray<TPSpecificUser*>* activeAccounts = [self.accountsAdapter inflateAllTPSpecificUsers:SecCKKSContainerName octagonContextID:OTDefaultContext];
        TPSpecificUser* possibleAccount = nil;
        
        for (TPSpecificUser* activeAccount in activeAccounts) {
            if ([activeAccount.personaUniqueString isEqualToString: persona]) {
                possibleAccount = activeAccount;
                break;
            }
        }
        
        if (possibleAccount == nil) {
            ckkserror_global("handleKeychainEventDbConnection", "did not find an active account for the persona");
            return;
        }
        ckkserror_global("handleKeychainEventDbConnection", "using tpspecific user: %@", possibleAccount);
        ckks = [[OTManager manager] ckksAccountSyncForContainer:possibleAccount.cloudkitContainerName
                                                      contextID:possibleAccount.octagonContextID
                                                possibleAccount:possibleAccount];
    }
    
    if (!ckks) {
        ckkserror_global("handleKeychainEventDbConnection", "ckks view is nil! returning.");
        return;
    }
    
    [ckks handleKeychainEventDbConnection:dbconn
                                   source:txionSource
                                    added:added
                                  deleted:deleted
                              rateLimiter:self.globalRateLimiter];
}

-(NSError*)defaultViewError
{
    return [NSError errorWithDomain:CKKSErrorDomain
                                 code:CKKSNoSuchView
                           userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for %@, %@", SecCKKSContainerName, OTDefaultContext]}];
}


-(void)setCurrentItemForAccessGroup:(NSData* _Nonnull)newItemPersistentRef
                               hash:(NSData*)newItemSHA1
                        accessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                           viewHint:(NSString*)viewHint
                          replacing:(NSData* _Nullable)oldCurrentItemPersistentRef
                               hash:(NSData*)oldItemSHA1
                           complete:(void (^) (NSError* operror)) complete
{
    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];
    
    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            complete(findViewError);
        } else {
            complete([self defaultViewError]);
        }
        return;
    }

    [view setCurrentItemForAccessGroup:newItemPersistentRef
                                  hash:newItemSHA1
                           accessGroup:accessGroup
                            identifier:identifier
                              viewHint:viewHint
                             replacing:oldCurrentItemPersistentRef
                                  hash:oldItemSHA1
                              complete:complete];
}

- (void)unsetCurrentItemsForAccessGroup:(NSString*)accessGroup
                            identifiers:(NSArray<NSString*>*)identifiers
                               viewHint:(NSString*)viewHint
                               complete:(void (^)(NSError* operror))complete
{
    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            complete(findViewError);
        } else {
            complete([self defaultViewError]);
        }
        return;
    }

    [view unsetCurrentItemsForAccessGroup:accessGroup
                              identifiers:identifiers
                                 viewHint:viewHint
                                 complete:complete];
}

-(void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                           viewHint:(NSString*)viewHint
                    fetchCloudValue:(bool)fetchCloudValue
                           complete:(void (^) (CKKSCurrentItemData* data, NSError* operror)) complete
{
    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            complete(nil, findViewError);
        } else {
            complete(nil, [self defaultViewError]);
        }
        return;
    }

    [view getCurrentItemForAccessGroup:accessGroup
                            identifier:identifier
                              viewHint:viewHint
                       fetchCloudValue:fetchCloudValue
                              complete:complete];
}

+ (instancetype)manager
{
    return [OTManager manager].viewManager;
}

-(void)notifyNewTLKsInKeychain {
    // Why two functions here? Limitation of OCMock, unfortunately: can't stub and expect the same method
    ckksnotice_global("ckksbackup", "New TLKs have arrived");
    [self syncBackupAndNotifyAboutSync];
}

- (void)syncBackupAndNotifyAboutSync {
    SOSAccount* account = (__bridge SOSAccount*)SOSKeychainAccountGetSharedAccount();
    
    if(!account) {
        ckksnotice_global("ckks", "Failed to get account object");
        return;
    }

    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        CFErrorRef error = NULL;
        NSSet* ignore = CFBridgingRelease(SOSAccountCopyBackupPeersAndForceSync(txn, &error));
        (void)ignore;

        if(error) {
            ckkserror_global("backup", "Couldn't process sync with backup peers: %@", error);
        } else {
            ckksnotice_global("ckksbackup", "telling CloudServices about TLK arrival");
            notify_post(kSecItemBackupNotification);
        };
        CFReleaseNull(error);
    }];
}

- (NSArray<CKKSKeychainBackedKey*>* _Nullable)currentTLKsFilteredByPolicy:(BOOL)restrictToPolicy error:(NSError**)error
{
    NSError* findViewError = nil;
    CKKSKeychainView* ckks = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!ckks || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (error) {
            if (findViewError) {
                *error = findViewError;
            } else {
                *error = [self defaultViewError];
            }
        }
        return nil;
    }
    
    CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* keyFetchOperation = [ckks findKeySets:NO];
    [keyFetchOperation timeout:10*NSEC_PER_SEC];
    [keyFetchOperation waitUntilFinished];

    if(keyFetchOperation.error) {
        ckkserror_global("ckks", "Error getting keyset: %@", keyFetchOperation.error);
        if(error) {
            *error = keyFetchOperation.error;
        }
        return nil;
    }

    NSMutableArray<CKKSKeychainBackedKey*>* tlks = [NSMutableArray array];

    for(CKRecordZoneID* zoneID in keyFetchOperation.intendedZoneIDs) {
        if(restrictToPolicy && ![ckks.syncingPolicy.viewsToPiggybackTLKs containsObject:zoneID.zoneName]) {
            continue;
        }

        CKKSCurrentKeySet* keyset = keyFetchOperation.keysets[zoneID];

        if(!keyset) {
            ckkserror_global("ckks", "Do not have keyset: %@", keyset);
            continue;
        }

        if(keyset.tlk) {
            // Keys provided by this function must have the key material loaded
            NSError* loadError = nil;

            CKKSKeychainBackedKey* keycore = [keyset.tlk ensureKeyLoadedForContextID:CKKSDefaultContextID  error:&loadError];
            if(keycore == nil || loadError) {
                ckkserror_global("ckks", "Error loading key: %@", loadError);
                if(error) {
                    *error = loadError;
                }
            } else {
                [tlks addObject:[keycore copy]];
            }
        } else {
            ckkserror_global("ckks", "Do not have TLK: %@", keyset);
        }
    }

    return tlks;
}

#pragma mark - RPCs to manage and report state

- (void)performanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *counter))reply {
    reply(@{});
}

- (void)rpcResetLocal:(NSString*)viewName
                reply:(void(^)(NSError* result))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a resetLocal RPC: %@", clientError);
        reply(clientError);
        return;
    }
    
    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([self defaultViewError]);
        }
        return;
    }

    NSSet<NSString*>* viewNamesToReset = nil;

    if(viewName) {
        viewNamesToReset = [NSSet setWithObject:viewName];
    }

    [view rpcResetLocal:viewNamesToReset reply:reply];
}

- (void)rpcResetCloudKit:(NSString*)viewName
                  reason:(NSString *)reason
                   reply:(void(^)(NSError* result))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a resetCloudKit RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([self defaultViewError]);
        }
        return;
    }

    NSSet<NSString*>* viewNamesToReset = nil;

    if(viewName) {
        viewNamesToReset = [NSSet setWithObject:viewName];
    }

    [view rpcResetCloudKit:viewNamesToReset reply:reply];
}

- (void)rpcResync:(NSString*)viewName
            reply:(void(^)(NSError* result))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a resync RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([self defaultViewError]);
        }
        return;
    }

    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    op.name = @"rpc-resync-cloudkit";
    __weak __typeof(op) weakOp = op;

    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [op setCompletionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        ckksnotice_global("ckks", "Ending rsync-CloudKit rpc with %@", strongOp.error);
        reply(CKXPCSuitableError(strongOp.error));
    }];

    ckksnotice("ckksresync", view, "Beginning resync (CloudKit) for %@", view);

    // TODO: insert view name?
    CKKSSynchronizeOperation* resyncOp = [view resyncWithCloud];
    [op addSuccessDependency:resyncOp];

    [op timeout:240*NSEC_PER_SEC];
    [self.operationQueue addOperation:op];
}

- (void)rpcResyncLocal:(NSString*)viewName
                 reply:(void(^)(NSError* result))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a resync-local RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([self defaultViewError]);
        }
        return;
    }

    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    op.name = @"rpc-resync-local";
    __weak __typeof(op) weakOp = op;
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [op setCompletionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        ckksnotice_global("ckks", "Ending rsync-local rpc with %@", strongOp.error);
        reply(CKXPCSuitableError(strongOp.error));
    }];

    ckksnotice("ckksresync", view, "Beginning resync (local) for %@", view);

    // TODO: insert view name?
    CKKSLocalSynchronizeOperation* resyncOp = [view resyncLocal];
    [op addSuccessDependency:resyncOp];

    [op timeout:240*NSEC_PER_SEC];
}

- (void)rpcStatus:(NSString*)viewName
        fast:(BOOL)fast
        waitForNonTransientState:(dispatch_time_t)nonTransientStateTimeout
        reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a status RPC: %@", clientError);
        reply(nil, clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(nil, findViewError);
        } else {
            reply(nil, [self defaultViewError]);
        }
        return;
    }

    [view rpcStatus:viewName
          fast:fast
          waitForNonTransientState:nonTransientStateTimeout
          reply:reply];
}

- (void)rpcFetchAndProcessChanges:(NSString* _Nullable __unused)viewName classA:(bool)classAError onlyIfNoRecentFetch:(bool)onlyIfNoRecentFetch reply:(void(^)(NSError* _Nullable result))reply
{
    if(!SecCKKSIsEnabled()) {
        ckksinfo_global("ckks", "Skipping fetchAndProcessCKChanges due to disabled CKKS");
        reply([NSError errorWithDomain:CKKSErrorDomain code:CKKSNotInitialized description:@"CKKS disabled"]);
        return;
    }

    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a fetch-and-process RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([self defaultViewError]);
        }
        return;
    }

    if(onlyIfNoRecentFetch) {
        NSDate *earliestFetchTime = view.earliestFetchTime;
        if(earliestFetchTime.timeIntervalSinceNow > -600) {
            ckksnotice_global("ckks", "Skipping rpcFetchAndProcessChanges because a recent fetch was performed");
            reply(nil);
            return;
        }
    }

    CKKSResultOperation* blockOp = [CKKSResultOperation named:@"rpc-fetch-and-process-result" withBlock:^{}];

    __weak __typeof(blockOp) weakBlockOp = blockOp;
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [blockOp setCompletionBlock:^{
        __strong __typeof(blockOp) strongBlockOp = weakBlockOp;
        [strongBlockOp allDependentsSuccessful];
        reply(CKXPCSuitableError(strongBlockOp.error));
    }];

    ckksnotice("ckks", view, "Beginning fetch for %@", view);

    CKKSResultOperation* op = [view rpcFetchAndProcessIncomingQueue:nil
                                                            because:CKKSFetchBecauseAPIFetchRequest
                                               errorOnClassAFailure:classAError];
    [blockOp addSuccessDependency:op];

    [self.operationQueue addOperation: [blockOp timeout:(SecCKKSTestsEnabled() ? NSEC_PER_SEC * 5 : NSEC_PER_SEC * 300)]];
}

- (void)rpcPushOutgoingChanges:(NSString* _Nullable __unused)viewName
                         reply:(void(^)(NSError* _Nullable result))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a push RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([self defaultViewError]);
        }
        return;
    }

    CKKSResultOperation* blockOp = [[CKKSResultOperation alloc] init];
    blockOp.name = @"rpc-push";
    __weak __typeof(blockOp) weakBlockOp = blockOp;
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [blockOp setCompletionBlock:^{
        __strong __typeof(blockOp) strongBlockOp = weakBlockOp;
        [strongBlockOp allDependentsSuccessful];
        reply(CKXPCSuitableError(strongBlockOp.error));
    }];

    ckksnotice("ckks-rpc", view, "Beginning push for %@", view);

    CKKSResultOperation* op = [view rpcProcessOutgoingQueue:viewName ? [NSSet setWithObject:viewName] : nil
                                             operationGroup:[CKOperationGroup CKKSGroupWithName:@"rpc-push"]];
    [blockOp addSuccessDependency:op];

    [self.operationQueue addOperation: [blockOp timeout:(SecCKKSTestsEnabled() ? NSEC_PER_SEC * 2 : NSEC_PER_SEC * 300)]];
}

- (void)rpcGetCKDeviceIDWithReply:(void (^)(NSString *))reply {
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a push RPC: %@", clientError);
        reply(@"error");
        return;
    }

    reply(self.accountTracker.ckdeviceID);
}

- (void)rpcCKMetric:(NSString *)eventName attributes:(NSDictionary *)attributes reply:(void (^)(NSError *))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a ckmetric RPC: %@", clientError);
        reply(clientError);
        return;
    }

    if (eventName == NULL) {
        reply([NSError errorWithDomain:CKKSErrorDomain
                                  code:CKKSNoMetric
                           description:@"No metric name"]);
        return;
    }
    SecEventMetric *metric = [[SecEventMetric alloc] initWithEventName:eventName];
    [attributes enumerateKeysAndObjectsUsingBlock:^(id  _Nonnull key, id  _Nonnull obj, BOOL * _Nonnull __unused stop) {
        metric[key] = obj;
    }];
    [[SecMetrics managerObject] submitEvent:metric];
    reply(NULL);
}

- (NSError* _Nullable)sanitizeErrorDomain:(NSError* _Nullable)error
{
    if(error == nil) {
        return nil;
    }

    // CKKS commited this sin a long time ago. Let's not keep it going in new API.
    // Also, ensure that these errors can transit XPC to processes that don't link CK
    if([error.domain isEqualToString:@"securityd"]) {
        return CKXPCSuitableError([NSError errorWithDomain:NSOSStatusErrorDomain
                                                      code:error.code
                                                  userInfo:error.userInfo]);
    }

    return CKXPCSuitableError(error);
}

- (void)proposeTLKForSEView:(NSString*)seViewName
                proposedTLK:(CKKSExternalKey *)proposedTLK
              wrappedOldTLK:(CKKSExternalKey * _Nullable)wrappedOldTLK
                  tlkShares:(NSArray<CKKSExternalTLKShare*>*)shares
                      reply:(void(^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a proposeTLK RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* ckks = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!ckks || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([NSError errorWithDomain:CKKSErrorDomain
                                      code:CKKSNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No CKKS for %@, %@", SecCKKSContainerName, OTDefaultContext]}]);
        }
        return;
    }

    [ckks proposeTLKForExternallyManagedView:seViewName
                                 proposedTLK:proposedTLK
                               wrappedOldTLK:wrappedOldTLK
                                   tlkShares:shares
                                       reply:^(NSError * _Nullable error) {
        reply([self sanitizeErrorDomain:error]);
    }];
}

- (void)fetchSEViewKeyHierarchy:(NSString*)seViewName
                     forceFetch:(BOOL)forceFetch
                          reply:(void (^)(CKKSExternalKey* _Nullable currentTLK,
                                          NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                          NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                          NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a fetchSEViewHierarchy RPC: %@", clientError);
        reply(nil, nil, nil, clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* ckks = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!ckks || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(nil, nil, nil, findViewError);
        } else {
            reply(nil,
                  nil,
                  nil,
                  [NSError errorWithDomain:CKKSErrorDomain
                                      code:CKKSNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No CKKS for %@, %@", SecCKKSContainerName, OTDefaultContext]}]);
        }
        return;
    }

    [ckks fetchExternallyManagedViewKeyHierarchy:seViewName
                                      forceFetch:forceFetch
                                           reply:^(CKKSExternalKey* _Nullable currentTLK,
                                                   NSArray<CKKSExternalKey*>* _Nullable pastTLKs,
                                                   NSArray<CKKSExternalTLKShare*>* _Nullable currentTLKShares,
                                                   NSError* _Nullable error) {
        reply(currentTLK, pastTLKs, currentTLKShares, [self sanitizeErrorDomain:error]);
    }];
}

- (void)modifyTLKSharesForSEView:(NSString*)seViewName
                          adding:(NSArray<CKKSExternalTLKShare*>*)sharesToAdd
                        deleting:(NSArray<CKKSExternalTLKShare*>*)sharesToDelete
                          reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a modifyTLKShares RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* ckks = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!ckks || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([NSError errorWithDomain:CKKSErrorDomain
                                      code:CKKSNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No CKKS for %@, %@", SecCKKSContainerName, OTDefaultContext]}]);
        }
        return;
    }

    [ckks modifyTLKSharesForExternallyManagedView:seViewName
                                           adding:sharesToAdd
                                         deleting:sharesToDelete
                                            reply:^(NSError * _Nullable error) {
        reply([self sanitizeErrorDomain:error]);
    }];
}

- (void)deleteSEView:(NSString*)seViewName
               reply:(void (^)(NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a deleteSEView RPC: %@", clientError);
        reply(clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* ckks = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!ckks || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(findViewError);
        } else {
            reply([NSError errorWithDomain:CKKSErrorDomain
                                      code:CKKSNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No CKKS for %@, %@", SecCKKSContainerName, OTDefaultContext]}]);
        }
        return;
    }

    [ckks resetExternallyManagedCloudKitView:seViewName
                                       reply:^(NSError * _Nullable error) {
        reply([self sanitizeErrorDomain:error]);
    }];
}

- (void)toggleHavoc:(void (^)(BOOL havoc, NSError* _Nullable error))reply
{
    NSError* clientError = nil;
    if(![self allowClientRPC:&clientError]) {
        secnotice("ckks", "Rejecting a toggleHavoc RPC: %@", clientError);
        reply(NO, clientError);
        return;
    }

    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(NO, findViewError);
        } else {
            reply(NO, [self defaultViewError]);
        }
        return;
    }

    [view toggleHavoc:reply];
}

- (void)pcsMirrorKeysForServices:(NSDictionary<NSNumber*,NSArray<NSData*>*>*)services reply:(void (^)(NSDictionary<NSNumber*,NSArray<NSData*>*>* _Nullable result, NSError* _Nullable error))reply
{
    NSError* findViewError = nil;
    CKKSKeychainView* view = [[OTManager manager] ckksForClientRPC:[[OTControlArguments alloc] init]
                                                   createIfMissing:YES
                                           allowNonPrimaryAccounts:YES
                                                             error:&findViewError];

    if (!view || findViewError) {
        ckksnotice_global("ckks", "No CKKS view for %@, %@, error: %@", SecCKKSContainerName, OTDefaultContext, findViewError);
        if (findViewError) {
            reply(nil, findViewError);
        } else {
            reply(nil, [self defaultViewError]);
        }
        return;
    }

    [view pcsMirrorKeysForServices:services reply:reply];
}

-(void)xpc24HrNotification {
    // XPC has poked us and said we should do some cleanup!
    ckksnotice_global("ckks", "Received a 24hr notification from XPC");

    if (![self waitForTrustReady]) {
        ckksnotice_global("ckks", "Trust not ready, still going ahead");
    }

    // For now, poke the views and tell them to update their device states if they'd like


    CKKSKeychainView* ckks = [self ckksAccountSyncForContainer:SecCKKSContainerName
                                                     contextID:OTDefaultContext];
    CKOperationGroup* group = [CKOperationGroup CKKSGroupWithName:@"periodic-device-state-update"];
    [ckks updateDeviceState:true waitForKeyHierarchyInitialization:30*NSEC_PER_SEC ckoperationGroup:group];
    [ckks xpc24HrNotification];
}

- (void)haltAll
{
    [[OTManager manager] haltAll];
}

- (void)dropAllActors
{
    [[OTManager manager] dropAllActors];
}

#endif // OCTAGON
@end
