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

#include "IOHIDComplexEventDriver.h"
#include "IOHIDElementProcessor.h"
#include "IOHIDKeys.h"
#include "IOHIDPrivateKeys.h"
#include "../IOHIDDebug.h"
#include "IOHIDFamilyTrace.h"
#include <AssertMacros.h>

OSDefineMetaClassAndStructors(IOHIDComplexEventDriver, IOHIDEventService);

/// Create an `OSArray` from an `OSObject`.
///
/// If the provided object is an array, it will be returned with +1 retain count. If it is not an
/// array, a new array containing the object will be returned.
///
static OSSharedPtr<OSArray> arrayFromObject(const OSObject * object) {
    return OSDynamicCast(OSArray, object) ? OSSharedPtr<OSArray>(OSDynamicCast(OSArray, object), OSRetain) : OSArray::withObjects(&object, 1);
}

bool
IOHIDComplexEventDriver::handleStart(IOService * provider)
{
    bool started = false;
    bool ok = false;
    bool opened = false;
    bool success = false;
    IOReturn ret;
    OSSharedPtr<OSArray> eventTypes = NULL;

    started = super::handleStart(provider);
    require_action_quiet(started, exit, HIDServiceLogError("handleStart: super::handleStart failed"));

    _interface = OSSharedPtr<IOHIDInterface>(OSDynamicCast(IOHIDInterface, provider), OSRetain);
    require_action_quiet(_interface, exit, HIDServiceLogError("handleStart: unexpected provider type %s", provider->getMetaClass()->getClassName()));

    _elements = OSSharedPtr<OSArray>(_interface->createMatchingElements(), OSNoRetain);
    require_action_quiet(_elements && _elements->getCount() > 0, exit, HIDServiceLogError("handleStart: failed to get elements from IOHIDInterface"));

    initProcessors();
    require_action_quiet(_processors && _processors->getCount() > 1, exit, HIDServiceLogError("handleStart: failed to create any input processors"));

    _rootProcessor = OSSharedPtr<IOHIDElementProcessor>(OSRequiredCast(IOHIDRootElementProcessor, _processors->getObject(0)), OSNoRetain);
    ok = setProperty("ElementProcessors", _rootProcessor.get());
    require_action_quiet(ok, exit, HIDServiceLogError("handleStart: set ElementProcessors property failed"));

    _workloop = OSSharedPtr<IOWorkLoop>(getWorkLoop(), OSRetain);
    assert(_workloop);

    _gate = IOCommandGate::commandGate(this);
    assert(_gate);

    ret = _workloop->addEventSource(_gate.get());
    require_noerr_action_quiet(ret, exit, HIDServiceLogError("handleStart: addEventSource failed (0x%x)", ret));

    opened = _interface->open(this, 0, OSMemberFunctionCast(IOHIDInterface::InterruptReportAction, this, &IOHIDComplexEventDriver::handleInterruptReport), nullptr);
    require_action_quiet(opened, exit, HIDServiceLogError("handleStart: failed to open provider"));

    success = true;

exit:
    return success;
}

void
IOHIDComplexEventDriver::free()
{
    if (_workloop && _gate) {
        _workloop->removeEventSource(_gate.get());
    }
}

bool
IOHIDComplexEventDriver::didTerminate(IOService * provider, IOOptionBits options, bool * defer)
{
    if (_interface) {
        _interface->close(this);
    }
    return super::didTerminate(provider, options, defer);
}

OSArray *
IOHIDComplexEventDriver::getReportElements()
{
    return _elements.get();
}

IOReturn
IOHIDComplexEventDriver::setProperties(OSObject * properties)
{
    IOReturn ret = kIOReturnInvalid;
    OSDictionary * dict = OSDynamicCast(OSDictionary, properties);
    require_action_quiet(dict, exit, ret = kIOReturnBadArgument);

    if (dict->getObject(kIOHIDProcessorPropertyAccessKey) != nullptr) {
        OSSharedPtr<OSArray> requests = arrayFromObject(dict->getObject(kIOHIDProcessorPropertyAccessKey));
        assert(requests);

        // validate all requests first, bail out if any are malformed
        for (unsigned int i = 0; i < requests->getCount(); ++i) {
            require_action_quiet(isValidProcessorPropertyRequest(requests->getObject(i)), exit, ret = kIOReturnBadArgument);
        }

        ret = dispatchWorkloopSync(^IOReturn{
            for (unsigned int i = 0; i < requests->getCount(); ++i) {
                handleSetProcessorPropertyGated(requests->getObject(i));
            }
            return kIOReturnSuccess;
        });
    }

    ret = super::setProperties(properties);

exit:
    return ret;
}

IOHIDElementProcessor *
IOHIDComplexEventDriver::getProcessor(unsigned int cookie) const
{
    IOHIDElementProcessor * processor = nullptr;
    if (_processors->getObject(cookie)) {
        processor = OSRequiredCast(IOHIDElementProcessor, _processors->getObject(cookie));
    }
    return processor;
}

