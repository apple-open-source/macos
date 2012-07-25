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

#ifndef _IOKIT_HID_IOHIDEVENTDATA_H
#define _IOKIT_HID_IOHIDEVENTDATA_H

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

#define IOHIDEVENT_BASE             \
    uint32_t            size;       \
    IOHIDEventType      type;       \
    uint64_t            timestamp;  \
    uint32_t            options;    \
    uint8_t             depth;      \
    uint8_t             reserved[3];\
    uint64_t            deviceID;

/*!
    @typedef    IOHIDEventData
    @abstract   
    @discussion 
    @field      size        Size, in bytes, of the memory detailing this
                            particular event
    @field      type        Type of this particular event
    @field      options     Event specific options
    @field      deviceID    ID of the sending device
*/

struct IOHIDEventData{
    IOHIDEVENT_BASE;
};

typedef struct _IOHIDVendorDefinedEventData {
    IOHIDEVENT_BASE;
    uint16_t        usagePage;
    uint16_t        usage;
    uint32_t        version;
    uint32_t        length;
    uint8_t         data[0];
} IOHIDVendorDefinedEventData;

enum {
    kIOHIDKeyboardIsRepeat      = 0x00010000
};

typedef struct _IOHIDKeyboardEventData {
    IOHIDEVENT_BASE;                            // options = kHIDKeyboardRepeat
    uint16_t        usagePage;
    uint16_t        usage;
    boolean_t       down;
} IOHIDKeyboardEventData;

enum {
    kIOHIDEventOptionIgnore     = 0xf0000000
};

enum {
    kIOHIDTransducerRange       = 0x00010000,
    kIOHIDTransducerTouch       = 0x00020000,
    kIOHIDTransducerInvert      = 0x00040000,
};

enum {
    kIOHIDDigitizerOrientationTypeTilt = 0,
    kIOHIDDigitizerOrientationTypePolar,
    kIOHIDDigitizerOrientationTypeQuality
};
typedef uint8_t IOHIDDigitizerOrientationType;

#define IOHIDAXISEVENT_BASE     \
    struct {                    \
        IOFixed x;              \
        IOFixed y;              \
        IOFixed z;              \
    } position;

typedef struct _IOHIDAxisEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
} IOHIDAxisEventData, IOHIDTranslationData, IOHIDRotationEventData, IOHIDScrollEventData, IOHIDScaleEventData, IOHIDVelocityData, IOHIDOrientationEventData;

typedef struct _IOHIDAccelerometerEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
    uint32_t        acclType;
	uint32_t		acclSubType;
} IOHIDAccelerometerEventData;

typedef struct _IOHIDGyroEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
	uint32_t        gyroType;
	uint32_t		gyroSubType;
} IOHIDGyroEventData;

typedef struct _IOHIDCompassEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    IOHIDAXISEVENT_BASE;
	uint32_t        compassType;
} IOHIDCompassEventData;

typedef struct _IOHIDAmbientLightSensorEventData {
    IOHIDEVENT_BASE;                            // options = kHIDAxisRelative
    uint32_t        level;
    uint32_t        ch0;
    uint32_t        ch1;
    uint32_t        ch2;
    uint32_t        ch3;
    Boolean         brightnessChanged;
} IOHIDAmbientLightSensorEventData;

typedef struct _IOHIDTemperatureEventData {
    IOHIDEVENT_BASE;                            
    IOFixed        level;
} IOHIDTemperatureEventData;

typedef struct _IOHIDProximityEventData {
    IOHIDEVENT_BASE;                            
    uint32_t        detectionMask;
    uint32_t        level;
} IOHIDProximityEventData;

typedef struct _IOHIDProgressEventData {
    IOHIDEVENT_BASE;
    uint32_t        eventType;
    IOFixed         level;
} IOHIDProgressEventData;

typedef struct _IOHIDZoomToggleEventData {
    IOHIDEVENT_BASE;
} IOHIDZoomToggleEventData;

#define IOHIDBUTTONEVENT_BASE           \
    struct {                            \
        uint32_t        buttonMask;     \
        IOFixed         pressure;       \
        uint8_t         buttonNumber;   \
        uint8_t         clickState;     \
    } button;

typedef struct _IOHIDButtonEventData {
    IOHIDEVENT_BASE;
    IOHIDBUTTONEVENT_BASE;
} IOHIDButtonEventData;

typedef struct _IOHIDMouseEventData {
    IOHIDEVENT_BASE;
    IOHIDAXISEVENT_BASE;
    IOHIDBUTTONEVENT_BASE;
} IOHIDMouseEventData;


