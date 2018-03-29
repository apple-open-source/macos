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
#include <utilities/debugging.h>
#include <Security/SecureObjectSync/SOSCloudCircle.h>
#include <Security/SecureObjectSync/SOSCloudCircleInternal.h>
#include <Security/SecureObjectSync/SOSInternal.h>
#include <notify.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CloudKitCategories.h"
#import "keychain/ckks/CKKSCKAccountStateTracker.h"
#import "keychain/ckks/CKKSAnalytics.h"

@interface CKKSCKAccountStateTracker ()
@property (readonly) Class<CKKSNSNotificationCenter> nsnotificationCenterClass;

@property CKKSAccountStatus currentComputedAccountStatus;
@property (nullable, atomic) NSError* currentAccountError;

@property dispatch_queue_t queue;

@property NSMapTable<dispatch_queue_t, id<CKKSAccountStateListener>>* changeListeners;
@property CKContainer* container; // used only for fetching the CKAccountStatus

/* We have initialization races. We should report CKKSAccountStatusUnknown until both of 
 * these are true, otherwise on a race, it looks like we logged out. */
@property bool firstCKAccountFetch;
@property bool firstSOSCircleFetch;
@end

@implementation CKKSCKAccountStateTracker

-(instancetype)init: (CKContainer*) container nsnotificationCenterClass: (Class<CKKSNSNotificationCenter>) nsnotificationCenterClass {
    if((self = [super init])) {
        _nsnotificationCenterClass = nsnotificationCenterClass;
        _changeListeners = [NSMapTable strongToWeakObjectsMapTable]; // Backwards from how we'd like, but it's the best way to have weak pointers to CKKSAccountStateListener.
        _currentCKAccountInfo = nil;
        _currentCircleStatus = kSOSCCError;

        _currentComputedAccountStatus = CKKSAccountStatusUnknown;
        _currentComputedAccountStatusValid = [[CKKSCondition alloc] init];

        _container = container;

        _queue = dispatch_queue_create("ck-account-state", DISPATCH_QUEUE_SERIAL_WITH_AUTORELEASE_POOL);

        _firstCKAccountFetch = false;
        _firstSOSCircleFetch = false;

        _finishedInitialDispatches = [[CKKSCondition alloc] init];
        _ckdeviceIDInitialized = [[CKKSCondition alloc] init];

        id<CKKSNSNotificationCenter> notificationCenter = [self.nsnotificationCenterClass defaultCenter];
        secinfo("ckksaccount", "Registering with notification center %@", notificationCenter);
        [notificationCenter addObserver:self selector:@selector(notifyCKAccountStatusChange:) name:CKAccountChangedNotification object:NULL];

        __weak __typeof(self) weakSelf = self;

        // If this is a live server, register with notify
        if(!SecCKKSTestsEnabled()) {
            int token = 0;
            notify_register_dispatch(kSOSCCCircleChangedNotification, &token, dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^(int t) {
                [weakSelf notifyCircleChange:nil];
            });
        }

        // Fire off a fetch of the account status. Do not go on our local queue, because notifyCKAccountStatusChange will attempt to go back on it for thread-safety.
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_UTILITY, 0), ^{
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                return;
            }
            @autoreleasepool {
                [strongSelf notifyCKAccountStatusChange:nil];
                [strongSelf notifyCircleChange:nil];
                [strongSelf.finishedInitialDispatches fulfill];
            }
        });
    }
    return self;
}

-(void)dealloc {
    id<CKKSNSNotificationCenter> notificationCenter = [self.nsnotificationCenterClass defaultCenter];
    [notificationCenter removeObserver:self];
}

-(NSString*)descriptionInternal: (NSString*) selfString {
    return [NSString stringWithFormat:@"<%@: %@ (%@ %@) %@>",
              selfString,
              [self currentStatus],
              self.currentCKAccountInfo,
              SOSCCGetStatusDescription(self.currentCircleStatus),
            self.currentAccountError ?: @""];
}

-(NSString*)description {
    return [self descriptionInternal: [[self class] description]];
}

-(NSString*)debugDescription {
    return [self descriptionInternal: [super description]];
}

