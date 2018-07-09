//
//  IOHIDKeyboardFilter.cpp
//  IOHIDFamily
//
//  Created by Gopu Bhaskar on 3/10/15.
//
//

#include "IOHIDKeyboardFilter.h"
#include <TargetConditionals.h>
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
#include <IOKit/hidsystem/event_status_driver.h>
#include <IOKit/hidsystem/IOHIDShared.h>
#include <IOKit/hid/IOHIDServiceKeys.h>
#include "IOHIDFamilyPrivate.h"
#include <IOKit/IOKitLib.h>
#include <IOKit/usb/USB.h>
#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <IOKit/hid/IOHIDLibPrivate.h>
#include "IOHIDDebug.h"
#include "CF.h"
#include <sstream>
#include "IOHIDPrivateKeys.h"
#include "IOHIDKeys.h"
#include "IOHIDevicePrivateKeys.h"

#import "AppleKeyboardStateManager.h"

#define kCapsLockDelayMS    75
#define kEjectKeyDelayMS    0
#define kSlowKeyMinMS       1
#define kSlowRepeatDelayMS  420

#define kMouseKeyActivationCount      5
#define kMouseKeyActivationReset      30

#if TARGET_OS_EMBEDDED // Repeats handled in backboard on embedded platforms
#define kInitialKeyRepeatMS 0
#define kKeyRepeatMS        0
#define kMinKeyRepeatMS     0
#else
#define kInitialKeyRepeatMS 500
#define kKeyRepeatMS        83 // 1/12 sec
#define kMinKeyRepeatMS     16 // 1/60 sec
#endif

// Legacy modifier key values
// Used for compatibility with old modifier key remapping property
#define NX_MODIFIERKEY_NOACTION             -1
#define NX_MODIFIERKEY_ALPHALOCK            0
#define NX_MODIFIERKEY_SHIFT                1
#define NX_MODIFIERKEY_CONTROL              2
#define NX_MODIFIERKEY_ALTERNATE            3
#define NX_MODIFIERKEY_COMMAND              4
#define NX_MODIFIERKEY_NUMERICPAD           5
#define NX_MODIFIERKEY_HELP                 6
#define NX_MODIFIERKEY_SECONDARYFN          7
#define NX_MODIFIERKEY_NUMLOCK              8
#define NX_MODIFIERKEY_RSHIFT               9
#define NX_MODIFIERKEY_RCONTROL             10
#define NX_MODIFIERKEY_RALTERNATE           11
#define NX_MODIFIERKEY_RCOMMAND             12
#define NX_MODIFIERKEY_ALPHALOCK_STATELESS  13

// 5512668E-FF47-4E70-B33E-E1FFFAEF01A8
#define kIOHIDKeyboardFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x55, 0x12, 0x66, 0x8E, 0xFF, 0x47, 0x4E, 0x70, 0xB3, 0x3E, 0xE1, 0xFF, 0xFA, 0xEF, 0x01, 0xA8)

#define IOHIDEventIsSlowKeyPhaseEvent(e) \
    (IOHIDEventGetIntegerValue (e, kIOHIDEventFieldKeyboardSlowKeyPhase) == kIOHIDKeyboardSlowKeyPhaseStart || \
     IOHIDEventGetIntegerValue (e, kIOHIDEventFieldKeyboardSlowKeyPhase) == kIOHIDKeyboardSlowKeyPhaseAbort)

extern "C" void * IOHIDKeyboardFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);


bool    getUsageForLegacyModifier(SInt8 modifier, UInt32& usagePage, UInt32& usage);
UInt32  makeModifierLeftHanded(UInt32 usage);
UInt8   getModifierIndex(UInt32 usagePage, UInt32 usage);
void    getUsageForIndex(UInt32 index, UInt32& usagePage, UInt32& usage);
bool    isModifier(UInt32 usagePage, UInt32 usage);
bool    isShiftKey(UInt32 usagePage, UInt32 usage);
bool    isNotRepeated(UInt32 usagePage, UInt32 usage);
bool    isStickyModifier(UInt32 usagePage, UInt32 usage);
#if !TARGET_OS_EMBEDDED
static  NXEventHandle openHIDSystem(void);
#endif


const CFStringRef kIOHIDEventAttachment_Modified  = CFSTR("Modified");
const CFStringRef kIOHIDEventAttachment_Delayed   = CFSTR("Delayed");

#define SERVICE_ID (_service ? IOHIDServiceGetRegistryID(_service) : NULL)

//------------------------------------------------------------------------------
// IOHIDKeyboardFilterFactory
//------------------------------------------------------------------------------

void *IOHIDKeyboardFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDServiceFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDKeyboardFilter), 0);
        return new(p) IOHIDKeyboardFilter(kIOHIDKeyboardFilterFactory);
    }

    return NULL;
}

// The IOHIDKeyboardFilter function table.
IOHIDServiceFilterPlugInInterface IOHIDKeyboardFilter::sIOHIDKeyboardFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDKeyboardFilter::QueryInterface,
    IOHIDKeyboardFilter::AddRef,
    IOHIDKeyboardFilter::Release,
    // IOHIDSimpleServiceFilterPlugInInterface functions
    IOHIDKeyboardFilter::match,
    IOHIDKeyboardFilter::filter,
    NULL,
    // IOHIDServiceFilterPlugInInterface functions
    IOHIDKeyboardFilter::open,
    IOHIDKeyboardFilter::close,
    IOHIDKeyboardFilter::scheduleWithDispatchQueue,
    IOHIDKeyboardFilter::unscheduleFromDispatchQueue,
    IOHIDKeyboardFilter::copyPropertyForClient,
    IOHIDKeyboardFilter::setPropertyForClient,
    NULL,
    IOHIDKeyboardFilter::setEventCallback,
};

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::IOHIDKeyboardFilter
//------------------------------------------------------------------------------
IOHIDKeyboardFilter::IOHIDKeyboardFilter(CFUUIDRef factoryID)
:
_serviceInterface(&sIOHIDKeyboardFilterFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_matchScore(0),
_service(NULL),
_eventCallback(defaultEventCallback),
_eventTarget(0),
_eventContext(0),
_fnKeyMode(0),
_supportedModifiers(0),
_slowKeysSlowEvent(0),
_slowKeysDelayMS(0),
_stickyKeysShiftKeyCount(0),
_stickyKeyToggle(false),
_stickyKeyOn (false),
_stickyKeyDisable(false),
_keyRepeatEvent(NULL),
_keyRepeatInitialDelayMS(kInitialKeyRepeatMS),
_keyRepeatDelayMS (kKeyRepeatMS),
_delayedCapsLockEvent(0),
_capsLockDelayMS(kCapsLockDelayMS),
_capsLockDelayOverrideMS(-1),
_numLockOn (0),
#if !TARGET_OS_EMBEDDED
_delayedEjectKeyEvent(NULL),
_ejectKeyDelayMS(kEjectKeyDelayMS),
_mouseKeyActivationEnable(false),
_mouseKeyActivationCount (0),
_ejectKeyDelayTimer(0),
_mouseKeyActivationResetTimer(0),
#endif
_capsLockState (false),
_capsLockLEDState(false),
_capsLockLEDInhibit (false),
_capsLockLED(kIOHIDServiceCapsLockLEDKey_Auto),
_queue(NULL),
_stickyKeysShiftResetTimer(0),
_slowKeysTimer(0),
_keyRepeatTimer(0),
_capsLockDelayTimer(0),
_restoreState(nil),
_locationID(nil)

{
    CFPlugInAddInstanceForFactory( factoryID );
    for (int i = 0; i < MAX_STICKY_KEYS; i++) {
        _stickyKeyState[i] = kStickyKeyState_Reset;
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::~IOHIDKeyboardFilter
//------------------------------------------------------------------------------
IOHIDKeyboardFilter::~IOHIDKeyboardFilter()
{
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDKeyboardFilter::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDKeyboardFilter *>(self)->QueryInterface(iid, ppv);
}

// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDKeyboardFilter::QueryInterface( REFIID iid, LPVOID *ppv )
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    
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
// IOHIDKeyboardFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDKeyboardFilter::AddRef( void *self )
{
    return static_cast<IOHIDKeyboardFilter *>(self)->AddRef();
}

ULONG IOHIDKeyboardFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDKeyboardFilter::Release( void *self )
{
    return static_cast<IOHIDKeyboardFilter *>(self)->Release();
}

ULONG IOHIDKeyboardFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::open
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::open(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDKeyboardFilter *>(self)->open(service, options);
}

void IOHIDKeyboardFilter::open(IOHIDServiceRef service, IOOptionBits options)
{
    CFDictionaryRef propDict;
    CFTypeRef       value = NULL;
    
    (void)options;
    
    _service = service;
    
    _restoreState = CFBridgingRelease(IOHIDServiceCopyProperty(service, (__bridge CFStringRef)@kIOHIDKeyboardRestoreStateKey));
    _locationID = CFBridgingRelease(IOHIDServiceCopyProperty(service, (__bridge CFStringRef)@kIOHIDLocationIDKey));

#if !TARGET_OS_EMBEDDED
    uint32_t keyboardID;
    value = IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDSubinterfaceIDKey));
    if (value == NULL) {
        keyboardID = getKeyboardID();
        value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &keyboardID);
        if (value) {
            IOHIDServiceSetProperty(_service, CFSTR(kIOHIDSubinterfaceIDKey), value);
            CFRelease(value);
        }
    } else {
        CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &keyboardID);
        CFRelease(value);
    }
    value = IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDKeyboardSupportsF12EjectKey));
    if (value == NULL) {
        boolean_t f12Support = false;
        if (((keyboardID >= 0xc3) && (keyboardID <= 0xc9)) ||
            ((keyboardID >= 0x28) && (keyboardID <= 0x2a)) ||
              keyboardID <= 0x1e) {
            f12Support = true;
        }
        IOHIDServiceSetProperty(_service, CFSTR(kIOHIDKeyboardSupportsF12EjectKey), f12Support ? kCFBooleanTrue : kCFBooleanFalse);
    } else {
        CFRelease(value);
    }
