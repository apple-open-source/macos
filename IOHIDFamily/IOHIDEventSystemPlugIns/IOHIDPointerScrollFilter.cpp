//
//  IOHIDPointerScrollFilter.cpp
//  IOHIDFamily
//
//  Created by Yevgen Goryachok on 10/30/15.
//
//

#include <AssertMacros.h>
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
#include <IOKit/hid/IOHIDLibPrivate.h>
#include <IOKit/hid/IOHIDPreferences.h>
#include <IOKit/IOCFUnserialize.h>
#include  "IOHIDParameter.h"
#include  "IOHIDEventData.h"

#include <notify.h>
#include <pthread.h>
#include <asl.h>
#include <fcntl.h>
#include <cmath>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include "CF.h"
#include "IOHIDevicePrivateKeys.h"
#include <sstream>
#include "IOHIDPrivateKeys.h"
#include <CoreAnalytics/CoreAnalytics.h>
#include <xpc/xpc.h>


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

// Defer reporting accleration values when updated for 1 minute to avoid lots of transient values.
#define DEFAULT_STATS_DELAY_MS 60000ul
#define kCoreAnalyticsDictionaryAccelerationStats "com.apple.iohid.acceleration"

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
  _leagacyShim(false),
  _pointerAccelerationSupported(true),
  _scrollAccelerationSupported(true),
  _scrollMomentumMult(1.0),
  _dropPropertyEvents(true),
  _minPointerAcceleration(0.1),
  _statsTimer(NULL),
  _statsDelayMS(DEFAULT_STATS_DELAY_MS)
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

    createAccelStatsTimer();
 
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
    if (_statsTimer) {
        IOHIDServiceRef service = (IOHIDServiceRef)CFRetain(_service);
        dispatch_async(_queue, ^{
            dispatch_source_cancel(_statsTimer);
            dispatch_release(_statsTimer);
            CFRelease(service);
        });
    }
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
    CFSTR(kIOHIDUseLinearScalingMouseAccelerationKey),
    CFSTR(kIOHIDUserPointerAccelCurvesKey),
    CFSTR(kIOHIDUserScrollAccelCurvesKey),
    CFSTR(kIOHIDPointerAccelerationMultiplierKey),
    CFSTR(kIOHIDPointerAccelerationSupportKey),
    CFSTR(kIOHIDScrollAccelerationSupportKey),
    CFSTR(kHIDPointerReportRateKey),
    CFSTR(kIOHIDScrollReportRateKey),
    CFSTR(kIOHIDPointerAccelerationAlgorithmKey),
    CFSTR(kIOHIDScrollAccelerationAlgorithmKey),
    CFSTR(kIOHIDPointerAccelerationMinimumKey),
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

  if (CFStringCompare(key, CFSTR(kIOHIDDropAccelPropertyEventsKey), kNilOptions) == kCFCompareEqualTo) {
    _dropPropertyEvents = property == kCFBooleanTrue;
    return;
  }

  if (CFStringCompare(key, CFSTR("IOHIDAcclerationStatsDelayMS"), kNilOptions) == kCFCompareEqualTo) {
    if (CFGetTypeID(property) == CFNumberGetTypeID()) {
        CFNumberGetValue((CFNumberRef)property, kCFNumberLongLongType, &_statsDelayMS);
    }
    return;
  }
  
  for (size_t index = 0 ; index < sizeof(_cachedPropertyList) / sizeof(_cachedPropertyList[0]); index++) {
      if (CFEqual(key,  _cachedPropertyList[index])) {
          _cachedProperty.SetValueForKey(key, property);
          _property.SetValueForKey(key, property);
          updated = true;
          break;
      }
  }
  if (updated) {
      HIDLog("[%@] Acceleration key:%@ value:%@ apply:%s client:%@", SERVICE_ID, key, property, _queue ? "yes" : "no", client);
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

  event = filterPropertyEvent(event);

  if ((_pointerAccelerator && IOHIDEventConformsTo (event, kIOHIDEventTypePointer) && !IOHIDEventIsAbsolute(event)) ||
     ((_scrollAccelerators[0] || _scrollAccelerators[1] || _scrollAccelerators[2]) && IOHIDEventConformsTo (event, kIOHIDEventTypeScroll))) {
    accelerateEvent (event);
  }
  return event;
}

