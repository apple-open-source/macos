//
//  TestServiceTerminationEvents.m
//  IOHIDFamily
//
//  Created by YG on 10/25/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDUserDevice.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>

#define fulfill(exp) \
    exp.expectationDescription = [exp.expectationDescription stringByAppendingString:@"(fullfilled)"];\
    [exp fulfill];

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestServiceTerminationEvents : XCTestCase

@property IOHIDEventSystemClientRef         eventSystem;
@property IOHIDUserDeviceRef                userDevice;

@property IOHIDXCTestExpectation            * testServiceExpectation;
@property IOHIDXCTestExpectation            * testKbcDownEventExpectation;
@property IOHIDXCTestExpectation            * testKbcUpEventExpectation;
@property IOHIDXCTestExpectation            * testNullEventExpectation;

@property NSMutableArray                    * events;

@end

@implementation TestServiceTerminationEvents

- (void)setUp {
    [super setUp];

    self.events = [[NSMutableArray alloc] init];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];

    self.eventSystem = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertAndThrowTrue(self.eventSystem != NULL);

    NSDictionary *matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID};
    
    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);
    
    self.testServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Test HID service"];

    self.testKbcDownEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: keyboard event down"];

    self.testKbcUpEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Keyboard event Up"];

    self.testNullEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Null event"];
    
    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
        [self.testServiceExpectation fulfill];
    };
    
    IOHIDEventSystemClientRegisterDeviceMatchingBlock(self.eventSystem , handler, NULL, NULL);
    
    IOHIDEventSystemClientRegisterEventBlock(self.eventSystem, ^(void * _Nullable target __unused, void * _Nullable refcon __unused, void * _Nullable sender __unused, IOHIDEventRef  _Nonnull event) {
        NSLog(@"Event: %@", event);
        if (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage) == kHIDUsage_KeypadEqualSignAS400) {
            if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)) {
                [self.testKbcDownEventExpectation fulfill];
            } else {
                [self.testKbcUpEventExpectation fulfill];
            }
        } else if (IOHIDEventGetType(event) == kIOHIDEventTypeNULL) {
             [self.testNullEventExpectation fulfill];
        } else {
            [self.events addObject: (__bridge id _Nonnull)event];
        }
    }, NULL,  NULL);
    
    
    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystem, dispatch_get_main_queue());
    
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   };
    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)description);
    HIDXCTAssertAndThrowTrue(self.userDevice != NULL, "HID User Device config:%@", description);
    
}

- (void)tearDown {
  
    if (self.userDevice) {
        CFRelease(self.userDevice);
    }

    if (self.eventSystem) {
        CFRelease(self.eventSystem);
    }

    [super tearDown];
}

- (void)testKeyboardServiceTerminationEvent {
    XCTWaiterResult result;
    IOReturn        status;
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    XCTAssert(result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testServiceExpectation);
    
    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));
    
    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    XCTAssert(status == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", status);

    result = [XCTWaiter waitForExpectations:@[self.testKbcDownEventExpectation] timeout:10];
    XCTAssert(result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testKbcDownEventExpectation);
    
    CFRelease(self.userDevice);
    self.userDevice = NULL;
    
    result = [XCTWaiter waitForExpectations:@[self.testKbcUpEventExpectation, self.testNullEventExpectation] timeout:10];
    XCTAssert(result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, @[self.testKbcUpEventExpectation, self.testNullEventExpectation]);
    
    XCTAssert (self.events.count == 0, "Unexpected events:%@", self.events);

}

@end
