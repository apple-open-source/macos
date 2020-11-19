//
//  TestKeyboardLongPressEvents.m
//  IOHIDFamilyUnitTests
//
//  Created by Josh Kergan on 4/27/20.
//

#import <Foundation/Foundation.h>
#import "HIDDeviceTester.h"
#include "IOHIDUnitTestUtility.h"
#include "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <HID/HIDEvent.h>

#define HIDPhaseArrayKeyboardDescriptor \
0x05, 0x0C,                               /* Usage Page (Consumer) */\
0x09, 0x01,                               /* Usage (Consumer Control) */\
0xA1, 0x01,                               /* Collection (Application) */\
0x85, 0x01,                               /*   ReportID................ (1) */\
0x05, 0x0C,                               /*   Usage Page (Consumer) */\
0x09, 0xCD,                               /*   Usage (Play Or Pause) */\
0x09, 0xEA,                               /*   Usage (Volume Decrement) */\
0x09, 0xE9,                               /*   Usage (Volume Increment) */\
0x09, 0xE2,                               /*   Usage (Mute) */\
0x06, 0x01, 0xFF,                         /*   Usage Page (Apple Vendor Keyboard) */\
0x0A, 0x00, 0x01,                         /*   Usage (Long Press) */\
0x75, 0x01,                               /*   Report Size........... (1) */\
0x95, 0x05,                               /*   Report Count.......... (5) */\
0x15, 0x00,                               /*   Logical Minimum....... (0) */\
0x25, 0x01,                               /*   Logical Maximum....... (1) */\
0x81, 0x02,                               /*   Input (Data, Variable, Absolute) */\
0x75, 0x03,                               /*   Report Size........... (3) */\
0x95, 0x01,                               /*   Report Count.......... (1) */\
0x81, 0x01,                               /*   Input (Constant) */\
0x06, 0x1C, 0xFF,                         /*   Usage Page (Apple Vendor Phase) */\
0x09, 0x01,                               /*   Usage (Phase Began) */\
0x09, 0x02,                               /*   Usage (Phase Changed) */\
0x09, 0x03,                               /*   Usage (Phase Ended) */\
0x09, 0x04,                               /*   Usage (Phase Cancelled) */\
0x09, 0x05,                               /*   Usage (Phase May Begin) */\
0x15, 0x01,                               /*   Logical Minimum....... (1) */\
0x25, 0x05,                               /*   Logical Maximum....... (5) */\
0x75, 0x01,                               /*   Report Size........... (3) */\
0x95, 0x05,                               /*   Report Count.......... (1) */\
0x81, 0x03,                               /*   Input (Data, Array, Absolute) */\
0x75, 0x03,                               /*   Report Size........... (5) */\
0x95, 0x01,                               /*   Report Count.......... (1) */\
0x81, 0x01,                               /*   Input (Constant) */\
0xC0,                                     /*  End Collection */

typedef struct __attribute__((packed))
{
    uint8_t  reportId;                                 // Report ID = 0x01 (1)

    // Field:   1
    // Width:   1
    // Count:   4
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF07 LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:4
    // Locals:  USAG:FF070001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  000B0021 000C00EA 000C00E9
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF07: Vendor-defined
    // Collection: ConsumerControl
    uint8_t  TEL_ConsumerControlFlash : 1;             // Usage 0x000B0021: Flash, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeDecrement : 1;    // Usage 0x000C00EA: Volume Decrement, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeIncrement : 1;    // Usage 0x000C00E9: Volume Increment, Value = 0 to 1
    uint8_t  CD_ConsumerControlVolumeMute : 1;
    uint8_t  VEN_AppleVendorKeyboard_LongPress : 1;

    uint8_t : 3; // Pad

    // Field:   2
    // Width:   1
    // Count:   5
    // Flags:   00000002: 0=Data 1=Variable 0=Absolute 0=NoWrap 0=Linear 0=PrefState 0=NoNull 0=NonVolatile 0=Bitmap
    // Globals: PAGE:FF1C LMIN:0 LMAX:1 PMIN:0 PMAX:0 UEXP:0 UNIT:0 RSIZ:1 RID:0 RCNT:5
    // Locals:  USAG:FF1C0001 UMIN:0 UMAX:0 DIDX:0 DMIN:0 DMAX:0 SIDX:0 SMIN:0 SMAX:0
    // Usages:  FF1C0001 FF1C0002 FF1C0003 FF1C0004 FF1C0005
    // Coll:    ConsumerControl
    // Access:  Read/Write
    // Type:    Variable
    // Page 0xFF1C: Vendor-defined
    // Collection: ConsumerControl
    uint8_t VEN_Phase           : 3;                    // Usage 0xFF1C0001: Phase Began, Value = 0 to 5

    uint8_t : 5; // Pad

} HIDPhaseArrayKeyboardReport;

