//
//  #include "IOHIDPointerEventTranslation.h" IOHIDPointerEventTranslation.c
//  IOHIDFamily
//
//  Created by yg on 12/23/15.
//
//

#include <AssertMacros.h>
#include <dispatch/dispatch.h>
#include <CoreFoundation/CFRuntime.h>
#include <IOKit/hid/IOHIDServiceClient.h>
#include <IOKit/hid/IOHIDService.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include "AppleHIDUsageTables.h"
#include "IOHIDFamilyPrivate.h"
#include <IOKit/usb/USB.h>
#include "IOHIDEventData.h"
#include "IOHIDPointerEventTranslation.h"
#include "IOHIDevicePrivateKeys.h"
#include "IOHIDDebug.h"
#include <mach/mach_time.h>


#define kPhaseMask (kIOHIDEventPhaseBegan   | kIOHIDEventPhaseChanged  | kIOHIDEventPhaseEnded | kIOHIDEventPhaseCancelled | kIOHIDEventPhaseMayBegin)


#define ABS_TO_NS(t,b)               ((t * (b).numer)/ (b).denom)
#define NS_TO_ABS(t,b)               ((t * (b).denom)/ (b).numer)
#define FIXED_TO_DOUBLE(x)           (((double)x)/65536.0)

#define	EV_DCLICKTIME                500000000               /* Default nanoseconds for a double-click */
#define	EV_DCLICKSPACE               3                       /* Default pixel threshold for double-clicks */

#define kAbsoluteToPixelTranslation  1024

#define SIGN(x) ((x > 0) - (x < 0))



#define TRANSLATE_SCROLL_MOMENTUM(scrollEvent) \
  (((IOHIDEventGetPhase (scrollEvent) & kPhaseMask) << 8) | \
  ((IOHIDEventGetPhase (scrollEvent) >> 2) & kScrollTypeMomentumContinue) | \
  ((IOHIDEventGetPhase (scrollEvent) >> 1) & \
  (kScrollTypeMomentumStart | kScrollTypeMomentumEnd)))

#define kIOHIDScrollPixelToWheelScale           FIXED_TO_DOUBLE(0x0000199a)
#define kIOFixedOne                             0x10000ULL
#define kIOHIDScrollDefaultResolution           FIXED_TO_DOUBLE (9 * kIOFixedOne)
#define kIOHIDScrollConsumeResolution           FIXED_TO_DOUBLE (100 * kIOFixedOne)
#define kIOHIDScrollConsumeCountMultiplier      3
#define kIOHIDScrollCountMaxTimeDeltaBetween    600
#define kIOHIDScrollCountMaxTimeDeltaToSustain  250
#define kIOHIDScrollCountMinDeltaToStart        (30)
#define kIOHIDScrollCountMinDeltaToSustain      (20)
#define kIOHIDScrollCountIgnoreMomentumScrolls  true
#define kIOHIDScrollCountMouseCanReset          true
#define kIOHIDScrollCountMax                    2000
#define kIOHIDScrollCountAccelerationFactor     FIXED_TO_DOUBLE(163840)
#define NULLEVENTNUM                            0
#define INITEVENTNUM                            13





#define kZoomModifierMask (NX_COMMANDMASK| NX_ALTERNATEMASK | NX_CONTROLMASK | NX_SHIFTMASK)
#define kLegacyMouseEventsMask (NX_LMOUSEDOWNMASK|NX_RMOUSEDOWNMASK|NX_LMOUSEUPMASK|NX_RMOUSEUPMASK|NX_OMOUSEDOWNMASK|NX_OMOUSEUPMASK)


typedef struct {
  boolean_t       direction;
  double          accumulator;
  uint32_t        count;
  uint32_t        clearThreshold;
  uint32_t        countThreshold;
  boolean_t       hiResolution;
} CONSUME_RECORD;


typedef struct {
  uint32_t        buttons;
  boolean_t       isMultiTouchService;
  boolean_t       isContinuousScroll;
  uint32_t        buttonCount;
  boolean_t       clickRemappingDisable;
  CFTypeRef       service;
  IOHIDFloat      lastAbsoluteX;
  IOHIDFloat      lastAbsoluteY;
  CONSUME_RECORD  scrollConsume[3];
} SERVICE_RECORD;

typedef struct {
  uint32_t          globalButtons;
  uint32_t          flags;
  uint64_t          timestamp;
  uint64_t          serviceid;
  SERVICE_RECORD    *serviceRecord;
  CFMutableArrayRef eventCollection;
  IOHIDEventRef     event;
} EVENT_TRANSLATOR_CONTEXT;




static uint64_t __IOHIDEventTranslatorGetServiceIDForObject (CFTypeRef service);
CFTypeRef __IOHIDEventTranslatorCopyServiceProperty (CFTypeRef service, CFStringRef key);

void __IOHIDPointerEventTranslatorCreateMouseUpDownEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent);
void __IOHIDPointerEventTranslatorCreateSysdefineEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context);

static void __IOHIDPointerEventTranslatorFree( CFTypeRef object );
static CFStringRef __IOHIDPointerEventTranslatorCopyDebugDescription(CFTypeRef cf);
IOHIDPointerEventTranslatorRef __IOHIDPointerEventTranslatorCreatePrivate(CFAllocatorRef allocator, CFAllocatorContext * context __unused);
IOHIDEventRef __IOHIDPointerEventTranslatorCreateMouseMoveEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent);
void __IOHIDPointerEventTranslatorInitNxEvent (EVENT_TRANSLATOR_CONTEXT *context, NXEventExt *nxEvent, uint8_t type);
void __IOHIDPointerEventTranslatorProcessButtonState (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent);
void __IOHIDPointerEventTranslatorProcessClickState (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent, IOHIDEventRef buttonEvent);

void __IOHIDPointerEventTranslatorProcessScrollCount (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef scrollEvent);

IOHIDEventRef __IOHIDPointerEventTranslatorCreateScrollEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef scrollEvent, IOHIDEventRef accellScrollEvent);
boolean_t __IOHIDEventTranslatorGetBooleanProperty (CFTypeRef service, CFStringRef key, boolean_t defaultValue);
uint64_t __IOHIDEventTranslatorGetIntegerProperty (CFTypeRef service, CFStringRef key, uint64_t defaultValue);
SERVICE_RECORD * __IOHIDEventTranslatorGetServiceRecordForServiceID (IOHIDPointerEventTranslatorRef translator,  uint64_t serviceID);
static uint32_t __IOHIDPointerEventTranslatorGetUniqueEventNumber (IOHIDPointerEventTranslatorRef translator);
uint32_t __IOHIDPointerEventTranslatorRemapButtons (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint32_t buttons);
void __IOHIDPointerEventTranslatorProcessLegacyEvent (IOHIDPointerEventTranslatorRef translator,EVENT_TRANSLATOR_CONTEXT *context,  IOHIDEventRef legacyMouseEvent);
NXEventExt * __IOHIDPointerEventTranslatorGetNxMouseEvents (IOHIDEventRef event);
double __IOHIDPointerEventTranslatorGetScrollDelta (SERVICE_RECORD *record, int axisIndex, double value, double accelValue);
uint32_t __IOHIDPointerEventTranslatorInitScrollResolution (CFTypeRef service, int axisIndex);
void __IOHIDPointerEventTranslatorInitScrollConsumeRecords (SERVICE_RECORD * serviceRecord);


