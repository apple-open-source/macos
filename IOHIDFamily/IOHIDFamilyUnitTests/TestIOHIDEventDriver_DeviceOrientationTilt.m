//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDDeviceOrientationTiltDescriptor.h"

@interface TestIOHIDEventDriver_DeviceOrientationTilt : IOHIDEventDriverTestCase


@property XCTestExpectation                 * testEventExpectation;
@property XCTestExpectation                 * testReportExpectation;
@property XCTestExpectation                 * testCopyEventExpectation;

@end

@implementation TestIOHIDEventDriver_DeviceOrientationTilt

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testCopyEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: copy event"];
    self.testReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: report"];
    self.testEventExpectation.expectedFulfillmentCount = 3;
    
    static uint8_t descriptor [] = {
        HIDDisplayOrientationTilt
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
                                (long)result,
                                self.testServiceExpectation);

    HIDDisplayOrientationTiltInputReport01 report;

    memset(&report, 0, sizeof(report));
 
    report.reportId = 1;
    report.SNS_OrientationDeviceOrientationDataFieldTiltXAxis = 180;
    report.SNS_OrientationDeviceOrientationDataFieldTiltYAxis = 90;
    report.SNS_OrientationDeviceOrientationDataFieldTiltZAxis = 270;

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

 
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    
    report.SNS_OrientationDeviceOrientationDataFieldTiltXAxis = 0;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.SNS_OrientationDeviceOrientationDataFieldTiltXAxis = 90;
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

    IOHIDFloat x;
    IOHIDFloat y;
    IOHIDFloat z;

    x = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldOrientationTiltX);
    y = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldOrientationTiltY);
    z = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldOrientationTiltZ);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                x == 180.0 && y == 90.0 && z == 270.0,
                                "x:%f y:%f z:%f events:%@",
                                x,
                                y,
                                z,
                                self.events);

    x = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldOrientationTiltX);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                x == 0.0,
                                "x:%f events:%@",
                                x,
                                self.events);

    x = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldOrientationTiltX);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                x == 90.0,
                                "x:%f events:%@",
                                x,
                                self.events);
    
    
    __block IOHIDEventRef copyEvent;
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        copyEvent = IOHIDServiceClientCopyEvent(self.eventService, kIOHIDEventTypeOrientation, NULL, 0);
        [self.testCopyEventExpectation fulfill];
    });

    result = [XCTWaiter waitForExpectations:@[self.testCopyEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted && copyEvent != NULL,
                                "result:%ld copyEvent:%@ expectation:%@",
                                (long)result,
                                copyEvent,
                                self.testCopyEventExpectation);

    CFIndex value = IOHIDEventGetIntegerValue (copyEvent, kIOHIDEventFieldOrientationOrientationType);
    XCTAssert (value == kIOHIDOrientationTypeTilt);

    x = IOHIDEventGetFloatValue((IOHIDEventRef)copyEvent, kIOHIDEventFieldOrientationTiltX);
    y = IOHIDEventGetFloatValue((IOHIDEventRef)copyEvent, kIOHIDEventFieldOrientationTiltY);
    z = IOHIDEventGetFloatValue((IOHIDEventRef)copyEvent, kIOHIDEventFieldOrientationTiltZ);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                x == 90.0 && y == 180.0 && z == 0.0,
                                "x:%f y:%f z:%f copyEvent:%@",
                                x,
                                y,
                                z,
                                copyEvent);
    if (copyEvent) {
        CFRelease(copyEvent);
    }
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeOrientation  && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypeTilt) {
        [self.testEventExpectation fulfill];
    }
}


-(IOReturn)userDeviceGetReportHandler: (IOHIDReportType)type :(uint32_t)reportID :(uint8_t *)report :(NSUInteger *) length
{
    NSLog(@"userDeviceGetReportHandler:%d :%d :%p :%lu", type, reportID, report, (unsigned long)*length);
    
    if (*length < sizeof (HIDDisplayOrientationTiltInputReport01) || reportID != 1 || type != kIOHIDReportTypeInput) {

        return kIOReturnUnsupported;
    }
    
    HIDDisplayOrientationTiltInputReport01 * reportData = (HIDDisplayOrientationTiltInputReport01 *) report;
    reportData->reportId = (uint8_t) reportID;
    reportData->SNS_OrientationDeviceOrientationDataFieldTiltXAxis = 90;
    reportData->SNS_OrientationDeviceOrientationDataFieldTiltYAxis = 180;
    reportData->SNS_OrientationDeviceOrientationDataFieldTiltZAxis = 0;
    
    [self.testReportExpectation fulfill];
    
    return kIOReturnSuccess;
}

@end