static uint8_t phaseKeyboard[] = {
    HIDPhaseKeyboardDescriptor
};

static uint8_t phaseArrayKeyboard[] = {
    HIDPhaseArrayKeyboardDescriptor
};

static uint8_t phaseKeyboardUsages[] = {
    kHIDUsage_Csmr_VolumeIncrement,
    kHIDUsage_Csmr_VolumeDecrement,
    kHIDUsage_Csmr_Mute,
};

static uint8_t phaseUsages[] = {
    kHIDUsage_AppleVendorHIDEvent_PhaseBegan,
    kHIDUsage_AppleVendorHIDEvent_PhaseEnded,
    kHIDUsage_AppleVendorHIDEvent_PhaseChanged,
    kHIDUsage_AppleVendorHIDEvent_PhaseCancelled,
    kHIDUsage_AppleVendorHIDEvent_PhaseMayBegin,
};

static uint8_t phaseFlags;

@interface TestPhaseKeyboardEvents : HIDDeviceTester
@end

@implementation TestPhaseKeyboardEvents

- (void)setUp
{
    phaseFlags = 0;
    self.useDevice = false;

    self.properties[@kIOHIDVendorIDKey] = @(555);
    self.properties[@kIOHIDProductIDKey] = @(555);
    self.properties[@kIOHIDAppleVendorSupported] = @YES;

    self.descriptor = [NSData dataWithBytes:phaseKeyboard
                                     length:sizeof(phaseKeyboard)];

    [super setUp];
}

- (void)EMBEDDED_OS_ONLY_TEST_CASE(testLongPressEvents)
{
    XCTWaiterResult result;

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.userDevice,
                               "Failed to create HIDUserDevice");

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.client,
                               "Failed to create client");

    self.eventExp.expectedFulfillmentCount = (sizeof(phaseKeyboardUsages));

    for (unsigned int i = 0; i < sizeof(phaseKeyboardUsages); i++) {
        HIDPhaseKeyboardReport reportBytes;
        bzero(&reportBytes, sizeof(reportBytes));
        reportBytes.reportId = 1;
        reportBytes.VEN_AppleVendorKeyboard_LongPress = 1;
        switch(phaseKeyboardUsages[i]) {
            case kHIDUsage_Csmr_VolumeIncrement:
                reportBytes.CD_ConsumerControlVolumeIncrement = 1;
                break;
            case kHIDUsage_Csmr_VolumeDecrement:
                reportBytes.CD_ConsumerControlVolumeDecrement = 1;
                break;
            case kHIDUsage_Csmr_Mute:
                reportBytes.CD_ConsumerControlVolumeMute = 1;
                break;
        }
        NSData *report = [NSData dataWithBytesNoCopy:&reportBytes
                                              length:sizeof(reportBytes)
                                        freeWhenDone:false];

        [self.userDevice handleReport:report error:nil];
        usleep(1000);
        bzero(&reportBytes, sizeof(reportBytes));
        reportBytes.reportId = 1;
        [self.userDevice handleReport:report error:nil];
    }

    result = [XCTWaiter waitForExpectations:@[self.eventExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               self.eventExp);
}

#define kPhaseMask (kIOHIDEventPhaseBegan   | kIOHIDEventPhaseChanged  | kIOHIDEventPhaseEnded | kIOHIDEventPhaseCancelled | kIOHIDEventPhaseMayBegin)

