/*
 *  IOHIDEventSystemStatistics.cpp
 *  IOHIDEventSystemPlugIns
 *
 *  Created by Rob Yepez on 05/21/2013.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
 */

#include <new>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDSession.h>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <notify.h>
#include <pthread.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include "IOHIDEventSystemStatistics.h"
#include "CF.h"
#include <CoreAnalytics/CoreAnalytics.h>
#include <xpc/xpc.h>

#define kCoreAnalyticsDictionaryAppleButtonEvents               "com.apple.iokit.hid.button"
#define kCoreAnalyticsDictionaryHomeButtonWakeCountKey          "homeButtonWakeCount"
#define kCoreAnalyticsDictionaryPowerButtonWakeCountKey         "powerButtonWakeCount"
#define kCoreAnalyticsDictionaryPowerButtonSleepCountKey        "powerButtonSleepCount"

#define kCoreAnalyticsDictionaryAppleKeyboardEvents                 "com.apple.iokit.hid.keyboard"
#define kCoreAnalyticsDictionaryAppleKeyboardEnumerationCountKey    "enumerationCount"
#define kCoreAnalyticsDictionaryAppleKeyboardCharacterCountKey      "characterKeyCount"
#define kCoreAnalyticsDictionaryAppleKeyboardSymbolCountKey         "symbolKeyCount"
#define kCoreAnalyticsDictionaryAppleKeyboardSpacebarCountKey       "spacebarKeyCount"
#define kCoreAnalyticsDictionaryAppleKeyboardArrowCountKey          "arrowKeyCount"
#define kCoreAnalyticsDictionaryAppleKeyboardCursorCountKey         "cursorKeyCount"
#define kCoreAnalyticsDictionaryAppleKeyboardModifierCountKey       "modifierKeyCount"

#define kCoreAnalyticsDictionaryAppleMultiPressEvents               "com.apple.iohid.buttons.doublePressTiming"
#define kCoreAnalyticsDictionaryAppleMultiPressFailIntervalsKey     "PressFailureTime"
#define kCoreAnalyticsDictionaryAppleMultiPressPassIntervalsKey     "PressSuccessTime"


#define kMaxMultiPressTime (2 * NSEC_PER_SEC)

#define ABS_TO_NS(t,b)               ((t * (b).numer)/ (b).denom)

#define kAppleVendorID 1452

// 072BC077-E984-4C2A-BB72-D4769CE44FAF
#define kIOHIDEventSystemStatisticsFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x07, 0x2B, 0xC0, 0x77, 0xE9, 0x84, 0x4C, 0x2A, 0xBB, 0x72, 0xD4, 0x76, 0x9C, 0xE4, 0x4F, 0xAF)

extern "C" void * IOHIDEventSystemStatisticsFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);

//------------------------------------------------------------------------------
// IOHIDEventSystemStatisticsFactory
//------------------------------------------------------------------------------
// Implementation of the factory function for this type.
void *IOHIDEventSystemStatisticsFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDEventSystemStatistics), 0);
        return new(p) IOHIDEventSystemStatistics(kIOHIDEventSystemStatisticsFactory);
    }
    // If the requested type is incorrect, return NULL.
    return NULL;
}

