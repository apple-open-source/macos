//
//  TestQueueClass.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 12/4/17.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <IOKit/hid/IOHIDQueue.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "HIDUserDevice.h"
#import "HIDDevicePrivate.h"
#import "HIDElementPrivate.h"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestQueueClass : XCTestCase

@property dispatch_queue_t      deviceQueue;
@property dispatch_queue_t      rootQueue;
@property __block HIDUserDevice *userDevice;
@property HIDDevice             *device;
@property dispatch_block_t      userDeviceCancelHandler;

@end;

@implementation TestQueueClass

- (void)setUp
{
    NSMutableDictionary *deviceConfig = [[NSMutableDictionary alloc] init];
    NSData *desc = [[NSData alloc] initWithBytes:descriptor
                                          length:sizeof(descriptor)];
    
    _rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    _deviceQueue = dispatch_queue_create_with_target("HIDUserDevice",
                                                     DISPATCH_QUEUE_SERIAL,
                                                     _rootQueue);
    HIDXCTAssertAndThrowTrue(_deviceQueue != nil);
    
    deviceConfig[@kIOHIDReportDescriptorKey] = desc;
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:deviceConfig];
    HIDXCTAssertAndThrowTrue(_userDevice);
    
    [_userDevice setDispatchQueue:_deviceQueue];
    
    _userDeviceCancelHandler = dispatch_block_create(0, ^{
        ;
    });
    HIDXCTAssertAndThrowTrue(_userDeviceCancelHandler);
    
    [_userDevice setCancelHandler:_userDeviceCancelHandler];
    
    [_userDevice activate];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    _device = [[HIDDevice alloc] initWithService:_userDevice.service];
    HIDXCTAssertAndThrowTrue(_device);
    
    [_device open];
}

- (void)tearDown
{
    NSLog(@"tearDown");
    [_device close];
    [_userDevice cancel];
    dispatch_wait(_userDeviceCancelHandler, DISPATCH_TIME_FOREVER);
    _userDevice = nil;
    [super tearDown];
}

static int _inputValueCount;
static int _inputReportCount;

static void valueAvailableCallback(void *context __unused,
                                   IOReturn result __unused,
                                   void *sender)
{
    IOHIDQueueRef queue = (IOHIDQueueRef)sender;
    IOHIDValueRef value = NULL;
    
    while ((value = IOHIDQueueCopyNextValue(queue))) {
        HIDElement *element = (__bridge HIDElement *)IOHIDValueGetElement(value);
        element.valueRef = value;
        
        if (element.usagePage == kHIDPage_KeyboardOrKeypad &&
            element.usage == kHIDUsage_KeyboardA) {
            _inputValueCount++;
        }
        
        NSLog(@"received input element uP: 0x%02lx u: 0x%02lx val: %ld",
              (long)element.usagePage, (long)element.usage, (long)element.integerValue);
        
        CFRelease(value);
    }
}

- (void)testQueue
{
    IOHIDQueueRef queue = NULL;
    HIDKeyboardDescriptorInputReport report = { 0 };
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&report
                                                      length:sizeof(report)
                                                freeWhenDone:NO];
    _inputValueCount = 0;
    
    queue = IOHIDQueueCreate(kCFAllocatorDefault, (__bridge IOHIDDeviceRef)_device, 1, 0);
    HIDXCTAssertAndThrowTrue(queue);
    
    NSArray *elements = [_device elementsMatching:@{}];
    HIDXCTAssertAndThrowTrue(elements);
    
    for(HIDElement *element in elements) {
        if (element.type <= HIDElementTypeInputButton) {
            IOHIDQueueAddElement(queue, (__bridge IOHIDElementRef)element);
            XCTAssert(IOHIDQueueContainsElement(queue, (__bridge IOHIDElementRef)element));
        }
    }
    
    IOHIDQueueRegisterValueAvailableCallback(queue,
                                             valueAvailableCallback,
                                             NULL);
    
    IOHIDQueueScheduleWithRunLoop(queue,
                                  CFRunLoopGetMain(),
                                  kCFRunLoopDefaultMode);
    
    IOHIDQueueStart(queue);
    
    report.KB_Keyboard[0] = kHIDUsage_KeyboardA;
    
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    report.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    XCTAssert(_inputValueCount == 2);
    
    IOHIDQueueStop(queue);
    IOHIDQueueUnscheduleFromRunLoop(queue,
                                    CFRunLoopGetMain(),
                                    kCFRunLoopDefaultMode);
    CFRelease(queue);
}

