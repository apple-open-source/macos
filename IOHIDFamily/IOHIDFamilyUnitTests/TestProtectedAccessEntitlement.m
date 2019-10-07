//
//  TestProtectedAccessEntitlement.m
//  IOHIDFamilyUnitTests
//
//  Created by Paul on 4/18/19.
//
//

#import <XCTest/XCTest.h>
#import "IOHIDUnitTestUtility.h"
#import "IOHIDUnitTestDescriptors.h"
#import "IOHIDXCTestExpectation.h"
#import <IOKit/hid/IOHIDEventSystemClient.h>
#import <IOKit/hid/IOHIDUserDevice.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>


@interface TestProtectedAccessEntitlement : XCTestCase

@property IOHIDEventSystemClientRef         eventSystem;
@property IOHIDServiceClientRef             eventService;
@property IOHIDUserDeviceRef                userDevice;
@property NSString                          * uniqueID;
@property NSData                            * hidDeviceDescriptor;
@property NSDictionary                      * userDeviceDescription;
@property IOHIDXCTestExpectation            * serviceExpectation;

@end

@implementation TestProtectedAccessEntitlement

- (void)setUp {

    [super setUp];

    self.uniqueID = [[[NSUUID alloc] init] UUIDString];

    self.eventSystem = IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeMonitor, NULL);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.eventSystem != NULL);

    NSDictionary *matching = @{
                               @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                               @"Hidden" : @"*"
                               };

    self.serviceExpectation = [[IOHIDXCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test protected service (uuid:%@)", self.uniqueID]];

    IOHIDEventSystemClientSetMatching(self.eventSystem , (__bridge CFDictionaryRef)matching);

    IOHIDServiceClientBlock handler = ^(void * _Nullable target __unused, void * _Nullable refcon __unused, IOHIDServiceClientRef  _Nonnull service __unused) {
        [self addService:service];
    };

    IOHIDEventSystemClientRegisterDeviceMatchingBlock(self.eventSystem , handler, NULL, NULL);

    IOHIDEventSystemClientScheduleWithDispatchQueue(self.eventSystem, dispatch_get_main_queue());

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

- (void)testProtectedAccessEntitlement {
    XCTWaiterResult result;

    static uint8_t descriptor[] = {
        HIDMouseDescriptor
    };

    self.hidDeviceDescriptor = [NSData dataWithBytes:descriptor length:sizeof(descriptor)];

    self.userDeviceDescription  = @{
                                    @kIOHIDPhysicalDeviceUniqueIDKey : self.uniqueID,
                                    @kIOHIDReportDescriptorKey : self.hidDeviceDescriptor,
                                    @kIOHIDProtectedAccessKey : @(YES),
                                    @kIOHIDVendorIDKey   : @(555),
                                    @kIOHIDProductIDKey  : @(555),
                                    };

    NSLog(@"Device description: %@",  self.userDeviceDescription);

    self.userDevice =  IOHIDUserDeviceCreate(kCFAllocatorDefault, (CFDictionaryRef)self.userDeviceDescription);
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.userDevice != NULL, "User device description: %@", self.userDeviceDescription);

    result = [XCTWaiter waitForExpectations:@[self.serviceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.serviceExpectation);
}

-(void)addService:(IOHIDServiceClientRef)service
{
    if (IOHIDServiceClientConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse)) {
        self.eventService = service;
        [self.serviceExpectation fulfill];
    }
}

@end
