//
//  HIDEvent.h
//  hidutil-internal
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDEvent.h>

@interface HIDEvent : NSObject {
IOHIDEventRef eventRef;
}

@property               NSNumber *timestamp;
@property               NSNumber *sender;
@property (readonly)    NSNumber *typeval;
@property (readonly)    NSNumber *latency;
@property               NSNumber *flags;
@property (readonly)    NSString *typestr;

- (id)initWithEvent:(IOHIDEventRef)event;

@end

HIDEvent *createHIDEvent(IOHIDEventRef event);

@interface HIDVendorDefinedEvent : HIDEvent

@property (readonly) NSNumber *length;
@property NSNumber *version;
@property NSNumber *usage;
@property (readonly) uint8_t *data;
@property NSNumber *usagepage;

@end


@interface HIDScaleEvent : HIDEvent

@property NSNumber *z;
@property NSNumber *x;
@property NSNumber *y;

@end


@interface HIDSymbolicHotKeyEvent : HIDEvent

@property NSNumber *value;
@property NSNumber *iscgsevent;

@end


@interface HIDTemperatureEvent : HIDEvent

@property NSNumber *level;

@end


@interface HIDAccelerometerEvent : HIDEvent

@property NSNumber *y;
@property NSNumber *x;
@property NSNumber *z;
@property NSNumber *type;
@property NSNumber *subtype;
@property NSNumber *sequence;

@end


@interface HIDProgressEvent : HIDEvent

@property NSNumber *level;
@property NSNumber *eventtype;

@end


@interface HIDGenericGestureEvent : HIDEvent

@property NSNumber *typetapcount;
@property (readonly) NSNumber *type;
@property NSNumber *typeswipeprogress;

@end


@interface HIDNULLEvent : HIDEvent

@property NSNumber *relative;
@property NSNumber *centerorigin;
@property NSNumber *builtin;
@property NSNumber *pixelunits;
@property NSNumber *collection;

@end


@interface HIDAmbientLightSensorEvent : HIDEvent

@property NSNumber *colorspace;
@property NSNumber *colorcomponent2;
@property NSNumber *colorcomponent1;
@property NSNumber *colorcomponent0;
@property NSNumber *rawchannel0;
@property NSNumber *rawchannel1;
@property NSNumber *rawchannel2;
@property NSNumber *rawchannel3;
@property NSNumber *level;
@property NSNumber *illuminance;
@property NSNumber *brightnesschanged;
@property NSNumber *colortemperature;

@end


@interface HIDPowerEvent : HIDEvent

@property NSNumber *type;
@property NSNumber *subtype;
@property NSNumber *measurement;

@end


@interface HIDForceEvent : HIDEvent

@property NSNumber *stagepressure;
@property NSNumber *stage;
@property NSNumber *progress;
@property NSNumber *behavior;

@end


@interface HIDMotionGestureEvent : HIDEvent

@property NSNumber *progress;
@property NSNumber *gesturetype;

@end


@interface HIDGameControllerEvent : HIDEvent

@property NSNumber *joystickaxisx;
@property NSNumber *joystickaxisy;
@property NSNumber *type;
@property NSNumber *directionpadright;
@property NSNumber *shoulderbuttonr1;
@property NSNumber *facebuttona;
@property NSNumber *facebuttonb;
@property NSNumber *directionpadleft;
@property NSNumber *thumbstickbuttonright;
@property NSNumber *directionpaddown;
@property NSNumber *thumbstickbuttonleft;
@property NSNumber *joystickaxisz;
@property NSNumber *shoulderbuttonr2;
@property NSNumber *facebuttony;
@property NSNumber *shoulderbuttonl2;
@property NSNumber *joystickaxisrz;
@property NSNumber *shoulderbuttonl1;
@property NSNumber *facebuttonx;
@property NSNumber *directionpadup;

@end


@interface HIDTranslationEvent : HIDEvent

@property NSNumber *y;
@property NSNumber *x;
@property NSNumber *z;

@end


@interface HIDDigitizerEvent : HIDEvent

@property NSNumber *type;
@property NSNumber *childeventmask;
@property NSNumber *auxiliarypressure;
@property NSNumber *qualityradiiaccuracy;
@property NSNumber *quality;
@property NSNumber *minorradius;
@property NSNumber *eventmask;
@property NSNumber *generationcount;
@property NSNumber *index;
@property NSNumber *touch;
@property NSNumber *azimuth;
@property NSNumber *tiltx;
@property NSNumber *tilty;
@property NSNumber *range;
@property NSNumber *pressure;
@property NSNumber *collection;
@property NSNumber *altitude;
@property NSNumber *density;
@property (readonly) NSNumber *orientationtype;
@property NSNumber *y;
@property NSNumber *willupdatemask;
@property NSNumber *identity;
@property NSNumber *twist;
@property NSNumber *x;
@property NSNumber *isdisplayintegrated;
@property NSNumber *z;
@property NSNumber *majorradius;
@property NSNumber *buttonmask;
@property NSNumber *irregularity;
@property NSNumber *didupdatemask;

