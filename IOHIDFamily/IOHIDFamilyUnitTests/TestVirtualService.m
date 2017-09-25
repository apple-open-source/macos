//
//  TestVirtualService.m
//  IOHIDFamily
//
//  Created by yg on 1/23/17.
//
//

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


static void  __IOHIDVirtualServiceNotifyCallback ( void * target, void *  context,  IOHIDServiceClientRef service, uint32_t type, CFDictionaryRef property);

static bool  __IOHIDVirtualServiceSetPropertyCallback (void * _Nullable target, void * _Nullable context, IOHIDServiceClientRef service, CFStringRef key, CFTypeRef value);

static CFTypeRef  __IOHIDVirtualServiceCopyPropertyCallback (void * _Nullable target, void * _Nullable context, IOHIDServiceClientRef service,  CFStringRef key);

static IOHIDEventRef  __IOHIDVirtualServiceCopyEvent (void * _Nullable target, void * _Nullable context, IOHIDServiceClientRef service, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options);

static IOReturn    __IOHIDVirtualServiceSetOutputEvent (void * _Nullable target, void * _Nullable context, IOHIDServiceClientRef service, IOHIDEventRef event);

@interface TestVirtualService : XCTestCase

@property       IOHIDEventSystemClientRef       virtualServiceSystemClient;
@property       dispatch_queue_t                virtualServiceQueue;
@property       dispatch_queue_t                rootQueue;
@property       IOHIDServiceClientRef           virtualService;
@property       NSMutableDictionary             *virtualServicePropertyCache;
@property       dispatch_queue_t                eventControllerQueue;
@property       IOHIDEventSystemTestController *eventController;
@property       NSInteger                       virtualServiceOutputEventCount;

@end

@implementation TestVirtualService

- (void)setUp {
    self.virtualServiceSystemClient =  IOHIDEventSystemClientCreateWithType (kCFAllocatorDefault, kIOHIDEventSystemClientTypeSimple, NULL);
    XCTAssert (self.virtualServiceSystemClient != NULL);
    
    self.rootQueue = IOHIDUnitTestCreateRootQueue(63, 2);
    XCTAssert(self.rootQueue != NULL) ;
    
    self.virtualServiceQueue = dispatch_queue_create_with_target ("IOHIDVirtualService", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    XCTAssert(self.virtualServiceQueue != NULL);

    self.virtualServicePropertyCache = [[NSMutableDictionary alloc] init];
    self.virtualServicePropertyCache[@kIOHIDPrimaryUsagePageKey] = @(kHIDPage_GenericDesktop);
    self.virtualServicePropertyCache[@kIOHIDPrimaryUsageKey] = @(kHIDUsage_GD_Mouse);
    self.virtualServicePropertyCache[@kIOHIDVendorIDKey] = @(0xff00);
    self.virtualServicePropertyCache[@kIOHIDProductIDKey] = @(0xff00);
    self.virtualServicePropertyCache[@kIOHIDDeviceUsagePairsKey] =  @[
                                                                        @{
                                                                           @kIOHIDDeviceUsagePageKey : @(kHIDPage_GenericDesktop),
                                                                           @kIOHIDDeviceUsageKey: @(kHIDUsage_GD_Pointer)
                                                                        },
                                                                        @{
                                                                            @kIOHIDDeviceUsagePageKey : @(kHIDPage_AppleVendor),
                                                                            @kIOHIDDeviceUsageKey: @(kHIDPage_AppleVendor)
                                                                        }
                                                                    ];
    
    self.virtualServicePropertyCache[@kIOHIDTransportKey] = @"Virtual";
    
    NSString * uniqueID = [[[NSUUID alloc] init] UUIDString];

    self.virtualServicePropertyCache[@kIOHIDPhysicalDeviceUniqueIDKey] = uniqueID;
    
    IOHIDEventSystemClientScheduleWithDispatchQueue(self.virtualServiceSystemClient, self.virtualServiceQueue);
    
    
    dispatch_sync (self.virtualServiceQueue, ^() {
        IOHIDVirtualServiceClientCallbacks callbacks = {
            __IOHIDVirtualServiceNotifyCallback,
            __IOHIDVirtualServiceSetPropertyCallback,
            __IOHIDVirtualServiceCopyPropertyCallback,
            __IOHIDVirtualServiceCopyEvent,
            __IOHIDVirtualServiceSetOutputEvent
        };
        self.virtualService = IOHIDVirtualServiceClientCreate (self.virtualServiceSystemClient, NULL, callbacks,  (__bridge void * _Nullable)(self), NULL);
    });
    
    HIDXCTAssertAndThrowTrue (self.virtualService != NULL);

    sleep (2);

    self.eventControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.eventControllerQueue != nil);
    
    self.eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(self.eventController != nil);

}

- (void)tearDown {
    @autoreleasepool {
        self.eventController = nil;
    }
    
    dispatch_sync (self.virtualServiceQueue, ^() {
        if (self.virtualService) {
            IOHIDVirtualServiceClientRemove (self.virtualService);
             CFRelease(self.virtualService);
        }
        if (self.virtualServiceSystemClient) {
            IOHIDEventSystemClientUnscheduleFromDispatchQueue (self.virtualServiceSystemClient, self.virtualServiceQueue);
            CFRelease (self.virtualServiceSystemClient);
        }
    });

    [super tearDown];
}

