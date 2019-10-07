//
//  HIDDeviceTester.m
//
//
//  Created by dekom on 2/22/19.
//  Copyright © 2019 apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HIDDeviceTester.h"
#import <IOKit/hid/IOHIDKeys.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#include "IOHIDUnitTestUtility.h"
#include <IOKit/IOKitLib.h>

@implementation HIDDeviceTester {
    XCTestExpectation *_serviceExp;
}

- (instancetype)initWithInvocation:(nullable NSInvocation *)invocation
{
    self = [super initWithInvocation:invocation];
    
    if (!self) {
        return self;
    }
    
    _useDevice = true;
    _useClient = true;
    
    _events = [NSMutableArray array];
    _eventExp = [[XCTestExpectation alloc] initWithDescription:@"Event Exp"];
    _reports = [NSMutableArray array];
    _reportExp = [[XCTestExpectation alloc] initWithDescription:@"Report Exp"];
    _properties = [NSMutableDictionary dictionary];
    _uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    return self;
}

- (void)setupUserDevice
{
    __weak HIDDeviceTester *self_ = self;
    
    assert(_descriptor);
    
    _properties[@kIOHIDReportDescriptorKey] = _descriptor;
    _properties[@kIOHIDPhysicalDeviceUniqueIDKey] = _uniqueID;
    _properties[(NSString *)kIOHIDServiceHiddenKey] = @YES;
    
    NSLog(@"Device properties:\n%@", _properties);
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:_properties];
    
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType type,
                                               NSInteger reportID,
                                               void *report,
                                               NSInteger *reportLength) {
        IOReturn ret = kIOReturnError;
        __strong HIDDeviceTester *strongSelf = self_;
        if (!strongSelf) {
            return kIOReturnOffline;
        }
        
        NSMutableData *reportData = [NSMutableData dataWithBytesNoCopy:report
                                                                length:*reportLength
                                                          freeWhenDone:false];
        
        ret = [strongSelf handleGetReport:reportData type:type reportID:reportID];
        
        *reportLength = reportData.length;
        return ret;
    }];
    
    [_userDevice setSetReportHandler:^IOReturn(HIDReportType type,
                                               NSInteger reportID,
                                               const void *report,
                                               NSInteger reportLength) {
        __strong HIDDeviceTester *strongSelf = self_;
        if (!strongSelf) {
            return kIOReturnOffline;
        }
        
        NSData *reportData = [NSData dataWithBytes:report length:reportLength];
        
        return [strongSelf handleSetReport:reportData type:type reportID:reportID];
    }];
    
    // use separate queue since device will run on main queue
    dispatch_queue_t q = dispatch_queue_create("", DISPATCH_QUEUE_SERIAL);
    [_userDevice setDispatchQueue:q];
    
    [_userDevice activate];
    
    mach_timespec_t waitTime = {30, 0};
    IOServiceWaitQuiet(_userDevice.service, &waitTime);
}

- (void)setupDevice
{
    __weak HIDDeviceTester *self_ = self;
    
    _device = [[HIDDevice alloc] initWithService:_userDevice.service];
    
    [_device open];
    
    [_device setInputReportHandler:^(HIDDevice *sender,
                                     uint64_t timestamp,
                                     HIDReportType type,
                                     NSInteger reportID,
                                     NSData *report) {
        __strong HIDDeviceTester *strongSelf = self_;
        if (!strongSelf) {
            return;
        }
        
        [strongSelf handleInputReport:report
                            timestamp:timestamp
                                 type:type
                             reportID:reportID
                               device:sender];
    }];
    
    [_device setDispatchQueue:dispatch_get_main_queue()];
    
    [_device activate];
}

- (void)serviceAdded:(HIDServiceClient *)service
{
    __weak HIDServiceClient *service_ = service;
    
    if (_service) {
        return;
    }
    
    NSLog(@"Service added:\n%@", service);
    
    [service setRemovalHandler:^{
        NSLog(@"Service removed:\n%@", service_);
    }];
    
    _service = service;
    [_serviceExp fulfill];
}

- (void)setupClient
{
    XCTWaiterResult result;
    __weak HIDDeviceTester *self_ = self;
    
    _serviceExp = [[XCTestExpectation alloc] initWithDescription:@"Service Exp"];
    
    _client = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    [_client setMatching:@{ @kIOHIDPhysicalDeviceUniqueIDKey : _uniqueID,
                            (NSString *)kIOHIDServiceHiddenKey : @YES }];
    
    [_client setServiceNotificationHandler:^(HIDServiceClient *service) {
        __strong HIDDeviceTester *strongSelf = self_;
        if (!strongSelf) {
            return;
        }
        
        [strongSelf serviceAdded:service];
    }];
    
    [_client setEventHandler:^(HIDServiceClient *service, HIDEvent *event) {
        __strong HIDDeviceTester *strongSelf = self_;
        if (!strongSelf) {
            return;
        }
        
        [strongSelf handleEvent:event forService:service];
    }];
    
    [_client setDispatchQueue:dispatch_get_main_queue()];
    [_client activate];
    
    for (HIDServiceClient *service in [_client services]) {
        [self serviceAdded:service];
    }
    
    result = [XCTWaiter waitForExpectations:@[_serviceExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               _serviceExp);
}

- (void)setUp
{
    if (!_userDevice) {
        [self setupUserDevice];
    }
    
    if (!_device && _useDevice) {
        [self setupDevice];
    }
    
    if (!_client && _useClient) {
        [self setupClient];
    }
    
    [super setUp];
}

- (void)tearDown
{
    if (_client) {
        [_client cancel];
    }
    
    if (_device) {
        [_device close];
        [_device cancel];
    }
    
    if (_userDevice) {
        [_userDevice cancel];
    }
    
    [super tearDown];
}

- (IOReturn)handleGetReport:(NSMutableData * __unused)report
                       type:(HIDReportType __unused)type
                   reportID:(NSInteger __unused)reportID
{
    return kIOReturnUnsupported;
}

- (IOReturn)handleSetReport:(NSData * __unused)report
                       type:(HIDReportType __unused)type
                   reportID:(NSInteger __unused)reportID
{
    return kIOReturnUnsupported;
}

- (void)handleInputReport:(NSData *)report
                timestamp:(uint64_t __unused)timestamp
                     type:(HIDReportType __unused)type
                 reportID:(NSInteger __unused)reportID
                   device:(HIDDevice * __unused)device
{
    NSLog(@"received report:\n%@", report);
    
    [_reports addObject:report];
    
    if (_reportExp) {
        [_reportExp fulfill];
    }
}

- (void)handleEvent:(HIDEvent *)event
         forService:(HIDServiceClient * __unused)service
{
    NSLog(@"received event:\n%@", event);
    
    [_events addObject:event];
    
    if (_eventExp) {
        [_eventExp fulfill];
    }
}

@end
