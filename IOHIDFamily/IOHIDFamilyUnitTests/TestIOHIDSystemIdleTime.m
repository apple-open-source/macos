//
//  TestIOHIDSystemActivity.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 8/21/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDXCTestExpectation.h"
#import "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDUserDevice.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#import <IOKit/hidsystem/event_status_driver.h>
#import <notify.h>
#include <IOKit/pwr_mgt/IOPMLibPrivate.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestIOHIDSystemIdleTime : XCTestCase 

@property IOHIDEventSystemClientRef         eventSystem;
@property IOHIDUserDeviceRef                userDevice;
@property io_service_t                      hidSystemService;
@property IOHIDXCTestExpectation            * testServiceExpectation;
@property IOHIDXCTestExpectation            * testEventExpectation;
@property IOHIDXCTestExpectation            * testHIDActivityExpectation;

@end

@implementation TestIOHIDSystemIdleTime

- (void)setUp {

    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.eventSystem = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertAndThrowTrue(self.eventSystem != NULL);
    
    NSDictionary *matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID};
    
    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);
    
    self.testServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Test HID service"];
    self.testHIDActivityExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Test HID activity"];

    self.testEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Test HID event"];
    self.testEventExpectation.expectedFulfillmentCount = 2;
    
    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
        [self.testServiceExpectation fulfill];
    };
    
    IOHIDEventSystemClientRegisterDeviceMatchingBlock(self.eventSystem , handler, NULL, NULL);
    
    IOHIDEventSystemClientRegisterEventBlock(self.eventSystem, ^(void * _Nullable target __unused, void * _Nullable refcon __unused, void * _Nullable sender __unused, IOHIDEventRef  _Nonnull event) {
        NSLog(@"Event: %@", event);
        if (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard) {
            if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)) {
                [self.testEventExpectation fulfill];
            }
        }
    }, NULL,  NULL);
    
    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystem, dispatch_get_main_queue());
    
    CFTypeRef val = IOHIDEventSystemClientCopyProperty(self.eventSystem, CFSTR (kIOHIDEventSystemClientIsUnresponsive));
    if (val) {
        CFRelease(val);
    }
 
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   };
    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)description);

    self.hidSystemService = IORegistryEntryFromPath(kIOMasterPortDefault, kIOServicePlane ":/IOResources/IOHIDSystem" );
    
}

- (void)tearDown {
 
    if (self.userDevice) {
        CFRelease(self.userDevice);
    }
    
    if (self.eventSystem) {
        CFRelease(self.eventSystem);
    }
    
    if (self.hidSystemService) {
        IOObjectRelease(self.hidSystemService);
    }
}

- (void)testIOHIDSystemIdleTime {
    IOReturn        status;
    XCTWaiterResult result;
    id              value1;
    id              value2;
    
    XCTAssert(self.hidSystemService != IO_OBJECT_NULL, "hidSystemService:%d", self.hidSystemService);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    XCTAssert(result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testServiceExpectation);
 
 
    value1 = CFBridgingRelease(IORegistryEntryCreateCFProperty(self.hidSystemService, CFSTR (kIOHIDIdleTimeKey), kCFAllocatorDefault, 0));
    HIDXCTAssertWithParameters(RETURN_FROM_TEST,
                               value1  && [value1  isKindOfClass:[NSNumber class]],
                               "value:%@",
                               value1);
    
    sleep(1);
    
    value2 = CFBridgingRelease(IORegistryEntryCreateCFProperty(self.hidSystemService, CFSTR (kIOHIDIdleTimeKey), kCFAllocatorDefault, 0));
    HIDXCTAssertWithParameters(RETURN_FROM_TEST,
                               value2  && [value2  isKindOfClass:[NSNumber class]] && ((NSNumber *)value1).unsignedLongLongValue != ((NSNumber *)value2).unsignedLongLongValue,
                               "value1:%@ value2:%@",
                               value1, value2);

    
    NSLog(@"value1:%@ value2:%@", value1, value2);
    
    
    sleep (3);

    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));

    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    XCTAssert(status == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", status);
    
    report.KB_Keyboard[0] = 0;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    XCTAssert(status == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", status);

    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    XCTAssert(status == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", status);
    
    report.KB_Keyboard[0] = 0;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    XCTAssert(status == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", status);

    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testEventExpectation);


    for (NSInteger index = 0; index < 10 ; index++) {
        value2 = CFBridgingRelease(IORegistryEntryCreateCFProperty(self.hidSystemService, CFSTR (kIOHIDIdleTimeKey), kCFAllocatorDefault, 0));

        NSLog(@"[%ld] value2:%@", (long)index, value2);
        if (![value2  isKindOfClass:[NSNumber class]]) {
            break;
        }
        if (((NSNumber *)value2).unsignedLongLongValue < NSEC_PER_SEC) {
            break;
        }
       
        usleep (50000);
    }
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE | COLLECT_HIDUTIL | COLLECT_IOREG,
                                   value2  && [value2  isKindOfClass:[NSNumber class]] && ((NSNumber *)value2).unsignedLongLongValue < NSEC_PER_SEC,
                                   "value2:%@",
                                   value2);
}

@end
