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
#import "HIDButtonLoggingServiceFilter.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#import <HIDPreferences/HIDPreferencesCAPI.h>
#import <QuartzCore/QuartzCore.h>
#import <CoreMotion/CMSuppressionManager.h>

#define kIOHIDButtonLoggingResetCount       "IOHIDButtonLoggingResetCount"
#define kIOHIDButtonLoggingPresses          "IOHIDButtonLoggingPresses"
#define kIOHIDEnableSuppressionUpdates      "IOHIDEnableSuppressionUpdates"

//labels for button types
#define kIOHIDButtonLoggingVolumeUp             0
#define kIOHIDButtonLoggingVolumeDown           1
#define kIOHIDButtonLoggingPower                2
#define kIOHIDButtonLoggingStaccato             3
#define kIOHIDButtonLoggingTostada              4
#define kIOHIDButtonLoggingRinger               5
#define kIOHIDButtonLoggingTostadaSuppressed    6
#define kIOHIDButtonLoggingCount                7

//indexes for usage table
#define usageTableUsageIndex 0
#define usageTablePageIndex 1
#define usageTableLabelIndex 2

#define HIDButtonLog(fmt, ...) os_log(_IOHIDButtonLog(), fmt "\n", ##__VA_ARGS__)

static mach_timebase_info_data_t sTimebaseInfo;

os_log_t _IOHIDButtonLog(void)
{
    static os_log_t log = NULL;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        log = os_log_create("com.apple.iohid.internal", "buttonlogging");
    });
    return log;
}

@implementation HIDButtonLoggingServiceFilter {
    uint64_t _buttonCounts[kIOHIDButtonLoggingCount];
    uint64_t _buttonLongPressCounts[kIOHIDButtonLoggingCount]; // => 500ms
    uint64_t _startTime;
    uint64_t _elapsedTime;
    HIDBlock _cancelHandler;
    bool _keyState;
    NSString *_buttonNames[kIOHIDButtonLoggingCount];
    NSString *_buttonLongPressNames[kIOHIDButtonLoggingCount];
    NSArray *_usageTable;
    CMSuppressionManager *_suppressionManager;
    CMSuppressionEventHandler _suppressionEventHandler;
    HIDSuppressionStateType _suppressionState;
    bool _suppressionUpdatesEnabled;
    dispatch_queue_t _suppressionDispatchQueue;
    NSOperationQueue * _suppressionQueue;
    CMSuppressionSource _suppressionSource;
}

- (BOOL)isTostadaSupported:(HIDEventService *)service
{
    //AppleM68Buttons property indicates all usage pairs associated with buttons on device
    //Use this to determine if tostada button is present
    id usagePairs = [service propertyForKey:@"ButtonUsagePairs"];
    if(usagePairs && [usagePairs isKindOfClass:[NSArray class]]) {
        NSNumber *tostadaUsagePair = [NSNumber numberWithUnsignedLongLong:(((UInt64)kHIDPage_CameraControl << 32) | kHIDUsage_CC_Shutter)];
        if([usagePairs containsObject:tostadaUsagePair]) {
            return true;
        }
    }
    return false;
}

