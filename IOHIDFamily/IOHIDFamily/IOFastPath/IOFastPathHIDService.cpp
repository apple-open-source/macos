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

#include "IOHIDUsageTables.h"
#include "AppleHIDUsageTables.h"
#include "IOFastPathHIDService.h"
#include <IOKit/hid/IOHIDElement.h>
#include "IOHIDDevice.h"
#include "IOHIDElementPrivate.h"
#include <IOKit/hid/IOHIDEventData.h>
#include <AssertMacros.h>
#include <math.h>
#include "IOHIDTimeSyncService.h"
#include "IOHIDTimeSyncKeys.h"
#include <IOKit/IOKitKeys.h>
#include "../IOHIDDebug.h"
#include "IOHIDFamilyTrace.h"

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


#pragma mark - IOFastPathHIDService

OSDefineMetaClassAndAbstractStructors(IOFastPathHIDService, IOFastPathService);

bool
IOFastPathHIDService::start(IOService * provider)
{
    OSSharedPtr<OSObject> prop = nullptr;
    bool started = false;
    bool opened = false;
    bool success = false;

    _service = OSSharedPtr<IOHIDEventService>(OSRequiredCast(IOHIDEventService, provider), OSRetain);

    started = super::start(provider);
    require_action(started, exit, HIDServiceLogError("super::start failed"));

    prop = getProvider()->copyProperty(kIOHIDPhysicalDeviceUniqueIDKey, gIOServicePlane);
    if (prop) {
        setProperty(kIOHIDPhysicalDeviceUniqueIDKey, prop.get());
    }

    setupTimeSync();

    _sample = OSData::withCapacity(copyDescriptor()->getSampleSize());
    assert(_sample);
    _sample->appendBytes(NULL, copyDescriptor()->getSampleSize());

    opened = _service->open(this, 0, NULL, OSMemberFunctionCast(IOHIDEventService::Action, this, &IOFastPathHIDService::handleEvent));
    require_action(opened, exit, HIDServiceLogError("failed to open provider"));

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
IOFastPathHIDService::willTerminate(IOService * provider, IOOptionBits options)
{
    cleanupHelper();
    return super::willTerminate(provider, options);
}

void
IOFastPathHIDService::cleanupHelper()
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

    if (_service->isOpen(this)) {
        _service->close(this);
    }
}

OSSharedPtr<IOHIDEventService>
IOFastPathHIDService::copyService() const
{
    return _service;
}

OSSharedPtr<OSData>
IOFastPathHIDService::copySample() const
{
    return _sample;
}

bool
IOFastPathHIDService::supportsTimeSync() const
{
    OSSharedPtr<OSBoolean> prop = OSDynamicPtrCast<OSBoolean>(_service->copyProperty(kIOHIDTimeSyncEnabledKey, gIOServicePlane));
    return prop && prop == kOSBooleanTrue;
}

