/*
 *  IOHIDDFREventFilter.cpp
 *  IOHIDEventSystemPlugIns
 *
 *  Created by dekom on 08/16/2016.
 *  Copyright 2016 Apple Inc. All rights reserved.
 *
 */

#include "IOHIDDFREventFilter.hpp"
#include "IOHIDDebug.h"
#include "IOHIDProperties.h"
#include <new>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDSession.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include <mach/mach_time.h>

#define kKeyboardCancelThresholdMS      100
#define kDFRTouchCancelThresholdMS      100
#define kBioCancelThresholdMS           100

#define ABSOLUTE_TO_MS(abs)         ((abs) * sDFREventFilterTimebaseInfo.numer / sDFREventFilterTimebaseInfo.denom / NSEC_PER_MSEC)
#define DELTA_IN_MS(cur, prev)      ABSOLUTE_TO_MS(cur - prev)

static mach_timebase_info_data_t    sDFREventFilterTimebaseInfo;

// 4F2A35AF-A17D-4020-B62A-0E64B825F069
#define kIOHIDDFREventFilterFactor CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x4F, 0x2A, 0x35, 0xAF, 0xA1, 0x7D, 0x40, 0x20, 0xB6, 0x2A, 0x0E, 0x64, 0xB8, 0x25, 0xF0, 0x69)

extern "C" void * IOHIDDFREventFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);

//------------------------------------------------------------------------------
// IOHIDDFREventFilterFactory
//------------------------------------------------------------------------------
void *IOHIDDFREventFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDDFREventFilter), 0);
        return new(p) IOHIDDFREventFilter(kIOHIDDFREventFilterFactor);
    }
    return NULL;
}

