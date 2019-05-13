//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDGyroDescriptor.h"

@interface TestIOHIDEventDriver_Gyro : IOHIDEventDriverTestCase


@property XCTestExpectation                 * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_Gyro

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: Gyro event"];
    self.testEventExpectation.expectedFulfillmentCount = 6;
    
    static uint8_t descriptor [] = {
        HIDGyro
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testGyroEvent {
    IOReturn        status;
    
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testServiceExpectation);

    HIDGyroInputReport01 report;

    memset(&report, 0, sizeof(report));
 
    report.reportId = 1;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutXAxis = 1000;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutYAxis = 1000;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutZAxis = 1000;

    report.MotionGyrometer000B = 1;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.MotionGyrometer000B = 2;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    
    report.MotionGyrometer000B = 3;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutZAxis = 2000;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.MotionGyrometer000B = 4;
    report.MotionGyrometer0009 = kIOHIDMotionTypeShake;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutZAxis = -1000;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

 
    report.MotionGyrometer000B = 5;
    report.MotionGyrometer0009 = kIOHIDMotionTypePath;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutZAxis = -1000;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.MotionGyrometer000B = 6;
    report.MotionGyrometer0009 = kIOHIDMotionTypePath;
    report.MotionGyrometer000A = kIOHIDMotionPathEnd;
    report.SNS_MotionGyrometerDataFieldAngularVelocityAboutZAxis = -1000;
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

    IOHIDFloat value0;
    IOHIDFloat value1;

    value0 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGyroX);
    value1 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGyroX);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                value0 == value1 && value0 != 0,
                                "events:%@",
                                self.events);

    value0 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGyroY);
    value1 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGyroY);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                value0 == value1 && value0 != 0,
                                "events:%@",
                                self.events);

    value0 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGyroZ);
    value1 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGyroZ);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                value0 == value1 && value0 != 0,
                                "events:%@",
                                self.events);

    value0 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldGyroZ);
    value1 = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[3], kIOHIDEventFieldGyroZ);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                value0 != value1 && value1 < 0,
                                "events:%@",
                                self.events);

    for (NSInteger index = 0; index < self.events.count; index++) {
        NSInteger sequence = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[index], kIOHIDEventFieldGyroSequence);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                    sequence == index + 1,
                                    "index:%ld events:%@",
                                    (long)index,
                                    self.events);

        NSInteger expectedType;
        NSInteger type = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[index], kIOHIDEventFieldGyroType);
        if (index < 3) {
            expectedType = 0;
        } else if (index < 4) {
            expectedType = kIOHIDMotionTypeShake;
        } else {
            expectedType = kIOHIDMotionTypePath;
        }
        HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                    type == expectedType,
                                    "type:%ld (%ld) index:%ld events:%@",
                                    (long)type,
                                    (long)expectedType,
                                    (long)index,
                                    self.events);

        NSInteger expectedSubType;
        NSInteger subType = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[index], kIOHIDEventFieldGyroSubType);
        if (index < 5) {
            expectedSubType = 0;
        } else {
            expectedSubType = 1;
        }
        HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                    subType == expectedSubType,
                                    "index:%ld events:%@",
                                    (long)index,
                                    self.events);
    }
    
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeGyro) {
        [self.testEventExpectation fulfill];
    }
}

@end
