//
//  TestDigitizerReportRate.m
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


/** Test Objective : Test  digitizer report rate property
 */

static XCTestExpectation *_devAddedExp;
static XCTestExpectation *_userDeviceCancelExp;
static XCTestExpectation *_eventSystemCancelExp;
static XCTestExpectation *_reportRateCallbackExp;

HIDTouchPadKeyboardFeatureReport03 featureReport3;

static uint8_t descriptor[] = {
    HIDTouchPadKeyboard
};

@interface TestDigitizerReportRate : XCTestCase {
    HIDUserDevice           *_userDevice;
    HIDEventSystemClient    *_eventSystemClient;
    HIDServiceClient        *_serviceClient;
    NSString                *_uuid;
}

@end

@implementation TestDigitizerReportRate

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
    
    [_userDevice setGetReportHandler:^IOReturn(HIDReportType type , NSInteger reportID , void * _Nonnull report __unused, NSInteger * _Nonnull reportLength __unused) {
      
        NSLog(@"Get Report Type  : %d ReportID : %ld ", (int)type, reportID);
        
        if (type == HIDReportTypeFeature && reportID == 3) {
            memcpy(report, &featureReport3, sizeof(HIDTouchPadKeyboardFeatureReport03));
            *reportLength = sizeof(HIDTouchPadKeyboardFeatureReport03);
            [_reportRateCallbackExp fulfill];
        }
        return kIOReturnSuccess;
    }];
    
    [_userDevice activate];
    
}

-(void) createHIDEventSystemClient {
    
    __weak TestDigitizerReportRate *weakSelf = self;
    _eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _eventSystemClient != NULL);
    
    
    [_eventSystemClient setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid}];
    
    [_eventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
         __strong TestDigitizerReportRate *strongSelf = weakSelf;
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
    _reportRateCallbackExp = [[XCTestExpectation alloc] initWithDescription:@"report rate call"];
    featureReport3.reportId = 3;
    featureReport3.DIG_TouchPadReportRate = 74;
    featureReport3.DIG_TouchPadSurfaceSwitch = 1;
    
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

-(void) testDigitizerReportRate {
    XCTWaiterResult result;
    id res = nil;
    result = [XCTWaiter waitForExpectations:@[_devAddedExp, _reportRateCallbackExp] timeout:5];
       HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                  result == XCTWaiterResultCompleted);

    
    res = [_serviceClient propertyForKey:@(kIOHIDReportIntervalKey)];
    NSLog(@"%@",res);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
    res && [res isKindOfClass:[NSNumber class]] && ((NSNumber*)res).intValue == 13513); // 1000000/74
    
}


@end
