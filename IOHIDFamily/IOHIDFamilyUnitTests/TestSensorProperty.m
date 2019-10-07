//
//  TestSensorProperty.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 4/17/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDPrivateKeys.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"
#import  "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDServiceKeys.h>
#import "IOHIDEventDriverTestCase.h"


@interface TestSensorProperty : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testReportExpectation;
@property XCTestExpectation                 * testEventExpectation;
@property uint32_t                          reportInterval;
@property uint32_t                          reportLatency;

@end

@implementation TestSensorProperty

- (void)setUp {
    

    static uint8_t descriptor [] = {
        HIDAccel
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];

    self.testReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: report"];
    self.testReportExpectation.expectedFulfillmentCount = 2;
    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: events"];
    self.testEventExpectation.expectedFulfillmentCount = 100;

    [super setUp];
}

- (void)tearDown {


    [super tearDown];
}

- (void)testSensorProperty {
    id              value;
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);

    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    value = @(100000);
    IOHIDServiceClientSetProperty(self.eventService, CFSTR(kIOHIDServiceReportIntervalKey), (CFTypeRef) value);
    value = @(1000000);
    IOHIDServiceClientSetProperty(self.eventService, CFSTR(kIOHIDServiceBatchIntervalKey), (CFTypeRef) value);

    result = [XCTWaiter waitForExpectations:@[self.testReportExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                                self.reportInterval == 100000 && self.reportLatency == 1000000,
                                "reportInterval:%d reportLatency:%d",
                                self.reportInterval,
                                self.reportLatency);
}


- (void)testSensorBatching {
    IOReturn        status;
    id              value;
    XCTWaiterResult result;

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);

    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%d %@",
                                (int)result,
                                self.testServiceExpectation);

    value = @(100000);
    IOHIDServiceClientSetProperty(self.eventService, CFSTR(kIOHIDServiceReportIntervalKey), (CFTypeRef) value);
    value = @(1000000);
    IOHIDServiceClientSetProperty(self.eventService, CFSTR(kIOHIDServiceBatchIntervalKey), (CFTypeRef) value);

    dispatch_queue_t queue = dispatch_queue_create_with_target ("TestSensorProperty.reports", DISPATCH_QUEUE_SERIAL,dispatch_get_global_queue(0, 0));
    
    dispatch_async(queue, ^{
        HIDAccelInputReport01 report;
        report.reportId = 1;
        for (NSUInteger index = 0; index < 100; index++) {
            report.MotionAccelerometer3D000B = index;
            IOReturn ret = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
            HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                        ret == kIOReturnSuccess,
                                        "IOHIDUserDeviceHandleReport:0x%x",
                                        status);
            
        }
    });

    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "result:%d %@ events:%@",
                                (int)result,
                                self.testEventExpectation,
                                self.events
                                );
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeAccelerometer) {
        [self.testEventExpectation fulfill];
    }
}

-(IOReturn)userDeviceSetReportHandler: (IOHIDReportType )type :(uint32_t __unused)reportID :(uint8_t *)report :(NSUInteger) length {
    TestLog (@"report type:%d id:%d report:%@", (unsigned int) type, (unsigned int) reportID, [NSData dataWithBytes:report length:length]);
 
    if (kIOHIDReportTypeFeature == type && length >= 4) {
        if (reportID == 1 && kIOHIDReportTypeFeature == type) {
            self.reportInterval = *(uint32_t *)(report + 1);
        }
        if (reportID == 2 && kIOHIDReportTypeFeature == type && length >= 4) {
            self.reportLatency = *(uint32_t *) (report + 1);
        }
        [self.testReportExpectation fulfill];
    }
    return  kIOReturnSuccess;
}

@end
