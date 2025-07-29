/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 2018 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "AppleUserHIDEventService.h"
#include "IOHIDPrivateKeys.h"
#include <AssertMacros.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "IOHIDDebug.h"
#include <IOKit/hid/AppleHIDUsageTables.h>
#include "IOHIDevicePrivateKeys.h"
#include "IOHIDFamilyPrivate.h"
#include <IOKit/IOKitKeys.h>
#include <IOKit/IOCommand.h>
#include <stdatomic.h>

#define kIOHIDEventServiceDextEntitlement "com.apple.developer.driverkit.family.hid.eventservice"


#define HIDEventServiceLogFault(fmt, ...)   HIDLogFault("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLogError(fmt, ...)   HIDLogError("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLog(fmt, ...)        HIDLog("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLogInfo(fmt, ...)    HIDLogInfo("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)
#define HIDEventServiceLogDebug(fmt, ...)   HIDLogDebug("%s:0x%llx " fmt "\n", getName(), getRegistryEntryID(), ##__VA_ARGS__)

enum {
    kAppleUserHIDEventServiceStateStarted     = 1,
    kAppleUserHIDEventServiceStateStopped     = 2,
    kAppleUserHIDEventServiceStateKRStart     = 4,
    kAppleUserHIDEventServiceStateDKStart     = 8,
};

#define SET_DICT_NUM(dict, key, val) do { \
    OSNumber *num = OSNumber::withNumber(val, 64); \
    if (num) { \
        dict->setObject(key, num); \
        num->release(); \
    } \
} while (0);


struct EventCopyCaller
{
    STAILQ_ENTRY(EventCopyCaller) eventLink;
    uint64_t id;
    IOHIDEvent * event;
};

struct SetPropertiesCaller
{
    STAILQ_ENTRY(SetPropertiesCaller) setLink;
    uint64_t id;
    IOReturn status;
};

struct SetLEDCaller
{
    STAILQ_ENTRY(SetLEDCaller) setLink;
    uint64_t id;
    IOReturn status;
};

//===========================================================================
// AppleUserHIDEventService class
//===========================================================================

#define super IOHIDEventDriver

OSDefineMetaClassAndStructors( AppleUserHIDEventService, IOHIDEventDriver )

#define _elements              ivar->elements
#define _provider              ivar->provider
#define _state                 ivar->state
#define _workLoop              ivar->workLoop
#define _commandGate           ivar->commandGate
#define _eventCopyCallers     (ivar->eventCopyCallers)
#define _setPropertiesCallers (ivar->setPropertiesCallers)
#define _setLEDCallers        (ivar->setLEDCallers)
#define _eventCopyAction       ivar->eventCopyAction
#define _setPropertiesAction   ivar->setPropertiesAction
#define _setLEDAction          ivar->setLEDAction


IOService * AppleUserHIDEventService::probe(IOService *provider, SInt32 *score)
{
    if (isSingleUser()) {
        return NULL;
    }
    
    return super::probe(provider, score);
}

bool AppleUserHIDEventService::init(OSDictionary *dict)
{
    if (!super::init(dict)) {
        return false;
    }
    
    ivar = IOMallocType(AppleUserHIDEventService::AppleUserHIDEventService_IVars);
    if (!ivar) {
        return false;
    }
    
    super::setProperty(kIOServiceDEXTEntitlementsKey, kIOHIDEventServiceDextEntitlement);
    
    super::setProperty(kIOHIDRegisterServiceKey, kOSBooleanFalse);

    return true;
}

void AppleUserHIDEventService::free()
{
    if (ivar) {
        if (_workLoop && _commandGate) {
            _workLoop->removeEventSource(_commandGate);
        }

        OSSafeReleaseNULL(_elements);
        OSSafeReleaseNULL(_commandGate);
        OSSafeReleaseNULL(_workLoop);
        IOFreeType(ivar, AppleUserHIDEventService::AppleUserHIDEventService_IVars);
    }
    
    super::free();
}