- (void)setUpSuppressionManager
{
    bool isAvailable = [CMSuppressionManager isAvailable];
    if (!isAvailable) {
        HIDButtonLog("CMSuppressionManager not available, Setting suppression state to: kHIDSuppressionStateTypeUnknown");
        _suppressionState = kHIDSuppressionStateTypeUnknown;
        _suppressionUpdatesEnabled = false;
        _suppressionManager = nil;
    } else {
        bool isAlwaysOnVOAvailable = [CMSuppressionManager isSourceAvailable:kCMSuppressionSourceAlwaysOnViewObstructed];
        if (isAlwaysOnVOAvailable) {
            //Preferable source type in Springboard's suppression manager
            _suppressionSource = kCMSuppressionSourceAlwaysOnViewObstructed;
            HIDButtonLog("Enabling View Obstructed updates using source: kCMSuppressionSourceAlwaysOnViewObstructed");
        } else {
            _suppressionSource = kCMSuppressionSourceViewObstructed;
            HIDButtonLog("Enabling View Obstructed updates using source: kCMSuppressionSourceViewObstructed.");
        }
        _suppressionQueue = [[NSOperationQueue alloc] init];
        _suppressionDispatchQueue = dispatch_queue_create("com.apple.HID.suppressionManager", DISPATCH_QUEUE_SERIAL);
        _suppressionQueue.underlyingQueue = _suppressionDispatchQueue;
        [_suppressionQueue setMaxConcurrentOperationCount:1];
        //We use the same client type as Springboard in order to get this info prior to camera launch
        _suppressionManager = [[CMSuppressionManager alloc] initWithClientType:CMSuppressionClientTypeCameraPreLaunch];
        HIDButtonLog("Starting suppression manager: %@", _suppressionManager);
        //Create suppression event handler; will be reused if we disable and re-enable suppression updates
        __weak typeof(self) weakSelf = self;
        _suppressionEventHandler = ^(CMSuppressionEvent *event, NSError *error) {
            __strong typeof(self) strongSelf = weakSelf;
            if(!strongSelf) {
                return;
            }
            if (error != nil || event == nil) {
                self->_suppressionState = kHIDSuppressionStateTypeUnknown;
            } else {
                HIDButtonLog("(%{public}@) suppression event: %{public}@", strongSelf, event);
                switch ( [event type] ) {
                    case kCMSuppressionEventTypeSuppress:
                        strongSelf->_suppressionState = kHIDSuppressionStateTypeSuppressed;
                        break;
                    case kCMSuppressionEventTypeUnsuppress:
                        strongSelf->_suppressionState = kHIDSuppressionStateTypeUnsuppressed;
                        break;
                    case kCMSuppressionEventTypeUnknown:
                        strongSelf->_suppressionState = kHIDSuppressionStateTypeUnknown;
                        break;
                    default:
                        break;
                }
            }
        };
        [_suppressionManager startSuppressionUpdatesToQueue:_suppressionQueue withOptions:_suppressionSource withHandler:_suppressionEventHandler];
        _suppressionUpdatesEnabled = true;
    }
}