bool
IOFastPathHIDService::sharesHIDDeviceWith(IOHIDTimeSyncService * service) const
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
IOFastPathHIDService::setupTimeSync()
{
    if (!supportsTimeSync()) {
        return;
    }

    OSSharedPtr<OSDictionary> matching = serviceMatching("IOHIDTimeSyncService");
    assert(matching);

    _serviceMatchThread = thread_call_allocate_with_options(OSMemberFunctionCast(thread_call_func_t, this, &IOFastPathHIDService::timeSyncServiceMatchHandler),
                                                            this,
                                                            THREAD_CALL_PRIORITY_KERNEL,
                                                            THREAD_CALL_OPTIONS_ONCE);
    assert(_serviceMatchThread);

    IOServiceMatchingNotificationHandlerBlock handler = ^bool(IOService * newService, IONotifier * notifier) {
        if (OSDynamicCast(IOHIDTimeSyncService, newService) && sharesHIDDeviceWith(OSDynamicCast(IOHIDTimeSyncService, newService))) {
            os_atomic(UInt32) state = atomic_fetch_or(&_state, kStateTimeSyncMatched);
            require_quiet(!(state & kStateTimeSyncMatched), exit);

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
IOFastPathHIDService::timeSyncServiceMatchHandler(thread_call_param_t param __unused)
{
    require_quiet(!isInactive(), exit);

    getWorkLoop()->runActionBlock(^IOReturn{
        if (!isInactive()) {
            IOHIDTimeSyncService::EventHandler handler = ^(IOHIDTimeSyncService::Event event, IOHIDTimeSyncService::Precision precision) {
                HIDServiceLog("TimeSync event:%d (precision:%d)", event, precision);
                switch (event) {
                    case IOHIDTimeSyncService::Event::EventActive:
                        HIDServiceLog("%llu attempts to time-sync before active", tsNotActiveCnt);
                        atomic_fetch_or(&_state, kStateTimeSyncActive);
                        break;
                    case IOHIDTimeSyncService::Event::EventInactive:
                    case IOHIDTimeSyncService::Event::EventTerminating:
                        if (_state & kStateTimeSyncActive) {
                            HIDServiceLog("synced %llu remote, %llu local timestamps during session", tsToLocalCnt, tsToRemoteCnt);
                            tsNotActiveCnt = 0;
                            tsToLocalCnt = 0;
                            tsToRemoteCnt = 0;
                        }
                        atomic_fetch_and(&_state, ~kStateTimeSyncActive);
                        break;
                    default:
                        break;
                }
            };

            bool ok = _timeSync->open(this, handler);
            if (ok) {
                HIDServiceLog("time-sync service opened (%llu earlier attempts to time-sync failed)", tsNotOpenCnt);
            }
            else {
                HIDServiceLogError("IOHIDTimeSyncService::open failed");
            }

            atomic_fetch_or(&_state, kStateTimeSyncOpened);
        }
        return kIOReturnSuccess;
    });

exit:
    return;
}

IOReturn
IOFastPathHIDService::doTimeSyncForHIDEventGated(IOHIDEvent * event, UInt64 * outSyncedTime)
{
    IOReturn ret = kIOReturnInvalid;
    IOHIDEvent * child = nullptr;
    OSSharedPtr<OSData> tsTimestamp = nullptr;

    assert(getWorkLoop()->inGate());
    require_action_quiet(!isInactive(), exit, ret = kIOReturnOffline);
    require_action_quiet(_state & kStateTimeSyncOpened, exit, tsNotOpenCnt++; ret = kIOReturnNotReady);
    require_action_quiet(_state & kStateTimeSyncActive, exit, tsNotActiveCnt++; ret = kIOReturnNotReady);

    child = getMatchingChildEvent(event, kIOHIDEventTypeVendorDefined, kHIDPage_AppleVendorSensor, kHIDUsage_AppleVendorSensor_TimeSyncTimestamp);
    require_action_quiet(child, exit, ret = kIOReturnUnsupported); // no time sync child event

    tsTimestamp = OSData::withBytes(child->getDataValue(kIOHIDEventFieldVendorDefinedData), child->getIntegerValue(kIOHIDEventFieldVendorDefinedDataLength));
    assert(tsTimestamp);

    ret = _timeSync->toSyncedTime(tsTimestamp.get(), outSyncedTime);
    if (ret == kIOReturnSuccess) {
        ++tsToLocalCnt;
    }

exit:
    return ret;
}

IOReturn
IOFastPathHIDService::doTimeSyncForLocalTimeGated(UInt64 timestamp, OSData ** outTime)
{
    IOReturn ret = kIOReturnInvalid;

    assert(getWorkLoop()->inGate());
    require_action_quiet(!isInactive(), exit, ret = kIOReturnOffline);
    require_action_quiet(_state & kStateTimeSyncOpened, exit, tsNotOpenCnt++; ret = kIOReturnNotReady);
    require_action_quiet(_state & kStateTimeSyncActive, exit, tsNotActiveCnt++; ret = kIOReturnNotReady);

    ret = _timeSync->toTimeData(timestamp, outTime);
    if (ret == kIOReturnSuccess) {
        ++tsToRemoteCnt;
    }

exit:
    return ret;
}


#pragma mark - IOFastPathAccelHIDService

OSDefineMetaClassAndStructors(IOFastPathHIDAccelService, IOFastPathHIDService);

bool
IOFastPathHIDAccelService::start(IOService * provider)
{
    bool ok = super::start(provider);
    require(ok, exit);

    setName("accel");
    registerService();

exit:
    return ok;
}

OSSharedPtr<IOFastPathDescriptor>
IOFastPathHIDAccelService::createDescriptor()
{
    OSSharedPtr<OSArray> fields = OSArray::withCapacity(6);
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyTimestamp, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, eventTimestamp), sizeof(QueueEntry::eventTimestamp)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeySampleTimestamp, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, sampleTimestamp), sizeof(QueueEntry::sampleTimestamp)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeySampleID, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, sampleID), sizeof(QueueEntry::sampleID)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyAccelX, kIOFastPathFieldTypeDouble, offsetof(QueueEntry, x), sizeof(QueueEntry::x)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyAccelY, kIOFastPathFieldTypeDouble, offsetof(QueueEntry, y), sizeof(QueueEntry::y)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyAccelZ, kIOFastPathFieldTypeDouble, offsetof(QueueEntry, z), sizeof(QueueEntry::z)));

    return IOFastPathDescriptor::create(fields.get());
}

