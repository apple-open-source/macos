//
//  IOHIDDFREventFilter.hpp
//  IOHIDFamily
//
//  Created by Matty on 8/16/16.
//
//

#ifndef IOHIDDFREventFilter_hpp
#define IOHIDDFREventFilter_hpp

#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#include <map>

#include "CF.h"
#include "IOHIDUtility.h"

class IOHIDDFREventFilter
{
public:
    IOHIDDFREventFilter(CFUUIDRef factoryID);
    ~IOHIDDFREventFilter();
    HRESULT QueryInterface(REFIID iid, LPVOID *ppv);
    ULONG AddRef();
    ULONG Release();
    
    IOHIDEventRef filter(IOHIDServiceRef sender, IOHIDEventRef event);
    boolean_t open(IOHIDSessionRef session, IOOptionBits options);
    void close(IOHIDSessionRef session, IOOptionBits options);
    void registerService(IOHIDServiceRef service);
    void unregisterService(IOHIDServiceRef service);
    void scheduleWithDispatchQueue(dispatch_queue_t queue);
    void unscheduleFromDispatchQueue(dispatch_queue_t queue);
    CFTypeRef getPropertyForClient(CFStringRef key, CFTypeRef client);
    void setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client);
    
private:
    IOHIDSessionFilterPlugInInterface   *_sessionInterface;
    CFUUIDRef                           _factoryID;
    UInt32                              _refCount;
    dispatch_queue_t                    _dispatchQueue;
    
    IOHIDServiceRef                     _keyboard;
    IOHIDServiceRef                     _dfr;
    IOHIDSessionRef                     _session;
    IOHIDEventRef                       _lastDFREvent;
    
    std::map<Key,KeyAttribute>          _activeKeys;
    boolean_t                           _keyboardFilterEnabled;
    boolean_t                           _touchIDFilterEnabled;
    boolean_t                           _touchInProgress;
    boolean_t                           _bioInProgress;
    boolean_t                           _cancel;
    
private:
    static IOHIDSessionFilterPlugInInterface sIOHIDDFREventFilterFtbl;
    
    static HRESULT QueryInterface(void *self, REFIID iid, LPVOID *ppv);
    static ULONG AddRef(void *self);
    static ULONG Release(void *self);
    
    static IOHIDEventRef filter(void *self, IOHIDServiceRef sender, IOHIDEventRef event);
    static boolean_t open(void *self, IOHIDSessionRef inSession, IOOptionBits options);
    static void close(void *self, IOHIDSessionRef inSession, IOOptionBits options);
    static void registerService(void *self, IOHIDServiceRef service);
    static void unregisterService(void *self, IOHIDServiceRef service);
    static void scheduleWithDispatchQueue(void *self, dispatch_queue_t queue);
    static void unscheduleFromDispatchQueue(void *self, dispatch_queue_t queue);
    static CFTypeRef getPropertyForClient(void *self, CFStringRef key, CFTypeRef client);
    static void setPropertyForClient(void *self, CFStringRef key, CFTypeRef property, CFTypeRef client);
    
    void handleKeyboardEvent(IOHIDEventRef event);
    void handleBiometricEvent(IOHIDEventRef event);
    bool modifierPressed();
    IOHIDEventRef handleDFREvent(IOHIDEventRef event);
    void startTouchCancellation();
    
private:
    IOHIDDFREventFilter();
    IOHIDDFREventFilter(const IOHIDDFREventFilter &);
    IOHIDDFREventFilter &operator=(const IOHIDDFREventFilter &);
};

#endif /* IOHIDDFREventFilter_hpp */
