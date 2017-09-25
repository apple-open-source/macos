//
//  IOHIDNXEventTranslatorServiceFilter.cpp
//  IOHIDFamily
//
//  Created by Yevgen Goryachok on 10/30/15.
//
//

#include "IOHIDNXEventTranslatorServiceFilter.h"
#include <new>
#include <CoreFoundation/CoreFoundation.h>
#include <CoreFoundation/CFLogUtilities.h>
#include <IOKit/hid/IOHIDServiceFilterPlugIn.h>
#include <IOKit/hid/IOHIDEventSystemPrivate.h>
#include <IOKit/hid/IOHIDEventTypes.h>
#include <IOKit/hid/IOHIDSession.h>

#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDPrivateKeys.h>
#include <IOKit/hid/IOHIDEventSystemKeys.h>
#include <IOKit/hid/AppleHIDUsageTables.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDKeys.h>
#include <IOKit/hid/IOHIDServiceKeys.h>
#include "IOHIDParameter.h"
#include "IOHIDEventData.h"

#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include "IOHIDDebug.h"
#include "CF.h"
#include "IOHIDevicePrivateKeys.h"


// 8410C8C6-D024-4708-AF6E-7CC3D1FD7E12
#define kIOHIDNXEventTranslatorServiceFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x84, 0x10, 0xC8, 0xC6, 0xD0, 0x24, 0x47, 0x08, 0xAF, 0x6E, 0x7C, 0xC3, 0xD1, 0xFD, 0x7E, 0x12)


extern "C" void * IOHIDNXEventTranslatorServiceFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);



//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilterFactory
//------------------------------------------------------------------------------

void *IOHIDNXEventTranslatorServiceFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDServiceFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDNXEventTranslatorServiceFilter), 0);
        return new(p) IOHIDNXEventTranslatorServiceFilter(kIOHIDNXEventTranslatorServiceFilterFactory);
    }

    return NULL;
}

