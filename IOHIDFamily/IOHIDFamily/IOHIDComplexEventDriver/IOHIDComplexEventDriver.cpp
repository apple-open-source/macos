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
#include "IOHIDDevice.h"
#include "IOHIDTimeSyncService.h"
#include "IOHIDKeys.h"
#include "IOHIDPrivateKeys.h"
#include "AppleHIDUsageTables.h"
#include "../IOHIDDebug.h"
#include "IOHIDFamilyTrace.h"
#include <AssertMacros.h>

/// Helper method to get a child event matching a set of parameters.
///
/// @param  event
///     Parent event.
///
/// @param  type
///     Event type.
///
/// @param  page
///     If type is `kIOHIDEventTypeVendorDefined`, value of `kIOHIDEventFieldVendorDefinedUsagePage`
///     field. Pass 0 to match any value. Ignored for other event types.
///
/// @param  usage
///     If type is `kIOHIDEventTypeVendorDefined`, value of `kIOHIDEventFieldVendorDefinedUsage`
///     field. Pass 0 to match any value. Ignored for other event types.
///
/// @return
///     The first child event of `event` that matches, or `nullptr` if no match is found. The
///     returned object is not retained.
///
static IOHIDEvent * getMatchingChildEvent(IOHIDEvent * event, IOHIDEventType type, UInt32 page = 0, UInt32 usage = 0)
{
    IOHIDEvent * ret = nullptr;
    OSArray * children = event->getChildren();
    require_quiet(children, exit);

    for (unsigned int i = 0, count = children->getCount(); i < count; ++i) {
        IOHIDEvent * child = OSRequiredCast(IOHIDEvent, children->getObject(i));
        require_quiet(type == child->getType(), loop);
        if (type == kIOHIDEventTypeVendorDefined) {
            require_quiet(page == 0 || page == child->getIntegerValue(kIOHIDEventFieldVendorDefinedUsagePage), loop);
            require_quiet(usage == 0 || usage == child->getIntegerValue(kIOHIDEventFieldVendorDefinedUsage), loop);
        }
        else if (type == kIOHIDEventTypeCollection) {
            require_quiet(page == 0 || page == child->getIntegerValue(kIOHIDEventFieldCollectionUsagePage), loop);
            require_quiet(usage == 0 || usage == child->getIntegerValue(kIOHIDEventFieldCollectionUsage), loop);
        }
        ret = child;
        break;

    loop:
        continue;
    }

exit:
    return ret;
}

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

    started = super::handleStart(provider);
    require_action_quiet(started, exit, HIDServiceLogError("handleStart: super::handleStart failed"));

    _workloop = OSSharedPtr<IOWorkLoop>(getWorkLoop(), OSRetain);
    assert(_workloop);

    if (OSDynamicPtrCast<OSBoolean>(copyProperty(kIOHIDTimeSyncEnabledKey, gIOServicePlane)) == kOSBooleanTrue) {
        _timeSyncSupported = true;
        setupTimeSync();
    }

    _interface = OSSharedPtr<IOHIDInterface>(OSDynamicCast(IOHIDInterface, provider), OSRetain);
    require_action_quiet(_interface, exit, HIDServiceLogError("handleStart: unexpected provider type %s", provider->getMetaClass()->getClassName()));

    _elements = OSSharedPtr<OSArray>(_interface->createMatchingElements(), OSNoRetain);
    require_action_quiet(_elements && _elements->getCount() > 0, exit, HIDServiceLogError("handleStart: failed to get elements from IOHIDInterface"));

    initProcessors();
    require_action_quiet(_processors && _processors->getCount() > 1, exit, HIDServiceLogError("handleStart: failed to create any input processors"));

    _rootProcessor = OSSharedPtr<IOHIDElementProcessor>(OSRequiredCast(IOHIDRootElementProcessor, _processors->getObject(0)), OSRetain);
    ok = setProperty("ElementProcessors", _rootProcessor.get());
    require_action_quiet(ok, exit, HIDServiceLogError("handleStart: set ElementProcessors property failed"));

    opened = _interface->open(this, 0, OSMemberFunctionCast(IOHIDInterface::InterruptReportAction, this, &IOHIDComplexEventDriver::handleInterruptReport), nullptr);
    require_action_quiet(opened, exit, HIDServiceLogError("handleStart: failed to open provider"));

    success = true;

exit:
    if (!success) {
        cleanupHelper();
        if (started) {
            super::stop(provider);
        }
    }
    return success;
}

