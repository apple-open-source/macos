/*
 *  IOHIDNXEventTranslatorSessionFilter.cpp
 *  IOHIDEventSystemPlugIns
 *
 *  Created by Rob Yepez on 05/21/2013.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
 */

#include <new>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
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
#include <IOKit/pwr_mgt/IOPM.h>
#include <SkyLight/SkyLight.h>
#include <SkyLight/SLSDisplayManager.h>
#include "IOHIDNXEventTranslatorServiceFilter.h"
#include "IOHIDDebug.h"
#include "IOHIDNXEventTranslatorSessionFilter.h"
#include <IOKit/hidsystem/IOHIDParameter.h>
#include "IOHIDEventTranslation.h"
#include <IOKit/IOMessage.h>
#include <sstream>
#include <iomanip>
#include <sys/time.h>


enum {
  kIOHIDGlobalModifersReportToService    = 0x1,
  kIOHIDGlobalModifersUse                = 0x2
};

// AC658E84-3DFE-43DE-945D-6E7919FBEDE1
#define kIOHIDNXEventTranslatorSessionFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0xAC, 0x65, 0x8E, 0x84, 0x3D, 0xFE, 0x43, 0xDE, 0x94, 0x5D, 0x6E, 0x79, 0x19, 0xFB, 0xED, 0xE1)

extern "C" void * IOHIDNXEventTranslatorSessionFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);

static boolean_t IsCompanionService(IOHIDServiceRef s1, IOHIDServiceRef s2);
static NXEventHandle openHIDSystem(void);


enum {
  kPMDisplayOff        = 0,
  kPMDisplayDim        = 1,
  kPMDisplayOn         = 2
};

static const int64_t one_mil = 1000*1000;

#define to_ns(ticks) ((ticks * tb_info.numer) / (tb_info.denom))
#define to_ms(ticks) (to_ns(ticks)/one_mil)
#define ns_to_absolute_time(x)  x / (tb_info.numer / tb_info.denom);

static mach_timebase_info_data_t tb_info;

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilterFactory
//------------------------------------------------------------------------------
// Implementation of the factory function for this type.
void *IOHIDNXEventTranslatorSessionFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDNXEventTranslatorSessionFilter), 0);
        return new(p) IOHIDNXEventTranslatorSessionFilter(kIOHIDNXEventTranslatorSessionFilterFactory);
    }
    // If the requested type is incorrect, return NULL.
    return NULL;
}

// The IOHIDEventSystemStatistics function table.
IOHIDSessionFilterPlugInInterface IOHIDNXEventTranslatorSessionFilter::sIOHIDNXEventTranslatorSessionFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDNXEventTranslatorSessionFilter::QueryInterface,
    IOHIDNXEventTranslatorSessionFilter::AddRef,
    IOHIDNXEventTranslatorSessionFilter::Release,
    // IOHIDSimpleSessionFilterPlugInInterface functions
    IOHIDNXEventTranslatorSessionFilter::filter,
    NULL,
    NULL,
    // IOHIDSessionFilterPlugInInterface functions
    IOHIDNXEventTranslatorSessionFilter::open,
    IOHIDNXEventTranslatorSessionFilter::close,
    NULL,
    NULL,
    IOHIDNXEventTranslatorSessionFilter::registerService,
    IOHIDNXEventTranslatorSessionFilter::unregisterService,
    IOHIDNXEventTranslatorSessionFilter::scheduleWithDispatchQueue,
    IOHIDNXEventTranslatorSessionFilter::unscheduleFromDispatchQueue,
    IOHIDNXEventTranslatorSessionFilter::getPropertyForClient,
    IOHIDNXEventTranslatorSessionFilter::setPropertyForClient,
};

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::IOHIDNXEventTranslatorSessionFilter
//------------------------------------------------------------------------------
IOHIDNXEventTranslatorSessionFilter::IOHIDNXEventTranslatorSessionFilter(CFUUIDRef factoryID):
_sessionInterface(&sIOHIDNXEventTranslatorSessionFilterFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_globalModifiers (0),
_hidSystem(MACH_PORT_NULL),
_translator (IOHIDPointerEventTranslatorCreate (kCFAllocatorDefault, 0)),
_powerConnect (MACH_PORT_NULL),
_powerNotifier(MACH_PORT_NULL),
_powerPort (NULL),
_powerState (0),
_powerOnThresholdEventCount (0),
_powerOnThreshold (kIOHIDPowerOnThresholdMS),
_displayState (kPMDisplayOn),
_displaySleepAbortThreshold (kIOHIDDisplaySleepAbortThresholdMS),
_displayWakeAbortThreshold (kIOHIDDisplayWakeAbortThresholdMS),
_previousEventTime (0),
_declareActivityThreshold (0),
_powerStateChangeTime (0),
_displayStateChangeTime (0),
_displayLog(NULL),
_nxEventLog(NULL),
_isTranslationEnabled(true)
{
    CFPlugInAddInstanceForFactory( factoryID );
    
    if (tb_info.denom == 0) {
        mach_timebase_info(&tb_info);
    }
    _declareActivityThreshold = ns_to_absolute_time(kIOHIDDeclareActivityThresholdMS * NSEC_PER_MSEC);
    _updateActivityQueue = dispatch_queue_create("com.apple.HID.updateActivity", NULL);
}
//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::IOHIDNXEventTranslatorSessionFilter
//------------------------------------------------------------------------------
IOHIDNXEventTranslatorSessionFilter::~IOHIDNXEventTranslatorSessionFilter()
{
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}
//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDNXEventTranslatorSessionFilter::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->QueryInterface(iid, ppv);
}
// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDNXEventTranslatorSessionFilter::QueryInterface( REFIID iid, LPVOID *ppv )
{
    // Create a CoreFoundation UUIDRef for the requested interface.
    CFUUIDRef interfaceID = CFUUIDCreateFromUUIDBytes( NULL, iid );
    // Test the requested ID against the valid interfaces.
    if (CFEqual(interfaceID, kIOHIDSimpleSessionFilterPlugInInterfaceID) || CFEqual(interfaceID, kIOHIDSessionFilterPlugInInterfaceID)) {
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
// IOHIDNXEventTranslatorSessionFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDNXEventTranslatorSessionFilter::AddRef( void *self )
{
    return static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->AddRef();
}
ULONG IOHIDNXEventTranslatorSessionFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}
//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDNXEventTranslatorSessionFilter::Release( void *self )
{
    return static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->Release();
}
ULONG IOHIDNXEventTranslatorSessionFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}
//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::open
//------------------------------------------------------------------------------
boolean_t IOHIDNXEventTranslatorSessionFilter::open(void * self, IOHIDSessionRef session, IOOptionBits options)
{
    return static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->open(session, options);
}

