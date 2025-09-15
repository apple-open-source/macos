/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * This document is the property of Apple Inc.
 * It is considered confidential and proprietary.
 *
 * This document may not be reproduced or transmitted in any form,
 * in whole or in part, without the express written permission of
 * Apple Inc.
 */

#define IOKIT_ENABLE_SHARED_PTR

#include "IOHIDElementProcessor.h"
#include <IOKit/IOService.h>
#include <IOKit/hid/IOHIDEvent.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDElement.h>
#include "IOHIDUsageTables.h"
#include "AppleHIDUsageTables.h"
#include <IOKit/IOLib.h>
#include <libkern/c++/OSDictionary.h>
#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSNumber.h>
#include <libkern/c++/OSString.h>
#include <AssertMacros.h>
#include <math.h>
#include "../IOHIDDebug.h"

#define HIDElementProcessorLog(fmt, ...)       HIDLog("[%s] " fmt "\n", this->getMetaClass()->getClassName(), ##__VA_ARGS__)
#define HIDElementProcessorLogError(fmt, ...)  HIDLogError("[%s] " fmt "\n", this->getMetaClass()->getClassName(), ##__VA_ARGS__)

static constexpr UInt32 HIDUsagePair(UInt32 page, UInt32 usage) {
    return ((page & 0xFFFF) << 16) | (usage & 0xFFFF);
}

#pragma mark - IOHIDElementProcessor

OSDefineMetaClassAndAbstractStructors(IOHIDElementProcessor, OSObject);

bool
IOHIDElementProcessor::init(IOService * owner, UInt8 reportID, IOHIDEventType type, UInt32 page, UInt32 usage)
{
    assert(super::init());
    _owner = owner;
    _reportID = reportID;
    _type = type;
    _page = page;
    _usage = usage;
    return true;
}

OSSharedPtr<IOHIDEvent>
IOHIDElementProcessor::processInput(uint64_t timestamp, uint8_t reportID)
{
    OSSharedPtr<IOHIDEvent> event = NULL;
    OSSharedPtr<OSArray> childEvents = NULL;
    bool ok = false;

    if (_children && _children->getCount() > 0) {
        for (unsigned int i = 0; i < _children->getCount(); ++i) {
            IOHIDElementProcessor * child = OSRequiredCast(IOHIDElementProcessor, _children->getObject(i));
            OSSharedPtr<IOHIDEvent> childEvent = child->processInput(timestamp, reportID);
            if (childEvent) {
                if (!childEvents) {
                    childEvents = OSArray::withCapacity(_children->getCount());
                    assert(childEvents);
                }
                ok = childEvents->setObject(childEvent.get());
                assert(ok);
            }
        }
    }

    if (getReportID() == reportID || (childEvents && childEvents->getCount() > 0)) {
        event = createEvent(timestamp);
        assert(event);
        if (childEvents) {
            for (unsigned int i = 0; i < childEvents->getCount(); ++i) {
                IOHIDEvent * childEvent = OSRequiredCast(IOHIDEvent, childEvents->getObject(i));
                event->appendChild(childEvent);
            }
        }
    }

    return event;
}

void
IOHIDElementProcessor::setProperty(OSString * key, OSObject * val)
{
    bool ok = false;
    require_quiet(key && val, exit);

    // lazily allocate properties dictionary
    if (!_properties) {
        _properties = OSDictionary::withCapacity(1);
        assert(_properties);
    }

    ok = _properties->setObject(key, val);
    assert(ok);

exit:
    return;
}

bool
IOHIDElementProcessor::serialize(OSSerialize * serializer) const
{
    bool ok = false;
    OSSharedPtr<OSDictionary> dict = OSDictionary::withCapacity(6);
    assert(dict);

    ok = dict->setObject("IOHIDEventType", OSNumber::withNumber(getType(), 32));
    assert(ok);

    ok = dict->setObject("Cookie", OSNumber::withNumber(getCookie(), 32));
    assert(ok);

    ok = dict->setObject("UsagePage", OSNumber::withNumber(getUsagePage(), 32));
    assert(ok);

    ok = dict->setObject("Usage", OSNumber::withNumber(getUsage(), 32));
    assert(ok);

    if (_properties) {
        ok = dict->merge(_properties.get());
        assert(ok);
    }
    if (_children) {
        ok = dict->setObject("Children", _children);
        assert(ok);
    }

    return dict->serialize(serializer);
}

void
IOHIDElementProcessor::appendChild(IOHIDElementProcessor * child)
{
    assert(child);
    assert(!isParentOf(child));

    // lazily allocate children array
    if (!_children) {
        _children = OSArray::withCapacity(1);
        assert(_children);
    }

    _children->setObject(child);
}

void
IOHIDElementProcessor::appendChildren(OSArray * children)
{
    assert(children);
    for (unsigned int i = 0; i < children->getCount(); ++i) {
        IOHIDElementProcessor * child = OSRequiredCast(IOHIDElementProcessor, children->getObject(i));
        appendChild(child);
    }
}

