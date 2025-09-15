/*!
 * HIDEventService.m
 * HID
 *
 * Copyright © 2022 Apple Inc. All rights reserved.
 */

#import <HID/HIDEventService.h>
#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDServicePrivate.h>


@implementation HIDEventService (HIDFramework)

- (id)propertyForKey:(NSString *)key
{
    return (id)CFBridgingRelease(IOHIDServiceCopyProperty(
                                                (__bridge IOHIDServiceRef)self,
                                                (__bridge CFStringRef)key));
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
{
    return IOHIDServiceSetProperty((__bridge IOHIDServiceRef)self,
                                   (__bridge CFStringRef)key,
                                   (__bridge CFTypeRef)value);
}

- (BOOL)conformsToUsagePage:(NSInteger)usagePage usage:(NSInteger)usage
{
    return IOHIDServiceConformsTo((__bridge IOHIDServiceRef)self,
                                  (uint32_t)usagePage,
                                  (uint32_t)usage);
}

- (HIDEvent *)eventMatching:(NSDictionary *)matching
{
    return (HIDEvent *)CFBridgingRelease(IOHIDServiceCopyMatchingEvent(
                                        (__bridge IOHIDServiceRef)self,
                                        (__bridge CFDictionaryRef)matching,
                                        NULL));
}

- (uint64_t)serviceID
{
    id regID = (__bridge id)IOHIDServiceGetRegistryID(
                                                (__bridge IOHIDServiceRef)self);
    return regID ? [regID unsignedLongLongValue] : 0;
}

- (void)registerWithSystem
{
    IOHIDServiceRegister((__bridge IOHIDServiceRef)self);
}

- (int)workIntervalStart:(uint64_t)start deadline:(uint64_t)deadline complexity:(uint64_t)complexity
{
    return IOHIDServiceWorkIntervalStart((__bridge IOHIDServiceRef)self, start, deadline, complexity);
}

- (int)workIntervalUpdate:(uint64_t)deadline complexity:(uint64_t)complexity
{
    return IOHIDServiceWorkIntervalUpdate((__bridge IOHIDServiceRef)self, deadline, complexity);
}

- (int)workIntervalFinish
{
    return IOHIDServiceWorkIntervalFinish((__bridge IOHIDServiceRef)self);
}

- (int)workIntervalCancel
{
    return IOHIDServiceWorkIntervalCancel((__bridge IOHIDServiceRef)self);
}

@end

@implementation HIDEventService (HIDFrameworkPrivate)

- (void)dispatchEvent:(HIDEvent *)event
{
    _IOHIDServiceDispatchEvent((__bridge IOHIDServiceRef)self,
                               (__bridge IOHIDEventRef)event);
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key and:(HIDConnection *)connection
{
    return _IOHIDServiceSetPropertyForClient((__bridge IOHIDServiceRef)self,
                                              (__bridge CFStringRef)key,
                                              (__bridge CFTypeRef)value,
                                              (__bridge CFTypeRef)connection);
}


- (nullable NSDictionary *)eventStatistics
{
    return (NSDictionary *) CFBridgingRelease(_IOHIDServiceCopyEventCounts((__bridge IOHIDServiceRef)self));
}

@end
