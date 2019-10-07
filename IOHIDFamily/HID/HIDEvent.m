//
//  HIDEvent.m
//  HID
//
//  Created by dekom on 12/20/17.
//

#import "HIDEvent.h"
#import "NSError+IOReturn.h"
#import <IOKit/hid/IOHIDEventPrivate.h>
#import <IOKit/hid/IOHIDEventData.h>

@implementation HIDEvent (HIDFramework)

- (instancetype)initWithType:(IOHIDEventType)type
                   timestamp:(uint64_t)timestamp
                    senderID:(uint64_t)senderID
{
    self = (__bridge_transfer HIDEvent *)IOHIDEventCreate(kCFAllocatorDefault, type, timestamp, 0);
    
    if (!self) {
        return self;
    }
    
    IOHIDEventSetSenderID((__bridge IOHIDEventRef)self,
                          (IOHIDEventSenderID)senderID);
    
    return self;
}

- (instancetype)initWithData:(NSData *)data
{
    return (__bridge_transfer HIDEvent *)IOHIDEventCreateWithData(
                                                    kCFAllocatorDefault,
                                                    (__bridge CFDataRef)data);
}

- (instancetype)initWithBytes:(const void *)bytes length:(NSInteger)length
{
    return (__bridge_transfer HIDEvent *)IOHIDEventCreateWithBytes(
                                                        kCFAllocatorDefault,
                                                        bytes,
                                                        length);
}

- (nonnull id)copyWithZone:(nullable NSZone * __unused)zone {
    return (__bridge_transfer HIDEvent *)IOHIDEventCreateCopy(kCFAllocatorDefault,
                                                              (__bridge IOHIDEventRef)self);
}
- (BOOL)isEqualToHIDEvent:(HIDEvent *)event {
    if (!event) {
        return NO;
    }
    
    return _IOHIDEventEqual((__bridge IOHIDEventRef)self,
                            (__bridge IOHIDEventRef)event);
}

- (BOOL)isEqual:(id)object {
    if (self == object) {
        return YES;
    }
    
    if (![object isKindOfClass:[HIDEvent class]]) {
        return NO;
    }
    
    return [self isEqualToHIDEvent:(HIDEvent *)object];
}

- (NSData *)serialize:(HIDEventSerializationType)type
                error:(out NSError **)outError
{
    NSData *data = nil;
    
    if (type == HIDEventSerializationTypeFast) {
        data = (__bridge_transfer NSData *)IOHIDEventCreateData(
                                                kCFAllocatorDefault,
                                                (__bridge IOHIDEventRef)self);
    }
    
    if (outError && !data) {
        // todo: have a more descriptive error..
        *outError = [NSError errorWithIOReturn:kIOReturnError];
    }
    
    return data;
}

- (NSInteger)integerValueForField:(IOHIDEventField)field
{
    return (NSInteger)IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self,
                                                field);
}

- (void)setIntegerValue:(NSInteger)value forField:(IOHIDEventField)field
{
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, field,
                              (CFIndex)value);
}

- (double)doubleValueForField:(IOHIDEventField)field
{
    return (double)IOHIDEventGetDoubleValue((__bridge IOHIDEventRef)self,
                                            field);
}

- (void)setDoubleValue:(double)value forField:(IOHIDEventField)field
{
    IOHIDEventSetDoubleValue((__bridge IOHIDEventRef)self,
                             field,
                             (IOHIDDouble)value);
}

- (void *)dataValueForField:(IOHIDEventField)field
{
    return (void *)IOHIDEventGetDataValue((__bridge IOHIDEventRef)self, field);
}

- (void)appendEvent:(HIDEvent *)event
{
    IOHIDEventAppendEvent((__bridge IOHIDEventRef)self,
                          (__bridge IOHIDEventRef)event,
                          0);
}

- (void)removeEvent:(HIDEvent *)event
{
    IOHIDEventRemoveEvent((__bridge IOHIDEventRef)self,
                          (__bridge IOHIDEventRef)event,
                          0);
}

- (void)removeAllEvents
{
    for (HIDEvent *event in self.children) {
        IOHIDEventRemoveEvent((__bridge IOHIDEventRef)self,
                              (__bridge IOHIDEventRef)event,
                              0);
    }
}

- (BOOL)conformsToEventType:(IOHIDEventType)type
{
    return IOHIDEventConformsTo((__bridge IOHIDEventRef)self, type);
}

- (uint64_t)timestamp
{
    return IOHIDEventGetTimeStamp((__bridge IOHIDEventRef)self);
}

- (void)setTimestamp:(uint64_t)timestamp
{
    IOHIDEventSetTimeStamp((__bridge IOHIDEventRef)self, timestamp);
}

- (uint64_t)senderID
{
    return IOHIDEventGetSenderID((__bridge IOHIDEventRef)self);
}

- (IOHIDEventType)type
{
    return IOHIDEventGetType((__bridge IOHIDEventRef)self);
}

- (uint32_t)options
{
    return IOHIDEventGetEventFlags((__bridge IOHIDEventRef)self);
}

- (void)setOptions:(uint32_t)options
{
    IOHIDEventSetEventFlags((__bridge IOHIDEventRef)self, options);
}

- (HIDEvent *)parent
{
    return (__bridge HIDEvent *)IOHIDEventGetParent(
                                                (__bridge IOHIDEventRef)self);
}

- (NSArray<HIDEvent *> *)children
{
    return (__bridge NSArray *)IOHIDEventGetChildren(
                                                (__bridge IOHIDEventRef)self);
}

@end
