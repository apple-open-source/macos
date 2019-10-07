//
//  TestHIDManagerDQ.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 10/5/17.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import <IOKit/hid/IOHIDLib.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "HID.h"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestHIDManagerDQ : XCTestCase

@property dispatch_queue_t      deviceQueue;
@property dispatch_queue_t      rootQueue;
@property __block HIDUserDevice *hidUserDevice;
@property dispatch_block_t      userDeviceCancelHandler;
@property NSString              *userDeviceID;

@end;

@implementation TestHIDManagerDQ

- (void)setUp
{
    [super setUp];
    NSLog(@"setUp");
    _rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    _deviceQueue = dispatch_queue_create_with_target("IOHIDUserDeviceTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(_deviceQueue != nil);
    
    NSMutableDictionary* deviceConfig = [[NSMutableDictionary alloc] init];
    _userDeviceID = [[[NSUUID alloc] init] UUIDString];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    deviceConfig [@kIOHIDPhysicalDeviceUniqueIDKey] = _userDeviceID;
    
    _hidUserDevice = [[HIDUserDevice alloc] initWithProperties:deviceConfig];
    HIDXCTAssertAndThrowTrue(_hidUserDevice);
    
    [_hidUserDevice setDispatchQueue:_deviceQueue];
    
    _userDeviceCancelHandler = dispatch_block_create(0, ^{
        ;
    });
    HIDXCTAssertAndThrowTrue(_userDeviceCancelHandler);
    
    [_hidUserDevice setCancelHandler:_userDeviceCancelHandler];
}

- (void)tearDown
{
    NSLog(@"tearDown");
    [super tearDown];
}

- (void)testDeviceElements
{
    [_hidUserDevice setSetReportHandler:^IOReturn(HIDReportType type,
                                                  NSInteger reportID __unused,
                                                  const void *report,
                                                  NSInteger reportLength)
    {
        // Output report is a bitmask of LEDs where caps is 1 << 1
        if (type == HIDReportTypeOutput &&
            reportLength == 1 &&
            ((uint8_t *)report)[0] == 0x02) {
            return kIOReturnSuccess;
        }
        
        return kIOReturnError;
    }];
    
    [_hidUserDevice setGetReportHandler:^IOReturn(HIDReportType type,
                                                  NSInteger reportID,
                                                  void *report,
                                                  NSInteger *reportLength)
     {
         HIDKeyboardDescriptorOutputReport outReport = { 0 };
         NSInteger length = MIN((unsigned long)*reportLength, sizeof(outReport));
         
         assert(type == HIDReportTypeOutput);
         
         outReport.LED_KeyboardCapsLock = 1;
         bcopy((const void *)&outReport, report, length);
         
         *reportLength = length;
         
         NSLog(@"HIDUserDevice getReport type: %lu reportID: %ld",
               (unsigned long)type, (long)reportID);
         
         return kIOReturnSuccess;
     }];
    
    [_hidUserDevice activate];
    
    // allow enumeration
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    __block HIDDevice *hidDevice = [[HIDDevice alloc] initWithService:_hidUserDevice.service];
    HIDXCTAssertAndThrowTrue(hidDevice);
    
    [hidDevice open];

    NSDictionary *matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_LEDs),
                                 @kIOHIDElementUsageKey : @(kHIDUsage_LED_CapsLock) };
    
    NSArray *elements = [hidDevice elementsMatching:matching];
    HIDXCTAssertAndThrowTrue(elements && elements.count == 1);
    
    HIDElement *capsLEDElement = [elements objectAtIndex:0];
    capsLEDElement.integerValue = 1;
    
    BOOL ret = [hidDevice commitElements:@[capsLEDElement]
                               direction:HIDDeviceCommitDirectionOut
                                   error:nil];
    HIDXCTAssertAndThrowTrue(ret);
    
    capsLEDElement.integerValue = 0;
    ret = [hidDevice commitElements:@[capsLEDElement]
                          direction:HIDDeviceCommitDirectionIn
                              error:nil];
    HIDXCTAssertAndThrowTrue(ret);
    HIDXCTAssertAndThrowTrue(capsLEDElement.integerValue == 1);
    
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardCapsLock) };
    
    [hidDevice setInputElementMatching:matching];
    
    __block int inputElementCount = 0;
    [hidDevice setInputElementHandler:^(HIDElement * _Nullable element) {
        if (element.usagePage == kHIDPage_KeyboardOrKeypad &&
            element.usage == kHIDUsage_KeyboardCapsLock) {
            inputElementCount++;
        }
    }];
    
    dispatch_block_t deviceCancelHandler = dispatch_block_create(0, ^{
        ;
    });
    
    [hidDevice setCancelHandler:deviceCancelHandler];
    [hidDevice setDispatchQueue:_deviceQueue];
    [hidDevice activate];
    
    uint8_t inputReport[8] = { 0x00, 0x00, kHIDUsage_KeyboardCapsLock, 0x00, 0x00, 0x00, 0x00, 0x00 };
    CFIndex inputReportLength = sizeof(inputReport)/sizeof(inputReport[0]);
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&inputReport
                                                      length:inputReportLength
                                                freeWhenDone:NO];
    
    [_hidUserDevice handleReport:reportData error:nil];
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    HIDXCTAssertAndThrowTrue(inputElementCount == 1);

    [_hidUserDevice cancel];
    dispatch_wait(_userDeviceCancelHandler, DISPATCH_TIME_FOREVER);
    _hidUserDevice = nil;
    
    [hidDevice close];
    [hidDevice cancel];
    dispatch_wait(deviceCancelHandler, DISPATCH_TIME_FOREVER);
    hidDevice = nil;
}