//----------------------------------------------------------------------------------------------------
// AppleUserHIDEventService::start
//----------------------------------------------------------------------------------------------------
bool AppleUserHIDEventService::start(IOService * provider)
{
    bool            ok        = false;
    IOReturn        ret       = kIOReturnSuccess;
    IOHIDInterface *interface = NULL;
    
    bool dkStart = getProperty(kIOHIDDKStartKey) == kOSBooleanTrue;

    HIDEventServiceLog ("start (state:0x%x)", _state);

    bool krStart = ((atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateKRStart) &  kAppleUserHIDEventServiceStateKRStart) == 0);

    if (dkStart &&
        (atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateDKStart) &  kAppleUserHIDEventServiceStateDKStart)) {
        //attemt to call kernel multiple times
        HIDEventServiceLogError("Attempt to do kernel start for device multiple times");
        return kIOReturnError;
    }
    
    if (krStart) {
        interface = OSDynamicCast(IOHIDInterface, provider);
        require(interface, exit);
        
        _elements = interface->createMatchingElements();
        require(_elements, exit);
        
        _provider = interface;

        STAILQ_INIT(&_eventCopyCallers);
        STAILQ_INIT(&_setPropertiesCallers);
        STAILQ_INIT(&_setLEDCallers);

        _workLoop = getWorkLoop();
        require_action (_workLoop, exit, HIDEventServiceLogError("workLoop\n"));
        _workLoop->retain();

        _commandGate = IOCommandGate::commandGate(this);
        require_action(_commandGate && _workLoop->addEventSource(_commandGate) == kIOReturnSuccess, exit, HIDEventServiceLogError("_commandGate"));
    }
    
    if (!dkStart) {
        ret = Start(provider);
        require_noerr_action(ret, exit, HIDEventServiceLogError("AppleUserHIDEventService::Start:0x%x\n", ret));
    } else if (krStart) {
        ok = super::start(provider);
        require_action(ok, exit, HIDEventServiceLogError("super::start:0x%x\n", ok));
    } else {
        return super::start(provider);
    }

    atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateStarted);

    ok = true;
    
exit:
    if (_state & kAppleUserHIDEventServiceStateStarted && !ok) {
        stop(provider);
    }
    
    return ok;
}

void AppleUserHIDEventService::handleStop(IOService * service)
{
    atomic_fetch_or((_Atomic UInt32 *)&_state, kAppleUserHIDEventServiceStateStopped);

    if (_workLoop && _commandGate) {
        __block EventCopyCaller     * eventCaller;
        __block SetPropertiesCaller * setCaller;
        __block SetLEDCaller        * setLEDCaller;

        _commandGate->runActionBlock(^IOReturn {
            STAILQ_FOREACH (eventCaller, &_eventCopyCallers, eventLink) {
                _commandGate->commandWakeup(eventCaller);
            }

            STAILQ_FOREACH (setCaller, &_setPropertiesCallers, setLink) {
                _commandGate->commandWakeup(setCaller);
            }

            STAILQ_FOREACH (setLEDCaller, &_setLEDCallers, setLink) {
                _commandGate->commandWakeup(setLEDCaller);
            }

            return kIOReturnSuccess;
        });

    }

    OSSafeReleaseNULL(_eventCopyAction);
    OSSafeReleaseNULL(_setPropertiesAction);
    OSSafeReleaseNULL(_setLEDAction);

    super::handleStop(service);
}


OSArray *AppleUserHIDEventService::getReportElements()
{
    return _elements;
}

