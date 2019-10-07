//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDAssistiveSwitchControlDescriptor.h"

@interface TestIOHIDEventDriver_AssistiveSwitchControl : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_AssistiveSwitchControl

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: events"];
    
    self.testEventExpectation.expectedFulfillmentCount = 16 * 2;
    
    static uint8_t descriptor [] = {
        HIDAssistiveSwitchControl
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

    HIDAssistiveSwitchControlInputReport report;

    memset(&report, 0xff, sizeof(report));
 
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    memset(&report, 0x00, sizeof(report));
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


    uint16 buttonDownMask = 0;
    uint16 buttonUpMask = 0;

    for (NSInteger index = 0; index < self.events.count; index++) {

        HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                    IOHIDEventGetType((IOHIDEventRef)self.events[index]) == kIOHIDEventTypeButton,
                                    "index:%ld events:%@",
                                    (long)index,
                                    self.events);

        
        NSInteger state =  IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[index], kIOHIDEventFieldButtonState);
        NSInteger number = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[index], kIOHIDEventFieldButtonNumber);
        if (state) {
            HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                        (buttonDownMask & (1 << (number - 1))) == 0,
                                        "index:%ld events:%@",
                                        (long)index,
                                        self.events);
            buttonDownMask |= (1 << (number - 1));
        } else {
            HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                        (buttonUpMask & (1 << (number - 1))) == 0,
                                        "index:%ld events:%@",
                                        (long)index,
                                        self.events);
            buttonUpMask |= (1 << (number - 1));
        }
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                    buttonUpMask == buttonDownMask,
                                    "buttonUpMask:%x buttonDownMask:%x events:%@",
                                    buttonUpMask,
                                    buttonDownMask,
                                    self.events);
    }
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeButton) {
        [self.testEventExpectation fulfill];
    }
}

@end
