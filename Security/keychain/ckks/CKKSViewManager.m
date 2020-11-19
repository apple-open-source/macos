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
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/OctagonAPSReceiver.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/analytics/SecEventMetric.h"
#import "keychain/analytics/SecMetrics.h"

#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTDefines.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ot/ObjCImprovements.h"

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
@property CKKSNearFutureScheduler* savedTLKNotifier;;
@property NSOperationQueue* operationQueue;

@property CKKSListenerCollection<id<CKKSPeerUpdateListener>>* peerChangeListenerCollection;

@property (nonatomic) BOOL overrideCKKSViewsFromPolicy;
@property (nonatomic) BOOL valueCKKSViewsFromPolicy;
@property (nonatomic) BOOL startCKOperationAtViewCreation;

@property BOOL itemModificationsBeforePolicyLoaded;

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
        cloudKitClassDependencies:(CKKSCloudKitClassDependencies*)cloudKitClassDependencies
{
    if(self = [super init]) {
        _cloudKitClassDependencies = cloudKitClassDependencies;
        _sosPeerAdapter = sosAdapter;

        _viewAllowList = nil;
        _container = container;
        _accountTracker = accountTracker;
        _lockStateTracker = lockStateTracker;
        [_lockStateTracker addLockStateObserver:self];
        _reachabilityTracker = [[CKKSReachabilityTracker alloc] init];
        _itemModificationsBeforePolicyLoaded = NO;

        _zoneChangeFetcher = [[CKKSZoneChangeFetcher alloc] initWithContainer:_container
                                                                   fetchClass:cloudKitClassDependencies.fetchRecordZoneChangesOperationClass
                                                          reachabilityTracker:_reachabilityTracker];
        OctagonAPSReceiver* globalAPSReceiver = [OctagonAPSReceiver receiverForNamedDelegatePort:SecCKKSAPSNamedPort
                                                                              apsConnectionClass:cloudKitClassDependencies.apsConnectionClass];
        [globalAPSReceiver registerCKKSReceiver:_zoneChangeFetcher];

        _zoneModifier = [[CKKSZoneModifier alloc] initWithContainer:_container
                                                reachabilityTracker:_reachabilityTracker
                                               cloudkitDependencies:cloudKitClassDependencies];

        _operationQueue = [[NSOperationQueue alloc] init];

        _peerChangeListenerCollection = [[CKKSListenerCollection alloc] initWithName:@"sos-peer-set"];

        _views = [[NSMutableDictionary alloc] init];
        _pendingSyncCallbacks = [[NSMutableDictionary alloc] init];

        _startCKOperationAtViewCreation = NO;

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

+ (CKContainer*)makeCKContainer:(NSString*)containerName
                         usePCS:(bool)usePCS
{
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

    for (NSString* viewName in [self viewList]) {
        [[CKKSAnalytics logger] AddMultiSamplerForName:[NSString stringWithFormat:@"CKKS-%@-healthSummary", viewName] withTimeInterval:SFAnalyticsSamplerIntervalOncePerReport block:^NSDictionary<NSString *,NSNumber *> *{
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
            CKKSKeychainView* view = [self findOrCreateView:viewName];
            NSDate* dateOfLastSyncClassA = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassA zoneName:view.zoneName];
            NSDate* dateOfLastSyncClassC = [[CKKSAnalytics logger] dateOfLastSuccessForEvent:CKKSEventProcessIncomingQueueClassC zoneName:view.zoneName];
            NSDate* dateOfLastKSR = [[CKKSAnalytics logger] datePropertyForKey:CKKSAnalyticsLastKeystateReady zoneName:view.zoneName];

            NSInteger fuzzyDaysSinceClassASync = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastSyncClassA];
            NSInteger fuzzyDaysSinceClassCSync = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastSyncClassC];
            NSInteger fuzzyDaysSinceKSR = [CKKSAnalytics fuzzyDaysSinceDate:dateOfLastKSR];
            [values setValue:@(fuzzyDaysSinceClassASync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassASync", viewName]];
            [values setValue:@(fuzzyDaysSinceClassCSync) forKey:[NSString stringWithFormat:@"%@-daysSinceClassCSync", viewName]];
            [values setValue:@(fuzzyDaysSinceKSR) forKey:[NSString stringWithFormat:@"%@-daysSinceLastKeystateReady", viewName]];

            BOOL hasTLKs = [view.stateMachine.currentState isEqualToString:SecCKKSZoneKeyStateReady] || [view.stateMachine.currentState isEqualToString:SecCKKSZoneKeyStateReadyPendingUnlock];
            /* only synced recently if between [0...7, ie within 7 days */
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

#pragma mark - View List handling

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

- (NSSet<NSString*>*)viewList {
    return [self.views.allKeys copy];
}

- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy
{
    return [self setCurrentSyncingPolicy:syncingPolicy policyIsFresh:NO];
}

- (BOOL)setCurrentSyncingPolicy:(TPSyncingPolicy* _Nullable)syncingPolicy policyIsFresh:(BOOL)policyIsFresh
{
    if(syncingPolicy == nil) {
        ckksnotice_global("ckks-policy", "Nil syncing policy presented; ignoring");
        return NO;
    }

    NSSet<NSString*>* viewNames = syncingPolicy.viewList;
    ckksnotice_global("ckks-policy", "New syncing policy: %@ views: %@", syncingPolicy, viewNames);

    if(![self useCKKSViewsFromPolicy]) {
        // Thanks, but no thanks.
        viewNames = [self defaultViewList];
        ckksnotice_global("ckks-policy", "Reverting to default view list: %@", viewNames);
    }

    if(self.viewAllowList) {
        ckksnotice_global("ckks-policy", "Intersecting view list with allow list: %@", self.viewAllowList);
        NSMutableSet<NSString*>* set = [viewNames mutableCopy];
        [set intersectSet:self.viewAllowList];

        viewNames = set;
        ckksnotice_global("ckks-policy", "Final list: %@", viewNames);
    }

    // We need to not be synchronized on self.views before issuing commands to CKKS views.
    // So, store the pointers for use after the critical section.
    NSArray<CKKSKeychainView*>* activeViews = nil;
    BOOL scanAll = NO;
    BOOL viewsChanged = NO;

    @synchronized(self.views) {
        self.policy = syncingPolicy;

        NSArray* previousViewNames = [self.views.allKeys copy];

        // First, shut down any views that are no longer in the set
        for(NSString* viewName in previousViewNames) {
            if(![viewNames containsObject:viewName]) {
                ckksnotice_global("ckks-policy", "Stopping old view %@", viewName);
                [self clearView:viewName];
                viewsChanged = YES;
            }
        }

        for(NSString* viewName in viewNames) {
            CKKSKeychainView* view = nil;

            if([previousViewNames containsObject:viewName]) {
                view = [self findView:viewName];
                ckksinfo_global("ckks-policy", "Already have view %@", view);

            } else {
                view = [self findOrCreateView:viewName];
                ckksnotice_global("ckks-policy", "Created new view %@", view);
                viewsChanged = YES;
            }
        }

        activeViews = [self.views.allValues copy];

        if(self.itemModificationsBeforePolicyLoaded) {
            ckksnotice_global("ckks-policy", "Issuing scan suggestions to handle missed items");
            scanAll = YES;
            self.itemModificationsBeforePolicyLoaded = NO;
        }
    }

    for(CKKSKeychainView* view in activeViews) {
        [view setCurrentSyncingPolicy:self.policy policyIsFresh:policyIsFresh];

        if(scanAll) {
            [view scanLocalItems:@"item-added-before-policy"];
        }
    }

    // The policy is considered loaded once the views have been created
    [self.policyLoaded fulfill];
    return viewsChanged;
}

- (void)setSyncingViewsAllowList:(NSSet<NSString*>*)viewNames
{
    self.viewAllowList = viewNames;
}

- (void)resetSyncingPolicy
{
    ckksnotice_global("ckks-policy", "Setting policy to nil");
    self.policy = nil;
    self.policyLoaded = [[CKKSCondition alloc] init];

    self.startCKOperationAtViewCreation = NO;
}

#pragma mark - View Handling

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

        self.startCKOperationAtViewCreation = NO;
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

- (CKKSKeychainView* _Nullable)findView:(NSString*)viewName error:(NSError**)error
{
    if([self.policyLoaded wait:5*NSEC_PER_SEC] != 0) {
        ckkserror_global("ckks", "Haven't yet received a syncing policy; expect failure finding views");

        if([self useCKKSViewsFromPolicy]) {
            if(error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSErrorPolicyNotLoaded
                                         userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"CKKS syncing policy not yet loaded; cannot find view '%@'", viewName]}];

            }
            return nil;
        }
    }

    @synchronized(self.views) {
        CKKSKeychainView* view = self.views[viewName];
        if(!view) {
            if(error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNoSuchView
                                         userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for '%@'", viewName]}];
            }
            return nil;
        }

        return view;
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

        if(self.startCKOperationAtViewCreation) {
            [self.views[viewName] beginCloudKitOperation];
        }
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
    if(![self useCKKSViewsFromPolicy]) {
        // In the future, the CKKSViewManager needs to persist its policy property through daemon restarts
        // and load it here, before creating whatever views it was told to (in a previous daemon lifetime)
        for (NSString* viewName in [self defaultViewList]) {
            CKKSKeychainView* view = [self findOrCreateView:viewName];
            (void)view;
        }
    } else {
        ckksnotice_global("ckks-views", "Not loading default view list due to enabled CKKS4All");
    }
}

- (void)beginCloudKitOperationOfAllViews
{
    self.startCKOperationAtViewCreation = YES;

    for (CKKSKeychainView* view in self.views.allValues) {
        [view beginCloudKitOperation];
    }
}

- (void)haltZone:(NSString*)viewName
{
    @synchronized(self.views) {
        CKKSKeychainView* view = self.views[viewName];
        [view halt];
        [view cancelAllOperations];
        self.views[viewName] = nil;
    }
}

- (CKKSKeychainView*)restartZone:(NSString*)viewName {
    [self haltZone:viewName];
    CKKSKeychainView* view = [self findOrCreateView: viewName];

    [view setCurrentSyncingPolicy:self.policy policyIsFresh:NO];

    return view;
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
        BOOL viewsFromPolicy = os_feature_enabled(Security, CKKSViewsFromPolicy);

        static dispatch_once_t onceToken;
        dispatch_once(&onceToken, ^{
            ckksnotice_global("ckks", "ViewsFromPolicy feature flag: %@", viewsFromPolicy ? @"on" : @"off");
        });
        return viewsFromPolicy;
    }
}

- (NSString* _Nullable)viewNameForItem:(SecDbItemRef)item
{
    if ([self useCKKSViewsFromPolicy]) {
        CFErrorRef cferror = NULL;
        NSMutableDictionary *dict = (__bridge_transfer NSMutableDictionary*) SecDbItemCopyPListWithMask(item, kSecDbSyncFlag, &cferror);

        if(cferror) {
            ckkserror_global("ckks", "Couldn't fetch attributes from item: %@", cferror);
            CFReleaseNull(cferror);
            return nil;
        }

        // Ensure that we've added the class name, because SecDbItemCopyPListWithMask doesn't do that for some reason.
        dict[(__bridge NSString*)kSecClass] = (__bridge NSString*)item->class->name;

        NSString* view = [self.policy mapDictionaryToView:dict];
        if (view == nil) {
            ckkserror_global("ckks", "No view returned from policy (%@): %@", self.policy, item);
            return nil;
        }

        return view;
    } else {
        CFErrorRef cferror = NULL;
        NSString* viewHint = (__bridge NSString*) SecDbItemGetValue(item, &v7vwht, &cferror);

        if(cferror) {
            ckkserror_global("ckks", "Couldn't fetch the viewhint for some reason: %@", cferror);
            CFReleaseNull(cferror);
            viewHint = nil;
        }

        return [self viewNameForViewHint: viewHint];
    }
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

    SecDbItemRef modified = added ? added : deleted;

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

    bool addedSync   = added   && SecDbItemIsSyncable(added);
    bool deletedSync = deleted && SecDbItemIsSyncable(deleted);

    if(!addedSync && !deletedSync) {
        // Local-only change. Skip with prejudice.
        ckksinfo_global("ckks", "skipping sync of non-sync item (%d, %d)", addedSync, deletedSync);
        return;
    }

    NSString* viewName = nil;

    @synchronized(self.views) {
        if([self useCKKSViewsFromPolicy] && !self.policy) {
            ckkserror_global("ckks", "No policy configured(%@). Skipping item: %@", self.policy, modified);
            self.itemModificationsBeforePolicyLoaded = YES;

            return;
        }
    }

    viewName = [self viewNameForItem:modified];

    if(!viewName) {
        ckksnotice_global("ckks", "No intended CKKS view for item; skipping: %@", modified);
        return;
    }

    // Looks like a normal item. Proceed!
    CKKSKeychainView* view = [self findView:viewName];

    if(!view) {
        ckksnotice_global("ckks", "No CKKS view for %@, skipping: %@", viewName, modified);

        NSString* uuid = (__bridge NSString*) SecDbItemGetValue(modified, &v10itemuuid, NULL);
        SecBoolNSErrorCallback syncCallback = [self claimCallbackForUUID:uuid];

        if(syncCallback) {
            syncCallback(false, [NSError errorWithDomain:CKKSErrorDomain
                                                    code:CKKSNoSuchView
                                                userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for '%@'", viewName]}]);
        }
        return;
    }

    ckksnotice("ckks", view, "Routing item to zone %@: %@", viewName, modified);
    [view handleKeychainEventDbConnection:dbconn
                                   source:txionSource
                                    added:added
                                  deleted:deleted
                              rateLimiter:self.globalRateLimiter];
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
    NSError* viewError = nil;
    CKKSKeychainView* view = [self findView:viewHint error:&viewError];

    if(!view) {
        ckksnotice_global("ckks", "No CKKS view for %@, skipping setcurrent request: %@", viewHint, viewError);
        complete(viewError);
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
    NSError* viewError = nil;
    CKKSKeychainView* view = [self findView:viewHint error:&viewError];
    if(!view) {
        ckksnotice_global("ckks", "No CKKS view for %@, skipping current fetch request: %@", viewHint, viewError);
        complete(NULL, viewError);
        return;
    }

    [view getCurrentItemForAccessGroup:accessGroup
                            identifier:identifier
                       fetchCloudValue:fetchCloudValue
                              complete:complete];
}

+ (instancetype)manager
{
    return [OTManager manager].viewManager;
}

- (void)cancelPendingOperations {
    [self.savedTLKNotifier cancel];
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
    }];
}



- (NSArray<CKKSKeychainBackedKey*>* _Nullable)currentTLKsFilteredByPolicy:(BOOL)restrictToPolicy error:(NSError**)error
{
    NSError* localError = nil;
    NSArray<CKKSKeychainView*>* actualViews = [self views:nil operation:@"current TLKs" error:&localError];
    if(localError) {
        ckkserror_global("ckks", "Error getting views: %@", localError);
        if(error) {
            *error = localError;
        }
        return nil;
    }

    NSMutableArray<CKKSResultOperation<CKKSKeySetProviderOperationProtocol>*>* keyFetchOperations = [NSMutableArray array];
    for (CKKSKeychainView* view in actualViews) {
        if(restrictToPolicy && [self useCKKSViewsFromPolicy] && ![self.policy.viewsToPiggybackTLKs containsObject:view.zoneName]) {
            continue;
        }

        CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op = [view findKeySet:NO];
        [op timeout:10*NSEC_PER_SEC];
        [keyFetchOperations addObject:op];
    }

    NSMutableArray<CKKSKeychainBackedKey*>* tlks = [NSMutableArray array];

    for(CKKSResultOperation<CKKSKeySetProviderOperationProtocol>* op in keyFetchOperations) {
        [op waitUntilFinished];

        if(op.error) {
            ckkserror_global("ckks", "Error getting keyset: %@", op.error);
            if(error) {
                *error = op.error;
            }
        } else {
            if(op.keyset.tlk) {
                // Keys provided by this function must have the key material loaded
                NSError* loadError = nil;
                [op.keyset.tlk ensureKeyLoaded:&loadError];
                if(loadError) {
                    ckkserror_global("ckks", "Error loading key: %@", loadError);
                    if(error) {
                        *error = loadError;
                    }
                } else {
                    [tlks addObject:[op.keyset.tlk.keycore copy]];
                }
            } else {
                ckkserror_global("ckks", "Do not have TLK: %@", op.keyset);
            }
        }
    }

    return tlks;
}

#pragma mark - RPCs to manage and report state

- (void)performanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *counter))reply {
    reply(@{});
}

- (NSArray<CKKSKeychainView*>*)views:(NSString*)viewName operation:(NSString*)opName error:(NSError**)error
{
    return [self views:viewName operation:opName errorOnPolicyMissing:YES error:error];
}

- (NSArray<CKKSKeychainView*>*)views:(NSString*)viewName
                           operation:(NSString*)opName
                errorOnPolicyMissing:(BOOL)errorOnPolicyMissing
                               error:(NSError**)error
{
    NSArray* actualViews = nil;

    // Ensure we've actually set up, but don't wait too long. Clients get impatient.
    if([self.completedSecCKKSInitialize wait:5*NSEC_PER_SEC]) {
        ckkserror_global("ckks", "Haven't yet initialized zones; expect failure fetching views");
    }

    // If the caller doesn't mind if the policy is missing, wait some, but not the full 5s
    BOOL policyLoaded = [self.policyLoaded wait:(errorOnPolicyMissing ? 5 : 0.5)*NSEC_PER_SEC] == 0;
    if(!policyLoaded) {
        ckkserror_global("ckks", "Haven't yet received a policy; expect failure fetching views");
    }

    if(viewName) {
        CKKSKeychainView* view = errorOnPolicyMissing ? [self findView:viewName error:error] : [self findView:viewName];
        ckksnotice_global("ckks", "Received a %@ request for zone %@ (%@)", opName, viewName, view);

        if(!view) {
            return nil;
        }

        actualViews = @[view];

    } else {
        if(!policyLoaded && [self useCKKSViewsFromPolicy] && errorOnPolicyMissing) {
            if(error) {
                if(error) {
                    *error = [NSError errorWithDomain:CKKSErrorDomain
                                                 code:CKKSErrorPolicyNotLoaded
                                             userInfo:@{NSLocalizedDescriptionKey: @"CKKS syncing policy not yet loaded; cannot list all views"}];
                }
            }
            return nil;
        }

        @synchronized(self.views) {
            actualViews = [self.views.allValues copy];
            ckksnotice_global("ckks", "Received a %@ request for all zones: %@", opName, actualViews);
        }
    }
    actualViews = [actualViews sortedArrayUsingDescriptors:@[[NSSortDescriptor sortDescriptorWithKey:@"zoneName" ascending:YES]]];
    return actualViews;
}

- (void)rpcResetLocal:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    NSError* localError = nil;
    NSArray* actualViews = [self views:viewName operation:@"local reset" error:&localError];
    if(localError) {
        ckkserror_global("ckks", "Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"local-reset-zones-waiter" withBlockTakingSelf:^(CKKSResultOperation * _Nonnull strongOp) {
        if(!strongOp.error) {
            ckksnotice_global("ckksreset", "Completed rpcResetLocal");
        } else {
            ckksnotice_global("ckks", "Completed rpcResetLocal with error: %@", strongOp.error);
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
        ckkserror_global("ckks", "Error getting view %@: %@", viewName, localError);
        reply(localError);
        return;
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-reset-zones-waiter" withBlock:^() {}];

    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    __weak __typeof(op) weakOp = op;
    [op setCompletionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        if(!strongOp.error) {
            ckksnotice_global("ckksreset", "Completed rpcResetCloudKit");
        } else {
            ckksnotice_global("ckksreset", "Completed rpcResetCloudKit with error: %@", strongOp.error);
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
        ckkserror_global("ckks", "Error getting view %@: %@", viewName, localError);
        reply(localError);
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
        ckkserror_global("ckks", "Error getting view %@: %@", viewName, localError);
        reply(localError);
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

    // Now, query the views about their status. Don't wait for the policy to be loaded
    NSError* error = nil;
    NSArray* actualViews = [self views:viewName operation:@"status" errorOnPolicyMissing:NO error:&error];
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
                                     @"policy":              stringify(self.policy),
                                     @"viewsFromPolicy":     [self useCKKSViewsFromPolicy] ? @"yes" : @"no",
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
        ckkserror_global("account", "Haven't yet figured out cloudkit account state");
    }

    if(self.accountTracker.currentCKAccountInfo.accountStatus == CKAccountStatusAvailable) {
        if (![self waitForTrustReady]) {
            ckkserror_global("trust", "Haven't yet figured out trust status");
        }

        for(CKKSKeychainView* view in actualViews) {
            OctagonStateMultiStateArrivalWatcher* waitForTransient = [[OctagonStateMultiStateArrivalWatcher alloc] initNamed:@"rpc-watcher"
                                                                                                                 serialQueue:view.queue
                                                                                                                      states:CKKSKeyStateNonTransientStates()];
            [waitForTransient timeout:5*NSEC_PER_SEC];
            [view.stateMachine registerMultiStateArrivalWatcher:waitForTransient];

            [statusOp addDependency:waitForTransient.result];
        }
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
    ckksnotice_global("ckks", "Received a 24hr notification from XPC");

    if (![self waitForTrustReady]) {
        ckksnotice_global("ckks", "Trust not ready, still going ahead");
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
        [view xpc24HrNotification];
    }
}

- (void)haltAll
{
    @synchronized(self.views) {
        for(CKKSKeychainView* view in self.views.allValues) {
            [view halt];
        }
    }

    [self.zoneModifier halt];
}

#endif // OCTAGON
@end
