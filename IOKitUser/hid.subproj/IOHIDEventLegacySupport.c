
#include <CoreFoundation/CFRuntime.h>
#include "IOHIDEventLegacySupport.h"
#include <strings.h>
#include <IOKit/hid/IOHIDEventData.h>
#include <IOKit/hid/IOHIDEventMacroDefs.h>
#include <math.h>

#if TARGET_OS_VISION
void             __IOHIDEventPopulateDigitizerLegacyData(IOHIDDigitizerLegacyEventData * legacyEventData, IOHIDDigitizerEventData * currentEventData);
void             __IOHIDEventPopulatePointerLegacyData(IOHIDPointerLegacyEventData * legacyEventData, IOHIDPointerEventData * currentEventData);
void             __IOHIDEventPopulateTranslationLegacyData(IOHIDTranslationLegacyEventData * legacyEventData, IOHIDTranslationEventData * currentEventData);
void             __IOHIDEventPopulateDigitizerCurrentData(IOHIDDigitizerLegacyEventData * legacyEventData, IOHIDDigitizerEventData * currentEventData);
void             __IOHIDEventPopulatePointerCurrentData(IOHIDPointerLegacyEventData * legacyEventData, IOHIDPointerEventData * currentEventData);
void             __IOHIDEventPopulateTranslationCurrentData(IOHIDTranslationLegacyEventData * legacyEventData, IOHIDTranslationEventData * currentEventData);
#endif /* TARGET_OS_VISION */


//------------------------------------------------------------------------------
// __IOHIDEventHasLegacyEventData
//------------------------------------------------------------------------------
bool __IOHIDEventHasLegacyEventData(IOHIDEventType type)
{
 if( type == kIOHIDEventTypeDigitizer ||
     type == kIOHIDEventTypePointer ||
     type == kIOHIDEventTypeTranslation) {
     return true;
 }
 return false;
}

//------------------------------------------------------------------------------
// __IOHIDEventDataAppendFromLegacyEvent
//------------------------------------------------------------------------------
CFIndex __IOHIDEventDataAppendFromLegacyEvent(IOHIDEventData * eventData, UInt8* buffer)
{
    CFIndex size = 0;
#if TARGET_OS_VISION
    switch(eventData->type) {
        case kIOHIDEventTypeDigitizer:
            size = sizeof(IOHIDDigitizerLegacyEventData);
            bzero(buffer, size);
            __IOHIDEventPopulateDigitizerLegacyData((IOHIDDigitizerLegacyEventData *)buffer, (IOHIDDigitizerEventData *)eventData);
            break;
        case kIOHIDEventTypePointer:
            size = sizeof(IOHIDPointerLegacyEventData);
            bzero(buffer, size);
            __IOHIDEventPopulatePointerLegacyData((IOHIDPointerLegacyEventData *)buffer, (IOHIDPointerEventData *)eventData);
            break;
        case kIOHIDEventTypeTranslation:
            size = sizeof(IOHIDTranslationLegacyEventData);
            bzero(buffer, size);
            __IOHIDEventPopulateTranslationLegacyData((IOHIDTranslationLegacyEventData *)buffer, (IOHIDTranslationEventData *)eventData);
            break;
    }
#endif /* TARGET_OS_VISION */
    return size;
}

//------------------------------------------------------------------------------
// __IOHIDEventPopulateCurrentEventData
//------------------------------------------------------------------------------
void __IOHIDEventPopulateCurrentEventData(IOHIDEventData * eventData, IOHIDEventData * newEventData)
{
    IOHIDEventType type = (IOHIDEventType)eventData->type;
#if TARGET_OS_VISION
    if(type == kIOHIDEventTypeDigitizer) {
        __IOHIDEventPopulateDigitizerCurrentData((IOHIDDigitizerLegacyEventData *)eventData, (IOHIDDigitizerEventData *)newEventData);
    } else if (type == kIOHIDEventTypePointer) {
        __IOHIDEventPopulatePointerCurrentData((IOHIDPointerLegacyEventData *)eventData, (IOHIDPointerEventData *)newEventData);
    } else if (type == kIOHIDEventTypeTranslation) {
        __IOHIDEventPopulateTranslationCurrentData((IOHIDTranslationLegacyEventData *)eventData, (IOHIDTranslationEventData *)newEventData);
    }
#endif /* TARGET_OS_VISION */
}

