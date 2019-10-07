//
//  TestIOHIDNonVirtualUserDevice.m
//  IOHIDFamilyUnitTests
//
//  Created by AB on 9/10/18.
//

#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import <IOKit/hid/IOHIDEventData.h>

static uint8_t descriptor [] = {
    HIDKeyboardDescriptor
};

@interface TestIOHIDNonVirtualUserDevice : XCTestCase

@property HIDUserDevice             * userDevice;
@property XCTestExpectation         * eventSystemClientCancelExpectation;
@property XCTestExpectation         * testServiceExpectation;
@property HIDEventSystemClient      * hidEventSystemClient;
@property HIDServiceClient          * hidServiceClient;

@end

@implementation TestIOHIDNonVirtualUserDevice

- (void)setUp {
   
    [super setUp];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(1234),
                                   @kIOHIDProductIDKey  : @(1234),
                                   @kIOHIDVirtualHIDevice : @NO,
    };
    
    self.eventSystemClientCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: event System Client cancel"];
    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation : user device service"];
    
    self.hidEventSystemClient  = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.hidEventSystemClient != NULL);
    
    [self.hidEventSystemClient setMatching:@{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID}];
    
    [self.hidEventSystemClient setDispatchQueue:dispatch_get_main_queue()];
    
    __weak TestIOHIDNonVirtualUserDevice * weakSelf = self;
    
    [self.hidEventSystemClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        
        __strong TestIOHIDNonVirtualUserDevice *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        
        strongSelf.hidServiceClient = service;
        [strongSelf.testServiceExpectation fulfill];
    }];
    
    [self.hidEventSystemClient setCancelHandler:^{
        
        __strong TestIOHIDNonVirtualUserDevice *strongSelf = weakSelf;
        if (!strongSelf) {
            return;
        }
        [strongSelf.eventSystemClientCancelExpectation fulfill];
    }];
    
    [self.hidEventSystemClient activate];
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties:description];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL);
    
}

- (void)tearDown {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    [self.hidEventSystemClient cancel];
    
    result = [XCTWaiter waitForExpectations:@[self.eventSystemClientCancelExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.eventSystemClientCancelExpectation]);
    
    [super tearDown];
    
}

// Test creation of non virtual user device
// This seperates detection of physical device which can be virtual (eg bluetooth device)
// from true virtual device
- (void)testHIDNonVirtualUserDevice {
    
    XCTWaiterResult result = XCTWaiterResultCompleted;
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:3];
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, result == XCTWaiterResultCompleted, "%@", @[self.testServiceExpectation]);
    
    
    // Test : Check property
    
     HIDXCTAssertWithParameters (RETURN_FROM_TEST, [[self.hidServiceClient propertyForKey:@kIOHIDVirtualHIDevice] boolValue] == NO, "kIOHIDVirtualHIDevice true");
    
}



@end
