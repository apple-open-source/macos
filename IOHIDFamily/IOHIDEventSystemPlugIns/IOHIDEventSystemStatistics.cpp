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
#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include "IOHIDEventSystemStatistics.h"
#include "IOHIDDebug.h"


#define kAggregateDictionaryKeyboardEnumerationCountKey         "com.apple.iokit.hid.keyboard.enumerationCount"
#define kAggregateDictionaryHomeButtonWakeCountKey              "com.apple.iokit.hid.homeButton.wakeCount"
#define kAggregateDictionaryPowerButtonWakeCountKey             "com.apple.iokit.hid.powerButton.wakeCount"
#define kAggregateDictionaryPowerButtonSleepCountKey            "com.apple.iokit.hid.powerButton.sleepCount"

#define kAggregateDictionaryPowerButtonPressedCountKey              "com.apple.iokit.hid.powerButton.pressed"
#define kAggregateDictionaryPowerButtonFilteredCountKey             "com.apple.iokit.hid.powerButton.filtered"
#define kAggregateDictionaryVolumeIncrementButtonPressedCountKey    "com.apple.iokit.hid.volumeIncrementButton.pressed"
#define kAggregateDictionaryVolumeIncrementButtonFilteredCountKey   "com.apple.iokit.hid.volumeIncrementButton.filtered"
#define kAggregateDictionaryVolumeDecrementButtonPressedCountKey    "com.apple.iokit.hid.volumeDecrementButton.pressed"
#define kAggregateDictionaryVolumeDecrementButtonFilteredCountKey   "com.apple.iokit.hid.volumeDecrementButton.filtered"

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

#define kAppleVendorID 1452

// 072BC077-E984-4C2A-BB72-D4769CE44FAF
#define kIOHIDEventSystemStatisticsFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x07, 0x2B, 0xC0, 0x77, 0xE9, 0x84, 0x4C, 0x2A, 0xBB, 0x72, 0xD4, 0x76, 0x9C, 0xE4, 0x4F, 0xAF)

#define kStringLength   128

extern "C" void * IOHIDEventSystemStatisticsFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);
static mach_timebase_info_data_t    sTimebaseInfo;

