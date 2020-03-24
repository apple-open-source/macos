//
//  TestPublicHIDUserDevice.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 6/5/19.
//

#import <Foundation/Foundation.h>

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "IOHIDXCTestExpectation.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hidsystem/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <HID/HID.h>
#import <mach/mach_time.h>
#include <IOKit/IOKitLib.h>
#include "HIDEventAccessors_Private.h"

uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestPublicHIDUserDevice : XCTestCase {
    IOHIDUserDeviceRef      _userDevice;
    HIDEventSystemClient    *_client;
    HIDManager              *_manager;
    HIDDevice               *_device;
    NSString                *_uuid;
}
@end

XCTestExpectation *_devAddedExp;
XCTestExpectation *_devRemovedExp;
XCTestExpectation *_servAddedExp;
XCTestExpectation *_servRemovedExp;
XCTestExpectation *_reportExp;
XCTestExpectation *_eventExp;
XCTestExpectation *_cancelExp;
XCTestExpectation *_ledSetExp;

@implementation TestPublicHIDUserDevice

- (void)setupClient
{
    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid };
    
    _client = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    [_client setMatching:matching];
    [_client setServiceNotificationHandler:^(HIDServiceClient *service) {
        NSLog(@"Service added:\n%@", service);
        [service setRemovalHandler:^{
            NSLog(@"Service removed");
            [_servRemovedExp fulfill];
        }];
        [_servAddedExp fulfill];
    }];
    [_client setEventHandler:^(HIDServiceClient * service __unused, HIDEvent *event) {
        NSLog(@"received event:\n%@", event);
        if (event.keyboardUsage == kHIDUsage_KeyboardLeftShift) {
            [_eventExp fulfill];
        }
    }];
    [_client setDispatchQueue:dispatch_get_main_queue()];
    [_client activate];
}

- (void)setupUserDevice
{
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    dispatch_queue_t udQueue = dispatch_queue_create("", DISPATCH_QUEUE_SERIAL);
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = IOHIDUserDeviceCreateWithProperties(kCFAllocatorDefault,
                                                      (__bridge CFDictionaryRef)properties,
                                                      0);
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               _userDevice != NULL);
    
    IOHIDUserDeviceRegisterGetReportBlock(_userDevice, ^IOReturn(IOHIDReportType type,
                                                                 uint32_t reportID,
                                                                 uint8_t *report,
                                                                 CFIndex *reportLength)
    {
        HIDKeyboardDescriptorOutputReport *outReport = (typeof(outReport))report;
        
        NSData *reportData = [NSData dataWithBytesNoCopy:(void *)report
                                                         length:*reportLength
                                                   freeWhenDone:false];
        NSLog(@"received getReport call type:%d reportID: %d data: %@",
                     type, reportID, reportData);
        
        XCTAssert(type == HIDReportTypeOutput &&
                  *reportLength == sizeof(HIDKeyboardDescriptorOutputReport));
        outReport->LED_KeyboardCapsLock = 1;
        
       
        
        return kIOReturnSuccess;
    });
    
    IOHIDUserDeviceRegisterSetReportBlock(_userDevice, ^IOReturn(IOHIDReportType type,
                                                                 uint32_t reportID,
                                                                 const uint8_t *report,
                                                                 CFIndex reportLength)
    {
        HIDKeyboardDescriptorOutputReport *outReport = (typeof(outReport))report;
        
        
        
        NSData *reportData = [NSData dataWithBytesNoCopy:(void *)report
                                                  length:reportLength
                                            freeWhenDone:false];
        NSLog(@"received setReport call type:%d reportID: %d data: %@",
        type, reportID, reportData);
        // with changes in 57612353 , backboardd attempt to set 0 value won't be guarded
        if (type == HIDReportTypeOutput && reportLength == sizeof(HIDKeyboardDescriptorOutputReport) && outReport->LED_KeyboardCapsLock == 1) {
            [_ledSetExp fulfill];
        }
        
        return kIOReturnSuccess;
    });
    
    IOHIDUserDeviceSetDispatchQueue(_userDevice, udQueue);
    
    IOHIDUserDeviceSetCancelHandler(_userDevice, ^{
        [_cancelExp fulfill];
        NSLog(@"user device cancelled");
    });
    
    IOHIDUserDeviceActivate(_userDevice);
}

