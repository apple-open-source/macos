/*
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2009 Apple Computer, Inc.  All Rights Reserved.
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

#include <IOKit/hidevent/IOHIDEventDriver.h>
#include "IOHIDInterface.h"
#include "IOHIDKeys.h"
#include "IOHIDTypes.h"
#include "AppleHIDUsageTables.h"
#include "IOHIDPrivateKeys.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/IOLib.h>
#include <IOKit/usb/USB.h>
#include "IOHIDFamilyTrace.h"

enum {
    kMouseButtons   = 0x1,
    kMouseXAxis     = 0x2,
    kMouseYAxis     = 0x4
};

enum {
    kBootMouse      = (kMouseXAxis | kMouseYAxis | kMouseButtons)
};

enum {
    kBootProtocolNone   = 0,
    kBootProtocolKeyboard,
    kBootProtocolMouse
};

// Describes the handler(s) at each report dispatch table slot.
//
struct IOHIDReportHandler
{
    OSArray * array[ kIOHIDReportTypeCount ];
};

#define kReportHandlerSlots	8

#define GetReportHandlerSlot(id)    \
    ((id) & (kReportHandlerSlots - 1))

#define GetElementArray(slot, type) \
    _reportHandlers[slot].array[type]

#define GetReportType( type )                                               \
    ((type <= kIOHIDElementTypeInput_ScanCodes) ? kIOHIDReportTypeInput :   \
    (type <= kIOHIDElementTypeOutput) ? kIOHIDReportTypeOutput :            \
    (type <= kIOHIDElementTypeFeature) ? kIOHIDReportTypeFeature : -1)

//===========================================================================
// IOHIDEventDriver class

#define super IOHIDEventService

OSDefineMetaClassAndStructors( IOHIDEventDriver, IOHIDEventService )

//====================================================================================================
// IOHIDEventDriver::init
//====================================================================================================
bool IOHIDEventDriver::init( OSDictionary * dictionary )
{
    if ( ! super::init ( dictionary ) )
        return false;

	_cachedRangeState					= true;
	_relativeButtonCollection   = false;
    _multipleReports            = false;
    
    return true;
}

//====================================================================================================
// IOHIDEventDriver::free
//====================================================================================================
void IOHIDEventDriver::free ()
{
    if ( _reportHandlers )
    {
        OSArray * temp;
        for (int i=0; i<kReportHandlerSlots; i++)
        {
            for (int j=0; j<kIOHIDReportTypeCount; j++)
            {
                temp = GetElementArray(i, j);
                if ( temp == NULL ) continue;
                
                temp->release();
            }
        }
        IOFree( _reportHandlers,
                sizeof(IOHIDReportHandler) * kReportHandlerSlots );
        _reportHandlers = 0;
    }
    
    if ( _supportedElements )
    {
        _supportedElements->release();
        _supportedElements = 0;
    }
    
    super::free();
}

//====================================================================================================
// IOHIDEventDriver::handleStart
//====================================================================================================
bool IOHIDEventDriver::handleStart( IOService * provider )
{
	_interface = OSDynamicCast( IOHIDInterface, provider );
	
	if ( !_interface )
		return false;

    IOService * service = provider->getProvider();
    
    // Check to see if this is a product of an IOHIDevice or IOHIDDevice shim
    while ( NULL != (service = service->getProvider()) )
    {
        if(service->metaCast("IOHIDevice") || service->metaCast("IOHIDDevice"))
        {
            return false;
        }
    }
						
	if (!_interface->open(this, 0, _handleInterruptReport, 0))
        return false;

    _reportHandlers = (IOHIDReportHandler *)
                      IOMalloc( sizeof(IOHIDReportHandler) *
                                kReportHandlerSlots );
    if ( _reportHandlers == 0 )
        return false;

    bzero( _reportHandlers, sizeof(IOHIDReportHandler) * kReportHandlerSlots );
    
    UInt32      bootProtocol    = 0;
    OSNumber *  number          = (OSNumber *)_interface->copyProperty("BootProtocol");
    
    if (number) 
        bootProtocol = number->unsigned32BitValue();
    OSSafeReleaseNULL(number);
        
    setProperty("BootProtocol", number);

    if ( !findElements ( _interface->createMatchingElements(), bootProtocol ))
        return false;
        
    return true;
}

//====================================================================================================
// IOHIDEventDriver::getTransport
//====================================================================================================
OSString * IOHIDEventDriver::getTransport ()
{
    return _interface->getTransport();
}

//====================================================================================================
// IOHIDEventDriver::getManufacturer
//====================================================================================================
OSString * IOHIDEventDriver::getManufacturer ()
{
    return _interface->getManufacturer();
}

//====================================================================================================
// IOHIDEventDriver::getProduct
//====================================================================================================
OSString * IOHIDEventDriver::getProduct ()
{
    return _interface->getProduct();
}

//====================================================================================================
// IOHIDEventDriver::getSerialNumber
//====================================================================================================
OSString * IOHIDEventDriver::getSerialNumber ()
{
    return _interface->getSerialNumber();
}

//====================================================================================================
// IOHIDEventDriver::getLocationID
//====================================================================================================
UInt32 IOHIDEventDriver::getLocationID ()
{
    return _interface->getLocationID();
}

//====================================================================================================
// IOHIDEventDriver::getVendorID
//====================================================================================================
UInt32 IOHIDEventDriver::getVendorID ()
{
    return _interface->getVendorID();
}

//====================================================================================================
// IOHIDEventDriver::getVendorIDSource
//====================================================================================================
UInt32 IOHIDEventDriver::getVendorIDSource ()
{
    return _interface->getVendorIDSource();
}

//====================================================================================================
// IOHIDEventDriver::getProductID
//====================================================================================================
UInt32 IOHIDEventDriver::getProductID ()
{
    return _interface->getProductID();
}

//====================================================================================================
// IOHIDEventDriver::getVersion
//====================================================================================================
UInt32 IOHIDEventDriver::getVersion ()
{
    return _interface->getVersion();
}

//====================================================================================================
// IOHIDEventDriver::getCountryCode
//====================================================================================================
UInt32 IOHIDEventDriver::getCountryCode ()
{
    return _interface->getCountryCode();
}


//====================================================================================================
// IOHIDEventDriver::handleStop
//====================================================================================================
void IOHIDEventDriver::handleStop(  IOService * provider __unused )
{
    //_interface->close(this);
}

//=============================================================================================
// IOHIDEventDriver::didTerminate
//=============================================================================================
bool IOHIDEventDriver::didTerminate( IOService * provider, IOOptionBits options, bool * defer )
{
    _interface->close(this);

    return super::didTerminate(provider, options, defer);
}

//====================================================================================================
// IOHIDEventDriver::findElements
//====================================================================================================
bool IOHIDEventDriver::findElements ( OSArray* elementArray, UInt32 bootProtocol)
{
    UInt32              count       = 0;
    UInt32              index       = 0;
    UInt32              usage       = 0;
    UInt32              usagePage   = 0;
    bool                stored      = false;
    bool                pointer     = false;
    bool                supportsInk = false;
    IOHIDElement *      element     = 0;
	IOHIDElement *		buttonCollection = 0;
	IOHIDElement *		relativeCollection = 0;
    OSArray *           buttonArray = 0;
    
    if ( bootProtocol == kBootProtocolMouse )
        _bootSupport = kBootMouse;
    
    if ( elementArray )
    {
        _supportedElements = elementArray;
            
        count = elementArray->getCount();
        
        for ( index = 0; index < count; index++ )
        {
            element = (IOHIDElement *) elementArray->getObject(index);
            
            if ((element == NULL) || 
                (element->getType() == kIOHIDElementTypeCollection)) continue;
                    
            if ( element->getReportID() > 0)
                _multipleReports = true;

            usagePage   = element->getUsagePage();
            usage       = element->getUsage();

            if ( usagePage == kHIDPage_GenericDesktop )
            {
                switch ( usage )
                {
                    case kHIDUsage_GD_Wheel:
                    case kHIDUsage_GD_Z:
                        stored |= storeReportElement ( element ); 
                        break;       
                        
                    case kHIDUsage_GD_X:
                        _bootSupport &= ~kMouseXAxis;
                            
                        if ((element->getFlags() & kIOHIDElementFlagsRelativeMask) != 0)
                            relativeCollection = element->getParentElement();
                        else
                            supportsInk = true;
                        
                        stored |= storeReportElement ( element ); 
                        pointer = true;
                        break;       

                    case kHIDUsage_GD_Y:
                        _bootSupport &= ~kMouseYAxis;
                        
                        stored |= storeReportElement ( element );
                        pointer = true;
                        break;       
                        
                    case kHIDUsage_GD_SystemPowerDown:
                    case kHIDUsage_GD_SystemSleep:
                    case kHIDUsage_GD_SystemWakeUp:
                        stored |= storeReportElement ( element ); 
                        break;       
                }
            }
        
            else if ( usagePage == kHIDPage_Digitizer )
            {
                switch ( usage ) 
                {
                    case kHIDUsage_Dig_InRange:
                        _cachedRangeState = false;
                        stored |= storeReportElement ( element );
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        supportsInk = true;
                    case kHIDUsage_Dig_BarrelSwitch:
                    case kHIDUsage_Dig_TipSwitch:
                    case kHIDUsage_Dig_Eraser:
                        _bootSupport &= ~kMouseButtons;
                    case kHIDUsage_Dig_XTilt:
                    case kHIDUsage_Dig_YTilt:
                    case kHIDUsage_Dig_Twist:
                    case kHIDUsage_Dig_TransducerIndex:
                    case kHIDUsage_Dig_Invert:
                        stored |= storeReportElement ( element );
                        break;
                }
            }
                
            else if (( usagePage == kHIDPage_KeyboardOrKeypad ) &&
                (( usage >= kHIDUsage_KeyboardA ) && ( usage <= kHIDUsage_KeyboardRightGUI )))
            {
                    stored |= storeReportElement ( element );
            }

            else if ( usagePage == kHIDPage_Button )
            {
                if ( !buttonArray )
                    buttonArray = OSArray::withCapacity(4);
                    
                // RY: Save the buttons for later.
                if ( buttonArray )
                    buttonArray->setObject(element);
            }

            else if ( usagePage == kHIDPage_Consumer )
            {
                stored |= storeReportElement ( element );
            }
            
            else if (( usagePage == kHIDPage_LEDs ) &&
                (((usage == kHIDUsage_LED_NumLock) || (usage == kHIDUsage_LED_CapsLock))
                && (_ledElements[usage - kHIDUsage_LED_NumLock] == 0)))
            {
                _ledElements[usage - kHIDUsage_LED_NumLock] = element;
            }
            else if ((getVendorID() == kIOUSBVendorIDAppleComputer) 
                && (((usagePage == kHIDPage_AppleVendorTopCase) && (usage == kHIDUsage_AV_TopCase_KeyboardFn)) 
                || ((usagePage == kHIDPage_AppleVendorKeyboard) && (usage == kHIDUsage_AppleVendorKeyboard_Function))))
            {
                stored |= storeReportElement ( element );
            }
        }
    }
    
    // RY: Add the buttons only if elements of a pointer have been discovered.
    if ( buttonArray )
    {
        if ( pointer )
        {
            count = buttonArray->getCount();
                        
            for (index=0; index<count; index++)
            {
                element = (IOHIDElement *)buttonArray->getObject(index);
                
                if ( !element ) continue;
                
                _bootSupport &= ~kMouseButtons;
                    
                buttonCollection = element->getParentElement();
                stored |= storeReportElement ( element );
            }
        }
        buttonArray->release();
    }

	
	if (supportsInk)
	{
		setProperty("SupportsInk", 1, 32);
	}
	
	if (buttonCollection == relativeCollection)
		_relativeButtonCollection = true;
    
    return ( stored || _bootSupport );

}

//====================================================================================================
// IOHIDEventDriver::storeReportElement
//====================================================================================================
bool IOHIDEventDriver::storeReportElement ( IOHIDElement * element )
{
    OSArray **  array;
    SInt32      type;
    
    type = GetReportType(element->getType());
    if ( type == -1 ) return false;
    
    array = &(GetElementArray(GetReportHandlerSlot(element->getReportID()), type));
    
    if ( *array == NULL )
    {
        (*array) = OSArray::withCapacity(4);
    }
    
    (*array)->setObject ( element );
    
    return true;
}


//====================================================================================================
// IOHIDEventDriver::getReportElements
//====================================================================================================
OSArray * IOHIDEventDriver::getReportElements()
{
    return _supportedElements;
}

//====================================================================================================
// IOHIDEventDriver::_handleInterruptReport
//====================================================================================================
void IOHIDEventDriver::_handleInterruptReport (
                                OSObject *                  target,
                                AbsoluteTime                timeStamp,
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID,
                                void *                      refcon __unused)
{
    IOHIDEventDriver *  self = (IOHIDEventDriver *)target;
    
    self->handleInterruptReport(timeStamp, report, reportType, reportID);
}

//====================================================================================================
// IOHIDEventDriver::setButtonState
//====================================================================================================
static inline void setButtonState(UInt32 * state, UInt32 bit, UInt32 value) 
{
    
	UInt32 buttonMask = (1 << bit);
	
    if ( value )
        (*state) |= buttonMask;
    else
        (*state) &= ~buttonMask;
}

//====================================================================================================
// IOHIDEventDriver::handleInterruptReport
//====================================================================================================
void IOHIDEventDriver::handleInterruptReport (
                                AbsoluteTime                timeStamp,
                                IOMemoryDescriptor *        report,
                                IOHIDReportType             reportType,
                                UInt32                      reportID)
{
    OSNumber *      number;
    OSArray *       elements;
    IOHIDElement *  element;
    IOGBounds       bounds;
    UInt32          count           = 0;
    UInt32          index           = 0;
    UInt32          usage           = 0;
    UInt32          usagePage       = 0;
    UInt32          buttonState     = _cachedButtonState;
    UInt32          transducerID    = reportID;
    UInt32          volumeHandled   = 0;
    UInt32          volumeState     = 0;
    SInt32          relativeX       = 0;
    SInt32          relativeY       = 0;
    SInt32          absoluteX       = 0;
    SInt32          absoluteY       = 0;
    SInt32          absoluteZ       = 0;
    SInt32          scrollVert      = 0;
    SInt32          scrollHoriz     = 0;
    SInt32          barrelPressure  = 0;
    SInt32          tiltX           = 0;
    SInt32          tiltY           = 0;
    SInt32          twist           = 0;
    SInt32          tipPressure     = EV_MAXPRESSURE;
    SInt32          tipPressureMin  = EV_MAXPRESSURE;
    SInt32          tipPressureMax  = EV_MAXPRESSURE;
    SInt32          barrelPressureMin = 0;
    SInt32          barrelPressureMax = 0;
    SInt32          boundsDiff      = 0;
    SInt32          absoluteAxisRemovalPercentage = 15;
    bool            absoluteAxis    = false;
    bool            pointingHandled = false;
    bool            tabletHandled   = false;
    bool            proximityHandled = false;
    bool            invert          = false;
    bool            elementIsCurrent= false;
    bool            inRange         = _cachedRangeState;
    AbsoluteTime    elementTS;
    AbsoluteTime    reportTS;
    
    if (!readyForReports())
        return;
    
    elements = GetElementArray(GetReportHandlerSlot(reportID), reportType);
    
    IOHID_DEBUG(kIOHIDDebugCode_InturruptReport, reportType, reportID, elements ? elements->getCount() : -1, 0);
    
    if ( elements ) {
        number = (OSNumber*)copyProperty(kIOHIDAbsoluteAxisBoundsRemovalPercentage);
        if ( OSDynamicCast(OSNumber, number) ) {
            absoluteAxisRemovalPercentage = number->unsigned32BitValue();
        }
        OSSafeReleaseNULL(number);
            
        count = elements->getCount();
        
        for (index = 0; index < count; index++) {
            element = (IOHIDElement *)elements->getObject(index);
            
            elementTS           = element->getTimeStamp();
            reportTS            = timeStamp;
            
            elementIsCurrent    = (CMP_ABSOLUTETIME(&elementTS, &reportTS) == 0);
            
            if ( element->getReportID() != reportID )
                continue;
                
            usagePage       = element->getUsagePage();
            usage           = element->getUsage();
            
            
            if ( usagePage == kHIDPage_GenericDesktop ) {
                switch ( element->getUsage() ) {
                    case kHIDUsage_GD_X:
                        pointingHandled |= true;
                        if (element->getFlags() & kIOHIDElementFlagsRelativeMask) {
                            if ( elementIsCurrent )
                                relativeX = element->getValue();
                        }
                        else {
                            tabletHandled  |= elementIsCurrent;
                            absoluteAxis    = true;
                            absoluteX       = element->getValue();
                            bounds.minx     = element->getLogicalMin();
                            bounds.maxx     = element->getLogicalMax();
                            boundsDiff      = ((bounds.maxx - bounds.minx) * absoluteAxisRemovalPercentage) / 200;
                            bounds.minx     += boundsDiff;
                            bounds.maxx     -= boundsDiff;
                        }
                        break;
                        
                    case kHIDUsage_GD_Y:
                        pointingHandled |= true;
                        if (element->getFlags() & kIOHIDElementFlagsRelativeMask) {
                            if ( elementIsCurrent )
                                relativeY = element->getValue();
                        }
                        else {
                            tabletHandled  |= elementIsCurrent;
                            absoluteAxis    = true;
                            absoluteY       = element->getValue();
                            bounds.miny     = element->getLogicalMin();
                            bounds.maxy     = element->getLogicalMax();
                            bounds.maxy     = element->getLogicalMax();
                            boundsDiff      = ((bounds.maxy - bounds.miny) * absoluteAxisRemovalPercentage) / 200;
                            bounds.miny     += boundsDiff;
                            bounds.maxy     -= boundsDiff;
                        }
                        break;
                        
                    case kHIDUsage_GD_Z:
                        if (element->getFlags() & kIOHIDElementFlagsRelativeMask) {
                            if ( elementIsCurrent )
                                scrollHoriz = element->getValue();
                        }
                        else {
                            tabletHandled  |= elementIsCurrent;
                            absoluteZ       = element->getValue();
                        }
                        break;
                        
                    case kHIDUsage_GD_Wheel:
                        if ( elementIsCurrent ) {
                            scrollVert = element->getValue();
                        }
                        break;
                    case kHIDUsage_GD_SystemPowerDown:
                    case kHIDUsage_GD_SystemSleep:
                    case kHIDUsage_GD_SystemWakeUp:
                        if ( elementIsCurrent )
                            dispatchKeyboardEvent( timeStamp, usagePage, usage, element->getValue());
                        break;
                }
            }
            else if ( usagePage == kHIDPage_Digitizer ) {
                pointingHandled |= elementIsCurrent;
                
                switch ( usage ) {
                    case kHIDUsage_Dig_TipSwitch:
                        setButtonState ( &buttonState, 0, element->getValue());
                        break;
                    case kHIDUsage_Dig_BarrelSwitch:
                        setButtonState ( &buttonState, 1, element->getValue());
                        break;
                    case kHIDUsage_Dig_Eraser:
                        setButtonState ( &buttonState, 2, element->getValue());
                        break;
                    case kHIDUsage_Dig_InRange:
                        if ( elementIsCurrent ) {
                            inRange = (element->getValue() != 0);
                            proximityHandled |= ( inRange != _cachedRangeState );
                        }
                        break;
                    case kHIDUsage_Dig_BarrelPressure:
                        tabletHandled      |= elementIsCurrent;
                        barrelPressure      = element->getValue();
                        barrelPressureMin   = element->getLogicalMin();
                        barrelPressureMax   = element->getLogicalMax();
                        barrelPressureMin  += ((barrelPressureMax - barrelPressureMin) * 15) / 100;
                        break;
                    case kHIDUsage_Dig_TipPressure:
                        tabletHandled  |= elementIsCurrent;
                        tipPressure     = element->getValue();
                        tipPressureMin  = element->getLogicalMin();
                        tipPressureMax  = element->getLogicalMax();
                        tipPressureMin  += ((tipPressureMax - tipPressureMin) * 15) / 100;
                        break;
                    case kHIDUsage_Dig_XTilt:
                        tabletHandled  |= elementIsCurrent;
                        tiltX           = element->getValue();
                        break;
                    case kHIDUsage_Dig_YTilt:
                        tabletHandled  |= elementIsCurrent;
                        tiltY           = element->getValue();
                        break;
                    case kHIDUsage_Dig_Twist:
                        tabletHandled  |= elementIsCurrent;
                        twist           = element->getValue();
                        break;
                    case kHIDUsage_Dig_TransducerIndex:
                        tabletHandled  |= elementIsCurrent;
                        transducerID    = element->getValue();
                        break;
                    case kHIDUsage_Dig_Invert:
                        proximityHandled |= elementIsCurrent;
                        invert          = (element->getValue() != 0);
                        break;
                    default:
                        break;
                }
            }
            else if ( usagePage == kHIDPage_Button ) {
                pointingHandled |= true;
                
                tabletHandled  |= ( absoluteAxis && elementIsCurrent );
                
                setButtonState ( &buttonState, (element->getUsage() - 1), element->getValue());
            }
            else if (( usagePage == kHIDPage_KeyboardOrKeypad ) && elementIsCurrent ) {
                dispatchKeyboardEvent( timeStamp, usagePage, usage, element->getValue());
            }
            else if (( usagePage == kHIDPage_Consumer ) && elementIsCurrent ) {
                switch ( usage ) {
                    case kHIDUsage_Csmr_VolumeIncrement:
                        volumeHandled   |= 0x1;
                        volumeState     |= (element->getValue() != 0) ? 0x1:0;
                        break;
                    case kHIDUsage_Csmr_VolumeDecrement:
                        volumeHandled   |= 0x2;
                        volumeState     |= (element->getValue() != 0) ? 0x2:0;
                        break;
                    case kHIDUsage_Csmr_Mute:
                        volumeHandled   |= 0x4;
                        volumeState     |= (element->getValue() != 0) ? 0x4:0;
                        break;
                    case kHIDUsage_Csmr_ACPan:
                        scrollHoriz = -element->getValue();
                        break;
                    default:
                        dispatchKeyboardEvent(timeStamp, usagePage, usage, element->getValue());
                        break;
                }
            }
            else if (elementIsCurrent &&
                     (((usagePage == kHIDPage_AppleVendorTopCase) && (usage == kHIDUsage_AV_TopCase_KeyboardFn)) ||
                      ((usagePage == kHIDPage_AppleVendorKeyboard) && (usage == kHIDUsage_AppleVendorKeyboard_Function)))) {
                dispatchKeyboardEvent(timeStamp, usagePage, usage, element->getValue());
            }
            
        }
        
        // RY: Handle the case where Vol Increment, Decrement, and Mute are all down
        // If such an event occurs, it is likely that the device is defective,
        // and should be ignored.
        if ( (volumeState != 0x7) && (volumeHandled != 0x7) ) {
            // Volume Increment
            if ( volumeHandled & 0x1 )
                dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeIncrement, ((volumeState & 0x1) != 0));
            // Volume Decrement
            if ( volumeHandled & 0x2 )
                dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_VolumeDecrement, ((volumeState & 0x2) != 0));
            // Volume Mute
            if ( volumeHandled & 0x4 )
                dispatchKeyboardEvent(timeStamp, kHIDPage_Consumer, kHIDUsage_Csmr_Mute, ((volumeState & 0x4) != 0));
        }
        
        if ( scrollVert || scrollHoriz )
            dispatchScrollWheelEvent(timeStamp, scrollVert, scrollHoriz, 0);
    }
    
    if ( (_bootSupport & kBootMouse) && (reportID == 0)) {
        handleBootPointingReport(report, &relativeX, &relativeY, &buttonState);
        pointingHandled |= true;
    }
    
    
    if ( proximityHandled ) {
        dispatchTabletProximityEvent(timeStamp, transducerID, inRange, invert);
        _cachedRangeState = inRange;
    }
    
    if ( tabletHandled && inRange ) {
        dispatchTabletPointerEvent(timeStamp, transducerID, absoluteX, absoluteY, absoluteZ, &bounds, buttonState, tipPressure, tipPressureMin, tipPressureMax, barrelPressure, barrelPressureMin, barrelPressureMax, tiltX, tiltY, twist);
    }
    
    if ( pointingHandled || proximityHandled ) {
        if ( proximityHandled || (absoluteAxis && inRange && !relativeX && !relativeY &&
                                  !((buttonState != _cachedButtonState) && _relativeButtonCollection))) {
            if ( !inRange ) {
                buttonState = 0;
                tipPressure = tipPressureMin;
            }
            
            dispatchAbsolutePointerEvent(timeStamp, absoluteX, absoluteY, &bounds, buttonState, inRange, tipPressure, tipPressureMin, tipPressureMax);
        }
        else if (relativeX || relativeY || (buttonState != _cachedButtonState)) {
            dispatchRelativePointerEvent(timeStamp, relativeX, relativeY, buttonState);
        }
        
        _cachedButtonState = buttonState;
    }
}

//====================================================================================================
// IOHIDEventDriver::handleBootPointingReport
//====================================================================================================
void IOHIDEventDriver::handleBootPointingReport (
                                IOMemoryDescriptor *        report,
                                SInt32 *                    dX,
                                SInt32 *                    dY,
                                UInt32 *                    buttonState)
{
    UInt32          bootOffset;
    UInt8 *         mouseData;
    IOByteCount     reportLength;
    
    // Get a pointer to the data in the descriptor.
    reportLength = report->getLength();
    
    if ( !reportLength )
        return;
        
    mouseData = (UInt8 *)IOMalloc(reportLength);
    
    if ( !mouseData )
        return;
        
    report->readBytes( 0, (void *)mouseData, reportLength );

    if ( reportLength >= 3 )
    {
        bootOffset = ( _multipleReports ) ? 1 : 0;
        
        if ( _bootSupport & kMouseButtons )
            *buttonState = mouseData[bootOffset];
        
        if ( _bootSupport & kMouseXAxis )
            *dX = mouseData[bootOffset + 1];

        if ( _bootSupport & kMouseYAxis )
            *dY = mouseData[bootOffset + 2];
    }

    IOFree((void *)mouseData, reportLength);
}


//====================================================================================================
// IOHIDEventDriver::setElementValue
//====================================================================================================
void IOHIDEventDriver::setElementValue (
                                UInt32                      usagePage,
                                UInt32                      usage,
                                UInt32                      value )
{
	IOHIDElement *element = 0;
    
    if ( usagePage == kHIDPage_LEDs )
        element = _ledElements[usage - kHIDUsage_LED_NumLock];
	
	if (element)
		element->setValue(value);        
}

//====================================================================================================
// IOHIDEventDriver::getElementValue
//====================================================================================================
UInt32 IOHIDEventDriver::getElementValue ( 
                                UInt32                      usagePage,
                                UInt32                      usage )
{
	IOHIDElement *element = 0;
    
    if ( usagePage == kHIDPage_LEDs )
        element = _ledElements[usage - kHIDUsage_LED_NumLock];
	
    return (element) ? element->getValue() : 0;
}


OSMetaClassDefineReservedUnused(IOHIDEventDriver,  0);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  1);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  2);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  3);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  4);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  5);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  6);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  7);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  8);
OSMetaClassDefineReservedUnused(IOHIDEventDriver,  9);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 10);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 11);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 12);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 13);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 14);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 15);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 16);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 17);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 18);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 19);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 20);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 21);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 22);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 23);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 24);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 25);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 26);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 27);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 28);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 29);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 30);
OSMetaClassDefineReservedUnused(IOHIDEventDriver, 31);

