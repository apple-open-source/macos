//
//  TestIOHIDPostEvent.m
//  IOHIDFamilyUnitTests
//
//  Created by Abhishek Nayyar on 3/5/18.
//

#import <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDEventSystemClientPrivate.h>
#include <IOKit/hid/IOHIDEventSystemKeysPrivate.h>
#include <IOKit/hid/IOHIDEventSystem.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include "AppleHIDUsageTables.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#import <IOKit/hidsystem/event_status_driver.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDXCTestExpectation.h"
#include <TargetConditionals.h>

#define HIDPostEventTestThreshold 10

@interface TestIOHIDPostEvent : XCTestCase

@property IOHIDEventSystemClientRef   eventSystem;
@property NXEventHandle               eventDriver;
@property IOHIDXCTestExpectation      * testEventExpectation;

-(void) handleIOHIDPostEvent:(IOHIDEventRef) event;

@end

@implementation TestIOHIDPostEvent

- (void)setUp {
    
    [super setUp];
    
    _eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);

    HIDXCTAssertAndThrowTrue(_eventSystem != NULL);
    
    IOHIDEventSystemClientRegisterEventBlock(_eventSystem, eventBlock, (__bridge void*)self, NULL);
    
    NSDictionary *matchingDict = @{@kIOHIDServiceDeviceUsagePageKey : @(kHIDPage_AppleVendor), @kIOHIDServiceDeviceUsageKey : @(kHIDUsage_AppleVendor_NXEvent)};
    IOHIDEventSystemClientSetMatching(_eventSystem, (__bridge CFDictionaryRef)(matchingDict));
    IOHIDEventSystemClientScheduleWithRunLoop(_eventSystem, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDEventSystemClientCopyProperty(_eventSystem, CFSTR (kIOHIDEventSystemClientIsUnresponsive));

    self.testEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"expectation: Test HID event"];
    self.testEventExpectation.expectedFulfillmentCount = 2;
    
    _eventDriver = NXOpenEventStatus();
}

- (void)tearDown {

    [super tearDown];
    
    if (_eventSystem) {
        IOHIDEventSystemClientUnscheduleWithRunLoop(_eventSystem, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
        CFRelease(_eventSystem);
    }
    
    if (_eventDriver) {
        NXCloseEventStatus(_eventDriver);
    }
}

IOHIDEventBlock eventBlock = ^(void * target __unused, __unused void * refcon, void * sender __unused, IOHIDEventRef event)
{
    TestIOHIDPostEvent *me = (__bridge TestIOHIDPostEvent*)target;
    [me handleIOHIDPostEvent:event];
};

- (void) testIOHIDPostEvent {
    
    kern_return_t   kr;
    NXEventData     event;
    IOGPoint        loc;
    XCTWaiterResult result;

    HIDXCTAssertWithParameters(RETURN_FROM_TEST,  MACH_PORT_NULL != _eventDriver);
    
    loc.x = 200;
    loc.y = 220;
    
    //set key  event
    event.key.repeat = FALSE;
    event.key.keyCode = 0;
    event.key.charSet = NX_ASCIISET;
    event.key.charCode = 'a';
    event.key.origCharSet = event.key.charSet;
    event.key.origCharCode = event.key.charCode;
    
    kr = IOHIDPostEvent ( _eventDriver, NX_KEYUP, loc, &event, FALSE, 0, FALSE );
    XCTAssert( KERN_SUCCESS == kr, "kr:%x", kr);
    
    kr = IOHIDPostEvent ( _eventDriver, NX_KEYDOWN, loc, &event, FALSE, 0, FALSE );
    XCTAssert( KERN_SUCCESS == kr, "kr:%x", kr);
    
    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:HIDPostEventTestThreshold];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST, result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testEventExpectation);
}

-(void) handleIOHIDPostEvent:(IOHIDEventRef) event
{
    NSLog(@"Event: %@", event);
    
    XCTAssert(IOHIDEventConformsTo (event, kIOHIDEventTypeVendorDefined));
    
    CFIndex usage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage);
    
    XCTAssert(usage == kHIDUsage_AppleVendor_NXEvent);
    
    CFIndex usagePage  = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsagePage);
    
    XCTAssert(usagePage == kHIDPage_AppleVendor);
    
    NXEventExt *nxEvent = NULL;
    CFIndex nxEventLength = 0;
    IOHIDEventGetVendorDefinedData (event, (uint8_t**)&nxEvent, &nxEventLength);
    
    XCTAssert(nxEvent);
    
    XCTAssert(nxEventLength == sizeof(NXEventExt), "lenght:%d", (int)nxEventLength);
    
    XCTAssert(nxEvent->payload.location.x == 200);
    XCTAssert(nxEvent->payload.location.y == 220);
    XCTAssert(nxEvent->payload.data.key.charSet == NX_ASCIISET);
    
    //verify audit trailer (pid)
    XCTAssert((pid_t)nxEvent->extension.audit.val[5] == getpid(), "pid:%d", nxEvent->extension.audit.val[5]);
    
    [self.testEventExpectation fulfill];
}

@end
