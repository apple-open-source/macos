#include <assert.h>
#include <AssertMacros.h>
#include <stdio.h>
#include <stdlib.h>
#include <DriverKit/DriverKit.h>
#include <DriverKit/OSCollections.h>
#include <DriverKit/IOService.h>
#include <DriverKit/IOBufferMemoryDescriptor.h>
#include <DriverKit/IOMemoryMap.h>
#include <HIDDriverKit/HIDDriverKit_Private.h>

#define DEFAULT_EVENT_SIZE 4096     // default buffer size for dispatching events
#define MAX_EVENT_SIZE 16384        // max event size that can be defined by user

#define MAX_REPORT_SIZE 16384       // max report size that can be defined by user

#define DEFAULT_POOL_SIZE 1         // number of report buffers to create
#define MAX_POOL_SIZE 10

typedef struct __attribute__((packed)) {
    uint32_t pointerType;
    uint32_t effect;
    uint64_t uniqueID;
} StylusExtendedData;

struct IOUserHIDEventService_IVars
{
    IOHIDInterface              *interface;
    OSDictionaryPtr             properties;
    OSArray                     *elements;
    OSAction                    *reportAction;
    uint32_t                    maxEventSize;
    
    OSDictionary                *reportPool;
    IOBufferMemoryDescriptor    *eventMD;
    void                        *eventMemory;
};

#define _interface              ivars->interface
#define _properties             ivars->properties
#define _elements               ivars->elements
#define _reportAction           ivars->reportAction
#define _maxEventSize           ivars->maxEventSize
#define _reportPool             ivars->reportPool
#define _eventMD                ivars->eventMD
#define _eventMemory            ivars->eventMemory

#undef super
#define super IOHIDEventService

bool IOUserHIDEventService::init()
{
    bool ret;
    
    ret = super::init();
    require_action(ret, exit, HIDLogError("init:%x", ret));
    
    assert(IOService::ivars);
    
    ivars = IONewZero(IOUserHIDEventService_IVars, 1);

exit:

    return ret;
}

void IOUserHIDEventService::free()
{
    if (ivars) {
        OSSafeReleaseNULL(_properties);
        OSSafeReleaseNULL(_elements);
        OSSafeReleaseNULL(_reportAction);
        OSSafeReleaseNULL(_reportPool);
        OSSafeReleaseNULL(_eventMD);
    }
    
    IOSafeDeleteNULL(ivars, IOUserHIDEventService_IVars, 1);
    super::free();
}

kern_return_t
IMPL(IOUserHIDEventService, Start)
{
    bool result = false;
    kern_return_t ret = kIOReturnError;
    uint64_t address;
    uint64_t length;
    uint64_t regID;
    
    provider->GetRegistryEntryID(&regID);
    HIDTrace(kHIDDK_ES_Start, getRegistryID(), regID, 0, 0);
    
    ret = Start(provider, SUPERDISPATCH);
    require_noerr_action(ret, exit, HIDServiceLogError("Start:%x", ret));
    
    provider->CopyProperties(&_properties);
    require(_properties, exit);
    
    _maxEventSize = OSDictionaryGetUInt64Value(_properties, kIOHIDMaxEventSizeKey);
    
    if (!_maxEventSize) {
        _maxEventSize = DEFAULT_EVENT_SIZE;
    }
    
    if (_maxEventSize > MAX_EVENT_SIZE) {
        _maxEventSize = MAX_EVENT_SIZE;
    }
    
    require_action(handleStart(provider), exit, HIDServiceLogError("HandleStart:%x", ret));

    _interface = OSDynamicCast(IOHIDInterface, provider);
    require_action(_interface, exit, HIDServiceLogError("Invalid provider"));
    
    _interface->retain();
    
    ret = CreateActionReportAvailable(
                           sizeof(uint64_t),
                           &_reportAction);
    require_noerr_action(ret, exit, HIDServiceLogError("CreateActionReportAvailable%x", ret));
    
    ret = IOHIDEventService::KernelStart(provider);
    require_noerr_action(ret, exit, HIDServiceLogError("IOHIDEventService::KernelStart:%x", ret));
    
    _elements = _interface->getElements();
    require(_elements, exit);
    
    _elements->retain();
    
    ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionInOut,
                                           _maxEventSize,
                                           IOVMPageSize,
                                           &_eventMD);
    require_noerr_action(ret, exit, HIDServiceLogError("BMD create: 0x%x", ret));
    
    SetEventMemory(_eventMD);
    
    ret = _eventMD->Map(0, 0, 0, 0, &address, &length);
    require_noerr(ret, exit);
    
    _eventMemory = (void *)address;
    
    require_action(createReportPool(), exit, HIDServiceLogError("Failed to create report pool"));
    
    ret = _interface->Open(this, 0, _reportAction);
    HIDServiceLog("Open interface: 0x%llx", regID);
    require_noerr_action(ret, exit, HIDServiceLogError("Open: 0x%x",ret));
    
    result = true;
    
