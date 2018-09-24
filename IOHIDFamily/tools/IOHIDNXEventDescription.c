//
//  IOHIDNXEventDescription.c
//  IOHIDFamily
//
//  Created by YG on 9/21/15.
//
//

#include "IOHIDNXEventDescription.h"
#include <mach/mach_time.h>

CFStringRef NxEventCreateGenericDescription  (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateMouseDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateKeyboardDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateCompoundDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateScrollZoomDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateTabletDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateMouseMoveDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateTabletProximityDescription (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventCreateHeaderDescription  (NXEvent *event, NXEventExtension *ext);
CFStringRef NxEventGetTitleForType (SInt32 type);
CFStringRef NxEventCreateProximityDescription (NXTabletProximityData *proximity);
CFStringRef NxEventCreatePointDataDescription (NXTabletPointData *point);
CFStringRef __NxEventCreateDescription (NXEvent *event, NXEventExtension *ext);

typedef CFStringRef (*NXEVENT_DESCRIPTION_FUNC) (NXEvent *event, NXEventExtension *ext);

static CFStringRef __nxEventDescription [] = {
  [NX_NULLEVENT]      = CFSTR("NX_NULLEVENT"),
  [NX_LMOUSEDOWN]     = CFSTR("NX_LMOUSEDOWN"),
  [NX_LMOUSEUP]       = CFSTR("NX_LMOUSEUP"),
  [NX_RMOUSEDOWN]     = CFSTR("NX_RMOUSEDOWN"),
  [NX_RMOUSEUP]       = CFSTR("NX_RMOUSEUP"),
  [NX_MOUSEMOVED]     = CFSTR("NX_MOUSEMOVED"),
  [NX_LMOUSEDRAGGED]  = CFSTR("NX_LMOUSEDRAGGED"),
  [NX_RMOUSEDRAGGED]  = CFSTR("NX_RMOUSEDRAGGED"),
  [NX_MOUSEENTERED]   = CFSTR("NX_MOUSEENTERED"),
  [NX_MOUSEEXITED]    = CFSTR("NX_MOUSEEXITED"),

  [NX_KEYDOWN]        = CFSTR("NX_KEYDOWN"),
  [NX_KEYUP]          = CFSTR("NX_KEYUP"),

  [NX_FLAGSCHANGED]   = CFSTR("NX_FLAGSCHANGED"),

  [NX_KITDEFINED]     = CFSTR("NX_KITDEFINED"),
  [NX_SYSDEFINED]     = CFSTR(" NX_SYSDEFINED"),
  [NX_APPDEFINED]     = CFSTR("NX_APPDEFINED"),

  [NX_SCROLLWHEELMOVED] = CFSTR("NX_SCROLLWHEELMOVED"),

  [NX_OMOUSEDOWN]     = CFSTR("NX_OMOUSEDOWN"),
  [NX_OMOUSEUP]       = CFSTR("NX_OMOUSEUP"),
  [NX_OMOUSEDRAGGED]  = CFSTR("NX_OMOUSEDRAGGED"),

  [NX_TABLETPOINTER]  = CFSTR("NX_TABLETPOINTER"),
  [NX_TABLETPROXIMITY]= CFSTR("NX_TABLETPROXIMITY"),
  
  [NX_ZOOM]           = CFSTR("NX_ZOOM")
};

static NXEVENT_DESCRIPTION_FUNC __nxEventDescriptionHelper[] = {
  [NX_NULLEVENT]      = NxEventCreateGenericDescription,
  [NX_LMOUSEDOWN]     = NxEventCreateMouseDescription,
  [NX_LMOUSEUP]       = NxEventCreateMouseDescription,
  [NX_RMOUSEDOWN]     = NxEventCreateMouseDescription,
  [NX_RMOUSEUP]       = NxEventCreateMouseDescription,
  [NX_MOUSEMOVED]     = NxEventCreateMouseMoveDescription,
  [NX_LMOUSEDRAGGED]  = NxEventCreateMouseMoveDescription,
  [NX_RMOUSEDRAGGED]  = NxEventCreateMouseMoveDescription,
  [NX_MOUSEENTERED]   = NxEventCreateMouseDescription,
  [NX_MOUSEEXITED]    = NxEventCreateMouseDescription,

  [NX_KEYDOWN]        = NxEventCreateKeyboardDescription,
  [NX_KEYUP]          = NxEventCreateKeyboardDescription,

  [NX_FLAGSCHANGED]   = NxEventCreateKeyboardDescription,


  [NX_KITDEFINED]     = NxEventCreateGenericDescription,
  [NX_SYSDEFINED]     = NxEventCreateCompoundDescription,
  [NX_APPDEFINED]     = NxEventCreateGenericDescription,

  [NX_SCROLLWHEELMOVED] = NxEventCreateScrollZoomDescription,

  [NX_TABLETPOINTER]    = NxEventCreateTabletDescription,
  [NX_TABLETPROXIMITY]  = NxEventCreateTabletProximityDescription,

  [NX_OMOUSEDOWN]     = NxEventCreateMouseDescription,
  [NX_OMOUSEUP]       = NxEventCreateMouseDescription,
  [NX_OMOUSEDRAGGED]  = NxEventCreateMouseMoveDescription,

  [NX_ZOOM]           = NxEventCreateScrollZoomDescription,
};


CFStringRef NxEventExtCreateDescription (NXEventExt *event) {
    CFStringRef eventDescription = __NxEventCreateDescription (&(event->payload), &(event->extension));
    CFStringRef result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                  CFSTR("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
                                                         "%@"
                                                         "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
                                                         ),
                                                         eventDescription
                                                  );
    if (eventDescription) {
        CFRelease(eventDescription);
    }
    return result;
}

CFStringRef NxEventCreateDescription (NXEvent *event) {
    CFStringRef eventDescription = __NxEventCreateDescription (event, NULL);
    CFStringRef result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                  CFSTR("+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
                                                        "%@"
                                                        "+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n"
                                                        ),
                                                  eventDescription
                                                  );
    if (eventDescription) {
        CFRelease(eventDescription);
    }
    return result;
}

