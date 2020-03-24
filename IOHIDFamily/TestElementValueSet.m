//
//  TestElementValueSet.m
//  IOHIDFamilyUnitTests
//
//  Created by abhishek on 1/2/20.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import <HID/HID.h>
#import <AssertMacros.h>
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventData.h>
#import <HID/HIDManager.h>
#import <HID/HIDUserDevice.h>
#import <IOKit/hid/IOHIDEventSystemClient.h>


/*! Test Objective : Problems like 57612353, Inital attempt shouldn't be guarded
 */

static XCTestExpectation *_devAddedExp;
static XCTestExpectation *_devReportCallback;

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestElementValueSet : XCTestCase {
     HIDUserDevice           *_userDevice;
     NSString                *_uuid;
     IOHIDServiceClientRef   _serviceClient;
     IOHIDEventSystemClientRef _eventSystemClient;
}

@end

@implementation TestElementValueSet

- (void)setUp {
    [super setUp];
    
    _uuid = [[[NSUUID alloc] init] UUIDString];
    _devAddedExp = [[XCTestExpectation alloc] initWithDescription:@"device added"];
    _devReportCallback = [[XCTestExpectation alloc] initWithDescription:@"device report"];
    
    [self createHIDEventSystem];
    [self createUserDevice];
}

- (void)tearDown {
    [super tearDown];
    
    if (_userDevice) {
        [_userDevice cancel];
    }
    
    if (_eventSystemClient) {
        IOHIDEventSystemClientCancel(_eventSystemClient);
    }
}

-(void) createUserDevice {
    
    
    NSData *desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    NSDictionary *properties = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid,
                                  @kIOHIDReportDescriptorKey : desc };
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _userDevice != NULL);
    
    [_userDevice setDispatchQueue:dispatch_queue_create("com.apple.HID.UserDeviceTest", nil)];
    
    
    [_userDevice setSetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, const void * _Nonnull report __unused, NSInteger reportLength __unused) {
        NSLog(@"Set Report Type  : %d ReportID : %ld ", (int)type, reportID);
        
        [_devReportCallback fulfill];
        
        
        return kIOReturnSuccess;
    }];
    
    [_userDevice activate];
    
}

-(void) createHIDEventSystem {
    
    __weak TestElementValueSet *weakSelf = self;
    
    NSDictionary *matching = @{ @kIOHIDPhysicalDeviceUniqueIDKey : _uuid};
    
    _eventSystemClient = IOHIDEventSystemClientCreate(kCFAllocatorDefault);
    
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
    _eventSystemClient != NULL);
    
    IOHIDEventSystemClientRegisterDeviceMatchingBlock(_eventSystemClient, ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service) {
        
        __strong TestElementValueSet *strongSelf = weakSelf;
        if (!strongSelf) return;
        
        NSLog(@"Added %@",service);
        strongSelf->_serviceClient = service;
        [_devAddedExp fulfill];
        
    }, NULL, NULL);
    
    
    IOHIDEventSystemClientSetMatching(_eventSystemClient, (__bridge CFDictionaryRef)matching);
    
    IOHIDEventSystemClientSetDispatchQueue(_eventSystemClient, dispatch_queue_create("com.apple.HID.Test", nil));
    
    IOHIDEventSystemClientActivate(_eventSystemClient);
    
}


- (void)testElementValueSet {
   
    
    XCTWaiterResult result;
    IOReturn kr = kIOReturnError;
    
    result = [XCTWaiter waitForExpectations:@[_devAddedExp] timeout:35];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                  result == XCTWaiterResultCompleted);
    
    
    kr = IOHIDServiceClientSetElementValue(_serviceClient, kHIDPage_LEDs, kHIDUsage_LED_CapsLock, 0);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
    kr == kIOReturnSuccess);
    
    result = [XCTWaiter waitForExpectations:@[_devReportCallback] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                                  result == XCTWaiterResultCompleted);
    
}


@end
