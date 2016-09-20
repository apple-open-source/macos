/*
 *  IOHIDNXEventTranslatorSessionFilter.h
 *  IOHIDEventSystemPlugIns
 *
 *  Created by Yevgen Goryachok on 12/07/2015.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
 */

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif
#include <IOKit/hidsystem/event_status_driver.h>
#include <mach/mach_time.h>
#include <chrono>
#include <queue>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include  <shared_mutex>
#include "CF.h"


#define kIOHIDPowerOnThresholdMS                    (500)            // 1/2 second
#define kIOHIDDisplaySleepAbortThresholdMS          (5000)           // 5 seconds
#define kIOHIDDisplaySleepPolicyUpgradeThresholdMS  (25)
#define LOG_MAX_ENTRIES                             (50)
#define kIOHIDDisplayWakeAbortThresholdMS           (50)

struct LogEntry {
    std::chrono::time_point<std::chrono::steady_clock>  displayTime;
    IOHIDEventSenderID  serviceID;
};

class IOHIDNXEventTranslatorSessionFilter
{
public:
    IOHIDNXEventTranslatorSessionFilter(CFUUIDRef factoryID);
    ~IOHIDNXEventTranslatorSessionFilter();
    HRESULT QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG AddRef();
    ULONG Release();
    
    IOHIDEventRef filter(IOHIDServiceRef sender, IOHIDEventRef event);
    boolean_t open(IOHIDSessionRef session, IOOptionBits options);
    void close(IOHIDSessionRef session, IOOptionBits options);
    void registerService(IOHIDServiceRef service);
    void unregisterService(IOHIDServiceRef service);
    void scheduleWithDispatchQueue(dispatch_queue_t queue);
    void unscheduleFromDispatchQueue(dispatch_queue_t queue);
    void setPropertyForClient (CFStringRef key, CFTypeRef property, CFTypeRef client);
    CFTypeRef getPropertyForClient (CFStringRef key, CFTypeRef client);
  
private:

    IOHIDSessionFilterPlugInInterface *_sessionInterface;
    CFUUIDRef                       _factoryID;
    UInt32                          _refCount;
    dispatch_queue_t                _dispatch_queue;

    uint32_t                        _globalModifiers;
    NXEventHandle                   _hidSystem;
    IOHIDPointerEventTranslatorRef  _translator;
    io_connect_t                    _powerConnect;
    io_object_t                     _powerNotifier;
    IONotificationPortRef           _powerPort;
    uint32_t                        _powerState;
    uint32_t                        _powerOnThresholdEventCount;
    uint32_t                        _powerOnThreshold;
    IONotificationPortRef           _port;
    io_object_t                     _wranglerNotifier;
    io_service_t                    _wrangler;
    uint32_t                        _displayState;
    uint32_t                        _displaySleepAbortThreshold;
    uint32_t                        _displayWakeAbortThreshold;
    static IOPMAssertionID          _AssertionID;
  
    CFMutableDictionaryRefWrap      _modifiers;
    CFMutableDictionaryRefWrap      _companions;
    CFMutableSetRefWrap             _keyboards;
    CFMutableSetRefWrap             _reportModifiers;
    CFMutableSetRefWrap             _updateModifiers;
  
  
    //typedef  std::chrono::steady_clock clock_type_;
    typedef  std::chrono::steady_clock clock;
    typedef  std::chrono::time_point<std::chrono::steady_clock> time_point;
    #define  duration_cast_ms std::chrono::duration_cast<std::chrono::milliseconds>
    #define  duration_cast_us std::chrono::duration_cast<std::chrono::microseconds>
    
    time_point _powerStateChangeTime;
    time_point _displayStateChangeTime;
    time_point _displayTickleTime;
    uint64_t   _maxDisplayTickleDuration;
  
    std::queue<LogEntry> _displayLog;
    
private:

    static IOHIDSessionFilterPlugInInterface sIOHIDNXEventTranslatorSessionFilterFtbl;
    static HRESULT QueryInterface( void *self, REFIID iid, LPVOID *ppv );
    static ULONG AddRef( void *self );
    static ULONG Release( void *self );
    
    static IOHIDEventRef filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event);

    static boolean_t open(void * self, IOHIDSessionRef inSession, IOOptionBits options);
    static void close(void * self, IOHIDSessionRef inSession, IOOptionBits options);
    static void registerService(void * self, IOHIDServiceRef service);
    static void unregisterService(void * self, IOHIDServiceRef service);
  
    static void scheduleWithDispatchQueue(void * self, dispatch_queue_t queue);
    static void unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue);
    static void setPropertyForClient (void * self, CFStringRef key, CFTypeRef property, CFTypeRef client);
    static CFTypeRef getPropertyForClient (void * self, CFStringRef key, CFTypeRef client);

    void updateModifiers();
    void updateButtons();
    void updateActivity (bool active);
    void updateDisplayLog(IOHIDEventSenderID serviceID);
    
    IOHIDServiceRef getCompanionService(IOHIDServiceRef service);
    
    IOHIDEventRef powerStateFilter (IOHIDEventRef  event);
    static void powerNotificationCallback (void * refcon, io_service_t	service, uint32_t messageType, void * messageArgument);
    void powerNotificationCallback (io_service_t	service, uint32_t messageType, void * messageArgument);
  
    void displayMatchNotificationCallback (io_iterator_t iterator);
    static void displayMatchNotificationCallback (void * refcon, io_iterator_t iterator);
    static void displayNotificationCallback (void * refcon, io_service_t	service, uint32_t messageType, void * messageArgument);
    void displayNotificationCallback (io_service_t	service, uint32_t messageType, void * messageArgument);
    void displayTickle ();
    IOHIDEventRef displayStateFilter (IOHIDEventRef  event);
  
    boolean_t shouldCancelEvent (IOHIDEventRef  event);
 
    void serialize (CFMutableDictionaryRef dict) const;
    static std::string timePointToTimeString (time_point pt);
  
  
  
private:
    IOHIDNXEventTranslatorSessionFilter();
    IOHIDNXEventTranslatorSessionFilter(const IOHIDNXEventTranslatorSessionFilter &);
    IOHIDNXEventTranslatorSessionFilter &operator=(const IOHIDNXEventTranslatorSessionFilter &);
};
