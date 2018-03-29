//
//  HIDEvent.m
//  hidutil-internal
//
//  Created by Matt Dekom on 8/3/17.
//

#import "HIDEvent.h"

@implementation HIDEvent

- (id)initWithEvent:(IOHIDEventRef)event {
    self = [super init];
    
    if (self) {
        self->eventRef = event;
        CFRetain(self->eventRef);
    }
    
    return self;
}

- (void)dealloc {
    if (self->eventRef) {
        CFRelease(self->eventRef);
    }
}

-(void)setTimestamp:(NSNumber *)timestamp {
    IOHIDEventSetTimeStamp(self->eventRef, timestamp.unsignedLongLongValue);
}

- (NSNumber *)timestamp {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetTimeStamp(self->eventRef)];
}

- (void)setSender:(NSNumber *)sender {
    IOHIDEventSetSenderID(self->eventRef, sender.unsignedLongLongValue);
}

- (NSNumber *)sender {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetSenderID(self->eventRef)];
}

- (NSNumber *)typeval {
    return [NSNumber numberWithInt:IOHIDEventGetType(self->eventRef)];
}

- (NSNumber *)latency {
    return [NSNumber numberWithUnsignedLongLong:IOHIDEventGetLatency(self->eventRef, kMicrosecondScale)];
}

- (void)setFlags:(NSNumber *)flags {
    IOHIDEventSetEventFlags(self->eventRef, flags.unsignedIntValue);
}

- (NSNumber *)flags {
    return [NSNumber numberWithInt:IOHIDEventGetEventFlags(self->eventRef)];
}

- (NSString *)typestr {
    return [[NSString stringWithUTF8String:IOHIDEventGetTypeString(IOHIDEventGetType(self->eventRef))] lowercaseString];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"timestamp:%llu sender:0x%llx typeval:%d typestr:%@ latency:%llu flags:0x%08x", self.timestamp.unsignedLongLongValue, self.sender.unsignedLongLongValue, self.typeval.unsignedIntValue, self.typestr, self.latency.unsignedLongLongValue, self.flags.unsignedIntValue];
}

@end