CFStringRef __NxEventCreateDescription (NXEvent *event, NXEventExtension *ext) {
    CFStringRef header      = NULL;
    CFStringRef data        = NULL;
    CFStringRef extension   = NULL;
    
    CFStringRef title =  NxEventGetTitleForType (event->type);
    header = NxEventCreateHeaderDescription(event, ext);
    if (title != NULL) {
        data = __nxEventDescriptionHelper[event->type] (event, ext);
    }
    if (ext) {
        extension = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                             CFSTR(
                                             "NXEventExtension\n"
                                             "flags      : 0x%x\n"
                                             "audit      : 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n"
                                             ),
                                             (unsigned int)ext->flags,
                                             ext->audit.val[0], ext->audit.val[1], ext->audit.val[2], ext->audit.val[3], ext->audit.val[4], ext->audit.val[5], ext->audit.val[6], ext->audit.val[7]
                                             );
    }
    
    CFStringRef result = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                                  CFSTR(
                                                  "%@"
                                                  "%@"
                                                  "%@"
                                                  ),
                                                  header,
                                                  data,
                                                  extension);
    if (data) {
        CFRelease(data);
    }
    if (header) {
        CFRelease(header);
    }
    if (extension) {
        CFRelease(extension);
    }
    return result;
}

CFStringRef NxEventGetTitleForType  (SInt32 type) {
    if (type < 0 || type >= (SInt32)(sizeof(__nxEventDescriptionHelper) / sizeof(__nxEventDescriptionHelper[0])) || __nxEventDescriptionHelper[type] == NULL) {
        return NULL;
    }
    return __nxEventDescription[type];
}

