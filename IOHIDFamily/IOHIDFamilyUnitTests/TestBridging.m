//
//  TestBridging.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 7/26/18.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <IOKit/hid/IOHIDLibPrivate.h>

@interface TestBridging : XCTestCase
@end

@implementation TestBridging

- (void)testHIDEventBridgingNStoCF
{
    HIDEvent *event = [[HIDEvent alloc] initWithType:kIOHIDEventTypeKeyboard
                                           timestamp:1234
                                            senderID:123456];
    IOHIDEventRef eventRef = NULL;
    
    [event setIntegerValue:kHIDPage_KeyboardOrKeypad forField:kIOHIDEventFieldKeyboardUsagePage];
    [event setIntegerValue:kHIDUsage_KeyboardA forField:kIOHIDEventFieldKeyboardUsage];
    [event setIntegerValue:1 forField:kIOHIDEventFieldKeyboardDown];
    
    eventRef = (__bridge IOHIDEventRef)event;
    XCTAssert(IOHIDEventGetTypeID() == CFGetTypeID(eventRef));
    
    NSLog(@"EventRef: %@", eventRef);
    
    XCTAssert(IOHIDEventGetIntegerValue(eventRef, kIOHIDEventFieldKeyboardUsagePage) == kHIDPage_KeyboardOrKeypad);
    XCTAssert(IOHIDEventGetIntegerValue(eventRef, kIOHIDEventFieldKeyboardUsage) == kHIDUsage_KeyboardA);
    XCTAssert(IOHIDEventGetIntegerValue(eventRef, kIOHIDEventFieldKeyboardDown) == 1);
}

- (void)testHIDEventBridgingCFtoNS
{
    IOHIDEventRef eventRef = IOHIDEventCreate(kCFAllocatorDefault,
                                              kIOHIDEventTypeKeyboard,
                                              1234,
                                              0);
    HIDEvent *event = nil;
    
    IOHIDEventSetIntegerValue(eventRef, kIOHIDEventFieldKeyboardUsagePage, kHIDPage_KeyboardOrKeypad);
    IOHIDEventSetIntegerValue(eventRef, kIOHIDEventFieldKeyboardUsage, kHIDUsage_KeyboardA);
    IOHIDEventSetIntegerValue(eventRef, kIOHIDEventFieldKeyboardDown, 1);
    
    event = (__bridge HIDEvent *)eventRef;
    
    NSLog(@"Event: %@", event);
    
    XCTAssert([event integerValueForField:kIOHIDEventFieldKeyboardUsagePage] == kHIDPage_KeyboardOrKeypad);
    XCTAssert([event integerValueForField:kIOHIDEventFieldKeyboardUsage] == kHIDUsage_KeyboardA);
    XCTAssert([event integerValueForField:kIOHIDEventFieldKeyboardDown] == 1);
    CFRelease(eventRef);
}

- (void)testHIDElementBridging
{
    IOHIDElementRef elementRef = NULL;
    HIDElement *element;
    IOHIDElementStruct elementStruct = { 0 };
    CFDataRef dummyData = CFDataCreateMutable(kCFAllocatorDefault, 1);
    IOHIDValueRef value;
    
    // size of 1 byte to hold value
    elementStruct.size = 1;
    
    // logical min/max
    elementStruct.min = 12;
    elementStruct.max = 34;
    
    // physical min/max
    elementStruct.scaledMin = 56;
    elementStruct.scaledMax = 78;
    
    // 10^3
    elementStruct.unitExponent = 3;
    
    elementRef = _IOHIDElementCreateWithParentAndData(kCFAllocatorDefault, NULL, dummyData, &elementStruct, 0);
    XCTAssert(elementRef);
    
    value = IOHIDValueCreateWithIntegerValue(kCFAllocatorDefault, elementRef, 0, 1);
    XCTAssert(value);
    
    _IOHIDElementSetValue(elementRef, value);
    
    element = (__bridge HIDElement *)elementRef;
    
    XCTAssert(element.logicalMin == 12);
    XCTAssert(element.logicalMax == 34);
    XCTAssert(element.physicalMin == 56);
    XCTAssert(element.physicalMax == 78);
    XCTAssert(element.unitExponent == 3);
    XCTAssert(element.integerValue == 1);
    
    XCTAssert(IOHIDElementGetTypeID() == CFGetTypeID(elementRef));
    
    CFRelease(elementRef);
    CFRelease(value);
    CFRelease(dummyData);
}

@end
