//
//  AppleUserHIDEventDriver.cpp
//  AppleUserHIDDrivers
//
//  Created by dekom on 1/14/19.
//

#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>
#include "AppleUserHIDEventDriver.h"

#undef  super
#define super IOUserHIDEventDriver

#define kIOHIDAppleVendorID 1452
#define kDFRV2ProductID 33538
#define kDFRV1ProductID 34304

// Bits associated with AppleUserHIDEventDriver-debug boot-arg
enum {
    kDebugDisableDriver = 1 << 0
};

struct AppleUserHIDEventDriver_IVars
{
    OSDictionaryPtr properties;
    
    struct {
        bool appleVendorSupported;
        OSArray *elements;
    } keyboard;
    
    struct {
        OSArray *elements;
        OSArray *pendingEvents;
    } vendor;
};

#define _properties     ivars->properties
#define _keyboard       ivars->keyboard
#define _vendor         ivars->vendor

bool AppleUserHIDEventDriver::init()
{
    bool result = false;
    
    result = super::init();
    require_action(result, exit, HIDLogError("Init:%x", result));
    
    ivars = IONewZero(AppleUserHIDEventDriver_IVars, 1);
    require(ivars, exit);
    
exit:
    return result;
}

void AppleUserHIDEventDriver::free()
{
    if (ivars) {
        OSSafeReleaseNULL(_properties);
        OSSafeReleaseNULL(_keyboard.elements);
        OSSafeReleaseNULL(_vendor.elements);
        OSSafeReleaseNULL(_vendor.pendingEvents);
    }
    
    IOSafeDeleteNULL(ivars, AppleUserHIDEventDriver_IVars, 1);
    super::free();
}

void AppleUserHIDEventDriver::printDescription()
{
    OSNumber *up, *u, *vid, *pid;
    OSString *product = NULL;
    
    require(_properties, exit);
    
    product = OSDynamicCast(OSString, OSDictionaryGetValue(_properties, kIOHIDProductKey));
    up = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDPrimaryUsagePageKey));
    u = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDPrimaryUsageKey));
    vid = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDVendorIDKey));
    pid = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDProductIDKey));
    
    HIDServiceLog("%{public}s usagePage: %d usage: %d vid: %d pid: %d",
                  product ? product->getCStringNoCopy() : "",
                  up ? up->unsigned32BitValue() : 0,
                  u ? u->unsigned32BitValue() : 0,
                  vid ? vid->unsigned32BitValue() : 0,
                  pid ? pid->unsigned32BitValue() : 0);
    
exit:
    return;
}

bool AppleUserHIDEventDriver::deviceSupported()
{
    bool result = false;
    __block bool supported = true;
    OSArray *pairs = NULL;
    OSNumber *vid = NULL;
    OSNumber *pid = NULL;
    
    require(_properties, exit);
    
    vid = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDVendorIDKey));
    pid = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDProductIDKey));
    
    if (vid && pid) {
        // DFR
        if (vid->unsigned32BitValue() == kIOHIDAppleVendorID &&
            (pid->unsigned32BitValue() == kDFRV2ProductID ||
             pid->unsigned32BitValue() == kDFRV1ProductID)) {
                result = true;
                goto exit;
            }
        
        // Internal headset
        if (vid->unsigned32BitValue() == kIOHIDAppleVendorID) {
            OSNumber *up = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDPrimaryUsagePageKey));
            OSNumber *u = OSDynamicCast(OSNumber, OSDictionaryGetValue(_properties, kIOHIDPrimaryUsageKey));
            
            require(up && u, exit);
            
            // only support headset for Apple vendorID
            require(up->unsigned32BitValue() == kHIDPage_Consumer &&
                    u->unsigned32BitValue() == kHIDUsage_Csmr_ConsumerControl, exit);
        }
    }
    
    pairs = OSDynamicCast(OSArray, OSDictionaryGetValue(_properties, kIOHIDDeviceUsagePairsKey));
    require(pairs, exit);
    
    pairs->iterateObjects(^bool(OSObject *object) {
        OSDictionary *entry = OSDynamicCast(OSDictionary, object);
        OSNumber *up = NULL;
        OSNumber *u = NULL;
        uint32_t usagePage = 0;
        uint32_t usage = 0;
        
        if (!entry) {
            return false;
        }
        
        up = OSDynamicCast(OSNumber, OSDictionaryGetValue(entry, kIOHIDDeviceUsagePageKey));
        u = OSDynamicCast(OSNumber, OSDictionaryGetValue(entry, kIOHIDDeviceUsageKey));

        if (!up || !u) {
            return false;
        }
        
        usagePage = up->unsigned32BitValue();
        usage = u->unsigned32BitValue();
        
        // no support for GC yet
        if (usagePage == kHIDPage_GenericDesktop &&
            (usage == kHIDUsage_GD_GamePad ||
             usage == kHIDUsage_GD_Joystick ||
             usage == kHIDUsage_GD_MultiAxisController)) {
            supported = false;
            return true;
        }
        
        return false;
    });
    
    result = true;
    