// The IOHIDEventSystemStatistics function table.
IOHIDSessionFilterPlugInInterface IOHIDEventSystemStatistics::sIOHIDEventSystemStatisticsFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDEventSystemStatistics::QueryInterface,
    IOHIDEventSystemStatistics::AddRef,
    IOHIDEventSystemStatistics::Release,
    // IOHIDSimpleSessionFilterPlugInInterface functions
    IOHIDEventSystemStatistics::filter,
    NULL,
    NULL,
    // IOHIDSessionFilterPlugInInterface functions
    IOHIDEventSystemStatistics::open,
    IOHIDEventSystemStatistics::close,
    NULL,
    NULL,
    IOHIDEventSystemStatistics::registerService,
    IOHIDEventSystemStatistics::unregisterService,
    IOHIDEventSystemStatistics::scheduleWithDispatchQueue,
    IOHIDEventSystemStatistics::unscheduleFromDispatchQueue,
    IOHIDEventSystemStatistics::getPropertyForClient,
    NULL,
};

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::IOHIDEventSystemStatistics
//------------------------------------------------------------------------------
IOHIDEventSystemStatistics::IOHIDEventSystemStatistics(CFUUIDRef factoryID)
:
_sessionInterface(&sIOHIDEventSystemStatisticsFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_displayState(1),
_displayToken(0),
_dispatch_queue(0),
_pending_source(0),
_keyServices(NULL)
{
    bzero(&_pending_buttons, sizeof(_pending_buttons));
    bzero(&_pending_keystats, sizeof(_pending_keystats));
    CFPlugInAddInstanceForFactory( factoryID );
}
//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::IOHIDEventSystemStatistics
//------------------------------------------------------------------------------
IOHIDEventSystemStatistics::~IOHIDEventSystemStatistics()
{
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}
//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDEventSystemStatistics::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDEventSystemStatistics *>(self)->QueryInterface(iid, ppv);
}
// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDEventSystemStatistics::QueryInterface( REFIID iid, LPVOID *ppv )
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
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
    CFRelease( interfaceID );
    return E_NOINTERFACE;
}
//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDEventSystemStatistics::AddRef( void *self )
{
    return static_cast<IOHIDEventSystemStatistics *>(self)->AddRef();
}
ULONG IOHIDEventSystemStatistics::AddRef()
{
    _refCount += 1;
    return _refCount;
}
//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::Release
//------------------------------------------------------------------------------
ULONG IOHIDEventSystemStatistics::Release( void *self )
{
    return static_cast<IOHIDEventSystemStatistics *>(self)->Release();
}
ULONG IOHIDEventSystemStatistics::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}
//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::open
//------------------------------------------------------------------------------
boolean_t IOHIDEventSystemStatistics::open(void * self, IOHIDSessionRef session, IOOptionBits options)
{
    return static_cast<IOHIDEventSystemStatistics *>(self)->open(session, options);
}

boolean_t IOHIDEventSystemStatistics::open(IOHIDSessionRef session, IOOptionBits options)
{
    (void)session;
    (void)options;
    
    _keyServices = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    
    _attachEvent = IOHIDEventCreateKeyboardEvent(kCFAllocatorDefault, 0,
                                                 kHIDPage_AppleVendorSmartCover,
                                                 kHIDUsage_AppleVendorSmartCover_Attach,
                                                 0, 0);

    _multiPressServices = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

    mach_timebase_info(&_timeInfo);
    
    if (!_keyServices || !_attachEvent || !_multiPressServices) {
        return false;
    }
    return true;
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::close
//------------------------------------------------------------------------------
void IOHIDEventSystemStatistics::close(void * self, IOHIDSessionRef session, IOOptionBits options)
{
    static_cast<IOHIDEventSystemStatistics *>(self)->close(session, options);
}

void IOHIDEventSystemStatistics::close(IOHIDSessionRef session, IOOptionBits options)
{
    (void) session;
    (void) options;
    
    if (_keyServices) {
        CFRelease(_keyServices);
        _keyServices = NULL;
    }
    
    if (_attachEvent) {
        CFRelease(_attachEvent);
        _attachEvent = NULL;
    }

    if (_multiPressServices) {
        CFRelease(_multiPressServices);
        _multiPressServices = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::registerService
//------------------------------------------------------------------------------
void IOHIDEventSystemStatistics::registerService(void * self, IOHIDServiceRef service)
{
    static_cast<IOHIDEventSystemStatistics *>(self)->registerService(service);
}
void IOHIDEventSystemStatistics::registerService(IOHIDServiceRef service)
{
    registerKeyboardService(service);
    registerMultiPressService(service);
}

void IOHIDEventSystemStatistics::registerKeyboardService(IOHIDServiceRef service)
{
    if (!IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard)) {
        return;
    }

    CFTypeRef   obj;
    uint32_t    vendorID = 0;
    
    obj = IOHIDServiceGetProperty(service, CFSTR(kIOHIDVendorIDKey));
    if (obj && CFGetTypeID(obj) == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)obj, kCFNumberIntType, &vendorID);
    }
    
    // Check for Apple vendorID.
    if (vendorID != kAppleVendorID) {
        return;
    }

    // Check for AccessoryID Bus transport.
    obj = IOHIDServiceGetProperty(service, CFSTR(kIOHIDTransportKey));
    if (!obj || CFGetTypeID(obj) != CFStringGetTypeID() || !CFEqual((CFStringRef)obj, CFSTR(kIOHIDTransportAIDBValue))) {
        return;
    }

    _pending_keystats.enumeration_count++;
    
    if (_keyServices) {
        CFSetAddValue(_keyServices, service);
    }
    
    HIDLogDebug("Apple Keyboard registered");
}