static const char kButtonPower[]            = "power/hold";
static const char kButtonVolumeIncrement[]  = "volume_inc";
static const char kButtonVolumeDecrement[]  = "volume_dec";
static const char kButtonMenu[]             = "menu/home";

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
    NULL,
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
_pending_source(0),
_dispatch_queue(0),
_last_motionstat_ts(0),
_keyServices(NULL),
_logButtonFiltering(false),
_logStrings(NULL),
_logfd(-1),
_asl(NULL)
{
    bzero(&_pending_buttons, sizeof(_pending_buttons));
    bzero(&_pending_motionstats, sizeof(_pending_motionstats));
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
    CFTypeRef            bootArgs = nil;
    io_registry_entry_t  entry    = IO_OBJECT_NULL;
    
    (void)session;
    (void)options;
    
    entry = IORegistryEntryFromPath(kIOMasterPortDefault, "IODeviceTree:/options");
    if(entry){
        bootArgs = IORegistryEntryCreateCFProperty(entry, CFSTR("boot-args"), nil, 0);
        if (bootArgs){
            if (CFGetTypeID(bootArgs) == CFStringGetTypeID()){
                CFRange         findRange;
                CFStringRef     bootArgsString = (CFStringRef)bootArgs;
                
                findRange = CFStringFind(bootArgsString, CFSTR("opposing-button-logging"), 0);
                
                if (findRange.length != 0)
                    _logButtonFiltering = true;
            }
            CFRelease(bootArgs);
            IOObjectRelease(entry);
        }
    }
    
    if (_logButtonFiltering) {
        _logStrings = CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
        _asl = asl_open("ButtonLogging", "Button Filtering Information", 0);
        
        _logfd = ::open("/var/mobile/Library/Logs/button.log", O_CREAT | O_APPEND | O_RDWR, 0644);
        
        if ((_logfd != -1) && (_asl != NULL))
            asl_add_log_file(_asl, _logfd);
    }
    
    _keyServices = CFSetCreateMutable(kCFAllocatorDefault, 0, &kCFTypeSetCallBacks);

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
    
    if (_logStrings) {
        CFRelease(_logStrings);
        _logStrings = NULL;
    }
    
    if (_asl) {
        asl_close(_asl);
        if (_logfd != -1) ::close(_logfd);
    }
    
    if (_keyServices) {
        CFRelease(_keyServices);
        _keyServices = NULL;
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
    if ( IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) )
    {
        CFTypeRef   obj;
        uint32_t    vendorID = 0;
        
        ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryKeyboardEnumerationCountKey), 1);
        
        obj = IOHIDServiceGetProperty(service, CFSTR(kIOHIDVendorIDKey));
        if (obj && CFGetTypeID(obj) == CFNumberGetTypeID())
            CFNumberGetValue((CFNumberRef)obj, kCFNumberIntType, &vendorID);
        
        // Check for Apple vendorID.
        if (vendorID != kAppleVendorID)
            return;
        
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
        dispatch_release(_pending_source);
        _pending_source = NULL;
    }
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
    __block Buttons     buttons     = {};
    __block CFArrayRef  logStrings  = NULL;
    __block MotionStats motionStats = {};
    __block KeyStats    keyStats    = {};
    dispatch_sync(_dispatch_queue, ^{
        bcopy(&_pending_buttons, &buttons, sizeof(Buttons));
        bzero(&_pending_buttons, sizeof(Buttons));

        bcopy(&_pending_motionstats, &motionStats, sizeof(MotionStats));
        bzero(&_pending_motionstats, sizeof(MotionStats));
        
        bcopy(&_pending_keystats, &keyStats, sizeof(KeyStats));
        bzero(&_pending_keystats, sizeof(KeyStats));

        if (_logStrings) {
            logStrings = CFArrayCreateCopy(kCFAllocatorDefault, _logStrings);
            CFArrayRemoveAllValues(_logStrings);
        }
    });
    
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryHomeButtonWakeCountKey), buttons.home_wake);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryPowerButtonWakeCountKey), buttons.power_wake);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryPowerButtonSleepCountKey), buttons.power_sleep);
    
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryPowerButtonPressedCountKey), buttons.power);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryPowerButtonFilteredCountKey), buttons.power_filtered);
    
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryVolumeIncrementButtonPressedCountKey), buttons.volume_increment);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryVolumeIncrementButtonFilteredCountKey), buttons.volume_increment_filtered);
    
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryVolumeDecrementButtonPressedCountKey), buttons.volume_decrement);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryVolumeDecrementButtonFilteredCountKey), buttons.volume_decrement_filtered);
    
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryButtonsHighLatencyCountKey), buttons.high_latency);

    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryMotionAccelSampleCount), motionStats.accel_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryMotionGyroSampleCount), motionStats.gyro_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryMotionMagSampleCount), motionStats.mag_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryMotionPressureSampleCount), motionStats.pressure_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryMotionDeviceMotionSampleCount), motionStats.devmotion_count);
    
    HIDLogDebug("Apple Keyboard char: %d symbol: %d spacebar: %d arrow: %d cursor: %d modifier: %d ",
                keyStats.character_count, keyStats.symbol_count, keyStats.spacebar_count,
                keyStats.arrow_count, keyStats.cursor_count, keyStats.modifier_count);
    
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryAppleKeyboardCharacterCountKey), keyStats.character_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryAppleKeyboardSymbolCountKey), keyStats.symbol_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryAppleKeyboardSpacebarCountKey), keyStats.spacebar_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryAppleKeyboardArrowCountKey), keyStats.arrow_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryAppleKeyboardCursorCountKey), keyStats.cursor_count);
    ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryAppleKeyboardModifierCountKey), keyStats.modifier_count);

    if (logStrings) {
        for (int i = 0; i < CFArrayGetCount(logStrings); i++) {
            CFStringRef cfstr;
            const char * cstr;
            
            cfstr = (CFStringRef) CFArrayGetValueAtIndex(logStrings, i);
            if (!cfstr) continue;
            
            cstr = CFStringGetCStringPtr(cfstr, kCFStringEncodingUTF8);
            if (!cstr) continue;
            
            asl_log(_asl, NULL, ASL_LEVEL_NOTICE, "%s", cstr);
        }
    
        CFRelease(logStrings);
    }
}

