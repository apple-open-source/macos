//
//  HIDEvent.m
//  hidutil-internal
//

#import "HIDEvent.h"
#import <IOKit/hid/AppleHIDUsageTables.h>
#import <IOKit/hid/IOHIDLibPrivate.h>

@implementation HIDEvent (HIDUtil)

-(void)setTimestamp:(NSNumber *)timestamp {
    IOHIDEventSetTimeStamp((__bridge IOHIDEventRef)self, timestamp.unsignedLongLongValue);
}

- (NSNumber *)timestamp {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetTimeStamp((__bridge IOHIDEventRef)self)];
}

- (void)setSender:(NSNumber *)sender {
    IOHIDEventSetSenderID((__bridge IOHIDEventRef)self, sender.unsignedLongLongValue);
}

- (NSNumber *)sender {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetSenderID((__bridge IOHIDEventRef)self)];
}

- (NSNumber *)typeval {
    return [NSNumber numberWithInt:IOHIDEventGetType((__bridge IOHIDEventRef)self)];
}

- (NSNumber *)latency {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetLatency((__bridge IOHIDEventRef)self, kMicrosecondScale)];
}

- (void)setFlags:(NSNumber *)flags {
    IOHIDEventSetEventFlags((__bridge IOHIDEventRef)self, flags.unsignedIntValue);
}

- (NSNumber *)flags {
    return [NSNumber numberWithInt:IOHIDEventGetEventFlags((__bridge IOHIDEventRef)self)];
}

- (NSString *)typestr {
    return [[NSString stringWithUTF8String:IOHIDEventGetTypeString(IOHIDEventGetType((__bridge IOHIDEventRef)self))] lowercaseString];
}

- (NSString *)description {
    NSMutableString *desc = [NSMutableString new];
    
    [desc appendString: [NSString stringWithFormat:@"timestamp:%llu sender:0x%llx typeval:%d typestr:%@ latency:%llu flags:0x%08x ", self.timestamp.unsignedLongLongValue, self.sender.unsignedLongLongValue, self.typeval.unsignedIntValue, self.typestr, self.latency.unsignedLongLongValue, self.flags.unsignedIntValue]];
    
    switch (self.typeval.unsignedIntValue) {
        case kIOHIDEventTypeNULL:
            [desc appendString:[self nullDescription]];
            break;
        case kIOHIDEventTypeVendorDefined:
            [desc appendString:[self vendorDefinedDescription]];
            break;
        case kIOHIDEventTypeButton:
            [desc appendString:[self buttonDescription]];
            break;
        case kIOHIDEventTypeKeyboard:
            [desc appendString:[self keyboardDescription]];
            break;
        case kIOHIDEventTypeTranslation:
            [desc appendString:[self translationDescription]];
            break;
        case kIOHIDEventTypeRotation:
            [desc appendString:[self rotationDescription]];
            break;
        case kIOHIDEventTypeScroll:
            [desc appendString:[self scrollDescription]];
            break;
        case kIOHIDEventTypeScale:
            [desc appendString:[self scaleDescription]];
            break;
        case kIOHIDEventTypeVelocity:
            [desc appendString:[self velocityDescription]];
            break;
        case kIOHIDEventTypeOrientation:
            [desc appendString:[self orientationDescription]];
            break;
        case kIOHIDEventTypeDigitizer:
            [desc appendString:[self digitizerDescription]];
            break;
        case kIOHIDEventTypeAmbientLightSensor:
            [desc appendString:[self ambientLightSensorDescription]];
            break;
        case kIOHIDEventTypeAccelerometer:
            [desc appendString:[self accelerometerDescription]];
            break;
        case kIOHIDEventTypeProximity:
            [desc appendString:[self proximityDescription]];
            break;
        case kIOHIDEventTypeTemperature:
            [desc appendString:[self temperatureDescription]];
            break;
        case kIOHIDEventTypeNavigationSwipe:
            [desc appendString:[self navigationSwipeDescription]];
            break;
        case kIOHIDEventTypePointer:
            [desc appendString:[self pointerDescription]];
            break;
        case kIOHIDEventTypeProgress:
            [desc appendString:[self progressDescription]];
            break;
        case kIOHIDEventTypeMultiAxisPointer:
            [desc appendString:[self multiAxisPointerDescription]];
            break;
        case kIOHIDEventTypeGyro:
            [desc appendString:[self gyroDescription]];
            break;
        case kIOHIDEventTypeCompass:
            [desc appendString:[self compassDescription]];
            break;
        case kIOHIDEventTypeDockSwipe:
            [desc appendString:[self dockSwipeDescription]];
            break;
        case kIOHIDEventTypeSymbolicHotKey:
            [desc appendString:[self symbolicHotKeyDescription]];
            break;
        case kIOHIDEventTypePower:
            [desc appendString:[self powerDescription]];
            break;
        case kIOHIDEventTypeLED:
            [desc appendString:[self ledDescription]];
            break;
        case kIOHIDEventTypeFluidTouchGesture:
            [desc appendString:[self fluidTouchGestureDescription]];
            break;
        case kIOHIDEventTypeBoundaryScroll:
            [desc appendString:[self boundaryScrollDescription]];
            break;
        case kIOHIDEventTypeBiometric:
            [desc appendString:[self biometricDescription]];
            break;
        case kIOHIDEventTypeUnicode:
            [desc appendString:[self unicodeDescription]];
            break;
        case kIOHIDEventTypeAtmosphericPressure:
            [desc appendString:[self atmosphericPressureDescription]];
            break;
        case kIOHIDEventTypeForce:
            [desc appendString:[self forceDescription]];
            break;
        case kIOHIDEventTypeMotionActivity:
            [desc appendString:[self motionActivityDescription]];
            break;
        case kIOHIDEventTypeMotionGesture:
            [desc appendString:[self motionGestureDescription]];
            break;
        case kIOHIDEventTypeGameController:
            [desc appendString:[self gameControllerDescription]];
            break;
        case kIOHIDEventTypeHumidity:
            [desc appendString:[self humidityDescription]];
            break;
        case kIOHIDEventTypeBrightness:
            [desc appendString:[self brightnessDescription]];
            break;
        case kIOHIDEventTypeGenericGesture:
            [desc appendString:[self genericGestureDescription]];
            break;
        default:
            break;
    }
    
    return desc;
}

@end

@implementation HIDEvent (HIDUtilVendorDefinedEvent)

- (NSNumber *)vendorDefinedDataLength {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedDataLength)];
}

- (NSNumber *)vendorDefinedVersion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedVersion)];
}

- (void)setVendorDefinedVersion:(NSNumber *)vendorDefinedVersion {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedVersion, vendorDefinedVersion.unsignedIntValue);
}

- (NSNumber *)vendorDefinedUsage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedUsage)];
}

- (void)setVendorDefinedUsage:(NSNumber *)vendorDefinedUsage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedUsage, vendorDefinedUsage.unsignedIntValue);
}

- (uint8_t *)vendorDefinedData {
    return IOHIDEventGetDataValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedData);
}

- (NSString *)vendorDefinedDatastr {
    NSString *vendorDefinedDatastr = [[NSString alloc] init];
    uint8_t *vendorDefinedData = self.vendorDefinedData;

    for (uint32_t i = 0; i < self.vendorDefinedDataLength.unsignedIntValue; i++) {
        vendorDefinedDatastr = [vendorDefinedDatastr stringByAppendingString:[NSString stringWithFormat:@"%02x ", vendorDefinedData[i]]];
    }

    return [vendorDefinedDatastr substringToIndex:vendorDefinedDatastr.length-1];
}

- (NSNumber *)vendorDefinedUsagePage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedUsagePage)];
}

- (void)setVendorDefinedUsagePage:(NSNumber *)vendorDefinedUsagePage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVendorDefinedUsagePage, vendorDefinedUsagePage.unsignedIntValue);
}

