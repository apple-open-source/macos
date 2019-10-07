//
//  TestIOHIDEventDriver_Gyro.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/13/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventDriverTestCase.h"
#import "IOHIDHeadsetRemoteTelephonyAndMediaPlaybackDescriptor.h"

@interface TestIOHIDEventDriver_HeadsetRemoteTelephonyAndMediaPlayback : IOHIDEventDriverTestCase

@property XCTestExpectation                 * testEventExpectation;
@property NSInteger                         count;

@end

@implementation TestIOHIDEventDriver_HeadsetRemoteTelephonyAndMediaPlayback

- (void)setUp {

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: events"];
    
    self.testEventExpectation.expectedFulfillmentCount = 22;
    
    static uint8_t descriptor [] = {
        HIDHeadsetRemoteTelephonyAndMediaPlayback
    };
    
    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];
    
    [super setUp];
}

- (void)tearDown {

    [super tearDown];
}


- (void)testHeadsetRemoteTelephonyAndMediaPlayback {
    
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

    
    HIDHeadsetRemoteTelephonyAndMediaPlaybackInputReport report;

    report.CD_ConsumerControlScanNextTrack = 1;      // Usage 0x000C00B5: Scan Next Track, Value = 0 to 1
    report.CD_ConsumerControlScanPreviousTrack = 1;  // Usage 0x000C00B6: Scan Previous Track, Value = 0 to 1
    report.CD_ConsumerControlRandomPlay = 1;         // Usage 0x000C00B9: Random Play, Value = 0 to 1
    report.CD_ConsumerControlRepeat = 1;             // Usage 0x000C00BC: Repeat, Value = 0 to 1
    report.CD_ConsumerControlAcPromote = 1;          // Usage 0x000C025B: AC Promote, Value = 0 to 1
    report.CD_ConsumerControlAcDemote = 1;           // Usage 0x000C025C: AC Demote, Value = 0 to 1
    report.CD_ConsumerControlAcAddToCart = 1;        // Usage 0x000C0262: AC Add to Cart, Value = 0 to 1
    report.CD_ConsumerControlVolumeIncrement = 0;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    report.CD_ConsumerControlVolumeDecrement = 0;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    report.CD_ConsumerControlMute = 1;               // Usage 0x000C00E2: Mute, Value = 0 to 1
    report.TEL_ConsumerControlFlash = 1;             // Usage 0x000B0021: Flash, Value = 0 to 1

    
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.CD_ConsumerControlVolumeIncrement = 0;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    report.CD_ConsumerControlVolumeDecrement = 1;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    report.CD_ConsumerControlMute = 0;               // Usage 0x000C00E2: Mute, Value = 0 to 1

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    report.CD_ConsumerControlVolumeIncrement = 1;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    report.CD_ConsumerControlVolumeDecrement = 0;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    report.CD_ConsumerControlMute = 0;               // Usage 0x000C00E2: Mute, Value = 0 to 1
    
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
        NSLog(@"Usage:%x", usage);
        switch (usage) {
            case kHIDUsage_Csmr_ScanNextTrack:
            case kHIDUsage_Csmr_ScanPreviousTrack:
            case kHIDUsage_Csmr_Mute:
            case kHIDUsage_Csmr_RandomPlay:
            case kHIDUsage_Csmr_Repeat:
            case kHIDUsage_Csmr_ACPromote:
            case kHIDUsage_Csmr_ACDemote:
            case kHIDUsage_Csmr_ACAddToCart:
            case kHIDUsage_Csmr_VolumeIncrement:
            case kHIDUsage_Csmr_VolumeDecrement:
            case kHIDUsage_Tfon_Flash:
 
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
