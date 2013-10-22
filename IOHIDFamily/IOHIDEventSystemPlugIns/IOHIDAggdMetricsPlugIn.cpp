/*
 *  IOHIDAggdMetricsPlugIn.cpp
 *  IOHIDEventSystemPlugIns
 *
 *  Created by Rob Yepez on 05/21/2013.
 *  Copyright 2013 Apple Inc. All rights reserved.
 *
 */

#include <new>
#include <AggregateDictionary/ADClient.h>
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/hid/IOHIDSessionFilterPlugIn.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDSession.h>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDDisplay.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <pthread.h>
#include "IOHIDAggdMetricsPlugIn.h"


#define kAggregateDictionaryKeyboardEnumerationCountKey "com.apple.iokit.hid.keyboard.enumerationCount"
#define kAggregateDictionaryHomeButtonWakeCountKey      "com.apple.iokit.hid.homeButton.wakeCount"

// 072BC077-E984-4C2A-BB72-D4769CE44FAF
#define kIOHIDAggdMetricsPlugInFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x07, 0x2B, 0xC0, 0x77, 0xE9, 0x84, 0x4C, 0x2A, 0xBB, 0x72, 0xD4, 0x76, 0x9C, 0xE4, 0x4F, 0xAF)

extern "C" void * IOHIDAggdMetricsPlugInFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);

