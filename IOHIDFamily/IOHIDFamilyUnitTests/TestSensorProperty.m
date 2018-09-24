//
//  TestSensorProperty.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 4/17/18.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDPrivateKeys.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"
#import  "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDServiceKeys.h>

static uint8_t descriptor[] = {
    HIDAccel
};


@interface TestSensorProperty : XCTestCase <IOHIDUserDeviceObserver>

@property IOHIDEventSystemTestController *  eventController;
@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  queue;
@property dispatch_queue_t                  rootQueue;
@property uint32_t                          reportInterval;
@property uint32_t                          reportLatency;

@end

@implementation TestSensorProperty

- (void)setUp {
    [super setUp];
    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    self.queue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.queue != nil);
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:self.queue];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);
    
    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID :self.queue :kIOHIDEventSystemClientTypeRateControlled];
    HIDXCTAssertAndThrowTrue(self.eventController != nil);

    self.sourceController.userDeviceObserver = self;
}

- (void)tearDown {

    [self.eventController  invalidate];
    [self.sourceController invalidate];
    
    @autoreleasepool {
        self.sourceController = nil;
        self.eventController = nil;
    }

    [super tearDown];
}

- (void)testSensorProperty {
    id value;
    
    
    value = @(100000);
    IOHIDServiceClientSetProperty(self.eventController.eventService, CFSTR(kIOHIDServiceReportIntervalKey), (CFTypeRef) value);
    value = @(1000000);
    IOHIDServiceClientSetProperty(self.eventController.eventService, CFSTR(kIOHIDServiceBatchIntervalKey), (CFTypeRef) value);

   usleep(kDefaultReportDispatchCompletionTime);

    XCTAssert (self.reportInterval == 100000, "reportInterval:%d", self.reportInterval);
    XCTAssert (self.reportLatency == 1000000, "reportLatency:%d", self.reportLatency);
}


- (void)testSensorBatching {
    id value;
    
    value = @(100000);
    IOHIDServiceClientSetProperty(self.eventController.eventService, CFSTR(kIOHIDServiceReportIntervalKey), (CFTypeRef) value);
    value = @(1000000);
    IOHIDServiceClientSetProperty(self.eventController.eventService, CFSTR(kIOHIDServiceBatchIntervalKey), (CFTypeRef) value);

    HIDAccelInputReport01 report;
    report.reportId = 1;
    for (NSUInteger index = 0; index < 100; index++) {
        report.MotionAccelerometer3D000B = index;
        [self.sourceController handleReport:(uint8_t *) &report  Length:sizeof(report) andInterval:0];
    }
    
    usleep(kDefaultReportDispatchCompletionTime);
    
    // Make copy
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
    
    XCTAssert (stats.counts[kIOHIDEventTypeAccelerometer] == 100, "Event count:%d", stats.counts[kIOHIDEventTypeAccelerometer]);
}


-(IOReturn) GetReportCallback: (IOHIDReportType __unused) type : (uint32_t __unused) reportID : (uint8_t * _Nonnull __unused) report : (CFIndex * _Nonnull __unused) reportLength
{
    return kIOReturnUnsupported;
}

-(IOReturn) SetReportCallback: (IOHIDReportType) type : (uint32_t) reportID : (uint8_t *_Nonnull) report : (CFIndex) reportLength
{
    TestLog (@"report type:%d id:%d length:%d", (unsigned int) type, (unsigned int) reportID, (unsigned int)reportLength);
    if (kIOHIDReportTypeFeature == type && reportLength >= 4) {
        if (reportID == 1 && kIOHIDReportTypeFeature == type) {
            self.reportInterval = *(uint32_t *)(report + 1);
        }
        if (reportID == 2 && kIOHIDReportTypeFeature == type) {
            self.reportLatency = *(uint32_t *) (report + 1);
        }
    }
    return  kIOReturnSuccess;
}

@end
