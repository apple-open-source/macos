//
//  HIDSession.m
//  HID
//
//  Created by dekom on 9/13/18.
//

#import "HIDSession.h"
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDSession.h>

@implementation HIDSession (HIDFramework)

- (id)propertyForKey:(NSString *)key
{
    return (id)(IOHIDSessionGetProperty((__bridge IOHIDSessionRef)self,
                                        (__bridge CFStringRef)key));
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDSessionSetProperty((__bridge IOHIDSessionRef)self,
                                   (__bridge CFStringRef)key,
                                   (__bridge CFTypeRef)value);
}

@end