#if TARGET_OS_VISION
//------------------------------------------------------------------------------
// __IOHIDEventPopulateDigitizerLegacyData
//------------------------------------------------------------------------------
void __IOHIDEventPopulateDigitizerCurrentData(IOHIDDigitizerLegacyEventData * legacyEventData, IOHIDDigitizerEventData * currentEventData)
{
    currentEventData->size                       = sizeof(IOHIDDigitizerEventData);
    currentEventData->type                       = legacyEventData->type;
    currentEventData->options.reserved0          = legacyEventData->options.reserved0;
    currentEventData->options.collection         = legacyEventData->options.collection;
    currentEventData->options.reserved15         = legacyEventData->options.reserved15;
    currentEventData->options.range              = legacyEventData->options.range;
    currentEventData->options.touch              = legacyEventData->options.touch;
    currentEventData->options.reserved16         = legacyEventData->options.reserved16;
    currentEventData->options.displayIntegrated  = legacyEventData->options.displayIntegrated;
    currentEventData->depth                      = legacyEventData->depth;
    currentEventData->reserved[0]                = legacyEventData->reserved[0];
    currentEventData->reserved[1]                = legacyEventData->reserved[1];
    currentEventData->reserved[2]                = legacyEventData->reserved[2];
    
    currentEventData->eventMask        = legacyEventData->eventMask;
    currentEventData->childEventMask   = legacyEventData->childEventMask;
    currentEventData->buttonMask       = legacyEventData->buttonMask;
    currentEventData->transducerIndex  = legacyEventData->transducerIndex;
    currentEventData->transducerType   = legacyEventData->transducerType;
    currentEventData->identity         = legacyEventData->identity;
 
    currentEventData->position.x       = CAST_FIXED_TO_DOUBLE(legacyEventData->position.x);
    currentEventData->position.y       = CAST_FIXED_TO_DOUBLE(legacyEventData->position.y);
    currentEventData->position.z       = CAST_FIXED_TO_DOUBLE(legacyEventData->position.z);
    
    currentEventData->pressure         = CAST_FIXED_TO_DOUBLE(legacyEventData->pressure);
    currentEventData->auxPressure      = CAST_FIXED_TO_DOUBLE(legacyEventData->auxPressure);
    currentEventData->angle.twist      = CAST_FIXED_TO_DOUBLE(legacyEventData->angle.twist);
    currentEventData->angle.roll       = CAST_FIXED_TO_DOUBLE(legacyEventData->angle.roll);
    
    currentEventData->orientationType  = legacyEventData->orientationType;
    
    switch(currentEventData->orientationType) {
        case kIOHIDDigitizerOrientationTypeTilt:
            currentEventData->orientation.tilt.x = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.tilt.x);
            currentEventData->orientation.tilt.y = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.tilt.y);
            break;
        case kIOHIDDigitizerOrientationTypePolar:
            currentEventData->orientation.polar.altitude = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.polar.altitude);
            currentEventData->orientation.polar.azimuth  = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.polar.azimuth);
            currentEventData->orientation.polar.quality  = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.polar.quality);
            currentEventData->orientation.polar.density  = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.polar.density);
            currentEventData->orientation.polar.majorRadius = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.polar.majorRadius);
            currentEventData->orientation.polar.minorRadius = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.polar.minorRadius);
            break;
        case kIOHIDDigitizerOrientationTypeQuality:
            currentEventData->orientation.quality.quality = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.quality.quality);
            currentEventData->orientation.quality.density = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.quality.density);
            currentEventData->orientation.quality.irregularity = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.quality.irregularity);
            currentEventData->orientation.quality.majorRadius = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.quality.majorRadius);
            currentEventData->orientation.quality.minorRadius = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.quality.minorRadius);
            currentEventData->orientation.quality.accuracy = CAST_FIXED_TO_DOUBLE(legacyEventData->orientation.quality.accuracy);
            break;
    }
    
    currentEventData->generationCount  = legacyEventData->generationCount;
    currentEventData->willUpdateMask   = legacyEventData->willUpdateMask;
    currentEventData->didUpdateMask    = legacyEventData->didUpdateMask;
}