- (void)testQueueDQ
{
    __block IOHIDQueueRef queue = NULL;
    HIDKeyboardDescriptorInputReport report = { 0 };
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&report
                                                      length:sizeof(report)
                                                freeWhenDone:NO];
    _inputValueCount = 0;
    
    queue = IOHIDQueueCreate(kCFAllocatorDefault, (__bridge IOHIDDeviceRef)_device, 1, 0);
    HIDXCTAssertAndThrowTrue(queue);
    
    NSArray *elements = [_device elementsMatching:@{}];
    HIDXCTAssertAndThrowTrue(elements);
    
    for(HIDElement *element in elements) {
        if (element.type <= HIDElementTypeInputButton &&
            element.usagePage == kHIDPage_KeyboardOrKeypad &&
            element.usage == kHIDUsage_KeyboardB) {
            IOHIDQueueAddElement(queue, (__bridge IOHIDElementRef)element);
            XCTAssert(IOHIDQueueContainsElement(queue, (__bridge IOHIDElementRef)element));
        }
    }
    
    IOHIDQueueRegisterValueAvailableCallback(queue,
                                             valueAvailableCallback,
                                             NULL);
    
    IOHIDQueueSetCancelHandler(queue, ^{
        CFRelease(queue);
        queue = NULL;
    });
    
    IOHIDQueueSetDispatchQueue(queue, dispatch_get_main_queue());
    IOHIDQueueActivate(queue); // activate will call IOHIDQueueStart
    
    report.KB_Keyboard[0] = kHIDUsage_KeyboardA;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    report.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    // make sure we don't receive unwated elements
    XCTAssert(_inputValueCount == 0);
    
    report.KB_Keyboard[0] = kHIDUsage_KeyboardB;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    report.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    XCTAssert(_inputValueCount == 0);
    
    IOHIDQueueCancel(queue); // cancel will call IOHIDQueueStop
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    XCTAssert(queue == NULL);
}

// This exercises the IOHIDQueue that is used by the IOHIDDeviceClass for
// receiving input reports
- (void)testDeviceQueue
{
    HIDKeyboardDescriptorInputReport kbdReport = { 0 };
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&kbdReport
                                                      length:sizeof(kbdReport)
                                                freeWhenDone:NO];
    _inputReportCount = 0;
    _inputValueCount = 0;
    NSDictionary *matching = nil;
    
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardB) };
    
    [_device setInputElementMatching:matching];
    
    [_device setInputElementHandler:^(HIDElement *element) {
        _inputValueCount++;
        
        NSLog(@"received input element uP: 0x%02lx u: 0x%02lx val: %ld",
              (long)element.usagePage, (long)element.usage, (long)element.integerValue);
    }];
    
    [_device setInputReportHandler:^(HIDDevice *sender __unused,
                                     uint64_t timestamp __unused,
                                     HIDReportType type __unused,
                                     NSInteger reportID __unused,
                                     NSData *report __unused)
    {
        _inputReportCount++;
    }];
    
    [_device setDispatchQueue:dispatch_get_main_queue()];
    [_device activate];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    kbdReport.KB_Keyboard[0] = kHIDUsage_KeyboardA;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    kbdReport.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    // make sure we don't receive unwated elements
    XCTAssert(_inputReportCount == 2 && _inputValueCount == 0);
    
    kbdReport.KB_Keyboard[0] = kHIDUsage_KeyboardB;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    kbdReport.KB_Keyboard[0] = 0;
    [_userDevice handleReport:reportData error:nil];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    XCTAssert(_inputReportCount == 4 && _inputValueCount == 2);
    
    [_device cancel];
}

@end
