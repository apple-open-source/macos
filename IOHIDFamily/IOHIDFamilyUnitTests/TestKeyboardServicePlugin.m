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
    
    NSDictionary *matching = @{@kIOHIDPhysicalDeviceUniqueIDKey : uniqueID};
    
    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);
    
    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: Test HID service"];
    
    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
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



- (void)checkRepeats: (NSUInteger) initialDelayMS :(NSUInteger) repeatDelayMS :(NSUInteger) repeatTime
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

 
    TestLog("initialDelayMS: %d, repeatDelayMS:%d repeatTime:%d", (int)initialDelayMS, (int)repeatDelayMS, (int)repeatTime);
    
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
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_HIDUTIL,
                                result == XCTWaiterResultCompleted,
                                "%@",
                                self.testKbcUpEventExpectation);
    
    
    NSInteger expectedCount = (repeatTime * 1000 - initialDelayMS) / repeatDelayMS;
    NSLog (@"expected count %d actual count %d", (int)expectedCount, (int)self.events.count);
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_HIDUTIL,
                               VALUE_IN_RANGE(expectedCount, VALUE_PST(self.events.count,-20) , VALUE_PST(self.events.count,+20)),
                               "expected count %d actual count %d",
                               (int)expectedCount,
                               (int)self.events.count
                               );
    
    
    uint64_t delta =  NS_TO_MS(IOHIDInitTestAbsoluteTimeToNanosecond (IOHIDEventGetTimeStamp((IOHIDEventRef)self.events[1]) - IOHIDEventGetTimeStamp((IOHIDEventRef)self.events[0]))) ;
    

    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_TAILSPIN,
                               VALUE_IN_RANGE(delta , VALUE_PST(initialDelayMS,-20) , VALUE_PST(initialDelayMS,+20)),
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
        
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_TAILSPIN,
                                    VALUE_IN_RANGE(delta , VALUE_PST(repeatDelayMS,-20) , VALUE_PST(repeatDelayMS,+20)),
                                    "repeat: %d  expected:%d - %d prev:%@ nexy:%@",
                                    (int)delta ,
                                    (int)VALUE_PST(repeatDelayMS,-20),
                                    (int)VALUE_PST(repeatDelayMS,+20),
                                    self.events[index - 1],
                                    self.events[index]
                                    );
    }
}

- (void) MAC_OS_ONLY_TEST_CASE(testKeyRepeat) {
    XCTWaiterResult result;

    KEY_REPEAT_INFO  info [] = {
      {250 , 33, 2},
      {500 , 66, 2}
    };
  
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:20];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "%@",
                                self.testServiceExpectation);

    for (size_t index = 0; index < (sizeof (info) / sizeof(info[0]));index++) {
        [self checkRepeats:info[index].initialDelay :info[index].repeatDelay : info[index].repeatTime];
        [self.events removeAllObjects];
    }
}

@end