- (NSString *)vendorDefinedDescription {
    NSString * desc;
    if (self.vendorDefinedUsagePage.unsignedIntValue == kHIDPage_AppleVendor && self.vendorDefinedUsage.unsignedIntValue == kHIDUsage_AppleVendor_Perf) {
        IOHIDEventPerfData *perfData = (IOHIDEventPerfData *)self.vendorDefinedData;
        NSString *perfStr = [NSString stringWithFormat:@"driverDispatchTime:%llu eventSystemReceiveTime:%llu eventSystemDispatchTime:%llu eventSystemFilterTime:%llu eventSystemClientDispatchTime:%llu",
            perfData->driverDispatchTime ? _IOHIDGetTimestampDelta(perfData->driverDispatchTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,
            perfData->eventSystemReceiveTime ? _IOHIDGetTimestampDelta(perfData->eventSystemReceiveTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,
            perfData->eventSystemDispatchTime ? _IOHIDGetTimestampDelta(perfData->eventSystemDispatchTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,
            perfData->eventSystemFilterTime ? _IOHIDGetTimestampDelta(perfData->eventSystemFilterTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0,
            perfData->eventSystemClientDispatchTime ? _IOHIDGetTimestampDelta(perfData->eventSystemClientDispatchTime, self.timestamp.unsignedLongLongValue, kMicrosecondScale) : 0];

        desc = [NSString stringWithFormat:@"vendorDefinedUsagePage:%@ vendorDefinedUsage:%@ vendorDefinedVersion:%@ vendorDefinedDataLength:%@ %@", self.vendorDefinedUsagePage, self.vendorDefinedUsage, self.vendorDefinedVersion, self.vendorDefinedDataLength, perfStr];
    } else {
        desc = [NSString stringWithFormat:@"vendorDefinedUsagePage:%@ vendorDefinedUsage:%@ vendorDefinedVersion:%@ vendorDefinedDataLength:%@ vendorDefinedDatastr:%@", self.vendorDefinedUsagePage, self.vendorDefinedUsage, self.vendorDefinedVersion, self.vendorDefinedDataLength, self.vendorDefinedDatastr];
    }
    return desc;
}
@end

@implementation HIDEvent (HIDUtilScaleEvent)

- (NSNumber *)scaleZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScaleZ)];
}

- (void)setScaleZ:(NSNumber *)scaleZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScaleZ, scaleZ.floatValue);
}

- (NSNumber *)scaleX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScaleX)];
}

- (void)setScaleX:(NSNumber *)scaleX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScaleX, scaleX.floatValue);
}

- (NSNumber *)scaleY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScaleY)];
}

- (void)setScaleY:(NSNumber *)scaleY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScaleY, scaleY.floatValue);
}

- (NSString *)scaleDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"scaleX:%@ scaleY:%@ scaleZ:%@", self.scaleX, self.scaleY, self.scaleZ];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilSymbolicHotKeyEvent)

- (NSNumber *)symbolicHotKeyValue {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldSymbolicHotKeyValue)];
}

- (void)setSymbolicHotKeyValue:(NSNumber *)symbolicHotKeyValue {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldSymbolicHotKeyValue, symbolicHotKeyValue.unsignedIntValue);
}

- (NSNumber *)symbolicHotKeyIsCGSEvent {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldSymbolicHotKeyIsCGSEvent)];
}

- (void)setSymbolicHotKeyIsCGSEvent:(NSNumber *)symbolicHotKeyIsCGSEvent {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldSymbolicHotKeyIsCGSEvent, symbolicHotKeyIsCGSEvent.unsignedIntValue);
}

- (NSString *)symbolicHotKeyDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"symbolicHotKeyIsCGSEvent:%@ symbolicHotKeyValue:%@", self.symbolicHotKeyIsCGSEvent, self.symbolicHotKeyValue];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilTemperatureEvent)

- (NSNumber *)temperatureLevel {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTemperatureLevel)];
}

- (void)setTemperatureLevel:(NSNumber *)temperatureLevel {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTemperatureLevel, temperatureLevel.floatValue);
}

- (NSString *)temperatureDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"temperatureLevel:%@", self.temperatureLevel];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilAccelerometerEvent)

- (NSNumber *)accelerometerY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerY)];
}

- (void)setAccelerometerY:(NSNumber *)accelerometerY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerY, accelerometerY.floatValue);
}

- (NSNumber *)accelerometerX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerX)];
}

- (void)setAccelerometerX:(NSNumber *)accelerometerX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerX, accelerometerX.floatValue);
}

- (NSNumber *)accelerometerZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerZ)];
}

- (void)setAccelerometerZ:(NSNumber *)accelerometerZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerZ, accelerometerZ.floatValue);
}

- (NSNumber *)accelerometerType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerType)];
}

- (void)setAccelerometerType:(NSNumber *)accelerometerType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerType, accelerometerType.unsignedIntValue);
}

- (NSNumber *)accelerometerSubType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerSubType)];
}

- (void)setAccelerometerSubType:(NSNumber *)accelerometerSubType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerSubType, accelerometerSubType.unsignedIntValue);
}

- (NSNumber *)accelerometerSequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerSequence)];
}

- (void)setAccelerometerSequence:(NSNumber *)accelerometerSequence {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAccelerometerSequence, accelerometerSequence.unsignedIntValue);
}

- (NSString *)accelerometerDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"accelerometerX:%@ accelerometerY:%@ accelerometerZ:%@ accelerometerType:%@ accelerometerSubType:%@ accelerometerSequence:%@", self.accelerometerX, self.accelerometerY, self.accelerometerZ, self.accelerometerType, self.accelerometerSubType, self.accelerometerSequence];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilProgressEvent)

- (NSNumber *)progressLevel {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProgressLevel)];
}

- (void)setProgressLevel:(NSNumber *)progressLevel {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProgressLevel, progressLevel.floatValue);
}

- (NSNumber *)progressEventType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProgressEventType)];
}

- (void)setProgressEventType:(NSNumber *)progressEventType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProgressEventType, progressEventType.unsignedIntValue);
}

- (NSString *)progressDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"progressEventType:%@ progressLevel:%@", self.progressEventType, self.progressLevel];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilGenericGestureEvent)

- (NSNumber *)genericGestureTypeTapCount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureTypeTapCount)];
}

- (void)setGenericGestureTypeTapCount:(NSNumber *)genericGestureTypeTapCount {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureTypeTapCount, genericGestureTypeTapCount.unsignedIntValue);
}

- (NSNumber *)genericGestureType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureType)];
}

- (NSNumber *)genericGestureTypeSwipeProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureTypeSwipeProgress)];
}

- (void)setGenericGestureTypeSwipeProgress:(NSNumber *)genericGestureTypeSwipeProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureTypeSwipeProgress, genericGestureTypeSwipeProgress.floatValue);
}

- (NSString *)genericGestureDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"genericGestureType:%@", self.genericGestureType];
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureType) == kIOHIDGenericGestureTypeTap) {
        desc = [NSString stringWithFormat:@"%@ genericGestureTypeTapCount:%@", desc, self.genericGestureTypeTapCount];
    }
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGenericGestureType) == kIOHIDGenericGestureTypeSwipe) {
        desc = [NSString stringWithFormat:@"%@ genericGestureTypeSwipeProgress:%@", desc, self.genericGestureTypeSwipeProgress];
    }
    return desc;
}
@end

@implementation HIDEvent (HIDUtilNULLEvent)

- (NSNumber *)isRelative {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsRelative)];
}

- (void)setIsRelative:(NSNumber *)isRelative {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsRelative, isRelative.unsignedIntValue);
}

- (NSNumber *)isCenterOrigin {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsCenterOrigin)];
}

- (void)setIsCenterOrigin:(NSNumber *)isCenterOrigin {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsCenterOrigin, isCenterOrigin.unsignedIntValue);
}

- (NSNumber *)isBuiltIn {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsBuiltIn)];
}

- (void)setIsBuiltIn:(NSNumber *)isBuiltIn {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsBuiltIn, isBuiltIn.unsignedIntValue);
}

- (NSNumber *)isPixelUnits {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsPixelUnits)];
}

- (void)setIsPixelUnits:(NSNumber *)isPixelUnits {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsPixelUnits, isPixelUnits.unsignedIntValue);
}

- (NSNumber *)isCollection {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsCollection)];
}

- (void)setIsCollection:(NSNumber *)isCollection {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldIsCollection, isCollection.unsignedIntValue);
}

- (NSString *)nullDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"isRelative:%@ isCollection:%@ isPixelUnits:%@ isCenterOrigin:%@ isBuiltIn:%@", self.isRelative, self.isCollection, self.isPixelUnits, self.isCenterOrigin, self.isBuiltIn];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilAmbientLightSensorEvent)

- (NSNumber *)ambientLightColorSpace {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorSpace)];
}

- (void)setAmbientLightColorSpace:(NSNumber *)ambientLightColorSpace {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorSpace, ambientLightColorSpace.unsignedIntValue);
}

- (NSNumber *)ambientLightColorComponent2 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorComponent2)];
}

- (void)setAmbientLightColorComponent2:(NSNumber *)ambientLightColorComponent2 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorComponent2, ambientLightColorComponent2.floatValue);
}

- (NSNumber *)ambientLightColorComponent1 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorComponent1)];
}

- (void)setAmbientLightColorComponent1:(NSNumber *)ambientLightColorComponent1 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorComponent1, ambientLightColorComponent1.floatValue);
}

- (NSNumber *)ambientLightColorComponent0 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorComponent0)];
}

- (void)setAmbientLightColorComponent0:(NSNumber *)ambientLightColorComponent0 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightColorComponent0, ambientLightColorComponent0.floatValue);
}

- (NSNumber *)ambientLightSensorRawChannel0 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel0)];
}

- (void)setAmbientLightSensorRawChannel0:(NSNumber *)ambientLightSensorRawChannel0 {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel0, ambientLightSensorRawChannel0.unsignedIntValue);
}

