//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDDeviceOrientationAngularSensorDescriptor.h"

@interface TestIOHIDEventDriver_DeviceOrientationAngularSensor : IOHIDEventDriverTestCase


@property XCTestExpectation                 * testEventExpectation;
@property XCTestExpectation                 * testReportExpectation;
@property XCTestExpectation                 * testCopyEventExpectation;

@end

@implementation TestIOHIDEventDriver_DeviceOrientationAngularSensor

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testEventExpectation.expectedFulfillmentCount = 2;
    
    static uint8_t descriptor [] = {
        HIDAngularSensor
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testDeviceOrientationTiltEvent {
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

    HIDAngularSensorInputReport01 report;

    memset(&report, 0, sizeof(report));
 
    report.reportId = 1;
    report.SNS_OrientationDeviceOrientationDataFieldTiltYAxis = 4550;

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);


    report.SNS_OrientationDeviceOrientationDataFieldTiltYAxis = 9000;

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

    IOHIDFloat y;

    y = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldOrientationTiltY);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                y == 45.5,
                                "y:%f events:%@",
                                y,
                                self.events);

    y = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldOrientationTiltY);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                y == 90.0,
                                "y:%f events:%@",
                                y,
                                self.events);

}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeOrientation  && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypeTilt) {
        [self.testEventExpectation fulfill];
    }
}


@end