#endif

    value = IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDKeyboardSupportedModifiersKey));
    if (value) {
        if (CFGetTypeID(value) == CFNumberGetTypeID()) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &_supportedModifiers);
        }
        CFRelease(value);
    }
  
    value = IOHIDServiceCopyProperty(_service, CFSTR(kFnFunctionUsageMapKey));
    if (value != NULL) {
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            _fnFunctionUsageMapKeyMap = createMapFromStringMap((CFStringRef)value);
        }
        CFRelease(value);
    }

    value = IOHIDServiceCopyProperty(_service, CFSTR(kFnKeyboardUsageMapKey));
    if (value != NULL ) {
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            _fnKeyboardUsageMapKeyMap = createMapFromStringMap((CFStringRef)value);
        }
        CFRelease(value);
    }

    value = IOHIDServiceCopyProperty(_service, CFSTR(kNumLockKeyboardUsageMapKey));
    if (value != NULL ) {
        if (CFGetTypeID(value) == CFStringGetTypeID()) {
            _numLockKeyboardUsageMapKeyMap = createMapFromStringMap((CFStringRef)value);
        }
        CFRelease(value);
    }

    // Set initial caps lock LED state.
    propDict = (CFDictionaryRef)IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDEventServicePropertiesKey));

    if (propDict) {
        
        value = CFDictionaryGetValue(propDict, CFSTR("HIDCapsLockStateCache"));
        _capsLockState = ( value ? CFBooleanGetValue((CFBooleanRef)value) : false );
        
        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceCapsLockLEDInhibitKey));
        _capsLockLEDInhibit = ( value ? CFBooleanGetValue((CFBooleanRef)value) : false );

        _capsLockLED = kIOHIDServiceCapsLockLEDKey_Auto;
        value = CFDictionaryGetValue(propDict, kIOHIDServiceCapsLockLEDKey);
        if (value) {
          if (CFEqual (value, kIOHIDServiceCapsLockLEDKey_On)) {
              _capsLockLED = kIOHIDServiceCapsLockLEDKey_On;
          } else if (CFEqual (value, kIOHIDServiceCapsLockLEDKey_Off)) {
              _capsLockLED = kIOHIDServiceCapsLockLEDKey_Off;
          } else if (CFEqual (value, kIOHIDServiceCapsLockLEDKey_Inhibit)) {
              _capsLockLED = kIOHIDServiceCapsLockLEDKey_Inhibit;
          }
        }

        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceModifierMappingPairsKey));
        if (value && CFGetTypeID(value) == CFArrayGetTypeID()) {
            _modifiersKeyMap = createMapFromArrayOfPairs((CFArrayRef) value);
        }
      
        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceSlowKeysDelayKey));
        if (value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &_slowKeysDelayMS);
            if (_slowKeysDelayMS && _slowKeysDelayMS < kSlowKeyMinMS) {
                _slowKeysDelayMS = kSlowKeyMinMS;
            }
        }


        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceInitialKeyRepeatDelayKey));
        if (value) {
            uint64_t valueProp = 0;
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &valueProp);
            _keyRepeatInitialDelayMS = (UInt32)valueProp / 1000000;
#if (kMinKeyRepeatMS > 0)
            if (_keyRepeatInitialDelayMS && _keyRepeatInitialDelayMS < kMinKeyRepeatMS) {
                _keyRepeatInitialDelayMS = kMinKeyRepeatMS;
            }
#endif
        }

        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceKeyRepeatDelayKey));
        if (value) {
            uint64_t valueProp = 0;
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt64Type, &valueProp);
            _keyRepeatDelayMS = (UInt32)valueProp / 1000000;
            // Bound key repeat interval.
            if (_keyRepeatDelayMS && _keyRepeatDelayMS < kMinKeyRepeatMS) {
                _keyRepeatDelayMS = kMinKeyRepeatMS;
            }
        }
      
#if !TARGET_OS_EMBEDDED
        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceEjectDelayKey));
        if (value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &_ejectKeyDelayMS);
        }
#endif
        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDServiceCapsLockDelayKey));
        if (value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &_capsLockDelayMS);
        }
        value = CFDictionaryGetValue(propDict, CFSTR(kIOHIDKeyboardCapsLockDelayOverride));
        if (value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &_capsLockDelayOverrideMS);
        }

        CFRelease(propDict);
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::close
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::close(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDKeyboardFilter *>(self)->close(service, options);
}

void IOHIDKeyboardFilter::close(IOHIDServiceRef service __unused, IOOptionBits options __unused)
{
    //_service = NULL;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDKeyboardFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDKeyboardFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    
    _queue = queue;
    
    _stickyKeysShiftResetTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (_stickyKeysShiftResetTimer != NULL) {
        dispatch_source_set_event_handler(_stickyKeysShiftResetTimer, ^{
            dispatch_source_set_timer(_stickyKeysShiftResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
                _stickyKeysShiftKeyCount = 0;
            });
        dispatch_source_set_timer(_stickyKeysShiftResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_stickyKeysShiftResetTimer);
    }
  
    _slowKeysTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (_slowKeysTimer != NULL) {
        dispatch_source_set_event_handler(_slowKeysTimer, ^{
            dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);
            dispatchSlowKey();
        });
        dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_slowKeysTimer);
    }
  
    _keyRepeatTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (_keyRepeatTimer != NULL) {
        dispatch_source_set_event_handler(_keyRepeatTimer, ^{
            dispatch_source_set_timer(_keyRepeatTimer, DISPATCH_TIME_FOREVER, 0, 0);
            dispatchKeyRepeat();
        });
        dispatch_source_set_timer(_keyRepeatTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_keyRepeatTimer);
    }
  
    _capsLockDelayTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (_capsLockDelayTimer != NULL) {

        dispatch_source_set_event_handler(_capsLockDelayTimer, ^{
            dispatch_source_set_timer(_capsLockDelayTimer, DISPATCH_TIME_FOREVER, 0, 0);
            dispatchCapsLock();
        });
        dispatch_source_set_timer(_capsLockDelayTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_capsLockDelayTimer);
    }

#if !TARGET_OS_EMBEDDED
    _mouseKeyActivationResetTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (_mouseKeyActivationResetTimer != NULL) {
        dispatch_source_set_event_handler(_mouseKeyActivationResetTimer, ^{
          _mouseKeyActivationCount = 0;
        });
        dispatch_source_set_timer(_mouseKeyActivationResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_mouseKeyActivationResetTimer);
    }
    
    _ejectKeyDelayTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (_ejectKeyDelayTimer != NULL) {
        dispatch_source_set_event_handler(_ejectKeyDelayTimer, ^(void){
            dispatch_source_set_timer(_ejectKeyDelayTimer, DISPATCH_TIME_FOREVER, 0, 0);
            dispatchEjectKey();
        });
        dispatch_source_set_timer(_ejectKeyDelayTimer, DISPATCH_TIME_FOREVER, 0, 0);
        dispatch_resume(_ejectKeyDelayTimer);
    }
