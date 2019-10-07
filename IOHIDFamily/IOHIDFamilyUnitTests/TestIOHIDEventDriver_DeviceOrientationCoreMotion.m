//
//  TestIOHIDEventDriver_DeviceOrientationCoreMotion.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDDeviceOrientationCoreMotionDescriptor.h"

@interface TestIOHIDEventDriver_DeviceOrientationCoreMotion : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_DeviceOrientationCoreMotion

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testEventExpectation.expectedFulfillmentCount = 7;
    
    static uint8_t descriptor [] = {
        HIDOrientationCoreMotion
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}

#define CMOrientationReportForUsage(u) (u - kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous + 1)


- (void)testDeviceOrientationCoreMotion {
    IOReturn        status;
    NSInteger       value;
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    HIDOrientationCoreMotionInputReport report;

    memset(&report, 0, sizeof(report));
 
    uint32_t usages [] = {
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypeAmbiguous,
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortrait,
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypePortraitUpsideDown,
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypeLandscapeLeft,
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypeLandscapeRight,
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypeFaceUp,
        kHIDUsage_AppleVendorMotion_DeviceOrientationTypeFaceDown,
    };

    for (NSUInteger index = 0 ; index < sizeof(usages) / sizeof(usages[0]); ++index) {
        report.OrientationDeviceOrientation = CMOrientationReportForUsage (usages[index]);
        
        status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
        HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                    status == kIOReturnSuccess,
                                    "IOHIDUserDeviceHandleReport:0x%x",
                                    status);
    
        __block IOHIDEventRef       event;
        __block XCTestExpectation * testCopyEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: copy event"];

        dispatch_async (dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            event = IOHIDServiceClientCopyEvent(self.eventService, kIOHIDEventTypeOrientation, NULL, 0);
            [testCopyEventExpectation fulfill];
        });
        
        result = [XCTWaiter waitForExpectations:@[testCopyEventExpectation] timeout:10];
        HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                    result == XCTWaiterResultCompleted && event != NULL,
                                    "result:%ld copyEvent:%@ expectation:%@",
                                    (long)result,
                                    event,
                                    testCopyEventExpectation);

        
        value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationOrientationType);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, value == kIOHIDOrientationTypeCMUsage,  "Event:%@", event);
        
        value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldOrientationDeviceOrientationUsage);
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, value == usages[index], "Event:%@", event);
        
        CFRelease(event);
    }
}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeOrientation && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypeCMUsage) {
        [self.testEventExpectation fulfill];
    }
}




@end
