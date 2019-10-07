//
//  HIDSessionFilterExample.m
//  HIDSessionFilterExample
//
//  Created by dekom on 9/26/18.
//

#import <Foundation/Foundation.h>
#import "HIDSessionFilterExample.h"
#import "IOHIDDebug.h"
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>

@implementation HIDSessionFilterExample {
    NSNumber *_prop;
}

- (nullable instancetype)initWithSession:(HIDSession *)session
{
    self = [super init];
    if (!self) {
        return self;
    }
    
    _session = session;
    
    HIDLog("HIDSessionFilterExample::initWithSession: %@", session);
    return self;
}

- (void)dealloc
{
    HIDLog("HIDSessionFilterExample::dealloc");
}

- (nullable id)propertyForKey:(NSString *)key
{
    id result = nil;
    
    if ([key isEqualToString:@"TestHIDSessionFilterGetProperty"]) {
        result = _prop;
        HIDLog("HIDSessionFilterExample::propertyForKey %@ value: %@", key, result);
    } else if ([key isEqualToString:@(kIOHIDSessionFilterDebugKey)]) {
        // debug dictionary that gets captured by hidutil
        NSMutableDictionary *debug = [NSMutableDictionary new];
        
        debug[@"FilterName"] = @"HIDSessionFilterExample";
        debug[@"aNumber"] = _prop;
        debug[@"anArray"] = @[ @"hello", @"world"];
        
        result = debug;
    }
    
    return result;
}

- (BOOL)setProperty:(nullable id)value
             forKey:(NSString *)key
{
    BOOL result = false;
    
    if ([key isEqualToString:@"TestHIDSessionFilterSetProperty"] &&
        [value isKindOfClass:[NSNumber class]]) {
        _prop = value;
        result = true;
        
        HIDLog("HIDSessionFilterExample::setProperty: %@ forKey: %@", value, key);
    }
    
    return result;
}

- (nullable HIDEvent *)filterEvent:(HIDEvent *)event
                        forService:(HIDEventService *)service
{
    if (service == _keyboard) {
        HIDLog("HIDSessionFilterExample::filterEvent: %@ for service: %@", event, service);
    }
    
    return event;
}

- (void)activate
{
    HIDLog("HIDSessionFilterExample::activate");
}

- (void)serviceNotification:(HIDEventService *)service added:(BOOL)added
{
    if (added) {
        // keep track of internal keyboard
        if ([[service propertyForKey:@(kIOHIDBuiltInKey)] boolValue] &&
            [service conformsToUsagePage:kHIDPage_GenericDesktop usage:kHIDUsage_GD_Keyboard]) {
            _keyboard = service;
            HIDLog("HIDSessionFilterExample::serviceNotification keyboard: %@ added", service);
        }
    } else {
        if ([[service propertyForKey:@(kIOHIDBuiltInKey)] boolValue] &&
            [service conformsToUsagePage:kHIDPage_GenericDesktop usage:kHIDUsage_GD_Keyboard]) {
            _keyboard = nil;
            HIDLog("HIDSessionFilterExample::serviceNotification keyboard: %@ removed", service);
        }
    }
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    HIDLog("HIDSessionFilterExample::setDispatchQueue %p", queue);
}

@end