boolean_t IOHIDNXEventTranslatorSessionFilter::open(IOHIDSessionRef session __unused, IOOptionBits options __unused)
{
    _powerConnect = IORegisterForSystemPower (this, &_powerPort, powerNotificationCallback, &_powerNotifier);
    
    return _keyboards && _companions  && _modifiers;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::close
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::close(void * self, IOHIDSessionRef session, IOOptionBits options)
{
    static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->close(session, options);
}

void IOHIDNXEventTranslatorSessionFilter::close(IOHIDSessionRef session __unused, IOOptionBits options __unused)
{
    if (_powerConnect) {
        IOServiceClose(_powerConnect);
    }
    
    if (_hidSystem != MACH_PORT_NULL) {
        IOServiceClose(_hidSystem);
        _hidSystem = MACH_PORT_NULL;
    }
    
    if (_displayLog) {
        CFRelease(_displayLog);
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::registerService
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::registerService(void * self, IOHIDServiceRef service)
{
    static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->registerService(service);
}
void IOHIDNXEventTranslatorSessionFilter::registerService(IOHIDServiceRef service)
{
    //
    // Keep track of all keyboard like services
    //
    if (_keyboards &&
       ( IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
        IOHIDServiceConformsTo(service, kHIDPage_Consumer, kHIDUsage_Csmr_ConsumerControl))) {
        _keyboards.SetValue(service);
    }
    
    if (IOHIDServiceConformsTo(service, kHIDPage_AppleVendor, kHIDUsage_AppleVendor_DFR)) {
        _dfr = service;
    }
    
    if (_modifiers) {
        CFNumberRefWrap modifiersMask (IOHIDServiceCopyProperty (service, CFSTR(kHIDEventTranslationModifierFlags)), true);
        if (modifiersMask) {
            _modifiers.SetValueForKey ((CFTypeRef)service, modifiersMask.Reference());
            if (_companions) {
                IOHIDServiceRef companion = getCompanionService (service);
                if (companion) {
                    _companions.SetValueForKey ((CFTypeRef)service, (CFTypeRef)companion);
                    _companions.SetValueForKey ((CFTypeRef)companion, (CFTypeRef)service);
                }
            }
            updateModifiers();
        }
    }
  
    CFNumberRefWrap value (IOHIDServiceCopyProperty(service, CFSTR(kIOHIDServiceGlobalModifiersUsageKey)), true);
    if (value) {
      HIDLogError("kIOHIDServiceGlobalModifiersUsageKey = %d", ((int)value));
      if (((SInt32)value) & kIOHIDGlobalModifersReportToService) {
        _reportModifiers.SetValue(service);
      }
      if (((SInt32)value) & kIOHIDGlobalModifersUse) {
        _updateModifiers.SetValue(service);
      }
    }
 
    if (_translator &&
       (IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) ||
        IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Pointer) ||
        IOHIDServiceConformsTo(service, kHIDPage_AppleVendor,    kHIDUsage_GD_Pointer) ||
        IOHIDServiceConformsTo(service, kHIDPage_AppleVendor,    kHIDUsage_AppleVendor_NXEvent))
        ){
        IOHIDPointerEventTranslatorRegisterService (_translator, service);
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::unregisterService
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::unregisterService(void * self, IOHIDServiceRef service)
{
  static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->unregisterService(service);
}
void IOHIDNXEventTranslatorSessionFilter::unregisterService(IOHIDServiceRef service)
{
    if (_keyboards) {
        _keyboards.RemoveValue(service);
    }
    
    if (service == _dfr) {
        _dfr = NULL;
    }
  
    if (_updateModifiers) {
        _updateModifiers.RemoveValue(service);
    }

    if (_reportModifiers) {
        _reportModifiers.RemoveValue(service);
    }
  
    if (_companions) {
        CFRefWrap<CFTypeRef> companion (_companions[service]);
        if (companion) {
            _companions.Remove(service);
            _companions.Remove(companion);
        }
    }
  
    if (_modifiers && _modifiers.ContainKey(service)) {
        _modifiers.Remove(service);
        updateModifiers();
    }

    if (_assertionNames && _assertionNames.ContainKey(service)) {
        _assertionNames.Remove(service);
    }
  
    if (_translator) {
       IOHIDPointerEventTranslatorUnRegisterService (_translator, service);
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->scheduleWithDispatchQueue(queue);
}
void IOHIDNXEventTranslatorSessionFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _dispatch_queue = queue;
    __block IOHIDNXEventTranslatorSessionFilter *blockSelf = this;
    CGError err;
    
    if (_powerPort) {
        IONotificationPortSetDispatchQueue (_powerPort, _dispatch_queue);
    }

    SLSDisplayManagerNotificationBlockType displayNotification = ^(const SLSDisplayPowerStateNotificationType state,
                                                                   void * _Nullable refcon)
    {
        blockSelf->displayNotificationCallback(state);
    };

    err = SLSDisplayManagerRegisterPowerStateNotificationOptions(_dispatch_queue,
                                                                 NULL,
                                                                 kPowerStateNotificationOptionDidWake |
                                                                 kPowerStateNotificationOptionWillSleep |
                                                                 kPowerStateNotificationOptionAsync,
                                                                 displayNotification);
    if (err != kCGErrorSuccess) {
        HIDLogError ("Unable to register display sleep/wake notifications. "
                     "HID events may not wake the system as expected. Error code: %d", err);
    }
    // Dim is not supported on all platforms, but the state isn't essential.
    err = SLSDisplayManagerRegisterPowerStateNotification(_dispatch_queue,
                                                          NULL,
                                                          kPowerStateNotificationTypeWillDim | kPowerStateNotificationTypeAsync,
                                                          displayNotification);
    if (err != kCGErrorSuccess) {
        HIDLogError ("Unable to register display dim notification: %d", err);
    }

}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->unscheduleFromDispatchQueue(queue);
}
void IOHIDNXEventTranslatorSessionFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    
    if (_powerNotifier) {
        IODeregisterForSystemPower (&_powerNotifier);
        _powerNotifier = MACH_PORT_NULL;
    }
    
    if ( _powerPort ) {
        IONotificationPortDestroy(_powerPort);
        _powerPort = NULL;
    }

    if ( _powerConnect ) {
        IOServiceClose(_powerConnect);
        _powerConnect = MACH_PORT_NULL;
    }

    CGError err;

    err = SLSDisplayManagerUnregisterPowerStateNotificationOptions(kPowerStateNotificationTypeDidWake | kPowerStateNotificationTypeDidSleep | kPowerStateNotificationTypeAsync);
    if (err) {
        HIDLogError ("Unable to unregister display sleep/wake notification: %d", err);
    }
    err = SLSDisplayManagerUnregisterPowerStateNotification(kPowerStateNotificationTypeDidDim | kPowerStateNotificationTypeAsync);
    if (err) {
        HIDLogError ("Unable to unregister display dim notification: %d", err);
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDNXEventTranslatorSessionFilter::filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event)
{
    return static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->filter(sender, event);
}
IOHIDEventRef IOHIDNXEventTranslatorSessionFilter::filter(IOHIDServiceRef sender, IOHIDEventRef event)
{
    if (!event) {
        return event;
    }
    
    if (!displayStateFilter (sender, event)) {
        CFRelease(event);
        return NULL;
    }
    
    if (!powerStateFilter (event)) {
        CFRelease(event);
        return NULL;
    }
    
    if (!sender) {
        return event;
    }
    

    bool doUpdateModifiers = false;

    // handles modifier updates, for syntetic CapsLock onoff with setting kIOHIDServiceCapsLockStateKey
    if (IOHIDEventConformsTo(event, kIOHIDEventTypeVendorDefined)) {
        IOHIDEventRef ev = IOHIDEventGetEvent(event, kIOHIDEventTypeVendorDefined);
        if (IOHIDEventGetIntegerValue(ev, kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendor_NXEvent_Translated) {
            NXEventExt  *nxEvent = NULL;
            CFIndex     nxEventLength = 0;
            IOHIDEventGetVendorDefinedData(ev, (uint8_t**)&nxEvent, &nxEventLength);
            if (nxEvent && nxEventLength >= sizeof (NXEventExt) && nxEvent->payload.type == NX_FLAGSCHANGED) {
                doUpdateModifiers = true;
            }
        }
    }

    if (IOHIDEventConformsTo(event, kIOHIDEventTypeKeyboard) && _modifiers.ContainKey(sender)) {
        CFArrayRefWrap childrens (IOHIDEventGetChildren(event));
        if (childrens.Reference()) {
            for (CFIndex index = 0; index < childrens.Count(); index++) {
                IOHIDEventRef childEvent = (IOHIDEventRef)childrens[index];
                if (childEvent && IOHIDEventGetType(childEvent) == kIOHIDEventTypeVendorDefined &&
                    IOHIDEventGetIntegerValue(childEvent, kIOHIDEventFieldVendorDefinedUsagePage) == kHIDPage_AppleVendor &&
                    IOHIDEventGetIntegerValue(childEvent, kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendor_NXEvent_Translated) {
                    
                    if (_companions) {
                        IOHIDServiceRef companion = (IOHIDServiceRef)_companions[sender];
                        if (companion) {
                            CFNumberRefWrap companionServiceModifiers ((CFNumberRef)_modifiers[companion]);
                            if (companionServiceModifiers) {
                                IOHIDKeyboardEventTranslatorUpdateWithCompanionModifiers(childEvent,  (uint32_t) companionServiceModifiers);
                            }
                        }
                    }
                    
                    if (_updateModifiers.ContainValue(sender)) {
                        IOHIDKeyboardEventTranslatorUpdateWithCompanionModifiers (childEvent, _globalModifiers);
                    }
                }
            }
        }
        doUpdateModifiers = true;
    }
    
    if (doUpdateModifiers) {
        CFNumberRefWrap currentServiceModifiers ((CFNumberRef)IOHIDServiceCopyProperty (sender, CFSTR(kHIDEventTranslationModifierFlags)), true);
        CFNumberRefWrap cachedServiceModifiers ((CFNumberRef)_modifiers[sender]);
        if (currentServiceModifiers && cachedServiceModifiers && currentServiceModifiers != cachedServiceModifiers) {
            _modifiers.SetValueForKey(sender, currentServiceModifiers.Reference());
            updateModifiers();
        }
    }
    
    if (_translator && _isTranslationEnabled &&
        (IOHIDEventGetType(event) == kIOHIDEventTypePointer ||
         IOHIDEventGetType(event) == kIOHIDEventTypeScroll  ||
         (IOHIDEventGetType(event) == kIOHIDEventTypeVendorDefined &&
          IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendor_NXEvent
          ))) {
             
        CFArrayRef collection = IOHIDPointerEventTranslatorCreateEventCollection (_translator, event, sender, _globalModifiers, 0);
        if (collection) {
            for (CFIndex index = 0; index < CFArrayGetCount(collection); index++) {
                IOHIDEventRef translatedEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(collection, index);
                if (translatedEvent) {
                    IOHIDEventAppendEvent(event, translatedEvent, 0);
                }
            }
            CFRelease(collection);
        }
    }
    
    if (resetStickyKeys(event)) {
        CFRetain(sender);
        dispatch_async(_dispatch_queue, ^() {
            [[NSNotificationCenter defaultCenter] postNotificationName:@(kIOHIDResetStickyKeyNotification)
                                                                object:(__bridge id)sender];
            CFRelease(sender);
        });
    }
    
    return event;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::resetStickyKeys
//------------------------------------------------------------------------------
boolean_t IOHIDNXEventTranslatorSessionFilter::resetStickyKeys(IOHIDEventRef event) {
    // Ignore digitizer events; the associated pointer event
    // will have the correct button state.
    if (IOHIDEventConformsTo(event, kIOHIDEventTypeDigitizer)) {
        return false;
    }
    
    if (IOHIDEventConformsTo(event, kIOHIDEventTypeButton)) {
        if (_translator) {
            // Reset when all buttons are up
            if (!IOHIDPointerEventTranslatorGetGlobalButtonState(_translator)) {
                return true;
            }
        }
    }
    
    if (IOHIDEventConformsTo(event, kIOHIDEventTypeVendorDefined)) {
        IOHIDEventRef ev = IOHIDEventGetEvent(event, kIOHIDEventTypeVendorDefined);
        if (IOHIDEventGetIntegerValue(ev, kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendor_NXEvent) {
            NXEventExt  *nxEvent = NULL;
            CFIndex     eventLength = 0;
            IOHIDEventGetVendorDefinedData(ev, (uint8_t**)&nxEvent, &eventLength);
            if (nxEvent) {
                if (nxEvent->payload.type == NX_LMOUSEDOWN ||
                    nxEvent->payload.type == NX_RMOUSEDOWN ||
                    nxEvent->payload.type == NX_OMOUSEDOWN) {
                    return true;
                }
            }
        }
    }
    
    if (IOHIDEventConformsTo(event, kIOHIDEventTypeKeyboard)) {
        IOHIDEventRef ev = IOHIDEventGetEvent(event, kIOHIDEventTypeKeyboard);
        if (IOHIDEventGetIntegerValue(ev, kIOHIDEventFieldKeyboardDown)) {
            return true;
        }
    }
    
    return false;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::getPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDNXEventTranslatorSessionFilter::getPropertyForClient (void * self, CFStringRef key, CFTypeRef client) {
   return static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->getPropertyForClient(key,client);
}

CFTypeRef IOHIDNXEventTranslatorSessionFilter::getPropertyForClient (CFStringRef key, CFTypeRef client __unused) {
    CFTypeRef result = NULL;

    if (CFEqual(key, CFSTR(kIOHIDSessionFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        serialize(serializer);

        if (serializer) {
          result = CFRetain(serializer.Reference());
        }
    } else if (CFEqual(key, CFSTR(kIOHIDNXEventTranslation))) {
        result = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt8Type, &_isTranslationEnabled);
    }
  
    return result;
  
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::setPropertyForClient (void * self, CFStringRef key, CFTypeRef property, CFTypeRef client) {
   static_cast<IOHIDNXEventTranslatorSessionFilter *>(self)->setPropertyForClient(key, property, client);
}
void IOHIDNXEventTranslatorSessionFilter::setPropertyForClient (CFStringRef key, CFTypeRef property, CFTypeRef client __unused) {

  if (CFEqual(key, CFSTR(kIOHIDActivityStateKey))) {
      dispatch_sync(_updateActivityQueue, ^{
          updateActivity(CFBooleanGetValue((CFBooleanRef)key));
      });
  } else if (CFEqual(key, CFSTR(kIOHIDDeclareActivityThreshold))) {
      if (property && CFGetTypeID(property) == CFNumberGetTypeID()) {
          uint32_t val;
          CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &val);
          _declareActivityThreshold = ns_to_absolute_time(val * NSEC_PER_MSEC);
      }
  } else if (CFEqual(key,CFSTR(kIOHIDNXEventTranslation))) {
      
      if (CFNumberGetTypeID() == CFGetTypeID(property)) {
          uint8_t tmp = 0;
          CFNumberGetValue((CFNumberRef)property, kCFNumberCharType, &tmp);
          _isTranslationEnabled = tmp ? true : false;
      }
  }
  if (_translator) {
      IOHIDPointerEventTranslatorSetProperty (_translator, key, property);
  }
    
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::updateModifiers
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::updateModifiers() {
     uint32_t globalModifiers = 0;
    _modifiers.Apply ([&globalModifiers](const void *key __unused, const void *value) {
        if (value ) {
           globalModifiers |= (uint32_t) CFNumberRefWrap((CFTypeRef)value);
        }
    });
    if (_globalModifiers != globalModifiers) {
        HIDLogDebug("Update modifier flags: 0x%x", globalModifiers);
        _globalModifiers = globalModifiers;
        
        NSSet *modifiers = [NSSet setWithSet:(__bridge NSSet *)_reportModifiers.Reference()];
        
        dispatch_async(dispatch_get_main_queue(), ^{
            [modifiers enumerateObjectsUsingBlock:^(id obj,
                                                    BOOL *stop __unused) {
                IOHIDServiceRef service = (__bridge IOHIDServiceRef)obj;
                
                IOHIDServiceSetProperty(service,
                                        CFSTR(kIOHIDKeyboardGlobalModifiersKey),
                                        CFNumberRefWrap(globalModifiers));
            }];
        });
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::getCompanionService
//------------------------------------------------------------------------------
IOHIDServiceRef IOHIDNXEventTranslatorSessionFilter::getCompanionService(IOHIDServiceRef service) {
  IOHIDServiceRef companion = NULL;
  CFIndex count = _modifiers.Count();
  if (count == 0 || service == NULL) {
    return companion;
  }
  IOHIDServiceRef *list = (IOHIDServiceRef*)malloc(count * sizeof (IOHIDServiceRef));
  if (list != NULL) {
    CFDictionaryGetKeysAndValues(_modifiers.Reference(), (const void **)list, NULL);
    for (CFIndex index = 0; index < count; ++index) {
      
      if (IsCompanionService(service, list[index])) {
        companion = list[index];
        break;
      }
    }
    free(list);
  }
                                               return companion;
}

void IOHIDNXEventTranslatorSessionFilter::displayNotificationCallback (SLSDisplayPowerStateNotificationType state) {

    HIDLogDebug ("displayNotificationCallback : displayState: 0x%x", state);

    uint32_t currentDisplayState;

    switch (state) {

    case  kPowerStateNotificationTypeWillWake:
    case  kPowerStateNotificationTypeDidWake:
        currentDisplayState = kPMDisplayOn;
        break;

    case  kPowerStateNotificationTypeWillDim:
    case  kPowerStateNotificationTypeDidDim:
        currentDisplayState = kPMDisplayDim;
        break;

    case kPowerStateNotificationTypeWillSleep:
    case kPowerStateNotificationTypeDidSleep:
        currentDisplayState = kPMDisplayOff;
        break;

    default:
        HIDLogError ("Got unrecognized display state: %#x", state);
        return;
    }
  
    if (_displayState != currentDisplayState) {
      _displayStateChangeTime = mach_continuous_time();
      HIDLogInfo ("displayNotificationCallback : displayState change 0x%x -> 0x%x", _displayState, currentDisplayState);
      _displayState = currentDisplayState;
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::_AssertionID
//------------------------------------------------------------------------------

IOPMAssertionID IOHIDNXEventTranslatorSessionFilter::_AssertionID = 0;

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::displayStateFilter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDNXEventTranslatorSessionFilter::displayStateFilter (IOHIDServiceRef sender, IOHIDEventRef  event)
{
    IOHIDEventPolicyValue   policy              = IOHIDEventGetPolicy(event, kIOHIDEventPowerPolicy);
    IOHIDEventSenderID      senderID            = IOHIDEventGetSenderID(event);
    IOHIDEventType          eventType           = IOHIDEventGetType(event);
    uint64_t                eventTime           = IOHIDEventGetTimeStamp(event);
    IOHIDEventRef           result              = event;
    uint64_t                deltaMS             = to_ms(mach_continuous_time()) - to_ms(_displayStateChangeTime);
    uint64_t                eventDelta          = eventTime - _previousEventTime;
    bool                    declareActivity     = true;
    bool                    nxEvent             = false;

    // Cancel keyboard/button events when display is asleep or waking up
    // Will still declare using activity.
    if ((_displayState < kPMDisplayDim || (_displayState > kPMDisplayDim && deltaMS < _displayWakeAbortThreshold)) &&
        shouldCancelEvent(event)) {
       result = NULL;
    }

    if (eventType == kIOHIDEventTypeVendorDefined) {
        IOHIDEventRef vendorEvent = IOHIDEventGetEvent(event, kIOHIDEventTypeVendorDefined);
        if (vendorEvent) {
            uint32_t usage = (uint32_t)IOHIDEventGetIntegerValue(vendorEvent, kIOHIDEventFieldVendorDefinedUsage);
            uint32_t page = (uint32_t)IOHIDEventGetIntegerValue(vendorEvent, kIOHIDEventFieldVendorDefinedUsagePage);
            if (page == kHIDPage_AppleVendor && usage == kHIDUsage_AppleVendor_NXEvent) {
                nxEvent = true;
            }
        }
    }
    
    if (policy == kIOHIDEventNoPolicy) {
        
        // Only wake/maintain policies should declare activity
        declareActivity = false;
        
    } else if (policy == kIOHIDEventPowerPolicyMaintainSystem &&
               nxEvent) {
        // Limit the number of event that declare activity.
        if (eventDelta < _declareActivityThreshold) {
            declareActivity = false;
        }
        // Don't post NXEvents to keep display active when display is off.
        if (_displayState < kPMDisplayDim) {
            declareActivity = false;
        }

    } else if (policy == kIOHIDEventPowerPolicyMaintainSystem &&
        _displayState < kPMDisplayDim &&
        deltaMS > _displaySleepAbortThreshold &&
        sender != _dfr) {
        
        // Prevent digitizer/pointer events from tickling display after 5s, unless sender is DFR.
        declareActivity = false;
        
    } else if (policy == kIOHIDEventPowerPolicyMaintainSystem &&
        eventDelta < _declareActivityThreshold) {
        // Only declare user activity every 250ms for digitizer/pointer events
        declareActivity = false;
    }

    if (declareActivity) {
        _previousEventTime = eventTime;
    }
    
    HIDLogActivityDebug ("displayStateFilter policy:%llu declareActivity:%d result:%p event:%d sender:0x%llx eventDelta:0x%llx deltaMS:0x%llx displayState:%u prevTimeStamp:0x%llx eventTime:0x%llx", policy, declareActivity, result, IOHIDEventGetType(event), senderID, eventDelta, deltaMS, _displayState, _previousEventTime, eventTime);
    
    if (declareActivity) {
        static const size_t kStrSize = 128;
        CFStringRef         tmpStr = NULL;
        CFStringRef         assertionNameStr = NULL;

        // Log display wakes
        if (_displayState < kPMDisplayDim) {
            if (nxEvent) {
                updateNXEventLog(policy, event, eventTime);
            } else {
                updateDisplayLog(senderID, policy, eventType, eventTime);
            }
        }

        if (sender) {
            tmpStr = (CFStringRef)_assertionNames[sender];
        }

        if (tmpStr) {
            CFRetain(tmpStr);
        }
        else {
            // Cache assertion name
            char        assertionName[kStrSize] = { 0 };
            CFStringRef nameCFStr = CFSTR("NULL");
            char        nameStr[kStrSize] = { 0 };
            CFStringRef productCFStr = CFSTR("NULL");
            char        productStr[kStrSize] = { 0 };

            if (sender) {
                CFTypeRef prop = IOHIDServiceGetProperty(sender, CFSTR(kIOClassKey));
                if (prop && CFGetTypeID(prop) == CFStringGetTypeID()) {
                    nameCFStr = (CFStringRef)prop;
                }

                prop = IOHIDServiceGetProperty(sender, CFSTR(kIOHIDProductKey));
                if (prop && CFGetTypeID(prop) == CFStringGetTypeID()) {
                    productCFStr = (CFStringRef)prop;
                }
            }
            CFStringGetCString(nameCFStr, nameStr, kStrSize, kCFStringEncodingUTF8);
            CFStringGetCString(productCFStr, productStr, kStrSize, kCFStringEncodingUTF8);

            // Assertion name can be up to 128 characters. Limit string sizes to fit this constraint.
            nameStr[19]    = '\0';
            productStr[19] = '\0';

            // Assertion name format:
            // com.apple.iohideventsystem.queue.tickle:serviceID:XXX name:XXX product:XXX eventType:XXX
            // Only eventType is variable
            int ret = snprintf(assertionName, kStrSize, "%s.queue.tickle serviceID:%llx name:%s product:%s eventType:",
                               kIOHIDEventSystemServerName,
                               (long long unsigned int)senderID,
                               nameStr,
                               productStr);
            if (ret < 0 || ret >= kStrSize) {
                HIDLogError ("Error generating assertion name (snprintf %d)", ret);
            }

            tmpStr = CFStringCreateWithCString(NULL, assertionName, kCFStringEncodingUTF8);
            if (sender && tmpStr) {
                _assertionNames.SetValueForKey(sender, tmpStr);
            } else {
                HIDLogError ("Error generating assertion string");
            }
        }

        if (tmpStr) {
            assertionNameStr = (CFStringRef)CFStringCreateMutableCopy(NULL, kStrSize, tmpStr);
            CFStringAppendFormat((CFMutableStringRef)assertionNameStr, NULL, CFSTR("%u"), (unsigned int)eventType);
            CFRelease(tmpStr);
        } else {
            HIDLogError ("No assertion name string for service:%llx", senderID);
            assertionNameStr = CFSTR(kIOHIDEventSystemServerName ".queue.tickle");
        }

        if (_updateActivityQueue) {
            CFRetain(assertionNameStr);
            dispatch_async(_updateActivityQueue, ^() {
                IOReturn status = IOPMAssertionDeclareUserActivity(assertionNameStr,
                                                                   kIOPMUserActiveLocal,
                                                                   &_AssertionID);
                if (status) {
                    HIDLogError ("IOPMAssertionDeclareUserActivity status:0x%x", status);
                }

                updateActivity(true);

                CFRelease(assertionNameStr);
            });
        }
        if (assertionNameStr) {
            CFRelease(assertionNameStr);
        }
    }
    return result;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::testCancelEvent
//------------------------------------------------------------------------------
boolean_t IOHIDNXEventTranslatorSessionFilter::shouldCancelEvent (IOHIDEventRef  event) {
    IOHIDEventRef testEvent;
    testEvent = IOHIDEventGetEvent (event, kIOHIDEventTypeKeyboard);
    if (testEvent && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)) {
      return true;
    }
    testEvent = IOHIDEventGetEvent (event, kIOHIDEventTypePointer);
    if (testEvent && IOHIDEventGetIntegerValue (event, kIOHIDEventFieldPointerButtonMask)) {
      return true;
    }
    testEvent = IOHIDEventGetEvent (event, kIOHIDEventTypeButton);
    if (testEvent && IOHIDEventGetIntegerValue (event, kIOHIDEventFieldButtonState)) {
      return true;
    }
    testEvent = IOHIDEventGetEvent(event, kIOHIDEventTypeVendorDefined);
    if (testEvent) {
        uint32_t usagePage = (uint32_t)IOHIDEventGetIntegerValue(testEvent, kIOHIDEventFieldVendorDefinedUsagePage);
        uint32_t usage = (uint32_t)IOHIDEventGetIntegerValue(testEvent, kIOHIDEventFieldVendorDefinedUsage);
        if (usagePage == kHIDPage_AppleVendor && usage == kHIDUsage_AppleVendor_NXEvent) {
            return true;
        }
    }
    return false;
}


//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::updateDisplayLog
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::updateDisplayLog (IOHIDEventSenderID serviceID, IOHIDEventPolicyValue policy, IOHIDEventType eventType, uint64_t timestamp) {
    LogEntry entry;
    
    gettimeofday(&entry.time, NULL);
    entry.serviceID     = serviceID;
    entry.policy        = policy;
    entry.eventType     = eventType;
    entry.timestamp     = timestamp;
    
    if (_displayLog == NULL) {
        _displayLog = _IOHIDSimpleQueueCreate(kCFAllocatorDefault, sizeof(LogEntry), LOG_MAX_ENTRIES);
    }
    if (_displayLog) {
        _IOHIDSimpleQueueEnqueue(_displayLog, &entry, true);
    }
}

void IOHIDNXEventTranslatorSessionFilter::updateNXEventLog (IOHIDEventPolicyValue policy, IOHIDEventRef event, uint64_t timestamp) {
    LogNXEventEntry entry;
    NXEventExt * nxEvent = NULL;
    CFIndex eventSize = 0;
    IOHIDEventRef vendorEvent;

    vendorEvent = IOHIDEventGetEvent(event, kIOHIDEventTypeVendorDefined);
    IOHIDEventGetVendorDefinedData(vendorEvent, (uint8_t**)&nxEvent, &eventSize);

    gettimeofday(&entry.time, NULL);
    entry.policy = policy;
    entry.senderPID = nxEvent ? nxEvent->payload.ext_pid : 0;
    entry.nxEventType = nxEvent ? nxEvent->payload.type : 0;
    entry.timestamp = timestamp;

    if (_nxEventLog == NULL) {
        _nxEventLog = _IOHIDSimpleQueueCreate(kCFAllocatorDefault, sizeof(LogNXEventEntry), LOG_MAX_ENTRIES);
    }
    if (_nxEventLog) {
        _IOHIDSimpleQueueEnqueue(_nxEventLog, &entry, true);
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::updateActivity
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::updateActivity (bool active) {
    if (_hidSystem == MACH_PORT_NULL) {
        _hidSystem = openHIDSystem();
    }
    
    kern_return_t status = IOHIDSetStateForSelector(_hidSystem, kIOHIDActivityUserIdle, active ? 0 : 1);
    if (status) {
         HIDLogError ("IOHIDSetStateForSelector(kIOHIDActivityUserIdle,%d):0x%x", status, active ? 0 : 1);
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::powerNotificationCallback
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::powerNotificationCallback (void * refcon, io_service_t service, uint32_t messageType, void * messageArgument) {
     static_cast<IOHIDNXEventTranslatorSessionFilter *>(refcon)->powerNotificationCallback(service, messageType, messageArgument);
}

void IOHIDNXEventTranslatorSessionFilter::powerNotificationCallback (io_service_t service __unused, uint32_t messageType, void * messageArgument) {
    switch (messageType) {
    case    kIOMessageCanSystemSleep:
    case    kIOMessageSystemWillSleep:
        IOAllowPowerChange (_powerConnect, (long)messageArgument);
    default:
        _powerStateChangeTime = mach_continuous_time();
        _powerState = messageType;
        break;
    }
    HIDLogDebug ("powerNotificationCallback message:0x%x\n", messageType);
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::powerStateFilter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDNXEventTranslatorSessionFilter::powerStateFilter (IOHIDEventRef  event) {
    uint64_t   deltaMS = to_ms(mach_continuous_time()) - to_ms(_powerStateChangeTime);
    
    if (_powerState == kIOMessageSystemWillPowerOn && deltaMS < _powerOnThreshold  && shouldCancelEvent (event)) {
        ++_powerOnThresholdEventCount;
        HIDLogDebug ("powerStateFilter : Cancel event (type:%d  power state:0x%x changed %lldms ago (threshold:%dms)\n",
                     IOHIDEventGetType(event),
                     _powerState,
                      deltaMS,
                     _powerOnThreshold
                     );
        return NULL;
    }
    _powerOnThresholdEventCount = 0;
    return event;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::serialize
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::serialize (CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDNXEventTranslatorSessionFilter"));
    serializer.SetValueForKey(CFSTR("CanceledEventCount"), CFNumberRefWrap(_powerOnThresholdEventCount));
    serializer.SetValueForKey(CFSTR("DisplayPowerStateChangeTime"), CFNumberRefWrap(to_ms(mach_continuous_time()) - to_ms(_displayStateChangeTime)));
    serializer.SetValueForKey(CFSTR("DisplayPowerState"), CFNumberRefWrap(_displayState));
    serializer.SetValueForKey(CFSTR("SystemPowerStateChangeTime"), CFNumberRefWrap(to_ms(mach_continuous_time()) - to_ms(_powerStateChangeTime)));
    serializer.SetValueForKey(CFSTR("SystemPowerState"), CFNumberRefWrap(_powerState));
    serializer.SetValueForKey(CFSTR("DeclareActivityThreshold"), CFNumberRefWrap(_declareActivityThreshold));
    serializer.SetValueForKey(CFSTR(kIOHIDNXEventTranslation), _isTranslationEnabled);

    CFMutableArrayRefWrap companions;
    _companions.Apply ([&companions](const void *key, const void *value) {
        CFMutableDictionaryRefWrap  pair;
        pair.SetValueForKey(CFSTR("ServiceId0"), IOHIDServiceGetRegistryID((IOHIDServiceRef)key));
        pair.SetValueForKey(CFSTR("ServiceId1"), IOHIDServiceGetRegistryID((IOHIDServiceRef)value));
        companions.Append(pair);
    });
    serializer.SetValueForKey(CFSTR("CompanionMap"), companions);

    CFMutableArrayRefWrap reportModifiers;
    _reportModifiers.Apply([&reportModifiers](const void * value) {
        reportModifiers.Append(IOHIDServiceGetRegistryID((IOHIDServiceRef)value));
    });
    serializer.SetValueForKey(CFSTR("ReportGlobalModifiers"), reportModifiers);
  
    CFMutableArrayRefWrap updateModifiers;
    _updateModifiers.Apply([&updateModifiers](const void * value) {
        updateModifiers.Append(IOHIDServiceGetRegistryID((IOHIDServiceRef)value));
    });
    serializer.SetValueForKey(CFSTR("TranslateWithGlobalModifiers"), updateModifiers);
    
    if (_displayLog) {
        CFMutableArrayRef displayLog = CFArrayCreateMutable(kCFAllocatorDefault, LOG_MAX_ENTRIES, &kCFTypeArrayCallBacks);
        if (displayLog) {
            _IOHIDSimpleQueueApplyBlock (_displayLog, ^(void * entry, void * ctx) {
                CFMutableArrayRef log = (CFMutableArrayRef)ctx;
                LogEntry * entryData = (LogEntry *)entry;
                
                CFMutableDictionaryRef entryDict = CFDictionaryCreateMutable(CFGetAllocator(log), 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                if (!entryDict) {
                    return;
                }
                
                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("ServiceID"), entryData->serviceID);
                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("Policy"), entryData->policy);
                _IOHIDDictionaryAddSInt32(entryDict, CFSTR("EventType"), entryData->eventType);
                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("timestamp"), entryData->timestamp);
                
                CFStringRef time = _IOHIDCreateTimeString(kCFAllocatorDefault, &entryData->time);
                if (time) {
                    CFDictionaryAddValue(entryDict, CFSTR("Time"), time);
                    CFRelease(time);
                }
                
                CFArrayAppendValue(log, entryDict);
                CFRelease(entryDict);
                
            }, (void *)displayLog);
            
            serializer.SetValueForKey(CFSTR("DisplayWakeLog"), displayLog);
            CFRelease(displayLog);
        }
    }
    if (_nxEventLog) {
        CFMutableArrayRef nxEventLog = CFArrayCreateMutable(kCFAllocatorDefault, LOG_MAX_ENTRIES, &kCFTypeArrayCallBacks);
        if (nxEventLog) {
            _IOHIDSimpleQueueApplyBlock (_nxEventLog, ^(void * entry, void * ctx) {
                CFMutableArrayRef log = (CFMutableArrayRef)ctx;
                LogNXEventEntry * entryData = (LogNXEventEntry *)entry;

                CFMutableDictionaryRef entryDict = CFDictionaryCreateMutable(CFGetAllocator(log), 4, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
                if (!entryDict) {
                    return;
                }

                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("SenderPID"), entryData->senderPID);
                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("NXEventType"), (int32_t)entryData->nxEventType);
                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("Policy"), entryData->policy);
                _IOHIDDictionaryAddSInt64(entryDict, CFSTR("timestamp"), entryData->timestamp);

                CFStringRef time = _IOHIDCreateTimeString(kCFAllocatorDefault, &entryData->time);
                if (time) {
                    CFDictionaryAddValue(entryDict, CFSTR("Time"), time);
                    CFRelease(time);
                }

                CFArrayAppendValue(log, entryDict);
                CFRelease(entryDict);

            }, (void *)nxEventLog);

            serializer.SetValueForKey(CFSTR("NXEventWakeLog"), nxEventLog);
            CFRelease(nxEventLog);
        }
    }
}

//------------------------------------------------------------------------------
// Helper funciton
//------------------------------------------------------------------------------

static const CFStringRef propertyTable [] = {
  CFSTR(kIOHIDVendorIDKey),
  CFSTR(kIOHIDProductIDKey),
  CFSTR(kIOHIDLocationIDKey),
  CFSTR(kIOHIDTransportKey)
};

static boolean_t IsAltSender(IOHIDServiceRef s1, IOHIDServiceRef s2) {

  CFTypeRef altSenderID = IOHIDServiceGetProperty (s1, CFSTR(kIOHIDAltSenderIdKey));
  if (altSenderID) {
     CFTypeRef registryID = IOHIDServiceGetRegistryID(s2);
     if (altSenderID && registryID && CFEqual(altSenderID, registryID)) {
        return true;
     }
  }
  return false;
}

static boolean_t IsCompanionService(IOHIDServiceRef s1, IOHIDServiceRef s2) {
  
  CFTypeRef s1Property;
  CFTypeRef s2Property;
  
  if (CFEqual(IOHIDServiceGetRegistryID(s1), IOHIDServiceGetRegistryID(s2))) {
    return false;
  }
  
  if (IsAltSender(s1,s2) || IsAltSender(s2,s1)) {
    return true;
  }
  
  for (size_t index = 0; index < sizeof(propertyTable)/sizeof(propertyTable[0]); ++index) {
    s1Property = IOHIDServiceGetProperty(s1,  propertyTable[index]);
    s2Property = IOHIDServiceGetProperty(s2,  propertyTable[index]);
    if (s1Property == NULL || s2Property == NULL || !CFEqual(s1Property, s2Property)) {
      return false;
    }
  }
  return true;
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