static void SetAccelProperties(const void *key, const void *value, void *context)
{
    IOHIDServiceRef service = (IOHIDServiceRef)context;

    IOHIDServiceSetProperty(service, (CFStringRef)key, (CFTypeRef)value);
}

IOHIDEventRef IOHIDPointerScrollFilter::filterPropertyEvent(IOHIDEventRef event)
{
    if (!(IOHIDEventConformsTo(event, kIOHIDEventTypeVendorDefined) &&
          IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsagePage) == kHIDPage_AppleVendor &&
          IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage) == kHIDUsage_AppleVendor_Properties)) {
        return event;
    }

    void* serializedProperties = IOHIDEventGetDataValue(event, kIOHIDEventFieldVendorDefinedData);
    CFStringRef errorStr = NULL;
    CFIndex propertiesSize = IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedDataLength);
    CFTypeRef eventData = IOCFUnserializeBinary((const char *)serializedProperties, propertiesSize, kCFAllocatorDefault, 0, &errorStr);
    if (errorStr) {
        IOHIDLogError("Unable to deserialize HID Property Event %s", CFStringGetCStringPtr(errorStr, kCFStringEncodingMacRoman));
        CFRelease(errorStr);
        errorStr = NULL;
        return event;
    }


    if (!eventData) {
        return event;
    }

    if (CFGetTypeID(eventData) != CFDictionaryGetTypeID()) {
        CFRelease(eventData);
        return event;
    }

    CFDictionaryRef hidProperties = (CFDictionaryRef)eventData;

    CFTypeRef accelData = CFDictionaryGetValue(hidProperties, CFSTR(kIOHIDServiceAccelerationProperties));

    if (!accelData || CFGetTypeID(accelData) != CFDictionaryGetTypeID()) {
        CFRelease(eventData);
        return event;
    }

    CFDictionaryRef accelProps = (CFDictionaryRef)accelData;

    CFDictionaryApplyFunction(accelProps, &SetAccelProperties, _service);

    if (_dropPropertyEvents) {
        CFRelease(eventData);
        return nil;
    }

    CFRelease(eventData);
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
      _pointerAccelerationSupported &&
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
  if (_scrollAccelerationSupported &&
      IOHIDEventGetType(event) == kIOHIDEventTypeScroll &&
      !(IOHIDEventGetEventFlags(event) & kIOHIDScrollEventOptionsNoAcceleration)) {
    if ((IOHIDEventGetEventFlags(event) & kIOHIDAccelerated) == 0) {
      static int axis [3] = {kIOHIDEventFieldScrollX, kIOHIDEventFieldScrollY, kIOHIDEventFieldScrollZ};
      double value [3];
      accelerated = false;

      if (IOHIDEventGetScrollMomentum(event) != 0) {
        CFTypeRef momentumScrollRate = _IOHIDEventCopyAttachment(event, CFSTR("ScrollMomentumDispatchRate"), 0);

        if (momentumScrollRate && CFGetTypeID(momentumScrollRate) == CFNumberGetTypeID()) {
          CFNumberRef momentumScrollValue = (CFNumberRef)momentumScrollRate;
          double prevScrollMomentumMult = _scrollMomentumMult;
          float dispatchRate = kIOHIDDefaultReportRate;
          
          CFNumberGetValue(momentumScrollValue, kCFNumberFloatType, &dispatchRate);
          _scrollMomentumMult = dispatchRate / kIOHIDDefaultReportRate;

          if (fabs(prevScrollMomentumMult  - _scrollMomentumMult) > 0.5) {
            HIDLogInfo("[%@] _scrollMomentumMult:%.3f->%.3f", SERVICE_ID, prevScrollMomentumMult, _scrollMomentumMult);
          }
        } else {
          _scrollMomentumMult = 1.0;
        }


        if (momentumScrollRate) {
          CFRelease(momentumScrollRate);
          momentumScrollRate = NULL;
        }
      } else {
        _scrollMomentumMult = 1.0;
      }

      
      for (int  index = 0; index < (int)(sizeof(axis) / sizeof(axis[0])); index++) {
        value[index] = IOHIDEventGetFloatValue (event, axis[index]);
        if (value[index] != 0 && _scrollAccelerators[index] != NULL) {
          double *scrollValue = (value + index);
          *scrollValue *= _scrollMomentumMult;
          accelerated |=_scrollAccelerators[index]->accelerate(scrollValue, 1, IOHIDEventGetTimeStamp(event));
          *scrollValue /= _scrollMomentumMult;
        }
      }
      if (accelerated && (accelEvent = IOHIDEventCreateCopy(kCFAllocatorDefault, event)) != NULL) {
        CFMutableArrayRef children = (CFMutableArrayRef) IOHIDEventGetChildren(accelEvent);
        if (children) {
          CFArrayRemoveAllValues(children);
        }
        for (int  index = 0; index < (int)(sizeof(axis) / sizeof(axis[0])); index++) {
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

IOHIDAccelerationAlgorithm * IOHIDPointerScrollFilter::createPointerTableAlgorithm(SInt32 resolution) {
    CFRefWrap<CFDataRef> table ((CFDataRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDPointerAccelerationTableKey)), true);

    if (table.Reference() == NULL) {
        table =  CFRefWrap<CFDataRef>(CFDataCreate(kCFAllocatorDefault, defaultAccelTable, sizeof (defaultAccelTable)), true);
    }

    if (table.Reference()) {
        return IOHIDTableAcceleration::CreateWithTable(
                                               table,
                                               _pointerAcceleration,
                                               FIXED_TO_DOUBLE(resolution),
                                               FRAME_RATE
                                               );
    }
    return nullptr;
}

IOHIDAccelerationAlgorithm * IOHIDPointerScrollFilter::createPointerParametricAlgorithm(SInt32 resolution) {
    CFArrayRefWrap userCurves ((CFArrayRef)copyCachedProperty (CFSTR(kIOHIDUserPointerAccelCurvesKey)), true);
    if (userCurves && userCurves.Count () > 0) {
        return IOHIDParametricAcceleration::CreateWithParameters(
                                                 userCurves,
                                                 _pointerAcceleration,
                                                 FIXED_TO_DOUBLE(resolution),
                                                 FRAME_RATE
                                                 );
    }
    CFRefWrap<CFArrayRef> driverCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDAccelParametricCurvesKey)), true);
    if (driverCurves.Reference()) {
        return IOHIDParametricAcceleration::CreateWithParameters(
                                                 driverCurves,
                                                 _pointerAcceleration,
                                                 FIXED_TO_DOUBLE(resolution),
                                                 FRAME_RATE
                                                 );
    }
    
    return nullptr;
}

