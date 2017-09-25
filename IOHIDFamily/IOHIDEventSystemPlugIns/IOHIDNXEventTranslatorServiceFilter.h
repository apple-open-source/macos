//
//  IOHIDNXEventTranslatorServiceFilter.h
//  IOHIDFamily
//
//  Created by Yevgen Goryachok 11/04/15.
//
//

#ifndef _IOHIDFamily_IOHIDNXEventTranslatorServiceFilter_
#define _IOHIDFamily_IOHIDNXEventTranslatorServiceFilter_
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include "IOHIDEventTranslation.h"

#define kHIDEventTranslationSupport         "HIDEventTranslationSupport"
#define kHIDEventTranslationModifierFlags   "HIDEventTranslationModifierFlags"

class IOHIDNXEventTranslatorServiceFilter
{
public:
    IOHIDNXEventTranslatorServiceFilter(CFUUIDRef factoryID);
    ~IOHIDNXEventTranslatorServiceFilter();
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
    static IOHIDServiceFilterPlugInInterface  sIOHIDEventSystemStatisticsFtbl;
    IOHIDServiceFilterPlugInInterface         *_serviceInterface;
    CFUUIDRef                                 _factoryID;
    UInt32                                    _refCount;
    SInt32                                    _matchScore;
 
    static IOHIDServiceFilterPlugInInterface sIOHIDNXEventTranslatorServiceFilterFtbl;
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

    dispatch_queue_t                _queue;
    IOHIDServiceRef                 _service;
    IOHIDKeyboardEventTranslatorRef _translator;
  
    void serialize (CFMutableDictionaryRef dict) const;

private:
  
    IOHIDNXEventTranslatorServiceFilter();
    IOHIDNXEventTranslatorServiceFilter(const IOHIDNXEventTranslatorServiceFilter &);
    IOHIDNXEventTranslatorServiceFilter &operator=(const IOHIDNXEventTranslatorServiceFilter &);
};


#endif /* defined(_IOHIDFamily_IOHIDNXEventTranslatorServiceFilter_) */
