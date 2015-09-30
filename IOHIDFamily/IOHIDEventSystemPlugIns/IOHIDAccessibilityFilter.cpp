//
//  IOHIDAccessibilityFilter.cpp
//  IOHIDFamily
//
//  Created by Gopu Bhaskar on 3/10/15.
//
//

#include "IOHIDAccessibilityFilter.h"

#include <new>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
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
#include <IOKit/hid/IOHIDServiceKeys.h>
#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <syslog.h>

#define DEBUG_LOGGING 0

#if DEBUG_LOGGING
#define DEBUG_LOG(fmt, ...) syslog(LOG_ERR, "IOHIDAccessibilityFilter: " #fmt "\n", ## __VA_ARGS__)
#else
#define DEBUG_LOG(...)
#endif


// 5512668E-FF47-4E70-B33E-E1FFFAEF01A8
#define kIOHIDAccessibilityFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x55, 0x12, 0x66, 0x8E, 0xFF, 0x47, 0x4E, 0x70, 0xB3, 0x3E, 0xE1, 0xFF, 0xFA, 0xEF, 0x01, 0xA8)


extern "C" void * IOHIDAccessibilityFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);

UInt32  makeModifierLeftHanded(UInt32 usage);
UInt8   getModifierIndex(UInt32 usagePage, UInt32 usage);
void    getUsageForIndex(UInt32 index, UInt32& usagePage, UInt32& usage);
bool    isModifier(UInt32 usagePage, UInt32 usage);
bool    isShiftKey(UInt32 usagePage, UInt32 usage);


//------------------------------------------------------------------------------
// IOHIDAccessibilityFilterFactory
//------------------------------------------------------------------------------

void *IOHIDAccessibilityFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDServiceFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDAccessibilityFilter), 0);
        return new(p) IOHIDAccessibilityFilter(kIOHIDAccessibilityFilterFactory);
    }

    return NULL;
}