static const CFRuntimeClass __IOHIDPointerEventTranslatorClass = {
  0,                      // version
  "IOHIDPointerEventTranslator",  // className
  NULL,                   // init
  NULL,                   // copy
  __IOHIDPointerEventTranslatorFree,     // finalize
  NULL,                   // equal
  NULL,                   // hash
  NULL,                   // copyFormattingDesc
  __IOHIDPointerEventTranslatorCopyDebugDescription,
  NULL,
  NULL
};

typedef struct  __IOHIDPointerEventTranslator {
  CFRuntimeBase             cfBase;   // base CFType information
  uint32_t                  globalButtons;
  CFMutableDictionaryRef    serviceRecord;
  mach_timebase_info_data_t timebaseInfo;
  uint32_t                  flags;
  uint8_t                   clickCount;
  uint64_t                  lastClickTime;
  uint64_t                  clickCountTimeTreshold;
  uint32_t                  clickCountPixelTreshold;
  IOHIDFloat                clickCountDeltaX;
  IOHIDFloat                clickCountDeltaY;
  uint16_t                  scrollCount;
  uint32_t                  scrollDicrection;
  uint64_t                  lastScrollTime;
  uint64_t                  lastScrollSustainTime;
  IOHIDFloat                scrollCountDeltaX;
  IOHIDFloat                scrollCountDeltaY;
  boolean_t                 scrollIncrementThisPhase;
  uint64_t                  scrollCountMaxTimeDeltaBetween;
  uint32_t                  scrollCountMaxTimeDeltaToSustain;
  uint32_t                  scrollCountMinDeltaToStartPow2;
  uint32_t                  scrollCountMinDeltaToSustainPow2;
  boolean_t                 scrollCountIgnoreMomentumScrolls;
  boolean_t                 scrollCountMouseCanReset;
  uint32_t                  scrollCountMax;
  double                    scrollCountAccelerationFactor;
  uint32_t                  zoomModifierMask;
  boolean_t                 zoomLastScrollWasZoom;
  uint32_t                  eventNumber;
  uint32_t                  lastLeftEventNum;
  uint32_t                  lastRightEventNum;
  uint32_t                  buttonMode;
} __IOHIDPointerEventTranslator;

static dispatch_once_t  __pointerTranslatorTypeInit            = 0;
static CFTypeID         __pointerTranslatorTypeID              = _kCFRuntimeNotATypeID;

//------------------------------------------------------------------------------
// IOHIDPointerEventTranslatorGetTypeID
//------------------------------------------------------------------------------
CFTypeID IOHIDPointerEventTranslatorGetTypeID(void)
{
  if ( _kCFRuntimeNotATypeID == __pointerTranslatorTypeID ) {
    dispatch_once(&__pointerTranslatorTypeInit, ^{
      __pointerTranslatorTypeID = _CFRuntimeRegisterClass(&__IOHIDPointerEventTranslatorClass);
    });
  }
  return __pointerTranslatorTypeID;
}


//------------------------------------------------------------------------------
// IOHIDPointerEventTranslatorCreate
//------------------------------------------------------------------------------
IOHIDPointerEventTranslatorRef IOHIDPointerEventTranslatorCreate (CFAllocatorRef allocator, uint32_t options)
{
  (void)options;
  
  IOHIDPointerEventTranslatorRef translator  = __IOHIDPointerEventTranslatorCreatePrivate(allocator, NULL);
  if (!translator) {
    return translator;
  }
  

  translator->serviceRecord = CFDictionaryCreateMutable (CFGetAllocator(translator), 0, NULL, &kCFTypeDictionaryValueCallBacks);
  require(translator->serviceRecord, error_exit);

  mach_timebase_info(&(translator->timebaseInfo));
  
  translator->clickCountTimeTreshold                = EV_DCLICKTIME;
  translator->clickCountPixelTreshold               = EV_DCLICKSPACE;
  translator->scrollCountMaxTimeDeltaBetween        = NS_TO_ABS (kIOHIDScrollCountMaxTimeDeltaBetween*kMillisecondScale,translator->timebaseInfo);
  translator->scrollCountMaxTimeDeltaToSustain      = NS_TO_ABS (kIOHIDScrollCountMaxTimeDeltaToSustain*kMillisecondScale,translator->timebaseInfo);
  translator->scrollCountMinDeltaToStartPow2        = kIOHIDScrollCountMinDeltaToStart * kIOHIDScrollCountMinDeltaToStart;
  translator->scrollCountMinDeltaToSustainPow2      = kIOHIDScrollCountMinDeltaToSustain * kIOHIDScrollCountMinDeltaToSustain;
  translator->scrollCountIgnoreMomentumScrolls      = kIOHIDScrollCountIgnoreMomentumScrolls;
  translator->scrollCountMouseCanReset              = kIOHIDScrollCountMouseCanReset;
  translator->scrollCountMax                        = kIOHIDScrollCountMax;
  translator->scrollCountAccelerationFactor         = kIOHIDScrollCountAccelerationFactor;
  translator->eventNumber                           = INITEVENTNUM;
  translator->buttonMode                            = NX_RightButton;
  
  return translator;
  
error_exit:
  
  CFRelease (translator);
  translator = NULL;
  return translator;
}


//------------------------------------------------------------------------------
// __IOHIDPointerEventTranslatorInitScrollResolution
//------------------------------------------------------------------------------

uint32_t __IOHIDPointerEventTranslatorInitScrollResolution (CFTypeRef service, int axisIndex) {
  static CFStringRef scrollRes [] = {CFSTR(kIOHIDScrollResolutionYKey), CFSTR(kIOHIDScrollResolutionXKey), CFSTR(kIOHIDScrollResolutionZKey)};
  uint32_t resolution = (uint32_t)__IOHIDEventTranslatorGetIntegerProperty (service, scrollRes[axisIndex], 0);
  if (!resolution) {
    resolution = (uint32_t)__IOHIDEventTranslatorGetIntegerProperty (service,  CFSTR(kIOHIDScrollResolutionKey), 0);
  }
  return resolution;
}

