//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDMediaPlaybackRemoteDescriptor.h"

@interface TestIOHIDEventDriver_MediaPlaybackRemote : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testEventExpectation;
@property NSInteger                         count;

@end

@implementation TestIOHIDEventDriver_MediaPlaybackRemote

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: events"];
    
    self.testEventExpectation.expectedFulfillmentCount = 10;
    
    static uint8_t descriptor [] = {
        HIDMediaPlaybackRemote
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testMediaPlaybackRemote {
    
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

    HIDMediaPlaybackRemoteInputReport report;

    report.CD_ConsumerControlPlayPause = 1;
    report.CD_ConsumerControlScanNextTrack = 1;
    report.CD_ConsumerControlScanPreviousTrack = 1;
    report.CD_ConsumerControlVolumeDecrement = 1;
    report.CD_ConsumerControlVolumeIncrement = 1;
    
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

    HIDXCTAssertWithParameters ( RETURN_FROM_TEST,
                                self.count == 0,
                                "count: %d  events: %@",
                                (int)self.count,
                                self.events);

}


-(void) handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef __unused) service
{
    
    [super handleEvent:event fromService:service];
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard) {
        uint16_t usage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
        switch (usage) {
            case kHIDUsage_Csmr_PlayOrPause:
            case kHIDUsage_Csmr_ScanNextTrack:
            case kHIDUsage_Csmr_ScanPreviousTrack:
            case kHIDUsage_Csmr_VolumeIncrement:
            case kHIDUsage_Csmr_VolumeDecrement:
                [self.testEventExpectation fulfill];
                if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)) {
                    ++self.count;
                } else {
                    --self.count;
                }
                break;
            default:
                break;
        }
    }
}

-(NSDictionary *)  userDeviceDescription
{
    NSDictionary * description = [super userDeviceDescription];
    
    NSMutableDictionary * updateDescription = [[NSMutableDictionary alloc] initWithDictionary:description];

    updateDescription[@"Hidden"] = @(YES);

    return updateDescription;
}

@end
