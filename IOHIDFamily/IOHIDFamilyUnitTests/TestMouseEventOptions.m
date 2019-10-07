//
//  TestMouseEventOptions.m
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
#import <IOKit/hid/IOHIDEventData.h>
#include "IOHIDUnitTestUtility.h"
#include <IOKit/hid/IOHIDEventServiceTypes.h>

@interface TestMouseEventOptions : XCTestCase <HIDVirtualEventServiceDelegate>

@property HIDVirtualEventService *service;
@property dispatch_queue_t queue;
@property HIDEventSystemClient *client;
@property HIDServiceClient *serviceClient;
@property NSMutableDictionary *properties;
@property XCTestExpectation *serviceExp;
@property XCTestExpectation *eventExp;

@end

@implementation TestMouseEventOptions

- (void)setUp
{
    __weak TestMouseEventOptions *self_ = self;
    NSString *uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    _serviceExp = [[XCTestExpectation alloc] initWithDescription:@"Service Exp"];
    _eventExp = [[XCTestExpectation alloc] initWithDescription:@"Event Exp"];
    _eventExp.expectedFulfillmentCount = 2;
    
    _properties = [[NSMutableDictionary alloc] init];
    _properties[@kIOHIDPrimaryUsagePageKey] = @(kHIDPage_GenericDesktop);
    _properties[@kIOHIDPrimaryUsageKey] = @(kHIDUsage_GD_Mouse);
    _properties[@kIOHIDTransportKey] = @"Virtual";
    _properties[@kIOHIDDeviceUsagePairsKey] =  @[@{@kIOHIDDeviceUsagePageKey :
                                                       @(kHIDPage_GenericDesktop),
                                                   @kIOHIDDeviceUsageKey :
                                                       @(kHIDUsage_GD_Mouse)}];
    _properties[@kIOHIDPhysicalDeviceUniqueIDKey] = uniqueID;
    _properties[@kIOHIDScrollAccelerationKey] = @(20480);
    _properties[@kIOHIDScrollAccelerationTypeKey] = @(kIOHIDMouseScrollAccelerationKey);
    _properties[@kIOHIDScrollResolutionKey] = @(589824);
    
    _client = [[HIDEventSystemClient alloc] initWithType: HIDEventSystemClientTypeMonitor];
    
    [_client setMatching: @{ @kIOHIDPhysicalDeviceUniqueIDKey : uniqueID }];
    [_client setServiceNotificationHandler:^(HIDServiceClient * service){
        NSLog(@"setServiceNotificationHandler service: %@", service);
        self_.serviceClient = service;
        [self_.serviceExp fulfill];
    }];
    
    [_client setEventHandler: ^(HIDServiceClient *service __unused,
                                HIDEvent *event) {
        bool fulfill = true;
        
        NSLog(@"event: %@", event);
        
        for (HIDEvent *child in event.children) {
            if (child.type != kIOHIDEventTypePointer &&
                child.type != kIOHIDEventTypeScroll) {
                continue;
            }
            
            if (child.options & kIOHIDAccelerated) {
                fulfill = false;
            }
        }
        
        if (fulfill) {
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
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
}

- (void)tearDown
{
    [_client cancel];
    
    dispatch_sync(_queue, ^{
        [self.service cancel];
    });
}

- (void)MAC_OS_ONLY_TEST_CASE(testNoAcceleration)
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
    
    // pointer
    HIDEvent *event = CFBridgingRelease(IOHIDEventCreateRelativePointerEvent(
                                    kCFAllocatorDefault,
                                    mach_absolute_time(),
                                    1.0,
                                    0.0,
                                    0.0,
                                    0,
                                    0,
                                    kIOHIDPointerEventOptionsNoAcceleration));
    [_service dispatchEvent:event];
    
    // scroll
    event = CFBridgingRelease(IOHIDEventCreateScrollEvent(
                                    kCFAllocatorDefault,
                                    mach_absolute_time(),
                                    0.0,
                                    1.0,
                                    0.0,
                                    kIOHIDScrollEventOptionsNoAcceleration));
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