HIDEvent *createHIDEvent(IOHIDEventRef event)
{
    HIDEvent *ev = NULL;
    
    if (!event) {
        return ev;
    }
    
    switch (IOHIDEventGetType(event)) {
        case kIOHIDEventTypeNULL:
            ev = [[HIDNULLEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeVendorDefined:
            ev = [[HIDVendorDefinedEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeButton:
            ev = [[HIDButtonEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeKeyboard:
            ev = [[HIDKeyboardEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeTranslation:
            ev = [[HIDTranslationEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeRotation:
            ev = [[HIDRotationEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeScroll:
            ev = [[HIDScrollEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeScale:
            ev = [[HIDScaleEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeVelocity:
            ev = [[HIDVelocityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeOrientation:
            ev = [[HIDOrientationEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeDigitizer:
            ev = [[HIDDigitizerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeAmbientLightSensor:
            ev = [[HIDAmbientLightSensorEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeAccelerometer:
            ev = [[HIDAccelerometerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeProximity:
            ev = [[HIDProximityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeTemperature:
            ev = [[HIDTemperatureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeNavigationSwipe:
            ev = [[HIDNavigationSwipeEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypePointer:
            ev = [[HIDPointerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeProgress:
            ev = [[HIDProgressEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeMultiAxisPointer:
            ev = [[HIDMultiAxisPointerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeGyro:
            ev = [[HIDGyroEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeCompass:
            ev = [[HIDCompassEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeDockSwipe:
            ev = [[HIDDockSwipeEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeSymbolicHotKey:
            ev = [[HIDSymbolicHotKeyEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypePower:
            ev = [[HIDPowerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeLED:
            ev = [[HIDLEDEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeFluidTouchGesture:
            ev = [[HIDFluidTouchGestureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeBoundaryScroll:
            ev = [[HIDBoundaryScrollEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeBiometric:
            ev = [[HIDBiometricEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeUnicode:
            ev = [[HIDUnicodeEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeAtmosphericPressure:
            ev = [[HIDAtmosphericPressureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeForce:
            ev = [[HIDForceEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeMotionActivity:
            ev = [[HIDMotionActivityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeMotionGesture:
            ev = [[HIDMotionGestureEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeGameController:
            ev = [[HIDGameControllerEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeHumidity:
            ev = [[HIDHumidityEvent alloc] initWithEvent:event];
            break;
        case kIOHIDEventTypeBrightness:
            ev = [[HIDBrightnessEvent alloc] initWithEvent:event];
            break;
        default:
            ev = [[HIDEvent alloc] initWithEvent:event];
    }
    
    return ev;
}


@implementation HIDNULLEvent

- (NSNumber *)relative {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldIsRelative)];
}

- (void)setRelative:(NSNumber *)relative {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldIsRelative, relative.unsignedIntValue);
}

- (NSNumber *)centerorigin {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldIsCenterOrigin)];
}

- (void)setCenterorigin:(NSNumber *)centerorigin {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldIsCenterOrigin, centerorigin.unsignedIntValue);
}

- (NSNumber *)builtin {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldIsBuiltIn)];
}

- (void)setBuiltin:(NSNumber *)builtin {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldIsBuiltIn, builtin.unsignedIntValue);
}

- (NSNumber *)pixelunits {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldIsPixelUnits)];
}

- (void)setPixelunits:(NSNumber *)pixelunits {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldIsPixelUnits, pixelunits.unsignedIntValue);
}

- (NSNumber *)collection {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldIsCollection)];
}

- (void)setCollection:(NSNumber *)collection {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldIsCollection, collection.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ relative:%@ centerorigin:%@ builtin:%@ pixelunits:%@ collection:%@", [super description], self.relative, self.centerorigin, self.builtin, self.pixelunits, self.collection];
}

@end


@implementation HIDVendorDefinedEvent

- (NSNumber *)length {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedDataLength)];
}

- (NSNumber *)version {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedVersion)];
}

- (void)setVersion:(NSNumber *)version {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedVersion, version.unsignedIntValue);
}

- (NSNumber *)usage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedUsage)];
}

- (void)setUsage:(NSNumber *)usage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedUsage, usage.unsignedIntValue);
}

- (uint8_t *)data {
    return IOHIDEventGetDataValue(self->eventRef, kIOHIDEventFieldVendorDefinedData);
}

- (NSString *)datastr {
    NSString *datastr = [[NSString alloc] init];
    uint8_t *data = self.data;
    
    for (uint32_t i = 0; i < self.length.unsignedIntValue; i++) {
        datastr = [datastr stringByAppendingString:[NSString stringWithFormat:@"%02x ", data[i]]];
    }
    
    return [datastr substringToIndex:datastr.length-1];
}

- (NSNumber *)usagepage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedUsagePage)];
}

- (void)setUsagepage:(NSNumber *)usagepage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldVendorDefinedUsagePage, usagepage.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ length:%@ version:%@ usage:%@ data:%@ usagepage:%@", [super description], self.length, self.version, self.usage, self.datastr, self.usagepage];
}

@end


@implementation HIDKeyboardEvent

- (NSNumber *)stickykeyphase {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardStickyKeyPhase)];
}

- (void)setStickykeyphase:(NSNumber *)stickykeyphase {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardStickyKeyPhase, stickykeyphase.unsignedIntValue);
}

- (NSNumber *)stickykeytoggle {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardStickyKeyToggle)];
}

- (void)setStickykeytoggle:(NSNumber *)stickykeytoggle {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardStickyKeyToggle, stickykeytoggle.unsignedIntValue);
}

- (NSNumber *)mousekeytoggle {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardMouseKeyToggle)];
}

- (void)setMousekeytoggle:(NSNumber *)mousekeytoggle {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardMouseKeyToggle, mousekeytoggle.unsignedIntValue);
}

- (NSNumber *)clickspeed {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardClickSpeed)];
}

- (void)setClickspeed:(NSNumber *)clickspeed {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardClickSpeed, clickspeed.unsignedIntValue);
}

- (NSNumber *)presscount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardPressCount)];
}

- (void)setPresscount:(NSNumber *)presscount {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardPressCount, presscount.unsignedIntValue);
}

- (NSNumber *)longpress {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardLongPress)];
}

- (void)setLongpress:(NSNumber *)longpress {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardLongPress, longpress.unsignedIntValue);
}

- (NSNumber *)usagepage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardUsagePage)];
}

- (void)setUsagepage:(NSNumber *)usagepage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardUsagePage, usagepage.unsignedIntValue);
}

- (NSNumber *)slowkeyphase {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardSlowKeyPhase)];
}

- (void)setSlowkeyphase:(NSNumber *)slowkeyphase {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardSlowKeyPhase, slowkeyphase.unsignedIntValue);
}

- (NSNumber *)down {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardDown)];
}

- (void)setDown:(NSNumber *)down {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardDown, down.unsignedIntValue);
}

- (NSNumber *)repeat {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardRepeat)];
}

- (void)setRepeat:(NSNumber *)repeat {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardRepeat, repeat.unsignedIntValue);
}

- (NSNumber *)usage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardUsage)];
}

- (void)setUsage:(NSNumber *)usage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldKeyboardUsage, usage.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ stickykeyphase:%@ stickykeytoggle:%@ mousekeytoggle:%@ clickspeed:%@ presscount:%@ longpress:%@ usagepage:%@ slowkeyphase:%@ down:%@ repeat:%@ usage:%@", [super description], self.stickykeyphase, self.stickykeytoggle, self.mousekeytoggle, self.clickspeed, self.presscount, self.longpress, self.usagepage, self.slowkeyphase, self.down, self.repeat, self.usage];
}

@end


@implementation HIDUnicodeEvent

- (uint8_t *)payload {
    return IOHIDEventGetDataValue(self->eventRef, kIOHIDEventFieldUnicodePayload);
}

- (NSString *)payloadstr {
    NSString *payloadstr = [[NSString alloc] init];
    uint8_t *payload = self.payload;
    
    for (uint32_t i = 0; i < self.length.unsignedIntValue; i++) {
        payloadstr = [payloadstr stringByAppendingString:[NSString stringWithFormat:@"%02x ", payload[i]]];
    }
    
    return [payloadstr substringToIndex:payloadstr.length-1];
}

- (NSNumber *)length {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldUnicodeLength)];
}

- (void)setLength:(NSNumber *)length {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldUnicodeLength, length.unsignedIntValue);
}

- (NSNumber *)quality {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldUnicodeQuality)];
}

- (void)setQuality:(NSNumber *)quality {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldUnicodeQuality, quality.floatValue);
}

- (NSNumber *)encoding {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldUnicodeEncoding)];
}

- (void)setEncoding:(NSNumber *)encoding {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldUnicodeEncoding, encoding.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ payload:%@ length:%@ quality:%@ encoding:%@", [super description], self.payloadstr, self.length, self.quality, self.encoding];
}

@end


@implementation HIDLEDEvent

- (NSNumber *)mask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldLEDMask)];
}

- (void)setMask:(NSNumber *)mask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldLEDMask, mask.unsignedIntValue);
}

- (NSNumber *)state {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldLEDState)];
}

