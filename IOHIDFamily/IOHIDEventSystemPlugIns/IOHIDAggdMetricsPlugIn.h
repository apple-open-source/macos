/*
 *  IOHIDAggdMetricsPlugIn.h
 *  IOHIDEventSystemPlugIns
 *
 *  Created by Rob Yepez on 05/21/2013.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
 */

#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif
class IOHIDAggdMetricsPlugIn
{
public:
    IOHIDAggdMetricsPlugIn(CFUUIDRef factoryID);
    ~IOHIDAggdMetricsPlugIn();
    HRESULT QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG AddRef();
    ULONG Release();
    
    IOHIDEventRef filter(IOHIDServiceRef sender, IOHIDEventRef event);
    void registerService(IOHIDServiceRef service);
    void setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client);
private:
    IOHIDSessionFilterPlugInInterface *_sessionInterface;
    CFUUIDRef                   _factoryID;
    UInt32                      _refCount;
    
    float                       _factor;
    
private:
    static IOHIDSessionFilterPlugInInterface sIOHIDAggdMetricsPlugInFtbl;
    static HRESULT QueryInterface( void *self, REFIID iid, LPVOID *ppv );
    static ULONG AddRef( void *self );
    static ULONG Release( void *self );
    
    static IOHIDEventRef filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event);
    static IOHIDEventRef copyEvent(void * self, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options);
    
    static boolean_t open(void * self, IOHIDSessionRef inSession, IOOptionBits options);
    static void close(void * self, IOHIDSessionRef inSession, IOOptionBits options);
    static void registerDisplay(void * self, IOHIDDisplayRef display);
    static void unregisterDisplay(void * self, IOHIDDisplayRef display);
    static void registerService(void * self, IOHIDServiceRef service);
    static void unregisterService(void * self, IOHIDServiceRef service);
    static void scheduleWithRunLoop(void * self, CFRunLoopRef runLoop, CFStringRef runLoopMode);
    static void unscheduleFromRunLoop(void * self, CFRunLoopRef runLoop, CFStringRef runLoopMode);
    static CFTypeRef getPropertyForClient(void * self, CFStringRef key, CFTypeRef client);
    static void setPropertyForClient(void * self, CFStringRef key, CFTypeRef property, CFTypeRef client);
    
private:
    IOHIDAggdMetricsPlugIn();
    IOHIDAggdMetricsPlugIn(const IOHIDAggdMetricsPlugIn &);
    IOHIDAggdMetricsPlugIn &operator=(const IOHIDAggdMetricsPlugIn &);
};
