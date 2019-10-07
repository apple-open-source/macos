//
//  TestKeyboardEventOptions.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 3/18/19.
//

#import <Foundation/Foundation.h>
#import <HID/HID_Private.h>
#import <IOKit/hid/IOHIDPrivateKeys.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <mach/mach_time.h>
#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include <IOKit/hid/IOHIDEventServiceTypes.h>

@interface TestKeyboardEventOptions : XCTestCase <HIDVirtualEventServiceDelegate>

@property HIDVirtualEventService *service;
@property dispatch_queue_t queue;
@property HIDEventSystemClient *client;
@property HIDServiceClient *serviceClient;
@property NSMutableDictionary *properties;
@property XCTestExpectation *serviceExp;
@property XCTestExpectation *eventExp;

@end

@implementation TestKeyboardEventOptions

- (void)setUp
{
    __weak TestKeyboardEventOptions * self_ = self;
    NSString *uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    _serviceExp = [[XCTestExpectation alloc] initWithDescription:@"Service Exp"];
    _eventExp = [[XCTestExpectation alloc] initWithDescription:@"Event Exp"];
    _eventExp.expectedFulfillmentCount = 2;
    _eventExp.assertForOverFulfill = true;
    
    _properties = [[NSMutableDictionary alloc] init];
    _properties[@kIOHIDPrimaryUsagePageKey] = @(kHIDPage_GenericDesktop);
    _properties[@kIOHIDPrimaryUsageKey] = @(kHIDUsage_GD_Keyboard);
    _properties[@kIOHIDTransportKey] = @"Virtual";
    _properties[@kIOHIDDeviceUsagePairsKey] =  @[@{@kIOHIDDeviceUsagePageKey :
                                                       @(kHIDPage_GenericDesktop),
                                                   @kIOHIDDeviceUsageKey :
                                                       @(kHIDUsage_GD_Keyboard)}];
    _properties[@kIOHIDPhysicalDeviceUniqueIDKey] = uniqueID;
    
    _client = [[HIDEventSystemClient alloc] initWithType: HIDEventSystemClientTypeMonitor];
    
    [_client setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID }];
    [_client setServiceNotificationHandler:^(HIDServiceClient * service){
        NSLog(@"setServiceNotificationHandler service: %@", service);
        self_.serviceClient = service;
        [self_.serviceExp fulfill];
    }];
    
    [_client setEventHandler: ^(HIDServiceClient *service __unused,
                                HIDEvent *event) {
        NSInteger usage;
        
        NSLog(@"event: %@", event);
        
        usage = [event integerValueForField:kIOHIDEventFieldKeyboardUsage];
        
        if (usage == kHIDUsage_KeyboardA) {
            [self_.eventExp fulfill];
        }
    }];
    
    [_client setDispatchQueue: dispatch_get_main_queue()];
    [_client activate];
    
    _queue = dispatch_queue_create("HIDVirtualEventService", DISPATCH_QUEUE_SERIAL);
    dispatch_sync(_queue , ^{
        self.service = [[HIDVirtualEventService alloc] init];
        [self.service setDispatchQueue: self.queue];
        self.service.delegate = self;
        [self.service activate];
    });
}

- (void)tearDown
{
    [_client cancel];
    
    dispatch_sync(_queue, ^{
        [self.service cancel];
    });
}

- (void)MAC_OS_ONLY_TEST_CASE(testNoKeyRepeat)
{
    XCTWaiterResult result;
    
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _client);
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_LOGARCHIVE, _service);
    
    result = [XCTWaiter waitForExpectations:@[_serviceExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted,
                               "result:%ld %@",
                               result,
                               _serviceExp);
    
    HIDEvent *event = CFBridgingRelease(IOHIDEventCreateKeyboardEvent(
                                        kCFAllocatorDefault,
                                        mach_absolute_time(),
                                        kHIDPage_KeyboardOrKeypad,
                                        kHIDUsage_KeyboardA,
                                        1,
                                        kIOHIDKeyboardEventOptionsNoKeyRepeat));
    [_service dispatchEvent:event];
    
    // run for 1s, which is enough time to generate key repeats.
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
    
    event = CFBridgingRelease(IOHIDEventCreateKeyboardEvent(
                                        kCFAllocatorDefault,
                                        mach_absolute_time(),
                                        kHIDPage_KeyboardOrKeypad,
                                        kHIDUsage_KeyboardA,
                                        0,
                                        kIOHIDKeyboardEventOptionsNoKeyRepeat));
    [_service dispatchEvent:event];
    
    result = [XCTWaiter waitForExpectations:@[_eventExp] timeout:5];
    HIDXCTAssertWithParameters(RETURN_FROM_TEST | COLLECT_TAILSPIN | COLLECT_IOREG,
                               result == XCTWaiterResultCompleted,
                               "result:%ld %@",
                               result,
                               _eventExp);
}

- (HIDEvent *)copyEventMatching:(NSDictionary * __unused)matching
                     forService:(id __unused)service
{
    return nil;
}

- (void)notification:(HIDVirtualServiceNotificationType __unused)type
        withProperty:(NSDictionary * __unused)property
          forService:(id __unused)service
{
    return;
}

- (id)propertyForKey:(NSString *)key forService:(id __unused)service
{
    return _properties[key];
}

- (BOOL)setOutputEvent:(HIDEvent * __unused)event
            forService:(id __unused)service
{
    return false;
}

- (BOOL)setProperty:(id)value forKey:(NSString *)key
         forService:(id __unused)service
{
    _properties[key] = value;
    return YES;
}

@end