bool
IOHIDElementProcessor::isParentOf(IOHIDElementProcessor * child)
{
    bool found = false;
    if (_children) {
        size_t count = _children->getCount();
        for (unsigned int i = 0; i < count; ++i) {
            found = _children->getObject(i)->isEqualTo(child);
            if (found) {
                break;
            }
        }
    }
    return found;
}

OSSharedPtr<IOHIDElement>
IOHIDElementProcessor::copyElement(OSArray * elements, IOHIDElementType type, uint16_t page, uint16_t usage)
{
    IOHIDElement * target = NULL;
    for (unsigned int i = 0; i < elements->getCount(); ++i) {
        IOHIDElement * element = OSRequiredCast(IOHIDElement, elements->getObject(i));
        if (element->getType() == type && element->getUsagePage() == page && (element->getUsage() == usage || 0 == usage)) {
            target = element;
            break;
        }
    }
    return OSSharedPtr<IOHIDElement>(target, OSRetain);
}


#pragma mark - IOHIDRootElementProcessor

OSDefineMetaClassAndStructors(IOHIDRootElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDRootElementProcessor>
IOHIDRootElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDRootElementProcessor> me = OSMakeShared<IOHIDRootElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDRootElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;

    assert(collection);

    require_action_quiet(collection->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type:%d", collection->getUsagePage()));
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypeApplication, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    success = super::init(owner, 0, kIOHIDEventTypeCollection, collection->getUsagePage(), collection->getUsage());

    success = true;

exit:
    return success;
}

OSSharedPtr<IOHIDEvent>
IOHIDRootElementProcessor::createEvent(uint64_t timestamp)
{
    IOHIDEvent * event = IOHIDEvent::collectionEvent(timestamp, getUsagePage(), getUsage(), false);
    assert(event);

    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}


#pragma mark - IOHIDAccelElementProcessor

OSDefineMetaClassAndStructors(IOHIDAccelElementProcessor, IOHIDElementProcessor);

static const uint32_t HID_UNIT_ACCELERATION = 0xE011; // cm/s^2

OSSharedPtr<IOHIDAccelElementProcessor>
IOHIDAccelElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDAccelElementProcessor> me = OSMakeShared<IOHIDAccelElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDAccelElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;
    int reportID = -1;
    unsigned int sampleCount = 0;

    _x = OSArray::withCapacity(1);
    _y = OSArray::withCapacity(1);
    _z = OSArray::withCapacity(1);
    _ts = OSArray::withCapacity(1);

    assert(collection);
    require_quiet(collection->getUsagePage() == kHIDPage_Sensor, exit);
    require_quiet(collection->getUsage() == kHIDUsage_Snsr_Motion_Accelerometer3D || collection->getUsage() == kHIDUsage_Snsr_Motion, exit);
    require_action_quiet(collection->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type:%d", collection->getUsagePage()));
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypePhysical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    for (unsigned int i = 0, count = children->getCount(); i < count; ++i) {
        IOHIDElement * element = OSRequiredCast(IOHIDElement, children->getObject(i));

        // only consider input elements
        if (element->getType() != kIOHIDElementTypeInput_Misc) {
            continue;
        }

        // all input elements should be in the same report
        if (reportID == -1) {
            reportID = element->getReportID();
        }
        else if (element->getReportID() != reportID) {
            continue;
        }

        switch(HIDUsagePair(element->getUsagePage(), element->getUsage())) {
            case HIDUsagePair(kHIDPage_Sensor, kHIDUsage_Snsr_Data_Motion_AccelerationAxisX):
                _x->setObject(element);
                break;
            case HIDUsagePair(kHIDPage_Sensor, kHIDUsage_Snsr_Data_Motion_AccelerationAxisY):
                _y->setObject(element);
                break;
            case HIDUsagePair(kHIDPage_Sensor, kHIDUsage_Snsr_Data_Motion_AccelerationAxisZ):
                _z->setObject(element);
                break;
            case HIDUsagePair(kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp):
                _ts->setObject(element);
                break;
            default:
                break;
        }
    }

    sampleCount = _x->getCount();
    require_action_quiet(sampleCount > 0, exit, HIDElementProcessorLogError("got no complete (x,y,z,ts) input tuples"));
    require_action_quiet(sampleCount == _y->getCount(), exit, HIDElementProcessorLogError("x(%d),y(%d) count mismatch", sampleCount, _y->getCount()));
    require_action_quiet(sampleCount == _z->getCount(), exit, HIDElementProcessorLogError("x(%d),z(%d) count mismatch", sampleCount, _z->getCount()));
    require_action_quiet(sampleCount == _ts->getCount(), exit, HIDElementProcessorLogError("x(%d),ts(%d) count mismatch", sampleCount, _ts->getCount()));
    require_action_quiet(reportID >= 0, exit, HIDElementProcessorLogError("bad report id:%d", reportID));

    // features
    _reportInterval = copyElement(children, kIOHIDElementTypeFeature, kHIDPage_Sensor, kHIDUsage_Snsr_Property_ReportInterval);
    require_action_quiet(_reportInterval.get(), exit, HIDElementProcessorLogError("missing report interval element"));

    _sampleInterval = copyElement(children, kIOHIDElementTypeFeature, kHIDPage_Sensor, kHIDUsage_Snsr_Property_SamplingRate);

    success = super::init(owner, reportID, kIOHIDEventTypeAccelerometer, kHIDPage_Sensor, kHIDUsage_Snsr_Motion_Accelerometer3D);
    if (success) {
        bool ok = owner->setProperty("SupportsAccelEvents", kOSBooleanTrue);
        assert(ok);
    }

exit:
    return success;
}

void
IOHIDAccelElementProcessor::setProperty(OSString * key, OSObject * val)
{
    OSNumber * num = nullptr;
    IOHIDElement * element = nullptr;
    assert(key);
    assert(val);

    if (key->isEqualTo(kIOHIDSensorPropertyReportIntervalKey)) {
        num = OSDynamicCast(OSNumber, val);
        element = _reportInterval.get();
    }
    else if (key->isEqualTo(kIOHIDSensorPropertySampleIntervalKey)) {
        num = OSDynamicCast(OSNumber, val);
        element = _sampleInterval.get();
    }

    if (element && num) {
        element->setValue(num->unsigned32BitValue(), kIOHIDValueOptionsUpdateElementValues);
    }

    super::setProperty(key, val);
}

OSSharedPtr<IOHIDEvent>
IOHIDAccelElementProcessor::createEvent(uint64_t timestamp)
{
    OSSharedPtr<IOHIDEvent> event = nullptr;
    const unsigned int sampleCount = _x->getCount();

    event = OSSharedPtr<IOHIDEvent>(IOHIDEvent::collectionEvent(timestamp, getUsagePage(), getUsage(), false), OSNoRetain);
    assert(event);

    for (unsigned int i = 0; i < sampleCount; ++i) {
        OSSharedPtr<IOHIDEvent> subevent = eventForSample(timestamp, i);
        assert(subevent);

        event->appendChild(subevent.get());
    }

    return event;
}

OSSharedPtr<IOHIDEvent>
IOHIDAccelElementProcessor::eventForSample(uint64_t timestamp, unsigned int i) const
{
    IOFixed x = getXAcceleration(i);
    IOFixed y = getYAcceleration(i);
    IOFixed z = getZAcceleration(i);
    OSData * ts = OSRequiredCast(IOHIDElement, _ts->getObject(i))->getDataValue();

    IOHIDEvent * event = IOHIDEvent::accelerometerEvent(timestamp, x, y, z);
    assert(event);

    IOHIDEvent * child = IOHIDEvent::vendorDefinedEvent(timestamp, kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp, 0, (UInt8 *)ts->getBytesNoCopy(), ts->getLength());
    assert(child);

    event->appendChild(child);
    OSSafeReleaseNULL(child);

    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}

IOFixed
IOHIDAccelElementProcessor::getXAcceleration(unsigned int i) const
{
    return getAccelerationValue(OSRequiredCast(IOHIDElement, _x->getObject(i)));
}

IOFixed
IOHIDAccelElementProcessor::getYAcceleration(unsigned int i) const
{
    return getAccelerationValue(OSRequiredCast(IOHIDElement, _y->getObject(i)));
}

IOFixed
IOHIDAccelElementProcessor::getZAcceleration(unsigned int i) const
{
    return getAccelerationValue(OSRequiredCast(IOHIDElement, _z->getObject(i)));
}

IOFixed
IOHIDAccelElementProcessor::getAccelerationValue(IOHIDElement * element)
{
    IOFixed value = element->getScaledFixedValue(kIOHIDValueScaleTypeExponent);
    if (element->getUnit() == HID_UNIT_ACCELERATION) {
        // convert HID acceleration units to G's
        value = IOFixedMultiply(value, CAST_DOUBLE_TO_FIXED(981));
    }
    return value;
}


#pragma mark - IOHIDGyroElementProcessor

OSDefineMetaClassAndStructors(IOHIDGyroElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDGyroElementProcessor>
IOHIDGyroElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDGyroElementProcessor> me = OSMakeShared<IOHIDGyroElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDGyroElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;
    int reportID = -1;
    unsigned int sampleCount = 0;

    _x = OSArray::withCapacity(1);
    _y = OSArray::withCapacity(1);
    _z = OSArray::withCapacity(1);
    _ts = OSArray::withCapacity(1);

    assert(collection);
    require_quiet(collection->getUsagePage() == kHIDPage_Sensor, exit);
    require_quiet(collection->getUsage() == kHIDUsage_Snsr_Motion_Gyrometer3D || collection->getUsage() == kHIDUsage_Snsr_Motion, exit);
    require_action_quiet(collection->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type:%d", collection->getUsagePage()));
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypePhysical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    for (unsigned int i = 0, count = children->getCount(); i < count; ++i) {
        IOHIDElement * element = OSRequiredCast(IOHIDElement, children->getObject(i));

        // only consider input elements
        if (element->getType() != kIOHIDElementTypeInput_Misc) {
            continue;
        }

        // all input elements should be in the same report
        if (reportID == -1) {
            reportID = element->getReportID();
        }
        else if (element->getReportID() != reportID) {
            continue;
        }

        switch(HIDUsagePair(element->getUsagePage(), element->getUsage())) {
            case HIDUsagePair(kHIDPage_Sensor, kHIDUsage_Snsr_Data_Motion_AngularVelocityXAxis):
                _x->setObject(element);
                break;
            case HIDUsagePair(kHIDPage_Sensor, kHIDUsage_Snsr_Data_Motion_AngularVelocityYAxis):
                _y->setObject(element);
                break;
            case HIDUsagePair(kHIDPage_Sensor, kHIDUsage_Snsr_Data_Motion_AngularVelocityZAxis):
                _z->setObject(element);
                break;
            case HIDUsagePair(kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp):
                _ts->setObject(element);
                break;
            default:
                break;
        }
    }

    sampleCount = _x->getCount();
    require_action_quiet(sampleCount > 0, exit, HIDElementProcessorLogError("got no complete (x,y,z,ts) tuples"));
    require_action_quiet(sampleCount == _y->getCount(), exit, HIDElementProcessorLogError("x(%d),y(%d) count mismatch", sampleCount, _y->getCount()));
    require_action_quiet(sampleCount == _z->getCount(), exit, HIDElementProcessorLogError("x(%d),z(%d) count mismatch", sampleCount, _z->getCount()));
    require_action_quiet(sampleCount == _ts->getCount(), exit, HIDElementProcessorLogError("x(%d),ts(%d) count mismatch", sampleCount, _ts->getCount()));
    require_action_quiet(reportID >= 0, exit, HIDElementProcessorLogError("bad report id:%d", reportID));

    // features
    _reportInterval = copyElement(children, kIOHIDElementTypeFeature, kHIDPage_Sensor, kHIDUsage_Snsr_Property_ReportInterval);
    require_action_quiet(_reportInterval.get(), exit, HIDElementProcessorLogError("missing report interval element"));

    _sampleInterval = copyElement(children, kIOHIDElementTypeFeature, kHIDPage_Sensor, kHIDUsage_Snsr_Property_SamplingRate);

    success = super::init(owner, reportID, kIOHIDEventTypeGyro, kHIDPage_Sensor, kHIDUsage_Snsr_Motion_Gyrometer3D);
    if (success) {
        bool ok = owner->setProperty("SupportsGyroEvents", kOSBooleanTrue);
        assert(ok);
    }

exit:
    return success;
}

void
IOHIDGyroElementProcessor::setProperty(OSString * key, OSObject * val)
{
    OSNumber * num = nullptr;
    IOHIDElement * element = nullptr;
    assert(key);
    assert(val);

    if (key->isEqualTo(kIOHIDSensorPropertyReportIntervalKey)) {
        num = OSDynamicCast(OSNumber, val);
        element = _reportInterval.get();
    }
    else if (key->isEqualTo(kIOHIDSensorPropertySampleIntervalKey)) {
        num = OSDynamicCast(OSNumber, val);
        element = _sampleInterval.get();
    }

    if (element && num) {
        element->setValue(num->unsigned32BitValue(), kIOHIDValueOptionsUpdateElementValues);
    }

    super::setProperty(key, val);
}

OSSharedPtr<IOHIDEvent>
IOHIDGyroElementProcessor::createEvent(uint64_t timestamp)
{
    OSSharedPtr<IOHIDEvent> event = nullptr;
    const unsigned int sampleCount = _x->getCount();

    event = OSSharedPtr<IOHIDEvent>(IOHIDEvent::collectionEvent(timestamp, getUsagePage(), getUsage(), false), OSNoRetain);
    assert(event);

    for (unsigned int i = 0; i < sampleCount; ++i) {
        OSSharedPtr<IOHIDEvent> subevent = eventForSample(timestamp, i);
        assert(subevent);

        event->appendChild(subevent.get());
    }

    return event;
}

OSSharedPtr<IOHIDEvent>
IOHIDGyroElementProcessor::eventForSample(uint64_t timestamp, unsigned int i) const
{
    IOFixed x = getXAngularVelocity(i);
    IOFixed y = getYAngularVelocity(i);
    IOFixed z = getZAngularVelocity(i);
    OSData * ts = OSRequiredCast(IOHIDElement, _ts->getObject(i))->getDataValue();

    IOHIDEvent * event = IOHIDEvent::gyroEvent(timestamp, x, y, z);
    assert(event);

    IOHIDEvent * child = IOHIDEvent::vendorDefinedEvent(timestamp, kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp, 0, (UInt8 *)ts->getBytesNoCopy(), ts->getLength());
    assert(child);

    event->appendChild(child);
    OSSafeReleaseNULL(child);

    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}

IOFixed
IOHIDGyroElementProcessor::getXAngularVelocity(unsigned int i) const
{
    return getAngularVelocityValue(OSRequiredCast(IOHIDElement, _x->getObject(i)));
}

IOFixed
IOHIDGyroElementProcessor::getYAngularVelocity(unsigned int i) const
{
    return getAngularVelocityValue(OSRequiredCast(IOHIDElement, _y->getObject(i)));
}

IOFixed
IOHIDGyroElementProcessor::getZAngularVelocity(unsigned int i) const
{
    return getAngularVelocityValue(OSRequiredCast(IOHIDElement, _z->getObject(i)));
}

IOFixed
IOHIDGyroElementProcessor::getAngularVelocityValue(IOHIDElement * element)
{
    return element->getScaledFixedValue(kIOHIDValueScaleTypeExponent);
}


#pragma mark - IOHIDThumbstickElementProcessor

OSDefineMetaClassAndStructors(IOHIDThumbstickElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDThumbstickElementProcessor>
IOHIDThumbstickElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDThumbstickElementProcessor> me = OSMakeShared<IOHIDThumbstickElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDThumbstickElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;

    assert(collection);

    require_quiet(collection->getUsagePage() == kHIDPage_GenericDesktop, exit);
    require_quiet(collection->getUsage() == kHIDUsage_GD_Thumbstick, exit);
    require_action_quiet(collection->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type:%d", collection->getUsagePage()));
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypePhysical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    if (children->getCount() == 1) {
        IOHIDElement * child = OSRequiredCast(IOHIDElement, children->getObject(0));
        require_action_quiet(child->getUsagePage() == kHIDPage_Ordinal, exit, HIDElementProcessorLogError("unexpected page for child:%d", child->getUsagePage()));
        require_action_quiet(child->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type for child:%d", child->getUsagePage()));
        require_action_quiet(child->getCollectionType() == kIOHIDElementCollectionTypeLogical, exit, HIDElementProcessorLogError("unexpected collection type for child:%d", child->getCollectionType()));

        _ordinal = child->getUsage();

        children = child->getChildElements();
        require_action_quiet(children, exit, HIDElementProcessorLogError("subcollection has no child elements"));
    }

    _x = copyElement(children, kIOHIDElementTypeInput_Misc, kHIDPage_GenericDesktop, kHIDUsage_GD_X);
    require_action_quiet(_x.get(), exit, HIDElementProcessorLogError("missing x-axis element"));

    _y = copyElement(children, kIOHIDElementTypeInput_Misc, kHIDPage_GenericDesktop, kHIDUsage_GD_Y);
    require_action_quiet(_y.get(), exit, HIDElementProcessorLogError("missing y-axis element"));

    require_action_quiet(_x->getReportID() == _y->getReportID(), exit, HIDElementProcessorLogError("x,y inputs do not have the same report id (%d/%d)", _x->getReportID(), _y->getReportID()));

    success = super::init(owner, _x->getReportID(), kIOHIDEventTypeMultiAxisPointer, kHIDPage_GenericDesktop, kHIDUsage_GD_Thumbstick);
    if (success) {
        bool ok = owner->setProperty("SupportsMultiAxisPointerEvents", kOSBooleanTrue);
        assert(ok);
    }

exit:
    return success;
}

OSSharedPtr<IOHIDEvent>
IOHIDThumbstickElementProcessor::createEvent(uint64_t timestamp)
{
    IOFixed x = getXAxisValue();
    IOFixed y = getYAxisValue();

    IOHIDEvent * event = IOHIDEvent::multiAxisPointerEvent(timestamp, x, y, 0, 0, 0, 0, 0);
    if (getOrdinal()) {
        IOHIDEvent * child = IOHIDEvent::vendorDefinedEvent(timestamp, kHIDPage_Ordinal, getOrdinal(), 0, nullptr, 0);
        assert(child);

        event->appendChild(child);
        OSSafeReleaseNULL(child);
    }
    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}

IOFixed
IOHIDThumbstickElementProcessor::getXAxisValue() const
{
    return _x->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
}

IOFixed
IOHIDThumbstickElementProcessor::getYAxisValue() const
{
    return _y->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
}

UInt16
IOHIDThumbstickElementProcessor::getOrdinal() const
{
    return _ordinal;
}


#pragma mark - IOHIDButtonElementProcessor

OSDefineMetaClassAndStructors(IOHIDButtonElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDButtonElementProcessor>
IOHIDButtonElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDButtonElementProcessor> me = OSMakeShared<IOHIDButtonElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool IOHIDButtonElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;
    OSSharedPtr<OSString> key = OSString::withCString("MultiBit");

    assert(collection);

    require_quiet(collection->getUsagePage() == kHIDPage_Button, exit);
    require_quiet(collection->getType() == kIOHIDElementTypeCollection, exit);
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypePhysical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    _input = copyElement(children, kIOHIDElementTypeInput_Button, kHIDPage_Button);
    if (_input) {
        // MC element
        require_action_quiet(_input->getReportSize() == 1, exit, HIDElementProcessorLogError("unexpected report size:%d for button %u", _input->getReportSize(), collection->getUsage()));
        require_action_quiet(_input->getLogicalMin() == 0, exit, HIDElementProcessorLogError("unexpected logical min:%d for button %u", _input->getLogicalMin(), collection->getUsage()));
        require_action_quiet(_input->getLogicalMax() == 1, exit, HIDElementProcessorLogError("unexpected logical max:%d for button %u", _input->getLogicalMax(), collection->getUsage()));

        setProperty(key.get(), kOSBooleanFalse);
    }
    else {
        // SV element
        _input = copyElement(children, kIOHIDElementTypeInput_Misc, kHIDPage_Button);
        require_action_quiet(_input, exit, HIDElementProcessorLog("missing input element for button %u", collection->getUsage()));
        require_action_quiet(_input->getPhysicalMin() == 0, exit, HIDElementProcessorLogError("unexpected physical min:%d for button %u", _input->getPhysicalMin(), collection->getUsage()));
        require_action_quiet(_input->getPhysicalMax() == 1, exit, HIDElementProcessorLogError("unexpected physical max:%d for button %u", _input->getPhysicalMax(), collection->getUsage()));

        setProperty(key.get(), kOSBooleanTrue);
    }

    _pressThreshold = getPressThreshold();
    _releaseThreshold = getReleaseThreshold();

    success = super::init(owner, _input->getReportID(), kIOHIDEventTypeButton, kHIDPage_Button, collection->getUsage());
    if (success) {
        bool ok = owner->setProperty("SupportsButtonEvents", kOSBooleanTrue);
        assert(ok);
    }

exit:
    return success;
}

void
IOHIDButtonElementProcessor::setProperty(OSString * key, OSObject * val)
{
    OSNumber * num = OSDynamicCast(OSNumber, val);

    if (key && key->isEqualTo("ButtonPressThreshold")) {
        require_quiet(num, exit);
        require_quiet(num->doubleValue() >= _input->getPhysicalMin(), exit);
        require_quiet(num->doubleValue() <= _input->getPhysicalMax(), exit);
        require_action_quiet(num->doubleValue() >= CAST_FIXED_TO_DOUBLE(_releaseThreshold), exit, HIDElementProcessorLogError("cannot set press threshold (%f) lower than release (%f)", num->doubleValue(), CAST_FIXED_TO_DOUBLE(_releaseThreshold)));
        _pressThreshold = CAST_DOUBLE_TO_FIXED(num->doubleValue());
    }
    else if (key && key->isEqualTo("ButtonReleaseThreshold")) {
        require_quiet(num, exit);
        require_quiet(num->doubleValue() >= _input->getPhysicalMin(), exit);
        require_quiet(num->doubleValue() <= _input->getPhysicalMax(), exit);
        require_action_quiet(num->doubleValue() <= CAST_FIXED_TO_DOUBLE(_pressThreshold), exit, HIDElementProcessorLogError("cannot set release threshold (%f) higher than press (%f)", num->doubleValue(), CAST_FIXED_TO_DOUBLE(_pressThreshold)));
        _releaseThreshold = CAST_DOUBLE_TO_FIXED(num->doubleValue());
    }

    super::setProperty(key, val);

exit:
    return;
}

OSSharedPtr<IOHIDEvent>
IOHIDButtonElementProcessor::createEvent(uint64_t timestamp)
{
    IOFixed pressure = getButtonPressure();
    IOHIDEvent * event = IOHIDEvent::buttonEvent(timestamp, 0, getButtonIdentifier(), pressure);
    assert(event);

    updateButtonState(pressure);
    event->setIntegerValue(kIOHIDEventFieldButtonState, getButtonState());

    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}

uint16_t
IOHIDButtonElementProcessor::getButtonIdentifier() const
{
    return _input->getUsage();
}

bool
IOHIDButtonElementProcessor::isDigitalButton() const
{
    return _input->getReportSize() == 1;
}

bool
IOHIDButtonElementProcessor::getButtonState() const
{
    return _state;
}

IOFixed
IOHIDButtonElementProcessor::getButtonPressure() const
{
    return _input->getScaledFixedValue(kIOHIDValueScaleTypePhysical);
}

void
IOHIDButtonElementProcessor::updateButtonState(IOFixed pressure)
{
    if (!_state && pressure >= _pressThreshold) {
        _state = true;
    }
    else if (_state && pressure < _releaseThreshold) {
        _state = false;
    }
}

IOFixed
IOHIDButtonElementProcessor::getPressThreshold() const
{
    // TODO: <rdar://148054441> Query this threshold from device.
    return CAST_DOUBLE_TO_FIXED(0.5);
}

IOFixed
IOHIDButtonElementProcessor::getReleaseThreshold() const
{
    // TODO: <rdar://148054441> Query this threshold from device.
    return CAST_DOUBLE_TO_FIXED(0.4);
}

#pragma mark - IOHIDForceSensorElementProcessor

OSDefineMetaClassAndStructors(IOHIDForceSensorElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDForceSensorElementProcessor>
IOHIDForceSensorElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDForceSensorElementProcessor> me = OSMakeShared<IOHIDForceSensorElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDForceSensorElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;

    assert(collection);

    require_quiet(collection->getUsagePage() == kHIDPage_Sensor, exit);
    require_quiet(collection->getUsage() == kHIDUsage_Snsr_Mechanical_Force, exit);
    require_action_quiet(collection->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type:%d", collection->getUsagePage()));
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypePhysical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    // inputs
    _force = copyElement(children, kIOHIDElementTypeInput_Misc, kHIDPage_Sensor, kHIDUsage_Snsr_Data_Mechanical_Force);
    require_action_quiet(_force.get(), exit, HIDElementProcessorLogError("missing force element"));

    success = super::init(owner, _force->getReportID(), kIOHIDEventTypeVendorDefined, kHIDPage_Sensor, kHIDUsage_Snsr_Mechanical_Force);
    if (success) {
        bool ok = owner->setProperty("SupportsForceEvents", kOSBooleanTrue);
        assert(ok);
    }

exit:
    return success;
}

OSSharedPtr<IOHIDEvent>
IOHIDForceSensorElementProcessor::createEvent(uint64_t timestamp)
{
    double force = getForce();

    IOHIDEvent * event = IOHIDEvent::vendorDefinedEvent(timestamp, kHIDPage_Sensor, kHIDUsage_Snsr_Data_Mechanical_Force, 0, (UInt8 *)&force, sizeof(force));
    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}

double
IOHIDForceSensorElementProcessor::getForce() const
{
    IOFixed force = _force->getScaledFixedValue(kIOHIDValueScaleTypeExponent);
    return CAST_FIXED_TO_DOUBLE(force);
}


#pragma mark - IOHIDProximityElementProcessor

OSDefineMetaClassAndStructors(IOHIDProximityElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDProximityElementProcessor>
IOHIDProximityElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDProximityElementProcessor> me = OSMakeShared<IOHIDProximityElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDProximityElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;

    assert(collection);

    require_quiet(collection->getUsagePage() == kHIDPage_Sensor, exit);
    require_quiet(collection->getUsage() == kHIDUsage_Snsr_Biometric_HumanProximity, exit);
    require_action_quiet(collection->getType() == kIOHIDElementTypeCollection, exit, HIDElementProcessorLogError("unexpected element type:%d", collection->getUsagePage()));
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypePhysical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    _touch = copyElement(children, kIOHIDElementTypeInput_Button, kHIDPage_Sensor, kHIDUsage_Snsr_Data_Biometric_HumanTouchState);
    require_action_quiet(_touch.get(), exit, HIDElementProcessorLog("missing touch element"));

    _prox = copyElement(children, kIOHIDElementTypeInput_Misc, kHIDPage_Sensor, kHIDUsage_Snsr_Data_Biometric_HumanProximityRange);

    if (_prox) {
        require_action_quiet(_touch->getReportID() == _prox->getReportID(), exit, HIDElementProcessorLogError("touch,prox inputs do not have the same report id (%d/%d)", _touch->getReportID(), _prox->getReportID()));
    }

    success = super::init(owner, _touch->getReportID(), kIOHIDEventTypeProximity, kHIDPage_Sensor, _prox ? kHIDUsage_Snsr_Biometric_HumanProximity : kHIDUsage_Snsr_Biometric_HumanTouch);
    if (success) {
        bool ok = owner->setProperty("SupportsProximityEvents", kOSBooleanTrue);
        assert(ok);
    }

exit:
    return success;
}

OSSharedPtr<IOHIDEvent>
IOHIDProximityElementProcessor::createEvent(uint64_t timestamp)
{
    bool touched = getTouchState();
    uint32_t level = getProximityRange();

    IOHIDEvent * event = IOHIDEvent::proximityEvent(timestamp, touched ? kIOHIDProximityDetectionFingerTouch : 0, level);
    return OSSharedPtr<IOHIDEvent>(event, OSNoRetain);
}

bool
IOHIDProximityElementProcessor::getTouchState() const
{
    return _touch->getValue();
}

uint32_t
IOHIDProximityElementProcessor::getProximityRange() const
{
    return _prox ? _prox->getValue() : 0;
}


#pragma mark - IOHIDLEDConstellationElementProcessor

OSDefineMetaClassAndStructors(IOHIDLEDConstellationElementProcessor, IOHIDElementProcessor);

OSSharedPtr<IOHIDLEDConstellationElementProcessor>
IOHIDLEDConstellationElementProcessor::create(IOService * owner, IOHIDElement * collection)
{
    OSSharedPtr<IOHIDLEDConstellationElementProcessor> me = OSMakeShared<IOHIDLEDConstellationElementProcessor>();
    assert(me);

    bool ok = me->init(owner, collection);
    if (!ok) {
        me.reset();
    }
    return me;
}

bool
IOHIDLEDConstellationElementProcessor::init(IOService * owner, IOHIDElement * collection)
{
    bool success = false;
    OSArray * children = nullptr;
    OSSharedPtr<IOHIDElement> modeCollection;
    OSArray * modeCollectionChildren = nullptr;

    assert(collection);

    require_quiet(collection->getUsagePage() == kHIDPage_AppleVendorLED, exit);
    require_quiet(collection->getUsage() == kHIDUsage_AppleVendorLED_Constellation, exit);
    require_quiet(collection->getType() == kIOHIDElementTypeCollection, exit);
    require_action_quiet(collection->getCollectionType() == kIOHIDElementCollectionTypeLogical, exit, HIDElementProcessorLogError("unexpected collection type:%d", collection->getCollectionType()));

    children = collection->getChildElements();
    require_action_quiet(children, exit, HIDElementProcessorLogError("collection has no child elements"));

    modeCollection = copyElement(children, kIOHIDElementTypeCollection, kHIDPage_LEDs, kHIDUsage_LED_UsageMultiModeIndicator);
    require_action_quiet(modeCollection->getCollectionType() == kIOHIDElementCollectionTypeUsageModifier, exit, HIDElementProcessorLogError("unexpected collection type for mode:%d", modeCollection->getCollectionType()));

    modeCollectionChildren = modeCollection->getChildElements();
    require_action_quiet(modeCollectionChildren, exit, HIDElementProcessorLogError("missing mode selector elements"));

    _modeOn = copyElement(modeCollectionChildren, kIOHIDElementTypeOutput, kHIDPage_LEDs, kHIDUsage_LED_IndicatorOn);
    require_action_quiet(_modeOn, exit, HIDElementProcessorLogError("missing On Mode element"));

    _modeOff = copyElement(modeCollectionChildren, kIOHIDElementTypeOutput, kHIDPage_LEDs, kHIDUsage_LED_IndicatorOff);
    require_action_quiet(_modeOff, exit, HIDElementProcessorLogError("missing Off Mode element"));

    _modeBlink = copyElement(modeCollectionChildren, kIOHIDElementTypeOutput, kHIDPage_LEDs, kHIDUsage_LED_IndicatorFastBlink);
    require_action_quiet(_modeBlink, exit, HIDElementProcessorLogError("missing Fast Blink Mode element"));

    _intensity = copyElement(children, kIOHIDElementTypeOutput, kHIDPage_LEDs, kHIDUsage_LED_LEDIntensity);
    require_action_quiet(_intensity.get(), exit, HIDElementProcessorLogError("missing intensity element"));
    require_action_quiet(_intensity->getReportID() == _modeOn->getReportID(), exit, HIDElementProcessorLogError("mode, intensity elements do not have the same report id (%d/%d)", _modeOn->getReportID(), _intensity->getReportID()));

    _blinkOnTime = copyElement(children, kIOHIDElementTypeOutput, kHIDPage_LEDs, kHIDUsage_LED_FastBlinkOnTime);
    require_action_quiet(_blinkOnTime.get(), exit, HIDElementProcessorLogError("missing fast blink on time element"));
    require_action_quiet(_blinkOnTime->getReportID() == _modeOn->getReportID(), exit, HIDElementProcessorLogError("mode, blink on time elements do not have the same report id (%d/%d)", _modeOn->getReportID(), _blinkOnTime->getReportID()));

    _blinkOffTime = copyElement(children, kIOHIDElementTypeOutput, kHIDPage_LEDs, kHIDUsage_LED_FastBlinkOffTime);
    require_action_quiet(_blinkOffTime.get(), exit, HIDElementProcessorLogError("missing fast blink off time element"));
    require_action_quiet(_blinkOffTime->getReportID() == _modeOn->getReportID(), exit, HIDElementProcessorLogError("mode, blink off time elements do not have the same report id (%d/%d)", _modeOn->getReportID(), _blinkOffTime->getReportID()));

    _ts = copyElement(children, kIOHIDElementTypeOutput, kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp);
    require_action_quiet(_ts.get(), exit, HIDElementProcessorLogError("missing time-sync timestamp element"));
    require_action_quiet(_ts->getReportID() == _modeOn->getReportID(), exit, HIDElementProcessorLogError("mode, time-sync elements do not have the same report id (%d/%d)", _modeOn->getReportID(), _ts->getReportID()));

    success = super::init(owner, 0, 0, kHIDPage_AppleVendorLED, kHIDUsage_AppleVendorLED_Constellation);
    if (success) {
        const OSObject * elements[] = { _modeOn.get(), _modeOff.get(), _modeBlink.get(), _intensity.get(), _blinkOnTime.get(), _blinkOffTime.get(), _ts.get() };
        const UInt32 count = sizeof(elements) / sizeof(elements[0]);
        OSSharedPtr<OSArray> array = OSArray::withObjects(elements, count);

        bool ok = owner->setProperty("SupportsLEDConstellation", kOSBooleanTrue);
        assert(ok);
        ok = owner->setProperty("LEDConstellationElements", array.get());
        assert(ok);
    }

exit:
    return success;
}

OSSharedPtr<IOHIDEvent>
IOHIDLEDConstellationElementProcessor::createEvent(uint64_t timestamp)
{
    return nullptr;
}
