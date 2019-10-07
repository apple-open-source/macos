//
//  TestDFRKeyboard.m
//
//
//  Created by dekom on 2/22/19.
//  Copyright © 2019 apple. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "HIDDeviceTester.h"
#include "IOHIDUnitTestUtility.h"
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>

uint8_t dfrKeyboard[] = {
    0x05, 0x01,                               // Usage Page (Generic Desktop)
    0x09, 0x06,                               // Usage (Keyboard)
    0xA1, 0x01,                               // Collection (Application)
    0x85, 0x01,                               //   ReportID................ (1)
    0x05, 0x07,                               //   Usage Page (Keyboard/Keypad)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (255)
    0x95, 0x06,                               //   Report Count............ (6)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0x85, 0x02,                               //   ReportID................ (2)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (255)
    0x95, 0x06,                               //   Report Count............ (6)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0x85, 0x03,                               //   ReportID................ (3)
    0x06, 0x01, 0xFF,                         //   Usage Page (65281)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (255)
    0x95, 0x02,                               //   Report Count............ (2)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0x85, 0x04,                               //   ReportID................ (4)
    0x05, 0xFF,                               //   Usage Page (Vendor Defined)
    0x19, 0x00,                               //   Usage Minimum........... (0)
    0x29, 0xFF,                               //   Usage Maximum........... (255)
    0x95, 0x04,                               //   Report Count............ (4)
    0x75, 0x08,                               //   Report Size............. (8)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x81, 0x00,                               //   Input...................(Data, Array, Absolute)
    0xC0,                                     // End Collection
};

static uint8_t avTopCaseUsages[] = {
    kHIDUsage_AV_TopCase_BrightnessDown,
    kHIDUsage_AV_TopCase_BrightnessUp,
    kHIDUsage_AV_TopCase_IlluminationDown,
    kHIDUsage_AV_TopCase_IlluminationUp,
    kHIDUsage_AV_TopCase_KeyboardFn
};

static uint8_t avKeyboardUsages[] = {
    kHIDUsage_AppleVendorKeyboard_Spotlight,
    kHIDUsage_AppleVendorKeyboard_Dashboard,
    kHIDUsage_AppleVendorKeyboard_Function,
    kHIDUsage_AppleVendorKeyboard_Launchpad,
    kHIDUsage_AppleVendorKeyboard_Reserved,
    kHIDUsage_AppleVendorKeyboard_CapsLockDelayEnable,
    kHIDUsage_AppleVendorKeyboard_PowerState,
    kHIDUsage_AppleVendorKeyboard_Expose_All,
    kHIDUsage_AppleVendorKeyboard_Expose_Desktop,
    kHIDUsage_AppleVendorKeyboard_Brightness_Up,
    kHIDUsage_AppleVendorKeyboard_Brightness_Down,
    kHIDUsage_AppleVendorKeyboard_Language
};

@interface TestDFRKeyboard : HIDDeviceTester

- (void)testAppleVendorEvents;

@end

@implementation TestDFRKeyboard

- (void)setUp
{
    self.useDevice = false;
    
    self.properties[@kIOHIDVendorIDKey] = @(1452);
    self.properties[@kIOHIDProductIDKey] = @(33538);
    self.properties[@kIOHIDAppleVendorSupported] = @YES;
    
    self.descriptor = [NSData dataWithBytes:dfrKeyboard
                                     length:sizeof(dfrKeyboard)];
    
    [super setUp];
}

- (void)testAppleVendorEvents
{
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.userDevice,
                               "Failed to create HIDUserDevice");
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.client,
                               "Failed to create client");
    
    self.eventExp.expectedFulfillmentCount = (sizeof(avKeyboardUsages) * 2);
    
    for (unsigned int i = 0; i < sizeof(avKeyboardUsages); i++) {
        uint8_t reportID = 3;
        uint8_t reportBytes[3] = { reportID, avKeyboardUsages[i], 0x00 };
        NSData *report = [NSData dataWithBytesNoCopy:reportBytes
                                              length:sizeof(reportBytes)
                                        freeWhenDone:false];
        
        [self.userDevice handleReport:report error:nil];
        usleep(1000);
        reportBytes[1] = 0;
        [self.userDevice handleReport:report error:nil];
    }
    
    result = [XCTWaiter waitForExpectations:@[self.eventExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               self.eventExp);
    
    self.eventExp = [[XCTestExpectation alloc] initWithDescription:@"Event Exp"];
    self.eventExp.expectedFulfillmentCount = (sizeof(avTopCaseUsages) * 2) - 1;
    
    for (unsigned int i = 0; i < sizeof(avTopCaseUsages); i++) {
        uint8_t reportID = 4;
        uint8_t reportBytes[5] = { reportID, avTopCaseUsages[i], 0x00, 0x00, 0x00 };
        NSData *report = [NSData dataWithBytesNoCopy:reportBytes
                                              length:sizeof(reportBytes)
                                        freeWhenDone:false];
        
        [self.userDevice handleReport:report error:nil];
        usleep(1000);
        reportBytes[1] = 0;
        [self.userDevice handleReport:report error:nil];
    }
    
    result = [XCTWaiter waitForExpectations:@[self.eventExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               self.eventExp);
}

@end
