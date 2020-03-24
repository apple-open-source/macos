//
//  TestDigitizerSurfaceSwitch.m
//  IOHIDFamilyUnitTests
//
//  Created by abhishek on 12/11/19.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import <HID/HID.h>
#import <AssertMacros.h>
#import "IOHIDKeyboardWithTouchpadDescriptor.h"
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventData.h>


/** Test Objective : Test surface switch property
 */

static XCTestExpectation *_devAddedExp;
static XCTestExpectation *_userDeviceCancelExp;
static XCTestExpectation *_eventSystemCancelExp;
static XCTestExpectation *_surfaceSwitchPropertyCallbackExp;

static uint8_t descriptor[] = {
    HIDTouchPadKeyboard
};

@interface TestDigitizerSurfaceSwitch : XCTestCase {
    HIDUserDevice           *_userDevice;
    HIDEventSystemClient    *_eventSystemClient;
    HIDServiceClient        *_serviceClient;
    NSString                *_uuid;
}

@end

@implementation TestDigitizerSurfaceSwitch

-(void) createUserDevice {
    
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _userDevice != NULL);
    
    [_userDevice setDispatchQueue:dispatch_queue_create("com.apple.HID.UserDeviceTest", nil)];
    [_userDevice setCancelHandler:^{
        
        [_userDeviceCancelExp fulfill];
    }];
    
    [_userDevice setSetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, const void * _Nonnull report __unused, NSInteger reportLength __unused) {
        NSLog(@"Set Report Type  : %d ReportID : %ld ", (int)type, reportID);
        
        if (reportID == 3 && type == HIDReportTypeFeature) {
            [_surfaceSwitchPropertyCallbackExp fulfill];
        }
        return kIOReturnSuccess;
    }];
    
    [_userDevice activate];
    
}

-(void) createHIDEventSystemClient {
    
    __weak TestDigitizerSurfaceSwitch *weakSelf = self;
    _eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _eventSystemClient != NULL);
    
    
    [_eventSystemClient setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid}];
    
    [_eventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
         __strong TestDigitizerSurfaceSwitch *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        NSLog(@"Added : %@",service);
        
        strongSelf->_serviceClient = service;
        [_devAddedExp fulfill];
        
        
    }];
    
    [_eventSystemClient setCancelHandler:^{
        [_eventSystemCancelExp fulfill];
    }];
    [_eventSystemClient setDispatchQueue:dispatch_queue_create("com.apple.HID.Test", nil)];
    [_eventSystemClient activate];
    
}

-(void) setUp {
    
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _userDeviceCancelExp = [[XCTestExpectation alloc] initWithDescription:@"user device cancel"];
    _eventSystemCancelExp = [[XCTestExpectation alloc] initWithDescription:@"event system cancel"];
    _surfaceSwitchPropertyCallbackExp = [[XCTestExpectation alloc] initWithDescription:@"surface switch property callback"];
    
    [self createHIDEventSystemClient];
    [self createUserDevice];
}

-(void) tearDown {
    XCTWaiterResult result;
    
    if (_eventSystemClient) {
        [_eventSystemClient cancel];
    }
    
    if (_userDevice) {
        [_userDevice cancel];
    }
    
    result = [XCTWaiter waitForExpectations:@[_userDeviceCancelExp, _eventSystemCancelExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
    result == XCTWaiterResultCompleted);
    
    [super tearDown];
}

-(void) testDigitizerSurfaceSwitch {
    XCTWaiterResult result;
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:5];
       HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                  result == XCTWaiterResultCompleted);
    
    
    [_serviceClient setProperty:@(YES) forKey:@(kIOHIDDigitizerSurfaceSwitchKey)];
    
    
    result = [XCTWaiter waitForExpectations:@[_surfaceSwitchPropertyCallbackExp] timeout:5];
          HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                     result == XCTWaiterResultCompleted);
}


@end
