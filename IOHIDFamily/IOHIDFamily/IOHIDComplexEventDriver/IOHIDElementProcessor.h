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

#pragma once

#include <libkern/c++/OSObject.h>
#include <IOKit/IOTypes.h>
#include <IOKit/hid/IOHIDDeviceTypes.h>
#include <IOKit/hid/IOHIDEventTypes.h>

class IOService;
class IOHIDEvent;
class IOHIDElement;

/// `IOHIDElementProcessor` is an abstract base class for an object which transforms HID input
/// report elements into HID events.
///
class IOHIDElementProcessor : public OSObject
{
    OSDeclareAbstractStructors(IOHIDElementProcessor);
    using super = OSObject;

protected:

    /// Subclasses should call this method during init.
    ///
    bool init(IOService * owner, UInt8 reportID, IOHIDEventType type, UInt32 page, UInt32 usage);

public:

    /// Element processor factory function type.
    ///
    /// @param  owner
    ///     `IOService` creating the element processor.
    ///
    /// @param  collection
    ///     Collection element from which to create the processor.
    ///
    typedef OSPtr<IOHIDElementProcessor>(*Factory)(IOService * owner, IOHIDElement * collection);

    /// Recursively run the event generator and its children.
    ///
    /// @param  timestamp
    ///     Timestamp of the input report being processed.
    ///
    /// @param  reportID
    ///     ID of the report being processed.
    ///
    /// @return
    ///     HID event produced by the input report, or `NULL` if no event is produced.
    ///
    virtual OSPtr<IOHIDEvent> processInput(uint64_t timestamp, uint8_t reportID);

    /// Set a property on the element processor.
    ///
    /// The default implementation maintains a simple property dictionary. This method provides no
    /// synchronization and the caller must synchronize all calls against the input report handling
    /// context.
    ///
    /// @param  key
    ///     The property key to set.
    ///
    /// @param  val
    ///     The property value to set.
    ///
    virtual void setProperty(OSString * key, OSObject * val);

    /// See OSObject.h
    virtual bool serialize(OSSerialize * serializer) const APPLE_KEXT_OVERRIDE;

    /// Append an event generator to the list of children. Must not already be a child of this
    /// object.
    ///
    /// @param  child
    ///     Child element processor.
    ///
    void appendChild(IOHIDElementProcessor * child);

    /// Append multiple event generators to the list of children. Must not already be children of
    /// this object.
    ///
    /// @param  child
    ///     Array of `IOHIDElementProcessor` objects.
    ///
    void appendChildren(OSArray * children);

    /// Set the processor's cookie.
    void setCookie(uint32_t cookie) { _cookie = cookie; }

protected:

    /// Create a new HID event from the generator's elements.
    ///
    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) = 0;

    /// Get the HID report ID of the generator's elements.
    ///
    uint8_t getReportID() const { return _reportID; };
    uint32_t getCookie() const { return _cookie; }
    IOHIDEventType getType() const { return _type; }
    uint32_t getUsagePage() const { return _page; }
    uint32_t getUsage() const { return _usage; }

    /// Copy the element with the given type, usage page, and usage in an array.
    ///
    /// This method asserts if `elements` contains any type besides `IOHIDElement`.
    ///
    /// @return
    ///     The matching element.
    ///
    static OSPtr<IOHIDElement> copyElement(OSArray * elements, IOHIDElementType type, uint16_t page, uint16_t usage = 0);

private:

    bool isParentOf(IOHIDElementProcessor * child);

    IOService * _owner;
    uint8_t _reportID;
    IOHIDEventType _type;
    uint32_t _cookie;
    uint32_t _page;
    uint32_t _usage;
    OSPtr<OSArray> _children;
    OSPtr<OSDictionary> _properties;
};


class IOHIDRootElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDRootElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDRootElementProcessor> create(IOService * owner, IOHIDElement * collection);
    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);
};


class IOHIDAccelElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDAccelElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDAccelElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual void setProperty(OSString * key, OSObject * val) APPLE_KEXT_OVERRIDE;
    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);
    OSPtr<IOHIDEvent> eventForSample(uint64_t timestamp, unsigned int i) const;
    IOFixed getXAcceleration(unsigned int i) const;
    IOFixed getYAcceleration(unsigned int i) const;
    IOFixed getZAcceleration(unsigned int i) const;

    static IOFixed getAccelerationValue(IOHIDElement * element);

    OSPtr<OSArray> _x;
    OSPtr<OSArray> _y;
    OSPtr<OSArray> _z;
    OSPtr<OSArray> _ts;
    OSPtr<IOHIDElement> _reportInterval;
    OSPtr<IOHIDElement> _sampleInterval;
};

class IOHIDGyroElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDGyroElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDGyroElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual void setProperty(OSString * key, OSObject * val) APPLE_KEXT_OVERRIDE;
    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);
    OSPtr<IOHIDEvent> eventForSample(uint64_t timestamp, unsigned int i) const;
    IOFixed getXAngularVelocity(unsigned int i) const;
    IOFixed getYAngularVelocity(unsigned int i) const;
    IOFixed getZAngularVelocity(unsigned int i) const;

    static IOFixed getAngularVelocityValue(IOHIDElement * element);

    OSPtr<OSArray> _x;
    OSPtr<OSArray> _y;
    OSPtr<OSArray> _z;
    OSPtr<OSArray> _ts;
    OSPtr<IOHIDElement> _reportInterval;
    OSPtr<IOHIDElement> _sampleInterval;
};

class IOHIDThumbstickElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDThumbstickElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDThumbstickElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);
    IOFixed getXAxisValue() const;
    IOFixed getYAxisValue() const;
    UInt16 getOrdinal() const;

    uint16_t _ordinal;
    OSPtr<IOHIDElement> _x;
    OSPtr<IOHIDElement> _y;
};

class IOHIDButtonElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDButtonElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDButtonElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual void setProperty(OSString * key, OSObject * val) APPLE_KEXT_OVERRIDE;
    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);

    uint16_t getButtonIdentifier() const;
    bool isDigitalButton() const;
    bool getButtonState() const;
    IOFixed getButtonPressure() const;

    void updateButtonState(IOFixed pressure);
    IOFixed getPressThreshold() const;
    IOFixed getReleaseThreshold() const;

    OSPtr<IOHIDElement> _input;
    IOFixed _pressThreshold;
    IOFixed _releaseThreshold;
    bool _state;
};

class IOHIDForceSensorElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDForceSensorElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDForceSensorElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);
    double getForce() const;

    OSPtr<IOHIDElement> _force;
};


class IOHIDProximityElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDProximityElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDProximityElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);

    bool getTouchState() const;
    uint32_t getProximityRange() const;

    OSPtr<IOHIDElement> _touch;
    OSPtr<IOHIDElement> _prox;
};

class IOHIDLEDConstellationElementProcessor : public IOHIDElementProcessor
{
    OSDeclareDefaultStructors(IOHIDLEDConstellationElementProcessor);
    using super = IOHIDElementProcessor;

public:

    static OSPtr<IOHIDLEDConstellationElementProcessor> create(IOService * owner, IOHIDElement * collection);

    virtual OSPtr<IOHIDEvent> createEvent(uint64_t timestamp) APPLE_KEXT_OVERRIDE;

private:

    bool init(IOService * owner, IOHIDElement * collection);

    OSPtr<IOHIDElement> _modeOn;
    OSPtr<IOHIDElement> _modeOff;
    OSPtr<IOHIDElement> _modeBlink;
    OSPtr<IOHIDElement> _intensity;
    OSPtr<IOHIDElement> _blinkOnTime;
    OSPtr<IOHIDElement> _blinkOffTime;
    OSPtr<IOHIDElement> _ts;
};
