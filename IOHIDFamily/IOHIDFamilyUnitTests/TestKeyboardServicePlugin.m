//
//  TestKeyRepeats.m
//  IOHIDFamily
//
//  Created by YG on 10/31/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventSystemTestController.h"
#import "IOHIDUserDeviceTestController.h"
#import "IOHIDDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"


typedef struct {
    uint32_t initialDelay;
    uint32_t repeatDelay;
} DELAY_RANGE;

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestKeyboardServicePlugin : XCTestCase

@property IOHIDEventSystemTestController *  eventController;
@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  rootQueue;
@property dispatch_queue_t                  eventControllerQueue;

@end

@implementation TestKeyboardServicePlugin

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

    [self.eventController  invalidate];
    [self.sourceController invalidate];
    
    @autoreleasepool {
        self.sourceController = nil;
        self.eventController = nil;
    }
    [super tearDown];
}



- (void)checkRepeats: (uint32_t) initialDelayMS :(uint32_t) repeatDelayMS {

    IOReturn status;
    
    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));
    
    NSNumber *valueInitialRepeat = @(MS_TO_NS(initialDelayMS));
    NSNumber *valueRepeatDelay = @(MS_TO_NS(repeatDelayMS));
    
    uint32_t numberOfRepeats = 10;
  
    // Allow event to be dispatched
    usleep(kDefaultReportDispatchCompletionTime);

    TestLog("initialDelayMS: %d, repeatDelayMS:%d", initialDelayMS, repeatDelayMS);
    
    IOHIDServiceClientSetProperty(self.eventController.eventService, CFSTR(kIOHIDServiceInitialKeyRepeatDelayKey), (__bridge CFTypeRef _Nonnull)(valueInitialRepeat));
    IOHIDServiceClientSetProperty(self.eventController.eventService, CFSTR(kIOHIDServiceKeyRepeatDelayKey), (__bridge CFTypeRef _Nonnull)(valueRepeatDelay));

    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:0];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    report.KB_Keyboard[0] = 0x00;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval: (MS_TO_US(initialDelayMS) + numberOfRepeats * MS_TO_US(repeatDelayMS))];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);

    sleep (2);

    // Make copy
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }

    // Check if event system reset occur
    XCTAssert(self.eventController.eventSystemResetCount == 0, "IOHIDEventSystem resetCount:%d", (int)self.eventController.eventSystemResetCount);

    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];
 
    HIDTestEventLatency(stats);

    XCTAssertTrue(VALUE_IN_RANGE(stats.counts[kIOHIDEventTypeKeyboard], numberOfRepeats , numberOfRepeats + 3),
                  "keyboard events count:%lu expected:%d events:%@",
                  (unsigned long)stats.counts[kIOHIDEventTypeKeyboard],
                  (numberOfRepeats + 3),
                  events
      );
  
    uint64_t prev =  IOHIDEventGetTimeStamp((IOHIDEventRef)events[0]);
    uint64_t initialDelayNS = MS_TO_NS(initialDelayMS);
    uint64_t repeatDelayNS  = MS_TO_NS(repeatDelayMS);
    
    for (size_t index = 1; index < (events.count - 1); index++) {
        uint64_t next =  IOHIDEventGetTimeStamp((IOHIDEventRef)events[index]);
        uint64_t delta = IOHIDInitTestAbsoluteTimeToNanosecond (next) - IOHIDInitTestAbsoluteTimeToNanosecond (prev);
        if (index == 1) {
            XCTAssertTrue (VALUE_IN_RANGE(delta , VALUE_PST(initialDelayNS,-20) , VALUE_PST(initialDelayNS,+20)) ,
                           "Index:%zu Initial  repeat:%lluns expected:%lluns - %lluns events:%@",
                           index,
                           delta ,
                           VALUE_PST(initialDelayNS,-20),
                           VALUE_PST(initialDelayNS,+20),
                           events
                           );
        } else {
            XCTAssertTrue (VALUE_IN_RANGE(delta , VALUE_PST(repeatDelayNS,-20) , VALUE_PST(repeatDelayNS,+20)),
                           "Index:%zu repeat: %lluns  expected:%lluns - %lluns events:%@",
                           index,
                           delta ,
                           VALUE_PST(repeatDelayNS,-20),
                           VALUE_PST(repeatDelayNS,+20),
                           events
                           );
        }
        prev = next;
    }
  
}

- (void) MAC_OS_ONLY_TEST_CASE(testKeyRepeat) {
    DELAY_RANGE  ranges [] = {
      {250 , 33},
      {500 , 66}
    };
  
    for (size_t index = 0; index < (sizeof (ranges) / sizeof(ranges[0]));index++){
        [self. eventController.events removeAllObjects];
        [self checkRepeats:ranges[index].initialDelay :ranges[index].repeatDelay];
    }
}

@end