- (NSNumber *)ambientLightSensorRawChannel1 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel1)];
}

- (void)setAmbientLightSensorRawChannel1:(NSNumber *)ambientLightSensorRawChannel1 {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel1, ambientLightSensorRawChannel1.unsignedIntValue);
}

- (NSNumber *)ambientLightSensorRawChannel2 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel2)];
}

- (void)setAmbientLightSensorRawChannel2:(NSNumber *)ambientLightSensorRawChannel2 {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel2, ambientLightSensorRawChannel2.unsignedIntValue);
}

- (NSNumber *)ambientLightSensorRawChannel3 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel3)];
}

- (void)setAmbientLightSensorRawChannel3:(NSNumber *)ambientLightSensorRawChannel3 {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorRawChannel3, ambientLightSensorRawChannel3.unsignedIntValue);
}

- (NSNumber *)ambientLightSensorLevel {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorLevel)];
}

- (void)setAmbientLightSensorLevel:(NSNumber *)ambientLightSensorLevel {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorLevel, ambientLightSensorLevel.unsignedIntValue);
}

- (NSNumber *)ambientLightSensorIlluminance {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorIlluminance)];
}

- (void)setAmbientLightSensorIlluminance:(NSNumber *)ambientLightSensorIlluminance {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorIlluminance, ambientLightSensorIlluminance.floatValue);
}

- (NSNumber *)ambientLightDisplayBrightnessChanged {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightDisplayBrightnessChanged)];
}

- (void)setAmbientLightDisplayBrightnessChanged:(NSNumber *)ambientLightDisplayBrightnessChanged {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightDisplayBrightnessChanged, ambientLightDisplayBrightnessChanged.unsignedIntValue);
}

- (NSNumber *)ambientLightSensorColorTemperature {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorColorTemperature)];
}

- (void)setAmbientLightSensorColorTemperature:(NSNumber *)ambientLightSensorColorTemperature {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAmbientLightSensorColorTemperature, ambientLightSensorColorTemperature.floatValue);
}

- (NSString *)ambientLightSensorDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"ambientLightSensorLevel:%@ ambientLightSensorRawChannel0:%@ ambientLightSensorRawChannel1:%@ ambientLightSensorRawChannel2:%@ ambientLightSensorRawChannel3:%@ ambientLightDisplayBrightnessChanged:%@ ambientLightColorSpace:%@ ambientLightColorComponent0:%@ ambientLightColorComponent1:%@ ambientLightColorComponent2:%@ ambientLightSensorColorTemperature:%@ ambientLightSensorIlluminance:%@", self.ambientLightSensorLevel, self.ambientLightSensorRawChannel0, self.ambientLightSensorRawChannel1, self.ambientLightSensorRawChannel2, self.ambientLightSensorRawChannel3, self.ambientLightDisplayBrightnessChanged, self.ambientLightColorSpace, self.ambientLightColorComponent0, self.ambientLightColorComponent1, self.ambientLightColorComponent2, self.ambientLightSensorColorTemperature, self.ambientLightSensorIlluminance];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilPowerEvent)

- (NSNumber *)powerType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPowerType)];
}

- (void)setPowerType:(NSNumber *)powerType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPowerType, powerType.unsignedIntValue);
}

- (NSNumber *)powerSubType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPowerSubType)];
}

- (void)setPowerSubType:(NSNumber *)powerSubType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPowerSubType, powerSubType.unsignedIntValue);
}

- (NSNumber *)powerMeasurement {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPowerMeasurement)];
}

- (void)setPowerMeasurement:(NSNumber *)powerMeasurement {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPowerMeasurement, powerMeasurement.floatValue);
}

- (NSString *)powerDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"powerMeasurement:%@ powerType:%@ powerSubType:%@", self.powerMeasurement, self.powerType, self.powerSubType];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilForceEvent)

- (NSNumber *)forceStagePressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceStagePressure)];
}

- (void)setForceStagePressure:(NSNumber *)forceStagePressure {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceStagePressure, forceStagePressure.floatValue);
}

- (NSNumber *)forceStage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceStage)];
}

- (void)setForceStage:(NSNumber *)forceStage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceStage, forceStage.unsignedIntValue);
}

- (NSNumber *)forceProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceProgress)];
}

- (void)setForceProgress:(NSNumber *)forceProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceProgress, forceProgress.floatValue);
}

- (NSNumber *)forceBehavior {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceBehavior)];
}

- (void)setForceBehavior:(NSNumber *)forceBehavior {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldForceBehavior, forceBehavior.unsignedIntValue);
}

- (NSString *)forceDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"forceBehavior:%@ forceProgress:%@ forceStage:%@ forceStagePressure:%@", self.forceBehavior, self.forceProgress, self.forceStage, self.forceStagePressure];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilMotionGestureEvent)

- (NSNumber *)motionGestureProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionGestureProgress)];
}

- (void)setMotionGestureProgress:(NSNumber *)motionGestureProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionGestureProgress, motionGestureProgress.floatValue);
}

- (NSNumber *)motionGestureGestureType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionGestureGestureType)];
}

- (void)setMotionGestureGestureType:(NSNumber *)motionGestureGestureType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionGestureGestureType, motionGestureGestureType.unsignedIntValue);
}

- (NSString *)motionGestureDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"motionGestureGestureType:%@ motionGestureProgress:%@", self.motionGestureGestureType, self.motionGestureProgress];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilGameControllerEvent)

- (NSNumber *)gameControllerJoyStickAxisX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisX)];
}

- (void)setGameControllerJoyStickAxisX:(NSNumber *)gameControllerJoyStickAxisX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisX, gameControllerJoyStickAxisX.floatValue);
}

- (NSNumber *)gameControllerJoyStickAxisY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisY)];
}

- (void)setGameControllerJoyStickAxisY:(NSNumber *)gameControllerJoyStickAxisY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisY, gameControllerJoyStickAxisY.floatValue);
}

- (NSNumber *)gameControllerType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerType)];
}

- (void)setGameControllerType:(NSNumber *)gameControllerType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerType, gameControllerType.unsignedIntValue);
}

- (NSNumber *)gameControllerDirectionPadRight {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadRight)];
}

- (void)setGameControllerDirectionPadRight:(NSNumber *)gameControllerDirectionPadRight {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadRight, gameControllerDirectionPadRight.floatValue);
}

- (NSNumber *)gameControllerShoulderButtonR1 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonR1)];
}

- (void)setGameControllerShoulderButtonR1:(NSNumber *)gameControllerShoulderButtonR1 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonR1, gameControllerShoulderButtonR1.floatValue);
}

- (NSNumber *)gameControllerFaceButtonA {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonA)];
}

- (void)setGameControllerFaceButtonA:(NSNumber *)gameControllerFaceButtonA {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonA, gameControllerFaceButtonA.floatValue);
}

- (NSNumber *)gameControllerFaceButtonB {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonB)];
}

- (void)setGameControllerFaceButtonB:(NSNumber *)gameControllerFaceButtonB {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonB, gameControllerFaceButtonB.floatValue);
}

- (NSNumber *)gameControllerDirectionPadLeft {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadLeft)];
}

- (void)setGameControllerDirectionPadLeft:(NSNumber *)gameControllerDirectionPadLeft {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadLeft, gameControllerDirectionPadLeft.floatValue);
}

- (NSNumber *)gameControllerThumbstickButtonRight {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerThumbstickButtonRight)];
}

- (void)setGameControllerThumbstickButtonRight:(NSNumber *)gameControllerThumbstickButtonRight {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerThumbstickButtonRight, gameControllerThumbstickButtonRight.unsignedIntValue);
}

- (NSNumber *)gameControllerDirectionPadDown {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadDown)];
}

- (void)setGameControllerDirectionPadDown:(NSNumber *)gameControllerDirectionPadDown {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadDown, gameControllerDirectionPadDown.floatValue);
}

- (NSNumber *)gameControllerThumbstickButtonLeft {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerThumbstickButtonLeft)];
}

- (void)setGameControllerThumbstickButtonLeft:(NSNumber *)gameControllerThumbstickButtonLeft {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerThumbstickButtonLeft, gameControllerThumbstickButtonLeft.unsignedIntValue);
}

- (NSNumber *)gameControllerJoyStickAxisZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisZ)];
}

- (void)setGameControllerJoyStickAxisZ:(NSNumber *)gameControllerJoyStickAxisZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisZ, gameControllerJoyStickAxisZ.floatValue);
}

- (NSNumber *)gameControllerShoulderButtonR2 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonR2)];
}

- (void)setGameControllerShoulderButtonR2:(NSNumber *)gameControllerShoulderButtonR2 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonR2, gameControllerShoulderButtonR2.floatValue);
}

- (NSNumber *)gameControllerFaceButtonY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonY)];
}

- (void)setGameControllerFaceButtonY:(NSNumber *)gameControllerFaceButtonY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonY, gameControllerFaceButtonY.floatValue);
}