void
IOFastPathHIDAccelService::handleEvent(IOHIDEventService * sender, void * context, IOHIDEvent * event, IOOptionBits options)
{
    switch (event->getType()) {
        case kIOHIDEventTypeAccelerometer:
            // base case: handle an accelerometer event
            handleAccelerometerEvent(event);
            break;
        case kIOHIDEventTypeCollection:
            // recursively handle all children
            for (unsigned int i = 0; event->getChildren() && i < event->getChildren()->getCount(); ++i) {
                IOHIDEvent * subevent = OSRequiredCast(IOHIDEvent, event->getChildren()->getObject(i));
                handleEvent(sender, context, subevent, options);
            }
            break;
        default:
            // do nothing
            break;
    }

    return;
}

void
IOFastPathHIDAccelService::handleAccelerometerEvent(IOHIDEvent * event)
{
    QueueEntry * entry = (QueueEntry *)copySample()->getBytesNoCopy();
    parseSampleFromHIDEvent(event, entry);

    IOHID_DEBUG(kIOHIDDebugCode_IOFastPath_EnqueueSample, event->getTimeStamp(), entry->sampleTimestamp, entry->sampleID, kIOHIDEventTypeAccelerometer);

    IOReturn ret = IOCircularDataQueueEnqueue(getQueue(), entry, copySample()->getLength());
    if (ret != kIOReturnSuccess) {
        HIDServiceLogError("IOCircularDataQueueEnqueue:0x%x", ret);
    }
}

void
IOFastPathHIDAccelService::parseSampleFromHIDEvent(IOHIDEvent * event, QueueEntry * sample)
{
    IOReturn ret = kIOReturnInvalid;
    UInt64 synced = 0;

    *sample = (QueueEntry) {
        .eventTimestamp = event->getTimeStamp(),
        .sampleTimestamp = 0,
        .sampleID = generation++,
        .x = event->getDoubleValue(kIOHIDEventFieldAccelerometerX, 0),
        .y = event->getDoubleValue(kIOHIDEventFieldAccelerometerY, 0),
        .z = event->getDoubleValue(kIOHIDEventFieldAccelerometerZ, 0),
    };

    ret = doTimeSyncForHIDEventGated(event, &synced);
    require_quiet(ret != kIOReturnNotReady, exit); // too chatty before TS becomes available to log
    require_noerr_action(ret, exit, HIDServiceLogError("toSyncedTime: 0x%x", ret));

    sample->sampleTimestamp = synced;

exit:
    return;
}


