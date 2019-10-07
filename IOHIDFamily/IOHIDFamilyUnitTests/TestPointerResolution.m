//
//  TestPointerResolution.m
//  IOHIDFamilyUnitTests
//
//  Created by Matt Dekom on 11/25/18.
//

#import <Foundation/Foundation.h>

#import <XCTest/XCTest.h>
#import <HID/HID.h>
#import "IOHIDUnitTestDescriptors.h"
#import "IOHIDUnitTestUtility.h"
#import <IOKit/IOKitLib.h>
#import <IOKit/hid/IOHIDKeys.h>
#import <math.h>
#import "HIDEventAccessors_Private.h"

#pragma clang diagnostic ignored "-Wunused-parameter"

static uint8_t appleMouseDesc[] = {
    0x05, 0x01,                               // Usage Page (Generic Desktop)
    0x09, 0x02,                               // Usage (Mouse)
    0xA1, 0x01,                               // Collection (Application)
    0x05, 0x01,                               //   Usage Page (Generic Desktop)
    0x09, 0x01,                               //   Usage (Pointer)
    0xA1, 0x00,                               //   Collection (Physical)
    0x16, 0x01, 0xFE,                         //     Logical Minimum......... (-511)
    0x26, 0xFF, 0x01,                         //     Logical Maximum......... (511)
    0x36, 0xC0, 0xFE,                         //     Physical Minimum........ (-320)
    0x46, 0x40, 0x01,                         //     Physical Maximum........ (320)
    0x65, 0x13,                               //     Unit.................... (19)
    0x55, 0x0D,                               //     Unit Exponent........... (13)
    0x09, 0x30,                               //     Usage (X)
    0x09, 0x31,                               //     Usage (Y)
    0x75, 0x10,                               //     Report Size............. (16)
    0x95, 0x02,                               //     Report Count............ (2)
    0x81, 0x06,                               //     Input...................(Data, Variable, Relative)
    0x95, 0x01,                               //     Report Count............ (1)
    0x75, 0x08,                               //     Report Size............. (8)
    0x15, 0x81,                               //     Logical Minimum......... (-127)
    0x25, 0x7F,                               //     Logical Maximum......... (127)
    0x09, 0x38,                               //     Usage (Wheel)
    0x81, 0x06,                               //     Input...................(Data, Variable, Relative)
    0x95, 0x01,                               //     Report Count............ (1)
    0x05, 0x0C,                               //     Usage Page (Consumer)
    0x0A, 0x38, 0x02,                         //     Usage (568 (0x238))
    0x81, 0x06,                               //     Input...................(Data, Variable, Relative) 
    0xC0,                                     //   End Collection
    0xC0,                                     // End Collection
};

@interface TestPointerResolution : XCTestCase {
    HIDUserDevice           *_userDevice;
    HIDDevice               *_device;
    HIDEventSystemClient    *_client;
}

@property XCTestExpectation *servAddedExp;
@property XCTestExpectation *eventExp;

@end

@implementation TestPointerResolution

- (void)setUp
{
    [super setUp];
    __weak TestPointerResolution *self_ = self;
    
    NSString *uuid = [[[NSUUID alloc] init] UUIDString];
    _servAddedExp = [[XCTestExpectation alloc] initWithDescription:@"service added"];
    _eventExp = [[XCTestExpectation alloc] initWithDescription:@"event exp"];
    
    NSData *descriptor = [[NSData alloc] initWithBytes:appleMouseDesc
                                                length:sizeof(appleMouseDesc)];
    NSMutableDictionary *properties = [[NSMutableDictionary alloc] init];
    
    properties[@kIOHIDReportDescriptorKey] = descriptor;
    properties[@kIOHIDPhysicalDeviceUniqueIDKey] = uuid;
    
    _client = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    
    [_client setMatching:@{ @kIOHIDPhysicalDeviceUniqueIDKey : uuid }];
    [_client setServiceNotificationHandler:^(HIDServiceClient *service) {
        NSLog(@"Service added:\n%@", service);
        [self_.servAddedExp fulfill];
    }];
    [_client setEventHandler:^(HIDServiceClient * service, HIDEvent *event) {
        NSLog(@"received event:\n%@", event);
        if (event.scrollX == 1.0) {
            [self_.eventExp fulfill];
        }
    }];
    [_client setDispatchQueue:dispatch_get_main_queue()];
    [_client activate];
    
    _userDevice = [[HIDUserDevice alloc] initWithProperties:properties];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _userDevice);
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_servAddedExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL,
                               result == XCTWaiterResultCompleted);
    
    _device = [[HIDDevice alloc] initWithService:_userDevice.service];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _device);
}

- (void)tearDown
{
    [_client cancel];
    [super tearDown];
}

- (void)testPointerResolution
{
    NSNumber *resolution = nil;
    IOFixed calculatedResolution = 0;
    NSArray *elements;
    HIDElement *element;
    NSInteger logicalDiff, physicalDiff, exponent;
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL, _userDevice && _device);
    
    elements = [_device elementsMatching:@{@(kIOHIDElementUsageKey): @(kHIDUsage_GD_X)}];
    XCTAssert(elements && elements.count == 1);
    
    element = [elements objectAtIndex:0];
    
    logicalDiff = element.logicalMax - element.logicalMin;
    physicalDiff = element.physicalMax - element.physicalMin;
    exponent = (NSInteger)pow(10, 0x10 - element.unitExponent);
    
    calculatedResolution = (IOFixed)(logicalDiff * exponent / physicalDiff) << 16;
    
    resolution = [_device propertyForKey:@(kIOHIDPointerResolutionKey)];
    NSLog(@"device resolution: %@ calculated: %d", resolution, calculatedResolution);
    
    XCTAssert(resolution && (IOFixed)resolution.unsignedLongValue == calculatedResolution);
}

- (void)testHorizontalScroll
{
    struct MouseReport {
        uint16_t x;
        uint16_t y;
        uint8_t scrollY;
        uint8_t scrollX;
    } report = { 0 };
    
    report.scrollX = 0xff;
    
    NSData *reportData = [NSData dataWithBytes:(void *)&report length:sizeof(report)];
    
    [_userDevice handleReport:reportData error:nil];
    
    XCTWaiterResult result = [XCTWaiter waitForExpectations:@[_eventExp] timeout:5];
    HIDXCTAssertWithParameters(COLLECT_LOGARCHIVE | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted);
}

@end