static mach_timebase_info_data_t    timeBaseinfo;

CFStringRef NxEventCreateHeaderDescription  (NXEvent *event, NXEventExtension *ext) {
    if (timeBaseinfo.denom==0) {
        mach_timebase_info(&timeBaseinfo);
    }
    CFStringRef title = NxEventGetTitleForType (event->type);
    if (ext && ext->flags & NX_EVENT_EXTENSION_LOCATION_TYPE_FLOAT) {
        return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                        CFSTR(
                                              "NXEvent type : %@\n"
                                              "type         : %d\n"
                                              "location.x   : %f\n"
                                              "location.y   : %f\n"
                                              "time         : %llu (%llu us)\n"
                                              "flags        : 0x%x\n"
                                              "window       : 0x%x\n"
                                              "service_id   : 0x%llx\n"
                                              "ext_pid      : %d\n"
                                              ),
                                        title == NULL ? CFSTR ("UNKNOWN") : title,
                                        (int)event->type,
                                        *((float*)&event->location.x),
                                        *((float*)&event->location.y),
                                        event->time, (uint64_t)((event->time * timeBaseinfo.numer) / (timeBaseinfo.denom * 1000)),
                                        (int)event->flags,
                                        (unsigned int)event->window,
                                        event->service_id,
                                        (int)event->ext_pid
                                        );
    }
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                    CFSTR(
                                      "NXEvent type : %@\n"
                                      "type         : %d\n"
                                      "location.x   : %d\n"
                                      "location.y   : %d\n"
                                      "time         : %llu (%llu us)\n"
                                      "flags        : 0x%x\n"
                                      "window       : 0x%x\n"
                                      "service_id   : 0x%llx\n"
                                      "ext_pid      : %d\n"
                                    ),
                                    title == NULL ? CFSTR ("UNKNOWN") : title,
                                    (int)event->type,
                                    (int)event->location.x,
                                    (int)event->location.y,
                                    event->time, (uint64_t)((event->time * timeBaseinfo.numer) / (timeBaseinfo.denom * 1000)),
                                    (int)event->flags,
                                    (unsigned int)event->window,
                                    event->service_id,
                                    (int)event->ext_pid
                                    );
}

CFStringRef NxEventCreateGenericDescription  (NXEvent *event __unused, NXEventExtension *ext __unused) {
  return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          ""
          )
          );
}


CFStringRef NxEventCreateMouseDescription  (NXEvent *event, NXEventExtension *ext __unused) {
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          "NXEvent.NXEventData.mouse:\n"
          "  subx         :  %d\n"
          "  suby         :  %d\n"
          "  eventNum     :  %d\n"
          "  click        :  %d\n"
          "  pressure     :  %d\n"
          "  buttonNumber :  %d\n"
          "  subType      :  %d\n"
          ),
          event->data.mouse.subx,
          event->data.mouse.suby,
          event->data.mouse.eventNum,
          (int)event->data.mouse.click,
          event->data.mouse.pressure,
          event->data.mouse.buttonNumber,
          event->data.mouse.subType
          );
}

CFStringRef NxEventCreateKeyboardDescription (NXEvent *event, NXEventExtension *ext __unused) {
  return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          "NXEvent.NXEventData.key:\n"
          "  origCharSet  :  %d\n"
          "  repeat       :  %d\n"
          "  charSet      :  %d\n"
          "  charCode     :  %d\n"
          "  keyCode      :  %d\n"
          "  origCharCode :  %d\n"
          "  keyboardType :  %d\n"
          ),
          event->data.key.origCharSet,
          event->data.key.repeat,
          event->data.key.charSet,
          event->data.key.charCode,
          event->data.key.keyCode,
          event->data.key.origCharCode,
          (unsigned int)event->data.key.keyboardType
          );
}

