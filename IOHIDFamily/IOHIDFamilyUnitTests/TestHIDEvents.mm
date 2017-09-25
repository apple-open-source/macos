//
//  TestHIDEvents.m
//  IOHIDFamily
//
//  Created by yg on 2/23/17.
//
//

#import <XCTest/XCTest.h>
#include "IOHIDUnitTestUtility.h"
#import "IOHIDEventSystemTestController.h"
#include <stddef.h>

namespace base {
    
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
    
    #include "IOHIDEventDataBase.h"
    
#pragma clang diagnostic pop

}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmacro-redefined"

#include "IOHIDEventData.h"

#pragma clang diagnostic pop


@interface TestHIDEvents : XCTestCase

@end

@implementation TestHIDEvents

- (void)setUp {
    [super setUp];
    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
    [super tearDown];
}

- (void)testKeyboardEvent {
    CFIndex value;

    IOHIDEventRef event = IOHIDEventCreateKeyboardEvent (kCFAllocatorDefault,
                                                         mach_absolute_time(),
                                                         kHIDUsage_GD_Keyboard,
                                                         kHIDUsage_KeyboardC,
                                                         1,
                                                         0
                                                         );
    HIDXCTAssertAndThrowTrue (event != NULL);

    XCTAssert (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard);

    XCTAssert (IOHIDEventConformsTo (event, kIOHIDEventTypeKeyboard));

    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 1);
    
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldKeyboardDown, 0);
 
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 0);
    
    CFRelease (event);
}

- (void)testEventFieldAccess {
    CFIndex     intValue;
    IOHIDFloat  floatValue;
    IOHIDFloat  doubleValue;
    
    IOHIDEventRef event =  IOHIDEventCreateDigitizerEvent(kCFAllocatorDefault,
                                                         mach_absolute_time(),
                                                         kIOHIDDigitizerTransducerTypeHand,
                                                         2,
                                                         2,
                                                         0,
                                                         0,
                                                         1.0,
                                                         2.0,
                                                         0.0,
                                                         0.1,
                                                         0.0,
                                                         1,
                                                         1,
                                                         0);
    HIDXCTAssertAndThrowTrue (event != NULL);
    
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerIndex);
    XCTAssert (intValue == 2);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerTouch);
    XCTAssert (intValue == 1);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerRange);
    XCTAssert (intValue == 1);
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldDigitizerRange, 0);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerRange);
    XCTAssert (intValue == 0);
    
    floatValue = IOHIDEventGetFloatValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (floatValue == 1.0);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (intValue == 1);
    doubleValue = IOHIDEventGetDoubleValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (doubleValue == 1.0);
    
    IOHIDEventSetFloatValue (event, kIOHIDEventFieldDigitizerX, 2.0);
    floatValue = IOHIDEventGetFloatValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (floatValue == 2.0);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (intValue == 2, "intValue: %ld (0x%lx)", intValue, intValue);
    doubleValue = IOHIDEventGetDoubleValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (doubleValue == 2.0);

    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldDigitizerX, 3);
    floatValue = IOHIDEventGetFloatValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (floatValue == 3.0);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (intValue == 3, "intValue: %ld (0x%lx)", intValue, intValue);
    doubleValue = IOHIDEventGetDoubleValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (doubleValue == 3.0);

    IOHIDEventSetFloatValue (event, kIOHIDEventFieldDigitizerX, NAN);
    floatValue = IOHIDEventGetFloatValue (event, kIOHIDEventFieldDigitizerX);
    XCTAssert (floatValue != floatValue, "floatValue: %f", floatValue);
    
    CFRelease(event);
    
    event = IOHIDEventCreateAmbientLightSensorEvent(kCFAllocatorDefault, mach_absolute_time(), 0, 0);
    HIDXCTAssertAndThrowTrue (event != NULL);
    
    IOHIDEventSetDoubleValue (event, kIOHIDEventFieldAmbientLightColorComponent2, 5.0);
    floatValue = IOHIDEventGetFloatValue (event, kIOHIDEventFieldAmbientLightColorComponent2);
    XCTAssert (floatValue == 5.0);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldAmbientLightColorComponent2);
    XCTAssert (intValue == 5);
    doubleValue = IOHIDEventGetDoubleValue (event, kIOHIDEventFieldAmbientLightColorComponent2);
    XCTAssert (doubleValue == 5.0);

    CFRelease(event);
    
    event = IOHIDEventCreateKeyboardEvent (kCFAllocatorDefault,
                                           mach_absolute_time(),
                                           kHIDUsage_GD_Keyboard,
                                           kHIDUsage_KeyboardC,
                                           1,
                                           0
                                           );
    HIDXCTAssertAndThrowTrue (event != NULL);
    
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldIsRelative, 1);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldIsRelative);
    XCTAssert (intValue == 1);
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldIsCenterOrigin, 1);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldIsCenterOrigin);
    XCTAssert (intValue == 1);
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldIsBuiltIn, 1);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldIsBuiltIn);
    XCTAssert (intValue == 1);
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldIsPixelUnits, 1);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldIsPixelUnits);
    XCTAssert (intValue == 1);
    IOHIDEventSetIntegerValue (event, kIOHIDEventFieldIsCollection, 1);
    intValue = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldIsCollection);
    XCTAssert (intValue == 1);
    
    CFRelease(event);
}