- (void)setupDevice
{
    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid };
    _manager = [HIDManager new];
    
    [_manager setDeviceMatching:matching];
    [_manager setDispatchQueue:dispatch_get_main_queue()];
    [_manager setDeviceNotificationHandler:^(HIDDevice *device, BOOL added) {
        NSLog(@"device %s:\n%@", added ? "added" : "removed", device);
        if (added) {
            _device = device;
            [_devAddedExp fulfill];
        } else {
            [_devRemovedExp fulfill];
        }
    }];
    [_manager setInputReportHandler:^(HIDDevice *sender,
                                      uint64_t timestamp,
                                      HIDReportType type,
                                      NSInteger reportID,
                                      NSData *report) {
        HIDKeyboardDescriptorInputReport *kbdReport = (typeof(kbdReport))report.bytes;
        NSLog(@"received input report ts: %llu type: %ld reportID: %ld report: %@\n%@",
              timestamp, type, reportID, report, sender);
        
        if (kbdReport->KB_Keyboard[0] == kHIDUsage_KeyboardLeftShift) {
            [_reportExp fulfill];
        }
    }];
    [_manager open];
    [_manager activate];
}

- (void)setUp
{
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _devRemovedExp = [[XCTestExpectation alloc] initWithDescription:@"device removed"];
    _servAddedExp = [[XCTestExpectation alloc] initWithDescription:@"service added"];
    _servRemovedExp = [[XCTestExpectation alloc] initWithDescription:@"service removed"];
    _cancelExp = [[XCTestExpectation alloc] initWithDescription:@"user device cancelled"];
    _reportExp = [[XCTestExpectation alloc] initWithDescription:@"report exp"];
    _eventExp = [[XCTestExpectation alloc] initWithDescription:@"event exp"];
    _ledSetExp = [[XCTestExpectation alloc] initWithDescription:@"led set exp"];
    
    [self setupClient];
    [self setupUserDevice];
    [self setupDevice];
}

- (void)tearDown
{
    [_client cancel];
    [_manager close];
    [_manager cancel];
    [super tearDown];
}

- (void)testHIDUserDevice {
    HIDKeyboardDescriptorInputReport inReport = { 0 };
    IOReturn ret;
    bool res;
    XCTWaiterResult result;
    NSError *err;
    
    // test enumeration
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    result = [XCTWaiter waitForExpectations:@[_servAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    // test set/get property
    res = IOHIDUserDeviceSetProperty(_userDevice,
                                     CFSTR("TestProp"),
                                     kCFBooleanTrue);
    XCTAssert(res);
    
    CFBooleanRef prop = (CFBooleanRef)IOHIDUserDeviceCopyProperty(_userDevice,
                                                                  CFSTR("TestProp"));
    XCTAssert(prop && prop == kCFBooleanTrue);
    
    // test input report
    inReport.KB_Keyboard[0] = kHIDUsage_KeyboardLeftShift;
    
    ret = IOHIDUserDeviceHandleReportWithTimeStamp(_userDevice,
                                                   mach_absolute_time(),
                                                   (uint8_t *)&inReport,
                                                   sizeof(inReport));
    XCTAssert(ret == kIOReturnSuccess);
    
    result = [XCTWaiter waitForExpectations:@[_reportExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted);
    
    result = [XCTWaiter waitForExpectations:@[_eventExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted);
    
    // test set/getReport
    HIDKeyboardDescriptorOutputReport outReport = { 0 };
    outReport.LED_KeyboardCapsLock = 1;
    NSMutableData *reportData = [[NSMutableData alloc] initWithBytes:&outReport
                                                              length:sizeof(outReport)];
    
    res = [_device setReport:reportData.bytes
                reportLength:reportData.length
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:&err];
    XCTAssert(res, "setReport failed ret: %d err: %@", res, err);
    
    result = [XCTWaiter waitForExpectations:@[_ledSetExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted);
    
    ((HIDKeyboardDescriptorOutputReport *)reportData.mutableBytes)->LED_KeyboardCapsLock = 0;
    NSInteger reportSize = sizeof(HIDKeyboardDescriptorOutputReport);
    res = [_device getReport:reportData.mutableBytes
                reportLength:&reportSize
              withIdentifier:0
                     forType:HIDReportTypeOutput
                       error:nil];
    XCTAssert(res && ((HIDKeyboardDescriptorOutputReport *)reportData.mutableBytes)->LED_KeyboardCapsLock == 1,
              "getReport failed ret: %d report: %@", ret, reportData);
    
    // test cancellation
    IOHIDUserDeviceCancel(_userDevice);
    
    result = [XCTWaiter waitForExpectations:@[_cancelExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted);
    
    // test termination
    result = [XCTWaiter waitForExpectations:@[_devRemovedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    result = [XCTWaiter waitForExpectations:@[_servRemovedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
}

@end