CFStringRef NxEventCreateCompoundDescription (NXEvent *event, NXEventExtension *ext __unused) {
  if (event->data.compound.subType == NX_SUBTYPE_AUX_CONTROL_BUTTONS) {
    return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
              CFSTR(
                "NXEvent.NXEventData.compound:\n"
                "  subType            :  %d\n"
                "  compound.misc.L[0] :  0x%x\n"
                "    flavor           :  0x%x\n"
                "    eventType        :  0x%x\n"
                "    repeat           :  0x%x\n"
                "  compound.misc.L[1] :  0x%x\n"
                "  compound.misc.L[2] :  0x%x\n"
                ),
                event->data.compound.subType,
                (int)event->data.compound.misc.L[0],
                (int)(event->data.compound.misc.L[0] >> 16),
                (unsigned int)(event->data.compound.misc.L[0] >> 8) & 0xff,
                (unsigned int)event->data.compound.misc.L[0] & 0xff,
                (int)event->data.compound.misc.L[1],
                (int)event->data.compound.misc.L[2]
                );
  }
    if (event->data.compound.subType == NX_SUBTYPE_AUX_MOUSE_BUTTONS) {
        return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                                        CFSTR(
                                              "NXEvent.NXEventData.compound:\n"
                                              "  subType            :  %d\n"
                                              "  compound.misc.L[0] :  0x%x\n"
                                              "    hwDelta          :  0x%x\n"
                                              "  compound.misc.L[1] :  0x%x\n"
                                              "    hwButtons        :  0x%x\n"
                                              ),
                                        event->data.compound.subType,
                                        (int)event->data.compound.misc.L[0],
                                        (int)event->data.compound.misc.L[0],
                                        (int)event->data.compound.misc.L[1],
                                        (int)event->data.compound.misc.L[1]
                                        );
    }

    
  return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          "NXEvent.NXEventData.compound:\n"
          "  subType  :  %d\n"
          ),
          event->data.compound.subType
          );
}

CFStringRef NxEventCreateScrollZoomDescription (NXEvent *event, NXEventExtension *ext __unused) {

  return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          "NXEvent.NXEventData.scrollWheel(zoom):\n"
          "  deltaAxis1      :  %d\n"
          "  deltaAxis2      :  %d\n"
          "  deltaAxis3      :  %d\n"
          "  fixedDeltaAxis1 :  %d\n"
          "  fixedDeltaAxis2 :  %d\n"
          "  fixedDeltaAxis3 :  %d\n"
          "  pointDeltaAxis1 :  %d\n"
          "  pointDeltaAxis2 :  %d\n"
          "  pointDeltaAxis3 :  %d\n"
          "  reserved1       :  0x%08x (options)\n"
          "  reserved8[0]    :  0x%08x (phase annotation)\n"
          "  reserved8[1]    :  0x%08x (scroll count)\n"
          ),
          event->data.scrollWheel.deltaAxis1,
          event->data.scrollWheel.deltaAxis2,
          event->data.scrollWheel.deltaAxis3,
          (int)event->data.scrollWheel.fixedDeltaAxis1,
          (int)event->data.scrollWheel.fixedDeltaAxis2,
          (int)event->data.scrollWheel.fixedDeltaAxis3,
          (int)event->data.scrollWheel.pointDeltaAxis1,
          (int)event->data.scrollWheel.pointDeltaAxis2,
          (int)event->data.scrollWheel.pointDeltaAxis3,
          event->data.scrollWheel.reserved1,
          (int)event->data.scrollWheel.reserved8[0],
          (int)event->data.scrollWheel.reserved8[1]
                                  
          );
}

CFStringRef NxEventCreateTabletDescription (NXEvent *event, NXEventExtension *ext __unused) {
 CFStringRef point  = NxEventCreatePointDataDescription ((NXTabletPointData*)&event->data.tablet);
 CFStringRef description = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                            CFSTR(
                              "NXEvent.NXEventData.tablet:\n"
                              "%@"
                            ),
                            point
                            );
 
 if (point) {
   CFRelease(point);
 }
 return description;
}

