//
//  IOHIDAccessibilityFilter.h
//  IOHIDFamily
//
//  Created by Gopu Bhaskar on 3/10/15.
//
//

#ifndef _IOHIDFamily_IOHIDAccessibilityFilter_
#define _IOHIDFamily_IOHIDAccessibilityFilter_
#include <sys/cdefs.h>
#include <CoreFoundation/CoreFoundation.h>
#if COREFOUNDATION_CFPLUGINCOM_SEPARATE
#include <CoreFoundation/CFPlugInCOM.h>
#endif

#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
#include <IOKit/hid/IOHIDUsageTables.h>


enum {
    kStickyKeysEnableCount = 5,
    kStickyKeysShiftKeyInterval = 30,
};

enum {
    kStickyKeyState_Reset   = 1 << 0,
    kStickyKeyState_Down    = 1 << 1,
    kStickyKeyState_Locked  = 2 << 2,
};

typedef UInt32 StickyKeyState;

#define MAX_STICKY_KEYS (kHIDUsage_KeyboardRightGUI - kHIDUsage_KeyboardLeftControl + 3)

class IOHIDAccessibilityFilter
{
public:
    IOHIDAccessibilityFilter(CFUUIDRef factoryID);
    ~IOHIDAccessibilityFilter();
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

    static IOHIDServiceFilterPlugInInterface sIOHIDAccessibilityFilterFtbl;
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

    bool _stickyKeysFeatureEnabled;
    bool _stickyKeysOn;
    bool _stickyKeysShiftKeyToggles;
    
    bool _slowKeysInProgress;
    UInt32 _slowKeysCurrentUsage;
    UInt32 _slowKeysCurrentUsagePage;
    IOHIDEventRef _slowKeysSlowEvent;
    
    UInt32 _slowKeysDelay;
    
    UInt32 _stickyKeysShiftKeyCount;
    StickyKeyState _stickyKeyState[MAX_STICKY_KEYS];

    IOHIDEventRef processStickyKeys(IOHIDEventRef event);
    void setStickyKeyState(UInt32 usagePage, UInt32 usage, StickyKeyState state);
    StickyKeyState getStickyKeyState(UInt32 usagePage, UInt32 usage);
    
    dispatch_queue_t _queue;
    dispatch_source_t _stickyKeysShiftResetTimer;
    dispatch_source_t _slowKeysTimer;
    
    void dispatchStickyKeys(int stateMask);
    bool processStickyKeyUp(UInt32 usagePage, UInt32 usage, UInt32& flags);
    bool processStickyKeyDown(UInt32 usagePage, UInt32 usage, UInt32& flags);
    void processStickyKeys(void);
    void processShiftKey(void);
    
    IOHIDEventRef processSlowKeys(IOHIDEventRef event);
    void dispatchSlowKey(void);
    void resetSlowKey(void);

    
private:
    IOHIDAccessibilityFilter();
    IOHIDAccessibilityFilter(const IOHIDAccessibilityFilter &);
    IOHIDAccessibilityFilter &operator=(const IOHIDAccessibilityFilter &);
};


#endif /* defined(_IOHIDFamily_IOHIDAccessibilityFilter_) */
