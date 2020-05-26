//
//  IOUserHIDEventDriver.cpp
//  HIDDriverKit
//
//  Created by dekom on 1/10/19.
//

#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

#define DEFAULT_TIP_THRESHOLD 75

struct IOUserHIDEventDriver_IVars
{
    OSArray *elements;
    
    struct {
        OSArray *elements;
    } keyboard;
    
    struct {
        OSArray *relative;
        OSArray *absolute;
    } pointer;
    
    struct {
        OSArray *elements;
    } scroll;
    
    struct {
        OSArray *elements;
    } led;
    
    struct {
        OSArray *collections;
    } digitizer;
};

#define _elements       ivars->elements
#define _keyboard       ivars->keyboard
#define _pointer        ivars->pointer
#define _scroll         ivars->scroll
#define _led            ivars->led
#define _digitizer      ivars->digitizer

#undef super
#define super IOUserHIDEventService

bool IOUserHIDEventDriver::init()
{
    bool result = false;
    
    result = super::init();
    require_action(result, exit, HIDLogError("Init:%x", result));
    
    assert(IOService::ivars);
    
    ivars = IONewZero(IOUserHIDEventDriver_IVars, 1);
    require(ivars, exit);
    
exit:
    return result;
}

void IOUserHIDEventDriver::free()
{
    if (ivars) {
        OSSafeReleaseNULL(_elements);
        OSSafeReleaseNULL(_keyboard.elements);
        OSSafeReleaseNULL(_pointer.relative);
        OSSafeReleaseNULL(_pointer.absolute);
        OSSafeReleaseNULL(_scroll.elements);
        OSSafeReleaseNULL(_led.elements);
    }
    
    IOSafeDeleteNULL(ivars, IOUserHIDEventDriver_IVars, 1);
    super::free();
}

kern_return_t
IMPL(IOUserHIDEventDriver, Start)
{
    kern_return_t ret = kIOReturnError;
    bool result = false;
    
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action(ret, exit, HIDServiceLogError("Start failed: 0x%x", ret));
    
    _elements = getElements();
    require_action(_elements, exit, HIDServiceLogError("Failed to get elements"));
    
    _elements->retain();
    
    require_action(parseElements(_elements), exit, HIDServiceLogError("No supported elements found"));
    
    result = true;
    
exit:
    if (!result) {
        HIDServiceLogError("Start failed: 0x%x", ret);
        ret = kIOReturnError;
    }
    
    return ret;
}

bool IOUserHIDEventDriver::parseElements(OSArray *elements)
{
    bool result = false;
    OSArray *buttonElements = NULL;
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElement *element = (IOHIDElement *)elements->getObject(i);
        
        if (element->getType() == kIOHIDElementTypeCollection ||
            !element->getUsage()) {
            continue;
        }
        
        if (parseKeyboardElement(element) ||
            parseDigitizerElement(element) ||
            parsePointerElement(element) ||
            parseScrollElement(element) ||
            parseLEDElement(element)) {
            result = true;
            continue;
        }
        
        if (element->getUsagePage() == kHIDPage_Button) {
            if (!buttonElements) {
                buttonElements = OSArray::withCapacity(4);
                require_action(buttonElements, exit, result = false);
            }
            buttonElements->setObject(element);
        }
    }
    
    if (buttonElements) {
        buttonElements->iterateObjects(^bool(OSObject *object) {
            if (_pointer.relative) {
                _pointer.relative->setObject(object);
            } else if (_pointer.absolute) {
                _pointer.absolute->setObject(object);
            } else {
                if (!_pointer.relative) {
                    _pointer.relative = OSArray::withCapacity(4);
                }
                if (_pointer.relative) {
                    _pointer.relative->setObject(object);
                }
            }
            
            return false;
        });

    }
    
    HIDServiceLog("parseElements: keyboard: %d digitizer: %d pointer: %d %d scroll: %d led: %d",
                  _keyboard.elements ? _keyboard.elements->getCount() : 0,
                  _digitizer.collections ? _digitizer.collections->getCount() : 0,
                  _pointer.relative ? _pointer.relative->getCount() : 0,
                  _pointer.absolute ? _pointer.absolute->getCount() : 0,
                  _scroll.elements ? _scroll.elements->getCount() : 0,
                  _led.elements ? _led.elements->getCount() : 0);
    
    setAccelerationProperties();
    setSurfaceDimensions();
    
