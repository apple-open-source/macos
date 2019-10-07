/*
 *  IOHIDEventSystemStatistics.cpp
 *  IOHIDEventSystemPlugIns
 *
 *  Created by Rob Yepez on 05/21/2013.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
 */

#include <new>
#include <AggregateDictionary/ADClient.h>
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
#include "IOHIDDebug.h"
#include "CF.h"
#include <WirelessDiagnostics/AWDSimpleMetrics.h>
#include "IOHIDFamilyCoverHESTEvents.hpp"
#include "AWDMetricIds_IOHIDFamily.h"

#define kAggregateDictionaryKeyboardEnumerationCountKey         "com.apple.iokit.hid.keyboard.enumerationCount"
#define kAggregateDictionaryHomeButtonWakeCountKey              "com.apple.iokit.hid.homeButton.wakeCount"
#define kAggregateDictionaryPowerButtonWakeCountKey             "com.apple.iokit.hid.powerButton.wakeCount"
#define kAggregateDictionaryPowerButtonSleepCountKey            "com.apple.iokit.hid.powerButton.sleepCount"

#define kAggregateDictionaryButtonsHighLatencyCountKey          "com.apple.iokit.hid.buttons.highLatencyCount"
#define kAggregateDictionaryButtonsHighLatencyMinMS             10000

#define kAggregateDictionaryMotionAccelSampleCount              "com.apple.CoreMotion.Accel.SampleCount"
#define kAggregateDictionaryMotionGyroSampleCount               "com.apple.CoreMotion.Gyro.SampleCount"
#define kAggregateDictionaryMotionMagSampleCount                "com.apple.CoreMotion.Mag.SampleCount"
#define kAggregateDictionaryMotionPressureSampleCount           "com.apple.CoreMotion.Pressure.SampleCount"
#define kAggregateDictionaryMotionDeviceMotionSampleCount       "com.apple.CoreMotion.DeviceMotion.SampleCount"

#define kAggregateDictionaryMotionFlushInterval 60.0

#define kAggregateDictionaryAppleKeyboardCharacterCountKey      "com.apple.iokit.hid.keyboard.characterKey.count"
#define kAggregateDictionaryAppleKeyboardSymbolCountKey         "com.apple.iokit.hid.keyboard.symbolKey.count"
#define kAggregateDictionaryAppleKeyboardSpacebarCountKey       "com.apple.iokit.hid.keyboard.spacebarKey.count"
#define kAggregateDictionaryAppleKeyboardArrowCountKey          "com.apple.iokit.hid.keyboard.arrowKey.count"
#define kAggregateDictionaryAppleKeyboardCursorCountKey         "com.apple.iokit.hid.keyboard.cursorKey.count"
#define kAggregateDictionaryAppleKeyboardModifierCountKey       "com.apple.iokit.hid.keyboard.modifierKey.count"

#define kAggregateDictionaryCoverHESOpenCount                   "com.apple.iokit.hid.hes.openCount"
#define kAggregateDictionaryCoverHESCloseCount                  "com.apple.iokit.hid.hes.closeCount"
#define kAggregateDictionaryCoverHESUsedRecently                "com.apple.iokit.hid.hes.usedRecently"
#define kAggregateDictionaryCoverHESToggled50ms                 "com.apple.iokit.hid.hes.toggled50ms"
#define kAggregateDictionaryCoverHESToggled50to100ms            "com.apple.iokit.hid.hes.toggled50to100ms"
#define kAggregateDictionaryCoverHESToggled100to250ms           "com.apple.iokit.hid.hes.toggled100to250ms"
#define kAggregateDictionaryCoverHESToggled250to500ms           "com.apple.iokit.hid.hes.toggled250to500ms"
#define kAggregateDictionaryCoverHESToggled500to1000ms          "com.apple.iokit.hid.hes.toggled500to1000ms"
#define kAggregateDictionaryCoverHESUnknownStateEnterCount      "com.apple.iokit.hid.hes.unknownStateEnterCount"
#define kAggregateDictionaryCoverHESUnknownStateExitCount       "com.apple.iokit.hid.hes.unknownStateExitCount"

#define kAppleVendorID 1452

// 072BC077-E984-4C2A-BB72-D4769CE44FAF
#define kIOHIDEventSystemStatisticsFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x07, 0x2B, 0xC0, 0x77, 0xE9, 0x84, 0x4C, 0x2A, 0xBB, 0x72, 0xD4, 0x76, 0x9C, 0xE4, 0x4F, 0xAF)