-(dispatch_semaphore_t)notifyOnAccountStatusChange:(id<CKKSAccountStateListener>)listener {
    // signals when we've successfully delivered the first account status
    dispatch_semaphore_t finishedSema = dispatch_semaphore_create(0);

    dispatch_async(self.queue, ^{
        bool alreadyRegisteredListener = false;
        NSEnumerator *enumerator = [self.changeListeners objectEnumerator];
        id<CKKSAccountStateListener> value;

        while ((value = [enumerator nextObject])) {
            // do pointer comparison
            alreadyRegisteredListener |= (value == listener);
        }

        if(listener && !alreadyRegisteredListener) {
            NSString* queueName = [NSString stringWithFormat: @"ck-account-state-%@", listener];

            dispatch_queue_t objQueue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL);
            [self.changeListeners setObject: listener forKey: objQueue];

            secinfo("ckksaccount", "adding a new listener: %@", listener);

            // If we know the current account status, let this listener know
            if(self.currentComputedAccountStatus != CKKSAccountStatusUnknown) {
                secinfo("ckksaccount", "notifying new listener %@ of current state %d", listener, (int)self.currentComputedAccountStatus);

                dispatch_group_t g = dispatch_group_create();
                if(!g) {
                    secnotice("ckksaccount", "Unable to get dispatch group.");
                    return;
                }

                [self _onqueueDeliverCurrentState:listener listenerQueue:objQueue oldStatus:CKKSAccountStatusUnknown group:g];

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

    [self.container accountInfoWithCompletionHandler:^(CKAccountInfo* ckAccountInfo, NSError * _Nullable error) {
        if(error) {
            secerror("ckksaccount: error getting account info: %@", error);
            dispatch_semaphore_signal(finishedSema);
            return;
        }

        dispatch_sync(self.queue, ^{
            self.firstCKAccountFetch = true;
            [self _onqueueUpdateAccountState:ckAccountInfo circle:self.currentCircleStatus deliveredSemaphore:finishedSema];
        });
    }];

    return finishedSema;
}

-(dispatch_semaphore_t)notifyCircleChange:(__unused id)object {
    dispatch_semaphore_t finishedSema = dispatch_semaphore_create(0);

    SOSCCStatus circleStatus = [CKKSCKAccountStateTracker getCircleStatus];
    dispatch_sync(self.queue, ^{
        self.firstSOSCircleFetch = true;

        [self _onqueueUpdateAccountState:self.currentCKAccountInfo circle:circleStatus deliveredSemaphore:finishedSema];
    });
    return finishedSema;
}

// Takes the new ckAccountInfo we're moving to
-(void)_onqueueUpdateCKDeviceID: (CKAccountInfo*)ckAccountInfo {
    dispatch_assert_queue(self.queue);
    __weak __typeof(self) weakSelf = self;

    // If we're in an account, opportunistically fill in the device id
    if(ckAccountInfo.accountStatus == CKAccountStatusAvailable) {
        [self.container fetchCurrentDeviceIDWithCompletionHandler:^(NSString* deviceID, NSError* ckerror) {
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckksaccount: Received fetchCurrentDeviceIDWithCompletionHandler callback with null AccountStateTracker");
                return;
            }

            // Make sure you synchronize here; if we've logged out before the callback returns, don't record the result
            dispatch_async(strongSelf.queue, ^{
                __strong __typeof(self) innerStrongSelf = weakSelf;
                if(innerStrongSelf.currentCKAccountInfo.accountStatus == CKAccountStatusAvailable) {
                    secnotice("ckksaccount", "CloudKit deviceID is: %@ %@", deviceID, ckerror);

                    innerStrongSelf.ckdeviceID = deviceID;
                    innerStrongSelf.ckdeviceIDError = ckerror;
                    [innerStrongSelf.ckdeviceIDInitialized fulfill];
                } else {
                    // Logged out! No ckdeviceid.
                    secerror("ckksaccount: Logged back out but still received a fetchCurrentDeviceIDWithCompletionHandler callback");

                    innerStrongSelf.ckdeviceID = nil;
                    innerStrongSelf.ckdeviceIDError = nil;
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

// Pulled out for mocking purposes
+(void)fetchCirclePeerID:(void (^)(NSString* _Nullable peerID, NSError* _Nullable error))callback {
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        CFErrorRef cferror = nil;
        SOSPeerInfoRef egoPeerInfo = SOSCCCopyMyPeerInfo(&cferror);
        NSString* egoPeerID = egoPeerInfo ? (NSString*)CFBridgingRelease(CFRetainSafe(SOSPeerInfoGetPeerID(egoPeerInfo))) : nil;
        CFReleaseNull(egoPeerInfo);

        callback(egoPeerID, CFBridgingRelease(cferror));
    });
}

// Takes the new ckAccountInfo we're moving to
-(void)_onqueueUpdateCirclePeerID: (SOSCCStatus)sosccstatus {
    dispatch_assert_queue(self.queue);
    __weak __typeof(self) weakSelf = self;

    // If we're in a circle, fetch the peer id
    if(sosccstatus == kSOSCCInCircle) {
        [CKKSCKAccountStateTracker fetchCirclePeerID:^(NSString* peerID, NSError* error) {
            __strong __typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                secerror("ckksaccount: Received fetchCirclePeerID callback with null AccountStateTracker");
                return;
            }

            dispatch_async(strongSelf.queue, ^{
                __strong __typeof(self) innerstrongSelf = weakSelf;

                if(innerstrongSelf.currentCircleStatus == kSOSCCInCircle) {
                    secnotice("ckksaccount", "Circle peerID is: %@ %@", peerID, error);
                    // Still in circle. Proceed.
                    innerstrongSelf.accountCirclePeerID = peerID;
                    innerstrongSelf.accountCirclePeerIDError = error;
                    [innerstrongSelf.accountCirclePeerIDInitialized fulfill];
                } else {
                    secerror("ckksaccount: Out of circle but still received a fetchCirclePeerID callback");
                    // Not in-circle. Throw away circle id.
                    strongSelf.accountCirclePeerID = nil;
                    strongSelf.accountCirclePeerIDError = nil;
                    // Don't touch the accountCirclePeerIDInitialized object; it should have been reset when the logout happened.
                }
            });
        }];
    } else {
        // Not in-circle, reset circle ID
        secnotice("ckksaccount", "out of circle(%d): resetting peer ID", sosccstatus);
        self.accountCirclePeerID = nil;
        self.accountCirclePeerIDError = nil;
        self.accountCirclePeerIDInitialized = [[CKKSCondition alloc] init];
    }
}

- (bool)_onqueueDetermineLoggedIn:(NSError**)error {
    // We are logged in if we are:
    //   in CKAccountStatusAvailable
    //   and supportsDeviceToDeviceEncryption == true
    //   and the iCloud account is not in grey mode
    //   and in circle
    dispatch_assert_queue(self.queue);
    if(self.currentCKAccountInfo) {
        if(self.currentCKAccountInfo.accountStatus != CKAccountStatusAvailable) {
            if(error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNotLoggedIn
                                      description:@"iCloud account is logged out"];
            }
            return false;
        } else if(!self.currentCKAccountInfo.supportsDeviceToDeviceEncryption) {
            if(error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSNotHSA2
                                      description:@"iCloud account is not HSA2"];
            }
            return false;
        } else if(!self.currentCKAccountInfo.hasValidCredentials) {
            if(error) {
                *error = [NSError errorWithDomain:CKKSErrorDomain
                                             code:CKKSiCloudGreyMode
                                      description:@"iCloud account is in grey mode"];
            }
            return false;
        }
    } else {
        if(error) {
            *error = [NSError errorWithDomain:CKKSErrorDomain
                                         code:CKKSNotLoggedIn
                                  description:@"No current iCloud account status"];
        }
        return false;
    }

    if(self.currentCircleStatus != kSOSCCInCircle) {
        if(error) {
            *error = [NSError errorWithDomain:(__bridge NSString*)kSOSErrorDomain
                                         code:kSOSErrorNotInCircle
                                  description:@"Not in circle"];
        }
        return false;
    }

    return true;
}

-(void)_onqueueUpdateAccountState:(CKAccountInfo*)ckAccountInfo circle:(SOSCCStatus)sosccstatus deliveredSemaphore:(dispatch_semaphore_t)finishedSema {
    dispatch_assert_queue(self.queue);

    if([self.currentCKAccountInfo isEqual: ckAccountInfo] && self.currentCircleStatus == sosccstatus) {
        // no-op.
        secinfo("ckksaccount", "received another notification of CK Account State %@ and Circle status %d", ckAccountInfo, (int)sosccstatus);
        dispatch_semaphore_signal(finishedSema);
        return;
    }

    if(![self.currentCKAccountInfo isEqual: ckAccountInfo]) {
        secnotice("ckksaccount", "moving to CK Account info: %@", ckAccountInfo);
        self.currentCKAccountInfo = ckAccountInfo;

        [self _onqueueUpdateCKDeviceID: ckAccountInfo];
    }
    if(self.currentCircleStatus != sosccstatus) {
        secnotice("ckksaccount", "moving to circle status: %@", SOSCCGetStatusDescription(sosccstatus));
        self.currentCircleStatus = sosccstatus;
        if (sosccstatus == kSOSCCInCircle) {
            [[CKKSAnalytics logger] setDateProperty:[NSDate date] forKey:CKKSAnalyticsLastInCircle];
        }
        [self _onqueueUpdateCirclePeerID: sosccstatus];
    }

    if(!self.firstSOSCircleFetch || !self.firstCKAccountFetch) {
        secnotice("ckksaccount", "Haven't received updates from all sources; not passing update along: %@", self);
        dispatch_semaphore_signal(finishedSema);
        return;
    }

    CKKSAccountStatus oldComputedStatus = self.currentComputedAccountStatus;

    NSError* error = nil;
    if([self _onqueueDetermineLoggedIn:&error]) {
        self.currentComputedAccountStatus = CKKSAccountStatusAvailable;
        self.currentAccountError = nil;
    } else {
        self.currentComputedAccountStatus = CKKSAccountStatusNoAccount;
        self.currentAccountError = error;
    }
    [self.currentComputedAccountStatusValid fulfill];

    if(oldComputedStatus == self.currentComputedAccountStatus) {
        secnotice("ckksaccount", "No change in computed account status: %@ (%@ %@)",
                  [self currentStatus],
                  self.currentCKAccountInfo,
                  SOSCCGetStatusDescription(self.currentCircleStatus));
        dispatch_semaphore_signal(finishedSema);
        return;
    }

    secnotice("ckksaccount", "New computed account status: %@ (%@ %@)",
              [self currentStatus],
              self.currentCKAccountInfo,
              SOSCCGetStatusDescription(self.currentCircleStatus));

    [self _onqueueDeliverStateChanges:oldComputedStatus deliveredSemaphore:finishedSema];
}

-(void)_onqueueDeliverStateChanges:(CKKSAccountStatus)oldStatus deliveredSemaphore:(dispatch_semaphore_t)finishedSema {
    dispatch_assert_queue(self.queue);

    dispatch_group_t g = dispatch_group_create();
    if(!g) {
        secnotice("ckksaccount", "Unable to get dispatch group.");
        return;
    }

    NSEnumerator *enumerator = [self.changeListeners keyEnumerator];
    dispatch_queue_t dq;

    // Queue up the changes for each listener.
    while ((dq = [enumerator nextObject])) {
        id<CKKSAccountStateListener> listener = [self.changeListeners objectForKey: dq];
        [self _onqueueDeliverCurrentState:listener listenerQueue:dq oldStatus:oldStatus group:g];
    }

    dispatch_group_notify(g, self.queue, ^{
        dispatch_semaphore_signal(finishedSema);
    });
}

-(void)_onqueueDeliverCurrentState:(id<CKKSAccountStateListener>)listener listenerQueue:(dispatch_queue_t)listenerQueue oldStatus:(CKKSAccountStatus)oldStatus group:(dispatch_group_t)g {
    dispatch_assert_queue(self.queue);

    __weak __typeof(listener) weakListener = listener;

    if(listener) {
        dispatch_group_async(g, listenerQueue, ^{
            [weakListener ckAccountStatusChange:oldStatus to:self.currentComputedAccountStatus];
        });
    }
}

-(void)notifyCKAccountStatusChangeAndWaitForSignal {
    dispatch_semaphore_wait([self notifyCKAccountStatusChange: nil], DISPATCH_TIME_FOREVER);
}

-(void)notifyCircleStatusChangeAndWaitForSignal {
    dispatch_semaphore_wait([self notifyCircleChange: nil], DISPATCH_TIME_FOREVER);
}

-(dispatch_group_t)checkForAllDeliveries {

    dispatch_group_t g = dispatch_group_create();
    if(!g) {
        secnotice("ckksaccount", "Unable to get dispatch group.");
        return nil;
    }

    dispatch_sync(self.queue, ^{
        NSEnumerator *enumerator = [self.changeListeners keyEnumerator];
        dispatch_queue_t dq;

        // Queue up the changes for each listener.
        while ((dq = [enumerator nextObject])) {
            id<CKKSAccountStateListener> listener = [self.changeListeners objectForKey: dq];

            secinfo("ckksaccountblock", "Starting blocking for listener %@", listener);
            __weak __typeof(listener) weakListener = listener;
            dispatch_group_async(g, dq, ^{
                __strong __typeof(listener) strongListener = weakListener;
                // Do nothing in particular. It's just important that this block runs.
                secinfo("ckksaccountblock", "Done blocking for listener %@", strongListener);
            });
        }
    });

    return g;
}

// This is its own function to allow OCMock to swoop in and replace the result during testing.
+(SOSCCStatus)getCircleStatus {
    CFErrorRef cferror = NULL;

    SOSCCStatus status = SOSCCThisDeviceIsInCircle(&cferror);
    if(cferror) {
        secerror("ckksaccount: error getting circle status: %@", cferror);
        CFReleaseNull(cferror);
        return kSOSCCError;
    }
    return status;
}

-(NSString*)currentStatus {
    return [CKKSCKAccountStateTracker stringFromAccountStatus:self.currentComputedAccountStatus];
}

+(NSString*)stringFromAccountStatus: (CKKSAccountStatus) status {
    switch(status) {
        case CKKSAccountStatusUnknown: return @"account state unknown";
        case CKKSAccountStatusAvailable: return @"logged in";
        case CKKSAccountStatusNoAccount: return @"no account";
    }
}

@end

#endif // OCTAGON