- (void)setState:(NSNumber *)state {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldLEDState, state.unsignedIntValue);
}

- (NSNumber *)number {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldLEDNumber)];
}

- (void)setNumber:(NSNumber *)number {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldLEDNumber, number.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ mask:%@ state:%@ number:%@", [super description], self.mask, self.state, self.number];
}

@end


@implementation HIDRotationEvent

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldRotationY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldRotationY, y.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldRotationX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldRotationX, x.floatValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldRotationZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldRotationZ, z.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ y:%@ x:%@ z:%@", [super description], self.y, self.x, self.z];
}

@end


@implementation HIDVelocityEvent

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldVelocityX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldVelocityX, x.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldVelocityY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldVelocityY, y.floatValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldVelocityZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldVelocityZ, z.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ x:%@ y:%@ z:%@", [super description], self.x, self.y, self.z];
}

@end


@implementation HIDTranslationEvent

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldTranslationY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldTranslationY, y.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldTranslationX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldTranslationX, x.floatValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldTranslationZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldTranslationZ, z.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ y:%@ x:%@ z:%@", [super description], self.y, self.x, self.z];
}

@end


@implementation HIDScrollEvent

- (NSNumber *)ispixels {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldScrollIsPixels)];
}

- (void)setIspixels:(NSNumber *)ispixels {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldScrollIsPixels, ispixels.unsignedIntValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldScrollX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldScrollX, x.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldScrollY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldScrollY, y.floatValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldScrollZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldScrollZ, z.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ ispixels:%@ x:%@ y:%@ z:%@", [super description], self.ispixels, self.x, self.y, self.z];
}

@end


@implementation HIDScaleEvent

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldScaleZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldScaleZ, z.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldScaleX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldScaleX, x.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldScaleY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldScaleY, y.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ z:%@ x:%@ y:%@", [super description], self.z, self.x, self.y];
}

@end


@implementation HIDOrientationEvent

- (NSNumber *)altitude {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldOrientationAltitude)];
}

- (void)setAltitude:(NSNumber *)altitude {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldOrientationAltitude, altitude.floatValue);
}

- (NSNumber *)azimuth {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldOrientationAzimuth)];
}

- (void)setAzimuth:(NSNumber *)azimuth {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldOrientationAzimuth, azimuth.floatValue);
}

- (NSNumber *)radius {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldOrientationRadius)];
}

- (void)setRadius:(NSNumber *)radius {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldOrientationRadius, radius.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ altitude:%@ azimuth:%@ radius:%@", [super description], self.altitude, self.azimuth, self.radius];
}

@end


@implementation HIDAccelerometerEvent

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAccelerometerY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAccelerometerY, y.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAccelerometerX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAccelerometerX, x.floatValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAccelerometerZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAccelerometerZ, z.floatValue);
}

- (NSNumber *)type {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAccelerometerType)];
}

- (void)setType:(NSNumber *)type {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAccelerometerType, type.unsignedIntValue);
}

- (NSNumber *)subtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAccelerometerSubType)];
}

- (void)setSubtype:(NSNumber *)subtype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAccelerometerSubType, subtype.unsignedIntValue);
}

- (NSNumber *)sequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAccelerometerSequence)];
}

- (void)setSequence:(NSNumber *)sequence {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAccelerometerSequence, sequence.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ y:%@ x:%@ z:%@ type:%@ subtype:%@ sequence:%@", [super description], self.y, self.x, self.z, self.type, self.subtype, self.sequence];
}

@end


@implementation HIDGyroEvent

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGyroX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGyroX, x.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGyroY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGyroY, y.floatValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGyroZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGyroZ, z.floatValue);
}

- (NSNumber *)subtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldGyroSubType)];
}

- (void)setSubtype:(NSNumber *)subtype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldGyroSubType, subtype.unsignedIntValue);
}

- (NSNumber *)sequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldGyroSequence)];
}

- (void)setSequence:(NSNumber *)sequence {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldGyroSequence, sequence.unsignedIntValue);
}

- (NSNumber *)type {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldGyroType)];
}

- (void)setType:(NSNumber *)type {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldGyroType, type.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ x:%@ y:%@ z:%@ subtype:%@ sequence:%@ type:%@", [super description], self.x, self.y, self.z, self.subtype, self.sequence, self.type];
}

@end


@implementation HIDCompassEvent

- (NSNumber *)type {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldCompassType)];
}

- (void)setType:(NSNumber *)type {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldCompassType, type.unsignedIntValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldCompassZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldCompassZ, z.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldCompassX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldCompassX, x.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldCompassY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldCompassY, y.floatValue);
}

- (NSNumber *)subtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldCompassSubType)];
}

- (void)setSubtype:(NSNumber *)subtype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldCompassSubType, subtype.unsignedIntValue);
}

- (NSNumber *)sequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldCompassSequence)];
}

- (void)setSequence:(NSNumber *)sequence {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldCompassSequence, sequence.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ type:%@ z:%@ x:%@ y:%@ subtype:%@ sequence:%@", [super description], self.type, self.z, self.x, self.y, self.subtype, self.sequence];
}

@end


@implementation HIDAmbientLightSensorEvent

- (NSNumber *)colorspace {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightColorSpace)];
}

- (void)setColorspace:(NSNumber *)colorspace {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightColorSpace, colorspace.unsignedIntValue);
}

- (NSNumber *)colorcomponent2 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAmbientLightColorComponent2)];
}

- (void)setColorcomponent2:(NSNumber *)colorcomponent2 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAmbientLightColorComponent2, colorcomponent2.floatValue);
}

