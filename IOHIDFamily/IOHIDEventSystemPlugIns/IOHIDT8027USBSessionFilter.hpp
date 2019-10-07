//
//  IOHIDT8027USBSessionFilter.hpp
//  IOHIDT8027USBSessionFilter
//
//  Created by Paul Doerr on 11/30/18.
//

#ifndef IOHIDT8027USBSessionFilter_hpp
#define IOHIDT8027USBSessionFilter_hpp

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <queue>
#include <IOKit/pwr_mgt/IOPMLib.h>
#include <IOKit/pwr_mgt/IOPMLibDefs.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#include "CF.h"

//
// IOHIDT8027USBSessionFilter's purpose is to mitigate a HW issue with T8027 SOC.
// T8027's USB controller is not able to resume the state of devices when waking
// from sleep. Instead, devices are immediately recreating. If a connected USB
// device causes the wake, the associated data (and HID report) will be lost.
// This plugin takes a power assertion when certain USB HID devices are attached
// or interacted with to prevent full sleep from happening when they are in use.
//

class IOHIDT8027USBSessionFilter
{
    static constexpr uint64_t DEFAULT_ASSERTION_TIMEOUT = 60*60*2; // 2 hours in seconds
    static CFStringRef const ASSERTION_NAME;

    IOHIDSessionFilterPlugInInterface * _sessionInterface;
    CFUUIDRef                           _factoryID;
    UInt32                              _refCount;
    dispatch_queue_t                    _queue;

    IONotificationPortRef               _port;
    io_iterator_t                       _iterator;
    CFMutableSetRefWrap                 _usbHIDServices;
    dispatch_source_t                   _timer;
    uint64_t                            _assertionTimeout;
    IOPMAssertionID                     _assertionID;
    bool                                _hasT8027USB;
    bool                                _asserting;
    bool                                _timedOut;

public:
    IOHIDT8027USBSessionFilter(CFUUIDRef factoryID);
    ~IOHIDT8027USBSessionFilter();

    HRESULT         QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG           AddRef();
    ULONG           Release();

    IOHIDEventRef   filter(IOHIDServiceRef sender, IOHIDEventRef event);
    void            registerService(IOHIDServiceRef service);
    void            unregisterService(IOHIDServiceRef service);
    void            scheduleWithDispatchQueue(dispatch_queue_t queue);
    void            unscheduleFromDispatchQueue(dispatch_queue_t queue);
    void            setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client);
    CFTypeRef       getPropertyForClient(CFStringRef key, CFTypeRef client);

private:
    static IOHIDSessionFilterPlugInInterface sIOHIDT8027USBSessionFilterFtbl;
    static HRESULT          _QueryInterface( void *self, REFIID iid, LPVOID *ppv );
    static ULONG            _AddRef(void *self);
    static ULONG            _Release(void *self);

    static IOHIDEventRef    _filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event);
    static void             _registerService(void * self, IOHIDServiceRef service);
    static void             _unregisterService(void * self, IOHIDServiceRef service);
    static void             _scheduleWithDispatchQueue(void * self, dispatch_queue_t queue);
    static void             _unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue);
    static void             _setPropertyForClient(void * self, CFStringRef key, CFTypeRef property, CFTypeRef client);
    static CFTypeRef        _getPropertyForClient(void * self, CFStringRef key, CFTypeRef client);

    static void             _serviceNotificationCallback(void * refcon, io_iterator_t iterator);
    void                    serviceNotificationCallback(io_iterator_t iterator);

    void                    preventIdleSleepAssertion(CFStringRef detail = NULL);
    void                    releaseIdleSleepAssertion();
    bool                    initTimer();
    void                    timerHandler();


    void serialize(CFMutableDictionaryRef dict) const;

    IOHIDT8027USBSessionFilter() = delete;
    IOHIDT8027USBSessionFilter(const IOHIDT8027USBSessionFilter &) = delete;
    IOHIDT8027USBSessionFilter &operator=(const IOHIDT8027USBSessionFilter &) = delete;
};

#endif /* IOHIDT8027USBSessionFilter_hpp */