typedef struct _IOHIDDigitizerEventData {
    IOHIDEVENT_BASE;                            // options = kIOHIDTransducerRange, kHIDTransducerTouch, kHIDTransducerInvert
    IOHIDAXISEVENT_BASE;

    uint32_t        transducerIndex;   
    uint32_t        transducerType;				// could overload this to include that both the hand and finger id.
    uint32_t        identity;                   // Specifies a unique ID of the current transducer action.
    uint32_t        eventMask;                  // the type of event that has occurred: range, touch, position
    uint32_t        childEventMask;             // CHILD: the type of event that has occurred: range, touch, position
    
    uint32_t        buttonMask;                 // Bit field representing the current button state
                                                // Pressure field are assumed to be scaled from 0.0 to 1.0
    IOFixed         tipPressure;                // Force exerted against the digitizer surface by the transducer.
    IOFixed         barrelPressure;             // Force exerted directly by the user on a transducer sensor.
    
    IOFixed         twist;                      // Specifies the clockwise rotation of the cursor around its own major axis.  Unsure it the device should declare units via properties or event.  My first inclination is force degrees as the is the unit already expected by AppKit, Carbon and OpenGL.
    uint32_t        orientationType;            // Specifies the orientation type used by the transducer.
    union {
        struct {                                // X Tilt and Y Tilt are used together to specify the tilt away from normal of a digitizer transducer. In its normal position, the values of X Tilt and Y Tilt for a transducer are both zero.
            IOFixed     x;                      // This quantity is used in conjunction with Y Tilt to represent the tilt away from normal of a transducer, such as a stylus. The X Tilt value represents the plane angle between the Y-Z plane and the plane containing the transducer axis and the Y axis. A positive X Tilt is to the right. 
            IOFixed     y;                      // This value represents the angle between the X-Z and transducer-X planes. A positive Y Tilt is toward the user.
        } tilt;
        struct {                                // X Tilt and Y Tilt are used together to specify the tilt away from normal of a digitizer transducer. In its normal position, the values of X Tilt and Y Tilt for a transducer are both zero.
            IOFixed  altitude;                  //The angle with the X-Y plane though a signed, semicicular range.  Positive values specify an angle downward and toward the positive Z axis. 
            IOFixed  azimuth;                   // Specifies the counter clockwise rotation of the cursor around the Z axis though a full circular range.
        } polar;
        struct {
            IOFixed  quality;                    // If set, indicates that the transducer is sensed to be in a relatively noise-free region of digitizing.
            IOFixed  density;
            IOFixed  irregularity;
            IOFixed  majorRadius;                // units in mm
            IOFixed  minorRadius;                // units in mm
        } quality;
    }orientation;
} IOHIDDigitizerEventData;

typedef struct _IOHIDSwipeEventData {
    IOHIDEVENT_BASE;                            
    IOHIDSwipeMask      swipeMask;      // legacy
    IOHIDGestureMotion  gestureMotion;  // horizontal, vertical, scale, rotate, tap, etc
    IOHIDGestureFlavor  flavor;         // event flavor for routing purposes
    IOFixed             progress;       // progress of gesture, as a fraction (1.0 = 100%)
    IOFixed             positionX;      // delta in position of gesture on Horizontal axis. Espressed as a fraction (1.0 = 100%)
    IOFixed             positionY;      // delta in position of gesture on Vertical axis. Espressed as a fraction (1.0 = 100%)
}   IOHIDSwipeEventData, 
    IOHIDNavagationSwipeEventData, 
    IOHIDDockSwipeEventData, 
    IOHIDFluidTouchGestureData,
    IOHIDBoundaryScrollData;

typedef struct _IOHIDSymbolicHotKeyEventData {
    IOHIDEVENT_BASE;
    uint32_t    hotKey;
} IOHIDSymbolicHotKeyEventData;

enum {
    kIOHIDSymbolicHotKeyOptionIsCGSHotKey = 0x00010000,
};

typedef struct _IOHIDPowerEventData {
    IOHIDEVENT_BASE;
    IOFixed         measurement;
    uint32_t        powerType;
    uint32_t        powerSubType;
} IOHIDPowerEventData;

typedef struct _IOHIDBrightnessEventData {
    IOHIDEVENT_BASE;                            
    IOFixed         level;      // 0..1 value, S space
} IOHIDBrightnessEventData;

/*!
    @typedef    IOHIDSystemQueueElement
    @abstract   Memory structure defining the layout of each event queue element
    @discussion The IOHIDEventQueueElement represents a portion of mememory in the
                new IOHIDEventQueue.  It is possible that a event queue element
                can contain multiple interpretations of a given event.  The first
                event is always considered the primary event.
    @field      version     Version of the event queue element
    @field      size        Size, in bytes, of this particular event queue element
    @field      timeStamp   Time at which event was dispatched
    @field      options     Options for further developement
    @field      eventCount  The number of events contained in this transaction
    @field      events      Begining offset of contiguous mememory that contains the
                            pertinent event data
*/
typedef struct _IOHIDSystemQueueElement {
    uint64_t        timeStamp;
    uint32_t        options;
    uint32_t        eventCount;
    IOHIDEventData  events[];
} IOHIDSystemQueueElement; 

