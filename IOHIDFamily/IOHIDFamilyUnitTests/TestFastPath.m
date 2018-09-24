//
//  TestFastPath.m
//  IOHIDFamily
//
//  Created by yg on 12/20/16.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#include "IOHIDPrivateKeys.h"
#import  "IOHIDEventSystemTestController.h"
#import  "IOHIDUserDeviceTestController.h"
#import  "IOHIDDeviceTestController.h"
#import "IOHIDUnitTestDescriptors.h"

static NSString * deviceDescription =
@"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">"
"   <plist version=\"1.0\">          "
"   <dict>                           "
"     <key>VendorID</key>            "
"     <integer>65280</integer>         "
"     <key>ProductID</key>           "
"     <integer>65280</integer>         "
"     <key>ReportInterval</key>      "
"     <integer>10000</integer>       "
"     <key>RequestTimeout</key>      "
"     <integer>5000000</integer>     "
"     <key>UnitTestService</key>     "
"     <true/>                        "
"     <key>Product</key>             "
"     <string>10318C6F-6C4F-4B62-A8AB-4E706E9B50E0</string>"
"   </dict>                          "
"   </plist>                         ";

static uint8_t descriptor[] = {
    HIDVendorMessage32BitDescriptor
};

@interface TestFastPath : XCTestCase

@property IOHIDUserDeviceTestController *   sourceController;

@property dispatch_queue_t                  eventControllerQueue;
@property dispatch_queue_t                  rootQueue;
@property NSString *                        uniqueID;
@end

@implementation TestFastPath

- (void)setUp {
    [super setUp];
    self.rootQueue = IOHIDUnitTestCreateRootQueue(31, 2);
    
    system("kextload /AppleInternal/CoreOS/tests/IOHIDFamily/IOHIDEventFastPathTestDriver.kext");
    
    self.eventControllerQueue = dispatch_queue_create_with_target ("IOHIDEventSystemTestController", DISPATCH_QUEUE_SERIAL, self.rootQueue);
    HIDXCTAssertAndThrowTrue(self.eventControllerQueue != nil);
    
    NSData * descriptorData = [[NSData alloc] initWithBytes:descriptor length:sizeof(descriptor)];
    self.uniqueID = [[[NSUUID alloc] init] UUIDString];
    
    NSMutableDictionary* deviceConfig = [NSPropertyListSerialization propertyListWithData:[deviceDescription dataUsingEncoding:NSUTF8StringEncoding] options:NSPropertyListMutableContainers format:NULL error:NULL];
    
    deviceConfig [@kIOHIDReportDescriptorKey] = descriptorData;
    deviceConfig [@kIOHIDPhysicalDeviceUniqueIDKey] = self.uniqueID;
    
    self.sourceController = [[IOHIDUserDeviceTestController alloc] initWithDeviceConfiguration:deviceConfig andQueue:nil];
    HIDXCTAssertAndThrowTrue(self.sourceController != nil);
}

- (void)tearDown {

    [self.sourceController invalidate];

    @autoreleasepool {
        self.sourceController = nil;
    }
    
    int status = system ("leaks hidxctest");
    XCTAssert (status == 0);
    
    system("kextunload -b com.apple.IOHIDEventFastPathDriver");
    
    [super tearDown];
}

- (void)MAC_OS_ONLY_TEST_CASE(testFastPathInit) {
    Boolean result;
    NSString *ioClassName;
    
    IOHIDEventSystemTestController * eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(eventController != nil);
    
    ioClassName  = CFBridgingRelease(IOHIDServiceClientCopyProperty(eventController.eventService,CFSTR("IOClass")));
    XCTAssertTrue (ioClassName && [ioClassName isEqualToString: @"IOHIDEventFastPathDriver"]);
    
    for (NSInteger index = 0; index < 10; index ++) {
        result = IOHIDServiceClientFastPathInit (eventController.eventService, NULL);
        XCTAssertTrue (result);
        IOHIDServiceClientFastPathInvalidate (eventController.eventService);
    }
    
    [eventController invalidate];
}