exit:
    if (!result) {
        HIDServiceLogFault("Start failed: 0x%x", ret);
        ret = kIOReturnError;
    }
    
    return ret;
}

bool IOUserHIDEventService::createReportPool()
{
    bool result = false;
    IOReturn ret;
    uint64_t inputReportSize = 0;
    uint64_t usagePage = 0;
    uint32_t poolSize = 0;
    
    inputReportSize = OSDictionaryGetUInt64Value(_properties, kIOHIDMaxReportSizeKey);
    
    if (!inputReportSize) {
        inputReportSize = OSDictionaryGetUInt64Value(_properties, kIOHIDMaxInputReportSizeKey);
        require_action(inputReportSize, exit, HIDServiceLogError("Max input report size = 0"));
    }
    
    if (inputReportSize > MAX_REPORT_SIZE) {
        inputReportSize = MAX_REPORT_SIZE;
    }
    
    poolSize = OSDictionaryGetUInt64Value(_properties, kIOHIDReportPoolSizeKey);
    if (!poolSize) {
        poolSize = DEFAULT_POOL_SIZE;
    }
    
    usagePage = OSDictionaryGetUInt64Value(_properties, kIOHIDPrimaryUsagePageKey);
    
    // increase pool size for devices with high report rates
    if ((conformsTo(kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) ||
        usagePage == kHIDPage_Digitizer) &&
        poolSize == DEFAULT_POOL_SIZE) {
        poolSize++;
    }
    
    if (poolSize > MAX_POOL_SIZE) {
        poolSize = MAX_POOL_SIZE;
    }
    
    _reportPool = OSDictionary::withCapacity(poolSize);
    require(_reportPool, exit);
    
    for (uint32_t i = 0; i < poolSize; i++) {
        IOBufferMemoryDescriptor *report;
        IOMemoryMap *map;
        
        ret = IOBufferMemoryDescriptor::Create(kIOMemoryDirectionInOut,
                                               inputReportSize,
                                               IOVMPageSize,
                                               &report);
        require_noerr_action(ret, exit, HIDServiceLogError("BMD create: 0x%x", ret));
        
        ret = report->CreateMapping(/* options */   0,
								    /* address */   0,
								    /* offset */    0,
								    /* length */    0,
								    /* alignment */ 0,
								    &map);

        require_noerr_action(ret, exit, {
            OSSafeReleaseNULL(report);
            HIDServiceLogError("map: 0x%x", ret);
        });
        
        _reportPool->setObject(report, map);
        _interface->AddReportToPool(report);
        
        OSSafeReleaseNULL(report);
        OSSafeReleaseNULL(map);
    }
    
    result = true;
    
exit:
    return result;
}

void
IMPL(IOUserHIDEventService, ReportAvailable)
{
    HIDTraceStart(kHIDDK_ES_HandleReportCB, getRegistryID(), timestamp, reportID, 0);
    IOMemoryMap *map;
    
    require(_interface, exit);
    
    map = (IOMemoryMap *)_reportPool->getObject(report);
    require_action(map, exit, HIDServiceLogError("Failed to find map for report: %p", report));
    
    _interface->processReport(timestamp,
                              (uint8_t *)map->GetAddress(),
                              reportLength,
                              type,
                              reportID);
    
    handleReport(timestamp,
                 (uint8_t *)map->GetAddress(),
                 reportLength,
                 type,
                 reportID);
    
    _interface->AddReportToPool((IOBufferMemoryDescriptor *)report);
    
exit:
    HIDTraceEnd(kHIDDK_ES_HandleReportCB, getRegistryID(), timestamp, reportID, 0);
}

void IOUserHIDEventService::handleReport(uint64_t timestamp,
                                         uint8_t *report,
                                         uint32_t reportLength,
                                         IOHIDReportType type,
                                         uint32_t reportID)
{
    
}

bool IOUserHIDEventService::handleStart(IOService *provider)
{
    return true;
}