exit:
    OSSafeReleaseNULL(buttonElements);
    return result;
}

void IOUserHIDEventDriver::setAccelerationProperties()
{
    OSDictionaryPtr properties = OSDictionaryCreate();
    
    require(properties, exit);
    
    if (conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse)) {
        OSDictionarySetStringValue(properties,
                                   kIOHIDPointerAccelerationTypeKey,
                                   kIOHIDMouseAccelerationTypeKey);
        
        if (_scroll.elements) {
            OSDictionarySetStringValue(properties,
                                       kIOHIDScrollAccelerationTypeKey,
                                       kIOHIDMouseScrollAccelerationKey);
        }
    } else if (conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Pointer)) {
        OSDictionarySetStringValue(properties,
                                   kIOHIDPointerAccelerationTypeKey,
                                   kIOHIDPointerAccelerationKey);
        
        if (_scroll.elements) {
            OSDictionarySetStringValue(properties,
                                       kIOHIDScrollAccelerationTypeKey,
                                       kIOHIDScrollAccelerationKey);
        }
    }
    
    if (OSDictionaryGetCount(properties)) {
        //OSObjectLog(properties);
        kern_return_t ret = SetProperties(properties);
        if (ret != kIOReturnSuccess) {
            HIDServiceLogError("Failed to set acceleration properties: 0x%x", ret);
        }
    }
    
exit:
    OSSafeReleaseNULL(properties);
}

void IOUserHIDEventDriver::setSurfaceDimensions()
{
    OSDictionaryPtr dimensions = NULL;
    bool (^elementInterator)(OSObject *object);
    
    require_quiet(_pointer.absolute || _digitizer.collections, exit);
    
    dimensions = OSDictionaryCreate();
    require(dimensions, exit);
    
    elementInterator = ^bool(OSObject *object) {
        IOHIDElement *element = OSDynamicCast(IOHIDElement, object);
        uint32_t usagePage, usage, pDelta;
        
        if (!element) {
            return false;
        }
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        pDelta = element->getPhysicalMax() - element->getPhysicalMin();
        
        if (usagePage != kHIDPage_GenericDesktop) {
            return false;
        }
        
        if (usage != kHIDUsage_GD_X && usage != kHIDUsage_GD_Y) {
            return false;
        }
        
        if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) != 0) {
            return false;
        }
        
        if (usage == kHIDUsage_GD_X) {
            OSDictionarySetUInt64Value(dimensions, kIOHIDWidthKey, pDelta);
        } else if (usage == kHIDUsage_GD_Y) {
            OSDictionarySetUInt64Value(dimensions, kIOHIDHeightKey, pDelta);
        }
        
        return false;
    };
    
    if (_pointer.absolute) {
        _pointer.absolute->iterateObjects(elementInterator);
    }
    
    require_quiet(!OSDictionaryGetCount(dimensions) && _digitizer.collections, exit);
    
    _digitizer.collections->iterateObjects(^bool(OSObject *object) {
        IOHIDDigitizerCollection *collection = OSDynamicCast(IOHIDDigitizerCollection, object);
        OSArray *elements = NULL;
        
        if (!collection) {
            return false;
        }
        
        elements = collection->getElements();
        if (!elements) {
            return false;
        }
        
        elements->iterateObjects(elementInterator);
        
        if (OSDictionaryGetCount(dimensions)) {
            return true;
        } else {
            return false;
        }
    });
    
exit:
    if (dimensions && OSDictionaryGetCount(dimensions)) {
        OSDictionaryPtr properties = OSDictionaryCreate();
        
        OSDictionarySetValue(properties, kIOHIDSurfaceDimensionsKey, dimensions);
        
        kern_return_t ret = SetProperties(properties);
        if (ret != kIOReturnSuccess) {
            HIDServiceLogError("Failed to set surface dimensions: 0x%x", ret);
        }
        
        properties->release();
    }
    
    OSSafeReleaseNULL(dimensions);
}

