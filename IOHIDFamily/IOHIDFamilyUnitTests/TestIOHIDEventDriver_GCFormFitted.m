//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDPrivateKeys.h"
#import "IOHIDGCFormfittedDescriptor.h"

@interface TestIOHIDEventDriver_GCFormFitted : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testEventExpectation;

@end

@implementation TestIOHIDEventDriver_GCFormFitted

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: event"];
    self.testEventExpectation.expectedFulfillmentCount = 3;
    
    static uint8_t descriptor [] = {
        HIDGCFormFitted
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testGCFormFitted {
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

    id property = CFBridgingRelease(IOHIDServiceClientCopyProperty(self.eventService, CFSTR("GameControllerPointer")));
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG, property[@"GameControllerCapabilities"], "GameControllerPointer:%@", property);
    NSLog(@"GameControllerCapabilities:0x%x", (int)((NSNumber *) property[@"GameControllerCapabilities"]).integerValue);

    HIDGCFormFittedInputReport report;

    memset(&report, 0, sizeof(report));
 
    report.BTN_GamePadButton1  = 1;
    report.BTN_GamePadButton2  = 1;
    report.BTN_GamePadButton3  = 1;
    report.BTN_GamePadButton4  = 1;
    report.BTN_GamePadButton5  = 1;
    report.BTN_GamePadButton6  = 1;
    report.BTN_GamePadButton7  = 255;
    report.BTN_GamePadButton8  = 255;
    report.BTN_GamePadButton9  = 1;
    report.BTN_GamePadButton10 = 1;
    

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.GD_GamePadPointerX =  63;
    report.GD_GamePadPointerY =  -63;
    report.GD_GamePadPointerZ =  63;
    report.GD_GamePadPointerRz = 127;
    
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    


    report.GD_GamePadDPadUp    = 1;
    report.GD_GamePadDPadRight = 1;
    report.GD_GamePadDPadDown  = 1;
    report.GD_GamePadDPadLeft  = 1;

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

    
   NSInteger  ivalue;
    IOHIDFloat fvalue;

    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerFaceButtonA);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerFaceButtonB);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);

    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerFaceButtonX);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerFaceButtonY);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);

    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerShoulderButtonR1);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerShoulderButtonR2);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerShoulderButtonL1);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerShoulderButtonL2);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,fvalue == 1.0, "events:%@", self.events);

    
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerThumbstickButtonRight);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 1, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerThumbstickButtonLeft);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 1, "events:%@", self.events);

    
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerJoyStickAxisX);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, fvalue == 0, "value:%f events:%@", ceil(fvalue * 10) / 10,  self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerJoyStickAxisY);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, fvalue == 0, "value:%f events:%@", floor(fvalue * 10) / 10, self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerJoyStickAxisZ);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, fvalue == 0, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerJoyStickAxisRz);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, fvalue == 0, "events:%@", self.events);

    
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGameControllerJoyStickAxisX);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, ceil(fvalue * 10) / 10 == 0.5, "value:%f events:%@", ceil(fvalue * 10) / 10,  self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGameControllerJoyStickAxisY);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, floor(fvalue * 10) / 10 == -0.5, "value:%f events:%@", floor(fvalue * 10) / 10, self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGameControllerJoyStickAxisZ);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ceil(fvalue * 10) / 10 == 0.5, "events:%@", self.events);
    fvalue = IOHIDEventGetFloatValue((IOHIDEventRef)self.events[1], kIOHIDEventFieldGameControllerJoyStickAxisRz);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, fvalue == 1.0, "events:%@", self.events);

    
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerDirectionPadUp);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 0, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerDirectionPadDown);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 0, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerDirectionPadLeft);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 0, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[0], kIOHIDEventFieldGameControllerDirectionPadRight);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 0, "events:%@", self.events);

    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldGameControllerDirectionPadUp);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 1, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldGameControllerDirectionPadDown);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 1, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldGameControllerDirectionPadLeft);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 1, "events:%@", self.events);
    ivalue = IOHIDEventGetIntegerValue((IOHIDEventRef)self.events[2], kIOHIDEventFieldGameControllerDirectionPadRight);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,ivalue == 1, "events:%@", self.events);

    property = CFBridgingRelease(IOHIDServiceClientCopyProperty(self.eventService, CFSTR(kIOHIDGameControllerFormFittingKey)));
    HIDXCTAssertWithParameters (RETURN_FROM_TEST, property && [property isKindOfClass: [NSNumber class]] && ((NSNumber *)property).boolValue, "kIOHIDGameControllerFormFittingKey:%@", property);

    property = CFBridgingRelease(IOHIDServiceClientCopyProperty(self.eventService, CFSTR(kIOHIDGameControllerTypeKey)));
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_IOREG, property && [property isKindOfClass: [NSNumber class]] && ((NSNumber *)property).integerValue == 1, "kIOHIDGameControllerTypeKey:%@", property);

}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeGameController) {
        [self.testEventExpectation fulfill];
    }
}


-(NSDictionary *)  userDeviceDescription
{
    NSDictionary * description = [super userDeviceDescription];
    
    NSMutableDictionary * gcDescription = [[NSMutableDictionary alloc] initWithDictionary:description];
    
    gcDescription[@"Hidden"] = @(YES);
    gcDescription[@(kIOHIDAuthenticatedDeviceKey)] = @(YES);
    
    return gcDescription;
}


@end