- (void)MAC_OS_ONLY_TEST_CASE(testFastPathInitError) {
    Boolean result;
    NSString *ioClassName;
    NSDictionary * properties;
    IOHIDEventRef event;
    
    IOHIDEventSystemTestController * eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(eventController != nil);
    
    ioClassName  = CFBridgingRelease(IOHIDServiceClientCopyProperty(eventController.eventService,CFSTR("IOClass")));
    XCTAssertTrue (ioClassName && [ioClassName isEqualToString: @"IOHIDEventFastPathDriver"]);


    properties = @{@"RequireEntitlement" : @YES};
    result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)properties);
    XCTAssertTrue (result == false);
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);


    properties = @{@"RequireEntitlement" : @NO};
    result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)properties);
    XCTAssertTrue (result == true);
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);
 
    
    properties = @{@"QueueSize" : @0};
    result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)properties);
    XCTAssertTrue (result == true);
    event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, NULL, 0);
    HIDXCTAssertAndThrowTrue (event == NULL);
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);
    
    
    properties = @{@"QueueSize" : @2};
    result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)properties);
    XCTAssertTrue (result == false);
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);
  
    
    properties = @{@"QueueSize" : @6};
    result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)properties);
    XCTAssertTrue (result == true);
    
    event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, NULL, 0);
    HIDXCTAssertAndThrowTrue (event == NULL);
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);

    properties = @{@"QueueSize" : @120};
    result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)properties);
    XCTAssertTrue (result == true);

    event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, NULL, 0);
    HIDXCTAssertAndThrowTrue (event != NULL);
    CFRelease(event);
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);

    [eventController invalidate];
}


- (void)MAC_OS_ONLY_TEST_CASE(testFastPathProperty) {
    Boolean result;
    CFTypeRef value;
    NSString *ioClassName;
    NSMutableArray *clients = [[NSMutableArray alloc] initWithCapacity:10];
    for (NSUInteger index = 1 ; index < 10 ; index++) {
        IOHIDEventSystemTestController *  eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
        HIDXCTAssertAndThrowTrue(eventController != nil);

        [clients addObject:eventController];
        ioClassName  = CFBridgingRelease(IOHIDServiceClientCopyProperty(eventController.eventService,CFSTR("IOClass")));
        HIDXCTAssertAndThrowTrue (ioClassName && [ioClassName isEqualToString: @"IOHIDEventFastPathDriver"]);
        
        NSNumber * clientIndex =  [[NSNumber alloc] initWithLong:index];
        NSDictionary * clientSpec = @{@"Client":clientIndex};
        result = IOHIDServiceClientFastPathInit (eventController.eventService, (CFDictionaryRef)clientSpec);
        HIDXCTAssertAndThrowTrue (result);
        
        value = IOHIDServiceClientFastPathCopyProperty (eventController.eventService, CFSTR("Client"));
        XCTAssertTrue (value != NULL && CFEqual (value ,(CFNumberRef)clientIndex));
        
        if (value) {
            CFRelease(value);
        }
        
        result = IOHIDServiceClientFastPathSetProperty (eventController.eventService, CFSTR("Test") , (CFNumberRef)clientIndex);
        XCTAssertTrue (result);
        
        value = IOHIDServiceClientFastPathCopyProperty (eventController.eventService, CFSTR("Test"));
        XCTAssertTrue (value != NULL && CFEqual (value ,(CFNumberRef)clientIndex));
 
        if (value) {
            CFRelease(value);
        }
        
        IOHIDServiceClientFastPathInvalidate (eventController.eventService);
        
        [eventController invalidate];

    }
}

- (void)MAC_OS_ONLY_TEST_CASE(testFastPathEventCopy) {
    Boolean result;
    NSString *ioClassName;
    IOHIDEventRef event;
    NSArray* children;

    IOHIDEventSystemTestController *  eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(eventController != nil);

    result = IOHIDServiceClientFastPathInit (eventController.eventService, NULL);
    HIDXCTAssertAndThrowTrue (result);

    event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, NULL, 0);
    HIDXCTAssertAndThrowTrue (event != NULL);
    CFRelease (event);

    id value = CFBridgingRelease(IOHIDServiceClientFastPathCopyProperty(eventController.eventService, CFSTR(kIOHIDEventServiceQueueSize)));
    NSLog(@"Queue size:%@", value);
 
    ioClassName  = CFBridgingRelease(IOHIDServiceClientCopyProperty(eventController.eventService,CFSTR("IOClass")));
    HIDXCTAssertAndThrowTrue (ioClassName && [ioClassName isEqualToString: @"IOHIDEventFastPathDriver"]);

    for (NSUInteger index = 1 ; index < 500 ; index++) {
        
        NSLog(@"Copy:%d events", (int)index);
        
        NSDictionary * dictCopySpec = @{@"NumberOfEventToCopy":@(index)};
        event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, (__bridge CFDictionaryRef)dictCopySpec, 0);
        HIDXCTAssertAndThrowTrue (event != NULL);
        if (index > 1) {
            children = (NSArray*)IOHIDEventGetChildren (event);
            XCTAssertTrue (children != NULL && children.count == index, "index:%d events:%@", (int)index, children);
        }
        CFRelease (event);
        
        NSData * dataCopySpec = [NSData dataWithBytes: &index length:sizeof(index)];
        event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, (__bridge CFDataRef)dataCopySpec, 0);
        HIDXCTAssertAndThrowTrue (event != NULL);
        if (index > 1) {
            children = (NSArray*)IOHIDEventGetChildren (event);
            XCTAssertTrue (children != NULL && children.count == index);
        }
        CFRelease (event);
        
    }
    IOHIDServiceClientFastPathInvalidate (eventController.eventService);
    [eventController invalidate];
}

