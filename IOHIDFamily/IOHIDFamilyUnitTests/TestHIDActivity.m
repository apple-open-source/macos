//
//  TestHIDActivity.m
//  IOHIDFamilyUnitTests
//
//  Created by yg on 5/26/17.
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventSystemTestController.h"
#import "IOHIDUserDeviceTestController.h"
#import "IOHIDDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"
#include <IOKit/hid/IOHIDEventSystemKeys.h>



static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};


@interface TestHIDActivity : XCTestCase <IOHIDPropertyObserver> 

@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  rootQueue;
@property dispatch_queue_t                  queue;


@property IOHIDEventSystemTestController * eventController;
@property NSNumber                       * currentActivity;
@end

@implementation TestHIDActivity

- (void)setUp {
    [super setUp];
    
    self.rootQueue = IOHIDUnitTestCreateRootQueue(37, 2);
    
    self.queue = dispatch_queue_create_with_target ("IOHIDEventSystemClientQueue", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.queue != nil);
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDescriptor:descriptorData DeviceUniqueID:uniqueID andQueue:nil];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);
    
    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID AndQueue:self.queue];
    HIDXCTAssertAndThrowTrue(self.eventController != nil);
    
    [self.eventController addPropertyObserver:self For:@(kIOHIDActivityStateKey)];
}

- (void)tearDown {
    
    [self.eventController removePropertyObserver:self For:@(kIOHIDActivityStateKey)];
    
    [self.eventController  invalidate];
    [self.sourceController invalidate];
    
    @autoreleasepool {
        self.sourceController = nil;
        self.eventController = nil;
    }
    
    [super tearDown];
}

- (void)testActivity {
    
    IOReturn status;
    
    self.currentActivity =  (NSNumber *)CFBridgingRelease(IOHIDEventSystemClientCopyProperty(self.eventController.eventSystemClient, CFSTR(kIOHIDActivityStateKey)));
    XCTAssert (self.currentActivity.intValue == 2);
    
    NSNumber * value = @(5);
    IOHIDEventSystemClientSetProperty(self.eventController.eventSystemClient, CFSTR(kIOHIDIdleNotificationTimeKey), (CFTypeRef)value);
    
    sleep (1);
    XCTAssert (self.currentActivity.intValue == 1);
    
    sleep (7);
    XCTAssert (self.currentActivity.intValue == 0);
    
    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));
    
    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;
    status = [self.sourceController handleReport: (uint8_t*)&report Length:sizeof(report) andInterval:0];
    XCTAssert(status == kIOReturnSuccess, "handleReport:%x", status);
    
    sleep (1);
    XCTAssert (self.currentActivity.intValue == 1);
}

-(void) PropertyCallback: (nonnull __unused CFStringRef) property And: (nullable CFTypeRef) value {
    if (value) {
        self.currentActivity = (__bridge NSNumber *) (value);
    }
    TestLog("activity state:%@", value);
}
@end
