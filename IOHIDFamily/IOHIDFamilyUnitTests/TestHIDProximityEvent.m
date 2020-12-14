//
//  TestHIDProximityEvent.m
//  IOHIDFamilyUnitTests
//
//  Created by Josh Kergan on 10/16/20.
//

#import <XCTest/XCTest.h>
#import "IOHIDXCTestExpectation.h"
#include "IOHIDUnitTestUtility.h"
#include "HIDDeviceTester.h"
#include "IOHIDUnitTestDescriptors.h"
#include "IOHIDEventSystemTestController.h"
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include "HIDEventAccessors_Private.h"


@interface TestHIDProximityEvent : XCTestCase

@property dispatch_queue_t                  eventClientQueue;
@property HIDEventSystemClient *            eventClient;
@property HIDUserDevice *                   sourceDevice;
@property uint32_t                          eventSystemResetCount;
@property NSMutableArray *                  events;
@property XCTestExpectation *               eventExpectation;
@property XCTestExpectation *               serviceExpectation;

@end

@implementation TestHIDProximityEvent

- (void)setUp {
    [super setUp];

    self.eventClientQueue = dispatch_queue_create ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL);
    HIDXCTAssertAndThrowTrue(self.eventClientQueue != nil);

    self.events = [NSMutableArray new];
}

- (void)tearDown {
    if (self.sourceDevice) {
        [self.sourceDevice cancel];
    }

    @autoreleasepool {
        self.sourceDevice = nil;
    }

    if (self.eventClient) {

        // Check if event system reset occur
        XCTAssert(self.eventSystemResetCount == 0, "IOHIDEventSystem resetCount:%d", (int)self.eventSystemResetCount);

        [self.eventClient cancel];
    }

    @autoreleasepool {
        self.eventClient = nil;
    }

    [super tearDown];
}

-(void)setupTestSystem : (NSData *) descriptorData {

    if (self.sourceDevice) {
        [self.sourceDevice cancel];
    }

    @autoreleasepool {
        self.sourceDevice = nil;
    }

    if (self.eventClient) {
        // Check if event system reset occur
        XCTAssert(self.eventSystemResetCount == 0, "IOHIDEventSystem resetCount:%d", (int)self.eventSystemResetCount);
        [self.eventClient cancel];
    }

    @autoreleasepool {
        self.eventClient = nil;
        [self.events removeAllObjects];
    }

    self.eventExpectation = [[XCTestExpectation alloc] initWithDescription:@"HIDEventSystem Event Expectation"];
    self.serviceExpectation = [[XCTestExpectation alloc] initWithDescription:@"HIDService Enumeration Expectation"];

    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];

    NSMutableDictionary * deviceProps = [NSMutableDictionary new];
    NSMutableDictionary * matchingDictionary = [NSMutableDictionary new];

    deviceProps[@kIOHIDUniqueIDKey] = uniqueID;
    deviceProps[@kIOHIDReportDescriptorKey] = descriptorData;
    matchingDictionary[@kIOHIDUniqueIDKey] = uniqueID;

    self.sourceDevice = [[HIDUserDevice alloc] initWithProperties:deviceProps];
    HIDXCTAssertAndThrowTrue(self.sourceDevice != nil);

    [self.sourceDevice setDispatchQueue:self.eventClientQueue];
    [self.sourceDevice activate];

    self.eventClient = [[HIDEventSystemClient alloc] initWithType:HIDEventSystemClientTypeMonitor];
    HIDXCTAssertAndThrowTrue(self.eventClient != nil);

    [self.eventClient setMatching:matchingDictionary];
    [self.eventClient setEventHandler:^(HIDServiceClient * _Nullable service, HIDEvent * _Nonnull event) {
        [self.events addObject:event];
        [self.eventExpectation fulfill];
    }];
    [self.eventClient setServiceNotificationHandler:^(HIDServiceClient * _Nonnull service) {
        [self.serviceExpectation fulfill];
    }];
    [self.eventClient setResetHandler:^{
        self.eventSystemResetCount++;
    }];
    [self.eventClient setDispatchQueue:self.eventClientQueue];
    [self.eventClient activate];
}

- (void)testProxmityEvent {

    IOReturn status;

    uint8_t descriptor [] = {HIDKeyboardProxmityDescriptor};

    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];

    [self setupTestSystem :  descriptorData];

    self.eventExpectation.expectedFulfillmentCount = 2;

    [XCTWaiter waitForExpectations:@[self.serviceExpectation] timeout:10];

    HIDKeyboardProximityReport report;
    memset (&report, 0 , sizeof(report));

    report.reportID = 1;
    report.CD_ConsumerProxity = 1;

    NSData *reportData = [NSData dataWithBytesNoCopy:&report length:sizeof(report) freeWhenDone:false];
    NSError *err = NULL;
    status = [self.sourceDevice handleReport:reportData error:&err];

    XCTAssert(err == NULL, "error sending report (%@)", err);

    report.CD_ConsumerProxity = 0;
    status = [self.sourceDevice handleReport:reportData error:&err];

    XCTAssert(err == NULL, "error sending report (%@)", err);

    // Allow event to be dispatched
    [XCTWaiter waitForExpectations:@[self.eventExpectation] timeout:10];

    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:self.events];

    XCTAssert(stats.totalCount == 2,
              "events count:%lu expected:%d events:%@", (unsigned long)stats.totalCount , 2, self.events);

    XCTAssert (stats.counts[kIOHIDEventTypeProximity] == 2, "Events:%@", self.events);

    for (HIDEvent * event in self.events) {
        IOHIDEventRef proximityEvent = IOHIDEventGetEvent((__bridge IOHIDEventRef)event, kIOHIDEventTypeProximity);
        long proxLevel = IOHIDEventGetIntegerValue(proximityEvent, kIOHIDEventFieldProximityLevel);
        long proxMask = IOHIDEventGetIntegerValue(proximityEvent, kIOHIDEventFieldProximityDetectionMask);
        if (proxLevel) {
            XCTAssert(proxMask == kIOHIDProximityDetectionLargeBodyContact, "Proximity Event Level : %ld Mask: %#lx", proxLevel, proxMask);
        }
    }
}

@end


