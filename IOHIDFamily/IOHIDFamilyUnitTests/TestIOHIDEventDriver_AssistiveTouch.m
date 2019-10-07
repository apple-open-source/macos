//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDAssistiveTouchDescriptor.h"

@interface TestIOHIDEventDriver_AssistiveTouch : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_AssistiveTouch

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: events"];
    
    self.testEventExpectation.expectedFulfillmentCount = 3;
    
    static uint8_t descriptor [] = {
        HIDAssistiveTouch
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testAssistiveSwitchControl {
    
    IOReturn        status;
    
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    HIDAssistiveTouchInputReport report;

    memset(&report, 0x00, sizeof(report));
 
    report.BTN_MousePointerButton1 = 1;
    report.BTN_MousePointerButton2 = 1;
    report.GD_MousePointerX        = 8;
    report.GD_MousePointerY        = 8;

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.BTN_MousePointerButton1 = 0;
 
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.GD_MousePointerY = 0;
    
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    
    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testEventExpectation);

    IOHIDFloat x = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldPointerX);
    IOHIDFloat y = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldPointerY);
    NSInteger  mask = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldPointerButtonMask);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                (x == 8.0 && y == 8.0 && mask == 0x3),
                                "x:%f y:%f mask:%x events:%@",
                                x,
                                y,
                                (int)mask,
                                self.events);

    
    x = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldPointerX);
    y = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldPointerY);
    mask = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldPointerButtonMask);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                (x == 8.0 && y == 8.0 && mask == 0x2),
                                "x:%f y:%f mask:%x events:%@",
                                x,
                                y,
                                (int)mask,
                                self.events);

    x = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldPointerX);
    y = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldPointerY);
    mask = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldPointerButtonMask);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                (x == 8.0 && y == 0.0 && mask == 0x2),
                                "x:%f y:%f mask:%x events:%@",
                                x,
                                y,
                                (int)mask,
                                self.events);
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypePointer) {
        [self.testEventExpectation fulfill];
    }
}

@end
