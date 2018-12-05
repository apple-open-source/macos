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

#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"

#import "keychain/ot/OTDefines.h"

#import "SecEntitlements.h"

#include <securityd/SecDbItem.h>
#include <securityd/SecDbKeychainItem.h>
#include <securityd/SecItemSchema.h>
#include <Security/SecureObjectSync/SOSViews.h>

#import <Foundation/NSXPCConnection.h>
#import <Foundation/NSXPCConnection_Private.h>

#include <Security/SecureObjectSync/SOSAccount.h>
#include <Security/SecItemBackup.h>

#if OCTAGON
#import <CloudKit/CloudKit.h>
#import <CloudKit/CloudKit_Private.h>

#import <SecurityFoundation/SFKey.h>
#import <SecurityFoundation/SFKey_Private.h>

#import "CKKSAnalytics.h"
#endif

@interface CKKSViewManager () <NSXPCListenerDelegate>
#if OCTAGON
@property NSXPCListener *listener;

// Once you set these, all CKKSKeychainViews created will use them
@property (readonly) Class<CKKSFetchRecordZoneChangesOperation> fetchRecordZoneChangesOperationClass;
@property (readonly) Class<CKKSFetchRecordsOperation> fetchRecordsOperationClass;
@property (readonly) Class<CKKSQueryOperation> queryOperationClass;
@property (readonly) Class<CKKSModifySubscriptionsOperation> modifySubscriptionsOperationClass;
@property (readonly) Class<CKKSModifyRecordZonesOperation> modifyRecordZonesOperationClass;
@property (readonly) Class<CKKSAPSConnection> apsConnectionClass;
@property (readonly) Class<CKKSNotifier> notifierClass;
@property (readonly) Class<CKKSNSNotificationCenter> nsnotificationCenterClass;

@property NSMutableDictionary<NSString*, CKKSKeychainView*>* views;

@property NSMutableDictionary<NSString*, SecBoolNSErrorCallback>* pendingSyncCallbacks;
@property CKKSNearFutureScheduler* savedTLKNotifier;;
@property NSOperationQueue* operationQueue;

@property NSMapTable<dispatch_queue_t, id<CKKSPeerUpdateListener>>* peerChangeListeners;
#endif
@end

#if OCTAGON
@interface CKKSViewManager (lockstateTracker) <CKKSLockStateNotification>
@end
#endif

@implementation CKKSViewManager
#if OCTAGON

- (instancetype)initCloudKitWithContainerName: (NSString*) containerName usePCS:(bool)usePCS {
    return [self initWithContainerName:containerName
                                usePCS:usePCS
  fetchRecordZoneChangesOperationClass:[CKFetchRecordZoneChangesOperation class]
            fetchRecordsOperationClass:[CKFetchRecordsOperation class]
                   queryOperationClass:[CKQueryOperation class]
     modifySubscriptionsOperationClass:[CKModifySubscriptionsOperation class]
       modifyRecordZonesOperationClass:[CKModifyRecordZonesOperation class]
                    apsConnectionClass:[APSConnection class]
             nsnotificationCenterClass:[NSNotificationCenter class]
                         notifierClass:[CKKSNotifyPostNotifier class]];
}

- (instancetype)initWithContainerName: (NSString*) containerName
                               usePCS: (bool)usePCS
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
           fetchRecordsOperationClass: (Class<CKKSFetchRecordsOperation>)fetchRecordsOperationClass
                  queryOperationClass: (Class<CKKSQueryOperation>)queryOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
            nsnotificationCenterClass: (Class<CKKSNSNotificationCenter>) nsnotificationCenterClass
                        notifierClass: (Class<CKKSNotifier>) notifierClass
{
    if(self = [super init]) {
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _fetchRecordsOperationClass = fetchRecordsOperationClass;
        _queryOperationClass = queryOperationClass;
        _modifySubscriptionsOperationClass = modifySubscriptionsOperationClass;
        _modifyRecordZonesOperationClass = modifyRecordZonesOperationClass;
        _apsConnectionClass = apsConnectionClass;
        _nsnotificationCenterClass = nsnotificationCenterClass;
        _notifierClass = notifierClass;

        _container = [self makeCKContainer: containerName usePCS:usePCS];
        _accountTracker = [[CKKSCKAccountStateTracker alloc] init:self.container nsnotificationCenterClass:nsnotificationCenterClass];
        _lockStateTracker = [[CKKSLockStateTracker alloc] init];
        [_lockStateTracker addLockStateObserver:self];
        _reachabilityTracker = [[CKKSReachabilityTracker alloc] init];

        _zoneChangeFetcher = [[CKKSZoneChangeFetcher alloc] initWithContainer:_container
                                                                   fetchClass:fetchRecordZoneChangesOperationClass
                                                          reachabilityTracker:_reachabilityTracker];

        _operationQueue = [[NSOperationQueue alloc] init];

        // Backwards from how we'd like, but it's the best way to have weak pointers to CKKSPeerUpdateListener.
        _peerChangeListeners = [NSMapTable strongToWeakObjectsMapTable];

        _views = [[NSMutableDictionary alloc] init];
        _pendingSyncCallbacks = [[NSMutableDictionary alloc] init];

        _initializeNewZones = false;

        _completedSecCKKSInitialize = [[CKKSCondition alloc] init];

        __weak __typeof(self) weakSelf = self;
        _savedTLKNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"newtlks"
                                                                    delay:5*NSEC_PER_SEC
                                                         keepProcessAlive:true
                                                dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                    block:^{
            [weakSelf notifyNewTLKsInKeychain];
        }];

        _listener = [NSXPCListener anonymousListener];
        _listener.delegate = self;
        [_listener resume];

        // If this is a live server, register with notify
        if(!SecCKKSTestsEnabled()) {
            int token = 0;
            notify_register_dispatch(kSOSCCCircleOctagonKeysChangedNotification, &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
                // Since SOS doesn't change the self peer, we can reliably just send "trusted peers changed"; it'll be mostly right
                secnotice("ckksshare", "Received a notification that the SOS Octagon peer set changed");
                [weakSelf sendTrustedPeerSetChangedUpdate];
            });
        }
    }
    return self;
}

