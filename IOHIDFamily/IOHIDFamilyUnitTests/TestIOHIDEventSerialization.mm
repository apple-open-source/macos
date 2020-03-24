//
//  TestIOHIDEventSerialization.mm
//  IOHIDFamilyUnitTests
//
//  Created by AB on 2/27/18.
//

#import <XCTest/XCTest.h>
#include <IOKit/hid/IOHIDEvent.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include "IOHIDUnitTestUtility.h"

#define APPLE_KEXT_OVERRIDE                  override

#include "TestIOHIDEventSerializationWrapper.h"

void OSDefineMetaClassAndStructors();
#define OSDeclareAbstractStructors( IOHIDEvent ) IOHIDEvent() : _data(NULL), _children(NULL), _parent(NULL), _capacity(0), _eventCount(0) {}
#define OSDefineMetaClassAndStructors(IOHIDEvent, OSObject) void OSDefineMetaClassAndStructors() {}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wfour-char-constants"
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
//dummy reference
#define GET_EVENT_DATA(event, field, value, options ) { if(field && options){ value = 0;}}
#define GET_EVENT_VALUE(event, field, value, options, typeToken) { if(field && options){ value = 0;}}
#define SET_EVENT_VALUE(event, field, value, options, typeToken)  { if(field && options){ value = 0;}}
#include "IOHIDEvent.h"
#include "IOHIDEvent.cpp"
#undef super
#pragma clang diagnostic pop

namespace base {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdocumentation"
#pragma clang diagnostic ignored "-Wmacro-redefined"
#include "IOHIDEventDataBase.h"
#pragma clang diagnostic pop
}


@interface TestIOHIDEventSerialization : XCTestCase
@end

@implementation TestIOHIDEventSerialization

- (void)setUp {
    [super setUp];
    
}

- (void)tearDown {
    
    [super tearDown];
}

- (void)testEventTreeVendorChildMiddle {
    
    uint8_t payload [256] = {0xaa, 0x55, 0xaa, 0x55} ;
    //create keyboard event
    IOHIDEvent *keyboardEvent = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 1);
    HIDXCTAssertAndThrowTrue (keyboardEvent != NULL);
    size_t keyboardEventLength = keyboardEvent->getLength();
    
    //create vendor event
    IOHIDEvent *vendorEvent = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payload, sizeof(payload));
    HIDXCTAssertAndThrowTrue (vendorEvent != NULL);
    size_t vendorEventLength = (IOByteCount)vendorEvent->getLength();
    
    //create scroll event
    IOHIDEvent *scrollEvent = IOHIDEvent::scrollEvent(mach_absolute_time(), 2, 3, 1);
    HIDXCTAssertAndThrowTrue (scrollEvent != NULL);
    size_t scrollEventLength = scrollEvent->getLength();
    
    //create ambient light sensor event
    IOHIDEvent *ambientLightSensorEvent = IOHIDEvent::ambientLightSensorEvent(mach_absolute_time(), 1);
    HIDXCTAssertAndThrowTrue (ambientLightSensorEvent != NULL);
    size_t ambientLightSensorEventLength = ambientLightSensorEvent->getLength();
    
    //construct tree
    keyboardEvent->appendChild(scrollEvent);
    keyboardEvent->appendChild(vendorEvent);
    keyboardEvent->appendChild(ambientLightSensorEvent);
    
    //get total length
    size_t totalLength = keyboardEvent->getLength();
    
    
    XCTAssert((keyboardEventLength + vendorEventLength + scrollEventLength + ambientLightSensorEventLength - 3*sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    
    uint8_t *eventBytes = (uint8_t*)malloc(totalLength);
    size_t readBytes = keyboardEvent->readBytes(eventBytes, (IOByteCount)totalLength);
    
    XCTAssert((readBytes + sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    IOHIDEventRef parentEvent =  IOHIDEventCreateWithBytes(kCFAllocatorDefault, eventBytes, (CFIndex)totalLength);
    HIDXCTAssertAndThrowTrue (parentEvent != NULL);
    
    //verify user event
    CFArrayRef children =  IOHIDEventGetChildren(parentEvent);
    IOHIDEventRef childEvent = NULL;
    CFIndex value;
    
    XCTAssert(CFArrayGetCount(children) == 3);
    
    //verify parent event
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);
    
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);
    
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 1);
    
    
    //verify left child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 0);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollX);
    XCTAssert (value == 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollY);
    XCTAssert (value == 3);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollZ);
    XCTAssert (value == 1);
    
    
    //verify middle child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 1);
    
    uint8_t *vendorData = IOHIDEventGetDataValue(childEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payload, vendorData, sizeof(payload)) == 0);
    
    //verify right child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldAmbientLightSensorLevel);
    XCTAssert (value == 1);
    
    //reconstruct back kernel event
    IOHIDEvent *eventCpy = IOHIDEvent::withBytes(eventBytes, (IOByteCount)totalLength);
    
    HIDXCTAssertAndThrowTrue (eventCpy != NULL);
    
    OSArray *eventCpyChildren = eventCpy->getChildren();
    
    XCTAssert(eventCpyChildren != NULL);
    
    XCTAssert(eventCpyChildren->getCount() == 3);
    
    eventCpy->release();
    
    for (unsigned int index = 0; index < eventCpyChildren->getCount(); index++) {
        IOHIDEvent *eventCpyChild = (IOHIDEvent*)eventCpyChildren->getObject(index);
        XCTAssert(eventCpyChild != NULL);
        eventCpyChild->release();
    }
    
    
    free(eventBytes);
    vendorEvent->release();
    scrollEvent->release();
    ambientLightSensorEvent->release();
    keyboardEvent->release();
    CFRelease(parentEvent);
}

