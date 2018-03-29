//
//  TestEventDriverAndVendorEvent.m
//  IOHIDFamily
//
//  Created by YG on 10/24/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"

#define kVendorMessageBitLength                128
#define kVendorPayloadBitLength                128
#define kVendorMessageReportID                 1
#define kVendorPayloadReportID                 2

typedef struct __attribute__ ((packed)) {
  uint8_t id;
  uint8_t data [kVendorMessageBitLength/8];
} REPORT_WITH_ID;

static uint8_t descriptor[] = {
    0x06, 0x00, 0xFF,                         // Usage Page (65280)
    0x09, 0x23,                               // Usage 35 (0x23) 
    0xA1, 0x01,                               // Collection (Application) 
    0x85, kVendorMessageReportID,             //   ReportID................ (VendorMessageReportID)
    0x09, 0x23,                               //   Usage 35 (0x23)
    0x06, 0x00, 0xFF,                         //   Usage Page (65280) 
    0x95, 0x01,                               //   Report Count............ (1)  
    0x75, kVendorMessageBitLength,            //   Report Size............. (VendorMessageBitLength)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute)
    0x06, 0x00, 0xFF,                         // Usage Page (65280)
    0x09, 0x25,                               // Usage 37 (0x25)
    0xA1, 0x01,                               // Collection (Application)
    0x85, kVendorPayloadReportID,             //   ReportID................ (VendorPayloadReportID)
    0x09, 0x25,                               //   Usage 37 (0x25)
    0x06, 0x00, 0xFF,                         //   Usage Page (65280) 
    0x95, 0x01,                               //   Report Count............ (1)  
    0x75, kVendorPayloadBitLength,            //   Report Size............. (VendorPayloadBitLength)
    0x81, 0x02,                               //   Input...................(Data, Variable, Absolute) 
    0xC0,                                     // End Collection
    0xC0,                                     // End Collection
};

@interface TestVendorMessageAndVendorPayload : XCTestCase

@property IOHIDDeviceTestController *       deviceController;
@property IOHIDEventSystemTestController *  eventController;
@property IOHIDUserDeviceTestController *   sourceController;

@property CFRunLoopRef                      deviceControllerRunloop;
@property dispatch_queue_t                  eventControllerQueue;
@property dispatch_queue_t                  rootQueue;

@end

@implementation TestVendorMessageAndVendorPayload

- (void)setUp {
    [super setUp];
  
    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
  
    self.eventControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.eventControllerQueue != nil);

    self.deviceControllerRunloop = IOHIDUnitTestCreateRunLoop (31);
    HIDXCTAssertAndThrowTrue(self.deviceControllerRunloop != NULL);

  
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
  
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:nil];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);

    self.deviceController = [[IOHIDDeviceTestController alloc] initWithDeviceUniqueID:uniqueID :self.deviceControllerRunloop];
    HIDXCTAssertAndThrowTrue(self.deviceController != nil);

    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(self.eventController != nil);

    NSArray * elementMatching = @[
        @{
          @kIOHIDElementUsagePageKey:@(kHIDPage_AppleVendor),
          @kIOHIDElementUsageKey:@(kHIDUsage_AppleVendor_Message) ,
        },
        @{
          @kIOHIDElementUsagePageKey:@(kHIDPage_AppleVendor),
          @kIOHIDElementUsageKey:@(kHIDUsage_AppleVendor_Payload) ,
        }
    ];
    IOHIDDeviceSetInputValueMatchingMultiple(self.deviceController.device, (CFArrayRef)elementMatching);
}

- (void)tearDown {
    
    [self.eventController  invalidate];
    [self.sourceController invalidate];
    [self.deviceController invalidate];

    @autoreleasepool {
        self.sourceController = nil;
        self.eventController  = nil;
        self.deviceController = nil;
    }

    if (self.deviceControllerRunloop) {
        IOHIDUnitTestDestroyRunLoop(self.deviceControllerRunloop);
    }

    [super tearDown];
}

- (void)testVendorDefinedMessageEvents {

    IOReturn status;
    
    REPORT_WITH_ID report;
  
    for (NSUInteger length = 1; length < sizeof(report.data); length++) {
        report.id = kVendorMessageReportID;
        for (uint8_t i = 0; i < length; i++) {
          report.data[i] = i;
        }
        NSData * reportData =  [[NSData alloc] initWithBytes:&report length: (sizeof(report.data) + 1)];
        status = [self.sourceController handleReport:reportData withInterval:2000];
        XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    }
  
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
  
    // Check if event system reset occur
    XCTAssert(self.eventController.eventSystemResetCount == 0, "IOHIDEventSystem resetCount:%d", (int)self.eventController.eventSystemResetCount);
  
    // Make copy
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }

    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];

    HIDTestEventLatency(stats);

    XCTAssertTrue(stats.totalCount == (sizeof(report.data) - 1),
        "events count:%lu expected:%lu", (unsigned long)events.count, sizeof(report.data) - 1
        );

    for (NSUInteger index = 0; index < events.count; ++index) {
        IOHIDEventRef event = (__bridge IOHIDEventRef)events[index];
      
        XCTAssertTrue(IOHIDEventConformsTo(event, kIOHIDEventTypeVendorDefined));
      
        NSArray* children = (NSArray*)IOHIDEventGetChildren (event);
        XCTAssertFalse(children && children.count !=0);
      
        uint8_t *payload  = NULL;
        CFIndex  lenght   = 0;
      
        IOHIDEventGetVendorDefinedData (event, &payload, &lenght);
        XCTAssertTrue(payload && ((NSUInteger)lenght == sizeof(report.data)));
    }
}

- (void)testVendorDefinedPayloadElements {
    
    IOReturn status;
    
    REPORT_WITH_ID report;

    for (NSUInteger length = 1; length < sizeof(report.data); length++) {
        report.id = kVendorPayloadReportID;
        for (uint8_t i = 0; i < length; i++) {
          report.data[i] = length;
        }
        NSData * reportData =  [[NSData alloc] initWithBytes:&report length: (sizeof(report.data) + 1)];
        status = [self.sourceController handleReport:reportData withInterval:0];
        XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    }
  
    // Allow elements value to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);
  
    // Make copy
    NSArray *values = nil;
    @synchronized (self.deviceController.values) {
        values = [self.deviceController.values copy];
    }

    // Check values
    XCTAssertTrue(values && values.count == (sizeof(report.data) - 1),
        "values count:%lu expected:%lu", (unsigned long)values.count, sizeof(report.data) - 1
        );
  
    for (NSUInteger index = 0; index < values.count; ++index) {
        NSDictionary *value = values[index];
        NSData *data = value[@"data"];
        XCTAssertTrue (data && data.length == sizeof(report.data), "Data %@, Length:%lu", data, (unsigned long)data.length);
        IOHIDElementRef element = (__bridge IOHIDElementRef)value[@"element"];
        XCTAssertTrue (element && IOHIDElementGetUsagePage (element) == kHIDPage_AppleVendor && IOHIDElementGetUsage (element) == kHIDUsage_AppleVendor_Payload);
    }

}

@end