- (NSNumber *)colorcomponent1 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAmbientLightColorComponent1)];
}

- (void)setColorcomponent1:(NSNumber *)colorcomponent1 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAmbientLightColorComponent1, colorcomponent1.floatValue);
}

- (NSNumber *)colorcomponent0 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAmbientLightColorComponent0)];
}

- (void)setColorcomponent0:(NSNumber *)colorcomponent0 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAmbientLightColorComponent0, colorcomponent0.floatValue);
}

- (NSNumber *)rawchannel0 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel0)];
}

- (void)setRawchannel0:(NSNumber *)rawchannel0 {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel0, rawchannel0.unsignedIntValue);
}

- (NSNumber *)rawchannel1 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel1)];
}

- (void)setRawchannel1:(NSNumber *)rawchannel1 {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel1, rawchannel1.unsignedIntValue);
}

- (NSNumber *)rawchannel2 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel2)];
}

- (void)setRawchannel2:(NSNumber *)rawchannel2 {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel2, rawchannel2.unsignedIntValue);
}

- (NSNumber *)rawchannel3 {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel3)];
}

- (void)setRawchannel3:(NSNumber *)rawchannel3 {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorRawChannel3, rawchannel3.unsignedIntValue);
}

- (NSNumber *)level {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorLevel)];
}

- (void)setLevel:(NSNumber *)level {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightSensorLevel, level.unsignedIntValue);
}

- (NSNumber *)brightnesschanged {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightDisplayBrightnessChanged)];
}

- (void)setBrightnesschanged:(NSNumber *)brightnesschanged {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAmbientLightDisplayBrightnessChanged, brightnesschanged.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ colorspace:%@ colorcomponent2:%@ colorcomponent1:%@ colorcomponent0:%@ rawchannel0:%@ rawchannel1:%@ rawchannel2:%@ rawchannel3:%@ level:%@ brightnesschanged:%@", [super description], self.colorspace, self.colorcomponent2, self.colorcomponent1, self.colorcomponent0, self.rawchannel0, self.rawchannel1, self.rawchannel2, self.rawchannel3, self.level, self.brightnesschanged];
}

@end


@implementation HIDTemperatureEvent

- (NSNumber *)level {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldTemperatureLevel)];
}

- (void)setLevel:(NSNumber *)level {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldTemperatureLevel, level.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ level:%@", [super description], self.level];
}

@end


@implementation HIDProximityEvent

- (NSNumber *)level {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldProximityLevel)];
}

- (void)setLevel:(NSNumber *)level {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldProximityLevel, level.unsignedIntValue);
}

- (NSNumber *)detectionmask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldProximityDetectionMask)];
}

- (void)setDetectionmask:(NSNumber *)detectionmask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldProximityDetectionMask, detectionmask.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ level:%@ detectionmask:%@", [super description], self.level, self.detectionmask];
}

@end


@implementation HIDProgressEvent

- (NSNumber *)level {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldProgressLevel)];
}

- (void)setLevel:(NSNumber *)level {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldProgressLevel, level.floatValue);
}

- (NSNumber *)eventtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldProgressEventType)];
}

- (void)setEventtype:(NSNumber *)eventtype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldProgressEventType, eventtype.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ level:%@ eventtype:%@", [super description], self.level, self.eventtype];
}

@end


@implementation HIDBiometricEvent

- (NSNumber *)eventtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricEventType)];
}

- (void)setEventtype:(NSNumber *)eventtype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricEventType, eventtype.unsignedIntValue);
}

- (NSNumber *)usage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricUsage)];
}

- (void)setUsage:(NSNumber *)usage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricUsage, usage.unsignedIntValue);
}

- (NSNumber *)level {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldBiometricLevel)];
}

- (void)setLevel:(NSNumber *)level {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldBiometricLevel, level.floatValue);
}

- (NSNumber *)tapcount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricTapCount)];
}

- (void)setTapcount:(NSNumber *)tapcount {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricTapCount, tapcount.unsignedIntValue);
}

- (NSNumber *)usagepage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricUsagePage)];
}

- (void)setUsagepage:(NSNumber *)usagepage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBiometricUsagePage, usagepage.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ eventtype:%@ usage:%@ level:%@ tapcount:%@ usagepage:%@", [super description], self.eventtype, self.usage, self.level, self.tapcount, self.usagepage];
}

@end


@implementation HIDButtonEvent

- (NSNumber *)pressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldButtonPressure)];
}

- (void)setPressure:(NSNumber *)pressure {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldButtonPressure, pressure.floatValue);
}

- (NSNumber *)clickcount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldButtonClickCount)];
}

- (void)setClickcount:(NSNumber *)clickcount {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldButtonClickCount, clickcount.unsignedIntValue);
}

- (NSNumber *)mask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldButtonMask)];
}

- (void)setMask:(NSNumber *)mask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldButtonMask, mask.unsignedIntValue);
}

- (NSNumber *)state {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldButtonState)];
}

- (void)setState:(NSNumber *)state {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldButtonState, state.unsignedIntValue);
}

- (NSNumber *)number {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldButtonNumber)];
}

- (void)setNumber:(NSNumber *)number {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldButtonNumber, number.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ pressure:%@ clickcount:%@ mask:%@ state:%@ number:%@", [super description], self.pressure, self.clickcount, self.mask, self.state, self.number];
}

@end


@implementation HIDPointerEvent

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldPointerZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldPointerZ, z.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldPointerY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldPointerY, y.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldPointerX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldPointerX, x.floatValue);
}

- (NSNumber *)buttonmask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldPointerButtonMask)];
}