IOReturn AppleUserHIDEventService::setElementValue(UInt32 usagePage,
                                                   UInt32 usage,
                                                   UInt32 value)
{
    __block IOReturn status = kIOReturnNotReady;
    SetLEDCaller caller;
    static uint64_t callerID = 0;

    require(!isInactive() && _setLEDAction, exit);

    if (!_workLoop->inGate()) {
        _commandGate->runActionBlock(^{
            status = AppleUserHIDEventService::setElementValue(usagePage,
                                                               usage,
                                                               value);
            return kIOReturnSuccess;
        });
        return status;
    }

    retain();

    caller.id = callerID++;
    STAILQ_INSERT_TAIL(&_setLEDCallers, &caller, setLink);
    caller.status = kIOReturnAborted;

    SetLEDAction(usagePage, usage, value, caller.id, _setLEDAction);

    _commandGate->commandSleep(&caller, THREAD_UNINT);

    status = caller.status;

    STAILQ_REMOVE(&_setLEDCallers, &caller, SetLEDCaller, setLink);

    release();

exit:
    return status;
}

bool AppleUserHIDEventService::terminate(IOOptionBits options)
{
    
    // This should be tracked by driverkit and we need not to explicitly call
    // close for provider as we earlier did as fix for 48119007
    
    return super::terminate(options);
}

bool AppleUserHIDEventService::handleStart(IOService *provider __unused)
{
    return true;
}

OSString *AppleUserHIDEventService::getTransport()
{
    return IOHIDEventService::getTransport();
}

OSString *AppleUserHIDEventService::getManufacturer()
{
    return IOHIDEventService::getManufacturer();
}

OSString *AppleUserHIDEventService::getProduct()
{
    return IOHIDEventService::getProduct();
}

OSString *AppleUserHIDEventService::getSerialNumber()
{
    return IOHIDEventService::getSerialNumber();
}

UInt32 AppleUserHIDEventService::getLocationID()
{
    return IOHIDEventService::getLocationID();
}

UInt32 AppleUserHIDEventService::getVendorID()
{
    return IOHIDEventService::getVendorID();
}

UInt32 AppleUserHIDEventService::getVendorIDSource()
{
    return IOHIDEventService::getVendorIDSource();
}

UInt32 AppleUserHIDEventService::getProductID()
{
    return IOHIDEventService::getProductID();
}

UInt32 AppleUserHIDEventService::getVersion()
{
    return IOHIDEventService::getVersion();
}

UInt32 AppleUserHIDEventService::getCountryCode()
{
    return IOHIDEventService::getCountryCode();
}


void AppleUserHIDEventService::dispatchKeyboardEvent(AbsoluteTime                timeStamp,
                                                     UInt32                      usagePage,
                                                     UInt32                      usage,
                                                     UInt32                      value,
                                                     IOOptionBits                options)
{
    require_action (_state & kAppleUserHIDEventServiceStateStarted, exit, HIDEventServiceLogError("HID EventService not ready (state:0x%x)", _state));
    super::dispatchKeyboardEvent (timeStamp, usagePage, usage, value, options);
exit:
    return;
}



void AppleUserHIDEventService::dispatchScrollWheelEventWithFixed(AbsoluteTime               timeStamp,
                                                                IOFixed                     deltaAxis1,
                                                                IOFixed                     deltaAxis2,
                                                                IOFixed                     deltaAxis3,
                                                                IOOptionBits                options)
{
    require_action (_state & kAppleUserHIDEventServiceStateStarted, exit, HIDEventServiceLogError("HID EventService not ready (state:0x%x)", _state));
    super::dispatchScrollWheelEventWithFixed (timeStamp, deltaAxis1, deltaAxis2, deltaAxis3, options);
exit:
    return;
}

void AppleUserHIDEventService::dispatchEvent(IOHIDEvent * event, IOOptionBits options)
{
    require_action (_state & kAppleUserHIDEventServiceStateStarted, exit, HIDEventServiceLogError("HID EventService not ready (state:0x%x)", _state));
    super::dispatchEvent (event, options);
exit:
    return;
}