void IOHIDEventSystemStatistics::registerMultiPressService(IOHIDServiceRef service)
{
    CFTypeRef enabledProp = IOHIDServiceGetProperty(service, CFSTR(kIOHIDKeyboardPressCountTrackingEnabledKey));
    if (!enabledProp) {
        return;
    }

    bool isEnabled = false;
    if (CFGetTypeID(enabledProp) == CFBooleanGetTypeID()) {
        CFBooleanRef boolVal = (CFBooleanRef)enabledProp;
        isEnabled = boolVal == kCFBooleanTrue;
    } else if (CFGetTypeID(enabledProp) == CFNumberGetTypeID()) {
        CFNumberRef enabledNum = (CFNumberRef)enabledProp;
        uint64_t enabledVal = 0;
        CFNumberGetValue(enabledNum, kCFNumberLongLongType, &enabledVal);
        isEnabled = enabledVal != 0;
    }

    if (isEnabled && _multiPressServices) {
        CFMutableDictionaryRef mpServiceDict = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
        if (mpServiceDict) {
            CFTypeRef multiPressUsages = IOHIDServiceGetProperty(service, CFSTR(kIOHIDKeyboardPressCountUsagePairsKey));
            if (multiPressUsages) {
                CFDictionarySetValue(mpServiceDict, CFSTR("MultiPressUsages"), multiPressUsages);
            }
            CFDictionarySetValue(_multiPressServices, service, mpServiceDict);
            HIDLogDebug("Added MultiPress Analytics for service:%@ %@", IOHIDServiceGetRegistryID(service), multiPressUsages);
            CFRelease(mpServiceDict);
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::unregisterService
//------------------------------------------------------------------------------
void IOHIDEventSystemStatistics::unregisterService(void * self, IOHIDServiceRef service)
{
    static_cast<IOHIDEventSystemStatistics *>(self)->unregisterService(service);
}
void IOHIDEventSystemStatistics::unregisterService(IOHIDServiceRef service)
{
    if (_keyServices && CFSetGetValue(_keyServices, service)) {
        CFSetRemoveValue(_keyServices, service);
        
        HIDLogDebug("Apple Keyboard unregistered");
    }

    if (_multiPressServices && CFDictionaryContainsKey(_multiPressServices, service)) {
        CFDictionaryRemoveValue(_multiPressServices, service);

        HIDLogDebug("MultiPress service removed: %@", IOHIDServiceGetRegistryID(service));
    }
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDEventSystemStatistics::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDEventSystemStatistics *>(self)->scheduleWithDispatchQueue(queue);
}
void IOHIDEventSystemStatistics::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _dispatch_queue = queue;
    
    if ( _dispatch_queue ) {
        _pending_source = dispatch_source_create(DISPATCH_SOURCE_TYPE_DATA_ADD, 0, 0, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0));
        dispatch_set_context(_pending_source, this);
        dispatch_source_set_event_handler_f(_pending_source, IOHIDEventSystemStatistics::handlePendingStats);
        dispatch_resume(_pending_source);
        
        notify_register_dispatch( "com.apple.iokit.hid.displayStatus", &_displayToken,_dispatch_queue, ^(__unused int token){
            
            notify_get_state(_displayToken, &_displayState);
        });
    }
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDEventSystemStatistics::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDEventSystemStatistics *>(self)->unscheduleFromDispatchQueue(queue);
}
void IOHIDEventSystemStatistics::unscheduleFromDispatchQueue(dispatch_queue_t queue)
{
    if ( _dispatch_queue != queue )
        return;
    
    if ( _pending_source ) {
        dispatch_source_cancel(_pending_source);
        dispatch_release(_pending_source);
        _pending_source = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::getPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDEventSystemStatistics::getPropertyForClient (void * self, CFStringRef key, CFTypeRef client) {
    return static_cast<IOHIDEventSystemStatistics *>(self)->getPropertyForClient(key,client);
}

CFTypeRef IOHIDEventSystemStatistics::getPropertyForClient (CFStringRef key, CFTypeRef client __unused) {
    CFTypeRef result = NULL;

    if (CFEqual(key, CFSTR(kIOHIDSessionFilterDebugKey))) {
        
        CFMutableDictionaryRefWrap debug;

        debug.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDEventSystemStatistics"));

        if (_keyServices) {
            __block CFMutableArrayRefWrap kbcServices;
            _IOHIDCFSetApplyBlock (_keyServices, ^(CFTypeRef value) {
                kbcServices.Append(IOHIDServiceGetRegistryID((IOHIDServiceRef)value));
            });
            debug.SetValueForKey(CFSTR("KeyboardServices"), kbcServices.Reference());
        }

        result = CFRetain(debug.Reference());
    }

    return result;
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::handlePendingStats
//------------------------------------------------------------------------------
void IOHIDEventSystemStatistics::handlePendingStats(void * self)
{
    return static_cast<IOHIDEventSystemStatistics *>(self)->handlePendingStats();
}

void IOHIDEventSystemStatistics::handlePendingStats()
{
    __block Buttons               buttons          = {};
    __block KeyStats              keyStats         = {};
    CFMutableDictionaryRefWrap    adclientKeys     = NULL;
    
    dispatch_sync(_dispatch_queue, ^{
        bcopy(&_pending_buttons, &buttons, sizeof(Buttons));
        bzero(&_pending_buttons, sizeof(Buttons));
        
        bcopy(&_pending_keystats, &keyStats, sizeof(KeyStats));
        bzero(&_pending_keystats, sizeof(KeyStats));

    });
    
    analytics_send_event_lazy(kCoreAnalyticsDictionaryAppleButtonEvents, ^xpc_object_t {
        xpc_object_t keyDictionary = xpc_dictionary_create(NULL, NULL, 0);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryHomeButtonWakeCountKey, buttons.home_wake);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryPowerButtonWakeCountKey, buttons.power_wake);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryPowerButtonSleepCountKey, buttons.power_sleep);

        return keyDictionary;
    });
    
    HIDLogDebug("Apple Keyboard char: %d symbol: %d spacebar: %d arrow: %d cursor: %d modifier: %d ",
                keyStats.character_count, keyStats.symbol_count, keyStats.spacebar_count,
                keyStats.arrow_count, keyStats.cursor_count, keyStats.modifier_count);
    

    analytics_send_event_lazy(kCoreAnalyticsDictionaryAppleKeyboardEvents, ^xpc_object_t {
        xpc_object_t keyDictionary = xpc_dictionary_create(NULL, NULL, 0);

        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardEnumerationCountKey, keyStats.enumeration_count);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardCharacterCountKey, keyStats.character_count);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardSymbolCountKey, keyStats.symbol_count);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardSpacebarCountKey, keyStats.spacebar_count);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardArrowCountKey, keyStats.arrow_count);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardCursorCountKey, keyStats.cursor_count);
        xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleKeyboardModifierCountKey, keyStats.modifier_count);

        return keyDictionary;
    });
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::collectKeyStats
//------------------------------------------------------------------------------
bool IOHIDEventSystemStatistics::collectKeyStats(IOHIDServiceRef sender, IOHIDEventRef event)
{
    uint16_t    usagePage;
    uint16_t    usage;
    
    if (sender == NULL || _keyServices == NULL)
        return false;

    // Collect stats only for keyboard event key-downs.
    if (IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard ||
        !IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown))
        return false;
    
    // Check if the event is from a registered Apple Keyboard service.
    if (!CFSetGetValue(_keyServices, sender))
        return false;
        
    usagePage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    usage     = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    
    if (isCharacterKey(usagePage, usage)) {
        HIDLogDebug("Apple Keyboard character key");
        _pending_keystats.character_count++;
    }
    else if (isSymbolKey(usagePage, usage)) {
        HIDLogDebug("Apple Keyboard symbol key");
        _pending_keystats.symbol_count++;
    }
    else if (isSpacebarKey(usagePage, usage)) {
        HIDLogDebug("Apple Keyboard spacebar key");
        _pending_keystats.spacebar_count++;
    }
    else if (isArrowKey(usagePage, usage)) {
        HIDLogDebug("Apple Keyboard arrow key");
        _pending_keystats.arrow_count++;
    }
    else if (isCursorKey(usagePage, usage)) {
        HIDLogDebug("Apple Keyboard cursor key");
        _pending_keystats.cursor_count++;
    }
    else if (isModifierKey(usagePage, usage)) {
        HIDLogDebug("Apple Keyboard modifier key");
        _pending_keystats.modifier_count++;
    }
    
    // Return true to signal AggD update.
    return true;
}