IOHIDAccelerationAlgorithm * IOHIDPointerScrollFilter::createPointerAlgorithm(SInt32 resolution) {

    CFNumberRefWrap pointerOverride((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationAlgorithmKey)), true);

    if (pointerOverride) {
        switch ((uint32_t)pointerOverride) {
        case kIOHIDAccelerationAlgorithmTypeTable:
            return createPointerTableAlgorithm(resolution);
            break;
        case kIOHIDAccelerationAlgorithmTypeParametric:
            return createPointerParametricAlgorithm(resolution);
            break;
        case kIOHIDAccelerationAlgorithmTypeDefault:
            // Fall out of switch to use default resolution.
            break;
        default:
            return nullptr;
        }
    }

    IOHIDAccelerationAlgorithm * algorithm = nullptr;

    algorithm = createPointerParametricAlgorithm(resolution);

    return algorithm ? algorithm : createPointerTableAlgorithm(resolution);
}


static CFStringRef AccelTableKeys[] = {
    CFSTR(kIOHIDScrollAccelerationTableXKey),
    CFSTR(kIOHIDScrollAccelerationTableYKey),
    CFSTR(kIOHIDScrollAccelerationTableZKey)
};

IOHIDAccelerationAlgorithm * IOHIDPointerScrollFilter::createScrollTableAlgorithm(size_t index, SInt32 resolution, SInt32 rate) {
    CFRefWrap<CFDataRef> table ((CFDataRef)IOHIDServiceCopyProperty(_service, AccelTableKeys[index]), true);
    
    if (table.Reference() == NULL) {
        table = CFRefWrap<CFDataRef>((CFDataRef)IOHIDServiceCopyProperty(_service, CFSTR(kIOHIDScrollAccelerationTableKey)), true);
    }
    
    if (table.Reference() == NULL) {
        table =  CFRefWrap<CFDataRef>(CFDataCreate(kCFAllocatorDefault, defaultAccelTable, sizeof (defaultAccelTable)), true);
    }
    
    if (table.Reference()) {
        return IOHIDTableAcceleration::CreateWithTable (
                                                 table,
                                                 _scrollAcceleration,
                                                 FIXED_TO_DOUBLE(resolution),
                                                 FIXED_TO_DOUBLE(rate)
                                                 );
   }
   return nullptr;
}