bool IOUserHIDEventDriver::parseKeyboardElement(IOHIDElement *element)
{
    bool result = false;
    uint32_t usagePage = element->getUsagePage();
    uint32_t usage = element->getUsage();
    
    switch (usagePage) {
        case kHIDPage_GenericDesktop:
            switch (usage) {
                case kHIDUsage_GD_Start:
                case kHIDUsage_GD_Select:
                case kHIDUsage_GD_SystemPowerDown:
                case kHIDUsage_GD_SystemSleep:
                case kHIDUsage_GD_SystemWakeUp:
                case kHIDUsage_GD_SystemContextMenu:
                case kHIDUsage_GD_SystemMainMenu:
                case kHIDUsage_GD_SystemAppMenu:
                case kHIDUsage_GD_SystemMenuHelp:
                case kHIDUsage_GD_SystemMenuExit:
                case kHIDUsage_GD_SystemMenuSelect:
                case kHIDUsage_GD_SystemMenuRight:
                case kHIDUsage_GD_SystemMenuLeft:
                case kHIDUsage_GD_SystemMenuUp:
                case kHIDUsage_GD_SystemMenuDown:
                case kHIDUsage_GD_DPadUp:
                case kHIDUsage_GD_DPadDown:
                case kHIDUsage_GD_DPadRight:
                case kHIDUsage_GD_DPadLeft:
                    result = true;
                    break;
            }
            break;
        case kHIDPage_KeyboardOrKeypad:
            if (usage < kHIDUsage_KeyboardA ||
                usage > kHIDUsage_KeyboardRightGUI) {
                break;
            }
        case kHIDPage_Consumer:
            // kHIDUsage_Csmr_ACPan is handled in parseScrollElement
            if (usage != kHIDUsage_Csmr_ACPan) {
                result = true;
            }
            break;
        case kHIDPage_Telephony:
        case kHIDPage_CameraControl:
            result = true;
            break;
        default:
            break;
    }
    
    require_quiet(result, exit);
    
    if (!_keyboard.elements) {
        _keyboard.elements = OSArray::withCapacity(4);
        require(_keyboard.elements, exit);
    }
    
    _keyboard.elements->setObject(element);
    
exit:
    return result;
}

bool IOUserHIDEventDriver::parsePointerElement(IOHIDElement *element)
{
    bool result = false;
    uint32_t usagePage = element->getUsagePage();
    uint32_t usage = element->getUsage();
    bool absolute = false;
    
    switch (usagePage) {
        case kHIDPage_GenericDesktop:
            switch (usage) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                    if (!(element->getFlags() & kIOHIDElementFlagsRelativeMask)) {
                        IOHIDElementPrivate *priv = (IOHIDElementPrivate *)element;
                        priv->setCalibration(0, 1, element->getLogicalMin(), element->getLogicalMax(), 0, 0, 0);
                        absolute = true;
                    }
                    result = true;
                    break;
            }
            break;
        default:
            break;
    }
    
    require_quiet(result, exit);
    
    if (absolute) {
        if (!_pointer.absolute) {
            _pointer.absolute = OSArray::withCapacity(4);
            require(_pointer.absolute, exit);
        }
        
        _pointer.absolute->setObject(element);
    } else {
        if (!_pointer.relative) {
            _pointer.relative = OSArray::withCapacity(4);
            require(_pointer.relative, exit);
        }
        
        _pointer.relative->setObject(element);
    }
    
exit:
    return result;
}

bool IOUserHIDEventDriver::parseScrollElement(IOHIDElement *element)
{
    bool result = false;
    uint32_t usagePage = element->getUsagePage();
    uint32_t usage = element->getUsage();
    
    switch (usagePage) {
        case kHIDPage_GenericDesktop:
            switch (usage) {
                case kHIDUsage_GD_Dial:
                case kHIDUsage_GD_Wheel:
                case kHIDUsage_GD_Z:
                    //if ((element->getFlags() & (kIOHIDElementFlagsNoPreferredMask|kIOHIDElementFlagsRelativeMask)) == 0) {
                    //    calibrateCenteredPreferredStateElement(element, _preferredAxisRemovalPercentage);
                    //}
                    
                    result = true;
                    break;
            }
            break;
        case kHIDPage_Consumer:
            switch (usage) {
                case kHIDUsage_Csmr_ACPan:
                    result = true;
                    break;
            }
            break;
    }
    
    require_quiet(result, exit);
    
    if (!_scroll.elements) {
        _scroll.elements = OSArray::withCapacity(4);
        require(_scroll.elements, exit);
    }
    
    _scroll.elements->setObject(element);
    
exit:
    return result;
}