#endif
    
    if (_restoreState.boolValue &&
        [[AppleKeyboardStateManager sharedManager] isCapsLockEnabled:_locationID]) {
        HIDLogInfo("[%@] Restoring capslock state", SERVICE_ID);
        
        IOHIDEventRef ev = IOHIDEventCreateKeyboardEvent(kCFAllocatorDefault,
                                                         mach_absolute_time(),
                                                         kHIDPage_KeyboardOrKeypad,
                                                         kHIDUsage_KeyboardCapsLock,
                                                         true,
                                                         kIOHIDEventOptionIsZeroEvent);
        
        if (ev) {
            _eventCallback(_eventTarget, _eventContext, &_serviceInterface, ev, 0);
            CFRelease(ev);
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDKeyboardFilter *>(self)->unscheduleFromDispatchQueue(queue);
}
void IOHIDKeyboardFilter::unscheduleFromDispatchQueue(__unused dispatch_queue_t queue)
{
    

#if !TARGET_OS_EMBEDDED
    if (_mouseKeyActivationResetTimer) {
        dispatch_source_cancel(_mouseKeyActivationResetTimer);
    }
#endif

    if (_stickyKeysShiftResetTimer) {
        dispatch_source_cancel(_stickyKeysShiftResetTimer);
    }

    
    if (_slowKeysTimer) {
        dispatch_source_cancel(_slowKeysTimer);
    }
  
    if (_keyRepeatTimer) {
        dispatch_source_cancel(_keyRepeatTimer);
    }
    
    if (_capsLockDelayTimer) {
        dispatch_source_cancel(_capsLockDelayTimer);
    }
  

#if !TARGET_OS_EMBEDDED
    if (_ejectKeyDelayTimer) {
        dispatch_source_cancel(_ejectKeyDelayTimer);
    }
#endif
    __block IOHIDServiceRef service = (IOHIDServiceRef)CFRetain(_service);
    
    dispatch_async(_queue, ^(void){
        
        stopStickyKey();
        
        if (_slowKeysSlowEvent) {
            CFRelease(_slowKeysSlowEvent);
            _slowKeysSlowEvent = NULL;
        }
#if !TARGET_OS_EMBEDDED
        if (_keyRepeatEvent) {
            CFRelease(_keyRepeatEvent);
            _keyRepeatEvent = NULL;
        }
        if (_delayedEjectKeyEvent) {
            CFRelease(_delayedEjectKeyEvent);
            _delayedEjectKeyEvent = NULL;
        }
#endif
        if (_delayedCapsLockEvent) {
            CFRelease(_delayedCapsLockEvent);
            _delayedCapsLockEvent = NULL;
        }
        CFRelease(service);
    });
    
    //_queue = NULL;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::setEventCallback
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    static_cast<IOHIDKeyboardFilter *>(self)->setEventCallback(callback, target, refcon);
}

void IOHIDKeyboardFilter::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback  = callback ? callback : defaultEventCallback;
    _eventTarget    = target;
    _eventContext   = refcon;
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::copyPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDKeyboardFilter::copyPropertyForClient(void * self,CFStringRef key,CFTypeRef client __unused)
{
    return  static_cast<IOHIDKeyboardFilter *>(self)->copyPropertyForClient(key, client);
}

CFTypeRef IOHIDKeyboardFilter::copyPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{
    CFTypeRef result = NULL;
    if (CFEqual(key, CFSTR(kIOHIDServiceCapsLockStateKey))) {
        return _capsLockState ? kCFBooleanTrue : kCFBooleanFalse;
    } else if (CFEqual(key, CFSTR(kIOHIDServiceFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        if (serializer) {
          serialize(serializer);
          result = CFRetain(serializer.Reference());
        }
    } else if (CFEqual(key, kIOHIDServiceCapsLockLEDKey)) {
        result = _capsLockLEDState ? kIOHIDServiceCapsLockLEDKey_On : kIOHIDServiceCapsLockLEDKey_Off;
    } else if (CFEqual(key, CFSTR(kIOHIDStickyKeysOnKey))) {
        result = CFNumberRefWrap((SInt32)_stickyKeyOn);
    }
    return result;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::setPropertyForClient(void * self,CFStringRef key,CFTypeRef property,CFTypeRef client)
{
    static_cast<IOHIDKeyboardFilter *>(self)->setPropertyForClient(key, property, client);
}


void IOHIDKeyboardFilter::setPropertyForClient(CFStringRef key,CFTypeRef property, CFTypeRef client)
{
    boolean_t   stickyKeyOn = _stickyKeyOn;
    boolean_t   stickyKeyDisable = _stickyKeyDisable;

    
    
    CFBooleanRef boolProp = (CFBooleanRef)property;
    if (!boolProp) boolProp = kCFBooleanFalse;
    
    if (!key) {
        goto exit;
    }
  
    HIDLogDebug("IOHIDKeyboardFilter::setPropertyForClient: %@  %@  %@", key, property, client);
  
    if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysDisabledKey), kNilOptions) == kCFCompareEqualTo) {
        stickyKeyDisable = (property && CFBooleanGetValue((CFBooleanRef)property)) ? true : false;
        HIDLogDebug("_stickyKeyDisable: %d", _stickyKeyDisable);
    
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysOnKey), kNilOptions) == kCFCompareEqualTo) {
        
        stickyKeyOn = (property && CFBooleanGetValue((CFBooleanRef)property)) ? true : false;
    
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceStickyKeysShiftTogglesKey), kNilOptions) == kCFCompareEqualTo) {
        
        _stickyKeyToggle = (property && CFBooleanGetValue((CFBooleanRef)property)) ? true : false;
        _stickyKeysShiftKeyCount = 0;
        
        HIDLogDebug("[%@] _stickyKeyToggle: %d", SERVICE_ID,  _stickyKeyToggle);
    
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceInitialKeyRepeatDelayKey), kNilOptions) == kCFCompareEqualTo) {
        
        CFNumberRef     numProp = (CFNumberRef)property;
      
        if (numProp) {
            uint64_t valueProp = 0;
            CFNumberGetValue(numProp, kCFNumberSInt64Type, &valueProp);
            _keyRepeatInitialDelayMS = (UInt32)valueProp / 1000000;
            // Bound key repeat interval. 0 disables key repeats.
            if (_keyRepeatInitialDelayMS && _keyRepeatInitialDelayMS < kMinKeyRepeatMS) {
                _keyRepeatInitialDelayMS = kMinKeyRepeatMS;
            }
            HIDLogDebug("[%@] _keyRepeatInitialDelayMS: %d", SERVICE_ID, (int)_keyRepeatInitialDelayMS);
        }
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceKeyRepeatDelayKey), kNilOptions) == kCFCompareEqualTo) {
        
        CFNumberRef     numProp = (CFNumberRef)property;
      
        if (numProp) {
            uint64_t valueProp = 0;
            CFNumberGetValue(numProp, kCFNumberSInt64Type, &valueProp);
            _keyRepeatDelayMS = (UInt32)valueProp / 1000000;
            // Bound key repeat interval.
            if (_keyRepeatDelayMS && _keyRepeatDelayMS < kMinKeyRepeatMS) {
                _keyRepeatDelayMS = kMinKeyRepeatMS;
            }
            HIDLogDebug("[%@] _keyRepeatDelayMS: %d", SERVICE_ID, (int)_keyRepeatDelayMS);
        }
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceCapsLockStateKey), kNilOptions) == kCFCompareEqualTo) {
        
        bool capsLockState = (property && CFBooleanGetTypeID() == CFGetTypeID(property)) ? CFBooleanGetValue((CFBooleanRef)property) : false;
        
        setCapsLockState(capsLockState, client);
        
        HIDLogDebug("[%@] capsLockState: %d", SERVICE_ID, capsLockState);
    }
    
    else if (CFEqual(key, kIOHIDServiceCapsLockLEDKey)) {
        if (property) {
           _capsLockLED = kIOHIDServiceCapsLockLEDKey_Auto;
            if (CFEqual (property, kIOHIDServiceCapsLockLEDKey_On)) {
                _capsLockLED = kIOHIDServiceCapsLockLEDKey_On;
            } else if (CFEqual (property, kIOHIDServiceCapsLockLEDKey_Off)) {
                _capsLockLED = kIOHIDServiceCapsLockLEDKey_Off;
            } else if (CFEqual (property, kIOHIDServiceCapsLockLEDKey_Inhibit)) {
                _capsLockLED = kIOHIDServiceCapsLockLEDKey_Inhibit;
            }
            updateCapslockLED(client);
        }

        HIDLogDebug("[%@] _capsLockLED: %@", SERVICE_ID, _capsLockLED);

    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceCapsLockLEDInhibitKey), kNilOptions) == kCFCompareEqualTo) {
        
        _capsLockLEDInhibit = CFBooleanGetValue(boolProp);
        
        // If ledInhibit is being turned on, turn off LED, if on.
        updateCapslockLED(client);

        HIDLogDebug("[%@] _capsLockLEDInhibit: %d", SERVICE_ID, _capsLockLEDInhibit);
    
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceModifierMappingPairsKey), kNilOptions) == kCFCompareEqualTo) {
        
        if (property && CFGetTypeID(property) == CFArrayGetTypeID()) {
          
            _modifiersKeyMap = createMapFromArrayOfPairs((CFArrayRef) property);
          
            // 29348498: Set caps lock to 0 if it was remapped to something else
            KeyMap::const_iterator iter;
            iter = _modifiersKeyMap.find(Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardCapsLock));
            if (iter != _modifiersKeyMap.end()) {
                setCapsLockState(false, client);
            }
            
            HIDLogDebug("[%@] _modifiersKeyMap initialized", SERVICE_ID);
        }
        
    }  else if (CFStringCompare(key, CFSTR(kIOHIDFKeyModeKey), kNilOptions) == kCFCompareEqualTo) {
        
        if (property && CFGetTypeID(property) == CFNumberGetTypeID()) {
          
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_fnKeyMode);
 
            HIDLogDebug("[%@] _fnKeyMode: %x", SERVICE_ID, _fnKeyMode);
        }
 
    } else if (CFStringCompare(key, CFSTR(kIOHIDUserKeyUsageMapKey), kNilOptions) == kCFCompareEqualTo) {
        
        if (property && CFGetTypeID(property) == CFArrayGetTypeID()) {
          
            _userKeyMap = createMapFromArrayOfPairs((CFArrayRef) property);
          
            HIDLogDebug("[%@] _userKeyMap initialized", SERVICE_ID);
        }
        
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceSlowKeysDelayKey), kNilOptions) == kCFCompareEqualTo) {
        if ( property && CFGetTypeID(property) == CFNumberGetTypeID() )
        {
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_slowKeysDelayMS);
            HIDLogDebug("[%@] _slowKeysDelayMS = %d", SERVICE_ID, (unsigned int)_slowKeysDelayMS);
            if (_slowKeysDelayMS == 0) {
                resetSlowKey();
            } else {
                _slowKeysDelayMS =  _slowKeysDelayMS < kSlowKeyMinMS ? kSlowKeyMinMS : _slowKeysDelayMS;
                resetCapsLockDelay();
#if !TARGET_OS_EMBEDDED
                resetEjectKeyDelay();
#endif
            }
        }
#if !TARGET_OS_EMBEDDED
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceEjectDelayKey), kNilOptions) == kCFCompareEqualTo) {
        if ( property && CFGetTypeID(property) == CFNumberGetTypeID() )
        {
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_ejectKeyDelayMS);
            HIDLogDebug("[%@] _ejectKeyDelayMS: %d", SERVICE_ID, (unsigned int)_ejectKeyDelayMS);
            if (_ejectKeyDelayMS == 0) {
                resetEjectKeyDelay();
            }
        }
#endif
    } else if (CFStringCompare(key, CFSTR(kIOHIDServiceCapsLockDelayKey), kNilOptions) == kCFCompareEqualTo) {
        if ( property && CFGetTypeID(property) == CFNumberGetTypeID() ) {
            
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_capsLockDelayMS);
            HIDLogDebug("[%@] _capsLockDelayMS: %d", SERVICE_ID, (unsigned int)_capsLockDelayMS);
            if (_capsLockDelayMS == 0) {
                resetCapsLockDelay();
            }
        }
    } else if (CFStringCompare(key, CFSTR(kIOHIDKeyboardCapsLockDelayOverride), kNilOptions) == kCFCompareEqualTo) {
        if ( property && CFGetTypeID(property) == CFNumberGetTypeID() ) {
            
            CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_capsLockDelayOverrideMS);
            HIDLogDebug("[%@] _capsLockDelayOverrideMS: %d", SERVICE_ID, (int)_capsLockDelayOverrideMS);
            if ((SInt32)_capsLockDelayMS == _capsLockDelayOverrideMS) {
                _capsLockDelayOverrideMS = -1;
            }
            
        }
    } else if (CFEqual(key,CFSTR(kIOHIDResetStickyKeyNotification))) {
       //reset in flight modifier key
       if (_queue) {
         
            updateStickyKeysState(kStickyKeyState_Down_Locked, kStickyKeyState_Down_Unlocked);
         
            dispatch_async(_queue, ^{
                dispatchStickyKeys(kStickyKeyState_Down);
            });
       }
       //reset in flight mouse key activation
#if !TARGET_OS_EMBEDDED
       _mouseKeyActivationCount = 0;
#endif       
       //reset in flight sticky key activation
       _stickyKeysShiftKeyCount = 0;
       
#if !TARGET_OS_EMBEDDED
    } else if (CFEqual(key, CFSTR (kIOHIDMouseKeysOnKey)) && property) {
          
        CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_numLockOn);
        HIDLogDebug("[%@] _numLockOn: %d", SERVICE_ID, _numLockOn);
        
    } else if (CFEqual(key, CFSTR (kIOHIDMouseKeysOptionTogglesKey)) && property) {
          
        CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &_mouseKeyActivationEnable);
        HIDLogDebug("[%@] _mouseKeyActivationEnable: %d", SERVICE_ID, _mouseKeyActivationEnable);

        if (!_mouseKeyActivationEnable) {
            if (_mouseKeyActivationResetTimer) {
                dispatch_source_set_timer(_mouseKeyActivationResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
            }
            _mouseKeyActivationCount = 0;

        }
#endif
    }

 
    if (stickyKeyDisable != _stickyKeyDisable) {
         _stickyKeyDisable = stickyKeyDisable;
        if (stickyKeyDisable) {
            stickyKeyOn = false;
        }
    }
    if (stickyKeyOn != _stickyKeyOn) {
        
        _stickyKeyOn = stickyKeyOn;
        
        HIDLogDebug("[%@] _stickyKeyOn: %d", SERVICE_ID, stickyKeyOn);
        
        if (_queue) {
            dispatch_async(_queue, ^{
                if (stickyKeyOn) {
                    startStickyKey ();
                } else {
                    stopStickyKey();
                }
            });
        }
    }
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::match
//------------------------------------------------------------------------------
SInt32 IOHIDKeyboardFilter::match(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    return static_cast<IOHIDKeyboardFilter *>(self)->match(service, options);
}