// The IOHIDAccessibilityFilter function table.
IOHIDServiceFilterPlugInInterface IOHIDAccessibilityFilter::sIOHIDAccessibilityFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDAccessibilityFilter::QueryInterface,
    IOHIDAccessibilityFilter::AddRef,
    IOHIDAccessibilityFilter::Release,
    // IOHIDSimpleServiceFilterPlugInInterface functions
    IOHIDAccessibilityFilter::match,
    IOHIDAccessibilityFilter::filter,
    NULL,
    // IOHIDServiceFilterPlugInInterface functions
    IOHIDAccessibilityFilter::open,
    IOHIDAccessibilityFilter::close,
    IOHIDAccessibilityFilter::scheduleWithDispatchQueue,
    IOHIDAccessibilityFilter::unscheduleFromDispatchQueue,
    IOHIDAccessibilityFilter::copyPropertyForClient,
    IOHIDAccessibilityFilter::setPropertyForClient,
    NULL,
    IOHIDAccessibilityFilter::setEventCallback,
};

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::IOHIDAccessibilityFilter
//------------------------------------------------------------------------------
IOHIDAccessibilityFilter::IOHIDAccessibilityFilter(CFUUIDRef factoryID)
:
_serviceInterface(&sIOHIDAccessibilityFilterFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_stickyKeysFeatureEnabled(false),
_stickyKeysShiftKeyToggles(false),
_stickyKeysShiftKeyCount(0),
_stickyKeysOn(false),
_stickyKeysShiftResetTimer(0),
_eventCallback(0),
_eventTarget(0),
_eventContext(0),
_queue(0),
_slowKeysDelay(0),
_slowKeysTimer(0),
_slowKeysInProgress(0),
_slowKeysCurrentUsage(0),
_slowKeysCurrentUsagePage(0),
_slowKeysSlowEvent(0)
{
    CFPlugInAddInstanceForFactory( factoryID );
    
    for (int i = 0; i < MAX_STICKY_KEYS; i++)
        _stickyKeyState[i] = kStickyKeyState_Reset;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::~IOHIDAccessibilityFilter
//------------------------------------------------------------------------------
IOHIDAccessibilityFilter::~IOHIDAccessibilityFilter()
{
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDAccessibilityFilter::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDAccessibilityFilter *>(self)->QueryInterface(iid, ppv);
}

// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDAccessibilityFilter::QueryInterface( REFIID iid, LPVOID *ppv )
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);
    
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSimpleServiceFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDServiceFilterPlugInInterfaceID)) {
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
// IOHIDAccessibilityFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDAccessibilityFilter::AddRef( void *self )
{
    return static_cast<IOHIDAccessibilityFilter *>(self)->AddRef();
}

ULONG IOHIDAccessibilityFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDAccessibilityFilter::Release( void *self )
{
    return static_cast<IOHIDAccessibilityFilter *>(self)->Release();
}

ULONG IOHIDAccessibilityFilter::Release()
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::open
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::open(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDAccessibilityFilter *>(self)->open(service, options);
}

void IOHIDAccessibilityFilter::open(IOHIDServiceRef service, IOOptionBits options)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    (void)service;
    (void)options;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::close
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::close(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDAccessibilityFilter *>(self)->close(service, options);
}

void IOHIDAccessibilityFilter::close(IOHIDServiceRef service, IOOptionBits options)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    (void) service;
    (void) options;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDAccessibilityFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDAccessibilityFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    _queue = queue;
    
    _stickyKeysShiftResetTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    
    dispatch_set_context(_stickyKeysShiftResetTimer, this);
    dispatch_source_set_event_handler(_stickyKeysShiftResetTimer, ^{
        dispatch_source_set_timer(_stickyKeysShiftResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
        _stickyKeysShiftKeyCount = 0;
        });
    dispatch_source_set_timer(_stickyKeysShiftResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_resume(_stickyKeysShiftResetTimer);
    
    _slowKeysTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    
    dispatch_set_context(_slowKeysTimer, this);
    dispatch_source_set_event_handler(_slowKeysTimer, ^{
        dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatchSlowKey();
    });
    dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_resume(_slowKeysTimer);
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDAccessibilityFilter *>(self)->unscheduleFromDispatchQueue(queue);
}
void IOHIDAccessibilityFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    if (_stickyKeysOn)
        dispatchStickyKeys(kStickyKeyState_Down | kStickyKeyState_Locked);
    
    if (_slowKeysDelay)
        dispatchSlowKey();

    _queue = NULL;
    
    if (_stickyKeysShiftResetTimer) {
        dispatch_source_cancel(_stickyKeysShiftResetTimer);
        _stickyKeysShiftResetTimer = NULL;
    }
    
    if (_slowKeysTimer) {
        dispatch_source_cancel(_slowKeysTimer);
        _slowKeysTimer = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::setEventCallback
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    static_cast<IOHIDAccessibilityFilter *>(self)->setEventCallback(callback, target, refcon);
}

void IOHIDAccessibilityFilter::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    DEBUG_LOG("%s callback = %p target = %p refcon = %p", __PRETTY_FUNCTION__, callback, target, refcon);

    _eventCallback  = callback;
    _eventTarget    = target;
    _eventContext   = refcon;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::copyPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDAccessibilityFilter::copyPropertyForClient(void * self, CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDAccessibilityFilter *>(self)->copyPropertyForClient(key, client);
}

CFTypeRef IOHIDAccessibilityFilter::copyPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    CFTypeRef result = NULL;
    
    if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysDisabledKey), kNilOptions) == kCFCompareEqualTo) {
        result = _stickyKeysFeatureEnabled ? kCFBooleanTrue : kCFBooleanFalse;
    }
    
    else if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysOnKey), kNilOptions) == kCFCompareEqualTo) {
        result = _stickyKeysOn ? kCFBooleanTrue : kCFBooleanFalse;
    }
    
    else if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysShiftTogglesKey), kNilOptions) == kCFCompareEqualTo) {
        result = _stickyKeysShiftKeyToggles ? kCFBooleanTrue : kCFBooleanFalse;
    }
    
    else if (CFStringCompare(key, CFSTR(kIOHIDServiceSlowKeysDelayKey), kNilOptions) == kCFCompareEqualTo) {
        result = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &_slowKeysDelay);
    }
    
    return result;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::setPropertyForClient(void * self,CFStringRef key,CFTypeRef property,CFTypeRef client)
{
    static_cast<IOHIDAccessibilityFilter *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDAccessibilityFilter::setPropertyForClient(CFStringRef key,CFTypeRef property,CFTypeRef client __unused)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);
    
    CFBooleanRef    boolProp        = (CFBooleanRef)property;
    
    if (!key)
        goto exit;
    
    if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysDisabledKey), kNilOptions) == kCFCompareEqualTo) {
        _stickyKeysFeatureEnabled = !(boolProp == kCFBooleanTrue);
        
        if (!_stickyKeysFeatureEnabled) {
            _stickyKeysOn = false;
            dispatchStickyKeys(kStickyKeyState_Down | kStickyKeyState_Locked);
        }

        DEBUG_LOG("_stickyKeysFeatureEnabled = %d", _stickyKeysFeatureEnabled);
    }
    
    else if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysOnKey), kNilOptions) == kCFCompareEqualTo) {
        _stickyKeysOn = (boolProp == kCFBooleanTrue);
        
        if (!_stickyKeysOn)
            dispatchStickyKeys(kStickyKeyState_Down | kStickyKeyState_Locked);
        
        DEBUG_LOG("_stickyKeysOn = %d\n", _stickyKeysOn);
    }
    
    else if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysShiftTogglesKey), kNilOptions) == kCFCompareEqualTo) {
        _stickyKeysShiftKeyToggles = (boolProp == kCFBooleanTrue);
        
        if (!_stickyKeysShiftKeyToggles)
            _stickyKeysShiftKeyCount = 0;
        
        DEBUG_LOG("_stickyKeysShiftKeyToggles = %d\n", _stickyKeysShiftKeyToggles);
    }
    
    else if (CFStringCompare(key, CFSTR(kIOHIDServiceSlowKeysDelayKey), kNilOptions) == kCFCompareEqualTo) {
        CFNumberRef     numProp = (CFNumberRef)property;
        
        CFNumberGetValue(numProp, kCFNumberSInt32Type, &_slowKeysDelay);
        
        DEBUG_LOG("_slowKeysDelay = %d\n", (int)_slowKeysDelay);
    }
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::match
//------------------------------------------------------------------------------
SInt32 IOHIDAccessibilityFilter::match(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    return static_cast<IOHIDAccessibilityFilter *>(self)->match(service, options);
}

