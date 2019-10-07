//
//  TestProperty.m
//  IOHIDFamily
//
//  Created by dekom on 2/21/17.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#import "HID.h"
#import <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/IOKitLib.h>

// Apple 3.5mm earbuds that use consumer usages
static uint8_t appleEarbuds[] = {
    0x05, 0x0C,                               // Usage Page (Consumer)
    0x09, 0x01,                               // Usage 1 (0x1)
    0xA1, 0x01,                               // Collection (Application)
    0x85, 0x01,                               //   ReportID................ (1)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x09, 0xCD,                               //   Usage 205 (0xcd)
    0x09, 0xE9,                               //   Usage 233 (0xe9)
    0x09, 0xEA,                               //   Usage 234 (0xea)
    0x09, 0xCF,                               //   Usage 207 (0xcf)
    0x95, 0x04,                               //   Report Count............ (4)
    0x75, 0x01,                               //   Report Size............. (1)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x95, 0x0C,                               //   Report Count............ (12)
    0x81, 0x05,                               //   Input...................(Constant)
    0xC0,                                     // End Collection
};

// Apple lightning earbuds that use kHIDUsage_Tfon_Flash
static uint8_t appleLightningEarbuds[] = {
    0x05, 0x0B,                               // Usage Page (Telephony Device)
    0x09, 0x05,                               // Usage 5 (0x5)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x0B,                               //   Usage Page (Telephony Device)
    0x09, 0x21,                               //   Usage 33 (0x21)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x09, 0xEA,                               //   Usage 234 (0xea)
    0x09, 0xE9,                               //   Usage 233 (0xe9)
    0x06, 0x07, 0xFF,                         //   Usage Page (65287)
    0x09, 0x01,                               //   Usage 1 (0x1)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x95, 0x04,                               //   Report Count............ (4)
    0x75, 0x01,                               //   Report Size............. (1)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x95, 0x04,                               //   Report Count............ (4)
    0x81, 0x05,                               //   Input...................(Constant)
    0xC0,                                     // End Collection
};

// Third party earbuds with unsupported usages
static uint8_t thirdPartyEarbuds[] = {
    0x05, 0x0C,                               // Usage Page (Consumer)
    0x09, 0x01,                               // Usage 1 (0x1)
    0xA1, 0x01,                               // Collection (Application)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x09, 0xE9,                               //   Usage 233 (0xe9)
    0x09, 0xEA,                               //   Usage 234 (0xea)
    0x09, 0xB5,                               //   Usage 181 (0xb5)
    0x09, 0xB6,                               //   Usage 182 (0xb6)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x04,                               //   Report Count............ (4)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x09, 0xE2,                               //   Usage 226 (0xe2)
    0x09, 0xB7,                               //   Usage 183 (0xb7)
    0x09, 0xCD,                               //   Usage 205 (0xcd)
    0x95, 0x03,                               //   Report Count............ (3)
    0x81, 0x06,                               //   Input...................(Data, Variable, Relative)
    0x05, 0x0B,                               //   Usage Page (Telephony Device)
    0x09, 0x20,                               //   Usage 32 (0x20)
    0x95, 0x01,                               //   Report Count............ (1)
    0x81, 0x06,                               //   Input...................(Data, Variable, Relative)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x26, 0xFF, 0x00,                         //   Logical Maximum......... (255)
    0x09, 0x00,                               //   Usage 0 (0x0)
    0x75, 0x08,                               //   Report Size............. (8)
    0x95, 0x03,                               //   Report Count............ (3)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x09, 0x00,                               //   Usage 0 (0x0)
    0x95, 0x04,                               //   Report Count............ (4)
    0x91, 0x02,                               //   Output..................(Data, Variable, Absolute)
    0xC0,                                     // End Collection
};

