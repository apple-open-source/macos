//
//  TestVirtualService.m
//  IOHIDFamily
//
//  Created by yg on 1/23/17.
//
//
#if 0
#import  <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>
#include <IOKit/hid/IOHIDEvent.h>
#include <mach/mach_time.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDPrivateKeys.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"
#import  "IOHIDUnitTestDescriptors.h"
#import  <HID/HID_Private.h>

@interface TestHIDVirtualService : XCTestCase <HIDVirtualEventServiceDelegate>

@property       HIDVirtualEventService *        service;
@property       HIDEventSystemClient *          client;
@property       HIDServiceClient *              serviceClient;

@property       NSMutableDictionary *           servicePropertyCache;
@property       NSMutableDictionary *           servicePropertyCacheCopy;

@property       dispatch_queue_t                serviceQueue;

@property       XCTestExpectation   *           testEventExpectation;
@property       XCTestExpectation   *           testOutputEventExpectation;
@property       XCTestExpectation   *           testServiceCancelExpectation;
@property       XCTestExpectation   *           testServiceExpectation;


@end

@implementation TestHIDVirtualService

- (NSMutableDictionary *) createPropertyCache {
    NSMutableDictionary * servicePropertyCache = [[NSMutableDictionary alloc] init];
    servicePropertyCache[@kIOHIDPrimaryUsagePageKey] = @(kHIDPage_GenericDesktop);
    servicePropertyCache[@kIOHIDPrimaryUsageKey] = @(kHIDUsage_GD_Mouse);
    servicePropertyCache[@kIOHIDVendorIDKey] = @(0xff00);
    servicePropertyCache[@kIOHIDProductIDKey] = @(0xff00);
    servicePropertyCache[@kIOHIDTransportKey] = @"Virtual";
    servicePropertyCache[@kIOHIDDeviceUsagePairsKey] =  @[
                                                               @{
                                                                   @kIOHIDDeviceUsagePageKey : @(kHIDPage_GenericDesktop),
                                                                   @kIOHIDDeviceUsageKey: @(kHIDUsage_GD_Pointer)
                                                                   },
                                                               @{
                                                                   @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                                   @kIOHIDDeviceUsageKey: @(kHIDPage_AppleVendor)
                                                                   }
                                                               ];
    return servicePropertyCache;
}


- (void)setUp {
    __weak TestHIDVirtualService * self_ = self;
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];

    self.testEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: events"];
    self.testOutputEventExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: output events"];
    self.testServiceCancelExpectation = [[XCTestExpectation alloc] initWithDescription:@"Expectation: service cancel"];
    self.testServiceCancelExpectation.expectedFulfillmentCount = 2;
    self.testServiceExpectation = [[XCTestExpectation alloc] initWithDescription: [NSString stringWithFormat:@"Expectation: Test HID service (uuid:%@)", uniqueID]];

    self.client = [[HIDEventSystemClient alloc] initWithType: HIDEventSystemClientTypeMonitor];
    NSDictionary *matching = @{
                               @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID,
                               @"Hidden" : @"*"
                               };
    [self.client setMatching: matching];
    [self.client setServiceNotificationHandler:^(HIDServiceClient * service){
        NSLog(@"setServiceNotificationHandler service:%@", service);
        self_.serviceClient = service;
        [self_.testServiceExpectation fulfill];
    }];
    [self.client setCancelHandler: ^{
        NSLog(@"setCancelHandler");
        [self_.testServiceCancelExpectation fulfill];
    }];
    [self.client setEventHandler: ^(HIDServiceClient * service __unused,
                                    HIDEvent *event) {
        NSLog(@"event:%@", event);
        [self_.testEventExpectation fulfill];
    }];

    [self.client setDispatchQueue: dispatch_get_main_queue()];

    [self.client activate];

    
    self.servicePropertyCache = [self createPropertyCache];

    self.servicePropertyCache[@kIOHIDPhysicalDeviceUniqueIDKey] = uniqueID;
    
    self.serviceQueue = dispatch_queue_create("HIDVirtualEventService", DISPATCH_QUEUE_SERIAL);
    dispatch_sync (self.serviceQueue , ^{
        self.service = [[HIDVirtualEventService alloc] init];
        [self.service setDispatchQueue: self.serviceQueue];
        self.service.delegate = self;
        [self.service setCancelHandler: ^{
            [self_.testServiceCancelExpectation fulfill];
        }];
        [self.service activate];
    });
}

- (void)tearDown {
    XCTWaiterResult result;
    
    [self.client cancel];
    
    dispatch_sync (self.serviceQueue, ^() {
        [self.service cancel];
    });

    result = [XCTWaiter waitForExpectations:@[self. self.testServiceCancelExpectation] timeout:20];
    HIDXCTAssertWithParameters (RETURN_FROM_TEST,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testServiceCancelExpectation);

    [super tearDown];
}