-(void)EMBEDDED_OS_ONLY_TEST_CASE(testPhaseEvents) {
    XCTWaiterResult result;

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.userDevice,
                               "Failed to create HIDUserDevice");

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.client,
                               "Failed to create client");

    self.eventExp.expectedFulfillmentCount = (sizeof(phaseUsages));

    for (unsigned int i = 0; i < sizeof(phaseUsages); i++) {
        HIDPhaseKeyboardReport reportBytes;
        bzero(&reportBytes, sizeof(reportBytes));
        reportBytes.reportId = 1;
        reportBytes.CD_ConsumerControlVolumeIncrement = 1;
        switch(phaseUsages[i]) {
            case kHIDUsage_AppleVendorHIDEvent_PhaseBegan:
                reportBytes.VEN_Phase_Began = 1;
                break;
            case kHIDUsage_AppleVendorHIDEvent_PhaseEnded:
                reportBytes.VEN_Phase_Ended = 1;
                break;
            case kHIDUsage_AppleVendorHIDEvent_PhaseChanged:
                reportBytes.VEN_Phase_Changed = 1;
                break;
            case kHIDUsage_AppleVendorHIDEvent_PhaseCancelled:
                reportBytes.VEN_Phase_Cancelled = 1;
                break;
            case kHIDUsage_AppleVendorHIDEvent_PhaseMayBegin:
                reportBytes.VEN_Phase_MayBegin = 1;
                break;
        }
        NSData *report = [NSData dataWithBytesNoCopy:&reportBytes
                                              length:sizeof(reportBytes)
                                        freeWhenDone:false];

        [self.userDevice handleReport:report error:nil];
        usleep(1000);
        bzero(&reportBytes, sizeof(reportBytes));
        reportBytes.reportId = 1;
        [self.userDevice handleReport:report error:nil];
    }

    result = [XCTWaiter waitForExpectations:@[self.eventExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               self.eventExp);

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, phaseFlags == kPhaseMask, "Did not fill phase mask as expected: %#x", phaseFlags);
}

-(void)handleEvent:(HIDEvent *)event forService:(nonnull HIDServiceClient *)service {
    NSLog(@"%@", event);
    NSInteger keyDown = IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)event, kIOHIDEventFieldKeyboardDown);
    NSInteger longPress = IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)event, kIOHIDEventFieldKeyboardLongPress);
    NSInteger phase = IOHIDEventGetPhase((__bridge IOHIDEventRef)event);
    if (keyDown && longPress) {
        [self.eventExp fulfill];
    }
    if (phase) {
        [self.eventExp fulfill];
        phaseFlags |= phase;
    }
    [self.events addObject:event];
}

@end

@interface TestPhaseArrayKeyboardEvents : HIDDeviceTester
@end

@implementation TestPhaseArrayKeyboardEvents

- (void)setUp
{
    phaseFlags = 0;
    self.useDevice = false;

    self.properties[@kIOHIDVendorIDKey] = @(555);
    self.properties[@kIOHIDProductIDKey] = @(555);
    self.properties[@kIOHIDAppleVendorSupported] = @YES;

    self.descriptor = [NSData dataWithBytes:phaseArrayKeyboard
                                     length:sizeof(phaseArrayKeyboard)];

    [super setUp];
}

#define kPhaseMask (kIOHIDEventPhaseBegan   | kIOHIDEventPhaseChanged  | kIOHIDEventPhaseEnded | kIOHIDEventPhaseCancelled | kIOHIDEventPhaseMayBegin)

-(void)EMBEDDED_OS_ONLY_TEST_CASE(testPhaseEvents) {
    XCTWaiterResult result;

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.userDevice,
                               "Failed to create HIDUserDevice");

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                               self.client,
                               "Failed to create client");

    self.eventExp.expectedFulfillmentCount = (sizeof(phaseUsages));

    for (unsigned int i = 0; i < sizeof(phaseUsages); i++) {
        HIDPhaseArrayKeyboardReport reportBytes;
        bzero(&reportBytes, sizeof(reportBytes));
        reportBytes.reportId = 1;
        reportBytes.CD_ConsumerControlVolumeIncrement = 1;
        reportBytes.VEN_Phase = phaseUsages[i];

        NSData *report = [NSData dataWithBytesNoCopy:&reportBytes
                                              length:sizeof(reportBytes)
                                        freeWhenDone:false];

        [self.userDevice handleReport:report error:nil];
        usleep(1000);
        bzero(&reportBytes, sizeof(reportBytes));
        reportBytes.reportId = 1;
        [self.userDevice handleReport:report error:nil];
    }

    result = [XCTWaiter waitForExpectations:@[self.eventExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted,
                               "expectation: %@",
                               self.eventExp);

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, phaseFlags == kPhaseMask, "Did not fill phase mask as expected: %#x", phaseFlags);
}

-(void)handleEvent:(HIDEvent *)event forService:(nonnull HIDServiceClient *)service {
    NSLog(@"%@", event);
    NSInteger keyDown = IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)event, kIOHIDEventFieldKeyboardDown);
    NSInteger longPress = IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)event, kIOHIDEventFieldKeyboardLongPress);
    NSInteger phase = IOHIDEventGetPhase((__bridge IOHIDEventRef)event);
    if (keyDown && longPress) {
        [self.eventExp fulfill];
    }
    if (phase) {
        [self.eventExp fulfill];
        phaseFlags |= phase;
    }
    [self.events addObject:event];
}

@end


#import <Foundation/Foundation.h>
