//
//  TestProperty.m
//  IOHIDFamily
//
//  Created by dekom on 2/21/17.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import <Foundation/Foundation.h>
#include <IOKit/hid/IOHIDEventSystemClient.h>

@interface TestProperty : XCTestCase

@property   IOHIDEventSystemClientRef   client;

@end

@implementation TestProperty

- (void)setUp {
    self.client = IOHIDEventSystemClientCreateSimpleClient(kCFAllocatorDefault);
    HIDXCTAssertAndThrowTrue(self.client != NULL);
}

- (void)tearDown {
    CFRelease(self.client);
    [super tearDown];
}

- (void)testIdleTimeProperty {
    CFNumberRef property = (CFNumberRef)IOHIDEventSystemClientCopyProperty(self.client, CFSTR(kIOHIDIdleTimeMicrosecondsKey));
    HIDXCTAssertAndThrowTrue(property != NULL);
    
    uint64_t val = 0;
    CFNumberGetValue(property, kCFNumberSInt64Type, &val);
    XCTAssert(val != 0);
    
    sleep(1);
    property = (CFNumberRef)IOHIDEventSystemClientCopyProperty(self.client, CFSTR(kIOHIDIdleTimeMicrosecondsKey));
    HIDXCTAssertAndThrowTrue(property != NULL);
    
    uint64_t newVal = 0;
    CFNumberGetValue(property, kCFNumberSInt64Type, &newVal);
    XCTAssert(newVal != 0);
    XCTAssert(newVal != val);
}


@end