extern "C" void * IOHIDEventSystemStatisticsFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);
static mach_timebase_info_data_t    sTimebaseInfo;

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
_last_motionstat_ts(0),
_keyServices(NULL),
_hesServices(NULL)
{
    bzero(&_pending_buttons, sizeof(_pending_buttons));
    bzero(&_pending_motionstats, sizeof(_pending_motionstats));
    bzero(&_pending_keystats, sizeof(_pending_keystats));
    bzero(&_pending_hesstats, sizeof(_pending_hesstats));
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
    _hesServices = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);
    
    _attachEvent = IOHIDEventCreateKeyboardEvent(kCFAllocatorDefault, 0,
                                                 kHIDPage_AppleVendorSmartCover,
                                                 kHIDUsage_AppleVendorSmartCover_Attach,
                                                 0, 0);
    
    if (!_keyServices || !_hesServices || !_attachEvent) {
        return false;
    }
    
    _awdConnection = std::make_shared<awd::AWDServerConnection>(AWDComponentId_IOHIDFamily);
    
    _awdConnection->RegisterQueriableMetricCallbackForIdentifier(AWDComponentId_IOHIDFamily, ^(uint32_t metricId __unused) {

    });
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
    
    if (_keyServices) {
        CFRelease(_keyServices);
        _keyServices = NULL;
    }
    
    if (_hesServices) {
        CFRelease(_hesServices);
        _hesServices = NULL;
    }
    
    if (_attachEvent) {
        CFRelease(_attachEvent);
        _attachEvent = NULL;
    }
    
    _awdConnection.reset();
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
    IOHIDEventRef copiedEvent;
    
    if ( (copiedEvent = IOHIDServiceCopyEvent(service, kIOHIDEventTypeKeyboard, _attachEvent, 0)) )
    {
        CFRelease(copiedEvent);
        
        if (_hesServices)
            CFSetAddValue(_hesServices, service);
        
        HIDLogDebug("HES service registered");
    }
    
    if ( IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) )
    {
        CFTypeRef   obj;
        uint32_t    vendorID = 0;
        
        obj = IOHIDServiceGetProperty(service, CFSTR(kIOHIDVendorIDKey));
        if (obj && CFGetTypeID(obj) == CFNumberGetTypeID())
            CFNumberGetValue((CFNumberRef)obj, kCFNumberIntType, &vendorID);
        
        // Check for Apple vendorID.
        if (vendorID != kAppleVendorID)
            return;

        // Check for AccessoryID Bus transport.
        obj = IOHIDServiceGetProperty(service, CFSTR(kIOHIDTransportKey));
        if (!obj || CFGetTypeID(obj) != CFStringGetTypeID() || !CFEqual((CFStringRef)obj, CFSTR(kIOHIDTransportAIDBValue)))
            return;

        ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryKeyboardEnumerationCountKey), 1);
        
        if (_keyServices)
            CFSetAddValue(_keyServices, service);
        
        HIDLogDebug("Apple Keyboard registered");
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
    
    if (_hesServices && CFSetGetValue(_hesServices, service)) {
        CFSetRemoveValue(_hesServices, service);
        
        HIDLogDebug("HES service unregistered");
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

        if (_hesServices) {
            __block CFMutableArrayRefWrap hesServices;
            _IOHIDCFSetApplyBlock (_keyServices, ^(CFTypeRef value) {
                hesServices.Append(IOHIDServiceGetRegistryID((IOHIDServiceRef)value));
            });
            debug.SetValueForKey(CFSTR("HESServices"), hesServices.Reference());
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
    __block MotionStats           motionStats      = {};
    __block KeyStats              keyStats         = {};
    __block HESStats              hesStats         = {};
    CFMutableDictionaryRefWrap    adclientKeys     = NULL;
    
    dispatch_sync(_dispatch_queue, ^{
        bcopy(&_pending_buttons, &buttons, sizeof(Buttons));
        bzero(&_pending_buttons, sizeof(Buttons));

        bcopy(&_pending_motionstats, &motionStats, sizeof(MotionStats));
        bzero(&_pending_motionstats, sizeof(MotionStats));
        
        bcopy(&_pending_keystats, &keyStats, sizeof(KeyStats));
        bzero(&_pending_keystats, sizeof(KeyStats));
        
        bcopy(&_pending_hesstats, &hesStats, sizeof(HESStats));
        bzero(&_pending_hesstats, sizeof(HESStats));
    });
    
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryHomeButtonWakeCountKey), buttons.home_wake);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryPowerButtonWakeCountKey), buttons.power_wake);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryPowerButtonSleepCountKey), buttons.power_sleep);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryButtonsHighLatencyCountKey), buttons.high_latency);

    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryMotionAccelSampleCount), motionStats.accel_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryMotionGyroSampleCount), motionStats.gyro_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryMotionMagSampleCount), motionStats.mag_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryMotionPressureSampleCount), motionStats.pressure_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryMotionDeviceMotionSampleCount), motionStats.devmotion_count);
    
    HIDLogDebug("Apple Keyboard char: %d symbol: %d spacebar: %d arrow: %d cursor: %d modifier: %d ",
                keyStats.character_count, keyStats.symbol_count, keyStats.spacebar_count,
                keyStats.arrow_count, keyStats.cursor_count, keyStats.modifier_count);
    
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryAppleKeyboardCharacterCountKey), keyStats.character_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryAppleKeyboardSymbolCountKey), keyStats.symbol_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryAppleKeyboardSpacebarCountKey), keyStats.spacebar_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryAppleKeyboardArrowCountKey), keyStats.arrow_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryAppleKeyboardCursorCountKey), keyStats.cursor_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryAppleKeyboardModifierCountKey), keyStats.modifier_count);
    
    HIDLogDebug("Cover HES open: %d close: %d <50ms: %d 50-100ms: %d 100-250ms: %d 250-500ms: %d 500-1000ms: %d",
                hesStats.open_count, hesStats.close_count, hesStats.toggled_50ms,
                hesStats.toggled_50_100ms, hesStats.toggled_100_250ms, hesStats.toggled_250_500ms, hesStats.toggled_500_1000ms);
    
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESOpenCount), hesStats.open_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESCloseCount), hesStats.close_count);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESToggled50ms), hesStats.toggled_50ms);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESToggled50to100ms), hesStats.toggled_50_100ms);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESToggled100to250ms), hesStats.toggled_100_250ms);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESToggled250to500ms), hesStats.toggled_250_500ms);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESToggled500to1000ms), hesStats.toggled_500_1000ms);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESUnknownStateEnterCount), hesStats.unknownStateEnter);
    adclientKeys.SetValueForKey(CFSTR(kAggregateDictionaryCoverHESUnknownStateExitCount), hesStats.unknownStateExit);
    
    ADClientBatchKeys(adclientKeys.Reference(), NULL);
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

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::collectHESStats
//------------------------------------------------------------------------------
bool IOHIDEventSystemStatistics::collectHESStats(IOHIDServiceRef sender, IOHIDEventRef event)
{
    uint16_t            usagePage;
    uint16_t            usage;
    bool                down;
    uint64_t            ts;
    static uint64_t     lastOpenTS = 0;
    bool    result      =  false;
    
    if (sender == NULL || _hesServices == NULL)
        return result;
    
    // Collect stats only for keyboard events.
    if (IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard) {
        return result;
    }
    
    // Check if the event is from a registered HES service.
    if (!CFSetGetValue(_hesServices, sender)) {
        return result;
    }
    
    usagePage = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    usage     = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    down      = (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown) == 1);
    ts        = IOHIDEventGetTimeStamp(event);
    
    if (usagePage != kHIDPage_AppleVendorSmartCover) {
        return result;
    }

    awdmetrics::IOHIDFamilyCoverHESTEvents  metric;
    
    if (usage == kHIDUsage_AppleVendorSmartCover_Open) {
        result = true;
        if (!down) {
            HIDLogDebug("Cover HES open");
            _pending_hesstats.open_count++;
             metric.openCount (_pending_hesstats.open_count);
            lastOpenTS = ts;
        } else {
            HIDLogDebug("Cover HES close");
            _pending_hesstats.close_count++;
            metric.closeCount(_pending_hesstats.close_count);
            // Compute MS since last cover open.
            if (ts >= lastOpenTS) {
                uint64_t elapsedMS = _IOHIDGetTimestampDelta(ts, lastOpenTS, kMillisecondScale);
                
                if (elapsedMS < 50) {
                    HIDLogDebug("Cover HES toggle <50ms: %llu", elapsedMS);
                    _pending_hesstats.toggled_50ms++;
                    metric.toggled50ms (_pending_hesstats.toggled_50ms);
                }
                else if (elapsedMS < 100) {
                    HIDLogDebug("Cover HES toggle 50 to 100ms: %llu", elapsedMS);
                    _pending_hesstats.toggled_50_100ms++;
                    metric.toggled50100ms (_pending_hesstats.toggled_50_100ms);
                }
                else if (elapsedMS < 250) {
                    HIDLogDebug("Cover HES toggle 100 to 250ms: %llu", elapsedMS);
                    _pending_hesstats.toggled_100_250ms++;
                    metric.toggled100250ms (_pending_hesstats.toggled_100_250ms);

                }
                else if (elapsedMS < 500) {
                    HIDLogDebug("Cover HES toggle 250 to 500ms: %llu", elapsedMS);
                    _pending_hesstats.toggled_250_500ms++;
                     metric.toggled250500ms (_pending_hesstats.toggled_250_500ms);
                }
                else if (elapsedMS < 1000) {
                    HIDLogDebug("Cover HES toggle 500 to 1000ms: %llu", elapsedMS);
                    _pending_hesstats.toggled_500_1000ms++;
                    metric.toggled5001000ms(_pending_hesstats.toggled_500_1000ms);
                }
            }
        }
        AWDPostMetric (AWDMetricId_IOHIDFamily_CoverHESTEvents, metric);
    } else if ( usage == kHIDUsage_AppleVendorSmartCover_StateUnknown) {
        result = true;
        if (down) {
            HIDLogDebug("Cover HES unknown state enter");
            _pending_hesstats.unknownStateEnter++;
            metric.unknownStateEnter(_pending_hesstats.unknownStateEnter);
        } else {
            HIDLogDebug("Cover HES unknown state exit");
            _pending_hesstats.unknownStateExit++;
            metric.unknownStateExit(_pending_hesstats.unknownStateExit);
        }
        AWDPostMetric (AWDMetricId_IOHIDFamily_CoverHESTEvents, metric);
    }
    // Return true to signal AggD update.
    return result;
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::collectMotionStats
//------------------------------------------------------------------------------
bool IOHIDEventSystemStatistics::collectMotionStats(IOHIDServiceRef sender __unused, IOHIDEventRef event)
{
    // Synchronize writing to motionstats on the session queue
    switch (IOHIDEventGetType(event)) {
        case kIOHIDEventTypeAccelerometer:
            _pending_motionstats.accel_count++;
            break;
        case kIOHIDEventTypeGyro:
            _pending_motionstats.gyro_count++;
            break;
        case kIOHIDEventTypeCompass:
            _pending_motionstats.mag_count++;
            break;
        case kIOHIDEventTypeAtmosphericPressure:
            _pending_motionstats.pressure_count++;
            break;
        case kIOHIDEventTypeVendorDefined:
            if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsagePage) == kHIDPage_AppleVendorMotion &&
                IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendorMotion_DeviceMotion) {
                _pending_motionstats.devmotion_count++;
            }
            break;
    }

    uint64_t event_ts = IOHIDEventGetTimeStamp(event);
    uint64_t elapsed_ts = event_ts - _last_motionstat_ts;
    if ( sTimebaseInfo.denom == 0 ) {
        (void) mach_timebase_info(&sTimebaseInfo);
    }
    elapsed_ts = elapsed_ts * sTimebaseInfo.numer / sTimebaseInfo.denom;
    float elapsed_secs = (float)elapsed_ts / NSEC_PER_SEC;

    // signal aggd low priority queue if >1min has passed since last committed sample
    // this is easier than tracking the first uncommitted sample
    if (elapsed_secs > kAggregateDictionaryMotionFlushInterval) {
        _last_motionstat_ts = event_ts;
        return true; // signal pending source
    } else {
        return false;
    }
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
            
            if (collectHESStats(sender, event)) {
                
                signal = true;
            }
            else if (collectKeyStats(sender, event)) {
                
                signal = true;
            }
            else if ((IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard)
                && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage) == kHIDPage_Consumer)) {
                
                if (IOHIDEventGetLatency(event, kMillisecondScale) > kAggregateDictionaryButtonsHighLatencyMinMS) {
                
                    switch (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage) ) {
                        case kHIDUsage_Csmr_Menu:
                        case kHIDUsage_Csmr_Power:
                        case kHIDUsage_Csmr_VolumeDecrement:
                        case kHIDUsage_Csmr_VolumeIncrement:
                            HIDLogDebug("Recording high latency button event");
                            _pending_buttons.high_latency++;
                            signal = true;
                            break;
                        default:
                            break;
                    }
                }
                
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
            else {
                signal = collectMotionStats(sender, event);
            }

            if ( signal )
                dispatch_source_merge_data(_pending_source, 1);
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