#pragma mark - IOFastPathGyroHIDService

OSDefineMetaClassAndStructors(IOFastPathHIDGyroService, IOFastPathHIDService);

bool
IOFastPathHIDGyroService::start(IOService * provider)
{
    bool ok = super::start(provider);
    require(ok, exit);

    setName("gyro");
    registerService();

exit:
    return ok;
}

OSSharedPtr<IOFastPathDescriptor>
IOFastPathHIDGyroService::createDescriptor()
{
    OSSharedPtr<OSArray> fields = OSArray::withCapacity(6);
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyTimestamp, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, eventTimestamp), sizeof(QueueEntry::eventTimestamp)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeySampleTimestamp, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, sampleTimestamp), sizeof(QueueEntry::sampleTimestamp)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeySampleID, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, sampleID), sizeof(QueueEntry::sampleID)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyGyroX, kIOFastPathFieldTypeDouble, offsetof(QueueEntry, x), sizeof(QueueEntry::x)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyGyroY, kIOFastPathFieldTypeDouble, offsetof(QueueEntry, y), sizeof(QueueEntry::y)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyGyroZ, kIOFastPathFieldTypeDouble, offsetof(QueueEntry, z), sizeof(QueueEntry::z)));

    return IOFastPathDescriptor::create(fields.get());
}

void
IOFastPathHIDGyroService::handleEvent(IOHIDEventService * sender, void * context, IOHIDEvent * event, IOOptionBits options)
{
    switch (event->getType()) {
        case kIOHIDEventTypeGyro:
            // base case: handle a gyro event
            handleGyroEvent(event);
            break;
        case kIOHIDEventTypeCollection:
            // recursively handle all children
            for (unsigned int i = 0; event->getChildren() && i < event->getChildren()->getCount(); ++i) {
                IOHIDEvent * subevent = OSRequiredCast(IOHIDEvent, event->getChildren()->getObject(i));
                handleEvent(sender, context, subevent, options);
            }
            break;
        default:
            // do nothing
            break;
    }

    return;
}

void
IOFastPathHIDGyroService::handleGyroEvent(IOHIDEvent * event)
{
    QueueEntry * entry = (QueueEntry *)copySample()->getBytesNoCopy();
    parseSampleFromHIDEvent(event, entry);

    IOHID_DEBUG(kIOHIDDebugCode_IOFastPath_EnqueueSample, event->getTimeStamp(), entry->sampleTimestamp, entry->sampleID, kIOHIDEventTypeGyro);

    IOReturn ret = IOCircularDataQueueEnqueue(getQueue(), entry, copySample()->getLength());
    if (ret != kIOReturnSuccess) {
        HIDServiceLogError("IOCircularDataQueueEnqueue:0x%x", ret);
    }
}

void
IOFastPathHIDGyroService::parseSampleFromHIDEvent(IOHIDEvent * event, QueueEntry * sample)
{
    IOReturn ret = kIOReturnInvalid;
    UInt64 synced = 0;

    *sample = (QueueEntry) {
        .eventTimestamp = event->getTimeStamp(),
        .sampleTimestamp = 0,
        .sampleID = generation++,
        .x = event->getDoubleValue(kIOHIDEventFieldGyroX, 0),
        .y = event->getDoubleValue(kIOHIDEventFieldGyroY, 0),
        .z = event->getDoubleValue(kIOHIDEventFieldGyroZ, 0),
    };

    ret = doTimeSyncForHIDEventGated(event, &synced);
    require_quiet(ret != kIOReturnNotReady, exit); // too chatty before TS becomes available to log
    require_noerr_action(ret, exit, HIDServiceLogError("toSyncedTime: 0x%x", ret));

    sample->sampleTimestamp = synced;

exit:
    return;
}