- (NSNumber *)gameControllerShoulderButtonL2 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonL2)];
}

- (void)setGameControllerShoulderButtonL2:(NSNumber *)gameControllerShoulderButtonL2 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonL2, gameControllerShoulderButtonL2.floatValue);
}

- (NSNumber *)gameControllerJoyStickAxisRz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisRz)];
}

- (void)setGameControllerJoyStickAxisRz:(NSNumber *)gameControllerJoyStickAxisRz {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerJoyStickAxisRz, gameControllerJoyStickAxisRz.floatValue);
}

- (NSNumber *)gameControllerShoulderButtonL1 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonL1)];
}

- (void)setGameControllerShoulderButtonL1:(NSNumber *)gameControllerShoulderButtonL1 {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerShoulderButtonL1, gameControllerShoulderButtonL1.floatValue);
}

- (NSNumber *)gameControllerFaceButtonX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonX)];
}

- (void)setGameControllerFaceButtonX:(NSNumber *)gameControllerFaceButtonX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerFaceButtonX, gameControllerFaceButtonX.floatValue);
}

- (NSNumber *)gameControllerDirectionPadUp {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadUp)];
}

- (void)setGameControllerDirectionPadUp:(NSNumber *)gameControllerDirectionPadUp {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGameControllerDirectionPadUp, gameControllerDirectionPadUp.floatValue);
}

- (NSString *)gameControllerDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"gameControllerType:%@ gameControllerDirectionPadUp:%@ gameControllerDirectionPadDown:%@ gameControllerDirectionPadLeft:%@ gameControllerDirectionPadRight:%@ gameControllerFaceButtonX:%@ gameControllerFaceButtonY:%@ gameControllerFaceButtonA:%@ gameControllerFaceButtonB:%@ gameControllerJoyStickAxisX:%@ gameControllerJoyStickAxisY:%@ gameControllerJoyStickAxisZ:%@ gameControllerJoyStickAxisRz:%@ gameControllerShoulderButtonL1:%@ gameControllerShoulderButtonL2:%@ gameControllerShoulderButtonR1:%@ gameControllerShoulderButtonR2:%@ gameControllerThumbstickButtonLeft:%@ gameControllerThumbstickButtonRight:%@", self.gameControllerType, self.gameControllerDirectionPadUp, self.gameControllerDirectionPadDown, self.gameControllerDirectionPadLeft, self.gameControllerDirectionPadRight, self.gameControllerFaceButtonX, self.gameControllerFaceButtonY, self.gameControllerFaceButtonA, self.gameControllerFaceButtonB, self.gameControllerJoyStickAxisX, self.gameControllerJoyStickAxisY, self.gameControllerJoyStickAxisZ, self.gameControllerJoyStickAxisRz, self.gameControllerShoulderButtonL1, self.gameControllerShoulderButtonL2, self.gameControllerShoulderButtonR1, self.gameControllerShoulderButtonR2, self.gameControllerThumbstickButtonLeft, self.gameControllerThumbstickButtonRight];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilTranslationEvent)

- (NSNumber *)translationY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTranslationY)];
}

- (void)setTranslationY:(NSNumber *)translationY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTranslationY, translationY.floatValue);
}

- (NSNumber *)translationX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTranslationX)];
}

- (void)setTranslationX:(NSNumber *)translationX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTranslationX, translationX.floatValue);
}

- (NSNumber *)translationZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTranslationZ)];
}

- (void)setTranslationZ:(NSNumber *)translationZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTranslationZ, translationZ.floatValue);
}

- (NSString *)translationDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"translationX:%@ translationY:%@ translationZ:%@", self.translationX, self.translationY, self.translationZ];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilDigitizerEvent)

- (NSNumber *)digitizerType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerType)];
}

- (void)setDigitizerType:(NSNumber *)digitizerType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerType, digitizerType.unsignedIntValue);
}

- (NSNumber *)digitizerChildEventMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerChildEventMask)];
}

- (void)setDigitizerChildEventMask:(NSNumber *)digitizerChildEventMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerChildEventMask, digitizerChildEventMask.unsignedIntValue);
}

- (NSNumber *)digitizerAuxiliaryPressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerAuxiliaryPressure)];
}

- (void)setDigitizerAuxiliaryPressure:(NSNumber *)digitizerAuxiliaryPressure {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerAuxiliaryPressure, digitizerAuxiliaryPressure.floatValue);
}

- (NSNumber *)digitizerQualityRadiiAccuracy {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerQualityRadiiAccuracy)];
}

- (void)setDigitizerQualityRadiiAccuracy:(NSNumber *)digitizerQualityRadiiAccuracy {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerQualityRadiiAccuracy, digitizerQualityRadiiAccuracy.floatValue);
}

- (NSNumber *)digitizerQuality {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerQuality)];
}

- (void)setDigitizerQuality:(NSNumber *)digitizerQuality {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerQuality, digitizerQuality.floatValue);
}

- (NSNumber *)digitizerMinorRadius {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerMinorRadius)];
}

- (void)setDigitizerMinorRadius:(NSNumber *)digitizerMinorRadius {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerMinorRadius, digitizerMinorRadius.floatValue);
}

- (NSNumber *)digitizerEventMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerEventMask)];
}

- (void)setDigitizerEventMask:(NSNumber *)digitizerEventMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerEventMask, digitizerEventMask.unsignedIntValue);
}

- (NSNumber *)digitizerGenerationCount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerGenerationCount)];
}

- (void)setDigitizerGenerationCount:(NSNumber *)digitizerGenerationCount {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerGenerationCount, digitizerGenerationCount.unsignedIntValue);
}

- (NSNumber *)digitizerIndex {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIndex)];
}

- (void)setDigitizerIndex:(NSNumber *)digitizerIndex {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIndex, digitizerIndex.unsignedIntValue);
}

- (NSNumber *)digitizerTouch {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTouch)];
}

- (void)setDigitizerTouch:(NSNumber *)digitizerTouch {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTouch, digitizerTouch.unsignedIntValue);
}

- (NSNumber *)digitizerAzimuth {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerAzimuth)];
}

- (void)setDigitizerAzimuth:(NSNumber *)digitizerAzimuth {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerAzimuth, digitizerAzimuth.floatValue);
}

- (NSNumber *)digitizerTiltX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTiltX)];
}

- (void)setDigitizerTiltX:(NSNumber *)digitizerTiltX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTiltX, digitizerTiltX.floatValue);
}

- (NSNumber *)digitizerTiltY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTiltY)];
}

- (void)setDigitizerTiltY:(NSNumber *)digitizerTiltY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTiltY, digitizerTiltY.floatValue);
}

- (NSNumber *)digitizerRange {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerRange)];
}

- (void)setDigitizerRange:(NSNumber *)digitizerRange {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerRange, digitizerRange.unsignedIntValue);
}

- (NSNumber *)digitizerPressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerPressure)];
}

- (void)setDigitizerPressure:(NSNumber *)digitizerPressure {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerPressure, digitizerPressure.floatValue);
}

- (NSNumber *)digitizerCollection {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerCollection)];
}

- (void)setDigitizerCollection:(NSNumber *)digitizerCollection {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerCollection, digitizerCollection.unsignedIntValue);
}

- (NSNumber *)digitizerAltitude {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerAltitude)];
}

- (void)setDigitizerAltitude:(NSNumber *)digitizerAltitude {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerAltitude, digitizerAltitude.floatValue);
}

- (NSNumber *)digitizerDensity {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerDensity)];
}

- (void)setDigitizerDensity:(NSNumber *)digitizerDensity {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerDensity, digitizerDensity.floatValue);
}

- (NSNumber *)digitizerOrientationType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerOrientationType)];
}

- (NSNumber *)digitizerY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerY)];
}

- (void)setDigitizerY:(NSNumber *)digitizerY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerY, digitizerY.floatValue);
}

- (NSNumber *)digitizerWillUpdateMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerWillUpdateMask)];
}

- (void)setDigitizerWillUpdateMask:(NSNumber *)digitizerWillUpdateMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerWillUpdateMask, digitizerWillUpdateMask.unsignedIntValue);
}

- (NSNumber *)digitizerIdentity {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIdentity)];
}

- (void)setDigitizerIdentity:(NSNumber *)digitizerIdentity {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIdentity, digitizerIdentity.unsignedIntValue);
}

- (NSNumber *)digitizerTwist {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTwist)];
}

- (void)setDigitizerTwist:(NSNumber *)digitizerTwist {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerTwist, digitizerTwist.floatValue);
}

- (NSNumber *)digitizerX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerX)];
}

- (void)setDigitizerX:(NSNumber *)digitizerX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerX, digitizerX.floatValue);
}

- (NSNumber *)digitizerIsDisplayIntegrated {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIsDisplayIntegrated)];
}

- (void)setDigitizerIsDisplayIntegrated:(NSNumber *)digitizerIsDisplayIntegrated {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIsDisplayIntegrated, digitizerIsDisplayIntegrated.unsignedIntValue);
}

