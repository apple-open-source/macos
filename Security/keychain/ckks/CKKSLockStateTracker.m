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

#include <notify.h>
#include <dispatch/dispatch.h>
#include <utilities/SecAKSWrappers.h>

#import "keychain/ckks/CKKS.h"
#import "keychain/ckks/CKKSResultOperation.h"
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSLockStateTracker.h"

@interface CKKSLockStateTracker ()
@property (readwrite) bool isLocked;
@property dispatch_queue_t queue;
@property NSOperationQueue* operationQueue;
@property NSHashTable<id<CKKSLockStateNotification>> *observers;

@property (nullable) NSDate* lastUnlockedTime;

@end

@implementation CKKSLockStateTracker

- (instancetype)init {
    if((self = [super init])) {
        _queue = dispatch_queue_create("lock-state-tracker", DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];

        _isLocked = true;
        _observers = [NSHashTable weakObjectsHashTable];
        [self resetUnlockDependency];

        __weak __typeof(self) weakSelf = self;

        // If this is a live server, register with notify
        if(!SecCKKSTestsEnabled()) {
            int token = 0;
            notify_register_dispatch(kUserKeybagStateChangeNotification, &token, _queue, ^(int t) {
                [weakSelf _onqueueRecheck];
            });
        }

        dispatch_async(_queue, ^{
            [weakSelf _onqueueRecheck];
        });
    }
    return self;
}

- (NSDate*)lastUnlockTime {
    // If unlocked, the last unlock time is now. Otherwise, used the cached value.
    __block NSDate* date = nil;
    dispatch_sync(self.queue, ^{
        if(self.isLocked) {
            date = self.lastUnlockedTime;
        } else {
            date = [NSDate date];
            self.lastUnlockedTime = date;
        }
    });
    return date;
}

-(NSString*)description {
    return [NSString stringWithFormat: @"<CKKSLockStateTracker: %@ last:%@>",
            self.isLocked ? @"locked" : @"unlocked",
            self.isLocked ? self.lastUnlockedTime : @"now"];
}

-(void)resetUnlockDependency {
    if(self.unlockDependency == nil || ![self.unlockDependency isPending]) {
        CKKSResultOperation* op = [CKKSResultOperation named:@"keybag-unlocked-dependency" withBlock: ^{
            secinfo("ckks", "Keybag unlocked");
        }];
        op.descriptionErrorCode = CKKSResultDescriptionPendingUnlock;
        self.unlockDependency = op;
    }
}

+(bool)queryAKSLocked {
    CFErrorRef aksError = NULL;
    bool locked = true;

    if(!SecAKSGetIsLocked(&locked, &aksError)) {
        secerror("ckks: error querying lock state: %@", aksError);
        CFReleaseNull(aksError);
    }

    return locked;
}

-(void)_onqueueRecheck {
    dispatch_assert_queue(self.queue);

    static bool first = true;
    bool wasLocked = self.isLocked;
    self.isLocked = [CKKSLockStateTracker queryAKSLocked];

    if(wasLocked != self.isLocked || first) {
        first = false;
        if(self.isLocked) {
            // We're locked now.
            [self resetUnlockDependency];

            if(wasLocked) {
                self.lastUnlockedTime = [NSDate date];
            }
        } else {
            [self.operationQueue addOperation: self.unlockDependency];
            self.unlockDependency = nil;
            self.lastUnlockedTime = [NSDate date];
        }

        bool isUnlocked = (self.isLocked == false);
        for (id<CKKSLockStateNotification> observer in _observers) {
            __strong typeof(observer) strongObserver = observer;
            if (strongObserver == NULL)
                return;
            dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
                [strongObserver lockStateChangeNotification:isUnlocked];
            });
        }
    }
}

-(void)recheck {
    dispatch_sync(self.queue, ^{
        [self _onqueueRecheck];
    });
}

-(bool)isLockedError:(NSError *)error {
    return ([error.domain isEqualToString:@"securityd"] || [error.domain isEqualToString:(__bridge NSString*)kSecErrorDomain])
            && error.code == errSecInteractionNotAllowed;
}

-(void)addLockStateObserver:(id<CKKSLockStateNotification>)object
{
    dispatch_async(self.queue, ^{
        [self->_observers addObject:object];
        bool isUnlocked = (self.isLocked == false);
        dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
            [object lockStateChangeNotification:isUnlocked];
        });
    });
}


@end

#endif // OCTAGON