- (void)testVirtualServiceDispatchEvent {
    
    uint8_t buffer[] = {1,2,3};
    IOHIDEventRef event = IOHIDEventCreateVendorDefinedEvent (kCFAllocatorDefault, mach_absolute_time(), 0,0,0, buffer, sizeof(buffer), 0);
    XCTAssert (event != NULL);
    
    for (NSInteger index = 0 ; index < 10; index++) {
        XCTAssert(IOHIDVirtualServiceClientDispatchEvent (self.virtualService, event) == TRUE);
    }
    
    // Allow event to be dispatched
    usleep (kDefaultReportDispatchCompletionTime);

    // Make copy
    NSArray *events = nil;
    @synchronized (self.eventController.events) {
        events = [self.eventController.events copy];
    }
    
    EVENTS_STATS stats =  [IOHIDEventSystemTestController getEventsStats:events];

    HIDTestEventLatency(stats);

    XCTAssert(stats.counts[kIOHIDEventTypeVendorDefined] == 10);
    

}

- (void)testVirtualServiceCopyEvent {
    
    IOHIDEventRef event = IOHIDServiceClientCopyEvent (self.eventController.eventService, 0, NULL, 0);
    XCTAssert(event != NULL);
}

- (void)testVirtualServiceSetOutputEvent {
    IOReturn  status;
    
    status = IOHIDServiceClientSetElementValue(self.eventController.eventService, kHIDPage_LEDs, kHIDUsage_LED_CapsLock, 1);
    XCTAssert (status == kIOReturnSuccess, "IOHIDServiceClientSetElementValue:%x",status);
    sleep (1);
    XCTAssert (self.virtualServiceOutputEventCount == 1);
    status = IOHIDServiceClientSetElementValue(self.eventController.eventService, kHIDPage_LEDs, kHIDUsage_LED_CapsLock, 0);
    XCTAssert (status == kIOReturnSuccess, "IOHIDServiceClientSetElementValue:%x",status);
    sleep (1);
    XCTAssert (self.virtualServiceOutputEventCount == 2);
}


- (id) serviceCopyProperty: (IOHIDServiceClientRef) __unused service   : (NSString *) key {
    return self.virtualServicePropertyCache[key];
}

- (Boolean) serviceSetProperty: (IOHIDServiceClientRef) __unused service  : (NSString *) key  : (id) value {
    self.virtualServicePropertyCache[key] = value;
    return true;
}

- (IOHIDEventRef) serviceCopyEvent: (IOHIDServiceClientRef) __unused service  : (IOHIDEventType) __unused type  : (IOHIDEventRef) __unused matching :  (IOOptionBits) __unused options {
    uint8_t buffer[] = {1,2,3};
    IOHIDEventRef event = IOHIDEventCreateVendorDefinedEvent (kCFAllocatorDefault, mach_absolute_time(), 0,0,0, buffer, sizeof(buffer), 0);
    XCTAssert (event != NULL);
    return event;
}


@end

void  __IOHIDVirtualServiceNotifyCallback ( void * __unused target, void *  __unused context,  IOHIDServiceClientRef __unused service, uint32_t type, CFDictionaryRef property) {

    NSLog(@"__IOHIDVirtualServiceNotifyCallback: type:%d property:%@", type, property);

}

bool  __IOHIDVirtualServiceSetPropertyCallback (void * target, void * __unused context, IOHIDServiceClientRef service, CFStringRef key, CFTypeRef value) {
    TestVirtualService *self = (__bridge TestVirtualService *)target;

    NSLog(@"__IOHIDVirtualServiceSetPropertyCallback Key:%@ Value:%@", key , value);

    return [self serviceSetProperty: service : (__bridge NSString *)(key) : (__bridge id)(value)];
}

CFTypeRef  __IOHIDVirtualServiceCopyPropertyCallback (void * target, void * __unused context, IOHIDServiceClientRef service,  CFStringRef key) {
    TestVirtualService *self = (__bridge TestVirtualService *)target;
    
    CFTypeRef value =  (__bridge CFTypeRef)([self serviceCopyProperty: service : (__bridge NSString *)(key)]);
    
    NSLog(@"__IOHIDVirtualServiceCopyPropertyCallback Key:%@ Value:%@", key, value);
    
    return value ? CFRetain(value) : NULL;
}

IOHIDEventRef  __IOHIDVirtualServiceCopyEvent (void * target, void * __unused context, IOHIDServiceClientRef service, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options) {
    TestVirtualService *self = (__bridge TestVirtualService *)target;

    NSLog(@"__IOHIDVirtualServiceCopyEvent");
    
    return [self serviceCopyEvent:service :type :matching :options];;
}

static IOReturn    __IOHIDVirtualServiceSetOutputEvent (void * target, void * __unused context, IOHIDServiceClientRef __unused service, IOHIDEventRef __unused event) {
    TestVirtualService *self = (__bridge TestVirtualService *)target;

    NSLog(@"__IOHIDVirtualServiceSetOutputEvent");

    self.virtualServiceOutputEventCount += 1;
    return kIOReturnSuccess;
}