CFStringRef NxEventCreateTabletProximityDescription (NXEvent *event, NXEventExtension *ext __unused) {
 CFStringRef proximity  = NxEventCreateProximityDescription ((NXTabletProximityData*)&event->data.proximity);
 CFStringRef description = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
                            CFSTR(
                              "NXEvent.NXEventData.proximity:\n"
                              "%@"
                            ),
                            proximity
                            );
  
 if (proximity) {
   CFRelease(proximity);
 }
 return description;
}

CFStringRef NxEventCreateMouseMoveDescription (NXEvent *event, NXEventExtension *ext) {
 
    if (ext && ext->flags & NX_EVENT_EXTENSION_MOUSE_DELTA_TYPE_FLOAT) {
      return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
             CFSTR(
               "NXEvent.NXEventData.mouseMove:\n"
              "  dx       :  %f\n"
              "  dy       :  %f\n"
              "  subx     :  %d\n"
              "  suby     :  %d\n"
              "  subType  :  %d\n"
              ),
              *((float*)&event->data.mouseMove.dx),
              *((float*)&event->data.mouseMove.dy),
              event->data.mouseMove.subx,
              event->data.mouseMove.suby,
              event->data.mouseMove.subType
              );
    } else {
     return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
            CFSTR(
              "NXEvent.NXEventData.mouseMove:\n"
              "  dx       :  %d\n"
              "  dy       :  %d\n"
              "  subx     :  %d\n"
              "  suby     :  %d\n"
              "  subType  :  %d\n"
              ),
              (int)event->data.mouseMove.dx,
              (int)event->data.mouseMove.dy,
              event->data.mouseMove.subx,
              event->data.mouseMove.suby,
              event->data.mouseMove.subType
              );
    }
}

CFStringRef NxEventCreateProximityDescription (NXTabletProximityData *proximity) {

 return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          "NXEvent.NXEventData.proximity:\n"
          "  vendorID            :  0x%x\n"
          "  tabletID            :  0x%x\n"
          "  pointerID           :  %d\n"
          "  deviceID            :  0x%x\n"
          "  systemTabletID      :  %d\n"
          "  vendorPointerType   :  %d\n"
          "  pointerSerialNumber :  %d\n"
          "  uniqueID            :  0x%llx\n"
          "  capabilityMask      :  0x%x\n"
          "  pointerType         :  %d\n"
          "  enterProximity      :  %d\n"
          ),
          proximity->vendorID,
          proximity->tabletID,
          proximity->pointerID,
          proximity->deviceID,
          proximity->systemTabletID,
          proximity->vendorPointerType,
          (unsigned int)proximity->pointerSerialNumber,
          proximity->uniqueID,
          (unsigned int)proximity->capabilityMask,
          proximity->pointerType,
          proximity->enterProximity
          );
}

CFStringRef NxEventCreatePointDataDescription (NXTabletPointData *point) {

 return CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
        CFSTR(
          "  x                  :  %d\n"
          "  y                  :  %d\n"
          "  z                  :  %d\n"
          "  buttons            :  %d\n"
          "  pressure           :  %d\n"
          "  tilt.x             :  %d\n"
          "  tilt.y             :  %d\n"
          "  rotation           :  %d\n"
          "  tangentialPressure :  %d\n"
          "  deviceID           :  %d\n"
          "  vendor1            :  %d\n"
          "  vendor2            :  %d\n"
          "  vendor3            :  %d\n"
          ),
          (int)point->x,
          (int)point->y,
          (int)point->z,
          point->buttons,
          point->pressure,
          point->tilt.x,
          point->tilt.y,
          point->rotation,
          point->tangentialPressure,
          point->deviceID,
          point->vendor1,
          point->vendor2,
          point->vendor3
          );
}