//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugInFactory
//------------------------------------------------------------------------------
// Implementation of the factory function for this type.
void *IOHIDAggdMetricsPlugInFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    // If correct type is being requested, allocate an
    // instance of TestType and return the IUnknown interface.
    if (CFEqual(typeUUID, kIOHIDSessionFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDAggdMetricsPlugIn), 0);
        return new(p) IOHIDAggdMetricsPlugIn(kIOHIDAggdMetricsPlugInFactory);
    }
    // If the requested type is incorrect, return NULL.
    return NULL;
}
// The IOHIDAggdMetricsPlugIn function table.
IOHIDSessionFilterPlugInInterface IOHIDAggdMetricsPlugIn::sIOHIDAggdMetricsPlugInFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDAggdMetricsPlugIn::QueryInterface,
    IOHIDAggdMetricsPlugIn::AddRef,
    IOHIDAggdMetricsPlugIn::Release,
    // IOHIDSimpleSessionFilterPlugInInterface functions
    IOHIDAggdMetricsPlugIn::filter,
    IOHIDAggdMetricsPlugIn::filter,      // filterCopyEvent
    IOHIDAggdMetricsPlugIn::copyEvent,
    // IOHIDSessionFilterPlugInInterface functions
    IOHIDAggdMetricsPlugIn::open,
    IOHIDAggdMetricsPlugIn::close,
    IOHIDAggdMetricsPlugIn::registerDisplay,
    IOHIDAggdMetricsPlugIn::unregisterDisplay,
    IOHIDAggdMetricsPlugIn::registerService,
    IOHIDAggdMetricsPlugIn::unregisterService,
    IOHIDAggdMetricsPlugIn::scheduleWithRunLoop,
    IOHIDAggdMetricsPlugIn::unscheduleFromRunLoop,
    IOHIDAggdMetricsPlugIn::getPropertyForClient,
    IOHIDAggdMetricsPlugIn::setPropertyForClient,
}; // Interface implementation
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::IOHIDAggdMetricsPlugIn
//------------------------------------------------------------------------------
IOHIDAggdMetricsPlugIn::IOHIDAggdMetricsPlugIn(CFUUIDRef factoryID)
:
_sessionInterface(&sIOHIDAggdMetricsPlugInFtbl),
_factoryID( static_cast<CFUUIDRef>( CFRetain(factoryID) ) ),
_refCount(1),
_factor(1.0f)
{
    CFPlugInAddInstanceForFactory( factoryID );
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::IOHIDAggdMetricsPlugIn
//------------------------------------------------------------------------------
IOHIDAggdMetricsPlugIn::~IOHIDAggdMetricsPlugIn()
{
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDAggdMetricsPlugIn::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDAggdMetricsPlugIn *>(self)->QueryInterface(iid, ppv);
}
// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDAggdMetricsPlugIn::QueryInterface( REFIID iid, LPVOID *ppv )
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
// IOHIDAggdMetricsPlugIn::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDAggdMetricsPlugIn::AddRef( void *self )
{
    return static_cast<IOHIDAggdMetricsPlugIn *>(self)->AddRef();
}
ULONG IOHIDAggdMetricsPlugIn::AddRef()
{
    _refCount += 1;
    return _refCount;
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::Release
//------------------------------------------------------------------------------
ULONG IOHIDAggdMetricsPlugIn::Release( void *self )
{
    return static_cast<IOHIDAggdMetricsPlugIn *>(self)->Release();
}
ULONG IOHIDAggdMetricsPlugIn::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::open
//------------------------------------------------------------------------------
boolean_t IOHIDAggdMetricsPlugIn::open(void * self, IOHIDSessionRef session, IOOptionBits options)
{
    return true;
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::close
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::close(void * self, IOHIDSessionRef inSession, IOOptionBits options)
{
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::registerDisplay
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::registerDisplay(void * self, IOHIDDisplayRef display)
{
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::unregisterDisplay
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::unregisterDisplay(void * self, IOHIDDisplayRef display)
{
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::registerService
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::registerService(void * self, IOHIDServiceRef service)
{
    static_cast<IOHIDAggdMetricsPlugIn *>(self)->registerService(service);
}
void IOHIDAggdMetricsPlugIn::registerService(IOHIDServiceRef service)
{
    if ( IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) )
    {
        ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryKeyboardEnumerationCountKey), 1);
    }
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::unregisterService
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::unregisterService(void * self, IOHIDServiceRef inService)
{
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::scheduleWithRunLoop
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::scheduleWithRunLoop(void * self, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
    return;
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::unscheduleFromRunLoop
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::unscheduleFromRunLoop(void * self, CFRunLoopRef runLoop, CFStringRef runLoopMode)
{
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::getPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDAggdMetricsPlugIn::getPropertyForClient(void * self, CFStringRef key, CFTypeRef client)
{
    return NULL;
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDAggdMetricsPlugIn::setPropertyForClient(void * self, CFStringRef key, CFTypeRef property, CFTypeRef client)
{
    static_cast<IOHIDAggdMetricsPlugIn *>(self)->setPropertyForClient(key, property, client);
}
void IOHIDAggdMetricsPlugIn::setPropertyForClient(CFStringRef key, CFTypeRef property, CFTypeRef client)
{
    Boolean factorChange        = CFEqual(key, CFSTR(kIOHIDDisplayBrightnessFactorKey)) || CFEqual(key, CFSTR(kIOHIDDisplayBrightnessFactorWithFadeKey));
    if ( factorChange ) {
        if ( CFGetTypeID(property) == CFNumberGetTypeID() ) {
            float factor = _factor;
            CFNumberGetValue((CFNumberRef)property, kCFNumberFloatType, &factor);
            // After application, did anything change relating to the display
            if ( factor != _factor ) {
                _factor = factor;
           }
        }
    }
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDAggdMetricsPlugIn::filter(void * self, IOHIDServiceRef sender, IOHIDEventRef event)
{
    return static_cast<IOHIDAggdMetricsPlugIn *>(self)->filter(sender, event);
}
IOHIDEventRef IOHIDAggdMetricsPlugIn::filter(IOHIDServiceRef sender, IOHIDEventRef event)
{
    float factor = _factor;
    CFRetain(event);
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_LOW, 0), ^{
                
        if ( event && (IOHIDEventGetType(event) == kIOHIDEventTypeKeyboard)
            && IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardDown)
            && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsagePage) == kHIDPage_Consumer)
            && (IOHIDEventGetIntegerValue(event, kIOHIDEventFieldKeyboardUsage) == kHIDUsage_Csmr_Menu)
            && (factor == 0.0)) {
            
            ADClientAddValueForScalarKey(CFSTR(kAggregateDictionaryHomeButtonWakeCountKey), 1);
        }
        
        CFRelease(event);
    });
    
    return event;
}
//------------------------------------------------------------------------------
// IOHIDAggdMetricsPlugIn::copyEvent
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDAggdMetricsPlugIn::copyEvent(void * self, IOHIDEventType type, IOHIDEventRef matching, IOOptionBits options)
{
    return NULL;
}