- (void)setButtonmask:(NSNumber *)buttonmask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldPointerButtonMask, buttonmask.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ z:%@ y:%@ x:%@ buttonmask:%@", [super description], self.z, self.y, self.x, self.buttonmask];
}

@end


@implementation HIDMultiAxisPointerEvent

- (NSNumber *)ry {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerRy)];
}

- (void)setRy:(NSNumber *)ry {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerRy, ry.floatValue);
}

- (NSNumber *)rx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerRx)];
}

- (void)setRx:(NSNumber *)rx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerRx, rx.floatValue);
}

- (NSNumber *)rz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerRz)];
}

- (void)setRz:(NSNumber *)rz {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerRz, rz.floatValue);
}

- (NSNumber *)buttonmask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerButtonMask)];
}

- (void)setButtonmask:(NSNumber *)buttonmask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerButtonMask, buttonmask.unsignedIntValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerZ, z.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerX, x.floatValue);
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMultiAxisPointerY, y.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ ry:%@ rx:%@ rz:%@ buttonmask:%@ z:%@ x:%@ y:%@", [super description], self.ry, self.rx, self.rz, self.buttonmask, self.z, self.x, self.y];
}

@end


@implementation HIDDigitizerEvent

- (NSNumber *)type {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerType)];
}

- (void)setType:(NSNumber *)type {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerType, type.unsignedIntValue);
}

- (NSNumber *)childeventmask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerChildEventMask)];
}

- (void)setChildeventmask:(NSNumber *)childeventmask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerChildEventMask, childeventmask.unsignedIntValue);
}

- (NSNumber *)auxiliarypressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerAuxiliaryPressure)];
}

- (void)setAuxiliarypressure:(NSNumber *)auxiliarypressure {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerAuxiliaryPressure, auxiliarypressure.floatValue);
}

- (NSNumber *)qualityradiiaccuracy {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerQualityRadiiAccuracy)];
}

- (void)setQualityradiiaccuracy:(NSNumber *)qualityradiiaccuracy {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerQualityRadiiAccuracy, qualityradiiaccuracy.floatValue);
}

- (NSNumber *)quality {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerQuality)];
}

- (void)setQuality:(NSNumber *)quality {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerQuality, quality.floatValue);
}

- (NSNumber *)minorradius {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerMinorRadius)];
}

- (void)setMinorradius:(NSNumber *)minorradius {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerMinorRadius, minorradius.floatValue);
}

- (NSNumber *)eventmask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerEventMask)];
}

- (void)setEventmask:(NSNumber *)eventmask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerEventMask, eventmask.unsignedIntValue);
}

- (NSNumber *)generationcount {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerGenerationCount)];
}

- (void)setGenerationcount:(NSNumber *)generationcount {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerGenerationCount, generationcount.unsignedIntValue);
}

- (NSNumber *)index {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerIndex)];
}

- (void)setIndex:(NSNumber *)index {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerIndex, index.unsignedIntValue);
}

- (NSNumber *)touch {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerTouch)];
}

- (void)setTouch:(NSNumber *)touch {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerTouch, touch.unsignedIntValue);
}

- (NSNumber *)azimuth {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerAzimuth)];
}

- (void)setAzimuth:(NSNumber *)azimuth {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerAzimuth, azimuth.floatValue);
}

- (NSNumber *)tiltx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerTiltX)];
}

- (void)setTiltx:(NSNumber *)tiltx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerTiltX, tiltx.floatValue);
}

- (NSNumber *)tilty {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerTiltY)];
}

- (void)setTilty:(NSNumber *)tilty {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerTiltY, tilty.floatValue);
}

- (NSNumber *)range {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerRange)];
}

- (void)setRange:(NSNumber *)range {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerRange, range.unsignedIntValue);
}

- (NSNumber *)pressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerPressure)];
}

- (void)setPressure:(NSNumber *)pressure {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerPressure, pressure.floatValue);
}

- (NSNumber *)collection {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerCollection)];
}

- (void)setCollection:(NSNumber *)collection {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerCollection, collection.unsignedIntValue);
}

- (NSNumber *)altitude {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerAltitude)];
}

- (void)setAltitude:(NSNumber *)altitude {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerAltitude, altitude.floatValue);
}

- (NSNumber *)density {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerDensity)];
}

- (void)setDensity:(NSNumber *)density {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerDensity, density.floatValue);
}

- (NSNumber *)orientationtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerOrientationType)];
}

- (NSNumber *)y {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerY)];
}

- (void)setY:(NSNumber *)y {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerY, y.floatValue);
}

- (NSNumber *)willupdatemask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerWillUpdateMask)];
}

- (void)setWillupdatemask:(NSNumber *)willupdatemask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerWillUpdateMask, willupdatemask.unsignedIntValue);
}

- (NSNumber *)identity {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerIdentity)];
}

- (void)setIdentity:(NSNumber *)identity {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerIdentity, identity.unsignedIntValue);
}

- (NSNumber *)twist {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerTwist)];
}

- (void)setTwist:(NSNumber *)twist {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerTwist, twist.floatValue);
}

- (NSNumber *)x {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerX)];
}

- (void)setX:(NSNumber *)x {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerX, x.floatValue);
}

- (NSNumber *)isdisplayintegrated {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerIsDisplayIntegrated)];
}

- (void)setIsdisplayintegrated:(NSNumber *)isdisplayintegrated {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerIsDisplayIntegrated, isdisplayintegrated.unsignedIntValue);
}

- (NSNumber *)z {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerZ)];
}

- (void)setZ:(NSNumber *)z {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerZ, z.floatValue);
}

- (NSNumber *)majorradius {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerMajorRadius)];
}

