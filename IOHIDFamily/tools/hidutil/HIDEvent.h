//
//  HIDEvent.h
//  hidutil-internal
//

#import <Foundation/Foundation.h>
#import <IOKit/hid/IOHIDEvent.h>
#import <HID/HIDEvent.h>

@interface HIDEvent (HIDUtil)

@property               NSNumber *timestamp;
@property               NSNumber *sender;
@property (readonly)    NSNumber *typeval;
@property (readonly)    NSNumber *latency;
@property               NSNumber *flags;
@property (readonly)    NSString *typestr;

@end

@interface HIDEvent (HIDUtilVendorDefinedEvent)

@property (readonly) NSNumber *vendorDefinedDataLength;
@property NSNumber *vendorDefinedVersion;
@property NSNumber *vendorDefinedUsage;
@property (readonly) uint8_t *vendorDefinedData;
@property NSNumber *vendorDefinedUsagePage;

- (NSString *)vendorDefinedDescription;

@end


@interface HIDEvent (HIDUtilScaleEvent)

@property NSNumber *scaleZ;
@property NSNumber *scaleX;
@property NSNumber *scaleY;

- (NSString *)scaleDescription;

@end


@interface HIDEvent (HIDUtilSymbolicHotKeyEvent)

@property NSNumber *symbolicHotKeyValue;
@property NSNumber *symbolicHotKeyIsCGSEvent;

- (NSString *)symbolicHotKeyDescription;

@end


@interface HIDEvent (HIDUtilTemperatureEvent)

@property NSNumber *temperatureLevel;

- (NSString *)temperatureDescription;

@end


@interface HIDEvent (HIDUtilAccelerometerEvent)

@property NSNumber *accelerometerY;
@property NSNumber *accelerometerX;
@property NSNumber *accelerometerZ;
@property NSNumber *accelerometerType;
@property NSNumber *accelerometerSubType;
@property NSNumber *accelerometerSequence;

- (NSString *)accelerometerDescription;

@end


@interface HIDEvent (HIDUtilProgressEvent)

@property NSNumber *progressLevel;
@property NSNumber *progressEventType;

- (NSString *)progressDescription;

@end


@interface HIDEvent (HIDUtilGenericGestureEvent)

@property NSNumber *genericGestureTypeTapCount;
@property (readonly) NSNumber *genericGestureType;
@property NSNumber *genericGestureTypeSwipeProgress;

- (NSString *)genericGestureDescription;

@end


@interface HIDEvent (HIDUtilNULLEvent)

@property NSNumber *isRelative;
@property NSNumber *isCenterOrigin;
@property NSNumber *isBuiltIn;
@property NSNumber *isPixelUnits;
@property NSNumber *isCollection;

- (NSString *)nullDescription;

@end


@interface HIDEvent (HIDUtilAmbientLightSensorEvent)

@property NSNumber *ambientLightColorSpace;
@property NSNumber *ambientLightColorComponent2;
@property NSNumber *ambientLightColorComponent1;
@property NSNumber *ambientLightColorComponent0;
@property NSNumber *ambientLightSensorRawChannel0;
@property NSNumber *ambientLightSensorRawChannel1;
@property NSNumber *ambientLightSensorRawChannel2;
@property NSNumber *ambientLightSensorRawChannel3;
@property NSNumber *ambientLightSensorLevel;
@property NSNumber *ambientLightSensorIlluminance;
@property NSNumber *ambientLightDisplayBrightnessChanged;
@property NSNumber *ambientLightSensorColorTemperature;

- (NSString *)ambientLightSensorDescription;

@end


@interface HIDEvent (HIDUtilPowerEvent)

@property NSNumber *powerType;
@property NSNumber *powerSubType;
@property NSNumber *powerMeasurement;

- (NSString *)powerDescription;

@end


@interface HIDEvent (HIDUtilForceEvent)

@property NSNumber *forceStagePressure;
@property NSNumber *forceStage;
@property NSNumber *forceProgress;
@property NSNumber *forceBehavior;

- (NSString *)forceDescription;

@end


@interface HIDEvent (HIDUtilMotionGestureEvent)

@property NSNumber *motionGestureProgress;
@property NSNumber *motionGestureGestureType;

- (NSString *)motionGestureDescription;

@end


@interface HIDEvent (HIDUtilGameControllerEvent)

@property NSNumber *gameControllerJoyStickAxisX;
@property NSNumber *gameControllerJoyStickAxisY;
@property NSNumber *gameControllerType;
@property NSNumber *gameControllerDirectionPadRight;
@property NSNumber *gameControllerShoulderButtonR1;
@property NSNumber *gameControllerFaceButtonA;
@property NSNumber *gameControllerFaceButtonB;
@property NSNumber *gameControllerDirectionPadLeft;
@property NSNumber *gameControllerThumbstickButtonRight;
@property NSNumber *gameControllerDirectionPadDown;
@property NSNumber *gameControllerThumbstickButtonLeft;
@property NSNumber *gameControllerJoyStickAxisZ;
@property NSNumber *gameControllerShoulderButtonR2;
@property NSNumber *gameControllerFaceButtonY;
@property NSNumber *gameControllerShoulderButtonL2;
@property NSNumber *gameControllerJoyStickAxisRz;
@property NSNumber *gameControllerShoulderButtonL1;
@property NSNumber *gameControllerFaceButtonX;
@property NSNumber *gameControllerDirectionPadUp;