-(CKContainer*)makeCKContainer:(NSString*)containerName usePCS:(bool)usePCS {
    CKContainer* container = [CKContainer containerWithIdentifier:containerName];
    if(!usePCS) {
        CKContainerOptions* containerOptions = [[CKContainerOptions alloc] init];
        containerOptions.bypassPCSEncryption = YES;

        // We don't have a great way to set these, so replace the entire container object
        container = [[CKContainer alloc] initWithContainerID: container.containerID options:containerOptions];
    }
    return container;
}

- (void)setupAnalytics
{
    __weak __typeof(self) weakSelf = self;

    // Tests shouldn't continue here; it leads to entitlement crashes with CloudKit if the mocks aren't enabled when this function runs
    if(SecCKKSTestsEnabled()) {
        return;
    }

    [[CKKSAnalytics logger] AddMultiSamplerForName:@"CKKS-healthSummary" withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
        __strong __typeof(self) strongSelf = weakSelf;
        if(!strongSelf) {
            return nil;
        }

        NSMutableDictionary* values = [NSMutableDictionary dictionary];
        BOOL inCircle = (strongSelf.accountTracker.currentCircleStatus == kSOSCCInCircle);
        if (inCircle) {
            [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastInCircle];
        }
        values[CKKSAnalyticsInCircle] = @(inCircle);

        BOOL validCredentials = strongSelf.accountTracker.currentCKAccountInfo.hasValidCredentials;
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

    for (NSString* viewName in [self viewList]) {
        [[CKKSAnalytics logger] AddMultiSamplerForName:[NSString stringWithFormat:@"CKKS-%@-healthSummary", viewName] withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                return nil;
            }
            BOOL inCircle = strongSelf.accountTracker && strongSelf.accountTracker.currentCircleStatus == kSOSCCInCircle;
            NSMutableDictionary* values = [NSMutableDictionary dictionary];
            CKKSKeychainView* view = [strongSelf findOrCreateView:viewName];
            NSDate* dateOfLastSyncClassA = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassA inView:view];
            NSDate* dateOfLastSyncClassC = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassC inView:view];
            NSDate* dateOfLastKSR = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastKeystateReady inView:view];

            NSInteger fuzzyDaysSinceClassASync = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastSyncClassA];
            NSInteger fuzzyDaysSinceClassCSync = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastSyncClassC];
            NSInteger fuzzyDaysSinceKSR = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastKSR];
            [values setValue:@(fuzzyDaysSinceClassASync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassASync", viewName]];
            [values setValue:@(fuzzyDaysSinceClassCSync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassCSync", viewName]];
            [values setValue:@(fuzzyDaysSinceKSR) forKey:[NSString stringWithFormat:@"%@-daysSinceLastKeystateReady", viewName]];

            BOOL hasTLKs = [view.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateReady] || [view.keyHierarchyState isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock];
            /* only synced recently if between [0...7, ie withing 7 days */
            BOOL syncedClassARecently = fuzzyDaysSinceClassASync >= 0 && fuzzyDaysSinceClassASync < 7;
            BOOL syncedClassCRecently = fuzzyDaysSinceClassCSync >= 0 && fuzzyDaysSinceClassCSync < 7;
            BOOL incomingQueueIsErrorFree = view.lastIncomingQueueOperation.error == nil;
            BOOL outgoingQueueIsErrorFree = view.lastOutgoingQueueOperation.error == nil;

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

-(void)dealloc {
    [self clearAllViews];
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
        CKKSZoneStateEntry* allEntry = [CKKSZoneStateEntry tryFromDatabase: @"all" error:&error];

        if(error) {
            secerror("CKKSViewManager: couldn't load global zone state: %@", error);
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

// Mostly exists to be mocked out.
-(NSSet*)viewList {
    return CFBridgingRelease(SOSViewCopyViewSet(kViewSetCKKS));
}

- (void)setView: (CKKSKeychainView*) obj {
    CKKSKeychainView* kcv = nil;

    @synchronized(self.views) {
        kcv = self.views[obj.zoneName];
        self.views[obj.zoneName] = obj;
    }

    if(kcv) {
        [kcv cancelAllOperations];
    }
}

- (void)clearAllViews {
    NSArray<CKKSKeychainView*>* tempviews = nil;
    @synchronized(self.views) {
        tempviews = [self.views.allValues copy];
        [self.views removeAllObjects];
    }

    for(CKKSKeychainView* view in tempviews) {
        [view cancelAllOperations];
    }
}

- (void)clearView:(NSString*) viewName {
    CKKSKeychainView* kcv = nil;
    @synchronized(self.views) {
        kcv = self.views[viewName];
        self.views[viewName] = nil;
    }

    if(kcv) {
        [kcv cancelAllOperations];
    }
}

- (CKKSKeychainView*)findView:(NSString*)viewName {
    if(!viewName) {
        return nil;
    }
    @synchronized(self.views) {
        return self.views[viewName];
    }
}

- (CKKSKeychainView*)findOrCreateView:(NSString*)viewName {
    @synchronized(self.views) {
        CKKSKeychainView* kcv = self.views[viewName];
        if(kcv) {
            return kcv;
        }

        self.views[viewName] = [[CKKSKeychainView alloc] initWithContainer: self.container
                                                                  zoneName: viewName
                                                            accountTracker: self.accountTracker
                                                          lockStateTracker: self.lockStateTracker
                                                       reachabilityTracker: self.reachabilityTracker
                                                             changeFetcher:self.zoneChangeFetcher
                                                          savedTLKNotifier: self.savedTLKNotifier
                                                              peerProvider:self
                                      fetchRecordZoneChangesOperationClass: self.fetchRecordZoneChangesOperationClass
                                                fetchRecordsOperationClass: self.fetchRecordsOperationClass
                                                       queryOperationClass:self.queryOperationClass
                                         modifySubscriptionsOperationClass: self.modifySubscriptionsOperationClass
                                           modifyRecordZonesOperationClass: self.modifyRecordZonesOperationClass
                                                        apsConnectionClass: self.apsConnectionClass
                                                             notifierClass: self.notifierClass];

        if(self.initializeNewZones) {
            [self.views[viewName] initializeZone];
        }

        return self.views[viewName];
    }
}

- (NSDictionary<NSString *,NSString *> *)activeTLKs
{
    NSMutableDictionary<NSString *,NSString *> *tlks = [NSMutableDictionary new];
    @synchronized(self.views) {
        for (NSString *name in self.views) {
            CKKSKeychainView *view = self.views[name];
            NSString *tlk = view.lastActiveTLKUUID;
            if (tlk) {
                tlks[name] = tlk;
            }
        }
    }
    return tlks;
}

- (CKKSKeychainView*)restartZone:(NSString*)viewName {
    @synchronized(self.views) {
        [self.views[viewName] halt];
        self.views[viewName] = nil;
    }
    return [self findOrCreateView: viewName];
}

// Allows all views to begin initializing, and opens the floodgates so that new views will be initalized immediately
- (void)initializeZones {
    if(!SecCKKSIsEnabled()) {
        secnotice("ckks", "Not initializing CKKS view set as CKKS is disabled");
        return;
    }

    @synchronized(self.views) {
        self.initializeNewZones = true;

        NSSet* viewSet = [self viewList];
        for(NSString* s in viewSet) {
            [self findOrCreateView:s]; // initializes any newly-created views
        }
    }

    [self setupAnalytics];
}

- (NSString*)viewNameForViewHint: (NSString*) viewHint {
    // For now, choose view based on viewhints.
    if(viewHint && ![viewHint isEqual: [NSNull null]]) {
        return viewHint;
    }

    //  If there isn't a provided view hint, use the "keychain" view if we're testing. Otherwise, nil.
    if(SecCKKSTestsEnabled()) {
        return @"keychain";
    } else {
        return nil;
    }
}

- (NSString*)viewNameForItem: (SecDbItemRef) item {
    CFErrorRef cferror = NULL;
    NSString* viewHint = (__bridge NSString*) SecDbItemGetValue(item, &v7vwht, &cferror);

    if(cferror) {
        secerror("ckks: Couldn't fetch the viewhint for some reason: %@", cferror);
        CFReleaseNull(cferror);
        viewHint = nil;
    }

    return [self viewNameForViewHint: viewHint];
}

- (NSString*)viewNameForAttributes: (NSDictionary*) item {
    return [self viewNameForViewHint: item[(id)kSecAttrSyncViewHint]];
}

- (void)registerSyncStatusCallback: (NSString*) uuid callback: (SecBoolNSErrorCallback) callback {
    // Someone is requesting future notification of this item.
    @synchronized(self.pendingSyncCallbacks) {
        self.pendingSyncCallbacks[uuid] = callback;
    }
}

- (void) handleKeychainEventDbConnection: (SecDbConnectionRef) dbconn source:(SecDbTransactionSource)txionSource added: (SecDbItemRef) added deleted: (SecDbItemRef) deleted {

    SecDbItemRef modified = added ? added : deleted;

    NSString* viewName = [self viewNameForItem: modified];
    NSString* keyViewName = [CKKSKey isItemKeyForKeychainView: modified];

    if(keyViewName) {
        // This might be some key material for this view! Poke it.
        CKKSKeychainView* view = [self findView: keyViewName];

        if(!SecCKKSTestDisableKeyNotifications()) {
            ckksnotice("ckks", view, "Potential new key material from %@ (source %lu)",
                       keyViewName, (unsigned long)txionSource);
            [view keyStateMachineRequestProcess];
        } else {
            ckksnotice("ckks", view, "Ignoring potential new key material from %@ (source %lu)",
                       keyViewName, (unsigned long)txionSource);
        }
        return;
    }

    // When SOS is in charge of a view, CKKS is not.
    // Since this isn't a CKKS key item, we don't care about it.
    if(txionSource == kSecDbSOSTransaction) {
        secinfo("ckks", "Ignoring new non-CKKS item in kSecDbSOSTransaction notification");
    }

    // Looks like a normal item. Proceed!
    CKKSKeychainView* view = [self findView:viewName];

    NSString* uuid = (__bridge NSString*) SecDbItemGetValue(modified, &v10itemuuid, NULL);
    SecBoolNSErrorCallback syncCallback = nil;
    if(uuid) {
        @synchronized(self.pendingSyncCallbacks) {
            syncCallback = self.pendingSyncCallbacks[uuid];
            self.pendingSyncCallbacks[uuid] = nil;

            if(syncCallback) {
                secinfo("ckks", "Have a pending callback for %@; passing along", uuid);
            }
        }
    }

    if(!view) {
        secinfo("ckks", "No CKKS view for %@, skipping: %@", viewName, modified);
        if(syncCallback) {
            syncCallback(false, [NSError errorWithDomain:@"securityd"
                                                    code:kSOSCCNoSuchView
                                                userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for '%@'", viewName]}]);
        }
        return;
    }

    ckksnotice("ckks", view, "Routing item to zone %@: %@", viewName, modified);
    [view handleKeychainEventDbConnection: dbconn added:added deleted:deleted rateLimiter:self.globalRateLimiter syncCallback: syncCallback];
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
    CKKSKeychainView* view = [self findView:viewHint];

    if(!view) {
        secnotice("ckks", "No CKKS view for %@, skipping current request", viewHint);
        complete([NSError errorWithDomain:CKKSErrorDomain
                                     code:CKKSNoSuchView
                              description:[NSString stringWithFormat: @"No syncing view for view hint '%@'", viewHint]]);
        return;
    }

    [view setCurrentItemForAccessGroup:newItemPersistentRef
                                  hash:newItemSHA1
                           accessGroup:accessGroup
                            identifier:identifier
                             replacing:oldCurrentItemPersistentRef
                                  hash:oldItemSHA1
                              complete:complete];
}

-(void)getCurrentItemForAccessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                           viewHint:(NSString*)viewHint
                    fetchCloudValue:(bool)fetchCloudValue
                           complete:(void (^) (NSString* uuid, NSError* operror)) complete
{
    CKKSKeychainView* view = [self findView:viewHint];
    if(!view) {
        secnotice("ckks", "No CKKS view for %@, skipping current fetch request", viewHint);
        complete(NULL, [NSError errorWithDomain:CKKSErrorDomain
                                           code:CKKSNoSuchView
                                    description:[NSString stringWithFormat: @"No view for '%@'", viewHint]]);
        return;
    }

    [view getCurrentItemForAccessGroup:accessGroup
                            identifier:identifier
                       fetchCloudValue:fetchCloudValue
                              complete:complete];
}


+ (instancetype) manager {
    return [self resetManager: false setTo: nil];
}

+ (instancetype) resetManager: (bool) reset setTo: (CKKSViewManager*) obj {
    static CKKSViewManager* manager = nil;

    if([CKDatabase class] == nil) {
        secerror("CKKS: CloudKit.framework appears to not be linked. Can't create CKKS objects.");
        return nil;
    }

    if(!manager || reset || obj) {
        @synchronized([self class]) {
            if(obj != nil) {
                [manager clearAllViews];
                manager = obj;
            } else {
                if(reset) {
                    [manager clearAllViews];
                    manager = nil;
                } else if (manager == nil && SecCKKSIsEnabled()) {
                    manager = [[CKKSViewManager alloc] initCloudKitWithContainerName:SecCKKSContainerName usePCS:SecCKKSContainerUsePCS];
                }
            }
        }
    }

    return manager;
}

- (void)cancelPendingOperations {
    [self.savedTLKNotifier cancel];
}

-(void)notifyNewTLKsInKeychain {
    // Why two functions here? Limitation of OCMock, unfortunately: can't stub and expect the same method
    secnotice("ckksbackup", "New TLKs have arrived");
    [self syncBackupAndNotifyAboutSync];
}

- (void)syncBackupAndNotifyAboutSync {
    SOSAccount* account = (__bridge SOSAccount*)SOSKeychainAccountGetSharedAccount();
    
    if(!account) {
        secnotice("ckks", "Failed to get account object");
        return;
    }

    [account performTransaction:^(SOSAccountTransaction * _Nonnull txn) {
        CFErrorRef error = NULL;
        NSSet* ignore = CFBridgingRelease(SOSAccountCopyBackupPeersAndForceSync(txn, &error));
        (void)ignore;

        if(error) {
            secerror("ckksbackup: Couldn't process sync with backup peers: %@", error);
        } else {
            secnotice("ckksbackup", "telling CloudServices about TLK arrival");
            notify_post(kSecItemBackupNotification);
        };
    }];
}

#pragma mark - RPCs to manage and report state

- (void)performanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *counter))reply {
    reply(@{});
}

- (NSArray<CKKSKeychainView*>*)views:(NSString*)viewName operation:(NSString*)opName error:(NSError**)error
{
    NSArray* actualViews = nil;

    // Ensure we've actually set up, but don't wait too long. Clients get impatient.
    if([self.completedSecCKKSInitialize wait:5*NSEC_PER_SEC]) {
        secerror("ckks: Haven't yet initialized zones; expect failure fetching views");
    }

    @synchronized(self.views) {
        if(viewName) {
            CKKSKeychainView* view = self.views[viewName];
            secnotice("ckks", "Received a %@ request for zone %@ (%@)", opName, viewName, view);

            if(!view) {
                if(error) {
                    *error = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSNoSuchView
                                             userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No view for '%@'", viewName]}];
                }
                return nil;
            }

            actualViews = @[view];
        } else {
            actualViews = [self.views.allValues copy];
            secnotice("ckks", "Received a %@ request for all zones: %@", opName, actualViews);
        }
    }
    actualViews = [actualViews sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"zoneName" ascending:YES]]];
    return actualViews;
}

- (void)rpcResetLocal:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    NSError* localError = nil;
    NSArray* actualViews = [self views:viewName operation:@"local reset" error:&localError];
    if(localError) {
        secerror("ckks: Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"local-reset-zones-waiter" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull strongOp) {
        if(!strongOp.error) {
            secnotice("ckksreset", "Completed rpcResetLocal");
        } else {
            secnotice("ckks", "Completed rpcResetLocal with error: %@", strongOp.error);
        }
        reply(CKXPCSuitableError(strongOp.error));
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksreset", view, "Beginning local reset for %@", view);
        [op addSuccessDependency:[view resetLocalData]];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation: op];
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-implementations"
- (void)rpcResetCloudKit:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    [self rpcResetCloudKit:viewName reason:@"unknown" reply:reply];
}
#pragma clang diagnostic pop

- (void)rpcResetCloudKit:(NSString*)viewName reason:(NSString *)reason reply:(void(^)(NSError* result)) reply {
    NSError* localError = nil;
    NSArray* actualViews = [self views:viewName operation:@"CloudKit reset" error:&localError];
    if(localError) {
        secerror("ckks: Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-reset-zones-waiter" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull strongOp) {
        if(!strongOp.error) {
            secnotice("ckksreset", "Completed rpcResetCloudKit");
        } else {
            secnotice("ckksreset", "Completed rpcResetCloudKit with error: %@", strongOp.error);
        }
        reply(CKXPCSuitableError(strongOp.error));
    }];

    for(CKKSKeychainView* view in actualViews) {
        NSString *operationGroupName = [NSString stringWithFormat:@"api-reset-%@", reason];
        ckksnotice("ckksreset", view, "Beginning CloudKit reset for %@: %@", view, reason);
        [op addSuccessDependency:[view resetCloudKitZone:[CKOperationGroup CKKSGroupWithName:operationGroupName]]];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation: op];
}

- (void)rpcResync:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    NSError* localError = nil;
    NSArray* actualViews = [self views:viewName operation:@"CloudKit resync" error:&localError];
    if(localError) {
        secerror("ckks: Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    op.name = @"rpc-resync-cloudkit";
    __weak __typeof(op) weakOp = op;
    [op addExecutionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        secnotice("ckks", "Ending rsync-CloudKit rpc with %@", strongOp.error);
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksresync", view, "Beginning resync (CloudKit) for %@", view);

        CKKSSynchronizeOperation* resyncOp = [view resyncWithCloud];
        [op addSuccessDependency:resyncOp];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation:op];
    [op waitUntilFinished];
    reply(CKXPCSuitableError(op.error));
}

- (void)rpcResyncLocal:(NSString*)viewName reply:(void(^)(NSError* result))reply {
    NSError* localError = nil;
    NSArray* actualViews = [self views:viewName operation:@"local resync" error:&localError];
    if(localError) {
        secerror("ckks: Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    op.name = @"rpc-resync-local";
    __weak __typeof(op) weakOp = op;
    [op addExecutionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        secnotice("ckks", "Ending rsync-local rpc with %@", strongOp.error);
        reply(CKXPCSuitableError(strongOp.error));
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksresync", view, "Beginning resync (local) for %@", view);

        CKKSLocalSynchronizeOperation* resyncOp = [view resyncLocal];
        [op addSuccessDependency:resyncOp];
    }

    [op timeout:120*NSEC_PER_SEC];
}

- (void)rpcStatus: (NSString*)viewName
           global:(bool)reportGlobal
            reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error)) reply
        viewBlock:(NSDictionary * (^)(CKKSKeychainView* view))viewBlock
{
    NSMutableArray* a = [[NSMutableArray alloc] init];

    // Now, query the views about their status
    NSError* error = nil;
    NSArray* actualViews = [self views:viewName operation:@"status" error:&error];
    if(!actualViews || error) {
        reply(nil, error);
        return;
    }

    __weak __typeof(self) weakSelf = self;
    CKKSResultOperation* statusOp = [CKKSResultOperation named:@"status-rpc" withBlock:^{
        __strong __typeof(self) strongSelf = weakSelf;

        if (reportGlobal) {
            // The first element is always the current global state (non-view-specific)
            NSError* selfPeersError = nil;
            CKKSSelves* selves = [strongSelf fetchSelfPeers:&selfPeersError];
            NSError* trustedPeersError = nil;
            NSSet<id<CKKSPeer>>* peers = [strongSelf fetchTrustedPeers:&trustedPeersError];

            // Get account state, even wait for it a little
            [self.accountTracker.ckdeviceIDInitialized wait:1*NSEC_PER_SEC];
            NSString *deviceID = self.accountTracker.ckdeviceID;
            NSError *deviceIDError = self.accountTracker.ckdeviceIDError;

            NSMutableArray<NSString*>* mutTrustedPeers = [[NSMutableArray alloc] init];
            [peers enumerateObjectsUsingBlock:^(id<CKKSPeer>  _Nonnull obj, BOOL * _Nonnull stop) {
                [mutTrustedPeers addObject: [obj description]];
            }];

#define stringify(obj) CKKSNilToNSNull([obj description])
            NSDictionary* global = @{
                                     @"view":                @"global",
                                     @"selfPeers":           stringify(selves),
                                     @"selfPeersError":      CKKSNilToNSNull(selfPeersError),
                                     @"trustedPeers":        CKKSNilToNSNull(mutTrustedPeers),
                                     @"trustedPeersError":   CKKSNilToNSNull(trustedPeersError),
                                     @"reachability":        strongSelf.reachabilityTracker.currentReachability ? @"network" : @"no-network",
                                     @"ckdeviceID":          CKKSNilToNSNull(deviceID),
                                     @"ckdeviceIDError":     CKKSNilToNSNull(deviceIDError),
                                     };
            [a addObject: global];
        }

        for(CKKSKeychainView* view in actualViews) {
            ckksnotice("ckks", view, "Fetching status for %@", view.zoneName);
            NSDictionary* status = viewBlock(view);
            ckksinfo("ckks", view, "Status is %@", status);
            if(status) {
                [a addObject: status];
            }
        }
        reply(a, nil);
    }];

    // If we're signed in, give the views a few seconds to enter what they consider to be a non-transient state (in case this daemon just launched)
    if([self.accountTracker.currentComputedAccountStatusValid wait:5*NSEC_PER_SEC]) {
        secerror("ckks status: Haven't yet figured out login state");
    }

    if(self.accountTracker.currentComputedAccountStatus == CKKSAccountStatusAvailable) {
        CKKSResultOperation* blockOp = [CKKSResultOperation named:@"wait-for-status" withBlock:^{}];
        [blockOp timeout:8*NSEC_PER_SEC];
        for(CKKSKeychainView* view in actualViews) {
            [blockOp addNullableDependency:view.keyStateNonTransientDependency];
            [statusOp addDependency:blockOp];
        }
        [self.operationQueue addOperation:blockOp];
    }
    [self.operationQueue addOperation:statusOp];

    return;
}

- (void)rpcStatus:(NSString*)viewName reply:(void (^)(NSArray<NSDictionary*>* result, NSError* error))reply
{
    [self rpcStatus:viewName global:true reply:reply viewBlock:^NSDictionary *(CKKSKeychainView *view) {
        return [view status];
    }];
}

- (void)rpcFastStatus:(NSString*)viewName reply:(void (^)(NSArray<NSDictionary*>* result, NSError* error))reply
{
    [self rpcStatus:viewName global:false reply:reply viewBlock:^NSDictionary *(CKKSKeychainView *view) {
        return [view fastStatus];
    }];
}

- (void)rpcFetchAndProcessChanges:(NSString*)viewName reply: (void(^)(NSError* result))reply {
    [self rpcFetchAndProcessChanges:viewName classA:false reply: (void(^)(NSError* result))reply];
}

- (void)rpcFetchAndProcessClassAChanges:(NSString*)viewName reply: (void(^)(NSError* result))reply {
    [self rpcFetchAndProcessChanges:viewName classA:true reply:(void(^)(NSError* result))reply];
}

- (void)rpcFetchAndProcessChanges:(NSString*)viewName classA:(bool)classAError reply: (void(^)(NSError* result)) reply {
    NSError* error = nil;
    NSArray* actualViews = [self views:viewName operation:@"fetch" error:&error];
    if(!actualViews || error) {
        reply(error);
        return;
    }

    CKKSResultOperation* blockOp = [[CKKSResultOperation alloc] init];
    blockOp.name = @"rpc-fetch-and-process-result";
    __weak __typeof(blockOp) weakBlockOp = blockOp;
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [blockOp setCompletionBlock:^{
        __strong __typeof(blockOp) strongBlockOp = weakBlockOp;
        [strongBlockOp allDependentsSuccessful];
        reply(CKXPCSuitableError(strongBlockOp.error));
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks", view, "Beginning fetch for %@", view);

        CKKSResultOperation* op = [view processIncomingQueue:classAError after:[view.zoneChangeFetcher requestSuccessfulFetch: CKKSFetchBecauseAPIFetchRequest]];
        [blockOp addDependency:op];
    }

    [self.operationQueue addOperation: [blockOp timeout:(SecCKKSTestsEnabled() ? NSEC_PER_SEC * 5 : NSEC_PER_SEC * 120)]];
}

- (void)rpcPushOutgoingChanges:(NSString*)viewName reply: (void(^)(NSError* result))reply {
    NSError* error = nil;
    NSArray* actualViews = [self views:viewName operation:@"push" error:&error];
    if(!actualViews || error) {
        reply(error);
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

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks-rpc", view, "Beginning push for %@", view);

        CKKSResultOperation* op = [view processOutgoingQueue: [CKOperationGroup CKKSGroupWithName:@"rpc-push"]];
        [blockOp addDependency:op];
    }

    [self.operationQueue addOperation: [blockOp timeout:(SecCKKSTestsEnabled() ? NSEC_PER_SEC * 2 : NSEC_PER_SEC * 120)]];
}

- (void)rpcGetCKDeviceIDWithReply:(void (^)(NSString *))reply {
    reply(self.accountTracker.ckdeviceID);
}

-(void)xpc24HrNotification {
    // XPC has poked us and said we should do some cleanup!

    // For now, poke the views and tell them to update their device states if they'd like
    NSArray* actualViews = nil;
    @synchronized(self.views) {
        // Can't safely iterate a mutable collection, so copy it.
        actualViews = self.views.allValues;
    }

    secnotice("ckks", "Received a 24hr notification from XPC");
    CKOperationGroup* group = [CKOperationGroup CKKSGroupWithName:@"periodic-device-state-update"];
    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks", view, "Starting device state XPC update");
        // Let the update know it should rate-limit itself
        [view updateDeviceState:true waitForKeyHierarchyInitialization:30*NSEC_PER_SEC ckoperationGroup:group];
    }
}

- (NSArray<NSDictionary *> * _Nullable)loadRestoredBottledKeysOfType:(OctagonKeyType)keyType error:(NSError**)error
{
    CFTypeRef result = NULL;
    NSMutableArray* bottledPeerKeychainItems = nil;

    NSDictionary* query = @{
                            (id)kSecClass : (id)kSecClassInternetPassword,
                            (id)kSecAttrAccessible: (id)kSecAttrAccessibleWhenUnlocked,
                            (id)kSecAttrNoLegacy : @YES,
                            (id)kSecAttrType : [[NSNumber alloc]initWithInt: keyType],
                            (id)kSecAttrServer : (keyType == 1) ? @"Octagon Signing Key" : @"Octagon Encryption Key",
                            (id)kSecAttrAccessGroup: @"com.apple.security.ckks",
                            (id)kSecMatchLimit : (id)kSecMatchLimitAll,
                            (id)kSecReturnAttributes: @YES,
                            (id)kSecReturnData: @YES,
                            };
    OSStatus status = SecItemCopyMatching((__bridge CFDictionaryRef)query, &result);

    if(status == errSecSuccess && result && isArray(result)) {
        bottledPeerKeychainItems = CFBridgingRelease(result);
        result = NULL;
    } else {
        if(error) {
            *error = [NSError errorWithDomain:NSOSStatusErrorDomain
                                         code:status
                                  description:@"could not load bottled peer keys"];
        }
        CFReleaseNull(result);
    }

    return bottledPeerKeychainItems;
}

-(NSDictionary *) keychainItemForPeerID:(NSString*)neededPeerID
                          keychainItems:(NSArray<NSDictionary*> *)keychainItems
                escrowSigningPubKeyHash:(NSString *)hashWeNeedToMatch
{
    NSDictionary* peerItem = nil;

    for(NSDictionary* item in keychainItems){
        if(item && [item count] > 0){
            NSString* peerIDFromItem = [item objectForKey:(id)kSecAttrAccount];
            NSString* hashToConsider = [item objectForKey:(id)kSecAttrLabel];
            if([peerIDFromItem isEqualToString:neededPeerID] &&
               [hashWeNeedToMatch isEqualToString:hashToConsider])
            {
                peerItem = [item copy];
                break;
            }
        }
    }

    return peerItem;
}

- (NSSet<id<CKKSSelfPeer>>*)pastSelves:(NSError**)error
{
    NSError* localError = nil;

    // get bottled peer identities from the keychain
    NSMutableSet<id<CKKSSelfPeer>>* allSelves = [NSMutableSet set];
    NSArray<NSDictionary*>* signingKeys = [self loadRestoredBottledKeysOfType:OctagonSigningKey error:&localError];
    if(!signingKeys) {
        // Item not found isn't actually an error here
        if(error && !(localError && [localError.domain isEqualToString: NSOSStatusErrorDomain] && localError.code == errSecItemNotFound)) {
            *error = localError;
        }

        return allSelves;
    }

    NSArray<NSDictionary*>* encryptionKeys = [self loadRestoredBottledKeysOfType:OctagonEncryptionKey error:&localError];
    if(!encryptionKeys) {
        if(error && !(localError && [localError.domain isEqualToString: NSOSStatusErrorDomain] && localError.code == errSecItemNotFound)) {
            *error = localError;
        }
        return allSelves;
    }

    for(NSDictionary* signingKey in signingKeys) {
        NSError* peerError = nil;
        NSString* peerid = signingKey[(id)kSecAttrAccount];
        NSString* hash = signingKey[(id)kSecAttrLabel]; // escrow signing pub key hash

        //use peer id AND escrow signing public key hash to look up the matching item in encryptionKeys list
        NSDictionary* encryptionKeyItem = [self keychainItemForPeerID:peerid keychainItems:encryptionKeys escrowSigningPubKeyHash:hash];
        if(!encryptionKeyItem) {
            secerror("octagon: no encryption key available to pair with signing key %@,%@", peerid, hash);
            continue;
        }

        NSData* signingKeyData = signingKey[(id)kSecValueData];
        if(!signingKeyData) {
            secerror("octagon: no signing key data for %@,%@", peerid,hash);
            continue;
        }

        SFECKeyPair* restoredSigningKey = [[SFECKeyPair alloc] initWithData:signingKeyData
                                                                  specifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]
                                                                      error:&peerError];
        if(!restoredSigningKey) {
            secerror("octagon: couldn't make signing key for %@,%@: %@", peerid, hash, peerError);
            continue;
        }

        NSData* encryptionKeyData = [encryptionKeyItem objectForKey:(id)kSecValueData];
        if(!encryptionKeyData) {
            secerror("octagon: no encryption key data for %@,%@", peerid,hash);
            continue;
        }

        SFECKeyPair* restoredEncryptionKey = [[SFECKeyPair alloc] initWithData:encryptionKeyData
                                                                     specifier:[[SFECKeySpecifier alloc] initWithCurve:SFEllipticCurveNistp384]
                                                                         error:&peerError];
        if(!restoredEncryptionKey) {
            secerror("octagon: couldn't make encryption key for %@,%@: %@", peerid,hash, peerError);
            continue;
        }

        //create the SOS self peer
        CKKSSOSSelfPeer* restoredIdentity = [[CKKSSOSSelfPeer alloc]initWithSOSPeerID:peerid encryptionKey:restoredEncryptionKey signingKey:restoredSigningKey];

        if(restoredIdentity){
            secnotice("octagon","adding bottled peer identity: %@", restoredIdentity);
            [allSelves addObject:restoredIdentity];
        } else {
            secerror("octagon: could not create restored identity from: %@: %@", peerid, peerError);
        }
    }
    return allSelves;
}

- (id<CKKSSelfPeer> _Nullable)currentSOSSelf:(NSError**)error
{
    __block SFECKeyPair* signingPrivateKey = nil;
    __block SFECKeyPair* encryptionPrivateKey = nil;

    __block NSError* localerror = nil;

    // Wait for this to initialize, but don't worry if it isn't.
    [self.accountTracker.accountCirclePeerIDInitialized wait:500*NSEC_PER_MSEC];
    NSString* peerID = self.accountTracker.accountCirclePeerID;
    if(!peerID || self.accountTracker.accountCirclePeerIDError) {
        secerror("ckkspeer: Error fetching self peer : %@", self.accountTracker.accountCirclePeerIDError);
        if(error) {
            *error = self.accountTracker.accountCirclePeerIDError;
        }
        return nil;
    }

    SOSCCPerformWithAllOctagonKeys(^(SecKeyRef octagonEncryptionKey, SecKeyRef octagonSigningKey, CFErrorRef cferror) {
        if(cferror) {
            localerror = (__bridge NSError*)cferror;
            return;
        }
        if (!cferror && octagonEncryptionKey && octagonSigningKey) {
            signingPrivateKey = [[SFECKeyPair alloc] initWithSecKey:octagonSigningKey];
            encryptionPrivateKey = [[SFECKeyPair alloc] initWithSecKey:octagonEncryptionKey];
        } else {
            localerror = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNoPeersAvailable
                                      description:@"Not all SOS peer keys available, but no error returned"];
        }
    });

    if(localerror) {
        if(![self.lockStateTracker isLockedError:localerror]) {
            secerror("ckkspeer: Error fetching self encryption keys: %@", localerror);
        }
        if(error) {
            *error = localerror;
        }
        return nil;
    }

    CKKSSOSSelfPeer* selfPeer = [[CKKSSOSSelfPeer alloc] initWithSOSPeerID:peerID
                                                             encryptionKey:encryptionPrivateKey
                                                                signingKey:signingPrivateKey];
    return selfPeer;
}

#pragma mark - CKKSPeerProvider implementation

- (CKKSSelves*)fetchSelfPeers:(NSError* __autoreleasing *)error {
    NSError* localError = nil;

    id<CKKSSelfPeer> selfPeer = [self currentSOSSelf:&localError];
    if(!selfPeer || localError) {
        if(![self.lockStateTracker isLockedError:localError]) {
            secerror("ckks: Error fetching current SOS self: %@", localError);
        }
        if(error) {
            *error = localError;
        }
        return nil;
    }

    NSSet<id<CKKSSelfPeer>>* allSelves = [self pastSelves:&localError];
    if(!allSelves || localError) {
        secerror("ckks: Error fetching past selves: %@", localError);
        if(error) {
            *error = localError;
        }
        return nil;
    }

    CKKSSelves* selves = [[CKKSSelves alloc] initWithCurrent:selfPeer allSelves:allSelves];
    return selves;
}

- (NSSet<id<CKKSPeer>>*)fetchTrustedPeers:(NSError* __autoreleasing *)error {
    __block NSMutableSet<id<CKKSPeer>>* peerSet = [NSMutableSet set];

    SOSCCPerformWithTrustedPeers(^(CFSetRef sosPeerInfoRefs, CFErrorRef cfTrustedPeersError) {
        if(cfTrustedPeersError) {
            secerror("ckks: Error fetching trusted peers: %@", cfTrustedPeersError);
            if(error) {
                *error = (__bridge NSError*)cfTrustedPeersError;
            }
        }

        CFSetForEach(sosPeerInfoRefs, ^(const void* voidPeer) {
            CFErrorRef cfPeerError = NULL;
            SOSPeerInfoRef sosPeerInfoRef = (SOSPeerInfoRef)voidPeer;

            if(!sosPeerInfoRef) {
                return;
            }

            CFStringRef cfpeerID = SOSPeerInfoGetPeerID(sosPeerInfoRef);
            SecKeyRef cfOctagonSigningKey = NULL, cfOctagonEncryptionKey = NULL;

            cfOctagonSigningKey = SOSPeerInfoCopyOctagonSigningPublicKey(sosPeerInfoRef, &cfPeerError);
            if (cfOctagonSigningKey) {
                cfOctagonEncryptionKey = SOSPeerInfoCopyOctagonEncryptionPublicKey(sosPeerInfoRef, &cfPeerError);
            }

            if(cfOctagonSigningKey == NULL || cfOctagonEncryptionKey == NULL) {
                // Don't log non-debug for -50; it almost always just means this peer didn't have octagon keys
                if(cfPeerError == NULL
                   || !(CFEqualSafe(CFErrorGetDomain(cfPeerError), kCFErrorDomainOSStatus) && (CFErrorGetCode(cfPeerError) == errSecParam)))
                {
                    secerror("ckkspeer: error fetching octagon keys for peer: %@ %@", sosPeerInfoRef, cfPeerError);
                } else {
                    secinfo("ckkspeer", "Peer(%@) doesn't have Octagon keys, but this is expected: %@", cfpeerID, cfPeerError);
                }
            }

            // Add all peers to the trust set: old-style SOS peers will just have null keys
            SFECPublicKey* signingPublicKey = cfOctagonSigningKey ? [[SFECPublicKey alloc] initWithSecKey:cfOctagonSigningKey] : nil;
            SFECPublicKey* encryptionPublicKey = cfOctagonEncryptionKey ? [[SFECPublicKey alloc] initWithSecKey:cfOctagonEncryptionKey] : nil;

            CKKSSOSPeer* peer = [[CKKSSOSPeer alloc] initWithSOSPeerID:(__bridge NSString*)cfpeerID
                                                   encryptionPublicKey:encryptionPublicKey
                                                      signingPublicKey:signingPublicKey];
            [peerSet addObject:peer];

            CFReleaseNull(cfOctagonSigningKey);
            CFReleaseNull(cfOctagonEncryptionKey);
            CFReleaseNull(cfPeerError);
        });
    });

    return peerSet;
}

- (void)registerForPeerChangeUpdates:(id<CKKSPeerUpdateListener>)listener {
    @synchronized(self.peerChangeListeners) {
        bool alreadyRegisteredListener = false;
        NSEnumerator *enumerator = [self.peerChangeListeners objectEnumerator];
        id<CKKSPeerUpdateListener> value;

        while ((value = [enumerator nextObject])) {
            // do pointer comparison
            alreadyRegisteredListener |= (value == listener);
        }

        if(listener && !alreadyRegisteredListener) {
            NSString* queueName = [NSString stringWithFormat: @"ck-peer-change-%@", listener];

            dispatch_queue_t objQueue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
            [self.peerChangeListeners setObject: listener forKey: objQueue];
        }
    }
}

- (void)iteratePeerListenersOnTheirQueue:(void (^)(id<CKKSPeerUpdateListener>))block {
    @synchronized(self.peerChangeListeners) {
        NSEnumerator *enumerator = [self.peerChangeListeners keyEnumerator];
        dispatch_queue_t dq;

        // Queue up the changes for each listener.
        while ((dq = [enumerator nextObject])) {
            id<CKKSPeerUpdateListener> listener = [self.peerChangeListeners objectForKey: dq];
            __weak id<CKKSPeerUpdateListener> weakListener = listener;

            if(listener) {
                dispatch_async(dq, ^{
                    __strong id<CKKSPeerUpdateListener> strongListener = weakListener;
                    block(strongListener);
                });
            }
        }
    }
}

- (void)sendSelfPeerChangedUpdate {
    [self.completedSecCKKSInitialize wait:5*NSEC_PER_SEC]; // Wait for bringup, but don't worry if this times out

    [self iteratePeerListenersOnTheirQueue: ^(id<CKKSPeerUpdateListener> listener) {
        [listener selfPeerChanged];
    }];
}

- (void)sendTrustedPeerSetChangedUpdate {
    [self.completedSecCKKSInitialize wait:5*NSEC_PER_SEC]; // Wait for bringup, but don't worry if this times out

    [self iteratePeerListenersOnTheirQueue: ^(id<CKKSPeerUpdateListener> listener) {
        [listener trustedPeerSetChanged];
    }];
}
#endif // OCTAGON
@end