exit:
    return (result && supported);
}

bool AppleUserHIDEventDriver::parseElements(OSArray *elements)
{
    bool result = false;
    
    result = super::parseElements(elements);
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElement *element = (IOHIDElement *)elements->getObject(i);
        
        if (element->getType() == kIOHIDElementTypeCollection ||
            !element->getUsage()) {
            continue;
        }
        
        if (parseVendorElement(element)) {
            result = true;
        }
    }
    
    HIDServiceLog("parseElements: vendor: %d",
                  _vendor.elements ? _vendor.elements-> getCount() : 0);
    
    return result;
}

bool AppleUserHIDEventDriver::parseKeyboardElement(IOHIDElement *element)
{
    bool result = false;
    uint32_t usagePage = element->getUsagePage();
    uint32_t usage = element->getUsage();
    
    result = super::parseKeyboardElement(element);
    require_quiet(!result, exit);
    
    switch (usagePage) {
        case kHIDPage_AppleVendorTopCase:
            if (_keyboard.appleVendorSupported) {
                switch (usage) {
                    case kHIDUsage_AV_TopCase_BrightnessDown:
                    case kHIDUsage_AV_TopCase_BrightnessUp:
                    case kHIDUsage_AV_TopCase_IlluminationDown:
                    case kHIDUsage_AV_TopCase_IlluminationUp:
                    case kHIDUsage_AV_TopCase_KeyboardFn:
                        result = true;
                        break;
                }
            }
            break;
        case kHIDPage_AppleVendorKeyboard:
            if (_keyboard.appleVendorSupported) {
                switch (usage) {
                    case kHIDUsage_AppleVendorKeyboard_Spotlight:
                    case kHIDUsage_AppleVendorKeyboard_Dashboard:
                    case kHIDUsage_AppleVendorKeyboard_Function:
                    case kHIDUsage_AppleVendorKeyboard_Launchpad:
                    case kHIDUsage_AppleVendorKeyboard_Reserved:
                    case kHIDUsage_AppleVendorKeyboard_CapsLockDelayEnable:
                    case kHIDUsage_AppleVendorKeyboard_PowerState:
                    case kHIDUsage_AppleVendorKeyboard_Expose_All:
                    case kHIDUsage_AppleVendorKeyboard_Expose_Desktop:
                    case kHIDUsage_AppleVendorKeyboard_Brightness_Up:
                    case kHIDUsage_AppleVendorKeyboard_Brightness_Down:
                    case kHIDUsage_AppleVendorKeyboard_Language:
                        result = true;
                        break;
                }
            }
            break;
        default:
            break;
    }
    
    require_quiet(result, exit);
    
    if (!_keyboard.elements) {
        _keyboard.elements = OSArray::withCapacity(1);
        require(_keyboard.elements, exit);
    }
    
    _keyboard.elements->setObject(element);
    
exit:
    return result;
}

bool AppleUserHIDEventDriver::parseVendorElement(IOHIDElement *element)
{
    IOHIDElement *parent = NULL;
    bool result = false;
    IOHIDElementCollectionType type;
    
    parent = element->getParentElement();
    require_quiet(parent, exit);
    
    type = parent->getCollectionType();
    
    require_quiet(type == kIOHIDElementCollectionTypeApplication ||
                  type == kIOHIDElementCollectionTypePhysical, exit);
    
    require_quiet(parent->getUsagePage() == kHIDPage_AppleVendor &&
                  parent->getUsage() == kHIDUsage_AppleVendor_Message, exit);
    
    if (!_vendor.elements) {
        _vendor.elements = OSArray::withCapacity(1);
    }
    require(_vendor.elements, exit);
    
    _vendor.elements->setObject(element);
    
    result = true;
    
exit:
    return result;
}

void AppleUserHIDEventDriver::handleReport(uint64_t timestamp,
                                           uint8_t *report,
                                           uint32_t reportLength,
                                           IOHIDReportType type,
                                           uint32_t reportID)
{
    handleVendorReport(timestamp, reportID);
    super::handleReport(timestamp, report, reportLength, type, reportID);
}