#pragma mark - IOFastPathLEDHIDService

OSDefineMetaClassAndStructors(IOFastPathHIDLEDService, IOFastPathHIDService);

IOService *
IOFastPathHIDLEDService::probe(IOService * provider, SInt32 * score)
{
    OSSharedPtr<OSArray> elements = OSDynamicPtrCast<OSArray>(provider->copyProperty("LEDConstellationElements"));
    if (!elements) {
        return nullptr;
    }

    bool parsed = parseElements(elements.get());
    if (!parsed) {
        return nullptr;
    }

    return super::probe(provider, score);
}

bool
IOFastPathHIDLEDService::start(IOService * provider)
{
    IOReturn ret = kIOReturnSuccess;
    IOHIDInterface * interface;
    IOHIDDevice * device;
    bool ok = false;

    require(super::start(provider), exit);

    interface = OSDynamicCast(IOHIDInterface, provider->getProvider());
    require_quiet(interface, exit);

    device = OSDynamicCast(IOHIDDevice, interface->getProvider());
    require_quiet(device, exit);

    _device = OSSharedPtr<IOHIDDevice>(device, OSRetain);
    assert(_device);

    // <rdar://143504169> Explore a signaling mechanism for handling queue data, rather than a timer.
    _timer = IOTimerEventSource::timerEventSource(kIOTimerEventSourceOptionsDefault, this, OSMemberFunctionCast(IOTimerEventSource::Action, this, &IOFastPathHIDLEDService::timerCallback));
    assert(_timer);

    ret = getWorkLoop()->addEventSource(_timer.get());
    assert(ret == kIOReturnSuccess);

    setName("leds");
    registerService();

    ok = true;

exit:
    return ok;
}

void
IOFastPathHIDLEDService::stop(IOService * provider)
{
    _timer->cancelTimeout();
    getWorkLoop()->removeEventSource(_timer.get());
    super::stop(provider);
}

UInt32
IOFastPathHIDLEDService::getTimerPeriodUS() const
{
    // <rdar://143504169> Query this value from the HID device.
    return 7500;
}

bool
IOFastPathHIDLEDService::handleOpen(IOService * forClient, IOOptionBits options, void * arg)
{
    bool ok = super::handleOpen(forClient, options, arg);
    if (ok) {
        _timer->setTimeoutUS(getTimerPeriodUS());
    }
    return ok;
}

void
IOFastPathHIDLEDService::handleClose(IOService * forClient, IOOptionBits options)
{
    _timer->cancelTimeout();
    return super::handleClose(forClient, options);
}

OSSharedPtr<IOFastPathDescriptor>
IOFastPathHIDLEDService::createDescriptor()
{
    OSSharedPtr<OSArray> fields = OSArray::withCapacity(5);
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyTimestamp, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, timestamp), sizeof(QueueEntry::timestamp)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyLEDMode, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, mode), sizeof(QueueEntry::mode)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyLEDIntensity, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, intensity), sizeof(QueueEntry::intensity)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyLEDBlinkDuration, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, blinkDuration), sizeof(QueueEntry::blinkDuration)));
    fields->setObject(IOFastPathField::create(kIOFastPathFieldKeyLEDBlinkPeriod, kIOFastPathFieldTypeInteger, offsetof(QueueEntry, blinkPeriod), sizeof(QueueEntry::blinkPeriod)));

    return IOFastPathDescriptor::create(fields.get());
}

void
IOFastPathHIDLEDService::handleEvent(IOHIDEventService * sender, void * context, IOHIDEvent * event, IOOptionBits options)
{
    return; // no-op
}