bool
IOHIDComplexEventDriver::willTerminate(IOService * provider, IOOptionBits options)
{
    cleanupHelper();
    return super::willTerminate(provider, options);
}

void
IOHIDComplexEventDriver::cleanupHelper()
{
    // remove notifier first, to ensure no thread call is not entered again
    if (_notifier) {
        _notifier->remove();
    }

    // cancel thread call, to ensure the match callback is not in-progress
    if (_serviceMatchThread) {
        thread_call_cancel_wait(_serviceMatchThread);
        thread_call_free(_serviceMatchThread);
    }

    if (_timeSync && _timeSync->isOpen(this)) {
        _timeSync->close(this);
    }

    if (_interface && _interface->isOpen(this)) {
        _interface->close(this);
    }
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
            if (_timeSyncSupported) {
                performTimeSync(event.get());
            }
            dispatchEvent(event.get());
        }
    }
}

IOReturn
IOHIDComplexEventDriver::convertDeviceToMachTimestamp(OSData * timestamp, UInt64 * outTime)
{
    return dispatchWorkloopSync(^IOReturn{
        IOReturn ret = kIOReturnInvalid;
        require_action_quiet(_timeSyncSupported, exit, ret = kIOReturnUnsupported);
        require_action_quiet(_timeSyncState & kTimeSyncStateOpened, exit, ret = kIOReturnNotReady; _debug.ts.notOpenCnt++);
        require_action_quiet(_timeSyncState & kTimeSyncStateActive, exit, ret = kIOReturnNotReady; _debug.ts.notActiveCnt++);

        ret = _timeSync->toSyncedTime(timestamp, outTime);
        if (ret == kIOReturnSuccess) {
            ++_debug.ts.toLocalCnt;
        }

    exit:
        return ret;
    });
}