//------------------------------------------------------------------------------
// __IOHIDPointerEventTranslatorInitScrollConsumeRecord
//------------------------------------------------------------------------------
void __IOHIDPointerEventTranslatorInitScrollConsumeRecords (SERVICE_RECORD * serviceRecord) {
  
  for (int index = 0; index < 3; index++) {
    uint32_t resolution = __IOHIDPointerEventTranslatorInitScrollResolution (serviceRecord->service, index);
    if (!resolution) {
      continue;
    }
    serviceRecord->scrollConsume[index].hiResolution   = ((FIXED_TO_DOUBLE(resolution) * 2) > kIOHIDScrollDefaultResolution);
    serviceRecord->scrollConsume[index].clearThreshold =  (uint32_t)floor((FIXED_TO_DOUBLE(resolution) / kIOHIDScrollConsumeResolution) * 2);
    serviceRecord->scrollConsume[index].countThreshold = serviceRecord->scrollConsume[index].clearThreshold * kIOHIDScrollConsumeCountMultiplier;
  }
}


//------------------------------------------------------------------------------
// IOHIDPointerEventTranslatorRegisterServcie
//------------------------------------------------------------------------------
void IOHIDPointerEventTranslatorRegisterService (IOHIDPointerEventTranslatorRef translator, CFTypeRef service)
{
  
  if (service == NULL || translator == NULL) {
    return;
  }

  uint64_t serviceID =  __IOHIDEventTranslatorGetServiceIDForObject (service);
  HIDLogDebug ("register service %llu", serviceID);
  
  
  if (CFDictionaryContainsKey (translator->serviceRecord, (const void*)serviceID)) {
    return;
  }
  
  SERVICE_RECORD  record = {
    .buttons = 0,
    .isMultiTouchService = false,
    .isContinuousScroll = false,
    .buttonCount = 0,
    .clickRemappingDisable = false,
    .service = service
  };

  record.isMultiTouchService = __IOHIDEventTranslatorGetBooleanProperty(service,  CFSTR("MTEventSource"), false);
  
  if (__IOHIDEventTranslatorGetIntegerProperty (service,  CFSTR(kIOHIDScrollResolutionXKey), 0) > (18 << 16) ||
      __IOHIDEventTranslatorGetIntegerProperty (service,  CFSTR(kIOHIDScrollResolutionYKey), 0) > (18 << 16) ||
      __IOHIDEventTranslatorGetIntegerProperty (service,  CFSTR(kIOHIDScrollResolutionZKey), 0) > (18 << 16) ||
      __IOHIDEventTranslatorGetIntegerProperty (service,  CFSTR(kIOHIDScrollResolutionKey),  0) > (18 << 16) ) {
    record.isContinuousScroll = true;
  }
  
  record.buttonCount = (uint32_t)__IOHIDEventTranslatorGetIntegerProperty (service,  CFSTR(kIOHIDPointerButtonCountKey), 1);
  
  record.clickRemappingDisable  = __IOHIDEventTranslatorGetBooleanProperty (service, CFSTR(kIOHIDDisallowRemappingOfPrimaryClickKey), false);
  
  __IOHIDPointerEventTranslatorInitScrollConsumeRecords (&record);
    
  CFMutableDataRef serviceRecord = CFDataCreateMutable(CFGetAllocator(translator), sizeof (record));
  if (serviceRecord) {
    CFDataSetLength(serviceRecord, sizeof (record));
    UInt8 * serviceRecordPtr = (UInt8*)CFDataGetBytePtr(serviceRecord);
    if (serviceRecordPtr) {
      memcpy(serviceRecordPtr, &record, sizeof(record));
      CFDictionarySetValue(translator->serviceRecord, (const void *) serviceID, serviceRecord);
    }
    CFRelease(serviceRecord);
  }
}

  
  
//------------------------------------------------------------------------------
// IOHIDPointerEventTranslatorUnRegisterService
//------------------------------------------------------------------------------
void IOHIDPointerEventTranslatorUnRegisterService (IOHIDPointerEventTranslatorRef translator, CFTypeRef service)
{
  if (service == NULL || translator == NULL) {
    return;
  }
  
  uint64_t serviceID =  __IOHIDEventTranslatorGetServiceIDForObject (service);
  CFDictionaryRemoveValue (translator->serviceRecord, (const void *)serviceID);
}


//------------------------------------------------------------------------------
// IOHIDPointerEventTranslatorSetProperty
//------------------------------------------------------------------------------
void IOHIDPointerEventTranslatorSetProperty (IOHIDPointerEventTranslatorRef translator, CFStringRef key, CFTypeRef property) {
  if (CFEqual(key, CFSTR(kIOHIDClickTimeKey)) && property && CFGetTypeID(property) == CFNumberGetTypeID()) {
    CFNumberGetValue(property, kCFNumberSInt64Type, &translator->clickCountTimeTreshold);
  } else if (CFEqual(key, CFSTR(kIOHIDScrollZoomModifierMaskKey)) && property && CFGetTypeID(property) == CFNumberGetTypeID()) {
    CFNumberGetValue(property, kCFNumberSInt32Type, &translator->zoomModifierMask);
    translator->zoomModifierMask &= kZoomModifierMask;
  } else if (CFEqual(key, CFSTR(kIOHIDPointerButtonMode)) && property && CFGetTypeID(property) == CFNumberGetTypeID()) {
    CFNumberGetValue(property, kCFNumberSInt32Type, &translator->buttonMode);

  }
}