//******************************************************************************
// MACROS
//******************************************************************************

#define IOHIDEventFieldEventType(field) ((field >> 16) & 0xffff)
#define IOHIDEventFieldOffset(field) (field & 0xffff)
#define IOHIDEventGetSize(type,size)    \
{                                       \
    switch ( type ) {                   \
        case kIOHIDEventTypeNULL:       \
        case kIOHIDEventTypeReset:      \
            size = sizeof(IOHIDEventData);\
            break;                      \
        case kIOHIDEventTypeVendorDefined:\
            size = sizeof(IOHIDVendorDefinedEventData);\
            break;                      \
        case kIOHIDEventTypeKeyboard:   \
            size = sizeof(IOHIDKeyboardEventData);\
            break;                      \
        case kIOHIDEventTypeTranslation:\
        case kIOHIDEventTypeRotation:   \
        case kIOHIDEventTypeScroll:     \
        case kIOHIDEventTypeScale:      \
        case kIOHIDEventTypeVelocity:   \
        case kIOHIDEventTypeOrientation:\
            size = sizeof(IOHIDAxisEventData);\
            break;                      \
        case kIOHIDEventTypeAccelerometer:\
            size = sizeof(IOHIDAccelerometerEventData);\
            break;\
		case kIOHIDEventTypeGyro:\
			size = sizeof(IOHIDGyroEventData);\
			break;\
		case kIOHIDEventTypeCompass:\
            size = sizeof(IOHIDCompassEventData);\
            break;                      \
        case kIOHIDEventTypeAmbientLightSensor:\
            size = sizeof(IOHIDAmbientLightSensorEventData);\
            break;                      \
        case kIOHIDEventTypeProximity:  \
            size = sizeof(IOHIDProximityEventData);\
            break;                      \
        case kIOHIDEventTypeButton:     \
            size = sizeof(IOHIDButtonEventData);\
            break;                      \
        case kIOHIDEventTypeDigitizer:  \
            size = sizeof(IOHIDDigitizerEventData);\
            break;                      \
        case kIOHIDEventTypeTemperature:\
            size = sizeof(IOHIDTemperatureEventData);\
            break;                      \
        case kIOHIDEventTypeNavigationSwipe:\
        case kIOHIDEventTypeDockSwipe:\
        case kIOHIDEventTypeFluidTouchGesture:\
        case kIOHIDEventTypeBoundaryScroll:\
            size = sizeof(IOHIDSwipeEventData);\
            break;                      \
        case kIOHIDEventTypeMouse:\
            size = sizeof(IOHIDMouseEventData);\
            break;                      \
        case kIOHIDEventTypeProgress:\
            size = sizeof(IOHIDProgressEventData);\
            break;                      \
        case kIOHIDEventTypeZoomToggle:\
            size = sizeof(IOHIDZoomToggleEventData);\
            break;                      \
        case kIOHIDEventTypeSymbolicHotKey:\
            size = sizeof(IOHIDSymbolicHotKeyEventData);\
            break;                      \
        case kIOHIDEventTypePower:\
            size = sizeof(IOHIDPowerEventData);\
            break;                      \
        case kIOHIDEventTypeBrightness:\
            size = sizeof(IOHIDBrightnessEventData);\
            break;                      \
        default:                        \
            size = 0;                   \
            break;                      \
    }                                   \
}
#define IOHIDEventGetQueueElementSize(type,size)\
{                                               \
    IOHIDEventGetSize(type,size);               \
    size += sizeof(IOHIDSystemQueueElement);    \
}

#ifdef KERNEL
    #define IOHIDEventValueFloat(value, isFixed)                (isFixed ? value : value>>16)
    #define IOHIDEventValueFixed(value, isFixed)                (isFixed ? value : value<<16)
    #define IOHIDEventGetEventWithOptions(event, type, options) event->getEvent(type, options)
    #define GET_EVENTDATA(event)                                event->_data
#else
    #define IOHIDEventValueFloat(value, isFixed)                (isFixed ? value : value / 65536.0)
    #define IOHIDEventValueFixed(value, isFixed)                (isFixed ? value : value * 65536)
    #define GET_EVENTDATA(event)                                event->eventData
#endif

