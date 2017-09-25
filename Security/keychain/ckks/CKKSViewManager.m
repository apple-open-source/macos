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
#import "CKKSAnalyticsLogger.h"

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
#endif

@interface CKKSViewManager () <NSXPCListenerDelegate>
#if OCTAGON
@property NSXPCListener *listener;

// Once you set these, all CKKSKeychainViews created will use them
@property (readonly) Class<CKKSFetchRecordZoneChangesOperation> fetchRecordZoneChangesOperationClass;
@property (readonly) Class<CKKSModifySubscriptionsOperation> modifySubscriptionsOperationClass;
@property (readonly) Class<CKKSModifyRecordZonesOperation> modifyRecordZonesOperationClass;
@property (readonly) Class<CKKSAPSConnection> apsConnectionClass;
@property (readonly) Class<CKKSNotifier> notifierClass;
@property (readonly) Class<CKKSNSNotificationCenter> nsnotificationCenterClass;

@property NSMutableDictionary<NSString*, CKKSKeychainView*>* views;

@property NSMutableDictionary<NSString*, SecBoolNSErrorCallback>* pendingSyncCallbacks;
@property CKKSNearFutureScheduler* savedTLKNotifier;;
@property NSOperationQueue* operationQueue;
#endif
@end

@implementation CKKSViewManager
#if OCTAGON

- (instancetype)initCloudKitWithContainerName: (NSString*) containerName usePCS:(bool)usePCS {
    return [self initWithContainerName:containerName
                                usePCS:usePCS
  fetchRecordZoneChangesOperationClass:[CKFetchRecordZoneChangesOperation class]
     modifySubscriptionsOperationClass:[CKModifySubscriptionsOperation class]
       modifyRecordZonesOperationClass:[CKModifyRecordZonesOperation class]
                    apsConnectionClass:[APSConnection class]
             nsnotificationCenterClass:[NSNotificationCenter class]
                         notifierClass:[CKKSNotifyPostNotifier class]
                             setupHold:nil];
}

