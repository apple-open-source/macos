/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#import <Foundation/Foundation.h>
#import "HIDStaccatoTestButtonServiceFilter.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#import <HIDPreferences/HIDPreferencesCAPI.h>
#import <QuartzCore/QuartzCore.h>

#define kIOHIDTestButtonResetCount   "HIDTestButtonResetCount"
#define kIOHIDLoggingTestButtonPresses   "IOHIDTestButtonPresses"
#define kIOHIDLoggingStaccatoTestButtonPresses   "IOHIDStaccatoTestButtonPresses"
#define kIOHIDLoggingStaccatoTestButtonLongPresses   "IOHIDStaccatoTestButtonLongPresses"

#define HIDTestButtonLog(fmt, ...) os_log(_IOHIDTestButtonLog(), fmt "\n", ##__VA_ARGS__)

os_log_t _IOHIDTestButtonLog(void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.iohid.internal", "staccato");
    });
    return log;
}

@implementation HIDStaccatoTestButtonServiceFilter {
    uint64_t _buttonCount;
    uint64_t _buttonLongPressCount; // => 500ms
    uint64_t _startTime;
    uint64_t _elapsedTime;
    HIDBlock _cancelHandler;
    bool _keyState;
}

- (instancetype)initWithService:(HIDEventService *)service
{
    self = [super init];
    if (!self) {
        return self;
    }

    _service = service;

    id prefValue = CFBridgingRelease(HIDPreferencesCopy(CFSTR(kIOHIDLoggingStaccatoTestButtonPresses), kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd")));

    if (prefValue && [prefValue isKindOfClass:[NSString class]]) {
        _buttonCount = (uint64_t)((NSString*)prefValue).longLongValue;
        HIDTestButtonLog("Restored staccato button count: %llu", _buttonCount);
    } else {
        HIDTestButtonLog("Unable to read count %@", prefValue);
    }
    
    prefValue = CFBridgingRelease(HIDPreferencesCopy(CFSTR(kIOHIDLoggingStaccatoTestButtonLongPresses), kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd")));

    if (prefValue && [prefValue isKindOfClass:[NSString class]]) {
        _buttonLongPressCount = (uint64_t)((NSString*)prefValue).longLongValue;
        HIDTestButtonLog("Restored staccato button long press count: %llu", _buttonLongPressCount);
    } else {
        HIDTestButtonLog("Unable to read count %@", prefValue);
    }
    
    return self;
}

- (void)dealloc
{
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"HIDStaccatoTestButtonServiceFilter"];
}

- (id)propertyForKey:(NSString *)key client:(HIDConnection *)client
{
    id result = nil;
    if ([key isEqualToString:@kIOHIDServiceFilterDebugKey]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];
        
        debug[@"FilterName"] = @"HIDStaccatoTestButtonServiceFilter";
        debug[@"buttonCount"] = @(_buttonCount);
        debug[@"buttonLongPressCount"] = @(_buttonLongPressCount);

        result = debug;
    } else if ([key isEqualToString:@kIOHIDLoggingStaccatoTestButtonPresses]) {
        result = @(_buttonCount);
    } else if ([key isEqualToString:@kIOHIDLoggingStaccatoTestButtonLongPresses]) {
        result = @(_buttonLongPressCount);
    }
    
    return result;
}

- (BOOL)setProperty:(id)value
             forKey:(NSString *)key
             client:(HIDConnection *)client
{
    bool result = false;
    
    if ([key isEqualToString:@kIOHIDTestButtonResetCount] &&
        [value isKindOfClass:[NSNumber class]]) {
        NSNumber * propNum = (NSNumber*)value;
        if (propNum.boolValue) {
            _buttonCount = 0;
            _buttonLongPressCount = 0;
            [self publishButtonCount];
        }
        result = true;
    }
    
    return result;
}

+ (BOOL)matchService:(HIDEventService *)service
             options:(NSDictionary *)options
               score:(NSInteger *)score
{
    *score = 550;
    
    return true;
}

- (HIDEvent *)filterEvent:(HIDEvent *)event
{
    HIDTestButtonLog("Got Event %d", event.type);
    if (event.type == kIOHIDEventTypeKeyboard) {
        NSInteger keyDown = [event integerValueForField:kIOHIDEventFieldKeyboardDown];
        NSInteger usage = [event integerValueForField:kIOHIDEventFieldKeyboardUsage];
        NSInteger usagePage = [event integerValueForField:kIOHIDEventFieldKeyboardUsagePage];
        NSInteger builtIn = [event integerValueForField:kIOHIDEventFieldIsBuiltIn];

        if((usage == kHIDUsage_Tfon_RingEnable) && (usagePage == kHIDPage_Telephony) && builtIn) { //Usage matches staccato button
            if (keyDown != _keyState) {
                if (!_keyState) {
                    //_startTime = CACurrentMediaTime();
                    _startTime = [event timestamp];
                    ++_buttonCount;
                    [self publishButtonCount];
                } else {
                    _elapsedTime = [event timestamp] - _startTime;
                    mach_timebase_info_data_t sTimebaseInfo;
                    mach_timebase_info(&sTimebaseInfo);
                    _elapsedTime *= sTimebaseInfo.numer;
                    _elapsedTime /= sTimebaseInfo.denom;
                    _elapsedTime /= 1000000; // convert from nanoseconds to milliseconds
                    if(_elapsedTime >= 500) {
                        ++_buttonLongPressCount;
                        [self publishButtonCount];
                    }
                }
                _keyState = !_keyState;
            }
        }

        HIDTestButtonLog("Test Button State: %s", keyDown ? "down" : "up");
    }
    
    return event;
}

- (HIDEvent *)filterEventMatching:(NSDictionary *)matching
                            event:(HIDEvent *)event
                        forClient:(HIDConnection *)client
{
    return event;
}

- (void)setCancelHandler:(HIDBlock)handler
{
    _cancelHandler = handler;
}

- (void)activate
{

}

- (void)cancel
{
    _cancelHandler();
    _cancelHandler = nil;
}

- (void)publishButtonCount
{

    HIDPreferencesSet(CFSTR(kIOHIDLoggingStaccatoTestButtonPresses), (__bridge CFTypeRef _Nullable)([NSString stringWithFormat:@"%llu", _buttonCount]), kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
    HIDPreferencesSynchronize(kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), CFSTR(kIOHIDLoggingTestButtonPresses), NULL, NULL, TRUE);
    HIDPreferencesSet(CFSTR(kIOHIDLoggingStaccatoTestButtonLongPresses), (__bridge CFTypeRef _Nullable)([NSString stringWithFormat:@"%llu", _buttonLongPressCount]), kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
    HIDPreferencesSynchronize(kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), CFSTR(kIOHIDLoggingTestButtonPresses), NULL, NULL, TRUE);
}

@end
