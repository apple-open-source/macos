//
//  IOHIDMotionEventSessionFilter.m
//  IOHIDMotionEventSessionFilter
//
//  Created by Paul Doerr on 2/20/2020.
//

#import <Foundation/Foundation.h>
#import <LocalAuthentication/LocalAuthentication_Private.h>
#import <AssertMacros.h>

#import <HID/HID_Private.h>
#import <HID/HIDEventAccessors_Private.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDEventSystemConnection.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/AppleHIDUsageTables.h>
#import "IOHIDDebug.h"


typedef struct {
    NSInteger usagePage;
    NSInteger usage;
} UsagePair;

static const UsagePair usagePairs[] = {
    { kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Gyro },
    { kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Accelerometer },
    { kHIDPage_AppleVendor, kHIDUsage_AppleVendor_Compass },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_Motion },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion6 },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion10 },
    { kHIDPage_AppleVendorMotion, kHIDUsage_AppleVendorMotion_DeviceMotion3 }
};


@interface IOHIDMotionEventSessionFilter : NSObject <HIDSessionFilter>

@property (nonatomic, readonly) HIDSession *        session;
@property (nonatomic, nullable) dispatch_queue_t    queue;
@property (nonatomic)           BOOL                LAUIActive;

- (nullable instancetype)initWithSession:(HIDSession *)session;

- (nullable id)propertyForKey:(NSString *)key;

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service;

- (void)activate;

- (void)setDispatchQueue:(dispatch_queue_t)queue;

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                      toConnection:(HIDConnection *)connection
                       fromService:(HIDEventService *)service;

@end


@implementation IOHIDMotionEventSessionFilter {

}

- (nullable instancetype)initWithSession:(HIDSession *)session
{
    self = [super init];
    if (!self) {
        return self;
    }

    _session = session;
    _queue = nil;
    _LAUIActive = NO;
    
    return self;
}

- (void)dealloc
{

}

- (nullable id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        NSMutableDictionary * debug = [NSMutableDictionary new];
        debug[@"Class"] = @"IOHIDMotionEventSessionFilter";
        debug[@"LAUIActive"] = @(self.LAUIActive);
        
        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(nullable id __unused)value
             forKey:(NSString * __unused)key
{
    return NO;
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService * __unused)service
{
    return event;
}

static void receiveLAUINotification(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    IOHIDMotionEventSessionFilter * self = (__bridge IOHIDMotionEventSessionFilter *)observer;

    os_log(_HIDLogCategory(kHIDLogCategoryDefault), "MotionEventSessionFilter CFNotification: %@", (__bridge NSString *)name);

    if ( CFEqual(name, (__bridge CFStringRef)LADarwinNotificationUIPresented) ) {
        self.LAUIActive = YES;
        [self.session setProperty:@(YES) forKey:@kIOHIDMotionEventRestrictedKey];
    }
    else if ( CFEqual(name, (__bridge CFStringRef)LADarwinNotificationUIDismissed) ) {
        self.LAUIActive = NO;
        [self.session setProperty:@(NO) forKey:@kIOHIDMotionEventRestrictedKey];
    }
}

- (void)activate
{
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge const void *)self, receiveLAUINotification, (CFStringRef)LADarwinNotificationUIPresented, NULL, 0);
    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), (__bridge const void *)self, receiveLAUINotification, (CFStringRef)LADarwinNotificationUIDismissed, NULL, 0);

    // Crash protection - reset property so that kernel drivers don't get stuck in restricted state.
    [self.session setProperty:@(NO) forKey:@kIOHIDMotionEventRestrictedKey];
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    self.queue = queue;
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                      toConnection:(HIDConnection *)connection
                       fromService:(HIDEventService *)service
{
    IOHIDEventSystemConnectionEntitlements * entitlements;

    // Filter only when LA UI is active.
    require_quiet(self.LAUIActive, exit);

    // Connections with the motion event entitlement should always receive events.
    entitlements = IOHIDEventSystemConnectionGetEntitlements((IOHIDEventSystemConnectionRef)connection);
    require_quiet(!entitlements->motionEventPrivileged, exit);

    // Filter only built-in services.
    require_quiet([event integerValueForField:kIOHIDEventFieldIsBuiltIn], exit);

    // Filter if it's a motion event.
    switch (event.type) {
        case kIOHIDEventTypeGyro:
        case kIOHIDEventTypeAccelerometer:
        case kIOHIDEventTypeCompass:
            return nil;
        default:
            break;
    }

    // Filter if the event came from a motion service.
    for ( NSUInteger i = 0; i < sizeof(usagePairs); i++) {
        if ([service conformsToUsagePage:usagePairs[i].usagePage usage:usagePairs[i].usage]) {
            return nil;
        }
    }

exit:
    return event;
}

@end

