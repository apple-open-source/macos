//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDDigitizerFlagsDescriptor.h"

@interface TestIOHIDEventDriver_DigitizerFlags : IOHIDEventDriverTestCase


@property XCTestExpectation                 * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_DigitizerFlags

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testEventExpectation.expectedFulfillmentCount = 11;
    
    static uint8_t descriptor [] = {
        HIDDigitizerFlags
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testDigitizerTablet {
    IOReturn        status;
    
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%d %@",
                                (int)result,
                                self.testServiceExpectation);

    HIDDigitizerFlagsInputReport report;

    memset(&report, 0, sizeof(report));
 
    report.DIG_TouchPadFingerTouch   = 0x1;
    report.DIG_TouchPadFingerInRange = 0x1;
    report.DIG_TouchPadFingerTransducerIndex = 1;
    report.GD_TouchPadFingerX = 0;
  
    for (NSUInteger index = 0; index < 10; index++) {
        status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
        HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                    status == kIOReturnSuccess,
                                    "IOHIDUserDeviceHandleReport:0x%x",
                                    status);
        report.GD_TouchPadFingerX = index;
    }

    report.DIG_TouchPadFingerTouch   = 0x0;
    report.DIG_TouchPadFingerInRange = 0x0;

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    
    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "result:%d %@",
                                (int)result,
                                self.testEventExpectation);

    NSUInteger touchChanged = 0;
    NSUInteger rangeChanged = 0;

    for (id event in self.events) {
        NSUInteger value = IOHIDEventGetIntegerValue((IOHIDEventRef)event, kIOHIDEventFieldDigitizerEventMask);
        if (value & kIOHIDDigitizerEventTouch) {
            ++touchChanged;
        }
        if (value & kIOHIDDigitizerEventRange) {
            ++rangeChanged;
        }
    }
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,touchChanged == 2 &&  rangeChanged == 2, "touchChanged:%lu  rangeChanged:%lu events:%@", (unsigned long)touchChanged, (unsigned long)rangeChanged, self.events);
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeDigitizer) {
        [self.testEventExpectation fulfill];
    }
}

@end