SInt32 IOHIDAccessibilityFilter::match(IOHIDServiceRef service, IOOptionBits options)
{
    SInt32  score = 0;
    
    (void) options;
    
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);
    
    score += IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard);
    
    score += IOHIDServiceConformsTo(service, kHIDPage_Consumer, kHIDUsage_Csmr_ConsumerControl);
    
    return score;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDAccessibilityFilter::filter(void * self, IOHIDEventRef event)
{
    return static_cast<IOHIDAccessibilityFilter *>(self)->filter(event);
}

IOHIDEventRef IOHIDAccessibilityFilter::filter(IOHIDEventRef event)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    do
    {
        if (!event)
            break;
        
        if (IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard)
            break;
        
        if (_slowKeysDelay)
            event = processSlowKeys(event);
        
        if (_stickyKeysFeatureEnabled)
            event = processStickyKeys(event);
    } while (0);
    
    return event;
}


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

#define MakeUsageIntoIndex(usage)  ((usage) - kHIDUsage_KeyboardLeftControl + 1)
#define MakeIndexIntoUsage(index)  (kHIDUsage_KeyboardLeftControl + (index) - 1)
#define kHighestKeyboardUsageIndex MakeUsageIntoIndex(kHIDUsage_KeyboardRightGUI)
#define kModifierIndexForFn        (kHighestKeyboardUsageIndex + 1)

bool isShiftKey(UInt32 usagePage, UInt32 usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad)
        return ((usage == kHIDUsage_KeyboardLeftShift) || (usage == kHIDUsage_KeyboardRightShift));
    else
        return false;
}

