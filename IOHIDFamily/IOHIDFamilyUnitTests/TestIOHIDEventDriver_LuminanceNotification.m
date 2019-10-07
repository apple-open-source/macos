//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDDeviceLuminanceNotificationDescriptor.h"

@interface TestIOHIDEventDriver_LuminanceNotification : IOHIDEventDriverTestCase


@property XCTestExpectation                 * testEventExpectation;
@property XCTestExpectation                 * testReportExpectation;
@property XCTestExpectation                 * testCopyEventExpectation;

@end

@implementation TestIOHIDEventDriver_LuminanceNotification

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testCopyEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: copy event"];
    self.testReportExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: report"];
    self.testEventExpectation.expectedFulfillmentCount = 2;
    
    static uint8_t descriptor [] = {
        HIDLuminanceNotification
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testDispatchLuminanceNotificationEvent {
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

    HIDLuminanceNotificationInputReport10 report;

    memset(&report, 0, sizeof(report));
 
    report.reportId = 0x10 ;
    report.VEN_VendorDefined0003 = 0x12344567;

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

    
    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testEventExpectation);

}

- (void)testCopyLuminanceNotificationEvent
{
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);
    
    __block IOHIDEventRef copyEvent = NULL;
    dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        uint32_t payload = 0;
        IOHIDEventRef matching  = IOHIDEventCreateVendorDefinedEvent(kCFAllocatorDefault, 0, kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_LuminanceData, 0, (uint8_t*)&payload, sizeof(uint32_t), 0);
        copyEvent = IOHIDServiceClientCopyEvent(self.eventService, kIOHIDEventTypeVendorDefined, matching, 0);
        [self.testCopyEventExpectation fulfill];
        CFRelease(matching);
    });
    
    result = [XCTWaiter waitForExpectations:@[self.testCopyEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST,
                                result == XCTWaiterResultCompleted && copyEvent != NULL,
                                "result:%ld copyEvent:%@ expectation:%@",
                                (long)result,
                                copyEvent,
                                self.testCopyEventExpectation);
    
    if (copyEvent) {
        CFRelease(copyEvent);
    }
}

-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeVendorDefined ) {
        [self.testEventExpectation fulfill];
    }
}


-(IOReturn)userDeviceGetReportHandler: (IOHIDReportType)type :(uint32_t)reportID :(uint8_t *)report :(NSUInteger *) length
{
    NSLog(@"userDeviceGetReportHandler:%d :%d :%p :%ld", type, reportID, report, *length);
    
    if (*length < sizeof (HIDLuminanceNotificationInputReport10) || reportID != 0x10 || type != kIOHIDReportTypeInput) {
        return kIOReturnUnsupported;
    }
    
    HIDLuminanceNotificationInputReport10 * reportData = (HIDLuminanceNotificationInputReport10 *) report;
    reportData->reportId = (uint8_t) reportID;
    reportData->VEN_VendorDefined0003 = 0x12345678;
    
    [self.testReportExpectation fulfill];
    
    return kIOReturnSuccess;
}

@end