IOReturn
IOHIDComplexEventDriver::convertMachToDeviceTimestamp(AbsoluteTime timestamp, OSData ** outTime)
{
    return dispatchWorkloopSync(^IOReturn{
        IOReturn ret = kIOReturnInvalid;
        require_action_quiet(_timeSyncSupported, exit, ret = kIOReturnUnsupported);
        require_action_quiet(_timeSyncState & kTimeSyncStateOpened, exit, ret = kIOReturnNotReady; _debug.ts.notOpenCnt++);
        require_action_quiet(_timeSyncState & kTimeSyncStateActive, exit, ret = kIOReturnNotReady; _debug.ts.notActiveCnt++);

        ret = _timeSync->toTimeData(timestamp, outTime);
        if (ret == kIOReturnSuccess) {
            ++_debug.ts.toRemoteCnt;
        }

    exit:
        return ret;
    });
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
        ret = _workloop->runActionBlock(^IOReturn{
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

//MARK: Time-Sync Support

bool
IOHIDComplexEventDriver::sharesHIDDeviceWith(IOHIDTimeSyncService * service) const
{
    bool ret = false;
    IOService * provider = nullptr;
    IOHIDDevice * device = nullptr;

    provider = service->getProvider();
    while (provider) {
        device = OSDynamicCast(IOHIDDevice, provider);
        if (device) {
            break;
        }
        provider = provider->getProvider();
    }
    require_quiet(device, exit);

    provider = this->getProvider();
    while (provider) {
        if (device == OSDynamicCast(IOHIDDevice, provider)) {
            ret = true;
            break;
        }
        provider = provider->getProvider();
    }

exit:
    return ret;
}

void
IOHIDComplexEventDriver::setupTimeSync()
{
    OSSharedPtr<OSDictionary> matching = serviceMatching("IOHIDTimeSyncService");
    assert(matching);

    _serviceMatchThread = thread_call_allocate_with_options(OSMemberFunctionCast(thread_call_func_t,
                                                                                 this,
                                                                                 &IOHIDComplexEventDriver::timeSyncServiceMatchHandler),
                                                            this,
                                                            THREAD_CALL_PRIORITY_KERNEL,
                                                            THREAD_CALL_OPTIONS_ONCE);
    assert(_serviceMatchThread);

    IOServiceMatchingNotificationHandlerBlock handler = ^bool(IOService * newService, IONotifier * notifier) {
        if (OSDynamicCast(IOHIDTimeSyncService, newService) && sharesHIDDeviceWith(OSDynamicCast(IOHIDTimeSyncService, newService))) {
            os_atomic(UInt32) state = atomic_fetch_or(&_timeSyncState, kTimeSyncStateMatched);
            require_quiet(!(state & kTimeSyncStateMatched), exit);

            assert(!_timeSync);
            _timeSync = OSSharedPtr<IOHIDTimeSyncService>(OSDynamicCast(IOHIDTimeSyncService, newService), OSRetain);

            thread_call_enter(_serviceMatchThread);
            notifier->disable();
        }
    exit:
        return true;
    };

    IONotifier * n = addMatchingNotification(gIOFirstPublishNotification, matching.get(), 0, handler);
    _notifier = OSSharedPtr<IONotifier>(n, OSRetain);
    assert(_notifier);

    bool ok = setProperty(kIOHIDTimeSyncEnabledKey, kOSBooleanTrue);
    assert(ok);
}

void
IOHIDComplexEventDriver::timeSyncServiceMatchHandler(thread_call_param_t param __unused)
{
    dispatchWorkloopSync(^IOReturn{
        timeSyncServiceMatchHandlerGated();
        return kIOReturnSuccess;
    });
}

void
IOHIDComplexEventDriver::timeSyncServiceMatchHandlerGated()
{
    bool ok = _timeSync->open(this, ^(IOHIDTimeSyncService::Event event, IOHIDTimeSyncService::Precision precision) {
        HIDServiceLog("TimeSync event:%d (precision:%d)", event, precision);
        switch (event) {
            case IOHIDTimeSyncService::Event::EventActive:
                handleTimeSyncActive();
                break;
            case IOHIDTimeSyncService::Event::EventInactive:
            case IOHIDTimeSyncService::Event::EventTerminating:
                handleTimeSyncInactive();
                break;
            default:
                break;
        }
    });

    if (ok) {
        HIDServiceLog("time-sync service opened (%llu earlier attempts to time-sync failed)", _debug.ts.notOpenCnt);
    }
    else {
        HIDServiceLogError("IOHIDTimeSyncService::open failed");
    }

    atomic_fetch_or(&_timeSyncState, kTimeSyncStateOpened);
}

void IOHIDComplexEventDriver::handleTimeSyncActive()
{
    HIDServiceLog("%llu attempts to time-sync before active", _debug.ts.notActiveCnt);
    atomic_fetch_or(&_timeSyncState, kTimeSyncStateActive);
}

void IOHIDComplexEventDriver::handleTimeSyncInactive()
{
    os_atomic(UInt32) prevState = atomic_fetch_and(&_timeSyncState, ~kTimeSyncStateActive);
    if (prevState & kTimeSyncStateActive) {
        HIDServiceLog("synced %llu remote, %llu local timestamps during session", _debug.ts.toLocalCnt, _debug.ts.toRemoteCnt);
        _debug.ts.notActiveCnt = 0;
        _debug.ts.toLocalCnt = 0;
        _debug.ts.toRemoteCnt = 0;
    }
}

void
IOHIDComplexEventDriver::performTimeSync(IOHIDEvent * root)
{
    if (!_timeSyncSupported) {
        return;
    }

    dispatchWorkloopSync(^IOReturn{
        performTimeSyncGated(root);
        return kIOReturnSuccess;
    });
}

void
IOHIDComplexEventDriver::performTimeSyncGated(IOHIDEvent * event)
{
    // recursively (depth-first) perform time-sync on all child events
    OSArray * children = event->getChildren();
    const unsigned int nChildren = children ? children->getCount() : 0;
    for (unsigned int i = 0; i < nChildren; ++i) {
        performTimeSyncGated(OSRequiredCast(IOHIDEvent, children->getObject(i)));
    }

    // perform time-sync for this event, if applicable
    IOHIDEvent * child = getMatchingChildEvent(event, kIOHIDEventTypeVendorDefined, kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_DeviceTimestamp);
    if (!child) {
        return; // no matchng child event found, nothing to do
    }

    OSSharedPtr<OSData> tsTimestamp = OSData::withBytes(child->getDataValue(kIOHIDEventFieldVendorDefinedData), child->getIntegerValue(kIOHIDEventFieldVendorDefinedDataLength));
    assert(tsTimestamp);

    UInt64 synced = 0;
    IOReturn ret = convertDeviceToMachTimestamp(tsTimestamp.get(), &synced);

    IOHIDEvent * tsEvent = nullptr;
    switch (ret)
    {
        case kIOReturnSuccess:
            tsEvent = IOHIDEvent::vendorDefinedEvent(event->getTimeStamp(), kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp, 0, (UInt8 *)&synced, sizeof(synced));
            assert(tsEvent);

            event->appendChild(tsEvent);
            tsEvent->release();
            break;
        default:
            // intentionally empty
            break;
    }
}
