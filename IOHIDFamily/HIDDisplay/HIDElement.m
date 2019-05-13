//
//  HIDElement.m
//  HIDDisplay
//
//  Created by AB on 1/16/19.
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import "HIDElement.h"
#import "HIDElementPrivate.h"

@implementation HIDElement
{
    IOHIDElementRef elementRef;
}
-(instancetype) initWithObject:(IOHIDElementRef) element
{
    self = [super init];
    if (!self) {
        return nil;
    }
    self.element = element;
    
    return self;
}

-(void) dealloc
{
    if (elementRef) {
        _IOHIDElementSetValue(elementRef, NULL);
        CFRelease(elementRef);
    }
}

-(IOHIDElementRef) element
{
    return elementRef;
}

-(void) setElement:(IOHIDElementRef) element
{
    CFRetain(element);
    elementRef = element;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"timestamp: %llu type: %ld \
            usagePage: %ld usage: %ld reportID: %ld value: %ld",
            self.timestamp, (long)self.type, (long)self.usagePage,
            (long)self.usage, (long)self.reportID, (long)self.integerValue];
}

- (IOHIDValueRef)valueRef
{
    return _IOHIDElementGetValue(elementRef);
}

-(void)setValueRef:(IOHIDValueRef)valueRef
{
    _IOHIDElementSetValue(elementRef, valueRef);
}

- (uint32_t)cookie
{
    return (uint32_t)IOHIDElementGetCookie(elementRef);
}

- (double)scaleValue:(HIDValueScaleType)type
{
    return self.valueRef ? IOHIDValueGetScaledValue(
                                                    self.valueRef,
                                                    (IOHIDValueScaleType)type) : 0;
}

- (NSInteger)integerValue
{
    return self.valueRef ? IOHIDValueGetIntegerValue(self.valueRef) : 0;
}

- (void)setIntegerValue:(NSInteger)integerValue
{
    IOHIDValueRef value = IOHIDValueCreateWithIntegerValue(
                                                           kCFAllocatorDefault,
                                                           elementRef,
                                                           0,
                                                           integerValue);
    self.valueRef = value;
    CFRelease(value);
}

- (nullable NSData *)dataValue
{
    return self.valueRef ? [NSData dataWithBytes:(void *)IOHIDValueGetBytePtr(self.valueRef)
                                          length:IOHIDValueGetLength(self.valueRef)] : nil;
}

- (void)setDataValue:(NSData *)dataValue
{
    IOHIDValueRef value = IOHIDValueCreateWithBytes(
                                                    kCFAllocatorDefault,
                                                    elementRef,
                                                    0,
                                                    [dataValue bytes],
                                                    [dataValue length]);
    self.valueRef = value;
    CFRelease(value);
}

- (HIDElement *)parent
{
    return (__bridge HIDElement *)IOHIDElementGetParent(elementRef);
}

- (NSArray<HIDElement *> *)children
{
    return (__bridge NSArray *)IOHIDElementGetChildren(elementRef);
}

- (HIDElementType)type
{
    return (HIDElementType)IOHIDElementGetType(elementRef);
}

- (NSInteger)usagePage
{
    return IOHIDElementGetUsagePage(elementRef);
}

- (NSInteger)usage
{
    return IOHIDElementGetUsage(elementRef);
}

- (NSInteger)reportID
{
    return IOHIDElementGetReportID(elementRef);
}

- (NSInteger)reportSize
{
    return IOHIDElementGetReportSize(elementRef);
}

- (NSInteger)unit
{
    return IOHIDElementGetUnit(elementRef);
}

- (NSInteger)unitExponent
{
    return IOHIDElementGetUnitExponent(elementRef);
}

- (NSInteger)logicalMin
{
    return IOHIDElementGetLogicalMin(elementRef);
}

- (NSInteger)logicalMax
{
    return IOHIDElementGetLogicalMax(elementRef);
}

- (NSInteger)physicalMin
{
    return IOHIDElementGetPhysicalMin(elementRef);
}

- (NSInteger)physicalMax
{
    return IOHIDElementGetPhysicalMax(elementRef);
}

- (uint64_t)timestamp
{
    return self.valueRef ? IOHIDValueGetTimeStamp(self.valueRef) : 0;
}

@end
