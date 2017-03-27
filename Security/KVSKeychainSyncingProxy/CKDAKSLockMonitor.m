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


@interface CKDAKSLockMonitor ()

@property XPCNotificationDispatcher* dispatcher;
@property XPCNotificationBlock notificationBlock;

@end

@implementation CKDAKSLockMonitor

+ (instancetype) monitor {
    return [[CKDAKSLockMonitor alloc] init];
}

- (instancetype)init {
    self = [super init];

    if (self) {
        XPCNotificationDispatcher* dispatcher = [XPCNotificationDispatcher dispatcher];

        [dispatcher addListener: self];

        self->_locked = true;
        self->_unlockedSinceBoot = false;

        [self recheck];
    }

    return self;
}

- (void) handleNotification:(const char *)name {
    if (strcmp(name, kUserKeybagStateChangeNotification) == 0) {
        [self recheck];
    }
}

- (void) notifyListener {
    if (self.listener) {
        if (self.locked) {
            [self.listener locked];
        } else {
            [self.listener unlocked];
        }
    }
}

- (void)connectTo: (NSObject<CKDLockListener>*) listener {
    self->_listener = listener;
    [self notifyListener];
}

- (void) recheck {
    CFErrorRef aksError = NULL;
    bool locked = true; // Assume locked if we get an error

    if (!SecAKSGetIsLocked(&locked, &aksError)) {
        secerror("%@ Got error querying lock state: %@", self, aksError);
        CFReleaseSafe(aksError);
    }

    BOOL previousLocked = self.locked;
    self->_locked = locked;

    if (!self.locked) {
        self->_unlockedSinceBoot = true;
    }

    if (previousLocked != self.locked) {
        // recheck might get called from ckdkvsproxy_queue (see 30510390)
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [self notifyListener];
        });
    }
}

@end