// The IOHIDDFREventFilter function table.
IOHIDSessionFilterPlugInInterface IOHIDDFREventFilter::sIOHIDDFREventFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDDFREventFilter::QueryInterface,
    IOHIDDFREventFilter::AddRef,
    IOHIDDFREventFilter::Release,
    // IOHIDSimpleSessionFilterPlugInInterface functions
    IOHIDDFREventFilter::filter,
    NULL,
    NULL,
    // IOHIDSessionFilterPlugInInterface functions
    IOHIDDFREventFilter::open,
    IOHIDDFREventFilter::close,
    NULL,
    NULL,
    IOHIDDFREventFilter::registerService,
    IOHIDDFREventFilter::unregisterService,
    IOHIDDFREventFilter::scheduleWithDispatchQueue,
    IOHIDDFREventFilter::unscheduleFromDispatchQueue,
    IOHIDDFREventFilter::getPropertyForClient,
    IOHIDDFREventFilter::setPropertyForClient,
};

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::IOHIDDFREventFilter
//------------------------------------------------------------------------------
IOHIDDFREventFilter::IOHIDDFREventFilter(CFUUIDRef factoryID):
_sessionInterface(&sIOHIDDFREventFilterFtbl),
_factoryID(static_cast<CFUUIDRef>(CFRetain(factoryID))),
_refCount(1),
_dispatchQueue(0),
_keyboard(NULL),
_dfr(NULL),
_session(NULL),
_lastDFREvent(NULL),
_keyboardFilterEnabled(true),
_touchIDFilterEnabled(true),
_cancelledTouchInProgress(false),
_bioInProgress(false),
_cancel(false),
_lastDFREventTime(0),
_lastKeyboardEventTime(0),
_cancelledEventCount(0),
_keyboardCancelThresholdMS(kKeyboardCancelThresholdMS),
_dfrTouchCancelThresholdMS(kDFRTouchCancelThresholdMS),
_bioCancelThresholdMS(kBioCancelThresholdMS),
_eventCancelTimer(0)
{
    CFPlugInAddInstanceForFactory(factoryID);
    
    if (sDFREventFilterTimebaseInfo.denom == 0) {
        (void) mach_timebase_info(&sDFREventFilterTimebaseInfo);
    }
};

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::~IOHIDDFREventFilter
//------------------------------------------------------------------------------
IOHIDDFREventFilter::~IOHIDDFREventFilter()
{
    CFPlugInRemoveInstanceForFactory(_factoryID);
    CFRelease(_factoryID);
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDDFREventFilter::QueryInterface(void *self, REFIID iid, LPVOID *ppv)
{
    return static_cast<IOHIDDFREventFilter *>(self)->QueryInterface(iid, ppv);
}

HRESULT IOHIDDFREventFilter::QueryInterface(REFIID iid, LPVOID *ppv)
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes(NULL, iid);
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSimpleSessionFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDSessionFilterPlugInInterfaceID)) {
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    if (CFEqual(interfaceID, IUnknownUUID)) {
        // If the IUnknown interface was requested, same as above.
        AddRef();
        *ppv = this;
        CFRelease(interfaceID);
        return S_OK;
    }
    // Requested interface unknown, bail with error.
    *ppv = NULL;
    CFRelease(interfaceID);
    return E_NOINTERFACE;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDDFREventFilter::AddRef(void *self)
{
    return static_cast<IOHIDDFREventFilter *>(self)->AddRef();
}

ULONG IOHIDDFREventFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDDFREventFilter::Release(void *self)
{
    return static_cast<IOHIDDFREventFilter *>(self)->Release();
}

ULONG IOHIDDFREventFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::open
//------------------------------------------------------------------------------
boolean_t IOHIDDFREventFilter::open(void *self, IOHIDSessionRef session, IOOptionBits options)
{
    return static_cast<IOHIDDFREventFilter *>(self)->open(session, options);
}

boolean_t IOHIDDFREventFilter::open(IOHIDSessionRef session, IOOptionBits options __unused)
{
    _session = session;
    
    return true;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::close
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::close(void *self, IOHIDSessionRef session, IOOptionBits options)
{
    static_cast<IOHIDDFREventFilter *>(self)->close(session, options);
}

void IOHIDDFREventFilter::close(IOHIDSessionRef session __unused, IOOptionBits options __unused)
{
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::registerService
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::registerService(void *self, IOHIDServiceRef service)
{
    static_cast<IOHIDDFREventFilter *>(self)->registerService(service);
}

void IOHIDDFREventFilter::registerService(IOHIDServiceRef service)
{
    if (IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard)) {
        CFBooleanRef builtin = (CFBooleanRef)IOHIDServiceGetProperty(service, CFSTR(kIOHIDBuiltInKey));
        if (builtin == kCFBooleanTrue) {
            _keyboard = service;
        }
    } else if (IOHIDServiceConformsTo(service, kHIDPage_AppleVendor, kHIDUsage_AppleVendor_DFR)) {
        _dfr = service;
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::unregisterService
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::unregisterService(void *self, IOHIDServiceRef service)
{
    static_cast<IOHIDDFREventFilter *>(self)->unregisterService(service);
}

void IOHIDDFREventFilter::unregisterService(IOHIDServiceRef service)
{
    if (service == _keyboard) {
        _keyboard = NULL;
    } else if (service == _dfr) {
        _dfr = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::scheduleWithDispatchQueue(void *self, dispatch_queue_t queue)
{
    static_cast<IOHIDDFREventFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDDFREventFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _dispatchQueue = queue;
    
    _eventCancelTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _dispatchQueue);
    if (_eventCancelTimer != NULL) {
        dispatch_source_set_event_handler(_eventCancelTimer, ^(void) {
            dispatch_source_set_timer(_eventCancelTimer, DISPATCH_TIME_FOREVER, 0, 0);
            _cancel = false;
            if (_bioInProgress) {
                // We're not guaranteed a finger off event, so set this to false
                _bioInProgress = false;
            }
        });
        dispatch_source_set_timer(_eventCancelTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_eventCancelTimer);
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::unscheduleFromDispatchQueue(void *self, dispatch_queue_t queue)
{
    static_cast<IOHIDDFREventFilter *>(self)->unscheduleFromDispatchQueue(queue);
}

void IOHIDDFREventFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    if (_lastDFREvent) {
        CFRelease(_lastDFREvent);
        _lastDFREvent = NULL;
    }
    
    if (_eventCancelTimer) {
        dispatch_source_cancel(_eventCancelTimer);
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDDFREventFilter::filter(void *self, IOHIDServiceRef sender, IOHIDEventRef event)
{
    return static_cast<IOHIDDFREventFilter *>(self)->filter(sender, event);
}

IOHIDEventRef IOHIDDFREventFilter::filter(IOHIDServiceRef sender, IOHIDEventRef event)
{
    if (!event) {
        goto exit;
    }
    
    if (sender != _dfr && sender != _keyboard && IOHIDEventGetType(event) != kIOHIDEventTypeBiometric) {
        goto exit;
    }
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard) {
        handleKeyboardEvent(event);
        goto exit;
    }
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeBiometric) {
        handleBiometricEvent(event);
        goto exit;
    }
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeDigitizer) {
        if ((IOHIDEventGetIntegerValue(event, kIOHIDEventFieldDigitizerEventMask) & kIOHIDDigitizerEventCancel) != 0) {
            goto exit;
        }
        
        if (!handleDFREvent(event)) {
            // keep track of active touches during cancellation phase, so we know to continue to cancel them
            _cancelledTouchInProgress = (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldDigitizerRange) == 1);
            
            HIDLogDebug("Event cancelled due to %s. touch: %d flags = %x", _bioInProgress ? "touch ID" : "active keys", _cancelledTouchInProgress,  (unsigned int)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldDigitizerEventMask));
            
            _cancelledEventCount++;
            CFRelease(event);
            event = NULL;
        } else if (_cancelledTouchInProgress) {
            // Touch has ended on DFR, allow events
            _cancelledTouchInProgress = (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldDigitizerRange) == 1);

            HIDLogDebug("Event cancelled due to touch in progress.");
            CFRelease(event);
            event = NULL;
        } else {
            // end cancellation phase
            _cancel = false;
        }
    }
    
exit:
    return event;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::getPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDDFREventFilter::getPropertyForClient(void *self, CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDDFREventFilter *>(self)->getPropertyForClient(key,client);
}

CFTypeRef IOHIDDFREventFilter::getPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{
    CFTypeRef result = NULL;
    
    if (CFEqual(key, CFSTR(kIOHIDDFRKeyboardEventFilterEnabledKey))) {
        result = _keyboardFilterEnabled ? kCFBooleanTrue : kCFBooleanFalse;
    } else if (CFEqual(key, CFSTR(kIOHIDDFRTouchIDEventFilterEnabledKey))) {
        result = _touchIDFilterEnabled ? kCFBooleanTrue : kCFBooleanFalse;
    } else if (CFEqual(key, CFSTR(kIOHIDDFREventFilterKeyboardCancelThreshold))) {
        result = CFNumberRefWrap((SInt32)_keyboardCancelThresholdMS);
    } else if (CFEqual(key, CFSTR(kIOHIDDFREventFilterDFRTouchCancelThreshold))) {
        result = CFNumberRefWrap((SInt32)_dfrTouchCancelThresholdMS);
    } else if (CFEqual(key, CFSTR(kIOHIDDFREventFilterBiometricCancelThreshold))) {
        result = CFNumberRefWrap((SInt32)_bioCancelThresholdMS);
    } else if (CFEqual(key, CFSTR(kIOHIDSessionFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        serialize(serializer);
        
        if (serializer) {
            result = CFRetain(serializer.Reference());
        }
    }
    
    return result;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::setPropertyForClient(void *self, CFStringRef key, CFTypeRef property, CFTypeRef client)
{
    static_cast<IOHIDDFREventFilter *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDDFREventFilter::setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client __unused)
{
    if (CFEqual(key, CFSTR(kIOHIDDFRKeyboardEventFilterEnabledKey))) {
        _keyboardFilterEnabled = CFBooleanGetValue((CFBooleanRef)property);
    } else if (CFEqual(key, CFSTR(kIOHIDDFRTouchIDEventFilterEnabledKey))) {
        _touchIDFilterEnabled = CFBooleanGetValue((CFBooleanRef)property);
    } else if (CFEqual(key, CFSTR(kIOHIDDFREventFilterKeyboardCancelThreshold))) {
        if (property && CFGetTypeID(property) == CFNumberGetTypeID()) {
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_keyboardCancelThresholdMS);
        }
    } else if (CFEqual(key, CFSTR(kIOHIDDFREventFilterDFRTouchCancelThreshold))) {
        if (property && CFGetTypeID(property) == CFNumberGetTypeID()) {
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_dfrTouchCancelThresholdMS);
        }
    } else if (CFEqual(key, CFSTR(kIOHIDDFREventFilterBiometricCancelThreshold))) {
        if (property && CFGetTypeID(property) == CFNumberGetTypeID()) {
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_bioCancelThresholdMS);
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::handleKeyboardEvent
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::handleKeyboardEvent(IOHIDEventRef event)
{
    uint32_t    usage;
    uint32_t    usagePage;
    uint32_t    keyDown;
    uint32_t    flags;
    Key         key;
    
    usage       = (uint32_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (uint32_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (uint32_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    flags       = (uint32_t)IOHIDEventGetEventFlags(event);
    
    if ((IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardSlowKeyPhase) == kIOHIDKeyboardSlowKeyPhaseStart ||
         IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardSlowKeyPhase) == kIOHIDKeyboardSlowKeyPhaseAbort)) {
        return;
    }
    
    key = Key(usagePage, usage);
    if (keyDown) {
        uint64_t current = mach_absolute_time();
        uint64_t deltaMS = _lastDFREventTime ? DELTA_IN_MS(current, _lastDFREventTime) : 0;
        
        _activeKeys.insert(std::make_pair(key, KeyAttribute(flags)));
        if (!modifierPressed() && key.isTopRow() && !(flags & kIOHIDKeyboardIsRepeat) && deltaMS <= _dfrTouchCancelThresholdMS) {
            _lastKeyboardEventTime = IOHIDEventGetTimeStamp(event);
            startTouchCancellation();
        }
    } else {
        auto iter = _activeKeys.find(key);
        if (iter!=_activeKeys.end()) {
            _activeKeys.erase(iter);
        }
        
        if (!topRowPressed() && !_bioInProgress && !_cancelledTouchInProgress) {
            _cancel = false;
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::handleBiometicEvent
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::handleBiometricEvent(IOHIDEventRef event)
{
    if (!_touchIDFilterEnabled) {
        return;
    }
    
    if (IOHIDEventGetType(event) == kIOHIDEventTypeBiometric) {
        if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldBiometricLevel) == 1) {
            _bioInProgress = true;
            startTouchCancellation();
            
            dispatch_source_set_timer(_eventCancelTimer,
                                      dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * _bioCancelThresholdMS),
                                      DISPATCH_TIME_FOREVER, 0);
        } else {
            _bioInProgress = false;
            _cancel = false;
            dispatch_source_set_timer(_eventCancelTimer, DISPATCH_TIME_FOREVER, 0, 0);
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::modifierPressed
//------------------------------------------------------------------------------
bool IOHIDDFREventFilter::modifierPressed() {
    auto iter = _activeKeys.begin();
    for (; iter != _activeKeys.end(); ++iter) {
        if (iter->first.isModifier()) {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::topRowPressed
//------------------------------------------------------------------------------
bool IOHIDDFREventFilter::topRowPressed() {
    auto iter = _activeKeys.begin();
    for (; iter != _activeKeys.end(); ++iter) {
        if (iter->first.isTopRow()) {
            return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::handleDFREvent
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDDFREventFilter::handleDFREvent(IOHIDEventRef event)
{
    IOHIDEventRef   result  = event;
    uint64_t        current = mach_absolute_time();
    uint64_t        deltaMS = DELTA_IN_MS(current, _lastKeyboardEventTime);
    
    // Cancel events when a non-modifier top row key is pressed within then cancel threshold
    // or biometric event is in progress
    
    if ((topRowPressed() && !modifierPressed() && _keyboardFilterEnabled && deltaMS <= _keyboardCancelThresholdMS) ||
        (_bioInProgress)) {
        result = NULL;
        goto exit;
    }
    
    if (_lastDFREvent) {
        CFRelease(_lastDFREvent);
        _lastDFREvent = NULL;
    }
    
    // keep track of how long we're touching for
    if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldDigitizerRange) == 1) {
        if (_lastDFREventTime == 0) {
            _lastDFREventTime = IOHIDEventGetTimeStamp(event);
        }
        // save last event to dispatch on next cancellation
        _lastDFREvent = event;
        CFRetain(event);
    } else {
        _lastDFREventTime = 0;
    }
    
exit:
    return result;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::startTouchCancellation
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::startTouchCancellation()
{
    if (_cancel) {
        return;
    }
    
    if (_lastDFREvent) {
        CFArrayRef children = NULL;
        
        // Set cancel flag for event and all child digitizer events
        IOHIDEventSetIntegerValue(_lastDFREvent, kIOHIDEventFieldDigitizerEventMask, kIOHIDDigitizerEventCancel);
        
        children = IOHIDEventGetChildren(_lastDFREvent);
        for (CFIndex index = 0, count = children ? CFArrayGetCount(children) : 0; index < count; index++) {
            IOHIDEventRef child = (IOHIDEventRef)CFArrayGetValueAtIndex(children, index);
            if (IOHIDEventGetType(child) == kIOHIDEventTypeDigitizer) {
                IOHIDEventSetIntegerValue(child, kIOHIDEventFieldDigitizerEventMask, kIOHIDDigitizerEventCancel);
            }
        }
        
        IOHIDEventSetTimeStamp(_lastDFREvent, mach_absolute_time());
        _IOHIDSessionDispatchEvent(_session, _lastDFREvent);
        HIDLogDebug("Dispatch touch cancel event");
        
        // a touch is in progress, continue to cancel events until it is lifted
        _cancelledTouchInProgress = true;
        CFRelease(_lastDFREvent);
        _lastDFREvent = NULL;
    }
    
    _cancel = true;
}

//------------------------------------------------------------------------------
// IOHIDDFREventFilter::serialize
//------------------------------------------------------------------------------
void IOHIDDFREventFilter::serialize(CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDDFREventFilter"));
    serializer.SetValueForKey(CFSTR("Keyboard Filter Enabled"), CFNumberRefWrap(_keyboardFilterEnabled));
    serializer.SetValueForKey(CFSTR("TouchID Filter Enabled"), CFNumberRefWrap(_touchIDFilterEnabled));
    serializer.SetValueForKey(CFSTR("Touch in Progress"), CFNumberRefWrap(_cancelledTouchInProgress));
    serializer.SetValueForKey(CFSTR("Biometry in Progress"), CFNumberRefWrap(_bioInProgress));
    serializer.SetValueForKey(CFSTR("Cancellation in Progress"), CFNumberRefWrap(_cancel));
    serializer.SetValueForKey(CFSTR("Cancelled Event Count"), CFNumberRefWrap(_cancelledEventCount));
    serializer.SetValueForKey(CFSTR("Keyboard Cancel Threshold (ms)"), CFNumberRefWrap(_keyboardCancelThresholdMS));
    serializer.SetValueForKey(CFSTR("DFR Touch Cancel Threshold (ms)"), CFNumberRefWrap(_dfrTouchCancelThresholdMS));
    serializer.SetValueForKey(CFSTR("Biometric Cancel Threshold (ms)"), CFNumberRefWrap(_bioCancelThresholdMS));
}
