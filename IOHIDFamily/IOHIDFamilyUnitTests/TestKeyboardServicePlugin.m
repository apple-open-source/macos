//
//  TestKeyboardServicePlugin.m
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
#import <IOKit/hid/IOHIDServiceKeys.h>


typedef struct {
    NSUInteger initialDelay;
    NSUInteger repeatDelay;
    NSUInteger repeatTime;
} KEY_REPEAT_INFO;

static uint8_t descriptor[] = {
    HIDKeyboardDescriptor
};

@interface TestKeyboardServicePlugin : XCTestCase

@property IOHIDEventSystemClientRef         eventSystem;
@property IOHIDServiceClientRef             eventService;

@property IOHIDUserDeviceRef                userDevice;

@property XCTestExpectation                 * testServiceExpectation;
@property XCTestExpectation                 * testKbcDownEventExpectation;
@property XCTestExpectation                 * testKbcUpEventExpectation;

@property NSMutableArray                    * events;

@end

@implementation TestKeyboardServicePlugin

- (void)setUp {

    [super setUp];
    
    self.events = [[NSMutableArray alloc] init];
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    self.eventSystem = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertAndThrowTrue(self.eventSystem != NULL);
    
    NSDictionary *matching = @{
                                @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                @"Hidden" : @"*"
                               };
    
    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);
    
    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: Test HID service"];
    
    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
        NSLog(@"service: %@", service);
        self.eventService = service;
        [self.testServiceExpectation fulfill];
    };
    
    IOHIDEventSystemClientRegisterDeviceMatchingBlock(self.eventSystem , handler, NULL, NULL);
    
    IOHIDEventSystemClientRegisterEventBlock(self.eventSystem, ^(void * _Nullable target __unused, void * _Nullable refcon __unused, void * _Nullable sender __unused, IOHIDEventRef  _Nonnull event) {
        NSLog(@"Event: %@", event);
        if (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard &&
            IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage) == kHIDUsage_KeypadEqualSignAS400) {
            if (!IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)) {
                [self.testKbcUpEventExpectation fulfill];
            }
        }
        [self.events addObject: (__bridge id _Nonnull)event];
    }, NULL,  NULL);
    
    
    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystem, dispatch_get_main_queue());
    
    NSDictionary * description = @{
                                   @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                                   @kIOHIDReportDescriptorKey : [NSData dataWithBytes:descriptor length:sizeof(descriptor)],
                                   @kIOHIDVendorIDKey   : @(555),
                                   @kIOHIDProductIDKey  : @(555),
                                   @"Hidden" : @YES,
                                   @kIOHIDAltHandlerIdKey : @(88)
                                   };
    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)description);

    HIDXCTAssertAndThrowTrue(self.userDevice != NULL, "HID User Device config:%@", description);
}

- (void)tearDown {

    if (self.userDevice) {
        CFRelease(self.userDevice);
    }
    
    if (self.eventSystem) {
        CFRelease(self.eventSystem);
    }
    
    [super tearDown];
}