@end


@interface HIDCompassEvent : HIDEvent

@property NSNumber *type;
@property NSNumber *z;
@property NSNumber *x;
@property NSNumber *y;
@property NSNumber *subtype;
@property NSNumber *sequence;

@end


@interface HIDRotationEvent : HIDEvent

@property NSNumber *y;
@property NSNumber *x;
@property NSNumber *z;

@end


@interface HIDMotionActivityEvent : HIDEvent

@property NSNumber *confidence;
@property NSNumber *activitytype;

@end


@interface HIDMultiAxisPointerEvent : HIDEvent

@property NSNumber *ry;
@property NSNumber *rx;
@property NSNumber *rz;
@property NSNumber *buttonmask;
@property NSNumber *z;
@property NSNumber *x;
@property NSNumber *y;

@end


@interface HIDBrightnessEvent : HIDEvent

@property NSNumber *targetbrightness;
@property NSNumber *currentbrightness;
@property NSNumber *transitiontime;

@end


@interface HIDGyroEvent : HIDEvent

@property NSNumber *x;
@property NSNumber *y;
@property NSNumber *z;
@property NSNumber *subtype;
@property NSNumber *sequence;
@property NSNumber *type;

@end


@interface HIDButtonEvent : HIDEvent

@property NSNumber *pressure;
@property NSNumber *clickcount;
@property NSNumber *mask;
@property NSNumber *state;
@property NSNumber *number;

@end


@interface HIDNavigationSwipeEvent : HIDEvent

@property NSNumber *flavor;
@property NSNumber *progress;
@property NSNumber *mask;
@property NSNumber *motion;
@property NSNumber *positiony;
@property NSNumber *positionx;
@property NSNumber *positionz;

@end


@interface HIDAtmosphericPressureEvent : HIDEvent

@property NSNumber *level;
@property NSNumber *sequence;

@end


@interface HIDHumidityEvent : HIDEvent

@property NSNumber *sequence;
@property NSNumber *rh;

@end


@interface HIDVelocityEvent : HIDEvent

@property NSNumber *x;
@property NSNumber *y;
@property NSNumber *z;

@end


@interface HIDScrollEvent : HIDEvent

@property NSNumber *ispixels;
@property NSNumber *x;
@property NSNumber *y;
@property NSNumber *z;

@end


@interface HIDBiometricEvent : HIDEvent

@property NSNumber *eventtype;
@property NSNumber *usage;
@property NSNumber *level;
@property NSNumber *tapcount;
@property NSNumber *usagepage;

@end


@interface HIDBoundaryScrollEvent : HIDEvent

@property NSNumber *progress;
@property NSNumber *flavor;
@property NSNumber *positiony;
@property NSNumber *positionx;
@property NSNumber *mask;
@property NSNumber *motion;

@end


@interface HIDLEDEvent : HIDEvent

@property NSNumber *mask;
@property NSNumber *state;
@property NSNumber *number;

@end


@interface HIDOrientationEvent : HIDEvent

@property (readonly) NSNumber *orientationtype;
@property NSNumber *tiltz;
@property NSNumber *tilty;
@property NSNumber *tiltx;
@property NSNumber *azimuth;
@property NSNumber *deviceorientationusage;
@property NSNumber *altitude;
@property NSNumber *radius;

@end


@interface HIDProximityEvent : HIDEvent

@property NSNumber *level;
@property NSNumber *detectionmask;

@end


@interface HIDFluidTouchGestureEvent : HIDEvent

@property NSNumber *positiony;
@property NSNumber *positionx;
@property NSNumber *mask;
@property NSNumber *progress;
@property NSNumber *motion;
@property NSNumber *flavor;

@end


@interface HIDDockSwipeEvent : HIDEvent

@property NSNumber *progress;
@property NSNumber *mask;
@property NSNumber *motion;
@property NSNumber *flavor;
@property NSNumber *positionx;
@property NSNumber *positiony;
@property NSNumber *positionz;

@end


@interface HIDUnicodeEvent : HIDEvent

@property (readonly) uint8_t *payload;
@property NSNumber *length;
@property NSNumber *quality;
@property NSNumber *encoding;

@end


@interface HIDKeyboardEvent : HIDEvent

@property NSNumber *stickykeyphase;
@property NSNumber *stickykeytoggle;
@property NSNumber *mousekeytoggle;
@property NSNumber *clickspeed;
@property NSNumber *presscount;
@property NSNumber *longpress;
@property NSNumber *usagepage;
@property NSNumber *slowkeyphase;
@property NSNumber *down;
@property NSNumber *repeat;
@property NSNumber *usage;

@end


@interface HIDPointerEvent : HIDEvent

@property NSNumber *z;
@property NSNumber *y;
@property NSNumber *x;
@property NSNumber *buttonmask;

@end


