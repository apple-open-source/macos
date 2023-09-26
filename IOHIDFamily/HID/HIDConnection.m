/*!
 * HIDConnection.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <HID/HIDConnection.h>
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDEventSystemConnection.h>

@implementation HIDConnection (HIDFramework)

- (NSString *)uuid
{
    return (__bridge NSString *)IOHIDEventSystemConnectionGetUUID(
                                (__bridge IOHIDEventSystemConnectionRef)self);
}

- (void)getAuditToken:(audit_token_t *)token
{
    IOHIDEventSystemConnectionGetAuditToken(
                                (__bridge IOHIDEventSystemConnectionRef)self, token);
}

@end