IOHIDEvent * AppleUserHIDEventService::copyEvent(
                                IOHIDEventType              type,
                                IOHIDEvent *                matching,
                                IOOptionBits                options __unused)
{
    OSDictionary * matchingDict = OSDictionary::withCapacity(3);
    IOHIDEvent * event = NULL;
    
    require_quiet(!isInactive(), exit);
    
    SET_DICT_NUM (matchingDict, kIOHIDEventTypeKey, type);
    
    if (matching) {
        if (type == kIOHIDEventTypeKeyboard) {
            
            SET_DICT_NUM (matchingDict, kIOHIDUsagePageKey, matching->getIntegerValue(kIOHIDEventFieldKeyboardUsagePage));
            SET_DICT_NUM (matchingDict, kIOHIDUsageKey, matching->getIntegerValue(kIOHIDEventFieldKeyboardUsage));
        } else if (type == kIOHIDEventTypeBiometric) {
            
            SET_DICT_NUM (matchingDict, kIOHIDUsagePageKey, matching->getIntegerValue(kIOHIDEventFieldBiometricUsagePage));
            SET_DICT_NUM (matchingDict, kIOHIDUsageKey, matching->getIntegerValue(kIOHIDEventFieldBiometricUsage));
        } else if (type == kIOHIDEventTypeVendorDefined) {

            SET_DICT_NUM (matchingDict, kIOHIDUsagePageKey, matching->getIntegerValue(kIOHIDEventFieldVendorDefinedUsagePage));
            SET_DICT_NUM (matchingDict, kIOHIDUsageKey, matching->getIntegerValue(kIOHIDEventFieldVendorDefinedUsage));
        } else if (type == kIOHIDEventTypeOrientation) {
            // Orientation events don't respect using usages
            SET_DICT_NUM(matchingDict, kIOHIDUsagePageKey, 0ull);
            SET_DICT_NUM(matchingDict, kIOHIDUsageKey, 0ull);
        }
    }
    event = copyMatchingEvent (matchingDict);

exit:
    OSSafeReleaseNULL(matchingDict);
    
    return event;
}

IOHIDEvent * AppleUserHIDEventService::copyMatchingEvent(OSDictionary * matching)
{
    __block IOHIDEvent * event = NULL;
    EventCopyCaller      caller;
    static uint64_t callerID = 0;

    require_quiet(!isInactive() && _eventCopyAction, exit);

    if (!_workLoop->inGate()) {
        _commandGate->runActionBlock(^{
            event = AppleUserHIDEventService::copyMatchingEvent(matching);
            return kIOReturnSuccess;
        });
        return event;
    }

    retain();
    
    caller.id = callerID++;
    STAILQ_INSERT_TAIL(&_eventCopyCallers, &caller, eventLink);
    caller.event = NULL;
    
    CopyEvent(matching, caller.id, _eventCopyAction);

    _commandGate->commandSleep(&caller, THREAD_UNINT);

    event = caller.event;
  
    STAILQ_REMOVE(&_eventCopyCallers, &caller, EventCopyCaller, eventLink);
    
    release();
    
exit:
    return event;
}


void AppleUserHIDEventService::completeCopyEvent(OSAction * action, IOHIDEvent * event, uint64_t context)
{
    _commandGate->runActionBlock(^IOReturn{
        EventCopyCaller * caller = NULL;

        if (!_eventCopyAction && action) {
            action->retain();

            action->SetAbortedHandler(^{
                __block EventCopyCaller * eventCaller;
                if (_commandGate) {
                    _commandGate->runActionBlock(^IOReturn{
                        STAILQ_FOREACH (eventCaller, &_eventCopyCallers, eventLink) {
                            _commandGate->commandWakeup(eventCaller);
                        }

                        OSSafeReleaseNULL(_eventCopyAction);
                        return kIOReturnSuccess;
                    });
                }
            });

            _eventCopyAction = action;

        } else if (_eventCopyAction && !action) {
            OSSafeReleaseNULL(_eventCopyAction);
        }

        STAILQ_FOREACH (caller, &_eventCopyCallers, eventLink) {
            if (caller->id == context) {
                if (event) {
                    event->retain();
                }
                caller->event = event;
                _commandGate->commandWakeup(caller);
                break;
            }
        }
        return kIOReturnSuccess;
    });
}

