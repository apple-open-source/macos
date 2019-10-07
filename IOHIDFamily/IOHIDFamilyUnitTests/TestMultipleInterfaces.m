//
//  TestMultipleInterfaces.m
//  IOHIDFamily
//
//  Created by Paul on 8/30/18.
//
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDMultipleInterfaceDescriptors.h"
#import "IOHIDXCTestExpectation.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hid/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDLibPrivate.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#include <TargetConditionals.h>


@interface TestMultipleInterfaces : XCTestCase

@property IOHIDEventSystemClientRef         eventSystem;
@property IOHIDServiceClientRef             keyboardEventService;
@property IOHIDServiceClientRef             buttonEventService;
@property IOHIDUserDeviceRef                userDevice;
@property NSString                          * uniqueID;
@property NSData                            * hidDeviceDescriptor;
@property NSDictionary                      * userDeviceDescription;
@property NSMutableArray                    * events;

@property IOHIDXCTestExpectation            * testKeyboardServiceExpectation;
@property IOHIDXCTestExpectation            * testButtonServiceExpectation;

@property IOHIDXCTestExpectation            * testKeyboardEventExpectation;
@property IOHIDXCTestExpectation            * testButtonEventExpectation;

@property IOHIDXCTestExpectation            * stressExpectation;

@end

@implementation TestMultipleInterfaces

- (void)setUp {

    [super setUp];

    self.events = [[NSMutableArray alloc] init];

    self.uniqueID = [[[NSUUID alloc] init] UUIDString];

    self.eventSystem = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);

    NSDictionary *matching = @{
                               @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                               @"Hidden" : @"*"
                               };

    self.testKeyboardServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test keyboard service (uuid:%@)", self.uniqueID]];
    self.testButtonServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test button service (uuid:%@)", self.uniqueID]];

    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);

    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
        [self addService:service];
    };

    IOHIDEventSystemClientRegisterDeviceMatchingBlock(self.eventSystem , handler, NULL, NULL);

    IOHIDEventSystemClientRegisterEventBlock(self.eventSystem, ^(void * _Nullable target __unused, void * _Nullable refcon __unused, void * _Nullable sender __unused, IOHIDEventRef  _Nonnull event) {
        NSLog(@"Event: %@", event);
        [self handleEvent:event fromService:sender];
    }, NULL,  NULL);


    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystem, dispatch_get_main_queue());

    self.testKeyboardEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Expectation: keyboard event"];
    self.testButtonEventExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription:@"Expectation: button event"];

    [super setUp];
}

- (void)tearDown
{
    if (self.userDevice) {
        CFRelease(self.userDevice);
    }

    if (self.eventSystem) {
        IOHIDEventSystemClientUnscheduleFromDispatchQueue(self.eventSystem, dispatch_get_main_queue());
        CFRelease(self.eventSystem);
    }

    [super tearDown];
}

- (void)testMultipleInterface {
    static uint8_t descriptorMultiCollection[] = {
        HIDKeyboardButtonMultiCollection
    };

    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptorMultiCollection length:sizeof(descriptorMultiCollection)];

    self.userDeviceDescription  = @{
                                    @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                                    @kIOHIDReportDescriptorKey : self.hidDeviceDescriptor,
                                    @kIOHIDMultipleInterfaceEnabledKey : @(YES),
                                    @kIOHIDVendorIDKey   : @(555),
                                    @kIOHIDProductIDKey  : @(555),
                                    };

    [self eventTestHelper];
}

- (void)testVendorMultipleInterface {
    static uint8_t descriptorVendorMultiCollection[] = {
     HIDAppleVendorMultiCollectionHeader
     HIDKeyboardButtonMultiCollection
     };

    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptorVendorMultiCollection length:sizeof(descriptorVendorMultiCollection)];

    self.userDeviceDescription  = @{
                                    @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                                    @kIOHIDReportDescriptorKey : self.hidDeviceDescriptor,
                                    @kIOHIDVendorIDKey   : @(555),
                                    @kIOHIDProductIDKey  : @(555),
                                    };

    [self eventTestHelper];
}

- (void)testMultipleInterfaceDisabled
{
    XCTWaiterResult result;

    static uint8_t descriptorMultiCollection[] = {
        HIDKeyboardButtonMultiCollection
    };

    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptorMultiCollection length:sizeof(descriptorMultiCollection)];

    // No kIOHIDMultipleInterfaceEnabledKey
    self.userDeviceDescription  = @{
                                    @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                                    @kIOHIDReportDescriptorKey : self.hidDeviceDescriptor,
                                    @kIOHIDVendorIDKey   : @(555),
                                    @kIOHIDProductIDKey  : @(555),
                                    };

    NSLog(@"Device description: %@",  self.userDeviceDescription);

    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)self.userDeviceDescription);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);

    result = [XCTWaiter waitForExpectations:@[self.testKeyboardServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testKeyboardServiceExpectation);

    // Expect that second event driver will not show up.
    result = [XCTWaiter waitForExpectations:@[self.testButtonServiceExpectation] timeout:5];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultTimedOut,
                                "result:%ld %@",
                                (long)result,
                                self.testButtonServiceExpectation);
}

