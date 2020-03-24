//
//  IOHIDPointerScrollFilter.cpp
//  IOHIDFamily
//
//  Created by Yevgen Goryachok on 10/30/15.
//
//

#include "IOHIDPointerScrollFilter.h"


#include <new>
#include <TargetConditionals.h>
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
#include <IOKit/hid/IOHIDEventServiceTypes.h>
#include  "IOHIDParameter.h"
#include  "IOHIDEventData.h"

#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include "IOHIDDebug.h"
#include "CF.h"
#include "IOHIDevicePrivateKeys.h"
#include <sstream>
#include "IOHIDPrivateKeys.h"


#define SERVICE_ID (_service ? IOHIDServiceGetRegistryID(_service) : NULL)


#define LEGACY_SHIM_SCROLL_DELTA_MULTIPLIER   0.1
#define LEGACY_SHIM_POINTER_DELTA_MULTIPLIER  1

//736169DC-A8BC-45B4-BC14-645B5526E585
#define kIOHIDPointerScrollFilterFactory CFUUIDGetConstantUUIDWithBytes(kCFAllocatorSystemDefault, 0x73, 0x61, 0x69, 0xDC, 0xA8, 0xBC, 0x45, 0xB4, 0xBC, 0x14, 0x64, 0x5B, 0x55, 0x26, 0xE5, 0x85)

extern "C" void * IOHIDPointerScrollFilterFactory(CFAllocatorRef allocator, CFUUIDRef typeUUID);


static const UInt8 defaultAccelTable[] = {
    0x00, 0x00, 0x80, 0x00,
    0x40, 0x32, 0x30, 0x30, 0x00, 0x02, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00,
    0x00, 0x09, 0x00, 0x00, 0x71, 0x3B, 0x00, 0x00,
    0x60, 0x00, 0x00, 0x04, 0x4E, 0xC5, 0x00, 0x10,
    0x80, 0x00, 0x00, 0x0C, 0x00, 0x00, 0x00, 0x5F,
    0x00, 0x00, 0x00, 0x16, 0xEC, 0x4F, 0x00, 0x8B,
    0x00, 0x00, 0x00, 0x1D, 0x3B, 0x14, 0x00, 0x94,
    0x80, 0x00, 0x00, 0x22, 0x76, 0x27, 0x00, 0x96,
    0x00, 0x00, 0x00, 0x24, 0x62, 0x76, 0x00, 0x96,
    0x00, 0x00, 0x00, 0x26, 0x00, 0x00, 0x00, 0x96,
    0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x96,
    0x00, 0x00
};


//------------------------------------------------------------------------------
// IOHIDPointerScrollFilterFactory
//------------------------------------------------------------------------------

void *IOHIDPointerScrollFilterFactory(CFAllocatorRef allocator __unused, CFUUIDRef typeUUID)
{
    if (CFEqual(typeUUID, kIOHIDServiceFilterPlugInTypeID)) {
        void *p = CFAllocatorAllocate(kCFAllocatorDefault, sizeof(IOHIDPointerScrollFilter), 0);
        return new(p) IOHIDPointerScrollFilter(kIOHIDPointerScrollFilterFactory);
    }

    return NULL;
}