// Apple Remote with unsupported usages
static uint8_t appleRemote[] = {
    0x05, 0x0C,                               // Usage Page (Consumer)
    0x09, 0x01,                               // Usage 1 (0x1)
    0xA1, 0x01,                               // Collection (Application)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x25, 0x01,                               //   Logical Maximum......... (1)
    0x85, 0xFA,                               //   ReportID................ (250)
    0x75, 0x01,                               //   Report Size............. (1)
    0x95, 0x08,                               //   Report Count............ (8)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x09, 0x60,                               //   Usage 96 (0x60)
    0x09, 0xE9,                               //   Usage 233 (0xe9)
    0x09, 0xEA,                               //   Usage 234 (0xea)
    0x09, 0xCD,                               //   Usage 205 (0xcd)
    0x09, 0x04,                               //   Usage 4 (0x4)
    0x05, 0x01,                               //   Usage Page (Generic Desktop)
    0x09, 0x86,                               //   Usage (System App Menu)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x09, 0x30,                               //   Usage 48 (0x30)
    0x09, 0x80,                               //   Usage 128 (0x80)
    0x81, 0x03,                               //   Input...................(Constant)
    0x05, 0x0C,                               //   Usage Page (Consumer)
    0x09, 0x01,                               //   Usage 1 (0x1)
    0x85, 0xFF,                               //   ReportID................ (255)
    0x76, 0x80, 0x06,                         //   Report Size............. (1664)
    0x95, 0x01,                               //   Report Count............ (1)
    0xB2, 0x02, 0x01,                         //   Feature.................(Data, Variable, Absolute, Buffered bytes)
    0xC0,                                     // End Collection
};

@interface TestProperty : XCTestCase

@property   IOHIDEventSystemClientRef   client;

@end

@implementation TestProperty

- (void)setUp {
    self.client = IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
    HIDXCTAssertAndThrowTrue(self.client != NULL);
}

- (void)tearDown {
    CFRelease(self.client);
    [super tearDown];
}

- (void)testIdleTimeProperty {
    CFNumberRef property = (CFNumberRef)IOHIDEventSystemClientCopyProperty(self.client, CFSTR(kIOHIDIdleTimeMicrosecondsKey));
    HIDXCTAssertAndThrowTrue(property != NULL);
    
    uint64_t val = 0;
    CFNumberGetValue(property, kCFNumberSInt64Type, &val);
    XCTAssert(val != 0);
    
    sleep(1);
    property = (CFNumberRef)IOHIDEventSystemClientCopyProperty(self.client, CFSTR(kIOHIDIdleTimeMicrosecondsKey));
    HIDXCTAssertAndThrowTrue(property != NULL);
    
    uint64_t newVal = 0;
    CFNumberGetValue(property, kCFNumberSInt64Type, &newVal);
    XCTAssert(newVal != 0);
    XCTAssert(newVal != val);
}

- (NSString *)hintForDescriptor:(uint8_t *)desc length:(size_t)length
{
    NSData *descriptor = nil;
    NSMutableDictionary *properties = [[NSMutableDictionary alloc] init];
    HIDUserDevice *device = nil;
    
    descriptor = [[NSData alloc] initWithBytes:desc
                                        length:length];
    properties[@kIOHIDReportDescriptorKey] = descriptor;
    
    device = [[HIDUserDevice alloc] initWithProperties:properties];
    HIDXCTAssertAndThrowTrue(device);
    
    mach_timespec_t waitTime = {30, 0};
    kern_return_t kr = IOServiceWaitQuiet(device.service, &waitTime);
    if (kr) {
        NSLog(@"IOServiceWaitQuiet:0x%x", kr);
        return nil;
    }
    
    return [device propertyForKey:@(kIOHIDDeviceTypeHintKey)];
}

- (void)test_40723179
{
    NSString *hint = nil;
    
    // Test 3.5mm apple ear buds. With this descriptor we would expect the
    // property kIOHIDDeviceTypeHintKey to be published with a value of
    // kIOHIDDeviceTypeHeadsetKey.
    hint = [self hintForDescriptor:appleEarbuds length:sizeof(appleEarbuds)];
    NSLog(@"Apple earbuds hint: %@", hint);
    XCTAssert(hint && [hint isEqualToString:@(kIOHIDDeviceTypeHeadsetKey)]);
    
    // Test apple lightning earbuds. With this descriptor we would expect the
    // property kIOHIDDeviceTypeHintKey to be published with a value of
    // kIOHIDDeviceTypeHeadsetKey.
    hint = [self hintForDescriptor:appleLightningEarbuds length:sizeof(appleLightningEarbuds)];
    NSLog(@"Apple lightning earbuds hint: %@", hint);
    XCTAssert(hint && [hint isEqualToString:@(kIOHIDDeviceTypeHeadsetKey)]);
    
    // Test a 3rd party descriptor. We would not expect any hint to be published
    // for this device, since it has unsupported usages.
    hint = [self hintForDescriptor:thirdPartyEarbuds length:sizeof(thirdPartyEarbuds)];
    NSLog(@"3rd party hint: %@", hint);
    XCTAssert(!hint);
    
    // Test apple remote. This should not publish any hint (45493575)
    hint = [self hintForDescriptor:appleRemote length:sizeof(appleRemote)];
    NSLog(@"Apple remote hint: %@", hint);
    XCTAssert(!hint);
}

@end