static bool isMultiPressUsage(CFTypeRef usages, CFIndex usagePage, CFIndex usage) {
    if (usages == NULL) {
        return true;
    }

    CFIndex usagePair = (usagePage << 16) | usage;
    CFNumberRef usagePairNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usagePair);
    bool isUsage = true;

    if (CFGetTypeID(usages) == CFArrayGetTypeID()) {
        CFArrayRef usagePairArray = (CFArrayRef)usages;
        CFRange range = CFRangeMake(0, CFArrayGetCount(usagePairArray));
        isUsage = CFArrayContainsValue(usagePairArray, range, usagePairNum);
    }

    CFRelease(usagePairNum);
    return isUsage;
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDEventSystemStatistics::filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event)
{
    return static_cast<IOHIDEventSystemStatistics *>(self)->filter(sender, event);
}
IOHIDEventRef IOHIDEventSystemStatistics::filter(IOHIDServiceRef sender, IOHIDEventRef event)
{
    (void) sender;
    
    if ( _pending_source ) {
        if ( event ) {
            bool signal = false;
            
            if (collectKeyStats(sender, event)) {
                
                signal = true;
            }
            else if ((IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard)
                && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage) == kHIDPage_Consumer)) {
                
                if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)) {
                    
                    signal = true;
                    
                    switch (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage) ) {
                        case kHIDUsage_Csmr_Menu:
                            if ( !_displayState )
                                _pending_buttons.home_wake++;
                            break;
                        case kHIDUsage_Csmr_Power:
                            if ( !_displayState )
                                _pending_buttons.power_wake++;
                            else
                                _pending_buttons.power_sleep++;
                            break;
                        default:
                            signal = false;
                            break;
                    }
                }
            }

            if ( signal ) {
                dispatch_source_merge_data(_pending_source, 1);
            }

            if (sender && _multiPressServices && CFDictionaryContainsKey(_multiPressServices, sender)) {
                CFTypeRef obj = CFDictionaryGetValue(_multiPressServices, sender);
                if (CFGetTypeID(obj) == CFDictionaryGetTypeID()) {
                    CFMutableDictionaryRef mpServiceInfo = (CFMutableDictionaryRef)obj;
                    uint64_t eventTime = ABS_TO_NS(IOHIDEventGetTimeStampOfType(event, kIOHIDEventTimestampTypeAbsolute), _timeInfo);
                    CFIndex eventPressCount = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardPressCount);
                    CFIndex eventState = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
                    CFIndex eventUsagePage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
                    CFIndex eventUsage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
                    CFIndex eventLongPress = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardLongPress);
                    IOHIDEventPhaseBits eventPhase = IOHIDEventGetPhase(event);
                    CFTypeRef usages = CFDictionaryGetValue(mpServiceInfo, CFSTR("MultiPressUsages"));
                    bool eventMultiPressUsage =  isMultiPressUsage(usages, eventUsagePage, eventUsage);

                    if (eventState && eventMultiPressUsage && !(eventPhase & kIOHIDEventPhaseEnded) && !eventLongPress) {
                        uint64_t prevEventTime = 0;
                        uint64_t prevPressCount = 0;
                        uint64_t pressInterval = 0;

                        obj = CFDictionaryGetValue(mpServiceInfo, CFSTR("PressCount"));
                        if (obj && CFGetTypeID(obj) == CFNumberGetTypeID()) {
                            CFNumberGetValue((CFNumberRef)obj, kCFNumberLongLongType, &prevPressCount);
                        }
                        obj = CFDictionaryGetValue(mpServiceInfo, CFSTR("MultiPressTime"));
                        if (obj && CFGetTypeID(obj) == CFNumberGetTypeID()) {
                            CFNumberGetValue((CFNumberRef)obj, kCFNumberLongLongType, &prevEventTime);
                        }

                        pressInterval = eventTime - prevEventTime;

                        if (prevPressCount && prevEventTime && prevPressCount == eventPressCount) {
                            if (pressInterval < kMaxMultiPressTime) {
                                HIDLogDebug("PressInterval Failed: %llu", pressInterval);
                                __block uint64_t pressTime = pressInterval;
                                analytics_send_event_lazy(kCoreAnalyticsDictionaryAppleMultiPressEvents, ^xpc_object_t{
                                    xpc_object_t keyDictionary = xpc_dictionary_create(NULL, NULL, 0);

                                    xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleMultiPressFailIntervalsKey, pressTime / NSEC_PER_MSEC);

                                    return keyDictionary;
                                });
                            }
                        } else if (prevPressCount && prevEventTime && prevPressCount < eventPressCount) {
                            HIDLogDebug("PressInterval Success: %llu", pressInterval);
                            __block uint64_t pressTime = pressInterval;
                            analytics_send_event_lazy(kCoreAnalyticsDictionaryAppleMultiPressEvents, ^xpc_object_t{
                                xpc_object_t keyDictionary = xpc_dictionary_create(NULL, NULL, 0);

                                xpc_dictionary_set_uint64(keyDictionary, kCoreAnalyticsDictionaryAppleMultiPressPassIntervalsKey, pressTime / NSEC_PER_MSEC);

                                return keyDictionary;
                            });
                        }
                        // Set the previous event time info
                        CFNumberRef eventPressNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &eventPressCount);
                        CFNumberRef eventTimeNum = CFNumberCreate(kCFAllocatorDefault, kCFNumberLongLongType, &eventTime);
                        CFDictionarySetValue(mpServiceInfo, CFSTR("PressCount"), eventPressNum);
                        CFDictionarySetValue(mpServiceInfo, CFSTR("MultiPressTime"), eventTimeNum);
                        CFRelease(eventPressNum);
                        CFRelease(eventTimeNum);
                    }
                }
            }
        }
        
    }
    
    return event;
}