- (void)testDevice
{
    [_hidUserDevice setGetReportHandler:^IOReturn(HIDReportType type,
                                                  NSInteger reportID __unused,
                                                  void *report,
                                                  NSInteger *reportLength __unused) {
        if (type == HIDReportTypeOutput) {
            ((uint8_t *)report)[0] = 0x02;
            return kIOReturnSuccess;
        }
        
        return kIOReturnError;
    }];
    
    [_hidUserDevice setSetReportHandler:^IOReturn(HIDReportType type,
                                                  NSInteger reportID __unused,
                                                  const void *report,
                                                  NSInteger reportLength)
    {
        if (type == HIDReportTypeOutput &&
            reportLength == 1 &&
            ((uint8_t *)report)[0] == 0x02) {
            return kIOReturnSuccess;
        }
        
        return kIOReturnError;
    }];

    [_hidUserDevice activate];
    
    // allow enumeration
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);

    __block HIDDevice *hidDevice = [[HIDDevice alloc] initWithService:_hidUserDevice.service];
    HIDXCTAssertAndThrowTrue(hidDevice);
    
    [hidDevice open];
    
    [hidDevice setProperty:@1 forKey:@"Test"];
    HIDXCTAssertAndThrowTrue([[hidDevice propertyForKey:@"Test"] isEqualToNumber:@1]);

    HIDXCTAssertAndThrowTrue([hidDevice conformsToUsagePage:kHIDPage_GenericDesktop usage:kHIDUsage_GD_Keyboard]);
    
    uint8_t outputReport[1] = { 0x02 };
    NSInteger outputReportLength = sizeof(outputReport)/sizeof(outputReport[0]);
    NSMutableData *reportData = [[NSMutableData alloc] initWithBytes:&outputReport
                                                              length:outputReportLength];

    
    BOOL ret = [hidDevice setReport:reportData.bytes
                       reportLength:reportData.length
                     withIdentifier:0
                            forType:HIDReportTypeOutput
                              error:nil];
    HIDXCTAssertAndThrowTrue(ret);

    ((uint8_t *)reportData.mutableBytes)[0] = 0;
    ret = [hidDevice getReport:reportData.mutableBytes
                  reportLength:&outputReportLength
                withIdentifier:0
                       forType:HIDReportTypeOutput
                         error:nil];
    HIDXCTAssertAndThrowTrue(ret &&
                             ((uint8_t *)reportData.mutableBytes)[0] == 0x02 &&
                             outputReportLength == 1);
    
    __block bool removeHandled = false;
    [hidDevice setRemovalHandler:^{
        removeHandled = true;
    }];

    __block int inputReportCount = 0;
    [hidDevice setInputReportHandler:^(HIDDevice *sender __unused,
                                       uint64_t timestamp __unused,
                                       HIDReportType type,
                                       NSInteger reportID __unused,
                                       NSData *report)
    {
        if (type == HIDReportTypeInput &&
            report.length == 8 &&
            ((uint8_t *)report.bytes)[2] == kHIDUsage_KeyboardCapsLock) {
            inputReportCount++;
        }
    }];
    
    dispatch_block_t deviceCancelHandler = dispatch_block_create(0, ^{
        ;
    });
    
    [hidDevice setCancelHandler:deviceCancelHandler];
    [hidDevice setDispatchQueue:_deviceQueue];
    [hidDevice activate];

    uint8_t inputReport[8] = { 0x00, 0x00, kHIDUsage_KeyboardCapsLock, 0x00, 0x00, 0x00, 0x00, 0x00 };
    CFIndex inputReportLength = sizeof(inputReport)/sizeof(inputReport[0]);
    reportData = [[NSMutableData alloc] initWithBytes:&inputReport
                                               length:inputReportLength];

    [_hidUserDevice handleReport:reportData error:nil];
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    HIDXCTAssertAndThrowTrue(inputReportCount == 1);

    [_hidUserDevice cancel];
    dispatch_wait(_userDeviceCancelHandler, DISPATCH_TIME_FOREVER);
    _hidUserDevice = nil;
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    HIDXCTAssertAndThrowTrue(removeHandled);

    [hidDevice close];
    [hidDevice cancel];
    dispatch_wait(deviceCancelHandler, DISPATCH_TIME_FOREVER);
    hidDevice = nil;
}