void
IOFastPathHIDLEDService::timerCallback(IOTimerEventSource * sender)
{
    IOReturn ret = kIOReturnInvalid;
    OSSharedPtr<OSData> sample = copySample();
    void * buf = const_cast<void *>(sample->getBytesNoCopy());
    size_t size = sample->getLength();
    LEDState newState;

    ret = IOCircularDataQueueCopyLatest(getQueue(), buf, &size);
    require_action_quiet(ret != kIOReturnUnderrun, exit, emptyQueueTimerCnt++);
    require_noerr_action_quiet(ret, exit, HIDServiceLogError("IOCircularDataQueueCopyLatest:0x%x", ret));

    if (!dequeuedSample) {
        HIDServiceLogDebug("%llu attempts to dequeue before first enqueue", emptyQueueTimerCnt);
        dequeuedSample = true;
    }

    newState = parseStateFromQueueEntry((QueueEntry *)buf);
    if (stateUpdateNeeded(newState)) {
        updateLEDState(newState);
    }

exit:
    _timer->setTimeoutUS(getTimerPeriodUS());
    return;
}

#define NUM_REQUIRED_LED_ELEMENTS 7

bool
IOFastPathHIDLEDService::parseElements(OSArray * elements)
{
    bool success = false;
    bool parsed = false;
    unsigned int numParsed = 0;
    require_quiet(elements, exit);

    for (unsigned int i = 0, count = elements->getCount(); i < count; ++i) {
        IOHIDElementPrivate * element = OSRequiredCast(IOHIDElementPrivate, elements->getObject(i));

        parsed = false;
        switch (element->getUsage()) {
            case kHIDUsage_LED_IndicatorOn:
                parsed = parseElement(element,
                                      kHIDPage_LEDs,
                                      kHIDUsage_LED_IndicatorOn,
                                      kIOHIDElementTypeOutput,
                                      1,
                                      &_modeOn);
                break;
            case kHIDUsage_LED_IndicatorOff:
                parsed = parseElement(element,
                                      kHIDPage_LEDs,
                                      kHIDUsage_LED_IndicatorOff,
                                      kIOHIDElementTypeOutput,
                                      1,
                                      &_modeOff);
                break;
            case kHIDUsage_LED_IndicatorFastBlink:
                parsed = parseElement(element,
                                      kHIDPage_LEDs,
                                      kHIDUsage_LED_IndicatorFastBlink,
                                      kIOHIDElementTypeOutput,
                                      1,
                                      &_modeBlink);
                break;
            case kHIDUsage_LED_LEDIntensity:
                parsed = parseElement(element,
                                      kHIDPage_LEDs,
                                      kHIDUsage_LED_LEDIntensity,
                                      kIOHIDElementTypeOutput,
                                      8,
                                      &_intensity);
                break;
            case kHIDUsage_LED_FastBlinkOnTime:
                parsed = parseElement(element,
                                      kHIDPage_LEDs,
                                      kHIDUsage_LED_FastBlinkOnTime,
                                      kIOHIDElementTypeOutput,
                                      16,
                                      &_blinkOnTime);
                break;
            case kHIDUsage_LED_FastBlinkOffTime:
                parsed = parseElement(element,
                                      kHIDPage_LEDs,
                                      kHIDUsage_LED_FastBlinkOffTime,
                                      kIOHIDElementTypeOutput,
                                      16,
                                      &_blinkOffTime);
                break;
            case kHIDUsage_AppleVendorSensor_TimeSyncTimestamp:
                parsed = parseElement(element,
                                      kHIDPage_AppleVendorSensor,
                                      kHIDUsage_AppleVendorSensor_TimeSyncTimestamp,
                                      kIOHIDElementTypeOutput,
                                      0, // do not restrict size
                                      &_ts);
            default:
                break;
        }
        require_quiet(parsed, exit);
        ++numParsed;
    }
    require_quiet(numParsed == NUM_REQUIRED_LED_ELEMENTS, exit);

    success = true;

exit:
    return success;
}