bool isModifier(UInt32 usagePage, UInt32 usage)
{
    bool isModifier = false;
    
    if ((usagePage == kHIDPage_KeyboardOrKeypad) &&
        (((usage >= kHIDUsage_KeyboardLeftControl) && (usage <= kHIDUsage_KeyboardRightGUI)) || (usage == kHIDUsage_KeyboardCapsLock))) {
        isModifier = true;
    }
    
    else if ((usagePage == kHIDPage_AppleVendorTopCase) && (usage == kHIDUsage_AV_TopCase_KeyboardFn)) {
        isModifier = true;
    }
    
    return isModifier;
}

UInt32 makeModifierLeftHanded(UInt32 usage)
{
    if (usage >= kHIDUsage_KeyboardRightControl)
        usage -= (kHIDUsage_KeyboardRightControl - kHIDUsage_KeyboardLeftControl);
    
    return usage;
}

UInt8 getModifierIndex(UInt32 usagePage, UInt32 usage)
{
    if (usage == kHIDUsage_KeyboardCapsLock)
        return 0;
    
    if ((usagePage == kHIDPage_AppleVendorTopCase) && (usage == kHIDUsage_AV_TopCase_KeyboardFn))
        return (kModifierIndexForFn);
    
    usage = makeModifierLeftHanded(usage);
    
    return MakeUsageIntoIndex(usage);
}