//------------------------------------------------------------------------------
// IOHIDEventSystemStatistics::collectKeyStats
//------------------------------------------------------------------------------
bool IOHIDEventSystemStatistics::collectKeyStats(IOHIDServiceRef sender, IOHIDEventRef event)
{
    bool        serviceMatch = false;
    uint16_t    usagePage;
    uint16_t    usage;
    
    if (sender == NULL || _keyServices == NULL)
        return false;
    
    // Check if the event is from a registered Apple Keyboard service.
    if (!CFSetGetValue(_keyServices, sender))
        return false;
    
    // Collect stats only for keyboard event key-downs.
    if (IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard ||
        !IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown))
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
// IOHIDEventSystemStatistics::collectMotionStats
//------------------------------------------------------------------------------
bool IOHIDEventSystemStatistics::collectMotionStats(IOHIDServiceRef sender, IOHIDEventRef event)
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
    const char *    button;
    uint64_t        ts;
    float           secs;
    
    (void) sender;
    
    if ( _pending_source ) {
        if ( event ) {
            bool signal = false;
            
            if (collectKeyStats(sender, event)) {
                
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
                            button = kButtonMenu;
                            if ( !_displayState )
                                _pending_buttons.home_wake++;
                            break;
                        case kHIDUsage_Csmr_Power:
                            _pending_buttons.power++;
                            button = kButtonPower;
                            if ( !_displayState )
                                _pending_buttons.power_wake++;
                            else
                                _pending_buttons.power_sleep++;
                            break;
                        case kHIDUsage_Csmr_VolumeDecrement:
                            _pending_buttons.volume_decrement++;
                            button = kButtonVolumeDecrement;
                            break;
                        case kHIDUsage_Csmr_VolumeIncrement:
                            _pending_buttons.volume_increment++;
                            button = kButtonVolumeIncrement;
                            break;
                        default:
                            signal = false;
                            break;
                    }
                }
                
                if (signal && _logButtonFiltering) {
                    if ( sTimebaseInfo.denom == 0 ) {
                        (void) mach_timebase_info(&sTimebaseInfo);
                    }
                    ts = IOHIDEventGetTimeStamp(event);
                    ts = ts * sTimebaseInfo.numer / sTimebaseInfo.denom;
                    secs = (float)ts / NSEC_PER_SEC;
                    
                    CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault,
                                                               0,
                                                               CFSTR("ts=%0.9f,action=down,button=%s"),
                                                               secs,
                                                               button ? button : "unknown");
                    
                    if (str) {
                        CFArrayAppendValue(_logStrings, str);
                        CFRelease(str);
                    }
                }
            }
            else if ((IOHIDEventGetType(event) == kIOHIDEventTypeVendorDefined)
                     && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsagePage) == kHIDPage_AppleVendorFilteredEvent)
                     && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage) == kIOHIDEventTypeKeyboard)) {
                
                CFIndex dataLength = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedDataLength);
                if ( dataLength >= sizeof(IOHIDKeyboardEventData)) {
                    IOHIDKeyboardEventData * data = (IOHIDKeyboardEventData*)IOHIDEventGetDataValue(event, kIOHIDEventFieldVendorDefinedData);
                    
                    if ( data->usagePage == kHIDPage_Consumer && data->down ) {
                        
                        signal = true;
                        
                        switch ( data->usage ) {
                            case kHIDUsage_Csmr_Power:
                                _pending_buttons.power_filtered++;
                                button = kButtonPower;
                                break;
                            case kHIDUsage_Csmr_VolumeDecrement:
                                _pending_buttons.volume_decrement_filtered++;
                                button = kButtonVolumeDecrement;
                                break;
                            case kHIDUsage_Csmr_VolumeIncrement:
                                _pending_buttons.volume_increment_filtered++;
                                button = kButtonVolumeIncrement;
                                break;
                            default:
                                signal = false;
                                break;
                        }
                    }
                    
                    if (signal && _logButtonFiltering) {
                        if ( sTimebaseInfo.denom == 0 ) {
                            (void) mach_timebase_info(&sTimebaseInfo);
                        }
                        
                        ts = IOHIDEventGetTimeStamp(event);
                        ts = ts * sTimebaseInfo.numer / sTimebaseInfo.denom;
                        secs = (float)ts / NSEC_PER_SEC;
                        
                        CFStringRef str = CFStringCreateWithFormat(kCFAllocatorDefault,
                                                                   0,
                                                                   CFStringCreateWithCString(kCFAllocatorDefault,
                                                                                             "ts=%0.9f,action=filtered,button=%s",
                                                                                             kCFStringEncodingUTF8) ,
                                                                   secs,
                                                                   button ? button : "unknown");
                        
                        if (str) {
                            CFArrayAppendValue(_logStrings, str);
                            CFRelease(str);
                        }
                    }
                }
            } else {
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