- (NSNumber *)digitizerZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerZ)];
}

- (void)setDigitizerZ:(NSNumber *)digitizerZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerZ, digitizerZ.floatValue);
}

- (NSNumber *)digitizerMajorRadius {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerMajorRadius)];
}

- (void)setDigitizerMajorRadius:(NSNumber *)digitizerMajorRadius {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerMajorRadius, digitizerMajorRadius.floatValue);
}

- (NSNumber *)digitizerButtonMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerButtonMask)];
}

- (void)setDigitizerButtonMask:(NSNumber *)digitizerButtonMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerButtonMask, digitizerButtonMask.unsignedIntValue);
}

- (NSNumber *)digitizerIrregularity {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIrregularity)];
}

- (void)setDigitizerIrregularity:(NSNumber *)digitizerIrregularity {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerIrregularity, digitizerIrregularity.floatValue);
}

- (NSNumber *)digitizerDidUpdateMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerDidUpdateMask)];
}

- (void)setDigitizerDidUpdateMask:(NSNumber *)digitizerDidUpdateMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerDidUpdateMask, digitizerDidUpdateMask.unsignedIntValue);
}

- (NSString *)digitizerDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"digitizerCollection:%@ digitizerRange:%@ digitizerTouch:%@ digitizerIsDisplayIntegrated:%@ digitizerX:%@ digitizerY:%@ digitizerZ:%@ digitizerIndex:%@ digitizerType:%@ digitizerIdentity:%@ digitizerEventMask:%@ digitizerChildEventMask:%@ digitizerButtonMask:%@ digitizerPressure:%@ digitizerAuxiliaryPressure:%@ digitizerTwist:%@ digitizerOrientationType:%@ digitizerGenerationCount:%@ digitizerWillUpdateMask:%@ digitizerDidUpdateMask:%@", self.digitizerCollection, self.digitizerRange, self.digitizerTouch, self.digitizerIsDisplayIntegrated, self.digitizerX, self.digitizerY, self.digitizerZ, self.digitizerIndex, self.digitizerType, self.digitizerIdentity, self.digitizerEventMask, self.digitizerChildEventMask, self.digitizerButtonMask, self.digitizerPressure, self.digitizerAuxiliaryPressure, self.digitizerTwist, self.digitizerOrientationType, self.digitizerGenerationCount, self.digitizerWillUpdateMask, self.digitizerDidUpdateMask];
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerOrientationType) == kIOHIDDigitizerOrientationTypePolar) {
        desc = [NSString stringWithFormat:@"%@ digitizerAltitude:%@ digitizerAzimuth:%@ digitizerQuality:%@ digitizerDensity:%@ digitizerMajorRadius:%@ digitizerMinorRadius:%@", desc, self.digitizerAltitude, self.digitizerAzimuth, self.digitizerQuality, self.digitizerDensity, self.digitizerMajorRadius, self.digitizerMinorRadius];
    }
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerOrientationType) == kIOHIDDigitizerOrientationTypeQuality) {
        desc = [NSString stringWithFormat:@"%@ digitizerQuality:%@ digitizerDensity:%@ digitizerIrregularity:%@ digitizerMajorRadius:%@ digitizerMinorRadius:%@ digitizerQualityRadiiAccuracy:%@", desc, self.digitizerQuality, self.digitizerDensity, self.digitizerIrregularity, self.digitizerMajorRadius, self.digitizerMinorRadius, self.digitizerQualityRadiiAccuracy];
    }
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDigitizerOrientationType) == kIOHIDDigitizerOrientationTypeTilt) {
        desc = [NSString stringWithFormat:@"%@ digitizerTiltX:%@ digitizerTiltY:%@", desc, self.digitizerTiltX, self.digitizerTiltY];
    }
    return desc;
}
@end

@implementation HIDEvent (HIDUtilCompassEvent)

- (NSNumber *)compassType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassType)];
}

- (void)setCompassType:(NSNumber *)compassType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassType, compassType.unsignedIntValue);
}

- (NSNumber *)compassZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassZ)];
}

- (void)setCompassZ:(NSNumber *)compassZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassZ, compassZ.floatValue);
}

- (NSNumber *)compassX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassX)];
}

- (void)setCompassX:(NSNumber *)compassX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassX, compassX.floatValue);
}

- (NSNumber *)compassY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassY)];
}

- (void)setCompassY:(NSNumber *)compassY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassY, compassY.floatValue);
}

- (NSNumber *)compassSubType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassSubType)];
}

- (void)setCompassSubType:(NSNumber *)compassSubType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassSubType, compassSubType.unsignedIntValue);
}

- (NSNumber *)compassSequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassSequence)];
}

- (void)setCompassSequence:(NSNumber *)compassSequence {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCompassSequence, compassSequence.unsignedIntValue);
}

- (NSString *)compassDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"compassX:%@ compassY:%@ compassZ:%@ compassType:%@ compassSubType:%@ compassSequence:%@", self.compassX, self.compassY, self.compassZ, self.compassType, self.compassSubType, self.compassSequence];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilRotationEvent)

- (NSNumber *)rotationY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldRotationY)];
}

- (void)setRotationY:(NSNumber *)rotationY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldRotationY, rotationY.floatValue);
}

- (NSNumber *)rotationX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldRotationX)];
}

- (void)setRotationX:(NSNumber *)rotationX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldRotationX, rotationX.floatValue);
}

- (NSNumber *)rotationZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldRotationZ)];
}

- (void)setRotationZ:(NSNumber *)rotationZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldRotationZ, rotationZ.floatValue);
}

- (NSString *)rotationDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"rotationX:%@ rotationY:%@ rotationZ:%@", self.rotationX, self.rotationY, self.rotationZ];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilMotionActivityEvent)

- (NSNumber *)motionActivityConfidence {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionActivityConfidence)];
}

- (void)setMotionActivityConfidence:(NSNumber *)motionActivityConfidence {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionActivityConfidence, motionActivityConfidence.floatValue);
}

- (NSNumber *)motionActivityActivityType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionActivityActivityType)];
}

- (void)setMotionActivityActivityType:(NSNumber *)motionActivityActivityType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMotionActivityActivityType, motionActivityActivityType.unsignedIntValue);
}

- (NSString *)motionActivityDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"motionActivityActivityType:%@ motionActivityConfidence:%@", self.motionActivityActivityType, self.motionActivityConfidence];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilMultiAxisPointerEvent)

- (NSNumber *)multiAxisPointerRy {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerRy)];
}

- (void)setMultiAxisPointerRy:(NSNumber *)multiAxisPointerRy {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerRy, multiAxisPointerRy.floatValue);
}

- (NSNumber *)multiAxisPointerRx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerRx)];
}

- (void)setMultiAxisPointerRx:(NSNumber *)multiAxisPointerRx {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerRx, multiAxisPointerRx.floatValue);
}

- (NSNumber *)multiAxisPointerRz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerRz)];
}

- (void)setMultiAxisPointerRz:(NSNumber *)multiAxisPointerRz {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerRz, multiAxisPointerRz.floatValue);
}

- (NSNumber *)multiAxisPointerButtonMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerButtonMask)];
}

- (void)setMultiAxisPointerButtonMask:(NSNumber *)multiAxisPointerButtonMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerButtonMask, multiAxisPointerButtonMask.unsignedIntValue);
}

- (NSNumber *)multiAxisPointerZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerZ)];
}

- (void)setMultiAxisPointerZ:(NSNumber *)multiAxisPointerZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerZ, multiAxisPointerZ.floatValue);
}

- (NSNumber *)multiAxisPointerX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerX)];
}

- (void)setMultiAxisPointerX:(NSNumber *)multiAxisPointerX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerX, multiAxisPointerX.floatValue);
}

- (NSNumber *)multiAxisPointerY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerY)];
}

- (void)setMultiAxisPointerY:(NSNumber *)multiAxisPointerY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldMultiAxisPointerY, multiAxisPointerY.floatValue);
}

- (NSString *)multiAxisPointerDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"multiAxisPointerX:%@ multiAxisPointerY:%@ multiAxisPointerZ:%@ multiAxisPointerButtonMask:%@ multiAxisPointerRx:%@ multiAxisPointerRy:%@ multiAxisPointerRz:%@", self.multiAxisPointerX, self.multiAxisPointerY, self.multiAxisPointerZ, self.multiAxisPointerButtonMask, self.multiAxisPointerRx, self.multiAxisPointerRy, self.multiAxisPointerRz];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilBrightnessEvent)

- (NSNumber *)targetBrightness {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTargetBrightness)];
}

- (void)setTargetBrightness:(NSNumber *)targetBrightness {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTargetBrightness, targetBrightness.floatValue);
}

- (NSNumber *)currentBrightness {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCurrentBrightness)];
}

- (void)setCurrentBrightness:(NSNumber *)currentBrightness {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldCurrentBrightness, currentBrightness.floatValue);
}