bool IOUserHIDEventDriver::parseLEDElement(IOHIDElement *element)
{
    uint32_t usagePage = element->getUsagePage();
    bool result = false;
    
    switch (usagePage) {
        case kHIDPage_LEDs:
            result = true;
            break;
    }
    
    require(result, exit);
    
    if (!_led.elements) {
        _led.elements = OSArray::withCapacity(4);
        require(_led.elements, exit);
    }
    
    _led.elements->setObject(element);
    
exit:
    return result;
}

void IOUserHIDEventDriver::setTipThreshold()
{
    OSDictionaryPtr props = NULL;
    OSDictionaryPtr newProps = NULL;
    kern_return_t ret;
    
    CopyProperties(&props);
    
    if (props) {
        require(!OSDictionaryGetValue(props, kIOHIDDigitizerTipThresholdKey), exit);
    }
    
    newProps = OSDictionaryCreate();
    require(newProps, exit);
    
    OSDictionarySetUInt64Value(newProps,
                               kIOHIDDigitizerTipThresholdKey,
                               DEFAULT_TIP_THRESHOLD);
    
    ret = SetProperties(newProps);
    if (ret != kIOReturnSuccess) {
        HIDServiceLogError("Failed to set tip threshold: 0x%x", ret);
    }
    
exit:
    OSSafeReleaseNULL(props);
    OSSafeReleaseNULL(newProps);
}

bool IOUserHIDEventDriver::parseDigitizerElement(IOHIDElement *element)
{
    bool result = false;
    IOHIDElement *parent = element;
    IOHIDDigitizerCollection *collection = NULL;
    
    require(element->getType() <= kIOHIDElementTypeInput_ScanCodes, exit);
    
    // find the top level collection element
    while ((parent = parent->getParentElement())) {
        IOHIDElementCollectionType collectionType = parent->getCollectionType();
        uint32_t usagePage = parent->getUsagePage();
        uint32_t usage = parent->getUsage();
        
        if (usagePage != kHIDPage_Digitizer) {
            continue;
        }
        
        if (collectionType == kIOHIDElementCollectionTypeLogical ||
            collectionType == kIOHIDElementCollectionTypePhysical) {
            if (usage >= kHIDUsage_Dig_Stylus &&
                usage <= kHIDUsage_Dig_GestureCharacter) {
                break;
            }
        } else if (collectionType == kIOHIDElementCollectionTypeApplication) {
            if (usage >= kHIDUsage_Dig_Digitizer &&
                usage <= kHIDUsage_Dig_DeviceConfiguration) {
                break;
            }
        }
    }
    
    require(parent, exit);
    
    switch (element->getUsagePage()) {
        case kHIDPage_GenericDesktop:
            switch (element->getUsage()) {
                case kHIDUsage_GD_X:
                case kHIDUsage_GD_Y:
                case kHIDUsage_GD_Z:
                    require_quiet((element->getFlags() & kIOHIDElementFlagsRelativeMask) == 0, exit);
                    
                    // todo: figure out a better way to "calibrate"
                    IOHIDElementPrivate *priv = (IOHIDElementPrivate *)element;
                    priv->setCalibration(0, 1, element->getLogicalMin(), element->getLogicalMax(), 0, 0, 0);
                    break;
            }
            break;
        case kHIDPage_Digitizer:
            switch (element->getUsage()) {
                case kHIDUsage_Dig_TipPressure:
                    setTipThreshold();
                case kHIDUsage_Dig_BarrelPressure:
                    // calibrate between 0 and 1
                    IOHIDElementPrivate *priv = (IOHIDElementPrivate *)element;
                    priv->setCalibration(0, 1, element->getLogicalMin(), element->getLogicalMax(), 0, 0, 0);
                    break;
            }
            break;
    }
    
    if (!_digitizer.collections) {
        _digitizer.collections = OSArray::withCapacity(4);
        require(_digitizer.collections, exit);
    }
    
    // see if a collection already exists for this parent
    for (unsigned int i = 0; i < _digitizer.collections->getCount(); i++) {
        IOHIDDigitizerCollection *tmp = OSDynamicCast(IOHIDDigitizerCollection,
                                                      _digitizer.collections->getObject(i));
        
        if (!tmp) {
            continue;
        }
        
        if (tmp->getParentCollection() == parent) {
            collection = tmp;
            break;
        }
    }
    
    if (!collection) {
        IOHIDDigitizerCollectionType type = kIOHIDDigitizerCollectionTypeStylus;
        
        switch (parent->getUsage()) {
            case kHIDUsage_Dig_Puck:
                type = kIOHIDDigitizerCollectionTypePuck;
                break;
            case kHIDUsage_Dig_Finger:
            case kHIDUsage_Dig_TouchScreen:
            case kHIDUsage_Dig_TouchPad:
                type = kIOHIDDigitizerCollectionTypeFinger;
                break;
            default:
                break;
        }
        
        collection = IOHIDDigitizerCollection::withType(type, parent);
        require(collection, exit);
        
        _digitizer.collections->setObject(collection);
        collection->release();
    }
    
    collection->addElement(element);
    result = true;
    
exit:
    return result;
}