IOHIDAccelerationAlgorithm * IOHIDPointerScrollFilter::createScrollParametricAlgorithm(size_t index, SInt32 resolution, SInt32 rate) {
    CFArrayRefWrap userCurves ((CFArrayRef)copyCachedProperty(CFSTR(kIOHIDUserScrollAccelCurvesKey)), true);
    if (userCurves &&  userCurves.Count() > 0) {
        return IOHIDParametricAcceleration::CreateWithParameters(
                                                  userCurves,
                                                  _scrollAcceleration,
                                                  FIXED_TO_DOUBLE(resolution),
                                                  FIXED_TO_DOUBLE(rate)
                                                  );
    }
    CFRefWrap<CFArrayRef> driverCurves ((CFArrayRef)IOHIDServiceCopyProperty (_service, CFSTR(kHIDScrollAccelParametricCurvesKey)), true);
    if (driverCurves.Reference()) {
        return IOHIDParametricAcceleration::CreateWithParameters(
                                               driverCurves,
                                               _scrollAcceleration,
                                               FIXED_TO_DOUBLE(resolution),
                                               FIXED_TO_DOUBLE(rate)
                                               );
    }
    
    return nullptr;
}

IOHIDAccelerationAlgorithm * IOHIDPointerScrollFilter::createScrollAlgorithm(size_t index, SInt32 resolution, SInt32 rate) {

    CFNumberRefWrap scrollOverride((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDScrollAccelerationAlgorithmKey)), true);

    if (scrollOverride) {
        switch ((uint32_t)scrollOverride) {
        case kIOHIDAccelerationAlgorithmTypeTable:
            return createScrollTableAlgorithm(index, resolution, rate);
            break;
        case kIOHIDAccelerationAlgorithmTypeParametric:
                return createScrollParametricAlgorithm(index, resolution, rate);
            break;
        case kIOHIDAccelerationAlgorithmTypeDefault:
            // Fall out of switch to use default resolution.
            break;
        default:
            return nullptr;
        }
    }

    IOHIDAccelerationAlgorithm * algorithm = nullptr;

    algorithm = createScrollParametricAlgorithm(index, resolution, rate);

    return algorithm ? algorithm : createScrollTableAlgorithm(index, resolution, rate);
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


    CFBooleanRefWrap enabled = CFBooleanRefWrap((CFBooleanRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationSupportKey)), true);
    if (enabled.Reference() == NULL || (bool)enabled) {
        _pointerAccelerationSupported = true;
    } else {
        _pointerAccelerationSupported = false;
    }
  
  CFNumberRefWrap resolution = CFNumberRefWrap((CFNumberRef)IOHIDServiceCopyProperty (_service, CFSTR(kIOHIDPointerResolutionKey)), true);
  
  if (resolution.Reference() == NULL || (SInt32)resolution == 0) {
    resolution = CFNumberRefWrap((SInt32) kDefaultPointerResolutionFixed);
    if (resolution.Reference () == NULL) {
        HIDLogInfo("[%@] Could not get/create pointer resolution", SERVICE_ID);
        return;
    }
  }
  
    CFNumberRefWrap defaultRate = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kHIDPointerReportRateKey)), true);
    if (defaultRate.Reference() == NULL) {
        defaultRate = CFNumberRefWrap((SInt32) 0);
        if (defaultRate.Reference () == NULL) {
            HIDLogInfo ("[%@] Could not get/create pointer report rate", SERVICE_ID);
            return;
        }
    }
    
    CFRefWrap<CFTypeRef>  pointerAccelType(NULL);
    CFRefWrap<CFTypeRef>  typePointerAccel(NULL);
    CFRefWrap<CFTypeRef>  mousePointerAccel(NULL);
    CFRefWrap<CFTypeRef>  basicPointerAccel(NULL);


    CFNumberRefWrap pointerAcceleration;
    bool isMouseAcceleration = false;

  
    pointerAccelType = CFRefWrap<CFTypeRef>(copyCachedProperty (CFSTR(kIOHIDPointerAccelerationTypeKey)), true);

    if (pointerAccelType.Reference()) {
        typePointerAccel =  CFRefWrap<CFTypeRef> (copyCachedProperty((CFStringRef)pointerAccelType.Reference()), true);
        pointerAcceleration = CFNumberRefWrap((CFNumberRef)typePointerAccel.Reference());
        isMouseAcceleration = CFEqual(pointerAccelType.Reference(), CFSTR(kIOHIDMouseAccelerationType));
    }
    if (pointerAcceleration.Reference() == NULL) {
        mousePointerAccel = CFRefWrap<CFTypeRef> (copyCachedProperty(CFSTR(kIOHIDMouseAccelerationType)), true);
        pointerAcceleration = CFNumberRefWrap((CFNumberRef)mousePointerAccel.Reference());
        isMouseAcceleration = true;
    }
    if (pointerAcceleration.Reference() == NULL) {
        basicPointerAccel =  CFRefWrap<CFTypeRef> (copyCachedProperty(CFSTR(kIOHIDPointerAccelerationKey)), true);
        pointerAcceleration = CFNumberRefWrap((CFNumberRef)basicPointerAccel.Reference());
        isMouseAcceleration = false;
    }
    if (pointerAcceleration.Reference()) {
        _pointerAcceleration = FIXED_TO_DOUBLE((SInt32)pointerAcceleration);
    }
    HIDLog ("[%@] Pointer acceleration (%s) %@:%@ %s:%@ %s:%@ %@",
            SERVICE_ID, _pointerAcceleration < 0 ? "disabled" : "enabled",
            pointerAccelType.Reference(), typePointerAccel.Reference(),
            kIOHIDMouseAccelerationType, mousePointerAccel.Reference(),
            kIOHIDPointerAccelerationKey, basicPointerAccel.Reference(),
            pointerAcceleration.Reference());

    if (_pointerAcceleration < 0) {
        return;
    }

    HIDLogDebug("[%@] Pointer acceleration value %f", SERVICE_ID, _pointerAcceleration);

    CFNumberRefWrap useLinearMousePointerAcceleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDUseLinearScalingMouseAccelerationKey)), true);
    HIDLogDebug("[%@] Is mouse acceleration? %s Use linear? %s", SERVICE_ID, isMouseAcceleration ? "yes" : "no", useLinearMousePointerAcceleration ? "yes" : "no");
    if (isMouseAcceleration && useLinearMousePointerAcceleration.Reference() && (SInt32)useLinearMousePointerAcceleration != 0) {
        HIDLog("[%@] Using linear scaling", SERVICE_ID);

        if (_pointerAcceleration == 0) {
            CFNumberRefWrap minimumAccleration = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationMinimumKey)), true);
            if (minimumAccleration && (SInt32)minimumAccleration != 0) {
                _minPointerAcceleration = FIXED_TO_DOUBLE((SInt32)minimumAccleration);
            } else {
                CFNumberRefWrap minPref = CFNumberRefWrap((CFNumberRef)IOHIDPreferencesCopyDomain(CFSTR(kIOHIDPointerAccelerationMinimumKey), kIOHIDFamilyPreferenceApplicationID), true);
                if (minPref.Reference() != NULL && (SInt32)minPref != 0) {
                    _minPointerAcceleration = FIXED_TO_DOUBLE((SInt32)minPref);
                }
            }
            _pointerAcceleration = _minPointerAcceleration;
            HIDLogDebug("[%@] Override pointer acceleration value with minimum %f", SERVICE_ID, _minPointerAcceleration);
        }
        _pointerAccelerator = new IOHIDSimpleAccelerator(_pointerAcceleration);
        return;
    }

    IOHIDAccelerationAlgorithm * algorithm = createPointerAlgorithm((SInt32)resolution);

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

    if (_leagacyShim) {
        for (int  index = 0; index < (int)(sizeof(AccelTableKeys)/sizeof(AccelTableKeys[0])); index++) {
            if (_scrollAccelerators[index] == NULL) {
                _scrollAccelerators[index] = new IOHIDSimpleAccelerator(LEGACY_SHIM_SCROLL_DELTA_MULTIPLIER);
            }
        }
        return;
    }

    CFBooleanRefWrap enabled = CFBooleanRefWrap((CFBooleanRef)copyCachedProperty(CFSTR(kIOHIDScrollAccelerationSupportKey)), true);
    if (enabled.Reference() == NULL || (bool)enabled) {
        _scrollAccelerationSupported = true;
    } else {
        _scrollAccelerationSupported = false;
    }
  
    CFNumberRefWrap       scrollAcceleration;

    CFRefWrap<CFTypeRef>  scrollAccelType(NULL);
    CFRefWrap<CFTypeRef>  typeScrollAccel(NULL);
    CFRefWrap<CFTypeRef>  mouseScrollAccel(NULL);
    CFRefWrap<CFTypeRef>  basicScrollAccel(NULL);

    scrollAccelType = CFRefWrap<CFTypeRef>(copyCachedProperty (CFSTR(kIOHIDScrollAccelerationTypeKey)), true);
  
    if (scrollAccelType.Reference()) {
        typeScrollAccel = CFRefWrap<CFTypeRef>(copyCachedProperty((CFStringRef)scrollAccelType.Reference()), true);
        scrollAcceleration = CFNumberRefWrap((CFNumberRef)typeScrollAccel.Reference());
    }
    if (scrollAcceleration.Reference() == NULL) {
        mouseScrollAccel = CFRefWrap<CFTypeRef>(copyCachedProperty(CFSTR(kIOHIDMouseScrollAccelerationKey)), true);
        scrollAcceleration = CFNumberRefWrap((CFNumberRef)mouseScrollAccel.Reference());
    }
    if (scrollAcceleration.Reference() == NULL) {
        basicScrollAccel = CFRefWrap<CFTypeRef>(copyCachedProperty(CFSTR(kIOHIDScrollAccelerationKey)), true);
        scrollAcceleration = CFNumberRefWrap((CFNumberRef)basicScrollAccel.Reference());
    }
    if (scrollAcceleration.Reference()) {
        _scrollAcceleration = FIXED_TO_DOUBLE((SInt32)scrollAcceleration);
    }

    HIDLog("[%@] Scroll acceleration (%s) %@:%@ %s:%@ %s:%@ %@",
            SERVICE_ID, _scrollAcceleration < 0 ? "disabled" : "enabled",
            scrollAccelType.Reference(), typeScrollAccel.Reference(),
            kIOHIDMouseScrollAccelerationKey, mouseScrollAccel.Reference(),
            kIOHIDScrollAccelerationKey, basicScrollAccel.Reference(),
            scrollAcceleration.Reference()
            );

    if (_scrollAcceleration < 0) {
        return;
    }
  
    HIDLogDebug("[%@] Scroll acceleration value %f", SERVICE_ID, _scrollAcceleration);
  
    CFNumberRefWrap rate = CFNumberRefWrap((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDScrollReportRateKey)), true);
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
            return;
        }

        algorithm = createScrollAlgorithm(index, resolution, rate);

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

  startAccelStatsTimer();
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
    if (_service == NULL){
        return NULL;
    }
    value = IOHIDServiceCopyProperty (_service, key);
    if (value) {
        return value;
    }
    value = _property[key];
    if (value) {
        CFRetain(value);
    }
    return value;
}