OSArray *IOUserHIDEventService::getElements()
{
    return _elements;
}

kern_return_t
IMPL(IOUserHIDEventService, Stop)
{
    uint64_t regID = 0;
    
    provider->GetRegistryEntryID(&regID);
    HIDTrace(kHIDDK_ES_Stop, getRegistryID(), regID, 0, 0);
    
    if (_interface) {
        IOReturn close = _interface->Close(this, 0);
        HIDServiceLog("Close interface: 0x%llx 0x%x", regID, close);
    }
    
    OSSafeReleaseNULL(_interface);
    
    if (_reportAction) {
        _reportAction->Cancel(^{
            Stop(provider, SUPERDISPATCH);
        });
    } else {
        Stop(provider, SUPERDISPATCH);
    }
    
    return kIOReturnSuccess;
}

bool IOUserHIDEventService::conformsTo(uint32_t usagePage, uint32_t usage)
{
    __block bool result = false;
    OSArray *usagePairs = NULL;
    
    require(_properties, exit);
    
    usagePairs = OSDynamicCast(OSArray, OSDictionaryGetValue(_properties, kIOHIDDeviceUsagePairsKey));
    require(usagePairs, exit);
    
    usagePairs->iterateObjects(^bool(OSObject *object) {
        OSDictionary *pair = OSDynamicCast(OSDictionary, object);
        OSNumber *up = NULL;
        OSNumber *u = NULL;
        
        if (!pair) {
            return false;
        }
        
        up = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsagePageKey));
        u = OSDynamicCast(OSNumber, pair->getObject(kIOHIDDeviceUsageKey));
        
        if (!up || !u) {
            return false;
        }
        
        if (up->unsigned32BitValue() == usagePage &&
            u->unsigned32BitValue() == usage) {
            result = true;
            return true;
        }
        
        return false;
    });
    
exit:
    return result;
}

void IOUserHIDEventService::dispatchEvent(IOHIDEvent *event)
{
    bzero(_eventMemory, _maxEventSize);
    event->readBytes(_eventMemory, event->getLength());
    EventAvailable(event->getLength());
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

kern_return_t IOUserHIDEventService::dispatchDigitizerStylusEvent(
                                        uint64_t timeStamp,
                                        IOHIDDigitizerStylusData *stylusData)
{
    IOReturn ret = kIOReturnError;
    uint32_t options = kIOHIDEventOptionIsAbsolute;
    uint32_t eventMask = 0;
    uint32_t buttonState = 0;
    IOHIDEvent *collection = NULL;
    IOHIDEvent *event = NULL;
    
    require_action(stylusData, exit, ret = kIOReturnBadArgument);
    
    collection = IOHIDEvent::withType(kIOHIDEventTypeDigitizer,
                                      timeStamp,
                                      0,
                                      kIOHIDEventOptionIsAbsolute);
    require(collection, exit);
    
    collection->setIntegerValue(kIOHIDEventFieldDigitizerCollection, true);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerType,
                                kIOHIDDigitizerCollectionTypeStylus);
    
    if (stylusData->invert) {
        options |= kIOHIDTransducerInvert;
    }
    
    event = IOHIDEvent::withType(kIOHIDEventTypeDigitizer, timeStamp, 0, options);
    require(event, exit);
    
    // Set the event mask
    if (stylusData->tipChanged) {
        eventMask |= kIOHIDDigitizerEventTouch;
    }
    
    if (stylusData->positionChanged) {
        eventMask |= kIOHIDDigitizerEventPosition;
    }
    
    if (stylusData->rangeChanged) {
        eventMask |= kIOHIDDigitizerEventRange;
    }
    
    setButtonState(&buttonState, 0, stylusData->tip);
    setButtonState(&buttonState, 1, stylusData->barrelSwitch);
    setButtonState(&buttonState, 2, stylusData->eraser);
    
    event->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);
    event->setIntegerValue(kIOHIDEventFieldDigitizerRange, stylusData->inRange);
    event->setIntegerValue(kIOHIDEventFieldDigitizerTouch, stylusData->tip);
    event->setIntegerValue(kIOHIDEventFieldDigitizerIndex, stylusData->identifier);
    event->setIntegerValue(kIOHIDEventFieldDigitizerType, kIOHIDDigitizerCollectionTypeStylus);
    event->setIntegerValue(kIOHIDEventFieldDigitizerButtonMask, buttonState);
    event->setFixedValue(kIOHIDEventFieldDigitizerX, stylusData->x);
    event->setFixedValue(kIOHIDEventFieldDigitizerY, stylusData->y);
    event->setFixedValue(kIOHIDEventFieldDigitizerPressure, stylusData->tipPressure);
    event->setFixedValue(kIOHIDEventFieldDigitizerAuxiliaryPressure, stylusData->barrelPressure);
    event->setFixedValue(kIOHIDEventFieldDigitizerTwist, stylusData->twist);
    event->setFixedValue(kIOHIDEventFieldDigitizerTiltX, stylusData->tiltX);
    event->setFixedValue(kIOHIDEventFieldDigitizerTiltY, stylusData->tiltY);
    
    collection->appendChild(event);
    
    if (stylusData->pointerType || stylusData->effect || stylusData->uniqueID) {
        IOHIDEvent *extendedData = NULL;
        StylusExtendedData data = {
            .pointerType = stylusData->pointerType,
            .effect = stylusData->effect,
            .uniqueID = stylusData->uniqueID
        };
        
        extendedData = IOHIDEvent::vendorDefinedEvent(timeStamp,
                                                      kHIDPage_AppleVendor,
                                                      kHIDUsage_AppleVendor_StylusData,
                                                      0,
                                                      (uint8_t *)&data,
                                                      sizeof(StylusExtendedData),
                                                      0);
        if (extendedData) {
            collection->appendChild(extendedData);
            extendedData->release();
        }
    }
    
    dispatchEvent(collection);
    ret = kIOReturnSuccess;
    