- (instancetype)initWithContainerName: (NSString*) containerName
                               usePCS:(bool)usePCS
 fetchRecordZoneChangesOperationClass: (Class<CKKSFetchRecordZoneChangesOperation>) fetchRecordZoneChangesOperationClass
    modifySubscriptionsOperationClass: (Class<CKKSModifySubscriptionsOperation>) modifySubscriptionsOperationClass
      modifyRecordZonesOperationClass: (Class<CKKSModifyRecordZonesOperation>) modifyRecordZonesOperationClass
                   apsConnectionClass: (Class<CKKSAPSConnection>) apsConnectionClass
            nsnotificationCenterClass: (Class<CKKSNSNotificationCenter>) nsnotificationCenterClass
                        notifierClass: (Class<CKKSNotifier>) notifierClass
                            setupHold: (NSOperation*) setupHold {
    if(self = [super init]) {
        _fetchRecordZoneChangesOperationClass = fetchRecordZoneChangesOperationClass;
        _modifySubscriptionsOperationClass = modifySubscriptionsOperationClass;
        _modifyRecordZonesOperationClass = modifyRecordZonesOperationClass;
        _apsConnectionClass = apsConnectionClass;
        _nsnotificationCenterClass = nsnotificationCenterClass;
        _notifierClass = notifierClass;

        _container = [self makeCKContainer: containerName usePCS:usePCS];
        _accountTracker = [[CKKSCKAccountStateTracker alloc] init:self.container nsnotificationCenterClass:nsnotificationCenterClass];
        _lockStateTracker = [[CKKSLockStateTracker alloc] init];

        _operationQueue = [[NSOperationQueue alloc] init];

        _views = [[NSMutableDictionary alloc] init];
        _pendingSyncCallbacks = [[NSMutableDictionary alloc] init];

        _zoneStartupDependency = setupHold;
        _initializeNewZones = false;

        _completedSecCKKSInitialize = [[CKKSCondition alloc] init];

        __weak __typeof(self) weakSelf = self;
        _savedTLKNotifier = [[CKKSNearFutureScheduler alloc] initWithName: @"newtlks" delay:5*NSEC_PER_SEC keepProcessAlive:true block:^{
            [weakSelf notifyNewTLKsInKeychain];
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

-(void)dealloc {
    [self clearAllViews];
}

dispatch_queue_t globalZoneStateQueue = NULL;
dispatch_once_t globalZoneStateQueueOnce;

// We can't load the rate limiter in an init method, as the method might end up calling itself (if the database layer isn't yet initialized).
// Lazy-load it here.
- (CKKSRateLimiter*)getGlobalRateLimiter {
    dispatch_once(&globalZoneStateQueueOnce, ^{
        globalZoneStateQueue = dispatch_queue_create("CKKS global zone state", DISPATCH_QUEUE_SERIAL);
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

// Mostly exists to be mocked out.
+(NSSet*)viewList {
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
                                                          savedTLKNotifier: self.savedTLKNotifier
                                      fetchRecordZoneChangesOperationClass: self.fetchRecordZoneChangesOperationClass
                                         modifySubscriptionsOperationClass: self.modifySubscriptionsOperationClass
                                           modifyRecordZonesOperationClass: self.modifyRecordZonesOperationClass
                                                        apsConnectionClass: self.apsConnectionClass
                                                             notifierClass: self.notifierClass];

        if(self.zoneStartupDependency) {
            [self.views[viewName].zoneSetupOperation addDependency: self.zoneStartupDependency];
        }

        if(self.initializeNewZones) {
            [self.views[viewName] initializeZone];
        }

        return self.views[viewName];
    }
}
+ (CKKSKeychainView*)findOrCreateView:(NSString*)viewName {
    return [[CKKSViewManager manager] findOrCreateView: viewName];
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

        NSSet* viewSet = [CKKSViewManager viewList];
        for(NSString* s in viewSet) {
            [self findOrCreateView:s]; // initializes any newly-created views
        }
    }
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
            ckksnotice("ckks", view, "Potential new key material from %@ (source %lu)", keyViewName, txionSource);
            [view keyStateMachineRequestProcess];
        } else {
            ckksnotice("ckks", view, "Ignoring potential new key material from %@ (source %lu)", keyViewName, txionSource);
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

-(void)setCurrentItemForAccessGroup:(SecDbItemRef)newItem
                               hash:(NSData*)newItemSHA1
                        accessGroup:(NSString*)accessGroup
                         identifier:(NSString*)identifier
                           viewHint:(NSString*)viewHint
                          replacing:(SecDbItemRef)oldItem
                               hash:(NSData*)oldItemSHA1
                           complete:(void (^) (NSError* operror)) complete
{
    CKKSKeychainView* view = [self findView:viewHint];

    if(!view) {
        secinfo("ckks", "No CKKS view for %@, skipping current request", viewHint);
        complete([NSError errorWithDomain:@"securityd"
                                     code:kSOSCCNoSuchView
                                 userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No syncing view for view hint '%@'", viewHint]}]);
        return;
    }

    [view setCurrentItemForAccessGroup:newItem
                                  hash:newItemSHA1
                           accessGroup:accessGroup
                            identifier:identifier
                             replacing:oldItem
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
        secinfo("ckks", "No CKKS view for %@, skipping current fetch request", viewHint);
        complete(NULL, [NSError errorWithDomain:@"securityd"
                                     code:kSOSCCNoSuchView
                                 userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No view for '%@'", viewHint]}]);
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
                } else if (manager == nil) {
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
    [CKKSViewManager syncBackupAndNotifyAboutSync];
}

+(void)syncBackupAndNotifyAboutSync {
    SOSAccount* account = (__bridge SOSAccount*)SOSKeychainAccountGetSharedAccount();

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

#pragma mark - XPC Endpoint

- (xpc_endpoint_t)xpcControlEndpoint {
    return [_listener.endpoint _endpoint];
}

- (BOOL)listener:(__unused NSXPCListener *)listener shouldAcceptNewConnection:(NSXPCConnection *)newConnection {
    NSNumber *num = [newConnection valueForEntitlement:(__bridge NSString *)kSecEntitlementPrivateCKKS];
    if (![num isKindOfClass:[NSNumber class]] || ![num boolValue]) {
        secinfo("ckks", "Client pid: %d doesn't have entitlement: %@",
                [newConnection processIdentifier], kSecEntitlementPrivateCKKS);
        return NO;
    }
    newConnection.exportedInterface = CKKSSetupControlProtocol([NSXPCInterface interfaceWithProtocol:@protocol(CKKSControlProtocol)]);
    newConnection.exportedObject = self;

    [newConnection resume];

    return YES;
}
#pragma mark - RPCs to manage and report state

- (void)performanceCounters:(void(^)(NSDictionary <NSString *, NSNumber *> *counter))reply {
    reply(@{ @"fake" : @(10) });
}

- (void)rpcResetLocal:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    NSArray* actualViews = nil;
    if(viewName) {
        secnotice("ckksreset", "Received a local reset RPC for zone %@", viewName);
        CKKSKeychainView* view = self.views[viewName];

        if(!view) {
            secerror("ckks: Zone %@ does not exist!", viewName);
            reply([NSError errorWithDomain:@"securityd"
                                      code:kSOSCCNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No view for '%@'", viewName]}]);
            return;
        }

        actualViews = @[view];
    } else {
        secnotice("ckksreset", "Received a local reset RPC for all zones");
        @synchronized(self.views) {
            // Can't safely iterate a mutable collection, so copy it.
            actualViews = [self.views.allValues copy];
        }
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"local-reset-zones-waiter" withBlock:^{}];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksreset", view, "Beginning local reset for %@", view);
        [op addSuccessDependency:[view resetLocalData]];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error) {
        secnotice("ckksreset", "Completed rpcResetLocal");
    } else {
        secnotice("ckksreset", "Completed rpcResetLocal with error: %@", op.error);
    }
    reply(op.error);
}

- (void)rpcResetCloudKit:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    NSArray* actualViews = nil;
    if(viewName) {
        secnotice("ckksreset", "Received a cloudkit reset RPC for zone %@", viewName);
        CKKSKeychainView* view = self.views[viewName];

        if(!view) {
            secerror("ckks: Zone %@ does not exist!", viewName);
            reply([NSError errorWithDomain:@"securityd"
                                      code:kSOSCCNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No view for '%@'", viewName]}]);
            return;
        }

        actualViews = @[view];
    } else {
        secnotice("ckksreset", "Received a cloudkit reset RPC for all zones");
        @synchronized(self.views) {
            // Can't safely iterate a mutable collection, so copy it.
            actualViews = [self.views.allValues copy];
        }
    }

    CKKSResultOperation* op = [CKKSResultOperation named:@"cloudkit-reset-zones-waiter" withBlock:^{}];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksreset", view, "Beginning CloudKit reset for %@", view);
        [op addSuccessDependency:[view resetCloudKitZone]];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation: op];

    [op waitUntilFinished];
    if(op.error) {
        secnotice("ckksreset", "Completed rpcResetCloudKit");
    } else {
        secnotice("ckksreset", "Completed rpcResetCloudKit with error: %@", op.error);
     }
    reply(op.error);
}

- (void)rpcResync:(NSString*)viewName reply: (void(^)(NSError* result)) reply {
    secnotice("ckksresync", "Received a resync RPC for zone %@. Beginning resync...", viewName);

    NSArray* actualViews = nil;
    if(viewName) {
        secnotice("ckks", "Received a resync RPC for zone %@", viewName);
        CKKSKeychainView* view = self.views[viewName];

        if(!view) {
            secerror("ckks: Zone %@ does not exist!", viewName);
            reply(nil);
            return;
        }

        actualViews = @[view];

    } else {
        @synchronized(self.views) {
            // Can't safely iterate a mutable collection, so copy it.
            actualViews = [self.views.allValues copy];
        }
    }

    CKKSResultOperation* op = [[CKKSResultOperation alloc] init];
    op.name = @"rpc-resync";
    __weak __typeof(op) weakOp = op;
    [op addExecutionBlock:^{
        __strong __typeof(op) strongOp = weakOp;
        secnotice("ckks", "Ending rsync rpc with %@", strongOp.error);
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckksresync", view, "Beginning resync for %@", view);

        CKKSSynchronizeOperation* resyncOp = [view resyncWithCloud];
        [op addSuccessDependency:resyncOp];
    }

    [op timeout:120*NSEC_PER_SEC];
    [self.operationQueue addOperation:op];
    [op waitUntilFinished];
    reply(op.error);
}

- (void)rpcStatus: (NSString*)viewName reply: (void(^)(NSArray<NSDictionary*>* result, NSError* error)) reply {
    NSMutableArray* a = [[NSMutableArray alloc] init];

    NSArray* actualViews = nil;
    if(viewName) {
        secnotice("ckks", "Received a status RPC for zone %@", viewName);
        CKKSKeychainView* view = self.views[viewName];

        if(!view) {
            secerror("ckks: Zone %@ does not exist!", viewName);
            reply(nil, nil);
            return;
        }

        actualViews = @[view];

    } else {
        @synchronized(self.views) {
            // Can't safely iterate a mutable collection, so copy it.
            actualViews = self.views.allValues;
        }
    }

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks", view, "Fetching status for %@", view.zoneName);
        NSDictionary* status = [view status];
        ckksinfo("ckks", view, "Status is %@", status);
        if(status) {
            [a addObject: status];
        }
    }
    reply(a, nil);
    return;
}

- (void)rpcFetchAndProcessChanges:(NSString*)viewName reply: (void(^)(NSError* result))reply {
    [self rpcFetchAndProcessChanges:viewName classA:false reply: (void(^)(NSError* result))reply];
}

- (void)rpcFetchAndProcessClassAChanges:(NSString*)viewName reply: (void(^)(NSError* result))reply {
    [self rpcFetchAndProcessChanges:viewName classA:true reply:(void(^)(NSError* result))reply];
}

- (void)rpcFetchAndProcessChanges:(NSString*)viewName classA:(bool)classAError reply: (void(^)(NSError* result)) reply {
    NSArray* actualViews = nil;
    if(viewName) {
        secnotice("ckks", "Received a fetch RPC for zone %@", viewName);
        CKKSKeychainView* view = self.views[viewName];

        if(!view) {
            secerror("ckks: Zone %@ does not exist!", viewName);
            reply([NSError errorWithDomain:@"securityd"
                                      code:kSOSCCNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No view for '%@'", viewName]}]);
            return;
        }

        actualViews = @[view];
    } else {
        secnotice("ckks", "Received a fetch RPC for all zones");
        @synchronized(self.views) {
            // Can't safely iterate a mutable collection, so copy it.
            actualViews = [self.views.allValues copy];
        }
    }

    CKKSResultOperation* blockOp = [[CKKSResultOperation alloc] init];
    blockOp.name = @"rpc-fetch-and-process-result";
    __weak __typeof(blockOp) weakBlockOp = blockOp;
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [blockOp setCompletionBlock:^{
        __strong __typeof(blockOp) strongBlockOp = weakBlockOp;
        [strongBlockOp allDependentsSuccessful];
        reply(strongBlockOp.error);
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks", view, "Beginning fetch for %@", view);

        CKKSResultOperation* op = [view processIncomingQueue:classAError after:[view.zoneChangeFetcher requestSuccessfulFetch: CKKSFetchBecauseAPIFetchRequest]];
        [blockOp addDependency:op];
    }

    [self.operationQueue addOperation: [blockOp timeout:60*NSEC_PER_SEC]];
}

- (void)rpcPushOutgoingChanges:(NSString*)viewName reply: (void(^)(NSError* result))reply {
    NSArray* actualViews = nil;
    if(viewName) {
        secnotice("ckks", "Received a push RPC for zone %@", viewName);
        CKKSKeychainView* view = self.views[viewName];

        if(!view) {
            secerror("ckks: Zone %@ does not exist!", viewName);
            reply([NSError errorWithDomain:@"securityd"
                                      code:kSOSCCNoSuchView
                                  userInfo:@{NSLocalizedDescriptionKey: [NSString stringWithFormat: @"No view for '%@'", viewName]}]);
            return;
        }

        actualViews = @[view];
    } else {
        secnotice("ckks", "Received a push RPC for all zones");
        @synchronized(self.views) {
            // Can't safely iterate a mutable collection, so copy it.
            actualViews = [self.views.allValues copy];
        }
    }

    CKKSResultOperation* blockOp = [[CKKSResultOperation alloc] init];
    blockOp.name = @"rpc-push";
    __weak __typeof(blockOp) weakBlockOp = blockOp;
    // Use the completion block instead of the operation block, so that it runs even if the cancel fires
    [blockOp setCompletionBlock:^{
        __strong __typeof(blockOp) strongBlockOp = weakBlockOp;
        [strongBlockOp allDependentsSuccessful];
        reply(strongBlockOp.error);
    }];

    for(CKKSKeychainView* view in actualViews) {
        ckksnotice("ckks-rpc", view, "Beginning push for %@", view);

        CKKSResultOperation* op = [view processOutgoingQueue: [CKOperationGroup CKKSGroupWithName:@"rpc-push"]];
        [blockOp addDependency:op];
    }

    [self.operationQueue addOperation: [blockOp timeout:60*NSEC_PER_SEC]];
}

- (void)rpcGetAnalyticsSysdiagnoseWithReply:(void (^)(NSString* sysdiagnose, NSError* error))reply
{
    NSError* error = nil;
    NSString* sysdiagnose = [[CKKSAnalyticsLogger logger] getSysdiagnoseDumpWithError:&error];
    reply(sysdiagnose, error);
}

- (void)rpcGetAnalyticsJSONWithReply:(void (^)(NSData* json, NSError* error))reply
{
    NSError* error = nil;
    NSData* json = [[CKKSAnalyticsLogger logger] getLoggingJSONWithError:&error];
    reply(json, error);
}

- (void)rpcForceUploadAnalyticsWithReply:(void (^)(BOOL success, NSError* error))reply
{
    NSError* error = nil;
    BOOL result = [[CKKSAnalyticsLogger logger] forceUploadWithError:&error];
    reply(result, error);
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
        [view updateDeviceState:true ckoperationGroup:group];
    }
}

#endif // OCTAGON
@end