//------------------------------------------------------------------------------
// IOHIDPointerEventTranslatorCreateEventCollection
//------------------------------------------------------------------------------
CFArrayRef IOHIDPointerEventTranslatorCreateEventCollection (IOHIDPointerEventTranslatorRef translator, IOHIDEventRef event, CFTypeRef sender,  uint32_t flags, uint32_t options __unused)
{
  
  if (!event) {
    return NULL;
  }
  
    
  CFMutableArrayRef eventCollection = CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks);
  if (eventCollection == NULL) {
    return NULL;
  }
  IOHIDEventRef            translatedEvent;
  EVENT_TRANSLATOR_CONTEXT context;
  
  context.serviceid = IOHIDEventGetSenderID (event);
  context.timestamp = IOHIDEventGetTimeStamp (event);
  context.flags     = flags;
  context.globalButtons   = translator->globalButtons;
  context.eventCollection = eventCollection;
  context.event           = event;
  context.serviceRecord = __IOHIDEventTranslatorGetServiceRecordForServiceID (translator, context.serviceid);
  
  if (context.serviceRecord == NULL) {
    IOHIDPointerEventTranslatorRegisterService(translator, sender);
    context.serviceRecord = __IOHIDEventTranslatorGetServiceRecordForServiceID (translator, context.serviceid);
    if (context.serviceRecord == NULL) {
      HIDLogError("unable to get/create service record for pointer event translation");
      return NULL;
    }
  }
  
  
  IOHIDEventRef legacyMouseEvent = __IOHIDPointerEventTranslatorGetNxMouseEvents (event) ? event : NULL;
  if (legacyMouseEvent) {
      __IOHIDPointerEventTranslatorProcessLegacyEvent (translator, &context, legacyMouseEvent);
  }
  
  IOHIDEventRef pointerEvent = IOHIDEventGetEvent (event, kIOHIDEventTypePointer);
  
  if (pointerEvent != NULL && (IOHIDEventGetEventFlags(pointerEvent) & kIOHIDAccelerated) == 0) {
    CFIndex i, count;
    CFArrayRef children = IOHIDEventGetChildren(pointerEvent);
    for (i=0, count = (children) ? CFArrayGetCount(children) : 0; i < count; i++) {
      IOHIDEventRef child = IOHIDEventGetEvent((IOHIDEventRef)CFArrayGetValueAtIndex(children, i), kIOHIDEventTypePointer);
      if (child && IOHIDEventGetEventFlags(child) & kIOHIDAccelerated) {
        pointerEvent = child;
        break;
      }
    }
  }

  IOHIDEventRef scrollEvent = IOHIDEventGetEvent (event, kIOHIDEventTypeScroll);
  IOHIDEventRef accelScrollEvent = scrollEvent;
  
  if (scrollEvent != NULL && (IOHIDEventGetEventFlags(scrollEvent) & kIOHIDAccelerated) == 0) {
    CFIndex i, count;
    CFArrayRef children = IOHIDEventGetChildren(scrollEvent);
    for (i=0, count = (children) ? CFArrayGetCount(children) : 0; i < count; i++) {
      IOHIDEventRef child = IOHIDEventGetEvent((IOHIDEventRef)CFArrayGetValueAtIndex(children, i), kIOHIDEventTypeScroll);
      if (child && IOHIDEventGetEventFlags(child) & kIOHIDAccelerated) {
        accelScrollEvent = child;
        break;
      }
    }
  }

  IOHIDEventRef buttonEvent = IOHIDEventGetEvent (event, kIOHIDEventTypeButton);

  if (buttonEvent || pointerEvent) {
    __IOHIDPointerEventTranslatorProcessClickState (translator, &context, pointerEvent, buttonEvent);
  }
 
  if (scrollEvent) {
    __IOHIDPointerEventTranslatorProcessScrollCount (translator, &context, accelScrollEvent);
    
    translatedEvent = __IOHIDPointerEventTranslatorCreateScrollEvent (translator, &context, scrollEvent, accelScrollEvent);
    if (translatedEvent) {
      CFArrayAppendValue(eventCollection, translatedEvent);
      CFRelease(translatedEvent);
    }
  }
  
  if (pointerEvent) {
    __IOHIDPointerEventTranslatorProcessButtonState (translator, &context, pointerEvent);
    if (context.globalButtons != translator->globalButtons) {
      __IOHIDPointerEventTranslatorCreateSysdefineEvent (translator, &context);
      __IOHIDPointerEventTranslatorCreateMouseUpDownEvent (translator, &context, pointerEvent);
    }
    
    translatedEvent = __IOHIDPointerEventTranslatorCreateMouseMoveEvent (translator, &context, pointerEvent);
    if (translatedEvent) {
      CFArrayAppendValue(eventCollection, translatedEvent);
      CFRelease(translatedEvent);
    }
  
    if (IOHIDEventIsAbsolute(pointerEvent)) {
      context.serviceRecord->lastAbsoluteX = IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerX);
      context.serviceRecord->lastAbsoluteY = IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerY);
    }
  }
  translator->flags = context.flags;
  return eventCollection;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCreateHidEventForNXEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
