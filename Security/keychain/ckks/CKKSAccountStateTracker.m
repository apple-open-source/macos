/*
 * Copyright (c) 2017 Apple Inc. All Rights Reserved.
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

#if OCTAGON

#include <dispatch/dispatch.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include "keychain/SecureObjectSync/SOSInternal.h"
#include <notify.h>

#import "keychain/ot/OTManager.h"
#import "keychain/ot/OTConstants.h"
#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSAccountStateTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"
#import "keychain/categories/NSError+UsefulConstructors.h"
#import "keychain/ot/ObjCImprovements.h"


NSString* CKKSAccountStatusToString(CKKSAccountStatus status)
{
    switch(status) {
        case CKKSAccountStatusAvailable:
            return @"available";
        case CKKSAccountStatusNoAccount:
            return @"no account";
        case CKKSAccountStatusUnknown:
            return @"unknown";
    }
}

@interface CKKSAccountStateTracker ()
@property (readonly) Class<CKKSNSNotificationCenter> nsnotificationCenterClass;

@property dispatch_queue_t queue;

@property NSMapTable<dispatch_queue_t, id<CKKSCloudKitAccountStateListener>>* ckChangeListeners;

@property CKContainer* container; // used only for fetching the CKAccountStatus
@property bool firstCKAccountFetch;

// make writable
@property (nullable) OTCliqueStatusWrapper* octagonStatus;
@property (nullable) NSString* octagonPeerID;
@property CKKSCondition* octagonInformationInitialized;

@property CKKSAccountStatus hsa2iCloudAccountStatus;
@property CKKSCondition* hsa2iCloudAccountInitialized;
@end

@implementation CKKSAccountStateTracker
@synthesize octagonPeerID = _octagonPeerID;

-(instancetype)init: (CKContainer*) container nsnotificationCenterClass: (Class<CKKSNSNotificationCenter>) nsnotificationCenterClass {
    if((self = [super init])) {
        _nsnotificationCenterClass = nsnotificationCenterClass;
        // These map tables are backwards from how we'd like, but it's the best way to have weak pointers to CKKSCombinedAccountStateListener.
        _ckChangeListeners = [NSMapTable strongToWeakObjectsMapTable];

        _currentCKAccountInfo = nil;
        _ckAccountInfoInitialized = [[CKKSCondition alloc] init];

        _currentCircleStatus = [[SOSAccountStatus alloc] init:kSOSCCError error:nil];

        _container = container;

        _queue = dispatch_queue_create("ck-account-state", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        _firstCKAccountFetch = false;

        _finishedInitialDispatches = [[CKKSCondition alloc] init];
        _ckdeviceIDInitialized = [[CKKSCondition alloc] init];

        _octagonInformationInitialized = [[CKKSCondition alloc] init];

        _hsa2iCloudAccountStatus = CKKSAccountStatusUnknown;
        _hsa2iCloudAccountInitialized = [[CKKSCondition alloc] init];

        id<CKKSNSNotificationCenter> notificationCenter = [self.nsnotificationCenterClass defaultCenter];
        ckksinfo_global("ckksaccount", "Registering with notification center %@", notificationCenter);
        [notificationCenter addObserver:self selector:@selector(notifyCKAccountStatusChange:) name:CKAccountChangedNotification object:NULL];

        WEAKIFY(self);

        // If this is a live server, register with notify
        if(!SecCKKSTestsEnabled()) {
            int token = 0;
            notify_register_dispatch(kSOSCCCircleChangedNotification, &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
                STRONGIFY(self);
                [self notifyCircleChange:nil];
            });

            // Fire off a fetch of the account status. Do not go on our local queue, because notifyCKAccountStatusChange will attempt to go back on it for thread-safety.
            // Note: if you're in the tests, you must call performInitialDispatches yourself!
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
                STRONGIFY(self);
                if(!self) {
                    return;
                }
                @autoreleasepool {
                    [self performInitialDispatches];
                }
            });
        }
    }
    return self;
}

- (void)performInitialDispatches
{
    @autoreleasepool {
        [self notifyCKAccountStatusChange:nil];
        [self notifyCircleChange:nil];
        [self.finishedInitialDispatches fulfill];
    }
}

-(void)dealloc {
    id<CKKSNSNotificationCenter> notificationCenter = [self.nsnotificationCenterClass defaultCenter];
    [notificationCenter removeObserver:self];
}

-(NSString*)descriptionInternal: (NSString*) selfString {
    return [NSString stringWithFormat:@"<%@: %@, hsa2: %@>",
            selfString,
            self.currentCKAccountInfo,
            CKKSAccountStatusToString(self.hsa2iCloudAccountStatus)];
}

-(NSString*)description {
    return [self descriptionInternal: [[self class] description]];
}

-(NSString*)debugDescription {
    return [self descriptionInternal: [super description]];
}

- (dispatch_semaphore_t)registerForNotificationsOfCloudKitAccountStatusChange:(id<CKKSCloudKitAccountStateListener>)listener {
    // signals when we've successfully delivered the first account status
    dispatch_semaphore_t finishedSema = dispatch_semaphore_create(0);

    dispatch_async(self.queue, ^{
        bool alreadyRegisteredListener = false;
        NSEnumerator *enumerator = [self.ckChangeListeners objectEnumerator];
        id<CKKSCloudKitAccountStateListener> value;

        while ((value = [enumerator nextObject])) {
            // do pointer comparison
            alreadyRegisteredListener |= (value == listener);
        }

        if(listener && !alreadyRegisteredListener) {
            NSString* queueName = [NSString stringWithFormat: @"ck-account-state-%@", listener];

            dispatch_queue_t objQueue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);
            [self.ckChangeListeners setObject:listener forKey: objQueue];

            ckksinfo_global("ckksaccount-ck", "adding a new listener: %@", listener);

            // If we know the current account status, let this listener know
            if(self.firstCKAccountFetch) {
                ckksinfo_global("ckksaccount-ck", "notifying new listener %@ of current state %@", listener, self.currentCKAccountInfo);

                dispatch_group_t g = dispatch_group_create();
                if(!g) {
                    ckkserror_global("ckksaccount-ck", "Unable to get dispatch group.");
                    dispatch_semaphore_signal(finishedSema);
                    return;
                }

                [self _onqueueDeliverCurrentCloudKitState:listener listenerQueue:objQueue oldStatus:nil group:g];

                dispatch_group_notify(g, self.queue, ^{
                    dispatch_semaphore_signal(finishedSema);
                });
            } else {
                dispatch_semaphore_signal(finishedSema);
            }
        } else {
            dispatch_semaphore_signal(finishedSema);
        }
    });

    return finishedSema;
}

- (dispatch_semaphore_t)notifyCKAccountStatusChange:(__unused id)object {
    // signals when this notify is Complete, including all downcalls.
    dispatch_semaphore_t finishedSema = dispatch_semaphore_create(0);

    WEAKIFY(self);

    [self.container accountInfoWithCompletionHandler:^(CKAccountInfo* ckAccountInfo, NSError * _Nullable error) {
        STRONGIFY(self);

        if(error) {
            ckkserror_global("ckksaccount", "error getting account info: %@", error);
            dispatch_semaphore_signal(finishedSema);
            return;
        }

        dispatch_sync(self.queue, ^{
            self.firstCKAccountFetch = true;
            ckksnotice_global("ckksaccount", "received CK Account info: %@", ckAccountInfo);
            [self _onqueueUpdateAccountState:ckAccountInfo deliveredSemaphore:finishedSema];
        });
    }];

    return finishedSema;
}

// Takes the new ckAccountInfo we're moving to
-(void)_onqueueUpdateCKDeviceID:(CKAccountInfo*)ckAccountInfo {
    dispatch_assert_queue(self.queue);
    WEAKIFY(self);

    // If we're in an account, opportunistically fill in the device id
    if(ckAccountInfo.accountStatus == CKAccountStatusAvailable) {
        [self.container fetchCurrentDeviceIDWithCompletionHandler:^(NSString* deviceID, NSError* ckerror) {
            STRONGIFY(self);
            if(!self) {
                ckkserror_global("ckksaccount", "Received fetchCurrentDeviceIDWithCompletionHandler callback with null AccountStateTracker");
                return;
            }

            // Make sure you synchronize here; if we've logged out before the callback returns, don't record the result
            dispatch_async(self.queue, ^{
                STRONGIFY(self);
                if(self.currentCKAccountInfo.accountStatus == CKAccountStatusAvailable) {
                    ckksnotice_global("ckksaccount", "CloudKit deviceID is: %@ %@", deviceID, ckerror);

                    self.ckdeviceID = deviceID;
                    self.ckdeviceIDError = ckerror;
                    [self.ckdeviceIDInitialized fulfill];
                } else {
                    // Logged out! No ckdeviceid.
                    ckkserror_global("ckksaccount", "Logged back out but still received a fetchCurrentDeviceIDWithCompletionHandler callback");

                    self.ckdeviceID = nil;
                    self.ckdeviceIDError = nil;
                    // Don't touch the ckdeviceIDInitialized object; it should have been reset when the logout happened.
                }
            });
        }];
    } else {
        // Logging out? no more device ID.
        self.ckdeviceID = nil;
        self.ckdeviceIDError = nil;
        self.ckdeviceIDInitialized = [[CKKSCondition alloc] init];
    }
}

- (dispatch_semaphore_t)notifyCircleChange:(__unused id)object {
    dispatch_semaphore_t finishedSema = dispatch_semaphore_create(0);

    SOSAccountStatus* sosstatus = [CKKSAccountStateTracker getCircleStatus];
    dispatch_sync(self.queue, ^{
        if(self.currentCircleStatus == nil || ![self.currentCircleStatus isEqual:sosstatus]) {
            ckksnotice_global("ckksaccount", "moving to circle status: %@", sosstatus);
            self.currentCircleStatus = sosstatus;

            if (sosstatus.status == kSOSCCInCircle) {
                [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastInCircle];
            }
            [self _onqueueUpdateCirclePeerID:sosstatus];
        }
        dispatch_semaphore_signal(finishedSema);
    });

    return finishedSema;
}

// Pulled out for mocking purposes
+ (void)fetchCirclePeerID:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))callback {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        CFErrorRef cferror = nil;
        SOSPeerInfoRef egoPeerInfo = SOSCCCopyMyPeerInfo(&cferror);
        NSString* egoPeerID = egoPeerInfo ? (NSString*)CFBridgingRelease(CFRetainSafe(SOSPeerInfoGetPeerID(egoPeerInfo))) : nil;
        CFReleaseNull(egoPeerInfo);

        callback(egoPeerID, CFBridgingRelease(cferror));
    });
}

// Takes the new ckAccountInfo we're moving to
- (void)_onqueueUpdateCirclePeerID:(SOSAccountStatus*)sosstatus {
    dispatch_assert_queue(self.queue);
    WEAKIFY(self);

    // If we're in a circle, fetch the peer id
    if(sosstatus.status == kSOSCCInCircle) {
        [CKKSAccountStateTracker fetchCirclePeerID:^(NSString* peerID, NSError* error) {
            STRONGIFY(self);
            if(!self) {
                ckkserror_global("ckksaccount", "Received fetchCirclePeerID callback with null AccountStateTracker");
                return;
            }

            dispatch_async(self.queue, ^{
                STRONGIFY(self);

                if(self.currentCircleStatus && self.currentCircleStatus.status == kSOSCCInCircle) {
                    ckksnotice_global("ckksaccount", "Circle peerID is: %@ %@", peerID, error);
                    // Still in circle. Proceed.
                    self.accountCirclePeerID = peerID;
                    self.accountCirclePeerIDError = error;
                    [self.accountCirclePeerIDInitialized fulfill];
                } else {
                    ckkserror_global("ckksaccount", "Out of circle but still received a fetchCirclePeerID callback");
                    // Not in-circle. Throw away circle id.
                    self.accountCirclePeerID = nil;
                    self.accountCirclePeerIDError = nil;
                    // Don't touch the accountCirclePeerIDInitialized object; it should have been reset when the logout happened.
                }
            });
        }];
    } else {
        // Not in-circle, reset circle ID
        ckksnotice_global("ckksaccount", "out of circle(%@): resetting peer ID", sosstatus);
        self.accountCirclePeerID = nil;
        self.accountCirclePeerIDError = nil;
        self.accountCirclePeerIDInitialized = [[CKKSCondition alloc] init];
    }
}

- (void)_onqueueUpdateAccountState:(CKAccountInfo*)ckAccountInfo
                deliveredSemaphore:(dispatch_semaphore_t)finishedSema
{
    // Launder the finishedSema into a dispatch_group.
    // _onqueueUpdateAccountState:circle:dispatchGroup: will then add any blocks it thinks is necessary,
    // then the group will fire the semaphore.
    dispatch_assert_queue(self.queue);

    dispatch_group_t g = dispatch_group_create();
    if(!g) {
        ckksnotice_global("ckksaccount", "Unable to get dispatch group.");
        dispatch_semaphore_signal(finishedSema);
        return;
    }

    [self _onqueueUpdateAccountState:ckAccountInfo
                       dispatchGroup:g];

    dispatch_group_notify(g, self.queue, ^{
        dispatch_semaphore_signal(finishedSema);
    });
}

- (void)_onqueueUpdateAccountState:(CKAccountInfo*)ckAccountInfo
                     dispatchGroup:(dispatch_group_t)g
{
    dispatch_assert_queue(self.queue);

    if([self.currentCKAccountInfo isEqual: ckAccountInfo]) {
        // no-op.
        ckksinfo_global("ckksaccount", "received another notification of CK Account State %@", ckAccountInfo);
        return;
    }

    if((self.currentCKAccountInfo == nil && ckAccountInfo != nil) ||
       !(self.currentCKAccountInfo == ckAccountInfo || [self.currentCKAccountInfo isEqual: ckAccountInfo])) {
        ckksnotice_global("ckksaccount", "moving to CK Account info: %@", ckAccountInfo);
        CKAccountInfo* oldAccountInfo = self.currentCKAccountInfo;
        self.currentCKAccountInfo = ckAccountInfo;
        [self.ckAccountInfoInitialized fulfill];

        [self _onqueueUpdateCKDeviceID: ckAccountInfo];

        [self _onqueueDeliverCloudKitStateChanges:oldAccountInfo dispatchGroup:g];
    }
}

- (void)_onqueueDeliverCloudKitStateChanges:(CKAccountInfo*)oldStatus
                              dispatchGroup:(dispatch_group_t)g
{
    dispatch_assert_queue(self.queue);

    NSEnumerator *enumerator = [self.ckChangeListeners keyEnumerator];
    dispatch_queue_t dq;

    // Queue up the changes for each listener.
    while ((dq = [enumerator nextObject])) {
        id<CKKSCloudKitAccountStateListener> listener = [self.ckChangeListeners objectForKey: dq];
        [self _onqueueDeliverCurrentCloudKitState:listener listenerQueue:dq oldStatus:oldStatus group:g];
    }
}

- (void)_onqueueDeliverCurrentCloudKitState:(id<CKKSCloudKitAccountStateListener>)listener
                              listenerQueue:(dispatch_queue_t)listenerQueue
                                  oldStatus:(CKAccountInfo* _Nullable)oldStatus
                                      group:(dispatch_group_t)g
{
    dispatch_assert_queue(self.queue);

    __weak __typeof(listener) weakListener = listener;

    if(listener) {
        dispatch_group_async(g, listenerQueue, ^{
            [weakListener cloudkitAccountStateChange:oldStatus to:self.currentCKAccountInfo];
        });
    }
}

- (BOOL)notifyCKAccountStatusChangeAndWait:(dispatch_time_t)timeout
{
    return dispatch_semaphore_wait([self notifyCKAccountStatusChange:nil], dispatch_time(DISPATCH_TIME_NOW, timeout)) == 0;
}

-(void)notifyCKAccountStatusChangeAndWaitForSignal {
    [self notifyCKAccountStatusChangeAndWait:DISPATCH_TIME_FOREVER];
}

-(void)notifyCircleStatusChangeAndWaitForSignal {
    dispatch_semaphore_wait([self notifyCircleChange: nil], DISPATCH_TIME_FOREVER);
}

-(dispatch_group_t)checkForAllDeliveries {

    dispatch_group_t g = dispatch_group_create();
    if(!g) {
        ckksnotice_global("ckksaccount", "Unable to get dispatch group.");
        return nil;
    }

    dispatch_sync(self.queue, ^{
        NSEnumerator *enumerator = [self.ckChangeListeners keyEnumerator];
        dispatch_queue_t dq;

        // Queue up the changes for each listener.
        while ((dq = [enumerator nextObject])) {
            id<CKKSCloudKitAccountStateListener> listener = [self.ckChangeListeners objectForKey: dq];

            ckksinfo_global("ckksaccountblock", "Starting blocking for listener %@", listener);
            WEAKIFY(listener);
            dispatch_group_async(g, dq, ^{
                STRONGIFY(listener);
                // Do nothing in particular. It's just important that this block runs.
                ckksinfo_global("ckksaccountblock", "Done blocking for listener %@", listener);
            });
        }
    });

    return g;
}

// This is its own function to allow OCMock to swoop in and replace the result during testing.
+ (SOSAccountStatus*)getCircleStatus {
    CFErrorRef cferror = NULL;

    SOSCCStatus status = SOSCCThisDeviceIsInCircle(&cferror);
    if(cferror) {
        ckkserror_global("ckksaccount", "error getting circle status: %@", cferror);
        return [[SOSAccountStatus alloc] init:kSOSCCError error:CFBridgingRelease(cferror)];
    }

    return [[SOSAccountStatus alloc] init:status error:nil];
}

+ (NSString*)stringFromAccountStatus: (CKKSAccountStatus) status {
    switch(status) {
        case CKKSAccountStatusUnknown: return @"account state unknown";
        case CKKSAccountStatusAvailable: return @"logged in";
        case CKKSAccountStatusNoAccount: return @"no account";
    }
}

- (void)triggerOctagonStatusFetch
{
    WEAKIFY(self);

    __block CKKSCondition* blockPointer = nil;
    dispatch_sync(self.queue, ^{
        self.octagonInformationInitialized = [[CKKSCondition alloc] initToChain:self.octagonInformationInitialized];
        blockPointer = self.octagonInformationInitialized;
    });

    // Explicitly do not use the OTClique API, as that might include SOS status as well
    OTOperationConfiguration* config = [[OTOperationConfiguration alloc] init];
    config.timeoutWaitForCKAccount = 100*NSEC_PER_MSEC;
    [[OTManager manager] fetchTrustStatus:nil
                                  context:OTDefaultContext
                            configuration:config
                                    reply:^(CliqueStatus status, NSString * _Nullable peerID, NSNumber * _Nullable numberOfPeersInOctagon, BOOL isExcluded, NSError * _Nullable error) {
                                        STRONGIFY(self);

                                        dispatch_sync(self.queue, ^{
                                            if(error) {
                                                ckkserror_global("ckksaccount", "error getting octagon status: %@", error);
                                                self.octagonStatus = [[OTCliqueStatusWrapper alloc] initWithStatus:CliqueStatusError];
                                            } else {
                                                ckksnotice_global("ckksaccount", "Caching octagon status as (%@, %@)", OTCliqueStatusToString(status), peerID);
                                                self.octagonStatus = [[OTCliqueStatusWrapper alloc] initWithStatus:status];
                                            }

                                            self.octagonPeerID = peerID;
                                            [blockPointer fulfill];
                                        });
                                    }];
}


- (void)setHSA2iCloudAccountStatus:(CKKSAccountStatus)status
{
    self.hsa2iCloudAccountStatus = status;
    if(status == CKKSAccountStatusUnknown) {
        self.hsa2iCloudAccountInitialized = [[CKKSCondition alloc] initToChain:self.hsa2iCloudAccountInitialized];
    } else {
        [self.hsa2iCloudAccountInitialized fulfill];
    }
}

@end

@implementation SOSAccountStatus
- (instancetype)init:(SOSCCStatus)status error:(NSError*)error
{
    if((self = [super init])) {
        _status = status;
        _error = error;
    }
    return self;
}

- (BOOL)isEqual:(id)object
{
    if(![object isKindOfClass:[SOSAccountStatus class]]) {
        return NO;
    }

    if(object == nil) {
        return NO;
    }

    SOSAccountStatus* obj = (SOSAccountStatus*) object;
    return self.status == obj.status &&
            ((self.error == nil && obj.error == nil) || [self.error isEqual:obj.error]);
}

- (NSString*)description
{
    return [NSString stringWithFormat:@"<SOSStatus: %@ (%@)>", SOSCCGetStatusDescription(self.status), self.error];
}
@end


@implementation OTCliqueStatusWrapper
- (instancetype)initWithStatus:(CliqueStatus)status
{
    if((self = [super init])) {
        _status = status;
    }
    return self;
}

- (BOOL)isEqual:(id)object
{
    if(![object isKindOfClass:[OTCliqueStatusWrapper class]]) {
        return NO;
    }

    OTCliqueStatusWrapper* obj = (OTCliqueStatusWrapper*)object;
    return obj.status == self.status;
}
- (NSString*)description
{
    return [NSString stringWithFormat:@"<CliqueStatus: %@>", OTCliqueStatusToString(self.status)];
}
@end


#endif // OCTAGON