bool
IOFastPathHIDLEDService::parseElement(IOHIDElementPrivate * element, UInt32 page, UInt32 usage, IOHIDElementType type, UInt32 bits, IOHIDElementPrivate ** output)
{
    assert(element);
    assert(output);
    bool success = false;

    require_quiet(element, exit);
    require_quiet(element->getUsagePage() == page, exit);
    require_quiet(element->getUsage() == usage, exit);
    require_quiet(element->getType() == type, exit);
    require_quiet(bits == 0 || element->getReportSize() == bits, exit);
    require_quiet(*output == nullptr, exit);

    *output = element;
    success = true;

exit:
    return success;
}

#define clamp(val, min, max) \
    val = ((val) < (min)) ? (min) : (val);  \
    val = ((val) > (max)) ? (max) : (val);

IOFastPathHIDLEDService::LEDState
IOFastPathHIDLEDService::parseStateFromQueueEntry(QueueEntry * entry) const
{
    UInt64 mode = entry->mode;
    clamp(mode, 0, UINT8_MAX);

    UInt64 intensity = entry->intensity;
    clamp(intensity, 0, UINT8_MAX);

    UInt64 duration = entry->blinkDuration;
    clamp(duration, 0, UINT16_MAX);

    UInt64 period = entry->blinkPeriod;
    clamp(period, duration, UINT16_MAX);

    return (LEDState) {
        .on = ((UInt8)mode == kLEDModeOn),
        .off = ((UInt8)mode == kLEDModeOff),
        .blink = ((UInt8)mode == kLEDModeBlink),
        .intensity = (UInt8)intensity,
        .blinkOnTime = (UInt16)duration,
        .blinkOffTime = (UInt16)(period - duration),
        .pulseMidpoint = entry->timestamp,
    };
}

bool
IOFastPathHIDLEDService::stateUpdateNeeded(LEDState newState)
{
    // <rdar://143504169> Deadband around control values to reduce number of set reports.
    return newState != _ledState;
}

void
IOFastPathHIDLEDService::updateLEDState(LEDState newState)
{
    assert(getWorkLoop()->inGate());

    OSSharedPtr<OSData> data = nullptr;
    IOHIDElementCookie cookies[] = {
        _modeOn->getCookie(),
        _modeOff->getCookie(),
        _modeBlink->getCookie(),
        _intensity->getCookie(),
        _blinkOnTime->getCookie(),
        _blinkOffTime->getCookie(),
        _ts->getCookie(),
    };
    UInt32 count = sizeof(cookies) / sizeof(cookies[0]);
    IOReturn ret = kIOReturnInvalid;
    OSData * ts = nullptr;

    // do timesync as first step; if unsuccessful, don't change anything
    ret = doTimeSyncForLocalTimeGated(newState.pulseMidpoint, &ts);
    require_noerr_action_quiet(ret, exit, HIDServiceLogError("doTimeSyncForLocalTimeGated:0x%x", ret));

    _ledState = newState;

    _ts->setDataBits(ts);

    data = OSData::withBytes(&_ledState.on, sizeof(LEDState::on));
    _modeOn->setDataBits(data.get());

    data = OSData::withBytes(&_ledState.off, sizeof(LEDState::off));
    _modeOff->setDataBits(data.get());

    data = OSData::withBytes(&_ledState.blink, sizeof(LEDState::blink));
    _modeBlink->setDataBits(data.get());

    data = OSData::withBytes(&_ledState.intensity, sizeof(LEDState::intensity));
    _intensity->setDataBits(data.get());

    data = OSData::withBytes(&_ledState.blinkOnTime, sizeof(LEDState::blinkOnTime));
    _blinkOnTime->setDataBits(data.get());

    data = OSData::withBytes(&_ledState.blinkOffTime, sizeof(LEDState::blinkOffTime));
    _blinkOffTime->setDataBits(data.get());

    ret = _device->postElementValues(cookies, count);
    require_noerr_action_quiet(ret, exit, HIDServiceLogError("setLEDOutputReport:0x%x", ret));

exit:
    OSSafeReleaseNULL(ts);
    return;
}