#define __IOHIDPointerEventTranslatorCreateHidEventForNXEvent(nxEvent, hidEvent, usage) \
  IOHIDEventCreateVendorDefinedEvent (      \
  CFGetAllocator(hidEvent),                 \
  nxEvent.payload.time,                     \
  kHIDPage_AppleVendor,                     \
  usage,                                    \
  0,                                        \
  (uint8_t*)&nxEvent,                       \
  sizeof (nxEvent),                         \
  0                                         \
  )

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorInitNxEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorInitNxEvent (EVENT_TRANSLATOR_CONTEXT *context, NXEventExt *nxEvent, uint8_t type) {
  memset(nxEvent, 0, sizeof(*nxEvent));
  nxEvent->payload.service_id   = context->serviceid;
  nxEvent->payload.time         = context->timestamp;
  nxEvent->payload.type         = type;
  nxEvent->payload.flags        = context->flags;
  nxEvent->extension.flags      = NX_EVENT_EXTENSION_LOCATION_INVALID;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCreateMouseMoveEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEventRef __IOHIDPointerEventTranslatorCreateMouseMoveEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent) {
  
  NXEventExt    nxEvent;
  uint8_t       type;
  uint32_t      button = __IOHIDPointerEventTranslatorRemapButtons (translator, context, translator->globalButtons);
  
  if (button & 0x1) {
    type = NX_LMOUSEDRAGGED;
  } else if (button & 0x2) {
    type = NX_RMOUSEDRAGGED;
  } else {
    type = NX_MOUSEMOVED;
  }
  
  __IOHIDPointerEventTranslatorInitNxEvent (context, &nxEvent, type);

  float x = (float)IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerX);
  float y = (float)IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerY);
  
  if ((x == 0.0 && y == 0.0) ||
     (IOHIDEventIsAbsolute(pointerEvent) && x == context->serviceRecord->lastAbsoluteX && y == context->serviceRecord->lastAbsoluteY)) {
    return NULL;
  }
  
  if (IOHIDEventIsAbsolute(pointerEvent)) {
      *((float*)&nxEvent.payload.location.x) = x;
      *((float*)&nxEvent.payload.location.y) = y;
      nxEvent.extension.flags = NX_EVENT_EXTENSION_LOCATION_TYPE_FLOAT | NX_EVENT_EXTENSION_LOCATION_DEVICE_SCALED;
  } else {
      *((float*)&nxEvent.payload.data.mouseMove.dx) = x;
      *((float*)&nxEvent.payload.data.mouseMove.dy) = y;
      nxEvent.extension.flags |= NX_EVENT_EXTENSION_MOUSE_DELTA_TYPE_FLOAT;
  }
  if (context->serviceRecord->isMultiTouchService) {
    nxEvent.payload.data.mouseMove.subType = NX_SUBTYPE_MOUSE_TOUCH;
  }
  
  return __IOHIDPointerEventTranslatorCreateHidEventForNXEvent(nxEvent, pointerEvent, kHIDUsage_AppleVendor_NXEvent_Translated);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorProcessButtonState
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorProcessButtonState (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context __unused, IOHIDEventRef pointerEvent) {
  
  uint32_t  buttons        =  (uint32_t)IOHIDEventGetIntegerValue (pointerEvent, kIOHIDEventFieldPointerButtonMask);

  if (context->serviceRecord->buttons != buttons) {
    context->serviceRecord->buttons = buttons;
    translator->globalButtons =  IOHIDPointerEventTranslatorGetGlobalButtonState (translator);
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorRemapButtons
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
uint32_t __IOHIDPointerEventTranslatorRemapButtons (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, uint32_t buttons) {
  uint32_t result = buttons;
  
  if (context->serviceRecord->clickRemappingDisable) {
    return result;
  }
  
  if (translator->buttonMode == NX_OneButton) {
    result = ((buttons & 0x3) ? 0x1 : 0) | (buttons & (~0x3));
  } else if (context->serviceRecord->buttonCount > 1 && translator->buttonMode == NX_LeftButton) {
    result = ((buttons & 0x1) << 1) | ((buttons & 0x2) >> 1) |  (buttons & (~0x3));
  }
  
  return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorProcessClickState
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorProcessClickState (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent, IOHIDEventRef buttonEvent) {
  
  if (context->flags != translator->flags) {
    translator->clickCount = 0;
  }
    
  if (pointerEvent) {
    IOHIDFloat dx = IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerX);
    IOHIDFloat dy = IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerY);
    
    if (IOHIDEventIsAbsolute(pointerEvent)) {
        //@todo really  hack
        dx = (dx - context->serviceRecord->lastAbsoluteX) * kAbsoluteToPixelTranslation;
        dy = (dy - context->serviceRecord->lastAbsoluteY) * kAbsoluteToPixelTranslation;
    }
      
    if (translator->clickCount >= 1) {
      translator->clickCountDeltaX += dx;
      translator->clickCountDeltaY += dy;
      if ((fabs(translator->clickCountDeltaX) > (IOHIDFloat)translator->clickCountPixelTreshold / 2) ||
          (fabs(translator->clickCountDeltaY) > (IOHIDFloat)translator->clickCountPixelTreshold / 2)) {
          translator->clickCount = 0;
      }
    } else {
      translator->clickCountDeltaX = 0;
      translator->clickCountDeltaY = 0;
    }
    if (translator->scrollCount >= 1) {
      translator->scrollCountDeltaX += dx;
      translator->scrollCountDeltaY += dy;
      if ((fabs(translator->scrollCountDeltaX) > (IOHIDFloat)translator->clickCountPixelTreshold / 2) ||
          (fabs(translator->scrollCountDeltaY) > (IOHIDFloat)translator->clickCountPixelTreshold / 2)) {
        translator->scrollCount = 0;
      }
    } else {
      translator->scrollCountDeltaX = 0;
      translator->scrollCountDeltaY = 0;
    }
  }
    
  if (buttonEvent && IOHIDEventGetIntegerValue (buttonEvent, kIOHIDEventFieldButtonState)) {
    uint64_t eventTime = ABS_TO_NS(IOHIDEventGetTimeStamp(buttonEvent), translator->timebaseInfo) ;
    if ((uint64_t)llabs((int64_t)eventTime - (int64_t)translator->lastClickTime) < translator->clickCountTimeTreshold) {
      ++translator->clickCount;
    } else {
      translator->clickCount = 1;
    }
    translator->lastClickTime = eventTime;
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCreateSysdefineEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorCreateSysdefineEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context) {

  uint32_t      buttons;
  NXEventExt    nxEvent;
  IOHIDEventRef translated;
  
  buttons = translator->globalButtons ^ context->globalButtons;
  
  __IOHIDPointerEventTranslatorInitNxEvent (context, &nxEvent,  NX_SYSDEFINED);

  nxEvent.payload.data.compound.subType   = NX_SUBTYPE_AUX_MOUSE_BUTTONS;
  nxEvent.payload.data.compound.misc.L[0] = __IOHIDPointerEventTranslatorRemapButtons (translator, context, buttons);
  nxEvent.payload.data.compound.misc.L[1] = __IOHIDPointerEventTranslatorRemapButtons (translator, context, translator->globalButtons);
  
  translated = __IOHIDPointerEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event, kHIDUsage_AppleVendor_NXEvent_Translated);
  if (translated) {
      CFArrayAppendValue(context->eventCollection, translated);
      CFRelease(translated);
  }
  
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCreateMouseUpDownEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorCreateMouseUpDownEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef pointerEvent) {

  uint32_t      changedButtons;
  uint32_t      buttons;
  NXEventExt    nxEvent;
  IOHIDEventRef translated;
  
  changedButtons =  __IOHIDPointerEventTranslatorRemapButtons (translator, context, translator->globalButtons ^ context->globalButtons);
  if (changedButtons == 0) {
    return;
  }
  
  __IOHIDPointerEventTranslatorInitNxEvent (context, &nxEvent, 0);

  buttons = __IOHIDPointerEventTranslatorRemapButtons (translator, context, translator->globalButtons);
  
  nxEvent.payload.data.mouse.click = translator->clickCount;
  
  if (context->serviceRecord->isMultiTouchService) {
    nxEvent.payload.data.mouse.subType = NX_SUBTYPE_MOUSE_TOUCH;
  }

  if (IOHIDEventIsAbsolute(pointerEvent)) {
    *((float*)&nxEvent.payload.location.x) = (float)IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerX);
    *((float*)&nxEvent.payload.location.y) = (float)IOHIDEventGetFloatValue (pointerEvent, kIOHIDEventFieldPointerY);
    nxEvent.extension.flags = NX_EVENT_EXTENSION_LOCATION_TYPE_FLOAT | NX_EVENT_EXTENSION_LOCATION_DEVICE_SCALED;
  }

  if (changedButtons & 1) {
    if (buttons & 1) {
      nxEvent.payload.type = NX_LMOUSEDOWN;
      nxEvent.payload.data.mouse.pressure = 255;
      translator->lastLeftEventNum = __IOHIDPointerEventTranslatorGetUniqueEventNumber (translator);
      nxEvent.payload.data.mouse.eventNum = translator->lastLeftEventNum;
    } else {
      nxEvent.payload.type = NX_LMOUSEUP;
      nxEvent.payload.data.mouse.eventNum = translator->lastLeftEventNum;
      translator->lastLeftEventNum = NULLEVENTNUM;
    }
    translated =  __IOHIDPointerEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event, kHIDUsage_AppleVendor_NXEvent_Translated);
    if (translated) {
      CFArrayAppendValue(context->eventCollection, translated);
      CFRelease(translated);
    }
  }

  if (changedButtons & 2) {
    if (buttons & 2) {
      nxEvent.payload.type = NX_RMOUSEDOWN;
      nxEvent.payload.data.mouse.pressure = 255;
      translator->lastRightEventNum = __IOHIDPointerEventTranslatorGetUniqueEventNumber (translator);
      nxEvent.payload.data.mouse.eventNum = translator->lastRightEventNum;
    } else {
      nxEvent.payload.type = NX_RMOUSEUP;
      nxEvent.payload.data.mouse.eventNum = translator->lastRightEventNum;
      translator->lastRightEventNum = NULLEVENTNUM;
    }
    translated =  __IOHIDPointerEventTranslatorCreateHidEventForNXEvent(nxEvent, context->event, kHIDUsage_AppleVendor_NXEvent_Translated);
    if (translated) {
      CFArrayAppendValue(context->eventCollection, translated);
      CFRelease(translated);
    }
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorProcessScrollDelta
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
double __IOHIDPointerEventTranslatorGetScrollDelta (SERVICE_RECORD *record, int axisIndex, double value, double accelValue) {
  double result = accelValue;
  if (record->scrollConsume[axisIndex].countThreshold) {
    boolean_t direction = (value >= 0);
    
    if (record->scrollConsume[axisIndex].direction != direction) {
      record->scrollConsume[axisIndex].accumulator = 0.0;
      record->scrollConsume[axisIndex].count = 0;
    }
    
    record->scrollConsume[axisIndex].direction = direction;
    record->scrollConsume[axisIndex].accumulator += accelValue;
    record->scrollConsume[axisIndex].count += fabs (value);
    
    if (value &&
       (fabs (value) > (record->scrollConsume[axisIndex].clearThreshold) ||
        record->scrollConsume[axisIndex].count >= record->scrollConsume[axisIndex].countThreshold)) {
      
     double accum = record->scrollConsume[axisIndex].accumulator;
      result =  copysign (fabs(floor(accum * 10) / 10), accum);
      record->scrollConsume[axisIndex].count = 0;
      record->scrollConsume[axisIndex].accumulator = 0.0;
    } else {
      result = 0.0;
    }
  }
  result = (result && fabs(result) < 0.1) ? copysign(0.1, result) : result;
  return result;
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCreateScrollEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDEventRef __IOHIDPointerEventTranslatorCreateScrollEvent (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context, IOHIDEventRef scrollEvent, IOHIDEventRef accellScrollEvent) {
  NXEventExt  nxEvent;
  SInt32      eventType = NX_SCROLLWHEELMOVED;
  
  if (translator->zoomModifierMask) {
    bool isZoom = ((context->flags & kZoomModifierMask) == translator->zoomModifierMask);
    bool isMomentum = (0 != (TRANSLATE_SCROLL_MOMENTUM(scrollEvent) & kScrollTypeMomentumAny));
    if ((isMomentum && translator->zoomLastScrollWasZoom) || (isZoom && !isMomentum)) {
      translator->zoomLastScrollWasZoom = true;
      eventType = NX_ZOOM;
    } else {
      translator->zoomLastScrollWasZoom = false;
    }
  } else {
    translator->zoomLastScrollWasZoom = false;
  }

  __IOHIDPointerEventTranslatorInitNxEvent (context, &nxEvent, eventType);
  
  double scrollAccell [3];
  double scrollNonAccell [3];
  double consumeScroll  [3];
  
  static int fields[] = {kIOHIDEventFieldScrollY, kIOHIDEventFieldScrollX, kIOHIDEventFieldScrollZ};
  for (int index = 0; index < 3; index++) {
    scrollAccell[index]    = IOHIDEventGetFloatValue (accellScrollEvent, fields[index]);
    scrollNonAccell[index] = IOHIDEventGetFloatValue (scrollEvent, fields[index]);
    consumeScroll [index]  = __IOHIDPointerEventTranslatorGetScrollDelta (context->serviceRecord, index, scrollNonAccell[index], scrollAccell[index]);
  }
  
  if (consumeScroll[0]) {
    nxEvent.payload.data.scrollWheel.deltaAxis1 = (SInt16) (fabs(consumeScroll[0]) < 1 ? copysign (1, consumeScroll[0]) : copysign(fabs(consumeScroll[0]), consumeScroll[0]));
    nxEvent.payload.data.scrollWheel.fixedDeltaAxis1 = (SInt32)copysign(ceil(fabs(consumeScroll[0] * 65536)), consumeScroll[0]);
  }
  if (consumeScroll[1]) {
    nxEvent.payload.data.scrollWheel.deltaAxis2 = (SInt16) (fabs(consumeScroll[1]) < 1 ? copysign (1, consumeScroll[1]) : copysign(fabs(consumeScroll[1]), consumeScroll[1]));
    nxEvent.payload.data.scrollWheel.fixedDeltaAxis2 = (SInt32)copysign(ceil(fabs(consumeScroll[1] * 65536)), consumeScroll[1]);
  }
  if (consumeScroll[2]) {
    nxEvent.payload.data.scrollWheel.deltaAxis3 = (SInt16) (fabs(consumeScroll[2]) < 1 ? copysign (1, consumeScroll[2]) : copysign(fabs(consumeScroll[2]), consumeScroll[2]));
    nxEvent.payload.data.scrollWheel.fixedDeltaAxis3 = (SInt32)copysign(ceil(fabs(consumeScroll[2] * 65536)), consumeScroll[2]);
  }

  nxEvent.payload.data.scrollWheel.pointDeltaAxis1 =  copysign(ceil(fabs(scrollAccell[0]) * 10), scrollAccell[0]);
  nxEvent.payload.data.scrollWheel.pointDeltaAxis2 =  copysign(ceil(fabs(scrollAccell[1]) * 10), scrollAccell[1]);
  nxEvent.payload.data.scrollWheel.pointDeltaAxis3 =  copysign(ceil(fabs(scrollAccell[2]) * 10), scrollAccell[2]);
  
  nxEvent.payload.data.scrollWheel.reserved1 = TRANSLATE_SCROLL_MOMENTUM(scrollEvent);
  
  
  if (context->serviceRecord->isMultiTouchService) {
    nxEvent.payload.data.scrollWheel.reserved1 |= kScrollTypeTouch;
  }
  if (context->serviceRecord->isContinuousScroll) {
    nxEvent.payload.data.scrollWheel.reserved1 |= kScrollTypeContinuous;
  }
  
  nxEvent.payload.data.scrollWheel.reserved8[1] = translator->scrollCount;
  nxEvent.payload.data.scrollWheel.reserved8[2] = (SInt32)context->serviceid;
  
  return __IOHIDPointerEventTranslatorCreateHidEventForNXEvent(nxEvent, scrollEvent, kHIDUsage_AppleVendor_NXEvent_Translated);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorProcessScrollCount
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorProcessScrollCount (IOHIDPointerEventTranslatorRef translator, EVENT_TRANSLATOR_CONTEXT *context __unused, IOHIDEventRef scrollEvent) {

  uint32_t phaseAndMomentum = TRANSLATE_SCROLL_MOMENTUM(scrollEvent);
  
  if (phaseAndMomentum == 0) {
    return;
  }
  
  boolean_t checkSustain    = false;
  uint32_t  scrollDirection;
  
  double axis [3]     = {
    10 * IOHIDEventGetFloatValue (scrollEvent, kIOHIDEventFieldScrollX),
    10 * IOHIDEventGetFloatValue (scrollEvent, kIOHIDEventFieldScrollY),
    10 * IOHIDEventGetFloatValue (scrollEvent, kIOHIDEventFieldScrollZ)};
  double axisPow2 [3] = {axis[0] * axis[0],axis[1] * axis[1], axis[2] * axis[2]};
  
  double scrollMagnitudeSquared = axisPow2[0] + axisPow2[1] + axisPow2[2];
  
  int m = 0;
  (void)((axisPow2[m] < axisPow2[1]) && (m = 1));
  (void)((axisPow2[m] < axisPow2[2]) && (m = 2));
  
  scrollDirection = SIGN (axis[m]) * m;
  
  if (scrollDirection && translator->scrollDicrection != scrollDirection) {
    translator->scrollCount = 0;
    translator->scrollDicrection = scrollDirection;
  }
  
  uint64_t ts = IOHIDEventGetTimeStamp(scrollEvent);
 
  switch (phaseAndMomentum) {
    case kScrollTypeOptionPhaseBegan: {
      if (translator->scrollCount > 0) {
        if ((translator->lastScrollTime + translator->scrollCountMaxTimeDeltaBetween) > ts) {
          if (!translator->scrollIncrementThisPhase) {
            translator->scrollCount++;
            translator->scrollIncrementThisPhase = true;
          }
          translator->lastScrollSustainTime = ts;
        }
        else {
          translator->scrollCount = 0;
        }
      }
      break;
    }
   case kScrollTypeOptionPhaseChanged: {
      if (translator->scrollCount == 0) {
        if (scrollMagnitudeSquared >= translator->scrollCountMinDeltaToStartPow2) {
          translator->scrollCount = 1;
          translator->lastScrollSustainTime = ts;
        }
      }
      else {
        if (translator->scrollCount > 2) {
          translator->scrollCount +=  sqrt(scrollMagnitudeSquared) / translator->scrollCountAccelerationFactor;
          if (translator->scrollCount > translator->scrollCountMax) {
            translator->scrollCount = translator->scrollCountMax;
          }
        }
        checkSustain = true;
      }
      break;
    }
    case kScrollTypeOptionPhaseEnded: {
      if (translator->scrollCount > 0) {
        translator->lastScrollTime = ts;
        translator->scrollIncrementThisPhase = false;
      }
      break;
    }
    case kScrollTypeOptionPhaseCanceled: {
      translator->scrollIncrementThisPhase = false;
      translator->scrollCount = 0;
      break;
    }
    case kScrollTypeOptionPhaseMayBegin: {
      if (translator->scrollCount > 0) {
        if (((translator->lastScrollTime + translator->scrollCountMaxTimeDeltaBetween) > ts) &&
             !translator->scrollIncrementThisPhase) {
          translator->scrollCount++;
          translator->scrollIncrementThisPhase = true;
          translator->lastScrollSustainTime = ts;
        }
        else {
          translator->scrollCount = 0;
          translator->scrollIncrementThisPhase  = false;
        }
      }
      break;
    }
    case kScrollTypeMomentumStart: {
      // do nothing
      break;
    }
    case kScrollTypeMomentumContinue: {
      checkSustain = true;
      break;
    }
    case kScrollTypeMomentumEnd: {
      if (translator->scrollCount > 0) {
        translator->lastScrollTime = ts;
        translator->scrollIncrementThisPhase = false;
      }
      break;
    }
    default:
      HIDLogError ("SCROLLCOUNT: Unknown phase 0x%x", phaseAndMomentum);
  }
  if (checkSustain) {
    if (scrollMagnitudeSquared > translator->scrollCountMinDeltaToSustainPow2) {
      translator->lastScrollSustainTime = ts;
    }
    else if (translator->lastScrollSustainTime + translator->scrollCountMaxTimeDeltaToSustain < ts) {
      translator->scrollCount = 0;
    }
  }
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCreatePrivate
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
IOHIDPointerEventTranslatorRef __IOHIDPointerEventTranslatorCreatePrivate(CFAllocatorRef allocator, CFAllocatorContext * context __unused)
{
  IOHIDPointerEventTranslatorRef translator = NULL;
  void *                          offset  = NULL;
  uint32_t                        size;
  
  /* allocate service */
  size  = sizeof(__IOHIDPointerEventTranslator) - sizeof(CFRuntimeBase);
  translator = (IOHIDPointerEventTranslatorRef)_CFRuntimeCreateInstance(allocator, IOHIDPointerEventTranslatorGetTypeID(), size, NULL);
  
  if (!translator)
    return NULL;
  
  offset = translator;
  bzero(offset + sizeof(CFRuntimeBase), size);
  
  return translator;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDKeyboardEventTranslatorFree
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void __IOHIDPointerEventTranslatorFree( CFTypeRef object )
{
  IOHIDPointerEventTranslatorRef translator = (IOHIDPointerEventTranslatorRef) object;
  if (!translator) {
    return;
  }
  CFRelease(translator->serviceRecord);
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorCopyDebugDescription
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static CFStringRef __IOHIDPointerEventTranslatorCopyDebugDescription(CFTypeRef cf)
{
  //IOHIDPointerEventTranslatorRef translator = ( IOHIDPointerEventTranslatorRef ) cf;
  
  return CFStringCreateWithFormat(CFGetAllocator(cf), NULL, CFSTR("IOHIDPointerEventTranslatorRef"));
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __ButtonsApplierFunction
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static void __ButtonsApplierFunction(const void *key __unused, const void *value, void *context)
{
  uint32_t *globalButtons = (uint32_t*)context;
  SERVICE_RECORD *serviceRecord = (SERVICE_RECORD*)CFDataGetBytePtr ((CFDataRef)value);
  *globalButtons |= serviceRecord->buttons;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// IOHIDPointerEventTranslatorGetGlobalButtonState
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
uint32_t IOHIDPointerEventTranslatorGetGlobalButtonState (IOHIDPointerEventTranslatorRef translator) {
  uint32_t globalButtons = 0;
  CFDictionaryApplyFunction (translator->serviceRecord, __ButtonsApplierFunction, &globalButtons);
  return globalButtons;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorGetUniqueEventNumber
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
uint32_t __IOHIDPointerEventTranslatorGetUniqueEventNumber (IOHIDPointerEventTranslatorRef translator) {
  while (++translator->eventNumber == 0);
  return translator->eventNumber;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDEventTranslatorGetServiceIDForObject
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
static uint64_t __IOHIDEventTranslatorGetServiceIDForObject (CFTypeRef service) {
  uint64_t    result = 0;
  CFNumberRef registryId = NULL;
  if (CFGetTypeID(service) == IOHIDServiceGetTypeID ()) {
    registryId = IOHIDServiceGetRegistryID((IOHIDServiceRef) service);
  } else if (CFGetTypeID(service) == IOHIDServiceClientGetTypeID ()) {
    registryId = IOHIDServiceClientGetRegistryID((IOHIDServiceClientRef) service);
  } else {
    HIDLogError ("Unknown service object type");
  }
  if (registryId) {
    CFNumberGetValue(registryId, kCFNumberSInt64Type, &result);
  }
  return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDEventTranslatorCopyServiceProperty
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
CFTypeRef __IOHIDEventTranslatorCopyServiceProperty (CFTypeRef service, CFStringRef key) {
  CFTypeRef   result = NULL;
  if (CFGetTypeID(service) == IOHIDServiceGetTypeID ()) {
    result = IOHIDServiceCopyProperty((IOHIDServiceRef) service, key);
  } else if (CFGetTypeID(service) == IOHIDServiceClientGetTypeID ()) {
    result = IOHIDServiceClientCopyProperty((IOHIDServiceClientRef) service, key);
  } else {
    HIDLogError ("Unknown service object type");
  }
  return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDEventTranslatorGetBooleanProperty
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
boolean_t __IOHIDEventTranslatorGetBooleanProperty (CFTypeRef service, CFStringRef key, boolean_t defaultValue) {
  boolean_t result = defaultValue;
  CFTypeRef value = __IOHIDEventTranslatorCopyServiceProperty (service, key);
  if (value) {
    if (CFGetTypeID(value) == CFBooleanGetTypeID()) {
      result = CFBooleanGetValue(value);
    }
    CFRelease(value);
  }
  return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDEventTranslatorGetIntegerProperty
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
uint64_t __IOHIDEventTranslatorGetIntegerProperty (CFTypeRef service, CFStringRef key, uint64_t defaultValue) {
  uint64_t result = defaultValue;
  CFTypeRef value = __IOHIDEventTranslatorCopyServiceProperty (service, key);
  if (value) {
    if (CFGetTypeID(value) == CFNumberGetTypeID()) {
      CFNumberGetValue(value, kCFNumberSInt64Type, &result);
    }
    CFRelease(value);
  }
  return result;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDEventTranslatorGetServiceRecordForServiceID
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
SERVICE_RECORD * __IOHIDEventTranslatorGetServiceRecordForServiceID (IOHIDPointerEventTranslatorRef translator,  uint64_t serviceID) {
  CFDataRef record = CFDictionaryGetValue(translator->serviceRecord, (const void*) serviceID);
  if (record == NULL) {
    return NULL;
  }
  return (SERVICE_RECORD*)CFDataGetBytePtr (record);
}


//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorGetNxMouseEvents
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

NXEventExt * __IOHIDPointerEventTranslatorGetNxMouseEvents (IOHIDEventRef event) {
    if (IOHIDEventGetType(event) != kIOHIDEventTypeVendorDefined ||
        IOHIDEventGetIntegerValue(event, kIOHIDEventFieldVendorDefinedUsage) != kHIDUsage_AppleVendor_NXEvent) {
        return NULL;
    }
    NXEventExt  *nxEvent = NULL;
    CFIndex     eventLength = 0;
    IOHIDEventGetVendorDefinedData (event, (uint8_t**)&nxEvent, &eventLength);
    if (nxEvent) {
        if ((1 << nxEvent->payload.type) & kLegacyMouseEventsMask) {
            return nxEvent;
        }
    }
    return NULL;
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// __IOHIDPointerEventTranslatorProcessLegacyEvent
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void __IOHIDPointerEventTranslatorProcessLegacyEvent (IOHIDPointerEventTranslatorRef translator,EVENT_TRANSLATOR_CONTEXT *context __unused,  IOHIDEventRef legacyMouseEvent) {

    if (legacyMouseEvent) {
        NXEventExt * nxEvent  = __IOHIDPointerEventTranslatorGetNxMouseEvents (legacyMouseEvent);
        uint32_t eventMask = (1 << nxEvent->payload.type);
        
        if (nxEvent) {
            if (eventMask & (NX_LMOUSEDOWNMASK | NX_RMOUSEDOWNMASK)) {
                
                uint64_t eventTime = ABS_TO_NS(IOHIDEventGetTimeStamp(legacyMouseEvent), translator->timebaseInfo) ;
                
                if ((uint64_t)llabs((int64_t)eventTime - (int64_t)translator->lastClickTime) < translator->clickCountTimeTreshold) {
                    ++translator->clickCount;
                } else {
                    translator->clickCount = 1;
                }
                
                translator->lastClickTime = eventTime;
                
                nxEvent->payload.data.mouse.click = translator->clickCount;
            }
            if (eventMask & (NX_LMOUSEUPMASK | NX_RMOUSEUPMASK)) {
                
                nxEvent->payload.data.mouse.click = translator->clickCount;
                
            }
            
            switch (nxEvent->payload.type) {
            case NX_RMOUSEDOWN:
                    translator->lastRightEventNum = __IOHIDPointerEventTranslatorGetUniqueEventNumber (translator);
                    nxEvent->payload.data.mouse.eventNum = translator->lastRightEventNum;
                    break;
            case NX_RMOUSEUP:
                    nxEvent->payload.data.mouse.eventNum = translator->lastRightEventNum;
                    translator->lastRightEventNum = NULLEVENTNUM;
                    break;
            case NX_LMOUSEDOWN:
                    translator->lastLeftEventNum = __IOHIDPointerEventTranslatorGetUniqueEventNumber (translator);
                    nxEvent->payload.data.mouse.eventNum = translator->lastLeftEventNum;
                    break;
            case NX_LMOUSEUP:
                    nxEvent->payload.data.mouse.eventNum = translator->lastLeftEventNum;
                    translator->lastLeftEventNum = NULLEVENTNUM;
                    break;
            }
            
            if (eventMask & (NX_LMOUSEDRAGGEDMASK | NX_RMOUSEDRAGGEDMASK | NX_MOUSEMOVEDMASK)) {
                if (translator->clickCount >= 1) {
                    
                    translator->clickCountDeltaX += nxEvent->payload.data.mouseMove.dx;
                    translator->clickCountDeltaY += nxEvent->payload.data.mouseMove.dy;
                    
                    if ((fabs(translator->clickCountDeltaX) > (float)translator->clickCountPixelTreshold / 2) ||
                        (fabs(translator->clickCountDeltaY) > (float)translator->clickCountPixelTreshold / 2)) {
                        translator->clickCount = 0;
                    }
                    
                } else {
                    translator->clickCountDeltaX = 0;
                    translator->clickCountDeltaY = 0;
                }
            }
        }
    }
}