SInt32 IOHIDKeyboardFilter::match(IOHIDServiceRef service, IOOptionBits options)
{
    (void) options;
    
    // Keyboard filter should be loaded before NX translator filter
    // for key re-mappping purposes, See 30834442.
    _matchScore = (IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
                   IOHIDServiceConformsTo(service, kHIDPage_Consumer, kHIDUsage_Csmr_ConsumerControl)) ? 300 : 0;
    
    HIDLogDebug("(%p) for ServiceID %@ with score %d", this, IOHIDServiceGetRegistryID(service), (int)_matchScore);
    
    return _matchScore;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::filter(void * self, IOHIDEventRef event)
{
    return static_cast<IOHIDKeyboardFilter *>(self)->filter(event);
}

IOHIDEventRef IOHIDKeyboardFilter::filter(IOHIDEventRef event)
{
    IOHIDEventSenderID serviceID = 0;
    
    if (!event || IOHIDEventGetType(event) != kIOHIDEventTypeKeyboard) {
        return event;
    }

    serviceID = IOHIDEventGetSenderID(event);
    
    do
    {
        processFnKeyState(event);
        event = processKeyMappings(event);
        
        if (_slowKeysDelayMS) {
            event = processSlowKeys(event);
        }
        
        if (!_slowKeysDelayMS && _capsLockDelayMS && _capsLockDelayOverrideMS != 0) {
            event = processCapsLockDelay(event);
        }

#if !TARGET_OS_EMBEDDED
        if (!_slowKeysDelayMS && _ejectKeyDelayMS && !isModifiersPressed()) {
            event = processEjectKeyDelay(event);
        }
#endif
        
        if (!_stickyKeyDisable) {
            event = processStickyKeys(event);
        }
        
        processCapsLockState(event);

#if !TARGET_OS_EMBEDDED
        if (_mouseKeyActivationEnable) {
            event = processMouseKeys(event);
        }
#endif
        // Process key repeats if both key delays are set.
        if (_keyRepeatInitialDelayMS) {
            if (_slowKeysDelayMS) {
                event = processKeyRepeats(event, kSlowRepeatDelayMS, kSlowRepeatDelayMS);
            } else {
                event = processKeyRepeats(event, _keyRepeatInitialDelayMS, _keyRepeatDelayMS);
            }
        }
      
        processKeyState (event);
      
    } while (0);
    
    if (!event) {
        HIDLogDebug("Event cancelled for serviceID: 0x%llx", serviceID);
    }

    return event;
}


//------------------------------------------------------------------------------
// Helper functions
//------------------------------------------------------------------------------

#define MakeUsageIntoIndex(usage)  ((usage) - kHIDUsage_KeyboardLeftControl + 1)
#define MakeIndexIntoUsage(index)  (kHIDUsage_KeyboardLeftControl + (index) - 1)
#define kHighestKeyboardUsageIndex MakeUsageIntoIndex(kHIDUsage_KeyboardRightGUI)
#define kModifierIndexForFn        (kHighestKeyboardUsageIndex + 1)

//------------------------------------------------------------------------------
// getUsageForLegacyModifier
//------------------------------------------------------------------------------
bool getUsageForLegacyModifier(SInt8 modifier, UInt32& usagePage, UInt32& usage)
{
    switch (modifier) {
        case NX_MODIFIERKEY_NOACTION: // "No action" - modifier key is unmapped.
            usagePage = 0;
            usage = 0;
            return true;
        case NX_MODIFIERKEY_ALPHALOCK:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardCapsLock;
            return true;
        case NX_MODIFIERKEY_CONTROL:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardLeftControl;
            return true;
        case NX_MODIFIERKEY_SHIFT:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardLeftShift;
            return true;
        case NX_MODIFIERKEY_ALTERNATE:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardLeftAlt;
            return true;
        case NX_MODIFIERKEY_COMMAND:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardLeftGUI;
            return true;
        case NX_MODIFIERKEY_RCONTROL:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardRightControl;
            return true;
        case NX_MODIFIERKEY_RSHIFT:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardRightShift;
            return true;
        case NX_MODIFIERKEY_RALTERNATE:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardRightAlt;
            return true;
        case NX_MODIFIERKEY_RCOMMAND:
            usagePage = kHIDPage_KeyboardOrKeypad;
            usage = kHIDUsage_KeyboardRightGUI;
            return true;
        case NX_MODIFIERKEY_SECONDARYFN:
            usagePage = kHIDPage_AppleVendorTopCase;
            usage = kHIDUsage_AV_TopCase_KeyboardFn;
            return true;
        default:
            return false;
    }
}

//------------------------------------------------------------------------------
// isShiftKey
//------------------------------------------------------------------------------
bool isShiftKey(UInt32 usagePage, UInt32 usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad)
        return ((usage == kHIDUsage_KeyboardLeftShift) || (usage == kHIDUsage_KeyboardRightShift));
    else
        return false;
}

//------------------------------------------------------------------------------
// isStickyModifier
//------------------------------------------------------------------------------
bool isStickyModifier(UInt32 usagePage, UInt32 usage)
{
    if (usagePage == kHIDPage_KeyboardOrKeypad && usage == kHIDUsage_KeyboardCapsLock) {
        return false;
    }
    return isModifier(usagePage, usage);
}


//------------------------------------------------------------------------------
// isModifier
//------------------------------------------------------------------------------
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

//------------------------------------------------------------------------------
// makeModifierLeftHanded
//------------------------------------------------------------------------------
UInt32 makeModifierLeftHanded(UInt32 usage)
{
    if (usage >= kHIDUsage_KeyboardRightControl)
        usage -= (kHIDUsage_KeyboardRightControl - kHIDUsage_KeyboardLeftControl);
    
    return usage;
}

//------------------------------------------------------------------------------
// getModifierIndex
//------------------------------------------------------------------------------
UInt8 getModifierIndex(UInt32 usagePage, UInt32 usage)
{
    if (usage == kHIDUsage_KeyboardCapsLock)
        return 0;
    
    if ((usagePage == kHIDPage_AppleVendorTopCase) && (usage == kHIDUsage_AV_TopCase_KeyboardFn))
        return (kModifierIndexForFn);
    
    usage = makeModifierLeftHanded(usage);
    
    return MakeUsageIntoIndex(usage);
}

