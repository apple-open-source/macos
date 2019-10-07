//
//  TestIndependentDevices.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 12/14/18.
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

XCTestExpectation   *_deviceAdded;
XCTestExpectation   *_deviceRemoved;
XCTestExpectation   *_deviceCancelled;
XCTestExpectation   *_managerCancelled;

@interface TestIndependentDevices : XCTestCase {
    NSString            *_uniqueID;
    HIDUserDevice       *_userDevice;
    HIDManager          *_manager;
    HIDDevice           *_device;
}

@end

@implementation TestIndependentDevices

- (void)setUp
{
    [super setUp];
    
    _uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    _deviceAdded = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _deviceRemoved = [[XCTestExpectation alloc] initWithDescription:@"device removed"];
    _deviceCancelled = [[XCTestExpectation alloc] initWithDescription:@"device cancelled"];
    _managerCancelled = [[XCTestExpectation alloc] initWithDescription:@"manager cancelled"];
    
    NSData *descriptor = [[NSData alloc] initWithBytes:kbdDescriptor
                                                length:sizeof(kbdDescriptor)];
    NSMutableDictionary *properties = [[NSMutableDictionary alloc] init];
    
    properties[@kIOHIDReportDescriptorKey] = descriptor;
    properties[@kIOHIDPhysicalDeviceUniqueIDKey] = _uniqueID;
    
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
    
    _manager = [[HIDManager alloc] initWithOptions:HIDManagerIndependentDevices];
    
    [_manager setDeviceMatching:@{ @kIOHIDPhysicalDeviceUniqueIDKey : _uniqueID }];
    
    [_manager setDeviceNotificationHandler:^(HIDDevice *device, BOOL added) {
        if (added) {
            NSLog(@"Device added: %@", device);
            _device = device;
            [_deviceAdded fulfill];
        }
    }];
    
    [_manager setCancelHandler:^{
        NSLog(@"Manager cancelled");
        [_managerCancelled fulfill];
    }];
    
    [_manager setDispatchQueue:dispatch_get_main_queue()];
    [_manager activate];
}

- (void)testIndependentDevices
{
    XCTWaiterResult result;
    
    result = [XCTWaiter waitForExpectations:@[_deviceAdded] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _deviceAdded);
    
    [_manager cancel];
    
    result = [XCTWaiter waitForExpectations:@[_managerCancelled] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _managerCancelled);
    
    _manager = nil;
    
    [_device setDispatchQueue:dispatch_get_main_queue()];
    
    [_device setRemovalHandler:^{
        NSLog(@"Device removed");
        [_deviceRemoved fulfill];
    }];
    
    [_device setCancelHandler:^{
        NSLog(@"Device cancelled");
        [_deviceCancelled fulfill];
    }];
    
    [_device activate];
    [_userDevice cancel];
    
    result = [XCTWaiter waitForExpectations:@[_deviceRemoved] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _deviceRemoved);
    
    [_device cancel];
    
    result = [XCTWaiter waitForExpectations:@[_deviceCancelled] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _deviceCancelled);
}
@end
