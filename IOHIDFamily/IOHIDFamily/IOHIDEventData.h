/*
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_IOHIDEVENTDATA_H
#define _IOKIT_IOHIDEVENTDATA_H

#include <IOKit/IOTypes.h>
#include <IOKit/hid/IOHIDEventTypes.h>

#ifdef KERNEL
#include <IOKit/IOLib.h>

#define IOHIDEventRef IOHIDEvent *
#else
#include <IOKit/hid/IOHIDEvent.h>
typedef struct IOHIDEventData IOHIDEventData;
#endif

//==============================================================================
// IOHIDEventData Declarations
//==============================================================================

//@todo review 
#define IOHIDEVENT_BASE             \
    uint32_t            size;       \
    IOHIDEventType      type;       \
    uint32_t            options;    \
    uint8_t             depth;      \
    uint8_t             reserved[3];\

#define IOHIDAXISEVENT_BASE     \
    struct {                    \
        IOFixed x;              \
        IOFixed y;              \
        IOFixed z;              \
    } position;

typedef struct _IOHIDAxisEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
} IOHIDAxisEventData;

typedef struct _IOHIDMotionEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
    uint32_t    motionType;
    uint32_t    motionSubType;
    uint32_t    motionSequence;
} IOHIDMotionEventData;

typedef struct _IOHIDSwipeEventData {
    IOHIDEVENT_BASE;
    IOHIDAXISEVENT_BASE;
    IOHIDSwipeMask      swipeMask;      // legacy
    IOHIDGestureMotion  gestureMotion;  // horizontal, vertical, scale, rotate, tap, etc
    IOHIDGestureFlavor  flavor;         // event flavor for routing purposes
    IOFixed             progress;       // progress of gesture, as a fraction (1.0 = 100%)
}   IOHIDSwipeEventData;

struct IOHIDEventData {
    uint32_t size;
    IOHIDEventType type;
    uint32_t options;
    uint8_t depth;
    uint8_t reserved[3];
};
//@todo review 

#include <IOKit/hid/IOHIDEventStructDefs.h>

enum {
    kIOHIDEventOptionIgnore         = 0xf0000000,
    kIOHIDEventOptionIsRepeat       = 0x00010000,
    kIOHIDEventOptionIsZeroEvent    = 0x00800000,
};

enum {
    kIOHIDKeyboardIsRepeat          = kIOHIDEventOptionIsRepeat, // DEPRECATED
    kIOHIDKeyboardStickyKeyDown     = 0x00020000,
    kIOHIDKeyboardStickyKeyLocked   = 0x00040000,
    kIOHIDKeyboardStickyKeyUp       = 0x00080000,
    kIOHIDKeyboardStickyKeysOn      = 0x00200000,
    kIOHIDKeyboardStickyKeysOff     = 0x00400000
};


enum {
    kIOHIDTransducerRange               = 0x00010000,
    kIOHIDTransducerTouch               = 0x00020000,
    kIOHIDTransducerInvert              = 0x00040000,
    kIOHIDTransducerDisplayIntegrated   = 0x00080000
};

enum {
    kIOHIDDigitizerOrientationTypeTilt = 0,
    kIOHIDDigitizerOrientationTypePolar,
    kIOHIDDigitizerOrientationTypeQuality
};
typedef uint8_t IOHIDDigitizerOrientationType;


enum {
    kIOHIDAccelerated                   = 0x00010000,
};

enum {
    kIOHIDSymbolicHotKeyOptionIsCGSHotKey = 0x00010000,
};

/*!
 @typedef    IOHIDSystemQueueElement
 @abstract   Memory structure defining the layout of each event queue element
 @discussion The IOHIDEventQueueElement represents a portion of mememory in the
 new IOHIDEventQueue.  It is possible that a event queue element
 can contain multiple interpretations of a given event.  The first
 event is always considered the primary event.
 @field      timeStamp   Time at which event was dispatched
 @field      senderID    RegistryID of sending service
 @field      options     Options for further developement
 @field      eventCount  The number of events contained in this transaction
 @field      payload     Begining offset of contiguous mememory that contains the
 pertinent attribute and event data
 */
typedef struct __attribute__((packed)) _IOHIDSystemQueueElement {
    uint64_t        timeStamp;
    uint64_t        senderID;
    uint32_t        options;
    uint32_t        attributeLength;
    uint32_t        eventCount;
    uint8_t         payload[0];
} IOHIDSystemQueueElement;

#define kIOFixedNaN             ((IOFixed)0x80000000)
#define IOFixedIsNaN            ((value) == kIOFixedNaN)