exit:
    OSSafeReleaseNULL(collection);
    OSSafeReleaseNULL(event);
    
    return ret;
}

kern_return_t IOUserHIDEventService::dispatchDigitizerTouchEvent(
                                            uint64_t timeStamp,
                                            IOHIDDigitizerTouchData *touchData,
                                            uint32_t touchDataCount)
{
    IOReturn ret = kIOReturnError;
    IOHIDEvent *collection = NULL;
    uint32_t collectionType = kIOHIDDigitizerCollectionTypeFinger;
    
    require_action(touchData && touchDataCount, exit, ret = kIOReturnBadArgument);
    
    collection = IOHIDEvent::withType(kIOHIDEventTypeDigitizer,
                                      timeStamp,
                                      0,
                                      kIOHIDEventOptionIsAbsolute);
    require(collection, exit);
    
    if (touchDataCount > 1) {
        collectionType = kIOHIDDigitizerCollectionTypeHand;
    }
    
    collection->setIntegerValue(kIOHIDEventFieldDigitizerCollection, true);
    collection->setIntegerValue(kIOHIDEventFieldDigitizerType, collectionType);
    
    for (unsigned int i = 0; i < touchDataCount; i++) {
        IOHIDEvent *event = NULL;
        uint32_t eventMask = 0;
        uint32_t buttonState = 0;
        IOHIDDigitizerTouchData data = touchData[i];
        
        event = IOHIDEvent::withType(kIOHIDEventTypeDigitizer,
                                     timeStamp,
                                     0,
                                     kIOHIDEventOptionIsAbsolute);
        require(event, exit);
        
        // Set the event mask
        if (data.touchChanged) {
            eventMask |= kIOHIDDigitizerEventTouch;
        }
        
        if (data.positionChanged) {
            eventMask |= kIOHIDDigitizerEventPosition;
        }
        
        if (data.rangeChanged) {
            eventMask |= kIOHIDDigitizerEventRange;
        }
        
        setButtonState(&buttonState, 0, data.touch);
        
        event->setIntegerValue(kIOHIDEventFieldDigitizerEventMask, eventMask);
        event->setIntegerValue(kIOHIDEventFieldDigitizerRange, data.inRange);
        event->setIntegerValue(kIOHIDEventFieldDigitizerTouch, data.touch);
        event->setIntegerValue(kIOHIDEventFieldDigitizerIndex, data.identifier);
        event->setIntegerValue(kIOHIDEventFieldDigitizerType, kIOHIDDigitizerCollectionTypeFinger);
        event->setIntegerValue(kIOHIDEventFieldDigitizerButtonMask, buttonState);
        event->setFixedValue(kIOHIDEventFieldDigitizerX, data.x);
        event->setFixedValue(kIOHIDEventFieldDigitizerY, data.y);
        
        collection->appendChild(event);
        event->release();
    }
    
    dispatchEvent(collection);
    ret = kIOReturnSuccess;
    
exit:
    OSSafeReleaseNULL(collection);
    
    return ret;
}