// The IOHIDNXEventTranslatorServiceFilter function table.
IOHIDServiceFilterPlugInInterface  IOHIDNXEventTranslatorServiceFilter::sIOHIDNXEventTranslatorServiceFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDNXEventTranslatorServiceFilter::QueryInterface,
    IOHIDNXEventTranslatorServiceFilter::AddRef,
    IOHIDNXEventTranslatorServiceFilter::Release,
    // IOHIDSimpleServiceFilterPlugInInterface functions
    IOHIDNXEventTranslatorServiceFilter::match,
    IOHIDNXEventTranslatorServiceFilter::filter,
    NULL,
    // IOHIDServiceFilterPlugInInterface functions
    IOHIDNXEventTranslatorServiceFilter::open,
    IOHIDNXEventTranslatorServiceFilter::close,
    IOHIDNXEventTranslatorServiceFilter::scheduleWithDispatchQueue,
    IOHIDNXEventTranslatorServiceFilter::unscheduleFromDispatchQueue,
    IOHIDNXEventTranslatorServiceFilter::copyPropertyForClient,
    IOHIDNXEventTranslatorServiceFilter::setPropertyForClient,
    NULL,
    IOHIDNXEventTranslatorServiceFilter::setEventCallback,
};

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::IOHIDNXEventTranslatorServiceFilter
//------------------------------------------------------------------------------
IOHIDNXEventTranslatorServiceFilter::IOHIDNXEventTranslatorServiceFilter(CFUUIDRef factoryID):
  _serviceInterface(&sIOHIDNXEventTranslatorServiceFilterFtbl),
  _factoryID(NULL),
  _refCount(1),
  _matchScore(0),
  _eventCallback(NULL),
  _eventTarget(NULL),
  _eventContext(NULL),
  _queue(NULL),
  _service(NULL),
  _translator(NULL)
{
  HIDLogDebug("(%p)", this);
  if (factoryID) {
    _factoryID = static_cast<CFUUIDRef>(CFRetain(factoryID));
    CFPlugInAddInstanceForFactory( factoryID );
  }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::~IOHIDNXEventTranslatorServiceFilter
//------------------------------------------------------------------------------
IOHIDNXEventTranslatorServiceFilter::~IOHIDNXEventTranslatorServiceFilter()
{
  if (_factoryID) {
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
  }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDNXEventTranslatorServiceFilter::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->QueryInterface(iid, ppv);
}

// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDNXEventTranslatorServiceFilter::QueryInterface( REFIID iid, LPVOID *ppv )
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
// IOHIDNXEventTranslatorServiceFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDNXEventTranslatorServiceFilter::AddRef( void *self )
{
    return static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->AddRef();
}

ULONG IOHIDNXEventTranslatorServiceFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDNXEventTranslatorServiceFilter::Release( void *self )
{
    return static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->Release();
}

ULONG IOHIDNXEventTranslatorServiceFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::open
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::open(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->open(service, options);
}

void IOHIDNXEventTranslatorServiceFilter::open(IOHIDServiceRef service, IOOptionBits options)
{
    HIDLogDebug("(%p)", this);

    (void)options;
    _service = service;
    _translator = IOHIDKeyboardEventTranslatorCreateWithService (CFGetAllocator(_service), _service);
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::close
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::close(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->close(service, options);
}

void IOHIDNXEventTranslatorServiceFilter::close(IOHIDServiceRef service, IOOptionBits options)
{
    HIDLogDebug("(%p)", this);

    (void) options;
    (void) service;
    _service = NULL;
    if (_translator) {
      CFRelease(_translator);
      _translator = NULL;
    }
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDNXEventTranslatorServiceFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _queue = queue;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->unscheduleFromDispatchQueue(queue);
}

void IOHIDNXEventTranslatorServiceFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    _queue = NULL;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::setEventCallback
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->setEventCallback(callback, target, refcon);
}

void IOHIDNXEventTranslatorServiceFilter::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    //HIDLogDebug("(%p) callback = %p target = %p refcon = %p", this, callback, target, refcon);

    _eventCallback  = callback;
    _eventTarget    = target;
    _eventContext   = refcon;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::copyPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDNXEventTranslatorServiceFilter::copyPropertyForClient(void * self, CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->copyPropertyForClient(key, client);
}

CFTypeRef IOHIDNXEventTranslatorServiceFilter::copyPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{
  
  CFTypeRef result = NULL;
  
  if (CFEqual (key, CFSTR(kHIDEventTranslationModifierFlags))) {
    
      uint32_t serviceModifiers = IOHIDKeyboardEventTranslatorGetModifierFlags (_translator);
      result = CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &serviceModifiers);
  
  } else if (CFEqual(key, CFSTR(kIOHIDServiceFilterDebugKey))) {
    
      CFMutableDictionaryRefWrap serializer;
      if (serializer) {
        serialize(serializer);
        result = CFRetain(serializer.Reference());
      }
  }

  return result;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::setPropertyForClient(void * self,CFStringRef key,CFTypeRef property,CFTypeRef client)
{
    static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDNXEventTranslatorServiceFilter::setPropertyForClient(CFStringRef key,CFTypeRef property __unused, CFTypeRef client __unused)
{
  if (CFEqual(key, CFSTR(kIOHIDServiceCapsLockStateKey))) {
      if (_queue && _translator) {
          CFRetain(_service);
          dispatch_async(_queue, ^(){
              CFBooleanRefWrap capsLockState ((CFBooleanRef)IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDServiceCapsLockStateKey)), true);
              uint32_t translationFlags = capsLockState ? kTranslationFlagCapsLockOn : 0;
            
              if ((IOHIDKeyboardEventTranslatorGetModifierFlags (_translator) & NX_ALPHASHIFTMASK) != (capsLockState ? NX_ALPHASHIFTMASK : 0)) {
                  CFRefWrap <IOHIDEventRef> dummyEvent (IOHIDEventCreate(kCFAllocatorDefault, kIOHIDEventTypeKeyboard, 0, 0), true);
                  CFArrayRef collection =  IOHIDKeyboardEventTranslatorCreateEventCollection (_translator, dummyEvent, translationFlags);
                  if (collection) {
                      for (CFIndex index = 0; index < CFArrayGetCount(collection); index++) {
                          IOHIDEventRef translatedEvent = (IOHIDEventRef)CFArrayGetValueAtIndex(collection, index);
                          if (translatedEvent) {
                            _eventCallback(_eventTarget, _eventContext, &_serviceInterface, translatedEvent, 0);
                          }
                      }
                      CFRelease(collection);
                  }
              }
              CFRelease(_service);
          });
      }
  }
  if (_translator) {
    IOHIDKeyboardEventTranslatorSetProperty (_translator, key, property);
  }
  return;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::match
//------------------------------------------------------------------------------
SInt32 IOHIDNXEventTranslatorServiceFilter::match(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    return static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->match(service, options);
}

SInt32 IOHIDNXEventTranslatorServiceFilter::match(IOHIDServiceRef service, IOOptionBits options)
{
    (void) options;
    _matchScore = (IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Keyboard) ||
                   IOHIDServiceConformsTo(service, kHIDPage_Consumer, kHIDUsage_Csmr_ConsumerControl)) ? 100 : 0;
    
    HIDLogDebug("(%p) for ServiceID %@ with score %d", this, IOHIDServiceGetRegistryID(service), (int)_matchScore);
    
    return _matchScore;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDNXEventTranslatorServiceFilter::filter(void * self, IOHIDEventRef event)
{
  return static_cast<IOHIDNXEventTranslatorServiceFilter *>(self)->filter(event);
}


IOHIDEventRef IOHIDNXEventTranslatorServiceFilter::filter(IOHIDEventRef event)
{
  if (!event || !_translator) {
      return event;
  }
  CFArrayRef collection;
  if (IOHIDEventConformsTo (event, kIOHIDEventTypeKeyboard)) {
    CFBooleanRefWrap capsLockState ((CFBooleanRef)IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDServiceCapsLockStateKey)), true);
    uint32_t translationFlags = capsLockState ? kTranslationFlagCapsLockOn : 0;
    collection =  IOHIDKeyboardEventTranslatorCreateEventCollection (_translator, event, translationFlags);
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
  return event;
}

//------------------------------------------------------------------------------
// IOHIDNXEventTranslatorServiceFilter::serialize
//------------------------------------------------------------------------------
void IOHIDNXEventTranslatorServiceFilter::serialize (CFMutableDictionaryRef dict) const {
    CFMutableDictionaryRefWrap serializer (dict);
    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDNXEventTranslatorServiceFilter"));
    serializer.SetValueForKey(CFSTR("MatchScore"), (uint64_t)_matchScore);
}