- (NSNumber *)transitionTime {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTransitionTime)];
}

- (void)setTransitionTime:(NSNumber *)transitionTime {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldTransitionTime, transitionTime.unsignedIntValue);
}

- (NSString *)brightnessDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"currentBrightness:%@ targetBrightness:%@ transitionTime:%@", self.currentBrightness, self.targetBrightness, self.transitionTime];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilGyroEvent)

- (NSNumber *)gyroX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroX)];
}

- (void)setGyroX:(NSNumber *)gyroX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroX, gyroX.floatValue);
}

- (NSNumber *)gyroY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroY)];
}

- (void)setGyroY:(NSNumber *)gyroY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroY, gyroY.floatValue);
}

- (NSNumber *)gyroZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroZ)];
}

- (void)setGyroZ:(NSNumber *)gyroZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroZ, gyroZ.floatValue);
}

- (NSNumber *)gyroSubType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroSubType)];
}

- (void)setGyroSubType:(NSNumber *)gyroSubType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroSubType, gyroSubType.unsignedIntValue);
}

- (NSNumber *)gyroSequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroSequence)];
}

- (void)setGyroSequence:(NSNumber *)gyroSequence {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroSequence, gyroSequence.unsignedIntValue);
}

- (NSNumber *)gyroType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroType)];
}

- (void)setGyroType:(NSNumber *)gyroType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldGyroType, gyroType.unsignedIntValue);
}

- (NSString *)gyroDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"gyroX:%@ gyroY:%@ gyroZ:%@ gyroType:%@ gyroSubType:%@ gyroSequence:%@", self.gyroX, self.gyroY, self.gyroZ, self.gyroType, self.gyroSubType, self.gyroSequence];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilButtonEvent)

- (NSNumber *)buttonPressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonPressure)];
}

- (void)setButtonPressure:(NSNumber *)buttonPressure {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonPressure, buttonPressure.floatValue);
}

- (NSNumber *)buttonClickCount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonClickCount)];
}

- (void)setButtonClickCount:(NSNumber *)buttonClickCount {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonClickCount, buttonClickCount.unsignedIntValue);
}

- (NSNumber *)buttonMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonMask)];
}

- (void)setButtonMask:(NSNumber *)buttonMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonMask, buttonMask.unsignedIntValue);
}

- (NSNumber *)buttonState {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonState)];
}

- (void)setButtonState:(NSNumber *)buttonState {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonState, buttonState.unsignedIntValue);
}

- (NSNumber *)buttonNumber {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonNumber)];
}

- (void)setButtonNumber:(NSNumber *)buttonNumber {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldButtonNumber, buttonNumber.unsignedIntValue);
}

- (NSString *)buttonDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"buttonMask:%@ buttonPressure:%@ buttonNumber:%@ buttonClickCount:%@ buttonState:%@", self.buttonMask, self.buttonPressure, self.buttonNumber, self.buttonClickCount, self.buttonState];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilNavigationSwipeEvent)

- (NSNumber *)navigationSwipeFlavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeFlavor)];
}

- (void)setNavigationSwipeFlavor:(NSNumber *)navigationSwipeFlavor {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeFlavor, navigationSwipeFlavor.unsignedIntValue);
}

- (NSNumber *)navigationSwipeProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeProgress)];
}

- (void)setNavigationSwipeProgress:(NSNumber *)navigationSwipeProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeProgress, navigationSwipeProgress.floatValue);
}

- (NSNumber *)navigationSwipeMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeMask)];
}

- (void)setNavigationSwipeMask:(NSNumber *)navigationSwipeMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeMask, navigationSwipeMask.unsignedIntValue);
}

- (NSNumber *)navigationSwipeMotion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeMotion)];
}

- (void)setNavigationSwipeMotion:(NSNumber *)navigationSwipeMotion {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipeMotion, navigationSwipeMotion.unsignedIntValue);
}

- (NSNumber *)navigationSwipePositionY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipePositionY)];
}

- (void)setNavigationSwipePositionY:(NSNumber *)navigationSwipePositionY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipePositionY, navigationSwipePositionY.floatValue);
}

- (NSNumber *)navigationSwipePositionX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipePositionX)];
}

- (void)setNavigationSwipePositionX:(NSNumber *)navigationSwipePositionX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipePositionX, navigationSwipePositionX.floatValue);
}

- (NSNumber *)navigationSwipePositionZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipePositionZ)];
}

- (void)setNavigationSwipePositionZ:(NSNumber *)navigationSwipePositionZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldNavigationSwipePositionZ, navigationSwipePositionZ.floatValue);
}

- (NSString *)navigationSwipeDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"navigationSwipePositionX:%@ navigationSwipePositionY:%@ navigationSwipePositionZ:%@ navigationSwipeMask:%@ navigationSwipeMotion:%@ navigationSwipeFlavor:%@ navigationSwipeProgress:%@", self.navigationSwipePositionX, self.navigationSwipePositionY, self.navigationSwipePositionZ, self.navigationSwipeMask, self.navigationSwipeMotion, self.navigationSwipeFlavor, self.navigationSwipeProgress];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilAtmosphericPressureEvent)

- (NSNumber *)atmosphericPressureLevel {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAtmosphericPressureLevel)];
}

- (void)setAtmosphericPressureLevel:(NSNumber *)atmosphericPressureLevel {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAtmosphericPressureLevel, atmosphericPressureLevel.floatValue);
}

- (NSNumber *)atmosphericSequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAtmosphericSequence)];
}

- (void)setAtmosphericSequence:(NSNumber *)atmosphericSequence {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldAtmosphericSequence, atmosphericSequence.unsignedIntValue);
}

- (NSString *)atmosphericPressureDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"atmosphericPressureLevel:%@ atmosphericSequence:%@", self.atmosphericPressureLevel, self.atmosphericSequence];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilHumidityEvent)

- (NSNumber *)humiditySequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldHumiditySequence)];
}

- (void)setHumiditySequence:(NSNumber *)humiditySequence {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldHumiditySequence, humiditySequence.unsignedIntValue);
}

- (NSNumber *)humidityRH {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldHumidityRH)];
}

- (void)setHumidityRH:(NSNumber *)humidityRH {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldHumidityRH, humidityRH.floatValue);
}

- (NSString *)humidityDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"humidityRH:%@ humiditySequence:%@", self.humidityRH, self.humiditySequence];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilVelocityEvent)

- (NSNumber *)velocityX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVelocityX)];
}

- (void)setVelocityX:(NSNumber *)velocityX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVelocityX, velocityX.floatValue);
}

- (NSNumber *)velocityY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVelocityY)];
}

- (void)setVelocityY:(NSNumber *)velocityY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVelocityY, velocityY.floatValue);
}

- (NSNumber *)velocityZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVelocityZ)];
}

- (void)setVelocityZ:(NSNumber *)velocityZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldVelocityZ, velocityZ.floatValue);
}

- (NSString *)velocityDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"velocityX:%@ velocityY:%@ velocityZ:%@", self.velocityX, self.velocityY, self.velocityZ];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilScrollEvent)

- (NSNumber *)scrollIsPixels {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollIsPixels)];
}

- (void)setScrollIsPixels:(NSNumber *)scrollIsPixels {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollIsPixels, scrollIsPixels.unsignedIntValue);
}

- (NSNumber *)scrollX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollX)];
}

- (void)setScrollX:(NSNumber *)scrollX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollX, scrollX.floatValue);
}

- (NSNumber *)scrollY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollY)];
}

- (void)setScrollY:(NSNumber *)scrollY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollY, scrollY.floatValue);
}

- (NSNumber *)scrollZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollZ)];
}

- (void)setScrollZ:(NSNumber *)scrollZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldScrollZ, scrollZ.floatValue);
}

- (NSString *)scrollDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"scrollIsPixels:%@ scrollX:%@ scrollY:%@ scrollZ:%@", self.scrollIsPixels, self.scrollX, self.scrollY, self.scrollZ];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilBiometricEvent)

- (NSNumber *)biometricEventType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricEventType)];
}

- (void)setBiometricEventType:(NSNumber *)biometricEventType {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricEventType, biometricEventType.unsignedIntValue);
}

- (NSNumber *)biometricUsage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricUsage)];
}

- (void)setBiometricUsage:(NSNumber *)biometricUsage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricUsage, biometricUsage.unsignedIntValue);
}

- (NSNumber *)biometricLevel {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricLevel)];
}

- (void)setBiometricLevel:(NSNumber *)biometricLevel {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricLevel, biometricLevel.floatValue);
}

- (NSNumber *)biometricTapCount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricTapCount)];
}

- (void)setBiometricTapCount:(NSNumber *)biometricTapCount {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricTapCount, biometricTapCount.unsignedIntValue);
}

- (NSNumber *)biometricUsagePage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricUsagePage)];
}