- (void)MAC_OS_ONLY_TEST_CASE(testFastPathEventCopyWithInvalidation) {
    Boolean result;
    NSString *ioClassName;
    IOHIDEventRef event;
    NSArray* children;
    
    IOHIDEventSystemTestController *  eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(eventController != nil);
    
    
    for (NSUInteger index = 1 ; index < 500 ; index++) {

        result = IOHIDServiceClientFastPathInit (eventController.eventService, NULL);
        HIDXCTAssertAndThrowTrue (result);
        
        event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, NULL, 0);
        HIDXCTAssertAndThrowTrue (event != NULL);
        CFRelease (event);
        
        ioClassName  = CFBridgingRelease(IOHIDServiceClientCopyProperty(eventController.eventService,CFSTR("IOClass")));
        HIDXCTAssertAndThrowTrue (ioClassName && [ioClassName isEqualToString: @"IOHIDEventFastPathDriver"]);

        NSLog(@"Copy:%d events", (int)index);
        
        NSDictionary * dictCopySpec = @{@"NumberOfEventToCopy":@(index)};
        event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, (__bridge CFDictionaryRef)dictCopySpec, 0);
        HIDXCTAssertAndThrowTrue (event != NULL);
        if (index > 1) {
            children = (NSArray*)IOHIDEventGetChildren (event);
            XCTAssertTrue (children != NULL && children.count == index, "index:%d events:%@", (int)index, children);
        }
        CFRelease (event);
        
        NSData * dataCopySpec = [NSData dataWithBytes: &index length:sizeof(index)];
        event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, (__bridge CFDataRef)dataCopySpec, 0);
        HIDXCTAssertAndThrowTrue (event != NULL);

        if (index > 1) {
            children = (NSArray*)IOHIDEventGetChildren (event);
            XCTAssertTrue (children != NULL && children.count == index);
        }
        CFRelease (event);

        IOHIDServiceClientFastPathInvalidate (eventController.eventService);
        [eventController invalidate];
    }
}


- (void)CONDTIONAL_TEST_CASE(PerformanceOfCopyWithDictSpec) {
    Boolean result;
    IOHIDEventSystemTestController *  eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    XCTAssertNotNil(eventController);
    
    result = IOHIDServiceClientFastPathInit (eventController.eventService, NULL);
    HIDXCTAssertAndThrowTrue (result);
    
    NSDictionary * copySpec = @{@"NumberOfEventToCopy":@(5)};
    [self measureBlock:^{
        IOHIDEventRef event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, (CFDictionaryRef)copySpec, 0);
        XCTAssertTrue (event != NULL);
        if (event) {
            CFRelease(event);
        }
    }];

    [eventController invalidate];

}
- (void)CONDTIONAL_TEST_CASE(PerformanceOfCopyWithNullSpec) {
    Boolean result;
    IOHIDEventSystemTestController *  eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    HIDXCTAssertAndThrowTrue(eventController != nil);
    
    result = IOHIDServiceClientFastPathInit (eventController.eventService, NULL);
    XCTAssertTrue (result);
    
    [self measureBlock:^{
        IOHIDEventRef event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, NULL, 0);
        XCTAssertTrue (event != NULL);
        if (event) {
            CFRelease(event);
        }
    }];

    [eventController invalidate];
}

- (void)CONDTIONAL_TEST_CASE(PerformanceOfCopyWithDataSpec) {
    Boolean result;
    IOHIDEventSystemTestController *  eventController = [[IOHIDEventSystemTestController alloc] initWithDeviceUniqueID:self.uniqueID AndQueue:self.eventControllerQueue];
    XCTAssertNotNil(eventController);
    
    result = IOHIDServiceClientFastPathInit (eventController.eventService, NULL);
    XCTAssertTrue (result);
    
    NSUInteger copyCount = 5;
    NSData * dataCopySpec = [NSData dataWithBytes: &copyCount length:sizeof(copyCount)];
    [self measureBlock:^{
        IOHIDEventRef event = IOHIDServiceClientFastPathCopyEvent (eventController.eventService, (__bridge CFDataRef)dataCopySpec,0);
        XCTAssertTrue (event != NULL);
        if (event) {
            CFRelease(event);
        }
    }];

    [eventController invalidate];

}

@end