CFStringRef IOHIDPointerScrollFilter::getAccelerationAlgorithmString(IOHIDAccelerationAlgorithmType type) const {
    switch (type) {
    case kIOHIDAccelerationAlgorithmTypeTable:
        return CFSTR("Table");
    case kIOHIDAccelerationAlgorithmTypeParametric:
        return CFSTR("Parametric");
    case kIOHIDAccelerationAlgorithmTypeDefault:
        return CFSTR("Default");
    default:
        return CFSTR("Unknown");
    }
}

void IOHIDPointerScrollFilter::statsTimerCallback(void * context) {
    IOHIDPointerScrollFilter * self = (IOHIDPointerScrollFilter*)context;
    
    if(!self->_service) {
        HIDLogError("_service is null, no acceleration stats will be collected");
        return;
    }
    
    dispatch_source_set_timer(self->_statsTimer, DISPATCH_TIME_FOREVER, 0, 0);

    uint16_t vendorID = 0;
    uint16_t productID = 0;
    uint64_t pointerAccelFixed = DOUBLE_TO_FIXED(self->_pointerAcceleration);
    uint64_t scrollAccelFixed = DOUBLE_TO_FIXED(self->_scrollAcceleration);
    CFNumberRefWrap productIDType = CFNumberRefWrap(IOHIDServiceCopyProperty(self->_service, CFSTR(kIOHIDProductIDKey)), true);
    CFNumberRefWrap vendorIDType = CFNumberRefWrap(IOHIDServiceCopyProperty(self->_service, CFSTR(kIOHIDVendorIDKey)), true);

    if (productIDType.Reference()) {
        productID = productIDType;
    }

    if (vendorIDType.Reference()) {
        vendorID = vendorIDType;
    }
    
    if(analytics_is_event_used(kCoreAnalyticsDictionaryAccelerationStats)) {
        xpc_object_t accelDict = xpc_dictionary_create(NULL, NULL, 0);
        
        xpc_dictionary_set_uint64(accelDict, "PointerAccelerationValue", pointerAccelFixed);
        xpc_dictionary_set_uint64(accelDict, "ScrollAccelerationValue", scrollAccelFixed);
        xpc_dictionary_set_uint64(accelDict, "VendorID", vendorID);
        xpc_dictionary_set_uint64(accelDict, "ProductID", productID);
        
        analytics_send_event(kCoreAnalyticsDictionaryAccelerationStats, accelDict);
        
        xpc_release(accelDict);
    }
}