- (void)setMajorradius:(NSNumber *)majorradius {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerMajorRadius, majorradius.floatValue);
}

- (NSNumber *)buttonmask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerButtonMask)];
}

- (void)setButtonmask:(NSNumber *)buttonmask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerButtonMask, buttonmask.unsignedIntValue);
}

- (NSNumber *)irregularity {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerIrregularity)];
}

- (void)setIrregularity:(NSNumber *)irregularity {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDigitizerIrregularity, irregularity.floatValue);
}

- (NSNumber *)didupdatemask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerDidUpdateMask)];
}

- (void)setDidupdatemask:(NSNumber *)didupdatemask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDigitizerDidUpdateMask, didupdatemask.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ type:%@ childeventmask:%@ auxiliarypressure:%@ qualityradiiaccuracy:%@ quality:%@ minorradius:%@ eventmask:%@ generationcount:%@ index:%@ touch:%@ azimuth:%@ tiltx:%@ tilty:%@ range:%@ pressure:%@ collection:%@ altitude:%@ density:%@ orientationtype:%@ y:%@ willupdatemask:%@ identity:%@ twist:%@ x:%@ isdisplayintegrated:%@ z:%@ majorradius:%@ buttonmask:%@ irregularity:%@ didupdatemask:%@", [super description], self.type, self.childeventmask, self.auxiliarypressure, self.qualityradiiaccuracy, self.quality, self.minorradius, self.eventmask, self.generationcount, self.index, self.touch, self.azimuth, self.tiltx, self.tilty, self.range, self.pressure, self.collection, self.altitude, self.density, self.orientationtype, self.y, self.willupdatemask, self.identity, self.twist, self.x, self.isdisplayintegrated, self.z, self.majorradius, self.buttonmask, self.irregularity, self.didupdatemask];
}

@end


@implementation HIDNavigationSwipeEvent

- (NSNumber *)flavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldNavigationSwipeFlavor)];
}

- (void)setFlavor:(NSNumber *)flavor {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldNavigationSwipeFlavor, flavor.unsignedIntValue);
}

- (NSNumber *)progress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipeProgress)];
}

- (void)setProgress:(NSNumber *)progress {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipeProgress, progress.floatValue);
}

- (NSNumber *)mask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldNavigationSwipeMask)];
}

- (void)setMask:(NSNumber *)mask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldNavigationSwipeMask, mask.unsignedIntValue);
}

- (NSNumber *)motion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldNavigationSwipeMotion)];
}

- (void)setMotion:(NSNumber *)motion {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldNavigationSwipeMotion, motion.unsignedIntValue);
}

- (NSNumber *)positiony {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipePositionY)];
}

- (void)setPositiony:(NSNumber *)positiony {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipePositionY, positiony.floatValue);
}

- (NSNumber *)positionx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipePositionX)];
}

- (void)setPositionx:(NSNumber *)positionx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipePositionX, positionx.floatValue);
}

- (NSNumber *)positionz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipePositionZ)];
}

- (void)setPositionz:(NSNumber *)positionz {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldNavigationSwipePositionZ, positionz.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ flavor:%@ progress:%@ mask:%@ motion:%@ positiony:%@ positionx:%@ positionz:%@", [super description], self.flavor, self.progress, self.mask, self.motion, self.positiony, self.positionx, self.positionz];
}

@end


@implementation HIDDockSwipeEvent

- (NSNumber *)progress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipeProgress)];
}

- (void)setProgress:(NSNumber *)progress {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipeProgress, progress.floatValue);
}

- (NSNumber *)mask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDockSwipeMask)];
}

- (void)setMask:(NSNumber *)mask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDockSwipeMask, mask.unsignedIntValue);
}

- (NSNumber *)motion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDockSwipeMotion)];
}

- (void)setMotion:(NSNumber *)motion {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDockSwipeMotion, motion.unsignedIntValue);
}

- (NSNumber *)flavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldDockSwipeFlavor)];
}

- (void)setFlavor:(NSNumber *)flavor {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldDockSwipeFlavor, flavor.unsignedIntValue);
}

- (NSNumber *)positionx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipePositionX)];
}

- (void)setPositionx:(NSNumber *)positionx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipePositionX, positionx.floatValue);
}

- (NSNumber *)positiony {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipePositionY)];
}

- (void)setPositiony:(NSNumber *)positiony {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipePositionY, positiony.floatValue);
}

- (NSNumber *)positionz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipePositionZ)];
}

- (void)setPositionz:(NSNumber *)positionz {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldDockSwipePositionZ, positionz.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ progress:%@ mask:%@ motion:%@ flavor:%@ positionx:%@ positiony:%@ positionz:%@", [super description], self.progress, self.mask, self.motion, self.flavor, self.positionx, self.positiony, self.positionz];
}

@end


@implementation HIDFluidTouchGestureEvent

- (NSNumber *)positiony {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldFluidTouchGesturePositionY)];
}

- (void)setPositiony:(NSNumber *)positiony {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldFluidTouchGesturePositionY, positiony.floatValue);
}

- (NSNumber *)positionx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldFluidTouchGesturePositionX)];
}

- (void)setPositionx:(NSNumber *)positionx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldFluidTouchGesturePositionX, positionx.floatValue);
}

- (NSNumber *)mask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureMask)];
}

- (void)setMask:(NSNumber *)mask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureMask, mask.unsignedIntValue);
}

- (NSNumber *)progress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureProgress)];
}

- (void)setProgress:(NSNumber *)progress {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureProgress, progress.floatValue);
}

- (NSNumber *)motion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureMotion)];
}

- (void)setMotion:(NSNumber *)motion {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureMotion, motion.unsignedIntValue);
}

