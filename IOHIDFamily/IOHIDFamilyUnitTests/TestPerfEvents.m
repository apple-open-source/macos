//
//  TestPerfEvents.m
//  IOHIDFamily
//
//  Created by YG on 11/28/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDPrivateKeys.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"
#import  "IOHIDUnitTestDescriptors.h"


static uint8_t descriptor[] = {
    HIDVendorMessage32BitDescriptor
};

@interface TestPerfEvents : XCTestCase

@property IOHIDEventSystemTestController *  eventController;
@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  eventControllerQueue;
@property dispatch_queue_t                  rootQueue;

@end

@implementation TestPerfEvents

- (void)setUp {
    [super setUp];

    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);

    self.eventControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.eventControllerQueue != nil);

    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
  
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:nil];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);

    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(self.eventController != nil);
}

- (void)tearDown {
    Boolean ret;
    
    NSNumber *debugMask = @(0x00);
 
    ret = IOHIDEventSystemClientSetProperty(self.eventController.eventSystemClient, CFSTR(kIOHIDDebugConfigKey), (__bridge CFTypeRef _Nonnull)(debugMask));
    XCTAssert(ret ==true);
    
    [self.eventController  invalidate];
    [self.sourceController invalidate];

    @autoreleasepool {
        self.sourceController = nil;
        self.eventController = nil;
    }
    [super tearDown];
}

- (void)testPerfEvent {
    IOReturn status;
    Boolean ret;
    
    HIDVendorMessage32BitDescriptorInputReport report = {0x01010101};
    
    NSNumber *debugMask = @(0x10);
    ret = IOHIDEventSystemClientSetProperty(self.eventController.eventSystemClient,CFSTR(kIOHIDDebugConfigKey), (__bridge CFTypeRef _Nonnull)(debugMask));
    XCTAssert(ret == true);
    
    status = [self.sourceController handleReport:(uint8_t*)&report Length: sizeof(report) andInterval:0];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

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

    XCTAssertTrue(stats.totalCount == 1);
    
    NSArray *children = (NSArray*)IOHIDEventGetChildren ((IOHIDEventRef)events[0]);
    
    XCTAssertTrue(children && children.count == 1);
    
    if (children.count) {
        XCTAssertTrue(IOHIDEventGetIntegerValue((IOHIDEventRef)children[0],kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendor_Perf);
    }
}

@end