#if !TARGET_OS_WATCH
- (void)testMultipleInterfaceStress
{
    XCTWaiterResult result;

    static uint8_t descriptorMultiCollection[] = {
        HIDKeyboardButtonMultiCollection
    };

    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptorMultiCollection length:sizeof(descriptorMultiCollection)];

    self.userDeviceDescription  = @{
                                    @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                                    @kIOHIDReportDescriptorKey : self.hidDeviceDescriptor,
                                    @kIOHIDMultipleInterfaceEnabledKey : @(YES),
                                    @kIOHIDVendorIDKey   : @(555),
                                    @kIOHIDProductIDKey  : @(555),
                                    };

    NSLog(@"Device description: %@",  self.userDeviceDescription);

    self.stressExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Stress (uuid:%@)", self.uniqueID]];

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
        for (size_t i = 0; i < 100; i++) {
            XCTWaiterResult blockResult;

            self.userDevice = IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)self.userDeviceDescription);

            HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);

            blockResult = [XCTWaiter waitForExpectations:@[self.testKeyboardServiceExpectation] timeout:2];
            HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                        blockResult == XCTWaiterResultCompleted,
                                        "result:%ld %@",
                                        (long)blockResult,
                                        self.testKeyboardServiceExpectation);

            blockResult = [XCTWaiter waitForExpectations:@[self.testButtonServiceExpectation] timeout:2];
            HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                        blockResult == XCTWaiterResultCompleted,
                                        "result:%ld %@",
                                        (long)blockResult,
                                        self.testButtonServiceExpectation);

            CFRelease(self.userDevice);
            self.userDevice = nil;

            self.testKeyboardServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test keyboard service (uuid:%@)", self.uniqueID]];
            self.testButtonServiceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test button service (uuid:%@)", self.uniqueID]];
        }
        [self.stressExpectation fulfill];
    });

    result = [XCTWaiter waitForExpectations:@[self.stressExpectation] timeout:30];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.stressExpectation);
}
#endif

- (void)eventTestHelper
{
    IOReturn        status;
    XCTWaiterResult result;

    NSLog(@"Device description: %@",  self.userDeviceDescription);

    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)self.userDeviceDescription);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);

    result = [XCTWaiter waitForExpectations:@[self.testKeyboardServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testKeyboardServiceExpectation);

    result = [XCTWaiter waitForExpectations:@[self.testButtonServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testButtonServiceExpectation);

    HIDKeyboardButtonMultiCollectionInputReport01 keyboardReport;
    HIDKeyboardButtonMultiCollectionInputReport02 buttonReport;

    memset(&keyboardReport, 0, sizeof(keyboardReport));
    memset(&buttonReport, 0, sizeof(buttonReport));

    keyboardReport.reportId = 1;
    keyboardReport.KB_Keyboard[0] = kHIDUsage_KeypadEqualSignAS400;

    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&keyboardReport, sizeof(keyboardReport));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    buttonReport.reportId = 2;
    buttonReport.CD_PhonePlus100 = 1;
    status = IOHIDUserDeviceHandleReport(self.userDevice, (uint8_t *)&buttonReport, sizeof(buttonReport));
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_HIDUTIL | COLLECT_IOREG | COLLECT_LOGARCHIVE,
                                status == kIOReturnSuccess,
                                "IOHIDUserDeviceHandleReport:0x%x",
                                status);

    result = [XCTWaiter waitForExpectations:@[self.testKeyboardEventExpectation] timeout:10];
    XCTAssert(result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testKeyboardEventExpectation);

    result = [XCTWaiter waitForExpectations:@[self.testButtonEventExpectation] timeout:10];
    XCTAssert(result == XCTWaiterResultCompleted, "result:%ld %@", (long)result, self.testButtonEventExpectation);
}

-(void)handleEvent: (IOHIDEventRef) event fromService:(IOHIDServiceClientRef) service
{
    [self.events addObject:(__bridge id _Nonnull)(event)];

    if (IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard) {
        return;
    }

    if (IOHIDServiceClientConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard)) {
        [self.testKeyboardEventExpectation fulfill];
    }
    else if (IOHIDServiceClientConformsTo(service, kHIDPage_Telephony, kHIDUsage_Tfon_Phone)) {
        [self.testButtonEventExpectation fulfill];
    }
}

-(void)addService:(IOHIDServiceClientRef)service
{
    if (IOHIDServiceClientConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard)) {
        self.keyboardEventService = service;
        [self.testKeyboardServiceExpectation fulfill];
    }
    else if (IOHIDServiceClientConformsTo(service, kHIDPage_Telephony, kHIDUsage_Tfon_Phone)) {
        self.buttonEventService = service;
        [self.testButtonServiceExpectation fulfill];
    }
}

@end