void AppleUserHIDEventDriver::handleKeyboardReport(uint64_t timestamp,
                                                   uint32_t reportID)
{
    super::handleKeyboardReport(timestamp, reportID);
    
    require_quiet(_keyboard.elements, exit);
    
    for (unsigned int i = 0; i < _keyboard.elements->getCount(); i++) {
        IOHIDElementPrivate *element = NULL;
        uint64_t elementTimeStamp;
        uint32_t usagePage, usage, value, preValue;
        
        element = (IOHIDElementPrivate *)_keyboard.elements->getObject(i);
        
        if (element->getReportID() != reportID) {
            continue;
        }
        
        elementTimeStamp = element->getTimeStamp();
        if (timestamp != elementTimeStamp) {
            continue;
        }
        
        preValue = element->getValue(kIOHIDValueOptionsFlagPrevious) != 0;
        value = element->getValue() != 0;
        
        if (value == preValue) {
            continue;
        }
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        
        dispatchKeyboardEvent(timestamp, usagePage, usage, value, 0, true);
    }
    
exit:
    return;
}

void AppleUserHIDEventDriver::handleVendorReport(uint64_t timestamp,
                                                 uint32_t reportID)
{
    require_quiet(_vendor.elements,  exit);
    
    if (!_vendor.pendingEvents) {
        _vendor.pendingEvents = OSArray::withCapacity(1);
    }
    require(_vendor.pendingEvents, exit);
    
    for (unsigned int i = 0; i < _vendor.elements->getCount(); i++) {
        IOHIDElementPrivate *element = NULL;
        uint64_t elementTimeStamp;
        OSData *value = NULL;
        uint32_t usagePage, usage;
        IOHIDEvent *event = NULL;
        
        element = OSDynamicCast(IOHIDElementPrivate, _vendor.elements->getObject(i));
        if (!element) {
            continue;
        }
        
        if (element->getReportID() != reportID) {
            continue;
        }
        
        elementTimeStamp = element->getTimeStamp();
        if (timestamp != elementTimeStamp) {
            continue;
        }
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        
        value = element->getDataValue();
        if (!value || !value->getLength()) {
            continue;
        }
        
        event = IOHIDEvent::vendorDefinedEvent(timestamp,
                                               usagePage,
                                               usage,
                                               0,
                                               (uint8_t *)value->getBytesNoCopy(),
                                               value->getLength(),
                                               0);
        if (!event) {
            continue;
        }
        
        _vendor.pendingEvents->setObject(event);
        event->release();
    }
    
exit:
    return;
}

void AppleUserHIDEventDriver::dispatchEvent(IOHIDEvent *event)
{
    require_quiet(_vendor.pendingEvents && _vendor.pendingEvents->getCount(), exit);
    
    for (unsigned int i = 0; i < _vendor.pendingEvents->getCount(); i++) {
        IOHIDEvent *child = OSDynamicCast(IOHIDEvent, _vendor.pendingEvents->getObject(i));
        
        if (child) {
            event->appendChild(child);
        }
    }
    
    _vendor.pendingEvents->flushCollection();
    
exit:
    super::dispatchEvent(event);
}

kern_return_t
IMPL(AppleUserHIDEventDriver, Start)
{
    kern_return_t ret = kIOReturnUnsupported;
    uint32_t debugMask = 0;
    
    IOParseBootArgNumber("AppleUserHIDEventDriver-debug", &debugMask, sizeof(debugMask));
    if (debugMask & kDebugDisableDriver) {
        return kIOReturnUnsupported;
    }
    
    // 48347141: need a way to iterate service plane
    _keyboard.appleVendorSupported = true;
    //getProperty(kIOHIDAppleVendorSupported, gIOServicePlane);
    
    provider->CopyProperties(&_properties);
    if (!_properties) {
        return kIOReturnError;
    }
    
    if (!deviceSupported()) {
        return kIOReturnUnsupported;
    }
    
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action(ret, exit, HIDServiceLogError("Start: 0x%x", ret));
    
    ret = RegisterService();
    require_noerr_action(ret, exit, HIDServiceLogError("RegisterService: 0x%x", ret));
    
exit:
    if (ret != kIOReturnSuccess) {
        HIDServiceLogFault("Start failed: 0x%x", ret);
        Stop(provider);
    }
    
    HIDServiceLog("Start ret: 0x%x", ret);
    printDescription();
    return ret;
}

kern_return_t
IMPL(AppleUserHIDEventDriver, Stop)
{
    kern_return_t   ret;
    
    ret = Stop(provider, SUPERDISPATCH);
    HIDServiceLog("Stop: 0x%x", ret);
    
    return ret;
}