- (NSNumber *)flavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureFlavor)];
}

- (void)setFlavor:(NSNumber *)flavor {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldFluidTouchGestureFlavor, flavor.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ positiony:%@ positionx:%@ mask:%@ progress:%@ motion:%@ flavor:%@", [super description], self.positiony, self.positionx, self.mask, self.progress, self.motion, self.flavor];
}

@end


@implementation HIDBoundaryScrollEvent

- (NSNumber *)progress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldBoundaryScrollProgress)];
}

- (void)setProgress:(NSNumber *)progress {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldBoundaryScrollProgress, progress.floatValue);
}

- (NSNumber *)flavor {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBoundaryScrollFlavor)];
}

- (void)setFlavor:(NSNumber *)flavor {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBoundaryScrollFlavor, flavor.unsignedIntValue);
}

- (NSNumber *)positiony {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldBoundaryScrollPositionY)];
}

- (void)setPositiony:(NSNumber *)positiony {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldBoundaryScrollPositionY, positiony.floatValue);
}

- (NSNumber *)positionx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldBoundaryScrollPositionX)];
}

- (void)setPositionx:(NSNumber *)positionx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldBoundaryScrollPositionX, positionx.floatValue);
}

- (NSNumber *)mask {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBoundaryScrollMask)];
}

- (void)setMask:(NSNumber *)mask {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBoundaryScrollMask, mask.unsignedIntValue);
}

- (NSNumber *)motion {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldBoundaryScrollMotion)];
}

- (void)setMotion:(NSNumber *)motion {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldBoundaryScrollMotion, motion.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ progress:%@ flavor:%@ positiony:%@ positionx:%@ mask:%@ motion:%@", [super description], self.progress, self.flavor, self.positiony, self.positionx, self.mask, self.motion];
}

@end


@implementation HIDSymbolicHotKeyEvent

- (NSNumber *)value {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldSymbolicHotKeyValue)];
}

- (void)setValue:(NSNumber *)value {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldSymbolicHotKeyValue, value.unsignedIntValue);
}

- (NSNumber *)iscgsevent {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldSymbolicHotKeyIsCGSEvent)];
}

- (void)setIscgsevent:(NSNumber *)iscgsevent {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldSymbolicHotKeyIsCGSEvent, iscgsevent.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ value:%@ iscgsevent:%@", [super description], self.value, self.iscgsevent];
}

@end


@implementation HIDPowerEvent

- (NSNumber *)type {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldPowerType)];
}

- (void)setType:(NSNumber *)type {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldPowerType, type.unsignedIntValue);
}

- (NSNumber *)subtype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldPowerSubType)];
}

- (void)setSubtype:(NSNumber *)subtype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldPowerSubType, subtype.unsignedIntValue);
}

- (NSNumber *)measurement {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldPowerMeasurement)];
}

- (void)setMeasurement:(NSNumber *)measurement {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldPowerMeasurement, measurement.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ type:%@ subtype:%@ measurement:%@", [super description], self.type, self.subtype, self.measurement];
}

@end


@implementation HIDAtmosphericPressureEvent

- (NSNumber *)level {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldAtmosphericPressureLevel)];
}

- (void)setLevel:(NSNumber *)level {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldAtmosphericPressureLevel, level.floatValue);
}

- (NSNumber *)sequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldAtmosphericSequence)];
}

- (void)setSequence:(NSNumber *)sequence {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldAtmosphericSequence, sequence.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ level:%@ sequence:%@", [super description], self.level, self.sequence];
}

@end


@implementation HIDForceEvent

- (NSNumber *)stagepressure {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldForceStagePressure)];
}

- (void)setStagepressure:(NSNumber *)stagepressure {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldForceStagePressure, stagepressure.floatValue);
}

- (NSNumber *)stage {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldForceStage)];
}

- (void)setStage:(NSNumber *)stage {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldForceStage, stage.unsignedIntValue);
}

- (NSNumber *)progress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldForceProgress)];
}

- (void)setProgress:(NSNumber *)progress {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldForceProgress, progress.floatValue);
}

- (NSNumber *)behavior {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldForceBehavior)];
}

- (void)setBehavior:(NSNumber *)behavior {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldForceBehavior, behavior.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ stagepressure:%@ stage:%@ progress:%@ behavior:%@", [super description], self.stagepressure, self.stage, self.progress, self.behavior];
}

@end


@implementation HIDMotionActivityEvent

- (NSNumber *)confidence {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMotionActivityConfidence)];
}

- (void)setConfidence:(NSNumber *)confidence {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMotionActivityConfidence, confidence.floatValue);
}

- (NSNumber *)activitytype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldMotionActivityActivityType)];
}

- (void)setActivitytype:(NSNumber *)activitytype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldMotionActivityActivityType, activitytype.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ confidence:%@ activitytype:%@", [super description], self.confidence, self.activitytype];
}

@end


@implementation HIDMotionGestureEvent

- (NSNumber *)progress {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldMotionGestureProgress)];
}

- (void)setProgress:(NSNumber *)progress {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldMotionGestureProgress, progress.floatValue);
}

- (NSNumber *)gesturetype {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldMotionGestureGestureType)];
}

- (void)setGesturetype:(NSNumber *)gesturetype {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldMotionGestureGestureType, gesturetype.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ progress:%@ gesturetype:%@", [super description], self.progress, self.gesturetype];
}

@end


@implementation HIDGameControllerEvent

- (NSNumber *)joystickaxisx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisX)];
}

- (void)setJoystickaxisx:(NSNumber *)joystickaxisx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisX, joystickaxisx.floatValue);
}