void IOUserHIDEventDriver::handleReport(uint64_t              timestamp,
                                        uint8_t               *report __unused,
                                        uint32_t              reportLength __unused,
                                        IOHIDReportType       type,
                                        uint32_t              reportID)
{
    handleKeyboardReport(timestamp, reportID);
    handleRelativePointerReport(timestamp, reportID);
    handleAbsolutePointerReport(timestamp, reportID);
    handleScrollReport(timestamp, reportID);
    handleDigitizerReport(timestamp, reportID);
}

void IOUserHIDEventDriver::handleKeyboardReport(uint64_t timestamp,
                                                uint32_t reportID)
{
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

static inline void setButtonState(uint32_t *state, uint32_t bit, uint32_t value)
{
    uint32_t buttonMask = (1 << bit);
    
    if (value != 0) {
        (*state) |= buttonMask;
    } else {
        (*state) &= ~buttonMask;
    }
}

void IOUserHIDEventDriver::handleRelativePointerReport(uint64_t timestamp,
                                                       uint32_t reportID)
{
    bool handled = false;
    IOFixed x = 0;
    IOFixed y = 0;
    uint32_t buttonState = 0;
    
    require_quiet(_pointer.relative, exit);
    
    for (unsigned int i = 0; i < _pointer.relative->getCount(); i++) {
        IOHIDElementPrivate *element;
        uint64_t elementTimeStamp;
        uint32_t usagePage, usage;
        bool elementIsCurrent;
        
        element = (IOHIDElementPrivate *)_pointer.relative->getObject(i);
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (timestamp == elementTimeStamp);
        
        handled |= elementIsCurrent;
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        
        switch (usagePage) {
            case kHIDPage_GenericDesktop:
                switch (usage) {
                    case kHIDUsage_GD_X:
                        x = elementIsCurrent ? element->getValue() << 16 : 0;
                        break;
                    case kHIDUsage_GD_Y:
                        y = elementIsCurrent ? element->getValue() << 16 : 0;
                        break;
                }
                break;
            case kHIDPage_Button:
                setButtonState(&buttonState, (usage - 1), element->getValue());
                break;
        }
    }
    
    require_quiet(handled, exit);
    
    //HIDServiceLog("DispatchRelativePointerEvent x: %d y: %d button: 0x%x", x, y, buttonState);
    
    dispatchRelativePointerEvent(timestamp, x, y, buttonState, 0, true);
    
exit:
    return;
}

void IOUserHIDEventDriver::handleAbsolutePointerReport(uint64_t timestamp,
                                                       uint32_t reportID)
{
    bool handled = false;
    IOFixed x = 0;
    IOFixed y = 0;
    uint32_t buttonState = 0;
    
    require_quiet(_pointer.absolute, exit);
    
    for (unsigned int i = 0; i < _pointer.absolute->getCount(); i++) {
        IOHIDElementPrivate *element;
        uint64_t elementTimeStamp;
        uint32_t usagePage, usage;
        bool elementIsCurrent;
        
        element = (IOHIDElementPrivate *)_pointer.absolute->getObject(i);
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (timestamp == elementTimeStamp);
        
        handled |= elementIsCurrent;
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        
        switch (usagePage) {
            case kHIDPage_GenericDesktop:
                switch (usage) {
                    case kHIDUsage_GD_X:
                        x = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        break;
                    case kHIDUsage_GD_Y:
                        y = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        break;
                }
                break;
            case kHIDPage_Button:
                setButtonState(&buttonState, (usage - 1), element->getValue());
                break;
        }
    }
    
    require_quiet(handled, exit);
    
    //HIDServiceLog("DispatchAbsolutePointerEvent x: %d y: %d button: 0x%x", x, y, buttonState);

    dispatchAbsolutePointerEvent(timestamp, x, y, buttonState, 0, true);
    
exit:
    return;
}

void IOUserHIDEventDriver::handleScrollReport(uint64_t timestamp,
                                              uint32_t reportID)
{
    IOFixed scrollVert = 0;
    IOFixed scrollHoriz = 0;
    
    require_quiet(_scroll.elements, exit);
    
    for (unsigned int i = 0; i < _scroll.elements->getCount(); i++) {
        IOHIDElementPrivate *element;
        uint64_t elementTimeStamp;
        uint32_t usagePage, usage;
        
        element = (IOHIDElementPrivate *)_scroll.elements->getObject(i);
        
        if (element->getReportID() != reportID) {
            continue;
        }
        
        elementTimeStamp = element->getTimeStamp();
        if (timestamp != elementTimeStamp) {
            continue;
        }
        
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        
        switch (usagePage) {
            case kHIDPage_GenericDesktop:
                switch (usage) {
                    case kHIDUsage_GD_Wheel:
                    case kHIDUsage_GD_Dial:
                        if (element->getFlags() & kIOHIDElementFlagsWrapMask) {
                            scrollVert = element->getValue(kIOHIDValueOptionsFlagRelativeSimple);
                        } else {
                            scrollVert = element->getValue();
                        }
                        break;
                    case kHIDUsage_GD_Z:
                        if (element->getFlags() & kIOHIDElementFlagsWrapMask) {
                            scrollHoriz = element->getValue(kIOHIDValueOptionsFlagRelativeSimple);
                        } else {
                            scrollHoriz = element->getValue();
                        }
                        break;
                    default:
                        break;
                }
                break;
            case kHIDPage_Consumer:
                switch (usage) {
                    case kHIDUsage_Csmr_ACPan:
                        scrollHoriz = (-element->getValue());
                        break;
                    default:
                        break;
                }
                break;
            default:
                break;
        }
    }
    
    require_quiet(scrollVert || scrollHoriz, exit);
    
    //HIDLog("DispatchScrollWheelEventWithFixed vert: %d horiz: %d", scrollVert, scrollHoriz);
    dispatchRelativeScrollWheelEvent(timestamp, scrollVert << 16, scrollHoriz << 16, 0, 0, true);
    
exit:
    return;
}

void IOUserHIDEventDriver::handleDigitizerReport(uint64_t timestamp,
                                                 uint32_t reportID)
{
    IOHIDEvent *collectionEvent = NULL;
    bool touch = false;
    bool range = false;
    uint32_t fingerCount = 0;
    uint32_t eventMask = 0;
    uint32_t buttonMask = 0;
    IOFixed touchX = 0;
    IOFixed touchY = 0;
    IOFixed inRangeX = 0;
    IOFixed inRangeY = 0;
    IOFixed centroidX = 0;
    IOFixed centroidY = 0;
    uint32_t touchCount = 0;
    uint32_t inRangeCount = 0;
    
    require_quiet(_digitizer.collections, exit);
    
    for (unsigned int i = 0; i < _digitizer.collections->getCount(); i++) {
        IOHIDDigitizerCollection *collection = OSDynamicCast(IOHIDDigitizerCollection,
                                                             _digitizer.collections->getObject(i));
        IOHIDEvent *event = NULL;
        bool eventTouch, eventInRange;
        
        if (!collection) {
            continue;
        }
        
        event = createEventForDigitizerCollection(collection, timestamp, reportID);
        if (!event) {
            continue;
        }
        
        if (!collectionEvent) {
            collectionEvent = IOHIDEvent::withType(kIOHIDEventTypeDigitizer,
                                                   timestamp,
                                                   0,
                                                   kIOHIDEventOptionIsAbsolute);
            require_action(collectionEvent, exit, event->release());
            
            collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerCollection, true);
            collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerType, collection->getType());
        }
        
        eventTouch = event->getIntegerValue(kIOHIDEventFieldDigitizerTouch) ? true : false;
        if (eventTouch) {
            touchCount++;
            touchX += event->getFixedValue(kIOHIDEventFieldDigitizerX);
            touchY += event->getFixedValue(kIOHIDEventFieldDigitizerY);
        }
        
        eventInRange = event->getIntegerValue(kIOHIDEventFieldDigitizerRange) ? true : false;
        if (eventInRange) {
            inRangeCount++;
            inRangeX += event->getFixedValue(kIOHIDEventFieldDigitizerX);
            inRangeY += event->getFixedValue(kIOHIDEventFieldDigitizerY);
        }
        
        touch |= eventTouch;
        range |= eventInRange;
        eventMask |= event->getIntegerValue(kIOHIDEventFieldDigitizerEventMask);
        buttonMask |= event->getIntegerValue(kIOHIDEventFieldDigitizerButtonMask);
        
        if (event->getIntegerValue(kIOHIDEventFieldDigitizerType) == kIOHIDDigitizerCollectionTypeFinger) {
            fingerCount++;
        }
        
        collectionEvent->appendChild(event);
        event->release();
    }
    
    require(collectionEvent, exit);
    
    if (touchCount) {
        centroidX = IOFixedDivide(touchX, touchCount << 16);
        centroidY = IOFixedDivide(touchY, touchCount << 16);
    } else if (inRangeCount) {
        centroidX = IOFixedDivide(inRangeX, inRangeCount << 16);
        centroidY = IOFixedDivide(inRangeY, inRangeCount << 16);
    }
    
    collectionEvent->setFixedValue(kIOHIDEventFieldDigitizerX, centroidX);
    collectionEvent->setFixedValue(kIOHIDEventFieldDigitizerY, centroidY);
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerRange, range);
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerTouch, touch);
    collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerButtonMask, buttonMask);
    if (fingerCount > 1) {
        collectionEvent->setIntegerValue(kIOHIDEventFieldDigitizerType, kIOHIDDigitizerCollectionTypeHand);
    }
    
    dispatchEvent(collectionEvent);
    