#define CAST_FIXED_TO_FIXED(value)          (value)
#define CAST_INTEGER_TO_INTEGER(value)      (value)
#define CAST_DOUBLE_TO_DOUBLE(value)        (value)
#define CAST_INTEGER_TO_FIXED(value)        (((IOFixed)(value) != kIOFixedNaN) ? ((value) * 65536) : (kIOFixedNaN))
#define CAST_FIXED_TO_INTEGER(value)        (((value) != kIOFixedNaN) ? (value / 65536) : kIOFixedNaN)
#define CAST_DOUBLE_TO_INTEGER(value)       (value)
#define CAST_FIXED_TO_DOUBLE(value)         (((value) != kIOFixedNaN) ? ((value) / 65536.0) : (NAN))
#define CAST_DOUBLE_TO_FIXED(value)         ((value != value) ? kIOFixedNaN : (IOFixed)((value) * 65536.0))
#define CAST_INTEGER_TO_DOUBLE(value)       ((double)(value))
#define CAST_SHORTINTEGER_TO_FIXED(value)   ((value) * 65536)

#define IOHIDEventFieldEventType(field) ((field >> 16) & 0xffff)
#define IOHIDEventFieldOffset(field) (field & 0xffff)

#ifdef KERNEL
#define IOHIDEventGetEventWithOptions(event, type, options) event->getEvent(type, options)
#define GET_EVENTDATA(event)                                event->_data
#else
#define GET_EVENTDATA(event)                                event->eventData
#endif

//@todo review

#define  _IOHIDUnknowDefaultField(event, field)

#define IOHIDEventGetQueueElementSize(type,size)\
{                                               \
    IOHIDEventGetSize(type,size);               \
    size += sizeof(IOHIDSystemQueueElement);    \
}

#define SET_EVENT_VALUE(event, field, value, options, typeToken)                        \
{                                                                                       \
    IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);                      \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {    \
        IOHIDEventData * e = GET_EVENTDATA(ev);                                         \
        switch (fieldEvType) {                                                              \
            IOHIDEventSet ## typeToken ## FieldsMacro(e,key);                           \
        }                                                                               \
    }                                                                                   \
}


#define GET_EVENT_VALUE(event, field, value, options, typeToken)                        \
{                                                                                       \
    IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);                      \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {    \
        IOHIDEventData * e = GET_EVENTDATA(ev);                                         \
        switch (fieldEvType) {                                                              \
            IOHIDEventGet ## typeToken ## FieldsMacro(e,key);                           \
        }                                                                               \
    }                                                                                   \
}

#define GET_EVENT_DATA(event, field, value, options )                                   \
{                                                                                       \
    IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);                      \
    IOHIDEventRef   ev          = (fieldEvType == kIOHIDEventTypeNULL) ? event : NULL;  \
    if ( ev || (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {    \
        IOHIDEventData * e = GET_EVENTDATA(ev);                                         \
        switch (fieldEvType) {                                                              \
            IOHIDEventGetDataFieldsMacro(e,key);                                        \
        }                                                                               \
    }                                                                                   \
}

#define _IOHIDDigitizerGetSynthesizedFieldsAsIntegerMacro(event, field)                                                      \
    case kIOHIDEventFieldDigitizerEstimatedMask: {                                                                           \
        value  = (((IOHIDDigitizerEventData*)event)->eventMask & kIOHIDDigitizerEventEstimatedAltitude) ? kIOHIDDigitizerEventUpdateAltitudeMask : 0; \
        value |= (((IOHIDDigitizerEventData*)event)->eventMask & kIOHIDDigitizerEventEstimatedAzimuth) ? kIOHIDDigitizerEventUpdateAzimuthMask : 0;   \
        value |= (((IOHIDDigitizerEventData*)event)->eventMask & kIOHIDDigitizerEventEstimatedPressure) ? kIOHIDDigitizerEventUpdatePressureMask : 0; \
        break;                                                                                                               \
    }                                                                                                                        \
    _IOHIDUnknowDefaultField(event, field)

#define _IOHIDDigitizerSetSynthesizedFieldsAsIntegerMacro(event, field)                                                      \
    case kIOHIDEventFieldDigitizerEstimatedMask: {                                                                           \
        ((IOHIDDigitizerEventData*)event)->eventMask |= ((value & kIOHIDDigitizerEventUpdateAltitudeMask) ? kIOHIDDigitizerEventEstimatedAltitude : 0); \
        ((IOHIDDigitizerEventData*)event)->eventMask |= ((value & kIOHIDDigitizerEventUpdateAzimuthMask) ? kIOHIDDigitizerEventEstimatedAzimuth : 0); \
        ((IOHIDDigitizerEventData*)event)->eventMask |= ((value & kIOHIDDigitizerEventUpdatePressureMask) ? kIOHIDDigitizerEventEstimatedPressure : 0); \
        break;                                                                                                               \
    }                                                                                                                        \
    _IOHIDUnknowDefaultField(event, field)

#include <IOKit/hid/IOHIDEventMacroDefs.h>

#endif /* _IOKIT_HID_IOHIDEVENTDATA_H */
