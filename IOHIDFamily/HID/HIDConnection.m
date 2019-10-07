//
//  HIDConnection.m
//  HID
//
//  Created by dekom on 9/16/18.
//

#import "HIDConnection.h"
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDEventSystemConnection.h>

@implementation HIDConnection (HIDFramework)

- (NSString *)uuid
{
    return (__bridge NSString *)IOHIDEventSystemConnectionGetUUID(
                                (__bridge IOHIDEventSystemConnectionRef)self);
}

@end