IOReturn AppleUserHIDEventService::setProperties(OSObject *properties)
{
    OSDictionaryPtr propDict = OSDynamicCast(OSDictionary, properties);
    __block IOReturn status = kIOReturnNotReady;
    SetPropertiesCaller caller;
    static uint64_t callerID = 0;

    require(!isInactive() && _setPropertiesAction && propDict, exit);

    if (!_workLoop->inGate()) {
        _commandGate->runActionBlock(^{
            status = AppleUserHIDEventService::setProperties(properties);
            return kIOReturnSuccess;
        });
        return status;
    }

    retain();

    caller.id = callerID++;
    STAILQ_INSERT_TAIL(&_setPropertiesCallers, &caller, setLink);
    caller.status = kIOReturnAborted;

    SetUserProperties(propDict, caller.id, _setPropertiesAction);

    _commandGate->commandSleep(&caller, THREAD_UNINT);

    status = caller.status;

    STAILQ_REMOVE(&_setPropertiesCallers, &caller, SetPropertiesCaller, setLink);

    release();

exit:
    return status;
}

void AppleUserHIDEventService::completeSetProperties(OSAction * action, IOReturn status, uint64_t context)
{
    _commandGate->runActionBlock(^IOReturn{
        SetPropertiesCaller * caller = NULL;

        if (!_setPropertiesAction && action) {
            action->retain();

            action->SetAbortedHandler(^{
                __block SetPropertiesCaller * setCaller;
                if (_commandGate) {
                    _commandGate->runActionBlock(^IOReturn{
                        STAILQ_FOREACH (setCaller, &_setPropertiesCallers, setLink) {
                            _commandGate->commandWakeup(setCaller);
                        }

                        OSSafeReleaseNULL(_setPropertiesAction);
                        return kIOReturnSuccess;
                    });
                }
            });

            _setPropertiesAction = action;

        } else if (_setPropertiesAction && !action) {
            OSSafeReleaseNULL(_setPropertiesAction);
        }

        STAILQ_FOREACH (caller, &_setPropertiesCallers, setLink) {
            if (caller->id == context) {
                caller->status = status;
                _commandGate->commandWakeup(caller);
                break;
            }
        }
        return kIOReturnSuccess;
    });
}

void AppleUserHIDEventService::completeSetLED(OSAction * action, IOReturn status, uint64_t context)
{
    _commandGate->runActionBlock(^IOReturn{
        SetLEDCaller * caller = NULL;

        if (!_setLEDAction && action) {
            action->retain();

            action->SetAbortedHandler(^{
                __block SetLEDCaller * setCaller;
                if (_commandGate) {
                    _commandGate->runActionBlock(^IOReturn{
                        STAILQ_FOREACH (setCaller, &_setLEDCallers, setLink) {
                            _commandGate->commandWakeup(setCaller);
                        }

                        OSSafeReleaseNULL(_setLEDAction);
                        return kIOReturnSuccess;
                    });
                }
            });

            _setLEDAction = action;

        } else if (_setLEDAction && !action) {
            OSSafeReleaseNULL(_setLEDAction);
        }

        STAILQ_FOREACH (caller, &_setLEDCallers, setLink) {
            if (caller->id == context) {
                caller->status = status;
                _commandGate->commandWakeup(caller);
                break;
            }
        }
        return kIOReturnSuccess;
    });
}

void AppleUserHIDEventService::updateElementsProperty(OSArray * userElements, OSArray * deviceElements)
{
    OSNumber * number;
    uint32_t cookie;

    require(userElements && deviceElements, exit);

    for (uint32_t index = 0; index < userElements->getCount(); index++) {
        if ((number = OSDynamicCast(OSNumber, userElements->getObject(index)))) {
            cookie = number->unsigned32BitValue();
            if (cookie < deviceElements->getCount()) {
                userElements->replaceObject(index, deviceElements->getObject(cookie));
            }
        }
    }

exit:
    return;
}

