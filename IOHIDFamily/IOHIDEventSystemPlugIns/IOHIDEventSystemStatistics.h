/*
 *  IOHIDEventSystemStatistics.h
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

#include <memory>


class IOHIDEventSystemStatistics
{
public:
    IOHIDEventSystemStatistics(CFUUIDRef factoryID);
    ~IOHIDEventSystemStatistics();
    HRESULT QueryInterface( REFIID iid, LPVOID *ppv );
    ULONG AddRef();
    ULONG Release();
    
    IOHIDEventRef filter(IOHIDServiceRef sender, IOHIDEventRef event);
    boolean_t open(IOHIDSessionRef session, IOOptionBits options);
    void close(IOHIDSessionRef session, IOOptionBits options);
    void registerService(IOHIDServiceRef service);
    void unregisterService(IOHIDServiceRef service);
    void handlePendingStats();
    void scheduleWithDispatchQueue(dispatch_queue_t queue);
    void unscheduleFromDispatchQueue(dispatch_queue_t queue);
    CFTypeRef getPropertyForClient (CFStringRef key, CFTypeRef client);

private:
    IOHIDSessionFilterPlugInInterface *_sessionInterface;
    CFUUIDRef                   _factoryID;
    UInt32                      _refCount;
    
    uint64_t                    _displayState;
    
    int                         _displayToken;
    
    dispatch_queue_t            _dispatch_queue;
    dispatch_source_t           _pending_source;
    
    typedef struct {
        uint32_t                    home_wake;
        uint32_t                    power_wake;
        uint32_t                    power_sleep;
    } Buttons;
    
    typedef struct {
        uint32_t                    enumeration_count;
        uint32_t                    character_count;
        uint32_t                    symbol_count;
        uint32_t                    spacebar_count;
        uint32_t                    arrow_count;
        uint32_t                    cursor_count;
        uint32_t                    modifier_count;
    } KeyStats;

    Buttons _pending_buttons;
    KeyStats _pending_keystats;
    
    CFMutableSetRef             _keyServices;
    
    IOHIDEventRef               _attachEvent;
        
private:
    static IOHIDSessionFilterPlugInInterface sIOHIDEventSystemStatisticsFtbl;
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

    static CFTypeRef getPropertyForClient (void * self, CFStringRef key, CFTypeRef client);

    static void handlePendingStats(void * self);
    
    bool collectKeyStats(IOHIDServiceRef sender, IOHIDEventRef event);
    
    static bool isCharacterKey(uint16_t usagePage, uint16_t usage);
    static bool isSymbolKey(uint16_t usagePage, uint16_t usage);
    static bool isSpacebarKey(uint16_t usagePage, uint16_t usage);
    static bool isArrowKey(uint16_t usagePage, uint16_t usage);
    static bool isCursorKey(uint16_t usagePage, uint16_t usage);
    static bool isModifierKey(uint16_t usagePage, uint16_t usage);
    
private:
    IOHIDEventSystemStatistics();
    IOHIDEventSystemStatistics(const IOHIDEventSystemStatistics &);
    IOHIDEventSystemStatistics &operator=(const IOHIDEventSystemStatistics &);
};