- (void)testEventTreeVendorChildLeft {
    
    uint8_t payload [256] = {0xaa, 0x55, 0xaa, 0x55} ;
    
    //create keyboard event
    IOHIDEvent *keyboardEvent = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 1);
    HIDXCTAssertAndThrowTrue (keyboardEvent != NULL);
    size_t keyboardEventLength = keyboardEvent->getLength();
    
    //create vendor event
    IOHIDEvent *vendorEvent = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payload, sizeof(payload));
    HIDXCTAssertAndThrowTrue (vendorEvent != NULL);
    size_t vendorEventLength = (IOByteCount)vendorEvent->getLength();
    
    //create scroll event
    IOHIDEvent *scrollEvent = IOHIDEvent::scrollEvent(mach_absolute_time(), 2, 3, 1);
    HIDXCTAssertAndThrowTrue (scrollEvent != NULL);
    size_t scrollEventLength = scrollEvent->getLength();
    
    //create ambient light sensor event
    IOHIDEvent *ambientLightSensorEvent = IOHIDEvent::ambientLightSensorEvent(mach_absolute_time(), 1);
    HIDXCTAssertAndThrowTrue (ambientLightSensorEvent != NULL);
    size_t ambientLightSensorEventLength = ambientLightSensorEvent->getLength();
    
    
    //construct tree
    keyboardEvent->appendChild(vendorEvent);
    keyboardEvent->appendChild(scrollEvent);
    keyboardEvent->appendChild(ambientLightSensorEvent);
    
    //get total length
    size_t totalLength = keyboardEvent->getLength();
    
    
    XCTAssert((keyboardEventLength + vendorEventLength + scrollEventLength + ambientLightSensorEventLength - 3*sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    
    uint8_t *eventBytes = (uint8_t*)malloc(totalLength);
    size_t readBytes = keyboardEvent->readBytes(eventBytes, (IOByteCount)totalLength);
    
    XCTAssert((readBytes + sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    IOHIDEventRef parentEvent =  IOHIDEventCreateWithBytes(kCFAllocatorDefault, eventBytes, (CFIndex)totalLength);
    HIDXCTAssertAndThrowTrue (parentEvent != NULL);
    
    //verify user event
    CFArrayRef children =  IOHIDEventGetChildren(parentEvent);
    IOHIDEventRef childEvent = NULL;
    CFIndex value;
    
    XCTAssert(CFArrayGetCount(children) == 3);
    
    //verify parent event
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);
    
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);
    
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 1);
    
    
    //verify middle child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 1);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollX);
    XCTAssert (value == 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollY);
    XCTAssert (value == 3);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollZ);
    XCTAssert (value == 1);
    
    
    //verify left child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 0);
    
    uint8_t *vendorData = IOHIDEventGetDataValue(childEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payload, vendorData, sizeof(payload)) == 0);
    
    //verify right child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldAmbientLightSensorLevel);
    XCTAssert (value == 1);
    
    //reconstruct back kernel event
    IOHIDEvent *eventCpy = IOHIDEvent::withBytes(eventBytes, (IOByteCount)totalLength);
    
    HIDXCTAssertAndThrowTrue (eventCpy != NULL);
    
    OSArray *eventCpyChildren = eventCpy->getChildren();
    
    XCTAssert(eventCpyChildren != NULL);
    
    XCTAssert(eventCpyChildren->getCount() == 3);
    
    eventCpy->release();
    
    for (unsigned int index = 0; index < eventCpyChildren->getCount(); index++) {
        IOHIDEvent *eventCpyChild = (IOHIDEvent*)eventCpyChildren->getObject(index);
        XCTAssert(eventCpyChild != NULL);
        eventCpyChild->release();
    }
    
    free(eventBytes);
    vendorEvent->release();
    scrollEvent->release();
    ambientLightSensorEvent->release();
    keyboardEvent->release();
    CFRelease(parentEvent);
    
}
- (void)testEventTreeVendorChildRight {
    
    uint8_t payload [256] = {0xaa, 0x55, 0xaa, 0x55} ;
    
    //create keyboard event
    IOHIDEvent *keyboardEvent = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 1);
    HIDXCTAssertAndThrowTrue (keyboardEvent != NULL);
    size_t keyboardEventLength = keyboardEvent->getLength();
    
    //create vendor event
    IOHIDEvent *vendorEvent = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payload, sizeof(payload));
    HIDXCTAssertAndThrowTrue (vendorEvent != NULL);
    size_t vendorEventLength = (IOByteCount)vendorEvent->getLength();
    
    //create scroll event
    IOHIDEvent *scrollEvent = IOHIDEvent::scrollEvent(mach_absolute_time(), 2, 3, 1);
    HIDXCTAssertAndThrowTrue (scrollEvent != NULL);
    size_t scrollEventLength = scrollEvent->getLength();
    
    //create ambient light sensor event
    IOHIDEvent *ambientLightSensorEvent = IOHIDEvent::ambientLightSensorEvent(mach_absolute_time(), 1);
    HIDXCTAssertAndThrowTrue (ambientLightSensorEvent != NULL);
    size_t ambientLightSensorEventLength = ambientLightSensorEvent->getLength();
    
    
    //construct tree
    keyboardEvent->appendChild(scrollEvent);
    keyboardEvent->appendChild(ambientLightSensorEvent);
    keyboardEvent->appendChild(vendorEvent);
    
    //get total length
    size_t totalLength = keyboardEvent->getLength();
    
    
    XCTAssert((keyboardEventLength + vendorEventLength + scrollEventLength + ambientLightSensorEventLength - 3*sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    
    uint8_t *eventBytes = (uint8_t*)malloc(totalLength);
    size_t readBytes = keyboardEvent->readBytes(eventBytes, (IOByteCount)totalLength);
    
    XCTAssert((readBytes + sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    IOHIDEventRef parentEvent =  IOHIDEventCreateWithBytes(kCFAllocatorDefault, eventBytes, (CFIndex)totalLength);
    HIDXCTAssertAndThrowTrue (parentEvent != NULL);
    
    //verify user event
    CFArrayRef children =  IOHIDEventGetChildren(parentEvent);
    IOHIDEventRef childEvent = NULL;
    CFIndex value;
    
    XCTAssert(CFArrayGetCount(children) == 3);
    
    //verify parent event
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);
    
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);
    
    value = IOHIDEventGetIntegerValue (parentEvent, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 1);
    
    
    //verify left child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 0);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollX);
    XCTAssert (value == 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollY);
    XCTAssert (value == 3);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollZ);
    XCTAssert (value == 1);
    
    
    //verify right child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 2);
    
    uint8_t *vendorData = IOHIDEventGetDataValue(childEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payload, vendorData, sizeof(payload)) == 0);
    
    //verify middle child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 1);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldAmbientLightSensorLevel);
    XCTAssert (value == 1);
    
    //reconstruct back kernel event
    IOHIDEvent *eventCpy = IOHIDEvent::withBytes(eventBytes, (IOByteCount)totalLength);
    
    HIDXCTAssertAndThrowTrue (eventCpy != NULL);
    
    OSArray *eventCpyChildren = eventCpy->getChildren();
    
    XCTAssert(eventCpyChildren != NULL);
    
    XCTAssert(eventCpyChildren->getCount() == 3);
    
    eventCpy->release();
    
    for (unsigned int index = 0; index < eventCpyChildren->getCount(); index++) {
        IOHIDEvent *eventCpyChild = (IOHIDEvent*)eventCpyChildren->getObject(index);
        XCTAssert(eventCpyChild != NULL);
        eventCpyChild->release();
    }
    
    free(eventBytes);
    vendorEvent->release();
    scrollEvent->release();
    ambientLightSensorEvent->release();
    keyboardEvent->release();
    CFRelease(parentEvent);
}
- (void)testEventTreeVendorParent {
    
    uint8_t payload [256] = {0xaa, 0x55, 0xaa, 0x55} ;
    //create keyboard event
    IOHIDEvent *keyboardEvent = IOHIDEvent::keyboardEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 1);
    HIDXCTAssertAndThrowTrue (keyboardEvent != NULL);
    size_t keyboardEventLength = keyboardEvent->getLength();
    
    //create vendor event
    IOHIDEvent *vendorEvent = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payload, sizeof(payload));
    HIDXCTAssertAndThrowTrue (vendorEvent != NULL);
    size_t vendorEventLength = (IOByteCount)vendorEvent->getLength();
    
    //create scroll event
    IOHIDEvent *scrollEvent = IOHIDEvent::scrollEvent(mach_absolute_time(), 2, 3, 1);
    HIDXCTAssertAndThrowTrue (scrollEvent != NULL);
    size_t scrollEventLength = scrollEvent->getLength();
    
    //create ambient light sensor event
    IOHIDEvent *ambientLightSensorEvent = IOHIDEvent::ambientLightSensorEvent(mach_absolute_time(), 1);
    HIDXCTAssertAndThrowTrue (ambientLightSensorEvent != NULL);
    size_t ambientLightSensorEventLength = ambientLightSensorEvent->getLength();
    
    //construct tree
    vendorEvent->appendChild(keyboardEvent);
    vendorEvent->appendChild(scrollEvent);
    vendorEvent->appendChild(ambientLightSensorEvent);
    
    //get total length
    size_t totalLength = vendorEvent->getLength();
    
    
    XCTAssert((keyboardEventLength + vendorEventLength + scrollEventLength + ambientLightSensorEventLength - 3*sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    
    uint8_t *eventBytes = (uint8_t*)malloc(totalLength);
    size_t readBytes = vendorEvent->readBytes(eventBytes, (IOByteCount)totalLength);
    
    XCTAssert((readBytes + sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    IOHIDEventRef parentEvent =  IOHIDEventCreateWithBytes(kCFAllocatorDefault, eventBytes, (CFIndex)totalLength);
    HIDXCTAssertAndThrowTrue (parentEvent != NULL);
    
    //verify user event
    CFArrayRef children =  IOHIDEventGetChildren(parentEvent);
    IOHIDEventRef childEvent = NULL;
    CFIndex value;
    
    XCTAssert(CFArrayGetCount(children) == 3);
    
    //verify parent event
    uint8_t *vendorData = IOHIDEventGetDataValue(parentEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payload, vendorData, sizeof(payload)) == 0);
    
    //verify left child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 0);
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldKeyboardUsagePage);
    XCTAssert (value == kHIDUsage_GD_Keyboard);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldKeyboardUsage);
    XCTAssert (value == kHIDUsage_KeyboardC);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldKeyboardDown);
    XCTAssert (value == 1);
    
    
    //verify middle child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 1);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollX);
    XCTAssert (value == 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollY);
    XCTAssert (value == 3);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldScrollZ);
    XCTAssert (value == 1);
    
    
    //verify right child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 2);
    
    value = IOHIDEventGetIntegerValue (childEvent, kIOHIDEventFieldAmbientLightSensorLevel);
    XCTAssert (value == 1);
    
    //reconstruct back kernel event
    IOHIDEvent *eventCpy = IOHIDEvent::withBytes(eventBytes, (IOByteCount)totalLength);
    
    HIDXCTAssertAndThrowTrue (eventCpy != NULL);
    
    OSArray *eventCpyChildren = eventCpy->getChildren();
    
    XCTAssert(eventCpyChildren != NULL);
    
    XCTAssert(eventCpyChildren->getCount() == 3);
    
    eventCpy->release();
    
    for (unsigned int index = 0; index < eventCpyChildren->getCount(); index++) {
        IOHIDEvent *eventCpyChild = (IOHIDEvent*)eventCpyChildren->getObject(index);
        XCTAssert(eventCpyChild != NULL);
        eventCpyChild->release();
    }
    
    free(eventBytes);
    vendorEvent->release();
    scrollEvent->release();
    ambientLightSensorEvent->release();
    keyboardEvent->release();
    CFRelease(parentEvent);
}
- (void)testEventTreeAllVendor {
    
    uint8_t payloadVendorParent [256] = {0xaa, 0x55, 0xaa, 0x55} ;
    uint8_t payloadVendorLeftChild [256] = {0x11, 0x55, 0x11, 0x55} ;
    uint8_t payloadVendorMiddleChild [256] = {0xbb, 0x45, 0xbb, 0x45} ;
    uint8_t payloadVendorRightChild [256] = {0xcc, 0x52, 0xcc, 0x52} ;
    
    
    //create vendor event parent
    IOHIDEvent *vendorEventParent = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payloadVendorParent, sizeof(payloadVendorParent));
    HIDXCTAssertAndThrowTrue (vendorEventParent != NULL);
    size_t vendorEventParentLength = (IOByteCount)vendorEventParent->getLength();
    
    //create vendor event left child
    IOHIDEvent *vendorEventLeftChild = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payloadVendorLeftChild, sizeof(payloadVendorLeftChild));
    HIDXCTAssertAndThrowTrue (vendorEventLeftChild != NULL);
    size_t vendorEventLeftChildLength = (IOByteCount)vendorEventLeftChild->getLength();
    
    //create vendor event middle child
    IOHIDEvent *vendorEventMiddleChild = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payloadVendorMiddleChild, sizeof(payloadVendorMiddleChild));
    HIDXCTAssertAndThrowTrue (vendorEventMiddleChild != NULL);
    size_t vendorEventMiddleChildLength = (IOByteCount)vendorEventMiddleChild->getLength();
    
    //create vendor event right child
    IOHIDEvent *vendorEventRightChild = IOHIDEvent::vendorDefinedEvent(mach_absolute_time(), kHIDUsage_GD_Keyboard, kHIDUsage_KeyboardC, 0, payloadVendorRightChild, sizeof(payloadVendorRightChild));
    HIDXCTAssertAndThrowTrue (vendorEventRightChild != NULL);
    size_t vendorEventRightChildLength = (IOByteCount)vendorEventRightChild->getLength();
    
    //construct tree
    vendorEventParent->appendChild(vendorEventLeftChild);
    vendorEventParent->appendChild(vendorEventMiddleChild);
    vendorEventParent->appendChild(vendorEventRightChild);
    
    //get total length
    size_t totalLength = vendorEventParent->getLength();
    
    
    XCTAssert((vendorEventParentLength + vendorEventLeftChildLength + vendorEventMiddleChildLength + vendorEventRightChildLength - 3*sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    
    uint8_t *eventBytes = (uint8_t*)malloc(totalLength);
    size_t readBytes = vendorEventParent->readBytes(eventBytes, (IOByteCount)totalLength);
    
    XCTAssert((readBytes + sizeof(base::IOHIDSystemQueueElement)) == totalLength);
    
    IOHIDEventRef parentEvent =  IOHIDEventCreateWithBytes(kCFAllocatorDefault, eventBytes, (CFIndex)totalLength);
    HIDXCTAssertAndThrowTrue (parentEvent != NULL);
    
    //verify user event
    CFArrayRef children =  IOHIDEventGetChildren(parentEvent);
    IOHIDEventRef childEvent = NULL;
    
    XCTAssert(CFArrayGetCount(children) == 3);
    
    //verify parent event
    uint8_t *vendorDataParent = IOHIDEventGetDataValue(parentEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payloadVendorParent, vendorDataParent, sizeof(payloadVendorParent)) == 0);
    
    //verify left child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 0);
    
    uint8_t *vendorDataLeftChild = IOHIDEventGetDataValue(childEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payloadVendorLeftChild, vendorDataLeftChild, sizeof(payloadVendorLeftChild)) == 0);
    
    //verify middle child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 1);
    
    uint8_t *vendorDataMiddleChild = IOHIDEventGetDataValue(childEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payloadVendorMiddleChild, vendorDataMiddleChild, sizeof(payloadVendorMiddleChild)) == 0);
    
    //verify right child
    childEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(children, 2);
    
    uint8_t *vendorDataRightChild = IOHIDEventGetDataValue(childEvent, kIOHIDEventFieldVendorDefinedData);
    
    XCTAssert(memcmp(&payloadVendorRightChild, vendorDataRightChild, sizeof(payloadVendorRightChild)) == 0);
    
    //reconstruct back kernel event
    IOHIDEvent *eventCpy = IOHIDEvent::withBytes(eventBytes, (IOByteCount)totalLength);
    
    HIDXCTAssertAndThrowTrue (eventCpy != NULL);
    
    OSArray *eventCpyChildren = eventCpy->getChildren();
    
    XCTAssert(eventCpyChildren != NULL);
    
    XCTAssert(eventCpyChildren->getCount() == 3);
    
    eventCpy->release();
    
    for (unsigned int index = 0; index < eventCpyChildren->getCount(); index++) {
        IOHIDEvent *eventCpyChild = (IOHIDEvent*)eventCpyChildren->getObject(index);
        XCTAssert(eventCpyChild != NULL);
        eventCpyChild->release();
    }
    
    free(eventBytes);
    vendorEventParent->release();
    vendorEventLeftChild->release();
    vendorEventMiddleChild->release();
    vendorEventRightChild->release();
    CFRelease(parentEvent);
    
}
@end