- (void)testVirtualServiceDispatchEvent {

    XCTWaiterResult result;

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.client);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.service);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    

    self.testEventExpectation.expectedFulfillmentCount = 100;

    dispatch_async(self.serviceQueue, ^{
        uint8_t buffer[] = {1, 2, 3};
        HIDEvent * event = (HIDEvent *) CFBridgingRelease(IOHIDEventCreateVendorDefinedEvent (kCFAllocatorDefault,
                                                                                              mach_absolute_time(),
                                                                                              0,
                                                                                              0,
                                                                                              0,
                                                                                              buffer,
                                                                                              sizeof(buffer),
                                                                                              0));
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL , event, "IOHIDEventCreateVendorDefinedEvent");

        for (NSInteger index = 0 ; index < 100; index++) {
            if (![self.service dispatchEvent: event]) {
                HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL , event, "dispatchEvent:NO");
            }
        }
    });
    
    result = [XCTWaiter waitForExpectations:@[self.testEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                (long)result,
                                self.testEventExpectation);
}

- (void)testVirtualServiceCopyEvent {
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.client, "self.client");
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.service, "self.service");
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    for (NSInteger index = 0; index < 100 ; index++) {
        HIDEvent * event = [self.serviceClient  eventMatching:nil];
        HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_ALL , event, "eventMatching:%@", event);
    }
}

- (void)testVirtualServiceSenderId {
    XCTWaiterResult result;

    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.client != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.service != NULL);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                self.serviceClient.serviceID == self.service.serviceID,
                                "client:%llx service:%llx",
                                self.serviceClient.serviceID,
                                self.service.serviceID);

}

- (void)testVirtualServiceSetOutputEvent {
    IOReturn  status;
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.client != NULL);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.service != NULL);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    self.testOutputEventExpectation.expectedFulfillmentCount = 100 * 2;
    
    for (NSInteger index = 0; index < 100 ; index++) {
        status = IOHIDServiceClientSetElementValue((IOHIDServiceClientRef)self.serviceClient,
                                                   kHIDPage_LEDs,
                                                   kHIDUsage_LED_CapsLock,
                                                   1);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST, status == kIOReturnSuccess, "IOHIDServiceClientSetElementValue:%d", status);

        status = IOHIDServiceClientSetElementValue((IOHIDServiceClientRef)self.serviceClient,
                                                   kHIDPage_LEDs,
                                                   kHIDUsage_LED_CapsLock,
                                                   0);
        HIDXCTAssertWithParameters(RETURN_FROM_TEST, status == kIOReturnSuccess, "IOHIDServiceClientSetElementValue:%d", status);
    }
    
    result = [XCTWaiter waitForExpectations:@[self.testOutputEventExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_ALL,
                                result == XCTWaiterResultCompleted,
                                "expectation:%@",
                                self.testOutputEventExpectation);
}

- (void)testVirtualServiceProperty {
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.client);
    
    HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE, self.service);
    
    result = [XCTWaiter waitForExpectations:@[self.testServiceExpectation] timeout:10];
    HIDXCTAssertWithParameters ( RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                                result == XCTWaiterResultCompleted,
                                "result:%ld %@",
                                result,
                                self.testServiceExpectation);

    NSDictionary * copyServicePropertyCache = [self createPropertyCache];
    [copyServicePropertyCache enumerateKeysAndObjectsUsingBlock:^(id key, id obj, BOOL *stop) {
        id value = [self.serviceClient propertyForKey:key];
        if ([value isEqual:obj] == NO) {
            *stop = YES;
        }
        HIDXCTAssertWithParameters (RETURN_FROM_TEST | COLLECT_LOGARCHIVE,
                                    [value isEqual:obj],
                                    "value:%@ expected:%@ key:%@",
                                    value, obj, key);

    }];
    

    for (NSInteger index = 0; index < 100 ; index++) {
        [self.serviceClient setProperty:@(index) forKey:@"test_value"];
        id value = [self.serviceClient propertyForKey:@"test_value"];
        HIDXCTAssertWithParameters(RETURN_FROM_TEST, [value isEqual:@(index)], "propertyForKey:%@", value);
    }
}

- (BOOL)setProperty:(nullable id)value forKey:(NSString *)key forService:(id __unused) service
{
    NSLog(@"setProperty:%@ for forKey:%@", value, key);
    self.servicePropertyCache[key] = value;
    return YES;
}

- (nullable id)propertyForKey:(NSString *)key forService:(id __unused) service
{
    NSLog(@"propertyForKey:%@", key);
    return self.servicePropertyCache[key];
}

- (nullable HIDEvent *)copyEventMatching:(nullable NSDictionary * __unused)matching forService:(id __unused) service
{
    uint8_t buffer[] = {3,2,1};
    HIDEvent * event = (HIDEvent *)CFBridgingRelease(IOHIDEventCreateVendorDefinedEvent (kCFAllocatorDefault,
                                                                                         mach_absolute_time(),
                                                                                         0,
                                                                                         0,
                                                                                         0,
                                                                                         buffer,
                                                                                         sizeof(buffer),
                                                                                         0));
    NSLog(@"eventMatching:%@", event);
    return event;
}

- (BOOL) setOutputEvent:(nullable HIDEvent *) event forService:(id __unused) service
{
    NSLog(@"setOutputEvent:%@", event);
    [self.testOutputEventExpectation fulfill];
    return YES;
}

- (void) notification:(HIDVirtualServiceNotificationType) type withProperty:(nullable NSDictionary *) property forService:(id __unused) service
{
    NSLog(@"notification:%d property:%@", (int)type, property);
}


@end

#endif
