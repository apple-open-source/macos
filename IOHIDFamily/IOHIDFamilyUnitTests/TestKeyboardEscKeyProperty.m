//
//  TestKeyboardEscKeyProperty.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 10/3/18.
//

#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDKeys.h>


static uint8_t descriptor [] = {
    HIDKeyboardDescriptor
};

@interface TestKeyboardEscKeyProperty : XCTestCase

@property HIDUserDevice             * userDevice;
@property XCTestExpectation         * eventSystemClientCancelExpectation;
@property XCTestExpectation         * testServiceExpectation;
@property HIDEventSystemClient      * hidEventSystemClient;
@property HIDServiceClient          * hidServiceClient;

@end

@implementation TestKeyboardEscKeyProperty

- (void)setUp {
    
    [super setUp];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDProductIDKey  : @(637),
                                   @kIOHIDVendorIDKey   : @(kIOHIDAppleVendorID),
    };
    
    self.eventSystemClientCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: event System Client cancel"];
    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation : user device service"];
    
    self.hidEventSystemClient  = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST , self.hidEventSystemClient != NULL);
    
    [self.hidEventSystemClient setMatching:@{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID}];
    
    [self.hidEventSystemClient setDispatchQueue:dispatch_get_main_queue()];
    
    __weak TestKeyboardEscKeyProperty * weakSelf = self;
    
    [self.hidEventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
        __strong TestKeyboardEscKeyProperty *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        strongSelf.hidServiceClient = service;
        [strongSelf.testServiceExpectation fulfill];
    }];
    
    [self.hidEventSystemClient setCancelHandler:^{
        
        __strong TestKeyboardEscKeyProperty *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        [strongSelf.eventSystemClientCancelExpectation fulfill];
    }];
    
    [self.hidEventSystemClient activate];
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties:description];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG, self.userDevice != NULL);
}

- (void)tearDown {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    [self.hidEventSystemClient cancel];
    
    result = [XCTWaiter waitForExpectations:@[self.eventSystemClientCancelExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.eventSystemClientCancelExpectation]);
    
    [super tearDown];
}

- (void)testKeyboardEscKeyProperty {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.testServiceExpectation]);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG, [[self.hidServiceClient propertyForKey:@kIOHIDKeyboardSupportsEscKey] boolValue] == NO, "@kIOHIDKeyboardSupportsEscKey true");
    
}



@end
