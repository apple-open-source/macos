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

#import "keychain/ckks/CKKSViewManager.h"
#import "keychain/ckks/CKKSKeychainView.h"
#import "keychain/ckks/CKKSSynchronizeOperation.h"
#import "keychain/ckks/CKKSKey.h"
#import "keychain/ckks/CKKSZoneStateEntry.h"
#import "keychain/ckks/CKKSNearFutureScheduler.h"
#import "keychain/ckks/CKKSNotifier.h"
#import "keychain/ckks/CKKSCondition.h"
#import "keychain/ckks/CKKSListenerCollection.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/analytics/SecEventMetric.h"
#import "keychain/analytics/SecMetrics.h"

#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/ObjCImprovements.h"

#import "TPPolicy.h"

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

@interface CKKSViewManager () <NSXPCListenerDelegate>
#if OCTAGON
@property NSXPCListener *listener;

// Once you set these, all CKKSKeychainViews created will use them
@property CKKSCloudKitClassDependencies* cloudKitClassDependencies;

@property NSMutableDictionary<NSString*, SecBoolNSErrorCallback>* pendingSyncCallbacks;
@property CKKSNearFutureScheduler* savedTLKNotifier;;
@property NSOperationQueue* operationQueue;

@property CKKSListenerCollection<id<CKKSPeerUpdateListener>>* peerChangeListenerCollection;

@property (nonatomic) BOOL overrideCKKSViewsFromPolicy;
@property (nonatomic) BOOL valueCKKSViewsFromPolicy;

#endif
@end

#if OCTAGON
@interface CKKSViewManager (lockstateTracker) <CKKSLockStateNotification>
@end
#endif

@implementation CKKSViewManager
#if OCTAGON

NSSet<NSString*>* _viewList;

