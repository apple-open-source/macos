//
//  TestHIDElementAndValue.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 10/5/17.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <IOKit/hid/IOHIDLib.h>
#import "HID.h"
#import "HIDElementPrivate.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};


@interface TestHIDElementAndValue : XCTestCase

@property HIDUserDevice *hidUserDevice;
@property HIDDevice     *hidDevice;

@end

@implementation TestHIDElementAndValue

- (void)setUp
{
    TestLog("TestHIDElementAndValue setUp");
    
    [super setUp];
    
    NSMutableDictionary* deviceConfig = [[NSMutableDictionary alloc] init];
    deviceConfig [@kIOHIDReportDescriptorKey] = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    
    _hidUserDevice = [[HIDUserDevice alloc] initWithProperties:deviceConfig];
    HIDXCTAssertAndThrowTrue(_hidUserDevice);
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    _hidDevice = [[HIDDevice alloc] initWithService:_hidUserDevice.service];
    HIDXCTAssertAndThrowTrue(_hidDevice);
}

- (void)tearDown
{
    TestLog("TestHIDElementAndValue tearDown");
    
    @autoreleasepool {
        _hidDevice = nil;
        _hidUserDevice  = nil;
    }

    [super tearDown];
}

- (void)testHIDElement
{
    NSDictionary *matching = nil;
    NSArray *elements = [_hidDevice elementsMatching:@{}];
    HIDXCTAssertAndThrowTrue(elements);

    for (HIDElement *element in elements) {
        // This will call into all of the element's getter functions
        NSString *description __unused = element.description;
    }
    
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                  @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardA) };

    elements = [_hidDevice elementsMatching:matching];
    HIDXCTAssertAndThrowTrue(elements && elements.count == 1);

    HIDElement *hidElement = [elements objectAtIndex:0];
    HIDXCTAssertAndThrowTrue(hidElement);
    
    // verify usage min / max works
    matching = @{ @kIOHIDElementUsagePageKey: @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageMinKey : @(kHIDUsage_KeyboardA),
                   @kIOHIDElementUsageMaxKey : @(kHIDUsage_KeyboardZ) };
    
    elements = [_hidDevice elementsMatching:matching];
    HIDXCTAssertAndThrowTrue(elements && elements.count == 26);
    
    // 38271397
    matching = @{ @kIOHIDElementUsageMinKey : @(kHIDUsage_KeyboardCapsLock),
                   @kIOHIDElementUsageMaxKey : @(kHIDUsage_KeyboardCapsLock) };
    
    elements = [_hidDevice elementsMatching:matching];
    HIDXCTAssertAndThrowTrue(elements && elements.count == 1);
}

- (void)testHIDValue
{
    NSDictionary * matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                                 @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardA)};
    
    NSArray *elements = [_hidDevice elementsMatching:matching];
    HIDXCTAssertAndThrowTrue(elements && elements.count == 1);
    
    HIDElement *element = [elements objectAtIndex:0];
    HIDXCTAssertAndThrowTrue(element);
    
    element.integerValue = 123;
    HIDXCTAssertAndThrowTrue(element.integerValue == 123);
    HIDXCTAssertAndThrowTrue([element scaleValue:kIOHIDValueScaleTypePhysical] == 123);
    
    uint8_t byte = 0x03;
    NSData *data = [NSData dataWithBytes:&byte length:1];
    element.dataValue = data;
    
    HIDXCTAssertAndThrowTrue(((uint8_t *)element.dataValue.bytes)[0] == 0x03);
}

@end
