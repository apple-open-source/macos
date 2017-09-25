//
//  CKDAKSLockMonitor.m
//  Security
//
//  Created by Mitch Adler on 11/2/16.
//
//

#import <Foundation/Foundation.h>

#import "CKDAKSLockMonitor.h"

#include <utilities/SecCFRelease.h>
#include <utilities/SecAKSWrappers.h>
#include <utilities/debugging.h>

#include <notify.h>

@interface CKDAKSLockMonitor ()

@property XPCNotificationDispatcher* dispatcher;
@property XPCNotificationBlock notificationBlock;
@property dispatch_queue_t queue;

@end

@implementation CKDAKSLockMonitor

+ (instancetype) monitor {
    return [[CKDAKSLockMonitor alloc] init];
}

- (instancetype)init {
    self = [super init];

    if (self) {
        XPCNotificationDispatcher* dispatcher = [XPCNotificationDispatcher dispatcher];

        _queue = dispatch_queue_create("CKDAKSLockMonitor", NULL);
        _locked = true;
        _unlockedSinceBoot = false;

        /* also use dispatch to make sure */
        int token = 0;
        __weak typeof(self) weakSelf = self;
        notify_register_dispatch(kUserKeybagStateChangeNotification, &token, _queue, ^(int t) {
            [weakSelf _onqueueRecheck];
        });

        [self recheck];

        [dispatcher addListener: self];
    }

    return self;
}

- (void) handleNotification:(const char *)name {
    if (strcmp(name, kUserKeybagStateChangeNotification) == 0 || strcmp(name, "com.apple.mobile.keybagd.lock_status") == 0) {
        [self recheck];
    }
}

- (void) notifyListener {
    // Take a strong reference:
    __strong __typeof(self.listener) listener = self.listener;

    if (listener) {
        if (self.locked) {
            [listener locked];
        } else {
            [listener unlocked];
        }
    }
}

- (void)connectTo: (NSObject<CKDLockListener>*) listener {
    _listener = listener;
    [self notifyListener];
}

- (void) recheck {
    dispatch_async(_queue, ^{
        [self _onqueueRecheck];
    });
}

- (void) _onqueueRecheck {
    CFErrorRef aksError = NULL;
    bool locked = true; // Assume locked if we get an error

    if (!SecAKSGetIsLocked(&locked, &aksError)) {
        secerror("%@ Got error querying lock state: %@", self, aksError);
        CFReleaseSafe(aksError);
    }

    BOOL previousLocked = self.locked;
    _locked = locked;

    if (!self.locked) {
        _unlockedSinceBoot = true;
    }

    if (previousLocked != self.locked) {
        // recheck might get called from ckdkvsproxy_queue (see 30510390)
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [self notifyListener];
        });
    }
}

@end
