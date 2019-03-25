//
//  TestNonStandardDescriptor.m
//  IOHIDFamilyUnitTests
//
//  Created by Paul Doerr on 10/15/18.
//

#import <XCTest/XCTest.h>
#import <IOKit/hid/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDDevice.h>
#import <IOKit/hid/IOHIDManager.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDXCTestExpectation.h"


static uint8_t constantLogicalDescriptor[] = {
    0x06, 0x00, 0xFF,                         // Usage Page (65280)
    0x09, 0x80,                               // Usage 128 (0x80)
    0xA1, 0x02,                               // Collection (Logical)
    0x85, 0x02,                               //   ReportID................ (2)
    0x75, 0x08,                               //   Report Size............. (8)
    0x95, 0x3F,                               //   Report Count............ (63)
    0x15, 0x00,                               //   Logical Minimum......... (0)
    0x26, 0x00, 0x10,                         //   Logical Maximum......... (4096)
    0x81, 0x01,                               //   Input...................(Constant)
    0x85, 0x03,                               //   ReportID................ (3)
    0x75, 0x08,                               //   Report Size............. (8)
    0x95, 0x3F,                               //   Report Count............ (63)
    0x81, 0x01,                               //   Input...................(Constant)
    0xC0,                                     // End Collection
};

static void HIDManagerDeviceAddedCallback ( void * _Nullable context, IOReturn  result, void * _Nullable  sender, IOHIDDeviceRef device);

@interface TestNonStandardDescriptor :  XCTestCase

@property NSString *                        uniqueID;
@property IOHIDManagerRef                   deviceManager;
@property IOHIDXCTestExpectation *          deviceMatchedExpection;
@property IOHIDUserDeviceRef                userDevice;

@end

@implementation TestNonStandardDescriptor

- (void)setUp {
    [super setUp];

    self.deviceMatchedExpection = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Expectation: Test HID Device"];
    HIDXCTAssertAndThrowTrue(self.deviceMatchedExpection != nil);

    self.uniqueID = [[[NSUUID alloc] init] UUIDString];

    self.deviceManager = IOHIDManagerCreate(kCFAllocatorDefault, kIOHIDOptionsTypeNone);
    HIDXCTAssertAndThrowTrue(self.deviceManager != nil);

    NSDictionary * matching = @{@kIOHIDPhysicalDeviceUniqueIDKey:self.uniqueID};
    IOHIDManagerSetDeviceMatching(self.deviceManager, (CFDictionaryRef)matching);
    IOHIDManagerRegisterDeviceMatchingCallback(self.deviceManager, HIDManagerDeviceAddedCallback, (__bridge void *)(self));
    IOHIDManagerScheduleWithRunLoop(self.deviceManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDManagerOpen(self.deviceManager, kIOHIDOptionsTypeNone);
}

- (void)tearDown {
    IOHIDManagerUnscheduleFromRunLoop(self.deviceManager, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    CFRelease(self.deviceManager);
    if (self.userDevice) {
        CFRelease(self.userDevice);
    }

    [super tearDown];
}

- (void)testConstantLogicalDescriptor {
    NSData * descriptorData = [[NSData alloc] initWithBytes:constantLogicalDescriptor length:sizeof(constantLogicalDescriptor)];

    NSDictionary * configDict = @{@kIOHIDPhysicalDeviceUniqueIDKey  : self.uniqueID,
                                  @kIOHIDReportDescriptorKey        : descriptorData};
    self.userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)configDict);
    HIDXCTAssertAndThrowTrue(self.userDevice != nil);

    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[self.deviceMatchedExpection] timeout:10];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "%@",
                                self.deviceMatchedExpection);
}

@end

void HIDManagerDeviceAddedCallback(void * _Nullable context, IOReturn  result __unused, void * _Nullable  sender __unused, IOHIDDeviceRef device __unused) {
    TestNonStandardDescriptor *self = (__bridge TestNonStandardDescriptor *)context;

    [self.deviceMatchedExpection fulfill];
}
