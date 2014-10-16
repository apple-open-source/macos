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
    NULL,
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
_logButtonFiltering(false),
_logStrings(NULL),
_logfd(-1),
_asl(NULL)
{
    bzero(&_pending_buttons, sizeof(_pending_buttons));
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
        ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryKeyboardEnumerationCountKey), 1);
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
    
    dispatch_sync(_dispatch_queue, ^{
        bcopy(&_pending_buttons, &buttons, sizeof(Buttons));
        bzero(&_pending_buttons, sizeof(Buttons));
        
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
            
            if ((IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard)
                && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)
                && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage) == kHIDPage_Consumer)) {
                
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
                                                                                         "ts=%0.9f,action=down,button=%s",
                                                                                         kCFStringEncodingUTF8),
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
            }
            
            if ( signal )
                dispatch_source_merge_data(_pending_source, 1);
        }
        
    }
    
    return event;
}