bool IOHIDEventSystemStatistics::isCharacterKey(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad) {
        
        if (usage >= kHIDUsage_KeyboardA && usage <= kHIDUsage_KeyboardZ) {
            
            return true;
        }
    }
    return false;
}

bool IOHIDEventSystemStatistics::isSymbolKey(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad) {
        
        // Check numeric keys
        if (usage >= kHIDUsage_Keyboard1 && usage <= kHIDUsage_Keyboard0) {
            
            return true;
        }
        
        // Non-numeric symbol keys
        switch (usage) {
                
            case kHIDUsage_KeyboardHyphen:
            case kHIDUsage_KeyboardEqualSign:
            case kHIDUsage_KeyboardOpenBracket:
            case kHIDUsage_KeyboardCloseBracket:
            case kHIDUsage_KeyboardBackslash:
            case kHIDUsage_KeyboardSemicolon:
            case kHIDUsage_KeyboardQuote:
            case kHIDUsage_KeyboardGraveAccentAndTilde:
            case kHIDUsage_KeyboardComma:
            case kHIDUsage_KeyboardPeriod:
            case kHIDUsage_KeyboardSlash:
                return true;
                
            default:
                break;
        }
    }
    return false;
}

bool IOHIDEventSystemStatistics::isSpacebarKey(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad && usage == kHIDUsage_KeyboardSpacebar) {
        
        return true;
    }
    return false;
}