- (BOOL)checkRepeats: (NSUInteger) initialDelayMS :(NSUInteger) repeatDelayMS :(NSUInteger) repeatTime
{
    
    XCTWaiterResult result;
    
    IOReturn        status;
    
    NSNumber *valueInitialRepeat = @(MS_TO_NS(initialDelayMS));
    NSNumber *valueRepeatDelay = @(MS_TO_NS(repeatDelayMS));

    self.testKbcUpEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: Keyboard event Up"];

    IOHIDServiceClientSetProperty(self.eventService,
                                  CFSTR(kIOHIDServiceInitialKeyRepeatDelayKey),
                                  (__bridge CFTypeRef _Nonnull)(valueInitialRepeat));
    
    IOHIDServiceClientSetProperty(self.eventService,
                                  CFSTR(kIOHIDServiceKeyRepeatDelayKey),
                                  (__bridge CFTypeRef _Nonnull)(valueRepeatDelay));

 
    TestLog("initialDelayMS:%d, repeatDelayMS:%d repeatTime:%d", (int)initialDelayMS, (int)repeatDelayMS, (int)repeatTime);
    
    HIDKeyboardDescriptorInputReport report;
    memset(&report, 0, sizeof(report));
    
    report.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;
    
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&report, sizeof(report));
    XCTAssert(status == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", status);

    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(repeatTime * NSEC_PER_SEC)), dispatch_get_main_queue(), ^{
        IOReturn        ret;
        HIDKeyboardDescriptorInputReport ureport;
        memset(&ureport, 0, sizeof(report));

        ret = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&ureport, sizeof(ureport));
        XCTAssert(ret == kIOReturnSuccess, "IOHIDUserDeviceHandleReport:%x", ret);

    });
 
    result = [XCTWaiter waitForExpectations:@[self.testKbcUpEventExpectation] timeout:10];
    HIDXCTAssertWithParametersAndReturn(RETURN_FROM_TEST | COLLECT_HIDUTIL,
                                        result == XCTWaiterResultCompleted,
                                        NO,
                                        "%@",
                                        self.testKbcUpEventExpectation);
    
    NSUInteger expectedCount = (repeatTime * 1000 - initialDelayMS) / repeatDelayMS;
    NSLog (@"expected count %d actual count %d", (int)expectedCount, (int)self.events.count);
    
    HIDXCTAssertWithParametersAndReturn(RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_HIDUTIL | COLLECT_LOGARCHIVE,
                                        VALUE_IN_RANGE(expectedCount, VALUE_PST(self.events.count,-20) , VALUE_PST(self.events.count,+20)),
                                        NO,
                                        "expected count %d actual count %d",
                                        (int)expectedCount,
                                        (int)self.events.count
                                        );
    
    
    uint64_t delta =  NS_TO_MS(IOHIDInitTestAbsoluteTimeToNanosecond (IOHIDEventGetTimeStamp((IOHIDEventRef)self.events[1]) - IOHIDEventGetTimeStamp((IOHIDEventRef)self.events[0]))) ;
    

    HIDXCTAssertWithParametersAndReturn(COLLECT_TAILSPIN | COLLECT_LOGARCHIVE,
                                        VALUE_IN_RANGE(delta , VALUE_PST(initialDelayMS,-20) , VALUE_PST(initialDelayMS,+20)),
                                        NO,
                                        "initial delay:%d expected:%d - %d first:%@ next:%@",
                                        (int)delta,
                                        (int)VALUE_PST(initialDelayMS,-20),
                                        (int)VALUE_PST(initialDelayMS,+20),
                                        self.events[0],
                                        self.events[0]
                                        );

    
    for (size_t index = 2; index < (self.events.count - 1); index++) {
        uint64_t prev =  IOHIDEventGetTimeStamp((IOHIDEventRef)self.events[index - 1]);
        uint64_t next =  IOHIDEventGetTimeStamp((IOHIDEventRef)self.events[index]);
        
        delta = NS_TO_MS(IOHIDInitTestAbsoluteTimeToNanosecond (next - prev));
        NSLog (@"repeat:%d expected:%d", (int)delta, (int)repeatDelayMS);
        
        HIDXCTAssertWithParametersAndReturn (RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_LOGARCHIVE,
                                             VALUE_IN_RANGE(delta , VALUE_PST(repeatDelayMS,-20) , VALUE_PST(repeatDelayMS,+20)),
                                             NO,
                                             "repeat:%d  expected:%d - %d prev:%@ next:%@",
                                             (int)delta,
                                             (int)VALUE_PST(repeatDelayMS,-20),
                                             (int)VALUE_PST(repeatDelayMS,+20),
                                             self.events[index - 1],
                                             self.events[index]
                                             );
    }
    return YES;
}


- (void) testKeyRepeat {
    XCTWaiterResult result;

    KEY_REPEAT_INFO  info [] = {
      {250 , 33, 2},
      {500 , 66, 2}
    };
  
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:20];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                result == XCTWaiterResultCompleted,
                                "%@ result:%d",
                                self.testServiceExpectation,
                                (int)result);

    for (size_t index = 0; index < (sizeof (info) / sizeof(info[0]));index++) {
        if ( NO == [self checkRepeats:info[index].initialDelay :info[index].repeatDelay : info[index].repeatTime]) {
            return;
        }
        [self.events removeAllObjects];
    }
}

- (void)MAC_OS_ONLY_TEST_CASE(testKeyboardLayoutProperty)
{
    XCTWaiterResult result;
    NSNumber *layout = nil;
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:5];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                result == XCTWaiterResultCompleted,
                                "%@ result:%d",
                                self.testServiceExpectation,
                                (int)result);
    
    // keyboard plugin publishes the kIOHIDSubinterfaceIDKey based on the
    // kIOHIDAltHandlerIdKey property on the device.
    layout = (NSNumber *)CFBridgingRelease(IOHIDServiceClientCopyProperty(_eventService, CFSTR(kIOHIDSubinterfaceIDKey)));
    XCTAssert(layout && [layout isEqualToNumber:@(88)]);
}

@end