- (void)testManager
{
    __block HIDManager *manager = [[HIDManager alloc] init];
    HIDXCTAssertAndThrowTrue(manager);

    [manager open];

    [manager setProperty:@1 forKey:@"Test"];
    HIDXCTAssertAndThrowTrue([[manager propertyForKey:@"Test"] isEqualToNumber:@1]);

    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _userDeviceID };
    [manager setDeviceMatching:matching];

    __block bool deviceAdded = false;
    __block bool deviceRemoved = false;
    [manager setDeviceNotificationHandler:^(HIDDevice *device __unused,
                                            BOOL added)
    {
        if (added) {
            deviceAdded = true;
        } else {
            deviceRemoved = true;
        }
    }];

    __block int inputReportCount = 0;
    [manager setInputReportHandler:^(HIDDevice *sender __unused,
                                     uint64_t timestamp __unused,
                                     HIDReportType type __unused,
                                     NSInteger reportID __unused,
                                     NSData *report __unused)
     {
         inputReportCount++;
     }];
    
    matching = @{ @kIOHIDElementUsagePageKey : @(kHIDPage_KeyboardOrKeypad),
                   @kIOHIDElementUsageKey : @(kHIDUsage_KeyboardCapsLock) };
    
    [manager setInputElementMatching:matching];
    
    __block int inputElementCount = 0;
    [manager setInputElementHandler:^(HIDDevice * _Nullable sender __unused, HIDElement * _Nullable element __unused)
    {
        if (element.usagePage == kHIDPage_KeyboardOrKeypad &&
            element.usage == kHIDUsage_KeyboardCapsLock) {
            inputElementCount++;
        }
    }];
    
    dispatch_block_t cancelHandler = dispatch_block_create(0, ^{
        ;
    });
    
    [manager setCancelHandler:cancelHandler];
    [manager setDispatchQueue:_deviceQueue];
    [manager activate];
    
    [_hidUserDevice activate];
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    HIDXCTAssertAndThrowTrue(deviceAdded);

    NSArray *devices = manager.devices;
    HIDXCTAssertAndThrowTrue(devices && devices.count == 1);

    uint8_t hidReport[8] = { 0x00, 0x00, kHIDUsage_KeyboardCapsLock, 0x00, 0x00, 0x00, 0x00, 0x00 };
    CFIndex hidReportLength = sizeof(hidReport)/sizeof(hidReport[0]);
    NSData *reportData = [[NSData alloc] initWithBytesNoCopy:&hidReport
                                                      length:hidReportLength
                                                freeWhenDone:NO];

    [_hidUserDevice handleReport:reportData error:nil];

    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    HIDXCTAssertAndThrowTrue(inputReportCount == 1);
    HIDXCTAssertAndThrowTrue(inputElementCount == 1);

    [_hidUserDevice cancel];
    dispatch_wait(_userDeviceCancelHandler, DISPATCH_TIME_FOREVER);
    _hidUserDevice = nil;
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    HIDXCTAssertAndThrowTrue(deviceRemoved);
    
    [manager close];
    [manager cancel];
    dispatch_wait(cancelHandler, DISPATCH_TIME_FOREVER);
}

@end