bool IOHIDEventSystemStatistics::isArrowKey(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad) {

        switch (usage) {
                
            case kHIDUsage_KeyboardRightArrow:
            case kHIDUsage_KeyboardLeftArrow:
            case kHIDUsage_KeyboardDownArrow:
            case kHIDUsage_KeyboardUpArrow:
                return true;
                
            default:
                break;
        }
    }
    return false;
}

bool IOHIDEventSystemStatistics::isCursorKey(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad) {
        
        switch (usage) {
                
            case kHIDUsage_KeyboardReturnOrEnter:
            case kHIDUsage_KeyboardDeleteOrBackspace:
            case kHIDUsage_KeyboardTab:
            case kHIDUsage_KeyboardCapsLock:
                return true;
                
            default:
                break;
        }
    }
    return false;
}

bool IOHIDEventSystemStatistics::isModifierKey(uint16_t usagePage, uint16_t usage)
{
    if (usagePage == kHIDPage_Consumer && usage == kHIDUsage_Csmr_ACKeyboardLayoutSelect) {
        
        return true;
    }
    else if (usagePage == kHIDPage_KeyboardOrKeypad) {
        
        switch (usage) {
                
            case kHIDUsage_KeyboardLeftControl:
            case kHIDUsage_KeyboardLeftShift:
            case kHIDUsage_KeyboardLeftAlt:
            case kHIDUsage_KeyboardLeftGUI:
            case kHIDUsage_KeyboardRightShift:
            case kHIDUsage_KeyboardRightAlt:
            case kHIDUsage_KeyboardRightGUI:
                return true;
                
            default:
                break;
        }
    }
    return false;
}
