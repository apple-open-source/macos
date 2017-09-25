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
_port (NULL),
_wranglerNotifier (MACH_PORT_NULL),
_displayState (kPMDisplayOn),
_displaySleepAbortThreshold (kIOHIDDisplaySleepAbortThresholdMS),
_displayWakeAbortThreshold (kIOHIDDisplayWakeAbortThresholdMS),
_previousEventTime (0),
_declareActivityThreshold (0),
_powerStateChangeTime (0),
_displayStateChangeTime (0),
_displayLog(NULL)
{
    CFPlugInAddInstanceForFactory( factoryID );
    
    if (tb_info.denom == 0) {
        mach_timebase_info(&tb_info);
    }
    _declareActivityThreshold = ns_to_absolute_time(kIOHIDDeclareActivityThresholdMS * NSEC_PER_MSEC);
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
    
    if (_powerPort) {
        IONotificationPortSetDispatchQueue (_powerPort, _dispatch_queue);
    }
  
 
    _port = IONotificationPortCreate(kIOMasterPortDefault);
  
    if (_port) {
      
        IONotificationPortSetDispatchQueue (_port, _dispatch_queue);

        kern_return_t status = IOServiceAddMatchingNotification(
                                  _port,
                                  kIOFirstPublishNotification,
                                  IOServiceMatching ("IODisplayWrangler"),
                                  displayMatchNotificationCallback,
                                  (void*) this,
                                  &_wranglerNotifier
                                  );
        if (status) {
            HIDLogError("IOServiceAddMatchingNotification with status 0x%x\n", status);
        }
        if (_wranglerNotifier) {
            displayMatchNotificationCallback (_wranglerNotifier);
        }
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
    
    if (_wranglerNotifier) {
        IOObjectRelease(_wranglerNotifier);
        _wranglerNotifier = MACH_PORT_NULL;
    }
    
    if ( _port ) {
        IONotificationPortDestroy(_port);
        _port = NULL;
    }
 
    if (_wrangler) {
        IOObjectRelease(_wrangler);
        _wrangler = MACH_PORT_NULL;
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
        
        CFNumberRefWrap currentServiceModifiers ((CFNumberRef)IOHIDServiceCopyProperty (sender, CFSTR(kHIDEventTranslationModifierFlags)), true);
        CFNumberRefWrap cachedServiceModifiers ((CFNumberRef)_modifiers[sender]);
        if (currentServiceModifiers && cachedServiceModifiers && currentServiceModifiers != cachedServiceModifiers) {
            _modifiers.SetValueForKey(sender, currentServiceModifiers.Reference());
            updateModifiers();
        }
    }
    
    if (_translator &&
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
        dispatch_async(_dispatch_queue, ^() {
            _keyboards.Apply ([sender](const void *value) {
                if (sender != (IOHIDServiceRef)value) {
                    IOHIDServiceSetProperty((IOHIDServiceRef)value, CFSTR(kIOHIDResetStickyKeyNotification), kCFBooleanTrue);
                }
            });
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
      updateActivity(CFBooleanGetValue((CFBooleanRef)key));
  } else if (CFEqual(key, CFSTR(kIOHIDDeclareActivityThreshold))) {
      if (property && CFGetTypeID(property) == CFNumberGetTypeID()) {
          uint32_t val;
          CFNumberGetValue((CFNumberRef)property, kCFNumberSInt32Type, &val);
          _declareActivityThreshold = ns_to_absolute_time(val * NSEC_PER_MSEC);
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
        _reportModifiers.Apply([this, &globalModifiers](const void * value) {
            IOHIDServiceSetProperty((IOHIDServiceRef)value, CFSTR(kIOHIDKeyboardGlobalModifiersKey), CFNumberRefWrap(globalModifiers));
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

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::displayMatchNotificationCallback
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::displayMatchNotificationCallback (void * refcon, io_iterator_t iterator) {
    static_cast<IOHIDNXEventTranslatorSessionFilter *>(refcon)->displayMatchNotificationCallback(iterator);
}

void IOHIDNXEventTranslatorSessionFilter::displayMatchNotificationCallback (io_iterator_t iterator) {
  
    _wrangler = IOIteratorNext (iterator);
 
    if (_wrangler == IO_OBJECT_NULL) {
        return;
    }
  
    IOObjectRelease (_wranglerNotifier);
    _wranglerNotifier = NULL;
  
    kern_return_t status = IOServiceAddInterestNotification (_port,
                                                             _wrangler,
                                                             kIOGeneralInterest,
                                                             displayNotificationCallback,
                                                             this,
                                                             &_wranglerNotifier
                                                             );

    if (status) {
        HIDLogError("IOServiceAddInterestNotification with status 0x%x\n", status);
    }

}


//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::displayNotificationCallback
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::displayNotificationCallback (void * refcon, io_service_t service, uint32_t messageType, void * messageArgument) {
    static_cast<IOHIDNXEventTranslatorSessionFilter *>(refcon)->displayNotificationCallback(service, messageType, messageArgument);
}

void IOHIDNXEventTranslatorSessionFilter::displayNotificationCallback (io_service_t service __unused, uint32_t messageType, void * messageArgument) {

    IOPowerStateChangeNotification *params = (IOPowerStateChangeNotification*)messageArgument;

    HIDLogDebug ("displayNotificationCallback : message: 0x%x powerState: 0x%lx", messageType, params->stateNumber);
  
    uint32_t currentDisplayState;

    // Display Wrangler power stateNumber values
    // 4 Display ON
    // 3 Display Dim
    // 2 Display Sleep
    // 1 Not visible to user
    // 0 Not visible to user
    switch (params->stateNumber) {
    case  4:
        currentDisplayState = kPMDisplayOn;
        break;
    case  3:
        currentDisplayState = kPMDisplayDim;
        break;
    default:
        currentDisplayState = kPMDisplayOff;
    }
  
    if (_displayState != currentDisplayState) {
      _displayStateChangeTime = mach_continuous_time();
      HIDLogDebug ("displayNotificationCallback : displayState change 0x%x -> 0x%x", _displayState, currentDisplayState);
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
    
    // Cancel keyboard/button events when display is asleep or waking up
    // Will still declare using activity.
    if ((_displayState < kPMDisplayDim || (_displayState > kPMDisplayDim && deltaMS < _displayWakeAbortThreshold)) &&
        shouldCancelEvent(event)) {
       HIDLogDebug ("displayStateFilter: Cancel event (type:%d)", IOHIDEventGetType(event));
       result = NULL;
    }
    
    // Prevent digitizer/pointer events from tickling display after 5s, unless sender is DFR.
    if (policy == kIOHIDEventPowerPolicyMaintainSystem &&
        _displayState < kPMDisplayDim &&
        deltaMS > _displaySleepAbortThreshold &&
        sender != _dfr) {
        declareActivity = false;
    }
    
    // Only wake/maintain policies should declare activity
    if (policy == kIOHIDEventNoPolicy) {
        declareActivity = false;
    }
    
    // Only declare user activity every 250ms for digitizer/pointer events
    if (policy == kIOHIDEventPowerPolicyMaintainSystem &&
        eventDelta < _declareActivityThreshold) {
        declareActivity = false;
    } else {
        _previousEventTime = eventTime;
    }
    
    if (declareActivity) {
        // Log display wakes
        if (_displayState < kPMDisplayDim) {
            updateDisplayLog(senderID, policy, eventType);
        }
        dispatch_async(dispatch_get_main_queue(), ^() {
            CFStringRefWrap activityString (std::string(kIOHIDEventSystemServerName) + std::string(".queue.tickle.") +
                                            std::to_string(senderID) +  std::string(".") + std::to_string(eventType));
            IOReturn status = IOPMAssertionDeclareUserActivity(activityString.Reference(),
                                                               kIOPMUserActiveLocal,
                                                               &_AssertionID);
            if (status) {
                HIDLogError ("IOPMAssertionDeclareUserActivity status:0x%x", status);
            }
            
            updateActivity(true);
        });
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
    return false;
}


//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorSessionFilter::updateDisplayLog
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorSessionFilter::updateDisplayLog (IOHIDEventSenderID serviceID, IOHIDEventPolicyValue policy, IOHIDEventType eventType) {
    LogEntry entry;
    
    gettimeofday(&entry.time, NULL);
    entry.serviceID     = serviceID;
    entry.policy        = policy;
    entry.eventType     = eventType;
    
    if (_displayLog == NULL) {
        _displayLog = _IOHIDSimpleQueueCreate(kCFAllocatorDefault, sizeof(LogEntry), LOG_MAX_ENTRIES);
    }
    if (_displayLog) {
        _IOHIDSimpleQueueEnqueue(_displayLog, &entry, true);
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
         HIDLogError ("updateActivity: IOHIDSetStateForSelector status:0x%x", status);
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
    HIDLogDebug ("powerNotificationCallback message: 0x%x \n", messageType);
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
    serializer.SetValueForKey(CFSTR("DisplayWrangelServiceObject"), CFNumberRefWrap(_wrangler));
    serializer.SetValueForKey(CFSTR("DeclareActivityThreshold"), CFNumberRefWrap(_declareActivityThreshold));

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