// The IOHIDPointerScrollFilter function table.
IOHIDServiceFilterPlugInInterface IOHIDPointerScrollFilter::sIOHIDPointerScrollFilterFtbl =
{
    // Required padding for COM
    NULL,
    // These three are the required COM functions
    IOHIDPointerScrollFilter::QueryInterface,
    IOHIDPointerScrollFilter::AddRef,
    IOHIDPointerScrollFilter::Release,
    // IOHIDSimpleServiceFilterPlugInInterface functions
    IOHIDPointerScrollFilter::match,
    IOHIDPointerScrollFilter::filter,
    NULL,
    // IOHIDServiceFilterPlugInInterface functions
    IOHIDPointerScrollFilter::open,
    IOHIDPointerScrollFilter::close,
    IOHIDPointerScrollFilter::scheduleWithDispatchQueue,
    IOHIDPointerScrollFilter::unscheduleFromDispatchQueue,
    IOHIDPointerScrollFilter::copyPropertyForClient,
    IOHIDPointerScrollFilter::setPropertyForClient,
    NULL,
    IOHIDPointerScrollFilter::setEventCallback,
};

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::IOHIDPointerScrollFilter
//------------------------------------------------------------------------------
IOHIDPointerScrollFilter::IOHIDPointerScrollFilter(CFUUIDRef factoryID):
  _serviceInterface(&sIOHIDPointerScrollFilterFtbl),
  _factoryID(NULL),
  _refCount(1),
  _matchScore(0),
  _eventCallback(NULL),
  _eventTarget(NULL),
  _eventContext(NULL),
  _pointerAccelerator(NULL),
  _queue(NULL),
  _cachedProperty (0),
  _service(NULL),
  _pointerAcceleration(-1),
  _scrollAcceleration(-1),
  _leagacyShim(false)
{
  for (size_t index = 0; index < sizeof(_scrollAccelerators)/sizeof(_scrollAccelerators[0]); index++) {
    _scrollAccelerators[index] = NULL;
  }
  if (factoryID) {
    _factoryID = static_cast<CFUUIDRef>(CFRetain(factoryID));
    CFPlugInAddInstanceForFactory( factoryID );
  }
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::~IOHIDPointerScrollFilter
//------------------------------------------------------------------------------
IOHIDPointerScrollFilter::~IOHIDPointerScrollFilter()
{
  if (_factoryID) {
    CFPlugInRemoveInstanceForFactory( _factoryID );
    CFRelease( _factoryID );
  }
  if (_pointerAccelerator) {
    delete _pointerAccelerator;
  }
  for (size_t index = 0; index < sizeof(_scrollAccelerators)/sizeof(_scrollAccelerators[0]); index++) {
    if (_scrollAccelerators[index]) {
      delete _scrollAccelerators[index];
    }
  }
    
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::QueryInterface
//------------------------------------------------------------------------------
HRESULT IOHIDPointerScrollFilter::QueryInterface( void *self, REFIID iid, LPVOID *ppv )
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->QueryInterface(iid, ppv);
}

// Implementation of the IUnknown QueryInterface function.
HRESULT IOHIDPointerScrollFilter::QueryInterface( REFIID iid, LPVOID *ppv )
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
// IOHIDPointerScrollFilter::AddRef
//------------------------------------------------------------------------------
ULONG IOHIDPointerScrollFilter::AddRef( void *self )
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->AddRef();
}

ULONG IOHIDPointerScrollFilter::AddRef()
{
    _refCount += 1;
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::Release
//------------------------------------------------------------------------------
ULONG IOHIDPointerScrollFilter::Release( void *self )
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->Release();
}