void AppleUserHIDEventService::setSensorProperties(OSDictionary * sensorProps, OSArray * deviceElements)
{
    OSNumber * propertyNum;
    static const char *propKeys[] = {"ReportInterval", "MaxFIFOEvents", "ReportLatency", "SniffControl"};
    uint32_t cookie;

    require(sensorProps, exit);

    for (const char * key : propKeys) {
        if ((propertyNum = OSDynamicCast(OSNumber, sensorProps->getObject(key)))) {
            cookie = propertyNum->unsigned32BitValue();
            if (cookie < deviceElements->getCount()) {
                sensorProps->setObject(key, deviceElements->getObject(cookie));
            }
        }
    }

    setProperty("SensorProperties", sensorProps);

exit:
    return;
}

void AppleUserHIDEventService::setDigitizerProperties(OSDictionary * digitizerProps, OSArray * deviceElements)
{
    OSNumber * propertyNum;
    OSArray * digitizerCollections;
    OSArray * elements;
    OSDictionary * collection;
    static const char * propKeys[] = {"touchCancelElement", "DeviceModeElement", kIOHIDDigitizerSurfaceSwitchKey, "ReportRate"};
    uint32_t cookie;

    require(digitizerProps && deviceElements, exit);

    if ((digitizerCollections = OSDynamicCast(OSArray, digitizerProps->getObject("Transducers")))) {
        for (uint32_t index = 0; index < digitizerCollections->getCount(); index++) {
            if ((collection = OSDynamicCast(OSDictionary, digitizerCollections->getObject(index)))) {
                if ((propertyNum = (OSDynamicCast(OSNumber, collection->getObject(kIOHIDElementParentCollectionKey))))) {
                    cookie = propertyNum->unsigned32BitValue();
                    if (cookie < deviceElements->getCount()) {
                        collection->setObject(kIOHIDElementParentCollectionKey, deviceElements->getObject(cookie));
                    }
                }

                if ((elements = OSDynamicCast(OSArray, collection->getObject(kIOHIDElementKey)))) {
                    updateElementsProperty(elements, deviceElements);
                }
            }
        }

    } else {
        digitizerProps->removeObject("Transducers");
    }

    for (const char * key : propKeys) {
        if ((propertyNum = OSDynamicCast(OSNumber, digitizerProps->getObject(key)))) {
            cookie = propertyNum->unsigned32BitValue();
            if (cookie < deviceElements->getCount()) {
                digitizerProps->setObject(key, deviceElements->getObject(cookie));
            }
        }
    }

    setProperty("Digitizer", digitizerProps);

exit:
    return;
}

void AppleUserHIDEventService::setUnicodeProperties(OSDictionary * unicodeProps, OSArray * deviceElements)
{
    OSNumber * propertyNum;
    OSArray * unicodeCandidates;
    OSArray * elements;
    OSDictionary * collection;
    uint32_t cookie;

    require(unicodeProps, exit);

    if ((propertyNum = OSDynamicCast(OSNumber, unicodeProps->getObject("GestureCharacterStateElement")))) {
        cookie = propertyNum->unsigned32BitValue();
        if (cookie < deviceElements->getCount()) {
            unicodeProps->setObject("GestureCharacterStateElement", deviceElements->getObject(cookie));
        }
    }

    if ((unicodeCandidates = OSDynamicCast(OSArray, unicodeProps->getObject("Gesture")))) {
        for (uint32_t index = 0; index < unicodeCandidates->getCount(); index++) {
            if ((collection = OSDynamicCast(OSDictionary, unicodeCandidates->getObject(index)))) {
                if ((propertyNum = (OSDynamicCast(OSNumber, collection->getObject(kIOHIDElementParentCollectionKey))))) {
                    cookie = propertyNum->unsigned32BitValue();
                    if (cookie < deviceElements->getCount()) {
                        collection->setObject(kIOHIDElementParentCollectionKey, deviceElements->getObject(cookie));
                    }
                }

                if ((elements = OSDynamicCast(OSArray, collection->getObject(kIOHIDElementKey)))) {
                    updateElementsProperty(elements, deviceElements);
                }
            }
        }

    } else {
        unicodeProps->removeObject("Gesture");
    }

    setProperty("Unicode", unicodeProps);

exit:
    return;
}