exit:
    OSSafeReleaseNULL(collectionEvent);
}

IOHIDEvent *IOUserHIDEventDriver::createEventForDigitizerCollection(IOHIDDigitizerCollection *collection,
                                                                    uint64_t timestamp,
                                                                    uint32_t reportID)
{
    IOHIDEvent *event = NULL;
    OSArray *elements = collection->getElements();
    bool handled = false;
    bool cancel = false;
    bool inRange = true;
    bool invert = false;
    bool touch = false;
    bool hasInRangeUsage = false;
    IOFixed x = 0;
    IOFixed y = 0;
    IOFixed z = 0;
    IOFixed tipPressure = 0;
    IOFixed barrelPressure = 0;
    IOFixed tiltX = 0;
    IOFixed tiltY = 0;
    IOFixed twist = 0;
    uint32_t buttonState = 0;
    uint32_t transducerID = reportID;
    uint32_t eventMask = 0;
    uint32_t eventOptions = kIOHIDEventOptionIsAbsolute;
    
    require(elements, exit);
    
    for (unsigned int i = 0; i < elements->getCount(); i++) {
        IOHIDElement *element = OSDynamicCast(IOHIDElement, elements->getObject(i));
        uint64_t elementTimeStamp;
        uint32_t usagePage, usage, value;
        bool elementIsCurrent;
        
        if (!element) {
            continue;
        }
        
        elementTimeStamp = element->getTimeStamp();
        elementIsCurrent = (element->getReportID()==reportID) && (timestamp == elementTimeStamp);
        usagePage = element->getUsagePage();
        usage = element->getUsage();
        value = element->getValue(0);
        
        switch (usagePage) {
            case kHIDPage_GenericDesktop:
                switch (usage) {
                    case kHIDUsage_GD_X:
                        x = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_GD_Y:
                        y = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_GD_Z:
                        z = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled |= elementIsCurrent;
                        break;
                }
                break;
                // TODO
//            case kHIDPage_Button:
//                setButtonState(&buttonState, (usage - 1), value);
//                handled    |= elementIsCurrent;
//                break;
            case kHIDPage_Digitizer:
                switch (usage) {
                    case kHIDUsage_Dig_TransducerIndex:
                    case kHIDUsage_Dig_ContactIdentifier:
                        transducerID = value;
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Untouch:
                        cancel = value;
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Touch:
                    case kHIDUsage_Dig_TipSwitch:
                        touch = value != 0;
                        setButtonState(&buttonState, 0, value);
                        handled |= (elementIsCurrent | (buttonState != 0));
                        break;
                    case kHIDUsage_Dig_BarrelSwitch:
                        setButtonState(&buttonState, 1, value);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Eraser:
                        setButtonState(&buttonState, 2, value);
                        invert = value != 0;
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_InRange:
                        inRange = value != 0;
                        handled |= elementIsCurrent;
                        hasInRangeUsage = true;
                        break;
                    case kHIDUsage_Dig_BarrelPressure:
                        barrelPressure = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        tipPressure = element->getScaledFixedValue(kIOHIDValueScaleTypeCalibrated);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_XTilt:
                        tiltX = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_YTilt:
                        tiltY = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Twist:
                        twist = element->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
                        handled |= elementIsCurrent;
                        break;
                    case kHIDUsage_Dig_Invert:
                        invert = value != 0;
                        handled |= elementIsCurrent;
                        break;
                    default:
                        break;
                }
                break;
        }
    }
    
    if (hasInRangeUsage == false && (cancel || touch == 0)) {
        inRange = false;
    }
    
    require(handled, exit);
    
    if (invert) {
        eventOptions |= kIOHIDTransducerInvert;
    }
    
    event = IOHIDEvent::withType(kIOHIDEventTypeDigitizer,
                                 timestamp,
                                 0,
                                 eventOptions);
    require(event, exit);
    
    // Set the event mask
    if (touch != collection->getTouch()) {
        eventMask |= kIOHIDDigitizerEventTouch;
    }
    
    if (cancel) {
        eventMask |= kIOHIDDigitizerEventCancel;
    }
    
    if (inRange) {
        if (collection->getX() != x ||
            collection->getY() != y ||
            collection->getZ() != z) {
            eventMask |= kIOHIDDigitizerEventPosition;
        }
    }
    
    if (inRange != collection->getInRange()) {
        eventMask |= kIOHIDDigitizerEventRange;
    }
    
    // If we get multiple untouch event we should discard it
    // reporting out of range , multiple untouch event can confuse
    // ui layer application
    if (collection->getTouch() == touch && touch == 0 && inRange == false) {
        event = NULL;
    } else {
        // Set the event fields
        event->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);
        event->setIntegerValue(kIOHIDEventFieldDigitizerRange, inRange);
        event->setIntegerValue(kIOHIDEventFieldDigitizerTouch, touch);
        event->setIntegerValue(kIOHIDEventFieldDigitizerIndex, transducerID);
        event->setIntegerValue(kIOHIDEventFieldDigitizerType, collection->getType());
        event->setIntegerValue(kIOHIDEventFieldDigitizerButtonMask, buttonState);
        event->setFixedValue(kIOHIDEventFieldDigitizerX, x);
        event->setFixedValue(kIOHIDEventFieldDigitizerY, y);
        event->setFixedValue(kIOHIDEventFieldDigitizerZ, z);
        event->setFixedValue(kIOHIDEventFieldDigitizerPressure, tipPressure);
        event->setFixedValue(kIOHIDEventFieldDigitizerAuxiliaryPressure, barrelPressure);
        event->setFixedValue(kIOHIDEventFieldDigitizerTwist, twist);
        event->setFixedValue(kIOHIDEventFieldDigitizerTiltX, tiltX);
        event->setFixedValue(kIOHIDEventFieldDigitizerTiltY, tiltY);
    }
    
    // Update the collection
    collection->setTouch(touch);
    collection->setX(x);
    collection->setY(y);
    collection->setZ(z);
    collection->setInRange(inRange);
    
exit:
    return event;
}

void
IMPL(IOUserHIDEventDriver, SetLED)
{
    require(_led.elements, exit);
    
    for (unsigned int i = 0; i < _led.elements->getCount(); i++) {
        IOHIDElement *element = (IOHIDElement *)_led.elements->getObject(i);
        
        if (element->getUsage() == usage) {
            IOReturn ret;
            element->setValue(on ? 1 : 0);
            ret = element->commit(kIOHIDElementCommitDirectionOut);
            HIDServiceLog("Set LED 0x%x: %d 0x%x", usage, on, ret);
            break;
        }
    }
    
exit:
    return;
}