- (void)testEventCopy {
    CFIndex value;
    IOHIDEventRef base = IOHIDEventCreateKeyboardEvent (kCFAllocatorDefault,
                                                         mach_absolute_time(),
                                                         kHIDUsage_GD_Keyboard,
                                                         kHIDUsage_KeyboardC,
                                                         1,
                                                         0
                                                         );
    HIDXCTAssertAndThrowTrue (base != NULL);
    
    IOHIDEventRef event = IOHIDEventCreateCopy (kCFAllocatorDefault, base);
    HIDXCTAssertAndThrowTrue (event != NULL);

    XCTAssert (event != base);
    XCTAssert (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard);
    
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);
    value = IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 1);
    
    CFRelease(event);
    CFRelease(base);
}

- (void)testEventCollection {
    CFIndex value;
    uint8_t payload [256] = {0xaa, 0x55, 0xaa, 0x55} ;
    IOHIDEventRef parent = IOHIDEventCreateKeyboardEvent (kCFAllocatorDefault,
                                                          mach_absolute_time(),
                                                          kHIDUsage_GD_Keyboard,
                                                          kHIDUsage_KeyboardC,
                                                          1,
                                                          0
                                                          );
    HIDXCTAssertAndThrowTrue (parent != NULL);

    IOHIDEventRef child = IOHIDEventCreateVendorDefinedEvent(kCFAllocatorDefault,
                                                             mach_absolute_time(),
                                                             kHIDUsage_GD_Keyboard,
                                                             kHIDUsage_KeyboardC,
                                                             0,
                                                             payload,
                                                             sizeof(payload),
                                                             0);
    HIDXCTAssertAndThrowTrue (child != NULL);
    
    IOHIDEventAppendEvent (parent, child, 0);
    
    XCTAssert (IOHIDEventConformsTo(parent, kIOHIDEventTypeKeyboard));
    XCTAssert (IOHIDEventConformsTo(parent, kIOHIDEventTypeVendorDefined));
    
    NSArray * children =  (NSArray*)IOHIDEventGetChildren (parent);
    XCTAssert (children && children.count);

    XCTAssert (IOHIDEventGetParent (child) == parent);
    
    IOHIDEventRef copy = IOHIDEventCreateCopy (kCFAllocatorDefault, parent);
    HIDXCTAssertAndThrowTrue (copy != NULL);
    
    children =  (NSArray*)IOHIDEventGetChildren (copy);
    XCTAssert (children && children.count);

    XCTAssert (IOHIDEventConformsTo(copy, kIOHIDEventTypeKeyboard));
    XCTAssert (IOHIDEventConformsTo(copy, kIOHIDEventTypeVendorDefined));

    IOHIDEventRef copy_child = IOHIDEventGetEvent(copy, kIOHIDEventTypeVendorDefined);
    HIDXCTAssertAndThrowTrue (copy_child != NULL);
    
    XCTAssert (copy_child != child);
 
    value = IOHIDEventGetIntegerValue (copy_child, kIOHIDEventFieldVendorDefinedUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);

    value = IOHIDEventGetIntegerValue (copy_child, kIOHIDEventFieldVendorDefinedUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);

    value = IOHIDEventGetIntegerValue (copy_child, kIOHIDEventFieldVendorDefinedDataLength);
    XCTAssert (value == sizeof (payload));
    
    uint8_t *data = IOHIDEventGetDataValue (copy_child, kIOHIDEventFieldVendorDefinedData);
    XCTAssert (data != NULL);
    XCTAssert (memcmp (data, payload, value) == 0);
    
    CFRelease (parent);
    CFRelease (child);
    CFRelease (copy);
    
}

- (void)testEventCompatibility {
    #include "TestHIDEventCompatibility.h"
}
@end