IOReturn AppleUserHIDEventService::setSystemProperties(OSDictionary * properties)
{
    IOReturn ret = kIOReturnSuccess;
    IOHIDInterface * interface = OSDynamicCast(IOHIDInterface, _provider);
    OSArray * deviceElements = NULL;
    OSArray * userElements;
    OSArray * existingElements;
    OSDictionary * elementProperty;
    OSDictionary * existingProperty;
    static const char * propKeys[] = {kIOHIDSurfaceDimensionsKey, kIOHIDEventServiceSensorPropertySupportedKey, kIOHIDKeyboardEnabledKey, kIOHIDKeyboardEnabledByEventKey, kIOHIDReportIntervalKey,
                                      kIOHIDSupportsGlobeKeyKey, kIOHIDGameControllerFormFittingKey, kIOHIDKeyboardEnabledEventKey, kIOHIDPointerAccelerationTypeKey, kIOHIDScrollAccelerationTypeKey,
                                      kIOHIDGameControllerTypeKey, kIOHIDDigitizerGestureCharacterStateKey, "SupportsInk"};
    static const char * elementKeys[] = {"Biometric", "GameControllerPointer", "MultiAxisPointer", "LED", "Keyboard", "Accel", "Gyro", "Temperature", "Compass",
                                         "Orientation", "RelativePointer", "DeviceOrientation", "Scroll", "PrimaryVendorMessage", "ChildVendorMessage"};

    require(properties, exit);

    // Set simple properties listed above if present
    for (const char * key : propKeys) {
        if (properties->getObject(key)) {
            setProperty(key, properties->getObject(key));
            properties->removeObject(key);
        }
    }

    require_action(interface, exit, ret = kIOReturnNoResources);
    deviceElements = interface->createMatchingElements(NULL, kIOHIDSearchDeviceElements);

    // Set "Elements" properties listed above if present
    for (const char * key : elementKeys) {
        if ((elementProperty = OSDynamicCast(OSDictionary, properties->getObject(key))) && (userElements = OSDynamicCast(OSArray, elementProperty->getObject(kIOHIDElementKey)))) {
            updateElementsProperty(userElements, deviceElements);
            if ((existingProperty = OSDynamicCast(OSDictionary, getProperty(key))) && (existingElements = OSDynamicCast(OSArray, existingProperty->getObject(kIOHIDElementKey)))) {
                userElements->merge(existingElements);
            }
            setProperty(key, elementProperty);
            properties->removeObject(key);
        }
    }

    setSensorProperties((OSDynamicCast(OSDictionary, properties->getObject("SensorProperties"))), deviceElements);
    properties->removeObject("SensorProperties");

    setDigitizerProperties((OSDynamicCast(OSDictionary, properties->getObject("Digitizer"))), deviceElements);
    properties->removeObject("Digitizer");

    setUnicodeProperties((OSDynamicCast(OSDictionary, properties->getObject("Unicode"))), deviceElements);
    properties->removeObject("Unicode");

    // Publish remaining properties under "HIDEventServiceProperties" key
    if (properties->getCount()) {
        properties->setObject(kIOHIDDeviceParametersKey, kOSBooleanTrue);
        ret = super::setSystemProperties(properties);
        properties->removeObject(kIOHIDDeviceParametersKey);
    }

exit:
    OSSafeReleaseNULL(deviceElements);
    return ret;
}