- (NSString *)gameControllerDescription;

@end


@interface HIDEvent (HIDUtilTranslationEvent)

@property NSNumber *translationY;
@property NSNumber *translationX;
@property NSNumber *translationZ;

- (NSString *)translationDescription;

@end


@interface HIDEvent (HIDUtilDigitizerEvent)

@property NSNumber *digitizerType;
@property NSNumber *digitizerChildEventMask;
@property NSNumber *digitizerAuxiliaryPressure;
@property NSNumber *digitizerQualityRadiiAccuracy;
@property NSNumber *digitizerQuality;
@property NSNumber *digitizerMinorRadius;
@property NSNumber *digitizerEventMask;
@property NSNumber *digitizerGenerationCount;
@property NSNumber *digitizerIndex;
@property NSNumber *digitizerTouch;
@property NSNumber *digitizerAzimuth;
@property NSNumber *digitizerTiltX;
@property NSNumber *digitizerTiltY;
@property NSNumber *digitizerRange;
@property NSNumber *digitizerPressure;
@property NSNumber *digitizerCollection;
@property NSNumber *digitizerAltitude;
@property NSNumber *digitizerDensity;
@property (readonly) NSNumber *digitizerOrientationType;
@property NSNumber *digitizerY;
@property NSNumber *digitizerWillUpdateMask;
@property NSNumber *digitizerIdentity;
@property NSNumber *digitizerTwist;
@property NSNumber *digitizerX;
@property NSNumber *digitizerIsDisplayIntegrated;
@property NSNumber *digitizerZ;
@property NSNumber *digitizerMajorRadius;
@property NSNumber *digitizerButtonMask;
@property NSNumber *digitizerIrregularity;
@property NSNumber *digitizerDidUpdateMask;

- (NSString *)digitizerDescription;

@end


@interface HIDEvent (HIDUtilCompassEvent)

@property NSNumber *compassType;
@property NSNumber *compassZ;
@property NSNumber *compassX;
@property NSNumber *compassY;
@property NSNumber *compassSubType;
@property NSNumber *compassSequence;

- (NSString *)compassDescription;

@end


@interface HIDEvent (HIDUtilRotationEvent)

@property NSNumber *rotationY;
@property NSNumber *rotationX;
@property NSNumber *rotationZ;

- (NSString *)rotationDescription;

@end


@interface HIDEvent (HIDUtilMotionActivityEvent)

@property NSNumber *motionActivityConfidence;
@property NSNumber *motionActivityActivityType;

- (NSString *)motionActivityDescription;

@end


@interface HIDEvent (HIDUtilMultiAxisPointerEvent)

@property NSNumber *multiAxisPointerRy;
@property NSNumber *multiAxisPointerRx;
@property NSNumber *multiAxisPointerRz;
@property NSNumber *multiAxisPointerButtonMask;
@property NSNumber *multiAxisPointerZ;
@property NSNumber *multiAxisPointerX;
@property NSNumber *multiAxisPointerY;

- (NSString *)multiAxisPointerDescription;

@end


@interface HIDEvent (HIDUtilBrightnessEvent)

@property NSNumber *targetBrightness;
@property NSNumber *currentBrightness;
@property NSNumber *transitionTime;

- (NSString *)brightnessDescription;

@end


@interface HIDEvent (HIDUtilGyroEvent)

@property NSNumber *gyroX;
@property NSNumber *gyroY;
@property NSNumber *gyroZ;
@property NSNumber *gyroSubType;
@property NSNumber *gyroSequence;
@property NSNumber *gyroType;

- (NSString *)gyroDescription;

@end


@interface HIDEvent (HIDUtilButtonEvent)

@property NSNumber *buttonPressure;
@property NSNumber *buttonClickCount;
@property NSNumber *buttonMask;
@property NSNumber *buttonState;
@property NSNumber *buttonNumber;

- (NSString *)buttonDescription;

@end


@interface HIDEvent (HIDUtilNavigationSwipeEvent)

@property NSNumber *navigationSwipeFlavor;
@property NSNumber *navigationSwipeProgress;
@property NSNumber *navigationSwipeMask;
@property NSNumber *navigationSwipeMotion;
@property NSNumber *navigationSwipePositionY;
@property NSNumber *navigationSwipePositionX;
@property NSNumber *navigationSwipePositionZ;

- (NSString *)navigationSwipeDescription;