bool
IOHIDComplexEventDriver::isValidProcessorPropertyRequest(OSObject * object)
{
    bool valid = false;
    OSDictionary * request = OSDynamicCast(OSDictionary, object);
    OSNumber * cookie = nullptr;

    require_action_quiet(request, exit,
                         HIDServiceLogError("isValidProcessorPropertyRequest: request is not dictionary"));
    require_action_quiet(request->getCount() == 3, exit,
                         HIDServiceLogError("isValidProcessorPropertyRequest: request has %d keys (expected 3)", request->getCount()));

    require_action_quiet(cookie = OSDynamicCast(OSNumber, request->getObject(kIOHIDProcessorID)), exit,
                         HIDServiceLogError("isValidProcessorPropertyRequest: missing key kIOHIDProcessorID"));
    require_action_quiet(getProcessor(cookie->unsigned32BitValue()), exit,
                         HIDServiceLogError("isValidProcessorPropertyRequest: unknown processor:%u", cookie->unsigned32BitValue()));

    require_action_quiet(OSDynamicCast(OSString, request->getObject(kIOHIDProcessorPropertyKey)), exit,
                         HIDServiceLogError("isValidProcessorPropertyRequest: missing key kIOHIDProcessorPropertyKey"));
    require_action_quiet(request->getObject(kIOHIDProcessorPropertyValue), exit,
                         HIDServiceLogError("isValidProcessorPropertyRequest: missing key kIOHIDProcessorPropertyValue"));

    valid = true;

exit:
    return valid;
}

void
IOHIDComplexEventDriver::handleSetProcessorPropertyGated(OSObject * object)
{
    OSDictionary * request = OSRequiredCast(OSDictionary, object);
    OSNumber * cookie = OSRequiredCast(OSNumber, request->getObject(kIOHIDProcessorID));
    OSString * key = OSRequiredCast(OSString, request->getObject(kIOHIDProcessorPropertyKey));
    OSObject * val = OSRequiredCast(OSNumber, request->getObject(kIOHIDProcessorPropertyValue));

    IOHIDElementProcessor * processor = getProcessor(cookie->unsigned32BitValue());
    assert(processor);

    processor->setProperty(key, val);
}

void
IOHIDComplexEventDriver::handleInterruptReport(AbsoluteTime timestamp, IOMemoryDescriptor * report __unused, IOHIDReportType type, UInt32 reportID)
{
    if (readyForReports() && type == kIOHIDReportTypeInput) {
        IOHID_DEBUG(kIOHIDDebugCode_CmplxEvtDrv_InterruptReport, timestamp, reportID, 0, 0);
        OSSharedPtr<IOHIDEvent> event = _rootProcessor->processInput(timestamp, reportID);
        if (event) {
            dispatchEvent(event.get());
        }
    }
}

void
IOHIDComplexEventDriver::initProcessors()
{
    _processors = OSArray::withCapacity(16);
    assert(_processors);

    parseCollection(OSRequiredCast(IOHIDElement, _elements->getObject(0)));
}

void
IOHIDComplexEventDriver::parseCollection(IOHIDElement * collection, IOHIDElementProcessor * parent)
{
    // Create element processor(s) associated with this collection.
    unsigned int count = parent ? createProcessors(collection, parent) : createRootProcessor(collection);

    // If at least one processor was created, recurse on all child collections.
    if (count > 0 && collection->getChildElements()) {
        OSArray * children = collection->getChildElements();
        parent = OSRequiredCast(IOHIDElementProcessor, _processors->getLastObject());
        for (unsigned int i = 0; i < children->getCount(); ++i) {
            IOHIDElement * element = OSRequiredCast(IOHIDElement, children->getObject(i));
            if (element->getType() == kIOHIDElementTypeCollection) {
                parseCollection(element, OSRequiredCast(IOHIDElementProcessor, parent));
            }
        }
    }
}

unsigned int
IOHIDComplexEventDriver::createProcessors(IOHIDElement * collection, IOHIDElementProcessor * parent)
{
    static const IOHIDElementProcessor::Factory factories[] = {
        (IOHIDElementProcessor::Factory)IOHIDAccelElementProcessor::create,
        (IOHIDElementProcessor::Factory)IOHIDGyroElementProcessor::create,
        (IOHIDElementProcessor::Factory)IOHIDProximityElementProcessor::create,
        (IOHIDElementProcessor::Factory)IOHIDThumbstickElementProcessor::create,
        (IOHIDElementProcessor::Factory)IOHIDButtonElementProcessor::create,
        (IOHIDElementProcessor::Factory)IOHIDForceSensorElementProcessor::create,
        (IOHIDElementProcessor::Factory)IOHIDLEDConstellationElementProcessor::create,
    };

    unsigned int count = 0;
    for (unsigned int i = 0; i < sizeof(factories)/sizeof(factories[0]); ++i) {
        OSSharedPtr<IOHIDElementProcessor> processor = factories[i](this, collection);
        if (processor) {
            ++count;
            processor->setCookie(_processors->getCount());
            bool ok = _processors->setObject(processor.get());
            assert(ok);
            if (parent) {
                parent->appendChild(processor.get());
            }
        }
    }

    return count;
}

IOReturn
IOHIDComplexEventDriver::dispatchWorkloopSync(IOEventSource::ActionBlock action)
{
    IOReturn ret = kIOReturnOffline;
    if (!isInactive()) {
        ret = _gate->runActionBlock(^IOReturn{
            return isInactive() ? kIOReturnOffline : action();
        });
    }
    return ret;
}

unsigned int
IOHIDComplexEventDriver::createRootProcessor(IOHIDElement * collection)
{
    unsigned int count = 0;
    OSSharedPtr<IOHIDRootElementProcessor> processor = IOHIDRootElementProcessor::create(this, collection);
    if (processor) {
        ++count;
        processor->setCookie(_processors->getCount());
        bool ok = _processors->setObject(processor.get());
        assert(ok);
    }
    return count;
}
