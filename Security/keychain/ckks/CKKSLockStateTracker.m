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
#import "keychain/ckks/CKKSGroupOperation.h"
#import "keychain/ckks/CKKSLockStateTracker.h"

@interface CKKSLockStateTracker ()
@property bool isLocked;
@property dispatch_queue_t queue;
@property NSOperationQueue* operationQueue;
@end

@implementation CKKSLockStateTracker

- (instancetype)init {
    if((self = [super init])) {
        _queue = dispatch_queue_create("lock-state-tracker", DISPATCH_QUEUE_SERIAL);
        _operationQueue = [[NSOperationQueue alloc] init];

        _isLocked = true;
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

-(NSString*)description {
    return [NSString stringWithFormat: @"<CKKSLockStateTracker: %@>", self.isLocked ? @"locked" : @"unlocked"];
}

-(void)resetUnlockDependency {
    if(self.unlockDependency == nil || ![self.unlockDependency isPending]) {
        self.unlockDependency = [NSBlockOperation blockOperationWithBlock: ^{
            secinfo("ckks", "Keybag unlocked");
        }];
        self.unlockDependency.name = @"keybag-unlocked-dependency";
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

    bool wasLocked = self.isLocked;
    self.isLocked = [CKKSLockStateTracker queryAKSLocked];

    if(wasLocked != self.isLocked) {
        if(self.isLocked) {
            // We're locked now.
            [self resetUnlockDependency];
        } else {
            [self.operationQueue addOperation: self.unlockDependency];
            self.unlockDependency = nil;
        }
    }
}

-(void)recheck {
    dispatch_sync(self.queue, ^{
        [self _onqueueRecheck];
    });
}

-(bool)isLockedError:(NSError *)error {
    return [error.domain isEqualToString:@"securityd"] && error.code == errSecInteractionNotAllowed;
}


@end

#endif // OCTAGON
