//
//  TestIOHIDConnectionFilterPlugin.m
//  IOHIDFamilyUnitTests
//
//  Created by Abhishek Nayyar on 3/29/18.
//

#import <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDEventSystemClientPrivate.h>
#include <IOKit/hid/IOHIDEventSystem.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <os/log.h>
#include "IOHIDUnitTestUtility.h"
#include "AppleHIDUsageTables.h"
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <IOKit/hid/IOHIDEvent.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>

#include <IOKit/IOKitLib.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#import "IOHIDUserDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"
#include <TargetConditionals.h>

#define kDefaultRunLoopTime 7
#define kUsages     "Usages"
#define kUsagePage "UsagePage"

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestIOHIDConnectionFilterPlugin : XCTestCase
@property IOHIDEventSystemClientRef   eventSystem;
@property IOHIDUserDeviceTestController *   sourceController;
-(void) handleEvent:(IOHIDEventRef) event;
@end

@implementation TestIOHIDConnectionFilterPlugin
{
    UInt32 _eventCount;
    NSDictionary *attributes;
    NSArray *usageFilter;
}

- (void)setUp {
    [super setUp];
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:nil];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);
    
    attributes = @{(__bridge_transfer NSString*)kCFBundleIdentifierKey : @("com.apple.IOHIDTestConnectionFilter")};
    
    _eventSystem = IOHIDEventSystemClientCreateWithType(kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, (__bridge CFDictionaryRef)attributes);
    
    HIDXCTAssertAndThrowTrue(_eventSystem != NULL);
    
}

- (void)tearDown {
    [self.sourceController invalidate];
    
    @autoreleasepool {
        self.sourceController = nil;
    }
    
    IOHIDEventSystemClientUnscheduleWithRunLoop(_eventSystem, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    
    if (_eventSystem) {
        CFRelease(_eventSystem);
    }
    
    [super tearDown];
}
IOHIDEventBlock keyboardEventBlock = ^(void * target __unused, __unused void * refcon, void * sender __unused, IOHIDEventRef event)
{
    TestIOHIDConnectionFilterPlugin *me = (__bridge TestIOHIDConnectionFilterPlugin*)target;
    [me handleEvent:event];
};
- (void)testIOHIDConnectionFilterPlugin {
    
    IOReturn status;
    
    IOHIDEventSystemClientRegisterEventBlock(_eventSystem, keyboardEventBlock, (__bridge void*)self, NULL);
    
    // set property for which you want to filter
    
    // Set usage page you want to filter
    // Set usage for usage page you want to filter
    
    usageFilter = @[@{@kUsagePage : @(kHIDPage_KeyboardOrKeypad)},@{@kUsages : @[@(kHIDUsage_KeyboardEqualSign)]}];
    
    IOHIDEventSystemClientSetProperty(_eventSystem, CFSTR(kIOHIDServiceDeviceDebugUsageFilter), (__bridge CFTypeRef)usageFilter);
    
    
    
    IOHIDEventSystemClientScheduleWithRunLoop(_eventSystem, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDEventSystemClientCopyProperty(_eventSystem, CFSTR (kIOHIDEventSystemClientIsUnresponsive));
    
    
    //set report
    HIDKeyboardDescriptorInputReport inReport = { 0 };
    inReport.KB_Keyboard[0] = kHIDUsage_KeyboardEqualSign;
    status = [self.sourceController handleReport: (uint8_t*)&inReport Length:sizeof(inReport) andInterval:0];
    
    XCTAssert(status == KERN_SUCCESS);
    
    inReport.KB_Keyboard[0] = 0x00;
    status = [self.sourceController handleReport: (uint8_t*)&inReport Length:sizeof(inReport) andInterval:0];
    
    XCTAssert(status == KERN_SUCCESS);
    
    //inject other event
    inReport.KB_Keyboard[0] = kHIDUsage_KeyboardHyphen;
    status = [self.sourceController handleReport: (uint8_t*)&inReport Length:sizeof(inReport) andInterval:0];
    XCTAssert(status == KERN_SUCCESS);
    
    inReport.KB_Keyboard[0] = 0x00;
    status = [self.sourceController handleReport: (uint8_t*)&inReport Length:sizeof(inReport) andInterval:0];
    
    XCTAssert(status == KERN_SUCCESS);
    
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, kDefaultRunLoopTime, false);
    TestLog("Event Count %u",(unsigned int)_eventCount);
    XCTAssert(_eventCount == 2);
    
    
}
-(void) handleEvent:(IOHIDEventRef) event
{
    _eventCount++;
    UInt32 usagePage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    UInt32 usage           = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    
    
    TestLog("Usage %u UsagePage %u",(unsigned int)usage, (unsigned int)usagePage);
    
    XCTAssert(usagePage == kHIDPage_KeyboardOrKeypad);
    XCTAssert(usage == kHIDUsage_KeyboardEqualSign);
}
@end



