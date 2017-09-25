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
#include <notify.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSCKAccountStateTracker.h"


@interface CKKSCKAccountStateTracker ()
@property (readonly) Class<CKKSNSNotificationCenter> nsnotificationCenterClass;

@property CKKSAccountStatus currentComputedAccountStatus;

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

        _container = container;

        _queue = dispatch_queue_create("ck-account-state", DISPATCH_QUEUE_SERIAL);

        _firstCKAccountFetch = false;
        _firstSOSCircleFetch = false;

        _finishedInitialCalls = [[CKKSCondition alloc] init];
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
            [strongSelf notifyCKAccountStatusChange:nil];
            [strongSelf notifyCircleChange:nil];
            [strongSelf.finishedInitialCalls fulfill];
        });
    }
    return self;
}

-(void)dealloc {
    id<CKKSNSNotificationCenter> notificationCenter = [self.nsnotificationCenterClass defaultCenter];
    [notificationCenter removeObserver:self];
}

-(NSString*)descriptionInternal: (NSString*) selfString {
    return [NSString stringWithFormat:@"<%@: %@ (%@ %@)",
              selfString,
              [self currentStatus],
              self.currentCKAccountInfo,
              SOSCCGetStatusDescription(self.currentCircleStatus)];
}

-(NSString*)description {
    return [self descriptionInternal: [[self class] description]];
}

-(NSString*)debugDescription {
    return [self descriptionInternal: [super description]];
}

-(CKKSAccountStatus)currentCKAccountStatusAndNotifyOnChange: (id<CKKSAccountStateListener>) listener {

    __block CKKSAccountStatus status = CKKSAccountStatusUnknown;

    dispatch_sync(self.queue, ^{
        status = self.currentComputedAccountStatus;

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
        }
    });
    return status;
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
    if(circleStatus == kSOSCCError) {
        dispatch_semaphore_signal(finishedSema);
        return finishedSema;
    }

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
        self.accountCirclePeerID = nil;
        self.accountCirclePeerIDError = nil;
        self.accountCirclePeerIDInitialized = [[CKKSCondition alloc] init];
    }
}

-(void)_onqueueUpdateAccountState: (CKAccountInfo*) ckAccountInfo circle: (SOSCCStatus) sosccstatus deliveredSemaphore: (dispatch_semaphore_t) finishedSema {
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

        [self _onqueueUpdateCirclePeerID: sosccstatus];
    }

    if(!self.firstSOSCircleFetch || !self.firstCKAccountFetch) {
        secnotice("ckksaccount", "Haven't received updates from all sources; not passing update along: %@", self);
        dispatch_semaphore_signal(finishedSema);
        return;
    }

    // We are CKKSAccountStatusAvailable if we are:
    //   in CKAccountStatusAvailable
    //   and in circle
    //   and supportsDeviceToDeviceEncryption == true
    CKKSAccountStatus oldComputedStatus = self.currentComputedAccountStatus;

    if(self.currentCKAccountInfo) {
        if(self.currentCKAccountInfo.accountStatus == CKAccountStatusAvailable) {
            // CloudKit thinks we're logged in. Double check!
            if(self.currentCKAccountInfo.supportsDeviceToDeviceEncryption && self.currentCircleStatus == kSOSCCInCircle) {
                self.currentComputedAccountStatus = CKKSAccountStatusAvailable;
            } else {
                self.currentComputedAccountStatus = CKKSAccountStatusNoAccount;
            }

        } else {
            // Account status is not CKAccountStatusAvailable; no more checking required.
            self.currentComputedAccountStatus = CKKSAccountStatusNoAccount;
        }
    } else {
        // No CKAccountInfo? We haven't received an update from cloudd yet; Change nothing.
    }

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
        __weak __typeof(listener) weakListener = listener;

        if(listener) {
            dispatch_group_async(g, dq, ^{
                [weakListener ckAccountStatusChange: oldComputedStatus to: self.currentComputedAccountStatus];
            });
        }
    }

    dispatch_group_notify(g, self.queue, ^{
        dispatch_semaphore_signal(finishedSema);
    });
}

-(void)notifyCKAccountStatusChangeAndWaitForSignal {
    dispatch_semaphore_wait([self notifyCKAccountStatusChange: nil], DISPATCH_TIME_FOREVER);
}

-(void)notifyCircleStatusChangeAndWaitForSignal {
    dispatch_semaphore_wait([self notifyCircleChange: nil], DISPATCH_TIME_FOREVER);
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