- (void)setBiometricUsagePage:(NSNumber *)biometricUsagePage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBiometricUsagePage, biometricUsagePage.unsignedIntValue);
}

- (NSString *)biometricDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"biometricEventType:%@ biometricLevel:%@ biometricUsagePage:%@ biometricUsage:%@ biometricTapCount:%@", self.biometricEventType, self.biometricLevel, self.biometricUsagePage, self.biometricUsage, self.biometricTapCount];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilBoundaryScrollEvent)

- (NSNumber *)boundaryScrollProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollProgress)];
}

- (void)setBoundaryScrollProgress:(NSNumber *)boundaryScrollProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollProgress, boundaryScrollProgress.floatValue);
}

- (NSNumber *)boundaryScrollFlavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollFlavor)];
}

- (void)setBoundaryScrollFlavor:(NSNumber *)boundaryScrollFlavor {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollFlavor, boundaryScrollFlavor.unsignedIntValue);
}

- (NSNumber *)boundaryScrollPositionY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollPositionY)];
}

- (void)setBoundaryScrollPositionY:(NSNumber *)boundaryScrollPositionY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollPositionY, boundaryScrollPositionY.floatValue);
}

- (NSNumber *)boundaryScrollPositionX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollPositionX)];
}

- (void)setBoundaryScrollPositionX:(NSNumber *)boundaryScrollPositionX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollPositionX, boundaryScrollPositionX.floatValue);
}

- (NSNumber *)boundaryScrollMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollMask)];
}

- (void)setBoundaryScrollMask:(NSNumber *)boundaryScrollMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollMask, boundaryScrollMask.unsignedIntValue);
}

- (NSNumber *)boundaryScrollMotion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollMotion)];
}

- (void)setBoundaryScrollMotion:(NSNumber *)boundaryScrollMotion {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldBoundaryScrollMotion, boundaryScrollMotion.unsignedIntValue);
}

- (NSString *)boundaryScrollDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"boundaryScrollPositionX:%@ boundaryScrollPositionY:%@ boundaryScrollMask:%@ boundaryScrollMotion:%@ boundaryScrollFlavor:%@ boundaryScrollProgress:%@", self.boundaryScrollPositionX, self.boundaryScrollPositionY, self.boundaryScrollMask, self.boundaryScrollMotion, self.boundaryScrollFlavor, self.boundaryScrollProgress];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilLEDEvent)

- (NSNumber *)ledMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldLEDMask)];
}

- (void)setLedMask:(NSNumber *)ledMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldLEDMask, ledMask.unsignedIntValue);
}

- (NSNumber *)ledState {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldLEDState)];
}

- (void)setLedState:(NSNumber *)ledState {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldLEDState, ledState.unsignedIntValue);
}

- (NSNumber *)ledNumber {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldLEDNumber)];
}

- (void)setLedNumber:(NSNumber *)ledNumber {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldLEDNumber, ledNumber.unsignedIntValue);
}

- (NSString *)ledDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"ledMask:%@ ledNumber:%@ ledState:%@", self.ledMask, self.ledNumber, self.ledState];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilOrientationEvent)

- (NSNumber *)orientationOrientationType {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationOrientationType)];
}

- (NSNumber *)orientationTiltZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationTiltZ)];
}

- (void)setOrientationTiltZ:(NSNumber *)orientationTiltZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationTiltZ, orientationTiltZ.floatValue);
}

- (NSNumber *)orientationTiltY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationTiltY)];
}

- (void)setOrientationTiltY:(NSNumber *)orientationTiltY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationTiltY, orientationTiltY.floatValue);
}

- (NSNumber *)orientationTiltX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationTiltX)];
}

- (void)setOrientationTiltX:(NSNumber *)orientationTiltX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationTiltX, orientationTiltX.floatValue);
}

- (NSNumber *)orientationAzimuth {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationAzimuth)];
}

- (void)setOrientationAzimuth:(NSNumber *)orientationAzimuth {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationAzimuth, orientationAzimuth.floatValue);
}

- (NSNumber *)orientationQuatZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatZ)];
}

- (void)setOrientationQuatZ:(NSNumber *)orientationQuatZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatZ, orientationQuatZ.floatValue);
}

- (NSNumber *)orientationQuatY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatY)];
}

- (void)setOrientationQuatY:(NSNumber *)orientationQuatY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatY, orientationQuatY.floatValue);
}

- (NSNumber *)orientationQuatX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatX)];
}

- (void)setOrientationQuatX:(NSNumber *)orientationQuatX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatX, orientationQuatX.floatValue);
}

- (NSNumber *)orientationQuatW {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatW)];
}

- (void)setOrientationQuatW:(NSNumber *)orientationQuatW {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationQuatW, orientationQuatW.floatValue);
}

- (NSNumber *)orientationDeviceOrientationUsage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationDeviceOrientationUsage)];
}

- (void)setOrientationDeviceOrientationUsage:(NSNumber *)orientationDeviceOrientationUsage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationDeviceOrientationUsage, orientationDeviceOrientationUsage.unsignedIntValue);
}

- (NSNumber *)orientationAltitude {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationAltitude)];
}

- (void)setOrientationAltitude:(NSNumber *)orientationAltitude {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationAltitude, orientationAltitude.floatValue);
}

- (NSNumber *)orientationRadius {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationRadius)];
}

- (void)setOrientationRadius:(NSNumber *)orientationRadius {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationRadius, orientationRadius.floatValue);
}

- (NSString *)orientationDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"orientationOrientationType:%@", self.orientationOrientationType];
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypeTilt) {
        desc = [NSString stringWithFormat:@"%@ orientationTiltX:%@ orientationTiltY:%@ orientationTiltZ:%@", desc, self.orientationTiltX, self.orientationTiltY, self.orientationTiltZ];
    }
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypeCMUsage) {
        desc = [NSString stringWithFormat:@"%@ orientationDeviceOrientationUsage:%@", desc, self.orientationDeviceOrientationUsage];
    }
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypePolar) {
        desc = [NSString stringWithFormat:@"%@ orientationRadius:%@ orientationAzimuth:%@ orientationAltitude:%@", desc, self.orientationRadius, self.orientationAzimuth, self.orientationAltitude];
    }
    if (IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldOrientationOrientationType) == kIOHIDOrientationTypeQuaternion) {
        desc = [NSString stringWithFormat:@"%@ orientationQuatW:%@ orientationQuatX:%@ orientationQuatY:%@ orientationQuatZ:%@", desc, self.orientationQuatW, self.orientationQuatX, self.orientationQuatY, self.orientationQuatZ];
    }
    return desc;
}
@end

@implementation HIDEvent (HIDUtilProximityEvent)

- (NSNumber *)proximityLevel {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProximityLevel)];
}

- (void)setProximityLevel:(NSNumber *)proximityLevel {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProximityLevel, proximityLevel.unsignedIntValue);
}

- (NSNumber *)proximityDetectionMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProximityDetectionMask)];
}

- (void)setProximityDetectionMask:(NSNumber *)proximityDetectionMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldProximityDetectionMask, proximityDetectionMask.unsignedIntValue);
}

- (NSString *)proximityDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"proximityDetectionMask:%@ proximityLevel:%@", self.proximityDetectionMask, self.proximityLevel];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilFluidTouchGestureEvent)

- (NSNumber *)fluidTouchGesturePositionY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGesturePositionY)];
}

- (void)setFluidTouchGesturePositionY:(NSNumber *)fluidTouchGesturePositionY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGesturePositionY, fluidTouchGesturePositionY.floatValue);
}

- (NSNumber *)fluidTouchGesturePositionX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGesturePositionX)];
}

- (void)setFluidTouchGesturePositionX:(NSNumber *)fluidTouchGesturePositionX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGesturePositionX, fluidTouchGesturePositionX.floatValue);
}

- (NSNumber *)fluidTouchGestureMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureMask)];
}

- (void)setFluidTouchGestureMask:(NSNumber *)fluidTouchGestureMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureMask, fluidTouchGestureMask.unsignedIntValue);
}

- (NSNumber *)fluidTouchGestureProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureProgress)];
}

- (void)setFluidTouchGestureProgress:(NSNumber *)fluidTouchGestureProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureProgress, fluidTouchGestureProgress.floatValue);
}

- (NSNumber *)fluidTouchGestureMotion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureMotion)];
}

- (void)setFluidTouchGestureMotion:(NSNumber *)fluidTouchGestureMotion {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureMotion, fluidTouchGestureMotion.unsignedIntValue);
}

- (NSNumber *)fluidTouchGestureFlavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureFlavor)];
}

- (void)setFluidTouchGestureFlavor:(NSNumber *)fluidTouchGestureFlavor {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldFluidTouchGestureFlavor, fluidTouchGestureFlavor.unsignedIntValue);
}