- (instancetype)initWithService:(HIDEventService *)service
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    //initialize button names arrays
    _buttonNames[kIOHIDButtonLoggingVolumeUp] = @"IOHIDButtonLoggingVolumeUpPress";
    _buttonLongPressNames[kIOHIDButtonLoggingVolumeUp] = @"IOHIDButtonLoggingVolumeUpLongPress";
    _buttonNames[kIOHIDButtonLoggingVolumeDown] = @"IOHIDButtonLoggingVolumeDownPress";
    _buttonLongPressNames[kIOHIDButtonLoggingVolumeDown] = @"IOHIDButtonLoggingVolumeDownLongPress";
    _buttonNames[kIOHIDButtonLoggingPower] = @"IOHIDButtonLoggingPowerPress";
    _buttonLongPressNames[kIOHIDButtonLoggingPower] = @"IOHIDButtonLoggingPowerLongPress";
    _buttonNames[kIOHIDButtonLoggingStaccato] = @"IOHIDButtonLoggingStaccatoPress";
    _buttonLongPressNames[kIOHIDButtonLoggingStaccato] = @"IOHIDButtonLoggingStaccatoLongPress";
    _buttonNames[kIOHIDButtonLoggingTostada] = @"IOHIDButtonLoggingTostadaPress";
    _buttonLongPressNames[kIOHIDButtonLoggingTostada] = @"IOHIDButtonLoggingTostadaLongPress";
    _buttonNames[kIOHIDButtonLoggingRinger] = @"IOHIDButtonLoggingRingerPress";
    _buttonLongPressNames[kIOHIDButtonLoggingRinger] = @"IOHIDButtonLoggingRingerLongPress";
    _buttonNames[kIOHIDButtonLoggingTostadaSuppressed] = @"IOHIDButtonLoggingTostadaPressSuppressed";
    _buttonLongPressNames[kIOHIDButtonLoggingTostadaSuppressed] = @"IOHIDButtonLoggingTostadaLongPressSuppressed";
    
    //initialize usage table, maps usage/usagePage to label
    _usageTable = @[ @[[NSNumber numberWithInt:kHIDUsage_Csmr_VolumeIncrement], [NSNumber numberWithInt:kHIDPage_Consumer], @kIOHIDButtonLoggingVolumeUp],
                     @[[NSNumber numberWithInt:kHIDUsage_Csmr_VolumeDecrement], [NSNumber numberWithInt:kHIDPage_Consumer], @kIOHIDButtonLoggingVolumeDown],
                     @[[NSNumber numberWithInt:kHIDUsage_Csmr_Power], [NSNumber numberWithInt:kHIDPage_Consumer], @kIOHIDButtonLoggingPower],
                     @[[NSNumber numberWithInt:kHIDUsage_Tfon_RingEnable], [NSNumber numberWithInt:kHIDPage_Telephony], @kIOHIDButtonLoggingStaccato],
                     @[[NSNumber numberWithInt:kHIDUsage_CC_Shutter], [NSNumber numberWithInt:kHIDPage_CameraControl], @kIOHIDButtonLoggingTostada],
                     @[[NSNumber numberWithInt:kHIDUsage_AppleVendorKeyboard_UserDefined1], [NSNumber numberWithInt:kHIDPage_AppleVendorKeyboard], @kIOHIDButtonLoggingRinger]];
    
    _service = service;

    for(NSUInteger i = 0; i < kIOHIDButtonLoggingCount; i++) {
        id prefValue = CFBridgingRelease(HIDPreferencesCopy((__bridge CFStringRef)_buttonNames[i], kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd")));
        
        if (prefValue && [prefValue isKindOfClass:[NSString class]]) {
            _buttonCounts[i] = (uint64_t)((NSString*)prefValue).longLongValue;
        } else {
            HIDButtonLog("Unable to read count %@", prefValue);
        }
        
        prefValue = CFBridgingRelease(HIDPreferencesCopy((__bridge CFStringRef)_buttonLongPressNames[i], kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd")));
        
        if (prefValue && [prefValue isKindOfClass:[NSString class]]) {
            _buttonLongPressCounts[i] = (uint64_t)((NSString*)prefValue).longLongValue;
        } else {
            HIDButtonLog("Unable to read count %@", prefValue);
        }
    }
    
    if([self isTostadaSupported:service]) {
        [self setUpSuppressionManager];
    } else {
        //If we are not on a tostada-enabled device, the suppression state info is not used, so we'll mark it as unknown
        _suppressionState = kHIDSuppressionStateTypeUnknown;
        _suppressionUpdatesEnabled = false;
        _suppressionManager = nil;
    }
    
    return self;
}

- (void)dealloc
{
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"HIDButtonLoggingServiceFilter"];
}

- (id)propertyForKey:(NSString *)key client:(HIDConnection *)client
{
    id result = nil;
    if ([key isEqualToString:@kIOHIDServiceFilterDebugKey]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];
        
        debug[@"FilterName"] = @"HIDButtonLoggingServiceFilter";
        debug[@"VolumeUpButtonPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingVolumeUp]);
        debug[@"VolumeUpButtonLongPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingVolumeUp]);
        debug[@"VolumeDownButtonPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingVolumeDown]);
        debug[@"VolumeDownButtonLongPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingVolumeDown]);
        debug[@"PowerButtonPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingPower]);
        debug[@"PowerButtonLongPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingPower]);
        debug[@"StaccatoButtonPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingStaccato]);
        debug[@"StaccatoButtonLongPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingStaccato]);
        debug[@"TostadaButtonPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingTostada]);
        debug[@"TostadaButtonLongPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingTostada]);
        debug[@"TostadaButtonSuppressedPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingTostadaSuppressed]);
        debug[@"TostadaButtonLongSuppressedPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingTostadaSuppressed]);
        debug[@"RingerButtonPressCount"] = @(_buttonCounts[kIOHIDButtonLoggingRinger]);
        debug[@"RingerButtonLongPressCount"] = @(_buttonLongPressCounts[kIOHIDButtonLoggingRinger]);
        debug[@"Suppression State"] = @(_suppressionState);
        debug[@"Suppression Updates Enabled"] = @(_suppressionUpdatesEnabled);

        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(id)value
             forKey:(NSString *)key
             client:(HIDConnection *)client
{
    bool result = false;
    
    if ([key isEqualToString:@kIOHIDButtonLoggingResetCount] &&
        [value isKindOfClass:[NSNumber class]]) {
        NSNumber * propNum = (NSNumber*)value;
        if (propNum.boolValue) {
            for(NSUInteger i = 0; i < kIOHIDButtonLoggingCount; i++) {
                _buttonCounts[i] = 0;
                _buttonLongPressCounts[i] = 0;
            }
            [self publishButtonCount];
        }
        result = true;
    } else if ([key isEqualToString:@kIOHIDEnableSuppressionUpdates] &&
               [value isKindOfClass:[NSNumber class]]) {
        NSNumber * propNum = (NSNumber*)value;
        if(propNum.boolValue) {
            //Only call start if updates are currently not enabled and a manager exists; otherwise this should be a noop
            if(!_suppressionUpdatesEnabled && _suppressionManager) {
                HIDButtonLog("Enabling suppression updates for tostada button");
                [_suppressionManager startSuppressionUpdatesToQueue:_suppressionQueue withOptions:_suppressionSource withHandler:_suppressionEventHandler];
                _suppressionUpdatesEnabled = true;
            }
        } else {
            //Only call stop if updates are currently enabled and a manager exists; otherwise this should be a noop
            if(_suppressionUpdatesEnabled && _suppressionManager) {
                HIDButtonLog("Disabling suppression updates for tostada button");
                [_suppressionManager stopSuppressionUpdates];
                _suppressionUpdatesEnabled = false;
            }
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

- (BOOL)updateButtonCounts:(NSInteger)label
                     usage:(NSInteger)usage
                 usagePage:(NSInteger)page
                  keyState:(NSInteger)keyDown
                 timestamp:(uint64_t)timestamp
{
    bool ret = false;
    bool isTostada = (usage == kHIDUsage_CC_Shutter) && (page == kHIDPage_CameraControl);
    if (keyDown != _keyState) {
        if (!_keyState) {
            _startTime = timestamp;
            ++_buttonCounts[label];
            //Increment suppressed counts only if the current state is suppressed and we are actively recieving updates
            if(isTostada && (_suppressionState == kHIDSuppressionStateTypeSuppressed) && _suppressionUpdatesEnabled) {
                HIDButtonLog("Tostada press while in suppressed state");
                ++_buttonCounts[kIOHIDButtonLoggingTostadaSuppressed];
            }
            ret = true;
        } else {
            _elapsedTime = timestamp - _startTime;
            mach_timebase_info(&sTimebaseInfo);
            _elapsedTime *= sTimebaseInfo.numer;
            _elapsedTime /= sTimebaseInfo.denom;
            _elapsedTime /= 1000000; // convert from nanoseconds to milliseconds
            if(_elapsedTime >= 500) {
                ++_buttonLongPressCounts[label];
                if(isTostada && (_suppressionState == kHIDSuppressionStateTypeSuppressed) && _suppressionUpdatesEnabled) {
                    HIDButtonLog("Tostada long press while in suppressed state");
                    ++_buttonLongPressCounts[kIOHIDButtonLoggingTostadaSuppressed];
                }
                ret = true;
            }
        }
        _keyState = !_keyState;
    }
    return ret;
}

- (HIDEvent *)filterEvent:(HIDEvent *)event
{
    if (event.type == kIOHIDEventTypeKeyboard) {
        NSInteger keyDown = [event integerValueForField:kIOHIDEventFieldKeyboardDown];
        NSInteger usage = [event integerValueForField:kIOHIDEventFieldKeyboardUsage];
        NSInteger usagePage = [event integerValueForField:kIOHIDEventFieldKeyboardUsagePage];
        NSInteger builtIn = [event integerValueForField:kIOHIDEventFieldIsBuiltIn];

        for(NSUInteger i = 0; i < _usageTable.count; i++) {
            if(([_usageTable[i][usageTableUsageIndex] integerValue] == usage) && ([_usageTable[i][usageTablePageIndex] integerValue] == usagePage) && builtIn) {
                if([self updateButtonCounts:[_usageTable[i][usageTableLabelIndex] integerValue] usage:usage usagePage:usagePage keyState:keyDown timestamp:[event timestamp]]) {
                    [self publishButtonCount];
                }
                HIDButtonLog("%s State: %s", [_buttonNames[i] UTF8String], keyDown ? "down" : "up");
            }
        }
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
    for(NSUInteger i = 0; i < kIOHIDButtonLoggingCount; i++) {
        
        HIDPreferencesSet((__bridge CFStringRef)_buttonNames[i], (__bridge CFTypeRef _Nullable)([NSString stringWithFormat:@"%llu", _buttonCounts[i]]), kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
        HIDPreferencesSynchronize(kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
        CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), CFSTR(kIOHIDButtonLoggingPresses), NULL, NULL, TRUE);
        
        HIDPreferencesSet((__bridge CFStringRef)_buttonLongPressNames[i], (__bridge CFTypeRef _Nullable)([NSString stringWithFormat:@"%llu", _buttonLongPressCounts[i]]), kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
        HIDPreferencesSynchronize(kCFPreferencesCurrentUser, kCFPreferencesAnyHost, CFSTR("com.apple.backboardd"));
        CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), CFSTR(kIOHIDButtonLoggingPresses), NULL, NULL, TRUE);
    }
}

@end
