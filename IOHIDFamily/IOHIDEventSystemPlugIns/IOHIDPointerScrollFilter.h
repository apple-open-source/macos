//
//  IOHIDPointerScrollFilter.h
//  IOHIDFamily
//
//  Created by Yevgen Goryachok 10/30/15.
//
//

#ifndef _IOHIDFamily_IOHIDPointerScrollFilter_
#define _IOHIDFamily_IOHIDPointerScrollFilter_
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include "IOHIDAcceleration.hpp"
#include "CF.h"

#define kDefaultPointerResolutionFixed (400 << 16)

class IOHIDPointerScrollFilter
{
public:
    IOHIDPointerScrollFilter(CFUUIDRef factoryID);
    ~IOHIDPointerScrollFilter();
    HRESULT QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG AddRef();
    ULONG Release();
    
    SInt32 match(IOHIDServiceRef service, IOOptionBits options);
    IOHIDEventRef filter(IOHIDEventRef event);
    void open(IOHIDServiceRef session, IOOptionBits options);
    void close(IOHIDServiceRef session, IOOptionBits options);
    void registerService(IOHIDServiceRef service);
    void handlePendingStats();
    void scheduleWithDispatchQueue(dispatch_queue_t queue);
    void unscheduleFromDispatchQueue(dispatch_queue_t queue);
    CFTypeRef copyPropertyForClient(CFStringRef key, CFTypeRef client);
    void setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client);
    void setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon);
    
private:
    static IOHIDServiceFilterPlugInInterface sIOHIDEventSystemStatisticsFtbl;
    IOHIDServiceFilterPlugInInterface *_serviceInterface;
    CFUUIDRef                   _factoryID;
    UInt32                      _refCount;
    SInt32                      _matchScore;
 
    static IOHIDServiceFilterPlugInInterface sIOHIDPointerScrollFilterFtbl;
    static HRESULT QueryInterface( void *self, REFIID iid, LPVOID *ppv );
    static ULONG AddRef( void *self );
    static ULONG Release( void *self );
    
    static SInt32 match(void * self, IOHIDServiceRef service, IOOptionBits options);
    static IOHIDEventRef filter(void * self, IOHIDEventRef event);
    
    static void open(void * self, IOHIDServiceRef inService, IOOptionBits options);
    static void close(void * self, IOHIDServiceRef inSession, IOOptionBits options);
    
    static void scheduleWithDispatchQueue(void * self, dispatch_queue_t queue);
    static void unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue);

    static CFTypeRef copyPropertyForClient(void * self, CFStringRef key, CFTypeRef client);
    static void setPropertyForClient(void * self,CFStringRef key, CFTypeRef property, CFTypeRef client);
    
    IOHIDServiceEventCallback _eventCallback;
    void * _eventTarget;
    void * _eventContext;
    static void setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon);
  
    void setupAcceleration ();
    void setupPointerAcceleration(double pointerAccelerationMultiplier);
    void setupScrollAcceleration(double scrollAccelerationMultiplier);
  
    void accelerateChildrens(IOHIDEventRef event);
    void accelerateEvent(IOHIDEventRef event);
  
    static CFStringRef          _cachedPropertyList[];

    IOHIDAccelerator            *_pointerAccelerator;
    IOHIDAccelerator            *_scrollAccelerators[3];
    
    dispatch_queue_t            _queue;
    CFMutableDictionaryRefWrap  _property;
    CFMutableDictionaryRefWrap  _cachedProperty;

    IOHIDServiceRef             _service;
    double                      _pointerAcceleration;
    double                      _scrollAcceleration;
    boolean_t                   _leagacyShim;
    bool                        _pointerAccelerationSupported;
    bool                        _scrollAccelerationSupported;
  
    void serialize (CFMutableDictionaryRef dict) const;
  
    CFTypeRef copyCachedProperty (CFStringRef key) const;
  
  
private:
  
    IOHIDPointerScrollFilter();
    IOHIDPointerScrollFilter(const IOHIDPointerScrollFilter &);
    IOHIDPointerScrollFilter &operator=(const IOHIDPointerScrollFilter &);
};


#endif /* defined(_IOHIDFamily_IOHIDPointerScrollFilter_) */