- (NSString *)fluidTouchGestureDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"fluidTouchGesturePositionX:%@ fluidTouchGesturePositionY:%@ fluidTouchGestureMask:%@ fluidTouchGestureMotion:%@ fluidTouchGestureFlavor:%@ fluidTouchGestureProgress:%@", self.fluidTouchGesturePositionX, self.fluidTouchGesturePositionY, self.fluidTouchGestureMask, self.fluidTouchGestureMotion, self.fluidTouchGestureFlavor, self.fluidTouchGestureProgress];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilDockSwipeEvent)

- (NSNumber *)dockSwipeProgress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeProgress)];
}

- (void)setDockSwipeProgress:(NSNumber *)dockSwipeProgress {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeProgress, dockSwipeProgress.floatValue);
}

- (NSNumber *)dockSwipeMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeMask)];
}

- (void)setDockSwipeMask:(NSNumber *)dockSwipeMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeMask, dockSwipeMask.unsignedIntValue);
}

- (NSNumber *)dockSwipeMotion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeMotion)];
}

- (void)setDockSwipeMotion:(NSNumber *)dockSwipeMotion {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeMotion, dockSwipeMotion.unsignedIntValue);
}

- (NSNumber *)dockSwipeFlavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeFlavor)];
}

- (void)setDockSwipeFlavor:(NSNumber *)dockSwipeFlavor {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipeFlavor, dockSwipeFlavor.unsignedIntValue);
}

- (NSNumber *)dockSwipePositionX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipePositionX)];
}

- (void)setDockSwipePositionX:(NSNumber *)dockSwipePositionX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipePositionX, dockSwipePositionX.floatValue);
}

- (NSNumber *)dockSwipePositionY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipePositionY)];
}

- (void)setDockSwipePositionY:(NSNumber *)dockSwipePositionY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipePositionY, dockSwipePositionY.floatValue);
}

- (NSNumber *)dockSwipePositionZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipePositionZ)];
}

- (void)setDockSwipePositionZ:(NSNumber *)dockSwipePositionZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldDockSwipePositionZ, dockSwipePositionZ.floatValue);
}

- (NSString *)dockSwipeDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"dockSwipePositionX:%@ dockSwipePositionY:%@ dockSwipePositionZ:%@ dockSwipeMask:%@ dockSwipeMotion:%@ dockSwipeFlavor:%@ dockSwipeProgress:%@", self.dockSwipePositionX, self.dockSwipePositionY, self.dockSwipePositionZ, self.dockSwipeMask, self.dockSwipeMotion, self.dockSwipeFlavor, self.dockSwipeProgress];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilUnicodeEvent)

- (uint8_t *)unicodePayload {
    return IOHIDEventGetDataValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodePayload);
}

- (NSString *)unicodePayloadstr {
    NSString *unicodePayloadstr = [[NSString alloc] init];
    uint8_t *unicodePayload = self.unicodePayload;

    for (uint32_t i = 0; i < self.vendorDefinedDataLength.unsignedIntValue; i++) {
        unicodePayloadstr = [unicodePayloadstr stringByAppendingString:[NSString stringWithFormat:@"%02x ", unicodePayload[i]]];
    }

    return [unicodePayloadstr substringToIndex:unicodePayloadstr.length-1];
}

- (NSNumber *)unicodeLength {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodeLength)];
}

- (void)setUnicodeLength:(NSNumber *)unicodeLength {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodeLength, unicodeLength.unsignedIntValue);
}

- (NSNumber *)unicodeQuality {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodeQuality)];
}

- (void)setUnicodeQuality:(NSNumber *)unicodeQuality {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodeQuality, unicodeQuality.floatValue);
}

- (NSNumber *)unicodeEncoding {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodeEncoding)];
}

- (void)setUnicodeEncoding:(NSNumber *)unicodeEncoding {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldUnicodeEncoding, unicodeEncoding.unsignedIntValue);
}

- (NSString *)unicodeDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"unicodeEncoding:%@ unicodeQuality:%@ unicodeLength:%@ unicodePayloadstr:%@", self.unicodeEncoding, self.unicodeQuality, self.unicodeLength, self.unicodePayloadstr];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilKeyboardEvent)

- (NSNumber *)keyboardStickyKeyPhase {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardStickyKeyPhase)];
}

- (void)setKeyboardStickyKeyPhase:(NSNumber *)keyboardStickyKeyPhase {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardStickyKeyPhase, keyboardStickyKeyPhase.unsignedIntValue);
}

- (NSNumber *)keyboardStickyKeyToggle {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardStickyKeyToggle)];
}

- (void)setKeyboardStickyKeyToggle:(NSNumber *)keyboardStickyKeyToggle {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardStickyKeyToggle, keyboardStickyKeyToggle.unsignedIntValue);
}

- (NSNumber *)keyboardMouseKeyToggle {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardMouseKeyToggle)];
}

- (void)setKeyboardMouseKeyToggle:(NSNumber *)keyboardMouseKeyToggle {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardMouseKeyToggle, keyboardMouseKeyToggle.unsignedIntValue);
}

- (NSNumber *)keyboardClickSpeed {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardClickSpeed)];
}

- (void)setKeyboardClickSpeed:(NSNumber *)keyboardClickSpeed {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardClickSpeed, keyboardClickSpeed.unsignedIntValue);
}

- (NSNumber *)keyboardPressCount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardPressCount)];
}

- (void)setKeyboardPressCount:(NSNumber *)keyboardPressCount {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardPressCount, keyboardPressCount.unsignedIntValue);
}

- (NSNumber *)keyboardLongPress {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardLongPress)];
}

- (void)setKeyboardLongPress:(NSNumber *)keyboardLongPress {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardLongPress, keyboardLongPress.unsignedIntValue);
}

- (NSNumber *)keyboardUsagePage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardUsagePage)];
}

- (void)setKeyboardUsagePage:(NSNumber *)keyboardUsagePage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardUsagePage, keyboardUsagePage.unsignedIntValue);
}

- (NSNumber *)keyboardSlowKeyPhase {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardSlowKeyPhase)];
}

- (void)setKeyboardSlowKeyPhase:(NSNumber *)keyboardSlowKeyPhase {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardSlowKeyPhase, keyboardSlowKeyPhase.unsignedIntValue);
}

- (NSNumber *)keyboardDown {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardDown)];
}

- (void)setKeyboardDown:(NSNumber *)keyboardDown {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardDown, keyboardDown.unsignedIntValue);
}

- (NSNumber *)keyboardRepeat {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardRepeat)];
}

- (void)setKeyboardRepeat:(NSNumber *)keyboardRepeat {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardRepeat, keyboardRepeat.unsignedIntValue);
}

- (NSNumber *)keyboardUsage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardUsage)];
}

- (void)setKeyboardUsage:(NSNumber *)keyboardUsage {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldKeyboardUsage, keyboardUsage.unsignedIntValue);
}

- (NSString *)keyboardDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"keyboardRepeat:%@ keyboardUsagePage:%@ keyboardUsage:%@ keyboardDown:%@ keyboardLongPress:%@ keyboardClickSpeed:%@ keyboardSlowKeyPhase:%@ keyboardMouseKeyToggle:%@ keyboardStickyKeyPhase:%@ keyboardStickyKeyToggle:%@ keyboardPressCount:%@", self.keyboardRepeat, self.keyboardUsagePage, self.keyboardUsage, self.keyboardDown, self.keyboardLongPress, self.keyboardClickSpeed, self.keyboardSlowKeyPhase, self.keyboardMouseKeyToggle, self.keyboardStickyKeyPhase, self.keyboardStickyKeyToggle, self.keyboardPressCount];
    return desc;
}
@end

@implementation HIDEvent (HIDUtilPointerEvent)

- (NSNumber *)pointerZ {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerZ)];
}

- (void)setPointerZ:(NSNumber *)pointerZ {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerZ, pointerZ.floatValue);
}

- (NSNumber *)pointerY {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerY)];
}

- (void)setPointerY:(NSNumber *)pointerY {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerY, pointerY.floatValue);
}

- (NSNumber *)pointerX {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerX)];
}

- (void)setPointerX:(NSNumber *)pointerX {
    IOHIDEventSetFloatValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerX, pointerX.floatValue);
}

- (NSNumber *)pointerButtonMask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerButtonMask)];
}

- (void)setPointerButtonMask:(NSNumber *)pointerButtonMask {
    IOHIDEventSetIntegerValue((__bridge IOHIDEventRef)self, kIOHIDEventFieldPointerButtonMask, pointerButtonMask.unsignedIntValue);
}

- (NSString *)pointerDescription {
    NSString * desc;
    desc = [NSString stringWithFormat:@"pointerX:%@ pointerY:%@ pointerZ:%@ pointerButtonMask:%@", self.pointerX, self.pointerY, self.pointerZ, self.pointerButtonMask];
    return desc;
}
@end

