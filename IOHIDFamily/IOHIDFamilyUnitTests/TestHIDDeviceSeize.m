//
//  TestHIDDeviceSeize.h
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/30/18.
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "HID.h"
#import "HIDDevicePrivate.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/IOKitLib.h>
#import <IOKit/hid/IOHIDEventSystemKeys.h>
#import <IOKit/hid/IOHIDServiceKeys.h>
#import "IOHIDUnitTestDescriptors.h"

#pragma clang diagnostic ignored "-Wunused-parameter"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestHIDDeviceSeize : XCTestCase

@property HIDUserDevice             * userDevice;
@property HIDDevice                 * device;
@property HIDEventSystemClient      * eventSystem;
@property HIDServiceClient          * eventService;
@property XCTestExpectation         * testCancelExpectation;
@property XCTestExpectation         * testPreSeizeDownEventsExpectation;
@property XCTestExpectation         * testPostSeizeDownEventsExpectation;
@property XCTestExpectation         * testUpEventsExpectation;
@property XCTestExpectation         * testServiceExpectation;

@property NSMutableArray            * events;
@end

@implementation TestHIDDeviceSeize

- (void)setUp {
   
    [super setUp];

    __weak TestHIDDeviceSeize * self_ = self;

    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   (NSString *)kIOHIDServiceHiddenKey : @YES
                                   };

    
    self.testCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: device cancel"];
    self.testCancelExpectation.expectedFulfillmentCount = 3;

    self.testPreSeizeDownEventsExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: pre seize down events"];
    self.testPreSeizeDownEventsExpectation.expectedFulfillmentCount = 2;

    self.testPostSeizeDownEventsExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: post seize down events"];

    
    self.testUpEventsExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: up events"];
    self.testUpEventsExpectation.expectedFulfillmentCount = 2;

    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription:@"expectation: service"];

    self.eventSystem = [[HIDEventSystemClient alloc] initWithType:(HIDEventSystemClientTypeMonitor)];
    
    [self.eventSystem setDispatchQueue:dispatch_get_main_queue()];
    
    [self.eventSystem setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        self_.eventService = service;
        [self_.testServiceExpectation fulfill];
    }];
    
    [self.eventSystem setEventHandler:^(HIDServiceClient * _Nullable service, HIDEvent * _Nonnull event) {
        NSLog(@"Event: %@", event);
        [self_.events addObject:event];
        
        if (event.type == kIOHIDEventTypeKeyboard && event  ) {
            XCTestExpectation * downExectation = [event integerValueForField:kIOHIDEventFieldKeyboardUsage] != kHIDUsage_KeyboardPower ? self_.testPreSeizeDownEventsExpectation : self_.testPostSeizeDownEventsExpectation;
            if ([event integerValueForField:kIOHIDEventFieldKeyboardDown] != 0) {
                [downExectation fulfill];
            } else {
                [self_.testUpEventsExpectation fulfill];
            }
        }
    }];
    
    [self.eventSystem setCancelHandler:^{
        [self_.testCancelExpectation fulfill];
    }];

    NSDictionary *matching = @{
                               @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                               @"Hidden" : @"*"
                               };
    
    [self.eventSystem setMatching:matching];

    [self.eventSystem activate];
    
    self.userDevice = [[HIDUserDevice alloc] initWithProperties:description];
    if (!self.userDevice) {
        return;
    }
    
    [self.userDevice setDispatchQueue:dispatch_get_main_queue()];

    
    [self.userDevice setCancelHandler:^{
        [self_.testCancelExpectation fulfill];
    }];

    [self.userDevice activate];
    
    mach_timespec_t waitTime = {30, 0};
    kern_return_t kr = IOServiceWaitQuiet (self.userDevice.service, &waitTime);
    if (kr) {
        NSLog(@"IOServiceWaitQuiet:0x%x", kr);
        return;
    }

    self.device = [[HIDDevice alloc] initWithService:self.userDevice.service];
    if (!self.device) {
        return;
    }
 
    [self.device setCancelHandler:^{
        [self_.testCancelExpectation fulfill];
    }];

    [self.device setDispatchQueue:dispatch_get_main_queue()];
    
    [self.device activate];
    
}

- (void)tearDown {

    [self.userDevice cancel];

    [self.device close];

    [self.device cancel];

    [self.eventSystem cancel];

    XCTWaiterResult result  = [XCTWaiter waitForExpectations:@[self.testCancelExpectation] timeout:2];
    HIDXCTAssertWithParameters (COLLECT_LOGARCHIVE, result == XCTWaiterResultCompleted, "expectation: %@", self.testCancelExpectation);
    
    [super tearDown];
    
}

- (void)testSeize {
    
    NSError     * error;
    BOOL        ret;
 
    XCTWaiterResult     result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, self.userDevice && self.device, "user device:%@ device:%@", self.userDevice, self.device);

    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:2];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG | COLLECT_TAILSPIN , result == XCTWaiterResultCompleted, "%@ result:%d", self.testServiceExpectation, (int )result);

    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));
    
    report.KB_Keyboard[0] = kHIDUsage_KeyboardMute;
    report.KB_Keyboard[1] = kHIDUsage_KeypadNumLock;
    
  
    ret = [self.userDevice handleReport:[NSData dataWithBytes:&report length:sizeof(report)] error:&error];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG, ret, "error:%@", error);

  
    result = [XCTWaiter waitForExpectations:@[self.testPreSeizeDownEventsExpectation] timeout:5];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@ events:%@",
                                result,
                                self.testPreSeizeDownEventsExpectation,
                                self.events);

    
    ret = [self.device openSeize:&error];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@", error);
    
    NSLog(@"Device seized");
    
    result = [XCTWaiter waitForExpectations:@[self.testUpEventsExpectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@ events:%@",
                                result,
                                self.testUpEventsExpectation,
                                self.events);


    
    report.KB_Keyboard[0] = 0;
    report.KB_Keyboard[1] = 0;

    ret = [self.userDevice handleReport:[NSData dataWithBytes:&report length:sizeof(report)] error:&error];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@", error);

    
    [self.device close];

    NSLog(@"Device closed");

    report.KB_Keyboard[2] = kHIDUsage_KeyboardPower;
    ret = [self.userDevice handleReport:[NSData dataWithBytes:&report length:sizeof(report)] error:&error];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, ret, "error:%@", error);

    result = [XCTWaiter waitForExpectations:@[self.testPostSeizeDownEventsExpectation] timeout:2];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@ events:%@",
                                result,
                                self.testPostSeizeDownEventsExpectation,
                                self.events);

}




@end
