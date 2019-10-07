//
//  TestHIDActionQueue.m
//  IOHIDFamilyUnitTests
//
//  Created by dekom on 3/15/18.
//

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import <IOKit/IOKitLib.h>

@interface TestHIDActionQueue : XCTestCase

@end

@implementation TestHIDActionQueue

- (void)setUp
{
    [super setUp];
    
#if TARGET_OS_OSX
    system("kextload /AppleInternal/CoreOS/tests/IOHIDFamily/IOHIDActionQueueTestDriver.kext");
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
#endif
}

- (void)tearDown
{
#if TARGET_OS_OSX
    system("kextunload /AppleInternal/CoreOS/tests/IOHIDFamily/IOHIDActionQueueTestDriver.kext");
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
#endif
    
    [super tearDown];
}

- (void)MAC_OS_ONLY_TEST_CASE(testActionQueue)
{
    io_service_t service;
    kern_return_t kr;
    CFBooleanRef result = NULL;
    
    service = IOServiceGetMatchingService(MACH_PORT_NULL, IOServiceMatching("IOHIDActionQueueTestDriver"));
    XCTAssert(service);
    
    kr = IORegistryEntrySetCFProperty(service, CFSTR("StartTest"), kCFBooleanTrue);
    XCTAssert(kr == kIOReturnSuccess);
    
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, 5.0, false);
    
    result = IORegistryEntryCreateCFProperty(service, CFSTR("TestStarted"), kCFAllocatorDefault, kNilOptions);
    NSLog(@"TestStart: %@", result);
    XCTAssert(result == kCFBooleanTrue);
    
    result = IORegistryEntryCreateCFProperty(service, CFSTR("TestFinished"), kCFAllocatorDefault, kNilOptions);
    NSLog(@"TestFinished: %@", result);
    XCTAssert(result == kCFBooleanTrue);
    
    result = IORegistryEntryCreateCFProperty(service, CFSTR("CancelHandlerCalled"), kCFAllocatorDefault, kNilOptions);
    NSLog(@"CancelHandlerCalled: %@", result);
    XCTAssert(result == kCFBooleanTrue);
    
    IOObjectRelease(service);
}

@end