//==============================================================================
// IOHIDEventGetValue MACRO
//==============================================================================
#define GET_EVENTDATA_VALUE(eventData, fieldEvType, fieldOffset, value, isFixed)\
{                                                       \
    switch ( fieldEvType ) {                            \
        case kIOHIDEventTypeNULL:                       \
            switch ( fieldOffset ) {                    \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsRelative): \
                    value = (eventData->options & kIOHIDEventOptionIsAbsolute) == 0; \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsCollection): \
                    value = (eventData->options & kIOHIDEventOptionIsCollection) != 0; \
                    break;                              \
            }; break;                                   \
        case kIOHIDEventTypeVendorDefined:              \
            {                                           \
                IOHIDVendorDefinedEventData * sysDef = (IOHIDVendorDefinedEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsagePage): \
                        value = sysDef->usagePage;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsage): \
                        value = sysDef->usage;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedVersion): \
                        value = sysDef->version;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedDataLength): \
                        value = sysDef->length;         \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedData): \
                        if (sysDef->data)               \
                            value = *((typeof(value)*) sysDef->data); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeProgress:                     \
            {                                           \
                IOHIDProgressEventData * progress = (IOHIDProgressEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressEventType): \
                        value = progress->eventType;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressLevel): \
                        value = IOHIDEventValueFloat(progress->level, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeBrightness:                  \
            {                                           \
                IOHIDBrightnessEventData * brightness = (IOHIDBrightnessEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBrightnessLevel): \
                        value = IOHIDEventValueFloat(brightness->level, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeButton:                     \
            {                                           \
                IOHIDButtonEventData * button = (IOHIDButtonEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonMask): \
                        value = button->button.buttonMask;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonNumber): \
                        value = button->button.buttonNumber;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonClickCount): \
                        value = button->button.clickState;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonPressure): \
                        value = IOHIDEventValueFloat(button->button.pressure, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeAccelerometer:                     \
            {                                           \
                IOHIDAccelerometerEventData * accl = (IOHIDAccelerometerEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerX): \
                        value = IOHIDEventValueFloat(accl->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerY): \
                        value = IOHIDEventValueFloat(accl->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerZ): \
                        value = IOHIDEventValueFloat(accl->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerType): \
                        value = accl->acclType;     \
                        break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerSubType): \
						value = accl->acclSubType;     \
						break;                          \
				};                                      \
            }                                           \
            break;                                      \
			case kIOHIDEventTypeGyro:                     \
			{                                           \
				IOHIDGyroEventData * gyro = (IOHIDGyroEventData*)eventData; \
				switch ( fieldOffset ) {                \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroX): \
						value = IOHIDEventValueFloat(gyro->position.x, isFixed); \
						break;                              \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroY): \
						value = IOHIDEventValueFloat(gyro->position.y, isFixed); \
						break;                              \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroZ): \
						value = IOHIDEventValueFloat(gyro->position.z, isFixed); \
						break;                              \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroType): \
						value = gyro->gyroType;     \
						break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroSubType): \
						value = gyro->gyroSubType;     \
						break;                          \
				};                                      \
			}                                           \
			break;                                      \
            case kIOHIDEventTypeCompass:                     \
            {                                           \
                IOHIDCompassEventData * compass = (IOHIDCompassEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassX): \
                        value = IOHIDEventValueFloat(compass->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassY): \
                        value = IOHIDEventValueFloat(compass->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassZ): \
                        value = IOHIDEventValueFloat(compass->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassType): \
                        value = compass->compassType;     \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
            case kIOHIDEventTypeMouse:                     \
            {                                           \
                IOHIDMouseEventData * mouse = (IOHIDMouseEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseX): \
                        value = IOHIDEventValueFloat(mouse->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseY): \
                        value = IOHIDEventValueFloat(mouse->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseZ): \
                        value = IOHIDEventValueFloat(mouse->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseButtonMask): \
                        value = mouse->button.buttonMask;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseNumber): \
                        value = mouse->button.buttonNumber;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseClickCount): \
                        value = mouse->button.clickState;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMousePressure): \
                        value = IOHIDEventValueFloat(mouse->button.pressure, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeNavigationSwipe:            \
        case kIOHIDEventTypeDockSwipe:                  \
        case kIOHIDEventTypeFluidTouchGesture:          \
        case kIOHIDEventTypeBoundaryScroll:             \
            {                                           \
                IOHIDSwipeEventData * swipe = (IOHIDSwipeEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMask):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMask):    \
                    */                                  \
                        value = swipe->swipeMask;       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMotion):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMotion):    \
                    */                                  \
                        value = swipe->gestureMotion;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeProgress):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollProgress):    \
                    */                                  \
                        value = IOHIDEventValueFloat(swipe->progress, isFixed);       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionX):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionX):    \
                    */                                  \
                        value = IOHIDEventValueFloat(swipe->positionX, isFixed);       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionY):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionY):    \
                    */                                  \
                        value = IOHIDEventValueFloat(swipe->positionY, isFixed);       \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeFlavor):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollFlavor):    \
                    */                                  \
                        value = swipe->flavor;          \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTemperature:                \
            {                                           \
                IOHIDTemperatureEventData * temp = (IOHIDTemperatureEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTemperatureLevel):       \
                        value = IOHIDEventValueFloat(temp->level, isFixed);\
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTranslation:                \
        case kIOHIDEventTypeRotation:                   \
        case kIOHIDEventTypeScroll:                     \
        case kIOHIDEventTypeScale:                      \
        case kIOHIDEventTypeVelocity:                   \
        case kIOHIDEventTypeOrientation:                \
            {                                           \
                IOHIDAxisEventData * axis = (IOHIDAxisEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationX):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollX):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleX):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationRadius):  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerX): \
                    */                                                              \
                        value = IOHIDEventValueFloat(axis->position.x, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationY):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollY):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleY):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAzimuth): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerY): \
                    */                                                              \
                        value = IOHIDEventValueFloat(axis->position.y, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationZ):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollZ):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleZ):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAltitude):\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerZ): \
                    */                                                              \
                        value = IOHIDEventValueFloat(axis->position.z, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollIsPixels):         \
                        value = ((axis->options & kIOHIDEventOptionPixelUnits) != 0);\
                        break;                              \
                };                                          \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeAmbientLightSensor:         \
            {                                           \
                IOHIDAmbientLightSensorEventData * alsEvent = (IOHIDAmbientLightSensorEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorLevel): \
                        value = alsEvent->level;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel0): \
                        value = alsEvent->ch0;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel1): \
                        value = alsEvent->ch1;          \
                        break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel2): \
						value = alsEvent->ch2;          \
						break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel3): \
						value = alsEvent->ch3;          \
						break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightDisplayBrightnessChanged): \
                        value = alsEvent->brightnessChanged; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeProximity:                  \
            {                                           \
                IOHIDProximityEventData * proxEvent = (IOHIDProximityEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProximityDetectionMask): \
                        value = proxEvent->detectionMask; \
                        break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldProximityLevel): \
						value = proxEvent->level; \
						break;                          \
					};                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeKeyboard:                   \
            {                                           \
                IOHIDKeyboardEventData * keyEvent = (IOHIDKeyboardEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsagePage): \
                        value = keyEvent->usagePage;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsage):     \
                        value = keyEvent->usage;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardDown):    \
                        value = keyEvent->down;         \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardRepeat):  \
                        value = (keyEvent->options & kIOHIDKeyboardIsRepeat);\
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeDigitizer:                  \
            {                                           \
                IOHIDDigitizerEventData * digEvent = (IOHIDDigitizerEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX): \
                        value = IOHIDEventValueFloat(digEvent->position.x, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY): \
                        value = IOHIDEventValueFloat(digEvent->position.y, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ): \
                        value = IOHIDEventValueFloat(digEvent->position.z, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerButtonMask): \
                        value = digEvent->buttonMask;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIndex): \
                        value = digEvent->transducerIndex; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIdentity): \
                        value = digEvent->identity;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerEventMask): \
                        value = digEvent->eventMask;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerChildEventMask): \
                        value = digEvent->childEventMask; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerRange): \
                        value = (digEvent->options & kIOHIDTransducerRange) != 0;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTouch): \
                        value = (digEvent->options & kIOHIDTransducerTouch) != 0;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerCollection): \
                        value = (digEvent->options & kIOHIDEventOptionIsCollection) != 0;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerPressure): \
                        value = IOHIDEventValueFloat(digEvent->tipPressure, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerBarrelPressure): \
                        value = IOHIDEventValueFloat(digEvent->barrelPressure, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTwist): \
                        value = IOHIDEventValueFloat(digEvent->twist, isFixed);        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                        switch ( digEvent->orientationType ) {\
                            case kIOHIDDigitizerOrientationTypeTilt:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.tilt.x, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.tilt.y, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypePolar:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAltitude): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.altitude, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAzimuth): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.polar.azimuth, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypeQuality:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.quality, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.density, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.irregularity, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.majorRadius, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                                        value = IOHIDEventValueFloat(digEvent->orientation.quality.minorRadius, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                        };                              \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeZoomToggle:                 \
            {                                           \
            /*  IOHIDZoomToggleEventData * zoom = (IOHIDZoomToggleEventData *)eventData; */\
                switch ( fieldOffset ) {                \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeSymbolicHotKey:             \
            {                                           \
                IOHIDSymbolicHotKeyEventData * symbolicEvent = (IOHIDSymbolicHotKeyEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyValue): \
                        value = symbolicEvent->hotKey;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyIsCGSEvent): \
                        value = (symbolicEvent->options & kIOHIDSymbolicHotKeyOptionIsCGSHotKey) != 0; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypePower:                      \
            {\
                IOHIDPowerEventData * pwrEvent = (IOHIDPowerEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerMeasurement): \
                        value = IOHIDEventValueFloat(pwrEvent->measurement, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerType): \
                        value = pwrEvent->powerType;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerSubType): \
                        value = pwrEvent->powerSubType; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeReset:                      \
            {                                           \
            /*  IOHIDEventResetEventData * reset = (IOHIDEventResetEventData *)eventData; */\
                switch ( fieldOffset ) {                \
                };                                      \
            }                                           \
            break;                                      \
    };                                                  \
}

