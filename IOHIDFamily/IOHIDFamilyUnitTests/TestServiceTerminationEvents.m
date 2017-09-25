//
//  TestServiceTerminationEvents.m
//  IOHIDFamily
//
//  Created by YG on 10/25/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventSystemTestController.h"
#import "IOHIDUserDeviceTestController.h"
#import "IOHIDDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestServiceTerminationEvents : XCTestCase

@property IOHIDEventSystemTestController *  eventController;
@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  rootQueue;
@property dispatch_queue_t                  eventControllerQueue;

@property NSInteger                         eventCount;
@property NSInteger                         filterCount;

@end

@implementation TestServiceTerminationEvents

- (void)setUp {
    [super setUp];

    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
  
    self.eventControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    XCTAssertNotNil(self.eventControllerQueue);

    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
  
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:nil];
    //
    // Ise EVAL to make sure XCTAssertTrue will ot hold ref to sourceController.
    //
    XCTAssertTrue(EVAL(self.sourceController != nil), "Event source controller is nil");

    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID AndQueue:self.eventControllerQueue];
    XCTAssertNotNil (self.eventController);
  

}

- (void)tearDown {
  
    @autoreleasepool {
        self.sourceController = nil;
        self.eventController = nil;
    }
  
    [super tearDown];
}

- (void)testKeyboardServiceTerminationEvent {

    IOReturn status;
    
    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));
    
    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;

    status = [self.sourceController handleReport: (uint8_t *)&report Length:sizeof(report) andInterval:0];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    @autoreleasepool {
      self.sourceController = nil;
    }
  
    // Wait for device & service to terminate
    sleep(4);

    // Make copy
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }

    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];

    HIDTestEventLatency(stats);

    // Check events
    XCTAssertTrue(stats.totalCount > 1,
        "events count:%lu expected:>%d", (unsigned long)stats.totalCount , 1);
  
    XCTAssertTrue (stats.counts[kIOHIDEventTypeNULL] == 1, "kIOHIDEventTypeNULL count: %lu", (unsigned long)stats.counts[kIOHIDEventTypeNULL]);

    // Check if any key left in down state
    NSMutableSet * usages = [[NSMutableSet alloc] init];
    for (NSUInteger index = 0; index < events.count; ++index) {
        IOHIDEventRef event = (__bridge IOHIDEventRef)events[index];
        if (IOHIDEventConformsTo(event, kIOHIDEventTypeKeyboard)) {
            CFIndex usage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
            CFIndex down  = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardDown);
            if (down) {
                [usages addObject: @(usage)];
            } else {
                [usages removeObject : @(usage)];
            }
        }
    }
    XCTAssert (usages.count == 0, "Outstanding usages:%@", usages);
}

@end