//------------------------------------------------------------------------------
// __IOHIDEventPopulateDigitizerLegacyData
//------------------------------------------------------------------------------
void __IOHIDEventPopulateDigitizerLegacyData(IOHIDDigitizerLegacyEventData * legacyEventData, IOHIDDigitizerEventData * currentEventData)
{
    legacyEventData->size                       = sizeof(IOHIDDigitizerLegacyEventData);
    legacyEventData->type                       = currentEventData->type;
    legacyEventData->options.reserved0          = currentEventData->options.reserved0;
    legacyEventData->options.collection         = currentEventData->options.collection;
    legacyEventData->options.reserved15         = currentEventData->options.reserved15;
    legacyEventData->options.range              = currentEventData->options.range;
    legacyEventData->options.touch              = currentEventData->options.touch;
    legacyEventData->options.reserved16         = currentEventData->options.reserved16;
    legacyEventData->options.displayIntegrated  = currentEventData->options.displayIntegrated;
    legacyEventData->depth                      = currentEventData->depth;
    legacyEventData->reserved[0]                = currentEventData->reserved[0];
    legacyEventData->reserved[1]                = currentEventData->reserved[1];
    legacyEventData->reserved[2]                = currentEventData->reserved[2];
    
    legacyEventData->eventMask        = currentEventData->eventMask;
    legacyEventData->childEventMask   = currentEventData->childEventMask;
    legacyEventData->buttonMask       = currentEventData->buttonMask;
    legacyEventData->transducerIndex  = currentEventData->transducerIndex;
    legacyEventData->transducerType   = currentEventData->transducerType;
    legacyEventData->identity         = currentEventData->identity;
 
    legacyEventData->position.x       = CAST_DOUBLE_TO_FIXED(currentEventData->position.x);
    legacyEventData->position.y       = CAST_DOUBLE_TO_FIXED(currentEventData->position.y);
    legacyEventData->position.z       = CAST_DOUBLE_TO_FIXED(currentEventData->position.z);
    
    legacyEventData->pressure         = CAST_DOUBLE_TO_FIXED(currentEventData->pressure);
    legacyEventData->auxPressure      = CAST_DOUBLE_TO_FIXED(currentEventData->auxPressure);
    legacyEventData->angle.twist      = CAST_DOUBLE_TO_FIXED(currentEventData->angle.twist);
    legacyEventData->angle.roll       = CAST_DOUBLE_TO_FIXED(currentEventData->angle.roll);
    
    legacyEventData->orientationType  = currentEventData->orientationType;
    
    switch(currentEventData->orientationType) {
        case kIOHIDDigitizerOrientationTypeTilt:
            legacyEventData->orientation.tilt.x = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.tilt.x);
            legacyEventData->orientation.tilt.y = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.tilt.y);
            break;
        case kIOHIDDigitizerOrientationTypePolar:
            legacyEventData->orientation.polar.altitude = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.polar.altitude);
            legacyEventData->orientation.polar.azimuth  = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.polar.azimuth);
            legacyEventData->orientation.polar.quality = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.polar.quality);
            legacyEventData->orientation.polar.density = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.polar.density);
            legacyEventData->orientation.polar.majorRadius = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.polar.majorRadius);
            legacyEventData->orientation.polar.minorRadius = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.polar.minorRadius);
            break;
        case kIOHIDDigitizerOrientationTypeQuality:
            legacyEventData->orientation.quality.quality = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.quality.quality);
            legacyEventData->orientation.quality.density = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.quality.density);
            legacyEventData->orientation.quality.irregularity = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.quality.irregularity);
            legacyEventData->orientation.quality.majorRadius = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.quality.majorRadius);
            legacyEventData->orientation.quality.minorRadius = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.quality.minorRadius);
            legacyEventData->orientation.quality.accuracy = CAST_DOUBLE_TO_FIXED(currentEventData->orientation.quality.accuracy);
            break;
    }
    
    legacyEventData->generationCount  = currentEventData->generationCount;
    legacyEventData->willUpdateMask   = currentEventData->willUpdateMask;
    legacyEventData->didUpdateMask    = currentEventData->didUpdateMask;
}