@end


@interface HIDEvent (HIDUtilAtmosphericPressureEvent)

@property NSNumber *atmosphericPressureLevel;
@property NSNumber *atmosphericSequence;

- (NSString *)atmosphericPressureDescription;

@end


@interface HIDEvent (HIDUtilHumidityEvent)

@property NSNumber *humiditySequence;
@property NSNumber *humidityRH;

- (NSString *)humidityDescription;

@end


@interface HIDEvent (HIDUtilVelocityEvent)

@property NSNumber *velocityX;
@property NSNumber *velocityY;
@property NSNumber *velocityZ;

- (NSString *)velocityDescription;

@end


@interface HIDEvent (HIDUtilScrollEvent)

@property NSNumber *scrollIsPixels;
@property NSNumber *scrollX;
@property NSNumber *scrollY;
@property NSNumber *scrollZ;

- (NSString *)scrollDescription;

@end


@interface HIDEvent (HIDUtilBiometricEvent)

@property NSNumber *biometricEventType;
@property NSNumber *biometricUsage;
@property NSNumber *biometricLevel;
@property NSNumber *biometricTapCount;
@property NSNumber *biometricUsagePage;

- (NSString *)biometricDescription;

@end


@interface HIDEvent (HIDUtilBoundaryScrollEvent)

@property NSNumber *boundaryScrollProgress;
@property NSNumber *boundaryScrollFlavor;
@property NSNumber *boundaryScrollPositionY;
@property NSNumber *boundaryScrollPositionX;
@property NSNumber *boundaryScrollMask;
@property NSNumber *boundaryScrollMotion;

- (NSString *)boundaryScrollDescription;

@end


@interface HIDEvent (HIDUtilLEDEvent)

@property NSNumber *ledMask;
@property NSNumber *ledState;
@property NSNumber *ledNumber;

- (NSString *)ledDescription;

@end


@interface HIDEvent (HIDUtilOrientationEvent)

@property (readonly) NSNumber *orientationOrientationType;
@property NSNumber *orientationTiltZ;
@property NSNumber *orientationTiltY;
@property NSNumber *orientationTiltX;
@property NSNumber *orientationAzimuth;
@property NSNumber *orientationQuatZ;
@property NSNumber *orientationQuatY;
@property NSNumber *orientationQuatX;
@property NSNumber *orientationQuatW;
@property NSNumber *orientationDeviceOrientationUsage;
@property NSNumber *orientationAltitude;
@property NSNumber *orientationRadius;

- (NSString *)orientationDescription;

@end


@interface HIDEvent (HIDUtilProximityEvent)

@property NSNumber *proximityLevel;
@property NSNumber *proximityDetectionMask;

- (NSString *)proximityDescription;

@end


@interface HIDEvent (HIDUtilFluidTouchGestureEvent)

@property NSNumber *fluidTouchGesturePositionY;
@property NSNumber *fluidTouchGesturePositionX;
@property NSNumber *fluidTouchGestureMask;
@property NSNumber *fluidTouchGestureProgress;
@property NSNumber *fluidTouchGestureMotion;
@property NSNumber *fluidTouchGestureFlavor;

- (NSString *)fluidTouchGestureDescription;

@end


@interface HIDEvent (HIDUtilDockSwipeEvent)

@property NSNumber *dockSwipeProgress;
@property NSNumber *dockSwipeMask;
@property NSNumber *dockSwipeMotion;
@property NSNumber *dockSwipeFlavor;
@property NSNumber *dockSwipePositionX;
@property NSNumber *dockSwipePositionY;
@property NSNumber *dockSwipePositionZ;

- (NSString *)dockSwipeDescription;

@end


@interface HIDEvent (HIDUtilUnicodeEvent)

@property (readonly) uint8_t *unicodePayload;
@property NSNumber *unicodeLength;
@property NSNumber *unicodeQuality;
@property NSNumber *unicodeEncoding;

- (NSString *)unicodeDescription;

@end


@interface HIDEvent (HIDUtilKeyboardEvent)

@property NSNumber *keyboardStickyKeyPhase;
@property NSNumber *keyboardStickyKeyToggle;
@property NSNumber *keyboardMouseKeyToggle;
@property NSNumber *keyboardClickSpeed;
@property NSNumber *keyboardPressCount;
@property NSNumber *keyboardLongPress;
@property NSNumber *keyboardUsagePage;
@property NSNumber *keyboardSlowKeyPhase;
@property NSNumber *keyboardDown;
@property NSNumber *keyboardRepeat;
@property NSNumber *keyboardUsage;

- (NSString *)keyboardDescription;

@end


@interface HIDEvent (HIDUtilPointerEvent)

@property NSNumber *pointerZ;
@property NSNumber *pointerY;
@property NSNumber *pointerX;
@property NSNumber *pointerButtonMask;

- (NSString *)pointerDescription;

@end


