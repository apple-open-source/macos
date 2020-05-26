//
//  TestHIDKeyboardWithTouchPad.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 10/23/19.
//


#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDKeyboardWithTouchpadDescriptor.h"
#import "IOHIDXCTestExpectation.h"
#import <HID/HID.h>
#import <IOKit/hid/IOHIDEventTypes.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEventData.h>

#pragma clang diagnostic ignored "-Wunused-parameter"


static uint8_t descriptor[] = {
    HIDTouchPadKeyboard
};


@interface TestHIDKeyboardPower : XCTestCase

@property HIDUserDevice *           userDevice;
@property XCTestExpectation *       serviceExpectation;
@property XCTestExpectation *       reportExpectation;
@property HIDServiceClient *        service;
@property HIDEventSystemClient *    eventSystemClient;
@property NSInteger                 keyboardPower;
@end

@implementation TestHIDKeyboardPower

-(void) setUp {
    
    [super setUp];
 
    
    __weak TestHIDKeyboardPower * self_ = self;
    
    self.keyboardPower = 1;
    
    self.serviceExpectation = [[XCTestExpectation alloc] initWithDescription:@"service"];
    
    self.reportExpectation = [[XCTestExpectation alloc] initWithDescription:@"report"];
    [self.reportExpectation setExpectedFulfillmentCount:2];
    
    NSString * uuid = [[[NSUUID alloc] init] UUIDString];
    
    NSData * desc = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    
    
    NSDictionary * properties = @{@(kIOHIDVendorIDKey) : @(kIOHIDAppleVendorID),
                                  @(kIOHIDProductIDKey): @(kIOHIDAppleProductID),
                                  @(kIOHIDPhysicalDeviceUniqueIDKey) : uuid,
                                  @(kIOHIDReportDescriptorKey) : desc };
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    
    [self.userDevice setSetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, const void * _Nonnull report, NSInteger reportLength) {
        
        NSLog (@"setSetReportHandler reportID:%d", (int)reportID);
        
        __strong TestHIDKeyboardPower * _self = self_;
        if (!_self){
            return kIOReturnOffline;
        }
        if (reportID == 2) {
            _self.keyboardPower =  *(uint8_t *) (report + 1);
            [_self.reportExpectation fulfill];
            NSLog (@"Set KeyboardPower :%d", (int)_self.keyboardPower);
            return kIOReturnSuccess;
        }
        return kIOReturnUnsupported;
    }];

    [self.userDevice setGetReportHandler:^IOReturn(HIDReportType type, NSInteger reportID, void * _Nonnull report, NSInteger * reportLength) {
        
        NSLog (@"setGetReportHandler reportID:%d", (int)reportID);
        
        __strong TestHIDKeyboardPower * _self = self_;
        if (!_self){
            return kIOReturnOffline;
        }
        if (reportID == 2) {
            *(uint8_t *) report       = 2;
            *(uint8_t *) (report + 1) = (uint8_t) _self.keyboardPower;
            *reportLength = 2;
            NSLog (@"Get KeyboardPower :%d", (int)_self.keyboardPower);
            return kIOReturnSuccess;
        }
        return kIOReturnUnsupported;
    }];

    [self.userDevice setDispatchQueue:dispatch_queue_create("TestHIDKeyboardPower.HIDUserDevice", DISPATCH_QUEUE_SERIAL)];
    
    [self.userDevice activate];
    
    self.eventSystemClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    [self.eventSystemClient setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : uuid}];
     
    [self.eventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        __strong TestHIDKeyboardPower * _self = self_;
        if (!_self){
            return;
        }
         
        NSLog(@"Added : %@",service);
         
        _self.service = service;
        [_self.serviceExpectation fulfill];
     }];
     
     
     [_eventSystemClient setEventHandler:^(HIDServiceClient * _Nullable service , HIDEvent * _Nonnull event) {
         
         __strong TestHIDKeyboardPower * _self = self_;
         if (!_self){
             return;
         }
          
         NSLog(@"Event Value: %@",event);
     }];
     
    [self.eventSystemClient setDispatchQueue:dispatch_get_main_queue()];
    [self.eventSystemClient activate];

}

-(void) tearDown {

    [self.userDevice cancel];
    [self.eventSystemClient cancel];
    
    [super tearDown];
}


-(void) testKeyboardPower {
    
    XCTWaiterResult result;

    result = [XCTWaiter waitForExpectations:@[self.serviceExpectation] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_IOREG, result == XCTWaiterResultCompleted);
 
    NSLog(@"%s:0", kIOHIDKeyboardEnabledKey);
    [self.service setProperty:@(NO) forKey:@(kIOHIDKeyboardEnabledKey)];
    
    NSLog(@"%s:1", kIOHIDKeyboardEnabledKey);
    [self.service setProperty:@(YES) forKey:@(kIOHIDKeyboardEnabledKey)];
    
    result = [XCTWaiter waitForExpectations:@[self.reportExpectation] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", self.reportExpectation);

}

@end

