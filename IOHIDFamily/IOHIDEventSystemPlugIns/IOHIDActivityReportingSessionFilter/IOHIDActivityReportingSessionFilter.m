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
#import <LocalAuthentication/LocalAuthentication_Private.h>
#import <AssertMacros.h>
#import <os/log.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#import <HID/HID_Private.h>
#import <HID/ProjectHeaders/HIDEventAccessors_Internal.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDEventSystemConnection.h>
#import <IOKit/hid/IOHIDLibPrivate.h>

const int USER_ACTIVE_ASSERTION_TIMEOUT_SECONDS = 30;

@interface IOHIDActivityReportingSessionFilter : NSObject <HIDSessionFilter>

@property (nonatomic, readonly) HIDSession *        session;
@property (nonatomic, nullable) dispatch_queue_t    queue;

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


@implementation IOHIDActivityReportingSessionFilter {
   
}

- (nullable instancetype)initWithSession:(HIDSession *)session
{
    self = [super init];
    if (!self) {
        return self;
    }

    _session = session;
    _queue = dispatch_queue_create("com.apple.HID.updateActivity", NULL);
    
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
        debug[@"Class"] = @"IOHIDActivityReportingSessionFilter";
        
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
                        forService:(HIDEventService *)service
{
    require_quiet ([event integerValueForField:kIOHIDEventFieldIsBuiltIn] == 1, exit);
    require_quiet (event.type == kIOHIDEventTypeKeyboard, exit);
    require_quiet ([event integerValueForField:kIOHIDEventFieldKeyboardDown] != 0 , exit);

    [self reportActivity:event fromService:service];
    
exit:
    return event;
}


- (void)activate
{
}

- (void)setDispatchQueue:(dispatch_queue_t)queue
{
    
}

- (nullable HIDEvent *)filterEvent:(HIDEvent * __unused)event
                      toConnection:(HIDConnection * __unused)connection
                       fromService:(HIDEventService * __unused)service
{

    return event;
}

- (void) reportActivity:(HIDEvent *)event fromService:(HIDEventService *)service
{
    
    NSString * assertionNameStr = nil;
    
    if (service) {
        assertionNameStr = [NSString stringWithFormat:@"com.apple.iohideventsystem.queue.tickle serviceID:%llx", service.serviceID];
    } else {
        assertionNameStr = @"com.apple.iohideventsystem.queue.tickle.queue.tickle";
    }
    
    HIDLogDebug ("reportActivity: %@", assertionNameStr);

    dispatch_async(self.queue, ^{
        static IOPMAssertionID _AssertionID;
        
        IOReturn status = IOPMAssertionDeclareUserActivity((__bridge CFStringRef)assertionNameStr,
                                                           kIOPMUserActiveLocal,
                                                           &_AssertionID);
        if (status) {
            HIDLogError ("IOPMAssertionDeclareUserActivity status:0x%x", status);
            return;
        }

        CFNumberRef timeoutValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &USER_ACTIVE_ASSERTION_TIMEOUT_SECONDS);
        status = IOPMAssertionSetProperty(_AssertionID, kIOPMAssertionTimeoutKey, timeoutValue);
        if (status) {
            HIDLogError ("IOPMAssertionSetProperty status:0x%x", status);
        }
        
        if (timeoutValue) {
            CFRelease(timeoutValue);
        }
        
        IOReturn propertyStatus = IOPMAssertionSetProperty(_AssertionID, kIOPMAssertionResourcesUsed, (__bridge CFArrayRef)@[(__bridge NSString *)kIOPMAssertionResourceCamera]);
        if (propertyStatus != kIOReturnSuccess) {
            HIDLogError ("IOPMAssertionSetProperty for ResourceCamera status:0x%x", status);
        }

    });
}

@end