ULONG IOHIDPointerScrollFilter::Release()
{
    _refCount -= 1;
    if (_refCount == 0) {
        delete this;
        return 0;
    }
    return _refCount;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::open
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::open(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->open(service, options);
}

void IOHIDPointerScrollFilter::open(IOHIDServiceRef service, IOOptionBits options __unused)
{
  
    _service = service;

    CFTypeRef leagcyShim = IOHIDServiceCopyProperty(service, CFSTR(kIOHIDCompatibilityInterface));
    if (leagcyShim) {
      _leagacyShim = true;
      CFRelease(leagcyShim);
    }

}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::close
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::close(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->close(service, options);
}

void IOHIDPointerScrollFilter::close(IOHIDServiceRef service __unused, IOOptionBits options __unused)
{
    _service = NULL;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::scheduleWithDispatchQueue
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::scheduleWithDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->scheduleWithDispatchQueue(queue);
}

void IOHIDPointerScrollFilter::scheduleWithDispatchQueue(dispatch_queue_t queue)
{
    _queue = queue;
 
    setupAcceleration();
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::unscheduleFromDispatchQueue
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::unscheduleFromDispatchQueue(void * self, dispatch_queue_t queue)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->unscheduleFromDispatchQueue(queue);
}

void IOHIDPointerScrollFilter::unscheduleFromDispatchQueue(dispatch_queue_t queue __unused)
{
    _queue = NULL;
    
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setEventCallback
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setEventCallback(void * self, IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->setEventCallback(callback, target, refcon);
}

void IOHIDPointerScrollFilter::setEventCallback(IOHIDServiceEventCallback callback, void * target, void * refcon)
{
    _eventCallback  = callback;
    _eventTarget    = target;
    _eventContext   = refcon;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::copyPropertyForClient
//------------------------------------------------------------------------------
CFTypeRef IOHIDPointerScrollFilter::copyPropertyForClient(void * self, CFStringRef key, CFTypeRef client)
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->copyPropertyForClient(key, client);
}

CFTypeRef IOHIDPointerScrollFilter::copyPropertyForClient(CFStringRef key, CFTypeRef client __unused)
{

    CFTypeRef result = NULL;
  
    if (CFEqual(key, CFSTR(kIOHIDServiceFilterDebugKey))) {
        CFMutableDictionaryRefWrap serializer;
        serialize(serializer);
        if (serializer) {
          result = CFRetain(serializer.Reference());
        }
    }

    return result;
}

CFStringRef IOHIDPointerScrollFilter::_cachedPropertyList[] = {
    CFSTR(kIOHIDPointerAccelerationKey),
    CFSTR(kIOHIDScrollAccelerationKey),
    CFSTR(kIOHIDMouseAccelerationType),
    CFSTR(kIOHIDMouseScrollAccelerationKey),
    CFSTR(kIOHIDTrackpadScrollAccelerationKey),
    CFSTR(kIOHIDTrackpadAccelerationType),
    CFSTR(kIOHIDScrollAccelerationTypeKey),
    CFSTR(kIOHIDPointerAccelerationTypeKey),
    CFSTR(kIOHIDUserPointerAccelCurvesKey),
    CFSTR(kIOHIDUserScrollAccelCurvesKey),
    CFSTR(kIOHIDPointerAccelerationMultiplierKey)
};

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setPropertyForClient
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setPropertyForClient(void * self,CFStringRef key,CFTypeRef property,CFTypeRef client)
{
    static_cast<IOHIDPointerScrollFilter *>(self)->setPropertyForClient(key, property, client);
}

void IOHIDPointerScrollFilter::setPropertyForClient(CFStringRef key,CFTypeRef property,CFTypeRef client __unused)
{
  bool  updated = false;
  if (key == NULL) {
      return;
  }
  
  for (size_t index = 0 ; index < sizeof(_cachedPropertyList) / sizeof(_cachedPropertyList[0]); index++) {
      if (CFEqual(key,  _cachedPropertyList[index])) {
          _cachedProperty.SetValueForKey(key, property);
          updated = true;
          break;
      }
  }
  if (updated && _queue) {
      setupAcceleration ();
      _cachedProperty.RemoveAll();
  }
  return;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::match
//------------------------------------------------------------------------------
SInt32 IOHIDPointerScrollFilter::match(void * self, IOHIDServiceRef service, IOOptionBits options)
{
    return static_cast<IOHIDPointerScrollFilter *>(self)->match(service, options);
}

SInt32 IOHIDPointerScrollFilter::match(IOHIDServiceRef service, IOOptionBits options __unused)
{
    _matchScore = (IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Mouse) ||
                   IOHIDServiceConformsTo(service, kHIDPage_GenericDesktop, kHIDUsage_GD_Pointer) ||
                   IOHIDServiceConformsTo(service, kHIDPage_AppleVendor,    kHIDUsage_GD_Pointer) ||
                   IOHIDServiceConformsTo(service, kHIDPage_Digitizer,    kHIDUsage_Dig_TouchPad)) ? 100 : 0;
    
    HIDLogDebug("(%p) for ServiceID %@ with score %d", this, IOHIDServiceGetRegistryID(service), (int)_matchScore);
    
    return _matchScore;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::filter
//------------------------------------------------------------------------------
IOHIDEventRef IOHIDPointerScrollFilter::filter(void * self, IOHIDEventRef event)
{
  return static_cast<IOHIDPointerScrollFilter *>(self)->filter(event);
}

IOHIDEventRef IOHIDPointerScrollFilter::filter(IOHIDEventRef event)
{
  if (!event) {
      return event;
  }

  if ((_pointerAccelerator && IOHIDEventConformsTo (event, kIOHIDEventTypePointer) && !IOHIDEventIsAbsolute(event)) ||
     ((_scrollAccelerators[0] || _scrollAccelerators[1] || _scrollAccelerators[2]) && IOHIDEventConformsTo (event, kIOHIDEventTypeScroll))) {
    accelerateEvent (event);
  }
  return event;
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::accelerateChildrens
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::accelerateChildrens(IOHIDEventRef event) {
  
  CFArrayRef children = IOHIDEventGetChildren (event);
  for (CFIndex index = 0 , count = children ? CFArrayGetCount(children) : 0 ; index < count ; index++) {
    accelerateEvent ((IOHIDEventRef)CFArrayGetValueAtIndex(children, index));
  }

}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::accelerateEvent
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::accelerateEvent(IOHIDEventRef event) {
  bool accelerated;
  IOHIDEventRef accelEvent;

  if (_pointerAccelerator &&
      IOHIDEventGetType(event) == kIOHIDEventTypePointer &&
      !(IOHIDEventGetEventFlags(event) & kIOHIDPointerEventOptionsNoAcceleration)) {
    double xy[2];
    if ((IOHIDEventGetEventFlags(event) & kIOHIDAccelerated) == 0) {
      
      xy[0] = IOHIDEventGetFloatValue (event, kIOHIDEventFieldPointerX);
      xy[1] = IOHIDEventGetFloatValue (event, kIOHIDEventFieldPointerY);
      if (xy[0] || xy[1]) {
        accelerated = _pointerAccelerator->accelerate(xy, sizeof (xy) / sizeof(xy[0]), IOHIDEventGetTimeStamp(event));
        
        if (accelerated && (accelEvent = IOHIDEventCreateCopy(kCFAllocatorDefault, event)) != NULL) {
          CFMutableArrayRef children = (CFMutableArrayRef)IOHIDEventGetChildren(accelEvent);
          if (children) {
            CFArrayRemoveAllValues(children);
          }
          IOHIDEventSetFloatValue (accelEvent, kIOHIDEventFieldPointerX, xy[0]);
          IOHIDEventSetFloatValue (accelEvent, kIOHIDEventFieldPointerY, xy[1]);
          IOHIDEventSetEventFlags (accelEvent, IOHIDEventGetEventFlags(accelEvent) | kIOHIDAccelerated);
          IOHIDEventAppendEvent (event, accelEvent, 0);
          CFRelease(accelEvent);
        }
      }
    }
  }
  if (IOHIDEventGetType(event) == kIOHIDEventTypeScroll &&
      !(IOHIDEventGetEventFlags(event) & kIOHIDScrollEventOptionsNoAcceleration)) {
    if ((IOHIDEventGetEventFlags(event) & kIOHIDAccelerated) == 0) {
      static int axis [3] = {kIOHIDEventFieldScrollX, kIOHIDEventFieldScrollY, kIOHIDEventFieldScrollZ};
      double value [3];
      accelerated = false;
      
      for (int  index = 0; index < (int)(sizeof(axis) / sizeof(axis[0])); index++) {
        value[index] = IOHIDEventGetFloatValue (event, axis[index]);
        if (value[index] != 0 && _scrollAccelerators[index] != NULL) {
          accelerated |=_scrollAccelerators[index]->accelerate(&value[index], 1, IOHIDEventGetTimeStamp(event));
        }
      }
      if (accelerated && (accelEvent = IOHIDEventCreateCopy(kCFAllocatorDefault, event)) != NULL) {
        CFMutableArrayRef children = (CFMutableArrayRef) IOHIDEventGetChildren(accelEvent);
        if (children) {
          CFArrayRemoveAllValues(children);
        }
        for (int  index = 0; index < (int)(sizeof(axis) / sizeof(axis[0])); index++) {
#if TARGET_OS_IPHONE
          // 10x gain factor on scrolling, inherited from older implementations.
          // TODO: This gain is handled in SkyLight:__IOHIDPointerEventTranslatorProcessScrollCount on macOS. Unify logic here.
          value[index] *= 10;
#endif
          IOHIDEventSetFloatValue (accelEvent, axis[index], value[index]);
        }
        IOHIDEventSetEventFlags (accelEvent, IOHIDEventGetEventFlags(accelEvent) | kIOHIDAccelerated);
        IOHIDEventAppendEvent (event, accelEvent, 0);
        CFRelease(accelEvent);
      }
    }
  }
  accelerateChildrens(event);
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setupPointerAcceleration
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setupPointerAcceleration(double pointerAccelerationMultiplier)
{
  
  if (_leagacyShim) {
    if (_pointerAccelerator == NULL) {
      _pointerAccelerator = new IOHIDSimpleAccelerator(LEGACY_SHIM_POINTER_DELTA_MULTIPLIER);
    }
    return;
  }
  
  IOHIDAccelerator *tmp = _pointerAccelerator;
  _pointerAccelerator = NULL;

  if (tmp) {
    delete tmp;
  }
  
  CFNumberRefWrap resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDPointerResolutionKey)), true);
  
  if (resolution.Reference() == NULL || (SInt32)resolution == 0) {
    resolution = CFNumberRefWrap((SInt32) kDefaultPointerResolutionFixed);
    if (resolution.Reference () == NULL) {
      HIDLogInfo("[%@] Could not get/create pointer resolution", SERVICE_ID);
      return;
    }
  }
  
    CFNumberRefWrap defaultRate = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDPointerReportRateKey)), true);
    if (defaultRate.Reference() == NULL) {
        defaultRate = CFNumberRefWrap((SInt32) 0);
        if (defaultRate.Reference () == NULL) {
            HIDLogInfo ("[%@] Could not get/create pointer report rate", SERVICE_ID);
            return;
        }
    }
  CFNumberRefWrap pointerAcceleration;
  
  CFRefWrap<CFStringRef>  accelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDPointerAccelerationTypeKey)), true);

  if (accelerationType.Reference()) {
    pointerAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(accelerationType), true);
  }
  if (pointerAcceleration.Reference() == NULL) {
    pointerAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDMouseAccelerationType)), true);
  }
  if (pointerAcceleration.Reference() == NULL) {
    pointerAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationKey)), true);
  }
  if (pointerAcceleration.Reference()) {
    _pointerAcceleration = FIXED_TO_DOUBLE((SInt32)pointerAcceleration);
  }
  if (_pointerAcceleration < 0) {
    HIDLogInfo("[%@] Could not find kIOHIDMouseAccelerationType or acceleration disabled", SERVICE_ID);
    return;
  }

  HIDLogDebug("[%@] Pointer acceleration value %f", SERVICE_ID, _pointerAcceleration);

  IOHIDAccelerationAlgorithm * algorithm = NULL;
  
  CFArrayRefWrap userCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDUserPointerAccelCurvesKey)), true);
  if (userCurves && userCurves.Count () > 0) {
    algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                 userCurves,
                                                 _pointerAcceleration,
                                                 FIXED_TO_DOUBLE((SInt32)resolution),
                                                 FRAME_RATE
                                                 );
  } else {
  
    CFRefWrap<CFArrayRef> driverCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDAccelParametricCurvesKey)), true);
    if (driverCurves.Reference()) {
      algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                 driverCurves,
                                                 _pointerAcceleration,
                                                 FIXED_TO_DOUBLE((SInt32)resolution),
                                                 FRAME_RATE
                                                 );
    } else {
      CFRefWrap<CFDataRef> table ((CFDataRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDPointerAccelerationTableKey)), true);
  
      if (table.Reference() == NULL) {
        table =  CFRefWrap<CFDataRef>(CFDataCreate(kCFAllocatorDefault, defaultAccelTable, sizeof (defaultAccelTable)), true);
      }
        
      if (table.Reference()) {
        algorithm = IOHIDTableAcceleration::CreateWithTable(
                                                   table,
                                                   _pointerAcceleration,
                                                   FIXED_TO_DOUBLE((SInt32)resolution),
                                                   FRAME_RATE
                                                   );
      }
    }
  }
  if (algorithm) {
    _pointerAccelerator = new IOHIDPointerAccelerator (algorithm, FIXED_TO_DOUBLE((SInt32)resolution), (SInt32)defaultRate, pointerAccelerationMultiplier);
  } else {
    HIDLogInfo("[%@] Could not create accelerator", SERVICE_ID);
  }
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setupScrollAcceleration
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setupScrollAcceleration(double scrollAccelerationMultiplier) {

  static CFStringRef ResolutionKeys[] = {
    CFSTR(kIOHIDScrollResolutionXKey),
    CFSTR(kIOHIDScrollResolutionYKey),
    CFSTR(kIOHIDScrollResolutionZKey)
  };
  static CFStringRef AccelTableKeys[] = {
    CFSTR(kIOHIDScrollAccelerationTableXKey),
    CFSTR(kIOHIDScrollAccelerationTableYKey),
    CFSTR(kIOHIDScrollAccelerationTableZKey)
  };

  if (_leagacyShim) {
    for (int  index = 0; index < (int)(sizeof(AccelTableKeys)/sizeof(AccelTableKeys[0])); index++) {
      if (_scrollAccelerators[index] == NULL) {
        _scrollAccelerators[index] = new IOHIDSimpleAccelerator(LEGACY_SHIM_SCROLL_DELTA_MULTIPLIER);
      }
    }
    return;
  }
  
  CFNumberRefWrap scrollAcceleration;
  
  CFRefWrap<CFStringRef>  accelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDScrollAccelerationTypeKey)), true);
  
  if (accelerationType.Reference()) {
    scrollAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(accelerationType), true);
  }
  if (scrollAcceleration.Reference() == NULL) {
    scrollAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDMouseScrollAccelerationKey)), true);
  }
  if (scrollAcceleration.Reference() == NULL) {
    scrollAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDScrollAccelerationKey)), true);
  }
  if (scrollAcceleration.Reference()) {
    _scrollAcceleration = FIXED_TO_DOUBLE((SInt32)scrollAcceleration);
  }
  
  if (_scrollAcceleration < 0) {
    HIDLogInfo("[%@] Could not find kIOHIDMouseScrollAccelerationKey or acceleration disabled", SERVICE_ID);
    return;
  }
  
  HIDLogDebug("[%@] Scroll acceleration value %f", SERVICE_ID, _scrollAcceleration);
  
  CFNumberRefWrap rate = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR("HIDScrollReportRate")), true);
  if (rate.Reference() == NULL || (SInt32)rate == 0) {
    rate = CFNumberRefWrap((SInt32)((int)FRAME_RATE << 16));
    if (rate.Reference() == NULL) {
      HIDLogInfo("[%@] Could not get/create report rate", SERVICE_ID);
      return;
    }
  }
  
  for (int  index = 0; index < (int)(sizeof(AccelTableKeys)/sizeof(AccelTableKeys[0])); index++) {
    
    IOHIDAccelerationAlgorithm * algorithm = NULL;
    
    IOHIDAccelerator *tmp = _scrollAccelerators[index];
    _scrollAccelerators[index] = NULL;

    if (tmp) {
      delete tmp;
    }
    
    CFNumberRefWrap resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, ResolutionKeys[index]), true);
    if (resolution.Reference() == NULL) {
      resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDScrollResolutionKey)), true);
    }
    if (resolution.Reference() == NULL) {
      HIDLogInfo("[%@] Could not get kIOHIDScrollResolutionKey", SERVICE_ID);
      continue;
    }

    CFArrayRefWrap userCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDUserScrollAccelCurvesKey)), true);
    if (userCurves &&  userCurves.Count() > 0) {
        algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                  userCurves,
                                                  _scrollAcceleration,
                                                  FIXED_TO_DOUBLE((SInt32)resolution),
                                                  FIXED_TO_DOUBLE((SInt32)rate)
                                                  );
    } else {
      CFRefWrap<CFArrayRef> driverCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDScrollAccelParametricCurvesKey)), true);
      if (driverCurves.Reference()) {
        algorithm = IOHIDParametricAcceleration::CreateWithParameters(
                                                   driverCurves,
                                                   _scrollAcceleration,
                                                   FIXED_TO_DOUBLE((SInt32)resolution),
                                                   FIXED_TO_DOUBLE((SInt32)rate)
                                                   );
      } else {
        
        CFRefWrap<CFDataRef> table ((CFDataRef)IOHIDServiceCopyProperty(_service, AccelTableKeys[index]), true);
        
        if (table.Reference() == NULL) {
          table = CFRefWrap<CFDataRef>((CFDataRef)IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDScrollAccelerationTableKey)), true);
        }
        
        if (table.Reference() == NULL) {
            table =  CFRefWrap<CFDataRef>(CFDataCreate(kCFAllocatorDefault, defaultAccelTable, sizeof (defaultAccelTable)), true);
        }
      
        if (table.Reference()) {
           algorithm = IOHIDTableAcceleration::CreateWithTable (
                                                     table,
                                                     _scrollAcceleration,
                                                     FIXED_TO_DOUBLE((SInt32)resolution),
                                                     FIXED_TO_DOUBLE((SInt32)rate)
                                                     );
        }
      }
    }
    if (algorithm) {
      _scrollAccelerators[index] = new IOHIDScrollAccelerator(algorithm, FIXED_TO_DOUBLE((SInt32)resolution), FIXED_TO_DOUBLE((SInt32)rate), scrollAccelerationMultiplier);
    }
  }
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::setAcceleration
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::setupAcceleration()
{
  if (!_service) {
    HIDLogDebug("(%p) setupAcceleration service not available", this);
    return;
  }

  CFNumberRefWrap pointerAccelerationMultiplier = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationMultiplierKey)), true);
  if (pointerAccelerationMultiplier.Reference() == NULL || (SInt32)pointerAccelerationMultiplier == 0) {
     pointerAccelerationMultiplier = CFNumberRefWrap((SInt32) DOUBLE_TO_FIXED((double)(1.0)));
     if (pointerAccelerationMultiplier.Reference() == NULL) {
       HIDLogInfo("[%@] Could not get/create pointer acceleration multiplier", SERVICE_ID);
       return;
     }
  }
  setupPointerAcceleration(FIXED_TO_DOUBLE((SInt32)pointerAccelerationMultiplier));

  // @reado scroll acceleration logic without timestamp
  // for now no need for fixed multiplier since timestamp
  // is average over multiple packets , so it's not same problem
  // as pointer accleration where rate multiplier considers
  // delta between two packets
  setupScrollAcceleration (1.0);
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::getCachedProperty
//------------------------------------------------------------------------------
CFTypeRef IOHIDPointerScrollFilter::copyCachedProperty (CFStringRef key)  const {
  CFTypeRef value = _cachedProperty [key];
  if (value) {
    CFRetain(value);
    return value;
  }
  return IOHIDServiceCopyProperty (_service, key);
}

