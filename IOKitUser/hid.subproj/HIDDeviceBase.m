//
//  HIDDeviceBase.m
//  iohidobjc
//
//  Created by dekom on 10/17/18.
//

#import "IOHIDDevicePrivate.h"
#import "IOHIDLibPrivate.h"
#import "HIDDeviceBase.h"
#import <CoreFoundation/CoreFoundation.h>

@implementation HIDDevice

- (CFTypeID)_cfTypeID {
    return IOHIDDeviceGetTypeID();
}

- (void)dealloc
{
    if (_device.propertyNotificationHandler) {
        Block_release(_device.propertyNotificationHandler);
        _device.propertyNotificationHandler = NULL;
    }

    if (_device.propertyNotificationKeys) {
        CFRelease(_device.propertyNotificationKeys);
        _device.propertyNotificationKeys = NULL;
    }

    if (_device.previousPropertyValues) {
        CFRelease(_device.previousPropertyValues);
        _device.previousPropertyValues = NULL;
    }

    if (_device.propertyNotify) {
        IOObjectRelease(_device.propertyNotify);
        _device.propertyNotify = IO_OBJECT_NULL;
    }

    if (_device.propertyPort) {
        IONotificationPortDestroy(_device.propertyPort);
        _device.propertyPort = NULL;
    }

    _IOHIDDeviceReleasePrivate((__bridge IOHIDDeviceRef)self);
    [super dealloc];
}

- (NSString *)description
{
    NSString *desc = (__bridge NSString *)IOHIDDeviceCopyDescription(
                                                (__bridge IOHIDDeviceRef)self);
    return [desc autorelease];
}

@end