#define GET_EVENT_VALUE(event, field, value, options) \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = NULL;                             \
    if ( (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        GET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, false);\
    }                                                               \
}

#define GET_EVENT_VALUE_FIXED(event, field, value, options) \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = NULL;                             \
    if ( (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        GET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, true);\
    }                                                               \
}

//==============================================================================
// IOHIDEventSetValue MACRO
//==============================================================================
#define SET_EVENTDATA_VALUE(eventData, fieldEvType, fieldOffset, value, isFixed) \
{   switch ( fieldEvType ) {                            \
        case kIOHIDEventTypeNULL:                       \
            switch ( fieldOffset ) {                    \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsRelative): \
                    if ( value )                        \
                        eventData->options &= ~kIOHIDEventOptionIsAbsolute; \
                    else                                \
                        eventData->options |= kIOHIDEventOptionIsAbsolute; \
                    break;                              \
                case IOHIDEventFieldOffset(kIOHIDEventFieldIsCollection): \
                    if ( value )                        \
                        eventData->options |= kIOHIDEventOptionIsCollection; \
                    else                                \
                        eventData->options &= ~kIOHIDEventOptionIsCollection; \
                    break;                              \
            }; break;                                   \
        case kIOHIDEventTypeVendorDefined:              \
            {                                           \
                IOHIDVendorDefinedEventData * sysDef = (IOHIDVendorDefinedEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsagePage): \
                        sysDef->usagePage = value;      \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedUsage): \
                        sysDef->usage = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedVersion): \
                        sysDef->version = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVendorDefinedData): \
                        if (sysDef->data)               \
                            *((typeof(value)*) sysDef->data) = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeProgress:                     \
            {                                           \
                IOHIDProgressEventData * progress = (IOHIDProgressEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressEventType): \
                        progress->eventType = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProgressLevel): \
                        progress->level = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeBrightness:                  \
            {                                           \
                IOHIDBrightnessEventData * brightness = (IOHIDBrightnessEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBrightnessLevel): \
                        brightness->level = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeButton:                     \
            {                                           \
                IOHIDButtonEventData * button = (IOHIDButtonEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonMask): \
                        button->button.buttonMask = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonNumber): \
                        button->button.buttonNumber = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonClickCount): \
                        button->button.clickState = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldButtonPressure): \
                        button->button.pressure = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeAccelerometer:              \
            {                                           \
                IOHIDAccelerometerEventData * accl = (IOHIDAccelerometerEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerX): \
                        accl->position.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerY): \
                        accl->position.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerZ): \
                        accl->position.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerType): \
                        accl->acclType = value;     \
                        break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerSubType): \
						accl->acclSubType = value;     \
						break;                          \
				};                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeGyro:              \
			{                                           \
				IOHIDGyroEventData * gyro = (IOHIDGyroEventData*)eventData; \
				switch ( fieldOffset ) {                \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroX): \
						gyro->position.x = IOHIDEventValueFixed(value, isFixed); \
						break;                              \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroY): \
						gyro->position.y = IOHIDEventValueFixed(value, isFixed); \
						break;                              \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroZ): \
						gyro->position.z = IOHIDEventValueFixed(value, isFixed); \
						break;                              \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroType): \
						gyro->gyroType = value;     \
						break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldGyroSubType): \
						gyro->gyroSubType = value;     \
						break;                          \
				};                                      \
			}                                           \
			break;                                      \
        case kIOHIDEventTypeCompass:                     \
            {                                           \
                IOHIDCompassEventData * compass = (IOHIDCompassEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassX): \
                        compass->position.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassY): \
                        compass->position.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassZ): \
                        compass->position.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldCompassType): \
                        value = compass->compassType;     \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeMouse:                     \
            {                                           \
                IOHIDMouseEventData * mouse = (IOHIDMouseEventData*)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseX): \
                        mouse->position.x = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseY): \
                        mouse->position.y = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseZ): \
                        mouse->position.z = IOHIDEventValueFixed(value, isFixed); \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseButtonMask): \
                        mouse->button.buttonMask = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseNumber): \
                        mouse->button.buttonNumber = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMouseClickCount): \
                        mouse->button.clickState = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldMousePressure): \
                        mouse->button.pressure = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeNavigationSwipe:            \
        case kIOHIDEventTypeDockSwipe:                  \
        case kIOHIDEventTypeFluidTouchGesture:          \
        case kIOHIDEventTypeBoundaryScroll:             \
            {                                           \
                IOHIDSwipeEventData * swipe = (IOHIDSwipeEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMask):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMask):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMask):    \
                    */                                  \
                        swipe->swipeMask = value;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeMotion):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureMotion):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollMotion):    \
                    */                                  \
                        swipe->gestureMotion = value;\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeProgress):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureProgress):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollProgress):    \
                    */                                  \
                        swipe->progress = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionX):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionX):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionX):    \
                    */                                  \
                        swipe->positionX = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipePositionY):       \
                    /*                                  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGesturePositionY):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollPositionY):    \
                    */                                  \
                        swipe->positionY = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSwipeFlavor):       \
                    /*  
                    case IOHIDEventFieldOffset(kIOHIDEventFieldNavigationSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDockSwipeFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldFluidTouchGestureFlavor):    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldBoundaryScrollFlavor):    \
                    */                                  \
                        swipe->flavor = value;          \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTemperature:                \
            {                                           \
                IOHIDTemperatureEventData * temp = (IOHIDTemperatureEventData *)eventData; \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTemperatureLevel):       \
                        temp->level = IOHIDEventValueFixed(value, isFixed);\
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeTranslation:                \
        case kIOHIDEventTypeRotation:                   \
        case kIOHIDEventTypeScroll:                     \
        case kIOHIDEventTypeScale:                      \
        case kIOHIDEventTypeVelocity:                   \
        case kIOHIDEventTypeOrientation:                \
            {                                           \
                IOHIDAxisEventData * axis = (IOHIDAxisEventData *)eventData;    \
                switch ( fieldOffset ) {                    \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationX):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollX):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleX):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityX):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationRadius):  \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerX):     \
                    */                                                              \
                        axis->position.x = IOHIDEventValueFixed(value, isFixed);             \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationY):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollY):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleY):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityY):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAzimuth): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerY):     \
                    */                                                              \
                        axis->position.y = IOHIDEventValueFixed(value, isFixed);             \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldTranslationZ):       \
                    /*                                                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldRotationZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollZ):            \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScaleZ):             \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldVelocityZ):          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ);         \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldOrientationAltitude):\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAccelerometerZ):     \
                    */                                                              \
                        axis->position.z = IOHIDEventValueFixed(value, isFixed);             \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldScrollIsPixels):         \
                        if ( value )                        \
                            axis->options |= kIOHIDEventOptionPixelUnits;           \
                        else                                \
                            axis->options &= ~kIOHIDEventOptionPixelUnits;           \
                        break;                              \
                };                                          \
            }                                               \
            break;                                          \
        case kIOHIDEventTypeAmbientLightSensor:             \
            {                                               \
                IOHIDAmbientLightSensorEventData * alsEvent = (IOHIDAmbientLightSensorEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorLevel): \
                        alsEvent->level = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel0): \
                        alsEvent->ch0 = value;          \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel1): \
                        alsEvent->ch1 = value;          \
                        break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel2): \
						alsEvent->ch2 = value;          \
						break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightSensorRawChannel3): \
						alsEvent->ch3 = value;          \
						break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldAmbientLightDisplayBrightnessChanged): \
                        alsEvent->brightnessChanged = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeProximity:                  \
            {                                           \
                IOHIDProximityEventData * proxEvent = (IOHIDProximityEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldProximityDetectionMask): \
                        proxEvent->detectionMask = value; \
                        break;                          \
					case IOHIDEventFieldOffset(kIOHIDEventFieldProximityLevel): \
						proxEvent->level = value; \
						break;                          \
					};                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeKeyboard:                       \
            {                                               \
                IOHIDKeyboardEventData * keyEvent = (IOHIDKeyboardEventData *)eventData;\
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsagePage): \
                        keyEvent->usagePage = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardUsage):     \
                        keyEvent->usage = value;        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardDown):    \
                        keyEvent->down = value;         \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldKeyboardRepeat):  \
                        if ( value )                        \
                            keyEvent->options |= kIOHIDKeyboardIsRepeat;            \
                        else                                \
                            keyEvent->options &= ~kIOHIDKeyboardIsRepeat;           \
                        break;                              \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeDigitizer:                  \
            {                                           \
                IOHIDDigitizerEventData * digEvent = (IOHIDDigitizerEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerX): \
                        digEvent->position.x = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerY): \
                        digEvent->position.y = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerZ): \
                        digEvent->position.z = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerButtonMask): \
                        digEvent->buttonMask = value;   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIndex): \
                        digEvent->transducerIndex = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIdentity): \
                        digEvent->identity = value;     \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerEventMask): \
                        digEvent->eventMask = value;    \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerChildEventMask): \
                        digEvent->childEventMask = value; \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerRange): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDTransducerRange;         \
                        else                                \
                            digEvent->options &= ~kIOHIDTransducerRange;        \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTouch): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDTransducerTouch;         \
                        else                                \
                            digEvent->options &= ~kIOHIDTransducerTouch;        \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerCollection): \
                        if ( value )                        \
                            digEvent->options |= kIOHIDEventOptionIsCollection;         \
                        else                                \
                            digEvent->options &= ~kIOHIDEventOptionIsCollection;        \
                        break;                              \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerPressure): \
                        digEvent->tipPressure = IOHIDEventValueFixed(value, isFixed);  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerBarrelPressure): \
                        digEvent->barrelPressure = IOHIDEventValueFixed(value, isFixed);   \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTwist): \
                        digEvent->twist = IOHIDEventValueFixed(value, isFixed);        \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                        switch ( digEvent->orientationType ) {\
                            case kIOHIDDigitizerOrientationTypeTilt:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltX): \
                                        digEvent->orientation.tilt.x = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerTiltY): \
                                        digEvent->orientation.tilt.y = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypePolar:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAltitude): \
                                        digEvent->orientation.polar.altitude = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerAzimuth): \
                                        digEvent->orientation.polar.azimuth = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                            case kIOHIDDigitizerOrientationTypeQuality:\
                                switch ( fieldOffset ) {\
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerQuality): \
                                        digEvent->orientation.quality.quality = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerDensity): \
                                        digEvent->orientation.quality.density = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerIrregularity): \
                                        digEvent->orientation.quality.irregularity = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMajorRadius): \
                                        digEvent->orientation.quality.majorRadius = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                    case IOHIDEventFieldOffset(kIOHIDEventFieldDigitizerMinorRadius): \
                                        digEvent->orientation.quality.minorRadius = IOHIDEventValueFixed(value, isFixed); \
                                        break;          \
                                };                      \
                                break;                  \
                        };                              \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeZoomToggle:                 \
            {                                           \
            /*  IOHIDZoomToggleEventData * zoom = (IOHIDZoomToggleEventData *)eventData; */\
                switch ( fieldOffset ) {                \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeSymbolicHotKey:             \
            {                                           \
                IOHIDSymbolicHotKeyEventData * symbolicEvent = (IOHIDSymbolicHotKeyEventData *)eventData; \
                switch ( fieldOffset ) {                \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyValue): \
                        symbolicEvent->hotKey = value;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldSymbolicHotKeyIsCGSEvent): \
                        if ( value )                    \
                            symbolicEvent->options |= kIOHIDSymbolicHotKeyOptionIsCGSHotKey; \
                        else                            \
                            symbolicEvent->options &= ~kIOHIDSymbolicHotKeyOptionIsCGSHotKey; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypePower:                      \
            {                                           \
                IOHIDPowerEventData * pwrEvent = (IOHIDPowerEventData *)eventData; \
                switch ( fieldOffset ) {\
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerMeasurement): \
                        pwrEvent->measurement = IOHIDEventValueFixed(value, isFixed); \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerType): \
                        pwrEvent->powerType = value;  \
                        break;                          \
                    case IOHIDEventFieldOffset(kIOHIDEventFieldPowerSubType): \
                        pwrEvent->powerSubType = value; \
                        break;                          \
                };                                      \
            }                                           \
            break;                                      \
        case kIOHIDEventTypeReset:                      \
            {                                           \
            /*  IOHIDEventResetEventData * reset = (IOHIDEventResetEventData *)eventData; */\
                switch ( fieldOffset ) {                \
                };                                      \
            }                                           \
            break;                                      \
    };                                                  \
}

#define SET_EVENT_VALUE(event, field, value, options)               \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = NULL;                             \
    if ( (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        SET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, false);\
    }                                                               \
}

#define SET_EVENT_VALUE_FIXED(event, field, value, options)               \
{   IOHIDEventType  fieldEvType = IOHIDEventFieldEventType(field);  \
    uint32_t        fieldOffset = IOHIDEventFieldOffset(field);     \
    IOHIDEventRef   ev          = NULL;                             \
    if ( (ev = IOHIDEventGetEventWithOptions(event, fieldEvType, options)) ) {\
        SET_EVENTDATA_VALUE(GET_EVENTDATA(ev),fieldEvType,fieldOffset,value, true);\
    }                                                               \
}

#endif /* _IOKIT_HID_IOHIDEVENTDATA_H */