- (NSNumber *)joystickaxisy {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisY)];
}

- (void)setJoystickaxisy:(NSNumber *)joystickaxisy {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisY, joystickaxisy.floatValue);
}

- (NSNumber *)type {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldGameControllerType)];
}

- (void)setType:(NSNumber *)type {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldGameControllerType, type.unsignedIntValue);
}

- (NSNumber *)directionpadright {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadRight)];
}

- (void)setDirectionpadright:(NSNumber *)directionpadright {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadRight, directionpadright.floatValue);
}

- (NSNumber *)shoulderbuttonr1 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonR1)];
}

- (void)setShoulderbuttonr1:(NSNumber *)shoulderbuttonr1 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonR1, shoulderbuttonr1.floatValue);
}

- (NSNumber *)facebuttona {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonA)];
}

- (void)setFacebuttona:(NSNumber *)facebuttona {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonA, facebuttona.floatValue);
}

- (NSNumber *)facebuttonb {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonB)];
}

- (void)setFacebuttonb:(NSNumber *)facebuttonb {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonB, facebuttonb.floatValue);
}

- (NSNumber *)directionpadleft {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadLeft)];
}

- (void)setDirectionpadleft:(NSNumber *)directionpadleft {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadLeft, directionpadleft.floatValue);
}

- (NSNumber *)directionpaddown {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadDown)];
}

- (void)setDirectionpaddown:(NSNumber *)directionpaddown {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadDown, directionpaddown.floatValue);
}

- (NSNumber *)joystickaxisz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisZ)];
}

- (void)setJoystickaxisz:(NSNumber *)joystickaxisz {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisZ, joystickaxisz.floatValue);
}

- (NSNumber *)shoulderbuttonr2 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonR2)];
}

- (void)setShoulderbuttonr2:(NSNumber *)shoulderbuttonr2 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonR2, shoulderbuttonr2.floatValue);
}

- (NSNumber *)facebuttony {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonY)];
}

- (void)setFacebuttony:(NSNumber *)facebuttony {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonY, facebuttony.floatValue);
}

- (NSNumber *)shoulderbuttonl2 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonL2)];
}

- (void)setShoulderbuttonl2:(NSNumber *)shoulderbuttonl2 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonL2, shoulderbuttonl2.floatValue);
}

- (NSNumber *)joystickaxisrz {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisRz)];
}

- (void)setJoystickaxisrz:(NSNumber *)joystickaxisrz {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerJoyStickAxisRz, joystickaxisrz.floatValue);
}

- (NSNumber *)shoulderbuttonl1 {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonL1)];
}

- (void)setShoulderbuttonl1:(NSNumber *)shoulderbuttonl1 {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerShoulderButtonL1, shoulderbuttonl1.floatValue);
}

- (NSNumber *)facebuttonx {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonX)];
}

- (void)setFacebuttonx:(NSNumber *)facebuttonx {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerFaceButtonX, facebuttonx.floatValue);
}

- (NSNumber *)directionpadup {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadUp)];
}

- (void)setDirectionpadup:(NSNumber *)directionpadup {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldGameControllerDirectionPadUp, directionpadup.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ joystickaxisx:%@ joystickaxisy:%@ type:%@ directionpadright:%@ shoulderbuttonr1:%@ facebuttona:%@ facebuttonb:%@ directionpadleft:%@ directionpaddown:%@ joystickaxisz:%@ shoulderbuttonr2:%@ facebuttony:%@ shoulderbuttonl2:%@ joystickaxisrz:%@ shoulderbuttonl1:%@ facebuttonx:%@ directionpadup:%@", [super description], self.joystickaxisx, self.joystickaxisy, self.type, self.directionpadright, self.shoulderbuttonr1, self.facebuttona, self.facebuttonb, self.directionpadleft, self.directionpaddown, self.joystickaxisz, self.shoulderbuttonr2, self.facebuttony, self.shoulderbuttonl2, self.joystickaxisrz, self.shoulderbuttonl1, self.facebuttonx, self.directionpadup];
}

@end


@implementation HIDHumidityEvent

- (NSNumber *)sequence {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldHumiditySequence)];
}

- (void)setSequence:(NSNumber *)sequence {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldHumiditySequence, sequence.unsignedIntValue);
}

- (NSNumber *)rh {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldHumidityRH)];
}

- (void)setRh:(NSNumber *)rh {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldHumidityRH, rh.floatValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ sequence:%@ rh:%@", [super description], self.sequence, self.rh];
}

@end


@implementation HIDBrightnessEvent

- (NSNumber *)targetbrightness {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldTargetBrightness)];
}

- (void)setTargetbrightness:(NSNumber *)targetbrightness {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldTargetBrightness, targetbrightness.floatValue);
}

- (NSNumber *)currentbrightness {
    return [NSNumber numberWithFloat:IOHIDEventGetFloatValue(self->eventRef, kIOHIDEventFieldCurrentBrightness)];
}

- (void)setCurrentbrightness:(NSNumber *)currentbrightness {
    IOHIDEventSetFloatValue(self->eventRef, kIOHIDEventFieldCurrentBrightness, currentbrightness.floatValue);
}

- (NSNumber *)transitiontime {
    return [NSNumber numberWithInteger:IOHIDEventGetIntegerValue(self->eventRef, kIOHIDEventFieldTransitionTime)];
}

- (void)setTransitiontime:(NSNumber *)transitiontime {
    IOHIDEventSetIntegerValue(self->eventRef, kIOHIDEventFieldTransitionTime, transitiontime.unsignedIntValue);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"%@ targetbrightness:%@ currentbrightness:%@ transitiontime:%@", [super description], self.targetbrightness, self.currentbrightness, self.transitiontime];
}

@end