void getUsageForIndex(UInt32 index, UInt32& usagePage, UInt32& usage)
{
    if (index == 0) {
        usagePage = kHIDPage_KeyboardOrKeypad;
        usage = kHIDUsage_KeyboardCapsLock;
    }
    
    else if (index < kModifierIndexForFn) {
        usagePage = kHIDPage_KeyboardOrKeypad;
        usage = MakeIndexIntoUsage(index);
    }

    else if (index == kModifierIndexForFn) {
        usagePage = kHIDPage_AppleVendorTopCase;
        usage = kHIDUsage_AV_TopCase_KeyboardFn;
    }
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::getStickyKeyState
//------------------------------------------------------------------------------
StickyKeyState IOHIDAccessibilityFilter::getStickyKeyState(UInt32 usagePage, UInt32 usage)
{
    return _stickyKeyState[getModifierIndex(usagePage, usage)];
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::setStickyKeyState
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::setStickyKeyState(UInt32 usagePage, UInt32 usage, StickyKeyState state)
{
    _stickyKeyState[getModifierIndex(usagePage, usage)] = state;
}

//
// State machine
// -------------
// First mod down                   --> dispatched with kIOHIDKeyboardStickyKeyDown
// Second consecutive mod down      --> dispatched with kIOHIDKeyboardStickyKeyLocked
// Mod up in down or lock           --> do not dispatch
// Third consecutive mod down       --> no dispatch
// Third consecutive key up         --> dispatched with kIOHIDKeyboardStickyKeyUp
// Non modifier key down            --> dispatched - All down state set to reset state
// Non modifier key up              --> dispatched
//

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::processStickyKeyDown
//------------------------------------------------------------------------------
bool IOHIDAccessibilityFilter::processStickyKeyDown(UInt32 usagePage, UInt32 usage, UInt32& flags)
{
    StickyKeyState state = getStickyKeyState(usagePage, usage);
    bool needsDispatch = false;
    
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);
    
    switch (state) {
        case kStickyKeyState_Reset:
            // First mod press
            needsDispatch = true;
            flags = kIOHIDKeyboardStickyKeyDown;
            setStickyKeyState(usagePage, usage, kStickyKeyState_Down);
            break;
            
        case kStickyKeyState_Down:
            // Second consecutive
            needsDispatch = true;
            flags = kIOHIDKeyboardStickyKeyLocked;
            setStickyKeyState(usagePage, usage, kStickyKeyState_Locked);
            break;
            
        case kStickyKeyState_Locked:
            // Send this event, user is unlocking
            needsDispatch = false;
            setStickyKeyState(usagePage, usage, kStickyKeyState_Reset);
            break;
            
        default:
            syslog(LOG_ERR, "IOHIDAccessibilityFilter: Sticky key down in bad state for usage %d\n", (int)usage);
    }
    
    return needsDispatch;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::processStickyKeyUp
//------------------------------------------------------------------------------
bool IOHIDAccessibilityFilter::processStickyKeyUp(UInt32 usagePage, UInt32 usage, UInt32& flags)
{
    StickyKeyState state = getStickyKeyState(usagePage, usage);
    bool needsDispatch = false;
    
    DEBUG_LOG("%s usage = %d state = %d", __PRETTY_FUNCTION__, (int)usage, (int)state);
    
    switch (state) {
        case kStickyKeyState_Reset:
            // going from lock to unlocked
            needsDispatch = true;
            flags = kIOHIDKeyboardStickyKeyUp;
            break;
            
        case kStickyKeyState_Down:
            // The down event caused the state to be down, filter the up event
            break;
            
        case kStickyKeyState_Locked:
            // The down event caused lock, filter the up
            break;
            
        default:
            syslog(LOG_ERR, "Sticky key up in bad state for usage %d\n", (int)usage);
    }
    
    return needsDispatch;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::dispatchStickyKeys
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::dispatchStickyKeys(int stateMask)
{
    int     i = 0;
    
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);
    
    for (;i < MAX_STICKY_KEYS; i++) {
        UInt32 usage = 0;
        UInt32 usagePage = 0;
        
        getUsageForIndex(i, usagePage, usage);
        
        DEBUG_LOG("index = %d, usage = %d", (int)i, (int)usage);

        if ((getStickyKeyState(usagePage, usage) & stateMask) == 0)
            continue;
        
        IOHIDEventRef event = IOHIDEventCreate(kCFAllocatorDefault, kIOHIDEventTypeKeyboard, mach_absolute_time(), 0);
        
        IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage, kHIDPage_KeyboardOrKeypad);
        IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardUsage, usage);
        IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardDown, 0);
        IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | kIOHIDKeyboardStickyKeyUp);
        
        _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
        
        DEBUG_LOG("dispatched usage %d", (int)usage);
        
        setStickyKeyState(usagePage, usage, kStickyKeyState_Reset);
    }
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::processShiftKey
//------------------------------------------------------------------------------
void IOHIDAccessibilityFilter::processShiftKey(void)
{
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    if (_stickyKeysShiftKeyToggles == false)
        return;
    
    bool timerWasEnabed = _stickyKeysShiftKeyCount++;
    
    if (!timerWasEnabed)
        dispatch_source_set_timer(_stickyKeysShiftResetTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * kStickyKeysShiftKeyInterval),
                                  DISPATCH_TIME_FOREVER,
                                  0);
                                  
    
    if (_stickyKeysShiftKeyCount >= kStickyKeysEnableCount) {
        _stickyKeysShiftKeyCount = 0;
        _stickyKeysOn = !_stickyKeysOn;
    }

    if (!_stickyKeysOn)
        dispatch_async(_queue, ^{dispatchStickyKeys(kStickyKeyState_Down | kStickyKeyState_Locked);});
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::processStickyKeys
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDAccessibilityFilter::processStickyKeys(IOHIDEventRef event)
{
    UInt32  usage               = 0;
    UInt32  usagePage           = 0;
    UInt32  flags               = 0;
    UInt32  oldFlags            = 0;
    bool    keyDown             = 0;
    bool    needsDispatch       = 0;
    bool    stickyKeysOldState  = _stickyKeysOn;
    bool    stickyKeysToggled   = false;
    
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);
    
    if (!event)
        goto exit;
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    oldFlags    = (UInt32)IOHIDEventGetEventFlags(event);
    
    //
    // If this is a key dispatched from this filter, ignore it
    if ((oldFlags & kIOHIDKeyboardStickyKeyUp) != 0)
        goto exit;
    
    //
    // If this was shift key check for toggles
    if (keyDown && isShiftKey(usagePage, usage))
        processShiftKey();
    
    stickyKeysToggled = (stickyKeysOldState != _stickyKeysOn);
    
    //
    // Stop processing if sticky keys are not ON
    if (!_stickyKeysOn) {
        //
        // Sticky key turned off by shift key
        if (stickyKeysToggled)
            IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | kIOHIDKeyboardStickyKeysOff);
        
        goto exit;
    }
    
    //
    // If this key is not a modifier, dispatch the up events for any
    // stuck keys
    if (!isModifier(usagePage, usage)) {
        if (_stickyKeysShiftKeyCount)
            dispatch_source_set_timer(_stickyKeysShiftResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
        
        // Dispatch any stuck keys on a non modifier key up
        if (!keyDown){
            dispatch_async(_queue, ^{
                dispatchStickyKeys(kStickyKeyState_Down);
                });
            
            _stickyKeysShiftKeyCount = 0;
        }
    }
    
    else {
        //
        // Sticky keys turned on by shift key ? send this shift key through
        if (stickyKeysToggled) {
            IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | kIOHIDKeyboardStickyKeysOn);
            goto exit;
        }
        
        //
        // At this point the the key we have is a modifier key
        // send it down the state machine
        if (keyDown)
            needsDispatch = processStickyKeyDown(usagePage, usage, flags);
        else
            needsDispatch = processStickyKeyUp(usagePage, usage, flags);
        
        if (!needsDispatch) {
            event = NULL;
        }
        
        else {
            usage = makeModifierLeftHanded(usage);
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardUsage, usage);
            IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | flags);
        }
    }
    
