//
//  TestElementBatching.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 11/6/18.
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDUsageTables.h>
#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import "IOHIDUnitTestUtility.h"
#import <IOKit/IOKitLib.h>

static uint8_t kbdDescriptor[] = {
    HIDKeyboardDescriptor
};

XCTestExpectation   *_elementExp;

@interface TestElementBatching : XCTestCase {
    NSString            *_uniqueID;
    HIDUserDevice       *_userDevice;
    HIDDevice           *_device;
}

@end

@implementation TestElementBatching

- (void)setUp
{
    [super setUp];
    
    NSData *descriptor = [[NSData alloc] initWithBytes:kbdDescriptor
                                                length:sizeof(kbdDescriptor)];
    NSMutableDictionary *properties = [[NSMutableDictionary alloc] init];
    
    properties[@kIOHIDReportDescriptorKey] = descriptor;
    properties[@kIOHIDPhysicalDeviceUniqueIDKey] = _uniqueID;
    properties[(__bridge NSString *)kIOHIDServiceHiddenKey] = @YES;
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _userDevice);
    
    [_userDevice setDispatchQueue:dispatch_get_main_queue()];
    [_userDevice activate];
    
    mach_timespec_t waitTime = {30, 0};
    kern_return_t kr = IOServiceWaitQuiet(_userDevice.service, &waitTime);
    if (kr) {
        NSLog(@"IOServiceWaitQuiet:0x%x", kr);
        return;
    }
    
    _device = [[HIDDevice alloc] initWithService:_userDevice.service];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _device);
    
    [_device open];
    [_device setDispatchQueue:dispatch_get_main_queue()];
    
    _elementExp = [[XCTestExpectation alloc] initWithDescription:@"elements expectation"];
}

- (void)tearDown
{
    [_device close];
    [_device cancel];
    [_userDevice cancel];
    [super tearDown];
}

- (void)testElementBatching
{
    HIDKeyboardDescriptorInputReport inReport = { 0 };
    XCTWaiterResult result;
    
    _elementExp.expectedFulfillmentCount = 3;
    _elementExp.assertForOverFulfill = true;
    
    [_device setBatchInputElementHandler:^(NSArray<HIDElement *> *elements) {
        NSLog(@"HIDDevice input elements: %@", elements);
        
        for (HIDElement *element in elements) {
            if (element.usage == kHIDUsage_KeyboardA ||
                element.usage == kHIDUsage_KeyboardB ||
                element.usage == kHIDUsage_KeyboardC) {
                [_elementExp fulfill];
            }
        }
    }];
    
    [_device activate];
    
    inReport.KB_Keyboard[0] = kHIDUsage_KeyboardA;
    inReport.KB_Keyboard[1] = kHIDUsage_KeyboardB;
    inReport.KB_Keyboard[2] = kHIDUsage_KeyboardC;
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&inReport
                                                      length:sizeof(inReport)
                                                freeWhenDone:NO];
    
    [_userDevice handleReport:reportData error:nil];
    
    result = [XCTWaiter waitForExpectations:@[_elementExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _elementExp);
}

- (void)testMatchingElementBatching
{
    NSArray *matching = nil;
    HIDKeyboardDescriptorInputReport inReport = { 0 };
    XCTWaiterResult result;
    
    _elementExp.expectedFulfillmentCount = 2;
    _elementExp.assertForOverFulfill = true;
    
    matching = @[@{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                    @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardA)},
                 @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                    @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardB)}];
    
    [_device setInputElementMatching:matching];
    
    [_device setBatchInputElementHandler:^(NSArray<HIDElement *> *elements) {
        NSLog(@"HIDDevice input elements: %@", elements);
        
        for (HIDElement *element in elements) {
            if (element.usage == kHIDUsage_KeyboardA ||
                element.usage == kHIDUsage_KeyboardB ||
                element.usage == kHIDUsage_KeyboardC) {
                [_elementExp fulfill];
            }
        }
    }];
    
    [_device activate];
    
    inReport.KB_Keyboard[0] = kHIDUsage_KeyboardA;
    inReport.KB_Keyboard[1] = kHIDUsage_KeyboardB;
    inReport.KB_Keyboard[2] = kHIDUsage_KeyboardC;
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&inReport
                                                      length:sizeof(inReport)
                                                freeWhenDone:NO];
    
    [_userDevice handleReport:reportData error:nil];
    
    result = [XCTWaiter waitForExpectations:@[_elementExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _elementExp);
}

@end