//------------------------------------------------------------------------------
// __IOHIDEventPopulatePointerLegacyData
//------------------------------------------------------------------------------
void __IOHIDEventPopulatePointerLegacyData(IOHIDPointerLegacyEventData * legacyEventData, IOHIDPointerEventData * currentEventData)
{
    legacyEventData->size          = sizeof(IOHIDPointerLegacyEventData);
    legacyEventData->type          = currentEventData->type;
    legacyEventData->options       = currentEventData->options;
    legacyEventData->depth         = currentEventData->depth;
    legacyEventData->reserved[0]   = currentEventData->reserved[0];
    legacyEventData->reserved[1]   = currentEventData->reserved[1];
    legacyEventData->reserved[2]   = currentEventData->reserved[2];
    
    legacyEventData->position.x    = CAST_DOUBLE_TO_FIXED(currentEventData->position.x);
    legacyEventData->position.y    = CAST_DOUBLE_TO_FIXED(currentEventData->position.y);
    legacyEventData->position.z    = CAST_DOUBLE_TO_FIXED(currentEventData->position.z);
    
    legacyEventData->button.mask   = currentEventData->button.mask;
}

//------------------------------------------------------------------------------
// __IOHIDEventPopulatePointerLegacyData
//------------------------------------------------------------------------------
void __IOHIDEventPopulatePointerCurrentData(IOHIDPointerLegacyEventData * legacyEventData, IOHIDPointerEventData * currentEventData)
{
    currentEventData->size          = sizeof(IOHIDPointerEventData);
    currentEventData->type          = legacyEventData->type;
    currentEventData->options       = legacyEventData->options;
    currentEventData->depth         = legacyEventData->depth;
    currentEventData->reserved[0]   = legacyEventData->reserved[0];
    currentEventData->reserved[1]   = legacyEventData->reserved[1];
    currentEventData->reserved[2]   = legacyEventData->reserved[2];
    
    currentEventData->position.x    = CAST_FIXED_TO_DOUBLE(legacyEventData->position.x);
    currentEventData->position.y    = CAST_FIXED_TO_DOUBLE(legacyEventData->position.y);
    currentEventData->position.z    = CAST_FIXED_TO_DOUBLE(legacyEventData->position.z);
    
    currentEventData->button.mask   = legacyEventData->button.mask;
}

//------------------------------------------------------------------------------
// __IOHIDEventPopulateTranslationLegacyData
//------------------------------------------------------------------------------
void __IOHIDEventPopulateTranslationLegacyData(IOHIDTranslationLegacyEventData * legacyEventData, IOHIDTranslationEventData * currentEventData)
{
    legacyEventData->size          = sizeof(IOHIDTranslationLegacyEventData);
    legacyEventData->type          = currentEventData->type;
    legacyEventData->options       = currentEventData->options;
    legacyEventData->depth         = currentEventData->depth;
    legacyEventData->reserved[0]   = currentEventData->reserved[0];
    legacyEventData->reserved[1]   = currentEventData->reserved[1];
    legacyEventData->reserved[2]   = currentEventData->reserved[2];
    
    legacyEventData->position.x    = CAST_DOUBLE_TO_FIXED(currentEventData->position.x);
    legacyEventData->position.y    = CAST_DOUBLE_TO_FIXED(currentEventData->position.y);
    legacyEventData->position.z    = CAST_DOUBLE_TO_FIXED(currentEventData->position.z);
}

//------------------------------------------------------------------------------
// __IOHIDEventPopulateTranslationLegacyData
//------------------------------------------------------------------------------
void __IOHIDEventPopulateTranslationCurrentData(IOHIDTranslationLegacyEventData * legacyEventData, IOHIDTranslationEventData * currentEventData)
{
    currentEventData->size          = sizeof(IOHIDTranslationEventData);
    currentEventData->type          = legacyEventData->type;
    currentEventData->options       = legacyEventData->options;
    currentEventData->depth         = legacyEventData->depth;
    currentEventData->reserved[0]   = legacyEventData->reserved[0];
    currentEventData->reserved[1]   = legacyEventData->reserved[1];
    currentEventData->reserved[2]   = legacyEventData->reserved[2];
    
    currentEventData->position.x    = CAST_FIXED_TO_DOUBLE(legacyEventData->position.x);
    currentEventData->position.y    = CAST_FIXED_TO_DOUBLE(legacyEventData->position.y);
    currentEventData->position.z    = CAST_FIXED_TO_DOUBLE(legacyEventData->position.z);
}

#endif

