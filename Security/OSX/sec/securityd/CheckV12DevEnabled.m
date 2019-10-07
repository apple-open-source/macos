//
//  CheckV12DevEnabled.m
//  Security_ios
//
//  Created by Wouter de Groot on 2018-10-20.
//

// TODO: remove me when keychain backup is enabled for all

#include "CheckV12DevEnabled.h"

#if __OBJC2__

#import <Foundation/Foundation.h>
#import <dispatch/dispatch.h>

static int realCheckV12DevEnabled() {
    static bool enabled = NO;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSUserDefaults* defaults = [[NSUserDefaults alloc] initWithSuiteName:@"com.apple.security"];
        enabled = [defaults boolForKey:@"enableKeychainBackupDevelopment"];
    });
    return enabled ? 1 : 0;
}

int (*checkV12DevEnabled)(void) = realCheckV12DevEnabled;

void resetCheckV12DevEnabled(void) {
    checkV12DevEnabled = realCheckV12DevEnabled;
}

#endif