//------------------------------------------------------------------------------
// getUsageForIndex
//------------------------------------------------------------------------------
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
// isNotRepeated
//------------------------------------------------------------------------------
bool isNotRepeated(UInt32 usagePage, UInt32 usage)
{
    bool isNotRepeated = false;
    
    if (isModifier(usagePage, usage)) {
        isNotRepeated = true;
    }
    
    else if ((usagePage == kHIDPage_KeyboardOrKeypad) &&
        ((usage == kHIDUsage_KeypadNumLock) ||
         (usage == kHIDUsage_KeyboardPower) ||
         (usage == kHIDUsage_KeyboardMute))) {
        isNotRepeated = true;
    }
    
    else if ((usagePage == kHIDPage_Consumer) &&
             ((usage == kHIDUsage_Csmr_Play) ||
              (usage == kHIDUsage_Csmr_Eject)||
              (usage == kHIDUsage_Csmr_PlayOrPause) ||
              (usage == kHIDUsage_Csmr_Menu) ||
              (usage == kHIDUsage_Csmr_Power) ||
              (usage == kHIDUsage_Csmr_Sleep)
              )) {
        isNotRepeated = true;
    }
    
    else if ((usagePage == kHIDPage_AppleVendorTopCase) &&
             ((usage == kHIDUsage_AV_TopCase_IlluminationToggle) ||
              (usage == kHIDUsage_AV_TopCase_VideoMirror))) {
        isNotRepeated = true;
    }
    
    else if (usagePage == kHIDPage_Telephony) {
        isNotRepeated = true;
    }
    
    return isNotRepeated;
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::createMapFromArrayOfPairs
//------------------------------------------------------------------------------
KeyMap IOHIDKeyboardFilter::createMapFromArrayOfPairs(CFArrayRef mappings) {
    KeyMap map;
    if ( mappings == NULL || !CFArrayGetCount(mappings) ) {
        return map;
    }
    // Set mappings
    for ( CFIndex i = 0; i < CFArrayGetCount(mappings); i++ ) {
        CFDictionaryRef	pair    = NULL;
        CFNumberRef     num     = NULL;
        uint64_t        src     = 0;
        uint64_t        dst     = 0;
        
        // Get pair dictionary.
        pair = (CFDictionaryRef)CFArrayGetValueAtIndex(mappings, i);
        if ( pair == NULL || CFGetTypeID(pair) != CFDictionaryGetTypeID()) {
            continue;
        }

        // Get source modifier key.
        num = (CFNumberRef)CFDictionaryGetValue(pair, CFSTR(kIOHIDServiceModifierMappingSrcKey));
        if ( !num ) {
          continue;
        }
        CFNumberGetValue(num, kCFNumberSInt64Type, &src);
        
        // Get destination modifier key.
        num = (CFNumberRef)CFDictionaryGetValue(pair, CFSTR(kIOHIDServiceModifierMappingDstKey));
        if ( !num )  {
          continue;
        }
        CFNumberGetValue(num, kCFNumberSInt64Type, &dst);
      
        map.insert(std::make_pair(Key (src), Key (dst)));
    }
    return map;
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::createMapFromStringMap
//------------------------------------------------------------------------------
KeyMap IOHIDKeyboardFilter::createMapFromStringMap(CFStringRef mappings) {
    KeyMap      map;
    const char *stringMap;
    if ( mappings == NULL || (stringMap = (CFStringGetCStringPtr(mappings, kCFStringEncodingMacRoman))) == NULL) {
        return map;
    }
    std::istringstream ss (stringMap);
    std::string srcS, dstS;
    while(std::getline(ss, srcS, ',') && std::getline(ss, dstS, ',')) {
        uint64_t usageAndPage;
        usageAndPage = std::stoul(dstS, nullptr, 16);
        if (usageAndPage == 0) {
            continue;
        }
        Key dstKey ((uint32_t)(usageAndPage >> 16), usageAndPage & 0xffff);
        usageAndPage = std::stoul(srcS, nullptr, 16);
        Key srcKey ((uint32_t)(usageAndPage >> 16), usageAndPage & 0xffff);
        map.insert(std::make_pair(srcKey, dstKey));
    }

    return map;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processFnKeyState
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::processFnKeyState(IOHIDEventRef event) {
    UInt32  usage;
    UInt32  usagePage;
    bool    keyDown;
    
    if (!event) {
        return;
    }
    
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    
    // only interested in key up
    if (keyDown) {
        return;
    }
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    
    // only interested in Fn keys
    if ( !((usagePage == kHIDPage_AppleVendorTopCase) && (usage == kHIDUsage_AV_TopCase_KeyboardFn)) &&
         !((usagePage == kHIDPage_AppleVendorKeyboard) && (usage == kHIDUsage_AppleVendorKeyboard_Function))) {
        return;
    }
    
    // This is an Fn key up, dispatch up event for all modified active keys
    // along with all active F keys (since these may not be modified, but need to be released)
    std::map<Key,KeyAttribute> activeKey = _activeKeys;
    auto iter = activeKey.begin();
    for (;iter != activeKey.end();++iter) {
        if (!iter->first._modified) {
            if (!((iter->first.usagePage() == kHIDPage_KeyboardOrKeypad) &&
                ((iter->first.usage() >= kHIDUsage_KeyboardF1) && (iter->first.usage() <= kHIDUsage_KeyboardF12)))) {
                continue;
            }
        }
        
        IOHIDEventRef ev    = IOHIDEventCreateKeyboardEvent(
                                                            kCFAllocatorDefault,
                                                            mach_absolute_time(),
                                                            iter->first.usagePage(),
                                                            iter->first.usage(),
                                                            0,
                                                            0);
        if (ev) {
            _eventCallback(_eventTarget, _eventContext, &_serviceInterface, ev, 0);
            CFRelease(ev);
        }
    }
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processKeyMappings
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processKeyMappings(IOHIDEventRef event)
{
    UInt32  usage;
    UInt32  usagePage;
    UInt32  flags;
  
    if (!event) {
        goto exit;
    }
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    flags       = (UInt32)IOHIDEventGetEventFlags(event);
    
    // do not remap active repeat key
    if (_keyRepeatEvent &&
        usage == (UInt32)IOHIDEventGetIntegerValue(_keyRepeatEvent, kIOHIDEventFieldKeyboardUsage) &&
        usagePage == (UInt32)IOHIDEventGetIntegerValue(_keyRepeatEvent, kIOHIDEventFieldKeyboardUsagePage)
        ) {
        goto exit;
    }
  
    // Ignore all events with flags - these events are generally synthesized
    // and have already been remapped.
    if ((flags & kKeyboardOptionMask) == 0  &&
         IOHIDEventGetIntegerValue (event, kIOHIDEventFieldKeyboardSlowKeyPhase) == kIOHIDKeyboardSlowKeyNone  &&
         !isDelayedEvent(event)) {
      
        Key src = Key (usagePage, usage);
        Key key = remapKey (src);

        if (!key.isValid()) {
          return NULL;
        }
      
        if (key.usage() != usage || key.usagePage() != usagePage) {
            _IOHIDEventSetAttachment(event, kIOHIDEventAttachment_Modified, kCFBooleanTrue, 0);
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardUsage, key.usage());
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage, key.usagePage());
        }
    }
  
exit:

    return event;
    
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::remapKey
//------------------------------------------------------------------------------
Key  IOHIDKeyboardFilter::remapKey(Key key) {

    KeyMap::const_iterator iter;
  
    if (key == Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardF5) &&
       (isKeyPressed (Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardLeftGUI)) ||
        isKeyPressed (Key(kHIDPage_KeyboardOrKeypad, kHIDUsage_KeyboardRightGUI)))) {
        
       return key;
    }
  
    boolean_t FnKeyDown = isKeyPressed(Key(kHIDPage_AppleVendorTopCase, kHIDUsage_AV_TopCase_KeyboardFn)) ||
                          isKeyPressed(Key(kHIDPage_AppleVendorKeyboard, kHIDUsage_AppleVendorKeyboard_Function));
  
    if (!(FnKeyDown ^ _fnKeyMode)) {
        iter = _fnFunctionUsageMapKeyMap.find(key);
        if (iter != _fnFunctionUsageMapKeyMap.end()) {
            key = Key (iter->second._value);
        }
    }
    
    if (FnKeyDown) {
        iter = _fnKeyboardUsageMapKeyMap.find(key);
        if (iter != _fnKeyboardUsageMapKeyMap.end()) {
            key = Key (iter->second._value);
        }
    }
    
    if (isNumLockMode()) {
        iter = _numLockKeyboardUsageMapKeyMap.find (key);
        if (iter != _numLockKeyboardUsageMapKeyMap.end()) {
            key = Key (iter->second._value);
        }
    }

    iter = _modifiersKeyMap.find(key);
    if (iter != _modifiersKeyMap.end()) {
        key = Key (iter->second._value);
    }

    iter = _userKeyMap.find(key);
    if (iter != _userKeyMap.end()) {
        key = Key (iter->second._value);
    }
    return key;
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::isNumLockMode
//------------------------------------------------------------------------------
bool  IOHIDKeyboardFilter::isNumLockMode() {
  return _numLockOn ? true : false;
}


bool  IOHIDKeyboardFilter::isKeyPressed (Key key) {
  return _activeKeys.find (key) != _activeKeys.end();
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::getStickyKeyState
//------------------------------------------------------------------------------
StickyKeyState IOHIDKeyboardFilter::getStickyKeyState(UInt32 usagePage, UInt32 usage)
{
    return _stickyKeyState[getModifierIndex(usagePage, usage)];
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::setStickyKeyState
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::setStickyKeyState(UInt32 usagePage, UInt32 usage, StickyKeyState state)
{
    uint8_t index = getModifierIndex(usagePage, usage);
    HIDLogDebug("StickyKey state %x -> %x", (unsigned int)_stickyKeyState[index], (unsigned int)state);
    _stickyKeyState[index] = state;
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
// IOHIDKeyboardFilter::processStickyKeyDown
//------------------------------------------------------------------------------
UInt32 IOHIDKeyboardFilter::processStickyKeyDown(UInt32 usagePage, UInt32 usage, UInt32 &flags)
{

    StickyKeyState state = getStickyKeyState(usagePage, usage);
    StickyKeyState newState = state;
    UInt32  stickyKeyPhase = 0;
    switch (state) {
        case kStickyKeyState_Reset:
            // First mod press
            flags = kIOHIDKeyboardStickyKeyDown;
            stickyKeyPhase = kIOHIDKeyboardStickyKeyPhaseDown;
            newState = kStickyKeyState_Down_Locked;
            break;
        case kStickyKeyState_Down:
            flags = kIOHIDKeyboardStickyKeyLocked;
            stickyKeyPhase = kIOHIDKeyboardStickyKeyPhaseLocked;
            newState = kStickyKeyState_Locked;
            break;
        case kStickyKeyState_Locked:
            // Send this event, user is unlocking
            newState = kStickyKeyState_Reset;
            break;
            
        default:
            HIDLogError("StickyKey DOWN in bad state for 0x%x:0x%x", (int)usagePage, (int)usage);
    }
  
    setStickyKeyState(usagePage, usage, newState);
  
    HIDLogDebug("StickyKey DOWN 0x%x:0x%x phase 0x%x",
              (unsigned int)usage,
              (unsigned int)usagePage,
              (unsigned int)stickyKeyPhase
              );

    return stickyKeyPhase;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processStickyKeyUp
//------------------------------------------------------------------------------
UInt32 IOHIDKeyboardFilter::processStickyKeyUp(UInt32 usagePage, UInt32 usage, UInt32 &flags)
{
    StickyKeyState state = getStickyKeyState(usagePage, usage);
    StickyKeyState newState = state;
    UInt32 stickyKeyPhase = 0;

    switch (state) {
        case kStickyKeyState_Reset:
            // going from lock to unlocked
            flags = kIOHIDKeyboardStickyKeyUp;
            stickyKeyPhase = kIOHIDKeyboardStickyKeyPhaseUp;
            break;
        case kStickyKeyState_Down_Locked:
            newState = kStickyKeyState_Down;
            break;
        case kStickyKeyState_Down_Unlocked:
            flags = kIOHIDKeyboardStickyKeyUp;
            stickyKeyPhase = kIOHIDKeyboardStickyKeyPhaseUp;
            newState = kStickyKeyState_Reset;
            break;
        case kStickyKeyState_Locked:
            // The down event caused lock, filter the up
            break;
            
        default:
             HIDLogError("StickyKey UP in bad state for 0x%x:0x%x", (int)usagePage, (int)usage);
    }
    if (state != newState) {
       setStickyKeyState(usagePage, usage, newState);
    }

    HIDLogDebug("StickyKey UP 0x%x:0x%x phase 0x%x",
              (unsigned int)usage,
              (unsigned int)usagePage,
              (unsigned int)stickyKeyPhase
              );
  
    return stickyKeyPhase;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::updateStickyKeysState
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::updateStickyKeysState(StickyKeyState from, StickyKeyState to) {
    int i = 0;
    for (;i < MAX_STICKY_KEYS; i++) {
        UInt32          usage = 0;
        UInt32          usagePage = 0;
        StickyKeyState  state;
        
        getUsageForIndex(i, usagePage, usage);
        
        state = getStickyKeyState(usagePage, usage);
        
        if (state == from) {
            setStickyKeyState(usagePage, usage, to);
        }
    }
}



//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::dispatchStickyKeys
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::dispatchStickyKeys(int stateMask)
{
    int  i = 0;
    
    for (;i < MAX_STICKY_KEYS; i++) {
        UInt32          usage = 0;
        UInt32          usagePage = 0;
        StickyKeyState  state;
        
        getUsageForIndex(i, usagePage, usage);
        
        state = getStickyKeyState(usagePage, usage);
        //HIDLogDebug("StickyKey [%d] 0x%x:0x%x state 0x%x mask 0x%x", (int)i, (int)usagePage, (int)usage, (unsigned int)state, stateMask);

        if ((state & stateMask) == 0)
            continue;

        IOHIDEventRef event = IOHIDEventCreateKeyboardEvent(kCFAllocatorDefault, mach_absolute_time(), usagePage, usage, 0, kIOHIDKeyboardStickyKeyUp);
        if (!event) {
            continue;
        }
        IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardStickyKeyPhase, kIOHIDKeyboardStickyKeyPhaseUp);
        _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
        
        CFRelease(event);
        
        setStickyKeyState(usagePage, usage, kStickyKeyState_Reset);
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processShiftKey
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::processShiftKey(void)
{

    if (_stickyKeyToggle == false) {
        return;
    }
    
    bool timerWasEnabed = _stickyKeysShiftKeyCount++;
    
    if (!timerWasEnabed)
        dispatch_source_set_timer(_stickyKeysShiftResetTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * kStickyKeysShiftKeyInterval),
                                  DISPATCH_TIME_FOREVER,
                                  0);
                                  
    
    if (_stickyKeysShiftKeyCount >= kStickyKeysEnableCount) {
      
        _stickyKeysShiftKeyCount = 0;
        _stickyKeyOn = !_stickyKeyOn;
      
        HIDLogDebug("[%@] StickyKey state change (5xSHIFT) to %s", SERVICE_ID, _stickyKeyOn ? "ON" : "OFF");
      
        IOHIDServiceSetProperty(_service, CFSTR(kIOHIDServiceStickyKeysOnKey), (_stickyKeyOn ? kCFBooleanTrue : kCFBooleanFalse));

#if !TARGET_OS_EMBEDDED
        setHIDSystemParam (CFSTR(kIOHIDStickyKeysOnKey), _stickyKeyOn ? 0 : 1);
#endif        
        if (_stickyKeyOn) {
            startStickyKey();
        } else {
            stopStickyKey();
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processStickyKeys
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processStickyKeys(IOHIDEventRef event)
{
    UInt32  usage;
    UInt32  usagePage;
    UInt32  flags = 0;
    bool    keyDown;
    bool    stickyKeysOn  = _stickyKeyOn;
    UInt32  stickyKeyPhase;
    
    if (!event || (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardStickyKeyPhase) == kIOHIDKeyboardStickyKeyPhaseUp) || IOHIDEventIsSlowKeyPhaseEvent(event)) {
        goto exit;
    }
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);

    //
    // If this was shift key check for toggles
    if (isShiftKey(usagePage, usage)) {
        if (keyDown == 0) {
            processShiftKey();
        }
    } else {
        _stickyKeysShiftKeyCount = 0;
    }
  
    //
    // Stop processing if sticky keys are not ON
    if (!_stickyKeyOn) {
        //
        // Sticky key turned off by shift key
        if (_stickyKeyOn != stickyKeysOn) {
            IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | kIOHIDKeyboardStickyKeysOff);
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardStickyKeyToggle, kIOHIDKeyboardStickyKeyToggleOff);
        }
        goto exit;
    }
    
    //
    // If this key is not a modifier, dispatch the up events for any
    // stuck keys
    if (!isStickyModifier(usagePage, usage)) {
        if (_stickyKeysShiftKeyCount)
            dispatch_source_set_timer(_stickyKeysShiftResetTimer, DISPATCH_TIME_FOREVER, 0, 0);
        
        // Dispatch any stuck keys on a non modifier key up
        if (!keyDown){
            dispatch_async(_queue, ^{
                dispatchStickyKeys(kStickyKeyState_Down);
            });
            
            _stickyKeysShiftKeyCount = 0;
        }
        updateStickyKeysState(kStickyKeyState_Down_Locked, kStickyKeyState_Down_Unlocked);
        
    } else {
        //
        // Sticky keys turned on by shift key ? send this shift key through
        if (_stickyKeyOn != stickyKeysOn) {
            IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | kIOHIDKeyboardStickyKeysOn);
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardStickyKeyToggle, kIOHIDKeyboardStickyKeyToggleOn);
            goto exit;
        }
        
        //
        // At this point the the key we have is a modifier key
        // send it down the state machine
        if (keyDown) {
            stickyKeyPhase = processStickyKeyDown(usagePage, usage, flags);
        } else {
            stickyKeyPhase = processStickyKeyUp(usagePage, usage, flags);
        }
        
        if (stickyKeyPhase == 0) {
            event = NULL;
        } else {
            usage = makeModifierLeftHanded(usage);
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardUsage, usage);
            IOHIDEventSetEventFlags(event, IOHIDEventGetEventFlags(event) | flags);
            IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardStickyKeyPhase, stickyKeyPhase);
        }
    }
    
exit:
    return event;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::startStickyKey
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::startStickyKey () {
    std::map<Key,KeyAttribute> activeKey = _activeKeys;
    auto iter = activeKey.begin();
    for (;iter != activeKey.end();++iter) {
        if (iter->first.isModifier()) {
            IOHIDEventRef event = IOHIDEventCreateKeyboardEvent(
                                      kCFAllocatorDefault,
                                      mach_absolute_time(),
                                      iter->first.usagePage(),
                                      iter->first.usage(),
                                      0,
                                      kIOHIDKeyboardStickyKeyUp);
            if (event) {
                IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardStickyKeyPhase, kIOHIDKeyboardStickyKeyPhaseUp);
                _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
                CFRelease(event);
            }
        }
    }
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::stopStickyKey
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::stopStickyKey () {
    dispatchStickyKeys (kStickyKeyState_Down | kStickyKeyState_Down_Locked | kStickyKeyState_Locked);
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::resetSlowKey
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::resetSlowKey(void)
{
    if (_slowKeysSlowEvent) {
        CFRelease(_slowKeysSlowEvent);
        _slowKeysSlowEvent = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::dispatchSlowKey
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::dispatchSlowKey(void)
{
    IOHIDEventRef event = _slowKeysSlowEvent;
    _slowKeysSlowEvent = NULL;
  
    if (!event)
        return;

    IOHIDEventSetTimeStamp(event, mach_absolute_time());
    IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardSlowKeyPhase, kIOHIDKeyboardSlowKeyOn);
    
    _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
  
    CFRelease(event);
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processSlowKeys
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processSlowKeys(IOHIDEventRef event)
{
    UInt32      usage         = 0;
    UInt32      usagePage     = 0;
    bool        keyDown       = 0;

    if (!event)
        goto exit;
    
    if (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardSlowKeyPhase) == kIOHIDKeyboardSlowKeyOn) {
        goto exit;
    }
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
    
#if !TARGET_OS_EMBEDDED
    // 29559398: slow key should not be allowed for mesa button since
    // triple-click is used to bring up accessibility options.
    if (usagePage == kHIDPage_Consumer && usage == kHIDUsage_Csmr_Menu) {
        goto exit;
    }
#endif
 
    if (keyDown) {
#if !TARGET_OS_EMBEDDED
        if (_slowKeysSlowEvent &&
            ((UInt32)IOHIDEventGetIntegerValue(_slowKeysSlowEvent, kIOHIDEventFieldKeyboardUsage) != usage ||
             (UInt32)IOHIDEventGetIntegerValue(_slowKeysSlowEvent, kIOHIDEventFieldKeyboardUsagePage)!= usagePage)) {
            
            IOHIDEventSetIntegerValue (event, kIOHIDEventFieldKeyboardSlowKeyPhase, kIOHIDKeyboardSlowKeyPhaseAbort);
            
            dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);
            resetSlowKey();
            
        } else {
            
            if (_slowKeysSlowEvent) {
                CFRelease(_slowKeysSlowEvent);
            }
            _slowKeysSlowEvent = IOHIDEventCreateCopy (CFGetAllocator(event), event);
            IOHIDEventSetIntegerValue (event, kIOHIDEventFieldKeyboardSlowKeyPhase, kIOHIDKeyboardSlowKeyPhaseStart);

            dispatch_source_set_timer(_slowKeysTimer,
                                      dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * _slowKeysDelayMS),
                                      DISPATCH_TIME_FOREVER, 0);
        }
#else
        CFRetain(event);
        if (_slowKeysSlowEvent) {
          CFRelease(_slowKeysSlowEvent);
        }
        _slowKeysSlowEvent = event;

        dispatch_source_set_timer(_slowKeysTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * _slowKeysDelayMS),
                                  DISPATCH_TIME_FOREVER, 0);
        event = NULL;
#endif
    } else {
        if (_slowKeysSlowEvent) {
            if (((UInt32)IOHIDEventGetIntegerValue(_slowKeysSlowEvent, kIOHIDEventFieldKeyboardUsage) == usage) &&
                ((UInt32)IOHIDEventGetIntegerValue(_slowKeysSlowEvent, kIOHIDEventFieldKeyboardUsagePage) == usagePage)) {
              
                dispatch_source_set_timer(_slowKeysTimer, DISPATCH_TIME_FOREVER, 0, 0);
              
                if ((IOHIDEventGetEventFlags(_slowKeysSlowEvent) & kIOHIDKeyboardIsRepeat) == 0) {
                    event = NULL;
                }
                resetSlowKey();
            }
        }
    }
    
exit:
    return event;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::dispatchKeyRepeat
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::dispatchKeyRepeat(void)
{
    IOHIDEventRef event =  _keyRepeatEvent;
    _keyRepeatEvent = NULL;
  
    if (!event) {
        return ;
    }
    
    IOHIDEventSetTimeStamp(event, mach_absolute_time());
    
    _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
    
    CFRelease(event);
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processKeyRepeats
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processKeyRepeats(IOHIDEventRef event, UInt32 keyRepeatInitialDelayMS, UInt32 keyRepeatDelayMS)
{
    UInt32      usage              = 0;
    UInt32      usagePage          = 0;
    bool        keyDown            = 0;
    
    if (!event || IOHIDEventIsSlowKeyPhaseEvent(event)) {
        goto exit;
    }
    
    //
    // if (keyDown)
    //      kill previous key repeat timer
    //      start repeat timer for this key
    //      key down (as usual)
    // else // key up
    //      if (repeatTimerForThisKey)
    //          kill key repeat timer
    //      key up (as usual)
    //
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    
    if (isNotRepeated(usagePage, usage)) {
        goto exit;
    }
    
    if (keyDown) {
        
        if (_keyRepeatEvent) {
            CFRelease(_keyRepeatEvent);
        }
 
        _keyRepeatEvent = IOHIDEventCreateKeyboardEvent(CFGetAllocator(event), mach_absolute_time(), usagePage, usage, true, kIOHIDKeyboardIsRepeat);
        
        dispatch_source_set_timer(_keyRepeatTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC *
                                                ((IOHIDEventGetEventFlags(event) & kIOHIDKeyboardIsRepeat) ? keyRepeatDelayMS : keyRepeatInitialDelayMS)),
                                  DISPATCH_TIME_FOREVER, 0);
    } else {
        if (_keyRepeatEvent &&
            usage == (UInt32)IOHIDEventGetIntegerValue(_keyRepeatEvent, kIOHIDEventFieldKeyboardUsage) &&
            usagePage == (UInt32)IOHIDEventGetIntegerValue(_keyRepeatEvent, kIOHIDEventFieldKeyboardUsagePage)
            ) {
            dispatch_source_set_timer(_keyRepeatTimer, DISPATCH_TIME_FOREVER, 0, 0);
            CFRelease(_keyRepeatEvent);
            _keyRepeatEvent = NULL;
        }
    }
    
exit:
    return event;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processCapsLockState
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::processCapsLockState(IOHIDEventRef event)
{
    UInt32       usage;
    UInt32       usagePage;
    bool         keyDown;
    
    if (!event || IOHIDEventIsSlowKeyPhaseEvent(event)) {
        goto exit;
    }
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
  
    if ( (usagePage == kHIDPage_KeyboardOrKeypad) && (usage == kHIDUsage_KeyboardCapsLock)) {
        // LED and Caps Lock state change on key down.
        if (keyDown) {
            setCapsLockState(!_capsLockState, NULL);
            IOHIDServiceSetProperty(_service, CFSTR("HIDCapsLockStateCache"), _capsLockState ? kCFBooleanTrue : kCFBooleanFalse);
        }
    }
    
exit:
    return;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::resetCapsLockDelay
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::resetCapsLockDelay(void)
{
    
    if (_delayedCapsLockEvent) {
        CFRelease(_delayedCapsLockEvent);
        _delayedCapsLockEvent = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::dispatchCapsLock
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::dispatchCapsLock(void)
{
    IOHIDEventRef event = _delayedCapsLockEvent;
    _delayedCapsLockEvent = NULL;
    
    if (!event)
        return;
    
    _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
    
    CFRelease(event);
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processCapsLockDelay
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processCapsLockDelay(IOHIDEventRef event)
{
    UInt32       usage;
    UInt32       usagePage;
    bool         keyDown;
    UInt32       capsLockDelayMS;
    
    
    if (!event)
        goto exit;
    
    //
    // If (keyDown && !capLockState) // Delay on activation only
    //      start new caps lock timer, override any old timer
    //      kill the down event
    // else // if key up or caps lock already on (deactivation)
    //      if (capsLockDelayInProgress)
    //          abort caps lock timer
    //          kill the up event
    //      else
    //          send caps lock up
    //
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    
    if ( !((usagePage == kHIDPage_KeyboardOrKeypad) && (usage == kHIDUsage_KeyboardCapsLock))) {
        goto exit;
    }
  
    // Let through already-delayed caps lock events.
    if (isDelayedEvent(event)) {
        _IOHIDEventRemoveAttachment(event, kIOHIDEventAttachment_Delayed, 0);
        goto exit;
    }


    capsLockDelayMS = _capsLockDelayOverrideMS > 0 ? _capsLockDelayOverrideMS : _capsLockDelayMS;

    if (keyDown && !_capsLockState) {
        dispatch_source_set_timer(_capsLockDelayTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * capsLockDelayMS),
                                  DISPATCH_TIME_FOREVER, 0);
      
        _IOHIDEventSetAttachment(event, kIOHIDEventAttachment_Delayed, kCFBooleanTrue, 0);

        _delayedCapsLockEvent = event;

        CFRetain(event);
        event = NULL;
    } else {
        if (_delayedCapsLockEvent) {
            
            dispatch_source_set_timer(_capsLockDelayTimer, DISPATCH_TIME_FOREVER, 0, 0);
                
            resetCapsLockDelay();
                
            event = NULL;
        }
    }
    
exit:
    return event;
}

#if !TARGET_OS_EMBEDDED
//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::resetEjectKeyDelay
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::resetEjectKeyDelay(void)
{
    
    if (_delayedEjectKeyEvent) {
        CFRelease(_delayedEjectKeyEvent);
        _delayedEjectKeyEvent = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::dispatchEjectKey
//------------------------------------------------------------------------------
void  IOHIDKeyboardFilter::dispatchEjectKey(void)
{
    IOHIDEventRef event = _delayedEjectKeyEvent;
    _delayedEjectKeyEvent = NULL;
    
    if (!event)
        return;
    
    _eventCallback(_eventTarget, _eventContext, &_serviceInterface, event, 0);
    
    CFRelease (event);
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processEjectKeyDelay
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processEjectKeyDelay(IOHIDEventRef event)
{
    UInt32      usage       = 0;
    UInt32      usagePage   = 0;
    bool        keyDown     = 0;
    
    
    if (!event)
        goto exit;

    //
    // If (keyDown)
    //      start new eject timer, override any old timer
    //      kill the down event
    // else // if key up
    //      if (ejectKeyDelayInProgress)
    //          abort ejectKey timer
    //          kill the up event
    //      else
    //          send eject up
    //
    
    usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    
    if (!((usagePage == kHIDPage_Consumer) && (usage == kHIDUsage_Csmr_Eject))) {
        goto exit;
    }
  
    if (isDelayedEvent(event)) {
        _IOHIDEventRemoveAttachment(event, kIOHIDEventAttachment_Delayed, 0);
        goto exit;
    }

    HIDLogDebug("keyDown = %d _delayedEjectKeyEvent = %p", (int)keyDown, _delayedEjectKeyEvent);
    
    if (keyDown) {
        dispatch_source_set_timer(_ejectKeyDelayTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * _ejectKeyDelayMS),
                                  DISPATCH_TIME_FOREVER, 0);
        
      
        _IOHIDEventSetAttachment(event, kIOHIDEventAttachment_Delayed, kCFBooleanTrue, 0);

        _delayedEjectKeyEvent = event;
        
        CFRetain(_delayedEjectKeyEvent);
        
        event = NULL;
    }
    
    else {
        if (_delayedEjectKeyEvent) {
            
            dispatch_source_set_timer(_ejectKeyDelayTimer, DISPATCH_TIME_FOREVER, 0, 0);
            
            resetEjectKeyDelay();
            
            event = NULL;
        }
    }
    
exit:
    return event;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processMouseKeys
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDKeyboardFilter::processMouseKeys (IOHIDEventRef event) {
  uint32_t  usage;
  uint32_t  usagePage;
  uint32_t  keyDown;
  
  if (!event || IOHIDEventIsSlowKeyPhaseEvent(event)) {
      goto exit;
  }
  
  usage       = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
  usagePage   = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
  keyDown     = (UInt32)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
  
  if (usagePage == kHIDPage_KeyboardOrKeypad &&
      (usage == kHIDUsage_KeyboardLeftAlt || usage == kHIDUsage_KeyboardRightAlt)
      ) {
    
    
    if (_mouseKeyActivationCount == 0) {
        dispatch_source_set_timer(_mouseKeyActivationResetTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC * kMouseKeyActivationReset),
                                  DISPATCH_TIME_FOREVER,
                                  0
                                  );
    }
    if (keyDown) {
    
        _mouseKeyActivationCount++;
    
    } else if (_mouseKeyActivationCount >= kMouseKeyActivationCount) {
    
        HIDLogDebug("[%@] MouseKey (5xALT) Toggle", SERVICE_ID);
        IOHIDEventSetIntegerValue(event, kIOHIDEventFieldKeyboardMouseKeyToggle, kIOHIDKeyboardMouseKeyToggle);
        _mouseKeyActivationCount = 0;
        
    }
  } else {
    _mouseKeyActivationCount = 0;
  }

exit:
  
  return event;
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::setHIDSystemParam
//------------------------------------------------------------------------------
kern_return_t IOHIDKeyboardFilter::setHIDSystemParam(CFStringRef key, uint32_t property) {
    kern_return_t kr = kIOReturnInvalid;
    NXEventHandle hidSystem = openHIDSystem();
    
    if (hidSystem) {
        CFNumberRef value = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &property);
        if (value) {
            kr = IOConnectSetCFProperty(hidSystem, key , value);
            CFRelease(value);
        }
        IOServiceClose(hidSystem);
    }
    return kr;
}
#endif

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::processKeyState
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::processKeyState (IOHIDEventRef event) {
    uint32_t  usage;
    uint32_t  usagePage;
    uint32_t  keyDown;
    uint32_t  flags;
    CFBooleanRef modified = NULL;
    Key key;
  
    if (!event || IOHIDEventIsSlowKeyPhaseEvent(event)) {
      return;
    }
  
    usage       = (uint32_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage);
    usagePage   = (uint32_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage);
    keyDown     = (uint32_t)IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown);
    flags       = (uint32_t)IOHIDEventGetEventFlags(event);
    
    modified = (CFBooleanRef)_IOHIDEventCopyAttachment(event, kIOHIDEventAttachment_Modified, 0);
    if (modified == kCFBooleanTrue) {
        key = Key(usagePage, usage, true);
    } else {
        key = Key(usagePage, usage);
    }
    
    if (modified) {
        CFRelease(modified);
    }
    
    if (keyDown) {
        _activeKeys.insert(std::make_pair(key, KeyAttribute(flags)));
    } else {
        auto iter = _activeKeys.find(key);
        if (iter!=_activeKeys.end()) {
            _activeKeys.erase(iter);
        }
    }
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::setCapsLockState
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::setCapsLockState(boolean_t state, CFTypeRef client) {
    if (state == _capsLockState) {
        return;
    }
    
    _capsLockState = state;
    
    [[AppleKeyboardStateManager sharedManager] setCapsLockEnabled:_capsLockState locationID:_locationID];
    
    HIDLogInfo("[%@] Set capslock state: %d client: %@", SERVICE_ID, state, client);
    
#if !TARGET_OS_EMBEDDED
    updateCapslockLED(client);
#endif
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::updateCapslockLED
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::updateCapslockLED(CFTypeRef client) {
  
    if ((_supportedModifiers & NX_ALPHASHIFT_STATELESS_MASK) == 0) {
        return;
    }

    _capsLockLEDState = _capsLockState;
    if (CFEqual(_capsLockLED,  kIOHIDServiceCapsLockLEDKey_Inhibit)) {
      return;
    } else if (CFEqual(_capsLockLED,  kIOHIDServiceCapsLockLEDKey_On)) {
        _capsLockLEDState = true;
    } else if (CFEqual(_capsLockLED,  kIOHIDServiceCapsLockLEDKey_Off)) {
        _capsLockLEDState = false;
    } else if (_capsLockLEDInhibit) {
        // this is wrong  and we want to deprecate , keep it to maintain compatibility
        _capsLockLEDState = false;
    }
    
    if (_service) {
        HIDLogInfo("[%@] Set capslock LED: %d client: %@", SERVICE_ID, _capsLockLEDState, client);
        IOHIDServiceSetElementValue(_service, kHIDPage_LEDs, kHIDUsage_LED_CapsLock, _capsLockLEDState);
    }
}


//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::serializeMapper
//------------------------------------------------------------------------------
CFMutableArrayRefWrap IOHIDKeyboardFilter::serializeMapper (const KeyMap &mapper) const {

    CFMutableArrayRefWrap result ((int)mapper.size());
    auto iter = mapper.begin();
    for ( ;iter!= mapper.end();iter++) {
      CFMutableDictionaryRefWrap pair (2);
      pair.SetValueForKey(CFSTR("Src"), iter->first._value);
      pair.SetValueForKey(CFSTR("Dst"), iter->second._value);
      result.Append(pair);
    }
    return result;
}
//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::serialize
//------------------------------------------------------------------------------
void IOHIDKeyboardFilter::serialize (CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDKeyboardFilter"));
    serializer.SetValueForKey(CFSTR(kFnFunctionUsageMapKey),serializeMapper(_fnFunctionUsageMapKeyMap));
    serializer.SetValueForKey(CFSTR(kFnKeyboardUsageMapKey), serializeMapper(_fnKeyboardUsageMapKeyMap));
    serializer.SetValueForKey(CFSTR(kIOHIDServiceModifierMappingPairsKey), serializeMapper(_modifiersKeyMap));
    serializer.SetValueForKey(CFSTR(kNumLockKeyboardUsageMapKey), serializeMapper(_numLockKeyboardUsageMapKeyMap));
    serializer.SetValueForKey(CFSTR(kIOHIDUserKeyUsageMapKey), serializeMapper(_userKeyMap));
    serializer.SetValueForKey(CFSTR(kIOHIDServiceInitialKeyRepeatDelayKey), (uint64_t)_keyRepeatInitialDelayMS);
    serializer.SetValueForKey(CFSTR(kIOHIDServiceKeyRepeatDelayKey), (uint64_t)_keyRepeatDelayMS);
    serializer.SetValueForKey(CFSTR("CapsLockState"), (uint64_t)_capsLockState);
    serializer.SetValueForKey(CFSTR("MatchScore"), (uint64_t)_matchScore);
}


void IOHIDKeyboardFilter::defaultEventCallback (void *target __unused, void *refcon __unused, void *sender __unused, IOHIDEventRef event __unused, IOOptionBits options __unused) {
    HIDLogDebug("Event dropped: %@", event);
}

bool IOHIDKeyboardFilter::isDelayedEvent(IOHIDEventRef event)
{
    bool            result = false;
    CFBooleanRef    value;
    
    value = (CFBooleanRef)_IOHIDEventCopyAttachment(event, kIOHIDEventAttachment_Delayed, 0);
    if (value) {
        result = (value == kCFBooleanTrue);
        CFRelease(value);
    }
    
    return result;
}

#if !TARGET_OS_EMBEDDED
//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::getKeyboardID
//------------------------------------------------------------------------------
uint32_t IOHIDKeyboardFilter::getKeyboardID () {
    // Determinei keyboard ID
    CFTypeRef value;
    uint32_t  keyboardID = kgestUSBUnknownANSIkd;
    value = IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDAltHandlerIdKey));
    if (value) {
        CFNumberGetValue((CFNumberRef)value, kCFNumberSInt32Type, &keyboardID);
        CFRelease(value);
    } else {
        uint16_t productID = 0xffff;
        uint16_t vendorID  = 0xffff;
    
        value = IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDProductIDKey));
        if (value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt16Type, &productID);
            CFRelease(value);
        }
    
        value = IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDVendorIDKey));
        if (value) {
            CFNumberGetValue((CFNumberRef)value, kCFNumberSInt16Type, &vendorID);
            CFRelease(value);
        }
        keyboardID = getKeyboardID(productID, vendorID);
    }
    return keyboardID;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::getKeyboardID
//------------------------------------------------------------------------------
uint32_t IOHIDKeyboardFilter::getKeyboardID (uint16_t productID, uint16_t vendorID) {
  uint16_t keyboardID = kgestUSBUnknownANSIkd;
  if (vendorID == kIOUSBVendorIDAppleComputer)
  {
    switch (productID)
    {
      case kprodUSBCosmoANSIKbd:  //Cosmo ANSI is 0x201
        keyboardID = kgestUSBCosmoANSIKbd; //0xc6
        break;
      case kprodUSBCosmoISOKbd:  //Cosmo ISO
        keyboardID = kgestUSBCosmoISOKbd; //0xc7
        break;
      case kprodUSBCosmoJISKbd:  //Cosmo JIS
        keyboardID = kgestUSBCosmoJISKbd;  //0xc8
        break;
      case kprodUSBAndyANSIKbd:  //Andy ANSI is 0x204
        keyboardID = kgestUSBAndyANSIKbd; //0xcc
        break;
      case kprodUSBAndyISOKbd:  //Andy ISO
        keyboardID = kgestUSBAndyISOKbd; //0xcd
        break;
      case kprodUSBAndyJISKbd:  //Andy JIS is 0x206
        keyboardID = kgestUSBAndyJISKbd; //0xce
        break;
      case kprodQ6ANSIKbd:  //Q6 ANSI
        keyboardID = kgestQ6ANSIKbd;
        break;
      case kprodQ6ISOKbd:  //Q6 ISO
        keyboardID = kgestQ6ISOKbd;
        break;
      case kprodQ6JISKbd:  //Q6 JIS
        keyboardID = kgestQ6JISKbd;
        break;
      case kprodQ30ANSIKbd:  //Q30 ANSI
        keyboardID = kgestQ30ANSIKbd;
        break;
      case kprodQ30ISOKbd:  //Q30 ISO
        keyboardID = kgestQ30ISOKbd;
        break;
      case kprodQ30JISKbd:  //Q30 JIS
        keyboardID = kgestQ30JISKbd;
        break;
      case kprodFountainANSIKbd:  //Fountain ANSI
        keyboardID = kgestFountainANSIKbd;
        break;
      case kprodFountainISOKbd:  //Fountain ISO
        keyboardID = kgestFountainISOKbd;
        break;
      case kprodFountainJISKbd:  //Fountain JIS
        keyboardID = kgestFountainJISKbd;
        break;
      case kprodSantaANSIKbd:  //Santa ANSI
        keyboardID = kgestSantaANSIKbd;
        break;
      case kprodSantaISOKbd:  //Santa ISO
        keyboardID = kgestSantaISOKbd;
        break;
      case kprodSantaJISKbd:  //Santa JIS
        keyboardID = kgestSantaJISKbd;
        break;
      default:
        keyboardID = kgestUSBCosmoANSIKbd;
        break;
    }
  }
  return keyboardID;
}

//------------------------------------------------------------------------------
// IOHIDKeyboardFilter::isModifiersPressed
//------------------------------------------------------------------------------
bool IOHIDKeyboardFilter::isModifiersPressed () {
    auto iter = _activeKeys.begin();
    for ( ; iter != _activeKeys.end(); ++iter) {
        if (iter->first.isModifier()) {
            return true;
        }
    }
    return false;
}

static NXEventHandle openHIDSystem(void)
{
    kern_return_t    kr;
    io_service_t     service = MACH_PORT_NULL;
    NXEventHandle    handle = MACH_PORT_NULL;
    mach_port_t      masterPort;
    
    kr = IOMasterPort(MACH_PORT_NULL, &masterPort);
    if(kr == KERN_SUCCESS) {
        service = IORegistryEntryFromPath(masterPort, kIOServicePlane ":/IOResources/IOHIDSystem");
        if (service) {
            IOServiceOpen(service, mach_task_self(), kIOHIDParamConnectType, &handle);
            IOObjectRelease(service);
        }
    }
    
    return handle;
}

#endif