exit:
    return event;
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::resetSlowKey
//------------------------------------------------------------------------------
void  IOHIDAccessibilityFilter::resetSlowKey(void)
{
    _slowKeysCurrentUsage = 0;
    _slowKeysCurrentUsagePage = 0;
    
    _slowKeysInProgress = false;

    if (_slowKeysSlowEvent) {
        CFRelease(_slowKeysSlowEvent);
        _slowKeysSlowEvent = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::dispatchSlowKey
//------------------------------------------------------------------------------
void  IOHIDAccessibilityFilter::dispatchSlowKey(void)
{
    IOHIDEventRef event = _slowKeysSlowEvent;
    
    DEBUG_LOG("%s", __PRETTY_FUNCTION__);

    if (!event)
        return;
    
    IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | kIOHIDKeyboardSlowKey);
    
    _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
    
    resetSlowKey();
}

//------------------------------------------------------------------------------
// IOHIDAccessibilityFilter::processSlowKeys
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDAccessibilityFilter::processSlowKeys(IOHIDEventRef event)
{
    UInt32  usage       = 0;
    UInt32  usagePage   = 0;
    bool    keyDown     = 0;
    
    DEBUG_LOG("%s event = %p flags = 0x%x", __PRETTY_FUNCTION__, event, event ? IOHIDEventGetEventFlags(event) : 0);
    
    if (!event)
        goto exit;
    
    if ((IOHIDEventGetEventFlags(event) & kIOHIDKeyboardSlowKey) != 0)
        goto exit;
    
    //
    // If (keyDown)
    //      if (slowKeyIsInProgress)
    //          kill the slow key
    //          start a slow key process for new key
    //      else //slow key not in progress
    //          start a slow key process for new key
    // else // if key up
    //      if (slowKeyInProgress)
    //          if (itIsTheSlowKey's up)
    //              abort the slow key
    //              kill the up event
    //      else
    //          send the key
    //
    // key up without a key down can be expected with this alg
    //
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    
    DEBUG_LOG("keyDown = %d usage = %d page = %d _slowKeysInProgress = %d",
              (int)keyDown, (int)usage, (int)usagePage, (int)_slowKeysInProgress);
    
    if (keyDown) {
        dispatch_source_set_timer(_slowKeysTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * _slowKeysDelay),
                                  DISPATCH_TIME_FOREVER, 0);
        
        _slowKeysCurrentUsage = usage;
        _slowKeysCurrentUsagePage = usagePage;
        
        _slowKeysSlowEvent = event;
        _slowKeysInProgress = true;
        
        CFRetain(event);
        event = NULL;
    }
    
    else {
        if (_slowKeysInProgress) {
            if ((_slowKeysCurrentUsage == usage) && (_slowKeysCurrentUsagePage == usagePage)) {
                dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);

                resetSlowKey();
                
                event = NULL;
            }
        }
    }
    
exit:
    return event;
}