- (instancetype)initWithContainerName: (NSString*) containerName
                               usePCS: (bool)usePCS
                           sosAdapter:(id<OTSOSAdapter> _Nullable)sosAdapter
            cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if(self = [super init]) {
        _cloudKitClassDependencies = cloudKitClassDependencies;
        _sosPeerAdapter = sosAdapter;
        _viewList = nil;

        _container = [self makeCKContainer: containerName usePCS:usePCS];
        _accountTracker = [[CKKSAccountStateTracker alloc] init:self.container nsnotificationCenterClass:cloudKitClassDependencies.nsnotificationCenterClass];
        _lockStateTracker = [[CKKSLockStateTracker alloc] init];
        [_lockStateTracker addLockStateObserver:self];
        _reachabilityTracker = [[CKKSReachabilityTracker alloc] init];

        _zoneChangeFetcher = [[CKKSZoneChangeFetcher alloc] initWithContainer:_container
                                                                   fetchClass:cloudKitClassDependencies.fetchRecordZoneChangesOperationClass
                                                          reachabilityTracker:_reachabilityTracker];

        _zoneModifier = [[CKKSZoneModifier alloc] initWithContainer:_container
                                                reachabilityTracker:_reachabilityTracker
                                               cloudkitDependencies:cloudKitClassDependencies];

        _operationQueue = [[NSOperationQueue alloc] init];

        _peerChangeListenerCollection = [[CKKSListenerCollection alloc] initWithName:@"sos-peer-set"];

        _views = [[NSMutableDictionary alloc] init];
        _pendingSyncCallbacks = [[NSMutableDictionary alloc] init];

        _completedSecCKKSInitialize = [[CKKSCondition alloc] init];

        WEAKIFY(self);
        _savedTLKNotifier = [[CKKSNearFutureScheduler alloc] initWithName:@"newtlks"
                                                                    delay:5*NSEC_PER_SEC
                                                         keepProcessAlive:true
                                                dependencyDescriptionCode:CKKSResultDescriptionNone
                                                                    block:^{
            STRONGIFY(self);
            [self notifyNewTLKsInKeychain];
        }];

        _listener = [NSXPCListener anonymousListener];
        _listener.delegate = self;
        [_listener resume];
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

- (BOOL)waitForTrustReady {
    static dispatch_once_t onceToken;
    __block BOOL success = YES;
    dispatch_once(&onceToken, ^{
        OTManager* manager = [OTManager manager];
        if (![manager waitForReady:OTCKContainerName context:OTDefaultContext wait:3*NSEC_PER_SEC]) {
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
            secerror("CKKSViewManager: couldn't fetch sos status for SF report: %@", sosCircleError);
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

    for (NSString* viewName in [self viewList]) {
        [[CKKSAnalytics logger] AddMultiSamplerForName:[NSString stringWithFormat:@"CKKS-%@-healthSummary", viewName] withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
            STRONGIFY(self);
            if(!self) {
                return nil;
            }

            NSError* sosCircleError = nil;
            SOSCCStatus sosStatus = [self.sosPeerAdapter circleStatus:&sosCircleError];
            if(sosCircleError) {
                secerror("CKKSViewManager: couldn't fetch sos status for SF report: %@", sosCircleError);
            }
            BOOL inCircle = (sosStatus == kSOSCCInCircle);
            NSMutableDictionary* values = [NSMutableDictionary dictionary];
            CKKSKeychainView* view = [self findOrCreateView:viewName];
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

- (NSSet<NSString*>*)defaultViewList {
    NSSet<NSString*>* fullList = [OTSOSActualAdapter sosCKKSViewList];

    // Not a great hack: if this platform is an aTV or a HomePod, then filter its list of views
    bool filter = false;

#if TARGET_OS_TV
    filter = true;
#elif TARGET_OS_WATCH
    filter = false;
#elif TARGET_OS_IOS
    filter = !OctagonPlatformSupportsSOS();
#elif TARGET_OS_OSX
    filter = false;
#endif

    if(filter) {
        // For now, use a hardcoded allow list for TV/HomePod
        NSMutableSet<NSString*>* allowList = [NSMutableSet setWithArray:@[@"Home", @"LimitedPeersAllowed"]];
        [allowList intersectSet:fullList];
        return allowList;
    }

    return fullList;
}

-(NSSet<NSString*>*)viewList {
    if (_viewList) {
        return _viewList;
    } else {
        return [self defaultViewList];
    }
}

- (void)setViewList:(NSSet<NSString*>* _Nullable)newViewList {
    _viewList = newViewList;
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
                                                              zoneModifier:self.zoneModifier
                                                          savedTLKNotifier: self.savedTLKNotifier
                                                 cloudKitClassDependencies:self.cloudKitClassDependencies];
        return self.views[viewName];
    }
}

- (NSSet<CKKSKeychainView*>*)currentViews
{
    @synchronized (self.views) {
        NSMutableSet<CKKSKeychainView*>* viewObjects = [NSMutableSet set];
        for(NSString* viewName in self.views) {
            [viewObjects addObject:self.views[viewName]];
        }
        return viewObjects;
    }
}

- (void)createViews
{
    // In the future, the CKKSViewManager needs to persist its policy property through daemon restarts
    // and load it here, before creating whatever views it was told to (in a previous daemon lifetime)
    for (NSString* viewName in self.viewList) {
        CKKSKeychainView* view = [self findOrCreateView:viewName];
        (void)view;
    }
}

- (void)beginCloudKitOperationOfAllViews
{
    for (NSString* viewName in self.viewList) {
        CKKSKeychainView* view = [self findView:viewName];
        [view beginCloudKitOperation];
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

- (void) setOverrideCKKSViewsFromPolicy:(BOOL)value {
    _overrideCKKSViewsFromPolicy = YES;
    _valueCKKSViewsFromPolicy = value;
}

- (BOOL)useCKKSViewsFromPolicy {
    if (self.overrideCKKSViewsFromPolicy) {
        return self.valueCKKSViewsFromPolicy;
    } else {
        return os_feature_enabled(Security, CKKSViewsFromPolicy);
    }
}

- (NSString*)viewNameForItem: (SecDbItemRef) item {
    if ([self useCKKSViewsFromPolicy]) {
        CFErrorRef cferror = NULL;
        NSMutableDictionary *dict = (__bridge_transfer NSMutableDictionary*) SecDbItemCopyPListWithMask(item, ~0, &cferror);

        if(cferror) {
            secerror("ckks: Couldn't fetch attributes from item: %@", cferror);
            CFReleaseNull(cferror);
            return [self viewNameForViewHint: nil];
        }
        TPPolicy* policy = self.policy;
        if (policy == nil) {
            return [self viewNameForViewHint: nil];
        }
        NSString* view = [policy mapKeyToView:dict];
        if (view == nil) {
            return [self viewNameForViewHint: nil];
        }
        return view;
    } else {
        CFErrorRef cferror = NULL;
        NSString* viewHint = (__bridge NSString*) SecDbItemGetValue(item, &v7vwht, &cferror);

        if(cferror) {
            secerror("ckks: Couldn't fetch the viewhint for some reason: %@", cferror);
            CFReleaseNull(cferror);
            viewHint = nil;
        }

        return [self viewNameForViewHint: viewHint];
    }
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
        if(viewName) {
            secnotice("ckks", "No CKKS view for %@, skipping: %@", viewName, modified);
        } else {
            secinfo("ckks", "No CKKS view for %@, skipping: %@", viewName, modified);
        }
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
                    // The CKKSViewManager doesn't do much with its adapter, so leave as nonessentialq
                    manager = [[CKKSViewManager alloc] initWithContainerName:SecCKKSContainerName
                                                                      usePCS:SecCKKSContainerUsePCS
                                                                  sosAdapter:[[OTSOSActualAdapter alloc] initAsEssential:NO]
                                                   cloudKitClassDependencies:[CKKSCloudKitClassDependencies forLiveCloudKit]];
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

- (void)rpcResetCloudKit:(NSString*)viewName reason:(NSString *)reason reply:(void(^)(NSError* result)) reply {
    NSError* localError = nil;
    NSArray* actualViews = [self views:viewName operation:@"CloudKit reset" error:&localError];
    if(localError) {
        secerror("ckks: Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-reset-zones-waiter" withBlock:^() {}];

    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    __weak __typeof(op) weakOp = op;
    [op setCompletionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
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

    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [op setCompletionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        secnotice("ckks", "Ending rsync-CloudKit rpc with %@", strongOp.error);
        reply(CKXPCSuitableError(strongOp.error));
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksresync", view, "Beginning resync (CloudKit) for %@", view);

        CKKSSynchronizeOperation* resyncOp = [view resyncWithCloud];
        [op addSuccessDependency:resyncOp];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation:op];
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
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [op setCompletionBlock:^{
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
             fast:(bool)fast
            reply:(void(^)(NSArray<NSDictionary*>* result, NSError* error)) reply
{
    NSMutableArray* a = [[NSMutableArray alloc] init];

    // Now, query the views about their status
    NSError* error = nil;
    NSArray* actualViews = [self views:viewName operation:@"status" error:&error];
    if(!actualViews || error) {
        reply(nil, error);
        return;
    }

    WEAKIFY(self);
    CKKSResultOperation* statusOp = [CKKSResultOperation named:@"status-rpc" withBlock:^{
        STRONGIFY(self);

        if (fast == false) {
            // Get account state, even wait for it a little
            [self.accountTracker.ckdeviceIDInitialized wait:1*NSEC_PER_SEC];
            NSString *deviceID = self.accountTracker.ckdeviceID;
            NSError *deviceIDError = self.accountTracker.ckdeviceIDError;
            NSDate *lastCKKSPush = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastCKKSPush];

#define stringify(obj) CKKSNilToNSNull([obj description])
            NSDictionary* global = @{
                                     @"view":                @"global",
                                     @"reachability":        self.reachabilityTracker.currentReachability ? @"network" : @"no-network",
                                     @"ckdeviceID":          CKKSNilToNSNull(deviceID),
                                     @"ckdeviceIDError":     CKKSNilToNSNull(deviceIDError),
                                     @"lockstatetracker":    stringify(self.lockStateTracker),
                                     @"cloudkitRetryAfter":  stringify(self.zoneModifier.cloudkitRetryAfter),
                                     @"lastCKKSPush":        CKKSNilToNSNull(lastCKKSPush),
                                     };
            [a addObject: global];
        }

        for(CKKSKeychainView* view in actualViews) {
            NSDictionary* status = nil;
            ckksnotice("ckks", view, "Fetching status for %@", view.zoneName);
            if (fast) {
                status = [view fastStatus];
            } else {
                status = [view status];
            }
            ckksinfo("ckks", view, "Status is %@", status);
            if(status) {
                [a addObject: status];
            }
        }
        reply(a, nil);
    }];

    // If we're signed in, give the views a few seconds to enter what they consider to be a non-transient state (in case this daemon just launched)
    if([self.accountTracker.ckAccountInfoInitialized wait:5*NSEC_PER_SEC]) {
        secerror("ckks status: Haven't yet figured out cloudkit account state");
    }

    if(self.accountTracker.currentCKAccountInfo.accountStatus == CKAccountStatusAvailable) {
        if (![self waitForTrustReady]) {
            secerror("ckks status: Haven't yet figured out trust status");
        }

        CKKSResultOperation* blockOp = [CKKSResultOperation named:@"wait-for-status" withBlock:^{}];
        [blockOp timeout:8*NSEC_PER_SEC];
        for(CKKSKeychainView* view in actualViews) {
            [blockOp addNullableDependency:view.keyStateNonTransientDependency];
        }
        [statusOp addDependency:blockOp];
        [self.operationQueue addOperation:blockOp];
    }
    [self.operationQueue addOperation:statusOp];

    return;
}

- (void)rpcStatus:(NSString*)viewName reply:(void (^)(NSArray<NSDictionary*>* result, NSError* error))reply
{
    [self rpcStatus:viewName fast:false reply:reply];
}

- (void)rpcFastStatus:(NSString*)viewName reply:(void (^)(NSArray<NSDictionary*>* result, NSError* error))reply
{
    [self rpcStatus:viewName fast:true reply:reply];
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

- (void)rpcCKMetric:(NSString *)eventName attributes:(NSDictionary *)attributes reply:(void (^)(NSError *))reply
{
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

-(void)xpc24HrNotification {
    // XPC has poked us and said we should do some cleanup!
    secnotice("ckks", "Received a 24hr notification from XPC");

    if (![self waitForTrustReady]) {
        secnotice("ckks", "Trust not ready, still going ahead");
    }

    [[CKKSAnalytics logger] dailyCoreAnalyticsMetrics:@"com.apple.security.CKKSHealthSummary"];

    // For now, poke the views and tell them to update their device states if they'd like
    NSArray* actualViews = nil;
    @synchronized(self.views) {
        // Can't safely iterate a mutable collection, so copy it.
        actualViews = self.views.allValues;
    }

    CKOperationGroup* group = [CKOperationGroup CKKSGroupWithName:@"periodic-device-state-update"];
    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks", view, "Starting device state XPC update");
        // Let the update know it should rate-limit itself
        [view updateDeviceState:true waitForKeyHierarchyInitialization:30*NSEC_PER_SEC ckoperationGroup:group];
    }
}

- (void)haltAll
{
    [self.zoneModifier halt];

    @synchronized(self.views) {
        for(CKKSKeychainView* view in self.views.allValues) {
            [view halt];
        }
    }

}

#endif // OCTAGON
@end