void IOHIDPointerScrollFilter::createAccelStatsTimer(void) {
    _statsTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER, 0, 0, _queue);
    if (!_statsTimer) {
        HIDLogError("Unable to create stats timer, no acceleration stats will be collected");
        return;
    }
    
    dispatch_set_context(_statsTimer, this);
    dispatch_source_set_event_handler_f(_statsTimer, IOHIDPointerScrollFilter::statsTimerCallback);

    dispatch_source_set_timer(_statsTimer, DISPATCH_TIME_FOREVER, 0, 0);
    dispatch_resume(_statsTimer);
}

void IOHIDPointerScrollFilter::startAccelStatsTimer(void) {
    if (_statsTimer) {
        dispatch_source_set_timer(_statsTimer, dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_MSEC * _statsDelayMS), DISPATCH_TIME_FOREVER, 0);
    }
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

    CFNumberRefWrap pointerAccelerationAlgorithmOverride((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDPointerAccelerationAlgorithmKey)), true);
    if (pointerAccelerationAlgorithmOverride) {
        CFStringRefWrap pointerType(getAccelerationAlgorithmString(pointerAccelerationAlgorithmOverride)); 
        serializer.SetValueForKey(CFSTR(kIOHIDPointerAccelerationAlgorithmKey), pointerType.Reference());
    }

    CFRefWrap<CFStringRef>  scrollAccelerationType ((CFStringRef)copyCachedProperty (CFSTR(kIOHIDScrollAccelerationTypeKey)), true);
    if (scrollAccelerationType) {
        serializer.SetValueForKey(CFSTR(kIOHIDScrollAccelerationTypeKey), scrollAccelerationType.Reference());
    }

    CFNumberRefWrap scrollAccelerationAlgorithmOverride((CFNumberRef)copyCachedProperty(CFSTR(kIOHIDScrollAccelerationAlgorithmKey)), true);
    if (scrollAccelerationAlgorithmOverride) {
        CFStringRefWrap scrollType(getAccelerationAlgorithmString(scrollAccelerationAlgorithmOverride)); 
        serializer.SetValueForKey(CFSTR(kIOHIDScrollAccelerationAlgorithmKey), scrollType.Reference());
    }

    serializer.SetValueForKey(CFSTR("Class"), CFSTR("IOHIDPointerScrollFilter"));
    serializer.SetValueForKey(CFSTR("PointerAccelerationValue"), DOUBLE_TO_FIXED(_pointerAcceleration));
    serializer.SetValueForKey(CFSTR("PointerAccelerationMinimum"), DOUBLE_TO_FIXED(_minPointerAcceleration));
    serializer.SetValueForKey(CFSTR("ScrollAccelerationValue") , DOUBLE_TO_FIXED(_scrollAcceleration));
    serializer.SetValueForKey(CFSTR("MatchScore"), (uint64_t)_matchScore);
    serializer.SetValueForKey(CFSTR("Property"), _property.Reference());

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