//------------------------------------------------------------------------------
// IOHIDPointerScrollFilter::serialize
//------------------------------------------------------------------------------
void IOHIDPointerScrollFilter::serialize (CFMutableDictionaryRef dict) const {
  CFMutableDictionaryRefWrap serializer (dict);
  const char * axis[] = {"X", "Y", "Z"};
  if (serializer.Reference() == NULL) {
      return;
  }
  
  CFRefWrap<CFStringRef>  pointerAccelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDPointerAccelerationTypeKey)), true);
  if (pointerAccelerationType) {
      serializer.SetValueForKey(CFSTR(kIOHIDPointerAccelerationTypeKey), pointerAccelerationType.Reference());
  }

  CFRefWrap<CFStringRef>  scrollAccelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDScrollAccelerationTypeKey)), true);
  if (scrollAccelerationType) {
      serializer.SetValueForKey(CFSTR(kIOHIDScrollAccelerationTypeKey), scrollAccelerationType.Reference());
  }
  
  serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDPointerScrollFilter"));
  serializer.SetValueForKey(CFSTR("PointerAccelerationValue"), DOUBLE_TO_FIXED(_pointerAcceleration));
  serializer.SetValueForKey(CFSTR("ScrollAccelerationValue") , DOUBLE_TO_FIXED(_scrollAcceleration));
  serializer.SetValueForKey(CFSTR("MatchScore"), (uint64_t)_matchScore);
    
  if (_pointerAccelerator) {
      CFMutableDictionaryRefWrap pa;
      _pointerAccelerator->serialize (pa);
      serializer.SetValueForKey(CFSTR("Pointer Accelerator"), pa);
  }
  for (size_t i = 0 ; i < sizeof(_scrollAccelerators)/ sizeof(_scrollAccelerators[0]); i++) {
      if (_scrollAccelerators[i]) {
          CFMutableDictionaryRefWrap sa;
          CFStringRefWrap axiskey (std::string("Scroll Accelerator ( axis: ") + std::string(axis[i]) +  std::string(")"));
          _scrollAccelerators[i]->serialize(sa);
          serializer.SetValueForKey(axiskey, sa);
      }
  }
}
